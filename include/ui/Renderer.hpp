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
    bool init(const std::string& fontPath, const std::string& boldFontPath);

    // Goal: draws every stone currently on 'board'. 'lastMove', if valid (x >= 0),
    // gets a small ring drawn around it so the player can immediately spot the
    // most recent move on a busy board — a genuine beginner-friendliness aid, not
    // just decoration.
    void drawStones(const Board& board, Move lastMove);

    // Goal: renders the right-side info panel: whose turn it is, each player's
    // capture count out of the 10 needed to win by capture, and a status/message
    // line (illegal move explanations, win announcements, etc.) — all in plain
    // language, since a beginner won't know terms like "double-three" without
    // them being spelled out when relevant.
    void drawSidePanel(Player currentPlayer, int blackCaptured, int whiteCaptured,
                    const std::string& statusMessage, const std::string& aiInfo, bool aiThinking);
    
    // Goal: wraps 'text' to fit within maxWidth pixels, breaking on word
    // boundaries, and draws each resulting line. Needed because status messages
    // (illegal-move reasons, win announcements) vary in length and the side panel
    // has a fixed width — a single un-wrapped line can overflow past the window
    // edge, as seen with longer messages like the double-three explanation.
    void drawWrappedText(const std::string& text, int x, int y, int maxWidth,
                        SDL_Color color, TTF_Font* font, int lineSpacing = 20);
    
    // Goal: draws a highlighted line connecting the winning stones, so the win is
    // visually obvious at a glance rather than only stated in text. 'line' comes
    // directly from Board::findWinningLine — already ordered spatially, so we can
    // just draw a line from its first cell to its last.
    void drawWinLine(const std::vector<Move>& line);

    // Goal: dims the board and draws a centered banner announcing the winner and
    // how they won, plus a prompt to restart — makes "the game has ended" visually
    // unmistakable instead of relying on a small side-panel message someone could
    // miss, especially important for a beginner who might not be sure what just
    // happened.
    void drawGameOverOverlay(const std::string& winnerText);

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
    TTF_Font* bodyFont = nullptr;
    TTF_Font* headerFont = nullptr;

    void drawGridLines();
    void drawStarPoints();
    void drawCoordinateLabels();
    void drawText(const std::string& text, int x, int y, SDL_Color color, TTF_Font* font);

    // Goal: self-contained filled-circle drawing (replaces SDL2_gfx, which
    // isn't available on this system). Draws horizontal scanlines across the
    // circle's width at each row — simple, correct, and fast enough for 361
    // cells redrawn every frame.
    void drawFilledCircle(int centerX, int centerY, int radius, SDL_Color color);
};