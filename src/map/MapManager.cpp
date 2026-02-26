#include "MapManager.h"
#include <SDL2/SDL_image.h>

MapManager::MapManager()
{
    initializeMap();
}

MapManager::~MapManager()
{
    clean();
}

void MapManager::initializeMap()
{
    // Tạm thời làm map viền là tường
    for (int r = 0; r < ROWS; r++)
    {
        for (int c = 0; c < COLS; c++)
        {
            if (r == 0 || r == ROWS - 1 || c == 0 || c == COLS - 1)
                map[r][c] = 1; // wall
            else
                map[r][c] = 0; // floor
        }
    }
}

bool MapManager::load(SDL_Renderer *renderer, const char *backgroundPath)
{
    backgroundTexture = IMG_LoadTexture(renderer, backgroundPath);
    return backgroundTexture != nullptr;
}

void MapManager::render(SDL_Renderer* renderer)
{
    // 1️⃣ Render background full screen
    if (backgroundTexture)
    {
        SDL_RenderCopy(renderer, backgroundTexture, nullptr, nullptr);
    }

    // 2️⃣ Tính offset để căn giữa grid
    int mapWidth = COLS * TILE_SIZE;
    int mapHeight = ROWS * TILE_SIZE;

    int screenWidth = 1280;
    int screenHeight = 720;

    int offsetX = (screenWidth - mapWidth) / 2;
    int offsetY = (screenHeight - mapHeight) / 2;

    // 3️⃣ Render grid logic
    SDL_Rect tileRect;
    tileRect.w = TILE_SIZE;
    tileRect.h = TILE_SIZE;

    for (int r = 0; r < ROWS; r++)
    {
        for (int c = 0; c < COLS; c++)
        {
            tileRect.x = offsetX + c * TILE_SIZE;
            tileRect.y = offsetY + r * TILE_SIZE;

            if (map[r][c] == 1)
            {
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 120);
                SDL_RenderFillRect(renderer, &tileRect);
            }
        }
    }
}
int MapManager::getTile(int row, int col) const
{
    if (row < 0 || row >= ROWS || col < 0 || col >= COLS)
        return 1; // coi ngoài map là tường

    return map[row][col];
}

bool MapManager::isWall(int row, int col) const
{
    return getTile(row, col) == 1;
}

void MapManager::clean()
{
    if (backgroundTexture)
    {
        SDL_DestroyTexture(backgroundTexture);
        backgroundTexture = nullptr;
    }
}
