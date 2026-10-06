#pragma once

#include "includes.hpp"

#include <functional>
#include <optional>

namespace renpy { struct Signal; }

// Ren'Py dialog node, ported from the renpy_scene example of the
// Documentation: plays .rpy scripts (see src/renpy/RenpyParser.h) in a
// visual-novel UI drawn by its own UIRenderer, added to the main window over
// whatever the host scene draws: the characters, the dialogue box, the menu choices and its
// settings & log (see src/dialog/DialogNodes.hpp).
//
// The script talks to the host through signals (see setOnSignal()), and its
// variables are the game's (see GameState.hpp): they're kept between
// conversations and saved with the game.
//
// Idle until start(), it can play any number of conversations one after the
// other, and updates itself while one is playing. The UI is rebuilt when the window is resized, the story going on
// where it was.
//
//   Space / Enter / left click   finish the line, then next line (or pick the highlighted choice)
//   Backspace / Left arrow       previous line
//   Up / Down / 1-9 / mouse      select a menu choice
//   Tab / click auto icon        toggle auto mode
//   Keypad + / - / click speed   change the text speed
//   X / click log icon           open / close the log (Escape closes it too)
//   Mouse wheel / Up / Down      scroll the log (Page Up / Page Down: a page at a time)
//
// Bound to Lua as Dialog, see mylua.cpp & dialog.lua
class Dialog {
public:
    // Parts that can be hidden (their input is then ignored, but for the
    // settings' keyboard shortcuts)
    enum class Part { Characters, Box, Choices, Controls };

    Dialog();
    ~Dialog();

    Dialog(const Dialog&) = delete;
    Dialog(Dialog&&) = delete;
    Dialog& operator=(const Dialog&) = delete;
    Dialog& operator=(Dialog&&) = delete;

    // Plays a conversation, replacing the current one if any (and showing
    // the dialog if it was hidden). name: Ren'Py script of resources/dialogs/,
    // with or without its extension ("dial1" -> dial1.*), else a direct path
    // (absolute, or relative to the working directory). Only .rpy & .txt
    // files are accepted, others being warned about. Its images are relative
    // to resources/assets/. Throws if it can't be loaded, the current
    // conversation going on.
    void start(std::string const& name);
    // Ends the conversation and destroys its UI. Settings are kept.
    void stop();
    // Plays a frame of every started dialog (not hidden), called by the main
    // loop once the scenes ran. When a story is over, its dialog stops and
    // calls its finished callback.
    static void updateAll();
    // Reads resources/characters/*.json again (mouths & speech bubble tails),
    // for every started dialog
    static void reloadCharacterData();
    // Shows each character's mouth & its direction, as the speech bubble
    // tails use them (console: debug_mouths()). Turning it on reloads the files.
    static void setDebugMouths(bool on);
    static bool debugMouths() noexcept;

    // Stop / resume drawing: everything is kept as is (line, choices, log),
    // and the story waits while hidden.
    void hide();
    void show();

    // A conversation is loaded, hidden or not
    bool isActive() const noexcept;
    bool isHidden() const noexcept;
    // Active and not hidden: it takes the inputs
    bool isVisible() const noexcept { return isActive() && !isHidden(); }
    bool isLogOpen() const noexcept;

    void setPartShown(Part part, bool shown);
    bool isPartShown(Part part) const noexcept;

    // Shown on the story's End step (SSS::TR markup), until confirmed.
    // Empty (default): the dialog finishes as soon as the last line is.
    void setEndText(std::string text);
    std::string const& getEndText() const noexcept;

    // Called once a conversation is over, after stop()
    void setOnFinished(std::function<void()> callback);

    // Signals of the script, sent as the story reaches them. The dialog only
    // reports them: it draws no background and plays no sound, the host
    // interprets them (see SignalManager.hpp). The return value tells whether
    // the host handled the signal, it changes nothing for the dialog. Going
    // back sends the state signals again (the previous background & music),
    // not the one-shot ones.
    //   background  { "image path" } | { 0xRRGGBB } | {}   state: `scene`
    //   music       { "file", loop } | {}                   state: `play music`, `stop music`
    //   sound       { "file" } | {}                         one-shot: `play sound`, `stop sound`
    //   <name>      { args... }                             one-shot: `signal name args...`
    // The callback may stop the dialog, or start another conversation.
    using SignalCallback = std::function<bool(renpy::Signal const& signal)>;
    void setOnSignal(SignalCallback callback);

    // Background set by the current step's `scene`, for the host to draw:
    // an image file (absolute path), or a 0xRRGGBB color. Both empty when
    // the script set none, or when idle.
    std::string backgroundImage() const;
    std::optional<uint32_t> backgroundColor() const;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};
