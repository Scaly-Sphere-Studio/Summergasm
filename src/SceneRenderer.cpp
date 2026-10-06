#include "SceneRenderer.hpp"

SceneRenderer::Shared SceneRenderer::create(SSS::GL::Camera::Shared camera)
{
    Shared renderer(new SceneRenderer());
    renderer->camera = std::move(camera);
    return renderer;
}

void SceneRenderer::addScrollLayer(std::vector<SSS::GL::Plane::Shared> const& planes)
{
    ScrollLayer layer;
    layer.planes.reserve(planes.size());
    for (auto const& plane : planes) {
        if (!plane)
            continue;
        addPlane(plane);
        layer.planes.emplace_back(plane);
    }
    if (!layer.planes.empty())
        _scroll_layers.emplace_back(std::move(layer));
}

void SceneRenderer::clearScrollLayers()
{
    _scroll_layers.clear();
}

void SceneRenderer::_setupLayer(ScrollLayer& layer)
{
    // Wait for every texture to be loaded
    for (auto const& plane : layer.planes) {
        auto const tex = plane->getTexture();
        if (!tex)
            return;
        int w, h;
        tex->getCurrentDimensions(w, h);
        if (w == 0 || h == 0)
            return;
    }

    // Line the tiles up along x, the first one centered on x = 0
    layer.width = 0.f;
    float offset = 0.f;
    for (auto const& plane : layer.planes) {
        int w, h;
        plane->getTexture()->getCurrentDimensions(w, h);
        float const p_width = plane->getScaling().x *
            (static_cast<float>(w) / static_cast<float>(h));
        if (offset == 0.f) {
            offset = p_width / 2.f;
            layer.width -= offset;
        }
        plane->translate(glm::vec3(layer.width + p_width / 2.f, 0.f, 0.f));
        layer.width += p_width;
    }
    layer.width += offset;

    layer.is_setup = true;
}

void SceneRenderer::_moveLayers()
{
    auto const now = std::chrono::steady_clock::now();
    std::chrono::duration<float> const diff = now - _last_update;
    _last_update = now;
    if (!_play)
        return;

    float const coeff = -scroll_speed * diff.count();
    for (auto& layer : _scroll_layers) {
        if (!layer.is_setup)
            continue;
        float const half_width = layer.width / 2.f;
        for (auto const& plane : layer.planes) {
            plane->translate(glm::vec3(coeff, 0.f, 0.f));
            float const x = plane->getTranslation().x;
            if (coeff < 0.f && x < -half_width)
                plane->translate(glm::vec3(layer.width, 0.f, 0.f));
            else if (coeff > 0.f && x > half_width)
                plane->translate(glm::vec3(-layer.width, 0.f, 0.f));
        }
    }
}

void SceneRenderer::_sortPlanes()
{
    if (!depth_sort || !camera || _planes.size() < 2)
        return;

    // View-space z of each plane's center: the lower, the farther
    glm::mat4 const view = camera->getView();
    std::vector<std::pair<float, std::shared_ptr<SSS::GL::PlaneBase>>> keyed;
    keyed.reserve(_planes.size());
    for (auto const& plane : _planes)
        keyed.emplace_back((view * plane->getModelMat4()[3]).z, plane);

    auto const farther = [](auto const& a, auto const& b) { return a.first < b.first; };
    if (std::is_sorted(keyed.cbegin(), keyed.cend(), farther))
        return;
    // Stable: planes at the same depth keep their insertion order
    std::stable_sort(keyed.begin(), keyed.end(), farther);
    for (size_t i = 0; i < keyed.size(); ++i)
        _planes[i] = std::move(keyed[i].second);
    _update_vbos = true;
}

void SceneRenderer::render()
{
    if (!isActive())
        return;
    for (auto& layer : _scroll_layers) {
        if (!layer.is_setup)
            _setupLayer(layer);
    }
    _moveLayers();
    _sortPlanes();
    SSS::GL::PlaneRenderer::render();
}

void SceneRenderer::pause()
{
    _play = false;
}

void SceneRenderer::toggle()
{
    if (_play)
        pause();
    else
        play();
}

void SceneRenderer::play()
{
    _play = true;
    _last_update = std::chrono::steady_clock::now();
}
