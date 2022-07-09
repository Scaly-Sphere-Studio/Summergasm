#include "imgui.hpp"

template<>
static void print_object(SSS::Audio::Source::Ptr const& source)
{
    if (!source) {
        return;
    }

    int volume = source->getVolume();
    if (ImGui::SliderInt(" Volume", &volume, 0, 100)) {
        source->setVolume(volume);
    }
    static constexpr ImGuiTableFlags table_flags =
        ImGuiTableFlags_NoHostExtendX
        | ImGuiTableFlags_SizingFixedFit
    ;
    if (ImGui::BeginTable("table1", 4, table_flags)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        if (ImGui::Button("Play", ImVec2(60.f, 0))) {
            source->play();
        }
        ImGui::TableSetColumnIndex(1);
        if (ImGui::Button("Pause", ImVec2(60.f, 0))) {
            source->pause();
        }
        ImGui::TableSetColumnIndex(2);
        if (ImGui::Button("Stop", ImVec2(60.f, 0))) {
            source->stop();
        }
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("State: %s.", source->isPlaying() ? "playing" :
            source->isPaused() ? "paused" : "stopped");
        ImGui::EndTable();
    }
    bool loop = source->isLooping();
    if (ImGui::Checkbox(" Loop", &loop)) {
        source->setLooping(loop);
    }

    SSS::Audio::Buffer::Map const& buffers = SSS::Audio::getBuffers();
    std::vector<uint32_t> buffer_ids = source->getBufferIDs();

    if (buffers.empty()) {
            ImGui::Text("No audio buffer exists.");
    }
    else if (ImGui::SmallButton("Add buffer")) {
        uint32_t new_id = buffers.cbegin()->first;
        for (auto const& pair : buffers) {
            bool found_similar = false;
            for (uint32_t id : buffer_ids) {
                if (pair.first == id) {
                    found_similar = true;
                    break;
                }
            }
            if (!found_similar) {
                new_id = pair.first;
                break;
            }
        }
        source->queueBuffers(std::vector<uint32_t>(1, new_id));
    }

    // Display each chunk object in an organizable single column table
    if (!buffer_ids.empty() && ImGui::BeginTable("##", 3, table_flags)) {
        char label[256];
        static bool hold_state = false;
        static size_t hold_i = 0;
        if (hold_state && !ImGui::IsMouseDown(0)) {
            hold_state = false;
        }
        if (hold_state) {
            SetCursor(ui_window, GLFW_HAND_CURSOR);
        }

        char drag_drop_id[256];
        sprintf_s(drag_drop_id, "Buffer_ID_Dragging");
        for (size_t i = 0; i < buffer_ids.size(); ) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            // Current object ID
            uint32_t& id = buffer_ids.at(i);
            // Selectable showing ID
            sprintf_s(label, "Buffer %u", id);
            bool selected = hold_state && hold_i == i;
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
                hold_i = i;
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (hold_i < buffer_ids.size()) {
                    int incr = hold_i < i ? 1 : -1;
                    for (size_t k = hold_i; k != i; k += incr) {
                        std::swap(buffer_ids.at(k), buffer_ids.at(k + incr));
                        source->detachBuffers();
                        source->queueBuffers(buffer_ids);
                    }
                }
                ImGui::EndDragDropTarget();
            }
            // Edit element
            ImGui::TableSetColumnIndex(1);
            char popup_map_id[256];
            sprintf_s(popup_map_id, "##edit_buffer_id%zu", i);
            sprintf_s(label, "*##edit_buffer_id%zu", i);
            if (Tooltip("Edit", ImGui::SmallButton, label)) {
                ImGui::OpenPopup(popup_map_id);
            }
            if (ImGui::BeginPopup(popup_map_id)) {
                ImGui::SetNextItemWidth(100.f);
                if (MapIDCombo(popup_map_id, SSS::Audio::getBuffers(), id)) {
                    source->detachBuffers();
                    source->queueBuffers(buffer_ids);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            // Delete element
            ImGui::TableSetColumnIndex(2);
            sprintf_s(label, "×##delete_buffer_id%zu", i);
            if (Tooltip("Delete", ImGui::SmallButton, label)) {
                buffer_ids.erase(buffer_ids.begin() + i);
                source->detachBuffers();
                source->queueBuffers(buffer_ids);
            }
            else
                ++i;
        }
        ImGui::EndTable();
    }

}

template<>
static void print_object(SSS::Audio::Buffer::Ptr const& buffer)
{
    if (!buffer) {
        return;
    }
    ImGui::FileBrowser& filebrowser = SSS::ImGuiH::getFilebrowser();
    // Load new file
    if (ImGui::Button("Load sound file")) {
        filebrowser.SetTypeFilters({ ".wav", ".mp3", ".flac", ".aiff" });
        filebrowser.Open();
    }
    // Display if needed
    filebrowser.Display();
    // If a file has been selected, update buffer
    if (filebrowser.HasSelected()) {
        buffer->loadFile(filebrowser.GetSelected().string());
        filebrowser.ClearSelected();
    }

    // Display properties
    ImGui::Text("%d Ko.", buffer->getProperty(AL_SIZE) / 1000);
    ImGui::Text("%d Hz.", buffer->getProperty(AL_FREQUENCY));
    ImGui::Text("%d bits.", buffer->getProperty(AL_BITS));
    ImGui::Text("%d channel(s).", buffer->getProperty(AL_CHANNELS));
}

template<>
static void create_object<SSS::Audio::Source>(uint32_t id)
{
    SSS::Audio::createSource(id);
}

template<>
static void create_object<SSS::Audio::Buffer>(uint32_t id)
{
    SSS::Audio::createBuffer(id);
}

template<>
static void remove_object<SSS::Audio::Source>(uint32_t id)
{
    SSS::Audio::removeSource(id);
}

template<>
static void remove_object<SSS::Audio::Buffer>(uint32_t id)
{
    SSS::Audio::removeBuffer(id);
}

void print_audio()
{
    int volume = SSS::Audio::getMainVolume();
    if (ImGui::SliderInt(" Volume", &volume, 0, 100)) {
        SSS::Audio::setMainVolume(volume);
    }
    std::vector<std::string> const devices = SSS::Audio::getDevices();
    std::string const current = SSS::Audio::getCurrentDevice();
    if (ImGui::BeginCombo(" Audio device", current.c_str())) {
        for (std::string const& name : devices) {
            bool is_selected = (name == current);
            if (ImGui::Selectable(name.c_str(), is_selected) && !is_selected) {
                SSS::Audio::selectDevice(name);
            }
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (ImGui::Button("Clean sources & buffers")) {
        SSS::Audio::cleanSources();
        SSS::Audio::cleanBuffers();
    }
    if (ImGui::TreeNode("Sources")) {
        print_objects<SSS::Audio::Source>(SSS::Audio::getSources());
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Buffers")) {
        print_objects<SSS::Audio::Buffer>(SSS::Audio::getBuffers());
        ImGui::TreePop();
    }
}