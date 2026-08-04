#include "ui/Renderer.hpp"
#include <SDL2/SDL.h>

// Goal: entry point for the GUI. SDL2 expects main() with the standard
// (argc, argv) signature on Linux — SDL.h can rename main -> SDL_main
// internally depending on build config, and that rename only matches
// cleanly against this signature. A zero-argument main() risks a link error
// or unpredictable startup behavior depending on how SDL was built.
int main(int argc, char* argv[]) {
    (void)argc; // unused — required by SDL's entry point convention, not by us
    (void)argv;

    Renderer renderer;
    if (!renderer.init("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")) {
        return 1; // init() already printed the specific error — never crash silently
    }

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = false;
        }

        renderer.clear();
        renderer.drawBoard();
        renderer.present();

        // Goal: cap the loop to roughly 60fps. Without this, the render loop
        // spins as fast as the CPU allows with nothing to wait on — pegging
        // one core at 100% for a static board that never even changes. This
        // matters more once the game loop is doing real work every frame.
        SDL_Delay(16);
    }

    return 0;
}