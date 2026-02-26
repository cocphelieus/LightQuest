#include "PlayScene.h"
#include <iostream>

PlayScene::PlayScene()
{
    testRect = {200, 150, 200, 100};
}

void PlayScene::load(SDL_Renderer *renderer)
{
    std::cout << "PlayScene Loaded\n";
    map.load(renderer, "assets/images/background/map_level_1.png");
    player.setPosition(1, 1);
}

void PlayScene::handleEvent(SDL_Event &event)
{
    if (event.type == SDL_KEYDOWN)
    {
        if (event.key.keysym.sym == SDLK_ESCAPE)
        {
            std::cout << "ESC pressed\n";
        }
    }
    player.handleEvent(event, map);
}

void PlayScene::update(float deltaTime)
{
    // sau này dùng deltaTime cho movement
}

void PlayScene::render(SDL_Renderer *renderer)
{
    SDL_SetRenderDrawColor(renderer, 0, 100, 255, 255);
    SDL_RenderFillRect(renderer, &testRect);
    map.render(renderer);
    int mapWidth = 20 * 32;
    int mapHeight = 20 * 32;

    int screenWidth = 1280;
    int screenHeight = 720;

    int offsetX = (screenWidth - mapWidth) / 2;
    int offsetY = (screenHeight - mapHeight) / 2;

    map.render(renderer);
    player.render(renderer, offsetX, offsetY);
}

void PlayScene::clean()
{
}
