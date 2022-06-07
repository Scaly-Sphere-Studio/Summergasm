#include "Summergasm.hpp"

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
        bool const ctrl = mods & GLFW_MOD_CONTROL;
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