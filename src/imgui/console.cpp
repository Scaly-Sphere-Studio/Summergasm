#include "imgui.hpp"
#include "mylua.hpp"
#include <regex>

struct CmdMemory {
    CmdMemory(std::string const& s) : str(s) {};
    std::string str;
    ImVec4 color{ 1, 1, 1, 1 };
};

struct ConsoleMemory {
    std::vector<CmdMemory> cmds;
    size_t index{ 0 };

    void callback(ImGuiInputTextCallbackData* data, std::string const& buffer);
    void pushCmd(std::string const& buffer);
};

struct ConsoleAutocomplete {
    std::unique_ptr<LuaConsoleData const> all_keys;
    size_t cursor{ 0 };
    bool reset_cursor{ true };
    std::string last_key;

    void callback(ImGuiInputTextCallbackData* data, std::string const& buffer);
};

struct Console {
    ConsoleMemory memory;
    ConsoleAutocomplete complete;
    std::string buffer;
    int callback(ImGuiInputTextCallbackData* data);
    static int static_callback(ImGuiInputTextCallbackData* data);
};


void ConsoleMemory::callback(ImGuiInputTextCallbackData* data, std::string const& buffer)
{
    if (cmds.empty()) return;
    // Up
    if (data->EventKey == ImGuiKey_UpArrow && index < cmds.size())
        ++index;
    // Down
    if (data->EventKey == ImGuiKey_DownArrow) {
        if (index != 0)
            --index;
        if (index == 0) {
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, buffer.c_str());
            return;
        }
    }
    // ... Print
    data->DeleteChars(0, data->BufTextLen);
    if (index != 0) {
        std::string const& str = cmds.at(cmds.size() - index).str;
        data->InsertChars(0, str.c_str());
    }
}

void ConsoleMemory::pushCmd(std::string const& buffer)
{
    cmds.emplace_back(buffer);
    auto result = g->lua.safe_script(buffer, *mylua_console_env, sol::script_pass_on_error);
    if (!result.valid()) {
        cmds.back().color = ImVec4(1, 0, 0, 1);
        sol::error err = result;
        LOG_CTX_ERR("Lua console", std::string("\n") + err.what());
    }
    index = 0;
}


void ConsoleAutocomplete::callback(ImGuiInputTextCallbackData* data, std::string const& buffer)
{
    std::string const buf = buffer.substr(0, cursor);

    auto const [table, key, separator] = [buf]() {
        std::regex const r("[.:]");

        std::string key = [](std::string buf) {
            std::regex const r("[ (]");
            std::smatch sm;
            while (std::regex_search(buf, sm, r)) {
                buf = sm.suffix();
            }
            return buf;
        }(buf);

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

    if (!all_keys) {
        sol::environment const env = *mylua_console_env;
        sol::table t = env;
        if (!table.empty()) {
            auto res = g->lua.safe_script("return " + table, env, sol::script_pass_on_error);
            if (!res.valid() || (res.get_type() != sol::type::table &&
                res.get_type() != sol::type::userdata))
                return;
            t = res;
        }
        all_keys = std::make_unique<LuaConsoleData const>(g->lua, t, env, table);
    }

    if (!last_key.empty()) {
        data->DeleteChars(buf.size(), last_key.size() - key.size());
        auto it = all_keys->find(last_key);
        if (it != all_keys->cend()) {
            ++it;
            while (it != all_keys->cend() && it->second.separator != separator)
                ++it;
            if (it != all_keys->cend()) {
                std::string const& new_key = it->first;
                if (new_key.find(key) == 0) {
                    data->InsertChars(buf.size(), new_key.c_str() + key.size());
                    last_key = new_key;
                    return;
                }
            }
        }
    }

    for (auto const& [k, v] : *all_keys) {
        if (v.separator == separator && k.find(key) == 0 && (!last_key.empty() || k != key)) {
            data->InsertChars(buf.size(), k.c_str() + key.size());
            last_key = k;
            return;
        }
    }
}

int Console::callback(ImGuiInputTextCallbackData* data)
{
    if (data->EventFlag != ImGuiInputTextFlags_CallbackCompletion || complete.reset_cursor) {
        if (complete.all_keys) {
            if (!complete.last_key.empty()) {
                buffer = data->Buf;
                complete.last_key.clear();
            }
            complete.all_keys.reset();
        }
        complete.cursor = data->CursorPos;
        complete.reset_cursor = false;
    }

    switch (data->EventFlag) {
    // History
    case ImGuiInputTextFlags_CallbackHistory:
        memory.callback(data, buffer);
        break;
    // Completion
    case ImGuiInputTextFlags_CallbackCompletion:
        complete.callback(data, buffer);
        break;
    // Normal edit
    case ImGuiInputTextFlags_CallbackEdit:
        memory.index = 0;
        buffer = data->Buf;
        break;
    }
    return 0;
}

int Console::static_callback(ImGuiInputTextCallbackData* data)
{
    return reinterpret_cast<Console*>(data->UserData)->callback(data);
}

void print_console()
{
    if (glfwGetKey(g->window->getGLFWwindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        g->console_display = false;
        g->window->unblockInputs();
        return;
    }

    static Console console;

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
            auto const& cmds = console.memory.cmds;

            for (CmdMemory const& cmd : cmds) {
                std::string str("> " + cmd.str);
                ImGui::TextColored(cmd.color, str.c_str());
            }
            
            static size_t mem_size = 0;
            if (mem_size != cmds.size()) {
                mem_size = cmds.size();
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

        if (glfwGetKey(g->window->getGLFWwindow(), GLFW_KEY_LEFT) ||
            glfwGetKey(g->window->getGLFWwindow(), GLFW_KEY_RIGHT)) {
            console.complete.reset_cursor = true;
        }

        static char buffer[4096];
        // Text input, with internal callback
        ImGui::InputText("##console_text", buffer, 4096, flags, Console::static_callback, &console);
        // Enter after edit
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            console.memory.pushCmd(buffer);
            buffer[0] = 0;
            console.buffer.clear();
        }
        ImGui::End();
    }

    // Render dear imgui into screen
    SSS::ImGuiH::render();
}