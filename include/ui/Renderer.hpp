#pragma once
#include "engine/Board.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>

// Goal: bundles everything the side panel displays. Replaces what had grown
// to a six-argument call about to become eight — a struct keeps the single
// call site readable and lets fields be added later without touching the
// signature again.
struct PanelInfo {
    Player currentPlayer = Player::Black;
    int blackCaptured = 0;
    int whiteCaptured = 0;
    std::string statusMessage;
    std::string aiInfo;
    std::string aiSectionLabel = "AI last move";
    std::string modeText = "Human vs AI";
    bool thinking = false;
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool init(const std::string& fontPath, const std::string& boldFontPath);

    void clear();
    void drawBoard();
    void present();

    void drawStones(const Board& board, Move lastMove);
    void drawHoverPreview(Move hoverMove, Player p, bool wouldBeLegal);
    void drawWinLine(const std::vector<Move>& line);
    void drawSidePanel(const PanelInfo& info);
    void drawGameOverOverlay(const std::string& winnerText);

    // Goal: marks the cell the engine recommends, using a cyan target ring
    // deliberately unlike the red last-move ring and the faint hover stone —
    // three different markers can be on screen at once, so they must be
    // instantly distinguishable.
    void drawSuggestion(Move m);

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
    void drawWrappedText(const std::string& text, int x, int y, int maxWidth,
                          SDL_Color color, TTF_Font* font, int lineSpacing = 20);
    void drawFilledCircle(int centerX, int centerY, int radius, SDL_Color color);
};