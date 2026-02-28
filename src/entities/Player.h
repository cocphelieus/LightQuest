#pragma once
#include <SDL2/SDL.h>

class MapManager;

class Player
{
public:
    Player();
    // ~Player();

    bool load(SDL_Renderer* renderer, const char* path);
    void handleEvent(SDL_Event& event, const MapManager& map);
    void update(float deltaTime, const MapManager& map);
    void render(SDL_Renderer* renderer, int offsetX, int offsetY);
    void clean();

    void setPosition(int row, int col, int tileSize);

private:
    int row, col;
    int targetRow, targetCol;

    float posX, posY;
    bool isMoving;
    float moveSpeed;

    SDL_Texture* texture = nullptr;

    int tileSize;
};