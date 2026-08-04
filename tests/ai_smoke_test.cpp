#include "engine/Board.hpp"
#include "ai/Minimax.hpp"
#include <iostream>
#include <chrono>

void runTwice(const std::string& label, Board& board, Player aiPlayer) {
    Minimax ai(1);

    for (int run = 1; run <= 2; run++) {
        ai.resetTTStats();
        int depthReached = 0;
        auto start = std::chrono::steady_clock::now();
        SearchResult result = ai.findBestMoveTimed(board, aiPlayer, 400.0, depthReached);
        auto end = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        std::cout << label << " run " << run << ": move (" << result.bestMove.x << ","
                  << result.bestMove.y << ") score=" << result.score
                  << " depth=" << depthReached << " time=" << ms << " ms\n";
        std::cout << "  TT: hits=" << ai.getTTHits() << " misses=" << ai.getTTMisses()
                  << " stores=" << ai.getTTStores() << " tableSize=" << ai.getTTSize() << "\n";
    }
}

int main() {
    Board sparse;
    sparse.setRaw(5, 5, Cell::Black);
    sparse.setRaw(6, 5, Cell::Black);
    sparse.setRaw(7, 5, Cell::Black);
    runTwice("Sparse", sparse, Player::White);

    std::cout << "\n";

    Board busy;
    busy.setRaw(5, 5, Cell::Black);
    busy.setRaw(6, 5, Cell::Black);
    busy.setRaw(7, 5, Cell::Black);
    busy.setRaw(9, 9, Cell::White);
    busy.setRaw(10, 10, Cell::Black);
    busy.setRaw(8, 11, Cell::White);
    busy.setRaw(11, 9, Cell::Black);
    busy.setRaw(9, 12, Cell::White);
    busy.setRaw(12, 8, Cell::Black);
    busy.setRaw(4, 4, Cell::White);
    busy.setRaw(3, 6, Cell::Black);
    busy.setRaw(6, 3, Cell::White);
    busy.setRaw(13, 13, Cell::Black);
    runTwice("Busy", busy, Player::White);

    return 0;
}