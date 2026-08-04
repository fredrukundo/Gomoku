#pragma once
#include "engine/Board.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>

// Goal: owns every SDL resource (window, renderer, fonts) and all drawing logic.
// Nothing outside this class touches SDL directly — main.cpp only calls these
// high-level methods, which keeps the GUI swappable/testable in isolation from
// the game logic.
class Renderer {
public:
    Renderer();
    ~Renderer(); // Goal: guarantees SDL_DestroyWindow/Renderer/Font run even on
                 // early exit paths, so resources are never leaked — relevant to
                 // the subject's "must never crash / never quit unexpectedly" rule,
                 // since a resource leak on repeated games could eventually cause
                 // exactly that.

    // Goal: creates the window, renderer, and loads fonts. Returns false on any
    // failure so main.cpp can print an error and exit cleanly, instead of the
    // program crashing on a null pointer deref deeper in the code.
    bool init(const std::string& fontPath);

    void clear();
    void drawBoard();
    void present();

    static const int CELL_SIZE = 32;
    static const int MARGIN = 50;
    static const int SIDE_PANEL_WIDTH = 240;
    static const int BOARD_PIXELS = CELL_SIZE * (Board::SIZE - 1);
    static const int WINDOW_WIDTH = MARGIN * 2 + BOARD_PIXELS + SIDE_PANEL_WIDTH;
    static const int WINDOW_HEIGHT = MARGIN * 2 + BOARD_PIXELS;

private:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    TTF_Font* labelFont = nullptr;

    void drawGridLines();
    void drawStarPoints();
    void drawCoordinateLabels();
    void drawText(const std::string& text, int x, int y, SDL_Color color);
};