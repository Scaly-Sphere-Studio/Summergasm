#include "Parallax.hpp"

void Parallax::_setupPlanes()
{
    if (_is_setup)
        return;
    for (auto& plane : planes) {
        auto const tex = plane->getTexture();
        if (!tex || tex->hasRunningThread())
            return;
    }

    _width = 0.f;
    float offset = 0.f;

    for (auto& plane : planes) {
        int w, h;
        plane->getTexture()->getCurrentDimensions(w, h);
        float const p_width = plane->getScaling().x *
            (static_cast<float>(w) / static_cast<float>(h));
        if (offset == 0.f) {
            offset = p_width / 2.f;
            _width -= offset;
        }
        plane->translate(glm::vec3(_width + p_width / 2, 0, 0));
        _width += p_width;
    }
    _width += offset;

    _is_setup = true;
    _last_update = std::chrono::steady_clock::now();
}

void Parallax::_movePlanes()
{
    if (!_is_setup || !_play)
        return;
    auto const now = std::chrono::steady_clock::now();
    std::chrono::duration<float> const diff = now - _last_update;
    float const coeff = -speed * diff.count();
    for (auto& plane : planes) {
        plane->translate(glm::vec3(coeff, 0.f, 0.f));
        if ((coeff < 0.f && plane->getTranslation().x < -_width / 2.f) ||
            (coeff > 0.f && plane->getTranslation().x > _width / 2.f)) {
            plane->translate(glm::vec3(coeff <= 0.f ? _width : -_width, 0.f, 0.f));
        }
    }
    _last_update = now;
}

void Parallax::render()
{
    _setupPlanes();
    _movePlanes();
    PlaneRendererBase::render();
}

void Parallax::pause()
{
    _play = false;
}

void Parallax::toggle()
{
    if (_play)
        pause();
    else
        play();
}

void Parallax::play()
{
    _play = true;
    _last_update = std::chrono::steady_clock::now();
}