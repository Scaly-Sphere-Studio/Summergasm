#version 430 core
// Lit planes of SceneRenderer, see PointLight in SceneRenderer.hpp.
// Hot reloaded when saved: compile errors are logged, the previous shader stays.

#define _TWO_PI 6.28318530718

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
    float _padding;
};
layout(std430, binding = 1) readonly buffer Instances { Instance instances[]; };

// Colors in linear space, intensity applied
struct Light {
    vec4 position_radius;
    vec4 color_falloff;
    vec4 terminator_width;
    vec4 shadow;
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

vec3 pointLight(Light l, vec3 N)
{
    vec3 L = l.position_radius.xyz - WorldPos;
    float d = length(L);
    float radius = l.position_radius.w;
    if (d >= radius)
        return vec3(0);
    float att = pow(1.0 - d / radius, l.color_falloff.w);
    float ndl = dot(N, L / max(d, 1e-4));

    // Wrapped Lambert: the light goes w past the terminator
    float w = l.terminator_width.w;
    float diffuse = clamp((ndl + w) / (1.0 + w), 0.0, 1.0);
    // Color lerp: terminator color at N.L <= 0, light color from N.L >= w
    vec3 hue = l.color_falloff.rgb;
    if (w > 0.0)
        hue = mix(l.terminator_width.rgb, l.color_falloff.rgb, smoothstep(0.0, w, ndl));
    // Unlit part of the light's area takes its shadow color
    return att * (hue * diffuse + l.shadow.rgb * (1.0 - diffuse));
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
