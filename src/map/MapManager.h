#pragma once
#include <SDL2/SDL.h>
#include <vector>
#include <utility>

class MapManager
{
public:
    enum TileType
    {
        FLOOR = 0,
        WALL  = 1,
        MINE  = 2,
        TORCH = 3
    };

    // constructor takes dimensions and counts; defaults are small map with many torches
    MapManager(int rows = 10, int cols = 10, int torchCount = 10, int mineCount = 20);
    ~MapManager();

    bool load(SDL_Renderer* renderer, const char* backgroundPath);
    void render(SDL_Renderer* renderer);
    void clean();
    void reset();

    int getRows() const { return rows; }
    int getCols() const { return cols; }
    static constexpr int TILE_SIZE = 32;

    int getTile(int row, int col) const;
    bool isWall(int row, int col) const;
    bool isMine(int row, int col) const;
    bool isTorch(int row, int col) const;

    // visibility controls
    void reveal(int row, int col);
    void revealArea(int row, int col);
    bool tileVisible(int row, int col) const;

    // torch interaction
    bool activateTorch(int row, int col, bool correct);
    bool torchActivatedAt(int row, int col) const;

private:
    int rows;
    int cols;
    int torchCount;
    int mineCount;

    std::vector<std::vector<int>> map;
    std::vector<std::vector<bool>> visible;
    std::vector<std::vector<bool>> torchActivated;

    SDL_Texture* backgroundTexture = nullptr;
    // additional tile / entity textures
    SDL_Texture* tileDarkTex = nullptr;
    SDL_Texture* tileLightTex = nullptr;
    SDL_Texture* mineTex = nullptr;
    SDL_Texture* torchTex = nullptr;
    SDL_Texture* startTex = nullptr;
    SDL_Texture* goalTex = nullptr;

    void initializeMap();
    void placeMines(int count);
    void placeTorches(int count);
    void carveSafePath();
    // store the safe path coordinates
    std::vector<std::pair<int,int>> safePath;
    void loadTileTextures(SDL_Renderer* renderer);
    void unloadTileTextures();
};
