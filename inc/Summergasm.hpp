#pragma once

#include <SSS/ImGuiH.hpp>
#include <SSS/GL.hpp>

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

// JSON operations

SSS::GL::Window::Shared createWindow(std::string const& json_path);

void loadWindowObjects(SSS::GL::Window::Shared const& window,
    std::string const& json_path);

void organizeRenderers(SSS::GL::Window::Shared const& window,
    std::string const& json_path);

void loadTextAreas(std::string const& json_path);

void print_imgui();
