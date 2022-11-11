#define SSS_LUA
#include "Summergasm.hpp"

using namespace SSS;

std::unique_ptr<GlobalData> g_data = std::make_unique<GlobalData>();

bool lua_file_script(std::string const& path)
{
    auto result = g_data->lua.safe_script_file(path, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        LOG_CTX_ERR("Lua file script", std::string("\n") + err.what());
        return true;
    }
    return false;
}

int main(void) try
{
    //SSS::Log::louden(true);
    //SSS::Log::GL::Context::silence(true);
    //SSS::Log::GL::Callbacks::louden(true);
    //SSS::Log::GL::Callbacks::get().mouse_button = true;

    g_data->lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::math);
    sol::state& lua = g_data->lua;
    lua_setup(lua);
    TR::lua_setup_TR(lua);
    GL::lua_setup_GL(lua);
    SSS::Audio::lua_load_audio_functions(lua.globals());

    lua_file_script("resources/Lua/audio.lua");
    if (lua_file_script("resources/Lua/Init.lua"))
        return -1;

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
    g_data->window = lua["window"];
    g_data->window->setCallback(glfwSetKeyCallback, key_callback);
    // UI Window callbacks
    g_data->ui_window = lua["ui_window"];
    g_data->ui_window->setCallback(glfwSetKeyCallback, key_callback);
    g_data->ui_window->setCallback(glfwSetWindowCloseCallback, close_callback);

    // Main loop
    while (!g_data->window->shouldClose()) {
        SSS::GL::pollEverything();
        lua_file_script("resources/Lua/Loop.lua");
        g_data->window->drawObjects();
        if (g_data->console_display)
            print_console();
        if (g_data->ui_display)
            print_imgui();
        g_data->window->printFrame();
        g_data->ui_window->printFrame();
    }

    g_data.reset();
}
CATCH_AND_LOG_FUNC_EXC