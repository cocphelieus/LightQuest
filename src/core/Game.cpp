#include "Game.h"
#include <SDL2/SDL.h>

void Game::run() {
    window.init("Dark Maze", 800, 600);
    menu.load(window.getRenderer());

    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = false;
        }

        window.clear();
        menu.render(window.getRenderer());
        window.present();
    }

    menu.clean();
    window.clean();
}
