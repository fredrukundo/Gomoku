# Gomoku — Code Reference

Plain-language explanation of every function in the project: what it does and why it
exists. Keep this at `docs/CODE_REFERENCE.md` in the repo.

This doubles as defense preparation. The subject requires you to explain the Minimax
implementation and the heuristic *thoroughly* — if you can't explain an entry here
without opening the code, revisit it before the defense.

Organized by module, matching the folder layout. Within each module, functions are
grouped by role rather than listed in file order.

**Contents**

- [Architecture at a glance](#architecture-at-a-glance)
- [common/Types.hpp](#commontypeshpp)
- [engine/Board](#engineboard)
- [ai/Minimax](#aiminimax)
- [ui/Renderer](#uirenderer)
- [ui/InputHandler](#uiinputhandler)
- [main.cpp](#maincpp)
- [Known limitations](#known-limitations)

---

## Architecture at a glance

Three layers, strictly one-directional. Nothing lower ever reaches upward.

```
main.cpp          game loop; owns all game state; wires the layers together
   |
   +-- ui/        Renderer (all SDL drawing), InputHandler (pixels -> cells)
   +-- ai/        Minimax (search); calls into engine, never into ui
   +-- engine/    Board (rules, captures, win conditions); depends on nothing
   +-- common/    shared plain types used by every layer
```

`Board` holds no knowledge of the AI or the UI. `Minimax` uses `Board` but never
draws. `Renderer` draws but never decides. `main.cpp` is the only place that knows
about all three.

---

## `common/Types.hpp`

Plain data types shared across every layer. Deliberately dependency-free so the UI
can talk about moves and scores without including the AI headers.

| Type | Purpose |
|---|---|
| `Cell` | What occupies a board intersection: `Empty`, `Black`, `White`. |
| `Player` | Whose turn / whose stones: `Black`, `White`. Distinct from `Cell` because a player is never "empty". |
| `Move` | An `{x, y}` board coordinate, 0-indexed. |
| `ScoredMove` | A `Move` plus the score the search assigned it. Lives here rather than in `Minimax.hpp` so the debug view can render search results without the `ui` layer including the `ai` layer. |

---

## `engine/Board`

Files: `include/engine/Board.hpp`, `src/engine/Board.cpp`

The authoritative game state and every rule from the subject. This layer is
self-contained: no SDL, no AI, no I/O. That's what makes it unit-testable in
`tests/rules_tests.cpp` without any of the rest of the program.

### Board access and basic placement

#### `Board()` — constructor
Initializes the 19x19 grid so every cell starts as `Cell::Empty`.

#### `isInBounds(x, y)`
Single source of truth for "is this coordinate actually on the board." Every function
that touches `grid[][]` goes through this first. It exists specifically because the
subject requires the program to *never* crash, and an unchecked array index is the
easiest way to violate that.

#### `isEmpty(x, y)`
Combines the bounds check and the emptiness check, since nearly every rule check
needs both together.

#### `get(x, y)`
Safe read access to a cell. Returns `Empty` for anything out of bounds rather than
indexing invalid memory, so callers never have to bounds-check before reading.

#### `placeStone(Move m, Player p)`
The only way a stone is added during real play. Refuses (returns `false`, board
unchanged) if the cell isn't empty or in bounds. Note it does **not** enforce the
double-three rule — that's `evaluateMove`'s job, and `main.cpp` calls that first.

#### `setRaw(x, y, Cell c)`
Rules-bypassing placement: no legality, no captures, no win tracking. Exists purely
for AI search, which places and removes tens of thousands of hypothetical stones per
move and must not touch real game state while doing it. Never called from the game
loop.

#### `print()`
Text dump of the board. Used for console testing before the GUI existed; retained for
debugging.

### Captures

#### `checkAndApplyCaptures(Move last, Player p)`
After a stone is placed, scans all 8 directions for the exact pattern
`mine-opponent-opponent-mine`. On a match, removes those two opponent stones and adds
2 to the capturing player's total. Because the pattern must match *exactly*, a single
stone or three-in-a-row is never captured — the subject's "you cannot be captured by
moving into a flanked position" case falls out of the pattern rather than needing
special handling.

#### `capturedBy(Player p)`
That player's running capture total. Feeds the 10-capture win condition and the side
panel display.

#### `wouldCaptureAnyPair(Move last, Player p)`
Read-only twin of `checkAndApplyCaptures`: same 8-direction check, no mutation. Lets
legality testing ask "would this move capture something?" without committing to the
move.

### Win conditions

#### `findWinningLine(Move last, Player p)`
Detects a 5-or-more alignment through the last-placed stone and returns the actual
ordered cells of that line (empty vector if none). Walks backward to the run's start
first, then forward, so the result is spatially ordered — required both for
`isLineVulnerable` and for drawing the win highlight.

#### `hasWinningLine(Move last, Player p)`
Non-allocating yes/no version of the above, for the search's hot path.
`findWinningLine` returns a `std::vector`, and at tens of thousands of calls per
search those heap allocations were pure waste when only a boolean was needed.
`findWinningLine` remains in use by the UI, which needs the actual cells.

#### `isLineVulnerable(line, attacker)`
Implements the subject's endgame-capture rule: a five only wins if the opponent can't
immediately break it by capturing a pair.

The first implementation of this was wrong and worth understanding. It checked only
pairs formed by *consecutive stones along the winning line's own axis* — which can
never trigger, because every internal pair of a solid run is flanked by another
same-colour line stone, never by an empty cell. The correct check is: for each stone
in the line, look in all 4 directions for *any* same-colour neighbour, on or off the
line's axis, forming a capturable pair (opponent stone on one flank, empty cell on
the other). A horizontal five can be broken by a vertical capture on one of its
stones.

#### `checkWinConditions(Move last, Player p)`
The single entry point called after every move. Three checks, in order:

1. **10-capture win** — immediate, no deferral.
2. **A fresh alignment** — wins instantly if not vulnerable; otherwise recorded as
   *pending*, giving the opponent exactly one move to break it.
3. **Resolution of a pending win** — after the opponent's reply, checks whether the
   line survived. If so, the win is now final and credited to the *original* player,
   not the one who just moved.

### Free-threes and legality

#### `extractLine(center, dx, dy, radius, p)`
Pulls a 1D slice of the board along one direction and encodes it as a string:
`X` = this player, `O` = opponent, `#` = off-board, `.` = empty. Turning a board
region into a short string makes free-three pattern matching readable, instead of
juggling raw coordinates.

#### `countFreeThrees(Move last, Player p)`
Counts how many free-three shapes pass through the just-placed stone across all 4
axes. Matches the straight form `.XXX.` and both broken forms `.XX.X.` and `.X.XX.`.
Only counts a match that actually covers the new stone, so pre-existing patterns
aren't attributed to this move.

#### `evaluateMove(Move m, Player p)`
The core legality check, and the implementation of the double-three rule. Temporarily
places the stone, records whether it would capture and how many free-threes it
creates, then rolls the placement back before returning. The move is illegal only
when it creates 2+ free-threes **and** captures nothing — the subject's explicit
exception being that a double-three produced by a capture is allowed.

#### `isLegal(Move m, Player p)`
Thin `bool` wrapper over `evaluateMove`, for callers that only need yes/no.

---

## `ai/Minimax`

Files: `include/ai/Minimax.hpp`, `src/ai/Minimax.cpp`

Move search. Depth-limited minimax with alpha-beta pruning, iterative deepening under
a hard time budget, move ordering, forward pruning, and a transposition table.

### Search entry points

#### `findBestMoveTimed(board, aiPlayer, timeLimitMs, depthReached)`
The entry point the game actually uses. Runs `findBestMove` at depth 1, then 2, then
3, and so on, feeding each depth's winning move into the next depth as the first move
to try. Stops the moment the time budget is spent — checked *inside* `minimax`, not
merely between depths — and discards whatever depth was mid-search when that
happened. `depthReached` and the returned move therefore always reflect a depth that
fully completed, never a partial guess.

Iterative deepening looks wasteful (each depth re-searches everything shallower) but
pays for itself twice over: it guarantees a usable answer whenever time runs out, and
the best-move reuse it enables is the single largest contributor to alpha-beta's
pruning efficiency.

#### `findBestMove(board, aiPlayer, preferredFirst)`
One fixed-depth search from the root. Generates candidates, orders them, caps the
list, tries each, and keeps the highest-scoring. `preferredFirst`, when supplied, is
moved to the front — that's the previous iteration's fully-verified answer, which
tightens alpha almost immediately.

#### `minimax(board, depth, maximizing, aiPlayer, alpha, beta)`
The recursive search itself.

`maximizing` alternates every level: true means it's the AI's simulated turn, so it
takes the highest-scoring child; false means the opponent's, so it takes the lowest —
the algorithm assumes the opponent always plays the strongest reply available. At
`depth == 0` it returns the heuristic score of the position.

**Alpha-beta**: `alpha` is the best score the maximizer can already guarantee, `beta`
the best the minimizer can. Once `alpha >= beta`, the remaining moves at that node
cannot change the outcome at any shallower level, so the loop breaks. This never
changes the answer plain minimax would give — only the time taken to reach it.

**Terminal detection**: both win conditions short-circuit here — an alignment via
`hasWinningLine`, and 10 captures via `totalCapturedBy`. Terminal scores are set far
outside the heuristic's normal range, offset by remaining depth so a faster forced
win outranks a slower one.

**Time**: `checkTimeUp` is called at node entry *and* again after move ordering,
because ordering itself costs real time that would otherwise go unmeasured between
checks.

### Board mutation during search

The search maintains its own copy of the occupancy information rather than
re-deriving it from the board, because the naive versions were re-scanning all 361
cells at every node.

#### `stonePositions` + `posIndex` — the sparse set
`stonePositions` lists every occupied cell; `posIndex` maps a cell back to its slot in
that list. This started as a simple push/pop stack, which worked only while moves were
placed and undone in strict LIFO order. Captures broke that — they remove stones from
the *middle* of the list — so removal is now swap-with-last plus an index fixup, O(1)
and order-independent. List order is arbitrary; nothing depends on it.

#### `addStone(m)` / `removeStone(m)`
Maintain the sparse set. Not called directly by search logic — always via
`applyMove`/`undoMove`, so the set can never drift from the board.

#### `syncStonePositions(board)`
Rebuilds the sparse set, the capture counters, and the Zobrist hash from the real
board. Called at the top of every `findBestMove`. This is essential: the hash is
otherwise only updated incrementally by `applyMove`/`undoMove`, but the *real* game's
moves (a human click, or the AI's own move being committed in `main.cpp`) go through
`Board::placeStone` and never touch it. Without this resync the hash would silently
drift and the transposition table would return scores from the wrong positions.

#### `applyMove(board, m, c)` / `undoMove(board, m)`
Apply and exactly reverse one hypothetical move, *including captures*. Each apply
pushes a `MoveUndo` recording the placed colour, the captured colour, and which cells
were removed — a fixed 16-entry array (8 directions × one pair) so applying a move
never allocates.

Capture detection is duplicated here rather than calling
`Board::checkAndApplyCaptures`, deliberately: that method mutates the real game's
capture counters and returns no undo information, both wrong for a hypothetical move.

#### `searchCapturedByBlack` / `searchCapturedByWhite`, `totalCapturedBy(board, p)`
Capture counts accumulated by hypothetical moves, kept separate from the board's real
counters so the search can never corrupt actual game state. `totalCapturedBy` adds the
two together wherever the true count matters — evaluation, and the 10-capture terminal
check.

### Move generation and ordering

#### `candidateMoves(board, radius = 2)`
Returns only empty cells within `radius` of an existing stone, rather than every empty
cell. This is what makes deep search possible at all: unrestricted, the branching
factor is ~359 and depth 3 alone took over two minutes when measured. Radius 2 rather
than 1 because broken-three shapes like `.XX.X.` have relevant cells two steps away.
Falls back to the centre cell on an empty board, since radius has nothing to anchor to
on the opening move.

#### `localRunScore(board, m, p)`
Estimates how strong a run player `p` would have through cell `m`, scanning outward a
bounded number of steps in each direction rather than examining the whole board.

#### `quickLocalScore(board, m, mover)`
Ordering score for one candidate: the mover's own potential *plus* the opponent's
potential at the same cell. Including the opponent's captures blocking value cheaply —
a cell that would be excellent for them is worth denying.

#### `orderMovesByQuickScore(board, moves, mover)`
Sorts candidates best-first. Alpha-beta prunes far more when good moves come first, so
ordering quality directly determines reachable depth.

An earlier version scored candidates with the full `evaluateHeuristic`, which rescans
the whole board. Accurate, but ruinous at ordering-time cost, and it made deep search
*worse* on busy boards. The lesson worth stating at a defense: an ordering heuristic
must be **cheap**, not accurate — accuracy is what the search depth itself is for.
Uses `partial_sort`, since only the kept prefix needs ordering.

#### `MAX_CANDIDATES_PER_NODE` / `MAX_CANDIDATES_AT_ROOT`
The dominant lever on reachable depth, and the most important thing to be able to
justify.

Nodes required grow as (effective branching)^depth, and under alpha-beta the effective
branching is roughly the square root of the candidate count. Cutting the per-node cap
from 12 to 6 takes effective branching from ~3.5 to ~2.5, which buys roughly three
extra depth levels — far more than any constant-factor speedup can. Measured directly:
every micro-optimization attempted produced no visible depth change, while this single
change moved depth from 7-8 to 11.

**This is forward pruning, and it must be stated plainly.** Only the top-N
heuristically-ranked moves are explored at each node, so the search is no longer exact
minimax: a genuinely best move ranked outside the top N is never seen. This is the
standard, unavoidable trade-off for real-time play — exhaustive depth-10 search on a
19x19 board is computationally impossible regardless of pruning quality. Claim the
depth, not the optimality.

### Evaluation (the heuristic)

#### `patternWeight(length, openStart, openEnd)`
The weight table translating a run's length and openness into a danger score. This
encodes the core insight that *shape matters more than stone count*: four scattered
stones are nearly worthless, while `.XXXX.` is already unstoppable. An open three
scores far above a blocked three, because it becomes an open four — two winning cells
at once, only one of which can be blocked.

#### `scorePlayerPatterns(board, p)`
Sums `patternWeight` over every run belonging to `p` across all 4 axes. Only begins
counting at a run's true start (verified by checking the cell behind it) so a run of
four isn't counted once per stone.

Iterates the sparse set rather than all 361 cells. This runs at every leaf node,
making it the most-called expensive function in the search — the difference between
~15 stones and 361 cells per call is substantial. **Dependency worth knowing**: this
requires `stonePositions` to be in sync with the board. Always true inside a search; a
standalone external call must run `syncStonePositions` first.

#### `evaluateHeuristic(board, aiPlayer)`
The evaluation function. Combines the pattern score difference (mine minus theirs)
with a capture score.

The capture term is **progressive, not linear** — `(mine² − theirs²) × 15`. Since 10
captures wins outright, the 8th stone lost matters far more than the 2nd. The original
flat weighting treated every capture as equally cheap, which is one reason the AI lost
capture races: it saw no escalating danger.

#### `evaluateStub(board, aiPlayer)`
The original "my stones minus theirs" heuristic from the search skeleton. No longer
called; retained for reference and comparison.

### Transposition table

#### `initZobrist()` / `captureHashComponent()`
Zobrist hashing: a precomputed random 64-bit number for every (cell, colour)
combination, XORed together to hash a position. XOR is its own inverse, so a stone can
be hashed in on placement and out on removal in O(1) — no re-hashing of the board at
each node.

`zobristCaptures` adds a component for each side's capture count. Without it, two
positions with identical stones but different capture totals would collide, and the
table would hand back a score computed for a different game state. The seed is fixed,
so search behaviour is reproducible run to run — useful for debugging and for
explaining a specific move choice consistently.

#### `transpositionTable`
Caches, keyed by position hash, what the search previously concluded there: score, the
depth it was searched to, whether the score is exact or a bound, and the best move
found. Two payoffs: genuine transpositions (A-then-B and B-then-A reach the same
board, constantly, in Gomoku), and iterative deepening's re-walking of shallow
positions at every new depth.

Entries are only reused when stored at a depth at least as deep as currently needed.
The stored best move is used as an ordering hint even when its depth is too shallow to
reuse the score.

**Realistic expectation, measured**: hit rates around 15% and a throughput gain around
10%. Real, but not enough to buy a depth level on its own, since depth scales
logarithmically with node count. Worth having; not the reason depth 10 was reached.

#### `getTTHits()` / `getTTMisses()` / `getTTStores()` / `getTTSize()` / `resetTTStats()`
Diagnostic counters, added when the table showed no measurable speedup and it wasn't
clear whether the cause was a bug or an expected limitation. It was the latter — but
the counters proved it rather than assuming it.

### Timing and introspection

#### `checkTimeUp()`
Returns whether the search should stop, setting `aborted` once the deadline passes.
Only reads the clock every 256 nodes, since `chrono` calls aren't free and doing it
per node measurably slows the search.

An early version checked only *between* depths, using a growth-rate guess to decide
whether the next depth would fit. That overshot a 400 ms budget by more than 4×,
because depth costs grow ~20× per level and no margin heuristic reliably out-guesses
exponential growth. Checking inside the search and aborting immediately is the fix — a
partial depth is thrown away rather than being allowed to run to completion.

#### `getRootScores()` / `getLastCompletedDepth()`
Expose every root candidate and the score the search gave it, sorted best-first.
`findBestMove` always computed these and discarded all but the winner; keeping them is
what makes the debug view possible.

Only a **fully completed** depth's scores are published — an aborted depth's partial
list would misrepresent the decision.

**Caveat to state at the defense**: because of alpha-beta, only the top move's score is
an exact evaluation. Lower-ranked moves were often cut off early and report the bound
that triggered the cut, not a full evaluation. The *ranking* is meaningful; the losing
numbers are not precise.

---

## `ui/Renderer`

Files: `include/ui/Renderer.hpp`, `src/ui/Renderer.cpp`

Owns every SDL resource and all drawing. Nothing outside this class touches SDL, so
the rest of the program never depends on the graphics library.

### Lifecycle

#### `Renderer()` / `~Renderer()`
The constructor deliberately does nothing — SDL setup lives in `init` so failure can
be reported and handled rather than thrown from a constructor. The destructor tears
down fonts, renderer, window, and the SDL/TTF subsystems in reverse order,
guaranteeing no leak even on an early return.

#### `init(fontPath, boldFontPath)`
Creates the window, renderer, and both fonts (regular and bold, for visual hierarchy).
Returns `false` on any failure so `main` can exit cleanly rather than continuing with a
null pointer.

Also enables `SDL_BLENDMODE_BLEND`. Without it SDL ignores the alpha channel entirely
and treats every colour as opaque — which silently rendered the stones'
semi-transparent shadow as a solid offset blob, most visible as a smudge on black
stones where nothing was drawn over it afterward.

#### `clear()` / `present()`
Wrap `SDL_RenderClear` (with the wood-tone background) and `SDL_RenderPresent`.

### Text and primitives

#### `drawText(text, x, y, color, font)`
Renders one line via SDL_ttf: surface, texture, blit, free both. Returns silently
without drawing if the surface can't be built — a missing label must never take down
the game.

#### `drawWrappedText(text, x, y, maxWidth, color, font, lineSpacing)`
Word-wraps to fit a pixel width, breaking only between words. Added after long status
messages (the double-three explanation in particular) overflowed the fixed-width side
panel.

#### `drawFilledCircle(centerX, centerY, radius, color)`
Filled circle drawn as one horizontal span per row. Written by hand to replace
`SDL2_gfx`, whose development headers weren't available on the build machine and
couldn't be installed without root. Since filled circles were the only thing needed
from that library, implementing it directly removed the dependency entirely.

### Board rendering

#### `drawBoard()`
Grid lines, the 9 traditional star points (4th/10th/16th intersections — decorative,
and a genuine aid for judging distance), and coordinate labels (`A`–`T` skipping `I`,
per Go convention; rows `1`–`19`). Composed from `drawGridLines`, `drawStarPoints`,
and `drawCoordinateLabels`.

#### `drawStones(board, lastMove)`
Every stone: soft offset shadow, then the stone (white ones get a darker outline so
they don't vanish against the light board), then a red ring on the most recent move.

The ring radius is kept strictly under half a cell width. An earlier version used a
larger ring that overlapped the neighbouring cell, which — combined with the
blend-mode bug above — produced the "stones don't sit cleanly" appearance on adjacent
moves.

#### `drawHoverPreview(hoverMove, p, wouldBeLegal)`
Faint preview stone under the cursor, tinted green when the move is legal and red when
it isn't. Lets a beginner see an illegal move (occupied cell, double-three) *before*
clicking, rather than discovering it from an error message afterward.

#### `drawSuggestion(m)`
Cyan target ring marking the recommended move. Drawn as a disc with the board colour
punched back out of the middle, plus a centre dot. Deliberately unlike both the red
last-move ring and the faint hover stone, since all three can be on screen at once.

#### `drawWinLine(line)`
Draws through the winning alignment, from the first cell to the last (the line arrives
spatially ordered from `findWinningLine`). Several offset parallel lines approximate a
thick stroke, since SDL draws only 1px lines natively.

### Panels and overlays

#### `PanelInfo` (struct)
Bundles everything the side panel displays. Replaced a parameter list that had grown
to six and was about to become eight; new fields can now be added without touching the
signature.

#### `drawSidePanel(info)`
The right-hand panel: current mode, whose turn (with a "(thinking...)" suffix during a
search), capture counts out of 10, the AI depth/timing readout — **this is what
satisfies the subject's mandatory AI-timer display** — a plain-language "How to win"
summary for newcomers, the keyboard controls, and any status message.

#### `drawCandidateMarkers(scores)`
Numbered markers on the cells the search actually considered, ranked best-first, top
move highlighted. Capped at 9 because a two-digit label isn't legible inside a 32px
cell. Showing *where* the AI looked, directly on the board, communicates the search far
better than a list of coordinates.

#### `drawDebugOverlay(scores, depth, forPlayerText)`
The same candidates as a ranked list with their scores, plus the depth the numbers came
from. Together with the markers, this is the "debugging process that lets you examine
the reasoning of your AI" the subject recommends for the defense.

#### `drawGameOverOverlay(winnerText)`
Dims the board (relying on the blend mode from `init`) and centres the result banner
plus a restart prompt. Centring is estimated from character count rather than measured
— adequate for a banner.

---

## `ui/InputHandler`

Files: `include/ui/InputHandler.hpp`, `src/ui/InputHandler.cpp`

#### `pixelToBoardCoord(pixelX, pixelY, outMove)`
Converts a mouse position into board coordinates — the exact inverse of `Renderer`'s
`MARGIN`/`CELL_SIZE` placement maths, so a click always lands on the same intersection
a stone would be drawn at.

Rounds to the nearest intersection, then *rejects* the click if it's further from that
intersection than `CLICK_TOLERANCE`. Without the rejection, any click vaguely near the
board would snap to whatever line happened to be closest, which feels imprecise on a
32px grid and makes misclicks likely. This is also what makes clicks on the side panel
and margins harmless.

A static method: a pure coordinate conversion with no state to hold.

---

## `main.cpp`

The entry point and game loop. Holds no game rules of its own — every move, human or
AI, goes through the same `Board` pipeline.

#### `main()`
Wrapped in a try/catch. An allocation failure raises `std::bad_alloc` rather than
returning null, and an uncaught exception would terminate the process — which the
subject treats as a non-functional project. This guarantees a clean exit with a
message, not resilience: it doesn't let the program *recover* from exhaustion, it stops
it from crashing.

Per frame, in strict order:

1. **Input** — quit, keyboard (`M` mode, `R` restart, `D` debug, `S` suggest), and
   left-clicks routed by game state.
2. **Hover** — recomputed from the live cursor position each frame.
3. **Render** — back to front: board, stones, candidate markers, suggestion, hover,
   win line, debug overlay, side panel, game-over overlay.
4. **Blocking searches** — the AI's turn and any requested suggestion.

**Why searches run after `present()`**: the search blocks for ~350 ms. Running it after
the frame is on screen guarantees "(thinking...)" is actually visible before the window
stops responding, instead of it appearing to freeze with no explanation.

#### `commitMove(m, p)` (lambda)
The single path by which a stone reaches the board: place, resolve captures, test both
win conditions, record the winning line, pass the turn. Returns the capture count so
the caller can word its own message.

The human and AI paths each had their own copy of this sequence until they were merged.
One implementation means one place for a rules bug to hide.

#### `runSearch(p, depthOut, msOut)` (lambda)
Runs the timed search for a player and captures the root scores for the debug view.
Used for both the AI's turn and the suggestion feature — identical search, different
consumer.

#### `resetGame()` (lambda)
Clears every piece of per-game state. One implementation, three triggers: `R`, a mode
switch, and clicking after game over.

#### `isHumanTurn()` (lambda)
Single source of truth for whether a click may place a stone: true for both colours in
hotseat, only for Black against the AI.

#### AI legality fallback
The search generates candidates without checking the double-three rule, so the AI's
chosen move is verified with `isLegal` before being played. If it somehow fails, the
game scans for any legal cell rather than applying an invalid move. A safety net, not a
strategy — see Known limitations.

---

## Known limitations

Be upfront about these at the defense. A grader will respect a clear-eyed account of
what the implementation doesn't do far more than an overclaim they then disprove.

**The search doesn't enforce the double-three rule.** `candidateMoves` returns any
empty cell in range without consulting `evaluateMove`, so the search can in principle
recommend a move that would be illegal for a human. `main.cpp` verifies the chosen move
and falls back to a legal cell if needed, which prevents an illegal move ever being
played — but the search still explores lines that couldn't legally occur. The proper
fix is legality filtering inside candidate generation, at some cost per node.

**The search is not exact minimax.** Forward pruning (top-6 candidates per node) means
a genuinely best move ranked outside that cut is never explored. Deliberate and
necessary; see `MAX_CANDIDATES_PER_NODE`.

**Root scores below the top one are bounds, not evaluations.** An alpha-beta
consequence, surfaced directly in the debug view. The ranking is meaningful; the
individual losing numbers are not.

**Reported depth varies with position.** Iterative deepening under a fixed time budget
means depth is a readout, not a setting — quiet, crowded positions reach less depth
than forcing ones where pruning cuts hard. Quote the range you actually observe in real
games, not a single best-case number from a synthetic test board.

**Out-of-memory handling is a graceful exit, not recovery.** See `main()`.