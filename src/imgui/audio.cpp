#include "imgui.hpp"

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
static void print_object(SSS::Audio::Source::Ptr const& source)
{
    if (!source) {
        return;
    }

}

template<>
static void create_object<SSS::Audio::Buffer>(uint32_t id)
{
    SSS::Audio::createBuffer(id);
}

template<>
static void create_object<SSS::Audio::Source>(uint32_t id)
{
    SSS::Audio::createSource(id);
}

template<>
static void remove_object<SSS::Audio::Buffer>(uint32_t id)
{
    SSS::Audio::removeBuffer(id);
}

template<>
static void remove_object<SSS::Audio::Source>(uint32_t id)
{
    SSS::Audio::removeSource(id);
}

void print_audio()
{
    if (ImGui::TreeNode("Buffers")) {
        print_objects<SSS::Audio::Buffer>(SSS::Audio::getBuffers());
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Sources")) {
        print_objects<SSS::Audio::Source>(SSS::Audio::getSources());
        ImGui::TreePop();
    }
}