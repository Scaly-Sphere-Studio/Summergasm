#pragma once

// Scene transitions, drawn over the frames of the window. Frames are rendered in
// an offscreen target, which keeps the previous frame for the dual transitions.
//
// Each transition is a Lua script, resources/lua/transitions/<name>.lua (see the
// README). Its kind decides how it is played:
// - single: the scene is drawn in the back, the transition's layers (or its
//   shader) cover the screen in front of it. The scene switch (on_midpoint)
//   happens at the midpoint, while covered, then the screen is revealed.
// - dual: the previous frame transitions into the new scene.
//
// A single transition is either a list of layers (declarative, see Layer), or a
// GLSL shader written in the script.

namespace transition {

enum class Kind { Single, Dual };

// A number that depends on the progress of the transition (0 to 1) and on the
// aspect ratio of the screen (width / height)
using Curve = std::function<float(float, float)>;
// A color (sRGB) that depends on the same
using ColorCurve = std::function<glm::vec3(float, float)>;

inline constexpr int max_layers = 16;
inline constexpr int max_textures = 4;

// Coverage of a field: 0 outside, 1 inside
enum class Shape { Full, Circle, Box, Ring, Diamond, Heart, Ramp, Noise, Texture };

// Numeric parameters of a field, in this order. Units: the screen's height is 1,
// centered (x in [-aspect/2, aspect/2], y in [-0.5, 0.5]). Defaults in Transition.cpp
enum Param {
    X, Y,           // Center of the shape
    Radius,         // Circle, ring, diamond, heart (scale): half size
    Width, Height,  // Box, full size
    Rotation,       // Box, shapes, radians
    Round,          // Box: corner radius
    Thickness,      // Ring: line width
    Angle,          // Ramp: direction, radians (0: left to right)
    Offset,         // Ramp: position of the edge along the direction
    Threshold,      // Noise & texture: coverage is 1 below it (values in [0, 1])
    Softness,       // Edge width
    param_count
};

// Scalar field, see fieldCoverage() in Transition.cpp
struct Field {
    Shape shape{ Shape::Full };
    bool invert{ false };
    // Index into Script::textures, texture shape only
    int texture{ -1 };
    // Unset: the parameter's default
    std::array<Curve, param_count> params;
};

// What a layer draws
struct Fill {
    // Previous: the frame shown before a dual transition (the old scene)
    enum class Kind { Solid, Gradient, Texture, Previous };
    Kind kind{ Kind::Solid };
    // Solid. Unset: the cover color of the call (the script's color by default)
    ColorCurve color;
    // Gradient: 2 to 4 sRGB colors, spread evenly along the angle across the screen
    std::vector<glm::vec3> stops;
    Curve angle;
    // Texture: index into Script::textures
    int texture{ -1 };
    // Previous: offset of the sampled frame, in uv units
    Curve shift_x;
    Curve shift_y;
};

// Coverage of the field, multiplied by the mask's (if any), filled with the fill
struct Layer {
    Field field;
    std::optional<Field> mask;
    Fill fill;
    // Alpha of the layer, 1 if unset
    Curve opacity;
};

struct Options {
    // sRGB, default: the transition's, black for single and white for dual ones
    std::optional<glm::vec3> color;
    // sRGB colors, the transition's palette if empty
    std::vector<glm::vec3> palette;
    // Grayscale file of resources/assets/transitions/
    std::string mask;
    std::optional<int> strips;
    std::optional<float> softness;
    // Radians
    std::optional<float> angle;
    // Single: the scene is switched. Dual: the new scene is shown
    std::function<void()> on_midpoint;
    std::function<void()> on_done;
};

// A transition read from its Lua script
struct Script {
    std::string name;
    std::string label;
    Kind kind{ Kind::Single };
    float duration{ 1.f };
    // Progress (0 to 1) of the scene switch, single transitions
    float midpoint{ 0.5f };
    // Layered transitions: motion of the scene drawn behind them, in uv units.
    // scene_x & scene_y shift it, wave is the amplitude of a horizontal wobble
    Curve scene_x;
    Curve scene_y;
    Curve wave;
    // Settings of the script, the callbacks are not used
    Options defaults;
    // GLSL body, the script's transition(uv, p) function, see Transition.cpp.
    // Unused when there are layers
    std::string shader;
    // Single transitions drawn with layers, instead of the shader
    std::vector<Layer> layers;
    // Files of resources/assets/transitions/, indexed by the layers' textures
    std::vector<std::string> textures;
    std::filesystem::path path;
};

struct Def {
    std::string name;
    std::string label;
    Kind kind{ Kind::Single };
    float duration{ 1.f };
    float midpoint{ 0.5f };
    Options defaults;
    // Shader transitions. Null for the layered ones
    SSS::GL::Shaders::Shared shaders;
    std::vector<Layer> layers;
    std::vector<SSS::GL::Texture::Shared> textures;
    Curve scene_x;
    Curve scene_y;
    Curve wave;
};

// Adds or replaces a transition, its shader is compiled now (the previous one is
// kept if it fails). Returns true on error
bool define(Script script);

// Every transition, sorted by kind then name
std::vector<Def> const& list();

// Starts a transition (replaces the running one, its callbacks are not called).
// Duration: the transition's if not given. Returns true on error (unknown name)
bool start(std::string const& name, std::optional<float> duration, Options options);
// Stops the running transition, its callbacks are not called
void stop();
bool isActive();

// Advances the running transition and fires its callbacks. Called after the scenes ran
void update();
// Frames are drawn between these two calls
void beginFrame();
void endFrame();

// Releases the GL objects, while the context is still alive
void free();

// ImGui panel: launches & previews every transition
void printPanel();

}
