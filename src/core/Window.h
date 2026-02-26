#pragma once
#include <SDL2/SDL.h>
#include <string>

class Window
{
public:
    bool init(const std::string& title, int width, int height, bool startFullscreen);
    void clean();

    void clear();
    void present();

    void toggleFullscreen();
    bool isFullscreen() const;

    SDL_Renderer* getRenderer() const;

private:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    bool fullscreen = false;
};