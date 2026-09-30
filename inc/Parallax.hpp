#pragma once

#include "includes.hpp"

class Parallax : public SSS::GL::PlaneRenderer
{
private:
    Parallax() = default;

public:
    using Shared = std::shared_ptr<Parallax>;
    static Shared create(SSS::GL::Camera::Shared camera = nullptr,
        bool clear_depth_buffer = false);

    std::vector<SSS::GL::Plane::Shared> getPlanes() const;
    void setPlanes(std::vector<SSS::GL::Plane::Shared> planes);

    void render() override;
    float getWidth() const noexcept { return _width; };

    void play();
    void pause();
    void toggle();

    inline bool isPlaying() const noexcept { return _play; };

    // In coordinates per second
    float speed{ 100.f };

private:
    bool _is_setup{ false };
    float _width{ 0 };
    bool _play{ false };
    std::chrono::steady_clock::time_point _last_update;
    void _setupPlanes();
    void _movePlanes();
};