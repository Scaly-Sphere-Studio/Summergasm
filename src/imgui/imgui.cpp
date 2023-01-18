#include "imgui.hpp"

SSS::GL::Window::Shared ui_window = nullptr;
static constexpr bool print_demo = false;

void SetCursor(SSS::GL::Window::Shared window, int shape)
{
    using CursorPtr = SSS::C_Ptr<GLFWcursor, void(*)(GLFWcursor*), glfwDestroyCursor>;
    static std::map<int, CursorPtr> cursors;
    
    if (shape == 0) {
        glfwSetCursor(window->getGLFWwindow(), nullptr);
        return;
    }
    if (cursors.count(shape) == 0) {
        cursors.try_emplace(shape);
        cursors.at(shape).reset(glfwCreateStandardCursor(shape));
    }
    glfwSetCursor(window->getGLFWwindow(), cursors.at(shape).get());
}

bool SmallColoredButton(char const* label, ImVec4 const& col, ImVec4 const& hover, ImVec4 const& active)
{
    ImGui::PushStyleColor(ImGuiCol_Button, col);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, active);
    bool const ret = ImGui::SmallButton(label);
    ImGui::PopStyleColor(3);
    return ret;
}

bool EditButton(char const* label)
{
    ImVec4 const col(0.6f, 0.2f, 0.6f, 1);
    ImVec4 const hover(0.8f, 0.f, 0.8f, 1);
    ImVec4 const active(0.7f, 0.f, 0.7f, 1);
    return SmallColoredButton(label, col, hover, active);
}
bool DeleteButton(char const* label)
{
    ImVec4 const col(0.7f, 0.f, 0.f, 1);
    ImVec4 const hover(0.85f, 0.f, 0.f, 1);
    ImVec4 const active(0.8f, 0.f, 0.f, 1);
    return SmallColoredButton(label, col, hover, active);
}
bool CreateButton(char const* label)
{
    ImVec4 const col(0.f, 0.6f, 0.2f, 1);
    ImVec4 const hover(0.f, 0.8f, 0.3f, 1);
    ImVec4 const active(0.f, 0.7f, 0.25f, 1);
    return SmallColoredButton(label, col, hover, active);
}
bool CopyButton(char const* label)
{
    ImVec4 const col(0.6f, 0.6f, 0.f, 1);
    ImVec4 const hover(.8f, .8f, 0.f, 1);
    ImVec4 const active(0.7f, 0.7f, 0.f, 1);
    return SmallColoredButton(label, col, hover, active);
}

void Tooltip(char const* description)
{
    if (description == nullptr) return;
    
    using clock = std::chrono::steady_clock;
    static clock::time_point t = clock::now();

    if (!ImGui::IsItemHovered()) {
        if (!ImGui::IsAnyItemHovered()) {
            t = clock::now();
        }
        return;
    }
    if (clock::now() - t < std::chrono::milliseconds(350)) {
        return;
    }
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
    ImGui::TextUnformatted(description);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

bool InputFloatWasEdited(const char* label, float* v, float step,
    float step_fast, const char* format, ImGuiInputTextFlags flags)
{
    char name[256];
    ImGui::SetNextItemWidth(160.f);
    sprintf_s(name, "##%s", label);
    ImGui::InputFloat(name, v, 0, step_fast, format, flags);
    bool ret = ImGui::IsItemDeactivatedAfterEdit();
    if (step != 0.f) {
        ImGui::PushButtonRepeat(true);
        ImGui::SameLine();
        sprintf_s(name, "-##%s", label);
        if (ImGui::Button(name) && v != nullptr) {
            *v -= step;
            ret = true;
        }
        sprintf_s(name, "+##%s", label);
        ImGui::SameLine();
        if (ImGui::Button(name) && v != nullptr) {
            *v += step;
            ret = true;
        }
        ImGui::PopButtonRepeat();
    }
    ImGui::SameLine();
    ImGui::Text(label);
    return (ret);
}

bool StringButtonEdit(char const* label, std::string& str)
{
    static std::string* ptr = nullptr;
    static char buff[256];
    static bool set_focus = false;
    if (ptr != &str) {
        if (ImGui::Button(label)) {
            ptr = &str;
            strcpy_s(buff, str.c_str());
            set_focus = true;
        }
    }
    else {
        if (set_focus) {
            ImGui::SetKeyboardFocusHere();
            set_focus = false;
        }
        ImGui::SetNextItemWidth(300.f);
        ImGui::InputText("###", buff, 256);
        if (ImGui::IsItemDeactivated()) {
            ptr = nullptr;
            if (buff[0] != '\0') {
                str = buff;
                return true;
            }
        }
    }
    return false;
}

void print_window_options()
{
    // Fullscreen mode
    bool fullscreen = g_data->window->isFullscreen();
    if (ImGui::Checkbox(" Fullscreen", &fullscreen)) {
        g_data->window->setFullscreen(fullscreen);
    }
    // Maximization mode
    bool maximized = g_data->window->isMaximized();
    if (ImGui::Checkbox(" Maximize", &maximized)) {
        g_data->window->setMaximization(maximized);
    }
    // Visibility mode
    bool show = g_data->window->isVisible();
    if (ImGui::Checkbox(" Visible", &show)) {
        g_data->window->setVisibility(show);
    }
    // Iconification mode
    bool iconify = g_data->window->isIconified();
    if (ImGui::Checkbox(" Iconify", &iconify)) {
        g_data->window->setIconification(iconify);
    }
    // Window title
    char title[256];
    strcpy_s(title, g_data->window->getTitle().c_str());
    ImGui::InputText(" Window title", title, 256);
    if (ImGui::IsItemDeactivated()) {
        g_data->window->setTitle(title);
    }
    // Window dimensions
    int w, h;
    g_data->window->getDimensions(w, h);
    ImGui::InputInt(" Window width", &w, 0);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        g_data->window->setDimensions(w, h);
    }
    ImGui::InputInt(" Window height", &h, 0);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        g_data->window->setDimensions(w, h);
    }
    // Window position
    int x, y;
    g_data->window->getPosition(x, y);
    ImGui::InputInt(" Window X pos", &x, 0);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        g_data->window->setPosition(x, y);
    }
    ImGui::InputInt(" Window Y pos", &y, 0);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        g_data->window->setPosition(x, y);
    }
    // VSYNC
    bool vsync = g_data->window->getVSYNC();
    if (ImGui::Checkbox(" VSYNC", &vsync)) {
        g_data->window->setVSYNC(vsync);
    }
    // FPS Limit
    int fps_limit = g_data->window->getFPSLimit();
    ImGui::InputInt(" FPS limit", &fps_limit);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        g_data->window->setFPSLimit(fps_limit);
    }
}

void print_imgui()
{
    if (!g_data->ui_use_separate_window &&
        glfwGetKey(g_data->window->getGLFWwindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        g_data->ui_display = false;
        g_data->window->unblockInputs();
        return;
    }

    ui_window = g_data->ui_use_separate_window ? g_data->ui_window : g_data->window;
    SSS::ImGuiH::setContext(ui_window->getGLFWwindow());

    // Make context current
    SSS::GL::Context const context(ui_window);
    if (!SSS::ImGuiH::newFrame()) {
        return;
    }
    
    // ImGui g_data->window presets (cropped to GL window)
    int ui_w, ui_h;
    ui_window->getDimensions(ui_w, ui_h);
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(ui_w), static_cast<float>(ui_h)));
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    constexpr ImGuiWindowFlags flags
        = ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove
    ;
    static float bg_alpha = 0.75f;
    ImGui::SetNextWindowBgAlpha(bg_alpha);

    // Render UI
    if (ImGui::Begin("Main UI g_data->window", nullptr, flags)) {
        ImGui::PushItemWidth(300.f);
        // Settings
        if (ImGui::CollapsingHeader("Settings & Object Management")) {
            ImGui::Indent(10.f);
            // UI options
            if (ImGui::CollapsingHeader("UI options")) {
                ImGui::SliderFloat(" Background Opacity", &bg_alpha, 0.f, 1.f);
                if (ImGui::Checkbox(" Display UI on a separate window",
                    &g_data->ui_use_separate_window)) {
                    if (g_data->ui_use_separate_window) {
                        glfwShowWindow(g_data->ui_window->getGLFWwindow());
                        g_data->window->unblockInputs();
                    }
                    else {
                        glfwHideWindow(g_data->ui_window->getGLFWwindow());
                        g_data->window->blockInputs(GLFW_KEY_F1);
                    }
                }
            }
            // Window options
            if (ImGui::CollapsingHeader("Window options")) {
                print_window_options();
            }
            // Window objects
            if (ImGui::CollapsingHeader("Window objects")) {
                print_window_objects();
            }
            // Audio
            if (ImGui::CollapsingHeader("Audio")) {
                print_audio();
            }
            ImGui::Unindent();
        }
        ImGui::End();
    }

    // Render Demo if needed
    if constexpr (print_demo) {
        if (ImGui::Begin("Demo ui_window", nullptr, flags)) {
            ImGui::ShowDemoWindow();
            ImGui::End();
        }
    }

    // Render dear imgui into screen
    SSS::ImGuiH::render();
    ui_window.reset();
}
