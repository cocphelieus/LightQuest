#include "MenuScene.h"
#include <SDL2/SDL_image.h>

// Add file-scope cursor handles
static SDL_Cursor* gArrowCursor = nullptr;
static SDL_Cursor* gHandCursor = nullptr;

MenuScene::MenuScene() = default;

MenuScene::~MenuScene() {
    clean();
}

bool MenuScene::load(SDL_Renderer* renderer) {

    // =========================
    // Load background (PNG)
    // =========================
    backgroundTexture = IMG_LoadTexture(renderer, "assets/images/background/menu.png");

    if (!backgroundTexture) {
        SDL_Log("Failed to load menu background: %s", IMG_GetError());
        return false;
    }

    // =========================
    // Initialize buttons
    // =========================
    initializeButtons();
    for (auto& button : buttons) {
        if (!button->load(renderer)) {
            SDL_Log("Failed to load a button");
            return false;
        }
    }

    // =========================
    // Load overlay textures (PNG)
    // =========================
    storyTexture  = IMG_LoadTexture(renderer, "assets/images/background/story.png");
    storyTexture2 = IMG_LoadTexture(renderer, "assets/images/background/story2.png");

    guideTexture  = IMG_LoadTexture(renderer, "assets/images/background/guide.png");
    guideTexture2 = IMG_LoadTexture(renderer, "assets/images/background/guide2.png");

    rankTexture = IMG_LoadTexture(renderer, "assets/images/background/rank.png");
    quitTexture = IMG_LoadTexture(renderer, "assets/images/background/quit.png");

    // Optional logs
    if (!storyTexture) SDL_Log("Story texture missing: %s", IMG_GetError());
    if (!guideTexture) SDL_Log("Guide texture missing: %s", IMG_GetError());
    if (!rankTexture)  SDL_Log("Rank texture missing: %s", IMG_GetError());
    if (!quitTexture)  SDL_Log("Quit texture missing: %s", IMG_GetError());

    // =========================
    // Overlay close button
    // =========================
    overlayCloseButton = std::make_unique<Button>(
        screenWidth - btnWidth - 30,
        screenHeight - btnHeight - 30,
        btnWidth,
        btnHeight,
        "assets/images/button/btn_exit.png"
    );

    if (!overlayCloseButton->load(renderer)) {
        SDL_Log("Failed to load overlay close button");
        overlayCloseButton.reset();
    }

    // =========================
    // Yes / No buttons
    // =========================
    const int qBtnW = 160;
    const int qBtnH = 64;
    const int qSpacing = 40;

    int qCenterX = screenWidth / 2;
    int qY = screenHeight - 180;

    overlayYesButton = std::make_unique<Button>(
        qCenterX - qBtnW - qSpacing / 2,
        qY,
        qBtnW,
        qBtnH,
        "assets/images/button/btn_co.png"
    );

    if (!overlayYesButton->load(renderer)) {
        SDL_Log("Failed to load yes button");
        overlayYesButton.reset();
    }

    overlayNoButton = std::make_unique<Button>(
        qCenterX + qSpacing / 2,
        qY,
        qBtnW,
        qBtnH,
        "assets/images/button/btn_khong.png"
    );

    if (!overlayNoButton->load(renderer)) {
        SDL_Log("Failed to load no button");
        overlayNoButton.reset();
    }

    // =========================
    // Cursors
    // =========================
    if (!gArrowCursor)
        gArrowCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);

    if (!gHandCursor)
        gHandCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);

    return true;
}


void MenuScene::initializeButtons() {

    int centerX = (screenWidth - btnWidth) / 2;
    int startY  = 200;
    int spacing = 110;

    buttons.push_back(std::make_unique<Button>(
        centerX, startY, btnWidth, btnHeight,
        "assets/images/button/btn_start.png"
    ));

    buttons.push_back(std::make_unique<Button>(
        centerX, startY + spacing, btnWidth, btnHeight,
        "assets/images/button/btn_story.png"
    ));

    buttons.push_back(std::make_unique<Button>(
        centerX, startY + spacing * 2, btnWidth, btnHeight,
        "assets/images/button/btn_guide.png"
    ));

    buttons.push_back(std::make_unique<Button>(
        centerX, startY + spacing * 3, btnWidth, btnHeight,
        "assets/images/button/btn_bxh.png"
    ));

    buttons.push_back(std::make_unique<Button>(
        screenWidth - btnWidth - 30,
        screenHeight - btnHeight - 30,
        btnWidth, btnHeight,
        "assets/images/button/btn_exit.png"
    ));
}


void MenuScene::handleEvent(SDL_Event& event) {
    // If an overlay is visible, special handling per overlay type
    if (overlayVisible) {
        // mouse clicks: check close button first
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
            int mx = event.button.x;
            int my = event.button.y;
            // If quit overlay, check yes/no first
            if (currentOverlayType == OverlayType::EXIT) {
                if (overlayYesButton && overlayYesButton->isClicked(mx, my)) {
                    currentState = MenuState::EXIT;
                    overlayVisible = false;
                    currentOverlayType = OverlayType::NONE;
                    currentOverlayPage = 0;
                } else if (overlayNoButton && overlayNoButton->isClicked(mx, my)) {
                    overlayVisible = false;
                    currentOverlayType = OverlayType::NONE;
                    currentOverlayPage = 0;
                }
                return;
            }

            if (overlayCloseButton && overlayCloseButton->isClicked(mx, my)) {
                overlayVisible = false;
                currentOverlayType = OverlayType::NONE;
                currentOverlayPage = 0;
            }
            return;
        }

        // keyboard: ESC closes, left/right change page (if available)
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                overlayVisible = false;
                currentOverlayType = OverlayType::NONE;
                currentOverlayPage = 0;
                return;
            }
            if (event.key.keysym.sym == SDLK_RIGHT) {
                // next page
                if ((currentOverlayType == OverlayType::STORY && storyTexture2) ||
                    (currentOverlayType == OverlayType::GUIDE && guideTexture2)) {
                    currentOverlayPage = 1;
                }
                return;
            }
            if (event.key.keysym.sym == SDLK_LEFT) {
                currentOverlayPage = 0;
                return;
            }
        }

        return;
    }

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
            currentState = MenuState::PLAY;
            break;
        case 1: // STORY
            SDL_Log("STORY button clicked!");
            if (storyTexture) { currentOverlayType = OverlayType::STORY; currentOverlayPage = 0; overlayVisible = true; }
            break;
        case 2: // GUIDE
            SDL_Log("GUIDE button clicked!");
            if (guideTexture) { currentOverlayType = OverlayType::GUIDE; currentOverlayPage = 0; overlayVisible = true; }
            break;
        case 3: // RANK
            SDL_Log("RANK button clicked!");
            if (rankTexture) { currentOverlayType = OverlayType::RANK; currentOverlayPage = 0; overlayVisible = true; }
            break;
        case 4: // EXIT
            SDL_Log("EXIT button clicked!");
            // show quit confirmation overlay instead of exiting immediately
            if (quitTexture) { currentOverlayType = OverlayType::EXIT; overlayVisible = true; }
            else { currentState = MenuState::EXIT; }
            break;
    }
}

void MenuScene::update() {
    // Get current mouse position
    int mouseX = 0, mouseY = 0;
    SDL_GetMouseState(&mouseX, &mouseY);
    
    // Update button hover states

    // If overlay is visible, update overlay controls and cursor, skip main buttons
    if (overlayVisible) {
        if (currentOverlayType == OverlayType::EXIT) {
            if (overlayYesButton) overlayYesButton->update(mouseX, mouseY);
            if (overlayNoButton) overlayNoButton->update(mouseX, mouseY);

            if ((overlayYesButton && overlayYesButton->isClicked(mouseX, mouseY)) ||
                (overlayNoButton && overlayNoButton->isClicked(mouseX, mouseY))) {
                if (gHandCursor) SDL_SetCursor(gHandCursor);
            } else if (gArrowCursor) {
                SDL_SetCursor(gArrowCursor);
            }
            return;
        }

        if (overlayCloseButton) overlayCloseButton->update(mouseX, mouseY);
        if (overlayCloseButton && overlayCloseButton->isClicked(mouseX, mouseY)) {
            if (gHandCursor) SDL_SetCursor(gHandCursor);
        } else if (gArrowCursor) {
            SDL_SetCursor(gArrowCursor);
        }
        return;
    }

    // normal button updates
    for (auto& button : buttons) {
        button->update(mouseX, mouseY);
    }

    // Determine if cursor should be hand (hovering any button)
    bool hovering = false;
    for (auto& button : buttons) {
        if (button->isClicked(mouseX, mouseY)) {
            hovering = true;
            break;
        }
    }

    // Set cursor accordingly
    if (hovering && gHandCursor) {
        SDL_SetCursor(gHandCursor);
    } else if (!hovering && gArrowCursor) {
        SDL_SetCursor(gArrowCursor);
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

    // Render overlay on top if active (stretched to full screen)
    if (overlayVisible) {
        SDL_Texture* tex = nullptr;
        if (currentOverlayType == OverlayType::STORY) {
            tex = (currentOverlayPage == 0) ? storyTexture : storyTexture2 ? storyTexture2 : storyTexture;
        } else if (currentOverlayType == OverlayType::GUIDE) {
            tex = (currentOverlayPage == 0) ? guideTexture : guideTexture2 ? guideTexture2 : guideTexture;
        } else if (currentOverlayType == OverlayType::RANK) {
            tex = rankTexture;
        } else if (currentOverlayType == OverlayType::EXIT) {
            tex = quitTexture;
        }
        if (tex) SDL_RenderCopy(renderer, tex, nullptr, nullptr);

        // render overlay controls depending on overlay type
        if (currentOverlayType == OverlayType::EXIT) {
            if (overlayYesButton) overlayYesButton->render(renderer);
            if (overlayNoButton) overlayNoButton->render(renderer);
        } else {
            if (overlayCloseButton) overlayCloseButton->render(renderer);
        }
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

    if (storyTexture) { SDL_DestroyTexture(storyTexture); storyTexture = nullptr; }
    if (storyTexture2) { SDL_DestroyTexture(storyTexture2); storyTexture2 = nullptr; }
    if (guideTexture) { SDL_DestroyTexture(guideTexture); guideTexture = nullptr; }
    if (guideTexture2) { SDL_DestroyTexture(guideTexture2); guideTexture2 = nullptr; }
    if (rankTexture) { SDL_DestroyTexture(rankTexture); rankTexture = nullptr; }
    if (quitTexture) { SDL_DestroyTexture(quitTexture); quitTexture = nullptr; }

    overlayCloseButton.reset();
    overlayYesButton.reset();
    overlayNoButton.reset();

    // Free system cursors created by this scene
    if (gArrowCursor) {
        SDL_FreeCursor(gArrowCursor);
        gArrowCursor = nullptr;
    }
    if (gHandCursor) {
        SDL_FreeCursor(gHandCursor);
        gHandCursor = nullptr;
    }
}
