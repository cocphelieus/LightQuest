#pragma once
#include "Window.h"
#include "../scenes/MenuScene.h"
#include "../scenes/LoadingScene.h"
#include "../scenes/PlayScene.h"
#include <SDL2/SDL.h>

enum class GameState {
    // Loading assets and intro animation.
    LOADING,
    // Main menu and overlay screens (story, guide, rank, exit confirm).
    MENU,
    // Active gameplay scene.
    PLAYING
};

class Game {
public:
    // Entry point for the entire runtime lifecycle.
    void run();

private:
    // ===== Core =====
    // Global loop flag. Set to false to end the app.
    bool running = true;
    GameState currentState = GameState::LOADING;

    // ===== Systems =====
    Window window;

    // ===== Scenes =====
    LoadingScene loading;
    MenuScene menu;
    PlayScene play;

    // ===== Timing =====
    // Previous frame timestamp for delta time calculation.
    Uint32 lastFrameTime = 0;

    static constexpr float TARGET_FPS = 60.0f;
    static constexpr float FRAME_TIME = 1000.0f / TARGET_FPS; // ms per frame
};
