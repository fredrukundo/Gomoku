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