#include "ui/Renderer.hpp"
#include "ui/InputHandler.hpp"
#include "engine/Board.hpp"
#include "ai/Minimax.hpp"
#include <SDL2/SDL.h>
#include <vector>
#include <string>
#include <sstream>
#include <chrono>

// Goal: which players are human. Hotseat is a mandatory subject requirement
// (two humans on one machine, with a move-suggestion feature).
enum class GameMode { HumanVsAI, Hotseat };

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Renderer renderer;
    if (!renderer.init("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf")) {
        return 1;
    }

    Board board;
    Player current = Player::Black;
    Move lastMove{-1, -1};
    std::string statusMessage;
    bool gameOver = false;
    std::vector<Move> winLine;

    GameMode mode = GameMode::HumanVsAI;
    const Player humanPlayer = Player::Black;
    const Player aiPlayer = Player::White;

    Minimax aiEngine(1);
    const double AI_TIME_LIMIT_MS = 350.0;
    std::string aiInfo = "No moves yet";

    // Goal: a hint, not a move — the player still has to click it. Cleared
    // whenever a stone is placed so a stale recommendation can't linger and
    // mislead on the following turn.
    Move suggestedMove{-1, -1};
    bool hasSuggestion = false;
    bool suggestionRequested = false;

    // Goal: single source of truth for "may a click place a stone right now."
    // In hotseat both sides are human; against the AI only Black is.
    auto isHumanTurn = [&]() {
        return mode == GameMode::Hotseat || current == humanPlayer;
    };

    auto resetGame = [&]() {
        board = Board();
        current = Player::Black;
        lastMove = Move{-1, -1};
        statusMessage.clear();
        gameOver = false;
        winLine.clear();
        aiInfo = "No moves yet";
        hasSuggestion = false;
        suggestionRequested = false;
    };

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;

            } else if (event.type == SDL_KEYDOWN) {
                SDL_Keycode key = event.key.keysym.sym;
                if (key == SDLK_m) {
                    // Switching mid-game would leave turn ownership ambiguous,
                    // so a mode change always starts a fresh board.
                    mode = (mode == GameMode::HumanVsAI) ? GameMode::Hotseat : GameMode::HumanVsAI;
                    resetGame();
                } else if (key == SDLK_r) {
                    resetGame();
                } else if (key == SDLK_s) {
                    if (!gameOver && isHumanTurn())
                        suggestionRequested = true;
                }

            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (gameOver) {
                    if (event.button.button == SDL_BUTTON_LEFT)
                        resetGame();

                } else if (event.button.button == SDL_BUTTON_LEFT && isHumanTurn()) {
                    Move clicked;
                    if (InputHandler::pixelToBoardCoord(event.button.x, event.button.y, clicked)) {

                        Board::MoveEvaluation eval = board.evaluateMove(clicked, current);

                        if (!eval.legal) {
                            statusMessage = (eval.freeThrees >= 2 && !eval.wouldCapture)
                                ? "Illegal: creates a double-three"
                                : "Illegal: cell occupied";
                        } else {
                            board.placeStone(clicked, current);
                            lastMove = clicked;
                            hasSuggestion = false;

                            int captured = board.checkAndApplyCaptures(clicked, current);
                            statusMessage = (captured > 0)
                                ? ("Captured " + std::to_string(captured) + " stone(s)!")
                                : "";

                            auto win = board.checkWinConditions(clicked, current);
                            if (win.won) {
                                gameOver = true;
                                statusMessage = std::string(win.winner == Player::Black ? "Black" : "White")
                                    + " wins by " + (win.reason == Board::WinReason::Capture ? "capture!" : "alignment!");
                                if (win.reason == Board::WinReason::Alignment) {
                                    winLine = board.findWinningLine(clicked, win.winner);
                                }
                            } else {
                                current = (current == Player::Black) ? Player::White : Player::Black;
                            }
                        }
                    }
                }
            }
        }

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

        renderer.clear();
        renderer.drawBoard();
        renderer.drawStones(board, lastMove);

        if (!gameOver && hasSuggestion)
            renderer.drawSuggestion(suggestedMove);

        if (showHover)
            renderer.drawHoverPreview(hoverMove, current, hoverLegal);

        if (gameOver && !winLine.empty())
            renderer.drawWinLine(winLine);

        PanelInfo panel;
        panel.currentPlayer = current;
        panel.blackCaptured = board.capturedBy(Player::Black);
        panel.whiteCaptured = board.capturedBy(Player::White);
        panel.statusMessage = statusMessage;
        panel.aiInfo = aiInfo;
        panel.aiSectionLabel = (mode == GameMode::Hotseat) ? "Last suggestion" : "AI last move";
        panel.modeText = (mode == GameMode::Hotseat) ? "Hotseat: 2 players" : "Human vs AI";
        panel.thinking = !gameOver &&
                          ((mode == GameMode::HumanVsAI && current == aiPlayer) || suggestionRequested);
        renderer.drawSidePanel(panel);

        if (gameOver)
            renderer.drawGameOverOverlay(statusMessage);

        renderer.present();

        // Both blocking searches run AFTER the frame is presented, so
        // "(thinking...)" is on screen before the window stops responding.
        if (!gameOver && suggestionRequested) {
            suggestionRequested = false;

            auto t0 = std::chrono::steady_clock::now();
            int depthReached = 0;
            SearchResult result = aiEngine.findBestMoveTimed(board, current, AI_TIME_LIMIT_MS, depthReached);
            auto t1 = std::chrono::steady_clock::now();
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

            if (board.isLegal(result.bestMove, current)) {
                suggestedMove = result.bestMove;
                hasSuggestion = true;

                static const char* COLS = "ABCDEFGHJKLMNOPQRST";
                std::ostringstream oss;
                oss << COLS[result.bestMove.x] << (result.bestMove.y + 1)
                    << " - depth " << depthReached << ", " << (int)ms << " ms";
                aiInfo = oss.str();
                statusMessage = "";
            } else {
                hasSuggestion = false;
                statusMessage = "No suggestion available";
            }
        }

        if (!gameOver && mode == GameMode::HumanVsAI && current == aiPlayer) {
            auto searchStart = std::chrono::steady_clock::now();
            int depthReached = 0;
            SearchResult result = aiEngine.findBestMoveTimed(board, aiPlayer, AI_TIME_LIMIT_MS, depthReached);
            auto searchEnd = std::chrono::steady_clock::now();
            double ms = std::chrono::duration<double, std::milli>(searchEnd - searchStart).count();

            Move aiMove = result.bestMove;

            if (!board.isLegal(aiMove, aiPlayer)) {
                bool found = false;
                for (int y = 0; y < Board::SIZE && !found; y++) {
                    for (int x = 0; x < Board::SIZE && !found; x++) {
                        Move candidate{x, y};
                        if (board.isLegal(candidate, aiPlayer)) {
                            aiMove = candidate;
                            found = true;
                        }
                    }
                }
            }

            board.placeStone(aiMove, aiPlayer);
            lastMove = aiMove;
            hasSuggestion = false;

            int captured = board.checkAndApplyCaptures(aiMove, aiPlayer);

            std::ostringstream oss;
            oss << "Depth " << depthReached << ", " << (int)ms << " ms";
            if (captured > 0) oss << " (captured!)";
            aiInfo = oss.str();

            auto win = board.checkWinConditions(aiMove, aiPlayer);
            if (win.won) {
                gameOver = true;
                statusMessage = std::string(win.winner == Player::Black ? "Black" : "White")
                    + " wins by " + (win.reason == Board::WinReason::Capture ? "capture!" : "alignment!");
                if (win.reason == Board::WinReason::Alignment) {
                    winLine = board.findWinningLine(aiMove, win.winner);
                }
            } else {
                statusMessage = (captured > 0)
                    ? ("AI captured " + std::to_string(captured) + " stone(s)!")
                    : "";
                current = humanPlayer;
            }
        }

        SDL_Delay(16);
    }

    return 0;
}