# Theory Behind Gomoku

A conceptual walkthrough of the whole project, for someone who has never seen this
codebase before.

This document assumes you know the rules of the game. It does not re-teach them — it
shows **where each rule lives in the code**, how a move travels through the program,
and what actually differs between the two game modes.

No code is reproduced here. This is the map, not the territory. For exact function
signatures and behaviour see `CODE_REFERENCE.md`; for the algorithm and heuristic in
depth see `DEFENSE_NOTES.md`.

**Contents**

- [What the program is](#what-the-program-is)
- [The three layers](#the-three-layers)
- [How the board is stored](#how-the-board-is-stored)
- [The life of a single move](#the-life-of-a-single-move)
- [Where each rule lives](#where-each-rule-lives)
- [Two boards: the real one and the imagined one](#two-boards-the-real-one-and-the-imagined-one)
- [How the AI thinks](#how-the-ai-thinks)
- [The two game modes](#the-two-game-modes)
- [Critical points: human vs human](#critical-points-human-vs-human)
- [Critical points: human vs AI](#critical-points-human-vs-ai)
- [How the screen is drawn](#how-the-screen-is-drawn)
- [Where state lives, and why undo works](#where-state-lives-and-why-undo-works)
- [Suggested reading order](#suggested-reading-order)

---

## What the program is

A single executable, `Gomoku`, that opens a window showing a 19×19 board and lets you
play the game in two ways: against a computer opponent, or against another person
sitting at the same keyboard.

Everything runs in one process, single-threaded. There is no network, no database, no
config file. The program starts, opens a window, loops until you close it, exits.

The interesting part is the computer opponent. It doesn't follow a script or a table
of openings — it looks ahead through possible futures of the game and picks the move
that leads to the best outcome it can find within a fixed time budget.

---

## The three layers

The code is split into three layers with a strict rule: **each layer knows only about
the ones below it, never above.**

```
        main.cpp
        the game loop. Owns all game state.
        The only file that knows all three layers exist.
              |
    +---------+---------+
    |                   |
   ui/                 ai/
   Renderer            Minimax
   InputHandler        (the opponent's brain)
    |                   |
    +---------+---------+
              |
           engine/
           Board  — the rules. Knows nothing else exists.
              |
           common/
           Types  — plain data shared by everyone
```

Read that from the bottom up:

**`common/Types.hpp`** holds three tiny data types with no behaviour: `Cell` (what
occupies an intersection: empty, black, white), `Player` (whose turn it is), and
`Move` (an x,y coordinate). Everything else is built from these.

**`engine/Board`** is the rulebook. It knows what a legal move is, what happens when
stones are captured, and when someone has won. It does not know a screen exists, and
it does not know an AI exists. That isolation is what makes it testable — `make test`
runs the rules with no graphics at all.

**`ai/Minimax`** is the opponent. It *uses* the Board to explore possible futures,
but it never draws anything and never decides when it's its turn.

**`ui/Renderer` and `ui/InputHandler`** handle pixels. The Renderer owns every
graphics call in the program; nothing outside it touches the graphics library.
InputHandler translates a mouse click position into a board coordinate.

**`main.cpp`** wires it together. It holds the actual game state and decides, each
frame, what should happen. It contains no rules of its own.

**Why this matters:** you can reason about any one piece without holding the others
in your head. A bug in how stones are drawn cannot be a rules bug, because the
drawing code cannot change the board.

---

## How the board is stored

A 19×19 grid, one `Cell` per intersection. That's it — a simple two-dimensional array
inside `Board`.

Coordinates are **0-indexed internally** (`0` to `18`) but **1-indexed on screen**
(rows `1`–`19`, columns `A`–`T`). The column letters skip `I`, following the
convention used on real Go boards, so `I` is never confused with the digit `1`.

So the internal coordinate `{9, 9}` is displayed as `K10`. Two functions bridge this
gap: `InputHandler::pixelToBoardCoord` converts a click into internal coordinates, and
`Renderer::drawCoordinateLabels` prints the human-readable labels. Nothing in between
ever thinks about pixels or letters.

Alongside the grid, `Board` keeps a small amount of extra state:

- how many stones each player has captured
- whether a win is currently *pending* (explained below under the endgame-capture
  rule)

Every function that reads or writes the grid goes through `isInBounds` first. That
isn't decoration: the subject says the program must never crash under any
circumstances, and an unchecked array index is the fastest way to violate that.

---

## The life of a single move

This is the spine of the entire program. Every move — whether a human clicked it or
the AI chose it — travels the exact same path:

```
   1. Is it legal?          Board::evaluateMove
             |
             v
   2. Place the stone       Board::placeStone
             |
             v
   3. Any captures?         Board::checkAndApplyCaptures
             |
             v
   4. Did that win?         Board::checkWinConditions
             |
        +----+----+
        |         |
      won      not won
        |         |
        v         v
   game over   swap turn
```

In the code this sequence lives in exactly one place: the `commitMove` helper inside
`main.cpp`. Both the mouse-click handler and the AI turn call it.

**Why one path matters.** Earlier versions had the human path and the AI path each
running their own copy of these four steps. That meant any rules bug could exist in
one and not the other — the AI might handle a capture correctly while a human move
didn't. Merging them means there is exactly one place for such a bug to hide, and
fixing it fixes both modes at once.

Note the order. Legality is checked *before* placing, because rejecting a move must
leave the board untouched. Captures are resolved *before* the win check, because a
capture can itself be the winning move (reaching ten) or can break an opponent's
alignment.

---

## Where each rule lives

Everything below is inside `engine/Board`.

| Rule | Function | What it does |
|---|---|---|
| Stones go on empty intersections | `placeStone` | Refuses occupied or out-of-bounds cells; leaves the board unchanged when it refuses. |
| Five or more in a row wins | `findWinningLine` | Scans outward from the last stone in all four directions. Returns the actual winning cells, not just yes/no — the display needs them to draw the highlight. |
| Capturing a pair | `checkAndApplyCaptures` | Looks in all eight directions for the exact pattern *mine, opponent, opponent, mine*. On a match, removes those two stones and adds two to the capture count. |
| Only exact pairs can be captured | (same function) | Falls out of the pattern being exact. A lone stone doesn't match; three in a row doesn't match. No special-case code needed. |
| You are safe moving into a flanked spot | (same function) | Same reason. Completing three-in-a-row between two enemy stones simply doesn't match the pattern, so nothing happens. |
| Ten captured stones wins | `checkWinConditions` | Checked first, before anything else. It's an immediate win with no exceptions. |
| A five only wins if it can't be broken | `isLineVulnerable` | Checks whether any pair inside the winning line could be captured next move. If so, the win is held *pending* and the opponent gets exactly one move to break it. |
| Resolving a pending win | `checkWinConditions` | After the opponent's reply, checks whether the line survived. If it did, the win is finalised and credited to the original player — not the one who just moved. |
| What counts as a free-three | `countFreeThrees` | Recognises the straight shape and both broken shapes, in all four directions, and only counts patterns that actually include the newly placed stone. |
| Double-three is forbidden | `evaluateMove` | Temporarily places the stone, counts the free-threes it would create, then removes it again. Two or more means illegal. |
| …unless it comes from a capture | `evaluateMove` | The same temporary placement also records whether the move would capture. If it would, the double-three is permitted — exactly as the subject specifies. |

Two of these deserve a closer look.

### The endgame-capture rule

This is the rule most people get wrong, because it contradicts ordinary Gomoku. Under
these rules, five in a row is **not automatically a win**. If the opponent can break
that line by capturing a pair from it, they get one move to try.

So `checkWinConditions` has three branches, evaluated in order: the ten-capture win
(immediate), a fresh alignment (immediate *if* not vulnerable, otherwise recorded as
pending), and the resolution of a pending win from the previous turn.

A subtlety worth knowing: the pair that breaks the line does **not** have to lie along
the same direction as the line. A horizontal five can be broken by capturing
vertically, if one of its stones happens to have a friendly neighbour above or below
it that's capturable. An early version of `isLineVulnerable` only checked along the
line's own direction, which meant it could never trigger at all — every internal pair
in a solid run is flanked by more of the same run, never by an empty cell.

### The double-three check

The trick here is that legality can only be determined by *simulating* the move. You
cannot tell whether a move creates two free-threes without putting the stone down and
looking.

So `evaluateMove` places the stone temporarily, measures, and rolls it back before
returning. Nothing outside ever sees the intermediate state. `isLegal` is a thin
wrapper that returns only the yes/no.

---

## Two boards: the real one and the imagined one

This is the single most important idea for understanding the AI.

When the AI thinks, it plays out thousands of hypothetical moves. It needs to put a
stone down, look at the result, and take it back — over and over, very fast. It
cannot use the same functions a real move uses, for two reasons:

1. **Speed.** The real path runs legality checks, updates capture counters, and
   tracks pending wins. Doing that tens of thousands of times per second would be far
   too slow.
2. **Correctness.** A hypothetical move must not change the real game. If the AI's
   imagination incremented the real capture counter, the actual score would be wrong.

So `Board` exposes a second, deliberately unsafe way to place a stone: `setRaw`. It
writes a cell and does nothing else — no rules, no counters, no checks. It exists
purely for search, and the game loop never calls it.

The AI builds its own machinery on top:

- It keeps its **own** capture counters, separate from the board's, so it can imagine
  captures without corrupting the real count.
- It records enough information about each hypothetical move to reverse it exactly,
  including which stones a capture removed.
- It maintains a list of which cells are occupied, so it never has to scan all 361
  cells to find them.

The rule to remember: **`placeStone` is for things that really happen; `setRaw` is for
things the AI is only imagining.**

---

## How the AI thinks

The full explanation is in `DEFENSE_NOTES.md`. Here is the shape of it.

**The tree.** From the current position, every possible move leads to a new position,
which has its own possible moves. That branching structure is a tree of possible
futures.

**Alternating perspective.** On levels where it's the AI's turn, it picks the
best-scoring option. On levels where it's the opponent's turn, it assumes they pick
the *worst* option for the AI. That alternation is the "min" and "max" in minimax.
Planning against a perfect opponent means the plan survives an imperfect one.

**Scoring the leaves.** The tree can't be explored to the end of the game, so at a
fixed depth the AI stops and *estimates* how good the position is. That estimate is
the heuristic (`evaluateHeuristic`), and it's the part with real Gomoku knowledge in
it: it scores runs of stones by length and by how free they are to grow, counts
captures, notices pairs hanging in danger, and does all of it for both players — the
final score is the difference between them, which is why the AI defends without any
separate defensive logic.

**Not exploring everything.** A 19×19 board has hundreds of empty cells, and looking
at all of them at every level is hopeless. Two restrictions make it feasible:
`candidateMoves` only considers cells near existing stones, and after ranking those
by a quick estimate, only the best handful are actually searched.

**Alpha-beta pruning.** While searching, the AI tracks what it can already guarantee.
The moment a branch proves it can't beat that, it abandons the branch. This finds the
same answer as searching everything, just faster.

**Iterative deepening.** Rather than fix a depth, the AI searches one level, then two,
then three, keeping the deepest result that *finished* before time ran out. If time
expires mid-level, that level is discarded entirely — a half-finished search has a
"best move" that simply hasn't met its competition yet.

**Remembering positions.** Playing move A then B reaches the same board as B then A.
The AI hashes each position to a number and caches what it concluded, so it doesn't
re-solve the same board twice.

---

## The two game modes

There are two: **Human vs AI** and **Hotseat** (two people at one keyboard). The `M`
key switches between them, which also starts a fresh game — carrying a half-played
board across modes would leave it ambiguous who owns which colour.

Here is the honest answer to "how much code is different": **almost none.**

### What is identical

- The rules. `Board` has no idea a mode exists.
- The move pipeline. Both modes call the same `commitMove`.
- Rendering. The board, stones, highlights, and win line are drawn the same way.
- The win/game-over handling.
- The move suggestion feature. Available in both.
- Undo and redo.

### What actually differs

Three things, and they're all small.

**1. Who is allowed to click.** A single helper, `isHumanTurn`, answers this. In
hotseat it is always true — either colour can be placed by clicking. Against the AI it
is true only when it's Black's turn, since White belongs to the engine.

**2. Whether the engine moves on its own.** Near the end of each frame, `main.cpp`
checks whether it's the AI's turn. In hotseat that condition is never met, so the
engine never plays by itself. That's the whole implementation of "hotseat" — the
engine block simply doesn't run.

**3. How far undo steps back.** In hotseat, undo removes one move. Against the AI it
rewinds until it's your turn again — usually two plies. Stopping on the AI's turn
would be useless: the engine would instantly replay its move and you'd never be able
to take anything back.

Everything else — panel wording, section labels — is cosmetic.

### The suggestion feature

The subject requires hotseat to include a move-suggestion feature. The implementation
is a single insight: **the same search that plays the AI's moves can be asked for a
recommendation instead of a move.**

Pressing `S` runs `findBestMoveTimed` for whoever is currently to play, then draws a
marker on the recommended cell. It does not place a stone. The player can follow it or
ignore it.

The important detail is *whoever is currently to play*. The search is not hard-coded
to a colour — it takes the player as a parameter. That's what makes the same feature
work for Black, for White, and against the AI, without three implementations.

---

## Critical points: human vs human

**The engine must be impartial.** Nowhere in `Board` is there logic that treats Black
differently from White. Both players go through identical checks. If a rule applied to
one and not the other, hotseat would immediately expose it — which is one reason
hotseat is a useful mode to test with even though the AI is the interesting part.

**Turn alternation must survive rejection.** If a player attempts an illegal move, the
turn must *not* pass to their opponent. They get to try again. This works because the
turn only changes inside `commitMove`, and `commitMove` is only reached after legality
has already passed. A rejected click never reaches it.

**Suggestions must follow the turn.** If pressing `S` always suggested for Black, it
would be worse than useless for White — it would recommend moves that help the
opponent. The search takes the current player as an argument for exactly this reason.

**Both players see the same information.** The capture counts, the last-move marker,
the legality preview under the cursor — all of it is shown regardless of whose turn it
is. There is no hidden state.

---

## Critical points: human vs AI

**The interface must not appear frozen.** The search blocks for roughly a third of a
second. If it ran before the screen updated, the window would simply stop responding
with no explanation, and the player wouldn't know whether it had crashed.

The solution is ordering. Each frame draws the screen and *then* runs the search. This
guarantees the "(thinking...)" label is visibly on screen before the pause begins.
That ordering is deliberate and is the one thing in the game loop that would be easy
to break by accident.

**The AI must never play an illegal move.** Its search generates candidate moves for
speed, and filters out ones that would break the double-three rule. But the final
chosen move is checked *again* with `isLegal` before it's played, and if that check
ever failed, the program would fall back to scanning for any legal cell rather than
placing an invalid stone. That second check should never trigger — it exists because
"should never" is not "cannot," and an illegal stone on the board would be a rules
failure a grader could see.

**The time limit is a hard requirement.** The subject fails the project if the AI
averages over half a second per move. The search is given a budget below that, leaving
margin for the window drawing that shares the same frame. Crucially, the deadline is
checked *inside* the search, not just between levels — an earlier version that only
checked between levels overshot its budget by more than four times, because each level
costs roughly twenty times the last and no estimate reliably predicts that.

**The AI's move goes through the same pipeline.** It doesn't get a shortcut. Its move
is placed with `placeStone`, its captures resolved with `checkAndApplyCaptures`, its
win checked with `checkWinConditions` — identical to a human's. The engine has no
privileges over the rules.

**Clicks during the AI's turn are ignored.** `isHumanTurn` is false while White is to
move, so a click that lands in that window does nothing rather than queueing up a
phantom move.

---

## How the screen is drawn

The window is redrawn from scratch roughly sixty times a second. Nothing is
incrementally updated; each frame paints everything in a fixed back-to-front order:

```
   board background and grid
        stones
             candidate markers  (debug view, if enabled)
                  suggestion marker  (if requested)
                       hover preview  (under the cursor)
                            win line  (if the game is over)
                                 debug overlay  (if enabled)
                                      side panel
                                           game-over banner
```

Order matters. The hover preview must be drawn after the stones or it would be hidden
behind them. The game-over banner is last because it dims everything beneath it.

The **side panel** on the right holds whose turn it is, both capture counts, the
keyboard controls, a short reminder of how to win, and the AI's search depth and
timing for its last move. That last item is not decoration — the subject requires a
visible timer showing how long the AI took, and states plainly that its absence fails
the project.

Three different markers can appear on the board at once — the red ring on the last
move, the faint tinted stone under the cursor, and the cyan target on a suggested
move. They're deliberately different shapes and colours so they're never confused.

---

## Where state lives, and why undo works

All game state lives in `main.cpp` as ordinary local variables: the board, whose turn
it is, the last move, whether the game is over, the winning line, the current status
message. Nothing is global, and nothing is hidden inside the UI.

That decision is what makes undo simple. `Board` is a copyable value — copying it
copies the grid, the capture counts, and the pending-win state together. So undo
doesn't need to reverse a move at all. Before each move, the program stores a complete
copy of the current state on a stack. Undoing means restoring the most recent copy.

The alternative — un-capturing stones, rewinding counters, un-setting the pending win
by hand — is far more error-prone for no real benefit. A snapshot is a few hundred
bytes, and a full game costs well under a megabyte.

Redo is the mirror image: rewound states go onto a second stack, and playing a new
move clears it, exactly as any editor behaves.

One thing undo deliberately does *not* touch is the AI's position cache. That cache is
keyed by board position, not by move history, and the search re-reads the board at the
start of every search — so rewinding cannot corrupt it.

---

## Suggested reading order

If someone wants to read the code, this order builds understanding fastest:

1. **`common/Types.hpp`** — a minute. Three types, no behaviour.
2. **`engine/Board.hpp`** — the header alone. This is the complete list of things the
   rules can do, without the implementation noise.
3. **`tests/rules_tests.cpp`** — each test names a rule and shows the expected
   outcome. Effectively an executable specification.
4. **`main.cpp`, the `commitMove` helper** — the four-step move pipeline. Everything
   the game does happens through it.
5. **`main.cpp`, the game loop** — input, then hover, then draw, then search. The
   ordering is the design.
6. **`ai/Minimax.cpp`, the `minimax` function** — the recursion itself. Read
   `DEFENSE_NOTES.md` alongside it.
7. **`ai/Minimax.cpp`, `evaluateHeuristic`** — where the actual Gomoku knowledge
   lives.
8. **`ui/Renderer.cpp`** — last, and only if you care about the drawing. It has no
   influence on how the game plays.