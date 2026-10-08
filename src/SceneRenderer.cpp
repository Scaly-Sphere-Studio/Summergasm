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

void SceneRenderer::setCastShadow(std::shared_ptr<SSS::GL::PlaneBase> const& plane, bool cast)
{
    if (plane)
        _plane_lighting[plane.get()].cast_shadow = cast;
}

bool SceneRenderer::getCastShadow(std::shared_ptr<SSS::GL::PlaneBase> const& plane) const
{
    auto const it = _plane_lighting.find(plane.get());
    return it == _plane_lighting.cend() ? true : it->second.cast_shadow;
}

void SceneRenderer::setShadowTransmission(std::shared_ptr<SSS::GL::PlaneBase> const& plane,
    float transmission)
{
    if (plane)
        _plane_lighting[plane.get()].shadow_transmission = std::clamp(transmission, 0.f, 1.f);
}

float SceneRenderer::getShadowTransmission(std::shared_ptr<SSS::GL::PlaneBase> const& plane) const
{
    auto const it = _plane_lighting.find(plane.get());
    return it == _plane_lighting.cend() ? 0.f : it->second.shadow_transmission;
}

static std::string read_file(std::filesystem::path const& path)
{
    std::ifstream ifs(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
}

// Loads the shaders, and reloads them when their files are edited (checked
// twice per second). Shaders failing to compile are logged by SSS::GL and the
// previous ones are kept.
bool SceneRenderer::HotShaders::update()
{
    auto const now = std::chrono::steady_clock::now();
    if (now - last_check < std::chrono::milliseconds(500))
        return shaders != nullptr;
    last_check = now;

    std::filesystem::path const vert = shaders_folder + name + ".vert";
    std::filesystem::path const frag = shaders_folder + name + ".frag";
    std::error_code ec_v, ec_f;
    auto const new_vert_time = std::filesystem::last_write_time(vert, ec_v);
    auto const new_frag_time = std::filesystem::last_write_time(frag, ec_f);
    // Missing files (or mid-save): keep the current state
    if (ec_v || ec_f)
        return shaders != nullptr;
    // Unchanged files: keep the current state (even a failed one)
    if (new_vert_time == vert_time && new_frag_time == frag_time)
        return shaders != nullptr;
    vert_time = new_vert_time;
    frag_time = new_frag_time;

    auto new_shaders = SSS::GL::Shaders::create();
    new_shaders->loadFromStrings(read_file(vert), read_file(frag));
    // Failed programs stay at id 0, which has no uniform
    if (new_shaders->getUniformLocation("u_VP") == -1) {
        LOG_ERR("SceneRenderer: " + name + " shaders failed to load, see the log above");
        return shaders != nullptr;
    }
    shaders = std::move(new_shaders);
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
    if (_shadow_fbo != 0)
        glDeleteFramebuffers(1, &_shadow_fbo);
    if (_shadow_depth != 0)
        glDeleteTextures(1, &_shadow_depth);
    if (_shadow_color != 0)
        glDeleteTextures(1, &_shadow_color);
    if (_shadow_white != 0)
        glDeleteTextures(1, &_shadow_white);
    if (_shadow_depth_sampler != 0)
        glDeleteSamplers(1, &_shadow_depth_sampler);
}

bool SceneRenderer::_isDrawn(std::shared_ptr<SSS::GL::PlaneBase> const& plane)
{
    return !plane->isHidden() && plane->sdf_mode == SSS::GL::PlaneBase::SDFMode::None;
}

void SceneRenderer::_followPlanes()
{
    for (auto const& light : lights) {
        if (!light)
            continue;
        if (auto const plane = light->follow.lock())
            light->position = plane->getTranslation() + light->follow_offset;
    }
}

bool SceneRenderer::_computeViewSlab(std::array<glm::vec3, 8>& corners) const
{
    if (!camera)
        return false;

    // View-space depth range of the planes that matter for shadows
    glm::mat4 const view = camera->getView();
    float d_min = FLT_MAX, d_max = -FLT_MAX;
    for (auto const& plane : _planes) {
        if (!_isDrawn(plane))
            continue;
        auto const it = _plane_lighting.find(plane.get());
        if (it != _plane_lighting.cend() && it->second.factor <= 0.f && !it->second.cast_shadow)
            continue;
        float const depth = -(view * plane->getModelMat4()[3]).z;
        d_min = std::min(d_min, depth);
        d_max = std::max(d_max, depth);
    }
    float const z_near = camera->getZNear();
    d_min = std::max(d_min, z_near);
    if (d_max < d_min)
        return false;

    // Unproject the screen corners at both depths
    glm::mat4 const proj = camera->getProjection();
    glm::mat4 const inv_vp = glm::inverse(proj * view);
    float const xy = 1.f + std::max(shadow_fit_margin, 0.f);
    size_t i = 0;
    for (float const depth : { d_min, d_max }) {
        glm::vec4 const clip = proj * glm::vec4(0.f, 0.f, -depth, 1.f);
        float const ndc_z = clip.z / clip.w;
        for (float const y : { -xy, xy }) {
            for (float const x : { -xy, xy }) {
                glm::vec4 const world = inv_vp * glm::vec4(x, y, ndc_z, 1.f);
                corners[i++] = glm::vec3(world) / world.w;
            }
        }
    }
    return true;
}

// Perspective view from the light, never wider than shadow_fov. When the view
// slab is entirely on one side of the light, the frustum is aimed at it and
// fitted to it: the shadow map's texels are spent on what's visible, and no
// visible shadow is cut by the frustum's edges. Otherwise, it's the whole
// cone toward shadow_direction.
glm::mat4 SceneRenderer::_shadowViewProjection(PointLight const& light, bool has_slab,
    std::array<glm::vec3, 8> const& slab, glm::vec4& frustum) const
{
    auto const look = [&](glm::vec3 dir) {
        if (glm::length(dir) < 1e-4f)
            dir = glm::vec3(0.f, 0.f, -1.f);
        dir = glm::normalize(dir);
        glm::vec3 const up = std::abs(dir.y) > 0.99f ? glm::vec3(0.f, 0.f, 1.f) : glm::vec3(0.f, 1.f, 0.f);
        return glm::lookAt(light.position, light.position + dir, up);
    };

    float const max_tan = std::tan(glm::radians(std::clamp(light.shadow_fov, 1.f, 170.f)) / 2.f);
    float z_near = std::max(1.f, light.radius * 0.001f);
    float z_far = light.radius;
    glm::vec2 tan_min(-max_tan), tan_max(max_tan);
    glm::mat4 view = look(light.shadow_direction);

    if (has_slab) {
        // Aimed at the slab: average direction of its corners
        glm::vec3 aim(0.f);
        for (auto const& corner : slab) {
            glm::vec3 const d = corner - light.position;
            float const length = glm::length(d);
            if (length > 1e-4f)
                aim += d / length;
        }
        glm::mat4 const aimed_view = look(aim);
        glm::vec2 fit_min(FLT_MAX), fit_max(-FLT_MAX);
        float depth_min = FLT_MAX, depth_max = 0.f;
        bool fits = glm::length(aim) > 1e-4f;
        for (size_t i = 0; fits && i < slab.size(); ++i) {
            glm::vec3 const v(aimed_view * glm::vec4(slab[i], 1.f));
            float const depth = -v.z;
            // Part of the view behind the light: keep the whole cone
            if (depth < z_near) {
                fits = false;
                break;
            }
            glm::vec2 const t = glm::vec2(v) / depth;
            fit_min = glm::min(fit_min, t);
            fit_max = glm::max(fit_max, t);
            depth_min = std::min(depth_min, depth);
            depth_max = std::max(depth_max, depth);
        }
        if (fits) {
            view = aimed_view;
            tan_min = glm::max(fit_min, glm::vec2(-max_tan));
            tan_max = glm::min(fit_max, glm::vec2(max_tan));
            // Tighter depth range: better depth precision
            z_near = std::max(z_near, depth_min * 0.5f);
            z_far = std::min(z_far, depth_max * 1.05f);
        }
    }
    if (tan_max.x <= tan_min.x || tan_max.y <= tan_min.y || z_far <= z_near) {
        frustum = glm::vec4(1.f, 2.f, 0.f, 0.f);
        return glm::perspective(glm::radians(1.f), 1.f, 1.f, 2.f) * view;
    }
    // A world width w at depth d spans w / (d * (tan_max - tan_min)) in uv
    glm::vec2 const light_uv = std::max(light.size, 0.f) / (tan_max - tan_min);
    frustum = glm::vec4(z_near, z_far, light_uv);
    return glm::frustum(tan_min.x * z_near, tan_max.x * z_near,
        tan_min.y * z_near, tan_max.y * z_near, z_near, z_far) * view;
}

// Lights are culled per screen tile (tile_size pixels): each light's bounding
// sphere is projected on screen, and its index is added to every tile its
// rectangle overlaps. Fragments only evaluate the lights of their tile.
// The first max_shadow_lights shadow casting lights get a shadow map layer.
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
    int shadow_layers = 0;
    std::array<glm::vec3, 8> slab;
    bool const has_slab = shadows && _computeViewSlab(slab);

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

        GPULight gpu_light{
            glm::vec4(light->position, light->radius),
            glm::vec4(to_linear(light->color, light->intensity), light->falloff),
            glm::vec4(to_linear(light->terminator_color, light->intensity),
                std::max(light->terminator_width, 0.f)),
            glm::vec4(to_linear(light->shadow_color, light->intensity), 0.f),
            glm::mat4(1.f),
            glm::vec4(-1.f, 0.f, 0.f, 0.f),
            glm::vec4(0.f)
        };
        if (shadows && light->cast_shadows && shadow_layers < max_shadow_lights) {
            gpu_light.shadow_vp = _shadowViewProjection(*light, has_slab, slab,
                gpu_light.shadow_frustum);
            gpu_light.shadow_params = glm::vec4(static_cast<float>(shadow_layers++),
                std::max(light->shadow_bias, 0.f), std::max(light->shadow_softness, 0.f), 0.f);
        }
        auto const index = static_cast<uint32_t>(_gpu_lights.size());
        _gpu_lights.push_back(gpu_light);

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
    _translucent_casters = false;
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
        instance.cast_shadow = 1;

        Texture* normal_map = nullptr;
        auto const it = _plane_lighting.find(plane.get());
        if (it != _plane_lighting.cend()) {
            instance.lighting = it->second.factor;
            instance.cast_shadow = it->second.cast_shadow ? 1 : 0;
            instance.shadow_transmission = it->second.shadow_transmission;
            _translucent_casters = _translucent_casters
                || (it->second.cast_shadow && it->second.shadow_transmission > 0.f);
            int w = 0, h = 0;
            if (it->second.normal_map)
                it->second.normal_map->getCurrentDimensions(w, h);
            if (w != 0 && h != 0)
                normal_map = it->second.normal_map.get();
        }

        // Planes without texture keep their instance (discarded by the shaders)
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

// Same instance VBOs as PlaneRenderer::render(), in _buildBatches() order.
// The VAO must be bound.
void SceneRenderer::_updateVBOs()
{
    using SSS::GL::PlaneBase;

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
}

void SceneRenderer::_setTextureUnits(SSS::GL::Shaders& shader)
{
    static std::vector<GLint> unit_ids;
    GLint const units = static_cast<GLint>(
        SSS::GL::Window::maxGLSLTextureUnits() - reserved_texture_units);
    while (static_cast<GLint>(unit_ids.size()) < units)
        unit_ids.push_back(static_cast<GLint>(unit_ids.size()));
    glUniform1iv(shader.getUniformLocation("u_Textures"), units, unit_ids.data());
}

void SceneRenderer::_drawBatches(SSS::GL::Shaders& shader) const
{
    GLint const loc_base_instance = shader.getUniformLocation("u_BaseInstance");
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
}

static void set_shadow_map_params(bool depth_compare)
{
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (depth_compare) {
        // Compared by sampler2DArrayShadow, bilinear: 2x2 PCF per sample
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    }
}

// Layers are only (re)allocated when more are needed or the size changed,
// and the transmittance only when there are translucent casters.
void SceneRenderer::_allocateShadowMaps(int layers)
{
    if (_shadow_fbo == 0) {
        glGenFramebuffers(1, &_shadow_fbo);
        glGenTextures(1, &_shadow_depth);
        glGenTextures(1, &_shadow_color);
        // Receivers' transmittance without translucent casters: no tint
        glGenTextures(1, &_shadow_white);
        glBindTexture(GL_TEXTURE_2D_ARRAY, _shadow_white);
        static constexpr GLfloat white[4] = { 1.f, 1.f, 1.f, 1.f };
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA16F, 1, 1, 1, 0, GL_RGBA, GL_FLOAT, white);
        set_shadow_map_params(false);
        // Same depth layers, read as depths instead of compared
        glGenSamplers(1, &_shadow_depth_sampler);
        glSamplerParameteri(_shadow_depth_sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glSamplerParameteri(_shadow_depth_sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glSamplerParameteri(_shadow_depth_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glSamplerParameteri(_shadow_depth_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glSamplerParameteri(_shadow_depth_sampler, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    }

    int const size =std::clamp(shadow_map_size, 64, 8192);
    if (size != _shadow_size) {
        _shadow_size = size;
        _shadow_layers = 0;
        _shadow_color_layers = 0;
    }
    if (layers > _shadow_layers) {
        _shadow_layers = layers;
        glBindTexture(GL_TEXTURE_2D_ARRAY, _shadow_depth);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24, size, size, layers,
            0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        set_shadow_map_params(true);
    }
    if (_translucent_casters && layers > _shadow_color_layers) {
        _shadow_color_layers = layers;
        glBindTexture(GL_TEXTURE_2D_ARRAY, _shadow_color);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA16F, size, size, layers,
            0, GL_RGBA, GL_HALF_FLOAT, nullptr);
        set_shadow_map_params(false);
    }
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

// Per shadow casting light, every casting plane is drawn from the light:
// 0. opaque texels write the depth layer,
// 1. only with translucent casters: their texels in front of the opaque ones
//    multiply the transmittance layer, and keep the nearest distance in its
//    alpha (GL_MIN).
// The VAO must be bound, the SSBOs uploaded and the batches built.
void SceneRenderer::_renderShadowMaps()
{
    int layers = 0;
    for (auto const& light : _gpu_lights)
        layers += light.shadow_params.x >= 0.f ? 1 : 0;
    if (layers == 0 || !_shadow_shaders.update())
        return;
    _allocateShadowMaps(layers);

    // Saved state, restored below
    GLint prev_fbo = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_fbo);
    glm::ivec4 prev_viewport;
    glGetIntegerv(GL_VIEWPORT, &prev_viewport.x);

    auto& shader = *_shadow_shaders.shaders;
    shader.use();
    _setTextureUnits(shader);
    GLint const loc_vp = shader.getUniformLocation("u_VP");
    GLint const loc_pass = shader.getUniformLocation("u_Pass");
    GLint const loc_light = shader.getUniformLocation("u_LightPosRadius");

    glBindFramebuffer(GL_FRAMEBUFFER, _shadow_fbo);
    glViewport(0, 0, _shadow_size, _shadow_size);
    if (!_translucent_casters) {
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 0, 0);
        glDrawBuffer(GL_NONE);
    }
    else
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
    static constexpr GLfloat white[4] = { 1.f, 1.f, 1.f, 1.f };
    static constexpr GLfloat far_depth = 1.f;

    for (auto const& light : _gpu_lights) {
        auto const layer = static_cast<GLint>(light.shadow_params.x);
        if (layer < 0)
            continue;
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, _shadow_depth, 0, layer);
        glUniformMatrix4fv(loc_vp, 1, GL_FALSE, &light.shadow_vp[0][0]);
        glUniform4fv(loc_light, 1, &light.position_radius.x);

        // Opaque depth
        glClearBufferfv(GL_DEPTH, 0, &far_depth);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glUniform1i(loc_pass, 0);
        _drawBatches(shader);
        if (!_translucent_casters)
            continue;

        // Translucent transmittance, behind nothing opaque
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, _shadow_color, 0, layer);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glClearBufferfv(GL_COLOR, 0, white);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendEquationSeparate(GL_FUNC_ADD, GL_MIN);
        glBlendFuncSeparate(GL_ZERO, GL_SRC_COLOR, GL_ONE, GL_ONE);
        glUniform1i(loc_pass, 1);
        _drawBatches(shader);
    }

    // Window's default state (see SSS::GL::Window)
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prev_fbo));
    glViewport(prev_viewport.x, prev_viewport.y, prev_viewport.z, prev_viewport.w);
}

// Same VAO & instance VBOs as PlaneRenderer::render(), with the lit shader.
// Per-instance data, lights and light tiles are in SSBOs (bindings 1 to 4,
// see lit_plane.frag). SDF planes are not supported.
void SceneRenderer::_renderLit(SSS::GL::Shaders& shader)
{
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

    _vao.bind();
    _updateVBOs();
    _renderShadowMaps();

    shader.use();
    glm::mat4 const vp = camera ? camera->getVP() : glm::mat4(1);
    glUniformMatrix4fv(shader.getUniformLocation("u_VP"), 1, GL_FALSE, &vp[0][0]);
    glUniform1i(shader.getUniformLocation("u_NormalYDown"), normal_map_y_down ? 1 : 0);
    glm::vec3 const ambient_lin = to_linear(ambient);
    glUniform3fv(shader.getUniformLocation("u_Ambient"), 1, &ambient_lin.x);
    glUniform4i(shader.getUniformLocation("u_TileInfo"), viewport.x, viewport.y, _tiles_x, tile_size);
    _setTextureUnits(shader);

    // Shadow maps on the first reserved units
    GLint const shadow_unit = static_cast<GLint>(
        SSS::GL::Window::maxGLSLTextureUnits() - reserved_texture_units);
    glActiveTexture(GL_TEXTURE0 + shadow_unit);
    glBindTexture(GL_TEXTURE_2D_ARRAY, _shadow_depth);
    glActiveTexture(GL_TEXTURE0 + shadow_unit + 1);
    glBindTexture(GL_TEXTURE_2D_ARRAY, _translucent_casters ? _shadow_color : _shadow_white);
    glUniform1i(shader.getUniformLocation("u_ShadowDepth"), shadow_unit);
    glUniform1i(shader.getUniformLocation("u_ShadowColor"), shadow_unit + 1);
    glActiveTexture(GL_TEXTURE0 + shadow_unit + 2);
    glBindTexture(GL_TEXTURE_2D_ARRAY, _shadow_depth);
    glBindSampler(shadow_unit + 2, _shadow_depth_sampler);
    glUniform1i(shader.getUniformLocation("u_ShadowDepthRaw"), shadow_unit + 2);

    if (clear_depth_buffer)
        glClear(GL_DEPTH_BUFFER_BIT);
    _drawBatches(shader);
    glBindSampler(shadow_unit + 2, 0);
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
    _followPlanes();
    _sortPlanes();
    if (lighting && _lit_shaders.update())
        _renderLit(*_lit_shaders.shaders);
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
