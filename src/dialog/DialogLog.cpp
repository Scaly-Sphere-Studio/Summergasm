#include "DialogNodes.hpp"

#include <algorithm>

namespace dialog {

// Above the dialogue box, as wide as it and without covering it (the
// settings bubbles are beside the box, below the log).
void DialogLog::build(DialogContext& ctx)
{
    Layout const& L = *ctx.L;
    float const log_gap = 24.f;                 // to the top of the screen, and to the box
    float const log_w = L.box_w, log_left = L.box_left;
    float const log_top = log_gap, log_h = L.box_top - log_gap - log_top;
    float const log_title_h = 48.f;

    _panel = SSS::GL::Plane::create(makePanel(int(log_w), int(log_h), 18.f, 3.f,
        color(PANEL_RGB, 0.95f), color(0xc9bfe8), color(0xf2b632, 0.45f)));
    _panel->setScaling(glm::vec3(std::min(log_w, log_h)));
    _panel->setTranslation(glm::vec3(L.W / 2.f, log_top + log_h / 2.f, 0.f));
    _panel->setAlpha(0.f);
    ctx.ui->addPlane(_panel);

    _title = ctx.makeText(textFormat(28, 0xf2b632));
    _title->setText("Historique");
    place(_title, SSS::AnchorMode::CenterTop, { L.W / 2.f, log_top + L.pad * 0.8f });
    setVisible(_title, false);

    // Fixed size area (setDimensions() turns the auto-sizing off, lines still
    // break at its width): its content scrolls.
    _node = ctx.makeText(textFormat(22));
    _area = _node->model->getTextArea();
    _area->setDimensions(int(log_w - 2.f * L.pad), int(log_h - log_title_h - 1.5f * L.pad));
    place(_node, SSS::AnchorMode::TopLeft, { log_left + L.pad, log_top + log_title_h + L.pad * 0.5f });
    setVisible(_node, false);
}

void DialogLog::destroy()
{
    _area.reset();
    freeText(_title);
    freeText(_node);
    _panel.reset();
}

void DialogLog::setOpen(DialogContext& ctx, bool open)
{
    ctx.settings.log_open = open;
    _panel->setAlpha(open ? 1.f : 0.f);
    setVisible(_title, open);
    setVisible(_node, open);
    _area->clear();                         // closed: its effects stop redrawing the hidden text
    if (!open || !ctx.player) return;
    std::string str;
    for (auto const& e : ctx.player->log()) {
        if (!str.empty()) str += "\n\n";
        if (e.choice)
            str += renpy::colored("\xC2\xBB " + e.text, 0xf2b632);   // "» choice", in gold
        else if (!e.speaker_name.empty())       // spoken: indented under the name
            str += (e.speaker_markup.empty() ? renpy::colored(e.speaker_name, e.speaker_color.value_or(0xc9bfe8))
                                             : e.speaker_markup) + "\n" + LOG_INDENT + e.text;
        else
            str += e.text;
    }
    _node->setText(str);
    // Its pixels are drawn asynchronously: until they are in, the area
    // shows the previous ones (scrolling is clamped to them).
    _area->scroll(1'000'000);               // clamped to the bottom
}

void DialogLog::scroll(DialogContext& ctx)
{
    auto const& keys = ctx.window.getKeyInputs();
    int pixels = int(-g->scroll_y * LOG_SCROLL_STEP);
    if (keys[GLFW_KEY_UP].is_pressed())        pixels -= LOG_SCROLL_STEP;
    if (keys[GLFW_KEY_DOWN].is_pressed())      pixels += LOG_SCROLL_STEP;
    if (keys[GLFW_KEY_PAGE_UP].is_pressed())   pixels -= _area->getHeight();
    if (keys[GLFW_KEY_PAGE_DOWN].is_pressed()) pixels += _area->getHeight();
    if (pixels != 0) _area->scroll(pixels);
}

} // namespace dialog
