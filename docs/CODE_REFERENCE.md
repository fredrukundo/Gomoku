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

---

## `main.cpp`

### `main()`
Minimal console harness for testing engine logic before the GUI exists. Alternates
turns, reads coordinates, validates input without crashing, and routes every move
through the Board's public API in order: `placeStone` → `checkAndApplyCaptures` →
`checkWinConditions`. This loop is a temporary stand-in for what the real game loop
(GUI-driven) will do starting at Step 14 — the sequence of API calls stays the same,
only the input/output mechanism changes.