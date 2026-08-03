#pragma once
#include "common/Types.hpp"

class Board {
public:
    static const int SIZE = 19;

    Board();

    bool isInBounds(int x, int y) const;
    bool isEmpty(int x, int y) const;
    Cell get(int x, int y) const;

    // returns false (and does nothing) if the move is invalid at this stage
    bool placeStone(Move m, Player p);

    void print() const;

private:
    Cell grid[SIZE][SIZE];
};