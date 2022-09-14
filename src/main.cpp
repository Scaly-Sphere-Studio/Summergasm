#include "Summergasm.hpp"

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

void lua_load_log_functions(sol::state& lua)
{
    lua["log_msg"] = SSS::log_msg<std::string>;
    lua["log_wrn"] = SSS::log_wrn<std::string>;
    lua["log_err"] = SSS::log_err<std::string>;
}

int main(void) try
{
    //SSS::Log::louden(true);
    //SSS::Log::GL::Context::silence(true);
    //SSS::Log::GL::Callbacks::louden(true);
    //SSS::Log::GL::Callbacks::get().mouse_button = true;

    g_data->lua.open_libraries(sol::lib::base, sol::lib::string);
    lua_load_log_functions(g_data->lua);
    //sol::table sss = g_data->lua["SSS"].get_or_create<sol::table>();
    //SSS::Audio::lua_load_audio_functions(sss);
    SSS::Audio::lua_load_audio_functions(g_data->lua.globals());

    lua_file_script("resources/Lua/audio.lua");

    SSS::GL::Plane::on_click_funcs = {
        { 0, nullptr },
        { 1, button_func_1 },
        { 2, button_func_2 }
    };
    SSS::GL::Plane::passive_funcs = {
        { 0, nullptr },
        { 1, passive_func_1 },
        { 2, passive_func_2 /*nullptr*/ }
    };

    // Load TR
    loadTextAreas("resources/json/TextRendering.json");

    // Create window & set callbacks
    g_data->window = createWindow("resources/json/Window.json");
    SSS::GL::Window::Shared& window = g_data->window;
    {
        SSS::GL::Context const context(window);
        window->setCallback(glfwSetKeyCallback, key_callback);
        window->setVSYNC(true);
        window->setFPSLimit(120);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        // Load static objects
        loadScene(window, "resources/json/StaticObjects.json");
        // Load scene
        loadScene(window, "resources/json/Scene1.json");
    }

    SSS::GL::Window::CreateArgs args;
    args.title = "Summergasm - ImGUI";
    args.w = 600;
    args.h = 600;
    args.monitor_id = 1;
    args.hidden = true;
    g_data->ui_window= SSS::GL::Window::create(args);
    g_data->ui_window->setCallback(glfwSetKeyCallback, key_callback);
    g_data->ui_window->setCallback(glfwSetWindowCloseCallback, close_callback);

    // Main loop
    while (!window->shouldClose()) {
        SSS::GL::pollEverything();
        window->drawObjects();
        if (g_data->console_display)
            print_console();
        if (g_data->ui_display)
            print_imgui();
        window->printFrame();
        g_data->ui_window->printFrame();
    }

    g_data.reset();
}
CATCH_AND_LOG_FUNC_EXC