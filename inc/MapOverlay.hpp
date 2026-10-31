#pragma once

#include "includes.hpp"

// City map shown over whatever the current scene draws, like the dialog: a
// FoldingMap in its own SceneRenderer, added to the main window over the
// scene's renderers. Kept 2D: orthographic camera straight on, unlit, with a
// faux perspective pinching the panels folding away (see FoldingMap::pinch).
// The map is turned 180 degrees in its plane: its blue cover (the front's
// bottom left panel) ends at the top right. The camera doesn't move, the
// whole map fitting the screen. Opening, the folded map shows the cover
// centered on screen, scaled up, the crease on top, then unfolds while
// easing to its place & size: the cover flips up like a card's (going
// behind), then the accordion slides open to the left, flat as against a
// board, the map's right edge staying in place. Closing folds everything
// back at once, the map shrinking & sliding out of the screen toward the
// bottom right, then removes the renderer. That full opening is played the
// first time only: opening again plays closing backward.
// The scenes are blurred behind it (see SceneRenderer::dialog_blur).
// Roads, monuments & characters' pins are printed on it (see MapContent.hpp).
// Open, flat & still, its monuments are hovered & clicked: a click closes it
// and calls map_overlay.on_location_clicked(id, scene) (see global_setup.lua).
//
//   M         open (not while a dialog is shown) / close
//   Escape    close
//
// A dialog showing up closes it. Bound to Lua as map_overlay, see mylua.cpp.
class MapOverlay {
public:
    // Opens / closes the map, or reverses its animation if it was closing /
    // opening. Opening is ignored while a dialog is shown.
    static void open();
    static void close();
    static void toggle();
    // Removes it at once, without folding it back
    static void hide();
    // Opening, open or closing: drawn
    static bool isShown() noexcept;
    // Opening or open: blurs the scenes behind it
    static bool isOpen() noexcept;

    // Inputs & animation, called by the main loop once the dialogs updated
    static void update();
    // Releases its GL objects, before the window is closed
    static void free();
};
