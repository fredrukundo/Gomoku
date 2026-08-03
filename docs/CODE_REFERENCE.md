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

### `checkWin(Move last, Player p)`  *(Step 3)*
Detects a simple 5-or-more-in-a-row through the last-placed stone, scanning outward
from that single point across all 4 axes (horizontal, vertical, both diagonals)
instead of rescanning the whole board. Left untouched by Step 5's more complete logic
so the already-tested Step 3 behavior doesn't get disturbed.

### `checkAndApplyCaptures(Move last, Player p)`  *(Step 4)*
After a stone is placed, scans all 8 directions for the exact pattern
`mine-opponent-opponent-mine`. On a match, removes the two opponent stones and adds
2 to that player's capture count. Only an *exact* pair matches — this is why 1 stone
or 3+ stones in a row are never captured, without needing special-case code (see the
"safe move into a capture" test from Appendix VI).

### `capturedBy(Player p)`  *(Step 4)*
Exposes each player's running capture total. Needed for the 10-capture win condition.

### `findWinningLine(Move last, Player p)`  *(Step 5)*
Like `checkWin`, but returns the actual ordered list of board cells making up the
winning alignment instead of just yes/no. Walks backward to the start of the run
first, then forward, so the result is spatially ordered — required for
`isLineVulnerable` to correctly check *adjacent* pairs within the line.

### `isLineVulnerable(line, attacker)`  *(Step 5)*
Implements the rule "a 5-alignment only wins if the opponent can't immediately break
it by capturing a pair." Checks every adjacent same-color pair inside the winning
line for the classic capture setup: a defender stone already on one flank, and an
empty cell on the other flank the defender could play into.

### `checkWinConditions(Move last, Player p)`  *(Step 5)*
The single entry point `main.cpp` calls after every move. Combines three checks in
order:
1. **10-capture win** — direct, no deferral.
2. **A fresh alignment from this move** — finalized immediately if not vulnerable,
   or marked "pending" (giving the opponent one move to break it) if it is.
3. **Resolution of a previously pending win** — after the opponent's response, checks
   whether the line they had one chance to break is still intact. If so, the win is
   now final, credited to the original player (not the one who just moved).

---

## `main.cpp`

### `main()`
Minimal console harness for testing engine logic before the GUI exists. Alternates
turns, reads coordinates, validates input without crashing, and routes every move
through the Board's public API in order: `placeStone` → `checkAndApplyCaptures` →
`checkWinConditions`. This loop is a temporary stand-in for what the real game loop
(GUI-driven) will do starting at Step 14 — the sequence of API calls stays the same,
only the input/output mechanism changes.