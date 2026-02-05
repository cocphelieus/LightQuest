#include "Game.h"
#include <SDL2/SDL.h>

void Game::run() {
    window.init("Dark Maze", 1280, 720);
    loading.load(window.getRenderer());
    menu.load(window.getRenderer());

    SDL_Event event;
    lastFrameTime = SDL_GetTicks();

    while (running) {
        Uint32 frameStart = SDL_GetTicks();
        float deltaTime = (frameStart - lastFrameTime) / 1000.0f;
        lastFrameTime = frameStart;

        // Handle events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = false;
            else if (event.type == SDL_KEYDOWN) {
                // Skip loading screen on SPACE
                if (currentState == GameState::LOADING && event.key.keysym.sym == SDLK_SPACE) {
                    currentState = GameState::MENU;
                }
            }
            
            // Pass events to menu
            if (currentState == GameState::MENU) {
                menu.handleEvent(event);
                if (menu.getState() == MenuState::EXIT) {
                    running = false;
                }
            }
        }

        // Update
        switch (currentState) {
            case GameState::LOADING:
                loading.update(deltaTime);
                if (loading.isComplete()) {
                    currentState = GameState::MENU;
                }
                break;
            case GameState::MENU:
                menu.update();
                break;
            case GameState::PLAYING:
                // Game update logic
                break;
        }

        // Render
        window.clear();
        switch (currentState) {
            case GameState::LOADING:
                loading.render(window.getRenderer());
                break;
            case GameState::MENU:
                menu.render(window.getRenderer());
                break;
            case GameState::PLAYING:
                // Game render logic
                break;
        }
        window.present();

        // Frame rate limiting
        Uint32 frameTime = SDL_GetTicks() - frameStart;
        if (frameTime < FRAME_TIME) {
            SDL_Delay(static_cast<Uint32>(FRAME_TIME - frameTime));
        }
    }

    loading.clean();
    menu.clean();
    window.clean();
}
