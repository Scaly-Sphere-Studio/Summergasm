#include "includes.hpp"
#include "Json_Operators.hpp"

std::unique_ptr<GlobalData> g_data = std::make_unique<GlobalData>();

void key_callback(GLFWwindow* ptr, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(ptr, GLFW_TRUE);
    }
}

void button_func_1(GLFWwindow* ptr, uint32_t id, int button, int action, int mods)
{
    __LOG_MSG("foo");
}

void button_func_2(GLFWwindow* ptr, uint32_t id, int button, int action, int mods)
{
    __LOG_MSG("bar");
}

int main(void) try
{
    SSS::TR::TextOpt opt(SSS::TR::Font::getShared("arial.ttf"));
    opt.style.charsize = 100;
    g_data->buffers[0] = SSS::TR::Buffer::create("", opt);
    g_data->text_areas[0] = SSS::TR::TextArea::create(300, 120);
    g_data->text_areas[0]->useBuffer(g_data->buffers[0]);

    // Create window & set callbacks
    SSS::GL::Window::Shared window = createWindow("resources/json/Window.json");
    window->setCallback(glfwSetKeyCallback, key_callback);
    // Load objects
    loadWindowObjects(window, "resources/json/WindowObjects.json");
    // Manually set renderers, for now
    organizeRenderers(window, "resources/json/Scene1.json");
    // Main loop
    SSS::FPS_Timer timer;
    while (window && !window->shouldClose()) {
        SSS::GL::Window::pollTextureThreads();
        window->render();
        if (timer.addFrame()) {
            g_data->buffers[0]->changeString(std::to_string(timer.get()));
        }
    }
}
__CATCH_AND_LOG_FUNC_EXC
