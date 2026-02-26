#include "Window.h"

bool Window::init(const std::string& title, int width, int height, bool startFullscreen)
{
    fullscreen = startFullscreen;

    Uint32 flags = SDL_WINDOW_SHOWN;
    if (fullscreen)
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

    window = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        flags
    );

    if (!window) return false;

    renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!renderer) return false;

    return true;
}

void Window::toggleFullscreen()
{
    fullscreen = !fullscreen;

    SDL_SetWindowFullscreen(
        window,
        fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0
    );
}

bool Window::isFullscreen() const
{
    return fullscreen;
}

void Window::clear()
{
    SDL_RenderClear(renderer);
}

void Window::present()
{
    SDL_RenderPresent(renderer);
}

SDL_Renderer* Window::getRenderer() const
{
    return renderer;
}

void Window::clean()
{
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
}