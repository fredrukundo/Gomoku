#pragma once
#include "common/Types.hpp"
#include <vector>
#include <string>

class Board {
public:
    static const int SIZE = 19;

    enum class WinReason { None, Alignment, Capture };

    struct WinResult {
        bool won = false;
        WinReason reason = WinReason::None;
        Player winner = Player::Black;
    };

    // Goal: single entry point called after every move. Combines the 10-capture win,
    // a fresh alignment win (finalized or deferred), and resolution of any alignment
    // win that was left pending from the opponent's previous move.
    WinResult checkWinConditions(Move last, Player p);
    Board();

    bool isInBounds(int x, int y) const;
    bool isEmpty(int x, int y) const;
    Cell get(int x, int y) const;

    // returns false (and does nothing) if the move is invalid at this stage
    bool placeStone(Move m, Player p);
    bool checkWin(Move last, Player p) const;

    // returns number of stones captured by this move (0, 2, 4, ... — each direction can independently capture a pair)
    int checkAndApplyCaptures(Move last, Player p);
    int capturedBy(Player p) const;

    // Goal: extract a 1D slice of the board along direction (dx,dy), centered on 'center',
    // 'radius' cells each side, encoded as a string for pattern matching:
    // 'X' = this player's stone, 'O' = opponent's stone, '#' = out of bounds (a hard
    // blocker, same as an opponent stone), '.' = empty.
    std::string extractLine(Move center, int dx, int dy, int radius, Player p) const;

    // Goal: counts how many distinct free-three alignments pass through 'last' for
    // player p, across all 4 axes (horizontal, vertical, 2 diagonals). Step 7 uses
    // this directly for the double-three legality check.
    int countFreeThrees(Move last, Player p) const;

    void print() const;

private:
    Cell grid[SIZE][SIZE];
    int capturedByBlack = 0;
    int capturedByWhite = 0;

    // Goal: like checkWin(), but returns the actual ordered cells of the winning
    // alignment (not just yes/no) so isLineVulnerable() can inspect adjacent pairs.
    std::vector<Move> findWinningLine(Move last, Player p) const;

    // Goal: implements "a 5-alignment only wins if the opponent can't immediately
    // break it by capturing a pair." True if any adjacent same-color pair in the
    // line has a defender stone on one flank and an empty cell on the other.
    bool isLineVulnerable(const std::vector<Move>& line, Player attacker) const;

    bool hasPendingWin = false;
    Player pendingWinPlayer = Player::Black;
    std::vector<Move> pendingWinLine;
};