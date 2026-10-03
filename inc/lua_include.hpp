#pragma once

// sol2 is heavy to parse: include this header only where Lua is actually used
// (g->lua(), sol::...), not from the common includes.hpp.

#include "includes.hpp"

#ifndef SOL_ALL_SAFETIES_ON
#define SOL_ALL_SAFETIES_ON 1
#endif
#include <sol/sol.hpp>

inline sol::state& GlobalData::lua() const { return *_lua; }
