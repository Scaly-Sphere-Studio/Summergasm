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

// Layout must match SceneRenderer::GPUInstance (see lit_plane.frag)
struct Instance {
    int texture_unit;
    int normal_unit;
    int uv_mode;
    int grayscale;
    vec2 uv_offset;
    float lighting;
    float shadow_transmission;
    int cast_shadow;
    int emissive_unit;
    int blur_clamp;
    int _padding;
    vec4 emissive;
};
layout(std430, binding = 1) readonly buffer Instances { Instance instances[]; };

// Depth of field, see lit_plane.frag
uniform vec4 u_Focus;
uniform vec4 u_ViewDepth;
// World size of a pixel: x, times the distance if y = 1 (perspective)
uniform vec2 u_PixelSize;
uniform float u_FullBlur;

// Same as lit_plane.frag's
float blurRadius(vec3 p)
{
    if (u_Focus.z <= 0.0)
        return 0.0;
    float d = max(dot(u_ViewDepth, vec4(p, 1)), 1e-4);
    float coc = 0.0;
    if (d < u_Focus.x)
        coc = u_Focus.x / d - 1.0;
    else if (d > u_Focus.y)
        coc = 1.0 - u_Focus.y / d;
    coc = mix(coc, 1.0, u_FullBlur);
    return min(coc * u_Focus.z, u_Focus.w);
}

void main()
{
    instanceIndex = u_BaseInstance + gl_InstanceID;
    vec3 pos = a_Pos;
    vec2 uv = a_UV;
    vec4 world = a_Model * vec4(pos, 1);
    // Depth of field: blurred planes grow by their blur radius (+ a pixel),
    // for the blur to spread past their edges, their uv beyond [0, 1]
    float blur = blurRadius(world.xyz);
    if (blur >= 0.5 && instances[instanceIndex].blur_clamp == 0) {
        float px = u_PixelSize.x * (u_PixelSize.y > 0.5 ? max(dot(u_ViewDepth, world), 0.0) : 1.0);
        vec2 size = max(vec2(length(a_Model[0].xyz), length(a_Model[1].xyz)), vec2(1e-4));
        vec2 grow = (blur + 1.0) * px / size;
        pos.xy += sign(a_Pos.xy) * grow;
        uv += sign(a_UV - 0.5) * grow;
        world = a_Model * vec4(pos, 1);
    }
    gl_Position = u_VP * world;
    WorldPos = world.xyz;
    UVW = vec3(uv, a_TextureOffset);
    Alpha = a_Alpha;

    mat3 m = mat3(a_Model);
    vec3 T = normalize(m[0]);
    vec3 B = normalize(m[1]);
    // Mirrored planes (negative scaling) keep their normal toward the front
    vec3 N = normalize(cross(T, B)) * sign(determinant(m));
    TBN = mat3(T, B, N);
}
