#include "DialogNodes.hpp"

#include <algorithm>
#include <cmath>

namespace dialog {

void DialogCharacters::build(DialogContext& ctx)
{
    _sprites.resize(MAX_SPRITES);
    for (auto& sp : _sprites) {
        sp.visible = _enabled;
        ctx.ui->addPlane(sp.plane);
    }
}

void DialogCharacters::destroy()
{
    _sprites.clear();
}

void DialogCharacters::present(DialogContext& ctx, renpy::Step const& step, bool)
{
    Layout const& L = *ctx.L;
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
        auto tex = ctx.textures.get(sp.image);
        tex->setGrayscale(!talking);
        placements.push_back({ tex,
            { L.spriteX(sp.pos) + dir * depth * L.stack_dx, L.H - depth * L.stack_rise },
            L.sprite_h * std::pow(L.stack_scale, float(depth)), depth });
    }
    // Sprite planes were added back to front: fill them from the back row.
    std::stable_sort(placements.begin(), placements.end(),
        [](Placement const& a, Placement const& b) { return a.depth > b.depth; });
    for (size_t i = 0; i < _sprites.size(); ++i) {
        if (i < placements.size())
            _sprites[i].set(placements[i].tex, placements[i].foot, placements[i].height);
        else
            _sprites[i].hide();
    }
}

void DialogCharacters::update(DialogContext&, DialogInput&)
{
    for (auto& sp : _sprites) sp.refresh();
}

void DialogCharacters::_refreshVisibility()
{
    for (auto& sp : _sprites) {
        sp.visible = _enabled;
        sp.refresh();
    }
}

} // namespace dialog
