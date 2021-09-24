#include "includes.hpp"
#include "Json_Operators.hpp"

void key_callback(GLFWwindow* ptr, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(ptr, GLFW_TRUE);
    }
}

int main(void) try
{
    // Create window & set callbacks
    SSS::GL::Window::Shared window = createWindow("resources/json/Window.json");
    window->setCallback(glfwSetKeyCallback, key_callback);
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
