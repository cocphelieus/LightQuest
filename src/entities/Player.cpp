#include "Player.h"
#include "../map/MapManager.h"
#include <SDL2/SDL_image.h>
#include <iostream>
#include <cmath>

Player::Player()
{
    row = col = 1;
    targetRow = targetCol = 1;
    posX = posY = 0;
    isMoving = false;
    moveSpeed = 200.0f; // pixel mỗi giây
}

void Player::setPosition(int r, int c, int tSize)
{
    row = targetRow = r;
    col = targetCol = c;

    tileSize = tSize;

    posX = col * tileSize;
    posY = row * tileSize;
}

bool Player::load(SDL_Renderer* renderer, const char* path)
{
    texture = IMG_LoadTexture(renderer, path);

    if (!texture)
    {
        std::cout << "Failed to load player texture: " << IMG_GetError() << "\n";
        return false;
    }

    return true;
}

void Player::handleEvent(SDL_Event& event, const MapManager& map)
{
    if (event.type == SDL_KEYDOWN && !isMoving && alive)
    {
        int newRow = row;
        int newCol = col;

        switch (event.key.keysym.sym)
        {
            case SDLK_w: newRow--; break;
            case SDLK_s: newRow++; break;
            case SDLK_a: newCol--; break;
            case SDLK_d: newCol++; break;
        }

        if (!map.isWall(newRow, newCol))
        {
            targetRow = newRow;
            targetCol = newCol;
            isMoving = true;
        }
    }
}

void Player::update(float deltaTime, MapManager& map)
{
    if (!alive) return;

    if (!isMoving)
    {
        // check current location for mines
        if (map.isMine(row, col))
        {
            alive = false;
            std::cout << "Stepped on a mine at (" << row << "," << col << ")!" << std::endl;
        }
        // reveal current tile so player can see where they've been
        map.reveal(row, col);
        return;
    }

    float targetX = targetCol * tileSize;
    float targetY = targetRow * tileSize;

    float dx = targetX - posX;
    float dy = targetY - posY;

    float distance = sqrt(dx*dx + dy*dy);

    if (distance < 1.0f)
    {
        posX = targetX;
        posY = targetY;

        row = targetRow;
        col = targetCol;

        isMoving = false;

        // after arriving, run the same checks as above
        if (map.isMine(row, col))
        {
            alive = false;
            std::cout << "Stepped on a mine at (" << row << "," << col << ")!" << std::endl;
            return;
        }
        // reveal the tile we've just stepped onto
        map.reveal(row, col);
    }
    else
    {
        posX += (dx / distance) * moveSpeed * deltaTime;
        posY += (dy / distance) * moveSpeed * deltaTime;
    }
}

void Player::render(SDL_Renderer* renderer, int offsetX, int offsetY)
{
    if (!texture) return;

    SDL_Rect dstRect;
    dstRect.w = tileSize;
    dstRect.h = tileSize;
    dstRect.x = offsetX + (int)posX;
    dstRect.y = offsetY + (int)posY;

    SDL_RenderCopy(renderer, texture, nullptr, &dstRect);
}

void Player::clean()
{
    if (texture)
    {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
}