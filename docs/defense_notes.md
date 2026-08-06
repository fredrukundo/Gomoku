# Gomoku — Defense Notes

Everything you need to explain at the evaluation, written to be spoken aloud.

The eval sheet is blunt about this:

> IF THE STUDENTS CAN NOT EXPLAIN THEIR ALGORITHM IN DETAIL, THEIR IMPLEMENTATION IS
> WORTH EXACTLY NOTHING, SO DO NOT GRADE THIS SECTION.

The same warning is repeated for the heuristic. Working code earns zero on both
sections if you can't explain it. This document exists so that doesn't happen.

**Contents**

- [How to use this](#how-to-use-this)
- [Part 1 — Minimax](#part-1--minimax)
- [Part 2 — Depth](#part-2--depth)
- [Part 3 — Search space](#part-3--search-space)
- [Part 4 — The heuristic](#part-4--the-heuristic)
- [Part 5 — The eval sheet, question by question](#part-5--the-eval-sheet-question-by-question)
- [Part 6 — Gaps worth closing before the defense](#part-6--gaps-worth-closing-before-the-defense)
- [Part 7 — Hard questions and honest answers](#part-7--hard-questions-and-honest-answers)
- [Part 8 — Demo script](#part-8--demo-script)

---

## How to use this

Read it through once. Then close it and explain Part 1 and Part 4 out loud to an
empty room. Anything you stumble on, come back and reread. Explaining out loud is
the only preparation that actually works — reading feels like understanding and
isn't.

Before the defense, fill in the depth table in Part 2 with **your own measured
numbers**. Do not quote numbers from this document.

---

## Part 1 — Minimax

### The one-sentence version

*"Minimax builds a tree of possible futures. I pick the move that leads to the best
outcome, assuming my opponent always picks the worst outcome for me."*

### The game tree

Every position has many possible moves. Each move leads to a new position, which
itself has many possible moves. That structure is a tree.

```
                        current board
                     /        |        \
                  move A    move B    move C
                  /    \    /    \    /    \
                ...    ...  ...  ...  ...  ...
```

Levels alternate between the two players, because turns alternate. One level is
called a **ply**.

### Why "min" and "max"

At levels where it's **my** turn, I choose the move with the **highest** score —
I'm trying to win. Those are **MAX** levels.

At levels where it's my **opponent's** turn, they choose the move that's **worst
for me** — the lowest score. Those are **MIN** levels.

The scores come from the bottom of the tree. When we reach the depth limit, the
heuristic (Part 4) looks at the board and returns a number: positive means good for
me, negative means good for the opponent.

Then the scores **bubble upward**. At each MIN level take the minimum of the
children; at each MAX level take the maximum. The root move that produced the best
bubbled-up value is the move I play.

```
              ROOT  (MAX — my turn)          value = max(3, 7) = 7  → play B
             /                    \
        A (MIN)                    B (MIN)
       /       \                  /       \
     10         3                7         9
                ↑                ↑
        min(10,3) = 3      min(7,9) = 7
```

Notice A contains a 10 — the single best leaf in the tree — and we still don't play
A. Because at A it's the opponent's turn, and they'd choose the 3. Minimax doesn't
hope; it assumes the opponent plays well.

**Say this if asked why minimax is pessimistic:** planning against a perfect
opponent means the plan holds against any opponent. Assuming they'll blunder means
being wrong whenever they don't.

### Where this is in the code

`Minimax::minimax(board, depth, maximizing, aiPlayer, alpha, beta)` in
`src/ai/Minimax.cpp`.

The `maximizing` flag is what alternates the levels. Every recursive call flips it:

```cpp
score = minimax(board, depth - 1, !maximizing, aiPlayer, alpha, beta);
                             ↑         ↑
                    one ply shallower   other player's turn
```

At `depth == 0` it stops recursing and calls `evaluateHeuristic`.

### Alpha-beta pruning

This is the "improved implementation" the eval sheet awards 5 for. It finds the
**exact same move** as plain minimax, just without wasting time on branches that
cannot matter.

Two running bounds:

- **alpha** — the best score the maximizer can already guarantee somewhere else
- **beta** — the best score the minimizer can already guarantee somewhere else

The rule: **when `alpha >= beta`, stop searching this node.**

#### A worked example — memorize this one

```
                 ROOT  (MAX)
                /            \
          A (MIN)             B (MIN)
          /    \              /     \
        5       3           2        ✗  ← never examined
```

Step by step:

1. Search A. Its children are 5 and 3. A is a MIN node, so A = 3.
2. Root is MAX, so it can now guarantee **at least 3**. Set `alpha = 3`.
3. Start searching B. Its first child is 2. B is MIN, so `beta = 2`.
4. Check: `alpha (3) >= beta (2)`. **Stop.** B's second child is never looked at.

Why is that safe? B is a MIN node, so B's final value can only go **down** from 2 as
more children are examined. It's already ≤ 2, and A already guarantees 3. B can
never beat A, so finishing B would be wasted work. The answer is identical either
way.

**The line that does it**, at the end of the move loop in `minimax()`:

```cpp
if (alpha >= beta) break;
```

**Measured effect:** at depth 3 on a test position, search time went from 379 ms to
89 ms — about 4× — with an identical chosen move and identical score. That
identical-result check is how I verified pruning hadn't broken correctness.

### Move ordering — why it matters more than it looks

Alpha-beta only prunes when it finds a good move **early**. If the best move is
tried last, alpha stays low the whole time and nothing gets cut.

So before searching a node's children, they're sorted best-first by a cheap
estimate: `orderMovesByQuickScore` → `quickLocalScore`.

That estimate scans only a few cells around each candidate. **It is deliberately
cheap, not accurate** — and that was a real lesson. An earlier version ordered moves
using the full `evaluateHeuristic`, which rescans the whole board. It was more
accurate and it made search *worse*, because the ordering cost outweighed the extra
pruning. Accuracy is what the search depth is for; ordering just needs to get the
good moves near the front.

`quickLocalScore` scores a cell as **my potential + the opponent's potential at that
same cell**. Including the opponent's value captures blocking cheaply: a cell that
would be great for them is worth denying.

### Iterative deepening

The AI doesn't search a fixed depth. It searches depth 1, then depth 2, then depth
3, and so on until the time budget runs out — keeping the deepest **completed**
result.

```
   0 ms                                                350 ms
   |----|--------|-------------------|-----------------|
     d=1   d=2          d=3                 d=4       ⏱ time up
      ✓     ✓            ✓                   ✗ discarded
                                       answer used: depth 3
```

This looks wasteful — each pass redoes everything shallower. Three reasons it wins:

1. **There's always an answer.** However little time is left, a completed depth is
   already in hand. A fixed-depth search that runs out of time has nothing.
2. **Move ordering gets much better.** Each depth's best move is tried first at the
   next depth (`preferredFirst`), which tightens alpha immediately — exactly what
   alpha-beta needs.
3. **The shallow passes are cheap.** Cost grows roughly 20× per level, so every pass
   before the last one together costs a small fraction of the final pass.

**Critical detail:** an interrupted depth is **thrown away entirely**, not used
partially. A half-searched move list would report a "best" move that simply hadn't
met its competition yet.

`findBestMoveTimed` in `src/ai/Minimax.cpp`.

### Time control — the requirement, and a bug worth mentioning

The subject: over half a second per move on average and the project fails.

The deadline is checked **inside** the recursion, in `checkTimeUp()`, every 256
nodes. Not every node — `chrono` calls aren't free, and calling one per node
measurably slowed the search.

**Worth telling the grader**, because it shows real debugging: the first version
checked the clock only *between* depths, using a growth estimate to guess whether
the next depth would fit. Asked for 400 ms, it took **1712 ms** — over 4× the
budget. The reason: depth costs grow ~20× per level, and no margin heuristic
reliably out-guesses exponential growth. Once a depth starts, nothing stops it. The
fix was checking mid-search and aborting immediately. Times now land at 400–406 ms
for a 400 ms budget.

### Transposition table (Zobrist hashing)

In Gomoku, playing move A then move B reaches the *same board* as B then A. These
are **transpositions**, and they're everywhere. Iterative deepening also re-walks
every shallow position at each new depth.

So each position gets hashed to a 64-bit number and results are cached:

- **Zobrist hashing** — a precomputed random number per (cell, colour). XOR them all
  together to hash a position. XOR is its own inverse, so placing or removing a
  stone updates the hash in O(1) instead of re-hashing 361 cells.
- The cache stores: the score, the depth it was searched to, whether the score is
  exact or a bound, and the best move found.
- An entry is only reused if it was searched **at least as deep** as needed now. The
  stored best move is used as an ordering hint even when the score can't be reused.

Two details that show care:

- The capture counts are part of the hash. Same stones + different capture totals =
  genuinely different game state, and without this the table would return a score
  computed for the wrong one.
- The table is cleared when the search's root player changes, because cached scores
  are relative to whoever the search was rooted for, and the hash doesn't record
  that.

**Be honest about the payoff:** measured hit rate ~15%, throughput gain ~10%. Real,
but it did *not* buy a depth level on its own — depth grows logarithmically with
node count, so 10% more nodes is a fraction of a level. Say that plainly; a grader
will respect measurement over a marketing claim.

### Forward pruning — say this before they find it

The search does **not** examine every legal move at every node. After ordering, only
the top N candidates are searched (`MAX_CANDIDATES_PER_NODE` — check the current
value in `include/ai/Minimax.hpp` before you present).

**This means the search is not exact minimax.** A genuinely best move ranked outside
the cut is never explored.

Why it's there — this is the arithmetic to be able to reproduce:

Nodes needed ≈ (effective branching)^depth. Under alpha-beta, effective branching is
roughly the **square root** of the candidate count.

| candidates/node | effective branching | depth at ~30k nodes |
|---|---|---|
| 12 | √12 ≈ 3.5 | ~8 |
| 6 | √6 ≈ 2.4 | ~11 |
| 5 | √5 ≈ 2.2 | ~12 |

The candidate count sits in the **base of an exponent**. Halving it buys ~3 depth
levels. Meanwhile every constant-factor optimisation I tried — a faster sort, the
transposition table — produced *no visible depth change*, because depth scales with
the logarithm of node count.

**The honest framing:** exhaustive depth-10 search on a 19×19 board is
computationally impossible regardless of pruning quality. Every real-time engine
narrows the tree somewhere. I claim the depth; I don't claim optimality.

---

## Part 2 — Depth

The eval sheet: *"If the implementation is a pruning one, like Alphabeta, take into
account the actual effective search depth, not the initial one."*

### What one level means

One level = one **ply** = one player's turn. Depth 9 means:

```
ply 1  AI plays              ply 6  I reply
ply 2  I reply               ply 7  AI plays
ply 3  AI plays              ply 8  I reply
ply 4  I reply               ply 9  AI plays
ply 5  AI plays
```

Roughly four and a half full rounds, with the opponent assumed to play their best
reply at every one of their turns.

### Why the number changes between moves

Depth is a **readout, not a setting**. Iterative deepening goes as deep as 350 ms
allows, and that varies with:

- **Stones on the board** — more stones means more candidate cells, so fewer levels
  fit.
- **How forcing the position is** — when a threat must be answered, alpha-beta cuts
  hard and depth goes up cheaply. Quiet positions with many plausible moves cost
  more.
- **Terminal short-circuits** — a forced win found down a line stops that whole
  subtree, freeing time to go deeper elsewhere.

### Measure your own numbers

**Do this before the defense.** Play a full game against the AI and record the
side-panel readout:

| Game stage | Depth reached | Time (ms) |
|---|---|---|
| Opening (first 5 moves) | | |
| Early middlegame (~10 stones) | | |
| Middlegame (~20 stones) | | |
| Crowded position (~30 stones) | | |
| **Worst case seen** | | |

Quote **the range you measured in real play** — not a best case from a synthetic
test board. If a grader watches a game and sees a number lower than you claimed,
you've lost credibility on everything else you said.

### If a grader challenges "effective depth"

They may argue that forward pruning means your effective depth is lower than
reported. A fair, honest answer:

> "The reported depth is a fully completed iterative-deepening pass — every one of
> those plies was actually searched to completion, and any partial pass is
> discarded. What's narrowed is the *width*: N candidates per node instead of all
> of them. So the lookahead genuinely goes that many plies deep; it explores fewer
> lines at each ply. I'd rather state that than claim an exhaustive search I don't
> have."

### The grading boundary

- 5–10 levels → **4 points**
- 10 or more → **5 points**

If your measured real-game depth sits at 8–9, one point is on the table.
`MAX_CANDIDATES_PER_NODE` from 6 to 5 is worth roughly one level. **But verify play
strength afterwards** — if the AI starts missing blocks, revert. A grader notices a
missed block long before they notice one fewer level, and the performance section is
worth 5 points on its own.

---

## Part 3 — Search space

Graded 0 / 3 / 5:

- Entire board → 0
- One rectangular window around all placed stones → 3
- **Multiple windows encompassing placed stones while minimizing wasted space → 5**

**Your implementation is the third one.** Make sure you say so, because it's not
obvious from the outside.

`candidateMoves()` doesn't build one big rectangle. It walks the list of stones
actually on the board and takes a radius-2 box around **each stone individually**,
deduplicating overlaps with a generation-stamped array. The result is the union of
many small windows that hugs the stones' actual shape.

```
   one big window (worth 3)          per-stone windows (worth 5)

   ┌───────────────────────┐          ┌─────┐
   │ . . . . . . . . . . . │          │. . .│
   │ . . ● . . . . . . . . │          │. ● .│      ┌─────┐
   │ . . . . . . . . . . . │          └─────┘      │. . .│
   │ . . . . . . . . . . . │                       │. ● .│
   │ . . . . . . . . . ● . │                       │. . .│
   │ . . . . . . . . . . . │                       └─────┘
   └───────────────────────┘
   huge empty area searched          only cells near real stones
```

Two stones far apart make the single-rectangle approach search an enormous empty
region. The per-stone version doesn't.

**Why radius 2 and not 1:** the broken free-three shape `. X X . X .` has a relevant
empty cell two steps from a stone. Radius 1 would miss the move that creates or
blocks it.

**The measurement that justifies all of this:** unrestricted, the branching factor
is ~359 and depth 3 alone took **138 seconds**. With restriction, the same position
at depth 3 took 160 ms. That's the number to quote — it makes the case instantly.

The empty board is a special case: no stones to anchor a radius to, so it returns
the centre point.

---

## Part 4 — The heuristic

The eval sheet breaks this into 8 separate questions. Part 5 answers them one by
one. This part explains the thing itself.

### The core idea

*"Stone count tells you almost nothing. Shape tells you everything."*

Four stones scattered around the board are nearly worthless. Four stones in a row
with both ends open have already won — there are two different cells that complete
five, and the opponent can only block one.

So the heuristic scores **runs** — maximal lines of one player's stones — by two
properties: **length**, and **freedom**.

### Freedom

```
  . X X X .     both ends open      FREE        most dangerous
  O X X X .     one end blocked     HALF-FREE   still a threat
  O X X X O     both ends blocked   FLANKED     worthless — can never make 5
```

This maps exactly onto the eval sheet's "Free, half-free, flanked" question, and
it's the `openStart` / `openEnd` pair in `patternWeight`.

### The weight table

`Minimax::patternWeight(length, openStart, openEnd)`:

| Length | Free (2 open) | Half-free (1 open) | Flanked (0 open) |
|---|---|---|---|
| 5+ | 100 000 | 100 000 | 100 000 |
| 4 | 10 000 | 1 000 | 0 |
| 3 | 500 | 100 | 0 |
| 2 | 50 | 10 | 0 |
| 1 | 1 | 1 | 0 |

Three things to be able to justify:

**Five is 100 000 regardless of freedom.** Five in a row is a win; nothing can block
it after the fact.

**Flanked runs score 0.** A run blocked on both ends can never reach five, so it has
no value. This is the heuristic knowing the difference between stones and threats.

**The jumps are steep, and deliberately so.** A free four (10 000) is worth twenty
free threes (500). That's not arbitrary — a free four is a *forced* win next move,
while a free three can still be answered. The gaps encode urgency.

### Both players

```cpp
int myPatterns    = scorePlayerPatterns(board, aiPlayer);
int theirPatterns = scorePlayerPatterns(board, opponent);
return (myPatterns - theirPatterns) + captureScore;
```

The score is a **difference**. This is what makes the AI defend without any separate
defensive logic: letting you build a free three costs it 500 points, exactly as much
as building one itself gains. Blocking and attacking are the same calculation.

### Captures

```cpp
int captureScore = (myCaps * myCaps - theirCaps * theirCaps) * 15;
```

**Squared, not linear** — and there's a story here worth telling.

Ten captured stones wins the game outright. So the 8th stone lost matters far more
than the 2nd: it's one pair from defeat. The original version weighted captures
linearly, and the AI kept **losing capture races** — it saw no escalating danger and
treated the 5th pair like the 1st. Squaring makes the pressure rise as the count
climbs.

With squaring: going from 8 to 10 captures is worth 64 → 100, a jump of 36 units
(×15 = 540). Going from 0 to 2 is worth 0 → 4, a jump of 4 (×15 = 60). Nine times
the urgency at the dangerous end.

### A worked example

Board slice, AI is White:

```
   . W W W .          White free three     = 500
   O B B B .          Black half-free three = 100      (O = White stone blocking)
   captures: White 2, Black 0
```

```
myPatterns    = 500
theirPatterns = 100
captureScore  = (2² − 0²) × 15 = 60

score = (500 − 100) + 60 = +460     → good for White
```

Now suppose Black completes their three into a free four:

```
myPatterns    = 500
theirPatterns = 10 000
captureScore  = 60

score = (500 − 10 000) + 60 = −9 440   → nearly lost
```

That's a swing of almost 10 000 from one Black move. The AI will spend everything to
prevent it, which is exactly right.

### Where captures are handled

Two separate mechanisms, don't confuse them:

- **Inside the search**, `applyMove` actually simulates captures — removes the
  stones, updates counts, and records enough to undo it exactly. So the search
  *sees* capture consequences several plies ahead.
- **In the static heuristic**, only the resulting capture counts are scored.

**Worth mentioning as a bug you found:** the search originally didn't simulate
captures at all. It played strong alignment defence and lost every capture race,
because captures were literally invisible to it — `capturedBy()` was a constant
during search, contributing nothing to any comparison between moves. Adding capture
simulation with proper undo fixed it. This is a good story: a symptom noticed in
play, traced to a specific cause, fixed, and verified.

### Efficiency

`scorePlayerPatterns` walks the list of stones that actually exist, not all 361
cells. It runs at every leaf node, making it the most-called expensive function in
the whole search — on a board with 15 stones, that's 15 iterations instead of 361.

Runs are counted **once**, not once per stone: the scan only starts at a run's true
beginning, verified by checking that the cell behind it isn't the same colour.
Otherwise a run of four would be counted four times.

---

## Part 5 — The eval sheet, question by question

The heuristic section is 8 yes/no items. Here's an honest read of where you stand.

| # | Question | Answer | What to say |
|---|---|---|---|
| 1 | Takes current **alignments** into account? | **Yes** | `scorePlayerPatterns` finds every run in all 4 directions and scores it by length. |
| 2 | Checks an alignment has **enough space** to develop into five? | **Partial** | The open/blocked ends check is a proxy for it. It doesn't verify there are 5 cells of room. See Part 6. |
| 3 | Weighs alignments by **freedom** (free / half-free / flanked)? | **Yes** | Exactly the three cases in the weight table; flanked scores 0. |
| 4 | Takes **potential** captures into account? | **In search, not statically** | `applyMove` simulates captures so the search sees them ahead; the static evaluation only scores completed captures. See Part 6. |
| 5 | Takes **current captured stones** into account? | **Yes** | Squared difference × 15, so danger escalates near 10. |
| 6 | Checks for advantageous **combinations** (figures)? | **Weak** | Summing all runs implicitly rewards multiple threats, but there's no explicit fork/double-threat bonus. See Part 6. |
| 7 | Takes **both players** into account? | **Yes** | The score is my patterns minus theirs — one calculation covers attack and defence. |
| 8 | **Dynamic part** — uses past moves to identify patterns? | **No** | The evaluation is purely static from the current position. Say so plainly. |

For the other sections:

| Section | Where you stand |
|---|---|
| Minimax algorithm | **5** — alpha-beta is explicitly named as the "improved" tier. |
| Move search depth | **4 or 5** — depends on your measured real-game depth. See Part 2. |
| Search space | **5** — per-stone windows, not one rectangle. Make sure you explain it (Part 3). |
| Rules | Pass — you have unit tests (`make test`) to prove it. |
| UI and AI performance | Graded on whether the AI actually beats the grader. Nothing to explain, only to demonstrate. |
| Bonuses | 1 point each, max 5. |

**On item 8, don't bluff.** "No, the evaluation is purely static" is a fine answer.
Claiming a dynamic component you don't have will be checked in the code, and being
caught inventing one poisons everything else you said.

---

## Part 6 — Gaps worth closing before the defense

Three of the heuristic questions are currently "no" or "partial", and two of them
are genuinely cheap to fix. Each converts directly into a graded item.

**Item 2 — space to reach five.** Right now `patternWeight` only looks at the cell
immediately past each end. A three with one open end, but a blocker two cells
further out, can never make five — yet it scores as a half-free three. A fix would
scan outward far enough to confirm 5 cells of usable room exist, and zero out runs
that can't reach five.

**Item 4 — potential captures in the static evaluation.** The heuristic could add a
term for pairs that are *currently capturable* — your own vulnerable pairs as a
penalty, the opponent's as a bonus. You already have `wouldCaptureAnyPair` and the
flank-scanning logic; this is largely reuse.

**Item 6 — combinations.** An explicit bonus when a single position contains two or
more separate free threes (a fork the opponent can't answer). You already count
free-threes for the double-three rule (`countFreeThrees`), so the detection exists.

Item 8 (dynamic evaluation) is a much larger piece of work and probably not worth
starting now.

Say the word if you want any of these implemented — each is a contained change to
`Minimax.cpp`, and they're worth more per hour of work than another bonus feature.

---

## Part 7 — Hard questions and honest answers

**"Why not just search deeper?"**
Cost grows ~20× per level. Depth 5 unrestricted took 46 seconds. The budget is under
half a second. Depth comes from narrowing the tree, not from waiting longer.

**"Your AI doesn't consider every move — isn't that broken?"**
It's deliberate forward pruning, and it's the reason depth 10 is reachable at all.
Exhaustive search at this depth on 19×19 is impossible. I narrowed width to buy
depth, and I can show the arithmetic behind the trade.

**"How do you know alpha-beta didn't break the search?"**
I ran identical positions before and after. Same chosen move, same score, ~4× faster
at depth 3. If pruning changed an answer, it would be a bug — alpha-beta is supposed
to be exact.

**"What are those numbers in the debug view?"**
Every root candidate and the score the search gave it, best first. Important caveat:
only the top score is an exact evaluation. Because of alpha-beta, lower-ranked moves
were often cut off early and report the bound that triggered the cut, not a full
evaluation. The ranking is meaningful; the losing numbers aren't precise.

**"Why does the depth number keep changing?"**
It's iterative deepening under a fixed time budget, so depth is a measurement of how
much the AI could afford in this position, not a setting. Crowded positions reach
less depth; forcing positions reach more because pruning cuts harder.

**"Show me the game rules are right."**
`make test` — the unit suite covers captures, the safe-move-into-capture case,
free-three detection, blocked threes, double-three rejection, and the 10-capture
win, using the exact scenarios from Appendix VI of the subject.

**"Did you hit any real bugs?"**
Have two or three ready. Good ones: the time check that overshot its budget by 4×;
the search that couldn't see captures and lost every capture race; the
`isLineVulnerable` check that could never fire because it only examined pairs along
the winning line's own axis. Each has a symptom, a cause, a fix, and a
verification — which is exactly what a grader wants to hear.

**"Anything you know is weak?"**
Yes, and volunteer it: the search generates candidates without checking the
double-three rule, so it can explore positions that couldn't legally occur. A filter
now removes them, and `main.cpp` verifies the final move regardless, so an illegal
move can never reach the board. Volunteering this reads as rigour; being caught
hiding it reads as the opposite.

---

## Part 8 — Demo script

Roughly this order, ~10 minutes:

1. **Prove the build.** `make fclean && make`, no warnings, binary named `Gomoku`.
   `make` again → nothing to do, no relink.
2. **Prove the rules.** `make test`, all green. Mention the Appendix VI scenarios.
3. **Show the game.** Play a few moves. Point out the hover preview refusing an
   illegal move before you click, and the timer/depth readout in the panel.
4. **Show the rules live.** Trigger a capture. Trigger a double-three rejection.
5. **Show the AI thinking.** Build an open three deliberately. It blocks. Press `D`
   and show that the top-ranked candidate was that blocking cell — the search's
   reasoning, on the board.
6. **Explain minimax** using Part 1's alpha-beta diagram. Draw it if there's a
   whiteboard.
7. **Explain the heuristic** using the weight table and the worked example.
8. **Give the depth numbers you measured**, and be precise about what they mean.
9. **Show hotseat and the suggestion feature** (`M`, then `S`).
10. **List the bonuses**, one at a time, each demonstrated.

Then let them play against it. That's the 5-point performance question, and nothing
you say affects it.

**Last thing:** if you don't know an answer, say so and say what you'd check. That
lands far better than a confident guess a grader can disprove in ten seconds.