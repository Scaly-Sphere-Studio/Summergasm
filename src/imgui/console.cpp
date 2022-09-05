#include "imgui.hpp"

void print_console() {
    if (glfwGetKey(g_data->window->getGLFWwindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        g_data->console_display = false;
        g_data->window->unblockInputs();
        return;
    }

    // Make context current
    SSS::GL::Context const context(g_data->window);
    SSS::ImGuiH::setContext(g_data->window->getGLFWwindow());
    if (!SSS::ImGuiH::newFrame()) {
        return;
    }

    // ImGui g_data->window presets (cropped to GL window)
    int ui_w, ui_h;
    g_data->window->getDimensions(ui_w, ui_h);
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(ui_w), static_cast<float>(300)));
    ImGui::SetNextWindowPos(ImVec2(0, ui_h - 300));
    constexpr ImGuiWindowFlags flags = 0
        | ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove
    ;
    ImGui::SetNextWindowBgAlpha(0.80);

    // Render UI
    if (ImGui::Begin("Console", nullptr, flags)) {
        struct CmdMemory {
            CmdMemory(std::string const& s) : str(s) {};
            std::string str;
            ImVec4 color{ 1, 1, 1, 1 };
        };
        static std::vector<CmdMemory> memory;
        for (CmdMemory const& cmd : memory) {
            ImGui::TextColored(cmd.color, cmd.str.c_str());
        }
        static char buffer[4096];
        ImGui::SetNextItemWidth(-1);
        ImGui::SetCursorPos(ImVec2(0, 272));
        ImGui::SetKeyboardFocusHere();
        ImGui::InputText("##console_text", buffer, 4096);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            memory.emplace_back(buffer);
            try {
                g_data->lua.script(buffer);
            }
            catch (...) {
                memory.back().color = ImVec4(1, 0, 0, 1);
            }
            buffer[0] = 0;
        }
        ImGui::End();
    }

    // Render dear imgui into screen
    SSS::ImGuiH::render();
}