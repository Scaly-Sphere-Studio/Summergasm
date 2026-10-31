#pragma once

#include "includes.hpp"

// Paper map of 4 columns by 2 rows, folded like a city map: first as an
// accordion along the 3 vertical creases, /\/\ in a cross section seen from
// below, the front up: column 0 (the left one) behind column 1, back to
// back, column 2 in front of column 1, face to face, column 3 in front of
// column 2, back to back. The left end stays in place: folded, the stack
// lies at the left of the flat map's area, the panels sliding out to the
// right. Then that strip is folded in half along the horizontal crease, the
// bottom half coming up in front of the top one.
// The folded map shows column 0's bottom panel's front (the cover), the
// horizontal crease at the bottom: turned 180 degrees in its plane (see
// MapOverlay), the crease is on top and the cover reads upright.
// Unfolding is the reverse: the half opens (the cover flips up, going
// behind), then the accordion.
// A procedural mesh (no texture seams), recomputed on the CPU when a fold
// changes: the vertical folds are rounded hinges, so the stacked panels sit
// apart, and the horizontal one bends the whole stack around a rounded
// crease.
// The creases can be anywhere on the paper (see setCreases), the panels
// then having different sizes.
// The map lies in its local xy plane, centered on the origin, its front (the
// front texture) toward +z.
// Drawn by SceneRenderer (see addMesh), lit only: the front texture on front
// faces, the back texture on back faces, mirrored as when turning the sheet
// over (left becomes right).
class FoldingMap : public SSS::GL::ModelBase
{
private:
    FoldingMap(SSS::GL::Texture::Shared front, SSS::GL::Texture::Shared back, float width, float height);

public:
    using Shared = std::shared_ptr<FoldingMap>;
    static Shared create(SSS::GL::Texture::Shared front, SSS::GL::Texture::Shared back,
        float width, float height);

    static constexpr int columns = 4;

    struct Vertex {
        glm::vec3 position;
        glm::vec2 uv;
        glm::vec3 normal;
        glm::vec3 tangent;
    };

    SSS::GL::Texture::Shared front;
    SSS::GL::Texture::Shared back;
    // Printed over the front texture, in order, with the same uv (front
    // faces only): e.g. the map's roads, monuments & pins (see MapContent).
    // Each one can be rebaked alone.
    std::array<SSS::GL::Texture::Shared, 3> layers;
    // Over each layer: a small texture printed on the uv rect (u0, v0, u1,
    // v1) of the paper, none if the rect is empty. E.g. what animates on the
    // map, baked every frame without rebaking the whole layer.
    std::array<SSS::GL::Texture::Shared, 3> overlays;
    std::array<glm::vec4, 3> overlay_rects{};
    bool hidden{ false };
    bool cast_shadow{ true };

    // 0: flat, 1: folded (clamped)
    // Bottom half in front of the top one
    void setHalfFold(float fold);
    // Column 0 behind column 1
    void setFlap(float fold);
    // Columns 2 & 3 in front of column 1
    void setAccordion(float fold);
    inline float getHalfFold() const noexcept { return _half_fold; };
    inline float getFlap() const noexcept { return _flap; };
    inline float getAccordion() const noexcept { return _accordion; };

    // Faux perspective, for orthographic cameras (2D): world x & y are
    // pinched toward pinch.xy by d / (d + away), where away = pinch.z - world
    // z is how far behind the plane z = pinch.z a vertex is, and d = pinch.w.
    // Panels folding away get a shorter far edge (trapezoids). w = 0: off.
    glm::vec4 pinch{ 0.f };

    // Creases, on the texture (0 -> 1, v = 0 at the top): the 3 vertical
    // ones' u, left to right, and the horizontal one's v. Default: evenly
    // spaced.
    void setCreases(std::array<float, columns - 1> const& u, float v);

    // Gap between the panels stacked by the accordion, in local units
    void setThickness(float thickness);
    inline float getThickness() const noexcept { return _thickness; };
    // Accordion's depth (z) scale, 1: physical (0 -> 1, clamped). Lower keeps
    // the panels flat against a board, sliding out from under each other
    // rather than turning toward the viewer.
    void setDepthScale(float scale);
    inline float getDepthScale() const noexcept { return _depth_scale; };
    // Inner radius of the horizontal crease: the two halves of the stack sit
    // twice that apart
    void setCreaseRadius(float radius);
    inline float getCreaseRadius() const noexcept { return _crease_radius; };

    inline float getWidth() const noexcept { return _width; };
    inline float getHeight() const noexcept { return _height; };

    // Local bounding box of the current shape (see update)
    inline glm::vec3 getBoundsMin() const noexcept { return _bounds_min; };
    inline glm::vec3 getBoundsMax() const noexcept { return _bounds_max; };

    // Recomputes the mesh if a fold or the paper changed
    void update();
    // Texture uv (v = 0 at the top) of the paper seen at local x & y, from
    // the front (+z): the nearest one, as shaped by the last update(). None
    // off the paper. Ignores pinch.
    std::optional<glm::vec2> uvAt(glm::vec2 xy) const;
    // Uploads the mesh if update changed it, binds the VAO and draws the
    // triangles with the shader in use, then unbinds the VAO. Needs the GL
    // context.
    void draw();

private:
    float _width, _height;
    float _half_fold{ 0.f };
    float _flap{ 0.f };
    float _accordion{ 0.f };
    float _thickness{ 2.f };
    std::array<float, columns - 1> _creases_u{ 0.25f, 0.5f, 0.75f };
    float _crease_v{ 0.5f };
    float _depth_scale{ 1.f };
    float _crease_radius{ 3.f };
    bool _dirty{ true };

    // Vertex columns of the grid along x (u), shared by every row
    struct GridColumn {
        float u;
        glm::vec2 xz;       // after the accordion
        glm::vec2 normal;   // xz, the front
        glm::vec2 tangent;  // xz
    };
    // Rows of the grid, bottom to top
    struct Row {
        float y;        // on the paper, flat
        float crease;   // 0 -> 1 around the horizontal crease (bottom half)
    };
    std::vector<Row> _rows;
    // Vertex columns of each paper column (hinge arc + panel)
    std::array<int, columns> _grid_widths{};
    int _grid_columns{ 0 };
    int _index_count{ 0 };

    std::vector<Vertex> _vertices;
    std::vector<uint32_t> _indices;
    glm::vec3 _bounds_min{ 0.f }, _bounds_max{ 0.f };

    SSS::GL::Basic::VAO _vao;
    SSS::GL::Basic::VBO _vbo;
    SSS::GL::Basic::IBO _ibo;
    bool _needs_upload{ false };

    void _buildRows();
    void _buildIndices();
    void _rebuild();
};
