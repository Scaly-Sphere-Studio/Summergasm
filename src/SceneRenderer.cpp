#include "SceneRenderer.hpp"
#include <fstream>

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

void SceneRenderer::addLight(PointLight::Shared light)
{
    if (light && std::find(lights.cbegin(), lights.cend(), light) == lights.cend())
        lights.emplace_back(std::move(light));
}

void SceneRenderer::removeLight(PointLight::Shared const& light)
{
    std::erase(lights, light);
}

void SceneRenderer::setNormalMap(std::shared_ptr<SSS::GL::PlaneBase> const& plane,
    SSS::GL::Texture::Shared normal_map)
{
    if (plane)
        _plane_lighting[plane.get()].normal_map = std::move(normal_map);
}

SSS::GL::Texture::Shared SceneRenderer::getNormalMap(
    std::shared_ptr<SSS::GL::PlaneBase> const& plane) const
{
    auto const it = _plane_lighting.find(plane.get());
    return it == _plane_lighting.cend() ? nullptr : it->second.normal_map;
}

void SceneRenderer::setLightingFactor(std::shared_ptr<SSS::GL::PlaneBase> const& plane,
    float factor)
{
    if (plane)
        _plane_lighting[plane.get()].factor = std::clamp(factor, 0.f, 1.f);
}

float SceneRenderer::getLightingFactor(std::shared_ptr<SSS::GL::PlaneBase> const& plane) const
{
    auto const it = _plane_lighting.find(plane.get());
    return it == _plane_lighting.cend() ? 1.f : it->second.factor;
}

static std::string read_file(std::filesystem::path const& path)
{
    std::ifstream ifs(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
}

// Loads the lit shader, and reloads it when its files are edited (checked
// twice per second). A shader that fails to compile is logged by SSS::GL and
// the previous one is kept. Returns whether a lit shader is usable.
bool SceneRenderer::_updateLitShader()
{
    auto const now = std::chrono::steady_clock::now();
    if (now - _last_shader_check < std::chrono::milliseconds(500))
        return _lit_shader != nullptr;
    _last_shader_check = now;

    std::filesystem::path const vert = shaders_folder + "lit_plane.vert";
    std::filesystem::path const frag = shaders_folder + "lit_plane.frag";
    std::error_code ec_v, ec_f;
    auto const vert_time = std::filesystem::last_write_time(vert, ec_v);
    auto const frag_time = std::filesystem::last_write_time(frag, ec_f);
    // Missing files (or mid-save): keep the current state
    if (ec_v || ec_f)
        return _lit_shader != nullptr;
    // Unchanged files: keep the current state (even a failed one)
    if (vert_time == _vert_write_time && frag_time == _frag_write_time)
        return _lit_shader != nullptr;
    _vert_write_time = vert_time;
    _frag_write_time = frag_time;

    auto shader = SSS::GL::Shaders::create();
    shader->loadFromStrings(read_file(vert), read_file(frag));
    // Failed programs stay at id 0, which has no uniform
    if (shader->getUniformLocation("u_VP") == -1) {
        LOG_ERR("SceneRenderer: lit_plane shaders failed to load, see the log above");
        return _lit_shader != nullptr;
    }
    _lit_shader = std::move(shader);
    return true;
}

// sRGB color to linear, times intensity
static glm::vec3 to_linear(glm::vec3 const& color, float intensity = 1.f)
{
    return glm::pow(glm::max(color, glm::vec3(0.f)), glm::vec3(2.2f)) * intensity;
}

SceneRenderer::~SceneRenderer()
{
    if (_ssbos[0] != 0)
        glDeleteBuffers(static_cast<GLsizei>(_ssbos.size()), _ssbos.data());
}

bool SceneRenderer::_isDrawn(std::shared_ptr<SSS::GL::PlaneBase> const& plane)
{
    return !plane->isHidden() && plane->sdf_mode == SSS::GL::PlaneBase::SDFMode::None;
}

// Lights are culled per screen tile (tile_size pixels): each light's bounding
// sphere is projected on screen, and its index is added to every tile its
// rectangle overlaps. Fragments only evaluate the lights of their tile.
void SceneRenderer::_cullLights(glm::ivec4 const& viewport)
{
    _tiles_x = std::max(1, (viewport.z + tile_size - 1) / tile_size);
    int const tiles_y = std::max(1, (viewport.w + tile_size - 1) / tile_size);
    _tile_lights.resize(static_cast<size_t>(_tiles_x) * tiles_y);
    for (auto& tile : _tile_lights)
        tile.clear();
    _gpu_lights.clear();

    glm::mat4 const view = camera ? camera->getView() : glm::mat4(1);
    glm::mat4 const proj = camera ? camera->getProjection() : glm::mat4(1);
    float const z_near = camera ? camera->getZNear() : 0.f;

    for (auto const& light : lights) {
        if (!light || !light->enabled || light->radius <= 0.f)
            continue;

        // Screen rectangle of the light, in NDC (full screen without camera)
        glm::vec2 ndc_min(-1.f), ndc_max(1.f);
        if (camera) {
            glm::vec3 const c(view * glm::vec4(light->position, 1.f));
            float const r = light->radius;
            // The camera looks down -z: visible points have z < -z_near
            if (c.z - r >= -z_near)
                continue;
            // Else the sphere crosses the near plane: full screen
            if (c.z + r < -z_near) {
                ndc_min = glm::vec2(FLT_MAX);
                ndc_max = glm::vec2(-FLT_MAX);
                for (int i = 0; i < 8; ++i) {
                    glm::vec3 const corner = c + r * glm::vec3(
                        (i & 1) ? 1.f : -1.f, (i & 2) ? 1.f : -1.f, (i & 4) ? 1.f : -1.f);
                    glm::vec4 const clip = proj * glm::vec4(corner, 1.f);
                    glm::vec2 const ndc = glm::vec2(clip) / clip.w;
                    ndc_min = glm::min(ndc_min, ndc);
                    ndc_max = glm::max(ndc_max, ndc);
                }
                if (ndc_max.x < -1.f || ndc_max.y < -1.f || ndc_min.x > 1.f || ndc_min.y > 1.f)
                    continue;
                ndc_min = glm::clamp(ndc_min, -1.f, 1.f);
                ndc_max = glm::clamp(ndc_max, -1.f, 1.f);
            }
        }

        auto const index = static_cast<uint32_t>(_gpu_lights.size());
        _gpu_lights.push_back({
            glm::vec4(light->position, light->radius),
            glm::vec4(to_linear(light->color, light->intensity), light->falloff),
            glm::vec4(to_linear(light->terminator_color, light->intensity),
                std::max(light->terminator_width, 0.f)),
            glm::vec4(to_linear(light->shadow_color, light->intensity), 0.f)
        });

        // NDC to tiles (origin at the bottom left, as gl_FragCoord)
        auto const to_tile = [](float ndc, int size, int tiles) {
            int const tile = static_cast<int>((ndc * 0.5f + 0.5f) * size) / tile_size;
            return std::clamp(tile, 0, tiles - 1);
        };
        int const x0 = to_tile(ndc_min.x, viewport.z, _tiles_x);
        int const x1 = to_tile(ndc_max.x, viewport.z, _tiles_x);
        int const y0 = to_tile(ndc_min.y, viewport.w, tiles_y);
        int const y1 = to_tile(ndc_max.y, viewport.w, tiles_y);
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x)
                _tile_lights[static_cast<size_t>(y) * _tiles_x + x].push_back(index);
        }
    }

    // Flatten: each tile is a range (offset, count) of _light_indices
    _tile_ranges.resize(_tile_lights.size());
    _light_indices.clear();
    for (size_t i = 0; i < _tile_lights.size(); ++i) {
        _tile_ranges[i] = glm::uvec2(_light_indices.size(), _tile_lights[i].size());
        _light_indices.insert(_light_indices.end(), _tile_lights[i].cbegin(), _tile_lights[i].cend());
    }
}

// Groups the drawn planes (in VBO order) into instanced draws. Planes sharing
// a texture share its texture unit: a new draw only starts when a plane needs
// more units than available, so many planes with few textures = one draw.
void SceneRenderer::_buildBatches()
{
    using SSS::GL::Texture;

    uint32_t const max_units = std::max(2u,
        SSS::GL::Window::maxGLSLTextureUnits() - reserved_texture_units);
    _gpu_instances.clear();
    _batches.clear();
    _batches.emplace_back();

    auto const is_missing = [](Batch const& batch, Texture const* texture) {
        return texture && std::find(batch.textures.cbegin(), batch.textures.cend(), texture)
            == batch.textures.cend();
    };
    auto const unit_of = [](Batch& batch, Texture* texture) {
        auto const it = std::find(batch.textures.cbegin(), batch.textures.cend(), texture);
        if (it != batch.textures.cend())
            return static_cast<GLint>(it - batch.textures.cbegin());
        batch.textures.push_back(texture);
        return static_cast<GLint>(batch.textures.size() - 1);
    };

    for (auto const& plane : _planes) {
        if (!_isDrawn(plane))
            continue;
        GPUInstance instance{};
        instance.texture_unit = -1;
        instance.normal_unit = -1;
        instance.lighting = 1.f;

        Texture* normal_map = nullptr;
        auto const it = _plane_lighting.find(plane.get());
        if (it != _plane_lighting.cend()) {
            instance.lighting = it->second.factor;
            int w = 0, h = 0;
            if (it->second.normal_map)
                it->second.normal_map->getCurrentDimensions(w, h);
            if (w != 0 && h != 0)
                normal_map = it->second.normal_map.get();
        }

        // Planes without texture keep their instance (discarded by the shader)
        if (auto const texture = plane->getTexture()) {
            Batch* batch = &_batches.back();
            size_t const needed = (is_missing(*batch, texture.get()) ? 1 : 0)
                + (is_missing(*batch, normal_map) ? 1 : 0);
            if (batch->textures.size() + needed > max_units) {
                _batches.push_back({ static_cast<uint32_t>(_gpu_instances.size()), 0, {} });
                batch = &_batches.back();
            }
            instance.texture_unit = unit_of(*batch, texture.get());
            if (normal_map)
                instance.normal_unit = unit_of(*batch, normal_map);
            instance.uv_mode = static_cast<GLint>(texture->getUVMode());
            instance.grayscale = texture->getGrayscale() ? 1 : 0;
            instance.uv_offset = texture->getUVOffset();
        }
        _gpu_instances.push_back(instance);
        ++_batches.back().count;
    }
}

static void upload_ssbo(GLuint ssbo, GLuint binding, GLsizeiptr size, void const* data)
{
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    // Orphan the previous storage; never empty, as 0-sized buffers can't be bound
    glBufferData(GL_SHADER_STORAGE_BUFFER, std::max<GLsizeiptr>(size, 16), nullptr, GL_DYNAMIC_DRAW);
    if (size > 0)
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, size, data);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, ssbo);
}

template <typename T>
static void upload_ssbo(GLuint ssbo, GLuint binding, std::vector<T> const& vec)
{
    upload_ssbo(ssbo, binding, static_cast<GLsizeiptr>(vec.size() * sizeof(T)), vec.data());
}

// Same VAO & instance VBOs as PlaneRenderer::render(), with the lit shader.
// Per-instance data, lights and light tiles are in SSBOs (bindings 1 to 4,
// see lit_plane.frag). SDF planes are not supported.
void SceneRenderer::_renderLit(SSS::GL::Shaders& shader)
{
    using SSS::GL::PlaneBase;

    if (_ssbos[0] == 0)
        glGenBuffers(static_cast<GLsizei>(_ssbos.size()), _ssbos.data());

    glm::ivec4 viewport;
    glGetIntegerv(GL_VIEWPORT, &viewport.x);
    _cullLights(viewport);
    _buildBatches();
    upload_ssbo(_ssbos[0], 1, _gpu_instances);
    upload_ssbo(_ssbos[1], 2, _gpu_lights);
    upload_ssbo(_ssbos[2], 3, _tile_ranges);
    upload_ssbo(_ssbos[3], 4, _light_indices);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    shader.use();
    glm::mat4 const vp = camera ? camera->getVP() : glm::mat4(1);
    glUniformMatrix4fv(shader.getUniformLocation("u_VP"), 1, GL_FALSE, &vp[0][0]);
    glUniform1i(shader.getUniformLocation("u_NormalYDown"), normal_map_y_down ? 1 : 0);
    glm::vec3 const ambient_lin = to_linear(ambient);
    glUniform3fv(shader.getUniformLocation("u_Ambient"), 1, &ambient_lin.x);
    glUniform4i(shader.getUniformLocation("u_TileInfo"), viewport.x, viewport.y, _tiles_x, tile_size);
    // Texture unit IDs, 0 to N (the last units are reserved for shadow maps)
    static std::vector<GLint> unit_ids;
    GLint const units = static_cast<GLint>(
        SSS::GL::Window::maxGLSLTextureUnits() - reserved_texture_units);
    while (static_cast<GLint>(unit_ids.size()) < units)
        unit_ids.push_back(static_cast<GLint>(unit_ids.size()));
    glUniform1iv(shader.getUniformLocation("u_Textures"), units, unit_ids.data());
    GLint const loc_base_instance = shader.getUniformLocation("u_BaseInstance");

    _vao.bind();
    if (clear_depth_buffer)
        glClear(GL_DEPTH_BUFFER_BIT);

    // Same instance order as _buildBatches()
    auto const edit_vbo = [&](auto get_member, SSS::GL::Basic::VBO& vbo) {
        using T = std::decay_t<decltype(((*_planes.front()).*get_member)())>;
        std::vector<T> vec;
        vec.reserve(_planes.size());
        for (auto const& plane : _planes) {
            if (_isDrawn(plane))
                vec.push_back(((*plane).*get_member)());
        }
        vbo.edit(vec, GL_DYNAMIC_DRAW);
        vbo.needs_edit = false;
    };
    if (!_planes.empty()) {
        if (_update_vbos || _model_vbo.needs_edit)
            edit_vbo(&PlaneBase::getModelMat4, _model_vbo);
        if (_update_vbos || _alpha_vbo.needs_edit)
            edit_vbo(&PlaneBase::getAlpha, _alpha_vbo);
        if (_update_vbos || _tex_offset_vbo.needs_edit)
            edit_vbo(&PlaneBase::getTexOffset, _tex_offset_vbo);
    }
    _update_vbos = false;

    for (auto const& batch : _batches) {
        if (batch.count == 0)
            continue;
        for (size_t unit = 0; unit < batch.textures.size(); ++unit) {
            glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(unit));
            batch.textures[unit]->bind();
        }
        glUniform1i(loc_base_instance, static_cast<GLint>(batch.first));
        glDrawElementsInstancedBaseInstance(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr,
            batch.count, batch.first);
    }
    _vao.unbind();
    glActiveTexture(GL_TEXTURE0);
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
    if (lighting && _updateLitShader())
        _renderLit(*_lit_shader);
    else
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
