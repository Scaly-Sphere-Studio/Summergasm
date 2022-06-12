#include "imgui.hpp"

template<>
static void print_object(SSS::Audio::Buffer::Ptr const& buffer)
{
    if (!buffer) {
        return;
    }

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