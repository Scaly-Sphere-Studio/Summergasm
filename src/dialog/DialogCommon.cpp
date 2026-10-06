#include "DialogCommon.hpp"
#include "lua_include.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace fs = std::filesystem;

namespace dialog {

// ── Textures ──────────────────────────────────────────────────────────────
SSS::RGBA32 rgba(uint32_t rgb, uint8_t a)
{
    return SSS::RGBA32(uint8_t(rgb >> 16), uint8_t(rgb >> 8), uint8_t(rgb), a);
}

glm::vec4 color(uint32_t rgb, float a)
{
    return glm::vec4((rgb >> 16 & 0xff) / 255.f, (rgb >> 8 & 0xff) / 255.f, (rgb & 0xff) / 255.f, a);
}

// Primitive coordinates are in texture pixels, centered, Y-down.
SSS::GL::Texture::Shared makePanel(int w, int h, float radius, float border,
    glm::vec4 fill, glm::vec4 edge, glm::vec4 inner_line)
{
    SSS::UIPrimitive box;
    box.shapeId     = SSS::sdRoundedBox;
    box.size        = glm::vec2(w, h) / 2.f - (border / 2.f + 1.5f); // half-extents, border & AA stay inside
    box.pos2        = glm::vec2(radius);    // top corner radii
    box.pos3        = glm::vec2(radius);    // bottom corner radii
    box.color       = fill;
    box.border      = edge;                 // the border is always opaque
    box.borderWidth = border;

    // Thin line inset along the border, for a framed look
    float const inset = border + 5.f;
    SSS::UIPrimitive line;
    line.shapeId     = SSS::sdRoundedBox;
    line.size        = box.size - inset;
    line.pos2        = glm::vec2(std::max(radius - inset, 2.f));
    line.pos3        = line.pos2;
    line.color       = inner_line;
    line.innerRadius = 0.6f;                // filled box -> outline of that half-thickness

    return SSS::GL::Texture::createSDF({ box, line }, w, h);
}

SSS::GL::Texture::Shared makeSquircle(int w, int h, float radius, float n, uint32_t rgb)
{
    radius = std::min({ radius, w / 2.f, h / 2.f });
    std::vector<SSS::RGBA32> px(size_t(w) * h, rgba(rgb, 0));
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            // Pixel center, from the nearest corner's center (0 out of the corners)
            float const dx = std::max(std::abs(x + 0.5f - w / 2.f) - (w / 2.f - radius), 0.f);
            float const dy = std::max(std::abs(y + 0.5f - h / 2.f) - (h / 2.f - radius), 0.f);
            // Superellipse "radius" of the pixel: its distance to the edge, close
            // enough near it for a one pixel anti-aliasing
            float const r = radius * std::pow(std::pow(dx / radius, n) + std::pow(dy / radius, n), 1.f / n);
            float const a = std::clamp(radius - r + 0.5f, 0.f, 1.f);
            if (a > 0.f) px[size_t(y) * w + x] = rgba(rgb, uint8_t(std::lround(a * 255.f)));
        }
    }
    auto tex = SSS::GL::Texture::create();
    tex->editRawPixels(px.data(), w, h);
    return tex;
}

SSS::GL::Texture::Shared makeContinueArrow(glm::vec4 fill, glm::vec4 edge)
{
    SSS::UIPrimitive arrow;
    arrow.shapeId      = SSS::sdTriangle;
    arrow.pos          = { -8.f, -5.f };
    arrow.pos2         = {  8.f, -5.f };
    arrow.pos3         = {  0.f,  6.f };    // Y-down: the tip is at the bottom
    arrow.cornerRadius = 1.5f;              // grows & rounds the triangle
    arrow.color        = fill;
    arrow.border       = edge;
    arrow.borderWidth  = 1.5f;
    return SSS::GL::Texture::createSDF({ arrow }, 28, 20);
}

// Right-pointing triangles overlapping like a fast-forward sign, plus an
// optional end bar.
SSS::GL::Texture::Shared makeSpeedIcon(TextSpeed const& speed, glm::vec4 fill)
{
    float const tri_w = 10.f, half_h = 7.f, step = 6.f;     // step < tri_w: triangles overlap
    float const bar_gap = 2.f, bar_w = 3.f;
    float const width = tri_w + (speed.triangles - 1) * step + (speed.bar ? bar_gap + bar_w : 0.f);
    float x = -width / 2.f;

    std::vector<SSS::UIPrimitive> shapes;
    for (int i = 0; i < speed.triangles; ++i, x += step) {
        SSS::UIPrimitive tri;
        tri.shapeId      = SSS::sdTriangle;
        tri.pos          = { x,         -half_h };
        tri.pos2         = { x,          half_h };
        tri.pos3         = { x + tri_w,  0.f };
        tri.cornerRadius = 1.f;
        tri.color        = fill;
        shapes.push_back(tri);
    }
    if (speed.bar) {
        SSS::UIPrimitive bar;
        bar.shapeId = SSS::sdRoundedBox;
        bar.pos     = { width / 2.f - bar_w / 2.f, 0.f };
        bar.size    = { bar_w / 2.f, half_h };              // half-extents
        bar.pos2    = glm::vec2(1.f);
        bar.pos3    = glm::vec2(1.f);
        bar.color   = fill;
        shapes.push_back(bar);
    }
    return SSS::GL::Texture::createSDF(shapes, SPEED_ICON_W, SPEED_ICON_H);
}

// Auto mode icon: a play triangle in a ring ("autoplay").
SSS::GL::Texture::Shared makeAutoIcon(glm::vec4 fill)
{
    SSS::UIPrimitive ring;
    ring.shapeId     = SSS::sdCircle;
    ring.size        = glm::vec2(8.5f);     // radius
    ring.innerRadius = 1.1f;                // filled circle -> ring of that half-thickness
    ring.color       = fill;

    SSS::UIPrimitive tri;
    tri.shapeId      = SSS::sdTriangle;
    tri.pos          = { -2.5f, -4.f };
    tri.pos2         = { -2.5f,  4.f };
    tri.pos3         = {  4.5f,  0.f };
    tri.cornerRadius = 0.5f;
    tri.color        = fill;
    return SSS::GL::Texture::createSDF({ ring, tri }, AUTO_ICON_SIZE, AUTO_ICON_SIZE);
}

// Log icon: a list, three dots each followed by a line.
SSS::GL::Texture::Shared makeLogIcon(glm::vec4 fill)
{
    std::vector<SSS::UIPrimitive> shapes;
    for (float y : { -5.5f, 0.f, 5.5f }) {
        SSS::UIPrimitive dot;
        dot.shapeId = SSS::sdCircle;
        dot.pos     = { -7.f, y };
        dot.size    = glm::vec2(1.8f);          // radius
        dot.color   = fill;
        shapes.push_back(dot);

        SSS::UIPrimitive line;
        line.shapeId = SSS::sdRoundedBox;
        line.pos     = { 2.5f, y };
        line.size    = { 5.5f, 1.3f };          // half-extents
        line.pos2    = glm::vec2(1.3f);
        line.pos3    = glm::vec2(1.3f);
        line.color   = fill;
        shapes.push_back(line);
    }
    return SSS::GL::Texture::createSDF(shapes, LOG_ICON_SIZE, LOG_ICON_SIZE);
}

SSS::GL::Texture::Shared makeSolid(SSS::RGBA32 color)
{
    std::vector<SSS::RGBA32> px(4, color);
    auto tex = SSS::GL::Texture::create();
    tex->editRawPixels(px.data(), 2, 2);
    return tex;
}

SSS::GL::Texture::Shared TextureCache::get(std::string const& path)
{
    if (auto it = _cache.find(path); it != _cache.end())
        return it->second;
    SSS::GL::Texture::Shared tex;
    if (fs::exists(path)) {
        tex = SSS::GL::Texture::create(path);
    }
    else {
        LOG_CTX_WRN("Dialog: missing image", path);
        tex = makeSolid(rgba(0xff00ff));
    }
    return _cache[path] = tex;
}

SSS::GL::Texture::Shared TextureCache::solid(uint32_t rgb)
{
    auto& tex = _solids[rgb];
    if (!tex) tex = makeSolid(rgba(rgb));
    return tex;
}

// ── Sprite planes ────────────────────────────────────────────────────────
void ImagePlane::set(SSS::GL::Texture::Shared t, glm::vec2 f, float h, bool m, float tilt_deg, float turn_w)
{
    if (t != tex) plane->setTexture(t);
    tex = std::move(t);
    foot = f;
    height = h;
    mirror = m;
    tilt = tilt_deg;
    turn = turn_w;
}

void ImagePlane::refresh()
{
    int tw = 0, th = 0;
    if (tex) tex->getCurrentDimensions(tw, th);
    Key const key{ tex.get(), tw, th, foot, height, mirror, tilt, turn, visible };
    if (key == _applied) return;
    _applied = key;

    // Alpha is used for visibility: PlaneBase::Hide() emits no event, so
    // the UIRenderer would not rebuild its per-instance buffers.
    bool const ready = tw > 0 && th > 0 && height > 0.f;
    plane->setAlpha(ready && visible ? 1.f : 0.f);
    if (!ready) return;

    // Fit the height, standing on the foot position
    float const k = height / th;
    float const s = std::min(tw, th) * k;
    plane->setScaling(glm::vec3((mirror ? -s : s) * turn, s, s));
    plane->setRotation(glm::vec3(0.f, 0.f, tilt));
    plane->setTranslation(glm::vec3(foot.x, foot.y - height / 2.f, 0.f));
}

std::optional<glm::vec2> ImagePlane::toScreen(glm::vec2 p, float w) const
{
    int tw = 0, th = 0;
    if (tex) tex->getCurrentDimensions(tw, th);
    if (!visible || tw <= 0 || th <= 0 || height <= 0.f)
        return std::nullopt;
    // Same transform as refresh(): centered, scaled to the height (mirrored,
    // narrowed while turning), then tilted around the center.
    float const width = tw * height / th;
    glm::mat4 m = glm::translate(glm::mat4(1.f), glm::vec3(foot.x, foot.y - height / 2.f, 0.f));
    m = glm::rotate(m, glm::radians(tilt), glm::vec3(0.f, 0.f, 1.f));
    m = glm::scale(m, glm::vec3((mirror ? -width : width) * turn, height, 1.f));
    glm::vec2 const out(m * glm::vec4(p - 0.5f * w, 0.f, w));
    if (w != 0.f) return out;
    float const len = glm::length(out);
    return len > 1e-4f ? out / len : glm::vec2(0.f);
}

// ── Text helpers ──────────────────────────────────────────────────────────
SSS::TR::Color trColor(uint32_t rgb)
{
    return SSS::TR::Color(uint8_t(rgb >> 16), uint8_t(rgb >> 8), uint8_t(rgb));
}

SSS::TR::Format textFormat(int charsize, uint32_t color)
{
    SSS::TR::Format fmt;
    fmt.charsize   = charsize;
    fmt.text_color = trColor(color);
    return fmt;
}

void place(SSS::Node_Text* node, SSS::AnchorMode mode, glm::vec2 pos)
{
    using A = SSS::AnchorMode;
    switch (mode) {
        case A::TopLeft:      mode = A::BottomLeft;   break;
        case A::CenterTop:    mode = A::CenterBottom; break;
        case A::TopRight:     mode = A::BottomRight;  break;
        case A::BottomLeft:   mode = A::TopLeft;      break;
        case A::CenterBottom: mode = A::CenterTop;    break;
        case A::BottomRight:  mode = A::TopRight;     break;
        default: break;
    }
    node->setAnchorMode(mode);
    node->setPosition(glm::vec3(pos, 0.f));
}

void setVisible(SSS::Node_Text* node, bool visible)
{
    node->model->setAlpha(visible ? 1.f : 0.f);
}

void freeText(SSS::Node_Text*& node)
{
    if (!node) return;
    node->clear();
    node->pop();
    node = nullptr;
}

bool hasGlyphs(std::string const& text)
{
    for (size_t i = 0; i < text.size();) {
        size_t const open  = text.find("{{", i);
        size_t const close = open == std::string::npos ? open : text.find("}}", open + 2);
        if (close == std::string::npos) return true;            // text up to the end
        if (open > i) return true;                              // text before the tag
        i = close + 2;
    }
    return false;
}

SSS::GL::Window& mainWindow()
{
    SSS::GL::Window* const window = g->lua()["window"].get_or<SSS::GL::Window*>(nullptr);
    if (!window)
        SSS::throw_exc("Dialog: the main window doesn't exist yet");
    return *window;
}

} // namespace dialog
