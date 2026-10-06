#pragma once

#include "includes.hpp"

// Renders a whole 3D scene of planes seen through a (perspective) camera.
// Planes are drawn back to front from the camera, so transparent sprites
// blend correctly within a single renderer and a single depth buffer.
// Scroll layers are sets of tiles lined up along x that scroll and wrap
// together, all at the same world speed: the visible parallax only comes
// from their depth.
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
};
