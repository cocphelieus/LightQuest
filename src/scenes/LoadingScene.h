#pragma once
#include <SDL2/SDL.h>
#include <string>
#include <cmath>

class LoadingScene {
public:
    LoadingScene();
    ~LoadingScene();
    
    bool load(SDL_Renderer* renderer);
    void update(float deltaTime);
    void render(SDL_Renderer* renderer);
    void clean();
    
    bool isComplete() const { return progress >= 100.0f; }
    float getProgress() const { return progress; }

private:
    SDL_Texture* backgroundTexture = nullptr;
    SDL_Renderer* renderer = nullptr;
    float progress = 0.0f;
    float loadingSpeed = 40.0f;
    float elapsedTime = 0.0f;
    
    int screenWidth = 1280;
    int screenHeight = 720;
    
    int barWidth = 400;
    int barHeight = 30;
    
    void renderProgressBar(SDL_Renderer* renderer);
    void drawFilledCircle(SDL_Renderer* renderer, int centerX, int centerY, int radius, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
};

