#include "Transition.hpp"
#include "includes.hpp"

namespace transition {
namespace {

using clock = std::chrono::steady_clock;

// Fullscreen triangle, no vertex buffer needed
char const* const vertex_source = R"GLSL(
#version 430 core
out vec2 v_UV;
void main()
{
    // Vertices (0,0), (2,0), (0,2) in UV space: covers the screen over [0,1]
    vec2 uv = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    v_UV = uv;
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
)GLSL";

// Common part of the fragment shaders. The script's shader goes between the
// head and the tail, and defines: vec3 transition(vec2 uv, vec2 p)
char const* const fragment_head = R"GLSL(
#version 430 core
in vec2 v_UV;
out vec4 o_Color;

uniform sampler2D u_Scene;   // Current frame (the new scene for dual transitions)
uniform sampler2D u_Old;     // Previous frame, kept when a dual transition starts
uniform sampler2D u_Mask;    // Mask file of the options, see u_HasMask
uniform int u_HasMask;
uniform float u_Cover;       // Single: coverage by u_Color (1: covered). Dual: progress (0: old, 1: new)
uniform vec3 u_Color;
uniform vec3 u_Palette[4];   // Palette of the options, u_PaletteCount colors used
uniform int u_PaletteCount;
uniform int u_Strips;
uniform float u_Softness;
uniform float u_Angle;
uniform float u_Aspect;      // Width / height of the screen

const float PI = 3.14159265;

float hash1(float n) { return fract(sin(n * 91.345) * 43758.5453); }
float hash2(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
float smooth01(float x) { x = clamp(x, 0.0, 1.0); return x * x * (3.0 - 2.0 * x); }

// Progress of stripe i: starts after a random delay (up to half the cover)
float stripeProgress(float i, float cover)
{
    float delay = 0.5 * hash1(i);
    return clamp((cover - delay) / 0.5, 0.0, 1.0);
}

// Half diagonal of the screen, in the units of p
float maxRadius() { return 0.5 * length(vec2(u_Aspect, 1.0)); }

vec3 scene(vec2 uv) { return texture(u_Scene, clamp(uv, 0.0, 1.0)).rgb; }
vec3 old(vec2 uv) { return texture(u_Old, clamp(uv, 0.0, 1.0)).rgb; }
float maskValue(vec2 uv) { return texture(u_Mask, clamp(uv, 0.0, 1.0)).r; }

// Single viewport: the base colored by 'color' with coverage m (1: covered).
// The screen is fully covered at the end of the cover phase, whatever m is.
vec3 covered(vec3 base, vec3 color, float m)
{
    return mix(base, color, max(m, step(0.9995, u_Cover)));
}

)GLSL";

char const* const fragment_tail = R"GLSL(

void main()
{
    vec2 uv = v_UV;
    // Centered, aspect corrected: y in [-0.5, 0.5]
    vec2 p = (uv - 0.5) * vec2(u_Aspect, 1.0);
    // Never true: keeps u_Cover in the program, so that its location tells
    // whether the program linked (see compile())
    if (u_Cover < -1.0e9)
        o_Color = vec4(0.0);
    else
        o_Color = vec4(transition(uv, p), 1.0);
}
)GLSL";

// Playing a transition is done with its own shader; without one, the frame is shown as is
char const* const passthrough_source = R"GLSL(
vec3 transition(vec2 uv, vec2 p) { return scene(uv); }
)GLSL";

// Layered single transitions: one fixed program, the layers are uniforms (see
// packField() & packFill()). Shapes are evaluated in the shape's frame, with
// the same units as the Lua side (screen height 1, see Param)
char const* const layer_fragment = R"GLSL(
#version 430 core
in vec2 v_UV;
out vec4 o_Color;

uniform sampler2D u_Scene;
uniform sampler2D u_Tex0;
uniform sampler2D u_Tex1;
uniform sampler2D u_Tex2;
uniform sampler2D u_Tex3;
uniform sampler2D u_Old;     // Previous frame, the old scene of dual transitions
uniform int u_Count;
uniform float u_Aspect;
uniform float u_Progress;
// Motion of the scene: uv shift, and amplitude of a horizontal wobble
uniform vec2 u_SceneShift;
uniform float u_Wave;
// Two fields per layer (the layer's, then its mask), 4 vec4 each:
// [cx, cy, radius, width] [height, rotation, round, thickness]
// [angle, offset, threshold, softness] [invert, shape, texture, -]
uniform vec4 u_Field[16 * 2 * 4];
// Six vec4 per layer: [rgb, opacity], 4 gradient stops [rgb, -],
// [fill kind, stop count, texture, gradient angle]. Previous: the stop block
// holds the uv shift of the sampled frame in its xy
uniform vec4 u_Fill[16 * 6];

float hash2(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }

vec4 sampleTexture(int slot, vec2 uv)
{
    uv = clamp(uv, 0.0, 1.0);
    if (slot == 1) return texture(u_Tex1, uv);
    if (slot == 2) return texture(u_Tex2, uv);
    if (slot == 3) return texture(u_Tex3, uv);
    return texture(u_Tex0, uv);
}

// Coverage of field f, in [0, 1]
float fieldCoverage(int f, vec2 p, vec2 uv)
{
    vec4 a = u_Field[f * 4];
    vec4 b = u_Field[f * 4 + 1];
    vec4 c = u_Field[f * 4 + 2];
    vec4 d = u_Field[f * 4 + 3];
    int shape = int(d.y + 0.5);
    float s = max(c.w, 0.001);
    float cov = 1.0;
    if (shape >= 1 && shape <= 5) {
        // Shape's frame: centered on (cx, cy), rotated by -rotation
        vec2 q = p - a.xy;
        float cs = cos(b.y), sn = sin(b.y);
        q = vec2(cs * q.x + sn * q.y, cs * q.y - sn * q.x);
        float dist;
        if (shape == 1) {
            dist = length(q) - a.z;
        }
        else if (shape == 2) {
            vec2 half_size = vec2(a.w, b.x) * 0.5 - b.z;
            vec2 dq = abs(q) - half_size;
            dist = length(max(dq, 0.0)) + min(max(dq.x, dq.y), 0.0) - b.z;
        }
        else if (shape == 3) {
            dist = abs(length(q) - a.z) - b.w * 0.5;
        }
        else if (shape == 4) {
            dist = (abs(q.x) + abs(q.y) - a.z) * 0.70710678;
        }
        else {
            // Implicit heart, scaled by radius: inside where negative
            vec2 h = vec2(q.x, 0.1 - q.y) / max(a.z, 0.0001);
            float t = h.x * h.x + h.y * h.y - 1.0;
            float v = t * t * t - h.x * h.x * h.y * h.y * h.y;
            dist = v * a.z * 0.25;
        }
        cov = 1.0 - smoothstep(-s, s, dist);
    }
    else if (shape == 6) {
        // Ramp: covered before the offset along the direction
        vec2 dir = vec2(cos(c.x), sin(c.x));
        cov = 1.0 - smoothstep(c.y - s, c.y + s, dot(p - a.xy, dir));
    }
    else if (shape == 7) {
        // Noise, one random value per 1/256 of the screen
        float v = hash2(floor(uv * 256.0));
        cov = 1.0 - smoothstep(c.z - s, c.z + s, v);
    }
    else if (shape == 8) {
        // Texture luma against the threshold
        vec4 tx = sampleTexture(int(d.z + 0.5), uv);
        float v = dot(tx.rgb, vec3(0.299, 0.587, 0.114));
        cov = 1.0 - smoothstep(c.z - s, c.z + s, v);
    }
    return d.x > 0.5 ? 1.0 - cov : cov;
}

vec3 gradientColor(int layer, vec2 p)
{
    float angle = u_Fill[layer * 6 + 5].w;
    vec2 dir = vec2(cos(angle), sin(angle));
    // 0 at the half diagonal before the screen's center, 1 after it
    float half_diagonal = 0.5 * length(vec2(u_Aspect, 1.0));
    float t = clamp(dot(p, dir) / half_diagonal * 0.5 + 0.5, 0.0, 1.0);
    int n = clamp(int(u_Fill[layer * 6 + 5].y + 0.5), 2, 4);
    vec3 stops[4];
    for (int j = 0; j < 4; ++j)
        stops[j] = u_Fill[layer * 6 + 1 + j].rgb;
    float g = t * float(n - 1);
    int k = min(int(floor(g)), n - 2);
    return mix(stops[k], stops[k + 1], g - float(k));
}

void main()
{
    vec2 uv = v_UV;
    vec2 p = (uv - 0.5) * vec2(u_Aspect, 1.0);
    // The scene, moved and wobbled as a whole
    vec2 suv = uv + u_SceneShift;
    suv.x += sin(uv.y * 30.0 + u_Progress * 12.0) * u_Wave;
    vec3 color = texture(u_Scene, clamp(suv, 0.0, 1.0)).rgb;
    for (int i = 0; i < u_Count; ++i) {
        float cov = fieldCoverage(2 * i, p, uv) * fieldCoverage(2 * i + 1, p, uv);
        vec4 base = u_Fill[i * 6];
        vec4 info = u_Fill[i * 6 + 5];
        int kind = int(info.x + 0.5);
        vec3 src = base.rgb;
        float alpha = cov * base.a;
        if (kind == 1) {
            src = gradientColor(i, p);
        }
        else if (kind == 2) {
            vec4 tx = sampleTexture(int(info.z + 0.5), uv);
            src = tx.rgb;
            alpha *= tx.a;
        }
        else if (kind == 3) {
            src = texture(u_Old, clamp(uv - u_Fill[i * 6 + 1].xy, 0.0, 1.0)).rgb;
        }
        color = mix(color, src, clamp(alpha, 0.0, 1.0));
    }
    o_Color = vec4(color, 1.0);
}
)GLSL";

std::vector<Def> defs;
SSS::GL::Shaders::Shared passthrough;
SSS::GL::Shaders::Shared layer_program;

// Offscreen frame, with the window's size
struct Target {
    GLuint fbo{ 0 };
    GLuint color{ 0 };
    GLuint depth{ 0 };
};
Target live;
Target snapshot;
GLint width = 0;
GLint height = 0;
GLuint composite_vao = 0;

struct Running {
    Def def;
    Options options;
    float duration{ 1.f };
    float elapsed{ 0.f };
    bool paused{ false };
    bool midpoint_done{ false };
    bool active{ false };
    // Changes at each start() & stop(), to know if a callback replaced the transition
    uint64_t id{ 0 };
    clock::time_point last;
};
Running running;
uint64_t last_id = 0;

// Settings of the panel, applied to the transitions it launches
struct PanelState {
    float duration{ 1.f };
    bool loop{ false };
    bool scrub{ false };
    float softness{ 0.05f };
    int strips{ 12 };
    float angle{ 0.f };
    float color[3]{ 0.f, 0.f, 0.f };
};
PanelState panel;

// Compiles a fragment shader from the script's body. Null on error (the driver's
// messages are logged by SSS::GL)
SSS::GL::Shaders::Shared compile(std::string const& body)
{
    auto shaders = SSS::GL::Shaders::create();
    shaders->loadFromStrings(vertex_source, fragment_head + body + fragment_tail);
    if (shaders->getUniformLocation("u_Cover") == -1)
        return nullptr;
    return shaders;
}

SSS::GL::Shaders::Shared const& passthroughShaders()
{
    if (!passthrough)
        passthrough = compile(passthrough_source);
    return passthrough;
}

SSS::GL::Shaders::Shared const& layerShaders()
{
    if (!layer_program) {
        layer_program = SSS::GL::Shaders::create();
        layer_program->loadFromStrings(vertex_source, layer_fragment);
    }
    return layer_program;
}

// Default of each Param, in the same order
constexpr std::array<float, param_count> param_defaults{
    0.f, 0.f,            // X, Y
    0.5f,                // Radius
    0.5f, 0.5f,          // Width, Height
    0.f, 0.f, 0.05f,     // Rotation, Round, Thickness
    0.f, 0.f,            // Angle, Offset
    0.5f, 0.05f,         // Threshold, Softness
};

// Uniform block of a field, see layer_fragment
void packField(Field const& field, float progress, float aspect, glm::vec4* out)
{
    std::array<float, param_count> v{};
    for (int k = 0; k != param_count; ++k)
        v[k] = field.params[k] ? field.params[k](progress, aspect) : param_defaults[k];
    out[0] = { v[X], v[Y], v[Radius], v[Width] };
    out[1] = { v[Height], v[Rotation], v[Round], v[Thickness] };
    out[2] = { v[Angle], v[Offset], v[Threshold], v[Softness] };
    out[3] = { field.invert ? 1.f : 0.f, static_cast<float>(field.shape), static_cast<float>(std::max(field.texture, 0)), 0.f };
}

// Uniform block of a layer's fill, see layer_fragment
void packFill(Layer const& layer, float progress, float aspect, glm::vec4* out)
{
    Fill const& fill = layer.fill;
    glm::vec3 const rgb = fill.color ? fill.color(progress, aspect) : glm::vec3(0.f);
    float const opacity = layer.opacity ? layer.opacity(progress, aspect) : 1.f;
    out[0] = { rgb, opacity };
    for (size_t j = 0; j != 4; ++j)
        out[1 + j] = { j < fill.stops.size() ? fill.stops[j] : glm::vec3(0.f), 0.f };
    if (fill.kind == Fill::Kind::Previous) {
        out[1] = { fill.shift_x ? fill.shift_x(progress, aspect) : 0.f, fill.shift_y ? fill.shift_y(progress, aspect) : 0.f, 0.f, 0.f };
    }
    float const angle = fill.angle ? fill.angle(progress, aspect) : 0.f;
    int kind = 2;
    if (fill.kind == Fill::Kind::Solid)
        kind = 0;
    else if (fill.kind == Fill::Kind::Gradient)
        kind = 1;
    else if (fill.kind == Fill::Kind::Previous)
        kind = 3;
    out[5] = { static_cast<float>(kind), static_cast<float>(fill.stops.size()), static_cast<float>(std::max(fill.texture, 0)), angle };
}

void release(Target& target)
{
    glDeleteFramebuffers(1, &target.fbo);
    glDeleteTextures(1, &target.color);
    glDeleteRenderbuffers(1, &target.depth);
    target = {};
}

void allocate(Target& target, bool with_depth)
{
    release(target);
    glGenTextures(1, &target.color);
    glBindTexture(GL_TEXTURE_2D, target.color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &target.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.color, 0);
    if (with_depth) {
        glGenRenderbuffers(1, &target.depth);
        glBindRenderbuffer(GL_RENDERBUFFER, target.depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, target.depth);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// Targets follow the window's viewport. Resizing loses their content.
void ensureSize()
{
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    if (vp[2] <= 0 || vp[3] <= 0)
        return;
    if (live.fbo != 0 && vp[2] == width && vp[3] == height)
        return;
    width = vp[2];
    height = vp[3];
    allocate(live, true);
    allocate(snapshot, false);
}

// Copies the frame drawn last (still in the live target) to the snapshot
void captureSnapshot()
{
    if (live.fbo == 0 || snapshot.fbo == 0)
        return;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, live.fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, snapshot.fbo);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

float smooth01(float x)
{
    x = std::clamp(x, 0.f, 1.f);
    return x * x * (3.f - 2.f * x);
}

// Coverage of a single viewport transition (cover then reveal), or progress of a dual one
float coverage()
{
    if (!running.active)
        return 0.f;
    float const t = std::clamp(running.elapsed / running.duration, 0.f, 1.f);
    if (running.def.kind == Kind::Dual)
        return smooth01(t);
    return t < 0.5f ? smooth01(2.f * t) : 1.f - smooth01(2.f * t - 1.f);
}

GLuint maskTexture()
{
    static std::map<std::string, SSS::GL::Texture::Shared> masks;
    if (!running.active || running.options.mask.empty())
        return 0;
    auto& texture = masks[running.options.mask];
    if (!texture)
        texture = SSS::GL::Texture::create(g->assets_folder + "transitions/" + running.options.mask);
    return texture->getBasicTextureID();
}

void draw(SSS::GL::Shaders& shaders)
{
    Options const& opts = running.options;
    bool const dual = running.def.kind == Kind::Dual;
    glm::vec3 const color = opts.color.value_or(dual ? glm::vec3(1.f) : glm::vec3(0.f));
    // At least one color, the uniform array holds 4
    std::vector<glm::vec3> const palette = opts.palette.empty() ? std::vector<glm::vec3>{ glm::vec3(1.f) } : opts.palette;
    glm::vec3 colors[4];
    for (size_t i = 0; i != 4; ++i)
        colors[i] = palette[std::min(i, palette.size() - 1)];
    GLuint const mask = maskTexture();

    shaders.use();
    glBindVertexArray(composite_vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, live.color);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, snapshot.color);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, mask);

    glUniform1i(shaders.getUniformLocation("u_Scene"), 0);
    glUniform1i(shaders.getUniformLocation("u_Old"), 1);
    glUniform1i(shaders.getUniformLocation("u_Mask"), 2);
    glUniform1i(shaders.getUniformLocation("u_HasMask"), mask != 0 ? 1 : 0);
    glUniform1f(shaders.getUniformLocation("u_Cover"), coverage());
    glUniform3fv(shaders.getUniformLocation("u_Color"), 1, &color.x);
    glUniform3fv(shaders.getUniformLocation("u_Palette"), 4, &colors[0].x);
    glUniform1i(shaders.getUniformLocation("u_PaletteCount"), static_cast<int>(palette.size()));
    glUniform1i(shaders.getUniformLocation("u_Strips"), opts.strips.value_or(12));
    glUniform1f(shaders.getUniformLocation("u_Softness"), opts.softness.value_or(0.05f));
    glUniform1f(shaders.getUniformLocation("u_Angle"), opts.angle.value_or(0.f));
    glUniform1f(shaders.getUniformLocation("u_Aspect"), static_cast<float>(width) / static_cast<float>(height));

    glDrawArrays(GL_TRIANGLES, 0, 3);

    glBindVertexArray(0);
    glUseProgram(0);
    glActiveTexture(GL_TEXTURE0);
}

// Progress of the running transition, 0 to 1
float progress()
{
    if (!running.active)
        return 0.f;
    return std::clamp(running.elapsed / running.duration, 0.f, 1.f);
}

// Draws the layers of the running transition over the live frame
void drawLayers(SSS::GL::Shaders& shaders, float const t)
{
    Def const& def = running.def;
    float const aspect = static_cast<float>(width) / static_cast<float>(height);
    std::array<glm::vec4, 2 * max_layers * 4> fields{};
    std::array<glm::vec4, max_layers * 6> fills{};
    glm::vec3 const cover = running.options.color.value_or(glm::vec3(0.f));
    int const count = static_cast<int>(std::min<size_t>(def.layers.size(), max_layers));
    for (int i = 0; i != count; ++i) {
        Layer const& layer = def.layers[i];
        // Unset masks stay zeroed: full coverage
        packField(layer.field, t, aspect, &fields[i * 8]);
        if (layer.mask)
            packField(*layer.mask, t, aspect, &fields[i * 8 + 4]);
        packFill(layer, t, aspect, &fills[i * 6]);
        // Solid fills without a color: the cover color
        if (layer.fill.kind == Fill::Kind::Solid && !layer.fill.color)
            fills[i * 6] = { cover, fills[i * 6].a };
    }
    glm::vec2 const shift{
        def.scene_x ? def.scene_x(t, aspect) : 0.f,
        def.scene_y ? def.scene_y(t, aspect) : 0.f,
    };
    float const wave = def.wave ? def.wave(t, aspect) : 0.f;

    shaders.use();
    glBindVertexArray(composite_vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, live.color);
    for (int slot = 0; slot != max_textures; ++slot) {
        glActiveTexture(GL_TEXTURE1 + slot);
        bool const loaded = slot < static_cast<int>(def.textures.size());
        glBindTexture(GL_TEXTURE_2D, loaded ? def.textures[slot]->getBasicTextureID() : 0);
    }
    glActiveTexture(GL_TEXTURE1 + max_textures);
    glBindTexture(GL_TEXTURE_2D, snapshot.color);

    glUniform1i(shaders.getUniformLocation("u_Scene"), 0);
    for (int slot = 0; slot != max_textures; ++slot)
        glUniform1i(shaders.getUniformLocation("u_Tex" + std::to_string(slot)), 1 + slot);
    glUniform1i(shaders.getUniformLocation("u_Old"), 1 + max_textures);
    glUniform1i(shaders.getUniformLocation("u_Count"), count);
    glUniform1f(shaders.getUniformLocation("u_Aspect"), aspect);
    glUniform1f(shaders.getUniformLocation("u_Progress"), t);
    glUniform2f(shaders.getUniformLocation("u_SceneShift"), shift.x, shift.y);
    glUniform1f(shaders.getUniformLocation("u_Wave"), wave);
    glUniform4fv(shaders.getUniformLocation("u_Field"), static_cast<GLsizei>(fields.size()), &fields[0].x);
    glUniform4fv(shaders.getUniformLocation("u_Fill"), static_cast<GLsizei>(fills.size()), &fills[0].x);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    glBindVertexArray(0);
    glUseProgram(0);
    glActiveTexture(GL_TEXTURE0);
}

// Taken by value: the callback may start or stop transitions, which replaces the
// stored one while it runs
void fire(std::function<void()> callback)
{
    if (!callback)
        return;
    try {
        callback();
    }
    catch (std::exception const& e) {
        LOG_FUNC_ERR(e.what());
    }
}

// Options of the call, the transition's defaults for what it doesn't set
Options withDefaults(Options const& defaults, Options options)
{
    if (!options.color)
        options.color = defaults.color;
    if (options.palette.empty())
        options.palette = defaults.palette;
    if (options.mask.empty())
        options.mask = defaults.mask;
    if (!options.strips)
        options.strips = defaults.strips;
    if (!options.softness)
        options.softness = defaults.softness;
    if (!options.angle)
        options.angle = defaults.angle;
    return options;
}

void launch(std::string const& name);

}

bool define(Script script)
{
    Def def;
    def.name = script.name;
    def.label = script.label;
    def.kind = script.kind;
    def.duration = script.duration;
    def.midpoint = script.midpoint;
    def.defaults = std::move(script.defaults);

    if (!script.layers.empty()) {
        bool const uses_previous = std::any_of(script.layers.begin(), script.layers.end(), [](Layer const& layer) {
            return layer.fill.kind == Fill::Kind::Previous;
        });
        if (script.textures.size() > max_textures) {
            LOG_CTX_ERR(script.name, "at most 4 textures per transition");
            return true;
        }
        if (uses_previous && script.kind != Kind::Dual) {
            LOG_CTX_ERR(script.name, "the previous frame fill is for dual transitions");
            return true;
        }
        def.scene_x = std::move(script.scene_x);
        def.scene_y = std::move(script.scene_y);
        def.wave = std::move(script.wave);
        try {
            for (auto const& file : script.textures)
                def.textures.push_back(SSS::GL::Texture::create(g->assets_folder + "transitions/" + file));
        }
        catch (std::exception const& e) {
            LOG_CTX_ERR(script.name, e.what());
            return true;
        }
        def.layers = std::move(script.layers);
    }
    else {
        auto shaders = compile(script.shader);
        if (!shaders) {
            LOG_CTX_ERR(script.name, "the shader doesn't compile, the transition is not updated");
            return true;
        }
        def.shaders = std::move(shaders);
    }

    auto const it = std::find_if(defs.begin(), defs.end(), [&](Def const& other) { return other.name == def.name; });
    if (it != defs.end())
        *it = std::move(def);
    else
        defs.push_back(std::move(def));
    std::sort(defs.begin(), defs.end(), [](Def const& a, Def const& b) {
        return a.kind != b.kind ? a.kind < b.kind : a.name < b.name;
    });
    return false;
}

std::vector<Def> const& list()
{
    return defs;
}

bool start(std::string const& name, std::optional<float> duration, Options options)
{
    auto const it = std::find_if(defs.begin(), defs.end(), [&](Def const& def) { return def.name == name; });
    if (it == defs.end()) {
        LOG_FUNC_CTX_WRN("Unknown transition", name);
        return true;
    }
    if (options.palette.size() > 4)
        options.palette.resize(4);

    // The previous frame is the old scene of the dual transitions
    if (it->kind == Kind::Dual)
        captureSnapshot();

    running.def = *it;
    running.options = withDefaults(it->defaults, std::move(options));
    running.duration = std::max(duration.value_or(it->duration), 0.05f);
    running.elapsed = 0.f;
    running.paused = false;
    running.midpoint_done = false;
    running.active = true;
    running.id = ++last_id;
    running.last = clock::now();
    return false;
}

void stop()
{
    running.active = false;
    running.options = {};
    running.id = ++last_id;
}

bool isActive()
{
    return running.active;
}

void update()
{
    if (!running.active)
        return;
    auto const now = clock::now();
    float const dt = std::min(std::chrono::duration<float>(now - running.last).count(), 0.1f);
    running.last = now;
    if (!running.paused)
        running.elapsed += dt;

    float const t = std::clamp(running.elapsed / running.duration, 0.f, 1.f);
    // Dual transitions switch scene at their first update, the single ones once covered
    float const midpoint = running.def.kind == Kind::Dual ? 0.f : running.def.midpoint;
    uint64_t const id = running.id;
    if (!running.midpoint_done && t >= midpoint) {
        running.midpoint_done = true;
        fire(running.options.on_midpoint);
        // Replaced or stopped by the callback
        if (running.id != id)
            return;
    }
    if (t >= 1.f) {
        auto const on_done = running.options.on_done;
        running.active = false;
        running.options = {};
        running.id = ++last_id;
        fire(on_done);
    }
}

void beginFrame()
{
    ensureSize();
    if (live.fbo == 0)
        return;
    glBindFramebuffer(GL_FRAMEBUFFER, live.fbo);
    static constexpr GLfloat black_opaque[4] = { 0.f, 0.f, 0.f, 1.f };
    static constexpr GLfloat far_depth = 1.f;
    // The window's default state, see SSS::GL::Window
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glClearBufferfv(GL_COLOR, 0, black_opaque);
    glClearBufferfv(GL_DEPTH, 0, &far_depth);
}

void endFrame()
{
    if (live.fbo == 0)
        return;
    // Composed without blending or depth test, the state is restored after
    bool const blend = glIsEnabled(GL_BLEND) == GL_TRUE;
    bool const depth = glIsEnabled(GL_DEPTH_TEST) == GL_TRUE;
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    if (composite_vao == 0)
        glGenVertexArrays(1, &composite_vao);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (running.active && !running.def.layers.empty()) {
        drawLayers(*layerShaders(), progress());
    }
    else if (auto const& shaders = running.active ? running.def.shaders : passthroughShaders(); shaders) {
        draw(*shaders);
    }
    else {
        // Without any shader, the frame is shown as is
        glBindFramebuffer(GL_READ_FRAMEBUFFER, live.fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    if (blend)
        glEnable(GL_BLEND);
    if (depth)
        glEnable(GL_DEPTH_TEST);
}

void free()
{
    release(live);
    release(snapshot);
    width = 0;
    height = 0;
    glDeleteVertexArrays(1, &composite_vao);
    composite_vao = 0;
    stop();
    passthrough.reset();
    layer_program.reset();
    defs.clear();
}

namespace {

Options panelOptions()
{
    Options options;
    options.color = glm::vec3(panel.color[0], panel.color[1], panel.color[2]);
    options.softness = panel.softness;
    options.strips = panel.strips;
    options.angle = panel.angle;
    return options;
}

// The loop of the panel relaunches the transition when it is done
void launch(std::string const& name)
{
    Options options = panelOptions();
    if (panel.loop)
        options.on_done = [name]() { launch(name); };
    start(name, panel.duration, std::move(options));
}

}

void printPanel()
{
    if (!ImGui::CollapsingHeader("Transitions"))
        return;
    ImGui::Indent(10.f);
    ImGui::SliderFloat(" Duration", &panel.duration, 0.2f, 4.f, "%.2f s");
    ImGui::Checkbox(" Loop", &panel.loop);
    ImGui::SameLine();
    ImGui::Checkbox(" Scrub", &panel.scrub);
    ImGui::ColorEdit3(" Color", panel.color, ImGuiColorEditFlags_NoInputs);
    ImGui::SliderFloat(" Softness", &panel.softness, 0.f, 0.3f);
    ImGui::SliderInt(" Strips", &panel.strips, 2, 32);
    ImGui::SliderFloat(" Angle", &panel.angle, -3.14159f, 3.14159f);

    // Scrubbing pauses the running transition, the slider then sets its progress
    if (running.active) {
        running.paused = panel.scrub;
        float progress = running.elapsed / running.duration;
        if (ImGui::SliderFloat(" Progress", &progress, 0.f, 1.f) && panel.scrub)
            running.elapsed = progress * running.duration;
    }
    if (ImGui::Button("Stop"))
        stop();

    bool first = true;
    Kind kind = Kind::Single;
    for (auto const& def : defs) {
        if (first || def.kind != kind) {
            kind = def.kind;
            first = false;
            ImGui::Separator();
            ImGui::TextDisabled(kind == Kind::Single ? "Single viewport" : "Dual scene");
        }
        ImGui::PushID(def.name.c_str());
        if (ImGui::Button("Launch"))
            launch(def.name);
        ImGui::SameLine();
        ImGui::TextUnformatted(def.label.c_str());
        ImGui::PopID();
    }
    ImGui::Unindent(10.f);
}

}
