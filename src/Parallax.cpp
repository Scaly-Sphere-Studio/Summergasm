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

    for (auto& plane : planes) {
        int w, h;
        plane->getTexture()->getCurrentDimensions(w, h);
        plane->setScaling(glm::vec3(h));
        plane->setTranslation(glm::vec3(_width, 0, 0));
        _width += w - 1;
    }

    _is_setup = true;
    _last_update = std::chrono::steady_clock::now();
    LOG_CTX_MSG("width", _width)
}

void Parallax::_movePlanes()
{
    if (!_is_setup)
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
