// ============================================================================
//  Gomoku — application entry point
//
//  Owns the game loop and wires the three layers together:
//      engine/  Board   - rules, captures, win conditions
//      ai/      Minimax - move search (alpha-beta + transposition table)
//      ui/      Renderer, InputHandler - drawing and mouse mapping
//
//  This file holds no game rules of its own. Every move — human or AI —
//  goes through the same Board pipeline in commitMove().
// ============================================================================

#include "ui/Renderer.hpp"
#include "ui/InputHandler.hpp"
#include "engine/Board.hpp"
#include "ai/Minimax.hpp"

#include <SDL2/SDL.h>
#include <chrono>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// --- Configuration ----------------------------------------------------------

// Human vs AI: Black is the player, White is the engine.
// Hotseat: both colours are played by humans, with S offering a hint.
enum class GameMode { HumanVsAI, Hotseat };

namespace {
    const char* FONT_REGULAR = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    const char* FONT_BOLD    = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf";

    // Subject requires an average under 500 ms per move. The margin covers
    // the event loop and rendering that share the same frame.
    const double AI_TIME_LIMIT_MS = 350.0;

    // Traditional Go column labels: 'I' is skipped to avoid confusion with 1.
    const char* COLS = "ABCDEFGHJKLMNOPQRST";

    // Formats a move the way the board labels it, e.g. {9,9} -> "K10".
    std::string coordText(Move m) {
        return std::string(1, COLS[m.x]) + std::to_string(m.y + 1);
    }

    std::string playerName(Player p) {
        return (p == Player::Black) ? "Black" : "White";
    }
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    // Any allocation failure (std::bad_alloc) or other library exception
    // unwinds to here and exits cleanly rather than terminating the process.
    // The subject treats an unexpected crash as a non-functional project.
    try {

    // --- Window and fonts ---------------------------------------------------

    Renderer renderer;
    if (!renderer.init(FONT_REGULAR, FONT_BOLD)) {
        // init() already reported the specific SDL/TTF error.
        return 1;
    }

    // --- Game state ---------------------------------------------------------

    Board board;                        // authoritative board and rules
    Player current = Player::Black;     // whose turn it is
    Move lastMove{-1, -1};              // highlighted with a red ring
    std::vector<Move> winLine;          // winning stones, drawn at game over
    std::string statusMessage;          // one-line feedback in the side panel
    bool gameOver = false;

    // --- Mode and engine ----------------------------------------------------

    GameMode mode = GameMode::HumanVsAI;
    const Player humanPlayer = Player::Black;
    const Player aiPlayer    = Player::White;

    Minimax aiEngine(1);                // depth is set per call by findBestMoveTimed
    std::string aiInfo = "No moves yet"; // depth + timing readout (required by subject)

    // --- Move suggestion (S) ------------------------------------------------
    // A hint only: the recommended cell is marked, never played automatically.

    Move suggestedMove{-1, -1};
    bool hasSuggestion = false;
    bool suggestionRequested = false;   // set by input, consumed after rendering

    // --- Debug view (D) -----------------------------------------------------
    // Copies of the search's root scores. A copy, not a reference: the next
    // search overwrites the engine's list, but the view must keep showing the
    // reasoning behind the move that was actually played.

    bool showDebug = false;
    std::vector<ScoredMove> debugScores;
    int debugDepth = 0;
    std::string debugPlayerText;

    // --- Helpers ------------------------------------------------------------

    // Single source of truth for whether a click may place a stone.
    auto isHumanTurn = [&]() {
        return mode == GameMode::Hotseat || current == humanPlayer;
    };

    // Clears every piece of per-game state. Used by R, by a mode switch, and
    // by clicking after game over.
    auto resetGame = [&]() {
        board = Board();
        current = Player::Black;
        lastMove = Move{-1, -1};
        winLine.clear();
        statusMessage.clear();
        gameOver = false;
        aiInfo = "No moves yet";
        hasSuggestion = false;
        suggestionRequested = false;
        debugScores.clear();
        debugDepth = 0;
        debugPlayerText.clear();
    };

    // The one path by which a stone reaches the board, shared by the human
    // click handler and the AI turn: place, resolve captures, test both win
    // conditions, then pass the turn. Returns how many stones were captured
    // so the caller can word its own status message.
    auto commitMove = [&](Move m, Player p) -> int {
        board.placeStone(m, p);
        lastMove = m;
        hasSuggestion = false;

        int captured = board.checkAndApplyCaptures(m, p);

        Board::WinResult win = board.checkWinConditions(m, p);
        if (win.won) {
            gameOver = true;
            statusMessage = playerName(win.winner) + " wins by " +
                (win.reason == Board::WinReason::Capture ? "capture!" : "alignment!");
            if (win.reason == Board::WinReason::Alignment)
                winLine = board.findWinningLine(m, win.winner);
        } else {
            current = (p == Player::Black) ? Player::White : Player::Black;
        }
        return captured;
    };

    // Runs the search for 'p' and records what it considered for the debug
    // view. Used for both the AI's own turn and the hint feature.
    auto runSearch = [&](Player p, int& depthOut, double& msOut) -> SearchResult {
        auto t0 = std::chrono::steady_clock::now();
        SearchResult result = aiEngine.findBestMoveTimed(board, p, AI_TIME_LIMIT_MS, depthOut);
        auto t1 = std::chrono::steady_clock::now();
        msOut = std::chrono::duration<double, std::milli>(t1 - t0).count();

        debugScores = aiEngine.getRootScores();
        debugDepth = depthOut;
        debugPlayerText = "for " + playerName(p);
        return result;
    };

    // ========================================================================
    //  Game loop
    // ========================================================================

    bool running = true;
    SDL_Event event;

    while (running) {

        // --- 1. Input -------------------------------------------------------

        while (SDL_PollEvent(&event)) {

            if (event.type == SDL_QUIT) {
                running = false;

            } else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_m:
                        // Switching mid-game would leave turn ownership
                        // ambiguous, so a mode change starts a fresh board.
                        mode = (mode == GameMode::HumanVsAI) ? GameMode::Hotseat
                                                             : GameMode::HumanVsAI;
                        resetGame();
                        break;
                    case SDLK_r:
                        resetGame();
                        break;
                    case SDLK_d:
                        showDebug = !showDebug;
                        break;
                    case SDLK_s:
                        // Deferred: the search blocks, so it runs after the
                        // frame is drawn (see section 4).
                        if (!gameOver && isHumanTurn())
                            suggestionRequested = true;
                        break;
                    default:
                        break;
                }

            } else if (event.type == SDL_MOUSEBUTTONDOWN &&
                       event.button.button == SDL_BUTTON_LEFT) {

                if (gameOver) {
                    resetGame();

                } else if (isHumanTurn()) {
                    Move clicked;
                    // Ignores clicks on the panel, the margins, or between
                    // intersections.
                    if (InputHandler::pixelToBoardCoord(event.button.x, event.button.y, clicked)) {

                        Board::MoveEvaluation eval = board.evaluateMove(clicked, current);

                        if (!eval.legal) {
                            statusMessage = (eval.freeThrees >= 2 && !eval.wouldCapture)
                                ? "Illegal: creates a double-three"
                                : "Illegal: cell occupied";
                        } else {
                            int captured = commitMove(clicked, current);
                            if (!gameOver) {
                                statusMessage = (captured > 0)
                                    ? "Captured " + std::to_string(captured) + " stone(s)!"
                                    : "";
                            }
                        }
                    }
                }
            }
        }

        // --- 2. Hover preview -----------------------------------------------
        // Recomputed from the live cursor each frame; shows where a click
        // would land and whether it would be legal.

        int mouseX, mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);
        Move hoverMove{-1, -1};
        bool showHover = false;
        bool hoverLegal = false;

        if (!gameOver && isHumanTurn()) {
            if (InputHandler::pixelToBoardCoord(mouseX, mouseY, hoverMove) &&
                board.isEmpty(hoverMove.x, hoverMove.y)) {
                showHover = true;
                hoverLegal = board.isLegal(hoverMove, current);
            }
        }

        // --- 3. Render ------------------------------------------------------
        // Drawn back to front: board, stones, markers, overlays, panel.

        renderer.clear();
        renderer.drawBoard();
        renderer.drawStones(board, lastMove);

        if (showDebug && !debugScores.empty())
            renderer.drawCandidateMarkers(debugScores);

        if (!gameOver && hasSuggestion)
            renderer.drawSuggestion(suggestedMove);

        if (showHover)
            renderer.drawHoverPreview(hoverMove, current, hoverLegal);

        if (gameOver && !winLine.empty())
            renderer.drawWinLine(winLine);

        if (showDebug && !debugScores.empty())
            renderer.drawDebugOverlay(debugScores, debugDepth, debugPlayerText);

        PanelInfo panel;
        panel.currentPlayer  = current;
        panel.blackCaptured  = board.capturedBy(Player::Black);
        panel.whiteCaptured  = board.capturedBy(Player::White);
        panel.statusMessage  = statusMessage;
        panel.aiInfo         = aiInfo;
        panel.aiSectionLabel = (mode == GameMode::Hotseat) ? "Last suggestion" : "AI last move";
        panel.modeText       = (mode == GameMode::Hotseat) ? "Hotseat: 2 players" : "Human vs AI";
        panel.thinking       = !gameOver &&
                               ((mode == GameMode::HumanVsAI && current == aiPlayer) ||
                                suggestionRequested);
        renderer.drawSidePanel(panel);

        if (gameOver)
            renderer.drawGameOverOverlay(statusMessage);

        renderer.present();

        // --- 4. Blocking searches -------------------------------------------
        // Both run AFTER present() so "(thinking...)" is actually on screen
        // before the window stops responding for the length of the search.

        // 4a. Move suggestion, for whoever is to play.
        if (!gameOver && suggestionRequested) {
            suggestionRequested = false;

            Player asking = current;
            int depth = 0;
            double ms = 0.0;
            SearchResult result = runSearch(asking, depth, ms);

            if (board.isLegal(result.bestMove, asking)) {
                suggestedMove = result.bestMove;
                hasSuggestion = true;

                std::ostringstream oss;
                oss << coordText(result.bestMove)
                    << " - depth " << depth << ", " << (int)ms << " ms";
                aiInfo = oss.str();
                statusMessage = "";
            } else {
                hasSuggestion = false;
                statusMessage = "No suggestion available";
            }
        }

        // 4b. The AI's own turn.
        if (!gameOver && mode == GameMode::HumanVsAI && current == aiPlayer) {
            int depth = 0;
            double ms = 0.0;
            SearchResult result = runSearch(aiPlayer, depth, ms);

            Move aiMove = result.bestMove;

            // The search generates candidates without checking the
            // double-three rule, so its choice is verified before being
            // played. Falling back to any legal cell is a safety net, not a
            // strategy — it should effectively never trigger.
            if (!board.isLegal(aiMove, aiPlayer)) {
                bool found = false;
                for (int y = 0; y < Board::SIZE && !found; y++) {
                    for (int x = 0; x < Board::SIZE && !found; x++) {
                        if (board.isLegal(Move{x, y}, aiPlayer)) {
                            aiMove = Move{x, y};
                            found = true;
                        }
                    }
                }
            }

            int captured = commitMove(aiMove, aiPlayer);

            // Required by the subject: the time the AI took to find its move.
            std::ostringstream oss;
            oss << "Depth " << depth << ", " << (int)ms << " ms";
            if (captured > 0) oss << " (captured!)";
            aiInfo = oss.str();

            if (!gameOver) {
                statusMessage = (captured > 0)
                    ? "AI captured " + std::to_string(captured) + " stone(s)!"
                    : "";
            }
        }

        SDL_Delay(16);  // ~60 fps; without it the loop spins a core at 100%
    }

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Fatal error: unknown exception\n";
        return 1;
    }

    return 0;
}