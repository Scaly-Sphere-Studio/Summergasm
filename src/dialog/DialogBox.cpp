#include "DialogNodes.hpp"

#include <algorithm>

namespace dialog {

void DialogBox::build(DialogContext& ctx)
{
    Layout const& L = *ctx.L;

    _box = SSS::GL::Plane::create(makePanel(int(L.box_w), int(L.box_h), 18.f, 3.f,
        color(PANEL_RGB, 0.85f), color(0xc9bfe8), color(0xf2b632, 0.45f)));
    _box->setScaling(glm::vec3(L.box_h));   // smaller side = height, width follows the ratio
    _box->setTranslation(glm::vec3(L.W / 2.f, L.box_top + L.box_h / 2.f, 0.f));
    ctx.ui->addPlane(_box);

    // Shown at the bottom of the dialogue box while the player can go forward
    // (hidden on menus); snaps down on each "next line" input.
    _arrow = SSS::GL::Plane::create(makeContinueArrow(color(0xf2b632), color(PANEL_RGB)));
    _arrow->setScaling(glm::vec3(20.f));    // texture's smaller side, drawn 1:1
    _arrow->setAlpha(0.f);
    _arrow_y = L.box_top + L.box_h - L.pad * 0.8f;
    _arrow->setTranslation(glm::vec3(L.W / 2.f, _arrow_y, 0.f));
    ctx.ui->addPlane(_arrow);

    // Text nodes add their own plane to the UIRenderer on construction.
    _name_fmt = textFormat(28);
    _name = ctx.makeText(_name_fmt);

    // Node_Text::setWrappingMin() only sets a minimum; the max wrap width
    // lives on the underlying TR::Area.
    _text = ctx.makeText(textFormat(24));
    _area = _text->model->getTextArea();
    _area->setWrappingMaxWidth(int(L.box_w - 2.f * L.pad));
    place(_text, SSS::AnchorMode::TopLeft, { L.box_left + L.pad, L.box_top + L.pad + 40.f });
    _typewriter = std::make_unique<TypewriterWatcher>(*_area);

    _has_name = false;
    _refreshVisibility();
}

void DialogBox::destroy()
{
    _typewriter.reset();
    _area.reset();
    freeText(_name);
    freeText(_text);
    _box.reset();
    _arrow.reset();
    _pressed_at.reset();
}

void DialogBox::present(DialogContext& ctx, renpy::Step const& step, bool animate)
{
    Layout const& L = *ctx.L;
    using Kind = renpy::Step::Kind;

    // Speaker name, on the side of the speaker's sprite
    _has_name = !step.speaker.empty() && step.kind != Kind::End;
    setVisible(_name, _enabled && _has_name);
    if (_has_name) {
        SSS::TR::Format fmt = _name_fmt;
        if (step.speaker_color) fmt.text_color = trColor(*step.speaker_color);
        _name->setText(step.speaker_name, fmt);

        auto const* sprite = step.scene.find(step.speaker);
        auto const side = sprite ? sprite->pos : renpy::Pos::Left;
        float const y = L.box_top + L.pad * 0.5f;
        if (side == renpy::Pos::Right)
            place(_name, SSS::AnchorMode::TopRight, { L.box_left + L.box_w - L.pad, y });
        else if (side == renpy::Pos::Center)
            place(_name, SSS::AnchorMode::CenterTop, { L.W / 2.f, y });
        else
            place(_name, SSS::AnchorMode::TopLeft, { L.box_left + L.pad, y });
    }

    // Dialogue
    _showLine(ctx, step.kind == Kind::End ? ctx.end_text : step.text, animate);
}

void DialogBox::update(DialogContext& ctx, DialogInput&)
{
    bool const can_continue = ctx.player && ctx.player->current().kind == renpy::Step::Kind::Say && !ctx.typing;
    _arrow->setAlpha(_enabled && can_continue ? 1.f : 0.f);

    // Continue arrow: still, it snaps down on input then eases back up.
    if (_pressed_at) {
        float const s = std::chrono::duration<float>(Clock::now() - *_pressed_at).count()
                      / ARROW_SNAP_TIME;
        float const k = 1.f - std::min(s, 1.f);
        _arrow->setTranslation(glm::vec3(ctx.L->W / 2.f, _arrow_y + ARROW_SNAP_PX * k * k, 0.f));
        if (s >= 1.f) _pressed_at.reset();
    }
}

void DialogBox::finishLine(DialogContext& ctx)
{
    _area->setPrintMode(SSS::TR::PrintMode::Instant);
    ctx.typing   = false;
    ctx.shown_at = Clock::now();
}

void DialogBox::applyTextSpeed(DialogContext& ctx)
{
    int const cps = TEXT_SPEEDS[ctx.settings.text_speed].cps;
    if (cps > 0)         _area->setTypeWriterSpeed(cps);
    else if (ctx.typing) finishLine(ctx);
}

// Area::parseString() keeps the typewriter cursor where it was (so that
// text can be appended while typing); clear() puts it back at the start.
void DialogBox::_showLine(DialogContext& ctx, std::string const& str, bool animate)
{
    int const cps = TEXT_SPEEDS[ctx.settings.text_speed].cps;
    _area->clear();
    _text->setText(str);
    _typewriter->done = false;              // drop the previous line's event
    // An empty text is never typed, so it would never emit the event.
    if (animate && cps > 0 && hasGlyphs(str)) {
        _area->setTypeWriterSpeed(cps);
        _area->setPrintMode(SSS::TR::PrintMode::Typewriter);
        ctx.typing = true;
    }
    else {
        finishLine(ctx);
    }
}

void DialogBox::_refreshVisibility()
{
    if (!_box) return;
    _box->setAlpha(_enabled ? 1.f : 0.f);
    if (!_enabled) _arrow->setAlpha(0.f);   // shown again by update()
    setVisible(_name, _enabled && _has_name);
    setVisible(_text, _enabled);
}

} // namespace dialog
