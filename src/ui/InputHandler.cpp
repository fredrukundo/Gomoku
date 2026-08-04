#include "ui/InputHandler.hpp"
#include <cmath>

bool InputHandler::pixelToBoardCoord(int pixelX, int pixelY, Move& outMove) {
    // Goal: find the nearest grid intersection, then reject the click if it's
    // too far from that intersection to plausibly have been aimed at it —
    // without this, a click anywhere in the vague vicinity of the board would
    // silently snap to the nearest line, which feels imprecise and error-prone
    // to a beginner still learning to aim clicks on a small 32px grid.
    static const int CLICK_TOLERANCE = Renderer::CELL_SIZE / 2 - 4;

    int gx = (int)std::round((pixelX - Renderer::MARGIN) / (double)Renderer::CELL_SIZE);
    int gy = (int)std::round((pixelY - Renderer::MARGIN) / (double)Renderer::CELL_SIZE);

    if (gx < 0 || gx >= Board::SIZE || gy < 0 || gy >= Board::SIZE)
        return false;

    int nearestPixelX = Renderer::MARGIN + gx * Renderer::CELL_SIZE;
    int nearestPixelY = Renderer::MARGIN + gy * Renderer::CELL_SIZE;
    int dx = pixelX - nearestPixelX;
    int dy = pixelY - nearestPixelY;

    if (dx * dx + dy * dy > CLICK_TOLERANCE * CLICK_TOLERANCE)
        return false;

    outMove = { gx, gy };
    return true;
}