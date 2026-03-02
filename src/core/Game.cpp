#include "Game.h"
#include <SDL2/SDL.h>
#include <cstdio>

static Difficulty difficultyForCampaignStage(int stage)
{
    if (stage <= 0)
        return Difficulty::EASY;
    if (stage == 1)
        return Difficulty::MEDIUM;
    return Difficulty::HARD;
}

static const char* stageName(int stage)
{
    if (stage <= 0)
        return "De";
    if (stage == 1)
        return "Trung binh";
    return "Kho";
}

static void showStageClearMessage(int stage)
{
    char message[256];
    std::snprintf(
        message,
        sizeof(message),
        "Chuc mung! Ban da pha dao man %s.",
        stageName(stage)
    );

    SDL_ShowSimpleMessageBox(
        SDL_MESSAGEBOX_INFORMATION,
        "Qua man",
        message,
        nullptr
    );
}

static void showCampaignCompleteMessage()
{
    SDL_ShowSimpleMessageBox(
        SDL_MESSAGEBOX_INFORMATION,
        "Chuc mung",
        "Ban da pha dao ca 3 man De - Trung binh - Kho!",
        nullptr
    );
}

static void startCampaignFromBeginning(PlayScene& play, SDL_Renderer* renderer)
{
    play.setDifficulty(difficultyForCampaignStage(0));
    play.load(renderer);
}

void Game::run()
{
    // ===== Init Window =====
    window.init("LightQuest", 1280, 720);

    // ===== Load Scenes =====
    loading.load(window.getRenderer());
    menu.load(window.getRenderer());

    SDL_Event event;
    lastFrameTime = SDL_GetTicks();

    int campaignStage = 0;

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
                    campaignStage = 0;
                    currentState = GameState::PLAYING;
                    startCampaignFromBeginning(play, window.getRenderer());
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
            if (play.shouldReturnToMenu())
            {
                PlayOutcome outcome = play.getOutcome();

                if (outcome == PlayOutcome::CLEARED)
                {
                    showStageClearMessage(campaignStage);
                    campaignStage++;

                    if (campaignStage < 3)
                    {
                        play.clean();
                        play.setDifficulty(difficultyForCampaignStage(campaignStage));
                        play.load(window.getRenderer());
                    }
                    else
                    {
                        showCampaignCompleteMessage();
                        currentState = GameState::MENU;
                        menu.resetToMain();
                        play.clean();
                        campaignStage = 0;
                    }
                }
                else
                {
                    currentState = GameState::MENU;
                    menu.resetToMain();
                    play.clean();
                    campaignStage = 0;
                }
            }
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
