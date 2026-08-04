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

    // Goal: every root candidate and the score the search gave it, sorted
    // best-first — this is the AI's actual reasoning, which findBestMove used
    // to compute and discard. Only ever holds a FULLY COMPLETED depth's
    // numbers; a depth cut off by the time limit is thrown away, since a
    // partially-searched move list would show misleading scores.
    const std::vector<ScoredMove>& getRootScores() const { return lastRootScores; }
    int getLastCompletedDepth() const { return lastCompletedDepth; }

private:
    int maxDepth;

    static constexpr size_t MAX_CANDIDATES_PER_NODE = 6;
    static constexpr size_t MAX_CANDIDATES_AT_ROOT = 12;

    int minimax(Board& board, int depth, bool maximizing, Player aiPlayer, int alpha, int beta);

    int evaluateStub(Board& board, Player aiPlayer) const;
    int patternWeight(int length, bool openStart, bool openEnd) const;
    int scorePlayerPatterns(const Board& board, Player p) const;

    std::vector<Move> stonePositions;
    int posIndex[Board::SIZE * Board::SIZE];
    void addStone(Move m);
    void removeStone(Move m);

    void applyMove(Board& board, Move m, Cell c);
    void undoMove(Board& board, Move m);
    std::vector<MoveUndo> undoStack;

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
    uint64_t zobristCaptures[2][11];
    uint64_t currentHash = 0;
    void initZobrist();
    uint64_t captureHashComponent() const;

    std::unordered_map<uint64_t, TTEntry> transpositionTable;

    long long ttHits = 0;
    long long ttMisses = 0;
    long long ttStores = 0;

    // currentRootScores is filled as findBestMove runs; it's only promoted to
    // lastRootScores once a depth finishes without being aborted.
    std::vector<ScoredMove> currentRootScores;
    std::vector<ScoredMove> lastRootScores;
    int lastCompletedDepth = 0;
};