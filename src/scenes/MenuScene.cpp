#include "MenuScene.h"

bool MenuScene::load(SDL_Renderer* renderer) {
    SDL_Surface* surface = SDL_LoadBMP("assets/images/menu.bmp");

    if (!surface) {
        SDL_Log("Load BMP failed: %s", SDL_GetError());
        return false;
    }

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    if (!texture) {
        SDL_Log("Create texture failed: %s", SDL_GetError());
        return false;
    }

    return true;
}

void MenuScene::render(SDL_Renderer* renderer) {
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
}

void MenuScene::clean() {
    SDL_DestroyTexture(texture);
}
