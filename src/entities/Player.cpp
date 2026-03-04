#include "Player.h"
#include "../map/MapManager.h"
#include <SDL2/SDL_image.h>

// Player::Player()
// {
//     row = 1;
//     col = 1;
// }

Player::Player()
{
    x = 64;   // vị trí pixel ban đầu (ví dụ tileSize = 64)
    y = 64;
    speed = 200.0f; // pixel / giây
    texture = nullptr;
}

Player::~Player()
{
    clean();
}

void Player::setPosition(int r, int c, int tileSize)
{
    row = r;
    col = c;
    // align pixel coordinates with tile grid
    x = static_cast<float>(col) * static_cast<float>(tileSize);
    y = static_cast<float>(row) * static_cast<float>(tileSize);
}

bool Player::loadTexture(SDL_Renderer* renderer)
{
    // load character sprite (bot) from assets
    if (!renderer)
        return false;

    SDL_Surface* surf = IMG_Load("assets/images/entities/bot.png");
    if (!surf)
        return false;
    texture = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);
    return (texture != nullptr);
}

void Player::clean()
{
    if (texture)
    {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
}

void Player::handleEvent(SDL_Event& event, const MapManager& map)
{
    // Movement is now handled by Player::update using continuous coordinates and velocity.
    // This stub remains for future input handling (interact with torches, etc.).
    (void)event;
    (void)map;
}

void Player::update(const Uint8* keystate, float deltaTime, const MapManager& map)
{
    float newX = x;
    float newY = y;

    float moveAmount = speed * deltaTime;
    if (keystate[SDL_SCANCODE_W] || keystate[SDL_SCANCODE_UP])
        newY -= moveAmount;

    if (keystate[SDL_SCANCODE_S] || keystate[SDL_SCANCODE_DOWN])
        newY += moveAmount;

    if (keystate[SDL_SCANCODE_A] || keystate[SDL_SCANCODE_LEFT])
        newX -= moveAmount;

    if (keystate[SDL_SCANCODE_D] || keystate[SDL_SCANCODE_RIGHT])
        newX += moveAmount;

    // ===== enforce map boundaries =====
    int tileSize = map.getTileSize();
    int mapW = map.getCols() * tileSize;
    int mapH = map.getRows() * tileSize;
    const float playerW = 40.0f;
    const float playerH = 40.0f;

    if (newX < 0.0f) newX = 0.0f;
    if (newY < 0.0f) newY = 0.0f;
    if (newX > mapW - playerW) newX = mapW - playerW;
    if (newY > mapH - playerH) newY = mapH - playerH;

    // ===== collision per axis =====
    // horizontal move first
    float testX = newX;
    int currRow = (int)(y / tileSize);
    int testCol = (int)(testX / tileSize);
    if (!map.isWall(currRow, testCol))
    {
        x = testX;
    }

    // vertical move
    float testY = newY;
    int currCol = (int)(x / tileSize);
    int testRow = (int)(testY / tileSize);
    if (!map.isWall(testRow, currCol))
    {
        y = testY;
    }

    // update logical tile coordinates
    row = (int)(y / tileSize);
    col = (int)(x / tileSize);
}

// void Player::render(SDL_Renderer* renderer, int offsetX, int offsetY, int tileSize)
// {
//     SDL_Rect rect;
//     rect.w = tileSize;
//     rect.h = tileSize;
//     rect.x = offsetX + col * tileSize;
//     rect.y = offsetY + row * tileSize;

//     SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
//     SDL_RenderFillRect(renderer, &rect);
// }
void Player::render(SDL_Renderer* renderer, int offsetX, int offsetY)
{
    int px = offsetX + static_cast<int>(x);
    int py = offsetY + static_cast<int>(y);

    if (texture)
    {
        SDL_Rect dst = {px, py, 40, 40};
        SDL_RenderCopy(renderer, texture, nullptr, &dst);
    }
    else
    {
        SDL_Rect rect;
        rect.w = 40;   // kích thước nhân vật
        rect.h = 40;
        rect.x = px;
        rect.y = py;

        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        SDL_RenderFillRect(renderer, &rect);
    }
}