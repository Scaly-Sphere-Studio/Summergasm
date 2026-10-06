#include "DialogNodes.hpp"
#include "Settings.hpp"

#include <algorithm>
#include <cmath>
#include <random>

namespace dialog {

using SSS::GL::Polyline;

namespace {

constexpr int      CIRCLE_POINTS = 24;

glm::vec3 v3(glm::vec2 p) { return glm::vec3(p, 0.f); }

// From the point (t = 0) to the base (t = 1)
SSS::Math::Gradient<float> taper()
{
    float const tip = TAIL_W_TIP, box = TAIL_W_BOX;
    SSS::Math::Gradient<float> g;
    g.push(std::pair<float, float const&>(0.f, tip));
    g.push(std::pair<float, float const&>(1.f, box));
    return g;
}

SSS::Math::Gradient<glm::vec4> plain(glm::vec4 const& c)
{
    SSS::Math::Gradient<glm::vec4> g;
    g.push(std::pair<float, glm::vec4 const&>(0.f, c));
    return g;
}

// Random value in [lo, hi)
float roll(std::mt19937& rng, float lo, float hi)
{
    return std::uniform_real_distribution<float>(lo, hi)(rng);
}

// Cubic Bezier, from the point a to the base d
struct Curve {
    glm::vec2 a, b, c, d;
    glm::vec2 at(float t) const
    {
        float const u = 1.f - t;
        return u * u * u * a + 3.f * u * u * t * b + 3.f * u * t * t * c + t * t * t * d;
    }
    glm::vec2 tangent(float t) const
    {
        float const u = 1.f - t;
        return 3.f * u * u * (b - a) + 6.f * u * t * (c - b) + 3.f * t * t * (d - c);
    }
};

// Ballistic: as if thrown sideways from the point, falling into the box (a
// quadratic Bezier whose control point is level with a, halfway across)
Curve ballistic(glm::vec2 a, glm::vec2 d)
{
    glm::vec2 const p((a.x + d.x) / 2.f, a.y);
    return { a, a + (p - a) * (2.f / 3.f), d + (p - d) * (2.f / 3.f), d };
}

// Leaving the point along dir, coming down into the box (d being
// TAIL_INSET_PX deep in it)
Curve bent(glm::vec2 a, glm::vec2 dir, glm::vec2 d)
{
    return { a, a + dir * (TAIL_BEND_ARM * glm::length(d - a)),
             d - glm::vec2(0.f, TAIL_INSET_PX + TAIL_BEND_RISE_PX), d };
}

Polyline::Vertex::Vec smoothPath(Curve const& curve)
{
    Polyline::Vertex::Vec path;
    for (int i = 0; i < TAIL_BEND_POINTS; ++i)
        path.emplace_back(v3(curve.at(float(i) / (TAIL_BEND_POINTS - 1))));
    return path;
}

// Zigzagging around the curve. Corners alternately on each side of it, their
// place and distance rolled from the seed: the same seed gives the same
// corners, so that it doesn't flicker.
Polyline::Vertex::Vec brokenPath(Curve const& curve, uint32_t seed)
{
    std::mt19937 rng(seed);
    Polyline::Vertex::Vec path{ v3(curve.a) };
    for (int i = 1; i <= BROKEN_CORNERS; ++i) {
        float const t = (float(i) + roll(rng, -BROKEN_SHIFT, BROKEN_SHIFT)) / (BROKEN_CORNERS + 1);
        glm::vec2 const tangent = curve.tangent(t);
        float const len = glm::length(tangent);
        if (len < 1e-3f) continue;
        float const sign = i % 2 ? 1.f : -1.f;
        float const amp = BROKEN_AMP_PX * roll(rng, 1.f - BROKEN_AMP_VAR, 1.f + BROKEN_AMP_VAR);
        path.emplace_back(v3(curve.at(t) + glm::vec2(-tangent.y, tangent.x) / len * sign * amp));
    }
    path.emplace_back(v3(curve.d));
    return path;
}

Polyline::Vertex::Vec circlePath(glm::vec2 center, float r)
{
    Polyline::Vertex::Vec path;
    for (int i = 0; i < CIRCLE_POINTS; ++i) {
        float const a = 6.2831853f * i / CIRCLE_POINTS;
        path.emplace_back(v3(center + r * glm::vec2(std::cos(a), std::sin(a))));
    }
    return path;
}

// Rounded to half pixels: the shapes are rebuilt when it changes
float px(float v) { return std::round(v * 2.f); }

} // namespace

void DialogTails::build(DialogContext&)
{
    _shapes.clear();
    _order.clear();
}

void DialogTails::destroy()
{
    _shapes.clear();
    _order.clear();
}

void DialogTails::present(DialogContext&, renpy::Step const& step, bool)
{
    // Other speakers: new tails (rolled again), the same ones keep theirs
    std::vector<std::string> speakers;
    for (auto const& s : step.speakers) speakers.push_back(s.id);
    if (speakers != _speakers) {
        _speakers = std::move(speakers);
        _seeds.clear();
        for (auto const& id : _speakers) _seeds[id] = _rng();
    }
    _say = step.kind != renpy::Step::Kind::End;
    _style.reset();
    if (step.tail) _style = parseTailStyle(*step.tail);
    _intensity = step.tail_intensity;
}

void DialogTails::update(DialogContext& ctx, DialogInput&)
{
    Layout const& L = *ctx.L;
    auto const mouths = _characters.mouths(ctx);

    if (!debug_mouths) _debug_logged = false;
    else if (!_debug_logged && !mouths.empty()) {
        _debug_logged = true;
        for (auto const& m : mouths)
            LOG_CTX_MSG("debug_mouths", m.tag + ": mouth at (" + std::to_string(int(m.pos.x)) + ", "
                + std::to_string(int(m.pos.y)) + ") px, image (" + std::to_string(m.info.mouth.x) + ", "
                + std::to_string(m.info.mouth.y) + ")");
    }

    // The shapes wanted this frame, in draw order. Only the changed ones are rebuilt.
    std::vector<std::string> order;
    bool changed = false;
    auto const want = [&](std::string id, std::vector<float> key, auto const& make) {
        Shape& s = _shapes[id];
        if (s.lines.empty() || s.key != key) {
            s.key = std::move(key);
            s.lines = make();
            changed = true;
        }
        order.push_back(std::move(id));
    };

    // Over everything else: hidden while the log or the choices are, and
    // when turned off in the settings
    bool const tails = settings::get().dialog_tails && _say && _box.isEnabled() && !ctx.settings.log_open && !_choices.shown();
    struct Tail { DialogCharacters::Mouth const* m; TailStyle style; float base_x; uint32_t seed; };
    std::vector<Tail> list;
    float const margin = L.pad + TAIL_W_BOX;
    float const min_x = L.box_left + margin, max_x = L.box_left + L.box_w - margin;
    for (auto const& m : mouths) {
        if (!tails || !m.talking) continue;
        TailStyle const style = _style.value_or(m.info.tail);
        float const intensity = std::clamp(_intensity.value_or(m.info.intensity), 0.f, 1.f);
        // Its base is on the side the mouth faces, further with the intensity,
        // and rolled around there: more so with the intensity
        float side = m.dir.x > 0.05f ? 1.f : m.dir.x < -0.05f ? -1.f : 0.f;
        if (side == 0.f) side = m.foot.x <= L.W / 2.f ? 1.f : -1.f;
        uint32_t const seed = _seeds.contains(m.tag) ? _seeds.at(m.tag) : 0;
        std::mt19937 rng(seed ^ 0x9e3779b9u);
        float const jitter = TAIL_JITTER_PX + intensity * TAIL_JITTER_INTENSITY_PX;
        float const x = m.pos.x + side * (TAIL_REACH_PX + intensity * TAIL_PUSH_PX) + roll(rng, -jitter, jitter);
        list.push_back({ &m, style, std::clamp(x, min_x, max_x), seed });
    }

    // Several speakers: their bases are in the order of their mouths, so that
    // the tails don't cross (one turned the other way then bends), at least
    // TAIL_SPREAD_PX apart, pushed to the right, then back to the left if past
    // the end of the box.
    std::stable_sort(list.begin(), list.end(), [](Tail const& l, Tail const& r) {
        return l.m->pos.x != r.m->pos.x ? l.m->pos.x < r.m->pos.x : l.base_x < r.base_x;
    });
    for (size_t i = 1; i < list.size(); ++i)
        list[i].base_x = std::max(list[i].base_x, list[i - 1].base_x + TAIL_SPREAD_PX);
    for (size_t i = list.size(); i-- > 0;)
        list[i].base_x = std::min(list[i].base_x, i + 1 < list.size() ? list[i + 1].base_x - TAIL_SPREAD_PX : max_x);
    for (auto& t : list) t.base_x = std::max(t.base_x, min_x);

    for (auto const& [mp, style, base_x, seed] : list) {
        auto const& m = *mp;
        glm::vec2 const d(base_x, L.box_top + TAIL_INSET_PX);

        // The point heads from the base toward the mouth: a straight tail only
        // hints at the speaker, a broken one reaches the mouth (TAIL_GAP_PX
        // off it). Never above the middle of the screen.
        glm::vec2 const target = m.pos + m.dir * TAIL_GAP_PX;
        float const dist = glm::length(target - d);
        glm::vec2 const toward = dist > 1.f ? (target - d) / dist : glm::vec2(0.f, -1.f);
        // Unless alone speaking, its mouth turned away from the base: then it
        // bends from the mouth back into the box, reaching it whatever the style
        bool const bend = list.size() == 1 && dist > 1.f && glm::dot(m.dir, -toward) < TAIL_BEND_DOT;
        glm::vec2 a = target;
        if (bend) a.y = std::max(a.y, L.H / 2.f);
        else {
            // A straight one's length counts out of the box: its base, deep in
            // the box, doesn't make it shorter
            float const hidden = TAIL_INSET_PX / std::max(-toward.y, 0.2f);
            float len = style == TailStyle::Broken ? dist : std::min(dist, TAIL_LEN_PX + hidden);
            if (toward.y < -1e-3f)
                len = std::min(len, (d.y - L.H / 2.f) / -toward.y);
            len = std::max(len, 1.f);
            a = d + toward * len;
        }

        std::vector<float> key{ px(a.x), px(a.y), px(d.x), px(d.y), float(style), float(seed & 0xFFFFFF), float(bend) };
        if (bend) key.insert(key.end(), { px(m.dir.x * 100.f), px(m.dir.y * 100.f) });
        want("tail:" + m.tag, std::move(key), [&] {
            Curve const curve = bend ? bent(a, m.dir, d) : ballistic(a, d);
            auto const path = style == TailStyle::Broken ? brokenPath(curve, seed)
                            : bend                       ? smoothPath(curve)
                                                         : Polyline::Vertex::Vec{ v3(a), v3(d) };
            auto const joint = style == TailStyle::Broken ? Polyline::JointType::MITER : Polyline::JointType::ROUND;
            return std::vector<Polyline::Shared>{
                Polyline::Line(path, taper(), plain(color(BOX_RGB)), joint, Polyline::TermType::BUTT),
            };
        });
    }

    if (debug_mouths) {
        for (auto const& m : mouths) {
            want("debug:" + m.tag, { px(m.pos.x), px(m.pos.y), px(m.dir.x * 100.f), px(m.dir.y * 100.f) }, [&] {
                return std::vector<Polyline::Shared>{
                    Polyline::Line(circlePath(m.pos, DEBUG_MOUTH_R), 2.f, color(0xff00ff),
                        Polyline::JointType::BEVEL, Polyline::TermType::CONNECT),
                    Polyline::Segment(v3(m.pos), v3(m.pos + m.dir * DEBUG_DIR_LEN), 2.f, color(0xffff00)),
                };
            });
        }
    }

    if (!changed && order == _order) return;
    std::erase_if(_shapes, [&](auto const& kv) { return std::find(order.begin(), order.end(), kv.first) == order.end(); });
    // Lines are drawn in the order they are added: all of them again, in order
    ctx.lines->clearLines();
    for (auto const& id : order) {
        for (auto const& line : _shapes[id].lines) {
            // Points in UI pixels (Y-down, origin at the top-left) to NDC, the
            // renderer having no camera. In front of the UI planes.
            line->setScaling(glm::vec3(2.f / L.W, -2.f / L.H, 1.f));
            line->setTranslation(glm::vec3(-1.f, 1.f, -0.5f));
            ctx.lines->addLine(line);
        }
    }
    _order = std::move(order);
}

} // namespace dialog
