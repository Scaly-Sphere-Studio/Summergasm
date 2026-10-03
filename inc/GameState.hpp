#pragma once

#include "includes.hpp"

#include <variant>

// Values kept for the whole game and saved with it, e.g. the player's
// choices. The dialog stores its script variables here (`default var = ...`,
// `$ var = ...`, see Dialog.hpp): a conversation starts with these values,
// and a `default` only sets the variables missing.
//
// Bound to Lua as game, see mylua.cpp:
//   game.vars.choice = "beach"     game.vars.choice  (nil when unset)
//   game.save([name])  game.load([name])  game.reset()
namespace game_state {

// Same type as renpy::Value: "text", 12, True / False
using Value = std::variant<std::string, int64_t, bool>;
using Vars = std::map<std::string, Value>;

Vars const& vars() noexcept;
std::optional<Value> get(std::string const& name);
void set(std::string const& name, Value value);
void erase(std::string const& name);
// Every value is erased (a new game)
void reset();

// saves/<name>.json, "save" by default. Return false on error (logged);
// load() also returns false when there is no such save, the values being kept.
bool save(std::string const& name = "save");
bool load(std::string const& name = "save");

} // namespace game_state
