#pragma once
#include "Window.h"
#include "../scenes/MenuScene.h"
#include "../scenes/LoadingScene.h"
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
    bool running = true;
    GameState currentState = GameState::LOADING;
    Window window;
    MenuScene menu;
    LoadingScene loading;
    
    Uint32 lastFrameTime = 0;
    const float TARGET_FPS = 60.0f;
    const float FRAME_TIME = 1000.0f / TARGET_FPS; // milliseconds
};
