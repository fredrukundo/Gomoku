#include "engine/Board.hpp"
#include "ai/Minimax.hpp"
#include <iostream>

static const char* COLS = "ABCDEFGHJKLMNOPQRST";

static std::string coord(Move m) {
    return std::string(1, COLS[m.x]) + std::to_string(m.y + 1);
}

int main() {
    // White has four stones in a plus shape. Playing the centre (10,10)
    // would create two open threes at once — a double-three, illegal.
    Board board;
    board.setRaw(9,  10, Cell::White);
    board.setRaw(11, 10, Cell::White);
    board.setRaw(10, 9,  Cell::White);
    board.setRaw(10, 11, Cell::White);

    // A few Black stones so the position isn't artificial.
    board.setRaw(6, 6, Cell::Black);
    board.setRaw(7, 7, Cell::Black);
    board.setRaw(8, 8, Cell::Black);

    Move centre{10, 10};

    // 1. Confirm the rule itself works.
    Board::MoveEvaluation eval = board.evaluateMove(centre, Player::White);
    std::cout << "Rule check at " << coord(centre) << ":\n"
              << "  freeThrees   = " << eval.freeThrees << "\n"
              << "  wouldCapture = " << eval.wouldCapture << "\n"
              << "  legal        = " << eval.legal << "\n\n";

    // 2. Run a real search and inspect what it considered.
    Minimax ai(1);
    int depth = 0;
    SearchResult result = ai.findBestMoveTimed(board, Player::White, 350.0, depth);

    std::cout << "Search chose " << coord(result.bestMove)
              << " (depth " << depth << ")\n";
    std::cout << "Chosen move legal? " << board.isLegal(result.bestMove, Player::White) << "\n\n";

    // 3. How many ROOT candidates were actually illegal?
    int illegalCount = 0;
    std::cout << "Root candidates:\n";
    for (const auto& sm : ai.getRootScores()) {
        bool legal = board.isLegal(sm.move, Player::White);
        if (!legal) illegalCount++;
        std::cout << "  " << coord(sm.move)
                  << "  score=" << sm.score
                  << (legal ? "" : "   <-- ILLEGAL (double-three)") << "\n";
    }
    std::cout << "\nIllegal moves in root candidate list: " << illegalCount << "\n";

    return 0;
}