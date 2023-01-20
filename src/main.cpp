#define SSS_LUA
#include "Summergasm.hpp"

using namespace SSS;

std::unique_ptr<GlobalData> g = std::make_unique<GlobalData>();

void register_scenes()
{
    for (auto const& entry : std::filesystem::directory_iterator(g->resources + "lua/")) {
        std::string const path = entry.path().string();
        std::string const name = path.substr(path.rfind('/') + 1);
        // Ensure filename ends with .lua
        size_t const dot = name.rfind('.');
        if (dot == std::string::npos || name.substr(dot) != ".lua")
            continue;
        LOG_MSG(name);
    }
}

bool lua_file_script(std::string const& path)
{
    std::string real_path = g->resources + "lua/" + path;
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
    std::string const path = g->lua["loop_filepath"];
    if (path.empty())
        return false;
    return lua_file_script(path);
}

bool lua_load_scene(std::string const& scene_name)
{
    if (lua_file_script(scene_name + "_init.lua"))
        return true;
    g->lua["loop_filepath"] = scene_name + "_loop.lua";
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
        g->home = SSS::PWD.substr(0, n + key.size() + 1);
        g->resources = g->home + "resources/";
    }
    register_scenes();

    g->lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::math);
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