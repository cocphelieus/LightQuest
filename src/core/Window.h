#pragma once
#include <SDL2/SDL.h>

class Window {
public:
    // Initialize SDL window + renderer and set logical render size.
    bool init(const char* title, int w, int h);
    // Clear current frame buffer.
    void clear();
    // Present current frame to screen.
    void present();
    // Release renderer/window resources.
    void clean();
    // Toggle between windowed and fullscreen while preserving logical coordinates.
    void toggleFullscreen();
    bool isFullscreenMode() const { return isFullscreen; }

    // Shared renderer used by all scenes.
    SDL_Renderer* getRenderer();

private:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    bool isFullscreen = false;
    // Logical resolution is fixed so scene input/render math stays stable across window sizes.
    int logicalWidth = 1280;
    int logicalHeight = 720;
};
