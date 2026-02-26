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
            row = newRow;
            col = newCol;
        }
    }
}

void Player::render(SDL_Renderer* renderer, int offsetX, int offsetY)
{
    SDL_Rect rect;
    rect.w = TILE_SIZE;
    rect.h = TILE_SIZE;
    rect.x = offsetX + col * TILE_SIZE;
    rect.y = offsetY + row * TILE_SIZE;

    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_RenderFillRect(renderer, &rect);
}