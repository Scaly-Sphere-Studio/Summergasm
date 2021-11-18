#include "Summergasm.hpp"

constexpr static bool print_demo = false;
static SSS::GL::Window::Shared ui_window;

bool InputFloatWasEdited(const char* label, float* v, float step = 0.0f,
    float step_fast = 0.0f, const char* format = "%.3f", ImGuiInputTextFlags flags = 0)
{
    char name[128];
    ImGui::SetNextItemWidth(250.f);
    sprintf_s(name, "##%s", label);
    ImGui::InputFloat(name, v, 0, step_fast, format, flags);
    bool ret = ImGui::IsItemDeactivatedAfterEdit();
    if (step != 0.f) {
        ImGui::SameLine();
        sprintf_s(name, "+##%s", label);
        if ((ImGui::Button(name) || ImGui::IsItemActive()) && v != nullptr) {
            *v += step;
            ret = true;
        }
        sprintf_s(name, "-##%s", label);
        ImGui::SameLine();
        if ((ImGui::Button(name) || ImGui::IsItemActive()) && v != nullptr) {
            *v -= step;
            ret = true;
        }
    }
    ImGui::SameLine();
    ImGui::Text(label);
    return (ret);
}

void print_window_options()
{
    // Fullscreen mode
    bool fullscreen = g_data->window->isFullscreen();
    if (ImGui::Checkbox(" Fullscreen", &fullscreen)) {
        g_data->window->setFullscreen(fullscreen);
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
void print_window_object(SSS::GL::Texture::Ptr const& texture)
{
    if (!texture) {
        return;
    }

    // Display combo to choose Texture Type
    static const char* tex_types[] = { "Raw", "Text" };
    SSS::GL::Texture::Type type = texture->getType();
    int type_id = static_cast<int>(type);
    if (ImGui::Combo(" Texture type", &type_id, tex_types, 2)) {
        type = static_cast<SSS::GL::Texture::Type>(type_id);
        texture->setType(type);
    }
    // Depending on Texture Type, display texture options
    if (type == SSS::GL::Texture::Type::Raw) {
        // Init filebrowser
        static ImGui::FileBrowser filebrowser = []() {
            ImGui::FileBrowser filebrowser;
            // Remove last '\' from PWD because for some reason
            // the file browser detects it as an empty directory
            std::string pwd = SSS::PWD;
            pwd.resize(pwd.size() - 1);
            filebrowser.SetPwd(pwd);
            filebrowser.SetTitle("Select an image");
            filebrowser.SetTypeFilters({ ".png", ".bmp", ".jpg", ".jpeg" });
            return filebrowser;
        }();
        // Button to display filebrowser
        if (ImGui::Button("New filepath")) {
            filebrowser.Open();
        }
        // Display if needed
        filebrowser.Display();
        // If a file has been selected, update texture
        if (filebrowser.HasSelected()) {
            texture->useFile(filebrowser.GetSelected().string());
            filebrowser.ClearSelected();
        }
    }
    else if (type == SSS::GL::Texture::Type::Text) {
        // TODO: integrate text area in GL
        static uint32_t current_id = 0;
        // Display combo to select TextArea ID
        if (ImGui::BeginCombo(" TextArea ID", std::to_string(current_id).c_str())) {
            // Loop over map to display each ID
            for (auto it = g_data->text_areas.cbegin(); it != g_data->text_areas.cend(); ++it) {
                uint32_t const id = it->first;
                bool is_selected = (current_id == id);
                // Display selectable ID
                if (ImGui::Selectable(std::to_string(id).c_str(), is_selected)) {
                    current_id = id;
                    texture->setTextArea(it->second);
                }
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }
    if (ui_window == g_data->window) {
        int w, h;
        texture->getDimensions(w, h);
        if (w != 0 && h != 0) {
            float const ratio = static_cast<float>(w) / 300.f;
            ImVec2 dim(static_cast<float>(w) / ratio, static_cast<float>(h) / ratio);
            // C4312
    #ifdef _WIN64
            uint64_t const id = texture->getTexID();
    #else
            uint32_t const id = texture->getTexID();
    #endif // _WIN64
            ImGui::Image(reinterpret_cast<void*>(id), dim);
        }
    }
    else {
        ImGui::Text("No preview when using a separate window");
    }
}
// Plane
template<>
void print_window_object(SSS::GL::Plane::Ptr const& plane)
{
    if (!plane) {
        return;
    }
    SSS::GL::Window::Objects const& objects = g_data->window->getObjects();

    // Display combo to select Texture ID
    uint32_t const current_texture_id = plane->getTextureID();
    if (ImGui::BeginCombo(" Texture ID", std::to_string(current_texture_id).c_str())) {
        // Loop over map to display each ID
        for (auto it = objects.textures.cbegin(); it != objects.textures.cend(); ++it) {
            uint32_t const id = it->first;
            bool is_selected = (current_texture_id == id);
            // Display selectable ID
            if (ImGui::Selectable(std::to_string(id).c_str(), is_selected)) {
                plane->setTextureID(id);
            }
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    // Display combo to select Hitbox Type
    int current_hitbox = static_cast<int>(plane->getHitbox());
    const char* hitboxes[] = { "None", "Alpha", "Full" };
    if (ImGui::Combo(" Hitbox Type", &current_hitbox, hitboxes, 3)) {
        plane->setHitbox(static_cast<SSS::GL::Plane::Hitbox>(current_hitbox));
    }
    // Display combo to select Button Function ID
    static uint32_t current_function_id = 0;
    if (ImGui::BeginCombo(" Function ID", std::to_string(current_function_id).c_str())) {
        // Loop over map to display each ID
        for (size_t i = 0; i < g_data->button_functions.size(); ++i) {
            uint32_t const id = static_cast<uint32_t>(i);
            bool is_selected = (current_function_id == id);
            // Display selectable ID
            if (ImGui::Selectable(std::to_string(id).c_str(), is_selected)) {
                current_function_id = id;
                plane->setFunction(g_data->button_functions[i]);
            }
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    glm::vec3 scaling, angles, translation;
    plane->getAllTransformations(scaling, angles, translation);

    // Scaling
    ImGui::Spacing();
    ImGui::Text("Plane Scaling:");
    if (ImGui::Button("Reset Plane Scaling"))
        plane->setScaling();
    float scaling_global = 1.f;
    float step = (scaling.x > scaling.y ? scaling.x : scaling.y) / 100.f;
    step = step > 0.01f ? 0.01f : step;
    if (InputFloatWasEdited(" Plane scaling (global)", &scaling_global, step))
        plane->setScaling(scaling * scaling_global);
    if (InputFloatWasEdited(" Plane scaling (x)", &scaling.x, 0.01f))
        plane->setScaling(scaling);
    if (InputFloatWasEdited(" Plane scaling (y)", &scaling.y, 0.01f))
        plane->setScaling(scaling);
    // Rotation
    ImGui::Spacing();
    ImGui::Text("Plane Rotation:");
    glm::vec3 old_angles(angles);
    if (ImGui::Button("Reset Plane Rotation"))
        plane->setRotation();
    if (InputFloatWasEdited(" Plane rotation (x)", &angles.x, 1.f))
        plane->rotate(angles - old_angles);
    if (InputFloatWasEdited(" Plane rotation (y)", &angles.y, 1.f))
        plane->rotate(angles - old_angles);
    if (InputFloatWasEdited(" Plane rotation (z)", &angles.z, 1.f))
        plane->rotate(angles - old_angles);
    // Translation
    ImGui::Spacing();
    ImGui::Text("Plane Translation:");
    if (ImGui::Button("Reset Plane Translation"))
        plane->setTranslation();
    if (InputFloatWasEdited(" Plane translation (x)", &translation.x, 0.01f))
        plane->setTranslation(translation);
    if (InputFloatWasEdited(" Plane translation (y)", &translation.y, 0.01f))
        plane->setTranslation(translation);
    if (InputFloatWasEdited(" Plane translation (z)", &translation.z, 0.01f))
        plane->setTranslation(translation);
}
// Renderer
template<>
void print_window_object(SSS::GL::Renderer::Ptr const& renderer)
{
    if (!renderer) {
        return;
    }
    SSS::GL::Window::Objects const& objects = g_data->window->getObjects();

    // Display title of the Renderer
    ImGui::Text("Title: \"%s\"", renderer->title.c_str());

    static constexpr ImGuiTableFlags table_flags =
        ImGuiTableFlags_RowBg
        | ImGuiTableFlags_Borders
        | ImGuiTableFlags_NoHostExtendX
        | ImGuiTableFlags_SizingFixedFit
    ;
    // Display chunks in an organizable single column table
    if (!ImGui::BeginTable("RenderChunks", 1, table_flags)) {
        return;
    }
    // Display each RenderChunk
    for (size_t i = 0; i < renderer->size(); ++i) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        SSS::GL::RenderChunk& chunk = renderer->at(i);
        // Open a tree node for each RenderChunk
        static char tree_title[128];
        sprintf_s(tree_title, "Chunk: \"%s\"", chunk.title.c_str());
        bool const tree_open = ImGui::TreeNode(tree_title);
        // Option to drag ...
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoHoldToOpenOthers)) {
            // Give dragged source value
            ImGui::SetDragDropPayload("Chunk_swapping", &i, sizeof(size_t));
            // Drag & Drop preview
            ImGui::Text("Selected Chunk: \"%s\"", chunk.title.c_str());
            ImGui::EndDragDropSource();
        }
        // ... and drop tree nodes, to move/swap chunks
        if (ImGui::BeginDragDropTarget()) {
            // Payload is nullptr until a dragged source has been dropped
            ImGuiPayload const* payload = ImGui::AcceptDragDropPayload("Chunk_swapping");
            if (payload != nullptr) {
                // Retrieve dragged source value
                IM_ASSERT(payload->DataSize == sizeof(size_t));
                size_t payload_n = *(const size_t*)payload->Data;
                // Swap source and target values
                std::swap(renderer->at(payload_n), chunk);
            }
            ImGui::EndDragDropTarget();
        }
        // Display Chunk content
        if (tree_open) {
            // Display options in tree node, but don't indent
            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 0.0f);
            if (ImGui::TreeNode("Options")) {
                // Display combo to select Camera ID
                ImGui::SetNextItemWidth(100.f);
                if (ImGui::BeginCombo(" Camera ID", std::to_string(chunk.camera_ID).c_str())) {
                    // Loop over map to display each ID
                    for (auto it = objects.cameras.cbegin(); it != objects.cameras.cend(); ++it) {
                        uint32_t const id = it->first;
                        bool is_selected = (chunk.camera_ID == id);
                        // Display selectable ID
                        if (ImGui::Selectable(std::to_string(id).c_str(), is_selected)) {
                            chunk.camera_ID = id;
                        }
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                // Checkboxes for related booleans
                ImGui::Checkbox(" Use Camera?", &chunk.use_camera);
                ImGui::Checkbox(" Reset Z-buffer before rendering?", &chunk.reset_depth_before);
                // End tree content
                ImGui::TreePop();
            }
            // Revert style for further indenting
            ImGui::PopStyleVar();
            
            // Display each chunk object in an organizable single column table
            if (ImGui::BeginTable("##", 1, table_flags)) {
                for (size_t j = 0; j < chunk.objects.size(); ++j) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    // Hold pointer when dragging an object to allow for
                    // cross chunk communication (the data given to ImGui
                    // is internally copied, which doesn't work for ptrs)
                    static uint32_t* source_id_ptr = nullptr;
                    // Current object ID
                    uint32_t& id = chunk.objects.at(j);
                    // Title (written on selectable and drag preview)
                    static char plane_title[128];
                    sprintf_s(plane_title, "Plane %04u", id);
                    ImGui::Selectable(plane_title);
                    // Drag ...
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoHoldToOpenOthers)) {
                        // Give dragged source value to our static ptr
                        ImGui::SetDragDropPayload("Plane_swapping", nullptr, 0);
                        source_id_ptr = &id;
                        // Drag & Drop preview
                        ImGui::Text("Selected: \"%s\"", plane_title);
                        ImGui::EndDragDropSource();
                    }
                    // ... and drop
                    if (ImGui::BeginDragDropTarget()) {
                        ImGuiPayload const* payload = ImGui::AcceptDragDropPayload(
                            "Plane_swapping");
                        if (payload != nullptr && source_id_ptr != nullptr) {
                            // Swap source and target values
                            std::swap(*source_id_ptr, id);
                            source_id_ptr = nullptr;
                        }
                        ImGui::EndDragDropTarget();
                    }
                }
                ImGui::EndTable();
            }

            ImGui::TreePop();
        }
    }
    ImGui::EndTable();
}

// Default, deleted
template<typename _Object>
void create_window_object(uint32_t id) = delete;
// Camera
template<>
void create_window_object<SSS::GL::Camera>(uint32_t id)
{
    g_data->window->createCamera(id);
}
// Texture
template<>
void create_window_object<SSS::GL::Texture>(uint32_t id)
{
    g_data->window->createTexture(id);
}
// Plane
template<>
void create_window_object<SSS::GL::Plane>(uint32_t id)
{
    g_data->window->createModel(id, SSS::GL::ModelType::Plane);
}
// Renderer
template<>
void create_window_object<SSS::GL::Renderer>(uint32_t id)
{
    g_data->window->createRenderer<SSS::GL::Plane::Renderer>(id);
}

// Default, deleted
template<typename _Object>
void remove_window_object(uint32_t id) = delete;
// Camera
template<>
void remove_window_object<SSS::GL::Camera>(uint32_t id)
{
    g_data->window->removeCamera(id);
}
// Texture
template<>
void remove_window_object<SSS::GL::Texture>(uint32_t id)
{
    g_data->window->removeTexture(id);
}
// Plane
template<>
void remove_window_object<SSS::GL::Plane>(uint32_t id)
{
    g_data->window->removeModel(id, SSS::GL::ModelType::Plane);
}
// Renderer
template<>
void remove_window_object<SSS::GL::Renderer>(uint32_t id)
{
    g_data->window->removeRenderer(id);
}

template<class _Object>
void print_window_objects(std::map<uint32_t, std::unique_ptr<_Object>> const& map)
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
                remove_window_object<_Object>(ids[n]);
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
                    create_window_object<_Object>(uid);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::EndTabBar();
    }
}

void print_all_window_objects()
{
    // Button to clean objects
    if (ImGui::Button(" Clean All Objects ")) {
        g_data->window->cleanObjects();
    }

    SSS::GL::Window::Objects const& objects = g_data->window->getObjects();
    
    // Cameras
    if (ImGui::TreeNode("Cameras")) {
        print_window_objects(objects.cameras);
        ImGui::TreePop();
    }
    // Textures
    if (ImGui::TreeNode("Textures")) {
        print_window_objects(objects.textures);
        ImGui::TreePop();
    }
    // Planes
    if (ImGui::TreeNode("Planes")) {
        print_window_objects(objects.planes);
        ImGui::TreePop();
    }
    // Renderers
    if (ImGui::TreeNode("Renderers")) {
        print_window_objects(objects.renderers);
        ImGui::TreePop();
    }
}

void print_imgui()
{
    ui_window = g_data->ui_use_separate_window ? g_data->ui_window : g_data->window;
    // Bool to swap ImGui context if needed
    static bool swap_windows = false;
    if (swap_windows) {
        ImGuiHandle::swapContext(ui_window);
        swap_windows = false;
    }
    // Make context current
    SSS::GL::Context const context(ui_window);
    ImGuiHandle::newFrame();
    
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
        // UI options
        if (ImGui::CollapsingHeader("UI options")) {
            ImGui::SliderFloat(" Background Opacity", &bg_alpha, 0.f, 1.f);
            if (ImGui::Checkbox(" Display UI on a separate window",
                &g_data->ui_use_separate_window)) {
                swap_windows = true;
                if (g_data->ui_use_separate_window)
                    glfwShowWindow(g_data->ui_window->getGLFWwindow());
                else
                    glfwHideWindow(g_data->ui_window->getGLFWwindow());
            }
        }
        // Window options
        if (ImGui::CollapsingHeader("Window options")) {
            print_window_options();
        }
        // Window objects
        if (ImGui::CollapsingHeader("Window objects")) {
            print_all_window_objects();
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
    ui_window.reset();
}
