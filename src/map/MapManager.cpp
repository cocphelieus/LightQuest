#include "MapManager.h"
#include <SDL2/SDL_image.h>
#include "../core/Asset.h"
#include <cstdlib>
#include <ctime>
#include <iostream>

#include <algorithm>  // for std::max

MapManager::MapManager(int r, int c, int tCount, int mCount)
    : rows(r), cols(c), torchCount(tCount), mineCount(mCount)
{
    std::srand((unsigned)std::time(nullptr));
    // allocate the grid vectors
    map.assign(rows, std::vector<int>(cols, FLOOR));
    visible.assign(rows, std::vector<bool>(cols, false));
    torchActivated.assign(rows, std::vector<bool>(cols, false));
    initializeMap();
}

void MapManager::reset()
{
    initializeMap();
}

MapManager::~MapManager()
{
    clean();
}

void MapManager::initializeMap()
{
    // resize/clear arrays
    map.assign(rows, std::vector<int>(cols, FLOOR));
    visible.assign(rows, std::vector<bool>(cols, false));
    torchActivated.assign(rows, std::vector<bool>(cols, false));

    // base layout: walls around, floor inside
    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            if (r == 0 || r == rows - 1 || c == 0 || c == cols - 1)
                map[r][c] = WALL;
            else
                map[r][c] = FLOOR;
        }
    }

    if (rows > 1 && cols > 1)
        visible[1][1] = true;

    carveSafePath();
    placeTorches(torchCount);
    placeMines(mineCount);
}

void MapManager::carveSafePath()
{
    safePath.clear();
    int r = 1, c = 1;
    safePath.emplace_back(r,c);
    while (r < rows - 2)
    {
        map[r][c] = FLOOR;
        r++;
        safePath.emplace_back(r,c);
    }
    while (c < cols - 2)
    {
        map[r][c] = FLOOR;
        c++;
        safePath.emplace_back(r,c);
    }
    if (rows > 2 && cols > 2)
    {
        map[rows - 2][cols - 2] = FLOOR;
        safePath.emplace_back(rows-2, cols-2);
    }
}

void MapManager::placeTorches(int count)
{
    int placed = 0;
    if (!safePath.empty())
    {
        int pathLen = safePath.size();
        int interval = std::max(1, pathLen / count);
        int idx = 0;
        while (placed < count && idx < pathLen)
        {
            auto [r,c] = safePath[idx];
            if (map[r][c] == FLOOR)
            {
                map[r][c] = TORCH;
                visible[r][c] = true;
                placed++;
            }
            idx += interval;
        }
        idx = 0;
        while (placed < count && idx < pathLen)
        {
            auto [r,c] = safePath[idx];
            if (map[r][c] == FLOOR)
            {
                map[r][c] = TORCH;
                visible[r][c] = true;
                placed++;
            }
            idx++;
        }
    }
    if (rows > 1 && cols > 1)
    {
        if (map[1][1] == TORCH) { map[1][1] = FLOOR; visible[1][1]=true; }
        if (map[rows-2][cols-2] == TORCH) { map[rows-2][cols-2] = FLOOR; visible[rows-2][cols-2]=true; }
    }
}

// helper to check safe path membership
static bool onSafePath(const std::vector<std::pair<int,int>>& path, int r, int c)
{
    for (auto &p : path)
        if (p.first == r && p.second == c)
            return true;
    return false;
}

void MapManager::placeMines(int count)
{
    int placed = 0;
    while (placed < count)
    {
        int r = rand() % (rows - 2) + 1;
        int c = rand() % (cols - 2) + 1;
        if ((r == 1 && c == 1) || (r == rows-2 && c == cols-2))
            continue;
        if (onSafePath(safePath, r, c))
            continue;
        if (map[r][c] == FLOOR)
        {
            map[r][c] = MINE;
            placed++;
        }
    }
}

bool MapManager::load(SDL_Renderer *renderer, const char *backgroundPath)
{
    backgroundTexture = IMG_LoadTexture(renderer, backgroundPath);
    loadTileTextures(renderer);
    return backgroundTexture != nullptr;
}

void MapManager::loadTileTextures(SDL_Renderer* renderer)
{
    if (!renderer) return;
    tileDarkTex  = IMG_LoadTexture(renderer, ASSET("entities/tile_dark.png"));
    tileLightTex = IMG_LoadTexture(renderer, ASSET("entities/tile_light.png"));
    mineTex      = IMG_LoadTexture(renderer, ASSET("entities/bom.png"));
    torchTex     = IMG_LoadTexture(renderer, ASSET("entities/fire.png"));
    startTex     = IMG_LoadTexture(renderer, ASSET("entities/bot.png"));
    goalTex      = nullptr;
}

void MapManager::render(SDL_Renderer* renderer)
{
    if (backgroundTexture)
    {
        SDL_RenderCopy(renderer, backgroundTexture, nullptr, nullptr);
    }

    int mapWidth = cols * TILE_SIZE;
    int mapHeight = rows * TILE_SIZE;
    int screenWidth = 1280;
    int screenHeight = 720;
    int offsetX = (screenWidth - mapWidth) / 2;
    int offsetY = (screenHeight - mapHeight) / 2;

    SDL_Rect tileRect{0, 0, TILE_SIZE, TILE_SIZE};

    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            tileRect.x = offsetX + c * TILE_SIZE;
            tileRect.y = offsetY + r * TILE_SIZE;

            SDL_Texture* tex = nullptr;
            if (tileVisible(r,c))
                tex = tileLightTex;
            else
                tex = tileDarkTex;

            if (tex) SDL_RenderCopy(renderer, tex, nullptr, &tileRect);
            else
            {
                if (tileVisible(r,c))
                    SDL_SetRenderDrawColor(renderer, 180,180,180,255);
                else
                    SDL_SetRenderDrawColor(renderer, 30,30,30,255);
                SDL_RenderFillRect(renderer,&tileRect);
            }

            if (tileVisible(r,c))
            {
                if (map[r][c] == TORCH && torchTex)
                    SDL_RenderCopy(renderer, torchTex, nullptr, &tileRect);
                else if (map[r][c] == MINE && mineTex)
                    SDL_RenderCopy(renderer, mineTex, nullptr, &tileRect);
                else if (r==1 && c==1 && startTex)
                    SDL_RenderCopy(renderer, startTex, nullptr, &tileRect);
                else if (r==rows-2 && c==cols-2 && goalTex)
                    SDL_RenderCopy(renderer, goalTex, nullptr, &tileRect);
            }
        }
    }
    // draw grid lines on top
    SDL_SetRenderDrawColor(renderer, 0,0,0,100);
    for(int r=0;r<=rows;r++){
        int y=offsetY + r*TILE_SIZE;
        SDL_RenderDrawLine(renderer, offsetX, y, offsetX+mapWidth, y);
    }
    for(int c=0;c<=cols;c++){
        int x=offsetX + c*TILE_SIZE;
        SDL_RenderDrawLine(renderer, x, offsetY, x, offsetY+mapHeight);
    }
}

int MapManager::getTile(int row, int col) const
{
    if (row < 0 || row >= rows || col < 0 || col >= cols)
        return WALL;
    return map[row][col];
}

bool MapManager::isWall(int row, int col) const
{
    return getTile(row,col) == WALL;
}

bool MapManager::isMine(int row, int col) const
{
    return getTile(row,col) == MINE;
}

bool MapManager::isTorch(int row, int col) const
{
    return getTile(row,col) == TORCH;
}

void MapManager::reveal(int row, int col)
{
    if (row < 0 || row >= rows || col < 0 || col >= cols)
        return;
    visible[row][col] = true;
}

void MapManager::revealArea(int row, int col)
{
    for (int dr = -1; dr <= 1; ++dr)
        for (int dc = -1; dc <= 1; ++dc)
            reveal(row + dr, col + dc);
}

bool MapManager::tileVisible(int row, int col) const
{
    if (row < 0 || row >= rows || col < 0 || col >= cols)
        return false;
    return visible[row][col] || map[row][col] == TORCH;
}

bool MapManager::activateTorch(int row, int col, bool correct)
{
    if (!isTorch(row,col) || torchActivated[row][col])
        return false;

    torchActivated[row][col] = true;
    if (correct)
    {
        std::cout << "Torch at (" << row << "," << col << ") activated with correct answer, revealing area.\n";
        revealArea(row,col);
    }
    else
    {
        std::cout << "Torch at (" << row << "," << col << ") activated but answer was wrong.\n";
    }
    return true;
}

bool MapManager::torchActivatedAt(int row, int col) const
{
    if (row < 0 || row >= rows || col < 0 || col >= cols)
        return false;
    return torchActivated[row][col];
}

void MapManager::unloadTileTextures()
{
    if (tileDarkTex) { SDL_DestroyTexture(tileDarkTex); tileDarkTex=nullptr; }
    if (tileLightTex) { SDL_DestroyTexture(tileLightTex); tileLightTex=nullptr; }
    if (mineTex) { SDL_DestroyTexture(mineTex); mineTex=nullptr; }
    if (torchTex) { SDL_DestroyTexture(torchTex); torchTex=nullptr; }
    if (startTex) { SDL_DestroyTexture(startTex); startTex=nullptr; }
    if (goalTex) { SDL_DestroyTexture(goalTex); goalTex=nullptr; }
}

void MapManager::clean()
{
    if (backgroundTexture)
    {
        SDL_DestroyTexture(backgroundTexture);
        backgroundTexture = nullptr;
    }
    unloadTileTextures();
}
