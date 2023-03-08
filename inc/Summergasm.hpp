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

class Scene;

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
    std::map<std::string, std::unique_ptr<Scene>> lua_scripts;

    std::vector<std::string> texts;
};
extern std::unique_ptr<GlobalData> g;

// Lua

class Scene {
public:
	Scene() = delete;
	Scene(std::string const& filename);
	~Scene();

	Scene(const Scene&)				= delete;	// Copy constructor
	Scene(Scene&&)					= delete;	// Move constructor
	Scene& operator=(const Scene&)	= delete;	// Copy assignment
	Scene& operator=(Scene&&)		= delete;	// Move assignment

    bool run();

private:
	sol::environment env{ g->lua, sol::create, g->lua.globals() };
	std::string path;
};

void mylua_register_scripts();
bool mylua_file_script(std::string const& path);
bool mylua_run_active_scenes();
bool mylua_load_scene(std::string const& scene_name);
bool mylua_unload_scene(std::string const& scene_name);

// Callbacks

void key_callback(GLFWwindow* ptr, int key, int scancode, int action, int mods);
void close_callback(GLFWwindow* ptr);

// ImGui

void print_console();
void print_imgui();
