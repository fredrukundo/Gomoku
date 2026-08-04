#include "ai/Minimax.hpp"
#include <limits>
#include <algorithm>

Minimax::Minimax(int maxDepth) : maxDepth(maxDepth) {}

std::vector<Move> Minimax::candidateMoves(const Board& board, int radius) const {
    std::vector<bool> seen(Board::SIZE * Board::SIZE, false);
    std::vector<Move> moves;
    bool boardHasStones = false;

    for (int y = 0; y < Board::SIZE; y++) {
        for (int x = 0; x < Board::SIZE; x++) {
            if (board.get(x, y) == Cell::Empty)
                continue;
            boardHasStones = true;

            for (int dy = -radius; dy <= radius; dy++) {
                for (int dx = -radius; dx <= radius; dx++) {
                    int nx = x + dx, ny = y + dy;
                    if (!board.isInBounds(nx, ny) || !board.isEmpty(nx, ny))
                        continue;
                    int idx = ny * Board::SIZE + nx;
                    if (!seen[idx]) {
                        seen[idx] = true;
                        moves.push_back({nx, ny});
                    }
                }
            }
        }
    }

    if (!boardHasStones)
        moves.push_back({Board::SIZE / 2, Board::SIZE / 2}); // opening move: center

    return moves;
}

int Minimax::patternWeight(int length, bool openStart, bool openEnd) const {
    int openEnds = (openStart ? 1 : 0) + (openEnd ? 1 : 0);
    if (length >= 5) return 100000;
    if (length == 4) return openEnds == 2 ? 10000 : (openEnds == 1 ? 1000 : 0);
    if (length == 3) return openEnds == 2 ? 500   : (openEnds == 1 ? 100  : 0);
    if (length == 2) return openEnds == 2 ? 50    : (openEnds == 1 ? 10   : 0);
    if (length == 1) return openEnds >= 1 ? 1 : 0;
    return 0;
}

int Minimax::scorePlayerPatterns(const Board& board, Player p) const {
    static const int dirs[4][2] = { {1,0}, {0,1}, {1,1}, {1,-1} };
    Cell mine = (p == Player::Black) ? Cell::Black : Cell::White;
    int total = 0;

    for (int y = 0; y < Board::SIZE; y++) {
        for (int x = 0; x < Board::SIZE; x++) {
            if (board.get(x, y) != mine) continue;

            for (const auto& d : dirs) {
                int dx = d[0], dy = d[1];
                int px = x - dx, py = y - dy;

                // Only start counting at the true beginning of a run — otherwise
                // a single run of length 4 gets counted 4 times, once per stone.
                if (board.isInBounds(px, py) && board.get(px, py) == mine)
                    continue;

                int len = 0;
                int fx = x, fy = y;
                while (board.isInBounds(fx, fy) && board.get(fx, fy) == mine) {
                    len++;
                    fx += dx; fy += dy;
                }

                bool openStart = board.isInBounds(px, py) && board.get(px, py) == Cell::Empty;
                bool openEnd = board.isInBounds(fx, fy) && board.get(fx, fy) == Cell::Empty;

                total += patternWeight(len, openStart, openEnd);
            }
        }
    }
    return total;
}

int Minimax::evaluateHeuristic(const Board& board, Player aiPlayer) const {
    Player opponent = (aiPlayer == Player::Black) ? Player::White : Player::Black;

    int myPatterns = scorePlayerPatterns(board, aiPlayer);
    int theirPatterns = scorePlayerPatterns(board, opponent);
    int captureScore = (board.capturedBy(aiPlayer) - board.capturedBy(opponent)) * 50;

    return (myPatterns - theirPatterns) + captureScore;
}

int Minimax::evaluateStub(Board& board, Player aiPlayer) const {
    Cell mineCell = (aiPlayer == Player::Black) ? Cell::Black : Cell::White;
    Cell theirsCell = (aiPlayer == Player::Black) ? Cell::White : Cell::Black;
    int mine = 0, theirs = 0;
    for (int y = 0; y < Board::SIZE; y++) {
        for (int x = 0; x < Board::SIZE; x++) {
            Cell c = board.get(x, y);
            if (c == mineCell) mine++;
            else if (c == theirsCell) theirs++;
        }
    }
    return mine - theirs;
}

int Minimax::minimax(Board& board, int depth, bool maximizing, Player aiPlayer, int alpha, int beta) {
    if (depth == 0)
        return evaluateHeuristic(board, aiPlayer);

    Player opponent = (aiPlayer == Player::Black) ? Player::White : Player::Black;
    Player current = maximizing ? aiPlayer : opponent;
    Cell currentCell = (current == Player::Black) ? Cell::Black : Cell::White;

    int best = maximizing ? std::numeric_limits<int>::min() : std::numeric_limits<int>::max();

    for (const auto& m : candidateMoves(board)) {
        board.setRaw(m.x, m.y, currentCell);

        int score;
        auto line = board.findWinningLine(m, current);
        if (!line.empty()) {
            score = maximizing ? (100000 + depth) : -(100000 + depth);
        } else {
            score = minimax(board, depth - 1, !maximizing, aiPlayer, alpha, beta);
        }

        board.setRaw(m.x, m.y, Cell::Empty);

        if (maximizing) {
            best = std::max(best, score);
            alpha = std::max(alpha, best);
        } else {
            best = std::min(best, score);
            beta = std::min(beta, best);
        }

        // The cut: once alpha meets or exceeds beta, this branch can no longer
        // change the final decision at a shallower level — stop exploring it.
        if (alpha >= beta)
            break;
    }

    return best;
}

SearchResult Minimax::findBestMove(Board& board, Player aiPlayer) {
    SearchResult result;
    result.score = std::numeric_limits<int>::min();
    Cell myCell = (aiPlayer == Player::Black) ? Cell::Black : Cell::White;
    int alpha = std::numeric_limits<int>::min();
    int beta = std::numeric_limits<int>::max();

    for (const auto& m : candidateMoves(board)) {
        board.setRaw(m.x, m.y, myCell);
        int score = minimax(board, maxDepth - 1, false, aiPlayer, alpha, beta);
        board.setRaw(m.x, m.y, Cell::Empty);

        if (score > result.score) {
            result.score = score;
            result.bestMove = m;
        }
        alpha = std::max(alpha, result.score);
    }

    return result;
}