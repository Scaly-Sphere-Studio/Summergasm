#pragma once

#include <SSS/ImGuiH.hpp>
#include <SSS/GL.hpp>
#include <SSS/Audio.hpp>

#pragma warning(push, 0)
#include <nlohmann/json.hpp>
#pragma warning(pop)


// Static data

struct GlobalData {
    SSS::GL::Window::Shared window;
    SSS::GL::Window::Shared ui_window;
    bool ui_display{ false };
    bool ui_use_separate_window{ false };
    std::vector<std::string> texts;
};
extern std::unique_ptr<GlobalData> g_data;

// Callbacks

void key_callback(GLFWwindow* ptr, int key, int scancode, int action, int mods);
void char_callback(GLFWwindow* window, unsigned int codepoint);
void close_callback(GLFWwindow* ptr);

// Model functions

void button_func_1(SSS::GL::Window::Shared window, SSS::GL::Plane::Shared plane,
    int button, int action, int mods);
void button_func_2(SSS::GL::Window::Shared window, SSS::GL::Plane::Shared plane,
    int button, int action, int mods);

void passive_func_1(SSS::GL::Window::Shared window, SSS::GL::Plane::Shared plane);
void passive_func_2(SSS::GL::Window::Shared window, SSS::GL::Plane::Shared plane);

// JSON operations

SSS::GL::Window::Shared createWindow(std::string const& json_path);

void loadScene(SSS::GL::Window::Shared const& window, std::string const& json_path);

void loadTextAreas(std::string const& json_path);

void print_imgui();
