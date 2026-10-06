#include "Summergasm.hpp"
#include "Dialog.hpp"
#include "SignalManager.hpp"
#include "Settings.hpp"

GlobalData::GlobalData() : _lua(std::make_unique<sol::state>()) {}
GlobalData::~GlobalData() = default;

std::unique_ptr<GlobalData> g = std::make_unique<GlobalData>();

// Laptops with a dedicated GPU: ask the NVIDIA Optimus & AMD switchable
// graphics drivers for it, instead of the integrated one
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

void free_imgui_objects();

void exitSummergasm(int status)
{
    free_imgui_objects();
    mylua_free_dialog();
    // Lua handlers must go before the Lua state does
    signals::clear();
    g->lua_scenes.clear();
    g->lua().collect_garbage();
    g->ui_window->close();
    g->window->close();
    g.reset();
    // Closes the OpenAL device now: left to sss-audio's static destructor, it
    // happens while DLLs unload, after OpenAL's threads were killed, and can
    // hang forever (the process stays alive and the .exe locked)
    SSS::Audio::terminate();
    LOG_CTX_MSG("Exiting with status", status);
    exit(status);
}

// Places the development watermark, and keeps its renderer drawn last so
// that it stays on top of any renderer added by scenes
static void update_watermark()
{
    sol::object const obj = g->lua()["watermark_renderer"];
    if (!obj.is<SSS::GL::RendererBase*>())
        return;
    g->lua()["update_watermark"]();
    auto const renderer = obj.as<SSS::GL::RendererBase*>()->getSharedBase();
    auto const& renderers = g->window->getRenderers();
    if (renderers.empty() || renderers.back() != renderer) {
        g->window->removeRenderer(renderer);
        g->window->addRenderer(renderer);
    }
}

int main(void) try
{
    // glad is linked statically: let SSS::GL also load this module's OpenGL
    // functions when it creates the context (SceneRenderer calls OpenGL)
    SSS_GL_EXPOSE_OPENGL;

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
        g->dialogs_folder = g->resources_folder + "dialogs/";
        g->characters_folder = g->resources_folder + "characters/";
        g->sounds_folder = g->resources_folder + "sounds/";
        g->musics_folder = g->resources_folder + "musics/";
        g->saves_folder = g->home_folder + "saves/";
        SSS::GL::Texture::setResourceFolder(g->assets_folder);
        SceneRenderer::shaders_folder = g->resources_folder + "shaders/";
        settings::load();
    }

    static_cast<void>(SSS::Audio::getDevices());
    if (setup_lua())
        return -1;

    // Main Window callbacks
    g->window->setCallback(glfwSetKeyCallback, key_callback);
    set_scroll_callback(g->window->getGLFWwindow());
    // UI Window callbacks
    g->ui_window->setCallback(glfwSetKeyCallback, key_callback);
    g->ui_window->setCallback(glfwSetWindowCloseCallback, close_callback);

    // Main loop
    while (!g->window->shouldClose()) {
        SSS::GL::pollEverything();
        // Back to the menu (scenes are switched outside of GLFW callbacks), or quit from it
        if (std::exchange(g->escape_pressed, false) && !mylua_on_escape())
            glfwSetWindowShouldClose(g->window->getGLFWwindow(), GLFW_TRUE);
        mylua_watch_scenes();
        mylua_run_active_scenes();
        // Started by the scenes, before scroll_y is reset (scrolls their log)
        Dialog::updateAll();
        // Music fades & audio events, after the scenes' play_music() requests
        SSS::Audio::update();
        g->scroll_y = 0.0;
        update_watermark();
        g->window->drawObjects();
        if (g->console_display)
            print_console();
        if (g->settings_display)
            print_settings();
        if (g->ui_display)
            print_imgui();
        g->window->printFrame();
        g->ui_window->printFrame();
    }

    exitSummergasm(0);
}
CATCH_AND_LOG_FUNC_EXC