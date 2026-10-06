#pragma once

#include <SSS/ImGuiH.hpp>
#include <SSS/GL.hpp>
#include <SSS/Audio.hpp>

#pragma warning(push, 0)
#include <nlohmann/json.hpp>
#pragma warning(pop)

// sol2 is heavy: see lua_include.hpp
namespace sol { class state; }

void exitSummergasm(int status);

class Scene;

struct GlobalData {
    // Defined in main.cpp, where sol::state is a complete type
    GlobalData();
    ~GlobalData();

    SSS::GL::Window* window;
    SSS::GL::Window* ui_window;

    bool ui_display{ false };
    bool ui_use_separate_window{ false };
    bool console_display{ false };
    // Player settings panel (F7)
    bool settings_display{ false };

    std::string home_folder;
    std::string resources_folder;
    std::string lua_folder;
    std::string assets_folder;
    // Ren'Py scripts (.rpy, .txt) played by the dialog node, see Dialog.hpp
    std::string dialogs_folder;
    // One <id>.json per Ren'Py character: mouth of each expression, for the
    // dialog's speech bubble tails (see dialog/CharacterData.hpp)
    std::string characters_folder;
    // Sounds (one-shots, can overlap) & musics (one at a time, crossfaded),
    // see the audio doc. The dialogs' `play` statements look in sounds_folder.
    std::string sounds_folder;
    std::string musics_folder;
    // Saved games, see GameState.hpp
    std::string saves_folder;

    // Defined in lua_include.hpp
    sol::state& lua() const;
    std::map<std::string, std::unique_ptr<Scene>> lua_scenes;
    std::unique_ptr<sol::state> _lua;

    std::vector<std::string> texts;

    // Mouse wheel of the main window, accumulated by its scroll callback
    // and reset every frame, after the scenes ran
    double scroll_y{ 0.0 };
    // Escape key of the main window, handled by the main loop
    bool escape_pressed{ false };
};
extern std::unique_ptr<GlobalData> g;
