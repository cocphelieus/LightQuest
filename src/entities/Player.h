#pragma once
#include <SDL2/SDL.h>

class MapManager;

class Player
{
public:
    Player();
    ~Player();

    // input handler kept for non-movement interactions; movement is handled in update()
    void handleEvent(SDL_Event &event, const MapManager &map);

    // continuous pixel movement, called every frame from PlayScene::update
    void update(const Uint8* keystate, float deltaTime, const MapManager &map);

    // draw the player at its current pixel position; the scene provides map offsets
    void render(SDL_Renderer *renderer, int offsetX, int offsetY);

    // set logical tile location and optionally supply tile size to position pixel coordinates
    void setPosition(int row, int col, int tileSize);
    int getRow() const { return row; }
    int getCol() const { return col; }

    // texture management
    bool loadTexture(SDL_Renderer* renderer);
    void clean();

private:
    static constexpr float BODY_WIDTH = 48.0f;
    static constexpr float BODY_HEIGHT = 48.0f;
    static constexpr float FOOT_OFFSET = 2.0f;

    int row;
    int col;
    float x, y;
    float speed;

    SDL_Texture* texture = nullptr;
};