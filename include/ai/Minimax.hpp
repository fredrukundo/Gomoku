#pragma once
#include "engine/Board.hpp"
#include <vector>
#include <chrono>
#include <limits>

// Goal: default-initialized so a SearchResult can never carry garbage
// coordinates/score if returned before a real result is ever assigned (e.g. an
// extremely tight time budget that aborts before depth 1 completes).
struct SearchResult {
    Move bestMove{0, 0};
    int score = std::numeric_limits<int>::min();
};

class Minimax {
public:
    explicit Minimax(int maxDepth);

    SearchResult findBestMove(Board& board, Player aiPlayer, const Move* preferredFirst = nullptr);
    SearchResult findBestMoveTimed(Board& board, Player aiPlayer, double timeLimitMs, int& depthReached);
    bool checkTimeUp();

private:
    int maxDepth;
    static const size_t MAX_CANDIDATES_PER_NODE = 12; // deeper nodes: narrow, cheap
    static const size_t MAX_CANDIDATES_AT_ROOT = 20;   // root: a bit wider, cost is amortized once

    int minimax(Board& board, int depth, bool maximizing, Player aiPlayer, int alpha, int beta);

    int evaluateStub(Board& board, Player aiPlayer) const;
    int evaluateHeuristic(const Board& board, Player aiPlayer) const;
    int patternWeight(int length, bool openStart, bool openEnd) const;
    int scorePlayerPatterns(const Board& board, Player p) const;

    // Goal: a live, incrementally-maintained list of every stone on the board
    // (both colors). Replaces rescanning all 361 cells for occupied ones at
    // every search node — that scan now only happens once per depth iteration.
    std::vector<Move> stonePositions;

    // Goal: rebuilds stonePositions from the real board. Called once per
    // findBestMove() call (once per depth iteration), never per node.
    void syncStonePositions(const Board& board);

    // Goal: places a stone AND records it in stonePositions together, so the
    // two can never drift apart. All AI search placement goes through this —
    // never call board.setRaw directly elsewhere in this class.
    void applyMove(Board& board, Move m, Cell c);

    // Goal: exact inverse of applyMove. Relies on strict LIFO usage (apply,
    // recurse, undo — never interleaved), which the search already guarantees.
    void undoMove(Board& board, Move m);

    // Goal: generation-stamped dedup array, replacing a fresh
    // std::vector<bool> allocated on every candidateMoves() call. Bumping
    // currentGeneration invalidates all previous stamps in O(1).
    std::vector<int> seenStamp;
    int currentGeneration = 0;

    // Goal: empty cells within 'radius' of any stone in stonePositions,
    // deduplicated via seenStamp. Falls back to the center cell if empty.
    std::vector<Move> candidateMoves(const Board& board, int radius = 2);

    void orderMovesByQuickScore(Board& board, std::vector<Move>& moves, Player mover);
    int quickLocalScore(const Board& board, Move m, Player mover) const;
    int localRunScore(const Board& board, Move m, Player p) const;

    std::chrono::steady_clock::time_point deadline;
    bool aborted = false;
    int nodesSinceCheck = 0;
};