#include "Summergasm.hpp"

constexpr static bool print_demo = false;
static SSS::GL::Window::Shared ui_window;

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

template <typename _Func, typename ...Args>
bool Tooltip(char const* description, _Func f, Args ...args)
{
    bool ret = f(args...);
    Tooltip(description);
    return ret;
}

bool InputFloatWasEdited(const char* label, float* v, float step = 0.0f,
    float step_fast = 0.0f, const char* format = "%.3f", ImGuiInputTextFlags flags = 0)
{
    char name[256];
    ImGui::SetNextItemWidth(250.f);
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

template<typename _T>
bool MapIDCombo(char const* label, std::map<uint32_t, _T> const& map, uint32_t& current_id)
{
    bool ret = false;
    if (ImGui::BeginCombo(label, std::to_string(current_id).c_str())) {
        // Loop over map to display each ID
        for (auto it = map.cbegin(); it != map.cend(); ++it) {
            uint32_t const id = it->first;
            bool is_selected = (current_id == id);
            // Display selectable ID
            if (ImGui::Selectable(std::to_string(id).c_str(), is_selected)) {
                current_id = id;
                ret = true;
            }
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return ret;
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
        uint32_t current_id = texture->getTextAreaID();
        if (MapIDCombo(" TextArea ID", SSS::TR::TextArea::getTextAreas(), current_id)) {
            texture->setTextAreaID(current_id);
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
    uint32_t texture_id = plane->getTextureID();
    if (MapIDCombo(" Texture ID", objects.textures, texture_id)) {
        plane->setTextureID(texture_id);
    }
    // Display combo to select Hitbox Type
    int current_hitbox = static_cast<int>(plane->getHitbox());
    const char* hitboxes[] = { "None", "Alpha", "Full" };
    if (ImGui::Combo(" Hitbox Type", &current_hitbox, hitboxes, 3)) {
        plane->setHitbox(static_cast<SSS::GL::Plane::Hitbox>(current_hitbox));
    }
    // Display combo to select On Click Function ID
    uint32_t on_click_func_id = plane->getOnClickFuncID();
    if (MapIDCombo(" OnClickFuncID", SSS::GL::Model::on_click_funcs, on_click_func_id)) {
        plane->setOnClickFuncID(on_click_func_id);
    }
    // Display combo to select Passive Function ID
    uint32_t passive_func_id = plane->getPassiveFuncID();
    if (MapIDCombo(" PassiveFuncID", SSS::GL::Model::passive_funcs, passive_func_id)) {
        plane->setOnClickFuncID(passive_func_id);
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
    ImGui::Text("Renderer title: \"%s\"", renderer->title.c_str());
    StringButtonEdit("Edit title", renderer->title);

    static constexpr ImGuiTableFlags table_flags =
        ImGuiTableFlags_RowBg
        | ImGuiTableFlags_BordersInnerH
        | ImGuiTableFlags_BordersOuter
        | ImGuiTableFlags_NoHostExtendX
        | ImGuiTableFlags_SizingFixedFit
    ;
    // Display chunks in an organizable single column table
    if (!ImGui::BeginTable("RenderChunks", 2, table_flags)) {
        return;
    }
    // Button to add chunks
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    static std::string new_chunk_title;
    if (StringButtonEdit("Add chunk", new_chunk_title)) {
        renderer->emplace_front().title = new_chunk_title;
        new_chunk_title.clear();
    }
    
    static void* active_item = nullptr;
    bool is_any_plane_hovered = false;
    char label[256];
    // Display each RenderChunk
    for (size_t i = 0; i < renderer->size(); ) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        SSS::GL::RenderChunk& chunk = renderer->at(i);
        // Open a tree node for each RenderChunk
        static char tree_title[256];
        sprintf_s(tree_title, "Chunk: \"%s\"", chunk.title.c_str());
        bool const tree_open = ImGui::TreeNode(tree_title);
        if (ImGui::IsItemHovered()) {
            SetCursor(ui_window, GLFW_HAND_CURSOR);
        }
        // Option to drag ...
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoHoldToOpenOthers)) {
            // Give dragged source value
            ImGui::SetDragDropPayload("Chunk_swapping", &i, sizeof(size_t));
            SetCursor(ui_window, GLFW_HAND_CURSOR);
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
                StringButtonEdit("Edit chunk title", chunk.title);
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
            if (objects.planes.empty()) {
                ImGui::Text("No plane exists.");
            }
            else if (ImGui::SmallButton("Add plane")) {
                uint32_t new_id = 0;
                for (auto it = objects.planes.cbegin(); it != objects.planes.cend(); ++it) {
                    bool found_similar = false;
                    for (uint32_t id : chunk.objects) {
                        if (it->first == id) {
                            found_similar = true;
                        }
                    }
                    if (!found_similar) {
                        new_id = it->first;
                        break;
                    }
                }
                chunk.objects.push_back(new_id);
            }
            
            // Display each chunk object in an organizable single column table
            if (!chunk.objects.empty() && ImGui::BeginTable("##", 3, table_flags)) {
                static bool hold_state = false;
                static size_t hold_chunk = 0;
                static size_t hold_j = 0;
                if (hold_state && !ImGui::IsMouseDown(0)) {
                    hold_state = false;
                }
                if (hold_state) {
                    SetCursor(ui_window, GLFW_HAND_CURSOR);
                }

                char drag_drop_id[256];
                sprintf_s(drag_drop_id, "Plane_Dragging_%zu", i);
                for (size_t j = 0; j < chunk.objects.size(); ) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    // Current object ID
                    uint32_t& id = chunk.objects.at(j);
                    // Selectable showing ID
                    sprintf_s(label, "Plane %u", id);
                    bool selected = hold_state && hold_chunk == i && hold_j == j;
                    ImGui::Selectable(label, selected);
                    if (ImGui::IsItemHovered()) {
                        SetCursor(ui_window, GLFW_HAND_CURSOR);
                    }
                    Tooltip("Drag to reorder");
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoPreviewTooltip
                        | ImGuiDragDropFlags_SourceNoDisableHover
                        | ImGuiDragDropFlags_SourceNoHoldToOpenOthers))
                    {
                        // Give dragged source value
                        ImGui::SetDragDropPayload(drag_drop_id, nullptr, 0);
                        hold_state = true;
                        hold_chunk = i;
                        hold_j = j;
                        ImGui::EndDragDropSource();
                    }
                    if (ImGui::BeginDragDropTarget()) {
                        if (hold_j < chunk.objects.size()) {
                            int incr = hold_j < j ? 1 : -1;
                            for (size_t k = hold_j; k != j; k += incr) {
                                std::swap(chunk.objects.at(k), chunk.objects.at(k + incr));
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                    // Edit element
                    ImGui::TableSetColumnIndex(1);
                    char popup_map_id[256];
                    sprintf_s(popup_map_id, "##edit_plane%zu", j);
                    sprintf_s(label, "*##edit_plane%zu", j);
                    if (Tooltip("Edit", ImGui::SmallButton, label)) {
                        ImGui::OpenPopup(popup_map_id);
                    }
                    if (ImGui::BeginPopup(popup_map_id)) {
                        ImGui::SetNextItemWidth(100.f);
                        if (MapIDCombo(popup_map_id, objects.planes, id)) {
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::EndPopup();
                    }
                    // Delete element
                    ImGui::TableSetColumnIndex(2);
                    sprintf_s(label, "×##delete_plane%zu", j);
                    if (Tooltip("Delete", ImGui::SmallButton, label)) {
                        chunk.objects.erase(chunk.objects.begin() + j);
                    }
                    else
                        ++j;
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
        // Destroy chunk
        ImGui::TableSetColumnIndex(1);
        sprintf_s(label, "×##chunk%zu", i);
        if (Tooltip("Delete", ImGui::SmallButton, label)) {
            renderer->erase(renderer->begin() + i);
        }
        else
            ++i;
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
        SSS::ImGuiH::setContext(ui_window->getGLFWwindow());
        swap_windows = false;
    }
    // Make context current
    SSS::GL::Context const context(ui_window);
    SSS::ImGuiH::newFrame();
    
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
    SSS::ImGuiH::render();
    ui_window.reset();
}
