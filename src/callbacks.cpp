#include "Summergasm.hpp"

void key_callback(GLFWwindow* ptr, int key, int scancode, int action, int mods)
{
    SSS::GL::Window::Shared const window = SSS::GL::Window::get(ptr);
    SSS::GL::Window::KeyInputs const& keys = window->getKeyInputs();

    if (action == GLFW_PRESS) {
        switch (key) {
        case GLFW_KEY_ESCAPE:
        case GLFW_KEY_KP_0:
            glfwSetWindowShouldClose(ptr, GLFW_TRUE);
            break;
        case GLFW_KEY_F10:
            window->saveScreenshot();
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
}

void close_callback(GLFWwindow* ptr)
{
    g_data->ui_display = false;
    glfwSetWindowShouldClose(ptr, GLFW_FALSE);
    g_data->ui_window->setVisibility(false);
}