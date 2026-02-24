#include "PlayScene.h"
#include <iostream>

PlayScene::PlayScene()
{
    testRect = { 200, 150, 200, 100 };
}

void PlayScene::load(SDL_Renderer* renderer)
{
    std::cout << "PlayScene Loaded\n";
}

void PlayScene::handleEvent(SDL_Event& event)
{
    if (event.type == SDL_KEYDOWN)
    {
        if (event.key.keysym.sym == SDLK_ESCAPE)
        {
            std::cout << "ESC pressed\n";
        }
    }
}

void PlayScene::update(float deltaTime)
{
    // sau này dùng deltaTime cho movement
}

void PlayScene::render(SDL_Renderer* renderer)
{
    SDL_SetRenderDrawColor(renderer, 0, 100, 255, 255);
    SDL_RenderFillRect(renderer, &testRect);
}

void PlayScene::clean()
{
}
