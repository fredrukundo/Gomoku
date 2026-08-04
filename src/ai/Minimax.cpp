#include "ai/Minimax.hpp"
#include <limits>
#include <algorithm>
#include <random>

namespace {
    inline int colorIndex(Cell c) { return (c == Cell::Black) ? 0 : 1; }
}

Minimax::Minimax(int maxDepth) : maxDepth(maxDepth) {
    deadline = std::chrono::steady_clock::now() + std::chrono::hours(24);
    stonePositions.reserve(400);
    undoStack.reserve(64);
    seenStamp.assign(Board::SIZE * Board::SIZE, 0);
    for (int i = 0; i < Board::SIZE * Board::SIZE; i++)
        posIndex[i] = -1;
    initZobrist();
    transpositionTable.reserve(1 << 16);
}

void Minimax::initZobrist() {
    std::mt19937_64 rng(0xC0FFEEULL);
    for (int x = 0; x < Board::SIZE; x++)
        for (int y = 0; y < Board::SIZE; y++)
            for (int pi = 0; pi < 2; pi++)
                zobristTable[x][y][pi] = rng();
    for (int p = 0; p < 2; p++)
        for (int n = 0; n <= 10; n++)
            zobristCaptures[p][n] = rng();
}

uint64_t Minimax::captureHashComponent() const {
    int b = std::min(searchCapturedByBlack, 10);
    int w = std::min(searchCapturedByWhite, 10);
    return zobristCaptures[0][b] ^ zobristCaptures[1][w];
}

void Minimax::addStone(Move m) {
    posIndex[m.y * Board::SIZE + m.x] = (int)stonePositions.size();
    stonePositions.push_back(m);
}

void Minimax::removeStone(Move m) {
    int slot = m.y * Board::SIZE + m.x;
    int idx = posIndex[slot];
    if (idx < 0) return;
    Move last = stonePositions.back();
    stonePositions[idx] = last;
    posIndex[last.y * Board::SIZE + last.x] = idx;
    stonePositions.pop_back();
    posIndex[slot] = -1;
}

void Minimax::syncStonePositions(const Board& board) {
    stonePositions.clear();
    undoStack.clear();
    for (int i = 0; i < Board::SIZE * Board::SIZE; i++)
        posIndex[i] = -1;
    searchCapturedByBlack = 0;
    searchCapturedByWhite = 0;
    currentHash = 0;

    for (int y = 0; y < Board::SIZE; y++) {
        for (int x = 0; x < Board::SIZE; x++) {
            Cell c = board.get(x, y);
            if (c != Cell::Empty) {
                addStone({x, y});
                currentHash ^= zobristTable[x][y][colorIndex(c)];
            }
        }
    }
    currentHash ^= captureHashComponent();
}

int Minimax::totalCapturedBy(const Board& board, Player p) const {
    return board.capturedBy(p) +
           ((p == Player::Black) ? searchCapturedByBlack : searchCapturedByWhite);
}

void Minimax::applyMove(Board& board, Move m, Cell c) {
    static const int dirs8[8][2] = {
        {1,0}, {-1,0}, {0,1}, {0,-1}, {1,1}, {-1,-1}, {1,-1}, {-1,1}
    };
    Cell opp = (c == Cell::Black) ? Cell::White : Cell::Black;

    currentHash ^= captureHashComponent();

    board.setRaw(m.x, m.y, c);
    addStone(m);
    currentHash ^= zobristTable[m.x][m.y][colorIndex(c)];

    MoveUndo u;
    u.placedColor = c;
    u.capturedColor = opp;
    u.capturedCount = 0;

    for (const auto& d : dirs8) {
        int dx = d[0], dy = d[1];
        int x1 = m.x + dx,     y1 = m.y + dy;
        int x2 = m.x + 2 * dx, y2 = m.y + 2 * dy;
        int x3 = m.x + 3 * dx, y3 = m.y + 3 * dy;

        if (board.isInBounds(x1, y1) && board.isInBounds(x2, y2) && board.isInBounds(x3, y3)
            && board.get(x1, y1) == opp && board.get(x2, y2) == opp && board.get(x3, y3) == c) {
            Move pair[2] = { {x1, y1}, {x2, y2} };
            for (const auto& cell : pair) {
                board.setRaw(cell.x, cell.y, Cell::Empty);
                removeStone(cell);
                currentHash ^= zobristTable[cell.x][cell.y][colorIndex(opp)];
                u.captured[u.capturedCount++] = cell;
            }
        }
    }

    if (c == Cell::Black) searchCapturedByBlack += u.capturedCount;
    else searchCapturedByWhite += u.capturedCount;

    currentHash ^= captureHashComponent();
    undoStack.push_back(u);
}

void Minimax::undoMove(Board& board, Move m) {
    if (undoStack.empty()) return;
    MoveUndo u = undoStack.back();
    undoStack.pop_back();

    currentHash ^= captureHashComponent();

    for (int i = 0; i < u.capturedCount; i++) {
        Move cell = u.captured[i];
        board.setRaw(cell.x, cell.y, u.capturedColor);
        addStone(cell);
        currentHash ^= zobristTable[cell.x][cell.y][colorIndex(u.capturedColor)];
    }

    if (u.placedColor == Cell::Black) searchCapturedByBlack -= u.capturedCount;
    else searchCapturedByWhite -= u.capturedCount;

    currentHash ^= zobristTable[m.x][m.y][colorIndex(u.placedColor)];
    board.setRaw(m.x, m.y, Cell::Empty);
    removeStone(m);

    currentHash ^= captureHashComponent();
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

    for (const auto& pos : stonePositions) {
        if (board.get(pos.x, pos.y) != mine) continue;
        int x = pos.x, y = pos.y;

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
    return total;
}

int Minimax::evaluateHeuristic(const Board& board, Player aiPlayer) const {
    Player opponent = (aiPlayer == Player::Black) ? Player::White : Player::Black;

    int myPatterns = scorePlayerPatterns(board, aiPlayer);
    int theirPatterns = scorePlayerPatterns(board, opponent);

    int myCaps = totalCapturedBy(board, aiPlayer);
    int theirCaps = totalCapturedBy(board, opponent);
    int captureScore = (myCaps * myCaps - theirCaps * theirCaps) * 15;

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

    size_t keepCount = std::min(scored.size(), MAX_CANDIDATES_AT_ROOT);
    std::partial_sort(scored.begin(), scored.begin() + keepCount, scored.end(),
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

    uint64_t hashKey = currentHash;
    int alphaOrig = alpha;

    auto ttIt = transpositionTable.find(hashKey);
    bool usableHit = (ttIt != transpositionTable.end() && ttIt->second.depth >= depth);

    if (usableHit) {
        ttHits++;
        const TTEntry& entry = ttIt->second;
        if (entry.flag == TTFlag::Exact) {
            return entry.score;
        } else if (entry.flag == TTFlag::LowerBound) {
            alpha = std::max(alpha, entry.score);
        } else if (entry.flag == TTFlag::UpperBound) {
            beta = std::min(beta, entry.score);
        }
        if (alpha >= beta) {
            return entry.score;
        }
    } else {
        ttMisses++;
    }

    if (depth == 0) {
        int score = evaluateHeuristic(board, aiPlayer);
        transpositionTable[hashKey] = TTEntry{ depth, score, TTFlag::Exact, Move{-1, -1} };
        ttStores++;
        return score;
    }

    Player opponent = (aiPlayer == Player::Black) ? Player::White : Player::Black;
    Player current = maximizing ? aiPlayer : opponent;
    Cell currentCell = (current == Player::Black) ? Cell::Black : Cell::White;

    auto moves = candidateMoves(board);
    orderMovesByQuickScore(board, moves, current);

    if (ttIt != transpositionTable.end()) {
        Move ttMove = ttIt->second.bestMove;
        for (size_t i = 0; i < moves.size(); i++) {
            if (moves[i].x == ttMove.x && moves[i].y == ttMove.y) {
                std::swap(moves[0], moves[i]);
                break;
            }
        }
    }

    if (moves.size() > MAX_CANDIDATES_PER_NODE)
        moves.resize(MAX_CANDIDATES_PER_NODE);

    if (checkTimeUp())
        return 0;

    int best = maximizing ? std::numeric_limits<int>::min() : std::numeric_limits<int>::max();
    Move bestMoveHere{-1, -1};

    for (const auto& m : moves) {
        if (aborted) break;

        applyMove(board, m, currentCell);

        int score;
        bool decisive = board.hasWinningLine(m, current) ||
                        (totalCapturedBy(board, current) >= 10);
        if (decisive) {
            score = maximizing ? (100000 + depth) : -(100000 + depth);
        } else {
            score = minimax(board, depth - 1, !maximizing, aiPlayer, alpha, beta);
        }

        undoMove(board, m);

        if (aborted) break;

        if (maximizing) {
            if (score > best) { best = score; bestMoveHere = m; }
            alpha = std::max(alpha, best);
        } else {
            if (score < best) { best = score; bestMoveHere = m; }
            beta = std::min(beta, best);
        }

        if (alpha >= beta) break;
    }

    if (!aborted) {
        TTFlag flag;
        if (best <= alphaOrig) flag = TTFlag::UpperBound;
        else if (best >= beta) flag = TTFlag::LowerBound;
        else flag = TTFlag::Exact;
        transpositionTable[hashKey] = TTEntry{ depth, best, flag, bestMoveHere };
        ttStores++;
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

    currentRootScores.clear();

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

        int score;
        bool decisive = board.hasWinningLine(m, aiPlayer) ||
                        (totalCapturedBy(board, aiPlayer) >= 10);
        if (decisive) {
            score = 100000 + maxDepth;
        } else {
            score = minimax(board, maxDepth - 1, false, aiPlayer, alpha, beta);
        }

        undoMove(board, m);

        if (aborted) break;

        // Recorded for the debug view. Note these are alpha-beta scores, not
        // independent evaluations: once alpha rises, later moves may be cut
        // early and report a bound rather than an exact value. The ordering
        // is still meaningful, the losers' exact numbers are not.
        currentRootScores.push_back(ScoredMove{ m, score });

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
    lastRootScores.clear();
    lastCompletedDepth = 0;
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

        // Promote only on a fully completed depth — an aborted depth's
        // partial score list would misrepresent what the AI actually decided.
        lastRootScores = currentRootScores;
        lastCompletedDepth = depth;
        std::sort(lastRootScores.begin(), lastRootScores.end(),
                  [](const ScoredMove& a, const ScoredMove& b) { return a.score > b.score; });

        double elapsedTotal = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - searchStart).count();
        if (elapsedTotal >= timeLimitMs)
            break;
    }

    return bestSoFar;
}