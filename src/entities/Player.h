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
    void update(float deltaTime, MapManager& map);
    void render(SDL_Renderer* renderer, int offsetX, int offsetY);
    void clean();

    void setPosition(int row, int col, int tileSize);

    bool isAlive() const { return alive; }
    int getRow() const { return row; }
    int getCol() const { return col; }

private:
    int row, col;
    int targetRow, targetCol;

    float posX, posY;
    bool isMoving;
    float moveSpeed;

    SDL_Texture* texture = nullptr;

    int tileSize;

    bool alive = true;
};