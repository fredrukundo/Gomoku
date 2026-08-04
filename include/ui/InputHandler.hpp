#pragma once
#include "engine/Board.hpp"
#include "ui/Renderer.hpp"

// Goal: converts a raw mouse-click pixel position into board coordinates, using
// the exact inverse of Renderer's MARGIN/CELL_SIZE placement math — must stay
// in sync with Renderer's constants, since a click has to land on the SAME
// intersection the stone was actually drawn at.
class InputHandler {
public:
    // Goal: returns true and fills 'outMove' if (pixelX, pixelY) is close
    // enough to a valid board intersection to count as a click on it; false if
    // the click was elsewhere (side panel, margins, between intersections).
    static bool pixelToBoardCoord(int pixelX, int pixelY, Move& outMove);
};