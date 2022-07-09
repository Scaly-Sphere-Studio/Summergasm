#include "Summergasm.hpp"

std::unique_ptr<GlobalData> g_data = std::make_unique<GlobalData>();

int main(void) try
{
    //SSS::Log::louden(true);
    //SSS::Log::GL::Context::silence(true);
    //SSS::Log::GL::Callbacks::louden(true);

    SSS::Audio::Buffer::Map const& buffers = SSS::Audio::getBuffers();
    SSS::Audio::createBuffer(0);
    buffers.at(0)->loadFile("resources/sounds/bat-la-rate.mp3");
    SSS::Audio::createBuffer(1);
    buffers.at(1)->loadFile("resources/sounds/ok.mp3");

    SSS::Audio::Source::Array const& sources = SSS::Audio::getSources();
    SSS::Audio::createSource(0);
    SSS::Audio::Source::Ptr const& source0 = sources.at(0);
    source0->setLooping(true);
    source0->setVolume(100);
    source0->queueBuffers({ 0, 1 });
    //source0->play();

    SSS::GL::Plane::on_click_funcs = {
        { 1, button_func_1 },
        { 2, button_func_2 }
    };
    SSS::GL::Plane::passive_funcs = {
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

    // Main loop
    while (!window->shouldClose()) {
        SSS::GL::pollEverything();
        window->drawObjects();
        if (g_data->ui_display)
            print_imgui();
        window->printFrame();
        g_data->ui_window->printFrame();
    }

    g_data.reset();
}
CATCH_AND_LOG_FUNC_EXC