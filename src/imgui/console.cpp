#include "imgui.hpp"
#include "mylua.hpp"
#include <regex>

struct CmdMemory {
    CmdMemory(std::string const& s, ImVec4 c = ImVec4(1, 1, 1, 1)) : str(s), color(c) {};
    std::string str;
    ImVec4 color;
};

struct ConsoleMemory {
    std::vector<CmdMemory> cmds;    // Command history
    std::vector<CmdMemory> lines;   // Displayed commands & outputs
    size_t index{ 0 };

    void callback(ImGuiInputTextCallbackData* data, std::string const& buffer);
    void pushCmd(std::string const& buffer);
    void pushOutput(std::string const& str, ImVec4 color = ImVec4(0.7f, 0.7f, 0.7f, 1));
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

void ConsoleMemory::pushOutput(std::string const& str, ImVec4 color)
{
    // Split multi-line outputs so each line is displayed separately
    size_t start = 0;
    while (start <= str.size()) {
        size_t const end = std::min(str.find('\n', start), str.size());
        lines.emplace_back(str.substr(start, end - start), color);
        start = end + 1;
    }
}

void ConsoleMemory::pushCmd(std::string const& buffer)
{
    cmds.emplace_back(buffer);
    lines.emplace_back("> " + buffer);
    size_t const cmd_line = lines.size() - 1;
    index = 0;

    // Aliases, with an optional argument (quotes optional):
    //  "h", "?", "help" [filter]   -> help([filter])
    //  "ls" [scene]                -> list_scenes() / load_scene(scene)
    //  "us" [scene]                -> unload_scene([scene])
    std::string cmd = buffer;
    std::smatch sm;
    static std::regex const alias_regex(R"re(\s*(h|\?|help|ls|us)(?:\s+"?([^\s"]+)"?)?\s*)re");
    if (std::regex_match(buffer, sm, alias_regex)) {
        std::string const alias = sm[1].str();
        std::string const arg = sm[2].matched ? '"' + sm[2].str() + '"' : "";
        if (alias == "ls")
            cmd = arg.empty() ? "list_scenes()" : "load_scene(" + arg + ")";
        else if (alias == "us")
            cmd = "unload_scene(" + arg + ")";
        else
            cmd = "help(" + arg + ")";
    }

    sol::environment const& env = *mylua_console_env;
    sol::table globals = g->lua.globals();
    sol::function const tostring = globals["tostring"];
    auto const to_string = [&](sol::object const& obj) -> std::string {
        return tostring(obj).get<std::string>();
    };

    // Redirect print to the console while the command runs
    sol::object const old_print = globals["print"];
    globals["print"] = [&](sol::variadic_args va) {
        std::string str;
        for (auto const& arg : va) {
            if (!str.empty())
                str += '\t';
            str += to_string(arg.get<sol::object>());
        }
        pushOutput(str);
    };

    auto const push_error = [&](std::string const& what) {
        lines.at(cmd_line).color = ImVec4(1, 0, 0, 1);
        cmds.back().color = ImVec4(1, 0, 0, 1);
        pushOutput(what, ImVec4(1, 0.4f, 0.4f, 1));
        LOG_CTX_ERR("Lua console", std::string("\n") + what);
    };

    // Try as an expression first (like the standard Lua REPL), then as a statement.
    // The load_result must be destroyed BEFORE calling the function: its
    // destructor pops the top of the Lua stack, which would otherwise be the
    // call's results.
    sol::protected_function f;
    std::string load_error;
    {
        sol::load_result expr = g->lua.load("return " + cmd, "console");
        if (expr.valid())
            f = expr;
    }
    if (!f.valid()) {
        sol::load_result stmt = g->lua.load(cmd, "console");
        if (stmt.valid())
            f = stmt;
        else
            load_error = stmt.get<sol::error>().what();
    }
    if (!f.valid()) {
        globals["print"] = old_print;
        push_error(load_error);
        return;
    }
    env.set_on(f);
    sol::protected_function_result result = f();

    globals["print"] = old_print;

    if (!result.valid()) {
        sol::error err = result;
        push_error(err.what());
        return;
    }

    // Print returned values, if any
    std::string str;
    for (int i = 0; i < result.return_count(); ++i) {
        if (!str.empty())
            str += '\t';
        str += to_string(result.get<sol::object>(i));
    }
    if (!str.empty())
        pushOutput(str);
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
            ImGuiChildFlags_AlwaysUseWindowPadding))
        {
            auto const& lines = console.memory.lines;

            for (CmdMemory const& line : lines) {
                ImGui::TextColored(line.color, "%s", line.str.c_str());
            }

            static size_t mem_size = 0;
            if (mem_size != lines.size()) {
                mem_size = lines.size();
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