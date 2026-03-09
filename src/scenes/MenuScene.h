#pragma once
#include <SDL2/SDL.h>
#include "../core/Button.h"
#include <vector>
#include <memory>
#include <string>

enum class MenuState {
    MAIN_MENU,
    NONE,
    PLAY,
    EXIT
};

class MenuScene {
public:
    struct RankingEntry {
        int seconds = 0;
        std::string timestamp;
    };

    MenuScene();
    ~MenuScene();
    
    bool load(SDL_Renderer* renderer);
    void handleEvent(SDL_Event& event);
    void update();
    void render(SDL_Renderer* renderer);
    void clean();
    void resetToMain();
    void refreshRankings();
    
    MenuState getState() const { return currentState; }

private:
    void mapMouseToLogical(int inX, int inY, int& outX, int& outY) const;

    SDL_Renderer* rendererRef = nullptr;
    SDL_Texture* backgroundTexture = nullptr;
    std::vector<std::unique_ptr<Button>> buttons;
    MenuState currentState = MenuState::MAIN_MENU;
    
    int screenWidth = 1280;
    int screenHeight = 720;
    
    // Button dimensions (wider so button text has more horizontal space)
    int btnWidth = 220;
    int btnHeight = 60;
    // Overlay textures for STORY / GUIDE / RANK
    SDL_Texture* storyTexture = nullptr;
    SDL_Texture* guideTexture = nullptr;
    SDL_Texture* rankTexture = nullptr;

    // Optional second page textures (if provided)
    SDL_Texture* storyTexture2 = nullptr;
    SDL_Texture* guideTexture2 = nullptr;

    // Current overlay control
    enum class OverlayType { NONE, STORY, GUIDE, RANK, EXIT };
    OverlayType currentOverlayType = OverlayType::NONE;
    int currentOverlayPage = 0; // 0 = first page, 1 = second page (if any)
    bool overlayVisible = false;

    // Close/back button shown while overlay is visible
    std::unique_ptr<Button> overlayCloseButton;

    // Quit confirmation overlay
    SDL_Texture* quitTexture = nullptr;
    SDL_Texture* quitPanelTexture = nullptr;
    std::unique_ptr<Button> overlayYesButton;
    std::unique_ptr<Button> overlayNoButton;
    void* quitTitleFontRaw = nullptr;
    void* quitBodyFontRaw = nullptr;
    bool ttfInitByScene = false;
    
    void initializeButtons();
    void handleButtonClick(int buttonIndex);
    void renderTextCentered(SDL_Renderer* renderer, void* fontRaw, const char* text, const SDL_Rect& rect, SDL_Color color);
    void renderTextLeft(SDL_Renderer* renderer, void* fontRaw, const char* text, const SDL_Rect& rect, SDL_Color color);
    std::vector<RankingEntry> localRankings;
};
