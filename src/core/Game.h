#pragma once
#include "Window.h"
#include "../scenes/MenuScene.h"
#include "../scenes/LoadingScene.h"
#include "../scenes/PlayScene.h"
#include <SDL2/SDL.h>

enum class GameState {
    LOADING,
    MENU,
    PLAYING
};

class Game {
public:
    void run();

private:
    // ===== Core =====
    bool running = true;
    GameState currentState = GameState::LOADING;

    // ===== Systems =====
    Window window;

    // ===== Scenes =====
    LoadingScene loading;
    MenuScene menu;
    PlayScene play;

    // ===== Timing =====
    Uint32 lastFrameTime = 0;

    static constexpr float TARGET_FPS = 60.0f;
    static constexpr float FRAME_TIME = 1000.0f / TARGET_FPS; // ms per frame
};
