#include "Player.h"
#include "../map/MapManager.h"

Player::Player()
{
    row = 1;
    col = 1;
}

void Player::setPosition(int r, int c)
{
    row = r;
    col = c;
}

void Player::handleEvent(SDL_Event& event, const MapManager& map)
{
    if (event.type == SDL_KEYDOWN)
    {
        if (event.key.repeat != 0)
            return;

        int newRow = row;
        int newCol = col;

        switch (event.key.keysym.sym)
        {
            case SDLK_w:
            case SDLK_UP:
                newRow--;
                break;
            case SDLK_s:
            case SDLK_DOWN:
                newRow++;
                break;
            case SDLK_a:
            case SDLK_LEFT:
                newCol--;
                break;
            case SDLK_d:
            case SDLK_RIGHT:
                newCol++;
                break;
        }

        if (!map.isWall(newRow, newCol))
        {
            row = newRow;
            col = newCol;
        }
    }
}

void Player::render(SDL_Renderer* renderer, int offsetX, int offsetY, int tileSize)
{
    SDL_Rect rect;
    rect.w = tileSize;
    rect.h = tileSize;
    rect.x = offsetX + col * tileSize;
    rect.y = offsetY + row * tileSize;

    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_RenderFillRect(renderer, &rect);
}