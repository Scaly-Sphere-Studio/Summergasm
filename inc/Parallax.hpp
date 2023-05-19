#pragma once

#include "includes.hpp"

class Parallax : public SSS::GL::PlaneRendererTemplate<Parallax>
{
    friend class SSS::GL::Basic::SharedBase<Parallax>;

private:
    Parallax() = default;

public:
    void render() override;
    float getWidth() const noexcept { return _width; };

    // In coordinates per second
    float speed{ 100.f };

private:
    bool _is_setup{ false };
    float _width{ 0 };
    std::chrono::steady_clock::time_point _last_update;
    void _setupPlanes();
    void _movePlanes();
};