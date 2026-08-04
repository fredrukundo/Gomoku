#include "engine/Board.hpp"
#include "ai/Minimax.hpp"
#include <iostream>
#include <chrono>

int main() {
    Board board;
    board.setRaw(5, 5, Cell::Black);
    board.setRaw(6, 5, Cell::Black);
    board.setRaw(7, 5, Cell::Black);

    for (int depth : {2, 3}) {
        Minimax ai(depth);
        auto start = std::chrono::steady_clock::now();
        SearchResult result = ai.findBestMove(board, Player::White);
        auto end = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        std::cout << "Depth " << depth << " -> move (" << result.bestMove.x << "," << result.bestMove.y
                  << ") score=" << result.score << " time=" << ms << " ms\n";
    }
    return 0;
}