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
    static uint32_t text_id = 0;
    if (action == GLFW_PRESS)
        g_data->text_areas.at(0)->parseString(g_data->texts[++text_id]);
}

int main(void) try
{
    // Create window & set callbacks
    SSS::GL::Window::Shared window = createWindow("resources/json/Window.json");
    window->setCallback(glfwSetKeyCallback, key_callback);
    // Load TR
    loadTextAreas("resources/json/TextRendering.json");
    // Load objects
    loadWindowObjects(window, "resources/json/WindowObjects.json");
    // Manually set renderers, for now
    organizeRenderers(window, "resources/json/Scene1.json");
    // Main loop
    while (window && !window->shouldClose()) {
        SSS::GL::Window::pollTextureThreads();
        window->render();
    }
}
__CATCH_AND_LOG_FUNC_EXC