#include "PlayScene.h"
#include "../config/TesterConfig.h"
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <SDL2/SDL_image.h>

#if __has_include(<SDL2/SDL_ttf.h>)
#include <SDL2/SDL_ttf.h>
#define LIGHTQUEST_HAS_SDL_TTF 1
#elif __has_include(<SDL_ttf.h>)
#include <SDL_ttf.h>
#define LIGHTQUEST_HAS_SDL_TTF 1
#else
#define LIGHTQUEST_HAS_SDL_TTF 0
#endif

namespace
{
    // Kiểm tra xem người dùng có đang giữ phím di chuyển không.
    // Dùng để tránh player cả động vô tình ngay khi stage vừa load xong.
    bool isMovementKeyHeld(const Uint8* keystate)
    {
        if (!keystate)
            return false;

        return keystate[SDL_SCANCODE_W] || keystate[SDL_SCANCODE_UP] ||
               keystate[SDL_SCANCODE_S] || keystate[SDL_SCANCODE_DOWN] ||
               keystate[SDL_SCANCODE_A] || keystate[SDL_SCANCODE_LEFT] ||
               keystate[SDL_SCANCODE_D] || keystate[SDL_SCANCODE_RIGHT];
    }

    // Trả về tên độ khó in hoa để hiển thị trên HUD.
    const char* difficultyLabel(Difficulty difficulty)
    {
        if (difficulty == Difficulty::EASY)
            return "EASY";
        if (difficulty == Difficulty::MEDIUM)
            return "MEDIUM";
        return "HARD";
    }

    // Trả về màu tương ứng với mỗi mức độ khó (xanh lá/vàng/đỏ).
    SDL_Color difficultyColor(Difficulty difficulty)
    {
        if (difficulty == Difficulty::EASY)
            return SDL_Color{110, 225, 140, 255};
        if (difficulty == Difficulty::MEDIUM)
            return SDL_Color{245, 200, 95, 255};
        return SDL_Color{235, 95, 95, 255};
    }

#if LIGHTQUEST_HAS_SDL_TTF
    void renderTextLine(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color color)
    {
        if (!renderer || !font || !text)
            return;

        SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
        if (!surface)
            return;

        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture)
        {
            SDL_Rect dst = {x, y, surface->w, surface->h};
            SDL_RenderCopy(renderer, texture, nullptr, &dst);
            SDL_DestroyTexture(texture);
        }

        SDL_FreeSurface(surface);
    }
#endif
}

// Đồng bộ tester overlay với map: ẩn/hiện mìn và torch theo cờ tester.
// Nếu không phải build tester, luôn tắt overlay.
void PlayScene::syncTesterOverlay()
{
    if (!kTesterEnabledBuild)
    {
        map.setTesterOverlay(false, false);
        return;
    }

    map.setTesterOverlay(testerShowMines, testerShowTorches);
}

// Mở panel debug (chỉ build tester): toggle hiện mìn/torch/đáp án, bỏ qua stage.
void PlayScene::openTesterPanel()
{
    if (!kTesterEnabledBuild)
        return;

    bool keepOpen = true;

    while (keepOpen)
    {
        char statusText[512];
        std::snprintf(
            statusText,
            sizeof(statusText),
            "TESTER PANEL\n\n"
            "Current status:\n"
            "- Show mines: %s\n"
            "- Show torches: %s\n"
            "- Show question answer: %s\n\n"
            "Select a button to toggle options or skip stage.",
            testerShowMines ? "ON" : "OFF",
            testerShowTorches ? "ON" : "OFF",
            testerShowAnswer ? "ON" : "OFF"
        );

        SDL_MessageBoxButtonData buttons[5] = {
            {0, 0, "Toggle mines"},
            {0, 1, "Toggle torches"},
            {0, 2, "Toggle answer"},
            {0, 3, "Next stage (goal)"},
            {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 4, "Close"}
        };

        SDL_MessageBoxData boxdata;
        boxdata.flags = SDL_MESSAGEBOX_INFORMATION;
        boxdata.window = nullptr;
        boxdata.title = "Tester";
        boxdata.message = statusText;
        boxdata.numbuttons = 5;
        boxdata.buttons = buttons;
        boxdata.colorScheme = nullptr;

        int buttonid = -1;
        if (SDL_ShowMessageBox(&boxdata, &buttonid) < 0)
            return;

        if (buttonid == 0)
            testerShowMines = !testerShowMines;
        else if (buttonid == 1)
            testerShowTorches = !testerShowTorches;
        else if (buttonid == 2)
            testerShowAnswer = !testerShowAnswer;
        else if (buttonid == 3)
        {
            outcome = PlayOutcome::CLEARED;
            returnToMenu = true;
            keepOpen = false;
        }
        else
            keepOpen = false;

        syncTesterOverlay();
    }
}

PlayScene::PlayScene()
{
}

// Reset toàn bộ trạng thái cho một stage mới:
// sinh lại map, reset player về điểm xuất phát, làm sạch các cờ.
void PlayScene::startLevel(Difficulty newDifficulty)
{
    difficulty = newDifficulty;
    map.reset(difficulty, campaignStage);
    map.activateStarterTorch();
    player.setPosition(map.getStartRow(), map.getStartCol(), map.getTileSize());
    checkpointRow = map.getStartRow();
    checkpointCol = map.getStartCol();
    torchActivated = 0;
    testerShowMines = false;
    testerShowTorches = false;
    testerShowAnswer = false;
    syncTesterOverlay();
    outcome = PlayOutcome::NONE;
    blockMovementUntilRelease = true;
    levelStartTick = SDL_GetTicks();
    levelIntroUntilTick = levelStartTick + 2200;
}

// Load toàn bộ tài nguyên cho PlayScene: map, texture player, font HUD,
// pool câu hỏi, rồi khởi động stage đầu tiên.
void PlayScene::load(SDL_Renderer* renderer)
{
    rendererRef = renderer;
    returnToMenu = false;

    std::cout << "PlayScene Loaded\n";
    map.load(renderer, "assets/images/background/map_level_1.png", difficulty);

#if LIGHTQUEST_HAS_SDL_TTF
    if (!uiFontRaw)
    {
        if (TTF_WasInit() == 0)
        {
            if (TTF_Init() == 0)
                ttfInitByScene = true;
        }

        if (TTF_WasInit() != 0)
        {
            TTF_Font* opened = TTF_OpenFont("assets/fonts/Times New Roman.ttf", 22);
            if (!opened)
                opened = TTF_OpenFont("assets/fonts/Times New Roman.ttf", 18);
            uiFontRaw = opened;
        }
    }
#endif

    // chuẩn bị texture cho player
    player.clean();
    player.loadTexture(renderer);

    bool loadedQuestions = questionManager.loadFromFile("data/question.txt");
    startLevel(difficulty);

    if (!loadedQuestions)
    {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_WARNING,
            "Warning",
            "Could not read data/question.txt. Using fallback questions.",
            nullptr
        );
    }

}

// Xử lý sự kiện rời rạc: ESC về menu, phím T mở panel tester.
void PlayScene::handleEvent(SDL_Event &event)
{
    if (outcome != PlayOutcome::NONE || returnToMenu)
        return;

    if (event.type == SDL_KEYDOWN)
    {
        if (kTesterEnabledBuild && event.key.keysym.sym == SDLK_t)
        {
            openTesterPanel();
            return;
        }

        if (event.key.keysym.sym == SDLK_ESCAPE)
        {
            outcome = PlayOutcome::EXITED;
            returnToMenu = true;
            return;
        }
    }

    int oldRow = player.getRow();
    int oldCol = player.getCol();

    player.handleEvent(event, map);

    if (oldRow != player.getRow() || oldCol != player.getCol())
    {
        onPlayerMoved(oldRow, oldCol);
    }
}

// Gọi khi player bước sang tile mới. Xử lý:
// - Bước vào mìn → FAILED
// - Đến đích → CLEARED
// - Bước vào torch → hiển thị câu hỏi, xử lý đúng/sai
// - Bước vào ô tối chưa biết → mở fog
void PlayScene::onPlayerMoved(int oldRow, int oldCol)
{
    int row = player.getRow();
    int col = player.getCol();

    bool enteredDarkTile = !map.isKnown(row, col);

    if (map.isMine(row, col))
    {
        outcome = PlayOutcome::FAILED;
        returnToMenu = true;
        return;
    }

    if (map.isGoal(row, col))
    {
        outcome = PlayOutcome::CLEARED;
        returnToMenu = true;
        return;
    }

    if (enteredDarkTile)
    {
        map.revealAt(row, col);

        if (!map.canAttemptTorch(row, col))
        {
            checkpointRow = row;
            checkpointCol = col;
            return;
        }
    }

    if (map.canAttemptTorch(row, col))
    {
        bool correct = false;
        bool answeredQuestion = false;

        if (questionManager.hasQuestions())
        {
            int questionIndex = questionManager.randomIndex();
            bool showAnswer = kTesterEnabledBuild && testerShowAnswer;
            correct = questionManager.askQuestion(questionIndex, rendererRef, showAnswer);
            answeredQuestion = true;
        }

        if (!answeredQuestion)
        {
            map.resolveTorch(row, col, false);
            player.setPosition(oldRow, oldCol, map.getTileSize());
            SDL_ShowSimpleMessageBox(
                SDL_MESSAGEBOX_WARNING,
                "No Questions",
                "This torch is locked because the question system is invalid.",
                nullptr
            );
            return;
        }

// Khửng hoá đường dẫn an toàn đến torch tiếp theo và cập nhật checkpoint.
        map.resolveTorch(row, col, correct);

        if (correct)
        {
            torchActivated++;
            map.clearTorchHint();
            map.revealPathToNextTorch(row, col, torchActivated);
            checkpointRow = row;
            checkpointCol = col;
        }
        else
        {
            // Trả player về vị trí cũ và gợi ý torch gần nhất khi trả lời sai.
            map.hintNearestTorch(row, col);
            player.setPosition(oldRow, oldCol, map.getTileSize());
        }

    }
    else
    {
        checkpointRow = row;
        checkpointCol = col;
    }
}

// Cập nhật vật lý player mỗi frame theo kiểu pixel movement.
// Block input nếu người dùng đang giữ phím ngay khi stage mới load.
void PlayScene::update(float deltaTime)
{
    if (outcome != PlayOutcome::NONE || returnToMenu)
        return;

    // Di chuyển player theo pixel, sau đó kiểm tra có đổi tile không.
    const Uint8* keystate = SDL_GetKeyboardState(NULL);

    if (blockMovementUntilRelease)
    {
        if (isMovementKeyHeld(keystate))
            return;

        blockMovementUntilRelease = false;
    }

    int oldRow = player.getRow();
    int oldCol = player.getCol();

    player.update(keystate, deltaTime, map);

    if (oldRow != player.getRow() || oldCol != player.getCol())
    {
        onPlayerMoved(oldRow, oldCol);
    }
}

// Vẽ map, player, và overlay HUD theo thứ tự: map (nền) → player → overlay (trên cùng).
void PlayScene::render(SDL_Renderer *renderer)
{
    int offsetX = map.getRenderOffsetX();
    int offsetY = map.getRenderOffsetY();

    map.render(renderer);
    player.render(renderer, offsetX, offsetY);
    renderOverlay(renderer);
}

// Hiệu ứng chết: lộ toàn bộ mìn trên bản đồ, vẽ hiệu ứng nổ nơi player đứng,
// flash đỏ dần mờ trong 1.4 giây trước khi chuyển sang Game Over.
bool PlayScene::playDeathSequence(SDL_Renderer* renderer)
{
    if (!renderer)
        return false;

    SDL_Texture* explosionTexture = IMG_LoadTexture(renderer, "assets/images/entities/no.gif");
    map.setTesterOverlay(true, testerShowTorches);

    Uint32 startedAt = SDL_GetTicks();
    const Uint32 durationMs = 1400;
    bool shouldQuit = false;

    int tileSize = map.getTileSize();
    int centerX = map.getRenderOffsetX() + player.getCol() * tileSize + tileSize / 2;
    int centerY = map.getRenderOffsetY() + player.getRow() * tileSize + tileSize / 2;

    while (SDL_GetTicks() - startedAt < durationMs)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                shouldQuit = true;
                break;
            }
        }

        if (shouldQuit)
            break;

        Uint32 elapsed = SDL_GetTicks() - startedAt;
        float t = static_cast<float>(elapsed) / static_cast<float>(durationMs);
        if (t < 0.0f)
            t = 0.0f;
        if (t > 1.0f)
            t = 1.0f;

        map.render(renderer);
        player.render(renderer, map.getRenderOffsetX(), map.getRenderOffsetY());

        // Flash đỏ để nhấn mạnh cảm giác chết, fade dần theo thời gian.
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        Uint8 flashAlpha = static_cast<Uint8>(110.0f * (1.0f - t));
        SDL_SetRenderDrawColor(renderer, 255, 80, 40, flashAlpha);
        SDL_Rect full = {0, 0, 1280, 720};
        SDL_RenderFillRect(renderer, &full);

        int explosionSize = static_cast<int>(90.0f + 160.0f * t);
        SDL_Rect explosionRect = {
            centerX - explosionSize / 2,
            centerY - explosionSize / 2,
            explosionSize,
            explosionSize
        };

        if (explosionTexture)
        {
            Uint8 alpha = static_cast<Uint8>(255.0f - 180.0f * t);
            SDL_SetTextureAlphaMod(explosionTexture, alpha);
            SDL_RenderCopy(renderer, explosionTexture, nullptr, &explosionRect);
            SDL_SetTextureAlphaMod(explosionTexture, 255);
        }
        else
        {
            SDL_SetRenderDrawColor(renderer, 255, 145, 45, 190);
            SDL_RenderFillRect(renderer, &explosionRect);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    if (explosionTexture)
        SDL_DestroyTexture(explosionTexture);

    if (!shouldQuit)
    {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR,
            "Defeat",
            "You stepped on a mine. All mine positions were revealed.",
            nullptr
        );
    }

    syncTesterOverlay();
    return shouldQuit;
}

// Giải phóng tất cả tài nguyên của scene (map, player, font).
void PlayScene::clean()
{
    map.clean();
    player.clean();
#if LIGHTQUEST_HAS_SDL_TTF
    if (uiFontRaw)
    {
        TTF_CloseFont(static_cast<TTF_Font*>(uiFontRaw));
        uiFontRaw = nullptr;
    }
    if (ttfInitByScene)
    {
        TTF_Quit();
        ttfInitByScene = false;
    }
#endif
    rendererRef = nullptr;
}

// Vẽ panel overlay góc trên trái:
// dòng 1: số stage + độ khó, dòng 2: thời gian campaign.
// Hiển thị thêm banner intro stage trong 2.2s đầu khi stage mới bắt đầu.
void PlayScene::renderOverlay(SDL_Renderer* renderer)
{
    if (!renderer)
        return;

    SDL_Rect panel = {12, 20, 320, 66};
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 8, 10, 18, 160);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 210, 210, 220, 170);
    SDL_RenderDrawRect(renderer, &panel);

    SDL_Color diffColor = difficultyColor(difficulty);

    Uint32 ticksNow = SDL_GetTicks();
    int elapsedSec = campaignElapsedSeconds;
    if (elapsedSec < 0)
        elapsedSec = 0;

#if LIGHTQUEST_HAS_SDL_TTF
    if (uiFontRaw)
    {
        TTF_Font* font = static_cast<TTF_Font*>(uiFontRaw);
        char line1[220];
        char line2[220];
        std::snprintf(line1, sizeof(line1), "STAGE %d  |  DIFFICULTY: %s", campaignStage + 1, difficultyLabel(difficulty));
        std::snprintf(line2, sizeof(line2), "TIME: %d SEC", elapsedSec);

        renderTextLine(renderer, font, line1, 22, 26, diffColor);
        renderTextLine(renderer, font, line2, 22, 50, SDL_Color{236, 236, 236, 255});

        char controlTip[200];
        std::snprintf(controlTip, sizeof(controlTip), "WASD/Arrow keys: Move  |  ESC: Menu");
        renderTextLine(renderer, font, controlTip, 24, 680, SDL_Color{210, 210, 210, 220});

        if (ticksNow < levelIntroUntilTick)
        {
            Uint32 remain = levelIntroUntilTick - ticksNow;
            int alpha = 70 + static_cast<int>((remain * 120U) / 2200U);
            if (alpha > 190)
                alpha = 190;

            SDL_Rect introBox = {350, 18, 580, 64};
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, static_cast<Uint8>(alpha));
            SDL_RenderFillRect(renderer, &introBox);
            SDL_SetRenderDrawColor(renderer, diffColor.r, diffColor.g, diffColor.b, 220);
            SDL_RenderDrawRect(renderer, &introBox);

            char introText[180];
            std::snprintf(introText, sizeof(introText), "STAGE %d - %s", campaignStage + 1, difficultyLabel(difficulty));
            renderTextLine(renderer, font, introText, 540, 38, SDL_Color{255, 255, 255, 255});
        }
    }
#endif
}
