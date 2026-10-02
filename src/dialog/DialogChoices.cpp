#include "DialogNodes.hpp"

#include <algorithm>
#include <cmath>

namespace dialog {

void DialogChoices::build(DialogContext& ctx)
{
    Layout const& L = *ctx.L;

    _tex = makePanel(int(L.choice_w), int(L.choice_h), 14.f, 2.f,
        color(PANEL_RGB, 0.85f), color(0x9d93bd), color(0xffffff, 0.15f));
    _hover_tex = makePanel(int(L.choice_w), int(L.choice_h), 14.f, 4.f,
        color(PANEL_RGB, 0.94f), color(0xf2b632), color(0xf2b632, 0.4f));  // gold border when selected
    _boxes.clear();
    for (size_t i = 0; i < MAX_CHOICES; ++i) {
        auto p = SSS::GL::Plane::create(_tex);
        p->setScaling(glm::vec3(L.choice_h));
        p->setAlpha(0.f);
        ctx.ui->addPlane(p);
        _boxes.push_back(p);
    }

    // Text nodes after the boxes: drawn over them
    _texts.clear();
    for (size_t i = 0; i < MAX_CHOICES; ++i) {
        auto* t = ctx.makeText(textFormat(24));
        t->model->getTextArea()->setWrappingMaxWidth(int(L.choice_w - 2.f * L.pad));
        place(t, SSS::AnchorMode::Center, { 0.f, 0.f });
        setVisible(t, false);
        _texts.push_back(t);
    }
    _count = 0;
    _selected = 0;
    _shown = false;
}

void DialogChoices::destroy()
{
    for (auto*& t : _texts) freeText(t);
    _texts.clear();
    _boxes.clear();
    _tex.reset();
    _hover_tex.reset();
    _count = 0;
    _shown = false;
}

// Laid out now, shown by reveal() once the line is done.
void DialogChoices::present(DialogContext& ctx, renpy::Step const& step, bool)
{
    Layout const& L = *ctx.L;
    _count = step.kind == renpy::Step::Kind::Menu ? std::min(step.choices.size(), MAX_CHOICES) : 0;
    _selected = 0;
    _show(false);
    for (size_t i = 0; i < _count; ++i) {
        glm::vec2 const c = _center(L, i);
        _boxes[i]->setTranslation(glm::vec3(c, 0.f));
        _texts[i]->setText(step.choices[i]);
        place(_texts[i], SSS::AnchorMode::Center, c);
    }
    _highlight();
}

void DialogChoices::reveal(DialogContext const& ctx)
{
    if (_count > 0 && !_shown && !ctx.typing && ctx.shownFor() >= CHOICE_DELAY)
        _show(true);
}

std::optional<size_t> DialogChoices::handleInput(DialogContext& ctx, DialogInput& input)
{
    if (!_enabled || !_shown || _count == 0) return std::nullopt;
    Layout const& L = *ctx.L;
    auto const& keys = ctx.window.getKeyInputs();

    size_t const prev = _selected;
    if (keys[GLFW_KEY_UP].is_pressed())   _selected = (_selected + _count - 1) % _count;
    if (keys[GLFW_KEY_DOWN].is_pressed()) _selected = (_selected + 1) % _count;
    auto const hovered = _underCursor(L, input.cursor);
    if (hovered && (input.cursor_moved || input.click))
        _selected = *hovered;
    if (_selected != prev) _highlight();

    std::optional<size_t> pick;
    for (size_t i = 0; i < _count && i < 9; ++i)
        if (keys[GLFW_KEY_1 + int(i)].is_pressed()) pick = i;
    if (input.confirm || (input.click && hovered)) pick = _selected;
    if (input.click && hovered) input.click = false;
    return pick;
}

glm::vec2 DialogChoices::_center(Layout const& L, size_t i) const
{
    float const total = _count * L.choice_h + (_count - 1) * L.choice_gap;
    float const top   = (L.box_top - total) / 2.f;
    return glm::vec2(L.W / 2.f, top + i * (L.choice_h + L.choice_gap) + L.choice_h / 2.f);
}

void DialogChoices::_highlight()
{
    for (size_t i = 0; i < _count; ++i)
        _boxes[i]->setTexture(i == _selected ? _hover_tex : _tex);
}

void DialogChoices::_show(bool on)
{
    _shown = on;
    for (size_t i = 0; i < MAX_CHOICES; ++i) {
        bool const visible = _enabled && on && i < _count;
        _boxes[i]->setAlpha(visible ? 1.f : 0.f);
        setVisible(_texts[i], visible);
    }
}

std::optional<size_t> DialogChoices::_underCursor(Layout const& L, glm::vec2 cursor) const
{
    for (size_t i = 0; i < _count; ++i) {
        glm::vec2 const c = _center(L, i);
        if (std::abs(cursor.x - c.x) <= L.choice_w / 2.f && std::abs(cursor.y - c.y) <= L.choice_h / 2.f)
            return i;
    }
    return std::nullopt;
}

void DialogChoices::_refreshVisibility()
{
    if (_boxes.empty()) return;
    _show(_shown);
}

} // namespace dialog
