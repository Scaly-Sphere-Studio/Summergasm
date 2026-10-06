#version 430 core
// Lit planes of SceneRenderer. Same attributes as the SSS::GL Plane preset.
layout(location = 0) in vec3 a_Pos;
layout(location = 1) in vec2 a_UV;

layout(location = 2) in mat4 a_Model;

layout(location = 6) in float a_Alpha;
layout(location = 7) in uint a_TextureOffset;

uniform mat4 u_VP;
// First instance of the current draw (gl_InstanceID starts at 0 for each)
uniform int u_BaseInstance;

out vec3 WorldPos;
out vec3 UVW;
out float Alpha;
// Plane tangent space in world space: T = plane x, B = plane y, N = front
out mat3 TBN;
// Index in the instances SSBO
flat out int instanceIndex;

void main()
{
    vec4 world = a_Model * vec4(a_Pos, 1);
    gl_Position = u_VP * world;
    WorldPos = world.xyz;
    UVW = vec3(a_UV, a_TextureOffset);
    Alpha = a_Alpha;
    instanceIndex = u_BaseInstance + gl_InstanceID;

    mat3 m = mat3(a_Model);
    vec3 T = normalize(m[0]);
    vec3 B = normalize(m[1]);
    // Mirrored planes (negative scaling) keep their normal toward the front
    vec3 N = normalize(cross(T, B)) * sign(determinant(m));
    TBN = mat3(T, B, N);
}
