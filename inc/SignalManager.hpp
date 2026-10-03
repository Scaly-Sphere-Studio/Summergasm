#pragma once

#include "includes.hpp"

#include <functional>

namespace renpy { struct Signal; }

// Interprets the signals the dialog node reports from its Ren'Py script (see
// Dialog.hpp): the dialog draws and plays nothing itself, the handlers
// registered here do. Any part of the game can handle a signal, by name:
//
//   background  a scene draws it (dialog.lua registers one while it runs)
//   music       the audio Mixer crossfades to it, or fades out (default)
//   sound       the audio Mixer plays it (default)
//   <name>      whatever registers a handler, else a warning is logged
//
// Handlers are called from the latest registered to the first, until one
// returns true: a scene's handler overrides the default one while it exists.
// Bound to Lua as signals, see mylua.cpp:
//   id = signals.on("background", function (name, ...) ... return true end)
//   signals.off(id)
namespace signals {

using Handler = std::function<bool(renpy::Signal const&)>;

// name: the signal's, or "*" for every one. Returns an id for off().
uint32_t on(std::string name, Handler handler);
void off(uint32_t id);
// Removes every handler, defaults included (see installDefaults())
void clear();

// Calls the handlers of the signal. false if none handled it. This is what
// Dialog::setOnSignal() receives.
bool dispatch(renpy::Signal const& signal);

// The music & sound handlers, calling the audio Mixer. Done once at startup.
void installDefaults();

} // namespace signals
