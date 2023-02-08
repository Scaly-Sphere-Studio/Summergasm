#define SSS_LUA
#include "Summergasm.hpp"

using namespace SSS;

std::unique_ptr<GlobalData> g = std::make_unique<GlobalData>();

Scene::Scene(std::string const& filename) try
    : path(g->lua_folder + filename)
{
    if (!env.valid()) {
        SSS::throw_exc("Could not initialize environment properly.");
        return;
    }
    env["filename"] = filename;
    env["is_loading"] = true;
    env["is_running"] = false;
    env["is_unloading"] = false;
    run();
    env["is_loading"] = false;
    env["is_running"] = true;
}
CATCH_AND_RETHROW_FUNC_EXC;

Scene::~Scene()
{
    env["is_running"] = false;
    env["is_unloading"] = true;
    run();
}

bool Scene::run()
{
    auto result = g->lua.safe_script_file(path, env, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error const err = result;
        std::cout << "\n" << err.what() << "\n\n";
        return true;
    }
    return false;
}

void mylua_register_scripts()
{
    for (auto const& entry : std::filesystem::directory_iterator(g->lua_folder)) {
        std::string const path = entry.path().string();
        std::string const name = path.substr(path.rfind('/') + 1);
        // Ensure filename ends with .lua
        size_t dot = name.rfind('.');
        if (dot == std::string::npos || name.substr(dot) != ".lua")
            continue;
        if (g->lua_scripts.count(name) == 0)
            g->lua_scripts.try_emplace(name);
    }
}

static std::string complete_script_name(std::string const& name)
{
    size_t const ret = name.rfind(".lua");
    if (ret != std::string::npos && ret + 4 == name.length()) {
        return name;
    }
    return name + ".lua";
}

bool mylua_file_script(std::string const& path)
{
    std::string const real_path = g->lua_folder + complete_script_name(path);
    auto result = g->lua.safe_script_file(real_path, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        std::cout << "\n" << err.what() << "\n\n";
        return true;
    }
    return false;
}

bool mylua_run_active_scenes()
{
    bool ret = false;
    for (auto const& pair : g->lua_scripts) {
        if (pair.second)
            pair.second->run();
    }
    return ret;
}

bool mylua_load_scene(std::string const& scene_name)
{
    std::string const script_name = complete_script_name(scene_name);
    if (g->lua_scripts.count(script_name) == 0) {
        LOG_FUNC_CTX_WRN("Given script wasn't registered", script_name);
        return true;
    }
    auto& scene = g->lua_scripts[script_name];
    if (scene) {
        LOG_FUNC_CTX_WRN("Given scene is already running", script_name);
        return true;
    }
    scene = std::make_unique<Scene>(script_name);
    return false;
}

bool mylua_unload_scene(std::string const& scene_name)
{
    std::string const script_name = complete_script_name(scene_name);
    if (g->lua_scripts.count(script_name) == 0) {
        LOG_FUNC_CTX_WRN("Given script wasn't registered", script_name);
        return true;
    }
    auto& scene = g->lua_scripts[script_name];
    if (!scene) {
        LOG_FUNC_CTX_WRN("Given scene was not running", script_name);
        return true;
    }
    scene.reset();
    return false;
}

int main(void) try
{
    //Log::louden(true);
    //Log::GL::Context::silence(true);
    //Log::GL::Callbacks::louden(true);
    //Log::GL::Callbacks::get().mouse_button = true;

    std::string const key = "Summergasm";
    auto const n = PWD.rfind(key);
    if (n != std::string::npos) {
        g->home_folder = PWD.substr(0, n + key.size() + 1);
        g->resources_folder = g->home_folder + "resources/";
        g->lua_folder = g->resources_folder + "lua/";
        g->assets_folder = g->resources_folder + "assets/";
    }
    mylua_register_scripts();

    g->lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::math, sol::lib::debug);
    sol::state& lua = g->lua;
    lua_setup(lua);
    TR::lua_setup_TR(lua);
    GL::lua_setup_GL(lua);
    Audio::lua_setup_Audio(lua);
    lua["file_script"] = mylua_file_script;
    lua["load_scene"] = mylua_load_scene;
    lua["unload_scene"] = mylua_unload_scene;
    lua["clear_map"] = []() { g->lua_scripts.clear(); };

    if (mylua_file_script("global_setup.lua"))
        return -1;

    mylua_file_script("audio_setup.lua");

    GL::Plane::on_click_funcs = {
        { 0, nullptr },
        { 1, button_func_1 },
        { 2, button_func_2 }
    };
    GL::Plane::passive_funcs = {
        { 0, nullptr },
        { 1, passive_func_1 },
        { 2, passive_func_2 }
    };

    // Main Window callbacks
    g->window = lua["window"];
    g->window->setCallback(glfwSetKeyCallback, key_callback);
    // UI Window callbacks
    g->ui_window = lua["ui_window"];
    g->ui_window->setCallback(glfwSetKeyCallback, key_callback);
    g->ui_window->setCallback(glfwSetWindowCloseCallback, close_callback);

    // Main loop
    while (!g->window->shouldClose()) {
        GL::pollEverything();
        mylua_run_active_scenes();
        g->window->drawObjects();
        if (g->console_display)
            print_console();
        if (g->ui_display)
            print_imgui();
        g->window->printFrame();
        g->ui_window->printFrame();
    }

    g->lua_scripts.clear();
    g.reset();
}
CATCH_AND_LOG_FUNC_EXC