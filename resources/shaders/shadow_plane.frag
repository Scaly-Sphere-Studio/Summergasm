#version 430 core
// Shadow maps of SceneRenderer, two passes per light (see _renderShadowMaps):
// 0. opaque texels write the depth,
// 1. translucent texels multiply the RGB transmittance (blending), and keep
//    their nearest distance to the light in alpha (GL_MIN).
// Hot reloaded when saved: compile errors are logged, the previous shader stays.

#define _TWO_PI 6.28318530718
// Texels letting less light through are opaque
#define OPAQUE_LIMIT 0.5
// Texels letting more light through don't cast anything
#define CLEAR_LIMIT 0.996

out vec4 FragColor;

in vec3 WorldPos;
in vec3 UVW;
in float Alpha;
flat in int instanceIndex;

// Layout must match SceneRenderer::GPUInstance
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

// The last units are kept for the shadow maps: must match
// SceneRenderer::reserved_texture_units
uniform sampler2DArray u_Textures[gl_MaxTextureImageUnits - 4];
uniform int u_Pass;
uniform vec4 u_LightPosRadius;

void main()
{
    Instance inst = instances[instanceIndex];
    if (inst.texture_unit < 0 || inst.cast_shadow == 0)
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
    float a = albedo.a * Alpha;

    // Light going through the texel: what its alpha lets pass, plus what its
    // color filters (linear) when the plane transmits light
    vec3 T = vec3(1.0 - a) + a * inst.shadow_transmission * pow(albedo.rgb, vec3(2.2));
    float t = max(T.r, max(T.g, T.b));

    if (u_Pass == 0) {
        if (t >= OPAQUE_LIMIT)
            discard;
        FragColor = vec4(0);
    } else {
        if (t < OPAQUE_LIMIT || t >= CLEAR_LIMIT)
            discard;
        float dist = length(u_LightPosRadius.xyz - WorldPos) / u_LightPosRadius.w;
        FragColor = vec4(T, dist);
    }
}
