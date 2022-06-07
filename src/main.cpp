#include "Summergasm.hpp"

std::unique_ptr<GlobalData> g_data = std::make_unique<GlobalData>();

int main(void) try
{
    //SSS::Log::louden(true);
    //SSS::Log::GL::Context::silence(true);
    //SSS::Log::GL::Callbacks::louden(true);

    SSS::GL::Plane::on_click_funcs = {
        { 1, button_func_1 },
        { 2, button_func_2 }
    };
    SSS::GL::Plane::passive_funcs = {
        { 1, passive_func_1 },
        { 2, passive_func_2 /*nullptr*/ }
    };

    // Load TR
    SSS::TR::init();
    SSS::TR::addFontDir("C:/Fonts");
    loadTextAreas("resources/json/TextRendering.json");

    // Create window & set callbacks
    g_data->window = createWindow("resources/json/Window.json");
    SSS::GL::Window::Shared& window = g_data->window;
    {
        SSS::GL::Context const context(window);
        window->setCallback(glfwSetKeyCallback, key_callback);
        window->setCallback(glfwSetCharCallback, char_callback);
        window->setVSYNC(false);
        window->setFPSLimit(120);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        // Load objects
        loadWindowObjects(window, "resources/json/WindowObjects.json");
        // Manually set renderers, for now
        organizeRenderers(window, "resources/json/Scene1.json");
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

    SSS::ImGuiH::init();
    SSS::ImGuiH::setContext((!g_data->ui_use_separate_window ?
        window : g_data->ui_window)->getGLFWwindow());

    // Main loop
    while (SSS::GL::pollEverything()) {
        if (window && window->shouldClose()) {
            window->cleanObjects();
            window.reset();
            continue;
        }
        if (window)
            window->drawObjects();
        if (g_data->ui_display)
            print_imgui();
        if (window)
            window->printFrame();
        if (g_data->ui_window)
            g_data->ui_window->printFrame();
    }

    SSS::ImGuiH::shutdown();
    SSS::TR::terminate();
    g_data.reset();
}
CATCH_AND_LOG_FUNC_EXC