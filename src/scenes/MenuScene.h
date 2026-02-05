#pragma once
#include <SDL2/SDL.h>
#include "../core/Button.h"
#include <vector>
#include <memory>

enum class MenuState {
    MAIN_MENU,
    EXIT
};

class MenuScene {
public:
    MenuScene();
    ~MenuScene();
    
    bool load(SDL_Renderer* renderer);
    void handleEvent(SDL_Event& event);
    void update();
    void render(SDL_Renderer* renderer);
    void clean();
    
    MenuState getState() const { return currentState; }

private:
    SDL_Texture* backgroundTexture = nullptr;
    std::vector<std::unique_ptr<Button>> buttons;
    MenuState currentState = MenuState::MAIN_MENU;
    
    int screenWidth = 1280;
    int screenHeight = 720;
    
    // Button dimensions
    int btnWidth = 150;
    int btnHeight = 60;
    
    void initializeButtons();
    void handleButtonClick(int buttonIndex);
};
