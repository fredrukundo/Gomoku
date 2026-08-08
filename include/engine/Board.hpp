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


    struct MoveEvaluation {
    bool legal = true;
    bool wouldCapture = false;
    int freeThrees = 0;
    };

    /*
        ****** Board access and basic placement ******
    */


    Board();

    // Goal: returns true if (x,y) is a valid cell on the board, false if out of bounds.
    bool isInBounds(int x, int y) const;

    // Goal: returns true if (x,y) is in bounds and empty, false if out of bounds or occupied.
    bool isEmpty(int x, int y) const;

    // Goal: safe read access to a Cell on the Board. return Empty if out of bounds, otherwise the Cell at (x,y).
    Cell get(int x, int y) const;

    // Goal: places a stone of player p at position (x,y) if the move is legal, returns true if successful.
    bool placeStone(Move m, Player p);

    // Goal: raw, rules-bypassing stone placement for AI search only.
    // Never touches the capture counters, never checks legality, never checks for a win.
    // Only used by the search.
    void setRaw(int x, int y, Cell c);

    // Goal: prints the board to stdout for debugging purposes
    // ( testing before the GUI is implemented, this is the only way to see the board state)
     void print() const;


    /*
        ****** Capture tracking ******
    */

    // returns number of stones captured by this move (0, 2, 4, ... — each direction can independently capture a pair)
    int checkAndApplyCaptures(Move last, Player p);

    // returns the number of stones captured by player p so far in the game and
    // stored in the board state (display it in the side panel).
    // This is used for the 10-capture win condition.
    int capturedBy(Player p) const;

    // Read-only twin of checkAndApplyCaptures: same 8-direction check.
    // Lets legality testing ask "would this move capture something?" without committing to the move.
    bool wouldCaptureAnyPair(Move last, Player p) const;

    /*
        ****** Win Conditions ******
    
    */


     // Goal: returns the full ordered run of same-color stones through 'last',
     // in any of the 4 axes (horizontal, vertical, 2 diagonals).
     // Returns an empty vector if no axis has a run of length >= 5.
    std::vector<Move> findWinningLine(Move last, Player p) const;

    // Goal: non-allocating twin of findWinningLine, for the search's hot path.
    // findWinningLine returns a std::vector (heap allocation) but the search
    // only needs a yes/no per move — at tens of thousands of calls per search,
    // those allocations are pure waste. findWinningLine is still used by the
    // GUI, which needs the actual cells to draw the win highlight.
    bool hasWinningLine(Move last, Player p) const;

    // Goal: checks the current board state for a win condition after the last move (10-capture or 5-in-a-row).
    // Returns a WinResult struct with the result.
    WinResult checkWinConditions(Move last, Player p);


    /*
        ****** Free-three detection and legality ******
    */


    // Goal: extract a 1D slice of the board along direction (dx,dy), centered on 'center',
    // 'radius' cells each side, encoded as a string for pattern matching:
    // 'X' = this player's stone, 'O' = opponent's stone, '#' = out of bounds (a hard
    // blocker, same as an opponent stone), '.' = empty.
    std::string extractLine(Move center, int dx, int dy, int radius, Player p) const;

    // Goal: counts how many free-three alignments pass through 'last' for
    // player p, across all 4 axes (horizontal, vertical, 2 diagonals).
    // will be used to implement the double-three rule: if a move creates 2+ free-threes, it is illegal.
    int countFreeThrees(Move last, Player p) const;

    // Goal: evaluates a move for legality by temporarily placing a stone, 
    // checking if it creates any free-threes, and whether it would capture any pairs.

    MoveEvaluation evaluateMove(Move m, Player p);

    // Goal: returns true if the move is legal for player p, false otherwise.
    bool isLegal(Move m, Player p);


private:

// Board state: 19x19 grid of Cells, plus capture counters for each player.
    Cell grid[SIZE][SIZE];
    int capturedByBlack = 0;
    int capturedByWhite = 0;

    // Goal: implements "a 5-alignment only wins if the opponent can't immediately
    // break it by capturing a pair." True if any adjacent same-color pair in the
    // line has a defender stone on one flank and an empty cell on the other.
    bool isLineVulnerable(const std::vector<Move>& line, Player attacker) const;

    bool hasPendingWin = false;
    Player pendingWinPlayer = Player::Black;
    std::vector<Move> pendingWinLine;
};