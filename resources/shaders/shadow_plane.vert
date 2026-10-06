#version 430 core
// Shadow maps of SceneRenderer: planes seen from a light.
layout(location = 0) in vec3 a_Pos;
layout(location = 1) in vec2 a_UV;

layout(location = 2) in mat4 a_Model;

layout(location = 6) in float a_Alpha;
layout(location = 7) in uint a_TextureOffset;

// Light's view projection
uniform mat4 u_VP;
// First instance of the current draw (gl_InstanceID starts at 0 for each)
uniform int u_BaseInstance;

out vec3 WorldPos;
out vec3 UVW;
out float Alpha;
// Index in the instances SSBO
flat out int instanceIndex;

// Layout must match SceneRenderer::GPUInstance
struct Instance {
    int texture_unit;
    int normal_unit;
    int uv_mode;
    int grayscale;
    vec2 uv_offset;
    float lighting;
    float shadow_transmission;
    int cast_shadow;
    int _padding;
};
layout(std430, binding = 1) readonly buffer Instances { Instance instances[]; };

void main()
{
    vec4 world = a_Model * vec4(a_Pos, 1);
    gl_Position = u_VP * world;
    WorldPos = world.xyz;
    UVW = vec3(a_UV, a_TextureOffset);
    Alpha = a_Alpha;
    instanceIndex = u_BaseInstance + gl_InstanceID;
    // Planes not casting shadows are clipped away, not rasterized
    Instance inst = instances[instanceIndex];
    if (inst.cast_shadow == 0 || inst.texture_unit < 0)
        gl_Position = vec4(2, 2, 2, 1);
}
