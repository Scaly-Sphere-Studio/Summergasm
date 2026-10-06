#include "imgui.hpp"
#include "Settings.hpp"

static void close_settings()
{
    g->settings_display = false;
    g->window->unblockInputs();
}

// Player settings (see Settings.hpp), over the main window. F7 or Escape closes it.
void print_settings()
{
    if (glfwGetKey(g->window->getGLFWwindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        close_settings();
        return;
    }

    SSS::GL::Context const context = g->window->setContext();
    SSS::ImGuiH::setContext(g->window->getGLFWwindow());
    if (!SSS::ImGuiH::newFrame()) {
        return;
    }

    // Centered the first time, then where the player leaves it
    int w, h;
    g->window->getDimensions(w, h);
    ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.5f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.9f);
    constexpr ImGuiWindowFlags flags
        = ImGuiWindowFlags_AlwaysAutoResize
        | ImGuiWindowFlags_NoCollapse
    ;

    bool open = true;
    if (ImGui::Begin("Settings (F7)", &open, flags)) {
        auto const& s = settings::get();
        ImGui::SeparatorText("Dialog");
        bool tails = s.dialog_tails;
        if (ImGui::Checkbox(" Speech bubble tails", &tails))
            settings::setDialogTails(tails);
        Tooltip("Tails from the dialogue box to the mouths of the characters speaking");
        bool grey = s.grey_characters;
        if (ImGui::Checkbox(" Grey out characters not talking", &grey))
            settings::setGreyCharacters(grey);
        Tooltip("Characters who are not talking are drawn in black & white");
        ImGui::Spacing();
        ImGui::TextDisabled("Saved in settings.ini");
    }
    ImGui::End();

    SSS::ImGuiH::render();

    if (!open)
        close_settings();
}
