#pragma once
#include <SDL2/SDL.h>

class MenuScene {
public:
    bool load(SDL_Renderer* renderer);
    void render(SDL_Renderer* renderer);
    void clean();

private:
    SDL_Texture* texture = nullptr;
};
