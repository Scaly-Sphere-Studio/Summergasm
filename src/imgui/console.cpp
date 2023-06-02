#include "imgui.hpp"
#include "mylua.hpp"
#include <regex>

struct CmdMemory {
    CmdMemory(std::string const& s) : str(s) {};
    std::string str;
    ImVec4 color{ 1, 1, 1, 1 };
};

static std::vector<CmdMemory> memory;

static char buffer[4096];
static size_t memory_index = 0;
static std::string buffer_memory;

static std::unique_ptr<LuaConsoleData const> console_data;
static std::string last_key;

static int inputTextCallback(ImGuiInputTextCallbackData* data)
{
    if (data->EventFlag != ImGuiInputTextFlags_CallbackCompletion && console_data) {
        if (!last_key.empty()) {
            buffer_memory = data->Buf;
            last_key.clear();
        }
        console_data.reset();
    }

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

        std::string const current = []() {
            size_t const n = buffer_memory.rfind(' ');
            return n == std::string::npos ?
                buffer_memory : buffer_memory.substr(n + 1);
        }();

        auto const [table_name, key, separator] = [current]() {
            std::regex const r("[.:]");
            std::string key = current;
            std::string table;
            char separator = '.';
            std::smatch sm;
            while (std::regex_search(key, sm, r)) {
                if (!table.empty()) {
                    table += separator;
                }
                separator = sm[0].str().at(0);
                table += sm.prefix().str();
                key = sm.suffix().str();
            }
            return std::make_tuple(table, key, separator);
        }();

        if (!console_data) {
            sol::environment const env = *mylua_console_env;
            sol::table t = env;
            if (!table_name.empty()) {
                auto res = g->lua.safe_script("return " + table_name, env, sol::script_pass_on_error);
                if (!res.valid() || (res.get_type() != sol::type::table &&
                    res.get_type() != sol::type::userdata))
                {
                    break;
                }
                t = res;
            }
            console_data = std::make_unique<LuaConsoleData const>(g->lua, t, env, table_name);
        }

        if (!last_key.empty()) {
            data->DeleteChars(buffer_memory.size(), last_key.size() - key.size());
            auto it = console_data->find(last_key);
            if (it != console_data->cend()) {
                ++it;
                while (it != console_data->cend() && it->second.separator != separator)
                    ++it;
                if (it != console_data->cend()) {
                    std::string const& new_key = it->first;
                    if (new_key.find(key) == 0) {
                        data->InsertChars(buffer_memory.size(), new_key.c_str() + key.size());
                        last_key = new_key;
                        break;
                    }
                }
            }
        }

        for (auto const& [k, v] : *console_data) {
            if (v.separator == separator && k.find(key) == 0 &&
                (!last_key.empty() || k != current))
            {
                data->InsertChars(buffer_memory.size(), k.c_str() + key.size());
                last_key = k;
                break;
            }
        }

    }   break;
    
    // Normal edit
    case ImGuiInputTextFlags_CallbackEdit: {
        memory_index = 0;
        buffer_memory = data->Buf;
    }   break;
    }
    return 0;
}

void print_console()
{
    if (glfwGetKey(g->window->getGLFWwindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        g->console_display = false;
        g->window->unblockInputs();
        return;
    }

    // Make context current
    SSS::GL::Context const context = g->window->setContext();
    SSS::ImGuiH::setContext(g->window->getGLFWwindow());
    if (!SSS::ImGuiH::newFrame()) {
        return;
    }

    // Crop to bottom of Window
    int ui_w, ui_h;
    g->window->getDimensions(ui_w, ui_h);
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

        if (ImGui::BeginChild("##memory", ImVec2(-FLT_MIN, 260), false,
            ImGuiWindowFlags_AlwaysUseWindowPadding))
        {
            for (CmdMemory const& cmd : memory) {
                std::string str("> " + cmd.str);
                ImGui::TextColored(cmd.color, str.c_str());
            }
            static size_t mem_size = 0;
            if (mem_size !=  memory.size()) {
                mem_size = memory.size();
                ImGui::SetScrollHereY();
            }
        }
        ImGui::EndChild();
        
        ImGui::SetNextItemWidth(-1);
        ImGui::SetKeyboardFocusHere();
        constexpr auto flags = 0
            | ImGuiInputTextFlags_CallbackHistory
            | ImGuiInputTextFlags_CallbackCompletion
            | ImGuiInputTextFlags_CallbackEdit
        ;
        // Text input, with internal callback
        ImGui::InputText("##console_text", buffer, 4096, flags, inputTextCallback);
        // Enter after edit
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            // Add command to memory
            memory.emplace_back(buffer);
            auto result = g->lua.safe_script(buffer, *mylua_console_env, sol::script_pass_on_error);
            if (!result.valid()) {
                memory.back().color = ImVec4(1, 0, 0, 1);
                sol::error err = result;
                LOG_CTX_ERR("Lua console", std::string("\n") + err.what());
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