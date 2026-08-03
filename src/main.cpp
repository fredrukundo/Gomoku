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
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }

        int freeThrees = board.countFreeThrees(Move{x, y}, current);
        if (freeThrees > 0)
            std::cout << "Free-three(s) created: " << freeThrees << "\n";

        // TEMPORARY debug — remove once you trust the output, keep while testing:
        static const int dirs[4][2] = { {1,0}, {0,1}, {1,1}, {1,-1} };
        for (const auto& d : dirs) {
            std::cout << "  window(" << d[0] << "," << d[1] << "): "
                    << board.extractLine(Move{x,y}, d[0], d[1], 4, current) << "\n";
        }

        int captured = board.checkAndApplyCaptures(Move{x, y}, current);
        if (captured > 0)
            std::cout << "Black captured: " << board.capturedBy(Player::Black)
          << " | White captured: " << board.capturedBy(Player::White) << "\n";

        Board::WinResult win = board.checkWinConditions(Move{x, y}, current);
        if (win.won) {
            board.print();
            std::cout << (win.winner == Player::Black ? "Black" : "White") << " wins by "
                    << (win.reason == Board::WinReason::Capture ? "capture" : "alignment") << "!\n";
            break;
        }

        current = (current == Player::Black) ? Player::White : Player::Black;
    }

    return 0;
}