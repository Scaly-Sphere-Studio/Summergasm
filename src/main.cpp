#include "Summergasm.hpp"

std::unique_ptr<GlobalData> g_data = std::make_unique<GlobalData>();

void key_callback(GLFWwindow* ptr, int key, int scancode, int action, int mods)
{
    SSS::GL::Window::Shared const window = SSS::GL::Window::get(ptr);

    if (action == GLFW_PRESS) {
        switch (key) {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(ptr, GLFW_TRUE);
            break;
        case GLFW_KEY_F11:
            window->setFullscreen(!window->isFullscreen());
            break;
        case GLFW_KEY_F1:
        {
            g_data->ui_display = !g_data->ui_display;
            if (g_data->ui_use_separate_window) {
                if (g_data->ui_display) {
                    glfwShowWindow(g_data->ui_window->getGLFWwindow());
                }
                else {
                    glfwHideWindow(g_data->ui_window->getGLFWwindow());
                }
            }
            break;
        }
        }
    }
}

void close_callback(GLFWwindow* ptr)
{
    g_data->ui_display = false;
    glfwSetWindowShouldClose(ptr, GLFW_FALSE);
    glfwHideWindow(g_data->ui_window->getGLFWwindow());
}

void button_func_1(GLFWwindow* ptr, uint32_t id, int button, int action, int mods)
{
    __LOG_MSG("foo");
}

void button_func_2(GLFWwindow* ptr, uint32_t id, int button, int action, int mods)
{
    static uint32_t text_id = 1;
    if (action == GLFW_PRESS && text_id < g_data->texts.size()) {
        SSS::TR::TextArea::getTextAreas().at(0)->parseString(g_data->texts[text_id++]);
    }
}

void passive_func_1(GLFWwindow* ptr, uint32_t id)
{
    SSS::GL::Window::Shared const window = SSS::GL::Window::get(ptr);
    if (!window) return;
    SSS::GL::Window::Objects const& objects = window->getObjects();
    SSS::GL::Plane::Ptr const& plane = objects.planes.at(id);

    plane->rotate(glm::vec3(0.1f));
}

void passive_func_2(GLFWwindow* ptr, uint32_t id)
{
    SSS::GL::Window::Shared const window = SSS::GL::Window::get(ptr);
    if (!window) return;
    SSS::GL::Window::Objects const& objects = window->getObjects();
    SSS::GL::Plane::Ptr const& plane = objects.planes.at(id);

    static std::chrono::steady_clock::time_point time = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    std::chrono::steady_clock::duration diff = now - time;
    if (diff < std::chrono::seconds(1))
        plane->translate(glm::vec3(-0.01f, 0, 0));
    else if (diff < std::chrono::seconds(2))
        plane->translate(glm::vec3(0, -0.01f, 0));
    else if (diff < std::chrono::seconds(3))
        plane->translate(glm::vec3(0.01f, 0, 0));
    else if (diff < std::chrono::seconds(4))
        plane->translate(glm::vec3(0, 0.01f, 0));
    else
        time = now;
}

int main(void) try
{
    SSS::GL::LOG::internal_callbacks::monitor = true;
    SSS::GL::LOG::internal_callbacks::window_pos = true;
    SSS::GL::Window::LOG::fps = false;
    //g_data->ui_use_separate_window = true;

    SSS::GL::Model::on_click_funcs = {
        { 0, nullptr },
        { 1, button_func_1 },
        { 2, button_func_2 }
    };
    SSS::GL::Model::passive_funcs = {
        { 0, nullptr },
        { 1, passive_func_1 },
        { 2, passive_func_2 }
    };

    // Create window & set callbacks
    g_data->window = createWindow("resources/json/Window.json");
    SSS::GL::Window::Shared& window = g_data->window;
    window->setCallback(glfwSetKeyCallback, key_callback);
    window->setVSYNC(true);
    {
        SSS::GL::Context const context(window);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    // Load TR
    loadTextAreas("resources/json/TextRendering.json");
    // Load objects
    loadWindowObjects(window, "resources/json/WindowObjects.json");
    // Manually set renderers, for now
    organizeRenderers(window, "resources/json/Scene1.json");

    SSS::GL::Window::Args args;
    args.title = "Summergasm - ImGUI";
    args.w = 600;
    args.h = 600;
    args.monitor_id = 1;
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    g_data->ui_window= SSS::GL::Window::create(args);
    g_data->ui_window->setCallback(glfwSetKeyCallback, key_callback);
    g_data->ui_window->setCallback(glfwSetWindowCloseCallback, close_callback);

    ImGuiHandle::init();
    ImGuiHandle::swapContext(!g_data->ui_use_separate_window ? window : g_data->ui_window);

    // Main loop
    while (window) {
        SSS::GL::pollEverything();
        if (window && window->shouldClose()) {
                window.reset();
                continue;
        }
        window->drawObjects();
        if (g_data->ui_display) {
            print_imgui();
        }
        window->printFrame();
        g_data->ui_window->printFrame();
    }

    ImGuiHandle::shutdown();
    g_data.reset();
    SSS::TR::TextArea::clearInstances();
}
__CATCH_AND_LOG_FUNC_EXC