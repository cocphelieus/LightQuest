#pragma once
#include <SDL2/SDL.h>

enum class Difficulty
{
    EASY,
    MEDIUM,
    HARD
};

class MapManager
{
public:
    MapManager();
    ~MapManager();

    bool load(SDL_Renderer* renderer, const char* backgroundPath, Difficulty difficulty = Difficulty::MEDIUM);
    void render(SDL_Renderer* renderer);
    void clean();
    void reset(Difficulty difficulty);

    int getTile(int row, int col) const;
    bool isWall(int row, int col) const;
    bool isMine(int row, int col) const;
    bool isTorch(int row, int col) const;
    bool isGoal(int row, int col) const;

    bool canAttemptTorch(int row, int col) const;
    void resolveTorch(int row, int col, bool correctAnswer);
    bool activateStarterTorch();
    void revealAroundTorch(int row, int col);
    void revealRadius(int row, int col, int radius, bool includeCenter);
    void revealAt(int row, int col);
    void revealForwardStep(int row, int col, int dRow, int dCol);
    void revealShadowAround(int row, int col, int radius);
    bool spawnTorchInOpenedArea(int centerRow, int centerCol, int maxDistance);
    void spawnNextWave(int playerRow, int playerCol, int mineToAdd, int torchToAdd);
    bool revealPathToNextTorch(int fromRow, int fromCol, int solvedTorchCount);
    void hintNearestTorch(int fromRow, int fromCol);
    void clearTorchHint();
    bool hasAnyUnresolvedTorch() const;
    bool isKnown(int row, int col) const;
    bool isDetailVisible(int row, int col) const;
    void setTesterOverlay(bool revealMines, bool revealTorches);

    int getRows() const { return ROWS; }
    int getCols() const { return COLS; }
    int getTileSize() const { return TILE_SIZE; }
    int getRenderOffsetX() const { return RENDER_OFFSET_X; }
    int getRenderOffsetY() const { return RENDER_OFFSET_Y; }
    int getStartRow() const { return startRow; }
    int getStartCol() const { return startCol; }
    int getGoalRow() const { return goalRow; }
    int getGoalCol() const { return goalCol; }

private:
    static const int ROWS = 10;
    static const int COLS = 12;
    static const int TILE_SIZE = 55;
    static const int RENDER_OFFSET_X = 310;
    static const int RENDER_OFFSET_Y = 84;

    enum TileType
    {
        FLOOR = 0,
        WALL = 1,
        MINE = 2,
        TORCH = 3,
        GOAL = 4
    };

    enum TorchState
    {
        TORCH_NOT_USED = 0,
        TORCH_FAILED = 1,
        TORCH_SUCCESS = 2
    };

    enum PlacementVisibility
    {
        ANY_VISIBILITY = -1,
        HIDDEN_ONLY = 0,
        VISIBLE_ONLY = 1
    };

    int map[ROWS][COLS];
    bool visible[ROWS][COLS];
    bool shadowVisible[ROWS][COLS];
    int torchState[ROWS][COLS];
    SDL_Texture* backgroundTexture = nullptr;

    int startRow = 0;
    int startCol = 0;
    int goalRow = ROWS - 1;
    int goalCol = COLS - 1;
    int hintTorchRow = -1;
    int hintTorchCol = -1;
    Uint32 hintUntilTick = 0;
    bool testerRevealMines = false;
    bool testerRevealTorches = false;

    void initializeMap();
    void placeRandomTiles(int tileType, int count);
    void placeMinesNearTorches(int count);
    bool placeOneRandomTile(int tileType, int safeRow, int safeCol, int safeRadius, int visibilityRule, bool revealPlacedTile);
    bool placeReachableVisibleTorch(int playerRow, int playerCol);
    void carveSafePath(int fromRow, int fromCol, int toRow, int toCol);
    void revealCell(int row, int col);
    void revealShadowCell(int row, int col);
    bool hasAdjacentMine(int row, int col) const;
    bool hasNearbyTorch(int row, int col, int minDistance) const;
    bool hasHiddenNeighbor(int row, int col) const;
    bool isForwardCell(int baseRow, int baseCol, int row, int col) const;
    int sign(int value) const;
    void revealTowardGoalPath(int fromRow, int fromCol, int steps);
    void revealRandomCluster(int centerRow, int centerCol, int seeds, int maxStep);
    void revealInitialArea();
    void ensureStarterTorch();
    void buildFixedTorchNetwork(Difficulty difficulty);
    bool findNearestUnresolvedTorch(int fromRow, int fromCol, int& outRow, int& outCol) const;
    bool findNextUnresolvedTorch(int fromRow, int fromCol, int& outRow, int& outCol) const;
};
