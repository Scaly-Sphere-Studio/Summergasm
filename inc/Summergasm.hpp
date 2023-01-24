#pragma once

#define SSS_LUA
#include <SSS/ImGuiH.hpp>
#include <SSS/GL.hpp>
#include <SSS/Audio.hpp>

#pragma warning(push, 0)
#include <nlohmann/json.hpp>
#pragma warning(pop)

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

// Lua

enum class SceneState {
    Inactive,
    Loading,
    Running,
    Unloading,
    NotRegistered
};

// Static data

struct GlobalData {
    SSS::GL::Window::Shared window;
    SSS::GL::Window::Shared ui_window;
    
    bool ui_display{ false };
    bool ui_use_separate_window{ false };
    bool console_display{ false };
    
    std::string home_folder;
    std::string resources_folder;
    std::string lua_folder;
    std::string assets_folder;

    sol::state lua;
    std::map<std::string, SceneState> lua_scripts;

    std::vector<std::string> texts;
};
extern std::unique_ptr<GlobalData> g;

// Lua

void mylua_register_scripts();
bool mylua_file_script(std::string const& path);
bool mylua_run_active_scenes();
bool mylua_load_scene(std::string const& scene_name);
bool mylua_unload_scene(std::string const& scene_name);
SceneState mylua_get_script_state(std::string const& scene_name);

// Callbacks

void key_callback(GLFWwindow* ptr, int key, int scancode, int action, int mods);
void close_callback(GLFWwindow* ptr);

// Model functions

void button_func_1(SSS::GL::Window::Shared window, SSS::GL::Plane::Shared plane,
    int button, int action, int mods);
void button_func_2(SSS::GL::Window::Shared window, SSS::GL::Plane::Shared plane,
    int button, int action, int mods);

void passive_func_1(SSS::GL::Window::Shared window, SSS::GL::Plane::Shared plane);
void passive_func_2(SSS::GL::Window::Shared window, SSS::GL::Plane::Shared plane);

// ImGui

void print_console();
void print_imgui();
