#include "engine/Board.hpp"
#include <iostream>

Board::Board() {
    for (int y = 0; y < SIZE; y++)
        for (int x = 0; x < SIZE; x++)
            grid[y][x] = Cell::Empty;
}

bool Board::isInBounds(int x, int y) const {
    return x >= 0 && x < SIZE && y >= 0 && y < SIZE;
}

bool Board::isEmpty(int x, int y) const {
    return isInBounds(x, y) && grid[y][x] == Cell::Empty;
}

Cell Board::get(int x, int y) const {
    if (!isInBounds(x, y))
        return Cell::Empty; // never index out of bounds
    return grid[y][x];
}

bool Board::placeStone(Move m, Player p) {
    if (!isEmpty(m.x, m.y))
        return false;
    grid[m.y][m.x] = (p == Player::Black) ? Cell::Black : Cell::White;
    return true;
}

bool Board::checkWin(Move last, Player p) const {
    // 4 axes: horizontal, vertical, diagonal down-right, diagonal up-right
    static const int dirs[4][2] = { {1, 0}, {0, 1}, {1, 1}, {1, -1} };
    Cell target = (p == Player::Black) ? Cell::Black : Cell::White;

    for (const auto& d : dirs) {
        int dx = d[0], dy = d[1];
        int count = 1; // the stone just placed

        int x = last.x + dx, y = last.y + dy;
        while (isInBounds(x, y) && grid[y][x] == target) {
            count++;
            x += dx;
            y += dy;
        }

        x = last.x - dx;
        y = last.y - dy;
        while (isInBounds(x, y) && grid[y][x] == target) {
            count++;
            x -= dx;
            y -= dy;
        }

        if (count >= 5)
            return true;
    }
    return false;
}

int Board::checkAndApplyCaptures(Move last, Player p) {
    static const int dirs8[8][2] = {
        {1,0}, {-1,0}, {0,1}, {0,-1},
        {1,1}, {-1,-1}, {1,-1}, {-1,1}
    };

    Cell mine = (p == Player::Black) ? Cell::Black : Cell::White;
    Cell opp  = (p == Player::Black) ? Cell::White : Cell::Black;
    int capturedCount = 0;

    for (const auto& d : dirs8) {
        int dx = d[0], dy = d[1];
        int x1 = last.x + dx,     y1 = last.y + dy;
        int x2 = last.x + 2 * dx, y2 = last.y + 2 * dy;
        int x3 = last.x + 3 * dx, y3 = last.y + 3 * dy;

        if (isInBounds(x1, y1) && isInBounds(x2, y2) && isInBounds(x3, y3)
            && grid[y1][x1] == opp && grid[y2][x2] == opp && grid[y3][x3] == mine) {
            grid[y1][x1] = Cell::Empty;
            grid[y2][x2] = Cell::Empty;
            capturedCount += 2;
        }
    }

    if (p == Player::Black) capturedByBlack += capturedCount;
    else capturedByWhite += capturedCount;

    return capturedCount;
}

int Board::capturedBy(Player p) const {
    return (p == Player::Black) ? capturedByBlack : capturedByWhite;
}

void Board::print() const {
    for (int y = 0; y < SIZE; y++) {
        for (int x = 0; x < SIZE; x++) {
            char c = '.';
            if (grid[y][x] == Cell::Black) c = 'X';
            else if (grid[y][x] == Cell::White) c = 'O';
            std::cout << c << ' ';
        }
        std::cout << '\n';
    }
}