#pragma once
#include <SDL2/SDL.h>

class PlayScene
{
public:
    PlayScene();

    void load(SDL_Renderer* renderer);
    void handleEvent(SDL_Event& event);
    void update(float deltaTime);
    void render(SDL_Renderer* renderer);
    void clean();

private:
    SDL_Rect testRect;
};
