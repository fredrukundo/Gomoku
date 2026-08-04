#include "ui/Renderer.hpp"
#include "ui/InputHandler.hpp"
#include "engine/Board.hpp"
#include <SDL2/SDL.h>

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
    Move lastMove{-1, -1}; // -1: no move yet, drawStones skips highlighting anything
    std::string statusMessage;
    bool gameOver = false;

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_MOUSEBUTTONDOWN && !gameOver) {
                if (event.button.button == SDL_BUTTON_LEFT) {
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
                            } else {
                                current = (current == Player::Black) ? Player::White : Player::Black;
                            }
                        }
                    }
                }
            }
        }

        renderer.clear();
        renderer.drawBoard();
        renderer.drawStones(board, lastMove);
        renderer.drawSidePanel(current, board.capturedBy(Player::Black),
                                board.capturedBy(Player::White), statusMessage);
        renderer.present();

        SDL_Delay(16);
    }

    return 0;
}