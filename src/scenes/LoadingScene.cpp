#include "LoadingScene.h"
#include <cmath>

LoadingScene::LoadingScene() = default;

LoadingScene::~LoadingScene() {
    clean();
}

bool LoadingScene::load(SDL_Renderer* renderer) {
    this->renderer = renderer;
    
    SDL_Surface* surface = SDL_LoadBMP("assets/images/background/main_menu_bg.bmp");
    
    if (!surface) {
        SDL_Log("Warning: Could not load loading screen background: %s", SDL_GetError());
    } else {
        backgroundTexture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        
        if (!backgroundTexture) {
            SDL_Log("Warning: Could not create texture: %s", SDL_GetError());
        }
    }
    
    progress = 0.0f;
    elapsedTime = 0.0f;
    return true;
}

void LoadingScene::update(float deltaTime) {
    elapsedTime += deltaTime;
    
    if (progress < 100.0f) {
        progress += loadingSpeed * deltaTime;
        
        if (progress > 100.0f) {
            progress = 100.0f;
        }
    }
}

void LoadingScene::render(SDL_Renderer* renderer) {
    // Render background
    if (backgroundTexture) {
        SDL_RenderCopy(renderer, backgroundTexture, nullptr, nullptr);
    }
    
    // Render progress bar
    renderProgressBar(renderer);
}

void LoadingScene::renderProgressBar(SDL_Renderer* renderer) {
    int centerX = screenWidth / 2;
    int centerY = screenHeight - 100;
    
    // Simple loading dots
    int numDots = 8;
    int dotRadius = 25;
    
    for (int i = 0; i < numDots; i++) {
        float angle = (2.0f * 3.14159f * i / numDots) + (elapsedTime * 3.0f);
        int x = static_cast<int>(centerX + dotRadius * cos(angle));
        int y = static_cast<int>(centerY - 50 + dotRadius * sin(angle));
        
        drawFilledCircle(renderer, x, y, 3, 200, 200, 200, 150);
    }
    
    // Progress bar - simple and clean
    SDL_Rect bgBar;
    bgBar.x = centerX - barWidth / 2;
    bgBar.y = centerY + 5;
    bgBar.w = barWidth;
    bgBar.h = barHeight;
    
    // Dark background
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 180);
    SDL_RenderFillRect(renderer, &bgBar);
    
    // Progress fill
    SDL_Rect progressBar;
    progressBar.x = bgBar.x + 1;
    progressBar.y = bgBar.y + 1;
    progressBar.w = static_cast<int>(((barWidth - 2) * progress) / 100.0f);
    progressBar.h = barHeight - 2;
    
    SDL_SetRenderDrawColor(renderer, 100, 200, 100, 200);
    SDL_RenderFillRect(renderer, &progressBar);
    
    // Simple border
    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 150);
    SDL_RenderDrawRect(renderer, &bgBar);
}

void LoadingScene::drawFilledCircle(SDL_Renderer* renderer, int centerX, int centerY, int radius, Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    
    for (int w = 0; w < radius * 2; w++) {
        for (int h = 0; h < radius * 2; h++) {
            int dx = radius - w;
            int dy = radius - h;
            if ((dx * dx + dy * dy) <= (radius * radius)) {
                SDL_RenderDrawPoint(renderer, centerX + dx, centerY + dy);
            }
        }
    }
}

void LoadingScene::clean() {
    if (backgroundTexture) {
        SDL_DestroyTexture(backgroundTexture);
        backgroundTexture = nullptr;
    }
}
