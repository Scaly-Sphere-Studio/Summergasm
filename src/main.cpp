#include "Summergasm.hpp"

std::unique_ptr<GlobalData> g_data = std::make_unique<GlobalData>();

void key_callback(GLFWwindow* ptr, int key, int scancode, int action, int mods)
{
    SSS::GL::Window::Shared const window = SSS::GL::Window::get(ptr);
    SSS::GL::Window::KeyInputs const& keys = window->getKeyInputs();

    if (action == GLFW_PRESS) {
        switch (key) {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(ptr, GLFW_TRUE);
            break;
        case GLFW_KEY_F11:
            window->setFullscreen(!window->isFullscreen());
            break;
        case GLFW_KEY_F1:
            g_data->ui_display = !g_data->ui_display;
            {
                if (g_data->ui_use_separate_window) {
                    g_data->ui_window->setVisibility(g_data->ui_display);
                }
            }
            break;
        }
    }

    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        bool const ctrl = keys[GLFW_KEY_LEFT_CONTROL] || keys[GLFW_KEY_RIGHT_CONTROL];
        switch (key) {
        case GLFW_KEY_LEFT:
            SSS::TR::Area::getMap().at(0)->cursorMove(
                ctrl ? SSS::TR::Move::CtrlLeft : SSS::TR::Move::Left);
            break;
        case GLFW_KEY_RIGHT:
            SSS::TR::Area::getMap().at(0)->cursorMove(
                ctrl ? SSS::TR::Move::CtrlRight : SSS::TR::Move::Right);
            break;
        case GLFW_KEY_DOWN:
            SSS::TR::Area::getMap().at(0)->cursorMove(SSS::TR::Move::Down);
            break;
        case GLFW_KEY_UP:
            SSS::TR::Area::getMap().at(0)->cursorMove(SSS::TR::Move::Up);
            break;
        case GLFW_KEY_HOME:
            SSS::TR::Area::getMap().at(0)->cursorMove(SSS::TR::Move::Start);
            break;
        case GLFW_KEY_END:
            SSS::TR::Area::getMap().at(0)->cursorMove(SSS::TR::Move::End);
            break;
        case GLFW_KEY_BACKSPACE:
            SSS::TR::Area::getMap().at(0)->cursorDeleteText(
                ctrl ? SSS::TR::Delete::CtrlLeft : SSS::TR::Delete::Left);
            break;
        case GLFW_KEY_DELETE:
            SSS::TR::Area::getMap().at(0)->cursorDeleteText(
                ctrl ? SSS::TR::Delete::CtrlRight : SSS::TR::Delete::Right);
            break;
        }
    }
}

void char_callback(GLFWwindow* window, unsigned int codepoint)
{
    std::u32string str;
    str.append(1, static_cast<char32_t>(codepoint));
    SSS::TR::Area::getMap().at(0)->cursorAddText(str);
}

void close_callback(GLFWwindow* ptr)
{
    g_data->ui_display = false;
    glfwSetWindowShouldClose(ptr, GLFW_FALSE);
    g_data->ui_window->setVisibility(false);
}

void button_func_1(GLFWwindow* ptr, uint32_t id, int button, int action, int mods)
{
    LOG_MSG("foo");
}

void button_func_2(GLFWwindow* ptr, uint32_t id, int button, int action, int mods)
{
    static uint32_t text_id = 1;
    if (action == GLFW_PRESS && text_id < g_data->texts.size()) {
        SSS::TR::Area::getMap().at(0)->parseString(g_data->texts[text_id++]);
        if (text_id >= g_data->texts.size())
            text_id = 0;
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
    SSS::Log::GL::Shaders::louden(true);
    SSS::GL::Window::LOG::fps = true;

    //g_data->ui_use_separate_window = true;

    SSS::GL::Model::on_click_funcs = {
        { 0, nullptr },
        { 1, button_func_1 },
        { 2, button_func_2 }
    };
    SSS::GL::Model::passive_funcs = {
        { 0, nullptr },
        { 1, passive_func_1 },
        { 2, /*passive_func_2*/ nullptr }
    };

    // Load TR
    SSS::TR::init();
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
            window.reset();
            continue;
        }
        window->drawObjects();
        if (g_data->ui_display) {
            print_imgui();
        }
        window->printFrame();
        if (g_data->ui_window)
            g_data->ui_window->printFrame();
    }

    SSS::ImGuiH::shutdown();
    g_data.reset();
    SSS::TR::terminate();
}
CATCH_AND_LOG_FUNC_EXC