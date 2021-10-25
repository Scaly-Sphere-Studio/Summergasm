#include "Summergasm.hpp"

void print_imgui()
{
    static bool swap_windows = false;
    SSS::GL::Window::Shared const& window = g_data->window;
    SSS::GL::Window::Shared const& ui_window
        = g_data->ui_use_separate_window ? g_data->ui_window : window;
    if (swap_windows) {
        ImGuiHandle::swapContext(ui_window);
        swap_windows = false;
    }
    SSS::GL::Context const context(ui_window);

    ImGuiHandle::newFrame();
    
    // render your GUI
    int ui_w, ui_h;
    ui_window->getDimensions(ui_w, ui_h);
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(ui_w), static_cast<float>(ui_h)));
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    constexpr ImGuiWindowFlags flags
        = ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove
    ;
    static float bg_alpha = 0.65f;
    ImGui::SetNextWindowBgAlpha(bg_alpha);

    // Render UI
    if (ImGui::Begin("Main UI window", nullptr, flags)) {
        ImGui::PushItemWidth(300.f);
        // UI options
        if (ImGui::CollapsingHeader("UI options")) {
            ImGui::SliderFloat(" Background Opacity", &bg_alpha, 0.f, 1.f);
            if (ImGui::Checkbox(" Display UI on a separate window", &g_data->ui_use_separate_window)) {
                swap_windows = true;
                if (g_data->ui_use_separate_window)
                    glfwShowWindow(g_data->ui_window->getGLFWwindow());
                else
                    glfwHideWindow(g_data->ui_window->getGLFWwindow());
            }
        }
        // Window options
        if (ImGui::CollapsingHeader("Window options")) {
            char label[512];
            sprintf_s(label, " Window title (%s)", window->getTitle().c_str());
            static char title[256];
            ImGui::InputText(label, title, 256);
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                window->setTitle(title);
            }
            static int dim[2]{ 1,1 };
            int w, h;
            window->getDimensions(w, h);
            sprintf_s(label, " Dimensions (%d/%d)", w, h);
            ImGui::InputInt2(label, dim);
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                window->setDimensions(dim[0], dim[1]);
            }
            static int pos[2];
            int x, y;
            window->getPosition(x, y);
            sprintf_s(label, " Position (%d/%d)", x, y);
            ImGui::InputInt2(label, pos);
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                window->setPosition(pos[0], pos[1]);
            }
            bool fullscreen = window->isFullscreen();
            if (ImGui::Checkbox(" Fullscreen", &fullscreen)) {
                window->setFullscreen(fullscreen);
            }
        }
        ImGui::End();
    }
    //if (ImGui::Begin("Demo ui_window", nullptr, flags)) {
    //    ImGui::ShowDemoWindow();
    //    ImGui::End();
    //}
    // Render dear imgui into screen
    ImGuiHandle::render();
}
