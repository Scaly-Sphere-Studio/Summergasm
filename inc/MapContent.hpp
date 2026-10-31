#pragma once

#include "includes.hpp"

// What's printed on the city map (see MapOverlay), over its paper texture,
// each in its own layer texture (see FoldingMap::layers) so that only the one
// changing is rebaked:
//   0. roads, drawn like a metro map (SDF segments, stations)
//   1. monuments, the places: clickable (see locationAt)
//   2. characters' pins, their icon over an SDF tip pointing at the location
//      they're at, spread around it
// Locations, roads & characters are predefined in assets/map/map.json, and
// unlocked / shown during the adventure: their state is kept in game_state,
// so saved with the game (see GameState.hpp), the JSON giving the defaults:
//   map_location_<id>          bool    unlocked
//   map_road_<id>              bool    unlocked
//   map_character_<id>         bool    shown
//   map_character_<id>_at      string  location the character is at
//   map_seen_location_<id>     bool    pop animation played
//   map_seen_road_<id>         bool    draw animation played
//   map_seen_character_<id>    string  location the pop animation played at
// Coordinates are pixels of the map as seen on screen, from its top left.
// The paper's texture is turned 180 degrees on screen (see MapOverlay): the
// layers are baked turned the same way.
namespace map_content {

struct Location {
    std::string id;
    std::string name;
    std::string image;
    glm::vec2 pos{ 0.f };
    // Width of the monument, in map pixels (height: the image's ratio)
    float size{ 100.f };
    std::string scene;
    bool unlocked{ false };
    // Distance of the characters' pins from pos, style's pin_ring if < 0
    float pin_ring{ -1.f };
};

struct Road {
    std::string id;
    std::string from, to;
    std::vector<glm::vec2> via;
    glm::vec4 color{ 1.f };
    // Elbows added between the points: straight then 45 degrees
    bool octilinear{ true };
    bool unlocked{ false };
};

struct Character {
    std::string id;
    std::string icon;
    glm::vec4 color{ 1.f };
    std::string location;
    bool unlocked{ false };
};

struct Style {
    float road_width{ 10.f };
    float road_outline_width{ 3.f };
    glm::vec4 road_outline{ 1.f };
    float station_radius{ 9.f };
    float station_ring{ 3.f };
    glm::vec4 station_fill{ 1.f };
    glm::vec4 interchange_ring{ 0.2f, 0.2f, 0.2f, 1.f };
    // Pin tip: how far it pokes out of the icon's edge, its width under the
    // icon's center
    float pin_tip_length{ 10.f };
    float pin_tip_width{ 26.f };
    float pin_outline{ 2.f };
    float pin_icon_size{ 56.f };
    // Distance of the icons' centers from the location
    float pin_ring{ 80.f };
};

struct Data {
    Style style;
    // Ordered as in the JSON (drawing order)
    std::vector<Location> locations;
    std::vector<Road> roads;
    std::vector<Character> characters;

    Location const* location(std::string const& id) const;
    Road const* road(std::string const& id) const;
    Character const* character(std::string const& id) const;
};

// Loaded from assets/map/map.json the first time (errors logged: empty)
Data const& data();

// State, game_state variables or the JSON's defaults. Unknown ids are logged.
bool isLocationUnlocked(std::string const& id);
void setLocationUnlocked(std::string const& id, bool unlocked);
bool isRoadUnlocked(std::string const& id);
void setRoadUnlocked(std::string const& id, bool unlocked);
bool isCharacterShown(std::string const& id);
void setCharacterShown(std::string const& id, bool shown);
std::string characterLocation(std::string const& id);
void moveCharacter(std::string const& id, std::string const& location);
// Everything locked & hidden, characters back at their JSON location, the
// animations to play again once unlocked
void clear();

// A monument or a pin, drawn once (see MapContent.cpp)
struct Sprite;

// The layer textures, rebaked when what they show changes. Needs the GL
// context.
class Layers {
public:
    // Size of the map, in pixels
    Layers(float width, float height);
    ~Layers();

    static constexpr int roads = 0;
    static constexpr int monuments = 1;
    static constexpr int characters = 2;
    inline std::array<SSS::GL::Texture::Shared, 3> const& textures() const noexcept { return _textures; };
    // Over each layer while it animates: only what animates, in a texture
    // printed on the uv rect (u0, v0, u1, v1) of the paper (empty: none), the
    // layer itself being rebaked once, when it's done (see
    // FoldingMap::overlays). One monument or pin: its sprite as is, scaled by
    // the rect. More: drawn together. A road: drawn as it grows.
    inline std::array<SSS::GL::Texture::Shared, 3> const& overlays() const noexcept { return _shown_overlays; };
    inline std::array<glm::vec4, 3> const& overlayRects() const noexcept { return _overlay_rects; };

    // Re-reads the state & rebakes the layers that changed. Animations (new
    // places popping, new roads drawing themselves, characters popping where
    // they appear) only play when animate is
    // true: the map open, flat. Needs the GL context.
    void update(float dt, bool animate);

    // Unlocked location whose monument is at that point (map pixels), if any
    std::optional<std::string> locationAt(glm::vec2 pos) const;
    // Drawn bigger
    void setHovered(std::optional<std::string> id);

private:
    float _width, _height;
    std::array<SSS::GL::Texture::Shared, 3> _textures;
    // Drawn in when needed, shown: these or a sprite's
    std::array<SSS::GL::Texture::Shared, 3> _overlays, _shown_overlays;
    std::array<glm::vec4, 3> _overlay_rects{};
    // Monuments & pins, by key, drawn once their images are loaded
    std::map<std::string, std::unique_ptr<Sprite>> _sprites;
    // Monuments' & icons' images, by path
    std::map<std::string, SSS::GL::Texture::Shared> _images;
    std::optional<std::string> _hovered;

    // Animations: the new places pop all at once, then the new roads draw
    // themselves one after another, then the characters just shown or moved
    // pop all at once
    float _pop_t{ 0.f };
    float _road_t{ 0.f };
    float _arrive_t{ 0.f };

    // What each layer & overlay was last baked from (overlay: empty if none)
    std::array<std::string, 3> _baked;
    std::array<std::string, 3> _overlay_baked;

    // Null until loaded
    SSS::GL::Texture const* _loadedImage(std::string const& path) const;
    // Map rect [min, max] (in: what's drawn, out: padded to whole pixels, on
    // the map) set as the layer's overlay rect. False: none, overlay off.
    bool _overlayRect(int layer, glm::vec2& min, glm::vec2& max);

    // In the whole layer, or only in its overlay around the roads drawing
    // themselves
    void _bakeRoads(std::vector<std::pair<Road const*, float>> const& roads, bool overlay);

    // Character's icon centered on pos, its tip pointing at target, scaled
    // (popping)
    struct Pin {
        Character const* character;
        glm::vec2 pos, target;
        float scale;
    };
    // Null until their images are loaded
    Sprite const* _monumentSprite(Location const& loc);
    Sprite const* _pinSprite(Pin const& pin);

    // Sprite centered on center (map pixels), scaled, brightened (hover)
    struct Placed {
        Sprite const* sprite;
        glm::vec2 center;
        float scale;
        float brighten;
    };
    static std::string _signature(std::vector<Placed> const& items);
    // The whole layer
    void _bakeLayer(int layer, std::vector<Placed> const& items);
    // In the layer's overlay, off if none
    void _showOverlay(int layer, std::vector<Placed> const& items);
};

} // namespace map_content
