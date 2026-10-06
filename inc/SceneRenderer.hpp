#pragma once

#include "includes.hpp"

// Point light of a SceneRenderer. Colors are sRGB, as picked; the lighting
// itself is computed in linear space.
// Diffuse is a wrapped Lambert: the light reaches terminator_width past the
// terminator (N.L = 0), and its color lerps from terminator_color at the
// terminator to color at N.L = terminator_width. This fakes subsurface
// scattering or a colored terminator without stacking lights.
// shadow_color fills the unlit part of the light's area (black: classic).
struct PointLight
{
    using Shared = std::shared_ptr<PointLight>;
    static Shared create() { return std::make_shared<PointLight>(); };

    glm::vec3 position{ 0.f };
    glm::vec3 color{ 1.f };
    float intensity{ 1.f };
    // Distance at which the light reaches 0, in world units
    float radius{ 1000.f };
    // Attenuation: pow(1 - distance / radius, falloff)
    float falloff{ 2.f };
    glm::vec3 terminator_color{ 1.f, 0.4f, 0.3f };
    // In N.L units, 0 disables the terminator color and the wrap
    float terminator_width{ 0.f };
    glm::vec3 shadow_color{ 0.f };
    bool enabled{ true };
};

// Renders a whole 3D scene of planes seen through a (perspective) camera.
// Planes are drawn back to front from the camera, so transparent sprites
// blend correctly within a single renderer and a single depth buffer.
// Scroll layers are sets of tiles lined up along x that scroll and wrap
// together, all at the same world speed: the visible parallax only comes
// from their depth.
// Planes are lit by any number of point lights (see PointLight), culled per
// screen tile, with optional per-plane normal maps, through
// resources/shaders/lit_plane.vert & .frag (hot reloaded when edited).
// Planes sharing a texture share a texture unit, so many planes with a few
// textures are drawn in a single instanced call.
// Written in SSS::GL style, to be moved into the library once validated.
class SceneRenderer : public SSS::GL::PlaneRenderer
{
private:
    SceneRenderer() = default;

public:
    using Shared = std::shared_ptr<SceneRenderer>;
    static Shared create(SSS::GL::Camera::Shared camera = nullptr);

    void render() override;

    // Adds the planes to the renderer, as a new scroll layer. The tiles are
    // lined up along x, from x = 0, once all their textures are loaded.
    void addScrollLayer(std::vector<SSS::GL::Plane::Shared> const& planes);
    // Stops scrolling all layers (their planes stay in the renderer)
    void clearScrollLayers();
    inline size_t getScrollLayerCount() const noexcept { return _scroll_layers.size(); };

    void play();
    void pause();
    void toggle();
    inline bool isPlaying() const noexcept { return _play; };

    // In world units per second, the same for every scroll layer
    float scroll_speed{ 100.f };
    // Sort planes back to front from the camera before each render
    bool depth_sort{ true };

    // Lighting

    // Folder of lit_plane.vert & lit_plane.frag, set at startup
    static inline std::string shaders_folder;
    // Screen tiles of the light culling, in pixels
    static constexpr int tile_size = 32;
    // Texture units kept free for the shadow maps
    static constexpr uint32_t reserved_texture_units = 4;

    // Off: unlit, drawn by the PlaneRenderer preset shader
    bool lighting{ true };
    // sRGB, white: planes look unlit when there is no light
    glm::vec3 ambient{ 1.f };
    std::vector<PointLight::Shared> lights;
    void addLight(PointLight::Shared light);
    void removeLight(PointLight::Shared const& light);

    // Normal maps: OpenGL convention (green up) unless set
    bool normal_map_y_down{ false };
    void setNormalMap(std::shared_ptr<SSS::GL::PlaneBase> const& plane,
        SSS::GL::Texture::Shared normal_map);
    SSS::GL::Texture::Shared getNormalMap(std::shared_ptr<SSS::GL::PlaneBase> const& plane) const;
    // 0: unlit (e.g. a sky), 1: fully lit (default)
    void setLightingFactor(std::shared_ptr<SSS::GL::PlaneBase> const& plane, float factor);
    float getLightingFactor(std::shared_ptr<SSS::GL::PlaneBase> const& plane) const;

private:
    struct ScrollLayer {
        std::vector<std::shared_ptr<SSS::GL::PlaneBase>> planes;
        float width{ 0.f };
        bool is_setup{ false };
    };
    std::vector<ScrollLayer> _scroll_layers;
    bool _play{ false };
    std::chrono::steady_clock::time_point _last_update{ std::chrono::steady_clock::now() };

    static void _setupLayer(ScrollLayer& layer);
    void _moveLayers();
    void _sortPlanes();

    struct PlaneLighting {
        SSS::GL::Texture::Shared normal_map;
        float factor{ 1.f };
    };
    std::unordered_map<SSS::GL::PlaneBase const*, PlaneLighting> _plane_lighting;

    SSS::GL::Shaders::Shared _lit_shader;
    std::filesystem::file_time_type _vert_write_time, _frag_write_time;
    std::chrono::steady_clock::time_point _last_shader_check;
    bool _updateLitShader();
    void _renderLit(SSS::GL::Shaders& shader);
    static bool _isDrawn(std::shared_ptr<SSS::GL::PlaneBase> const& plane);

    // GPU data, std430 layouts of lit_plane.frag
    struct GPUInstance {
        GLint texture_unit;     // -1: no texture, discarded
        GLint normal_unit;      // -1: flat
        GLint uv_mode;
        GLint grayscale;
        glm::vec2 uv_offset;
        float lighting;
        float _padding;
    };
    struct GPULight {
        glm::vec4 position_radius;
        glm::vec4 color_falloff;        // linear, intensity applied
        glm::vec4 terminator_width;     // linear, intensity applied
        glm::vec4 shadow;               // linear, intensity applied
    };
    static_assert(sizeof(GPUInstance) == 32 && sizeof(GPULight) == 64);

    // Instanced draw: instances [first, first + count) and their textures,
    // bound to units [0, textures.size())
    struct Batch {
        uint32_t first{ 0 };
        uint32_t count{ 0 };
        std::vector<SSS::GL::Texture*> textures;
    };
    std::vector<Batch> _batches;
    std::vector<GPUInstance> _gpu_instances;
    void _buildBatches();

    std::vector<GPULight> _gpu_lights;
    int _tiles_x{ 1 };
    std::vector<std::vector<uint32_t>> _tile_lights;
    std::vector<glm::uvec2> _tile_ranges;   // offset & count in _light_indices
    std::vector<uint32_t> _light_indices;
    void _cullLights(glm::ivec4 const& viewport);

    // Instances, lights, tile ranges, light indices
    std::array<GLuint, 4> _ssbos{};

public:
    ~SceneRenderer();
};
