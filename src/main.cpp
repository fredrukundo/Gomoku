#include "engine/Board.hpp"
#include <iostream>
#include <limits>

int main() {
    Board board;
    Player current = Player::Black;

    while (true) {
        board.print();
        std::cout << (current == Player::Black ? "Black" : "White") << " to move (x y): ";

        int x, y;
        if (!(std::cin >> x >> y)) {
            std::cout << "Invalid input, try again.\n";
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }

        if (!board.placeStone(Move{x, y}, current)) {
            std::cout << "Illegal move, try again.\n";
            continue;
        }

        current = (current == Player::Black) ? Player::White : Player::Black;
    }

    return 0;
}