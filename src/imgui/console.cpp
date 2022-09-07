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

    // Crop to bottom of Window
    int ui_w, ui_h;
    g_data->window->getDimensions(ui_w, ui_h);
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(ui_w), static_cast<float>(300)));
    ImGui::SetNextWindowPos(ImVec2(0, ui_h - 300));
    constexpr ImGuiWindowFlags flags = 0
        | ImGuiWindowFlags_NoCollapse
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
            std::string str("> " + cmd.str);
            ImGui::TextColored(cmd.color, str.c_str());
        }
        static char buffer[4096];
        static size_t memory_index = 0;
        static std::string buffer_memory;
        
        ImGui::SetNextItemWidth(-1);
        ImGui::SetCursorPos(ImVec2(0, 272));
        ImGui::SetKeyboardFocusHere();
        constexpr auto flags = 0
            | ImGuiInputTextFlags_CallbackHistory
            | ImGuiInputTextFlags_CallbackCompletion
            | ImGuiInputTextFlags_CallbackEdit
        ;
        // Text input, with internal callback
        ImGui::InputText("##console_text", buffer, 4096, flags,
            [](ImGuiInputTextCallbackData* data)->int
            {
                switch (data->EventFlag) {
                // History
                case ImGuiInputTextFlags_CallbackHistory: {
                    if (memory.empty()) break;
                    // Up
                    if (data->EventKey == ImGuiKey_UpArrow && memory_index < memory.size())
                        ++memory_index;
                    // Down
                    if (data->EventKey == ImGuiKey_DownArrow) {
                        if (memory_index != 0) --memory_index;
                        if (memory_index == 0) {
                            data->DeleteChars(0, data->BufTextLen);
                            data->InsertChars(0, buffer_memory.c_str());
                            break;
                        }
                    }
                    // ... Print
                    data->DeleteChars(0, data->BufTextLen);
                    if (memory_index != 0) {
                        std::string const& str = memory.at(memory.size() - memory_index).str;
                        data->InsertChars(0, str.c_str());
                    }
                }   break;
                // Completion
                case ImGuiInputTextFlags_CallbackCompletion: {
                    // TODO
                }   break;
                // Normal edit
                case ImGuiInputTextFlags_CallbackEdit: {
                    memory_index = 0;
                    buffer_memory = data->Buf;
                }   break;
                }
                return 0;
            }
        );
        // Enter after edit
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            // Add command to memory
            memory.emplace_back(buffer);
            auto result = g_data->lua.safe_script(buffer, sol::script_pass_on_error);
            if (!result.valid()) {
                memory.back().color = ImVec4(1, 0, 0, 1);
                sol::error err = result;
                LOG_CTX_ERR("Lua console", err.what());
            }
            buffer[0] = 0;
            memory_index = 0;
            buffer_memory.clear();
        }
        ImGui::End();
    }

    // Render dear imgui into screen
    SSS::ImGuiH::render();
}