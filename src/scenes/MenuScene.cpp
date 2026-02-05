#include "MenuScene.h"

MenuScene::MenuScene() = default;

MenuScene::~MenuScene() {
    clean();
}

bool MenuScene::load(SDL_Renderer* renderer) {
    // Load background
    SDL_Surface* surface = SDL_LoadBMP("assets/images/background/main_menu_bg.bmp");
    
    if (!surface) {
        SDL_Log("Failed to load menu background: %s", SDL_GetError());
        return false;
    }
    
    backgroundTexture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    
    if (!backgroundTexture) {
        SDL_Log("Failed to create background texture: %s", SDL_GetError());
        return false;
    }
    
    // Initialize and load buttons
    initializeButtons();
    for (auto& button : buttons) {
        if (!button->load(renderer)) {
            SDL_Log("Failed to load a button");
            return false;
        }
    }
    
    return true;
}

void MenuScene::initializeButtons() {
    // Better layout - buttons positioned more aesthetically
    int centerX = (screenWidth - btnWidth) / 2;
    int startY = 200;
    int spacing = 110;
    
    // START button - main action button
    buttons.push_back(std::make_unique<Button>(centerX, startY, btnWidth, btnHeight, 
                                               "assets/images/button/btn_start.bmp"));
    
    // STORY button
    buttons.push_back(std::make_unique<Button>(centerX, startY + spacing, btnWidth, btnHeight,
                                               "assets/images/button/btn_story.bmp"));
    
    // GUIDE button
    buttons.push_back(std::make_unique<Button>(centerX, startY + spacing * 2, btnWidth, btnHeight,
                                               "assets/images/button/btn_guide.bmp"));
    
    // RANK button
    buttons.push_back(std::make_unique<Button>(centerX, startY + spacing * 3, btnWidth, btnHeight,
                                               "assets/images/button/btn_bxh.bmp"));
    
    // EXIT button - positioned at bottom right
    buttons.push_back(std::make_unique<Button>(screenWidth - btnWidth - 30, screenHeight - btnHeight - 30, 
                                               btnWidth, btnHeight,
                                               "assets/images/button/btn_exit.bmp"));
}

void MenuScene::handleEvent(SDL_Event& event) {
    if (event.type == SDL_MOUSEBUTTONDOWN) {
        if (event.button.button == SDL_BUTTON_LEFT) {
            int mouseX = event.button.x;
            int mouseY = event.button.y;
            
            for (size_t i = 0; i < buttons.size(); i++) {
                if (buttons[i]->isClicked(mouseX, mouseY)) {
                    handleButtonClick(i);
                    break;
                }
            }
        }
    }
}

void MenuScene::handleButtonClick(int buttonIndex) {
    switch (buttonIndex) {
        case 0: // START
            SDL_Log("START button clicked!");
            break;
        case 1: // STORY
            SDL_Log("STORY button clicked!");
            break;
        case 2: // GUIDE
            SDL_Log("GUIDE button clicked!");
            break;
        case 3: // RANK
            SDL_Log("RANK button clicked!");
            break;
        case 4: // EXIT
            SDL_Log("EXIT button clicked!");
            currentState = MenuState::EXIT;
            break;
    }
}

void MenuScene::update() {
    // Get current mouse position
    int mouseX = 0, mouseY = 0;
    SDL_GetMouseState(&mouseX, &mouseY);
    
    // Update button hover states
    for (auto& button : buttons) {
        button->update(mouseX, mouseY);
    }
}

void MenuScene::render(SDL_Renderer* renderer) {
    // Render background
    if (backgroundTexture) {
        SDL_RenderCopy(renderer, backgroundTexture, nullptr, nullptr);
    }
    
    // Render buttons
    for (auto& button : buttons) {
        button->render(renderer);
    }
}

void MenuScene::clean() {
    for (auto& button : buttons) {
        button->clean();
    }
    buttons.clear();
    
    if (backgroundTexture) {
        SDL_DestroyTexture(backgroundTexture);
        backgroundTexture = nullptr;
    }
}
