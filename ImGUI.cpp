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
        char label[512];
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
        // Window objects
        if (ImGui::CollapsingHeader("Window objects")) {
            ImGui::Indent(5.f);
            SSS::GL::Window::Objects const& objects = window->getObjects();
            SSS::GL::Camera::Ptr const& camera = objects.cameras.cbegin()->second;
            //if (!camera)
            //    continue;

            // ID
            ImGui::Text("Projection:");
            // Projection type
            static const char* cam_proj_types[] = { "Ortho", "Perspective" };
            int proj_id = static_cast<int>(camera->getProjectionType());
            if (ImGui::Combo(" Projection type", &proj_id, cam_proj_types, 2)) {
                camera->setProjectionType(static_cast<SSS::GL::Camera::Projection>(proj_id));
            }
            // Projection fov
            float fov = camera->getFOV();
            if (ImGui::InputFloat(" FOV", &fov, 1.0f, 1.0f, "%.0f")) {
                if (fov < 1.f)
                    fov = 1.f;
                if (fov > 179.f)
                    fov = 179.f;
                camera->setFOV(fov);
            }
            // Projection z_near / z_far
            float z_near, z_far;
            camera->getRange(z_near, z_far);
            if (ImGui::InputFloat(" Range (z_near)", &z_near, 0.001f, 0.001f, "%0.6f")
                || ImGui::InputFloat(" Range (z_far)", &z_far, 1.f)) {
                if (z_near < 0.f) {
                    z_near = 0.001f;
                }
                camera->setRange(z_near, z_far);
            }
            ImGui::Spacing();
            // Position
            ImGui::Text("Position:");
            if (ImGui::Button("Reset position")) {
                camera->setPosition(glm::vec3(0));
            }
            glm::vec3 pos = camera->getPosition();
            if (ImGui::InputFloat(" Position (x)", &pos.x, 0.2f)
                || ImGui::InputFloat(" Position (y)", &pos.y, 0.2f)
                || ImGui::InputFloat(" Position (z)", &pos.z, 0.2f)) {
                camera->setPosition(pos);
            }
            ImGui::Spacing();
            // Rotation
            ImGui::Text("Rotation:");
            if (ImGui::Button("Reset rotation")) {
                __LOG_MSG("test")
                camera->setRotation(glm::vec2(0));
            }
            glm::vec2 rot = camera->getRotation();
            if (ImGui::InputFloat(" Rotation (x)", &rot.x, 0.4f)
                || ImGui::InputFloat(" Rotation (y)", &rot.y, 0.4f)) {
                camera->setRotation(rot);
            }
            ImGui::Unindent(5.f);
        }
        ImGui::End();
    }
    if (ImGui::Begin("Demo ui_window", nullptr, flags)) {
        ImGui::ShowDemoWindow();
        ImGui::End();
    }
    // Render dear imgui into screen
    ImGuiHandle::render();
}
