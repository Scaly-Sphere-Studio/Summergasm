#include "Summergasm.hpp"

void key_callback(GLFWwindow* ptr, int key, int scancode, int action, int mods)
{
    SSS::GL::Window* window = SSS::GL::Window::get(ptr);
    auto const& keys = window->getKeyInputs();

    if (action == GLFW_PRESS) {
        switch (key) {
        case GLFW_KEY_ESCAPE:
            // Main window: handled by the main loop (back to the menu, or quit)
            if (window == g->window) {
                g->escape_pressed = true;
                break;
            }
            [[fallthrough]];
        case GLFW_KEY_KP_0:
            glfwSetWindowShouldClose(ptr, GLFW_TRUE);
            break;
        case GLFW_KEY_F10:
            window->saveScreenshot();
            break;
        case GLFW_KEY_F11:
            window->setFullscreen(!window->isFullscreen());
            break;
        case GLFW_KEY_F1: {
            g->ui_display = !g->ui_display;
            if (g->ui_use_separate_window) {
                g->ui_window->setVisibility(g->ui_display);
            }
            else if (g->ui_display) {
                g->window->blockInputs(key);
            }
        }   break;
        case GLFW_KEY_F2: {
            g->console_display = !g->console_display;
            if (g->console_display) {
                window->blockInputs(key);
            }
        }   break;
        }
    }
}

// Accumulates the mouse wheel for the scenes, and forwards it to the
// previous callback (ImGui's, if installed before)
static GLFWscrollfun previous_scroll_callback{ nullptr };

static void scroll_callback(GLFWwindow* ptr, double x, double y)
{
    if (SSS::GL::Window::get(ptr) == g->window)
        g->scroll_y += y;
    if (previous_scroll_callback)
        previous_scroll_callback(ptr, x, y);
}

void set_scroll_callback(GLFWwindow* ptr)
{
    // Window::setCallback() doesn't handle scrolling: set it directly
    previous_scroll_callback = glfwSetScrollCallback(ptr, scroll_callback);
}

void close_callback(GLFWwindow* ptr)
{
    g->ui_display = false;
    glfwSetWindowShouldClose(ptr, GLFW_FALSE);
    g->ui_window->setVisibility(false);
}