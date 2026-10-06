#include "CharacterData.hpp"

#include <algorithm>
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

static std::optional<std::unordered_map<std::string, ExpressionInfo>> load(std::string const& id) try
{
    fs::path const path = fs::path(g->characters_folder) / (id + ".json");
    if (!fs::exists(path)) {
        LOG_CTX_WRN("CharacterData", "no " + path.string() + ": no speech bubble tail for '" + id + "'");
        return std::nullopt;
    }
    std::ifstream file(path, std::ios::binary);
    nlohmann::json const json = nlohmann::json::parse(file);

    std::unordered_map<std::string, ExpressionInfo> expressions;
    for (auto const& [name, e] : json.at("expressions").items()) {
        ExpressionInfo info;
        info.mouth = vec2At(e, "mouth");
        glm::vec2 const dir = vec2At(e, "mouth_dir");
        if (glm::length(dir) > 1e-4f) info.dir = glm::normalize(dir);
        else LOG_CTX_WRN("CharacterData", id + ".json: '" + name + "' has a null mouth_dir, pointing down");
        if (e.contains("tail")) {
            std::string const tail = e.at("tail").get<std::string>();
            if (auto const style = parseTailStyle(tail)) info.tail = *style;
            else LOG_CTX_WRN("CharacterData", id + ".json: unknown tail \"" + tail + "\" (straight, broken), using straight");
        }
        if (e.contains("intensity"))
            info.intensity = std::clamp(e.at("intensity").get<float>(), 0.f, 1.f);
        expressions[name] = info;
    }
    if (!expressions.contains("neutral"))
        LOG_CTX_WRN("CharacterData", id + ".json has no \"neutral\" expression");
    return expressions;
}
catch (std::exception const& e) {
    LOG_CTX_ERR("CharacterData", id + ".json: " + e.what());
    return std::nullopt;
}

std::optional<ExpressionInfo> CharacterData::get(std::string const& id, std::string const& expression)
{
    auto it = _chars.find(id);
    if (it == _chars.end())
        it = _chars.emplace(id, load(id)).first;
    if (!it->second)
        return std::nullopt;
    Expressions const& expressions = *it->second;
    if (auto const e = expressions.find(expression); e != expressions.end())
        return e->second;
    if (auto const e = expressions.find("neutral"); e != expressions.end())
        return e->second;
    return std::nullopt;
}

} // namespace dialog
