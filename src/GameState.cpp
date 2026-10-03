#include "GameState.hpp"
#include "renpy/RenpyParser.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

static_assert(std::is_same_v<game_state::Value, renpy::Value>,
    "the dialog's variables are stored as they are");

namespace game_state {

static Vars values;

Vars const& vars() noexcept
{
    return values;
}

std::optional<Value> get(std::string const& name)
{
    auto const it = values.find(name);
    if (it == values.end()) return std::nullopt;
    return it->second;
}

void set(std::string const& name, Value value)
{
    values[name] = std::move(value);
}

void erase(std::string const& name)
{
    values.erase(name);
}

void reset()
{
    values.clear();
}

static fs::path savePath(std::string const& name)
{
    return fs::path(g->saves_folder) / (name + ".json");
}

// { "vars": { "name": value, ... } }
bool save(std::string const& name) try
{
    nlohmann::json vars_json = nlohmann::json::object();
    for (auto const& [key, value] : values)
        std::visit([&](auto const& v) { vars_json[key] = v; }, value);

    fs::path const path = savePath(name);
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    if (!file)
        SSS::throw_exc("cannot write " + path.string());
    file << nlohmann::json{ { "vars", vars_json } }.dump(4);
    return true;
}
catch (std::exception const& e) {
    LOG_CTX_ERR("game_state::save", e.what());
    return false;
}

bool load(std::string const& name) try
{
    fs::path const path = savePath(name);
    if (!fs::exists(path))
        return false;
    std::ifstream file(path, std::ios::binary);
    nlohmann::json const json = nlohmann::json::parse(file);

    Vars loaded;
    for (auto const& [key, value] : json.at("vars").items()) {
        if (value.is_string())               loaded[key] = value.get<std::string>();
        else if (value.is_boolean())         loaded[key] = value.get<bool>();
        else if (value.is_number_integer())  loaded[key] = value.get<int64_t>();
        else LOG_CTX_WRN("game_state::load", "unsupported value of '" + key + "' in " + path.string());
    }
    values = std::move(loaded);
    return true;
}
catch (std::exception const& e) {
    LOG_CTX_ERR("game_state::load", e.what());
    return false;
}

} // namespace game_state
