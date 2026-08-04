#include "engine/Board.hpp"
#include "ai/Minimax.hpp"
#include <iostream>
#include <chrono>

int main() {
    Board board;
    board.setRaw(5, 5, Cell::Black);
    board.setRaw(6, 5, Cell::Black);
    board.setRaw(7, 5, Cell::Black);
    board.setRaw(9, 9, Cell::White);
    board.setRaw(10, 10, Cell::Black);
    board.setRaw(8, 11, Cell::White);
    board.setRaw(11, 9, Cell::Black);
    board.setRaw(9, 12, Cell::White);
    board.setRaw(12, 8, Cell::Black);
    board.setRaw(4, 4, Cell::White);
    board.setRaw(3, 6, Cell::Black);
    board.setRaw(6, 3, Cell::White);
    board.setRaw(13, 13, Cell::Black);

    Minimax ai(1);
    int depthReached = 0;

    auto start = std::chrono::steady_clock::now();
    SearchResult result = ai.findBestMoveTimed(board, Player::White, 400.0, depthReached);
    auto end = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "Move: (" << result.bestMove.x << "," << result.bestMove.y << ")\n";
    std::cout << "Score: " << result.score << "\n";
    std::cout << "Depth reached: " << depthReached << "\n";
    std::cout << "Total time: " << ms << " ms\n";
    return 0;
}