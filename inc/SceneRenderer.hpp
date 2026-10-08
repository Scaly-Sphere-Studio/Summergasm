#pragma once

#include "includes.hpp"

// Point light of a SceneRenderer. Colors are sRGB, as picked; the lighting
// itself is computed in linear space.
// Diffuse is a wrapped Lambert: the light reaches terminator_width past the
// terminator (N.L = 0), and its color lerps from terminator_color at the
// terminator to color at N.L = terminator_width. This fakes subsurface
// scattering or a colored terminator without stacking lights.
// shadow_color fills the unlit part of the light's area (black: classic),
// including its cast shadows, whose penumbras also take the terminator color.
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

    // When set, the light moves with this plane: position = the plane's
    // translation + follow_offset, updated after the scroll layers moved
    std::weak_ptr<SSS::GL::PlaneBase> follow;
    glm::vec3 follow_offset{ 0.f };

    // Cast shadows (see SceneRenderer::max_shadow_lights). The shadow map is
    // a perspective view from the light, aimed at and fitted to what the
    // camera sees when it's entirely on one side of the light. Otherwise,
    // it's the cone toward shadow_direction: only what's inside gets shadows.
    bool cast_shadows{ false };
    glm::vec3 shadow_direction{ 0.f, 0.f, -1.f };
    // Maximum cone angle, in degrees
    float shadow_fov{ 120.f };
    // Offset of the receivers toward the light, in world units, against
    // self-shadowing (planes 1 unit apart need < 1)
    float shadow_bias{ 0.5f };
    // Minimum penumbra size, in shadow map texels
    float shadow_softness{ 1.5f };
    // Radius of the light source, in world units (0: point light). Penumbras
    // widen with it and with the distance between casters and receivers:
    // sharp where a caster touches the receiver, softer farther away.
    float size{ 0.f };
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
// Lights can cast colored shadows: opaque texels block the light, and
// translucent ones (see setShadowTransmission) tint it with their color.
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
    ~SceneRenderer();

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

    // Folder of the lit_plane & shadow_plane shaders, set at startup
    static inline std::string shaders_folder;
    // Screen tiles of the light culling, in pixels
    static constexpr int tile_size = 32;
    // Last texture units, kept for the shadow maps
    static constexpr uint32_t reserved_texture_units = 4;
    // Shadow casting lights per frame (the first ones lighting the screen)
    static constexpr int max_shadow_lights = 8;

    // Off: unlit, drawn by the PlaneRenderer preset shader
    bool lighting{ true };
    // sRGB, white: planes look unlit when there is no light
    glm::vec3 ambient{ 1.f };
    std::vector<PointLight::Shared> lights;
    void addLight(PointLight::Shared light);
    void removeLight(PointLight::Shared const& light);

    // Off: no light casts shadows
    bool shadows{ true };
    // Width & height of each light's shadow map, in texels
    int shadow_map_size{ 1024 };
    // Shadow maps cover the camera's view, enlarged by this ratio to catch
    // shadows of casters just outside of it
    float shadow_fit_margin{ 0.25f };

    // Normal maps: OpenGL convention (green up) unless set
    bool normal_map_y_down{ false };
    void setNormalMap(std::shared_ptr<SSS::GL::PlaneBase> const& plane,
        SSS::GL::Texture::Shared normal_map);
    SSS::GL::Texture::Shared getNormalMap(std::shared_ptr<SSS::GL::PlaneBase> const& plane) const;
    // 0: unlit (e.g. a sky), 1: fully lit (default)
    void setLightingFactor(std::shared_ptr<SSS::GL::PlaneBase> const& plane, float factor);
    float getLightingFactor(std::shared_ptr<SSS::GL::PlaneBase> const& plane) const;
    // Whether the plane casts shadows (default: true)
    void setCastShadow(std::shared_ptr<SSS::GL::PlaneBase> const& plane, bool cast);
    bool getCastShadow(std::shared_ptr<SSS::GL::PlaneBase> const& plane) const;
    // How much light goes through the plane's texels, tinted by their color:
    // 0 = opaque (default), 1 = colored glass
    void setShadowTransmission(std::shared_ptr<SSS::GL::PlaneBase> const& plane, float transmission);
    float getShadowTransmission(std::shared_ptr<SSS::GL::PlaneBase> const& plane) const;

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
        bool cast_shadow{ true };
        float shadow_transmission{ 0.f };
    };
    std::unordered_map<SSS::GL::PlaneBase const*, PlaneLighting> _plane_lighting;

    // <name>.vert & <name>.frag of shaders_folder, reloaded when edited
    struct HotShaders {
        std::string name;
        SSS::GL::Shaders::Shared shaders;
        std::filesystem::file_time_type vert_time, frag_time;
        std::chrono::steady_clock::time_point last_check;
        // Returns whether the shaders are usable
        bool update();
    };
    HotShaders _lit_shaders{ "lit_plane" };
    HotShaders _shadow_shaders{ "shadow_plane" };

    void _renderLit(SSS::GL::Shaders& shader);
    static bool _isDrawn(std::shared_ptr<SSS::GL::PlaneBase> const& plane);
    void _updateVBOs();
    // Instanced draws of every batch (textures bound, u_BaseInstance set)
    void _drawBatches(SSS::GL::Shaders& shader) const;
    // Texture unit IDs uniform, for every shader sampling the planes
    static void _setTextureUnits(SSS::GL::Shaders& shader);

    // GPU data, std430 layouts of lit_plane.frag & shadow_plane.frag
    struct GPUInstance {
        GLint texture_unit;     // -1: no texture, discarded
        GLint normal_unit;      // -1: flat
        GLint uv_mode;
        GLint grayscale;
        glm::vec2 uv_offset;
        float lighting;
        float shadow_transmission;
        GLint cast_shadow;
        GLint _padding;
    };
    struct GPULight {
        glm::vec4 position_radius;
        glm::vec4 color_falloff;        // linear, intensity applied
        glm::vec4 terminator_width;     // linear, intensity applied
        glm::vec4 shadow;               // linear, intensity applied
        glm::mat4 shadow_vp;            // light's view projection
        glm::vec4 shadow_params;        // layer (-1: none), bias, softness
        // Shadow map's z_near, z_far, and light size in uv * depth (x, y)
        glm::vec4 shadow_frustum;
    };
    static_assert(sizeof(GPUInstance) == 40 && sizeof(GPULight) == 160);

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
    void _followPlanes();

    // Corners of the camera's view between the nearest and farthest lit or
    // shadow casting planes (with shadow_fit_margin), fitted by shadow maps.
    // Returns false without camera or planes.
    bool _computeViewSlab(std::array<glm::vec3, 8>& corners) const;
    // Also returns the frustum's z_near, z_far & light size in uv * depth
    glm::mat4 _shadowViewProjection(PointLight const& light, bool has_slab,
        std::array<glm::vec3, 8> const& slab, glm::vec4& frustum) const;

    // Instances, lights, tile ranges, light indices
    std::array<GLuint, 4> _ssbos{};

    // Shadow maps: one layer per shadow casting light. Depth of the opaque
    // casters, and RGB transmittance of the translucent ones with, in alpha,
    // the distance (/ radius) of the nearest one to the light. The
    // transmittance only exists when a caster has a shadow transmission,
    // else receivers sample a white 1x1 layer.
    GLuint _shadow_fbo{ 0 };
    GLuint _shadow_depth{ 0 };
    GLuint _shadow_color{ 0 };
    GLuint _shadow_white{ 0 };
    // Reads the depth layers without comparison (blocker search)
    GLuint _shadow_depth_sampler{ 0 };
    int _shadow_size{ 0 };
    int _shadow_layers{ 0 };
    int _shadow_color_layers{ 0 };
    bool _translucent_casters{ false };
    void _allocateShadowMaps(int layers);
    void _renderShadowMaps();
};
