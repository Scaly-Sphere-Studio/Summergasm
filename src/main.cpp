#define SSS_LUA
#include "Summergasm.hpp"

using namespace SSS;

std::unique_ptr<GlobalData> g = std::make_unique<GlobalData>();

void register_scenes()
{
    std::set<std::string> init, loop;
    for (auto const& entry : std::filesystem::directory_iterator(g->lua_folder)) {
        std::string const path = entry.path().string();
        std::string const name = path.substr(path.rfind('/') + 1);
        // Ensure filename ends with .lua
        size_t n = name.rfind('.');
        if (n == std::string::npos || name.substr(n) != ".lua")
            continue;
        g->lua_scripts.emplace(name);
        n = name.rfind('_');
        if (n != std::string::npos) {
            std::string const end = name.substr(n);
            if (end == "_init.lua") {
                init.emplace(name.substr(0, n));
            }
            else if (end == "_loop.lua") {
                loop.emplace(name.substr(0, n));
            }
        }
    }
    for (std::string const& str : init) {
        if (loop.count(str) != 0) {
            g->lua_scenes.emplace(str);
        }
    }
}

bool lua_file_script(std::string const& path)
{
    if (g->lua_scripts.count(path) == 0) {
        LOG_FUNC_CTX_WRN("Given script wasn't registered", path);
        return true;
    }

    std::string const real_path = g->lua_folder + path;
    auto result = g->lua.safe_script_file(real_path, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        LOG_CTX_ERR("Lua file script", std::string("\n") + err.what());
        return true;
    }
    return false;
}

bool lua_loop_script()
{
    if (g->lua_loop_script.empty())
        return false;
    return lua_file_script(g->lua_loop_script);
}

bool lua_load_scene(std::string const& scene_name)
{
    if (g->lua_scenes.count(scene_name) == 0) {
        LOG_FUNC_CTX_WRN("Given scene wasn't registered", scene_name);
        return true;
    }
    if (lua_file_script(scene_name + "_init.lua"))
        return true;
    g->lua_loop_script = scene_name + "_loop.lua";
    return false;
}

int main(void) try
{
    //SSS::Log::louden(true);
    //SSS::Log::GL::Context::silence(true);
    //SSS::Log::GL::Callbacks::louden(true);
    //SSS::Log::GL::Callbacks::get().mouse_button = true;

    std::string const key = "Summergasm";
    auto const n = SSS::PWD.rfind(key);
    if (n != std::string::npos) {
        g->home_folder = SSS::PWD.substr(0, n + key.size() + 1);
        g->resources_folder = g->home_folder + "resources/";
        g->lua_folder = g->resources_folder + "lua/";
        g->assets_folder = g->resources_folder + "assets/";
    }
    register_scenes();

    g->lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::math, sol::lib::debug);
    sol::state& lua = g->lua;
    lua_setup(lua);
    TR::lua_setup_TR(lua);
    GL::lua_setup_GL(lua);
    Audio::lua_setup_Audio(lua);
    lua["file_script"] = lua_file_script;
    lua["load_scene"] = lua_load_scene;

    if (lua_file_script("global_setup.lua"))
        return -1;

    lua_file_script("audio_setup.lua");

    SSS::GL::Plane::on_click_funcs = {
        { 0, nullptr },
        { 1, button_func_1 },
        { 2, button_func_2 }
    };
    SSS::GL::Plane::passive_funcs = {
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
        SSS::GL::pollEverything();
        lua_loop_script();
        g->window->drawObjects();
        if (g->console_display)
            print_console();
        if (g->ui_display)
            print_imgui();
        g->window->printFrame();
        g->ui_window->printFrame();
    }

    g.reset();
}
CATCH_AND_LOG_FUNC_EXC