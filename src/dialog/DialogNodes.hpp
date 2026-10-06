#pragma once
// The parts of the dialog node, built by the Dialog in this order (= draw
// order): characters, box, choices, controls, log. The speech bubble tails
// are drawn by their own renderer, over all of them.
#include "DialogCommon.hpp"

#include <random>

namespace dialog {

// Sprites of the characters on screen. Characters who are not talking are
// drawn in black & white (setting: grey_characters); characters sharing a position line up (the ones
// behind shifted inward and scaled down). The ones on the right side face the
// center (mirrored). A character whose place changes moves there, leaning.
class DialogCharacters : public DialogNode {
public:
    void build(DialogContext& ctx) override;
    void destroy() override;
    void present(DialogContext& ctx, renpy::Step const& step, bool animate) override;
    void update(DialogContext& ctx, DialogInput& input) override;

    // Mouth of a character on screen (see CharacterData), in UI pixels
    struct Mouth {
        std::string tag;
        bool talking;
        glm::vec2 pos;
        glm::vec2 dir;          // normalized
        glm::vec2 foot;         // the sprite's bottom center
        ExpressionInfo info;
    };
    // The characters shown that have data, front ones last. None while
    // hidden, nor for one turning around (its direction is meaningless).
    std::vector<Mouth> mouths(DialogContext& ctx) const;

protected:
    void _refreshVisibility() override;

private:
    void _animate();
    // Black & white for the ones not talking, if the setting says so
    void _grey();

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
        std::string expression;
        bool talking = false;
        Pose to;
        std::optional<Pose> from;
        Clock::time_point start;
    };

    std::vector<ImagePlane> _sprites;
    std::vector<Slot> _slots;
};

// Dialogue box, a creamy white squircle: speaker name in its color, line
// revealed character per character (the Area's Typewriter print mode) and the
// "continue" arrow.
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

// Speech bubble tails: from the mouth of each character speaking to the top
// of the dialogue box. Straight, or broken (a zigzag, for angry lines); the
// intensity pushes their base on the box away from the character. The
// speaker's expression gives them, a say line may override them (see
// CharacterData, renpy::Step::tail). They follow the sprites as they move.
// Drawn by ctx.lines, over the sprites & the box but under the name and the
// text: hidden while the log or the menu choices are shown, and when turned
// off in the settings (dialog_tails). With debug_mouths, each mouth on screen is shown too: a
// circle around it, and a line along its direction.
class DialogTails : public DialogNode {
public:
    DialogTails(DialogCharacters const& characters, DialogBox const& box, DialogChoices const& choices)
        : _characters(characters), _box(box), _choices(choices) {}

    void build(DialogContext& ctx) override;
    void destroy() override;
    void present(DialogContext& ctx, renpy::Step const& step, bool animate) override;
    void update(DialogContext& ctx, DialogInput& input) override;

private:
    // Lines drawn together (the debug circle & direction) and what they were built
    // from: Polyline points can't change, they are rebuilt when that does.
    struct Shape {
        std::vector<float> key;
        std::vector<SSS::GL::Polyline::Shared> lines;
    };

    DialogCharacters const& _characters;
    DialogBox const& _box;
    DialogChoices const& _choices;
    std::unordered_map<std::string, Shape> _shapes;
    std::vector<std::string> _order;    // of the shapes in ctx.lines
    bool _say = false;
    // The line's override of the speakers' tails
    std::optional<TailStyle> _style;
    std::optional<float> _intensity;
    // The tails' random shape (base & zigzag), rolled again when the speakers change
    std::vector<std::string> _speakers;
    std::unordered_map<std::string, uint32_t> _seeds;
    std::mt19937 _rng{ std::random_device{}() };
    bool _debug_logged = false;
};

} // namespace dialog
