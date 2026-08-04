#include "ui/Renderer.hpp"
#include <iostream>

Renderer::Renderer() {}

Renderer::~Renderer() {
    if (labelFont) TTF_CloseFont(labelFont);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
}

bool Renderer::init(const std::string& fontPath) {
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

    labelFont = TTF_OpenFont(fontPath.c_str(), 14);
    if (!labelFont) {
        std::cerr << "Font load failed: " << SDL_GetError() << "\n";
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

void Renderer::drawText(const std::string& text, int x, int y, SDL_Color color) {
    SDL_Surface* surf = TTF_RenderText_Blended(labelFont, text.c_str(), color);
    if (!surf) return; // fail quietly — a missing label must never crash the game
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_Rect dst = { x, y, surf->w, surf->h };
    SDL_RenderCopy(renderer, tex, nullptr, &dst);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

void Renderer::drawCoordinateLabels() {
    SDL_Color color = { 60, 40, 20, 255 };
    // Traditional Go/Gomoku column labels skip 'I' (historically avoids confusion
    // with the digit 1) — following that convention keeps this recognizable to
    // anyone who already knows Go, while still being a clean A-S range.
    const std::string cols = "ABCDEFGHJKLMNOPQRST";

    for (int x = 0; x < Board::SIZE; x++) {
        std::string label(1, cols[x]);
        drawText(label, MARGIN + x * CELL_SIZE - 4, MARGIN - 28, color);
    }
    for (int y = 0; y < Board::SIZE; y++) {
        std::string label = std::to_string(y + 1);
        drawText(label, MARGIN - 32, MARGIN + y * CELL_SIZE - 7, color);
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