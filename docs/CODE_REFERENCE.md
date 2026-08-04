# Gomoku — Code Reference

Plain-language explanation of what every finished function does and why it exists.
Updated at the end of each step — keep this in `docs/CODE_REFERENCE.md` in the repo.
This is also your defense-prep cheat sheet: if you can't explain an entry here without
looking at the code, revisit it before the defense.

---

## `engine/Board.hpp` / `Board.cpp`

### `Board()` — constructor
Initializes the 19x19 grid so every cell starts as `Cell::Empty`.

### `isInBounds(x, y)`
Single source of truth for "is this coordinate actually on the board." Every function
that touches `grid[][]` goes through this first — this exists specifically because the
subject requires the program to *never* crash, and an unchecked index is the easiest
way to violate that.

### `isEmpty(x, y)`
Combines the bounds check and the emptiness check into one call, since almost every
rule check needs both together.

### `get(x, y)`
Safe read access to a cell. Returns `Empty` for anything out of bounds instead of
crashing, so callers never need to bounds-check before reading.

### `placeStone(Move m, Player p)`
The only way a stone gets added to the board. Rejects the move (returns `false`,
doesn't mutate the board) if the target cell isn't empty or in bounds. Does **not**
yet enforce full legality (double-three rejection comes later, in Steps 6–7) — at
this stage it only guarantees basic physical validity.

### `print()`
Text dump of the board, used for console-testing before the GUI exists (Step 14+).
Will be dropped/unused once the real renderer is built.

### `checkAndApplyCaptures(Move last, Player p)`  *(Step 4)*
After a stone is placed, scans all 8 directions for the exact pattern
`mine-opponent-opponent-mine`. On a match, removes the two opponent stones and adds
2 to that player's capture count. Only an *exact* pair matches — this is why 1 stone
or 3+ stones in a row are never captured, without needing special-case code (see the
"safe move into a capture" test from Appendix VI).

### `capturedBy(Player p)`  *(Step 4)*
Exposes each player's running capture total. Needed for the 10-capture win condition.

### `findWinningLine(Move last, Player p)`  *(Step 5)*
The sole win-detection function (Step 3's separate `checkWin` was removed once this
superseded it). Returns the actual ordered list of board cells making up a 5+
alignment through the last-placed stone, or an empty vector if there isn't one.
Walks backward to the start of the run first, then forward, so the result is
spatially ordered — required for `isLineVulnerable` to correctly check same-color
neighbors of each line stone.

### `isLineVulnerable(line, attacker)`  *(Step 5, corrected)*
Implements the rule "a 5-alignment only wins if the opponent can't immediately break
it by capturing a pair." **Important correction from the first version:** it does
*not* just check pairs formed by consecutive stones along the winning line's own
axis — that can never trigger, since every internal pair in a solid run is flanked
by another same-color line stone, never an open/opponent flank. The real check is:
for every stone in the line, look in all 4 directions for *any* same-color neighbor
(on or off the line's own axis) that forms a capturable pair — one flank already
the opponent's, the other flank empty. A single stone in the winning line can be
vulnerable through a completely different-axis pair (e.g. a horizontal line is
broken via a vertical capture on one of its stones).

### `checkWinConditions(Move last, Player p)`  *(Step 5)*
The single entry point `main.cpp` calls after every move. Combines three checks in
order:
1. **10-capture win** — direct, no deferral.
2. **A fresh alignment from this move** — finalized immediately if not vulnerable,
   or marked "pending" (giving the opponent one move to break it) if it is.
3. **Resolution of a previously pending win** — after the opponent's response, checks
   whether the line they had one chance to break is still intact. If so, the win is
   now final, credited to the original player (not the one who just moved).

### `extractLine(center, dx, dy, radius, p)`  *(Step 6)*
Pulls a 1D slice of the board along one direction, centered on a cell, and encodes
it as a string (`X` = this player, `O` = opponent, `#` = off-board, `.` = empty).
Turning the board into a short string like this is what makes pattern-matching for
free-threes simple and readable, instead of juggling raw coordinates everywhere.

### `countFreeThrees(Move last, Player p)`  *(Step 6)*
Counts how many free-three shapes (straight `.XXX.` or broken `.XX.X.` / `.X.XX.`)
pass through the just-placed stone, checked across all 4 axes. Used directly by
Step 7's double-three legality rule.

### `wouldCaptureAnyPair(Move last, Player p)`  *(Step 7)*
Read-only twin of `checkAndApplyCaptures` — checks the same 8-direction flank
pattern but never mutates the board. Exists so legality checks can ask "would this
move capture anything?" without committing to the move first.

### `evaluateMove(Move m, Player p)`  *(Step 7)*
The core legality check. Temporarily places the stone, reads off whether it would
capture anything and how many free-threes it creates, then rolls the placement back
before returning. A move is illegal only if it creates 2+ free-threes **and** would
capture nothing — this is the subject's explicit exception: a double-three created
as a side effect of a capture is allowed.

### `isLegal(Move m, Player p)`  *(Step 7)*
Thin `bool`-only wrapper around `evaluateMove`, for callers (like the GUI later)
that just need a yes/no without the detail.

### `setRaw(x, y, Cell c)`  *(Step 9)*
Raw, rules-bypassing stone placement — no legality, capture, or win-state logic.
Exists purely for AI search, which tries and undoes thousands of hypothetical
moves per real move and can't afford (or want) the full rules pipeline each time.
Never called from the real game loop, only from search code.

---

## `ai/Minimax.hpp` / `Minimax.cpp`

### `Minimax(int maxDepth)` — constructor  *(Step 9)*
Stores the search depth. Note: `maxDepth` is later overwritten on every iteration
by `findBestMoveTimed` — the constructor's value only matters if `findBestMove` is
called directly, standalone, without going through the timed wrapper.

### `candidateMoves(board, radius = 2)`  *(Step 10)*
Returns only empty cells within `radius` of an existing stone, instead of every
empty cell on the board. This is what makes deep search conceivable at all —
without it, the branching factor is ~359 and depth 3 alone took over two minutes
(measured directly in Step 9). Falls back to the center cell when the board is
completely empty, since radius has nothing to anchor to on the opening move.

### `patternWeight(length, openStart, openEnd)`  *(Step 11)*
A lookup table translating a run's length and openness into a danger score — the
core of "shape matters more than stone count." An open three (free-three, same
concept as Step 6) scores far higher than a blocked one, because it can become an
unstoppable open four unless blocked immediately.

### `scorePlayerPatterns(board, p)`  *(Step 11)*
Scans the whole board once, finds every run belonging to player `p` in all 4
directions, and sums `patternWeight` over each one. Only starts counting at the
true beginning of a run (checks the cell just behind isn't the same player) so a
single run isn't counted once per stone in it.

### `evaluateHeuristic(board, aiPlayer)`  *(Step 11)*
The real evaluation function, replacing the Step 9 stub. Combines
`scorePlayerPatterns(aiPlayer) - scorePlayerPatterns(opponent)` with a weighted
capture-count difference, since reaching 10 captures is an independent win
condition worth scoring on its own, not just a side effect of stone totals.

### `evaluateStub(board, aiPlayer)`  *(Step 9, superseded)*
The original "mine minus theirs" stone-counting heuristic, kept only for
reference/comparison — no longer called anywhere in the search. Candidate for
deletion once `evaluateHeuristic` has been trusted for a while (same treatment as
the old `checkWin`).

### `minimax(board, depth, maximizing, aiPlayer, alpha, beta)`  *(Step 9, updated Steps 12–13)*
The recursive search itself. `maximizing` alternates each level — true means it's
the AI's simulated turn (pick the highest-scoring move), false means the
opponent's simulated turn (assumed to play optimally against the AI, so pick the
lowest-scoring move). At `depth == 0`, returns the heuristic score of that
hypothetical board. Uses `findWinningLine` (side-effect-free) as a short-circuit —
a forced win/loss scores far outside the heuristic's normal range so it always
dominates. **Step 12** added `alpha`/`beta`: once `alpha >= beta`, the rest of the
current node's moves are skipped, since they can't change the outcome at a
shallower level — same final answer as plain minimax, just without wasted work.
**Step 13** added a periodic deadline check (every 1024 nodes, to keep the
`chrono` call overhead negligible) that sets `aborted = true` and unwinds
immediately once the time budget is spent mid-search.

### `findBestMove(board, aiPlayer, preferredFirst = nullptr)`  *(Step 9, updated Steps 12–13)*
The entry point for one fixed-depth search: tries every candidate move at the
root, keeps whichever scores highest. **Step 13** added `preferredFirst` — when
given, that move is moved to the front of the candidate list before searching, so
alpha-beta tightens its bounds almost immediately instead of only after finding a
good move by chance. This is what actually gives iterative deepening its pruning
payoff (a first version of Step 13 described this reuse but never implemented it —
caught and fixed once real testing showed depth stalling out far below target).

### `findBestMoveTimed(board, aiPlayer, timeLimitMs, depthReached)`  *(Step 13)*
The real entry point used by the game. Runs `findBestMove` at depth 1, then 2,
then 3... feeding each depth's winning move into the next depth's search via
`preferredFirst`. Stops the instant the time budget is spent (checked inside
`minimax`, not just between depths) and discards whatever depth was mid-search
when that happened — `depthReached` and the returned move only ever reflect a
depth that *fully completed*, so the move handed back is always trustworthy, not
a partial guess.

---

## `main.cpp`

### `main()`
Minimal console harness for testing engine logic before the GUI exists. Alternates
turns, reads coordinates, validates input without crashing, and routes every move
through the Board's public API in order: `placeStone` → `checkAndApplyCaptures` →
`checkWinConditions`. This loop is a temporary stand-in for what the real game loop
(GUI-driven) will do starting at Step 14 — the sequence of API calls stays the same,
only the input/output mechanism changes.



#include "engine/Board.hpp"
#include "ai/Minimax.hpp"
#include <iostream>
#include <chrono>

int main() {
    Board board;
board.setRaw(5, 5, Cell::Black);
board.setRaw(6, 5, Cell::Black);
board.setRaw(7, 5, Cell::Black);

Minimax ai(1);
int depthReached = 0;

auto start = std::chrono::steady_clock::now();
SearchResult result = ai.findBestMoveTimed(board, Player::White, 400.0, depthReached);
auto end = std::chrono::steady_clock::now();
double ms = std::chrono::duration<double, std::milli>(end - start).count();

std::cout << "Move: (" << result.bestMove.x << "," << result.bestMove.y << ")\n";
std::cout << "Score: " << result.score << "\n";
std::cout << "Depth reached: " << depthReached << "\n";
std::cout << "Total time: " << ms << " ms\n";
    return 0;
}