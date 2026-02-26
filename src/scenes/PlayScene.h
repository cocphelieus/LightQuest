#pragma once
#include <SDL2/SDL.h>
#include "../map/MapManager.h"
#include "../entities/Player.h"

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
    Player player;
    SDL_Rect testRect;
};
