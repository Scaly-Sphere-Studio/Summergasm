#include "MapContent.hpp"
#include "GameState.hpp"

#include <filesystem>
#include <fstream>
#include <limits>
#include <numbers>
#include <set>

namespace fs = std::filesystem;
using nlohmann::json;

namespace map_content {

// What's drawn of a monument or a pin, drawn once (images supersampled when
// shrunk), premultiplied, turned 180 degrees as the paper, centered: drawn
// again from it at any scale (cheap), or shown as is by an overlay (texture)
struct Sprite {
    int w{ 0 }, h{ 0 };
    std::vector<glm::vec4> px;
    SSS::GL::Texture::Shared texture;

    glm::vec2 half() const { return glm::vec2(w, h) / 2.f; }

    // Bilinear, transparent off it. d: map offset from its center, at scale 1.
    glm::vec4 sample(glm::vec2 d) const
    {
        glm::vec2 const t = half() - d - 0.5f;
        glm::vec2 const f = glm::fract(t);
        int const x = static_cast<int>(std::floor(t.x)), y = static_cast<int>(std::floor(t.y));
        auto const at = [this](int tx, int ty) {
            return tx < 0 || ty < 0 || tx >= w || ty >= h ? glm::vec4(0.f) : px[static_cast<size_t>(ty) * w + tx];
        };
        return glm::mix(glm::mix(at(x, y), at(x + 1, y), f.x), glm::mix(at(x, y + 1), at(x + 1, y + 1), f.x), f.y);
    }
};

namespace {
    constexpr char const* data_file = "map/map.json";
    // Animations, in seconds
    constexpr float pop_time = 0.4f;
    constexpr float road_time = 0.8f;
    // Pause between the steps (places popping, each road, characters popping)
    constexpr float step_delay = 0.15f;
    // Roads are SDFs, cheap to bake finer than the paper
    constexpr float roads_scale = 2.f;
    constexpr float hover_scale = 1.08f;
    constexpr float hover_brighten = 0.15f;
    // Monuments' texels at least this opaque are clickable
    constexpr float hit_alpha = 0.3f;

    // "#rrggbb" or "#rrggbbaa"
    glm::vec4 parseColor(json const& j, char const* key, glm::vec4 fallback)
    {
        auto const it = j.find(key);
        if (it == j.end() || !it->is_string())
            return fallback;
        std::string s = it->get<std::string>();
        if (!s.empty() && s[0] == '#')
            s.erase(0, 1);
        if (s.size() != 6 && s.size() != 8)
            return fallback;
        uint32_t v = static_cast<uint32_t>(std::stoul(s, nullptr, 16));
        if (s.size() == 6)
            v = v << 8 | 0xff;
        return glm::vec4((v >> 24 & 0xff) / 255.f, (v >> 16 & 0xff) / 255.f,
            (v >> 8 & 0xff) / 255.f, (v & 0xff) / 255.f);
    }

    glm::vec2 parseVec2(json const& j)
    {
        if (!j.is_array() || j.size() != 2)
            SSS::throw_exc("expected [x, y], got " + j.dump());
        return glm::vec2(j[0].get<float>(), j[1].get<float>());
    }

    Data load() try
    {
        fs::path const path = fs::path(g->assets_folder) / data_file;
        std::ifstream file(path, std::ios::binary);
        if (!file)
            SSS::throw_exc("cannot read " + path.string());
        json const j = json::parse(file);

        Data d;
        if (j.contains("style")) {
            json const& s = j["style"];
            Style& st = d.style;
            st.road_width = s.value("road_width", st.road_width);
            st.road_outline_width = s.value("road_outline_width", st.road_outline_width);
            st.road_outline = parseColor(s, "road_outline", st.road_outline);
            st.station_radius = s.value("station_radius", st.station_radius);
            st.station_ring = s.value("station_ring", st.station_ring);
            st.station_fill = parseColor(s, "station_fill", st.station_fill);
            st.interchange_ring = parseColor(s, "interchange_ring", st.interchange_ring);
            st.pin_tip_length = s.value("pin_tip_length", st.pin_tip_length);
            st.pin_tip_width = s.value("pin_tip_width", st.pin_tip_width);
            st.pin_outline = s.value("pin_outline", st.pin_outline);
            st.pin_icon_size = s.value("pin_icon_size", st.pin_icon_size);
            st.pin_ring = s.value("pin_ring", st.pin_ring);
        }
        for (json const& l : j.value("locations", json::array())) {
            Location loc;
            loc.id = l.at("id").get<std::string>();
            loc.name = l.value("name", loc.id);
            loc.image = l.value("image", "");
            loc.pos = parseVec2(l.at("pos"));
            loc.size = l.value("size", loc.size);
            loc.scene = l.value("scene", "");
            loc.unlocked = l.value("unlocked", false);
            loc.pin_ring = l.value("pin_ring", -1.f);
            d.locations.push_back(std::move(loc));
        }
        for (json const& r : j.value("roads", json::array())) {
            Road road;
            road.id = r.at("id").get<std::string>();
            road.from = r.at("from").get<std::string>();
            road.to = r.at("to").get<std::string>();
            for (json const& p : r.value("via", json::array()))
                road.via.push_back(parseVec2(p));
            road.color = parseColor(r, "color", road.color);
            road.octilinear = r.value("octilinear", true);
            road.unlocked = r.value("unlocked", false);
            if (!d.location(road.from) || !d.location(road.to))
                LOG_CTX_WRN("map_content", "road '" + road.id + "' joins unknown locations");
            d.roads.push_back(std::move(road));
        }
        for (json const& c : j.value("characters", json::array())) {
            Character ch;
            ch.id = c.at("id").get<std::string>();
            ch.icon = c.value("icon", "");
            ch.color = parseColor(c, "color", ch.color);
            ch.location = c.value("location", "");
            ch.unlocked = c.value("unlocked", false);
            d.characters.push_back(std::move(ch));
        }
        return d;
    }
    catch (std::exception const& e) {
        LOG_CTX_ERR("map_content", e.what());
        return {};
    }

    bool getBool(std::string const& var, bool fallback)
    {
        auto const value = game_state::get(var);
        if (!value)
            return fallback;
        if (auto const b = std::get_if<bool>(&*value))
            return *b;
        if (auto const i = std::get_if<int64_t>(&*value))
            return *i != 0;
        return fallback;
    }

    std::string getString(std::string const& var, std::string const& fallback)
    {
        auto const value = game_state::get(var);
        if (!value)
            return fallback;
        if (auto const s = std::get_if<std::string>(&*value))
            return *s;
        return fallback;
    }

    std::string locationVar(std::string const& id) { return "map_location_" + id; }
    std::string roadVar(std::string const& id) { return "map_road_" + id; }
    std::string characterVar(std::string const& id) { return "map_character_" + id; }
    std::string characterAtVar(std::string const& id) { return "map_character_" + id + "_at"; }
    std::string seenLocationVar(std::string const& id) { return "map_seen_location_" + id; }
    std::string seenRoadVar(std::string const& id) { return "map_seen_road_" + id; }
    std::string seenCharacterVar(std::string const& id) { return "map_seen_character_" + id; }

    template<class T>
    T const* findId(std::vector<T> const& items, std::string const& id)
    {
        auto const it = std::find_if(items.cbegin(), items.cend(), [&](T const& t) { return t.id == id; });
        return it == items.cend() ? nullptr : &*it;
    }

    template<class T>
    T const* known(T const* item, char const* kind, std::string const& id)
    {
        if (!item)
            LOG_CTX_WRN("map_content", std::string("unknown ") + kind + " '" + id + "'");
        return item;
    }

    // From the location, through the via points, to the other one. With
    // octilinear, an elbow between two points: straight, then 45 degrees.
    std::vector<glm::vec2> polyline(Data const& d, Road const& road)
    {
        Location const* from = d.location(road.from);
        Location const* to = d.location(road.to);
        if (!from || !to)
            return {};
        std::vector<glm::vec2> points{ from->pos };
        points.insert(points.end(), road.via.cbegin(), road.via.cend());
        points.push_back(to->pos);
        if (!road.octilinear)
            return points;
        std::vector<glm::vec2> out{ points.front() };
        for (size_t i = 1; i < points.size(); ++i) {
            glm::vec2 const a = points[i - 1], b = points[i];
            glm::vec2 const delta = b - a;
            glm::vec2 const abs = glm::abs(delta);
            glm::vec2 elbow = a;
            if (abs.x > abs.y)
                elbow.x += glm::sign(delta.x) * (abs.x - abs.y);
            else
                elbow.y += glm::sign(delta.y) * (abs.y - abs.x);
            if (glm::distance(elbow, a) > 0.5f && glm::distance(elbow, b) > 0.5f)
                out.push_back(elbow);
            out.push_back(b);
        }
        return out;
    }

    // The first progress share of the polyline's length
    std::vector<glm::vec2> cut(std::vector<glm::vec2> const& points, float progress)
    {
        if (progress >= 1.f || points.size() < 2)
            return points;
        float total = 0.f;
        for (size_t i = 1; i < points.size(); ++i)
            total += glm::distance(points[i - 1], points[i]);
        float left = total * std::max(progress, 0.f);
        std::vector<glm::vec2> out{ points.front() };
        for (size_t i = 1; i < points.size() && left > 0.f; ++i) {
            float const length = glm::distance(points[i - 1], points[i]);
            if (length <= left) {
                out.push_back(points[i]);
                left -= length;
            }
            else {
                out.push_back(glm::mix(points[i - 1], points[i], left / length));
                left = 0.f;
            }
        }
        return out;
    }

    // Slow start, overshoots to 1.15, settles back to 1
    float popScale(float t)
    {
        t = std::clamp(t, 0.f, 1.f);
        constexpr float peak = 0.7f;
        if (t < peak) {
            float const f = 1.f - t / peak;
            return 1.15f * (1.f - f * f);
        }
        return glm::mix(1.15f, 1.f, (t - peak) / (1.f - peak));
    }

    bool isLoaded(SSS::GL::Texture const& texture)
    {
        auto const& frames = texture.getFrames();
        return frames.w > 0 && frames.h > 0 && !frames.empty()
            && frames[0].pixels.size() == static_cast<size_t>(frames.w) * frames.h;
    }

    // Straight alpha
    glm::vec4 texel(SSS::GL::Texture const& texture, int x, int y)
    {
        auto const& frames = texture.getFrames();
        x = std::clamp(x, 0, frames.w - 1);
        y = std::clamp(y, 0, frames.h - 1);
        SSS::RGBA32 const p = frames[0].pixels[static_cast<size_t>(y) * frames.w + x];
        return glm::vec4(p.r, p.g, p.b, p.a) / 255.f;
    }

    // Premultiplied, uv from the top left
    glm::vec4 sampleBilinear(SSS::GL::Texture const& texture, glm::vec2 uv)
    {
        auto const& frames = texture.getFrames();
        glm::vec2 const t = uv * glm::vec2(frames.w, frames.h) - 0.5f;
        glm::vec2 const f = glm::fract(t);
        int const x = static_cast<int>(std::floor(t.x)), y = static_cast<int>(std::floor(t.y));
        auto const premul = [&](int tx, int ty) {
            glm::vec4 const c = texel(texture, tx, ty);
            return glm::vec4(glm::vec3(c) * c.a, c.a);
        };
        return glm::mix(glm::mix(premul(x, y), premul(x + 1, y), f.x),
            glm::mix(premul(x, y + 1), premul(x + 1, y + 1), f.x), f.y);
    }

    // Map rect, grown to fit what's drawn
    struct Box {
        glm::vec2 min{ std::numeric_limits<float>::max() };
        glm::vec2 max{ std::numeric_limits<float>::lowest() };

        void add(glm::vec2 p, float margin = 0.f)
        {
            min = glm::min(min, p - margin);
            max = glm::max(max, p + margin);
        }
    };

    // CPU layer, premultiplied colors. Drawn in map coordinates (as seen on
    // screen), stored turned 180 degrees as the paper's texture: its (0, 0)
    // is the map rect's max corner.
    struct Canvas {
        int w, h;
        glm::vec2 corner;
        std::vector<glm::vec4> px;

        // Covering the map rect [min, max] (whole pixels)
        Canvas(glm::vec2 min, glm::vec2 max)
            : w(static_cast<int>(std::lround(max.x - min.x))), h(static_cast<int>(std::lround(max.y - min.y))),
            corner(max), px(static_cast<size_t>(w) * h, glm::vec4(0.f)) {}

        // Map coordinates of the pixel's point (+ sub, 0 -> 1)
        glm::vec2 mapPos(int x, int y, glm::vec2 sub = glm::vec2(0.5f)) const
        {
            return corner - (glm::vec2(x, y) + sub);
        }

        // Pixels covering the map rect [min, max]
        glm::ivec4 pixelRect(glm::vec2 min, glm::vec2 max) const
        {
            glm::vec2 const a = corner - max, b = corner - min;
            return glm::ivec4(std::max(static_cast<int>(std::floor(a.x)), 0),
                std::max(static_cast<int>(std::floor(a.y)), 0),
                std::min(static_cast<int>(std::ceil(b.x)), w),
                std::min(static_cast<int>(std::ceil(b.y)), h));
        }

        void over(int x, int y, glm::vec4 c)
        {
            glm::vec4& d = px[static_cast<size_t>(y) * w + x];
            d = c + d * (1.f - c.a);
        }

        // Image upright on the map, centered on center, of size (map pixels).
        // Supersampled when shrunk: slow, drawn once in sprites.
        void image(SSS::GL::Texture const& texture, glm::vec2 center, glm::vec2 size)
        {
            if (size.x < 0.5f || size.y < 0.5f)
                return;
            glm::vec2 const min = center - size / 2.f;
            glm::ivec4 const r = pixelRect(min, center + size / 2.f);
            auto const& frames = texture.getFrames();
            int const n = std::clamp(static_cast<int>(std::ceil(frames.w / size.x)), 1, 6);
            for (int y = r.y; y < r.w; ++y) {
                for (int x = r.x; x < r.z; ++x) {
                    glm::vec4 sum(0.f);
                    for (int sy = 0; sy < n; ++sy) {
                        for (int sx = 0; sx < n; ++sx) {
                            glm::vec2 const uv = (mapPos(x, y, (glm::vec2(sx, sy) + 0.5f) / float(n)) - min) / size;
                            if (uv.x >= 0.f && uv.y >= 0.f && uv.x <= 1.f && uv.y <= 1.f)
                                sum += sampleBilinear(texture, uv);
                        }
                    }
                    sum /= float(n * n);
                    if (sum.a > 0.f)
                        over(x, y, sum);
                }
            }
        }

        // Sprite centered on center, scaled, brightened (0 -> 1)
        void blit(Sprite const& sprite, glm::vec2 center, float scale, float brighten)
        {
            if (scale <= 0.f)
                return;
            glm::vec2 const half = sprite.half() * scale;
            glm::ivec4 const r = pixelRect(center - half, center + half);
            for (int y = r.y; y < r.w; ++y) {
                for (int x = r.x; x < r.z; ++x) {
                    glm::vec4 c = sprite.sample((mapPos(x, y) - center) / scale);
                    if (c.a <= 0.f)
                        continue;
                    if (brighten > 0.f)
                        c = glm::vec4(glm::mix(glm::vec3(c), glm::vec3(c.a), brighten), c.a);
                    over(x, y, c);
                }
            }
        }

        // SDF triangle: fill, outline of outline_width inside its edges
        void triangle(glm::vec2 a, glm::vec2 b, glm::vec2 c, float outline_width, glm::vec4 fill, glm::vec4 outline)
        {
            // Exact distance (Inigo Quilez), negative inside
            auto const sdf = [&](glm::vec2 p) {
                glm::vec2 const e0 = b - a, e1 = c - b, e2 = a - c;
                glm::vec2 const v0 = p - a, v1 = p - b, v2 = p - c;
                glm::vec2 const pq0 = v0 - e0 * std::clamp(glm::dot(v0, e0) / glm::dot(e0, e0), 0.f, 1.f);
                glm::vec2 const pq1 = v1 - e1 * std::clamp(glm::dot(v1, e1) / glm::dot(e1, e1), 0.f, 1.f);
                glm::vec2 const pq2 = v2 - e2 * std::clamp(glm::dot(v2, e2) / glm::dot(e2, e2), 0.f, 1.f);
                float const s = glm::sign(e0.x * e2.y - e0.y * e2.x);
                glm::vec2 const d = glm::min(glm::min(
                    glm::vec2(glm::dot(pq0, pq0), s * (v0.x * e0.y - v0.y * e0.x)),
                    glm::vec2(glm::dot(pq1, pq1), s * (v1.x * e1.y - v1.y * e1.x))),
                    glm::vec2(glm::dot(pq2, pq2), s * (v2.x * e2.y - v2.y * e2.x)));
                return -std::sqrt(d.x) * glm::sign(d.y);
            };
            glm::ivec4 const r = pixelRect(glm::min(glm::min(a, b), c) - 1.f, glm::max(glm::max(a, b), c) + 1.f);
            for (int y = r.y; y < r.w; ++y) {
                for (int x = r.x; x < r.z; ++x) {
                    float const d = sdf(mapPos(x, y));
                    float const coverage = std::clamp(0.5f - d, 0.f, 1.f);
                    if (coverage <= 0.f)
                        continue;
                    float const inner = std::clamp(0.5f - (d + outline_width), 0.f, 1.f);
                    glm::vec4 const col = glm::mix(outline, fill, inner);
                    float const alpha = col.a * coverage;
                    over(x, y, glm::vec4(glm::vec3(col) * alpha, alpha));
                }
            }
        }

        void upload(SSS::GL::Texture& texture) const { uploadPixels(px, w, h, texture); }

        // Premultiplied -> straight alpha
        static void uploadPixels(std::vector<glm::vec4> const& px, int w, int h, SSS::GL::Texture& texture)
        {
            std::vector<SSS::RGBA32> out(px.size());
            for (size_t i = 0; i < px.size(); ++i) {
                glm::vec4 const c = px[i];
                if (c.a <= 0.f)
                    continue;
                glm::vec3 const rgb = glm::clamp(glm::vec3(c) / c.a, 0.f, 1.f);
                SSS::RGBA32& o = out[i];
                o.r = static_cast<uint8_t>(std::lround(rgb.r * 255.f));
                o.g = static_cast<uint8_t>(std::lround(rgb.g * 255.f));
                o.b = static_cast<uint8_t>(std::lround(rgb.b * 255.f));
                o.a = static_cast<uint8_t>(std::lround(std::clamp(c.a, 0.f, 1.f) * 255.f));
            }
            texture.editRawPixels(out.data(), w, h);
        }
    };

    // Canvas covering [-half, half] around the sprite's center (half: whole
    // pixels, a transparent margin around what's drawn)
    Canvas spriteCanvas(glm::vec2 size)
    {
        glm::vec2 const half = glm::ceil(size / 2.f) + 2.f;
        return Canvas(-half, half);
    }

    std::unique_ptr<Sprite> toSprite(Canvas& canvas)
    {
        auto sprite = std::make_unique<Sprite>();
        sprite->w = canvas.w;
        sprite->h = canvas.h;
        sprite->px = std::move(canvas.px);
        sprite->texture = SSS::GL::Texture::create();
        Canvas::uploadPixels(sprite->px, sprite->w, sprite->h, *sprite->texture);
        return sprite;
    }
}

Location const* Data::location(std::string const& id) const { return findId(locations, id); }
Road const* Data::road(std::string const& id) const { return findId(roads, id); }
Character const* Data::character(std::string const& id) const { return findId(characters, id); }

Data const& data()
{
    static Data const d = load();
    return d;
}

bool isLocationUnlocked(std::string const& id)
{
    Location const* loc = known(data().location(id), "location", id);
    return loc && getBool(locationVar(id), loc->unlocked);
}

void setLocationUnlocked(std::string const& id, bool unlocked)
{
    if (known(data().location(id), "location", id))
        game_state::set(locationVar(id), unlocked);
}

bool isRoadUnlocked(std::string const& id)
{
    Road const* road = known(data().road(id), "road", id);
    return road && getBool(roadVar(id), road->unlocked);
}

void setRoadUnlocked(std::string const& id, bool unlocked)
{
    if (known(data().road(id), "road", id))
        game_state::set(roadVar(id), unlocked);
}

bool isCharacterShown(std::string const& id)
{
    Character const* ch = known(data().character(id), "character", id);
    return ch && getBool(characterVar(id), ch->unlocked);
}

void setCharacterShown(std::string const& id, bool shown)
{
    if (!known(data().character(id), "character", id))
        return;
    game_state::set(characterVar(id), shown);
    // Shown again: pops again
    if (!shown)
        game_state::set(seenCharacterVar(id), std::string());
}

std::string characterLocation(std::string const& id)
{
    Character const* ch = known(data().character(id), "character", id);
    if (!ch)
        return {};
    auto const value = game_state::get(characterAtVar(id));
    if (value)
        if (auto const s = std::get_if<std::string>(&*value))
            return *s;
    return ch->location;
}

void moveCharacter(std::string const& id, std::string const& location)
{
    if (known(data().character(id), "character", id) && known(data().location(location), "location", location))
        game_state::set(characterAtVar(id), location);
}

void clear()
{
    Data const& d = data();
    for (Location const& loc : d.locations) {
        game_state::set(locationVar(loc.id), false);
        game_state::set(seenLocationVar(loc.id), false);
    }
    for (Road const& road : d.roads) {
        game_state::set(roadVar(road.id), false);
        game_state::set(seenRoadVar(road.id), false);
    }
    for (Character const& ch : d.characters) {
        game_state::set(characterVar(ch.id), false);
        game_state::set(seenCharacterVar(ch.id), std::string());
        game_state::erase(characterAtVar(ch.id));
    }
}

Layers::Layers(float width, float height)
    : _width(width), _height(height)
{
    for (auto& texture : _textures)
        texture = SSS::GL::Texture::create();
    for (auto& texture : _overlays)
        texture = SSS::GL::Texture::create();
    _shown_overlays = _overlays;
}

Layers::~Layers() = default;

Sprite const* Layers::_monumentSprite(Location const& loc)
{
    std::string const key = "monument:" + loc.id;
    if (auto const it = _sprites.find(key); it != _sprites.end())
        return it->second.get();
    SSS::GL::Texture const* image = _loadedImage(loc.image);
    if (!image)
        return nullptr;
    auto const& frames = image->getFrames();
    glm::vec2 const size(loc.size, loc.size * frames.h / frames.w);
    Canvas canvas = spriteCanvas(size);
    canvas.image(*image, glm::vec2(0.f), size);
    return (_sprites[key] = toSprite(canvas)).get();
}

Sprite const* Layers::_pinSprite(Pin const& pin)
{
    Style const& st = data().style;
    // Like a pin, the icon is its head and a tip, from under the icon's
    // center, pokes out of its edge toward the location
    glm::vec2 const toward = pin.target - pin.pos;
    bool const tipped = glm::length(toward) > 0.01f;
    float const angle = tipped ? std::atan2(toward.y, toward.x) : 0.f;
    std::string const key = "pin:" + pin.character->id + ':' + std::to_string(std::lround(angle * 1000.f));
    if (auto const it = _sprites.find(key); it != _sprites.end())
        return it->second.get();
    // Waits for the icon, if any
    SSS::GL::Texture const* icon = _loadedImage(pin.character->icon);
    if (!icon && !pin.character->icon.empty())
        return nullptr;
    glm::vec2 size(st.pin_icon_size);
    if (icon) {
        auto const& frames = icon->getFrames();
        size.y *= static_cast<float>(frames.h) / frames.w;
    }
    // The icons are stickers: round, a margin around them
    float const reach = std::min(size.x, size.y) * 0.45f + st.pin_tip_length;
    Canvas canvas = spriteCanvas(glm::vec2(std::max({ size.x, size.y, 2.f * reach })));
    if (tipped) {
        glm::vec2 const dir(std::cos(angle), std::sin(angle));
        glm::vec2 const side = glm::vec2(-dir.y, dir.x) * st.pin_tip_width / 2.f;
        canvas.triangle(side, -side, dir * reach, st.pin_outline, pin.character->color, glm::vec4(1.f));
    }
    if (icon)
        canvas.image(*icon, glm::vec2(0.f), size);
    return (_sprites[key] = toSprite(canvas)).get();
}

std::string Layers::_signature(std::vector<Placed> const& items)
{
    std::string s;
    for (Placed const& p : items) {
        s += std::to_string(reinterpret_cast<uintptr_t>(p.sprite)) + ':' + std::to_string(p.center.x) + ','
            + std::to_string(p.center.y) + ',' + std::to_string(p.scale) + ',' + std::to_string(p.brighten) + ';';
    }
    return s;
}

void Layers::_bakeLayer(int layer, std::vector<Placed> const& items)
{
    Canvas canvas(glm::vec2(0.f), glm::vec2(_width, _height));
    for (Placed const& p : items)
        canvas.blit(*p.sprite, p.center, p.scale, p.brighten);
    canvas.upload(*_textures[layer]);
}

void Layers::_showOverlay(int layer, std::vector<Placed> const& items)
{
    if (items.empty()) {
        _overlay_rects[layer] = glm::vec4(0.f);
        _overlay_baked[layer].clear();
        return;
    }
    if (items.size() == 1) {
        // As is, scaled by its rect: nothing to bake
        Placed const& p = items.front();
        glm::vec2 const size(_width, _height), half = p.sprite->half() * p.scale;
        _shown_overlays[layer] = p.sprite->texture;
        // Turned 180 degrees as the paper's texture: the map's max is uv (0, 0)
        _overlay_rects[layer] = glm::vec4((size - (p.center + half)) / size, (size - (p.center - half)) / size);
        _overlay_baked[layer].clear();
        return;
    }
    // Several: drawn together in the layer's overlay
    _shown_overlays[layer] = _overlays[layer];
    std::string const signature = _signature(items);
    if (_overlay_baked[layer] == signature)
        return;
    _overlay_baked[layer] = signature;
    Box box;
    for (Placed const& p : items) {
        box.add(p.center - p.sprite->half() * p.scale);
        box.add(p.center + p.sprite->half() * p.scale);
    }
    glm::vec2 min = box.min, max = box.max;
    if (!_overlayRect(layer, min, max))
        return;
    Canvas canvas(min, max);
    for (Placed const& p : items)
        canvas.blit(*p.sprite, p.center, p.scale, p.brighten);
    canvas.upload(*_overlays[layer]);
}

SSS::GL::Texture const* Layers::_loadedImage(std::string const& path) const
{
    auto const it = _images.find(path);
    return it != _images.end() && isLoaded(*it->second) ? it->second.get() : nullptr;
}

void Layers::update(float dt, bool animate)
{
    Data const& d = data();
    // Images, loaded once (asynchronously)
    auto const request = [this](std::string const& path) {
        if (!path.empty() && !_images.contains(path))
            _images.emplace(path, SSS::GL::Texture::create(fs::path{ path }));
    };
    for (Location const& loc : d.locations)
        request(loc.image);
    for (Character const& ch : d.characters)
        request(ch.icon);

    // New places pop, all at once
    std::vector<Location const*> popping;
    for (Location const& loc : d.locations) {
        if (getBool(locationVar(loc.id), loc.unlocked) && !getBool(seenLocationVar(loc.id), loc.unlocked))
            popping.push_back(&loc);
    }
    // A step done: a pause before the next ones (negative times: waiting)
    auto const pause = [this](bool roads) {
        if (roads)
            _road_t = -step_delay / road_time;
        _arrive_t = -step_delay / pop_time;
    };
    // Popping once their images are loaded (none: never shown)
    bool const places_ready = std::all_of(popping.cbegin(), popping.cend(),
        [this](Location const* loc) { return loc->image.empty() || _monumentSprite(*loc); });
    if (popping.empty())
        _pop_t = 0.f;
    else if (animate && places_ready) {
        _pop_t += dt / pop_time;
        if (_pop_t >= 1.f) {
            for (Location const* loc : popping)
                game_state::set(seenLocationVar(loc->id), true);
            popping.clear();
            _pop_t = 0.f;
            // Roads may be drawing already
            pause(false);
        }
    }

    // Then new roads draw themselves, one after another, the first one along
    // with the new places popping (once they started: images loaded)
    bool const places_started = popping.empty() || _pop_t > 0.f;
    if (!places_started)
        _road_t = 0.f;
    std::vector<std::pair<Road const*, float>> roads;
    bool drawing = false;
    for (Road const& road : d.roads) {
        if (!getBool(roadVar(road.id), road.unlocked))
            continue;
        float progress = 1.f;
        if (!getBool(seenRoadVar(road.id), road.unlocked)) {
            progress = 0.f;
            if (!drawing && places_started) {
                drawing = true;
                if (animate)
                    _road_t += dt / road_time;
                if (_road_t >= 1.f) {
                    game_state::set(seenRoadVar(road.id), true);
                    pause(true);
                    progress = 1.f;
                    drawing = false;
                }
                else
                    // Shoots out, slows down reaching the end
                    progress = SSS::Math::easeOutCubic(std::clamp(_road_t, 0.f, 1.f));
            }
        }
        roads.emplace_back(&road, progress);
    }

    // Popping ones apart (overlay), hidden until they pop
    std::vector<Placed> monuments, popping_monuments;
    for (Location const& loc : d.locations) {
        Sprite const* sprite = _monumentSprite(loc);
        if (!sprite || !getBool(locationVar(loc.id), loc.unlocked))
            continue;
        bool const hovered = _hovered == loc.id;
        Placed p{ sprite, loc.pos, hovered ? hover_scale : 1.f, hovered ? hover_brighten : 0.f };
        if (std::find(popping.cbegin(), popping.cend(), &loc) == popping.cend())
            monuments.push_back(p);
        else if (_pop_t > 0.f) {
            p.scale *= popScale(_pop_t);
            popping_monuments.push_back(p);
        }
    }

    // Then the characters just shown, or moved, pop at their place, all at
    // once. Seen: where they last popped (shown by default: at their place).
    std::vector<Character const*> arriving;
    for (Character const& ch : d.characters) {
        if (getBool(characterVar(ch.id), ch.unlocked)
            && getString(seenCharacterVar(ch.id), ch.unlocked ? ch.location : "") != characterLocation(ch.id))
            arriving.push_back(&ch);
    }
    bool const characters_ready = std::all_of(arriving.cbegin(), arriving.cend(),
        [this](Character const* ch) { return ch->icon.empty() || _loadedImage(ch->icon); });
    if (arriving.empty())
        _arrive_t = 0.f;
    else if (animate && popping.empty() && !drawing && characters_ready) {
        _arrive_t += dt / pop_time;
        if (_arrive_t >= 1.f) {
            for (Character const* ch : arriving)
                game_state::set(seenCharacterVar(ch->id), characterLocation(ch->id));
            arriving.clear();
            _arrive_t = 0.f;
        }
    }

    // Pins, grouped by location, spread on an arc centered on top of it
    std::map<std::string, std::vector<Character const*>> at;
    for (Character const& ch : d.characters) {
        if (getBool(characterVar(ch.id), ch.unlocked))
            at[characterLocation(ch.id)].push_back(&ch);
    }
    std::vector<Pin> pins;
    Style const& st = d.style;
    for (auto& [location_id, characters] : at) {
        Location const* loc = d.location(location_id);
        if (!loc)
            continue;
        std::sort(characters.begin(), characters.end(),
            [](Character const* a, Character const* b) { return a->id < b->id; });
        float const n = static_cast<float>(characters.size());
        float ring = loc->pin_ring >= 0.f ? loc->pin_ring : st.pin_ring;
        // Angle between neighbours, for their icons not to overlap, closing
        // into a full circle (the ring growing) when crowded
        float const spacing = st.pin_icon_size * 1.05f;
        float step = 2.f * std::asin(std::min(spacing / (2.f * std::max(ring, 1.f)), 1.f));
        if (n > 1.f && step * n > 2.f * std::numbers::pi_v<float>) {
            step = 2.f * std::numbers::pi_v<float> / n;
            ring = spacing / (2.f * std::sin(step / 2.f));
        }
        for (size_t i = 0; i < characters.size(); ++i) {
            // Y down: -90 degrees is up
            float const angle = -std::numbers::pi_v<float> / 2.f + (static_cast<float>(i) - (n - 1.f) / 2.f) * step;
            bool const arrives = std::find(arriving.cbegin(), arriving.cend(), characters[i]) != arriving.cend();
            float const scale = arrives ? (_arrive_t > 0.f ? popScale(_arrive_t) : 0.f) : 1.f;
            pins.push_back({ characters[i], loc->pos + ring * glm::vec2(std::cos(angle), std::sin(angle)), loc->pos, scale });
        }
    }

    // Rebakes what changed. What animates is only baked in the overlays,
    // small, every frame: the layers are baked once it's done.
    auto const changed = [this](int layer, std::string signature) {
        if (_baked[layer] == signature && _textures[layer]->getFrames().w != 0)
            return false;
        _baked[layer] = std::move(signature);
        return true;
    };
    // Off when empty
    auto const overlay_changed = [this](int layer, std::string signature) {
        if (signature.empty())
            _overlay_rects[layer] = glm::vec4(0.f);
        if (_overlay_baked[layer] == signature)
            return false;
        _overlay_baked[layer] = std::move(signature);
        return !_overlay_baked[layer].empty();
    };
    // Unknown ids were logged when loading, not every frame
    auto const unlocked = [&d](std::string const& id) {
        Location const* loc = d.location(id);
        return loc && getBool(locationVar(id), loc->unlocked);
    };
    std::string signature, overlay;
    for (auto const& [road, progress] : roads) {
        std::string const s = road->id + ':' + std::to_string(unlocked(road->from)) + std::to_string(unlocked(road->to)) + ';';
        if (progress >= 1.f)
            signature += s;
        else if (progress > 0.f)
            overlay += s + std::to_string(progress) + ';';
    }
    // The drawn road's stations depend on the others
    if (!overlay.empty())
        overlay += signature;
    if (changed(Layers::roads, signature))
        _bakeRoads(roads, false);
    if (overlay_changed(Layers::roads, overlay))
        _bakeRoads(roads, true);

    if (changed(Layers::monuments, _signature(monuments)))
        _bakeLayer(Layers::monuments, monuments);
    _showOverlay(Layers::monuments, popping_monuments);

    // Arriving ones apart (overlay), hidden until they pop
    std::vector<Placed> placed_pins, arriving_pins;
    for (Pin const& pin : pins) {
        Sprite const* sprite = _pinSprite(pin);
        if (!sprite)
            continue;
        Placed const p{ sprite, pin.pos, pin.scale, 0.f };
        if (std::find(arriving.cbegin(), arriving.cend(), pin.character) == arriving.cend())
            placed_pins.push_back(p);
        else if (pin.scale > 0.f)
            arriving_pins.push_back(p);
    }
    if (changed(Layers::characters, _signature(placed_pins)))
        _bakeLayer(Layers::characters, placed_pins);
    _showOverlay(Layers::characters, arriving_pins);
}

bool Layers::_overlayRect(int layer, glm::vec2& min, glm::vec2& max)
{
    glm::vec2 const size(_width, _height);
    // Whole pixels, a transparent margin: nothing bleeds in when filtered
    min = glm::max(glm::floor(min) - 2.f, glm::vec2(0.f));
    max = glm::min(glm::ceil(max) + 2.f, glm::floor(size));
    if (max.x <= min.x || max.y <= min.y) {
        _overlay_rects[layer] = glm::vec4(0.f);
        return false;
    }
    // Turned 180 degrees as the paper's texture: the map's max is uv (0, 0)
    _overlay_rects[layer] = glm::vec4((size - max) / size, (size - min) / size);
    return true;
}

std::optional<std::string> Layers::locationAt(glm::vec2 pos) const
{
    Data const& d = data();
    // Drawn last: on top
    for (auto it = d.locations.crbegin(); it != d.locations.crend(); ++it) {
        Location const& loc = *it;
        SSS::GL::Texture const* image = _loadedImage(loc.image);
        if (!image || !getBool(locationVar(loc.id), loc.unlocked))
            continue;
        auto const& frames = image->getFrames();
        glm::vec2 const size(loc.size, loc.size * frames.h / frames.w);
        glm::vec2 const uv = (pos - (loc.pos - size / 2.f)) / size;
        if (uv.x < 0.f || uv.y < 0.f || uv.x >= 1.f || uv.y >= 1.f)
            continue;
        if (texel(*image, static_cast<int>(uv.x * frames.w), static_cast<int>(uv.y * frames.h)).a >= hit_alpha)
            return loc.id;
    }
    return std::nullopt;
}

void Layers::setHovered(std::optional<std::string> id)
{
    _hovered = std::move(id);
}

void Layers::_bakeRoads(std::vector<std::pair<Road const*, float>> const& roads, bool overlay)
{
    Data const& d = data();
    Style const& st = d.style;

    // Layer: the drawn roads. Overlay: the one drawing itself, and its
    // station, over them.
    struct Segment { glm::vec2 a, b; glm::vec4 color; };
    std::vector<Segment> segments;
    // Stations: roads reaching each location, and the color of the first one
    std::map<std::string, std::pair<int, glm::vec4>> stations;
    std::set<std::string> drawn_stations;
    Box box;
    float const line_margin = st.road_width / 2.f + st.road_outline_width + 1.f;
    for (auto const& [road, progress] : roads) {
        bool const done = progress >= 1.f;
        if (progress <= 0.f || (!overlay && !done))
            continue;
        auto [it, added] = stations.try_emplace(road->from, 0, road->color);
        ++it->second.first;
        if (done) {
            auto [to, to_added] = stations.try_emplace(road->to, 0, road->color);
            ++to->second.first;
        }
        if (overlay == done)
            continue;
        std::vector<glm::vec2> const points = cut(polyline(d, *road), progress);
        for (size_t i = 1; i < points.size(); ++i) {
            if (glm::distance(points[i - 1], points[i]) < 0.01f)
                continue;
            segments.push_back({ points[i - 1], points[i], road->color });
            box.add(points[i - 1], line_margin);
            box.add(points[i], line_margin);
        }
        drawn_stations.insert(road->from);
        if (done)
            drawn_stations.insert(road->to);
    }
    auto const shown = [&](std::string const& id) -> Location const* {
        Location const* loc = d.location(id);
        return loc && getBool(locationVar(id), loc->unlocked) && drawn_stations.contains(id) ? loc : nullptr;
    };
    for (auto const& id : drawn_stations) {
        if (Location const* loc = shown(id))
            box.add(loc->pos, st.station_radius + 1.f);
    }

    glm::vec2 min(0.f), max(_width, _height);
    if (overlay) {
        min = box.min;
        max = box.max;
        if (!_overlayRect(Layers::roads, min, max))
            return;
    }
    // SDF space: texture pixels from its center, Y down, turned 180 degrees
    glm::vec2 const center = (min + max) / 2.f;
    auto const to_sdf = [&](glm::vec2 p) { return (center - p) * roads_scale; };

    std::vector<SSS::UIPrimitive> outlines, lines;
    for (Segment const& s : segments) {
        SSS::UIPrimitive line;
        line.shapeId = SSS::sdSegment;
        line.pos = to_sdf(s.a);
        line.pos2 = to_sdf(s.b);
        line.size = glm::vec2(st.road_width * roads_scale);     // thickness
        line.color = s.color;
        lines.push_back(line);

        SSS::UIPrimitive outline = line;
        outline.size = glm::vec2((st.road_width + 2.f * st.road_outline_width) * roads_scale);
        outline.color = st.road_outline;
        outlines.push_back(outline);
    }

    // Outlines under every line: crossing lines stay readable
    std::vector<SSS::UIPrimitive> prims = std::move(outlines);
    prims.insert(prims.end(), lines.cbegin(), lines.cend());
    for (auto const& [id, s] : stations) {
        Location const* loc = shown(id);
        if (!loc)
            continue;
        SSS::UIPrimitive fill;
        fill.shapeId = SSS::sdCircle;
        fill.pos = to_sdf(loc->pos);
        fill.size = glm::vec2(st.station_radius * roads_scale);     // radius
        fill.color = st.station_fill;
        prims.push_back(fill);

        // One road: its color, more: an interchange
        SSS::UIPrimitive ring = fill;
        ring.size = glm::vec2((st.station_radius - st.station_ring / 2.f) * roads_scale);
        ring.innerRadius = st.station_ring / 2.f * roads_scale;     // filled circle -> ring
        ring.color = s.first > 1 ? st.interchange_ring : s.second;
        prims.push_back(ring);
    }
    // Never an empty list (sized SSBO): a transparent dot
    if (prims.empty())
        prims.emplace_back().color = glm::vec4(0.f);

    (overlay ? _overlays : _textures)[Layers::roads]->setSDF(std::move(prims),
        static_cast<int>(std::lround((max.x - min.x) * roads_scale)), static_cast<int>(std::lround((max.y - min.y) * roads_scale)));
}

} // namespace map_content
