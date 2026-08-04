#include "ui/Renderer.hpp"
#include "ui/InputHandler.hpp"
#include "engine/Board.hpp"
#include "ai/Minimax.hpp"
#include <SDL2/SDL.h>
#include <vector>
#include <string>
#include <sstream>
#include <chrono>

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

    // Goal: fixed roles for this mode — human plays Black and moves first,
    // AI plays White. A future bonus feature (color choice) could make this
    // configurable; kept simple and explicit for now.
    const Player humanPlayer = Player::Black;
    const Player aiPlayer = Player::White;
    Minimax aiEngine(1); // depth is overwritten every call by findBestMoveTimed
    // Real margin under the 500ms hard requirement — the GUI's own event
    // loop and rendering add overhead on top of the search itself.
    const double AI_TIME_LIMIT_MS = 350.0;
    std::string aiInfo = "White hasn't moved yet";

    auto resetGame = [&]() {
        board = Board();
        current = Player::Black;
        lastMove = Move{-1, -1};
        statusMessage.clear();
        gameOver = false;
        winLine.clear();
        aiInfo = "White hasn't moved yet";
    };

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;

            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (gameOver) {
                    if (event.button.button == SDL_BUTTON_LEFT)
                        resetGame();

                // Goal: only accept a click as a move when it's genuinely the
                // human's turn — guards against a click landing during the
                // brief window before the AI's blocking search actually runs.
                } else if (event.button.button == SDL_BUTTON_LEFT && current == humanPlayer) {
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
                                current = aiPlayer;
                            }
                        }
                    }
                }
            }
        }

        renderer.clear();
        renderer.drawBoard();
        renderer.drawStones(board, lastMove);

        if (gameOver && !winLine.empty())
            renderer.drawWinLine(winLine);

        renderer.drawSidePanel(current, board.capturedBy(Player::Black),
                                board.capturedBy(Player::White), statusMessage,
                                aiInfo, /*aiThinking=*/(!gameOver && current == aiPlayer));

        if (gameOver) {
            renderer.drawGameOverOverlay(statusMessage);
        }

        renderer.present();

        // Goal: the AI's turn is handled HERE, after the frame above has
        // already been presented — guarantees "(thinking...)" is actually
        // visible on screen for at least one frame before the blocking
        // search below runs, instead of the window silently freezing.
        if (!gameOver && current == aiPlayer) {
            auto searchStart = std::chrono::steady_clock::now();
            int depthReached = 0;
            SearchResult result = aiEngine.findBestMoveTimed(board, aiPlayer, AI_TIME_LIMIT_MS, depthReached);
            auto searchEnd = std::chrono::steady_clock::now();
            double ms = std::chrono::duration<double, std::milli>(searchEnd - searchStart).count();

            Move aiMove = result.bestMove;

            // Safety net: the search doesn't enforce the double-three rule
            // when generating candidates, so in rare cases its chosen move
            // could be illegal. Never apply an illegal move — fall back to
            // scanning for any legal cell instead.
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