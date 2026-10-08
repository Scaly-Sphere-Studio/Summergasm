#version 430 core
// Lit planes of SceneRenderer, see PointLight in SceneRenderer.hpp.
// Hot reloaded when saved: compile errors are logged, the previous shader stays.

#define _TWO_PI 6.28318530718
// Shadows fade out over this part of the shadow map's sides (in uv), instead
// of stopping hard at the edge of the light's cone
#define SHADOW_EDGE_FADE 0.15
// Soft shadows (PCSS): taps of the blocker search & of the filter, and the
// largest radius of both, in shadow map texels
#define BLOCKER_TAPS 20
#define PCF_TAPS 24
#define PCSS_MAX_RADIUS 32.0

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
    int _padding;
};
layout(std430, binding = 1) readonly buffer Instances { Instance instances[]; };

// Colors in linear space, intensity applied
struct Light {
    vec4 position_radius;
    vec4 color_falloff;
    vec4 terminator_width;
    vec4 shadow;
    mat4 shadow_vp;         // light's view projection
    vec4 shadow_params;     // layer (-1: none), bias, softness (texels)
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

    vec2 radius = l.shadow_params.z * texel;
    if (light_uv.x > 0.0 && light_uv.y > 0.0) {
        // Blocker search, over the light seen from the fragment: the
        // penumbra of a caster at the near plane
        vec2 search = min(light_uv * (d_r - z_near) / (z_near * d_r), max_radius);
        float blockers = 0.0;
        float depth_sum = 0.0;
        for (int i = 0; i < BLOCKER_TAPS; ++i) {
            vec2 offset = vogelDisk(i, BLOCKER_TAPS, angle) * search;
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
    for (int i = 0; i < PCF_TAPS; ++i) {
        vec2 offset = vogelDisk(i, PCF_TAPS, angle) * radius;
        float ref = uvz.z + dot(slope, offset) - slope_bias;
        visibility += texture(u_ShadowDepth, vec4(uvz.xy + offset, layer, ref));
        vec4 t = texture(u_ShadowColor, vec3(uvz.xy + offset, layer));
        transmittance += dist > t.a ? t.rgb : vec3(1);
    }
    visibility /= float(PCF_TAPS);
    transmittance /= float(PCF_TAPS);
    vec2 edge = min(uvz.xy, 1.0 - uvz.xy);
    float fade = smoothstep(0.0, SHADOW_EDGE_FADE, min(edge.x, edge.y));
    return mix(vec3(1), visibility * transmittance, fade);
}

vec3 pointLight(Light l, vec3 N)
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
    if (inst.texture_unit < 0)
        discard;

    vec2 uv = UVW.xy;
    if (inst.uv_mode == 1) {
        vec2 c = uv - vec2(0.5);
        float angle = atan(c.y, c.x) / _TWO_PI + 0.5 + inst.uv_offset.x;
        float radius = length(c) * 2.0 + inst.uv_offset.y;
        uv = vec2(angle, radius);
    } else {
        uv += inst.uv_offset;
    }
    vec4 albedo = texture(u_Textures[inst.texture_unit], vec3(uv, UVW.z));
    if (inst.grayscale == 1)
        albedo.rgb = vec3(dot(albedo.rgb, vec3(0.2126, 0.7152, 0.0722)));
    albedo.a *= Alpha;
    // Fully transparent texels don't write depth
    if (albedo.a < 0.004)
        discard;

    if (inst.lighting <= 0.0) {
        FragColor = albedo;
        return;
    }

    vec3 N = TBN[2];
    if (inst.normal_unit >= 0) {
        vec3 n = texture(u_Textures[inst.normal_unit], vec3(uv, 0)).xyz * 2.0 - 1.0;
        if (u_NormalYDown == 1)
            n.y = -n.y;
        N = normalize(TBN * n);
    }

    // Lights of this fragment's screen tile
    ivec2 tile = (ivec2(gl_FragCoord.xy) - u_TileInfo.xy) / u_TileInfo.w;
    uvec2 range = tiles[tile.y * u_TileInfo.z + tile.x];

    // Texels are sRGB: light in linear space
    vec3 light = u_Ambient;
    for (uint i = range.x; i < range.x + range.y; ++i)
        light += pointLight(lights[light_indices[i]], N);
    vec3 lit = pow(pow(albedo.rgb, vec3(2.2)) * light, vec3(1.0 / 2.2));
    FragColor = vec4(mix(albedo.rgb, lit, inst.lighting), albedo.a);
}
