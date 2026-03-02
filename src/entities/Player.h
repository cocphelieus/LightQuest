#pragma once
#include <SDL2/SDL.h>

class MapManager;

class Player
{
public:
    Player();

    void handleEvent(SDL_Event& event, const MapManager& map);
    void render(SDL_Renderer* renderer, int offsetX, int offsetY, int tileSize);

    void setPosition(int row, int col);
    int getRow() const { return row; }
    int getCol() const { return col; }

private:
    int row;
    int col;
};