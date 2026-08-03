#include "engine/Board.hpp"
#include <iostream>

Board::Board() {
    for (int y = 0; y < SIZE; y++)
        for (int x = 0; x < SIZE; x++)
            grid[y][x] = Cell::Empty;
}

bool Board::isInBounds(int x, int y) const {
    return x >= 0 && x < SIZE && y >= 0 && y < SIZE;
}

bool Board::isEmpty(int x, int y) const {
    return isInBounds(x, y) && grid[y][x] == Cell::Empty;
}

Cell Board::get(int x, int y) const {
    if (!isInBounds(x, y))
        return Cell::Empty; // never index out of bounds
    return grid[y][x];
}

bool Board::placeStone(Move m, Player p) {
    if (!isEmpty(m.x, m.y))
        return false;
    grid[m.y][m.x] = (p == Player::Black) ? Cell::Black : Cell::White;
    return true;
}

bool Board::checkWin(Move last, Player p) const {
    // 4 axes: horizontal, vertical, diagonal down-right, diagonal up-right
    static const int dirs[4][2] = { {1, 0}, {0, 1}, {1, 1}, {1, -1} };
    Cell target = (p == Player::Black) ? Cell::Black : Cell::White;

    for (const auto& d : dirs) {
        int dx = d[0], dy = d[1];
        int count = 1; // the stone just placed

        int x = last.x + dx, y = last.y + dy;
        while (isInBounds(x, y) && grid[y][x] == target) {
            count++;
            x += dx;
            y += dy;
        }

        x = last.x - dx;
        y = last.y - dy;
        while (isInBounds(x, y) && grid[y][x] == target) {
            count++;
            x -= dx;
            y -= dy;
        }

        if (count >= 5)
            return true;
    }
    return false;
}

int Board::checkAndApplyCaptures(Move last, Player p) {
    static const int dirs8[8][2] = {
        {1,0}, {-1,0}, {0,1}, {0,-1},
        {1,1}, {-1,-1}, {1,-1}, {-1,1}
    };

    Cell mine = (p == Player::Black) ? Cell::Black : Cell::White;
    Cell opp  = (p == Player::Black) ? Cell::White : Cell::Black;
    int capturedCount = 0;

    for (const auto& d : dirs8) {
        int dx = d[0], dy = d[1];
        int x1 = last.x + dx,     y1 = last.y + dy;
        int x2 = last.x + 2 * dx, y2 = last.y + 2 * dy;
        int x3 = last.x + 3 * dx, y3 = last.y + 3 * dy;

        if (isInBounds(x1, y1) && isInBounds(x2, y2) && isInBounds(x3, y3)
            && grid[y1][x1] == opp && grid[y2][x2] == opp && grid[y3][x3] == mine) {
            grid[y1][x1] = Cell::Empty;
            grid[y2][x2] = Cell::Empty;
            capturedCount += 2;
        }
    }

    if (p == Player::Black) capturedByBlack += capturedCount;
    else capturedByWhite += capturedCount;

    return capturedCount;
}

int Board::capturedBy(Player p) const {
    return (p == Player::Black) ? capturedByBlack : capturedByWhite;
}

// Goal: find the full ordered run of same-color stones through 'last', in
// whichever of the 4 axes reaches length >= 5. Walks backward to the start of
// the run first, then forward, so the result is spatially ordered — required
// for isLineVulnerable() to check adjacent pairs correctly.
std::vector<Move> Board::findWinningLine(Move last, Player p) const {
    static const int dirs[4][2] = { {1,0}, {0,1}, {1,1}, {1,-1} };
    Cell target = (p == Player::Black) ? Cell::Black : Cell::White;

    for (const auto& d : dirs) {
        int dx = d[0], dy = d[1];

        int sx = last.x, sy = last.y;
        while (isInBounds(sx - dx, sy - dy) && grid[sy - dy][sx - dx] == target) {
            sx -= dx; sy -= dy;
        }

        std::vector<Move> line;
        int x = sx, y = sy;
        while (isInBounds(x, y) && grid[y][x] == target) {
            line.push_back({x, y});
            x += dx; y += dy;
        }

        if ((int)line.size() >= 5)
            return line;
    }
    return {};
}

// Goal: check every adjacent pair inside the winning line for a capture setup
// the opponent could complete on their next move (defender stone on one side,
// empty cell on the other side of the pair).
bool Board::isLineVulnerable(const std::vector<Move>& line, Player attacker) const {
    Player defender = (attacker == Player::Black) ? Player::White : Player::Black;
    Cell mine = (attacker == Player::Black) ? Cell::Black : Cell::White;
    Cell defenderCell = (defender == Player::Black) ? Cell::Black : Cell::White;
    static const int dirs4[4][2] = { {1,0}, {0,1}, {1,1}, {1,-1} };

    // For every stone IN the winning line, check all 4 axis directions for a
    // same-color neighbor (which may or may not also be part of the line) —
    // that neighbor pair is what could actually get captured.
    for (const auto& cell : line) {
        for (const auto& d : dirs4) {
            int dx = d[0], dy = d[1];
            int nx = cell.x + dx, ny = cell.y + dy;
            if (!isInBounds(nx, ny) || grid[ny][nx] != mine)
                continue;

            int fx1 = cell.x - dx, fy1 = cell.y - dy;
            int fx2 = nx + dx,     fy2 = ny + dy;

            bool flank1Defender = isInBounds(fx1, fy1) && grid[fy1][fx1] == defenderCell;
            bool flank2Defender = isInBounds(fx2, fy2) && grid[fy2][fx2] == defenderCell;
            bool flank1Empty = isInBounds(fx1, fy1) && grid[fy1][fx1] == Cell::Empty;
            bool flank2Empty = isInBounds(fx2, fy2) && grid[fy2][fx2] == Cell::Empty;

            if ((flank1Defender && flank2Empty) || (flank2Defender && flank1Empty))
                return true;
        }
    }
    return false;
}

Board::WinResult Board::checkWinConditions(Move last, Player p) {
    WinResult result;

    // 1) Capture win — direct, no deferral.
    if (capturedBy(p) >= 10) {
        result.won = true;
        result.reason = WinReason::Capture;
        result.winner = p;
        return result;
    }

    // 2) A fresh alignment created by this move.
    std::vector<Move> line = findWinningLine(last, p);
    if (!line.empty()) {
        if (isLineVulnerable(line, p)) {
            // Don't end the game yet — give the opponent one move to break it.
            hasPendingWin = true;
            pendingWinPlayer = p;
            pendingWinLine = line;
        } else {
            result.won = true;
            result.reason = WinReason::Alignment;
            result.winner = p;
            return result;
        }
    }

    // 3) Resolve a win that was left pending after the opponent's previous move.
    if (hasPendingWin && pendingWinPlayer != p) {
        Cell target = (pendingWinPlayer == Player::Black) ? Cell::Black : Cell::White;
        bool stillIntact = true;
        for (const auto& m : pendingWinLine) {
            if (grid[m.y][m.x] != target) { stillIntact = false; break; }
        }
        if (stillIntact) {
            result.won = true;
            result.reason = WinReason::Alignment;
            result.winner = pendingWinPlayer;
        }
        hasPendingWin = false;
    }

    return result;
}

std::string Board::extractLine(Move center, int dx, int dy, int radius, Player p) const {
    Cell mine = (p == Player::Black) ? Cell::Black : Cell::White;
    Cell opp  = (p == Player::Black) ? Cell::White : Cell::Black;
    std::string line;

    for (int i = -radius; i <= radius; i++) {
        int x = center.x + dx * i, y = center.y + dy * i;
        if (!isInBounds(x, y)) line += '#';
        else if (grid[y][x] == mine) line += 'X';
        else if (grid[y][x] == opp) line += 'O';
        else line += '.';
    }
    return line;
}

namespace {
    // Goal: checks a length-9 extracted window against the 3 known free-three
    // shapes, only counting a match if it actually covers the just-placed stone
    // (at index 4, the window's center) — otherwise it'd be a pre-existing
    // pattern this move didn't create.
    bool matchesFreeThreeTemplate(const std::string& window, int centerIdx) {
        static const std::vector<std::string> templates = {
            ".XXX.",
            ".XX.X.",
            ".X.XX."
        };
        for (const auto& t : templates) {
            int len = (int)t.size();
            for (int s = 0; s <= (int)window.size() - len; s++) {
                if (s > centerIdx || s + len - 1 < centerIdx)
                    continue;
                bool match = true;
                for (int i = 0; i < len; i++) {
                    if (t[i] == 'X' && window[s + i] != 'X') { match = false; break; }
                    if (t[i] == '.' && window[s + i] != '.') { match = false; break; }
                }
                if (match) return true;
            }
        }
        return false;
    }
}

int Board::countFreeThrees(Move last, Player p) const {
    static const int dirs[4][2] = { {1,0}, {0,1}, {1,1}, {1,-1} };
    int count = 0;
    for (const auto& d : dirs) {
        std::string window = extractLine(last, d[0], d[1], 4, p);
        if (matchesFreeThreeTemplate(window, 4))
            count++;
    }
    return count;
}

bool Board::wouldCaptureAnyPair(Move last, Player p) const {
    static const int dirs8[8][2] = {
        {1,0}, {-1,0}, {0,1}, {0,-1},
        {1,1}, {-1,-1}, {1,-1}, {-1,1}
    };
    Cell mine = (p == Player::Black) ? Cell::Black : Cell::White;
    Cell opp  = (p == Player::Black) ? Cell::White : Cell::Black;

    for (const auto& d : dirs8) {
        int dx = d[0], dy = d[1];
        int x1 = last.x + dx,   y1 = last.y + dy;
        int x2 = last.x + 2*dx, y2 = last.y + 2*dy;
        int x3 = last.x + 3*dx, y3 = last.y + 3*dy;
        if (isInBounds(x1,y1) && isInBounds(x2,y2) && isInBounds(x3,y3)
            && grid[y1][x1] == opp && grid[y2][x2] == opp && grid[y3][x3] == mine)
            return true;
    }
    return false;
}

Board::MoveEvaluation Board::evaluateMove(Move m, Player p) {
    MoveEvaluation eval;

    if (!isEmpty(m.x, m.y)) {
        eval.legal = false;
        return eval;
    }

    Cell mine = (p == Player::Black) ? Cell::Black : Cell::White;
    grid[m.y][m.x] = mine; // temporary placement

    eval.wouldCapture = wouldCaptureAnyPair(m, p);
    eval.freeThrees = countFreeThrees(m, p);

    grid[m.y][m.x] = Cell::Empty; // rollback

    if (!eval.wouldCapture && eval.freeThrees >= 2)
        eval.legal = false;

    return eval;
}

bool Board::isLegal(Move m, Player p) {
    return evaluateMove(m, p).legal;
}

void Board::print() const {
    for (int y = 0; y < SIZE; y++) {
        for (int x = 0; x < SIZE; x++) {
            char c = '.';
            if (grid[y][x] == Cell::Black) c = 'X';
            else if (grid[y][x] == Cell::White) c = 'O';
            std::cout << c << ' ';
        }
        std::cout << '\n';
    }
}