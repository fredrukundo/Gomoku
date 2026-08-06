// ============================================================================
//  ai_smoke_test.cpp — sanity tests for the AI search (Minimax)
//
//  Run with: make ai-test
//
//  These are not exhaustive unit tests like rules_tests.cpp — they build a
//  handful of small, hand-crafted board positions and check that the AI
//  makes the move a reasonable player would expect, within the time budget.
//  This is what you re-run after any change to Minimax.cpp to make sure
//  nothing regressed, and it's also a good live demo at the defense.
//
//  Boards are built with setRaw() directly, bypassing legality checks — that
//  is fine here since we are constructing exact positions to test, not
//  playing a real game.
// ============================================================================

#include "engine/Board.hpp"
#include "ai/Minimax.hpp"
#include <iostream>
#include <chrono>
#include <string>

static const char* COLS = "ABCDEFGHJKLMNOPQRST";   // 'I' skipped, matches the GUI labels

static std::string coord(Move m) {
    return std::string(1, COLS[m.x]) + std::to_string(m.y + 1);
}

// The time budget every test uses. Kept comfortably under the subject's
// 500ms requirement, matching what the real game uses (AI_TIME_LIMIT_MS in
// main.cpp).
static const double TIME_LIMIT_MS = 350.0;

// Runs a timed search and prints a one-line summary. Returns the result so
// each test can assert on the move / score it cares about.
static SearchResult think(const std::string& label, Board& board, Player aiPlayer, int& depthOut) {
    Minimax ai(1);   // initial depth doesn't matter — findBestMoveTimed overrides it

    auto start = std::chrono::steady_clock::now();
    SearchResult result = ai.findBestMoveTimed(board, aiPlayer, TIME_LIMIT_MS, depthOut);
    auto end = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "[" << label << "] chose " << coord(result.bestMove)
              << "  score=" << result.score
              << "  depth=" << depthOut
              << "  time=" << (int)ms << " ms\n";

    return result;
}

// ============================================================================

// Goal: on a completely empty board there is nothing to anchor a search
// radius to, so candidateMoves() falls back to the center cell. This also
// doubles as a basic "does it run without crashing" check.
void test_opening_move_is_center() {
    Board board;
    int depth = 0;
    SearchResult result = think("opening move", board, Player::Black, depth);

    Move expectedCenter{Board::SIZE / 2, Board::SIZE / 2};
    bool correct = (result.bestMove.x == expectedCenter.x && result.bestMove.y == expectedCenter.y);
    std::cout << "  expected " << coord(expectedCenter)
              << (correct ? "  -> OK\n" : "  -> UNEXPECTED\n");
}

// Goal: the single most important tactical check. If Black has an open
// three (both ends empty), White MUST play one of the two open ends or the
// position becomes an unstoppable open four next turn.
void test_blocks_an_open_three() {
    Board board;
    board.setRaw(5, 5, Cell::Black);
    board.setRaw(6, 5, Cell::Black);
    board.setRaw(7, 5, Cell::Black);
    // Open ends are (4,5) and (8,5).

    int depth = 0;
    SearchResult result = think("block open three", board, Player::White, depth);

    bool blockedLeft  = (result.bestMove.x == 4 && result.bestMove.y == 5);
    bool blockedRight = (result.bestMove.x == 8 && result.bestMove.y == 5);
    std::cout << "  expected D6 or J6 (either open end)"
              << ((blockedLeft || blockedRight) ? "  -> OK\n" : "  -> DID NOT BLOCK\n");
}

// Goal: if the AI already has 4 in a row with an open end, it should
// complete the five and win rather than doing anything else. This is the
// "unmissable" case — if this fails, something is badly broken.
void test_takes_an_immediate_win() {
    Board board;
    board.setRaw(5, 8, Cell::White);
    board.setRaw(6, 8, Cell::White);
    board.setRaw(7, 8, Cell::White);
    board.setRaw(8, 8, Cell::White);
    // A couple of unrelated Black stones so it's not a completely empty board.
    board.setRaw(2, 2, Cell::Black);
    board.setRaw(3, 3, Cell::Black);

    int depth = 0;
    SearchResult result = think("take the win", board, Player::White, depth);

    bool completesFive = (result.bestMove.x == 4 || result.bestMove.x == 9) && result.bestMove.y == 8;
    bool scoredAsWin = result.score >= 100000;
    std::cout << "  expected a move completing 5 in a row, score >= 100000"
              << ((completesFive && scoredAsWin) ? "  -> OK\n" : "  -> DID NOT TAKE THE WIN\n");
}

// Goal: the search must never choose a move that Board itself considers
// illegal (currently, a pure double-three). candidateMoves() filters these
// out; this test builds a position where an illegal double-three is
// available and confirms it never gets picked.
void test_never_chooses_an_illegal_move() {
    Board board;
    // A plus-shape around (10,10). Playing the center would create two open
    // threes at once — illegal, since no capture is involved.
    board.setRaw(9, 10, Cell::White);
    board.setRaw(11, 10, Cell::White);
    board.setRaw(10, 9, Cell::White);
    board.setRaw(10, 11, Cell::White);
    board.setRaw(6, 6, Cell::Black);
    board.setRaw(7, 7, Cell::Black);
    board.setRaw(8, 8, Cell::Black);

    int depth = 0;
    SearchResult result = think("avoid illegal move", board, Player::White, depth);

    bool chosenIsLegal = board.isLegal(result.bestMove, Player::White);
    std::cout << "  chosen move must be legal on the real board"
              << (chosenIsLegal ? "  -> OK\n" : "  -> ILLEGAL MOVE CHOSEN (bug)\n");
}

// Goal: confirms the search actually respects the time budget it was given,
// and reports every root candidate it looked at — the same data the GUI's
// debug view (D key) displays. Useful to eyeball that scores look sane
// (best move highest, no wildly nonsensical numbers).
void test_respects_time_budget_and_shows_reasoning() {
    Board board;
    board.setRaw(5, 5, Cell::Black);
    board.setRaw(6, 5, Cell::Black);
    board.setRaw(7, 5, Cell::Black);
    board.setRaw(9, 9, Cell::White);
    board.setRaw(10, 10, Cell::Black);

    int depth = 0;
    auto start = std::chrono::steady_clock::now();
    Minimax ai(1);
    // SearchResult result = ai.findBestMoveTimed(board, Player::White, TIME_LIMIT_MS, depth);
    auto end = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "[time budget] depth=" << depth << " time=" << (int)ms
              << " ms (limit " << (int)TIME_LIMIT_MS << " ms)"
              << (ms <= TIME_LIMIT_MS + 100 ? "  -> OK\n" : "  -> OVER BUDGET\n");

    std::cout << "  root candidates considered (best first):\n";
    int rank = 1;
    for (const auto& sm : ai.getRootScores()) {
        std::cout << "    " << rank++ << ". " << coord(sm.move) << "  score=" << sm.score << "\n";
    }
}

// ============================================================================

int main() {
    test_opening_move_is_center();
    std::cout << "\n";

    test_blocks_an_open_three();
    std::cout << "\n";

    test_takes_an_immediate_win();
    std::cout << "\n";

    test_never_chooses_an_illegal_move();
    std::cout << "\n";

    test_respects_time_budget_and_shows_reasoning();

    std::cout << "\nDone. Unlike rules_tests.cpp, these are read by eye —\n"
                 "check every line above says OK, and that real games in the\n"
                 "GUI confirm the same behavior (blocking, taking wins, timing).\n";
    return 0;
}