#include "ai/Minimax.hpp"
#include <limits>
#include <algorithm>

Minimax::Minimax(int maxDepth) : maxDepth(maxDepth) {
    // Goal: safe, far-future default so a direct findBestMove() call (bypassing
    // findBestMoveTimed, as earlier smoke tests do) never mistakes a
    // default-constructed (epoch) deadline for "time's already up."
    deadline = std::chrono::steady_clock::now() + std::chrono::hours(24);
    stonePositions.reserve(400);
    seenStamp.assign(Board::SIZE * Board::SIZE, 0);
}

void Minimax::syncStonePositions(const Board& board) {
    stonePositions.clear();
    for (int y = 0; y < Board::SIZE; y++)
        for (int x = 0; x < Board::SIZE; x++)
            if (board.get(x, y) != Cell::Empty)
                stonePositions.push_back({x, y});
}

void Minimax::applyMove(Board& board, Move m, Cell c) {
    board.setRaw(m.x, m.y, c);
    stonePositions.push_back(m);
}

void Minimax::undoMove(Board& board, Move m) {
    board.setRaw(m.x, m.y, Cell::Empty);
    if (!stonePositions.empty())
        stonePositions.pop_back();
}

std::vector<Move> Minimax::candidateMoves(const Board& board, int radius) {
    std::vector<Move> moves;

    if (stonePositions.empty()) {
        moves.push_back({Board::SIZE / 2, Board::SIZE / 2});
        return moves;
    }

    currentGeneration++;
    for (const auto& s : stonePositions) {
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                int nx = s.x + dx, ny = s.y + dy;
                if (!board.isInBounds(nx, ny) || !board.isEmpty(nx, ny))
                    continue;
                int idx = ny * Board::SIZE + nx;
                if (seenStamp[idx] != currentGeneration) {
                    seenStamp[idx] = currentGeneration;
                    moves.push_back({nx, ny});
                }
            }
        }
    }
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

int Minimax::localRunScore(const Board& board, Move m, Player p) const {
    Cell target = (p == Player::Black) ? Cell::Black : Cell::White;
    static const int dirs[4][2] = { {1,0}, {0,1}, {1,1}, {1,-1} };
    static const int LOOKAHEAD = 4;

    int total = 0;
    for (const auto& d : dirs) {
        int dx = d[0], dy = d[1];
        int len = 1;

        int x = m.x + dx, y = m.y + dy, steps = 0;
        while (steps < LOOKAHEAD && board.isInBounds(x, y) && board.get(x, y) == target) {
            len++; x += dx; y += dy; steps++;
        }
        bool openEnd = board.isInBounds(x, y) && board.get(x, y) == Cell::Empty;

        x = m.x - dx; y = m.y - dy; steps = 0;
        while (steps < LOOKAHEAD && board.isInBounds(x, y) && board.get(x, y) == target) {
            len++; x -= dx; y -= dy; steps++;
        }
        bool openStart = board.isInBounds(x, y) && board.get(x, y) == Cell::Empty;

        total += patternWeight(len, openStart, openEnd);
    }
    return total;
}

int Minimax::quickLocalScore(const Board& board, Move m, Player mover) const {
    Player opponent = (mover == Player::Black) ? Player::White : Player::Black;
    return localRunScore(board, m, mover) + localRunScore(board, m, opponent);
}

void Minimax::orderMovesByQuickScore(Board& board, std::vector<Move>& moves, Player mover) {
    std::vector<std::pair<int, Move>> scored;
    scored.reserve(moves.size());
    for (const auto& m : moves)
        scored.push_back({quickLocalScore(board, m, mover), m});

    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    for (size_t i = 0; i < moves.size(); i++)
        moves[i] = scored[i].second;
}

bool Minimax::checkTimeUp() {
    if (aborted) return true;
    if (++nodesSinceCheck >= 256) {
        nodesSinceCheck = 0;
        if (std::chrono::steady_clock::now() >= deadline)
            aborted = true;
    }
    return aborted;
}

int Minimax::minimax(Board& board, int depth, bool maximizing, Player aiPlayer, int alpha, int beta) {
    if (checkTimeUp())
        return 0;

    if (depth == 0)
        return evaluateHeuristic(board, aiPlayer);

    Player opponent = (aiPlayer == Player::Black) ? Player::White : Player::Black;
    Player current = maximizing ? aiPlayer : opponent;
    Cell currentCell = (current == Player::Black) ? Cell::Black : Cell::White;

    auto moves = candidateMoves(board);
    orderMovesByQuickScore(board, moves, current);
    if (moves.size() > MAX_CANDIDATES_PER_NODE)
        moves.resize(MAX_CANDIDATES_PER_NODE);

    if (checkTimeUp())
        return 0;

    int best = maximizing ? std::numeric_limits<int>::min() : std::numeric_limits<int>::max();

    for (const auto& m : moves) {
        if (aborted) break;

        applyMove(board, m, currentCell);

        int score;
        auto line = board.findWinningLine(m, current);
        if (!line.empty()) {
            score = maximizing ? (100000 + depth) : -(100000 + depth);
        } else {
            score = minimax(board, depth - 1, !maximizing, aiPlayer, alpha, beta);
        }

        undoMove(board, m);

        if (aborted) break;

        if (maximizing) { best = std::max(best, score); alpha = std::max(alpha, best); }
        else { best = std::min(best, score); beta = std::min(beta, best); }

        if (alpha >= beta) break;
    }
    return best;
}

SearchResult Minimax::findBestMove(Board& board, Player aiPlayer, const Move* preferredFirst) {
    syncStonePositions(board);

    SearchResult result;
    result.score = std::numeric_limits<int>::min();
    Cell myCell = (aiPlayer == Player::Black) ? Cell::Black : Cell::White;
    int alpha = std::numeric_limits<int>::min();
    int beta = std::numeric_limits<int>::max();

    auto moves = candidateMoves(board);
    orderMovesByQuickScore(board, moves, aiPlayer);
    if (moves.size() > MAX_CANDIDATES_AT_ROOT)
        moves.resize(MAX_CANDIDATES_AT_ROOT);
    if (preferredFirst) {
        for (size_t i = 0; i < moves.size(); i++) {
            if (moves[i].x == preferredFirst->x && moves[i].y == preferredFirst->y) {
                std::swap(moves[0], moves[i]);
                break;
            }
        }
    }

    for (const auto& m : moves) {
        if (aborted) break;

        applyMove(board, m, myCell);
        int score = minimax(board, maxDepth - 1, false, aiPlayer, alpha, beta);
        undoMove(board, m);

        if (aborted) break;

        if (score > result.score) {
            result.score = score;
            result.bestMove = m;
        }
        alpha = std::max(alpha, result.score);
    }

    return result;
}

SearchResult Minimax::findBestMoveTimed(Board& board, Player aiPlayer, double timeLimitMs, int& depthReached) {
    auto searchStart = std::chrono::steady_clock::now();
    deadline = searchStart + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                  std::chrono::duration<double, std::milli>(timeLimitMs));
    SearchResult bestSoFar;
    depthReached = 0;
    bool havePrevious = false;
    Move previousBest{};

    for (int depth = 1; depth <= 30; depth++) {
        maxDepth = depth;
        aborted = false;
        nodesSinceCheck = 0;

        SearchResult result = findBestMove(board, aiPlayer, havePrevious ? &previousBest : nullptr);

        if (aborted)
            break;

        bestSoFar = result;
        depthReached = depth;
        previousBest = result.bestMove;
        havePrevious = true;

        double elapsedTotal = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - searchStart).count();
        if (elapsedTotal >= timeLimitMs)
            break;
    }

    return bestSoFar;
}