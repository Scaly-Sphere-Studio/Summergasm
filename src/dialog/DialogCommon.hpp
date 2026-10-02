#pragma once
// Shared pieces of the dialog node (see Dialog.hpp): layout, SDF textures,
// text helpers, and the DialogNode base its parts derive from (DialogNodes.hpp).
#include "includes.hpp"
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

// ── Textures ──────────────────────────────────────────────────────────────
SSS::RGBA32 rgba(uint32_t rgb, uint8_t a = 255);
// SDF primitive colors are normalized floats.
glm::vec4 color(uint32_t rgb, float a = 1.f);

// Panel drawn with SDF primitives *into* a texture (Texture::setSDF): the
// plane holding it keeps the add order, so the text still draws on top.
SSS::GL::Texture::Shared makePanel(int w, int h, float radius, float border,
    glm::vec4 fill, glm::vec4 edge, glm::vec4 inner_line);
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
private:
    std::unordered_map<std::string, SSS::GL::Texture::Shared> _cache;
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
    bool visible = true;

    void set(SSS::GL::Texture::Shared t, glm::vec2 f, float h);
    void hide() { set(nullptr, foot, height); }
    void refresh();

private:
    struct Key {
        void const* tex = nullptr; int tw = -1, th = -1; glm::vec2 f{ -1.f }; float h = -1.f; bool visible = false;
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
    Settings settings;
    // Shown on the story's End step (TR markup), "" to finish right away
    std::string end_text;

    std::optional<Layout> L;
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

    // Creates the planes & text nodes in ctx.ui, for the layout ctx.L
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
