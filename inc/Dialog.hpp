#pragma once

#include "includes.hpp"

// Ren'Py dialog player, ported from the renpy_scene example of the
// Documentation: parses a .rpy script (see src/renpy/RenpyParser.h) and plays
// it in a visual-novel UI drawn by its own UIRenderer, added to the main window.
// The UI is rebuilt when the window is resized, the story going on where it was.
//
//   Space / Enter / left click   finish the line, then next line (or pick the highlighted choice)
//   Backspace / Left arrow       previous line
//   Up / Down / 1-9 / mouse      select a menu choice
//   Tab / click auto icon        toggle auto mode
//   Keypad + / - / click speed   change the text speed
//   X / click log icon           open / close the log (Escape closes it too)
//   Mouse wheel / Up / Down      scroll the log (Page Up / Page Down: a page at a time)
//
// Bound to Lua as Dialog (see dialog.lua): Dialog.new(path), dialog:update(), dialog.log_open
class Dialog {
public:
    // path: .rpy script, relative to the assets folder. Throws if it can't be loaded.
    explicit Dialog(std::string const& path);
    ~Dialog();

    Dialog(const Dialog&) = delete;
    Dialog(Dialog&&) = delete;
    Dialog& operator=(const Dialog&) = delete;
    Dialog& operator=(Dialog&&) = delete;

    // Plays a frame. Returns true once the story is over and the player
    // confirmed the end screen (time to leave the scene).
    bool update();
    bool isLogOpen() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};
