#pragma once

#include <imgui.h>
#include "../imgui/imgui_impl_glfw.h"
#include "../imgui/imgui_impl_opengl3.h"
#include "../imgui/imfilebrowser.h"

#include <SSS/GL.hpp>

#pragma warning(push, 0)
#include <nlohmann/json.hpp>
#pragma warning(pop)


// Static data

void button_func_1(GLFWwindow* ptr, uint32_t id, int button, int action, int mods);
void button_func_2(GLFWwindow* ptr, uint32_t id, int button, int action, int mods);

struct GlobalData {
    SSS::GL::Window::Shared window;
    SSS::GL::Window::Shared ui_window;
    bool ui_display{ false };
    bool ui_use_separate_window{ false };
    std::map<uint32_t, SSS::TR::TextArea::Shared> text_areas;
    std::vector<std::string> texts;
    static constexpr std::array<SSS::GL::Plane::ButtonFunction, 3> button_functions = {
        nullptr,
        button_func_1,
        button_func_2
    };
};
extern std::unique_ptr<GlobalData> g_data;

// JSON operations

SSS::GL::Window::Shared createWindow(std::string const& json_path);

void loadWindowObjects(SSS::GL::Window::Shared const& window,
    std::string const& json_path);

void organizeRenderers(SSS::GL::Window::Shared const& window,
    std::string const& json_path);

void loadTextAreas(std::string const& json_path);

// ImGUI

namespace ImGuiHandle {
    inline void init() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::GetIO().IniFilename = nullptr;
    }
    inline void swapContext(SSS::GL::Window::Shared const& window) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui_ImplGlfw_InitForOpenGL(window->getGLFWwindow(), true);
        ImGui_ImplOpenGL3_Init("#version 330");
    }
    inline void newFrame() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }
    inline void render() {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
    inline void shutdown() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
};

void print_imgui();
