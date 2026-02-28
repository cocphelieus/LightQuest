#pragma once
#include <SDL2/SDL.h>
#include "../map/MapManager.h"
#include "../entities/Player.h"
#include "../quiz/QuestionManager.h"
#include <SDL2/SDL_ttf.h>
#include <string>

class PlayScene
{
public:
    PlayScene();

    void load(SDL_Renderer* renderer);
    void handleEvent(SDL_Event& event);
    void update(float deltaTime);
    void render(SDL_Renderer* renderer);
    void clean();

private:
    MapManager map;
    // configuration for current level, default to smaller grid with many torches
    int mapRows = 10;
    int mapCols = 10;
    int mapTorches = 15;
    int mapMines = 30;
    Player player;
    SDL_Rect testRect;

    SDL_Renderer* renderer = nullptr; // cached for reloads
    bool gameOver = false;
    bool hasWon = false;

    // question UI manager
    QuestionManager qManager;
    class HUD* hud = nullptr; // forward-declared later

    void resetLevel();

};

// forward declare HUD class for pointer in PlayScene
#include "../quiz/HUD.h"

