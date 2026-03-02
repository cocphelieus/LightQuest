#pragma once
#include <SDL2/SDL.h>

class HUD {
public:
    bool init(SDL_Renderer* renderer);
    void render(
        SDL_Renderer* renderer,
        int health,
        int maxHealth,
        int light,
        int maxLight,
        int score,
        int torchesActivated,
        int requiredTorches,
        int combo,
        int waveProgress,
        int waveThreshold,
        float elapsedSec
    );
    void clean();

private:
    void* fontRaw = nullptr; // opaque pointer to avoid needing SDL_ttf header in header
    SDL_Texture* textTexture = nullptr;
    int texW = 0, texH = 0;
    bool ttfInitByHUD = false;
};