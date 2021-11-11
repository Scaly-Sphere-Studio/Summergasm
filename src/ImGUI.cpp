#include "Summergasm.hpp"

constexpr bool print_demo = false;

void print_window_options(SSS::GL::Window::Shared window)
{
    // Window title
    char label[512];
    sprintf_s(label, " Window title (%s)", window->getTitle().c_str());
    static char title[256];
    ImGui::InputText(label, title, 256);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        window->setTitle(title);
    }
    // Window dimensions
    static int dim[2]{ 1,1 };
    int w, h;
    window->getDimensions(w, h);
    sprintf_s(label, " Dimensions (%d/%d)", w, h);
    ImGui::InputInt2(label, dim);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        window->setDimensions(dim[0], dim[1]);
    }
    // Window position
    static int pos[2];
    int x, y;
    window->getPosition(x, y);
    sprintf_s(label, " Position (%d/%d)", x, y);
    ImGui::InputInt2(label, pos);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        window->setPosition(pos[0], pos[1]);
    }
    // Fullscreen mode
    bool fullscreen = window->isFullscreen();
    if (ImGui::Checkbox(" Fullscreen", &fullscreen)) {
        window->setFullscreen(fullscreen);
    }
}

// Default, deleted
template<class _Object>
void print_window_object(std::unique_ptr<_Object> const& ptr) = delete;
// Camera
template<>
void print_window_object(SSS::GL::Camera::Ptr const& camera)
{
    if (!camera) {
        return;
    }

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
}
// Texture
template<>
void print_window_object(SSS::GL::Texture::Ptr const& ptr)
{
    // TODO: write logic here
}

// Default, deleted
template<typename _Object>
void create_window_object(SSS::GL::Window::Shared window, uint32_t id) = delete;
// Camera
template<>
void create_window_object<SSS::GL::Camera>(SSS::GL::Window::Shared window, uint32_t id)
{
    window->createCamera(id);
}
// Texture
template<>
void create_window_object<SSS::GL::Texture>(SSS::GL::Window::Shared window, uint32_t id)
{
    window->createTexture(id);
}

// Default, deleted
template<typename _Object>
void remove_window_object(SSS::GL::Window::Shared window, uint32_t id) = delete;
// Camera
template<>
void remove_window_object<SSS::GL::Camera>(SSS::GL::Window::Shared window, uint32_t id)
{
    window->removeCamera(id);
}
// Texture
template<>
void remove_window_object<SSS::GL::Texture>(SSS::GL::Window::Shared window, uint32_t id)
{
    window->removeTexture(id);
}

template<class _Object>
void print_window_objects(SSS::GL::Window::Shared window,
    std::map<uint32_t, std::unique_ptr<_Object>> const& map)
{
    // Tabs, used to display a single camera UI, along
    // with creation & deletion of cameras
    if (ImGui::BeginTabBar("###"))
    {
        // Retrieve all IDs
        std::vector<uint32_t> ids;
        ids.reserve(map.size());
        for (auto it = map.cbegin(); it != map.cend(); it++) {
            ids.push_back(it->first);
        }
        // Display all IDs as tabs
        for (size_t n = 0; n < ids.size(); n++)
        {
            // Option to close tab
            bool open = true;
            char label[32];
            sprintf_s(label, "%04u", ids[n]);
            // Display tab
            if (ImGui::BeginTabItem(label, &open)) {
                // Tab is active, display object UI
                print_window_object(map.at(ids[n]));
                ImGui::EndTabItem();
            }
            if (!open) {
                // Tab was closed, delete object
                remove_window_object<_Object>(window, ids[n]);
            }
        }

        // Bool to set focus on next popup
        static bool set_focus = false;
        // + Button to create new object
        if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing)) {
            ImGui::OpenPopup("###");
            set_focus = true;
        }
        // Popup to prompt for new ID
        if (ImGui::BeginPopup("###")) {
            int id = ids.empty() ? 0 : ids.back() + 1;
            if (set_focus) {
                ImGui::SetKeyboardFocusHere(0);
                set_focus = false;
            }
            ImGui::SetNextItemWidth(100.f);
            ImGui::InputInt("ID", &id, 0);
            // If an ID was specified, create object
            if (ImGui::IsItemDeactivated()) {
                uint32_t uid = static_cast<uint32_t>(id);
                // Only create if available ID
                if (map.count(uid) == 0)
                    create_window_object<_Object>(window, uid);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::EndTabBar();
    }
}

void print_all_window_objects(SSS::GL::Window::Shared window)
{
    SSS::GL::Window::Objects const& objects = window->getObjects();
    
    // Cameras
    if (ImGui::TreeNode("Cameras")) {
        print_window_objects(window, objects.cameras);
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Textures")) {
        print_window_objects(window, objects.textures);
        ImGui::TreePop();
    }
}

void print_imgui()
{
    SSS::GL::Window::Shared const& window = g_data->window;
    SSS::GL::Window::Shared const& ui_window
        = g_data->ui_use_separate_window ? g_data->ui_window : window;
    // Bool to swap ImGui context if needed
    static bool swap_windows = false;
    if (swap_windows) {
        ImGuiHandle::swapContext(ui_window);
        swap_windows = false;
    }
    // Make context current
    SSS::GL::Context const context(ui_window);
    ImGuiHandle::newFrame();
    
    // ImGui window presets (cropped to GL window)
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
            print_window_options(window);
        }
        // Window objects
        if (ImGui::CollapsingHeader("Window objects")) {
            print_all_window_objects(window);
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
    ImGuiHandle::render();
}
