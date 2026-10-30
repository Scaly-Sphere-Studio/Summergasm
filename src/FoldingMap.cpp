#include "FoldingMap.hpp"

#include <glm/gtc/constants.hpp>

// Segments of each accordion hinge's arc, and rows of the horizontal crease
static constexpr int hinge_segments = 6;
static constexpr int crease_rows = 32;

FoldingMap::Shared FoldingMap::create(SSS::GL::Texture::Shared front, SSS::GL::Texture::Shared back,
    float width, float height)
{
    return Shared(new FoldingMap(std::move(front), std::move(back), width, height));
}

FoldingMap::FoldingMap(SSS::GL::Texture::Shared front_tex, SSS::GL::Texture::Shared back_tex,
    float width, float height)
    : front(std::move(front_tex)), back(std::move(back_tex)),
    _width(std::max(width, 1e-3f)), _height(std::max(height, 1e-3f))
{
    // Column 0 is a flat panel, the others start with their hinge's arc
    _grid_columns = 0;
    for (int c = 0; c < columns; ++c) {
        _grid_widths[c] = (c == 0 ? 0 : hinge_segments) + 2;
        _grid_columns += _grid_widths[c];
    }
    _buildRows();

    // No VAO bound while editing the IBO, which would bind it to that VAO
    _vao.unbind();
    _buildIndices();
    _vao.setup([this]() {
        _vbo.bind();
        _ibo.bind();
        GLsizei const stride = sizeof(Vertex);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, position));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, uv));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, tangent));
    });
    _vao.unbind();
    update();
}

void FoldingMap::setHalfFold(float fold)
{
    fold = std::clamp(fold, 0.f, 1.f);
    _dirty = _dirty || fold != _half_fold;
    _half_fold = fold;
}

void FoldingMap::setFlap(float fold)
{
    fold = std::clamp(fold, 0.f, 1.f);
    _dirty = _dirty || fold != _flap;
    _flap = fold;
}

void FoldingMap::setAccordion(float fold)
{
    fold = std::clamp(fold, 0.f, 1.f);
    _dirty = _dirty || fold != _accordion;
    _accordion = fold;
}

void FoldingMap::setCreases(std::array<float, columns - 1> const& u, float v)
{
    float prev = 0.f;
    for (int i = 0; i < columns - 1; ++i) {
        prev = std::clamp(u[i], prev, 1.f);
        _creases_u[i] = prev;
    }
    _crease_v = std::clamp(v, 0.f, 1.f);
    // Same row count: the indices don't change
    _buildRows();
    _dirty = true;
}

void FoldingMap::setThickness(float thickness)
{
    _thickness = std::max(thickness, 0.f);
    _dirty = true;
}

void FoldingMap::setDepthScale(float scale)
{
    _depth_scale = std::clamp(scale, 1e-3f, 1.f);
    _dirty = true;
}

void FoldingMap::setCreaseRadius(float radius)
{
    _crease_radius = std::max(radius, 1e-3f);
    _dirty = true;
}

// Rows: the top half is flat, the bottom half starts with the crease's
// rows, all on the horizontal crease (flat, they're stacked), going around
// it once folded, dense for a smooth curve. Sorted bottom to top.
void FoldingMap::_buildRows()
{
    float const crease_y = _height * (0.5f - _crease_v);
    _rows.clear();
    _rows.push_back({ -_height / 2.f, 1.f });
    for (int j = crease_rows; j >= 0; --j)
        _rows.push_back({ crease_y, static_cast<float>(j) / crease_rows });
    _rows.push_back({ _height / 2.f, 0.f });
}

// Quads between consecutive vertex columns of each paper column (not across
// the columns: the hinge arcs start where the previous column ends), and
// between consecutive rows. Counter clockwise seen from the front.
void FoldingMap::_buildIndices()
{
    std::vector<uint32_t> indices;
    auto const row_count = static_cast<uint32_t>(_rows.size());
    auto const stride = static_cast<uint32_t>(_grid_columns);
    uint32_t start = 0;
    for (int c = 0; c < columns; ++c) {
        auto const width = static_cast<uint32_t>(_grid_widths[c]);
        for (uint32_t j = 0; j + 1 < row_count; ++j) {
            for (uint32_t g = start; g + 1 < start + width; ++g) {
                uint32_t const v00 = j * stride + g;
                uint32_t const v10 = v00 + 1;
                uint32_t const v01 = v00 + stride;
                uint32_t const v11 = v01 + 1;
                indices.insert(indices.end(), { v00, v10, v11, v11, v01, v00 });
            }
        }
        start += width;
    }
    _index_count = static_cast<int>(indices.size());
    _ibo.edit(static_cast<GLsizeiptr>(indices.size() * sizeof(uint32_t)), indices.data(), GL_STATIC_DRAW);
}

void FoldingMap::update()
{
    if (!_dirty)
        return;
    _rebuild();
    _dirty = false;
    _needs_upload = true;
}

// 1. Accordion, in the xz plane: each column goes along direction psi
//    (around y), chained from x = -width / 2: flap * -pi for column 0,
//    accordion * -pi for column 2, 0 for columns 1 & 3 (column 3 turns back
//    by as much as column 2 turned). Each hinge is an arc of radius
//    thickness / 2 turning from one direction to the next, centered on the
//    side it folds to: the stacked panels sit thickness apart. The chain is
//    then moved for its left end to stay at x = -width / 2 (column 0's
//    outer edge, once flat): unfolding, the panels spread out to the right
//    of it, and its depth (z) scaled by the depth scale: panels kept
//    against a board, sliding out from under each other.
// 2. Half fold, in the yz plane, applied to that stack: the bottom half goes
//    around an axis along x, at the horizontal crease, R (crease radius) in
//    front of the stack, by theta = half * pi: each point around an arc of
//    its distance to the axis (the crease's rows), the paper below the crease
//    straight on from there. The arcs are extra paper (as the hinges):
//    folded, the bottom half is in front of the top one, the same length.
void FoldingMap::_rebuild()
{
    float const pi = glm::pi<float>();
    float const R = _crease_radius;
    float const hinge_radius = _thickness / 2.f;
    std::array<float, columns> const psis{ -_flap * pi, 0.f, -_accordion * pi, 0.f };
    // Column edges, flat
    std::array<float, columns + 1> edges{};
    edges[0] = -_width / 2.f;
    for (int c = 1; c < columns; ++c)
        edges[c] = (_creases_u[c - 1] - 0.5f) * _width;
    edges[columns] = _width / 2.f;

    // Rotation around y by psi: (1, 0) -> direction, (0, 1) -> normal (xz)
    auto const rot_y = [](glm::vec2 v, float psi) {
        float const c = std::cos(psi), s = std::sin(psi);
        return glm::vec2(v.x * c + v.y * s, -v.x * s + v.y * c);
    };
    auto const dir = [&](float psi) { return rot_y(glm::vec2(1.f, 0.f), psi); };
    auto const nrm = [&](float psi) { return rot_y(glm::vec2(0.f, 1.f), psi); };

    std::vector<GridColumn> grid;
    grid.reserve(_grid_columns);
    glm::vec2 p(edges[0], 0.f);
    float prev_psi = 0.f;
    for (int c = 0; c < columns; ++c) {
        float const psi = psis[c];
        float const u0 = edges[c] / _width + 0.5f;
        float const u1 = edges[c + 1] / _width + 0.5f;
        if (c > 0) {
            float const delta = psi - prev_psi;
            float const side = delta >= 0.f ? 1.f : -1.f;
            glm::vec2 const center = p - side * nrm(prev_psi) * hinge_radius;
            for (int k = 0; k < hinge_segments; ++k) {
                float const phi = delta * static_cast<float>(k) / hinge_segments;
                grid.push_back({ u0, center + rot_y(p - center, phi),
                    nrm(prev_psi + phi), dir(prev_psi + phi) });
            }
            p = center + rot_y(p - center, delta);
        }
        grid.push_back({ u0, p, nrm(psi), dir(psi) });
        p += dir(psi) * (edges[c + 1] - edges[c]);
        grid.push_back({ u1, p, nrm(psi), dir(psi) });
        prev_psi = psi;
    }
    float left = FLT_MAX;
    for (auto const& g : grid)
        left = std::min(left, g.xz.x);
    float front_z = -FLT_MAX;
    for (auto& g : grid) {
        g.xz.x += edges[0] - left;
        g.xz.y *= _depth_scale;
        front_z = std::max(front_z, g.xz.y);
    }

    float const theta = _half_fold * pi;
    float const crease_y = _height * (0.5f - _crease_v);
    float const axis_z = front_z + R;

    _vertices.resize(_rows.size() * grid.size());
    _bounds_min = glm::vec3(FLT_MAX);
    _bounds_max = glm::vec3(-FLT_MAX);
    size_t i = 0;
    for (auto const [y, crease] : _rows) {
        float const phi = crease * theta;
        float const sin_phi = std::sin(phi), cos_phi = std::cos(phi);
        // Below the crease, straight
        float const straight = crease_y - y;
        for (auto const& g : grid) {
            Vertex& v = _vertices[i++];
            if (phi > 0.f) {
                // Around the axis, from (-straight, w) (yz, from the axis)
                float const w = g.xz.y - axis_z;
                v.position = glm::vec3(g.xz.x,
                    crease_y - straight * cos_phi + w * sin_phi,
                    axis_z + straight * sin_phi + w * cos_phi);
                v.normal = glm::vec3(g.normal.x, g.normal.y * sin_phi, g.normal.y * cos_phi);
                v.tangent = glm::vec3(g.tangent.x, g.tangent.y * sin_phi, g.tangent.y * cos_phi);
            }
            else {
                v.position = glm::vec3(g.xz.x, y, g.xz.y);
                v.normal = glm::vec3(g.normal.x, 0.f, g.normal.y);
                v.tangent = glm::vec3(g.tangent.x, 0.f, g.tangent.y);
            }
            // Texture's top row at v = 0
            v.uv = glm::vec2(g.u, 0.5f - y / _height);
            _bounds_min = glm::min(_bounds_min, v.position);
            _bounds_max = glm::max(_bounds_max, v.position);
        }
    }
}

void FoldingMap::draw()
{
    _vao.bind();
    if (_needs_upload) {
        _vbo.edit(_vertices, GL_DYNAMIC_DRAW);
        _needs_upload = false;
    }
    glDrawElements(GL_TRIANGLES, _index_count, GL_UNSIGNED_INT, nullptr);
    _vao.unbind();
}
