# Gomoku AI — Project Plan

**Language:** C++
**Team:** 2 developers (Person A — Engine & Rules, Person B — AI & Interface)
**Version:** 1.0

---

## Table of Contents

1. [Project Requirements](#1-project-requirements)
2. [Feature Scope Specification](#2-feature-scope-specification)
3. [System Architecture](#3-system-architecture)
4. [Team Division](#4-team-division)
5. [Development Roadmap & Phases](#5-development-roadmap--phases)
6. [Backlog](#6-backlog)
7. [Definition of Done](#7-definition-of-done)
8. [References](#8-references)

---

## 1. Project Requirements

### 1.1 Game Rules (as specified)

- Board: 19x19 Goban, no limit on number of stones.
- Win condition (alignment): 5 or more stones in a row (horizontal, vertical, diagonal) counts as a win.
- Win condition (capture): capturing 10 of the opponent's stones (5 pairs) wins the game.
- **Capture rule:** a pair of opponent stones flanked on both ends by your stones is removed from the board. Only exact pairs can be captured — not single stones, not 3+ in a row. Moving *into* a flanked position is safe (does not trigger self-capture).
- **Endgame-capture interaction:**
  - A 5-in-a-row only wins if the opponent cannot immediately break it by capturing a pair within that line.
  - If a player has already lost 4 pairs (8 stones) and the opponent can capture one more pair, the opponent wins by capture instead.
  - If neither is possible, the game can end normally.
- **No double-threes:** a move that creates two simultaneous free-three alignments is illegal. (A free-three is 3 stones that, if unblocked, becomes an unstoppable open four.) Note: a double-three created *via a capture* is explicitly **not** forbidden.

### 1.2 Hard Constraints (non-negotiable, project = 0 if violated)

- Executable must be named `Gomoku`.
- Program must **never crash**, including on out-of-memory conditions, and must never quit unexpectedly.
- Must provide a `Makefile` with at minimum: `$(NAME)`, `all`, `clean`, `fclean`, `re`. Must not relink on repeated `make`.
- AI must search **at least 10 levels deep** in the game tree to earn full credit.
- AI must find a move in **≤ 0.5 seconds on average**. Exceeding this fails project validation.
- A **visible timer** showing how long the AI took to find its last move is mandatory. No timer = no validation.

### 1.3 Required Game Modes

- Human vs AI (the core deliverable — AI must adapt to the human's actual moves, not play a fixed script).
- Human vs Human (hotseat) with an AI-powered **move-suggestion** feature.

---

## 2. Feature Scope Specification

### 2.1 Mandatory Scope

| Area | Feature | Notes |
|---|---|---|
| Board | 19x19 board, unlimited stones | Flat array or bitboard representation |
| Rules | Move legality checking | Includes double-three rejection |
| Rules | Capture detection & execution | Pair-flanking, safe move-into-capture |
| Rules | Win detection — alignment | 5+, with endgame-capture override |
| Rules | Win detection — capture | 10 captured stones |
| Rules | Free-three / double-three detection | Hardest rule to implement correctly |
| AI | Minimax with alpha-beta pruning | Depth ≥ 10 |
| AI | Heuristic evaluation function | Pattern-based (see 3.3) |
| AI | Move generation restricted to relevant cells | Full 19x19 search space is intractable |
| AI | Time-boxed search (iterative deepening) | Hard cutoff before 0.5s |
| Interface | Playable GUI | Any library, must look intentional, not thrown together |
| Interface | Human vs AI mode | |
| Interface | Human vs Human (hotseat) + move suggestion | |
| Interface | AI move timer displayed | Mandatory |
| Debug | AI reasoning / evaluation display | For your own tuning + required for defense |

### 2.2 Bonus Scope (only evaluated if mandatory part is 100% perfect)

Priority order — implement top-down, stop whenever time runs out:

1. **Selectable starting rules at game start** (Standard, Pro, Swap, Swap2) — explicitly suggested in the subject, highest bonus value.
2. **Transposition table (Zobrist hashing)** — big performance/depth win, also an impressive defense topic.
3. **Undo / move history + replay.**
4. **Adjustable AI difficulty (search depth / time budget slider).**
5. **Match/game log export** for post-game review.

Do not start bonus work until every mandatory requirement is verified working end-to-end.

---

## 3. System Architecture

### 3.1 High-Level Layout

Headers and implementation are kept in fully separate trees (`include/` vs `src/`) that mirror each other module-for-module. This keeps includes unambiguous (`-Iinclude` covers everything, always), keeps the Makefile's auto-discovery simple, and avoids ever mixing declarations and definitions in the same place.

```
Gomoku/
├── Makefile
├── include/
│   ├── common/
│   │   └── Types.hpp             (shared enums/structs: Move, Player, Cell)
│   ├── engine/
│   │   ├── Board.hpp
│   │   ├── Rules.hpp              (legality, captures, win detection)
│   │   ├── ThreatDetector.hpp     (free-three / double-three)
│   │   └── GameState.hpp
│   ├── ai/
│   │   ├── Minimax.hpp
│   │   ├── Evaluator.hpp          (heuristic / pattern scoring)
│   │   ├── MoveOrdering.hpp
│   │   └── TimeManager.hpp
│   └── ui/
│       ├── Renderer.hpp
│       ├── InputHandler.hpp
│       └── DebugPanel.hpp
├── src/
│   ├── main.cpp
│   ├── engine/
│   │   ├── Board.cpp
│   │   ├── Rules.cpp
│   │   ├── ThreatDetector.cpp
│   │   └── GameState.cpp
│   ├── ai/
│   │   ├── Minimax.cpp
│   │   ├── Evaluator.cpp
│   │   ├── MoveOrdering.cpp
│   │   └── TimeManager.cpp
│   └── ui/
│       ├── Renderer.cpp
│       ├── InputHandler.cpp
│       └── DebugPanel.cpp
├── obj/                           (generated at build time — gitignored)
├── tests/
│   └── rules_tests.cpp            (unit tests for capture/free-three edge cases)
└── docs/
    └── defense_notes.md           (minimax + heuristic explanation)
```

Add an `obj/` line to `.gitignore` along with the `Gomoku` binary itself — neither should ever be committed.

### 3.1.1 Makefile

Auto-discovers every `.cpp` under `src/`, mirrors the object tree under `obj/`, and uses compiler-generated dependency files (`-MMD -MP`) so that changing a single header only recompiles the `.cpp` files that actually include it — this is what makes "no unnecessary relink" true in practice, not just on the first build.

```makefile
NAME = Gomoku

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++17 -Iinclude
CXXFLAGS += $(shell pkg-config --cflags sdl2)
LDFLAGS = $(shell pkg-config --libs sdl2)

SRC_DIR = src
OBJ_DIR = obj

SRC = $(shell find $(SRC_DIR) -name '*.cpp')
OBJ = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC))
DEP = $(OBJ:.o=.d)

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(OBJ) -o $(NAME) $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(DEP)

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
```

Notes:
- You never edit the `SRC`/`OBJ` lists by hand — drop a new `.cpp` anywhere under `src/` and the next `make` picks it up automatically.
- `#include` paths in your `.cpp` files are always relative to `include/`, e.g. `#include "engine/Board.hpp"` — never relative `../` paths.
- If you later add `SDL2_ttf` (for the timer/debug text), extend both `pkg-config` calls: `` `pkg-config --cflags sdl2 SDL2_ttf` `` / `` `pkg-config --libs sdl2 SDL2_ttf` ``.

### 3.2 Engine ↔ AI ↔ UI Contract

The two halves of the team integrate through a small, stable API exposed by the engine. Agree on these signatures on Day 0 and avoid changing them later:

```cpp
// Board.hpp
enum class Cell { Empty, Black, White };

struct Move { int x, y; };

class Board {
public:
    bool isLegal(Move m, Player p) const;      // includes double-three check
    ApplyResult apply(Move m, Player p);        // places stone, resolves captures
    void undo();                                // needed for minimax tree search
    WinState checkWin(Player p) const;          // alignment + capture win conditions
    std::vector<Move> candidateMoves() const;   // cells near existing stones only
    int capturedPairs(Player p) const;
};
```

- Person A owns and unit-tests everything behind this interface.
- Person B's `Minimax` and `Renderer` only ever call this interface — never touch board internals directly. This is what lets both of you work in parallel without merge conflicts.

### 3.3 AI Design Notes

- **Search:** Minimax + alpha-beta pruning, iterative deepening (search depth 1, 2, 3… until the time budget is nearly used, always keeping the best move found from the last *completed* depth).
- **Move generation:** never scan the full empty board — only consider empty cells within a radius (e.g. 2) of an existing stone. This is what makes depth 10 feasible on a 19x19 board.
- **Heuristic:** score the board by scanning all 4 directions (horizontal, vertical, 2 diagonals) for patterns like open-two, open-three, closed-four, open-four, and combine with capture-threat scoring. Weight patterns that create unstoppable threats (open fours) far higher than everything else.
- **Move ordering:** evaluate the most promising moves first (near existing threats) so alpha-beta pruning cuts more branches.
- **Time management:** never let a single search exceed ~0.4s, leaving margin under the 0.5s ceiling.

---

## 4. Team Division

### Person A — Engine & Rules

Owns correctness of the game itself: if a rule is wrong, it doesn't matter how good the AI is.

- Board representation & state management
- Move legality
- Capture detection and execution (including the "safe move into capture" edge case)
- Win detection: alignment win + endgame-capture override + 10-capture win
- Free-three and double-three detection (budget the most time here — it's the trickiest logic in the project)
- Unit tests for all rule edge cases (see Appendix VI in the subject for the exact scenarios to test against)
- Makefile / build system ownership

### Person B — AI & Interface

Owns whether the program is actually good to play against and pleasant to use.

- Minimax + alpha-beta implementation
- Heuristic evaluation function
- Move ordering + candidate move restriction
- Iterative deepening + time-boxed search
- GUI: board rendering, input handling, move suggestion mode, AI move timer
- Debug/reasoning panel showing top candidate moves and their scores

### Shared responsibilities

- Integration points (whenever the engine API needs to change)
- Playtesting against the finished AI
- Defense preparation — **both people must be able to explain both the minimax implementation and the heuristic in full detail**, regardless of who wrote which part.

---

## 5. Development Roadmap & Phases

Estimated for a couple of focused sessions per day; compress or stretch based on your actual availability.

### Phase 0 — Setup (Day 0, both together, ~half day)

- Repo created, Makefile skeleton (`$(NAME)`, `all`, `clean`, `fclean`, `re`)
- Agree on `Board`, `Move`, `Player` types and the engine API contract (section 3.2)
- Write a rules checklist from the subject (including the Appendix VI capture/free-three examples) to test against later

### Phase 1 — Parallel Foundations (Days 1–3)

**Person A:** board representation, stone placement, basic 5-in-a-row win detection (no captures yet).
**Person B:** minimax skeleton running against a stub heuristic (e.g. stone count) on the in-progress board, to get the search/timer plumbing working end-to-end early. GUI shell (empty board rendering + click-to-place).

*Checkpoint: AI can play a full legal game with only the alignment-win rule active.*

### Phase 2 — Core Rules Complete (Days 3–5)

**Person A:** captures, endgame-capture interaction with win detection, free-three detection, double-three rejection (with the "double-three via capture is allowed" exception).
**Person B:** real pattern-based heuristic, move ordering, candidate-move restriction, first integration of AI against the real (not stub) rules engine.

*Checkpoint: all rules from Appendix VI reproduce correctly; AI uses the real rules end-to-end.*

### Phase 3 — Performance Push (Days 5–7)

- Alpha-beta pruning tuned, iterative deepening with a hard time cutoff
- Reach and verify depth ≥ 10 within the 0.5s budget (log actual depth reached + time per move during testing)
- (Bonus-adjacent but high value) Zobrist hashing / transposition table if time allows — this is often what closes the gap between depth 6-7 and depth 10

*Checkpoint: 100 test moves average under 0.5s, sustained depth 10.*

### Phase 4 — Interface Polish & Hotseat (Days 7–8)

- Move-suggestion feature for hotseat mode
- AI move timer displayed prominently
- Debug/reasoning panel (top-N candidate moves + scores) — doubles as your defense demo
- Visual polish pass on the GUI

*Checkpoint: full human-vs-AI and human-vs-human (with suggestions) playthroughs, no crashes.*

### Phase 5 — Hardening & Defense Prep (Day 9)

- Stress test: force out-of-memory / edge-case boards, confirm zero crashes
- Confirm Makefile does not relink, executable is exactly named `Gomoku`
- Both teammates independently walk through explaining the minimax implementation and the heuristic out loud — fix any gaps in understanding now, not during the defense
- Write `docs/defense_notes.md` together summarizing both systems

### Phase 6 — Bonus (only if Phase 0–5 are airtight)

Work top-down through the bonus priority list in section 2.2. Stop immediately and re-verify the mandatory part if any bonus work destabilizes it — a broken mandatory part zeroes bonus evaluation entirely regardless of how much bonus work exists.

---

## 6. Backlog

Use this as your issue tracker seed — copy into GitHub Issues / a project board.

**Engine (Person A)**
- [ ] Board data structure + accessors
- [ ] Move legality (basic: in-bounds, cell empty)
- [ ] Stone placement / apply / undo
- [ ] 5-in-a-row detection (all 4 directions)
- [ ] Capture detection (pair flanking)
- [ ] Capture execution + capture counters per player
- [ ] "Move into capture is safe" edge case
- [ ] Endgame-capture override logic
- [ ] 10-capture win condition
- [ ] Free-three detection (single direction)
- [ ] Free-three detection (all directions, combined)
- [ ] Double-three rejection on move legality
- [ ] Double-three-via-capture exception
- [ ] Unit tests covering every Appendix VI example

**AI (Person B)**
- [ ] Minimax skeleton (no pruning)
- [ ] Alpha-beta pruning
- [ ] Candidate move restriction (radius-based)
- [ ] Move ordering heuristic
- [ ] Pattern-based evaluation function (open-two/three/four, closed-four)
- [ ] Capture-aware scoring
- [ ] Iterative deepening
- [ ] Time-boxed cutoff (hard limit under 0.5s)
- [ ] Depth/time logging for validation testing
- [ ] (Bonus) Zobrist hashing + transposition table

**Interface (Person B)**
- [ ] Board rendering
- [ ] Click-to-place input
- [ ] Human vs AI mode
- [ ] Human vs Human (hotseat) mode
- [ ] Move-suggestion overlay for hotseat
- [ ] AI move timer display
- [ ] Debug panel (top candidate moves + scores)
- [ ] Win/game-over screen

**Bonus**
- [ ] Rule-set selector at game start (Standard/Pro/Swap/Swap2)
- [ ] Undo/redo + move history
- [ ] Difficulty selector
- [ ] Game log export

---

## 7. Definition of Done

Before calling the mandatory part "perfect":

- [ ] Zero crashes across an extended playtest, including deliberately malformed input
- [ ] `make`, `make clean`, `make fclean`, `make re` all behave correctly, no relinking
- [ ] Executable named exactly `Gomoku`
- [ ] AI consistently searches depth ≥ 10
- [ ] AI move time averages under 0.5s, timer visible on screen at all times
- [ ] All rules from the subject (captures, endgame-capture, free-three, double-three, double-three-via-capture) verified against the Appendix VI examples
- [ ] Both human-vs-AI and human-vs-human+suggestion modes fully playable
- [ ] Both teammates can explain the minimax implementation and the heuristic in detail, unaided, cold

---

## 8. References

**Minimax & alpha-beta (concept)**
- Video walkthrough with pseudocode: https://www.youtube.com/watch?v=l-hh51ncgDI
- Second explainer with worked examples: https://www.youtube.com/watch?v=ec5mluJnWM0

**Gomoku-specific AI design**
- Building a Gomoku AI — minimax + evaluation function overview: https://medium.com/@LukeASalamone/creating-an-ai-for-gomoku-28a4c84c7a52
  (companion repo: https://github.com/lukesalamone/gomoku-2049)
- Alpha-beta + pattern-based evaluation, including why full-board search is infeasible and how radius-limited move generation fixes it: https://dev.to/sendotltd/a-gomoku-ai-with-minimax-alpha-beta-pruning-and-pattern-based-evaluation-4lai
- Pattern/score table example for evaluation functions: https://github.com/Mgla96/GomokuAI

**Other 42-school Gomoku implementations (for architecture ideas — do not copy code directly)**
- C++, built explicitly to hit depth 10 within 0.5s: https://github.com/vklaouse/Gomoku
- C++: https://github.com/terngkub/gomoku
- Rust, layered architecture (useful for the engine/AI separation idea even though language differs): https://github.com/gbersac/gomoku_42

**Search technique background**
- Why strong Gomoku engines often pair minimax with threat-space search rather than brute-force alone: https://blog.theofekfoundation.org/artificial-intelligence/2015/12/18/minimax-improvements/

**Rule reference**
- The `gomoku.pdf` subject document itself, Appendix VI (Captures, Free-threes) — use its exact diagrams as your unit test cases.