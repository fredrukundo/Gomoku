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

    // Without this SDL ignores alpha entirely and treats every color as fully
    // opaque — the low-alpha stone shadow and the game-over dim both depend on
    // real blending.
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
    SDL_SetRenderDrawColor(renderer, 222, 184, 135, 255);
    SDL_RenderClear(renderer);
}

void Renderer::present() {
    SDL_RenderPresent(renderer);
}

void Renderer::drawGridLines() {
    SDL_SetRenderDrawColor(renderer, 60, 40, 20, 255);
    for (int i = 0; i < Board::SIZE; i++) {
        int pos = MARGIN + i * CELL_SIZE;
        SDL_RenderDrawLine(renderer, MARGIN, pos, MARGIN + BOARD_PIXELS, pos);
        SDL_RenderDrawLine(renderer, pos, MARGIN, pos, MARGIN + BOARD_PIXELS);
    }
}

void Renderer::drawStarPoints() {
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

void Renderer::drawFilledCircle(int centerX, int centerY, int radius, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int dy = -radius; dy <= radius; dy++) {
        int halfWidth = (int)std::sqrt((double)(radius * radius - dy * dy));
        SDL_RenderDrawLine(renderer, centerX - halfWidth, centerY + dy,
                                       centerX + halfWidth, centerY + dy);
    }
}

void Renderer::drawStones(const Board& board, Move lastMove) {
    static const int STONE_RADIUS = CELL_SIZE / 2 - 4;

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
                drawFilledCircle(cx, cy, STONE_RADIUS + 3, SDL_Color{220, 30, 30, 255});
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

void Renderer::drawHoverPreview(Move hoverMove, Player p, bool wouldBeLegal) {
    static const int STONE_RADIUS = CELL_SIZE / 2 - 4;
    int cx = MARGIN + hoverMove.x * CELL_SIZE;
    int cy = MARGIN + hoverMove.y * CELL_SIZE;

    SDL_Color base = (p == Player::Black) ? SDL_Color{20, 20, 20, 90} : SDL_Color{245, 245, 245, 90};
    SDL_Color tint = wouldBeLegal ? SDL_Color{40, 160, 40, 70} : SDL_Color{200, 40, 40, 90};

    drawFilledCircle(cx, cy, STONE_RADIUS, base);
    drawFilledCircle(cx, cy, STONE_RADIUS + 3, tint);
    drawFilledCircle(cx, cy, STONE_RADIUS, base);
}

void Renderer::drawSuggestion(Move m) {
    static const int STONE_RADIUS = CELL_SIZE / 2 - 4;
    int cx = MARGIN + m.x * CELL_SIZE;
    int cy = MARGIN + m.y * CELL_SIZE;

    SDL_Color cyan = { 40, 150, 220, 255 };
    SDL_Color boardBg = { 222, 184, 135, 255 };

    // Ring: a cyan disc with the board color punched back out of the middle,
    // leaving an annulus. The small center dot keeps it reading as a target
    // rather than an empty circle.
    drawFilledCircle(cx, cy, STONE_RADIUS + 3, cyan);
    drawFilledCircle(cx, cy, STONE_RADIUS, boardBg);
    drawFilledCircle(cx, cy, 3, cyan);
}

void Renderer::drawWinLine(const std::vector<Move>& line) {
    if (line.size() < 2) return;

    int x1 = MARGIN + line.front().x * CELL_SIZE;
    int y1 = MARGIN + line.front().y * CELL_SIZE;
    int x2 = MARGIN + line.back().x * CELL_SIZE;
    int y2 = MARGIN + line.back().y * CELL_SIZE;

    SDL_SetRenderDrawColor(renderer, 220, 30, 30, 220);
    for (int off = -1; off <= 1; off++) {
        SDL_RenderDrawLine(renderer, x1, y1 + off, x2, y2 + off);
        SDL_RenderDrawLine(renderer, x1 + off, y1, x2 + off, y2);
    }
}

void Renderer::drawSidePanel(const PanelInfo& info) {
    int panelX = MARGIN * 2 + BOARD_PIXELS;
    SDL_Color dark = { 40, 40, 40, 255 };
    SDL_Color divider = { 190, 160, 120, 255 };
    SDL_Color accent = { 40, 110, 60, 255 };
    SDL_Color turnColor = (info.currentPlayer == Player::Black)
        ? SDL_Color{20, 20, 20, 255} : SDL_Color{100, 100, 100, 255};

    auto hr = [&](int y) {
        SDL_SetRenderDrawColor(renderer, divider.r, divider.g, divider.b, divider.a);
        SDL_RenderDrawLine(renderer, panelX + 10, y, panelX + SIDE_PANEL_WIDTH - 20, y);
    };

    int y = 16;
    drawText(info.modeText, panelX + 10, y, accent, headerFont);
    y += 30;

    std::string turnText = std::string("Turn: ") +
        (info.currentPlayer == Player::Black ? "Black" : "White");
    if (info.thinking) turnText += " (thinking...)";
    drawText(turnText, panelX + 10, y, turnColor, headerFont);
    y += 34;

    hr(y);
    y += 16;

    drawText("Captures (10 to win)", panelX + 10, y, dark, headerFont);
    y += 26;
    drawText("Black: " + std::to_string(info.blackCaptured) + " / 10", panelX + 20, y, dark, bodyFont);
    y += 20;
    drawText("White: " + std::to_string(info.whiteCaptured) + " / 10", panelX + 20, y, dark, bodyFont);
    y += 30;

    hr(y);
    y += 16;

    drawText(info.aiSectionLabel, panelX + 10, y, dark, headerFont);
    y += 26;
    drawWrappedText(info.aiInfo, panelX + 20, y, SIDE_PANEL_WIDTH - 40, dark, bodyFont);
    y += 50;

    hr(y);
    y += 16;

    drawText("How to win", panelX + 10, y, dark, headerFont);
    y += 26;
    drawText("Get 5+ in a row", panelX + 20, y, dark, bodyFont);
    y += 19;
    drawText("(any direction)", panelX + 20, y, dark, bodyFont);
    y += 24;
    drawText("OR capture 10", panelX + 20, y, dark, bodyFont);
    y += 19;
    drawText("opponent stones", panelX + 20, y, dark, bodyFont);
    y += 30;

    hr(y);
    y += 16;

    drawText("Controls", panelX + 10, y, dark, headerFont);
    y += 26;
    drawText("M - switch mode", panelX + 20, y, dark, bodyFont);
    y += 19;
    drawText("S - suggest a move", panelX + 20, y, dark, bodyFont);
    y += 19;
    drawText("R - restart", panelX + 20, y, dark, bodyFont);
    y += 28;

    if (!info.statusMessage.empty()) {
        SDL_Color statusColor = { 180, 30, 30, 255 };
        drawWrappedText(info.statusMessage, panelX + 10, y, SIDE_PANEL_WIDTH - 30,
                         statusColor, headerFont);
    }
}

void Renderer::drawGameOverOverlay(const std::string& winnerText) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 140);
    SDL_Rect overlay = { 0, 0, MARGIN * 2 + BOARD_PIXELS, MARGIN * 2 + BOARD_PIXELS };
    SDL_RenderFillRect(renderer, &overlay);

    SDL_Color white = { 255, 255, 255, 255 };
    SDL_Color subtext = { 230, 230, 230, 255 };

    int centerX = (MARGIN * 2 + BOARD_PIXELS) / 2;
    int bannerY = (MARGIN * 2 + BOARD_PIXELS) / 2 - 40;

    int approxWidth = (int)winnerText.size() * 11;
    drawText(winnerText, centerX - approxWidth / 2, bannerY, white, headerFont);

    std::string prompt = "Click anywhere to play again";
    int approxPromptWidth = (int)prompt.size() * 9;
    drawText(prompt, centerX - approxPromptWidth / 2, bannerY + 40, subtext, bodyFont);
}