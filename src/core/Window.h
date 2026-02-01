#pragma once
#include <SDL2/SDL.h>

class Window {
public:
    bool init(const char* title, int w, int h);
    void clear();
    void present();
    void clean();

    SDL_Renderer* getRenderer();

private:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
};
