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