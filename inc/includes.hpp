#pragma once

#include <SSS/ImGuiH.hpp>
#include <SSS/GL.hpp>
#include <SSS/Audio.hpp>

#pragma warning(push, 0)
#include <nlohmann/json.hpp>
#pragma warning(pop)

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

class Scene;

struct GlobalData {
    SSS::GL::Window* window;
    SSS::GL::Window* ui_window;

    bool ui_display{ false };
    bool ui_use_separate_window{ false };
    bool console_display{ false };

    std::string home_folder;
    std::string resources_folder;
    std::string lua_folder;
    std::string assets_folder;

    sol::state lua;
    std::map<std::string, std::unique_ptr<Scene>> lua_scenes;

    std::vector<std::string> texts;
};
extern std::unique_ptr<GlobalData> g;

void exitSummergasm(int status);