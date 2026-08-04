#include "ui/Renderer.hpp"
#include <iostream>
#include <cmath>
#include <sstream>

Renderer::Renderer() {}

Renderer::~Renderer() {
    if (headerFont) TTF_CloseFont(headerFont);
    if (bodyFont) TTF_CloseFont(bodyFont);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
}

bool Renderer::init(const std::string& fontPath, const std::string& boldFontPath) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return false;
    }
    if (TTF_Init() != 0) {
        std::cerr << "TTF_Init failed: " << SDL_GetError() << "\n";
        return false;
    }

    window = SDL_CreateWindow("Gomoku", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << "\n";
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "Renderer creation failed: " << SDL_GetError() << "\n";
        return false;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    bodyFont = TTF_OpenFont(fontPath.c_str(), 15);
    if (!bodyFont) {
        std::cerr << "Body font load failed: " << SDL_GetError() << "\n";
        return false;
    }

    headerFont = TTF_OpenFont(boldFontPath.c_str(), 17);
    if (!headerFont) {
        std::cerr << "Header font load failed: " << SDL_GetError() << "\n";
        return false;
    }

    return true;
}

void Renderer::clear() {
    // Warm wood tone, matching a traditional board rather than a flat generic gray.
    SDL_SetRenderDrawColor(renderer, 222, 184, 135, 255);
    SDL_RenderClear(renderer);
}

void Renderer::drawGridLines() {
    SDL_SetRenderDrawColor(renderer, 60, 40, 20, 255);
    for (int i = 0; i < Board::SIZE; i++) {
        int pos = MARGIN + i * CELL_SIZE;
        SDL_RenderDrawLine(renderer, MARGIN, pos, MARGIN + BOARD_PIXELS, pos); // horizontal
        SDL_RenderDrawLine(renderer, pos, MARGIN, pos, MARGIN + BOARD_PIXELS); // vertical
    }
}

void Renderer::drawStarPoints() {
    // Goal: traditional decorative + orientation dots at the 4th/10th/16th lines
    // (index 3, 9, 15 in 0-based coordinates) — both authentic board styling and
    // a genuine aid for a beginner judging distance across the board at a glance.
    static const int points[3] = {3, 9, 15};
    SDL_SetRenderDrawColor(renderer, 60, 40, 20, 255);
    for (int yi : points) {
        for (int xi : points) {
            int cx = MARGIN + xi * CELL_SIZE;
            int cy = MARGIN + yi * CELL_SIZE;
            SDL_Rect dot = { cx - 3, cy - 3, 6, 6 };
            SDL_RenderFillRect(renderer, &dot);
        }
    }
}

void Renderer::drawText(const std::string& text, int x, int y, SDL_Color color, TTF_Font* font) {
    SDL_Surface* surf = TTF_RenderText_Blended(font, text.c_str(), color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_Rect dst = { x, y, surf->w, surf->h };
    SDL_RenderCopy(renderer, tex, nullptr, &dst);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

void Renderer::drawCoordinateLabels() {
    SDL_Color color = { 60, 40, 20, 255 };
    const std::string cols = "ABCDEFGHJKLMNOPQRST";

    for (int x = 0; x < Board::SIZE; x++) {
        std::string label(1, cols[x]);
        drawText(label, MARGIN + x * CELL_SIZE - 4, MARGIN - 28, color, bodyFont);
    }
    for (int y = 0; y < Board::SIZE; y++) {
        std::string label = std::to_string(y + 1);
        drawText(label, MARGIN - 32, MARGIN + y * CELL_SIZE - 7, color, bodyFont);
    }
}

void Renderer::drawBoard() {
    drawGridLines();
    drawStarPoints();
    drawCoordinateLabels();
}

void Renderer::present() {
    SDL_RenderPresent(renderer);
}

void Renderer::drawFilledCircle(int centerX, int centerY, int radius, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int dy = -radius; dy <= radius; dy++) {
        int halfWidth = (int)std::sqrt((double)(radius * radius - dy * dy));
        SDL_RenderDrawLine(renderer, centerX - halfWidth, centerY + dy,
                                       centerX + halfWidth, centerY + dy);
    }
}

void Renderer::drawStones(const Board& board, Move lastMove) {
    static const int STONE_RADIUS = CELL_SIZE / 2 - 5; // was -3; more breathing room between neighbors

    for (int y = 0; y < Board::SIZE; y++) {
        for (int x = 0; x < Board::SIZE; x++) {
            Cell c = board.get(x, y);
            if (c == Cell::Empty) continue;

            int cx = MARGIN + x * CELL_SIZE;
            int cy = MARGIN + y * CELL_SIZE;

            SDL_Color shadow = { 0, 0, 0, 60 };
            drawFilledCircle(cx + 2, cy + 2, STONE_RADIUS, shadow);

            if (c == Cell::Black) {
                drawFilledCircle(cx, cy, STONE_RADIUS, SDL_Color{20, 20, 20, 255});
            } else {
                drawFilledCircle(cx, cy, STONE_RADIUS, SDL_Color{80, 80, 80, 255});
                drawFilledCircle(cx, cy, STONE_RADIUS - 2, SDL_Color{245, 245, 245, 255});
            }

            if (x == lastMove.x && y == lastMove.y) {
                // Outer ring radius (STONE_RADIUS + 3) must stay under
                // CELL_SIZE/2 (16px) so it never reaches into a neighboring
                // cell's space, even when the last move is directly adjacent
                // to another stone.
                SDL_Color highlight = { 220, 30, 30, 255 };
                drawFilledCircle(cx, cy, STONE_RADIUS + 3, highlight);
                if (c == Cell::Black)
                    drawFilledCircle(cx, cy, STONE_RADIUS, SDL_Color{20, 20, 20, 255});
                else {
                    drawFilledCircle(cx, cy, STONE_RADIUS, SDL_Color{80, 80, 80, 255});
                    drawFilledCircle(cx, cy, STONE_RADIUS - 2, SDL_Color{245, 245, 245, 255});
                }
            }
        }
    }
}
void Renderer::drawWrappedText(const std::string& text, int x, int y, int maxWidth,
                                SDL_Color color, TTF_Font* font, int lineSpacing) {
    std::istringstream words(text);
    std::string word, line;
    int curY = y;

    while (words >> word) {
        std::string testLine = line.empty() ? word : line + " " + word;
        int w, h;
        TTF_SizeText(font, testLine.c_str(), &w, &h);

        if (w > maxWidth && !line.empty()) {
            drawText(line, x, curY, color, font);
            curY += lineSpacing;
            line = word;
        } else {
            line = testLine;
        }
    }
    if (!line.empty())
        drawText(line, x, curY, color, font);
}
void Renderer::drawSidePanel(Player currentPlayer, int blackCaptured, int whiteCaptured,
                              const std::string& statusMessage, const std::string& aiInfo,
                              bool aiThinking) {
    int panelX = MARGIN * 2 + BOARD_PIXELS;
    SDL_Color dark = { 40, 40, 40, 255 };
    SDL_Color divider = { 190, 160, 120, 255 };
    SDL_Color turnColor = (currentPlayer == Player::Black)
        ? SDL_Color{20, 20, 20, 255} : SDL_Color{100, 100, 100, 255};

    int y = 20;
    std::string turnText = std::string("Turn: ") +
        (currentPlayer == Player::Black ? "Black" : "White");
    if (aiThinking) turnText += " (thinking...)";
    drawText(turnText, panelX + 10, y, turnColor, headerFont);
    y += 40;

    SDL_SetRenderDrawColor(renderer, divider.r, divider.g, divider.b, divider.a);
    SDL_RenderDrawLine(renderer, panelX + 10, y, panelX + SIDE_PANEL_WIDTH - 20, y);
    y += 20;

    drawText("Captures (10 to win)", panelX + 10, y, dark, headerFont);
    y += 28;
    drawText("Black: " + std::to_string(blackCaptured) + " / 10", panelX + 20, y, dark, bodyFont);
    y += 22;
    drawText("White: " + std::to_string(whiteCaptured) + " / 10", panelX + 20, y, dark, bodyFont);
    y += 35;

    SDL_SetRenderDrawColor(renderer, divider.r, divider.g, divider.b, divider.a);
    SDL_RenderDrawLine(renderer, panelX + 10, y, panelX + SIDE_PANEL_WIDTH - 20, y);
    y += 20;

    drawText("AI last move", panelX + 10, y, dark, headerFont);
    y += 28;
    drawWrappedText(aiInfo, panelX + 20, y, SIDE_PANEL_WIDTH - 40, dark, bodyFont);
    y += 55;

    SDL_SetRenderDrawColor(renderer, divider.r, divider.g, divider.b, divider.a);
    SDL_RenderDrawLine(renderer, panelX + 10, y, panelX + SIDE_PANEL_WIDTH - 20, y);
    y += 20;

    drawText("How to win", panelX + 10, y, dark, headerFont);
    y += 28;
    drawText("Get 5+ in a row", panelX + 20, y, dark, bodyFont);
    y += 20;
    drawText("(any direction)", panelX + 20, y, dark, bodyFont);
    y += 28;
    drawText("OR capture 10", panelX + 20, y, dark, bodyFont);
    y += 20;
    drawText("opponent stones", panelX + 20, y, dark, bodyFont);
    y += 20;
    drawText("(in pairs)", panelX + 20, y, dark, bodyFont);
    y += 40;

    if (!statusMessage.empty()) {
        SDL_Color statusColor = { 180, 30, 30, 255 };
        drawWrappedText(statusMessage, panelX + 10, y, SIDE_PANEL_WIDTH - 30, statusColor, headerFont);
    }
}

void Renderer::drawWinLine(const std::vector<Move>& line) {
    if (line.size() < 2) return;

    int x1 = MARGIN + line.front().x * CELL_SIZE;
    int y1 = MARGIN + line.front().y * CELL_SIZE;
    int x2 = MARGIN + line.back().x * CELL_SIZE;
    int y2 = MARGIN + line.back().y * CELL_SIZE;

    SDL_SetRenderDrawColor(renderer, 220, 30, 30, 220);
    // A few offset parallel lines approximate a thicker stroke — plain SDL2
    // only draws 1px lines natively, and a single thin line is easy to miss
    // against the stones it's meant to be highlighting.
    for (int off = -1; off <= 1; off++) {
        SDL_RenderDrawLine(renderer, x1, y1 + off, x2, y2 + off);
        SDL_RenderDrawLine(renderer, x1 + off, y1, x2 + off, y2);
    }
}

void Renderer::drawGameOverOverlay(const std::string& winnerText) {
    // Dim the whole board area — relies on the SDL_BLENDMODE_BLEND we already
    // enabled for the stone shadows, so this semi-transparent black actually
    // blends instead of covering everything solidly.
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 140);
    SDL_Rect overlay = { 0, 0, MARGIN * 2 + BOARD_PIXELS, MARGIN * 2 + BOARD_PIXELS };
    SDL_RenderFillRect(renderer, &overlay);

    SDL_Color white = { 255, 255, 255, 255 };
    SDL_Color subtext = { 230, 230, 230, 255 };

    int centerX = (MARGIN * 2 + BOARD_PIXELS) / 2;
    int bannerY = (MARGIN * 2 + BOARD_PIXELS) / 2 - 40;

    // drawText positions from the top-left corner, so we roughly center by
    // estimating text width from character count — not pixel-perfect, but
    // close enough for a banner and avoids needing TTF_SizeText here too.
    int approxWidth = (int)winnerText.size() * 11;
    drawText(winnerText, centerX - approxWidth / 2, bannerY, white, headerFont);

    std::string prompt = "Click anywhere to play again";
    int approxPromptWidth = (int)prompt.size() * 9;
    drawText(prompt, centerX - approxPromptWidth / 2, bannerY + 40, subtext, bodyFont);
}
