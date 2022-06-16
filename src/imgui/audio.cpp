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
}

template<>
static void print_object(SSS::Audio::Buffer::Ptr const& buffer)
{
    if (!buffer) {
        return;
    }
    // Load new file
    if (ImGui::Button("Load sound file")) {
        SSS::ImGuiH::filebrowser.SetTypeFilters({ ".wav", ".mp3", ".flac", ".aiff" });
        SSS::ImGuiH::filebrowser.Open();
    }
    // Display if needed
    SSS::ImGuiH::filebrowser.Display();
    // If a file has been selected, update buffer
    if (SSS::ImGuiH::filebrowser.HasSelected()) {
        buffer->loadFile(SSS::ImGuiH::filebrowser.GetSelected().string());
        SSS::ImGuiH::filebrowser.ClearSelected();
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
    if (ImGui::TreeNode("Sources")) {
        print_objects<SSS::Audio::Source>(SSS::Audio::getSources());
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Buffers")) {
        print_objects<SSS::Audio::Buffer>(SSS::Audio::getBuffers());
        ImGui::TreePop();
    }
}