#pragma once

#include "includes.hpp"
#include "FoldingMap.hpp"

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
    // Share of the ambient light hidden by the cast shadows, when the light
    // reaching them is at full strength (1, linear); less for dimmer lights.
    // 0: shadows only remove this light's own contribution
    float shadow_ambient{ 1.f };
    // Radius of the light source, in world units (0: point light). Penumbras
    // widen with it and with the distance between casters and receivers:
    // sharp where a caster touches the receiver, softer farther away.
    float size{ 0.f };
};

// Renders a whole 3D scene of planes seen through a (perspective) camera.
// Lit planes are drawn in two passes: their opaque texels first, in any
// order, hidden by depth alone (planes of any size, tilted or not); then the
// translucent ones (e.g. sprites' edges), back to front from the camera,
// blended over them. Unlit, planes are only drawn back to front.
// Scroll layers are sets of tiles lined up along x that scroll and wrap
// together, all at the same world speed: the visible parallax only comes
// from their depth.
// Planes are lit by any number of point lights (see PointLight), culled per
// screen tile, with optional per-plane normal & emissive maps, through
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

    // Also forgets the plane's settings below (lighting, maps, gradient...)
    void removePlane(std::shared_ptr<SSS::GL::PlaneBase> plane);

    // Meshes (see FoldingMap): opaque, lit only (not drawn with lighting
    // off), drawn after the planes' opaque texels through
    // resources/shaders/lit_mesh.vert & lit_plane.frag, and casting shadows
    // through shadow_mesh.vert & shadow_plane.frag. Depth of field blurs them
    // inside their own uv (as blur clamped planes).
    void addMesh(FoldingMap::Shared mesh);
    void removeMesh(FoldingMap::Shared const& mesh);
    inline std::vector<FoldingMap::Shared> const& getMeshes() const noexcept { return _meshes; };

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
    // Texels at least this opaque are drawn in the first, opaque pass, the
    // others in the translucent pass (see above)
    float opaque_cutoff{ 0.99f };
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

    // Depth of field (lit planes only, see lighting). Planes are sharp inside
    // the focus band [near, far], distances from the camera along its view
    // axis, and blurred outside of it, the more the farther from it (thin
    // lens): by focus_blur * (1 - far / d) pixels behind it, reaching
    // focus_blur at infinity, and by focus_blur * (near / d - 1) in front of
    // it, up to focus_max_blur. Each plane blurs its own texture: the blur
    // doesn't spread past the plane's edges.
    // Everything sharp: a band from 0 to FLT_MAX. Everything blurred: a band
    // in front of every plane, e.g. setFocus(0, 0).
    bool depth_of_field{ false };
    // In pixels
    float focus_blur{ 12.f };
    float focus_max_blur{ 24.f };
    // Band of band_width centered on distance
    void setFocus(float distance, float band_width);
    // Moves the whole band by delta, away from the camera if positive
    void moveFocus(float delta);
    // Each pushes the other bound if needed
    void setFocusNear(float distance);
    void setFocusFar(float distance);
    // In seconds: the band lerps from where it is (even mid-transition) to
    // the one set. Immediate before the first render. An infinite far bound
    // lerps on 1 / far, as the blur behind the band does.
    float focus_transition{ 0.5f };
    // The band set, the one shown being mid-transition (see getCurrentFocus)
    inline float getFocusNear() const noexcept { return _focus_near; };
    inline float getFocusFar() const noexcept { return _focus_far; };
    inline float getFocusDistance() const noexcept { return _focus_near / 2.f + _focus_far / 2.f; };
    inline float getFocusBandWidth() const noexcept { return _focus_far - _focus_near; };
    // The band shown: near & far
    glm::vec2 getCurrentFocus() const;
    // Blurred planes grow by their blur radius, the blur spreading past their
    // edges and fading out. Clamped, the blur stays inside the plane, its
    // edges sharp but never transparent: for planes reaching the screen's
    // edges or tiled side by side. Default: clamped for scroll layer tiles
    void setBlurClamp(std::shared_ptr<SSS::GL::PlaneBase> const& plane, bool clamp);
    bool getBlurClamp(std::shared_ptr<SSS::GL::PlaneBase> const& plane) const;
    // While a dialog is shown (see Dialog::anyVisible), everything is blurred
    // by focus_blur, even with depth_of_field off: fades in and back out to
    // the settings above in focus_transition, which it leaves untouched
    bool dialog_blur{ true };
    // 0: the settings above, 1: everything blurred
    float getDialogBlur() const;

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
    // Emissive maps: their texels are added after lighting (not shadowed),
    // times emissive color & intensity, masked by the plane's own alpha
    void setEmissiveMap(std::shared_ptr<SSS::GL::PlaneBase> const& plane,
        SSS::GL::Texture::Shared emissive_map);
    SSS::GL::Texture::Shared getEmissiveMap(std::shared_ptr<SSS::GL::PlaneBase> const& plane) const;
    // sRGB, white by default
    void setEmissiveColor(std::shared_ptr<SSS::GL::PlaneBase> const& plane, glm::vec3 const& color);
    glm::vec3 getEmissiveColor(std::shared_ptr<SSS::GL::PlaneBase> const& plane) const;
    // 1 by default
    void setEmissiveIntensity(std::shared_ptr<SSS::GL::PlaneBase> const& plane, float intensity);
    float getEmissiveIntensity(std::shared_ptr<SSS::GL::PlaneBase> const& plane) const;

    // Gradient maps: rows of a 256x256 atlas, one gradient (sRGB RGBA) per row,
    // evaluated at 256 points. A plane with a gradient has its texture's
    // luminance mapped to the row's colors, alpha multiplied (e.g. greyscale
    // assets tinted in many color schemes, sharing one texture).
    // Returns the gradient's index, -1 when the atlas is full
    static constexpr int max_gradients = 256;
    int addGradient(SSS::Math::Gradient<glm::vec4> gradient);
    void setGradientStops(int index, SSS::Math::Gradient<glm::vec4> gradient);
    void clearGradients();
    inline int getGradientCount() const noexcept { return static_cast<int>(_gradients.size()); };
    // -1: none (default)
    void setGradient(std::shared_ptr<SSS::GL::PlaneBase> const& plane, int index);
    int getGradient(std::shared_ptr<SSS::GL::PlaneBase> const& plane) const;

    // Caustics maps: tiled over the plane by tiles of caustics_tile world
    // units, two scrolling copies combined (min) for a moving light pattern.
    // Added after lighting like the emissive maps, times color & intensity,
    // full at the waterline (world y) and fading out over fade world units
    // above it, none below it. The map is set to repeat.
    void setCausticsMap(std::shared_ptr<SSS::GL::PlaneBase> const& plane,
        SSS::GL::Texture::Shared caustics_map);
    SSS::GL::Texture::Shared getCausticsMap(std::shared_ptr<SSS::GL::PlaneBase> const& plane) const;
    // sRGB, white by default
    void setCausticsColor(std::shared_ptr<SSS::GL::PlaneBase> const& plane, glm::vec3 const& color);
    glm::vec3 getCausticsColor(std::shared_ptr<SSS::GL::PlaneBase> const& plane) const;
    // 1 by default
    void setCausticsIntensity(std::shared_ptr<SSS::GL::PlaneBase> const& plane, float intensity);
    float getCausticsIntensity(std::shared_ptr<SSS::GL::PlaneBase> const& plane) const;
    // World size of a tile, 100 by default
    void setCausticsTile(std::shared_ptr<SSS::GL::PlaneBase> const& plane, float tile);
    float getCausticsTile(std::shared_ptr<SSS::GL::PlaneBase> const& plane) const;
    // Waterline's world y (0) and fade height (100)
    void setCausticsWaterline(std::shared_ptr<SSS::GL::PlaneBase> const& plane,
        float water_y, float fade);
    glm::vec2 getCausticsWaterline(std::shared_ptr<SSS::GL::PlaneBase> const& plane) const;
    // In tiles per second, the same for every plane
    float caustics_speed{ 0.05f };

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

    float _focus_near{ 0.f };
    float _focus_far{ FLT_MAX };
    // Band at the start of the transition (near & far)
    glm::vec2 _focus_from{ 0.f, FLT_MAX };
    std::chrono::steady_clock::time_point _focus_start;
    bool _rendered{ false };
    void _setFocus(float near_dist, float far_dist);
    // Dialog blur: on or fading in, and the amount at the start of its fade
    bool _dialog_blur_on{ false };
    float _dialog_blur_from{ 0.f };
    std::chrono::steady_clock::time_point _dialog_blur_start;
    void _updateDialogBlur();

    struct PlaneLighting {
        SSS::GL::Texture::Shared normal_map;
        float factor{ 1.f };
        bool cast_shadow{ true };
        float shadow_transmission{ 0.f };
        SSS::GL::Texture::Shared emissive_map;
        glm::vec3 emissive_color{ 1.f };
        float emissive_intensity{ 1.f };
        // Unset: whether the plane is a scroll layer tile
        std::optional<bool> blur_clamp;
        int gradient{ -1 };
        SSS::GL::Texture::Shared caustics_map;
        glm::vec3 caustics_color{ 1.f };
        float caustics_intensity{ 1.f };
        float caustics_tile{ 100.f };
        float caustics_water_y{ 0.f };
        float caustics_fade{ 100.f };
    };
    bool _isScrollTile(SSS::GL::PlaneBase const* plane) const;
    std::unordered_map<SSS::GL::PlaneBase const*, PlaneLighting> _plane_lighting;

    // <name>.vert & <frag_name>.frag (<name>.frag if empty) of
    // shaders_folder, reloaded when edited
    struct HotShaders {
        std::string name;
        std::string frag_name;
        SSS::GL::Shaders::Shared shaders;
        std::filesystem::file_time_type vert_time, frag_time;
        std::chrono::steady_clock::time_point last_check;
        // Returns whether the shaders are usable
        bool update();
        // The driver's compile & link messages of these sources
        static std::string _diagnose(std::string const& vert, std::string const& frag);
    };
    HotShaders _lit_shaders{ "lit_plane" };
    HotShaders _shadow_shaders{ "shadow_plane" };
    HotShaders _lit_mesh_shaders{ "lit_mesh", "lit_plane" };
    HotShaders _shadow_mesh_shaders{ "shadow_mesh", "shadow_plane" };

    std::vector<FoldingMap::Shared> _meshes;
    // Instances of the drawn meshes, after the planes' (-1: none). Their
    // front texture is bound to unit 0, their back one to unit 1, their
    // front layers to units 2 to 4, their overlays to units 5 to 7.
    struct MeshDraw {
        FoldingMap* mesh;
        GLint front_instance;
        GLint back_instance;
    };
    std::vector<MeshDraw> _mesh_draws;
    // Lit: front & back faces culled apart, each with its instance. Shadows:
    // both at once, with the front instance. Binds the mesh's VAOs, leaving
    // none bound.
    void _drawMeshes(SSS::GL::Shaders& shader, bool shadow) const;

    std::vector<SSS::Math::Gradient<glm::vec4>> _gradients;
    SSS::GL::Texture::Shared _gradient_atlas;
    bool _gradients_dirty{ false };
    // Rebuilds the atlas if a gradient changed (needs the GL context)
    void _updateGradientAtlas();
    // Caustics clock
    std::chrono::steady_clock::time_point const _start_time{ std::chrono::steady_clock::now() };

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
        GLint emissive_unit;    // -1: none
        GLint blur_clamp;       // 1: the blur stays inside the plane
        GLint caustics_unit;    // -1: none
        glm::vec4 emissive;     // linear, intensity applied
        glm::vec4 caustics;     // linear, intensity applied
        glm::vec4 caustics_params;  // tiles along x & y, waterline y, fade height
        GLint gradient;         // atlas row, -1: none
        // Meshes' front layers (see FoldingMap::layers), composited over the
        // texture in order, -1: none
        GLint layer_units[3]{ -1, -1, -1 };
        // Their overlays (see FoldingMap::overlays), each over its layer, on
        // its uv rect, -1: none
        GLint overlay_units[3]{ -1, -1, -1 };
        GLint _padding0{ 0 };
        glm::vec4 overlay_rects[3]{};
    };
    struct GPULight {
        glm::vec4 position_radius;
        glm::vec4 color_falloff;        // linear, intensity applied
        glm::vec4 terminator_width;     // linear, intensity applied
        glm::vec4 shadow;               // linear, intensity applied; w: edge fade (uv)
        glm::mat4 shadow_vp;            // light's view projection
        glm::vec4 shadow_params;        // layer (-1: none), bias, softness, ambient hidden
        // Shadow map's z_near, z_far, and light size in uv * depth (x, y)
        glm::vec4 shadow_frustum;
    };
    static_assert(sizeof(GPUInstance) == 176 && sizeof(GPULight) == 160);

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
    // Also returns the frustum's z_near, z_far & light size in uv * depth, and
    // the width of the shadows' fade along its sides (0: none, view covered)
    glm::mat4 _shadowViewProjection(PointLight const& light, bool has_slab,
        std::array<glm::vec3, 8> const& slab, glm::vec4& frustum, float& edge_fade) const;

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
