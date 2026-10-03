#pragma once

// Precompiled header, force-included in every source file (see the vcxproj).
// Only put heavy and stable third-party headers here.

#include <algorithm>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <SSS/ImGuiH.hpp>
#include <SSS/GL.hpp>
#include <SSS/Audio.hpp>

#pragma warning(push, 0)
#include <nlohmann/json.hpp>
#pragma warning(pop)

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
