#pragma once
// Shared pieces of the dialog node (see Dialog.hpp): layout, SDF textures,
// text helpers, and the DialogNode base its parts derive from (DialogNodes.hpp).
#include "includes.hpp"
#include "CharacterData.hpp"
#include "../renpy/RenpyParser.h"

#include <SSS/SceneGraph/Node_UI.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace dialog {

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

inline constexpr size_t MAX_SPRITES = 6;
inline constexpr size_t MAX_CHOICES = 6;

// Characters moving to another place (`swap`): duration, and how far they lean
// toward their destination on the way
inline constexpr float SPRITE_MOVE_TIME = 0.22f;  // seconds
inline constexpr float SPRITE_TILT_DEG  = 8.f;

// Speech bubble tails, rising from the top of the dialogue box toward the
// speaker's mouth (pixels). Its base, on the box, is TAIL_REACH_PX from the
// mouth on the side the mouth faces, plus up to TAIL_PUSH_PX with the
// intensity. A straight tail is at most TAIL_LEN_PX long: it only hints at the
// speaker. A broken one reaches the mouth, its point TAIL_GAP_PX off it along
// the mouth's direction. Neither goes above the middle of the screen. It
// widens from its point (TAIL_W_TIP) to the box (TAIL_W_BOX).
inline constexpr float TAIL_GAP_PX    = 22.f;
inline constexpr float TAIL_LEN_PX    = 170.f;    // out of the box
inline constexpr float TAIL_REACH_PX  = 90.f;
inline constexpr float TAIL_PUSH_PX   = 260.f;
inline constexpr float TAIL_W_TIP     = 0.f;    // under 1 px: only the anti-aliasing is drawn
inline constexpr float TAIL_W_BOX     = 90.f;
// Base anchored this deep in the box: its wide end, slanted, never shows out of it
inline constexpr float TAIL_INSET_PX  = 1.5f * TAIL_W_BOX;
// The base is rolled around its place, up to TAIL_JITTER_PX away, plus up to
// TAIL_JITTER_INTENSITY_PX with the intensity. Rolled again when the speakers change.
inline constexpr float TAIL_JITTER_PX           = 30.f;
inline constexpr float TAIL_JITTER_INTENSITY_PX = 120.f;
// Several characters speaking at once: least distance between their bases
inline constexpr float TAIL_SPREAD_PX = 2.5f * TAIL_W_BOX;
// Broken (angry) tails zigzag around a ballistic curve: corners between the
// point and the box, and how far they stand from the curve
inline constexpr int   BROKEN_CORNERS = 4;
inline constexpr float BROKEN_AMP_PX  = 18.f;
// Rolled: corners shifted along the curve by up to this fraction of the space
// between two of them, their distance from it varying by +/- this fraction
inline constexpr float BROKEN_SHIFT   = 0.3f;
inline constexpr float BROKEN_AMP_VAR = 0.5f;
// A lone speaker's mouth turned away from its base (the dot of its direction
// and the way to the base under TAIL_BEND_DOT, e.g. a character facing out,
// its base held back by the end of the box): the tail leaves the mouth along its
// direction, TAIL_BEND_ARM times the mouth-to-base distance, and comes down
// into the box from TAIL_BEND_RISE_PX above it. Either style then reaches the
// mouth; a straight one is a smooth curve of TAIL_BEND_POINTS.
inline constexpr float TAIL_BEND_DOT     = 0.5f;
inline constexpr float TAIL_BEND_ARM     = 0.4f;
inline constexpr float TAIL_BEND_RISE_PX = 60.f;
inline constexpr int   TAIL_BEND_POINTS = 24;

// Debug overlay of the mouths (console: debug_mouths()): a circle on each
// mouth, and a line along its direction, out of the circle.
inline bool debug_mouths = false;
inline constexpr float DEBUG_MOUTH_R  = 14.f;
inline constexpr float DEBUG_DIR_LEN  = 3.f * DEBUG_MOUTH_R;

// "Continue" arrow press feedback
inline constexpr float ARROW_SNAP_PX   = 5.f;     // how far it snaps down
inline constexpr float ARROW_SNAP_TIME = 0.18f;   // seconds to ease back up

// Text speed setting, in characters per second (0: the whole line at once).
// Shown as a "play / fast-forward" icon: one triangle per step, and the
// instant speed adds a bar after the last one ("skip to the end" ▶▶▶|).
struct TextSpeed { int triangles; bool bar; int cps; };
inline constexpr TextSpeed TEXT_SPEEDS[] = {
    { 1, false, 20 }, { 2, false, 40 }, { 3, false, 80 }, { 3, true, 0 },
};
inline constexpr size_t TEXT_SPEED_COUNT   = std::size(TEXT_SPEEDS);
inline constexpr size_t DEFAULT_TEXT_SPEED = 1;

// Auto mode: pause once a line is fully shown (SSS_TR_TYPEWRITER_DONE), in seconds.
inline constexpr float AUTO_DELAY = 1.5f;
// Menu choices appear this long after the menu's line is fully shown, in seconds.
inline constexpr float CHOICE_DELAY = 0.5f;

// Log scrolling, in pixels per mouse wheel notch / Up / Down press.
inline constexpr int LOG_SCROLL_STEP = 40;
// Indent of the spoken lines in the log, under their speaker's name.
// SSS::TR has no tab stops ('\t' has no glyph), hence spaces.
inline constexpr char const* LOG_INDENT = "      ";

// Fill color of the dialogue and choice panels.
inline constexpr uint32_t PANEL_RGB = 0x14101e;

// Dialogue box (and the speech bubble tails): a plain creamy white squircle,
// dark text. Its corners take this share of its height, and are superellipses
// of this exponent (2: circular arcs, higher: squarer).
inline constexpr uint32_t BOX_RGB          = 0xfdf6e6;
inline constexpr uint32_t BOX_TEXT_RGB     = 0x4a3b52;
inline constexpr float    BOX_CORNER       = 0.45f;
inline constexpr float    BOX_SQUIRCLE_N   = 4.f;

// ── Textures ──────────────────────────────────────────────────────────────
SSS::RGBA32 rgba(uint32_t rgb, uint8_t a = 255);
// SDF primitive colors are normalized floats.
glm::vec4 color(uint32_t rgb, float a = 1.f);

// Panel drawn with SDF primitives *into* a texture (Texture::setSDF): the
// plane holding it keeps the add order, so the text still draws on top.
SSS::GL::Texture::Shared makePanel(int w, int h, float radius, float border,
    glm::vec4 fill, glm::vec4 edge, glm::vec4 inner_line);
// Squircle-cornered panel, rasterized on the CPU (no SDF primitive for it):
// a rectangle whose corners of the given radius are superellipse arcs,
// |x|^n + |y|^n = 1. Anti-aliased edges.
SSS::GL::Texture::Shared makeSquircle(int w, int h, float radius, float n, uint32_t rgb);
// Small triangle pointing down, shown when the text can be continued.
SSS::GL::Texture::Shared makeContinueArrow(glm::vec4 fill, glm::vec4 edge);

// Settings icons. Every speed icon has the same texture size (so the button
// keeps its hitbox), its content centered.
inline constexpr int SPEED_ICON_W = 36, SPEED_ICON_H = 20;
inline constexpr int AUTO_ICON_SIZE = 22;
inline constexpr int LOG_ICON_SIZE = 22;
SSS::GL::Texture::Shared makeSpeedIcon(TextSpeed const& speed, glm::vec4 fill);
SSS::GL::Texture::Shared makeAutoIcon(glm::vec4 fill);
SSS::GL::Texture::Shared makeLogIcon(glm::vec4 fill);

SSS::GL::Texture::Shared makeSolid(SSS::RGBA32 color);

// Image files load asynchronously; textures are cached so going back and
// forth between lines (or rebuilding the UI) doesn't reload them.
class TextureCache {
public:
    SSS::GL::Texture::Shared get(std::string const& path);
    // Plain 0xRRGGBB color
    SSS::GL::Texture::Shared solid(uint32_t rgb);
private:
    std::unordered_map<std::string, SSS::GL::Texture::Shared> _cache;
    std::unordered_map<uint32_t, SSS::GL::Texture::Shared> _solids;
};

// ── Sprite planes ────────────────────────────────────────────────────────
// Textured planes keep their texture's aspect ratio: setScaling(s) makes the
// *smaller* texture side s pixels long. Scaling can only be computed once the
// async load is done, so refresh() is called every frame and applies the
// layout when something changed.
struct ImagePlane {
    SSS::GL::Plane::Shared plane = SSS::GL::Plane::create();
    SSS::GL::Texture::Shared tex;
    glm::vec2 foot{ 0.f };      // bottom center
    float height = 0.f;
    bool mirror = false;        // flipped horizontally
    float turn = 1.f;           // 0..1: width, while turning around
    float tilt = 0.f;           // degrees, around the sprite's center
    bool visible = true;

    void set(SSS::GL::Texture::Shared t, glm::vec2 f, float h, bool m = false, float tilt_deg = 0.f, float turn_w = 1.f);
    void hide() { set(nullptr, foot, height); }
    void refresh();
    // Image space (normalized, origin at the top-left of the file's image)
    // to UI pixels, as the sprite is drawn: nullopt while not shown.
    // toScreen(p, 0) maps a direction (normalized, null when degenerate).
    std::optional<glm::vec2> toScreen(glm::vec2 p, float w = 1.f) const;

private:
    struct Key {
        void const* tex = nullptr; int tw = -1, th = -1; glm::vec2 f{ -1.f }; float h = -1.f;
        bool mirror = false; float tilt = 0.f; float turn = 1.f; bool visible = false;
        bool operator==(Key const&) const = default;
    } _applied;
};

// ── Text helpers ──────────────────────────────────────────────────────────
// RGB24(uint32_t) stores red in the *low* byte (0xBBGGRR), so build the
// color from components to keep the usual 0xRRGGBB notation.
SSS::TR::Color trColor(uint32_t rgb);
SSS::TR::Format textFormat(int charsize, uint32_t color = 0xFFFFFF);

// In the Y-down UIRenderer, Node_Text currently mirrors vertical anchors: a
// Top* mode puts the text's *bottom* edge on the position. place() takes the
// anchor you mean and compensates, so the rest of the code reads naturally.
void place(SSS::Node_Text* node, SSS::AnchorMode mode, glm::vec2 pos);
void setVisible(SSS::Node_Text* node, bool visible);
// Node_Text::clear() removes its plane from the UIRenderer. Nodes are owned
// by the SceneGraph: pop() queues their deletion, which happens in
// SceneGraph::update() (called by the Dialog once every part is destroyed).
void freeText(SSS::Node_Text*& node);

// Whether a line shows any character: format tags ({{...}}) are not shown.
bool hasGlyphs(std::string const& text);

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

// The Lua `window`: g->window isn't set yet while global_setup.lua runs
SSS::GL::Window& mainWindow();

// ── Node base ─────────────────────────────────────────────────────────────
// Settings, kept when the UI is rebuilt and between conversations
struct Settings {
    bool   auto_mode  = false;
    bool   log_open   = false;
    size_t text_speed = DEFAULT_TEXT_SPEED;
};

// What the parts of a dialog share: one UIRenderer they all draw in (built
// for the window size, see Dialog::Impl::build), the conversation being
// played and the state of its current line.
struct DialogContext {
    explicit DialogContext(SSS::GL::Window& w) : window(w) {}

    SSS::GL::Window& window;
    TextureCache textures;
    // Mouth & tail of each character (resources/characters/<id>.json)
    CharacterData characters;
    Settings settings;
    // Shown on the story's End step (TR markup), "" to finish right away
    std::string end_text;

    std::optional<Layout> L;
    // Drawn in this order: ui_back (the sprites & the dialogue box), lines
    // (the speech bubble tails, no camera: see DialogTails), then ui (the
    // rest: the name & text over the tails, choices, controls, log).
    SSS::GL::UIRenderer::Shared ui_back;
    SSS::GL::LineRenderer::Shared lines;
    SSS::GL::UIRenderer::Shared ui;
    renpy::Player const* player = nullptr;

    // Line state. shown_at: time the line was fully shown, when the
    // typewriter is done, or at the input that skipped it / showed the line
    // (instant speed, going back).
    bool typing = false;
    Clock::time_point shown_at;

    // Format::has_background is false by default, so the text is transparent.
    SSS::Node_Text* makeText(SSS::TR::Format const& fmt) const { return new SSS::Node_Text("", fmt, ui); }
    // Seconds the line has been fully shown, 0 while typing.
    float shownFor() const
    {
        return typing ? 0.f : std::chrono::duration<float>(Clock::now() - shown_at).count();
    }
};

// Input of the frame. A part that uses the click clears it, so that it is
// not also taken as "next line".
struct DialogInput {
    bool confirm = false;   // Space / Enter
    bool click   = false;   // left click
    bool back    = false;   // Backspace / Left arrow
    glm::vec2 cursor{ 0.f };
    bool cursor_moved = false;
};

// A part of the dialog. Textured planes are drawn in the order they are added
// (same Z, depth test GL_LEQUAL): the Dialog builds its parts back to front.
// A disabled part is still built, so that the draw order stays the same, but
// it isn't shown and takes no input.
class DialogNode {
public:
    virtual ~DialogNode() = default;

    // Creates the planes & text nodes in ctx.ui (ctx.ui_back for the ones
    // under the speech bubble tails), for the layout ctx.L
    virtual void build(DialogContext& ctx) = 0;
    // Releases every GL object (while the GL context is current)
    virtual void destroy() = 0;
    // A new step of the story. animate: type the line (false when going back,
    // or when the UI was rebuilt)
    virtual void present(DialogContext&, renpy::Step const&, bool /*animate*/) {}
    // Every frame, once the story moved on (sprites loading, animations)
    virtual void update(DialogContext&, DialogInput&) {}

    void setEnabled(bool enabled)
    {
        if (enabled == _enabled) return;
        _enabled = enabled;
        _refreshVisibility();
    }
    bool isEnabled() const noexcept { return _enabled; }

protected:
    // Applies _enabled (no-op when not built)
    virtual void _refreshVisibility() {}
    bool _enabled = true;
};

} // namespace dialog
