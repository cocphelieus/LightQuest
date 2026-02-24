#pragma once
#include <SDL2/SDL.h>

class MapManager
{
public:
    MapManager();
    ~MapManager();

    bool load(SDL_Renderer* renderer, const char* backgroundPath);
    void render(SDL_Renderer* renderer);
    void clean();

    int getTile(int row, int col) const;
    bool isWall(int row, int col) const;

private:
    static const int ROWS = 20;
    static const int COLS = 20;
    static const int TILE_SIZE = 32;

    int map[ROWS][COLS];
    SDL_Texture* backgroundTexture = nullptr;

    void initializeMap();
};
