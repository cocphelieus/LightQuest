#include "MenuScene.h"
#include "../core/SoundManager.h"
#include <SDL2/SDL_image.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdio>

#if __has_include(<SDL2/SDL_ttf.h>)
#include <SDL2/SDL_ttf.h>
#define LIGHTQUEST_HAS_SDL_TTF 1
#elif __has_include(<SDL_ttf.h>)
#include <SDL_ttf.h>
#define LIGHTQUEST_HAS_SDL_TTF 1
#else
#define LIGHTQUEST_HAS_SDL_TTF 0
#endif

// Add file-scope cursor handles
static SDL_Cursor* gArrowCursor = nullptr;
static SDL_Cursor* gHandCursor = nullptr;
static const char* kRankingFilePath = "data/ranking.txt";

MenuScene::MenuScene() = default;

MenuScene::~MenuScene() {
    clean();
}

bool MenuScene::load(SDL_Renderer* renderer) {
    rendererRef = renderer;

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
    quitPanelTexture = IMG_LoadTexture(renderer, "assets/images/button/khung_cau_hoi.png");

    // Optional logs
    if (!storyTexture) SDL_Log("Story texture missing: %s", IMG_GetError());
    if (!guideTexture) SDL_Log("Guide texture missing: %s", IMG_GetError());
    if (!rankTexture)  SDL_Log("Rank texture missing: %s", IMG_GetError());
    if (!quitTexture)  SDL_Log("Quit texture missing: %s", IMG_GetError());

#if LIGHTQUEST_HAS_SDL_TTF
    if (TTF_WasInit() == 0)
    {
        if (TTF_Init() == 0)
            ttfInitByScene = true;
    }

    if (TTF_WasInit() != 0)
    {
        quitTitleFontRaw = TTF_OpenFont("assets/fonts/Times New Roman.ttf", 54);
        quitBodyFontRaw = TTF_OpenFont("assets/fonts/Times New Roman.ttf", 28);
        if (quitTitleFontRaw)
            TTF_SetFontStyle(static_cast<TTF_Font*>(quitTitleFontRaw), TTF_STYLE_BOLD);
    }
#endif

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
    int qY = (screenHeight / 2) + 120;

    exitYesWidth = qBtnW;
    exitYesHeight = qBtnH;
    exitYesBaseX = qCenterX - qBtnW - qSpacing / 2;
    exitYesBaseY = qY;

    overlayYesButton = std::make_unique<Button>(
        exitYesBaseX,
        exitYesBaseY,
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

    refreshRankings();
    resetExitConfirmTroll();

    return true;
}

void MenuScene::resetExitConfirmTroll()
{
    // Re-seed required click count each time exit popup opens (6 to 8).
    exitConfirmClickCount = 0;
    exitConfirmClicksRequired = 6 + static_cast<int>(SDL_GetTicks() % 3U);
    exitNextEvadeTick = 0;
    exitYesCurrentX = exitYesBaseX;
    exitYesCurrentY = exitYesBaseY;
    if (overlayYesButton)
        overlayYesButton->setPosition(exitYesBaseX, exitYesBaseY);
}

void MenuScene::shuffleExitYesButton()
{
    if (!overlayYesButton)
        return;

    // Keep movement inside the popup action area so the button remains visible
    // but still annoying to click repeatedly.
    const int minX = (screenWidth / 2) - 360;
    const int maxX = (screenWidth / 2) - exitYesWidth - 50;
    const int minY = (screenHeight / 2) + 78;
    const int maxY = (screenHeight / 2) + 178;

    const int spanX = (maxX > minX) ? (maxX - minX + 1) : 1;
    const int spanY = (maxY > minY) ? (maxY - minY + 1) : 1;

    Uint32 t = SDL_GetTicks();
    int newX = minX + static_cast<int>(((t * 37U) + 17U) % static_cast<Uint32>(spanX));
    int newY = minY + static_cast<int>(((t * 53U) + 29U) % static_cast<Uint32>(spanY));
    exitYesCurrentX = newX;
    exitYesCurrentY = newY;
    overlayYesButton->setPosition(newX, newY);
}

void MenuScene::refreshRankings()
{
    localRankings.clear();

    std::ifstream in(kRankingFilePath);
    if (!in.is_open())
        return;

    std::string line;
    while (std::getline(in, line))
    {
        if (line.empty())
            continue;

        std::stringstream ss(line);
        std::string secondsText;
        std::string timestamp;
        if (!std::getline(ss, secondsText, '|'))
            continue;
        if (!std::getline(ss, timestamp))
            timestamp.clear();

        int seconds = 0;
        try {
            seconds = std::stoi(secondsText);
        } catch (...) {
            continue;
        }

        if (seconds < 0)
            continue;

        RankingEntry entry;
        entry.seconds = seconds;
        entry.timestamp = timestamp;
        localRankings.push_back(entry);
    }

    std::sort(localRankings.begin(), localRankings.end(), [](const RankingEntry& a, const RankingEntry& b) {
        return a.seconds < b.seconds;
    });

    if (localRankings.size() > 5)
        localRankings.resize(5);
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
        centerX,
        startY + spacing * 4,
        btnWidth, btnHeight,
        "assets/images/button/btn_exit.png"
    ));
}


void MenuScene::handleEvent(SDL_Event& event) {
    // If an overlay is visible, special handling per overlay type
    if (overlayVisible) {
        // mouse clicks: check close button first
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
            int mx = 0;
            int my = 0;
            SDL_GetMouseState(&mx, &my);
                mapMouseToLogical(mx, my, mx, my);
            // If quit overlay, check yes/no first
            if (currentOverlayType == OverlayType::EXIT) {
                if (overlayYesButton && overlayYesButton->isClicked(mx, my)) {
                    SoundManager::instance().playClick();
                    exitConfirmClickCount++;
                    if (exitConfirmClickCount >= exitConfirmClicksRequired) {
                        // Exit succeeds only after the required number of yes clicks.
                        currentState = MenuState::EXIT;
                        overlayVisible = false;
                        currentOverlayType = OverlayType::NONE;
                        currentOverlayPage = 0;
                        resetExitConfirmTroll();
                    } else {
                        // Not enough clicks yet: move yes button to new position.
                        shuffleExitYesButton();
                    }
                } else if (overlayNoButton && overlayNoButton->isClicked(mx, my)) {
                    SoundManager::instance().playClick();
                    overlayVisible = false;
                    currentOverlayType = OverlayType::NONE;
                    currentOverlayPage = 0;
                    resetExitConfirmTroll();
                }
                return;
            }

            if (overlayCloseButton && overlayCloseButton->isClicked(mx, my)) {
                SoundManager::instance().playClick();
                overlayVisible = false;
                currentOverlayType = OverlayType::NONE;
                currentOverlayPage = 0;
                resetExitConfirmTroll();
            }
            return;
        }

        // keyboard: ESC closes, left/right change page (if available)
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                overlayVisible = false;
                currentOverlayType = OverlayType::NONE;
                currentOverlayPage = 0;
                resetExitConfirmTroll();
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

    if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) {
            SoundManager::instance().playClick();
            currentState = MenuState::PLAY;
            return;
        }

        if (event.key.keysym.sym == SDLK_ESCAPE) {
            SoundManager::instance().playClick();
            if (quitTexture) {
                currentOverlayType = OverlayType::EXIT;
                currentOverlayPage = 0;
                overlayVisible = true;
                resetExitConfirmTroll();
            } else {
                currentState = MenuState::EXIT;
            }
            return;
        }
    }

    if (event.type == SDL_MOUSEBUTTONDOWN) {
        if (event.button.button == SDL_BUTTON_LEFT) {
            int mouseX = 0;
            int mouseY = 0;
            SDL_GetMouseState(&mouseX, &mouseY);
            mapMouseToLogical(mouseX, mouseY, mouseX, mouseY);
            
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
    SoundManager::instance().playClick();

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
            if (quitTexture) {
                currentOverlayType = OverlayType::EXIT;
                overlayVisible = true;
                resetExitConfirmTroll();
            }
            else { currentState = MenuState::EXIT; }
            break;
    }
}

void MenuScene::update() {
    // Get current mouse position
    int mouseX = 0, mouseY = 0;
    SDL_GetMouseState(&mouseX, &mouseY);
    mapMouseToLogical(mouseX, mouseY, mouseX, mouseY);
    
    // Update button hover states

    // If overlay is visible, update overlay controls and cursor, skip main buttons
    if (overlayVisible) {
        if (currentOverlayType == OverlayType::EXIT) {
            if (overlayYesButton) {
                // Extra troll: if cursor comes too close, yes button dodges periodically.
                int yesCenterX = exitYesCurrentX + (exitYesWidth / 2);
                int yesCenterY = exitYesCurrentY + (exitYesHeight / 2);
                int dx = mouseX - yesCenterX;
                int dy = mouseY - yesCenterY;
                int distanceSq = dx * dx + dy * dy;
                int radiusSq = exitEvadeRadius * exitEvadeRadius;
                Uint32 now = SDL_GetTicks();
                if (distanceSq <= radiusSq && now >= exitNextEvadeTick && exitConfirmClickCount < exitConfirmClicksRequired) {
                    shuffleExitYesButton();
                    exitNextEvadeTick = now + 140;
                }
            }

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

void MenuScene::mapMouseToLogical(int inX, int inY, int& outX, int& outY) const
{
    outX = inX;
    outY = inY;

    if (!rendererRef)
        return;

#if SDL_VERSION_ATLEAST(2, 0, 18)
    float logicalX = static_cast<float>(inX);
    float logicalY = static_cast<float>(inY);
    SDL_RenderWindowToLogical(rendererRef, inX, inY, &logicalX, &logicalY);
    outX = static_cast<int>(logicalX);
    outY = static_cast<int>(logicalY);
    return;
#endif

    int logicalW = 0;
    int logicalH = 0;
    SDL_RenderGetLogicalSize(rendererRef, &logicalW, &logicalH);
    if (logicalW <= 0 || logicalH <= 0)
        return;

    SDL_Rect viewport;
    SDL_RenderGetViewport(rendererRef, &viewport);
    if (viewport.w <= 0 || viewport.h <= 0)
        return;

    int clampedX = inX;
    int clampedY = inY;
    if (clampedX < viewport.x)
        clampedX = viewport.x;
    if (clampedX > viewport.x + viewport.w)
        clampedX = viewport.x + viewport.w;
    if (clampedY < viewport.y)
        clampedY = viewport.y;
    if (clampedY > viewport.y + viewport.h)
        clampedY = viewport.y + viewport.h;

    outX = ((clampedX - viewport.x) * logicalW) / viewport.w;
    outY = ((clampedY - viewport.y) * logicalH) / viewport.h;
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
            tex = nullptr;
        }
        if (tex) SDL_RenderCopy(renderer, tex, nullptr, nullptr);

        // render overlay controls depending on overlay type
        if (currentOverlayType == OverlayType::EXIT) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 145);
            SDL_Rect dim = {0, 0, screenWidth, screenHeight};
            SDL_RenderFillRect(renderer, &dim);

            SDL_Rect modalRect = {screenWidth / 2 - 400, screenHeight / 2 - 200, 800, 300};

            if (quitPanelTexture)
            {
                // Crop transparent margins of khung_cau_hoi texture for a clean center panel.
                SDL_Rect src = {70, 67, 475, 235};
                SDL_RenderCopy(renderer, quitPanelTexture, &src, &modalRect);
            }
            else
            {
                SDL_SetRenderDrawColor(renderer, 20, 20, 24, 230);
                SDL_RenderFillRect(renderer, &modalRect);
                SDL_SetRenderDrawColor(renderer, 220, 178, 100, 220);
                SDL_RenderDrawRect(renderer, &modalRect);
            }

            SDL_Rect titleRect = {screenWidth / 2 - 460, screenHeight / 2 - 90, 920, 70};
            SDL_Rect bodyRect = {screenWidth / 2 - 420, screenHeight / 2 - 20, 840, 52};
            renderTextCentered(renderer, quitTitleFontRaw, "EXIT GAME", titleRect, SDL_Color{120, 72, 26, 255});
            renderTextCentered(renderer, quitBodyFontRaw, "Are you sure you want to leave right now?", bodyRect, SDL_Color{72, 46, 24, 255});

            if (overlayYesButton) overlayYesButton->render(renderer);
            if (overlayNoButton) overlayNoButton->render(renderer);
        } else if (currentOverlayType == OverlayType::RANK) {
            // Ranking overlay: intentionally minimalist (no outer panel border/background fill)
            // to let the menu background remain visible.
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 140);
            SDL_Rect dim = {0, 0, screenWidth, screenHeight};
            SDL_RenderFillRect(renderer, &dim);

            SDL_Rect board = {screenWidth / 2 - 380, screenHeight / 2 - 225, 760, 470};

            SDL_Rect headerBand = {board.x + 36, board.y + 76, board.w - 72, 38};
            SDL_SetRenderDrawColor(renderer, 22, 42, 62, 170);
            SDL_RenderFillRect(renderer, &headerBand);
            SDL_SetRenderDrawColor(renderer, 95, 145, 165, 200);
            SDL_RenderDrawRect(renderer, &headerBand);

            SDL_Rect rankHeaderRect = {board.x + 56, board.y + 80, 120, 30};
            SDL_Rect timeHeaderRect = {board.x + 196, board.y + 80, 180, 30};
            SDL_Rect dateHeaderRect = {board.x + 406, board.y + 80, 300, 30};
            renderTextCentered(renderer, quitBodyFontRaw, "RANK", rankHeaderRect, SDL_Color{165, 180, 195, 255});
            renderTextCentered(renderer, quitBodyFontRaw, "TIME", timeHeaderRect, SDL_Color{165, 180, 195, 255});
            renderTextCentered(renderer, quitBodyFontRaw, "DATE", dateHeaderRect, SDL_Color{165, 180, 195, 255});

            if (localRankings.empty())
            {
                SDL_Rect emptyRect = {board.x + 20, board.y + 210, board.w - 40, 40};
                renderTextCentered(renderer, quitBodyFontRaw, "No records yet. Clear all 5 stages to enter the ranking.", emptyRect, SDL_Color{220, 220, 220, 255});
            }
            else
            {
                int startY = board.y + 126;
                for (size_t i = 0; i < localRankings.size() && i < 5; i++)
                {
                    const RankingEntry& e = localRankings[i];
                    int minutes = e.seconds / 60;
                    int seconds = e.seconds % 60;

                    SDL_Rect rowRect = {board.x + 36, startY + static_cast<int>(i) * 56, board.w - 72, 44};
                    SDL_SetRenderDrawColor(renderer, (i % 2 == 0) ? 18 : 14, (i % 2 == 0) ? 30 : 24, (i % 2 == 0) ? 46 : 38, 165);
                    SDL_RenderFillRect(renderer, &rowRect);
                    SDL_SetRenderDrawColor(renderer, 72, 106, 132, 170);
                    SDL_RenderDrawRect(renderer, &rowRect);

                    char rankText[16];
                    std::snprintf(rankText, sizeof(rankText), "#%d", static_cast<int>(i + 1));
                    char timeText[32];
                    std::snprintf(timeText, sizeof(timeText), "%02d:%02d", minutes, seconds);

                    SDL_Rect rankRect = {board.x + 56, rowRect.y + 6, 120, 30};
                    SDL_Rect timeRect = {board.x + 196, rowRect.y + 6, 180, 30};
                    SDL_Rect dateRect = {board.x + 406, rowRect.y + 6, 300, 30};
                    SDL_Color lineColor = (i < 3)
                        ? SDL_Color{245, 224, 150, 255}
                        : SDL_Color{225, 230, 235, 255};

                    renderTextCentered(renderer, quitBodyFontRaw, rankText, rankRect, lineColor);
                    renderTextCentered(renderer, quitBodyFontRaw, timeText, timeRect, lineColor);
                    renderTextLeft(renderer, quitBodyFontRaw, e.timestamp.empty() ? "(no timestamp)" : e.timestamp.c_str(), dateRect, lineColor);
                }
            }

            if (overlayCloseButton) overlayCloseButton->render(renderer);
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
    if (quitPanelTexture) { SDL_DestroyTexture(quitPanelTexture); quitPanelTexture = nullptr; }

#if LIGHTQUEST_HAS_SDL_TTF
    if (quitTitleFontRaw) {
        TTF_CloseFont(static_cast<TTF_Font*>(quitTitleFontRaw));
        quitTitleFontRaw = nullptr;
    }
    if (quitBodyFontRaw) {
        TTF_CloseFont(static_cast<TTF_Font*>(quitBodyFontRaw));
        quitBodyFontRaw = nullptr;
    }
    if (ttfInitByScene) {
        TTF_Quit();
        ttfInitByScene = false;
    }
#else
    quitTitleFontRaw = nullptr;
    quitBodyFontRaw = nullptr;
    ttfInitByScene = false;
#endif

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

void MenuScene::resetToMain() {
    currentState = MenuState::MAIN_MENU;
    overlayVisible = false;
    currentOverlayType = OverlayType::NONE;
    currentOverlayPage = 0;
}

void MenuScene::renderTextCentered(SDL_Renderer* renderer, void* fontRaw, const char* text, const SDL_Rect& rect, SDL_Color color)
{
#if !LIGHTQUEST_HAS_SDL_TTF
    (void)renderer;
    (void)fontRaw;
    (void)text;
    (void)rect;
    (void)color;
    return;
#else
    if (!renderer || !fontRaw || !text)
        return;

    TTF_Font* font = static_cast<TTF_Font*>(fontRaw);
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface)
        return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture)
    {
        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect dst = {
        rect.x + (rect.w - surface->w) / 2,
        rect.y + (rect.h - surface->h) / 2,
        surface->w,
        surface->h
    };
    SDL_RenderCopy(renderer, texture, nullptr, &dst);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
#endif
}

void MenuScene::renderTextLeft(SDL_Renderer* renderer, void* fontRaw, const char* text, const SDL_Rect& rect, SDL_Color color)
{
#if !LIGHTQUEST_HAS_SDL_TTF
    (void)renderer;
    (void)fontRaw;
    (void)text;
    (void)rect;
    (void)color;
    return;
#else
    if (!renderer || !fontRaw || !text)
        return;

    TTF_Font* font = static_cast<TTF_Font*>(fontRaw);
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface)
        return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture)
    {
        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect dst = {
        rect.x,
        rect.y + (rect.h - surface->h) / 2,
        surface->w,
        surface->h
    };
    SDL_RenderSetClipRect(renderer, &rect);
    SDL_RenderCopy(renderer, texture, nullptr, &dst);
    SDL_RenderSetClipRect(renderer, nullptr);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
#endif
}
