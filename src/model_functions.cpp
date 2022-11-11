#include "Summergasm.hpp"

void button_func_1(SSS::GL::Window::Shared window, SSS::GL::Plane::Shared plane,
    int button, int action, int mods)
{
    SSS::Audio::Source::Ptr const& source = SSS::Audio::getSources().at(0);
    if (action == GLFW_PRESS) {
        if (button == GLFW_MOUSE_BUTTON_1) {
            if (source->isPlaying()) {
                source->pause();
            }
            else {
                source->play();
            }
        }
        if (button == GLFW_MOUSE_BUTTON_3) {
            source->stop();
            source->play();
        }
        if (button == GLFW_MOUSE_BUTTON_2) {
            LOG_MSG(source->getPropertyInt(AL_SOURCE_TYPE));
        }
    }
}

void button_func_2(SSS::GL::Window::Shared window, SSS::GL::Plane::Shared plane,
    int button, int action, int mods)
{
    static uint32_t text_id = 1;
    if (action == GLFW_PRESS && text_id < g_data->texts.size()) {
        SSS::TR::Area& area = g_data->lua["area"];
        area.parseString(g_data->lua["string_array"][text_id++]);
        if (text_id >= g_data->texts.size())
            text_id = 0;
    }
}

void passive_func_1(SSS::GL::Window::Shared window, SSS::GL::Plane::Shared plane)
{
    if (!window) return;

    plane->rotate(glm::vec3(0.1f));
}

void passive_func_2(SSS::GL::Window::Shared window, SSS::GL::Plane::Shared plane)
{
    if (!window) return;

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
