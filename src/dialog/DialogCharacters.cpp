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
    _slots.clear();
}

void DialogCharacters::present(DialogContext& ctx, renpy::Step const& step, bool animate)
{
    Layout const& L = *ctx.L;
    using Kind = renpy::Step::Kind;

    // Characters sharing a position line up in script order: the first
    // one stands in front, the next ones behind it, shifted inward (on
    // the center spot: alternately right and left).
    struct Placement { std::string tag; SSS::GL::Texture::Shared tex; Pose pose; int depth; };
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
        // The ones on the right side face the center
        placements.push_back({ sp.tag, tex,
            { { L.spriteX(sp.pos) + dir * depth * L.stack_dx, L.H - depth * L.stack_rise },
              L.sprite_h * std::pow(L.stack_scale, float(depth)),
              sp.pos == renpy::Pos::Right },
            depth });
    }
    // Sprite planes were added back to front: fill them from the back row.
    std::stable_sort(placements.begin(), placements.end(),
        [](Placement const& a, Placement const& b) { return a.depth > b.depth; });

    // A character that was already there and whose place changed moves to its
    // new one, unless going back / rebuilding (animate is false).
    std::vector<Slot> slots;
    auto const now = Clock::now();
    for (auto const& p : placements) {
        Slot slot{ p.tag, p.pose, std::nullopt, now };
        auto const old = std::find_if(_slots.begin(), _slots.end(),
            [&](Slot const& s) { return s.tag == p.tag; });
        if (animate && old != _slots.end()) {
            if (old->to == p.pose) {            // same destination: keep going
                slot.from = old->from;
                slot.start = old->start;
            }
            else {
                slot.from = old->to;
            }
        }
        slots.push_back(std::move(slot));
    }
    _slots = std::move(slots);

    for (size_t i = 0; i < _sprites.size(); ++i) {
        if (i < placements.size())
            _sprites[i].set(placements[i].tex, _slots[i].to.foot, _slots[i].to.height, _slots[i].to.mirror);
        else
            _sprites[i].hide();
    }
    _animate();
}

void DialogCharacters::_animate()
{
    for (size_t i = 0; i < _slots.size() && i < _sprites.size(); ++i) {
        Slot& s = _slots[i];
        if (!s.from) continue;
        float const t = std::chrono::duration<float>(Clock::now() - s.start).count() / SPRITE_MOVE_TIME;
        if (t >= 1.f) {
            s.from.reset();
            _sprites[i].set(_sprites[i].tex, s.to.foot, s.to.height, s.to.mirror);
            continue;
        }
        float const e = t * t * (3.f - 2.f * t);                    // smoothstep
        Pose const& a = *s.from;
        float const dir = s.to.foot.x < a.foot.x ? -1.f : 1.f;
        // Faces its new side from halfway
        _sprites[i].set(_sprites[i].tex,
            glm::mix(a.foot, s.to.foot, e), glm::mix(a.height, s.to.height, e),
            t < 0.5f ? a.mirror : s.to.mirror,
            dir * SPRITE_TILT_DEG * std::sin(t * 3.14159265f));
    }
}

void DialogCharacters::update(DialogContext&, DialogInput&)
{
    _animate();
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
