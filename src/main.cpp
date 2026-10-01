#include "Summergasm.hpp"

std::unique_ptr<GlobalData> g = std::make_unique<GlobalData>();

void free_imgui_objects();

void exitSummergasm(int status)
{
    free_imgui_objects();
    g->lua_scenes.clear();
    g->lua.collect_garbage();
    g->ui_window->close();
    g->window->close();
    g.reset();
    LOG_CTX_MSG("Exiting with status", status);
    exit(status);
}

// Places the development watermark, and keeps its renderer drawn last so
// that it stays on top of any renderer added by scenes
static void update_watermark()
{
    sol::object const obj = g->lua["watermark_renderer"];
    if (!obj.is<SSS::GL::RendererBase*>())
        return;
    g->lua["update_watermark"]();
    auto const renderer = obj.as<SSS::GL::RendererBase*>()->getSharedBase();
    auto const& renderers = g->window->getRenderers();
    if (renderers.empty() || renderers.back() != renderer) {
        g->window->removeRenderer(renderer);
        g->window->addRenderer(renderer);
    }
}

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
        SSS::GL::Texture::setResourceFolder(g->assets_folder);
    }

    static_cast<void>(SSS::Audio::getDevices());
    if (setup_lua())
        return -1;

    // Main Window callbacks
    g->window->setCallback(glfwSetKeyCallback, key_callback);
    // UI Window callbacks
    g->ui_window->setCallback(glfwSetKeyCallback, key_callback);
    g->ui_window->setCallback(glfwSetWindowCloseCallback, close_callback);

    // Main loop
    while (!g->window->shouldClose()) {
        SSS::GL::pollEverything();
        mylua_run_active_scenes();
        update_watermark();
        g->window->drawObjects();
        if (g->console_display)
            print_console();
        if (g->ui_display)
            print_imgui();
        g->window->printFrame();
        g->ui_window->printFrame();
    }

    exitSummergasm(0);
}
CATCH_AND_LOG_FUNC_EXC