#pragma once
#include <SDL2/SDL.h>

class MapManager;

class Player
{
public:
    Player();

    void handleEvent(SDL_Event& event, const MapManager& map);
    void render(SDL_Renderer* renderer, int offsetX, int offsetY);

    void setPosition(int row, int col);

private:
    int row;
    int col;

    static const int TILE_SIZE = 32;
};