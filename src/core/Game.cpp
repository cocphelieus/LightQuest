#include "Game.h"
#include <SDL2/SDL.h>

void Game::run()
{
    // ===== Init Window =====
    window.init("LightQuest", 1280, 720, true);

    // ===== Load Scenes =====
    loading.load(window.getRenderer());
    menu.load(window.getRenderer());

    SDL_Event event;
    lastFrameTime = SDL_GetTicks();

    // ===== Main Game Loop =====
    while (running)
    {
        Uint32 frameStart = SDL_GetTicks();
        float deltaTime = (frameStart - lastFrameTime) / 1000.0f;
        lastFrameTime = frameStart;

        // =========================
        // HANDLE EVENTS
        // =========================
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
                running = false;

            // TOGGLE FULLSCREEN (F11)
            if (event.type == SDL_KEYDOWN &&
                event.key.keysym.sym == SDLK_F11)
            {
                window.toggleFullscreen();
            }

            // LOADING: Skip with SPACE
            if (currentState == GameState::LOADING &&
                event.type == SDL_KEYDOWN &&
                event.key.keysym.sym == SDLK_SPACE)
            {
                currentState = GameState::MENU;
            }

            // MENU
            if (currentState == GameState::MENU)
            {
                menu.handleEvent(event);

                if (menu.getState() == MenuState::EXIT)
                    running = false;

                if (menu.getState() == MenuState::PLAY)
                {
                    currentState = GameState::PLAYING;
                    play.load(window.getRenderer());
                }
            }

            // PLAYING
            if (currentState == GameState::PLAYING)
            {
                play.handleEvent(event);
            }
        }

        // =========================
        // UPDATE
        // =========================
        switch (currentState)
        {
        case GameState::LOADING:
            loading.update(deltaTime);
            if (loading.isComplete())
                currentState = GameState::MENU;
            break;

        case GameState::MENU:
            menu.update();
            break;

        case GameState::PLAYING:
            play.update(deltaTime);
            break;
        }

        // =========================
        // RENDER
        // =========================
        window.clear();

        switch (currentState)
        {
        case GameState::LOADING:
            loading.render(window.getRenderer());
            break;

        case GameState::MENU:
            menu.render(window.getRenderer());
            break;

        case GameState::PLAYING:
            play.render(window.getRenderer());
            break;
        }

        window.present();

        // =========================
        // FPS LIMIT
        // =========================
        Uint32 frameTime = SDL_GetTicks() - frameStart;
        if (frameTime < FRAME_TIME)
        {
            SDL_Delay(static_cast<Uint32>(FRAME_TIME - frameTime));
        }
    }

    // ===== Clean Up =====
    play.clean();
    menu.clean();
    loading.clean();
    window.clean();
}
