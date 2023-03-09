#include "Summergasm.hpp"

std::unique_ptr<GlobalData> g = std::make_unique<GlobalData>();

int main(void) try
{
    //Log::louden(true);
    //Log::GL::Context::silence(true);
    //Log::GL::Callbacks::louden(true);
    //Log::GL::Callbacks::get().mouse_button = true;

    std::string const key = "Summergasm";
    auto const n = SSS::PWD.rfind(key);
    if (n != std::string::npos) {
        g->home_folder = SSS::PWD.substr(0, n + key.size() + 1);
        g->resources_folder = g->home_folder + "resources/";
        g->lua_folder = g->resources_folder + "lua/";
        g->assets_folder = g->resources_folder + "assets/";
    }

    if (setup_lua())
        return -1;

    // Main Window callbacks
    g->window = g->lua["window"];
    g->window->setCallback(glfwSetKeyCallback, key_callback);
    // UI Window callbacks
    g->ui_window = g->lua["ui_window"];
    g->ui_window->setCallback(glfwSetKeyCallback, key_callback);
    g->ui_window->setCallback(glfwSetWindowCloseCallback, close_callback);

    // Main loop
    while (!g->window->shouldClose()) {
        SSS::GL::pollEverything();
        mylua_run_active_scenes();
        g->window->drawObjects();
        if (g->console_display)
            print_console();
        if (g->ui_display)
            print_imgui();
        g->window->printFrame();
        g->ui_window->printFrame();
    }

    g->lua_scenes.clear();
    g.reset();
}
CATCH_AND_LOG_FUNC_EXC