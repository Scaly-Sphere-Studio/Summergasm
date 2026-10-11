#include "CharacterData.hpp"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace dialog {

std::optional<TailStyle> parseTailStyle(std::string const& name)
{
    if (name == "straight") return TailStyle::Straight;
    if (name == "broken") return TailStyle::Broken;
    return std::nullopt;
}

static glm::vec2 vec2At(nlohmann::json const& json, char const* key)
{
    nlohmann::json const& v = json.at(key);
    if (!v.is_array() || v.size() != 2)
        SSS::throw_exc(std::string("'") + key + "' must be [x, y]");
    return { v[0].get<float>(), v[1].get<float>() };
}

static std::optional<uint32_t> colorAt(nlohmann::json const& json, std::string const& id)
{
    if (!json.contains("color"))
        return std::nullopt;
    std::string const value = json.at("color").get<std::string>();
    uint32_t color = 0;
    auto const parsed = value.size() == 7 && value.front() == '#'
        ? std::from_chars(value.data() + 1, value.data() + value.size(), color, 16)
        : std::from_chars(value.data(), value.data(), color, 16);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
        LOG_CTX_WRN("CharacterData", id + ".json: invalid color \"" + value + "\" (expected #RRGGBB)");
        return std::nullopt;
    }
    return color;
}

std::optional<CharacterData::CharacterInfo> CharacterData::load(std::string const& id) try
{
    fs::path const path = fs::path(g->characters_folder) / (id + ".json");
    if (!fs::exists(path)) {
        LOG_CTX_WRN("CharacterData", "no " + path.string() + ": no speech bubble tail for '" + id + "'");
        return std::nullopt;
    }
    std::ifstream file(path, std::ios::binary);
    nlohmann::json const json = nlohmann::json::parse(file);

    CharacterInfo character;
    character.color = colorAt(json, id);
    for (auto const& [name, e] : json.at("expressions").items()) {
        ExpressionInfo expression;
        expression.mouth = vec2At(e, "mouth");
        glm::vec2 const dir = vec2At(e, "mouth_dir");
        if (glm::length(dir) > 1e-4f) expression.dir = glm::normalize(dir);
        else LOG_CTX_WRN("CharacterData", id + ".json: '" + name + "' has a null mouth_dir, pointing down");
        if (e.contains("tail")) {
            std::string const tail = e.at("tail").get<std::string>();
            if (auto const style = parseTailStyle(tail)) expression.tail = *style;
            else LOG_CTX_WRN("CharacterData", id + ".json: unknown tail \"" + tail + "\" (straight, broken), using straight");
        }
        if (e.contains("intensity"))
            expression.intensity = std::clamp(e.at("intensity").get<float>(), 0.f, 1.f);
        character.expressions[name] = expression;
    }
    if (!character.expressions.contains("neutral"))
        LOG_CTX_WRN("CharacterData", id + ".json has no \"neutral\" expression");
    return character;
}
catch (std::exception const& e) {
    LOG_CTX_ERR("CharacterData", id + ".json: " + e.what());
    return std::nullopt;
}

void CharacterData::reload()
{
    _chars.clear();
    _aliases.clear();
    _image_prefixes.clear();
    _aliases_loaded = false;
}

std::string CharacterData::canonicalId(std::string const& id)
{
    if (!_aliases_loaded) {
        _aliases_loaded = true;
        fs::path const path = fs::path(g->characters_folder) / "aliases.json";
        if (fs::exists(path)) {
            try {
                std::ifstream file(path, std::ios::binary);
                nlohmann::json const json = nlohmann::json::parse(file);
                nlohmann::json const& aliases = json.at("aliases");
                if (!aliases.is_object())
                    SSS::throw_exc(path.string() + ": \"aliases\" must be an object");
                for (auto const& [alias, canonical] : aliases.items()) {
                    if (!canonical.is_string() || canonical.get<std::string>().empty())
                        SSS::throw_exc(path.string() + ": alias targets must be non-empty strings");
                    _aliases[alias] = canonical.get<std::string>();
                }
                if (json.contains("image_prefixes")) {
                    nlohmann::json const& prefixes = json.at("image_prefixes");
                    if (!prefixes.is_object())
                        SSS::throw_exc(path.string() + ": \"image_prefixes\" must be an object");
                    for (auto const& [character, prefix] : prefixes.items()) {
                        if (!prefix.is_string() || prefix.get<std::string>().empty())
                            SSS::throw_exc(path.string() + ": image prefixes must be non-empty strings");
                        _image_prefixes[character] = prefix.get<std::string>();
                    }
                }
            }
            catch (std::exception const& e) {
                LOG_CTX_ERR("CharacterData", path.string() + ": " + e.what());
                _aliases.clear();
                _image_prefixes.clear();
            }
        }
    }
    auto const it = _aliases.find(id);
    return it == _aliases.end() ? id : it->second;
}

std::optional<CharacterData::CharacterInfo> const& CharacterData::info(std::string const& id)
{
    std::string const canonical = canonicalId(id);
    auto it = _chars.find(canonical);
    if (it == _chars.end())
        it = _chars.emplace(canonical, load(canonical)).first;
    return it->second;
}

std::optional<ExpressionInfo> CharacterData::get(std::string const& id, std::string const& expression)
{
    auto const& character = info(id);
    if (!character)
        return std::nullopt;
    Expressions const& expressions = character->expressions;
    if (auto const e = expressions.find(expression); e != expressions.end())
        return e->second;
    if (auto const e = expressions.find("neutral"); e != expressions.end())
        return e->second;
    return std::nullopt;
}

std::optional<uint32_t> CharacterData::color(std::string const& id)
{
    auto const& character = info(id);
    return character ? character->color : std::nullopt;
}

std::string CharacterData::imagePrefix(std::string const& id)
{
    std::string const canonical = canonicalId(id);
    auto const it = _image_prefixes.find(canonical);
    return it == _image_prefixes.end() ? canonical : it->second;
}

} // namespace dialog
