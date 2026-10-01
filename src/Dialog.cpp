// Port of the Documentation's renpy_scene example (examples/renpy_scene.cpp)
// as a Summergasm scene component, see Dialog.hpp.
// Characters who are not talking are drawn in black & white, characters
// sharing a position line up (the ones behind shifted inward and scaled
// down). Panels and icons are SDF textures. Lines are revealed character per
// character (the Area's Typewriter print mode); the icons stacked in bubbles
// at the bottom right of the screen open the log, toggle the auto mode and
// change the text speed (▶ / ▶▶ / ▶▶▶ / ▶▶▶| instant).
#include "Dialog.hpp"
#include "renpy/RenpyParser.h"

#include <SSS/SceneGraph/Node_UI.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <unordered_map>

namespace fs = std::filesystem;

namespace {

// ── Layout (pixels, UIRenderer is Y-down with origin at the top-left) ─────
struct Layout {
    float W, H;
    float box_w, box_h, box_left, box_top;
    float pad = 22.f;
    float sprite_h;
    float choice_w, choice_h = 58.f, choice_gap = 18.f;
    // Characters sharing a position stand in a line: each one further back
    // is shifted toward the center, scaled down and slightly raised.
    float stack_dx, stack_rise, stack_scale = 0.88f;

    Layout(float w, float h) : W(w), H(h)
    {
        box_w    = W * 0.8f;
        box_h    = H * 0.26f;
        box_left = (W - box_w) / 2.f;
        box_top  = H - 24.f - box_h;
        sprite_h = H * 0.78f;
        choice_w = W * 0.7f;
        stack_dx   = W * 0.11f;
        stack_rise = H * 0.015f;
    }
    float spriteX(renpy::Pos pos) const
    {
        switch (pos) {
            case renpy::Pos::Left:  return W * 0.2f;
            case renpy::Pos::Right: return W * 0.8f;
            default:                return W * 0.5f;
        }
    }
};

constexpr size_t MAX_SPRITES = 6;
constexpr size_t MAX_CHOICES = 6;

// "Continue" arrow press feedback
constexpr float ARROW_SNAP_PX   = 5.f;     // how far it snaps down
constexpr float ARROW_SNAP_TIME = 0.18f;   // seconds to ease back up

// Text speed setting, in characters per second (0: the whole line at once).
// Shown as a "play / fast-forward" icon: one triangle per step, and the
// instant speed adds a bar after the last one ("skip to the end" ▶▶▶|).
struct TextSpeed { int triangles; bool bar; int cps; };
constexpr TextSpeed TEXT_SPEEDS[] = {
    { 1, false, 20 }, { 2, false, 40 }, { 3, false, 80 }, { 3, true, 0 },
};
constexpr size_t TEXT_SPEED_COUNT   = std::size(TEXT_SPEEDS);
constexpr size_t DEFAULT_TEXT_SPEED = 1;

// Auto mode: pause once a line is fully shown (SSS_TR_TYPEWRITER_DONE), in seconds.
constexpr float AUTO_DELAY = 1.5f;
// Menu choices appear this long after the menu's line is fully shown, in seconds.
constexpr float CHOICE_DELAY = 0.5f;

// Log scrolling, in pixels per mouse wheel notch / Up / Down press.
constexpr int LOG_SCROLL_STEP = 40;
// Indent of the spoken lines in the log, under their speaker's name.
// SSS::TR has no tab stops ('\t' has no glyph), hence spaces.
constexpr char const* LOG_INDENT = "      ";

// Fill color of the dialogue and choice panels.
constexpr uint32_t PANEL_RGB = 0x14101e;

// ── Textures ──────────────────────────────────────────────────────────────
SSS::RGBA32 rgba(uint32_t rgb, uint8_t a = 255)
{
    return SSS::RGBA32(uint8_t(rgb >> 16), uint8_t(rgb >> 8), uint8_t(rgb), a);
}

// SDF primitive colors are normalized floats.
glm::vec4 color(uint32_t rgb, float a = 1.f)
{
    return glm::vec4((rgb >> 16 & 0xff) / 255.f, (rgb >> 8 & 0xff) / 255.f, (rgb & 0xff) / 255.f, a);
}

// Panel drawn with SDF primitives *into* a texture (Texture::setSDF): the
// plane holding it keeps the add order, so the text still draws on top.
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

// Small triangle pointing down, shown when the text can be continued.
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

// Text speed icon: right-pointing triangles overlapping like a fast-forward
// sign, plus an optional end bar. Every icon has the same texture size (so
// the button keeps its hitbox), its content centered.
constexpr int SPEED_ICON_W = 36, SPEED_ICON_H = 20;
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
constexpr int AUTO_ICON_SIZE = 22;
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
constexpr int LOG_ICON_SIZE = 22;
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

// Image files load asynchronously; textures are cached so going back and
// forth between lines (or rebuilding the UI) doesn't reload them.
class TextureCache {
public:
    SSS::GL::Texture::Shared get(std::string const& path)
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
    SSS::GL::Texture::Shared solid(uint32_t rgb)
    {
        std::string const key = "#" + std::to_string(rgb);
        if (auto it = _cache.find(key); it != _cache.end())
            return it->second;
        return _cache[key] = makeSolid(rgba(rgb));
    }
private:
    std::unordered_map<std::string, SSS::GL::Texture::Shared> _cache;
};

// ── Image planes (background & sprites) ──────────────────────────────────
// Textured planes keep their texture's aspect ratio: setScaling(s) makes the
// *smaller* texture side s pixels long. Scaling can only be computed once the
// async load is done, so refresh() is called every frame and applies the
// layout when something changed.
struct ImagePlane {
    SSS::GL::Plane::Shared plane = SSS::GL::Plane::create();
    SSS::GL::Texture::Shared tex;
    glm::vec2 center{ 0.f };
    float height = 0.f;         // 0: cover the whole screen (background)

    void set(SSS::GL::Texture::Shared t, glm::vec2 c, float h)
    {
        if (t != tex) plane->setTexture(t);
        tex = std::move(t);
        center = c;
        height = h;
    }

    void refresh(Layout const& L)
    {
        int tw = 0, th = 0;
        if (tex) tex->getCurrentDimensions(tw, th);
        Key const key{ tex.get(), tw, th, center, height };
        if (key == _applied) return;
        _applied = key;

        // Alpha is used for visibility: PlaneBase::Hide() emits no event, so
        // the UIRenderer would not rebuild its per-instance buffers.
        bool const ready = tw > 0 && th > 0;
        plane->setAlpha(ready ? 1.f : 0.f);
        if (!ready) return;

        float const k = height > 0.f ? height / th                  // fit height (sprite)
                                     : std::max(L.W / tw, L.H / th); // cover (background)
        float const bottom_offset = height > 0.f ? -height / 2.f : 0.f;
        plane->setScaling(glm::vec3(std::min(tw, th) * k));
        plane->setTranslation(glm::vec3(center.x, center.y + bottom_offset, 0.f));
    }

    void hide() { set(nullptr, center, height); }

private:
    struct Key {
        void const* tex = nullptr; int tw = -1, th = -1; glm::vec2 c{ -1.f }; float h = -1.f;
        bool operator==(Key const&) const = default;
    } _applied;
};

// ── Text helpers ──────────────────────────────────────────────────────────
// RGB24(uint32_t) stores red in the *low* byte (0xBBGGRR), so build the
// color from components to keep the usual 0xRRGGBB notation.
SSS::TR::Color trColor(uint32_t rgb)
{
    return SSS::TR::Color(uint8_t(rgb >> 16), uint8_t(rgb >> 8), uint8_t(rgb));
}

SSS::TR::Format textFormat(int charsize, uint32_t color = 0xFFFFFF)
{
    SSS::TR::Format fmt;
    fmt.charsize   = charsize;
    fmt.text_color = trColor(color);
    return fmt;
}

// In the Y-down UIRenderer, Node_Text currently mirrors vertical anchors: a
// Top* mode puts the text's *bottom* edge on the position. place() takes the
// anchor you mean and compensates, so the rest of the code reads naturally.
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

// Whether a line shows any character: format tags ({{...}}) are not shown.
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

// Raises a flag when a text area's typewriter has revealed its last glyph
// (the Area emits SSS_TR_TYPEWRITER_DONE from its update, in pollEverything()).
class TypewriterWatcher : public SSS::Observer {
public:
    explicit TypewriterWatcher(SSS::TR::Area& area) { _observe(area); }
    bool done = false;
private:
    void _subjectUpdate(SSS::Subject const&, SSS::Event const& event) override
    {
        if (event.id == EVENT_ID("SSS_TR_TYPEWRITER_DONE")) done = true;
    }
};

// Clickable icon in its own bubble, the whole bubble being the hitbox.
struct Button {
    SSS::GL::Plane::Shared bubble, icon;
    glm::vec2 min{ 0.f }, max{ 0.f };
    bool contains(glm::vec2 p) const { return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y; }
};

using Clock = std::chrono::steady_clock;

// The Lua `window`, see Dialog::Impl::window
SSS::GL::Window& mainWindow()
{
    SSS::GL::Window* const window = g->lua["window"].get_or<SSS::GL::Window*>(nullptr);
    if (!window)
        SSS::throw_exc("Dialog: the main window doesn't exist yet");
    return *window;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────
struct Dialog::Impl {
    explicit Impl(std::string const& path);
    ~Impl();
    bool update();

    // Main window, from Lua: g->window isn't set yet while global_setup.lua runs
    SSS::GL::Window& window;
    renpy::Script script;
    renpy::Player player;
    TextureCache textures;

    // Settings, kept when the UI is rebuilt
    bool   auto_mode  = false;
    bool   log_open   = false;
    size_t text_speed = DEFAULT_TEXT_SPEED;

    // ── UI, built for the window size by build() ─────────────────────────
    std::optional<Layout> L;
    SSS::GL::UIRenderer::Shared ui;

    ImagePlane background;
    std::vector<ImagePlane> sprites;
    SSS::GL::Plane::Shared box, arrow;
    float arrow_y = 0.f;
    SSS::GL::Texture::Shared choice_tex, choice_hover_tex;
    std::vector<SSS::GL::Plane::Shared> choice_boxes;

    // Text nodes are owned by the SceneGraph, see freeTexts()
    SSS::TR::Format name_fmt;
    SSS::Node_Text* name_node = nullptr;
    SSS::Node_Text* text_node = nullptr;
    std::vector<SSS::Node_Text*> choice_texts;
    SSS::TR::Area::Shared text_area;
    std::unique_ptr<TypewriterWatcher> typewriter;

    Button log_button, auto_button, speed_button;
    SSS::GL::Texture::Shared log_icon, log_icon_hover, log_icon_active;
    SSS::GL::Texture::Shared auto_icon, auto_icon_hover, auto_icon_active;
    std::vector<SSS::GL::Texture::Shared> speed_icons, speed_icons_hover;

    SSS::GL::Plane::Shared log_panel;
    SSS::Node_Text* log_title = nullptr;
    SSS::Node_Text* log_node = nullptr;
    SSS::TR::Area::Shared log_area;

    // ── Playing state ────────────────────────────────────────────────────
    bool typing = false;
    // Time the line was fully shown: when the typewriter is done, or at the
    // input that skipped it / showed the line (instant speed, going back).
    Clock::time_point shown_at;
    size_t selected = 0;
    size_t choice_count = 0;
    bool   choices_shown = false;               // after the line is shown + CHOICE_DELAY
    std::optional<Clock::time_point> pressed_at; // last "next line" input
    Button const* hovered_button = nullptr;

    void build(int w, int h);
    void destroy();

    SSS::Node_Text* makeText(SSS::TR::Format const& fmt);
    void refreshSettings(Button const* hovered);
    void setLogOpen(bool open);
    float shownFor() const;
    void finishLine();
    void showLine(std::string const& str, bool animate);
    void setTextSpeed(size_t speed);
    glm::vec2 choiceCenter(size_t i) const;
    void highlight();
    void showChoices(bool on);
    void present(renpy::Step const& step, bool animate);
    std::optional<size_t> choiceUnderCursor() const;
};

Dialog::Impl::Impl(std::string const& path)
    : window(mainWindow()),
      script(renpy::Script::load(fs::absolute(g->assets_folder + path).string())),
      player(script)
{
    // The UIRenderer & SDF textures make raw GL calls: the window's context
    // must be current (Lua scenes run without any)
    SSS::GL::Context const context = window.setContext();
    auto const [w, h] = window.getDimensions();
    build(w, h);
    present(player.current(), true);
}

Dialog::Impl::~Impl()
{
    SSS::GL::Context const context = window.setContext();
    destroy();
}

// Format::has_background is false by default, so the text is transparent.
SSS::Node_Text* Dialog::Impl::makeText(SSS::TR::Format const& fmt)
{
    return new SSS::Node_Text("", fmt, ui);
}

// Textured planes are drawn in the order they are added (same Z, depth
// test GL_LEQUAL), so they're created back to front: background, sprites,
// dialogue box, choice boxes, then the text nodes, settings and log.
void Dialog::Impl::build(int w, int h)
{
    L.emplace(float(w), float(h));
    Layout const& L = *this->L;

    ui = SSS::GL::UIRenderer::create();
    ui->updateResolution(L.W, L.H);
    window.addRenderer(ui);

    background = ImagePlane{};
    ui->addPlane(background.plane);

    sprites.resize(MAX_SPRITES);
    for (auto& sp : sprites) ui->addPlane(sp.plane);

    box = SSS::GL::Plane::create(makePanel(int(L.box_w), int(L.box_h), 18.f, 3.f,
        color(PANEL_RGB, 0.85f), color(0xc9bfe8), color(0xf2b632, 0.45f)));
    box->setScaling(glm::vec3(L.box_h));    // smaller side = height, width follows the ratio
    box->setTranslation(glm::vec3(L.W / 2.f, L.box_top + L.box_h / 2.f, 0.f));
    ui->addPlane(box);

    // Shown at the bottom of the dialogue box while the player can go forward
    // (hidden on menus); snaps down on each "next line" input.
    arrow = SSS::GL::Plane::create(makeContinueArrow(color(0xf2b632), color(PANEL_RGB)));
    arrow->setScaling(glm::vec3(20.f));     // texture's smaller side, drawn 1:1
    arrow->setAlpha(0.f);
    arrow_y = L.box_top + L.box_h - L.pad * 0.8f;
    arrow->setTranslation(glm::vec3(L.W / 2.f, arrow_y, 0.f));
    ui->addPlane(arrow);

    choice_tex = makePanel(int(L.choice_w), int(L.choice_h), 14.f, 2.f,
        color(PANEL_RGB, 0.85f), color(0x9d93bd), color(0xffffff, 0.15f));
    choice_hover_tex = makePanel(int(L.choice_w), int(L.choice_h), 14.f, 4.f,
        color(PANEL_RGB, 0.94f), color(0xf2b632), color(0xf2b632, 0.4f));   // gold border when selected
    choice_boxes.clear();
    for (size_t i = 0; i < MAX_CHOICES; ++i) {
        auto p = SSS::GL::Plane::create(choice_tex);
        p->setScaling(glm::vec3(L.choice_h));
        p->setAlpha(0.f);
        ui->addPlane(p);
        choice_boxes.push_back(p);
    }

    // Text nodes add their own plane to the UIRenderer on construction.
    name_fmt = textFormat(28);
    name_node = makeText(name_fmt);

    // Node_Text::setWrappingMin() only sets a minimum; the max wrap width
    // lives on the underlying TR::Area.
    text_node = makeText(textFormat(24));
    text_area = text_node->model->getTextArea();
    text_area->setWrappingMaxWidth(int(L.box_w - 2.f * L.pad));
    place(text_node, SSS::AnchorMode::TopLeft, { L.box_left + L.pad, L.box_top + L.pad + 40.f });
    typewriter = std::make_unique<TypewriterWatcher>(*text_area);

    choice_texts.clear();
    for (size_t i = 0; i < MAX_CHOICES; ++i) {
        auto* t = makeText(textFormat(24));
        t->model->getTextArea()->setWrappingMaxWidth(int(L.choice_w - 2.f * L.pad));
        place(t, SSS::AnchorMode::Center, { 0.f, 0.f });
        setVisible(t, false);
        choice_texts.push_back(t);
    }

    // ── Settings (bottom right corner of the screen) ─────────────────────
    // Clickable icons stacked in their own bubble, from top to bottom: log,
    // auto mode, text speed. Icons in the idle, hovered and active colors.
    log_icon         = makeLogIcon(color(0x9d93bd));
    log_icon_hover   = makeLogIcon(color(0xffffff));
    log_icon_active  = makeLogIcon(color(0xf2b632));
    auto_icon        = makeAutoIcon(color(0x9d93bd));
    auto_icon_hover  = makeAutoIcon(color(0xffffff));
    auto_icon_active = makeAutoIcon(color(0xf2b632));
    speed_icons.clear();
    speed_icons_hover.clear();
    for (auto const& speed : TEXT_SPEEDS) {
        speed_icons.push_back(makeSpeedIcon(speed, color(0x9d93bd)));
        speed_icons_hover.push_back(makeSpeedIcon(speed, color(0xffffff)));
    }

    // Every bubble has the same size, fitting the widest (speed) icon.
    float const bubble_pad = 8.f, bubble_gap = 8.f, bubble_margin = 16.f;
    int const bubble_w = int(2.f * bubble_pad + std::max({ LOG_ICON_SIZE, AUTO_ICON_SIZE, SPEED_ICON_W }));
    int const bubble_h = int(2.f * bubble_pad + std::max({ LOG_ICON_SIZE, AUTO_ICON_SIZE, SPEED_ICON_H }));
    auto const bubble_tex = makePanel(bubble_w, bubble_h, 14.f, 2.f,
        color(PANEL_RGB, 0.85f), color(0x9d93bd), color(0, 0.f));      // no inner line
    glm::vec2 const bubble_size(bubble_w, bubble_h);
    float const bubble_x = L.W - bubble_margin - bubble_w / 2.f;       // centers
    float const speed_y  = L.H - bubble_margin - bubble_h / 2.f;
    float const auto_y   = speed_y - bubble_h - bubble_gap;
    float const log_y    = auto_y - bubble_h - bubble_gap;

    auto const makeButton = [&](Button& b, SSS::GL::Texture::Shared icon, float icon_side, float y) {
        glm::vec3 const center(bubble_x, y, 0.f);
        b.bubble = SSS::GL::Plane::create(bubble_tex);
        b.bubble->setScaling(glm::vec3(float(bubble_h)));
        b.bubble->setTranslation(center);
        b.icon = SSS::GL::Plane::create(std::move(icon));
        b.icon->setScaling(glm::vec3(icon_side));                      // texture's smaller side, drawn 1:1
        b.icon->setTranslation(center);
        b.min = glm::vec2(center) - bubble_size / 2.f;
        b.max = glm::vec2(center) + bubble_size / 2.f;
    };
    makeButton(log_button,   log_icon,                float(LOG_ICON_SIZE),  log_y);
    makeButton(auto_button,  auto_icon,               float(AUTO_ICON_SIZE), auto_y);
    makeButton(speed_button, speed_icons[text_speed], float(SPEED_ICON_H),   speed_y);
    // Bubbles first: textured planes are drawn in the order they are added.
    for (Button* b : { &log_button, &auto_button, &speed_button }) ui->addPlane(b->bubble);
    for (Button* b : { &log_button, &auto_button, &speed_button }) ui->addPlane(b->icon);
    hovered_button = nullptr;
    refreshSettings(nullptr);

    // ── Log ───────────────────────────────────────────────────────────────
    // Above the dialogue box, as wide as it and without covering it (the
    // settings bubbles are beside the box, below the log). Drawn over the
    // scene: added after every other plane and text node.
    float const log_gap = 24.f;                 // to the top of the screen, and to the box
    float const log_w = L.box_w, log_left = L.box_left;
    float const log_top = log_gap, log_h = L.box_top - log_gap - log_top;
    float const log_title_h = 48.f;

    log_panel = SSS::GL::Plane::create(makePanel(int(log_w), int(log_h), 18.f, 3.f,
        color(PANEL_RGB, 0.95f), color(0xc9bfe8), color(0xf2b632, 0.45f)));
    log_panel->setScaling(glm::vec3(std::min(log_w, log_h)));
    log_panel->setTranslation(glm::vec3(L.W / 2.f, log_top + log_h / 2.f, 0.f));
    log_panel->setAlpha(0.f);
    ui->addPlane(log_panel);

    log_title = makeText(textFormat(28, 0xf2b632));
    log_title->setText("Historique");
    place(log_title, SSS::AnchorMode::CenterTop, { L.W / 2.f, log_top + L.pad * 0.8f });
    setVisible(log_title, false);

    // Fixed size area (setDimensions() turns the auto-sizing off, lines still
    // break at its width): its content scrolls.
    log_node = makeText(textFormat(22));
    log_area = log_node->model->getTextArea();
    log_area->setDimensions(int(log_w - 2.f * L.pad), int(log_h - log_title_h - 1.5f * L.pad));
    place(log_node, SSS::AnchorMode::TopLeft, { log_left + L.pad, log_top + log_title_h + L.pad * 0.5f });
    setVisible(log_node, false);
}

// Releases every GL object of the UI (while the GL context is alive).
void Dialog::Impl::destroy()
{
    typewriter.reset();
    text_area.reset();
    log_area.reset();

    // Node_Text::clear() removes its plane from the UIRenderer. Nodes are
    // owned by the SceneGraph: pop() queues their deletion, which happens in
    // SceneGraph::update() (nothing else calls it, so it is done here).
    std::vector<SSS::Node_Text*> nodes{ name_node, text_node, log_title, log_node };
    nodes.insert(nodes.end(), choice_texts.begin(), choice_texts.end());
    for (auto* node : nodes) {
        if (!node) continue;
        node->clear();
        node->pop();
    }
    SSS::SceneGraph::update();
    name_node = text_node = log_title = log_node = nullptr;
    choice_texts.clear();

    log_panel.reset();
    log_button = {};
    auto_button = {};
    speed_button = {};
    hovered_button = nullptr;
    speed_icons.clear();
    speed_icons_hover.clear();
    log_icon.reset();
    log_icon_hover.reset();
    log_icon_active.reset();
    auto_icon.reset();
    auto_icon_hover.reset();
    auto_icon_active.reset();
    choice_boxes.clear();
    choice_tex.reset();
    choice_hover_tex.reset();
    box.reset();
    arrow.reset();
    sprites.clear();
    background.plane.reset();
    background.tex.reset();

    if (ui) {
        window.removeRenderer(ui);
        ui.reset();
    }
    L.reset();
}

void Dialog::Impl::refreshSettings(Button const* hovered)
{
    log_button.icon->setTexture(hovered == &log_button ? log_icon_hover
                              : log_open               ? log_icon_active : log_icon);
    auto_button.icon->setTexture(hovered == &auto_button ? auto_icon_hover
                               : auto_mode               ? auto_icon_active : auto_icon);
    speed_button.icon->setTexture((hovered == &speed_button ? speed_icons_hover : speed_icons)[text_speed]);
}

// Opens on the latest line.
void Dialog::Impl::setLogOpen(bool open)
{
    log_open = open;
    log_panel->setAlpha(open ? 1.f : 0.f);
    setVisible(log_title, open);
    setVisible(log_node, open);
    log_area->clear();                      // closed: its effects stop redrawing the hidden text
    if (!open) return;
    std::string str;
    for (auto const& e : player.log()) {
        if (!str.empty()) str += "\n\n";
        if (e.choice)
            str += renpy::colored("\xC2\xBB " + e.text, 0xf2b632);   // "» choice", in gold
        else if (!e.speaker_name.empty())       // spoken: indented under the name
            str += renpy::colored(e.speaker_name, e.speaker_color.value_or(0xc9bfe8)) + "\n" + LOG_INDENT + e.text;
        else
            str += e.text;
    }
    log_node->setText(str);
    // Its pixels are drawn asynchronously: until they are in, the area
    // shows the previous ones (scrolling is clamped to them).
    log_area->scroll(1'000'000);            // clamped to the bottom
}

// Seconds the line has been fully shown, 0 while typing.
float Dialog::Impl::shownFor() const
{
    return typing ? 0.f : std::chrono::duration<float>(Clock::now() - shown_at).count();
}

// Also shows the rest of the line when it is skipped.
void Dialog::Impl::finishLine()
{
    text_area->setPrintMode(SSS::TR::PrintMode::Instant);
    typing   = false;
    shown_at = Clock::now();
}

// Area::parseString() keeps the typewriter cursor where it was (so that
// text can be appended while typing); clear() puts it back at the start.
void Dialog::Impl::showLine(std::string const& str, bool animate)
{
    int const cps = TEXT_SPEEDS[text_speed].cps;
    text_area->clear();
    text_node->setText(str);
    typewriter->done = false;               // drop the previous line's event
    // An empty text is never typed, so it would never emit the event.
    if (animate && cps > 0 && hasGlyphs(str)) {
        text_area->setTypeWriterSpeed(cps);
        text_area->setPrintMode(SSS::TR::PrintMode::Typewriter);
        typing = true;
    }
    else {
        finishLine();
    }
}

// Applies to the line being typed too.
void Dialog::Impl::setTextSpeed(size_t speed)
{
    text_speed = speed;
    int const cps = TEXT_SPEEDS[speed].cps;
    if (cps > 0)     text_area->setTypeWriterSpeed(cps);
    else if (typing) finishLine();
}

glm::vec2 Dialog::Impl::choiceCenter(size_t i) const
{
    float const total = choice_count * L->choice_h + (choice_count - 1) * L->choice_gap;
    float const top   = (L->box_top - total) / 2.f;
    return glm::vec2(L->W / 2.f, top + i * (L->choice_h + L->choice_gap) + L->choice_h / 2.f);
}

void Dialog::Impl::highlight()
{
    for (size_t i = 0; i < choice_count; ++i)
        choice_boxes[i]->setTexture(i == selected ? choice_hover_tex : choice_tex);
}

void Dialog::Impl::showChoices(bool on)
{
    choices_shown = on;
    for (size_t i = 0; i < MAX_CHOICES; ++i) {
        bool const visible = on && i < choice_count;
        choice_boxes[i]->setAlpha(visible ? 1.f : 0.f);
        setVisible(choice_texts[i], visible);
    }
}

// animate: type the line (false when going back to a previous line)
void Dialog::Impl::present(renpy::Step const& step, bool animate)
{
    Layout const& L = *this->L;

    // Background & sprites
    auto const& bg = step.scene.background;
    if (bg.color)               background.set(textures.solid(*bg.color), { L.W / 2.f, L.H / 2.f }, 0.f);
    else if (!bg.image.empty()) background.set(textures.get(bg.image), { L.W / 2.f, L.H / 2.f }, 0.f);
    else                        background.set(textures.solid(0x000000), { L.W / 2.f, L.H / 2.f }, 0.f);

    using Kind = renpy::Step::Kind;
    // Characters sharing a position line up in script order: the first
    // one stands in front, the next ones behind it, shifted inward (on
    // the center spot: alternately right and left).
    struct Placement { SSS::GL::Texture::Shared tex; glm::vec2 foot; float height; int depth; };
    std::vector<Placement> placements;
    int per_pos[3] = {};
    for (auto const& sp : step.scene.sprites) {
        int const k     = per_pos[int(sp.pos)]++;
        int const depth = sp.pos == renpy::Pos::Center ? (k + 1) / 2 : k;
        float const dir = sp.pos == renpy::Pos::Left  ? 1.f
                        : sp.pos == renpy::Pos::Right ? -1.f
                        : (k % 2 ? 1.f : -1.f);
        // Characters who are not talking (all of them during narration) are drawn in black & white.
        bool const talking = step.kind != Kind::End && !step.speaker.empty() && sp.tag == step.speaker;
        auto tex = textures.get(sp.image);
        tex->setGrayscale(!talking);
        placements.push_back({ tex,
            { L.spriteX(sp.pos) + dir * depth * L.stack_dx, L.H - depth * L.stack_rise },
            L.sprite_h * std::pow(L.stack_scale, float(depth)), depth });
    }
    // Sprite planes were added back to front: fill them from the back row.
    std::stable_sort(placements.begin(), placements.end(),
        [](Placement const& a, Placement const& b) { return a.depth > b.depth; });
    for (size_t i = 0; i < sprites.size(); ++i) {
        if (i < placements.size())
            sprites[i].set(placements[i].tex, placements[i].foot, placements[i].height);
        else
            sprites[i].hide();
    }

    // Speaker name, on the side of the speaker's sprite
    bool const has_name = !step.speaker.empty() && step.kind != Kind::End;
    setVisible(name_node, has_name);
    if (has_name) {
        SSS::TR::Format fmt = name_fmt;
        if (step.speaker_color) fmt.text_color = trColor(*step.speaker_color);
        name_node->setText(step.speaker_name, fmt);

        auto const* sprite = step.scene.find(step.speaker);
        auto const side = sprite ? sprite->pos : renpy::Pos::Left;
        float const y = L.box_top + L.pad * 0.5f;
        if (side == renpy::Pos::Right)
            place(name_node, SSS::AnchorMode::TopRight, { L.box_left + L.box_w - L.pad, y });
        else if (side == renpy::Pos::Center)
            place(name_node, SSS::AnchorMode::CenterTop, { L.W / 2.f, y });
        else
            place(name_node, SSS::AnchorMode::TopLeft, { L.box_left + L.pad, y });
    }

    // Dialogue
    if (step.kind == Kind::End)
        showLine("{{\"effect\":\"FadingWaves\",\"effect_offset\":3}}~ Fin ~{{}}"
            "     (Retour arri\xC3\xA8re pour revenir, Entr\xC3\xA9" "e ou \xC3\x89" "chap pour retourner au menu)", animate);
    else
        showLine(step.text, animate);

    // Menu choices: laid out now, shown by update() once the line is done.
    choice_count = step.kind == Kind::Menu ? std::min(step.choices.size(), MAX_CHOICES) : 0;
    selected = 0;
    showChoices(false);
    for (size_t i = 0; i < choice_count; ++i) {
        glm::vec2 const c = choiceCenter(i);
        choice_boxes[i]->setTranslation(glm::vec3(c, 0.f));
        choice_texts[i]->setText(step.choices[i]);
        place(choice_texts[i], SSS::AnchorMode::Center, c);
    }
    highlight();
}

std::optional<size_t> Dialog::Impl::choiceUnderCursor() const
{
    auto [cx, cy] = window.getCursorPos();
    for (size_t i = 0; i < choice_count; ++i) {
        glm::vec2 const c = choiceCenter(i);
        if (std::abs(cx - c.x) <= L->choice_w / 2.f && std::abs(cy - c.y) <= L->choice_h / 2.f)
            return i;
    }
    return std::nullopt;
}

bool Dialog::Impl::update()
{
    SSS::GL::Context const context = window.setContext();

    // Window resized: rebuild the UI for the new size, the story goes on
    // where it was (the current line is shown at once). Skipped while
    // minimized (0 x 0).
    if (auto const [w, h] = window.getDimensions(); w > 0 && h > 0 && (w != int(L->W) || h != int(L->H))) {
        destroy();
        build(w, h);
        present(player.current(), false);
        if (log_open) setLogOpen(true);
    }

    auto const& keys   = window.getKeyInputs();
    auto const& clicks = window.getClickInputs();

    bool const confirm = keys[GLFW_KEY_SPACE].is_pressed() || keys[GLFW_KEY_ENTER].is_pressed()
                      || keys[GLFW_KEY_KP_ENTER].is_pressed();
    bool       click   = clicks[GLFW_MOUSE_BUTTON_LEFT].is_pressed();
    bool const back    = keys[GLFW_KEY_BACKSPACE].is_pressed() || keys[GLFW_KEY_LEFT].is_pressed();
    bool       leave   = false;

    // Settings: a click on them is not a "next line" input.
    {
        auto const [cx, cy] = window.getCursorPos();
        glm::vec2 const cursor(cx, cy);
        Button const* hovered = log_button.contains(cursor)   ? &log_button
                              : auto_button.contains(cursor)  ? &auto_button
                              : speed_button.contains(cursor) ? &speed_button : nullptr;
        bool settings_changed = hovered != hovered_button;
        hovered_button = hovered;

        // Escape only closes the log: otherwise it returns to the menu (see dialog.lua)
        if (keys[GLFW_KEY_X].is_pressed() || (click && hovered == &log_button)
            || (log_open && keys[GLFW_KEY_ESCAPE].is_pressed())) {
            setLogOpen(!log_open);
            settings_changed = true;
        }

        if (keys[GLFW_KEY_TAB].is_pressed() || (click && hovered == &auto_button)) {
            auto_mode = !auto_mode;         // shown_at is kept: a line read for long enough goes on at once
            settings_changed = true;
        }
        if (keys[GLFW_KEY_KP_ADD].is_pressed() && text_speed + 1 < TEXT_SPEED_COUNT) {
            setTextSpeed(text_speed + 1);
            settings_changed = true;
        }
        if (keys[GLFW_KEY_KP_SUBTRACT].is_pressed() && text_speed > 0) {
            setTextSpeed(text_speed - 1);
            settings_changed = true;
        }
        if (click && hovered == &speed_button) {
            setTextSpeed((text_speed + 1) % TEXT_SPEED_COUNT);
            settings_changed = true;
        }
        if (hovered) click = false;
        if (settings_changed) refreshSettings(hovered);
    }

    if (typing && typewriter->done)
        finishLine();

    using Kind = renpy::Step::Kind;
    auto const kind = player.current().kind;
    if (kind == Kind::Menu && !choices_shown && !typing && shownFor() >= CHOICE_DELAY)
        showChoices(true);
    bool const auto_next = auto_mode && !log_open && kind == Kind::Say && !typing
        && shownFor() >= AUTO_DELAY;
    bool changed = false;
    bool animate = true;

    if (log_open) {
        // The log takes every input but the settings: the story waits.
        int pixels = int(-g->scroll_y * LOG_SCROLL_STEP);
        if (keys[GLFW_KEY_UP].is_pressed())        pixels -= LOG_SCROLL_STEP;
        if (keys[GLFW_KEY_DOWN].is_pressed())      pixels += LOG_SCROLL_STEP;
        if (keys[GLFW_KEY_PAGE_UP].is_pressed())   pixels -= log_area->getHeight();
        if (keys[GLFW_KEY_PAGE_DOWN].is_pressed()) pixels += log_area->getHeight();
        if (pixels != 0) log_area->scroll(pixels);
    }
    else if (back) {
        changed = player.back();
        animate = false;
    }
    else if (typing && (confirm || click)) {
        finishLine();                       // first input shows the whole line
    }
    else if (kind == Kind::Say && (confirm || click || auto_next)) {
        player.next();
        changed = true;
        pressed_at = Clock::now();
    }
    else if (kind == Kind::Menu && choices_shown) {
        size_t const prev = selected;
        if (keys[GLFW_KEY_UP].is_pressed())   selected = (selected + choice_count - 1) % choice_count;
        if (keys[GLFW_KEY_DOWN].is_pressed()) selected = (selected + 1) % choice_count;
        auto const hovered = choiceUnderCursor();
        if (hovered && (window.getCursorDiff() != std::make_tuple(0, 0) || click))
            selected = *hovered;
        if (selected != prev) highlight();

        std::optional<size_t> pick;
        for (size_t i = 0; i < choice_count && i < 9; ++i)
            if (keys[GLFW_KEY_1 + int(i)].is_pressed()) pick = i;
        if (confirm || (click && hovered)) pick = selected;
        if (pick) {
            player.choose(*pick);
            changed = true;
        }
    }
    else if (kind == Kind::End && (confirm || click)) {
        leave = true;
    }

    if (changed) present(player.current(), animate);
    arrow->setAlpha(player.current().kind == Kind::Say && !typing ? 1.f : 0.f);

    background.refresh(*L);
    for (auto& sp : sprites) sp.refresh(*L);

    // Continue arrow: still, it snaps down on input then eases back up.
    if (pressed_at) {
        float const s = std::chrono::duration<float>(Clock::now() - *pressed_at).count()
                      / ARROW_SNAP_TIME;
        float const k = 1.f - std::min(s, 1.f);
        arrow->setTranslation(glm::vec3(L->W / 2.f, arrow_y + ARROW_SNAP_PX * k * k, 0.f));
        if (s >= 1.f) pressed_at.reset();
    }

    return leave;
}

// ─────────────────────────────────────────────────────────────────────────
Dialog::Dialog(std::string const& path)
    : _impl(std::make_unique<Impl>(path))
{
}

Dialog::~Dialog() = default;

bool Dialog::update()
{
    return _impl->update();
}

bool Dialog::isLogOpen() const noexcept
{
    return _impl->log_open;
}
