#pragma once
#include "engine/Board.hpp"
#include <vector>

struct SearchResult {
    Move bestMove;
    int score;
};

class Minimax {
public:
    explicit Minimax(int maxDepth);

    // Goal: entry point — searches from the current board and returns the best
    // move found for 'aiPlayer', plus its score.
    SearchResult findBestMove(Board& board, Player aiPlayer);

private:
    int maxDepth;

    // Goal: the recursive search. 'maximizing' is true when it's aiPlayer's turn
    // in this branch — that's what makes the algorithm alternate between picking
    // the highest-scoring move (our turn) and the lowest-scoring move (opponent's
    // turn, assumed optimal against us).
    int minimax(Board& board, int depth, bool maximizing, Player aiPlayer);

    // Goal: placeholder scoring for this skeleton step only — counts
    // (aiPlayer's stones - opponent's stones). Replaced by the real pattern-based
    // heuristic in Step 11.
    int evaluateStub(Board& board, Player aiPlayer) const;

    // Goal: instead of every empty cell, only return empty cells within 'radius' of
    // an existing stone — this is what makes depth 10 even conceivable, since it
    // collapses the branching factor from ~359 down to a small, localized set. Falls
    // back to the center cell on a fully empty board (radius has nothing to anchor to).
    std::vector<Move> candidateMoves(const Board& board, int radius = 2) const;

    // Goal: assigns a danger score to one run of stones based on its length and how
    // many ends are open (0, 1, or 2). This table encodes Gomoku threat theory: an
    // open three or better is dangerous because it can become unstoppable next move.
    int patternWeight(int length, bool openStart, bool openEnd) const;

    // Goal: sums patternWeight() over every run belonging to 'p' anywhere on the
    // board, across all 4 axes. Each run is counted exactly once by only starting a
    // scan from cells that are the true beginning of a run (previous cell along the
    // axis isn't the same player).
    int scorePlayerPatterns(const Board& board, Player p) const;

    // Goal: the real heuristic — replaces evaluateStub. Combines pattern-based
    // scoring (mine minus opponent's) with a weighted capture-count difference,
    // since captures are an independent win condition worth real weight, not just
    // a side effect of stone count.
    int evaluateHeuristic(const Board& board, Player aiPlayer) const;
};