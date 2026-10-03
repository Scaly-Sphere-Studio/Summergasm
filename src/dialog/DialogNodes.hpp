#pragma once
// The parts of the dialog node, built by the Dialog in this order (= draw
// order): characters, box, choices, controls, log.
#include "DialogCommon.hpp"

namespace dialog {

// Sprites of the characters on screen. Characters who are not talking are
// drawn in black & white; characters sharing a position line up (the ones
// behind shifted inward and scaled down). The ones on the right side face the
// center (mirrored). A character whose place changes moves there, leaning.
class DialogCharacters : public DialogNode {
public:
    void build(DialogContext& ctx) override;
    void destroy() override;
    void present(DialogContext& ctx, renpy::Step const& step, bool animate) override;
    void update(DialogContext& ctx, DialogInput& input) override;

protected:
    void _refreshVisibility() override;

private:
    void _animate();

    struct Pose {
        glm::vec2 foot{ 0.f };
        float height = 0.f;
        bool mirror = false;
        bool operator==(Pose const&) const = default;
    };
    // Where a character stands (_slots[i] is for _sprites[i]) and, while it
    // moves, where it comes from.
    struct Slot {
        std::string tag;
        Pose to;
        std::optional<Pose> from;
        Clock::time_point start;
    };

    std::vector<ImagePlane> _sprites;
    std::vector<Slot> _slots;
};

// Dialogue box: speaker name, line revealed character per character (the
// Area's Typewriter print mode) and the "continue" arrow.
class DialogBox : public DialogNode {
public:
    void build(DialogContext& ctx) override;
    void destroy() override;
    void present(DialogContext& ctx, renpy::Step const& step, bool animate) override;
    void update(DialogContext& ctx, DialogInput& input) override;

    bool typewriterDone() const noexcept { return _typewriter && _typewriter->done; }
    // Also shows the rest of the line when it is skipped.
    void finishLine(DialogContext& ctx);
    // Applies ctx.settings.text_speed, to the line being typed too.
    void applyTextSpeed(DialogContext& ctx);
    // "Next line" input: the arrow snaps down
    void pressed() { _pressed_at = Clock::now(); }

protected:
    void _refreshVisibility() override;

private:
    void _showLine(DialogContext& ctx, std::string const& str, bool animate);

    SSS::GL::Plane::Shared _box, _arrow;
    float _arrow_y = 0.f;
    SSS::TR::Format _name_fmt;
    // Text nodes are owned by the SceneGraph, see freeText()
    SSS::Node_Text* _name = nullptr;
    SSS::Node_Text* _text = nullptr;
    SSS::TR::Area::Shared _area;
    std::unique_ptr<TypewriterWatcher> _typewriter;
    bool _has_name = false;
    std::optional<Clock::time_point> _pressed_at;
};

// Menu choices, shown CHOICE_DELAY after the menu's line is fully shown.
class DialogChoices : public DialogNode {
public:
    void build(DialogContext& ctx) override;
    void destroy() override;
    void present(DialogContext& ctx, renpy::Step const& step, bool animate) override;

    bool shown() const noexcept { return _shown; }
    // Shows the choices once the line has been shown for CHOICE_DELAY.
    void reveal(DialogContext const& ctx);
    // Selection (Up / Down, mouse) and pick (1-9, confirm, click).
    std::optional<size_t> handleInput(DialogContext& ctx, DialogInput& input);

protected:
    void _refreshVisibility() override;

private:
    glm::vec2 _center(Layout const& L, size_t i) const;
    void _highlight();
    void _show(bool on);
    std::optional<size_t> _underCursor(Layout const& L, glm::vec2 cursor) const;

    SSS::GL::Texture::Shared _tex, _hover_tex;
    std::vector<SSS::GL::Plane::Shared> _boxes;
    std::vector<SSS::Node_Text*> _texts;
    size_t _selected = 0;
    size_t _count = 0;
    bool _shown = false;
};

// Clickable icons stacked in bubbles at the bottom right of the screen: log,
// auto mode, text speed (▶ / ▶▶ / ▶▶▶ / ▶▶▶| instant). Their keyboard
// shortcuts work even when the icons are hidden.
class DialogControls : public DialogNode {
public:
    void build(DialogContext& ctx) override;
    void destroy() override;

    struct Actions {
        bool toggle_log    = false;
        bool speed_changed = false;     // ctx.settings.text_speed
    };
    // Changes ctx.settings.auto_mode itself.
    Actions handleInput(DialogContext& ctx, DialogInput& input);
    // Icons for the current settings and hovered button
    void refresh(DialogContext const& ctx);

protected:
    void _refreshVisibility() override;

private:
    Button _log, _auto, _speed;
    SSS::GL::Texture::Shared _log_icon, _log_icon_hover, _log_icon_active;
    SSS::GL::Texture::Shared _auto_icon, _auto_icon_hover, _auto_icon_active;
    std::vector<SSS::GL::Texture::Shared> _speed_icons, _speed_icons_hover;
    Button const* _hovered = nullptr;
};

// History of the conversation, above the dialogue box. Drawn over the rest:
// built last.
class DialogLog : public DialogNode {
public:
    void build(DialogContext& ctx) override;
    void destroy() override;

    // Opens on the latest line. Sets ctx.settings.log_open.
    void setOpen(DialogContext& ctx, bool open);
    // Mouse wheel / Up / Down / Page Up / Page Down, while open
    void scroll(DialogContext& ctx);

private:
    SSS::GL::Plane::Shared _panel;
    SSS::Node_Text* _title = nullptr;
    SSS::Node_Text* _node = nullptr;
    SSS::TR::Area::Shared _area;
};

} // namespace dialog
