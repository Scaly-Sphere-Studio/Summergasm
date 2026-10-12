#version 430 core
// Lit planes of SceneRenderer, see PointLight in SceneRenderer.hpp.
// Hot reloaded when saved: compile errors are logged, the previous shader stays.

#define _TWO_PI 6.28318530718
// Soft shadows (PCSS): taps of the blocker search & of the filter, and the
// largest radius of both, in shadow map texels
#define BLOCKER_TAPS 20
#define PCF_TAPS 24
#define PCSS_MAX_RADIUS 32.0
// Depth of field: most taps of a blurred texel, and filter taps of the
// shadows on blurred fragments (wider filter, more taps against grain)
#define BLUR_MAX_TAPS 64
#define BLUR_PCF_TAPS 48

out vec4 FragColor;

in vec3 WorldPos;
in vec3 UVW;
in float Alpha;
in mat3 TBN;
flat in int instanceIndex;

// Layouts must match SceneRenderer::GPUInstance & GPULight
struct Instance {
    int texture_unit;   // -1: no texture
    int normal_unit;    // -1: flat
    int uv_mode;        // 0 = Cartesian, 1 = Polar (matches Texture::UVMode)
    int grayscale;
    vec2 uv_offset;
    float lighting;     // 0 = unlit, 1 = fully lit
    float shadow_transmission;  // 0 = opaque, 1 = colored glass
    int cast_shadow;
    int emissive_unit;  // -1: none
    int blur_clamp;     // 1: the blur stays inside the plane
    int _padding;
    vec4 emissive;      // linear, intensity applied
};
layout(std430, binding = 1) readonly buffer Instances { Instance instances[]; };

// Colors in linear space, intensity applied
struct Light {
    vec4 position_radius;
    vec4 color_falloff;
    vec4 terminator_width;
    vec4 shadow;            // w: shadows' fade along the map's sides (uv, 0: none)
    mat4 shadow_vp;         // light's view projection
    vec4 shadow_params;     // layer (-1: none), bias, softness (texels), ambient hidden
    vec4 shadow_frustum;    // z_near, z_far, light size in uv * depth (x, y)
};
layout(std430, binding = 2) readonly buffer Lights { Light lights[]; };
// Per screen tile: offset & count of its lights in light_indices
layout(std430, binding = 3) readonly buffer LightTiles { uvec2 tiles[]; };
layout(std430, binding = 4) readonly buffer LightIndices { uint light_indices[]; };

// The last units are kept for the shadow maps: must match
// SceneRenderer::reserved_texture_units
uniform sampler2DArray u_Textures[gl_MaxTextureImageUnits - 4];
// 1 = DirectX normal maps (green down)
uniform int u_NormalYDown;
uniform vec3 u_Ambient;
// xy = viewport origin, z = tiles per row, w = tile size in pixels
uniform ivec4 u_TileInfo;
// Shadow maps, one layer per shadow casting light (see SceneRenderer.hpp)
uniform sampler2DArrayShadow u_ShadowDepth;
uniform sampler2DArray u_ShadowColor;
// Same as u_ShadowDepth, read without comparison
uniform sampler2DArray u_ShadowDepthRaw;
// 0: opaque texels (alpha >= u_OpaqueCutoff), with depth, in any order
// 1: translucent ones, blended back to front over them
uniform int u_Pass;
uniform float u_OpaqueCutoff;
// Depth of field: x, y = focus band (near, far), z = blur at infinity,
// w = largest blur, in pixels (z = 0: off)
uniform vec4 u_Focus;
// Distance from the camera along its view axis: dot(u_ViewDepth, vec4(p, 1))
uniform vec4 u_ViewDepth;

// Depth of field: the fragment's blur radius along the screen's x & y, in
// world units on its plane (set by main, zero when sharp). Shadows are
// filtered over that disk too.
vec3 blur_world_x = vec3(0);
vec3 blur_world_y = vec3(0);

// Depth slope of the receiving plane in the shadow map: its depth is affine
// in the shadow map's uv (planes stay planes through a projection), which
// gives the receiver's own depth under each PCF tap. Without it, taps
// spread over a plane lit at an angle shadow the plane itself.
// Zero when the plane is seen edge-on from the light.
vec2 receiverDepthSlope(Light l, vec3 p, vec3 uvz)
{
    // Two other points of the plane, far enough for the depth precision
    float e = max(length(l.position_radius.xyz - p), 1.0) * 0.01;
    vec4 c1 = l.shadow_vp * vec4(p + TBN[0] * e, 1);
    vec4 c2 = l.shadow_vp * vec4(p + TBN[1] * e, 1);
    if (c1.w <= 0.0 || c2.w <= 0.0)
        return vec2(0);
    vec3 d1 = c1.xyz / c1.w * 0.5 + 0.5 - uvz;
    vec3 d2 = c2.xyz / c2.w * 0.5 + 0.5 - uvz;
    float det = d1.x * d2.y - d2.x * d1.y;
    if (abs(det) < 1e-3 * length(d1.xy) * length(d2.xy))
        return vec2(0);
    return vec2(d1.z * d2.y - d1.y * d2.z, d1.x * d2.z - d1.z * d2.x) / det;
}

// Sample i of n spread evenly over the unit disk (golden angle spiral),
// rotated by angle
vec2 vogelDisk(int i, int n, float angle)
{
    float r = sqrt((float(i) + 0.5) / float(n));
    float theta = float(i) * 2.39996323 + angle;
    return r * vec2(cos(theta), sin(theta));
}

// Per pixel noise in [0, 1): rotates the disks, trading banding for grain
float interleavedGradientNoise(vec2 pixel)
{
    return fract(52.9829189 * fract(dot(pixel, vec2(0.06711056, 0.00583715))));
}

// Light reaching the fragment through the casters: 1 = lit, 0 = shadowed,
// colored = through translucent casters.
// Soft shadows (PCSS) of a light of size w: a caster at depth d_b (from the
// light) shadows a receiver at depth d_r with a penumbra
// w * (d_r - d_b) / d_b wide. The casters' average depth is searched around
// the fragment, then the shadow is filtered over that penumbra.
vec3 shadowFilter(Light l, vec3 Ldir)
{
    float layer = l.shadow_params.x;
    if (layer < 0.0)
        return vec3(1);
    // Offset toward the light, against self-shadowing
    vec3 p = WorldPos + Ldir * l.shadow_params.y;
    vec4 clip = l.shadow_vp * vec4(p, 1);
    if (clip.w <= 0.0)
        return vec3(1);
    vec3 uvz = clip.xyz / clip.w * 0.5 + 0.5;
    // Outside of the light's cone: no shadow
    if (any(lessThan(uvz, vec3(0))) || any(greaterThan(uvz, vec3(1))))
        return vec3(1);

    vec2 texel = 1.0 / vec2(textureSize(u_ShadowDepth, 0).xy);
    vec2 max_radius = PCSS_MAX_RADIUS * texel;
    vec2 slope = receiverDepthSlope(l, p, uvz);
    // Taps use texels up to half a texel away (bilinear or nearest)
    float slope_bias = 0.5 * dot(abs(slope), texel);
    float angle = interleavedGradientNoise(gl_FragCoord.xy) * _TWO_PI;
    // Distance (/ radius) to the light, compared to the translucent casters'
    float dist = length(l.position_radius.xyz - p) / l.position_radius.w;
    // Depths along the light's view axis: the receiver's is clip.w
    float z_near = l.shadow_frustum.x;
    float z_far = l.shadow_frustum.y;
    vec2 light_uv = l.shadow_frustum.zw;
    float d_r = clip.w;

    // Depth of field: the fragment's blur disk in the shadow map (columns:
    // uv offsets of its x & y radii), added to every tap's offset
    mat2 blur_uv = mat2(0);
    bool blurred = blur_world_x != vec3(0) || blur_world_y != vec3(0);
    if (blurred) {
        vec4 cx = l.shadow_vp * vec4(p + blur_world_x, 1);
        vec4 cy = l.shadow_vp * vec4(p + blur_world_y, 1);
        if (cx.w > 0.0 && cy.w > 0.0)
            blur_uv = mat2(cx.xy / cx.w * 0.5 + 0.5 - uvz.xy, cy.xy / cy.w * 0.5 + 0.5 - uvz.xy);
    }
    int pcf_taps = blurred ? BLUR_PCF_TAPS : PCF_TAPS;

    vec2 radius = l.shadow_params.z * texel;
    if (light_uv.x > 0.0 && light_uv.y > 0.0) {
        // Blocker search, over the light seen from the fragment: the
        // penumbra of a caster at the near plane
        vec2 search = min(light_uv * (d_r - z_near) / (z_near * d_r), max_radius);
        float blockers = 0.0;
        float depth_sum = 0.0;
        for (int i = 0; i < BLOCKER_TAPS; ++i) {
            vec2 disk = vogelDisk(i, BLOCKER_TAPS, angle);
            vec2 offset = disk * search + blur_uv * disk;
            vec3 uvl = vec3(uvz.xy + offset, layer);
            float d = texture(u_ShadowDepthRaw, uvl).r;
            if (d < uvz.z + dot(slope, offset) - slope_bias) {
                // Window depth to view depth
                depth_sum += z_near * z_far / (z_far - d * (z_far - z_near));
                blockers += 1.0;
            }
            // Translucent casters, at the same ratio of the receiver's depth
            float t = texture(u_ShadowColor, uvl).a;
            if (t < dist) {
                depth_sum += d_r * t / dist;
                blockers += 1.0;
            }
        }
        // Nothing between the light and the fragment
        if (blockers == 0.0)
            return vec3(1);
        float d_b = depth_sum / blockers;
        vec2 penumbra = light_uv * max(d_r - d_b, 0.0) / (max(d_b, 1e-4) * d_r);
        radius = max(radius, penumbra);
    }
    radius = min(radius, max_radius);

    // Opaque casters: each tap is a 2x2 bilinear comparison against the
    // receiver's depth at the tap. Translucent casters only tint when nearer
    // to the light than the fragment.
    float visibility = 0.0;
    vec3 transmittance = vec3(0);
    for (int i = 0; i < pcf_taps; ++i) {
        vec2 disk = vogelDisk(i, pcf_taps, angle);
        vec2 offset = disk * radius + blur_uv * disk;
        float ref = uvz.z + dot(slope, offset) - slope_bias;
        visibility += texture(u_ShadowDepth, vec4(uvz.xy + offset, layer, ref));
        vec4 t = texture(u_ShadowColor, vec3(uvz.xy + offset, layer));
        transmittance += dist > t.a ? t.rgb : vec3(1);
    }
    visibility /= float(pcf_taps);
    transmittance /= float(pcf_taps);
    // Instead of stopping hard at the edge of the light's cone (none when the
    // shadow map covers the whole view)
    float fade = 1.0;
    if (l.shadow.w > 0.0) {
        vec2 edge = min(uvz.xy, 1.0 - uvz.xy);
        fade = smoothstep(0.0, l.shadow.w, min(edge.x, edge.y));
    }
    return mix(vec3(1), visibility * transmittance, fade);
}

// Blur radius of the fragment, in pixels, from its distance to the focus band
// (thin lens circle of confusion)
float blurRadius()
{
    if (u_Focus.z <= 0.0)
        return 0.0;
    float d = max(dot(u_ViewDepth, vec4(WorldPos, 1)), 1e-4);
    float coc = 0.0;
    if (d < u_Focus.x)
        coc = u_Focus.x / d - 1.0;
    else if (d > u_Focus.y)
        coc = 1.0 - u_Focus.y / d;
    return min(coc * u_Focus.z, u_Focus.w);
}

// Plane uv to texture uv
vec2 mapUV(Instance inst, vec2 uv)
{
    if (inst.uv_mode == 1) {
        vec2 c = uv - vec2(0.5);
        float angle = atan(c.y, c.x) / _TWO_PI + 0.5 + inst.uv_offset.x;
        float radius = length(c) * 2.0 + inst.uv_offset.y;
        return vec2(angle, radius);
    }
    return uv + inst.uv_offset;
}

// Blurred texel of one of the plane's textures: average over a disk of
// radius pixels around the fragment, weighted by alpha. px_to_uv: screen
// pixels to plane uv; grad_x & grad_y: texture uv per pixel. Taps off the
// plane are transparent (the plane grew to fit the blur, see lit_plane.vert),
// or clamped on it with blur_clamp. They sample a mip level fitting their
// spacing.
vec4 blurredTexel(Instance inst, int unit, float layer, float radius, mat2 px_to_uv,
    vec2 grad_x, vec2 grad_y, float angle)
{
    // Roughly one tap per 1.5 pixel², the spacing between taps prefiltered
    int taps = clamp(int(radius * radius * 0.75), 8, BLUR_MAX_TAPS);
    float spacing = max(radius * 1.77 / sqrt(float(taps)), 1.0);
    vec4 sum = vec4(0);
    for (int i = 0; i < taps; ++i) {
        vec2 uv = UVW.xy + px_to_uv * (vogelDisk(i, taps, angle) * radius);
        if (inst.blur_clamp != 0)
            uv = clamp(uv, 0.0, 1.0);
        else if (any(lessThan(uv, vec2(0))) || any(greaterThan(uv, vec2(1))))
            continue;
        vec4 t = textureGrad(u_Textures[unit], vec3(mapUV(inst, uv), layer),
            grad_x * spacing, grad_y * spacing);
        sum += vec4(t.rgb * t.a, t.a);
    }
    sum /= float(taps);
    return vec4(sum.a > 0.0 ? sum.rgb / sum.a : vec3(0), sum.a);
}

// Cast shadows also hide part of the ambient light, scaling ambient_vis down
vec3 pointLight(Light l, vec3 N, inout float ambient_vis)
{
    vec3 L = l.position_radius.xyz - WorldPos;
    float d = length(L);
    float radius = l.position_radius.w;
    if (d >= radius)
        return vec3(0);
    float att = pow(1.0 - d / radius, l.color_falloff.w);
    vec3 Ldir = L / max(d, 1e-4);
    float ndl = dot(N, Ldir);

    // Wrapped Lambert: the light goes w past the terminator
    float w = l.terminator_width.w;
    float diffuse = clamp((ndl + w) / (1.0 + w), 0.0, 1.0);
    // No need for shadows where the light doesn't reach anyway
    vec3 filtered = diffuse > 0.0 ? shadowFilter(l, Ldir) : vec3(1);
    float visibility = max(filtered.r, max(filtered.g, filtered.b));
    // The stronger the light reaching the fragment, the more ambient its
    // shadows hide: dim lights barely darken, strong ones leave deep shadows
    float strength = min(dot(att * l.color_falloff.rgb, vec3(0.2126, 0.7152, 0.0722)), 1.0)
        * l.shadow_params.w;
    ambient_vis *= 1.0 - strength * (1.0 - visibility);
    // Color lerp: terminator color at N.L <= 0 and in shadow penumbras,
    // light color from N.L >= w in full light
    vec3 hue = l.color_falloff.rgb;
    if (w > 0.0)
        hue = mix(l.terminator_width.rgb, l.color_falloff.rgb,
            smoothstep(0.0, w, ndl) * smoothstep(0.0, 1.0, visibility));
    // Unlit part of the light's area (shadows included) takes its shadow color
    vec3 lit = diffuse * filtered;
    return att * (hue * lit + l.shadow.rgb * (1.0 - lit));
}

void main()
{
    Instance inst = instances[instanceIndex];
    vec2 uv = mapUV(inst, UVW.xy);
    // Derivatives before any discard (blurred taps sample explicit gradients)
    mat2 px_to_uv = mat2(dFdx(UVW.xy), dFdy(UVW.xy));
    vec2 grad_x = dFdx(uv);
    vec2 grad_y = dFdy(uv);
    vec3 world_x = dFdx(WorldPos);
    vec3 world_y = dFdy(WorldPos);
    if (inst.texture_unit < 0)
        discard;

    float blur = blurRadius();
    // Below half a pixel, the blur wouldn't show
    bool blurred = blur >= 0.5;
    // Off the plane, where it grew for a blur this fragment doesn't have
    // (e.g. tilted planes)
    if (!blurred && (any(lessThan(UVW.xy, vec2(-1e-3))) || any(greaterThan(UVW.xy, vec2(1.001)))))
        discard;
    if (blurred) {
        blur_world_x = world_x * blur;
        blur_world_y = world_y * blur;
    }
    float blur_angle = interleavedGradientNoise(gl_FragCoord.xy) * _TWO_PI;
    vec4 albedo = blurred
        ? blurredTexel(inst, inst.texture_unit, UVW.z, blur, px_to_uv, grad_x, grad_y, blur_angle)
        : texture(u_Textures[inst.texture_unit], vec3(uv, UVW.z));
    if (inst.grayscale == 1)
        albedo.rgb = vec3(dot(albedo.rgb, vec3(0.2126, 0.7152, 0.0722)));
    albedo.a *= Alpha;
    if (u_Pass == 0) {
        if (albedo.a < u_OpaqueCutoff)
            discard;
        albedo.a = 1.0;
    }
    else if (albedo.a >= u_OpaqueCutoff || albedo.a < 0.004)
        discard;

    // Emission, linear: added after lighting, not shadowed
    vec3 emission = vec3(0);
    if (inst.emissive_unit >= 0) {
        vec4 e = blurred
            ? blurredTexel(inst, inst.emissive_unit, 0.0, blur, px_to_uv, grad_x, grad_y, blur_angle)
            : texture(u_Textures[inst.emissive_unit], vec3(uv, 0));
        emission = pow(e.rgb, vec3(2.2)) * e.a * inst.emissive.rgb;
    }

    if (inst.lighting <= 0.0) {
        if (inst.emissive_unit >= 0)
            albedo.rgb = pow(pow(albedo.rgb, vec3(2.2)) + emission, vec3(1.0 / 2.2));
        FragColor = albedo;
        return;
    }

    vec3 N = TBN[2];
    if (inst.normal_unit >= 0) {
        vec3 n = (blurred
            ? blurredTexel(inst, inst.normal_unit, 0.0, blur, px_to_uv, grad_x, grad_y, blur_angle)
            : texture(u_Textures[inst.normal_unit], vec3(uv, 0))).xyz * 2.0 - 1.0;
        if (u_NormalYDown == 1)
            n.y = -n.y;
        N = normalize(TBN * n);
    }

    // Lights of this fragment's screen tile
    ivec2 tile = (ivec2(gl_FragCoord.xy) - u_TileInfo.xy) / u_TileInfo.w;
    uvec2 range = tiles[tile.y * u_TileInfo.z + tile.x];

    // Texels are sRGB: light in linear space
    float ambient_vis = 1.0;
    vec3 direct = vec3(0);
    for (uint i = range.x; i < range.x + range.y; ++i)
        direct += pointLight(lights[light_indices[i]], N, ambient_vis);
    vec3 light = u_Ambient * ambient_vis + direct;
    vec3 lit = pow(pow(albedo.rgb, vec3(2.2)) * light, vec3(1.0 / 2.2));
    vec3 color = mix(albedo.rgb, lit, inst.lighting);
    if (inst.emissive_unit >= 0)
        color = pow(pow(color, vec3(2.2)) + emission, vec3(1.0 / 2.2));
    FragColor = vec4(color, albedo.a);
}
