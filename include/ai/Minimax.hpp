#pragma once
#include "engine/Board.hpp"
#include <vector>
#include <chrono>
#include <limits>
#include <cstdint>
#include <unordered_map>

struct SearchResult {
    Move bestMove{0, 0};
    int score = std::numeric_limits<int>::min();
};


enum class TTFlag : uint8_t { Exact, LowerBound, UpperBound };

struct TTEntry {
    int depth = -1;
    int score = 0;
    TTFlag flag = TTFlag::Exact;
    Move bestMove{-1, -1};
};

// Goal: everything needed to exactly reverse one search move, including any
// stones it captured. Fixed-size array (a move can capture at most 8 pairs,
// one per direction) so applying a move never heap-allocates — this runs tens
// of thousands of times per search.
struct MoveUndo {
    Cell placedColor = Cell::Empty;
    Cell capturedColor = Cell::Empty;
    Move captured[16];
    int capturedCount = 0;
};

class Minimax {
public:
    explicit Minimax(int maxDepth);

    SearchResult findBestMove(Board& board, Player aiPlayer, const Move* preferredFirst = nullptr);
    SearchResult findBestMoveTimed(Board& board, Player aiPlayer, double timeLimitMs, int& depthReached);
    bool checkTimeUp();

    long long getTTHits() const { return ttHits; }
    long long getTTMisses() const { return ttMisses; }
    long long getTTStores() const { return ttStores; }
    size_t getTTSize() const { return transpositionTable.size(); }
    void resetTTStats() { ttHits = 0; ttMisses = 0; ttStores = 0; }

    void syncStonePositions(const Board& board);
    int evaluateHeuristic(const Board& board, Player aiPlayer) const;

private:
    int maxDepth;

    // Goal: THE dominant lever on reachable depth. Nodes grow as
    // (effective branching)^depth, and effective branching under alpha-beta is
    // roughly the square root of this number — so cutting it buys depth levels
    // that no constant-factor speedup can match.
    //
    // TRADEOFF to state plainly at the defense: this is forward pruning. Only
    // the top-N moves by ordering heuristic are explored per node, so this is
    // no longer exact minimax — a best move ranked outside the top N is never
    // seen. Standard, unavoidable tradeoff for real-time play; exhaustive
    // depth-10 on 19x19 is computationally impossible regardless of pruning.
    static constexpr size_t MAX_CANDIDATES_PER_NODE = 5;
    static constexpr size_t MAX_CANDIDATES_AT_ROOT = 12;

    int minimax(Board& board, int depth, bool maximizing, Player aiPlayer, int alpha, int beta);

    int evaluateStub(Board& board, Player aiPlayer) const;
    int patternWeight(int length, bool openStart, bool openEnd) const;
    int scorePlayerPatterns(const Board& board, Player p) const;

    // Goal: sparse set — stonePositions holds every occupied cell, posIndex
    // maps a cell back to its slot in that list. Needed because captures
    // remove stones from the MIDDLE of the list, which strict push/pop LIFO
    // can't express. Swap-with-last removal keeps add/remove O(1); list order
    // is arbitrary, which nothing depends on.
    std::vector<Move> stonePositions;
    int posIndex[Board::SIZE * Board::SIZE];
    void addStone(Move m);
    void removeStone(Move m);

    // Goal: applies a move INCLUDING any captures it triggers, recording
    // enough to reverse it exactly. Capture detection is duplicated from
    // Board::checkAndApplyCaptures rather than reused, because that method
    // mutates the real game's capture counters and returns no undo
    // information — both wrong for hypothetical search moves.
    void applyMove(Board& board, Move m, Cell c);
    void undoMove(Board& board, Move m);
    std::vector<MoveUndo> undoStack;

    // Goal: capture counts accumulated by hypothetical search moves only,
    // kept separate from the board's real counters so search never corrupts
    // actual game state. Added to the real counts wherever total captures
    // matter (evaluation, and the 10-capture win check).
    int searchCapturedByBlack = 0;
    int searchCapturedByWhite = 0;
    int totalCapturedBy(const Board& board, Player p) const;

    std::vector<int> seenStamp;
    int currentGeneration = 0;
    std::vector<Move> candidateMoves(const Board& board, int radius = 2);

    void orderMovesByQuickScore(Board& board, std::vector<Move>& moves, Player mover);
    int quickLocalScore(const Board& board, Move m, Player mover) const;
    int localRunScore(const Board& board, Move m, Player p) const;

    std::chrono::steady_clock::time_point deadline;
    bool aborted = false;
    int nodesSinceCheck = 0;

    uint64_t zobristTable[Board::SIZE][Board::SIZE][2];
    // Goal: two positions with identical stones but different capture counts
    // are genuinely different game states — without this the table would
    // return a cached score from the wrong one.
    uint64_t zobristCaptures[2][11];
    uint64_t currentHash = 0;
    void initZobrist();
    uint64_t captureHashComponent() const;

    std::unordered_map<uint64_t, TTEntry> transpositionTable;

    long long ttHits = 0;
    long long ttMisses = 0;
    long long ttStores = 0;
};