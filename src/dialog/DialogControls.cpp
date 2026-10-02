#include "DialogNodes.hpp"

#include <algorithm>

namespace dialog {

void DialogControls::build(DialogContext& ctx)
{
    Layout const& L = *ctx.L;

    // Icons in the idle, hovered and active colors.
    _log_icon         = makeLogIcon(color(0x9d93bd));
    _log_icon_hover   = makeLogIcon(color(0xffffff));
    _log_icon_active  = makeLogIcon(color(0xf2b632));
    _auto_icon        = makeAutoIcon(color(0x9d93bd));
    _auto_icon_hover  = makeAutoIcon(color(0xffffff));
    _auto_icon_active = makeAutoIcon(color(0xf2b632));
    _speed_icons.clear();
    _speed_icons_hover.clear();
    for (auto const& speed : TEXT_SPEEDS) {
        _speed_icons.push_back(makeSpeedIcon(speed, color(0x9d93bd)));
        _speed_icons_hover.push_back(makeSpeedIcon(speed, color(0xffffff)));
    }

    // Every bubble has the same size, fitting the widest (speed) icon.
    // From top to bottom: log, auto mode, text speed.
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
    makeButton(_log,   _log_icon,                             float(LOG_ICON_SIZE),  log_y);
    makeButton(_auto,  _auto_icon,                            float(AUTO_ICON_SIZE), auto_y);
    makeButton(_speed, _speed_icons[ctx.settings.text_speed], float(SPEED_ICON_H),   speed_y);
    // Bubbles first: textured planes are drawn in the order they are added.
    for (Button* b : { &_log, &_auto, &_speed }) ctx.ui->addPlane(b->bubble);
    for (Button* b : { &_log, &_auto, &_speed }) ctx.ui->addPlane(b->icon);
    _hovered = nullptr;
    refresh(ctx);
    _refreshVisibility();
}

void DialogControls::destroy()
{
    _log = {};
    _auto = {};
    _speed = {};
    _hovered = nullptr;
    _speed_icons.clear();
    _speed_icons_hover.clear();
    _log_icon.reset();
    _log_icon_hover.reset();
    _log_icon_active.reset();
    _auto_icon.reset();
    _auto_icon_hover.reset();
    _auto_icon_active.reset();
}

DialogControls::Actions DialogControls::handleInput(DialogContext& ctx, DialogInput& input)
{
    auto const& keys = ctx.window.getKeyInputs();
    auto& settings = ctx.settings;
    Actions actions;

    // Hidden icons can't be hovered nor clicked
    Button const* hovered = !_enabled                    ? nullptr
                          : _log.contains(input.cursor)   ? &_log
                          : _auto.contains(input.cursor)  ? &_auto
                          : _speed.contains(input.cursor) ? &_speed : nullptr;
    bool changed = hovered != _hovered;
    _hovered = hovered;
    bool const click = input.click;

    // Escape only closes the log: otherwise the host scene handles it
    if (keys[GLFW_KEY_X].is_pressed() || (click && hovered == &_log)
        || (settings.log_open && keys[GLFW_KEY_ESCAPE].is_pressed())) {
        actions.toggle_log = true;
    }
    if (keys[GLFW_KEY_TAB].is_pressed() || (click && hovered == &_auto)) {
        settings.auto_mode = !settings.auto_mode;   // shown_at is kept: a line read for long enough goes on at once
        changed = true;
    }
    if (keys[GLFW_KEY_KP_ADD].is_pressed() && settings.text_speed + 1 < TEXT_SPEED_COUNT) {
        ++settings.text_speed;
        actions.speed_changed = true;
    }
    if (keys[GLFW_KEY_KP_SUBTRACT].is_pressed() && settings.text_speed > 0) {
        --settings.text_speed;
        actions.speed_changed = true;
    }
    if (click && hovered == &_speed) {
        settings.text_speed = (settings.text_speed + 1) % TEXT_SPEED_COUNT;
        actions.speed_changed = true;
    }
    // A click on the settings is not a "next line" input.
    if (hovered) input.click = false;
    // The log icon is refreshed by the Dialog, once the log is toggled
    if (changed || actions.speed_changed) refresh(ctx);
    return actions;
}

void DialogControls::refresh(DialogContext const& ctx)
{
    auto const& settings = ctx.settings;
    _log.icon->setTexture(_hovered == &_log ? _log_icon_hover
                        : settings.log_open ? _log_icon_active : _log_icon);
    _auto.icon->setTexture(_hovered == &_auto  ? _auto_icon_hover
                         : settings.auto_mode ? _auto_icon_active : _auto_icon);
    _speed.icon->setTexture((_hovered == &_speed ? _speed_icons_hover : _speed_icons)[settings.text_speed]);
}

void DialogControls::_refreshVisibility()
{
    if (!_log.bubble) return;
    float const alpha = _enabled ? 1.f : 0.f;
    for (Button* b : { &_log, &_auto, &_speed }) {
        b->bubble->setAlpha(alpha);
        b->icon->setAlpha(alpha);
    }
}

} // namespace dialog
