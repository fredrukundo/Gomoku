#include "engine/Board.hpp"
#include "ai/Minimax.hpp"
#include <iostream>
#include <chrono>

static const char* COLS = "ABCDEFGHJKLMNOPQRST";

void showReasoning(const std::string& label, Board& board, Player aiPlayer) {
    Minimax ai(1);
    int depthReached = 0;

    auto start = std::chrono::steady_clock::now();
    SearchResult result = ai.findBestMoveTimed(board, aiPlayer, 400.0, depthReached);
    auto end = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << label << ": chose " << COLS[result.bestMove.x] << (result.bestMove.y + 1)
              << "  depth=" << depthReached << "  time=" << (int)ms << " ms\n";
    std::cout << "  candidates considered (best first):\n";

    int rank = 1;
    for (const auto& sm : ai.getRootScores()) {
        std::cout << "    " << rank++ << ". " << COLS[sm.move.x] << (sm.move.y + 1)
                  << "  score=" << sm.score << "\n";
    }
    std::cout << "\n";
}

int main() {
    // Black has an open three at F6-H6. White should rank the two blocking
    // moves (E6 and J6) at the top.
    Board blockTest;
    blockTest.setRaw(5, 5, Cell::Black);
    blockTest.setRaw(6, 5, Cell::Black);
    blockTest.setRaw(7, 5, Cell::Black);
    showReasoning("Block an open three", blockTest, Player::White);

    // White has four in a row with both ends open. Completing it should
    // score in the 100000+ range, far above everything else.
    Board winTest;
    winTest.setRaw(5, 8, Cell::White);
    winTest.setRaw(6, 8, Cell::White);
    winTest.setRaw(7, 8, Cell::White);
    winTest.setRaw(8, 8, Cell::White);
    winTest.setRaw(2, 2, Cell::Black);
    winTest.setRaw(3, 3, Cell::Black);
    showReasoning("Take the win", winTest, Player::White);

    return 0;
}