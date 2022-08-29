#include "imgui.hpp"

// Camera
template<>
static void print_object(SSS::GL::Camera::Shared const& camera)
{
    if (!camera) {
        return;
    }

    TextCentered("Camera");
    ImGui::Text("");
    
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
    if (InputFloatWasEdited(" FOV", &fov, 1.0f, 1.0f, "%.0f")) {
        if (fov < 1.f)
            fov = 1.f;
        if (fov > 179.f)
            fov = 179.f;
        camera->setFOV(fov);
    }
    // Projection z_near / z_far
    float z_near, z_far;
    camera->getRange(z_near, z_far);
    if (InputFloatWasEdited(" Range (z_near)", &z_near, 0.001f, 0.001f, "%0.6f")) {
        if (z_near < 0.f)
            z_near = 0.001f;
        camera->setRange(z_near, z_far);
    }
    if (InputFloatWasEdited(" Range (z_far)", &z_far, 1.f))
        camera->setRange(z_near, z_far);
    ImGui::Spacing();
    // Position
    ImGui::Text("Position:");
    if (ImGui::Button("Reset position")) {
        camera->setPosition(glm::vec3(0));
    }
    glm::vec3 pos = camera->getPosition();
    if (InputFloatWasEdited(" Position (x)", &pos.x, 0.2f))
        camera->setPosition(pos);
    if (InputFloatWasEdited(" Position (y)", &pos.y, 0.2f))
        camera->setPosition(pos);
    if (InputFloatWasEdited(" Position (z)", &pos.z, 0.2f))
        camera->setPosition(pos);
    ImGui::Spacing();
    // Rotation
    ImGui::Text("Rotation:");
    if (ImGui::Button("Reset rotation")) {
        LOG_MSG("test")
            camera->setRotation(glm::vec2(0));
    }
    glm::vec2 rot = camera->getRotation();
    if (InputFloatWasEdited(" Rotation (x)", &rot.x, 0.4f))
        camera->setRotation(rot);
    if (InputFloatWasEdited(" Rotation (y)", &rot.y, 0.4f))
        camera->setRotation(rot);

    ImGui::Text("");
    TextCentered("(ADDR: 0x%p)", static_cast<void*>(&*camera));
}
// Texture
template<>
static void print_object(SSS::GL::Texture::Ptr const& texture)
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
        ImGui::FileBrowser& filebrowser = SSS::ImGuiH::getFilebrowser();
        // Button to display FileBrowser
        if (ImGui::Button("New filepath")) {
            filebrowser.SetTypeFilters({ ".png", ".bmp", ".jpg", ".jpeg" });
            filebrowser.Open();
        }
        // Display if needed
        filebrowser.Display();
        // If a file has been selected, update texture
        if (filebrowser.HasSelected()) {
            texture->loadImage(filebrowser.GetSelected().string());
            filebrowser.ClearSelected();
        }
    }
    else if (type == SSS::GL::Texture::Type::Text) {
        uint32_t current_id = texture->getTextAreaID();
        if (MapIDCombo(" TextArea ID", SSS::TR::Area::getMap(), current_id)) {
            texture->setTextAreaID(current_id);
        }
    }
    if (ui_window == g_data->window) {
        int w, h;
        texture->getCurrentDimensions(w, h);
        if (w != 0 && h != 0) {
            float const ratio = static_cast<float>(w) / 300.f;
            ImVec2 dim(static_cast<float>(w) / ratio, static_cast<float>(h) / ratio);
            // C4312
#ifdef _WIN64
            uint64_t const id = texture->getBasicTextureID();
#else
            uint32_t const id = texture->getBasicTextureID();
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
static void print_object(SSS::GL::Plane::Shared const& plane)
{
    if (!plane) {
        return;
    }
    SSS::GL::Window::Objects const& objects = g_data->window->getObjects();

    TextCentered("Plane");
    ImGui::Text("");

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
    if (MapIDCombo(" OnClickFuncID", SSS::GL::Plane::on_click_funcs, on_click_func_id)) {
        plane->setOnClickFuncID(on_click_func_id);
    }
    // Display combo to select Passive Function ID
    uint32_t passive_func_id = plane->getPassiveFuncID();
    if (MapIDCombo(" PassiveFuncID", SSS::GL::Plane::passive_funcs, passive_func_id)) {
        plane->setPassiveFuncID(passive_func_id);
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

    ImGui::Text("");
    TextCentered("(ADDR: 0x%p)", static_cast<void*>(&*plane));
}
// Renderer
template<>
static void print_object(SSS::GL::Renderer::Ptr const& renderer_ptr)
{
    if (!renderer_ptr) {
        return;
    }
    SSS::GL::Window::Objects const& objects = g_data->window->getObjects();

    SSS::GL::PlaneRenderer& renderer = renderer_ptr->castAs<SSS::GL::PlaneRenderer>();
    // Display title of the Renderer
    ImGui::Text("Renderer title: \"%s\"", renderer.title.c_str());
    StringButtonEdit("Edit title", renderer.title);

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
        renderer.chunks.emplace_back().title = new_chunk_title;
        new_chunk_title.clear();
    }

    SSS::GL::Plane::Vector const all_planes = SSS::GL::Plane::getInstances(g_data->window);

    char label[256];
    // Display each RenderChunk
    for (size_t i = 0; i < renderer.chunks.size(); ) {
        SSS::GL::PlaneRenderer::Chunk& chunk = renderer.chunks.at(i);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);

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
                std::swap(renderer.chunks.at(payload_n), chunk);
            }
            ImGui::EndDragDropTarget();
        }

        // Display Chunk content
        if (tree_open) {
            SSS::GL::Plane::Vector& planes = chunk.planes;

            // Edit chunk title
            StringButtonEdit("Edit chunk title", chunk.title);

            // Checkboxes for related booleans
            ImGui::Checkbox(" Reset Z-buffer before rendering?", &chunk.reset_depth_before);

            // Display camera
            ImGui::Text("Camera:");
            ImGui::SameLine();
            {
                if (chunk.camera) {
                    char popup_id[256];
                    sprintf_s(popup_id, "##edit_camera%zu", i);
                    sprintf_s(label, "Edit *##edit_camera%zu", i);
                    if (EditButton(label)) {
                        ImGui::OpenPopup(popup_id);
                    }
                    if (ImGui::BeginPopup(popup_id)) {
                        print_object(chunk.camera);
                        ImGui::EndPopup();
                    }
                    ImGui::SameLine();
                    sprintf_s(label, "Delete ×##delete_camera%zu", i);
                    if (DeleteButton(label)) {
                        chunk.camera.reset();
                    }
                }
                else {
                    sprintf_s(label, "Create +##create_camera%zu", i);
                    if (CreateButton(label)) {
                        chunk.camera = SSS::GL::Camera::create(g_data->window);
                    }
                    ImGui::SameLine();
                    char popup_id[256];
                    sprintf_s(popup_id, "##copy_camera%zu", i);
                    sprintf_s(label, "Use existing &##copy_camera%zu", i);
                    static uint32_t cam_id = 0;
                    if (CopyButton(label)) {
                        ImGui::OpenPopup(popup_id);
                        cam_id = 0;
                    }
                    if (ImGui::BeginPopup(popup_id)) {
                        SSS::GL::Camera::Vector const vec = SSS::GL::Camera::getInstances(g_data->window);
                        if (selectVectorElement(vec, cam_id)) {
                            chunk.camera = vec.at(cam_id);
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::EndPopup();
                    }
                }
            }

            // Display plane creation button if empty
            if (planes.empty()) {
                if (ImGui::BeginTable("##", 1, table_flags)) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    sprintf_s(label, "Create Plane +##create_plane%zu", i);
                    if (Tooltip("Create new Plane.", CreateButton, label)) {
                        planes.emplace_back(SSS::GL::Plane::create(g_data->window));
                    }
                    ImGui::EndTable();
                }
            }
            // Display each Plane in an organizable single column table
            else if (ImGui::BeginTable("##", 5, table_flags)) {

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

                
                // Display each planes
                for (size_t j = 0; j < planes.size(); ) {

                    SSS::GL::Plane::Shared const& plane = planes.at(j);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);

                    // Create selectable label based on instance ID
                    uint32_t const id = static_cast<uint32_t>(std::distance(
                        all_planes.cbegin(),
                        std::find(all_planes.cbegin(), all_planes.cend(), plane)
                    ));
                    sprintf_s(label, "Plane #%u", id);
                    // Whether this selectable is selected
                    bool const selected = hold_state && hold_chunk == i && hold_j == j;
                    // Display selectable plane ID
                    ImGui::Selectable(label, selected);
                    if (ImGui::IsItemHovered()) {
                        SetCursor(ui_window, GLFW_HAND_CURSOR);
                    }
                    Tooltip("Drag to reorder");

                    // Drag info
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
                    // Drag actions
                    if (ImGui::BeginDragDropTarget()) {
                        if (hold_chunk == i && hold_j < planes.size()) {
                            int incr = hold_j < j ? 1 : -1;
                            for (size_t k = hold_j; k != j; k += incr) {
                                std::swap(planes.at(k), planes.at(k + incr));
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    // Edit plane
                    ImGui::TableSetColumnIndex(1);
                    char popup_map_id[256];
                    sprintf_s(popup_map_id, "##edit_plane%zu", j);
                    sprintf_s(label, "*##edit_plane%zu", j);
                    if (Tooltip("Edit this Plane", EditButton, label)) {
                        ImGui::OpenPopup(popup_map_id);
                    }
                    if (ImGui::BeginPopup(popup_map_id)) {
                        print_object(plane);
                        ImGui::EndPopup();
                    }

                    // Create new plane
                    ImGui::TableSetColumnIndex(2);
                    sprintf_s(label, "+##create_plane%zu", j);
                    if (Tooltip("Create a new Plane", CreateButton, label)) {
                        planes.insert(planes.cbegin() + j + 1,
                            SSS::GL::Plane::create(g_data->window));
                    }

                    // Copy this plane
                    ImGui::TableSetColumnIndex(3);
                    sprintf_s(label, "&##copy_plane%zu", j);
                    if (Tooltip("Copy this Plane", CopyButton, label)) {
                        planes.insert(planes.cbegin() + j + 1, plane->duplicate());
                    }

                    // Delete plane
                    ImGui::TableSetColumnIndex(4);
                    sprintf_s(label, "×##delete_plane%zu", j);
                    if (Tooltip("Delete this Plane", DeleteButton, label)) {
                        planes.erase(planes.begin() + j);
                    }
                    else
                        ++j;
                }
                ImGui::EndTable();
            }

            // Close tree
            ImGui::TreePop();
        }
        
        // Destroy chunk
        ImGui::TableSetColumnIndex(1);
        sprintf_s(label, "×##chunk%zu", i);
        if (Tooltip("Delete", ImGui::SmallButton, label)) {
            renderer.chunks.erase(renderer.chunks.cbegin() + i);
        }
        else
            ++i;
    }
    ImGui::EndTable();
}

// Camera
template<>
static void create_object<SSS::GL::Camera>(uint32_t id)
{
}
// Texture
template<>
static void create_object<SSS::GL::Texture>(uint32_t id)
{
    g_data->window->createTexture(id);
}
// Plane
template<>
static void create_object<SSS::GL::Plane>(uint32_t id)
{
}
// Renderer
template<>
static void create_object<SSS::GL::Renderer>(uint32_t id)
{
    g_data->window->createRenderer<SSS::GL::PlaneRenderer>(id);
}

// Camera
template<>
static void remove_object<SSS::GL::Camera>(uint32_t id)
{
}
// Texture
template<>
static void remove_object<SSS::GL::Texture>(uint32_t id)
{
    g_data->window->removeTexture(id);
}
// Plane
template<>
static void remove_object<SSS::GL::Plane>(uint32_t id)
{
}
// Renderer
template<>
static void remove_object<SSS::GL::Renderer>(uint32_t id)
{
    g_data->window->removeRenderer(id);
}

void print_window_objects()
{
    // Button to clean objects
    if (ImGui::Button(" Clean All Objects ")) {
        g_data->window->cleanObjects();
    }

    SSS::GL::Window::Objects const& objects = g_data->window->getObjects();

    // Textures
    if (ImGui::TreeNode("Textures")) {
        print_objects<SSS::GL::Texture>(objects.textures);
        ImGui::TreePop();
    }
    // Renderers
    if (ImGui::TreeNode("Renderers")) {
        print_objects<SSS::GL::Renderer>(objects.renderers);
        ImGui::TreePop();
    }
}