#include "PlayScene.h"
#include <iostream>

PlayScene::PlayScene()
{
}

void PlayScene::load(SDL_Renderer *renderer)
{
    std::cout << "PlayScene Loaded\n";

    map.load(renderer, "assets/images/background/map_level_1.png");

    player.load(renderer, "assets/images/entities/bot.png");   

    int tileSize = 32;
    player.setPosition(1, 1, tileSize);
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
    player.update(deltaTime, map);
}

void PlayScene::render(SDL_Renderer *renderer)
{
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
    player.clean();
}