#include "Window.h"
#include <SDL2/SDL_image.h>

bool Window::init(const char* title, int w, int h) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        return false;

    logicalWidth = w;
    logicalHeight = h;

    window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        w, h,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (window)
    {
        SDL_Surface* iconSurface = IMG_Load("assets/images/entities/logo.avif");
        if (!iconSurface)
            iconSurface = IMG_Load("assets/images/entities/logo.png");
        if (!iconSurface)
            iconSurface = IMG_Load("assets/images/entities/bot.png");

        if (iconSurface)
        {
            SDL_SetWindowIcon(window, iconSurface);
            SDL_FreeSurface(iconSurface);
        }
    }

    renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!window || !renderer)
        return false;

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_RenderSetLogicalSize(renderer, logicalWidth, logicalHeight);
    SDL_RenderSetIntegerScale(renderer, SDL_FALSE);

    return true;
}

void Window::clear() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
}

void Window::present() {
    SDL_RenderPresent(renderer);
}

SDL_Renderer* Window::getRenderer() {
    return renderer;
}

void Window::toggleFullscreen()
{
    if (!window)
        return;

    if (!isFullscreen)
    {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        isFullscreen = true;
    }
    else
    {
        SDL_SetWindowFullscreen(window, 0);
        SDL_SetWindowSize(window, logicalWidth, logicalHeight);
        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        isFullscreen = false;
    }
}

void Window::clean() {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
