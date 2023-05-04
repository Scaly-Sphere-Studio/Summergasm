#include "imgui.hpp"
#include "mylua.hpp"

template <typename T>
static std::shared_ptr<T> print_member_object(std::shared_ptr<T> object, char const* id)
{
    char popup_id[256];
    char label[256];
    sprintf_s(label, "##select_%s", id);
    sprintf_s(label, "Select &##select_%s", id);
    static uint32_t object_id = 0;
    if (CopyButton(label)) {
        ImGui::OpenPopup(popup_id);
        std::vector<std::shared_ptr<T>> const vec = T::getInstances();
        auto const it = std::find(vec.cbegin(), vec.cend(), object);
        if (it != vec.cend()) {
            object_id = std::distance(vec.cbegin(), it);
        }
        else {
            object_id = 0;
        }
    }
    if (ImGui::BeginPopup(popup_id)) {
        std::vector<std::shared_ptr<T>> const vec = T::getInstances();
        if (selectVectorElement(vec, object_id)) {
            object = vec.at(object_id);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();

    if (object) {
        sprintf_s(popup_id, "##edit_%s", id);
        sprintf_s(label, "Edit *##edit_%s", id);
        if (EditButton(label)) {
            ImGui::OpenPopup(popup_id);
        }
        if (ImGui::BeginPopup(popup_id)) {
            print_object(*object);
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        sprintf_s(label, "Delete ×##delete_%s", id);
        if (DeleteButton(label)) {
            object.reset();
        }
    }
    else {
        sprintf_s(label, "Create +##create_%s", id);
        if (CreateButton(label)) {
            object = T::create();
        }
    }
    return object;
}

template<>
static void print_object(SSS::Base& base)
{
    ImGui::Text("Has been alive for %s",
        SSS::convertTime(std::chrono::steady_clock::now() - base.getCreationTime()).c_str());
}
// Camera
template<>
static void print_object(SSS::GL::Camera& camera)
{
    print_object<SSS::Base>(camera);

    // ID
    ImGui::Text("Projection:");
    // Projection type
    static const char* cam_proj_types[] = { "Ortho", "OrthoFixed", "Perspective" };
    int proj_id = static_cast<int>(camera.getProjectionType());
    if (ImGui::Combo(" Projection type", &proj_id, cam_proj_types, 3)) {
        camera.setProjectionType(static_cast<SSS::GL::Camera::Projection>(proj_id));
    }
    // Projection fov
    float fov = camera.getFOV();
    if (InputFloatWasEdited(" FOV", &fov, 1.0f, 1.0f, "%.0f")) {
        if (fov < 1.f)
            fov = 1.f;
        if (fov > 179.f)
            fov = 179.f;
        camera.setFOV(fov);
    }
    // Projection z_near / z_far
    float z_near, z_far;
    camera.getRange(z_near, z_far);
    if (InputFloatWasEdited(" Range (z_near)", &z_near, 0.001f, 0.001f, "%0.6f")) {
        if (z_near < 0.f)
            z_near = 0.001f;
        camera.setRange(z_near, z_far);
    }
    if (InputFloatWasEdited(" Range (z_far)", &z_far, 1.f))
        camera.setRange(z_near, z_far);
    ImGui::Spacing();
    // Position
    ImGui::Text("Position:");
    if (ImGui::Button("Reset position")) {
        camera.setPosition(glm::vec3(0));
    }
    glm::vec3 pos = camera.getPosition();
    if (InputFloatWasEdited(" Position (x)", &pos.x, 0.2f))
        camera.setPosition(pos);
    if (InputFloatWasEdited(" Position (y)", &pos.y, 0.2f))
        camera.setPosition(pos);
    if (InputFloatWasEdited(" Position (z)", &pos.z, 0.2f))
        camera.setPosition(pos);
    ImGui::Spacing();
    // Rotation
    ImGui::Text("Rotation:");
    if (ImGui::Button("Reset rotation")) {
        LOG_MSG("test")
            camera.setRotation(glm::vec2(0));
    }
    glm::vec2 rot = camera.getRotation();
    if (InputFloatWasEdited(" Rotation (x)", &rot.x, 0.4f))
        camera.setRotation(rot);
    if (InputFloatWasEdited(" Rotation (y)", &rot.y, 0.4f))
        camera.setRotation(rot);
}
// Texture
template<>
static void print_object(SSS::GL::Texture& texture)
{
    print_object<SSS::Base>(texture);

    // Get & Display dimensions
    int w, h;
    texture.getCurrentDimensions(w, h);
    ImGui::Text("Width: %d / Height: %d", w, h);

    // Display combo to choose Texture Type
    static const char* tex_types[] = { "Raw", "Text" };
    SSS::GL::Texture::Type type = texture.getType();
    int type_id = static_cast<int>(type);
    if (ImGui::Combo(" Texture type", &type_id, tex_types, 2)) {
        type = static_cast<SSS::GL::Texture::Type>(type_id);
        texture.setType(type);
        texture.getCurrentDimensions(w, h);
    }

    static float preview_width = 300.f;
    ImGui::SliderFloat("Preview's width (px)", &preview_width, 100.f, ui_window->getWidth(), "%.0f");

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
            texture.loadImage(filebrowser.GetSelected().string());
            filebrowser.ClearSelected();
        }
    }
    else if (type == SSS::GL::Texture::Type::Text) {
        //uint32_t current_id = texture.getTextAreaID();
        //if (MapIDCombo(" TextArea ID", SSS::TR::Area::getMap(), current_id)) {
        //    texture.setTextAreaID(current_id);
        //}
    }

    if (w != 0 && h != 0) {
        static SSS::GL::Basic::Texture tex([]() {
            SSS::GL::Basic::Texture tex(GL_TEXTURE_2D);
            tex.parameteri(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            tex.parameteri(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            tex.parameteri(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            tex.parameteri(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            return tex;
        }());

        void const* pixels = nullptr;
        switch (texture.getType()) {
            using Type = SSS::GL::Texture::Type;
        case Type::Raw: {
            pixels = texture.getRawPixels().data();
        }   break;
        case Type::Text: {
            pixels = texture.getTextArea()->pixelsGet();
        }   break;
        }

        tex.bind();
        tex.editSettings(w, h);
        tex.editPixels(pixels);

        float const ratio = static_cast<float>(w) / preview_width;
        ImVec2 dim(static_cast<float>(w) / ratio, static_cast<float>(h) / ratio);
        ImGui::Image(reinterpret_cast<void*>(static_cast<uintptr_t>(tex.id)), dim);
    }
}
// Plane
template<>
static void print_object(SSS::GL::Plane& plane)
{
    print_object<SSS::Base>(plane);
    // Display combo to select Hitbox Type
    int current_hitbox = static_cast<int>(plane.getHitbox());
    const char* hitboxes[] = { "None", "Alpha", "Full" };
    if (ImGui::Combo(" Hitbox Type", &current_hitbox, hitboxes, 3)) {
        plane.setHitbox(static_cast<SSS::GL::Plane::Hitbox>(current_hitbox));
    }
    
    ImGui::Text("Texture: ");
    ImGui::SameLine();
    plane.setTexture(print_member_object(plane.getTexture(), plane.getName().c_str()));

    glm::vec3 scaling, angles, translation;
    plane.getAllTransformations(scaling, angles, translation);

    // Scaling
    ImGui::Spacing();
    ImGui::Text("Plane Scaling:");
    if (ImGui::Button("Reset Plane Scaling"))
        plane.setScaling();
    float scaling_global = 1.f;
    float step = (scaling.x > scaling.y ? scaling.x : scaling.y) / 100.f;
    step = step > 0.01f ? 0.01f : step;
    if (InputFloatWasEdited(" Plane scaling (global)", &scaling_global, step))
        plane.setScaling(scaling * scaling_global);
    if (InputFloatWasEdited(" Plane scaling (x)", &scaling.x, 0.01f))
        plane.setScaling(scaling);
    if (InputFloatWasEdited(" Plane scaling (y)", &scaling.y, 0.01f))
        plane.setScaling(scaling);
    // Rotation
    ImGui::Spacing();
    ImGui::Text("Plane Rotation:");
    glm::vec3 old_angles(angles);
    if (ImGui::Button("Reset Plane Rotation"))
        plane.setRotation();
    if (InputFloatWasEdited(" Plane rotation (x)", &angles.x, 1.f))
        plane.rotate(angles - old_angles);
    if (InputFloatWasEdited(" Plane rotation (y)", &angles.y, 1.f))
        plane.rotate(angles - old_angles);
    if (InputFloatWasEdited(" Plane rotation (z)", &angles.z, 1.f))
        plane.rotate(angles - old_angles);
    // Translation
    ImGui::Spacing();
    ImGui::Text("Plane Translation:");
    if (ImGui::Button("Reset Plane Translation"))
        plane.setTranslation();
    if (InputFloatWasEdited(" Plane translation (x)", &translation.x, 0.01f))
        plane.setTranslation(translation);
    if (InputFloatWasEdited(" Plane translation (y)", &translation.y, 0.01f))
        plane.setTranslation(translation);
    if (InputFloatWasEdited(" Plane translation (z)", &translation.z, 0.01f))
        plane.setTranslation(translation);
}
// Renderer
template<>
static void print_object(SSS::GL::PlaneRenderer& renderer)
{
    print_object<SSS::Base>(renderer);

    static constexpr ImGuiTableFlags table_flags =
        ImGuiTableFlags_RowBg
        | ImGuiTableFlags_BordersInnerH
        | ImGuiTableFlags_BordersOuter
        | ImGuiTableFlags_NoHostExtendX
        | ImGuiTableFlags_SizingFixedFit
    ;

    char label[256];
    SSS::GL::Plane::Vector& planes = renderer.planes;

    // Checkboxes for related booleans
    ImGui::Checkbox(" Reset Z-buffer before rendering?", &renderer.clear_depth_buffer);

    // Display camera
    ImGui::Text("Camera:");
    ImGui::SameLine();
    renderer.camera = print_member_object(renderer.camera, renderer.getName().c_str());

    // Display plane creation button if empty
    if (planes.empty()) {
        if (ImGui::BeginTable("##", 1, table_flags)) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            sprintf_s(label, "Create Plane +##create_plane%s", renderer.getName().c_str());
            if (Tooltip("Create new Plane.", CreateButton, label)) {
                planes.emplace_back(SSS::GL::Plane::create());
            }
            ImGui::EndTable();
        }
    }
    // Display each Plane in an organizable single column table
    else if (ImGui::BeginTable("##", 5, table_flags)) {

        static bool hold_state = false;
        static size_t hold_i = 0;

        if (hold_state && !ImGui::IsMouseDown(0)) {
            hold_state = false;
        }
        if (hold_state) {
            SetCursor(ui_window, GLFW_HAND_CURSOR);
        }
        char drag_drop_id[256];
        sprintf_s(drag_drop_id, "Plane_Dragging_%s", renderer.getName().c_str());

                
        // Display each planes
        for (size_t i = 0; i < planes.size(); ) {

            SSS::GL::Plane::Shared const& plane = planes.at(i);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            sprintf_s(label, "%s", plane->getName().c_str());
            // Whether this selectable is selected
            bool const selected = hold_state && hold_i == i;
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
                hold_i = i;
                ImGui::EndDragDropSource();
            }
            // Drag actions
            if (ImGui::BeginDragDropTarget()) {
                if (hold_i < planes.size()) {
                    int incr = hold_i < i ? 1 : -1;
                    for (size_t j = hold_i; j != i; j += incr) {
                        std::swap(planes.at(j), planes.at(j + incr));
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // Edit plane
            ImGui::TableSetColumnIndex(1);
            char popup_map_id[256];
            sprintf_s(popup_map_id, "##edit_plane%zu", i);
            sprintf_s(label, "*##edit_plane%zu", i);
            if (Tooltip("Edit this Plane", EditButton, label)) {
                ImGui::OpenPopup(popup_map_id);
            }
            if (ImGui::BeginPopup(popup_map_id)) {
                print_object(*plane);
                ImGui::EndPopup();
            }

            // Create new plane
            ImGui::TableSetColumnIndex(2);
            static std::string new_plane_name;
            sprintf_s(label, "+##create_plane%zu", i);
            if (Tooltip("Create a new Plane", StringCreateButton, label, std::ref(new_plane_name))) {
                auto new_plane = SSS::GL::Plane::create();
                new_plane->setName(new_plane_name);
                new_plane_name.clear();
                planes.insert(planes.cbegin() + i + 1, new_plane);
                return;
            }

            // Copy this plane
            ImGui::TableSetColumnIndex(3);
            sprintf_s(label, "&##copy_plane%zu", i);
            if (Tooltip("Copy this Plane", CopyButton, label)) {
                planes.insert(planes.cbegin() + i + 1, plane->duplicate());
            }

            // Delete plane
            ImGui::TableSetColumnIndex(4);
            sprintf_s(label, "×##delete_plane%zu", i);
            if (Tooltip("Delete this Plane", DeleteButton, label)) {
                planes.erase(planes.begin() + i);
            }
            else
                ++i;
        }
        ImGui::EndTable();
    }
}

template<>
static void print_object(SSS::TR::Area& area)
{
    print_object<SSS::Base>(area);
    ImGui::Text("TODO: Text area displays");
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
}
// Plane
template<>
static void create_object<SSS::GL::Plane>(uint32_t id)
{
}
// Renderer
template<>
static void create_object<SSS::GL::PlaneRenderer>(uint32_t id)
{
}
// Area
template<>
static void create_object<SSS::TR::Area>(uint32_t id)
{
}
// Base
template<>
static void create_object<SSS::Base>(uint32_t id)
{
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
}
// Plane
template<>
static void remove_object<SSS::GL::Plane>(uint32_t id)
{
}
// Renderer
template<>
static void remove_object<SSS::GL::PlaneRenderer>(uint32_t id)
{
}
// Area
template<>
static void remove_object<SSS::TR::Area>(uint32_t id)
{
}
// Base
template<>
static void remove_object<SSS::Base>(uint32_t id)
{
}

template<typename T>
void print_tab_object(sol::object const& value, ImGuiCol text_col)
{
    ImGui::PushStyleColor(ImGuiCol_Tab, ImColor(0.2f, 0.2f, 0.2f).Value);
    ImGui::PushStyleColor(ImGuiCol_TabHovered, ImColor(0.45f, 0.45f, 0.45f).Value);
    ImGui::PushStyleColor(ImGuiCol_TabActive, ImColor(0.35f, 0.35f, 0.35f).Value);
    ImGui::PushStyleColor(ImGuiCol_Text, text_col);
    bool const ret = ImGui::BeginTabItem(value.as<SSS::Base>().getName().c_str());
    ImGui::PopStyleColor(4);
    if (ret) {
        print_object(value.as<T>());
        ImGui::EndTabItem();
    }
}

static void print_env(char const* name, sol::environment const& env)
{
    using namespace SSS;

    if (!ImGui::TreeNode(name))
        return;
    if (!ImGui::BeginTabBar("###")) {
        ImGui::TreePop();
        return;
    }

    std::map<std::chrono::steady_clock::time_point, sol::object> objects;
    for (auto&& [key, value] : env) {
        if (value.get_type() != sol::type::userdata || key.get_type() != sol::type::string)
            continue;
        if (key.as<std::string>().find("sol.") == 0)
            continue;
        objects[value.as<Base>().getCreationTime()] = value;
    }
    for (auto&& [time, value] : objects) {
        if (value.is<GL::Camera>()) {
            print_tab_object<GL::Camera>(value, ImColor(0.f, 0.8f, 0.2f));
        }
        else if (value.is<GL::Texture>()) {
            print_tab_object<GL::Texture>(value, ImColor(0.f, 0.7f, 0.7f));
        }
        else if (value.is<GL::Plane>()) {
            print_tab_object<GL::Plane>(value, ImColor(0.8f, 0.3f, 0.3f));
        }
        else if (value.is<GL::PlaneRenderer>()) {
            print_tab_object<GL::PlaneRenderer>(value, ImColor(0.7f, 0.f, 0.8f));
        }
        else if (value.is<TR::Area>()) {
            print_tab_object<TR::Area>(value, ImColor(0.3f, 0.8f, 0.3f));
        }
        else {
            print_tab_object<SSS::Base>(value, ImColor(0.8f, 0.8f, 0.8f));
        }
    }
    ImGui::EndTabBar();
    ImGui::TreePop();
};

void print_window_objects()
{
    print_env("Globals", g->lua.globals());
    for (auto const& [key, scene] : g->lua_scenes) {
        if (!scene)
            continue;
        print_env(key.c_str(), scene->getEnv());
    }

    //// Textures
    //if (ImGui::TreeNode("Textures")) {
    //    print_objects<SSS::GL::Texture>(SSS::GL::Texture::getInstances());
    //    ImGui::TreePop();
    //}
    //// Renderers
    //if (ImGui::TreeNode("Renderers")) {
    //    auto renderers = g->window->getRenderers();
    //    std::vector<SSS::GL::PlaneRenderer::Shared> v;
    //    for (auto renderer : renderers) {
    //        auto plane_renderer = std::dynamic_pointer_cast<SSS::GL::PlaneRenderer>(renderer);
    //        if (plane_renderer)
    //            v.emplace_back(plane_renderer);
    //    }
    //    print_objects<SSS::GL::PlaneRenderer>(v);
    //    ImGui::TreePop();
    //}
}