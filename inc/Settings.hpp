#pragma once

#include "includes.hpp"

// Player settings, kept in settings.ini (home folder), loaded at startup:
//
//   [dialog]
//   tails = true              ; speech bubble tails from the speakers' mouths
//   grey_characters = true    ; characters not talking drawn in black & white
//
// A missing file or key keeps the default value; the file is written again
// with every value when one changes.
//
// Bound to Lua, see mylua.cpp: dialog_tails([on]), grey_characters([on])
namespace settings {

struct Values {
    bool dialog_tails = true;
    bool grey_characters = true;
};

Values const& get() noexcept;

void setDialogTails(bool on);
void setGreyCharacters(bool on);

// Return false on error (logged). load() writes the file when there is none.
bool load();
bool save();

} // namespace settings
