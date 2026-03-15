#include "Game.h"
#include "SoundManager.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <string>
#include <ctime>

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
    struct RankingEntry
    {
        int seconds = 0;
        std::string timestamp;
    };

    const char* kRankingFilePath = "data/ranking.txt";

    // Đọc file ranking.txt, parse từng dòng thành cặp (giây | timestamp).
    std::vector<RankingEntry> loadRankings()
    {
        std::vector<RankingEntry> entries;
        std::ifstream in(kRankingFilePath);
        if (!in.is_open())
            return entries;

        std::string line;
        while (std::getline(in, line))
        {
            if (line.empty())
                continue;

            std::stringstream ss(line);
            std::string secText;
            std::string timestamp;
            if (!std::getline(ss, secText, '|'))
                continue;
            if (!std::getline(ss, timestamp))
                timestamp.clear();

            int seconds = 0;
            try {
                seconds = std::stoi(secText);
            } catch (...) {
                continue;
            }

            if (seconds < 0)
                continue;

            RankingEntry e;
            e.seconds = seconds;
            e.timestamp = timestamp;
            entries.push_back(e);
        }

        return entries;
    }

    // Ghi danh sách ranking ra file, mỗi entry 1 dòng “giây|timestamp”.
    void saveRankings(const std::vector<RankingEntry>& entries)
    {
        std::ofstream out(kRankingFilePath, std::ios::trunc);
        if (!out.is_open())
            return;

        for (size_t i = 0; i < entries.size(); i++)
            out << entries[i].seconds << "|" << entries[i].timestamp << "\n";
    }

    // Lấy thời điểm hiện tại dưới dạng chuỗi "YYYY-MM-DD HH:MM:SS".
    std::string currentTimestamp()
    {
        std::time_t now = std::time(nullptr);
        std::tm localTm;
#ifdef _WIN32
        localtime_s(&localTm, &now);
#else
        localTm = *std::localtime(&now);
#endif
        char buffer[32];
        std::snprintf(
            buffer,
            sizeof(buffer),
            "%04d-%02d-%02d %02d:%02d:%02d",
            localTm.tm_year + 1900,
            localTm.tm_mon + 1,
            localTm.tm_mday,
            localTm.tm_hour,
            localTm.tm_min,
            localTm.tm_sec
        );
        return std::string(buffer);
    }

    // Thêm kết quả mới vào ranking, sắp xếp theo thời gian tăng dần,
    // giữ tối đa 5 entry. Trả về hạng của người chơi (1-based) hoặc -1 nếu không vào top 5.
    int addCampaignClearToRanking(int elapsedSeconds)
    {
        std::vector<RankingEntry> entries = loadRankings();
        RankingEntry added;
        added.seconds = elapsedSeconds;
        added.timestamp = currentTimestamp();
        entries.push_back(added);

        std::sort(entries.begin(), entries.end(), [](const RankingEntry& a, const RankingEntry& b) {
            if (a.seconds != b.seconds)
                return a.seconds < b.seconds;
            return a.timestamp < b.timestamp;
        });

        int rank = -1;
        for (size_t i = 0; i < entries.size(); i++)
        {
            if (entries[i].seconds == added.seconds && entries[i].timestamp == added.timestamp)
            {
                rank = static_cast<int>(i) + 1;
                break;
            }
        }

        if (entries.size() > 5)
            entries.resize(5);

        bool stillInTop5 = false;
        for (size_t i = 0; i < entries.size(); i++)
        {
            if (entries[i].seconds == added.seconds && entries[i].timestamp == added.timestamp)
            {
                rank = static_cast<int>(i) + 1;
                stillInTop5 = true;
                break;
            }
        }

        saveRankings(entries);
        if (!stillInTop5)
            return -1;
        return rank;
    }

#if LIGHTQUEST_HAS_SDL_TTF
    void drawCenteredText(SDL_Renderer* renderer, TTF_Font* font, const char* text, SDL_Color color, const SDL_Rect& rect)
    {
        if (!renderer || !font || !text)
            return;

        SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
        if (!surface)
            return;

        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture)
        {
            SDL_Rect dst = {
                rect.x + (rect.w - surface->w) / 2,
                rect.y + (rect.h - surface->h) / 2,
                surface->w,
                surface->h
            };
            SDL_RenderCopy(renderer, texture, nullptr, &dst);
            SDL_DestroyTexture(texture);
        }

        SDL_FreeSurface(surface);
    }
#endif
}

// Xác định độ khó theo stage của campaign:
// Stage 0-1 = EASY, 2-3 = MEDIUM, 4+ = HARD.
static Difficulty difficultyForCampaignStage(int stage)
{
    if (stage <= 1)
        return Difficulty::EASY;
    if (stage <= 3)
        return Difficulty::MEDIUM;
    return Difficulty::HARD;
}

// Trả về tên độ khó dưới dạng chuỗi để hiển thị.
static const char* difficultyName(Difficulty difficulty)
{
    if (difficulty == Difficulty::EASY)
        return "Easy";
    if (difficulty == Difficulty::MEDIUM)
        return "Medium";
    return "Hard";
}

// Hiển thị popup "Stage Cleared" sau khi vượt mỗi stage.
// Cho thấy độ khó vừa qua và thông tin stage tiếp theo.
// Trả về true nếu người dùng đóng cửa sổ trong khi popup hiển thị.
static bool showStageClearMessage(SDL_Renderer* renderer, int stage)
{
    if (!renderer)
        return false;

    Difficulty currentDifficulty = difficultyForCampaignStage(stage);
    Difficulty nextDifficulty = difficultyForCampaignStage(stage + 1);

#if !LIGHTQUEST_HAS_SDL_TTF
    char message[256];
    std::snprintf(
        message,
        sizeof(message),
        "Great job! You cleared Stage %d (%s).\n"
        "Next stage: %d (%s).",
        stage + 1,
        difficultyName(currentDifficulty),
        stage + 2,
        difficultyName(nextDifficulty)
    );

    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Stage Cleared", message, nullptr);
    return false;
#else
    bool ttfInitedHere = false;
    if (TTF_WasInit() == 0)
    {
        if (TTF_Init() != 0)
            return false;
        ttfInitedHere = true;
    }

    TTF_Font* titleFont = TTF_OpenFont("assets/fonts/Times New Roman.ttf", 58);
    TTF_Font* bodyFont = TTF_OpenFont("assets/fonts/Times New Roman.ttf", 28);
    if (!titleFont || !bodyFont)
    {
        if (titleFont)
            TTF_CloseFont(titleFont);
        if (bodyFont)
            TTF_CloseFont(bodyFont);
        if (ttfInitedHere)
            TTF_Quit();
        return false;
    }

    TTF_SetFontStyle(titleFont, TTF_STYLE_BOLD);

    Uint32 startedAt = SDL_GetTicks();
    const Uint32 minShowMs = 500;
    const Uint32 maxShowMs = 1500;
    bool shouldQuit = false;
    bool done = false;

    char line1[128];
    char line2[160];
    std::snprintf(line1, sizeof(line1), "STAGE %d CLEARED", stage + 1);
    std::snprintf(line2, sizeof(line2), "Next: Stage %d (%s)", stage + 2, difficultyName(nextDifficulty));

    while (!done)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                shouldQuit = true;
                done = true;
                break;
            }

            if ((event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN) && SDL_GetTicks() - startedAt >= minShowMs)
            {
                done = true;
            }
        }

        Uint32 elapsed = SDL_GetTicks() - startedAt;
        if (elapsed >= maxShowMs)
            done = true;

        float t = static_cast<float>(elapsed) / static_cast<float>(maxShowMs);
        if (t < 0.0f)
            t = 0.0f;
        if (t > 1.0f)
            t = 1.0f;

        SDL_SetRenderDrawColor(renderer, 5, 8, 16, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 170);
        SDL_Rect full = {0, 0, 1280, 720};
        SDL_RenderFillRect(renderer, &full);

        SDL_Rect panel = {220, 170, 840, 360};
        SDL_SetRenderDrawColor(renderer, 12, 18, 34, 220);
        SDL_RenderFillRect(renderer, &panel);

        SDL_SetRenderDrawColor(renderer, 82, 245, 170, 220);
        SDL_Rect topGlow = {panel.x + 20, panel.y + 20, panel.w - 40, 10};
        SDL_RenderFillRect(renderer, &topGlow);

        SDL_SetRenderDrawColor(renderer, 140, 230, 190, 230);
        SDL_RenderDrawRect(renderer, &panel);

        SDL_Rect titleRect = {panel.x, panel.y + 58, panel.w, 86};
        SDL_Rect bodyRectA = {panel.x, panel.y + 176, panel.w, 44};
        SDL_Rect bodyRectB = {panel.x, panel.y + 226, panel.w, 40};
        SDL_Rect bodyRectC = {panel.x, panel.y + 286, panel.w, 30};

        drawCenteredText(renderer, titleFont, line1, SDL_Color{132, 255, 198, 255}, titleRect);

        char currentLine[128];
        std::snprintf(currentLine, sizeof(currentLine), "Difficulty beaten: %s", difficultyName(currentDifficulty));
        drawCenteredText(renderer, bodyFont, currentLine, SDL_Color{230, 236, 242, 255}, bodyRectA);
        drawCenteredText(renderer, bodyFont, line2, SDL_Color{230, 236, 242, 255}, bodyRectB);

        int remain = static_cast<int>((maxShowMs > elapsed) ? (maxShowMs - elapsed) : 0U);
        char skipHint[80];
        std::snprintf(skipHint, sizeof(skipHint), "Press any key to continue (%0.1fs)", static_cast<float>(remain) / 1000.0f);
        drawCenteredText(renderer, bodyFont, skipHint, SDL_Color{160, 170, 188, 255}, bodyRectC);

        SDL_Rect barBg = {panel.x + 80, panel.y + panel.h - 46, panel.w - 160, 8};
        SDL_SetRenderDrawColor(renderer, 42, 52, 76, 230);
        SDL_RenderFillRect(renderer, &barBg);
        SDL_SetRenderDrawColor(renderer, 82, 245, 170, 230);
        SDL_Rect barFill = {barBg.x, barBg.y, static_cast<int>(static_cast<float>(barBg.w) * t), barBg.h};
        SDL_RenderFillRect(renderer, &barFill);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    TTF_CloseFont(titleFont);
    TTF_CloseFont(bodyFont);
    if (ttfInitedHere)
        TTF_Quit();

    return shouldQuit;
#endif
}

// Khởi động campaign từ đầu: đặt stage 0 và load PlayScene.
static void startCampaignFromBeginning(PlayScene& play, SDL_Renderer* renderer)
{
    play.setCampaignStage(0);
    play.setDifficulty(difficultyForCampaignStage(0));
    play.load(renderer);
}

// Hiển thị màn hình chiến thắng cuối campaign:
// tổng thời gian, hạng xếp hạng, hiệu ứng pulse và timer tự đóng.
static bool showCampaignCompleteMessage(SDL_Renderer* renderer, int stage, int elapsedSeconds, int rankingPlace)
{
    if (!renderer)
        return false;

    char message[360];
    int minutes = elapsedSeconds / 60;
    int seconds = elapsedSeconds % 60;
    if (rankingPlace > 0)
    {
        std::snprintf(
            message,
            sizeof(message),
            "You conquered all %d LightQuest stages!\n"
            "Total clear time: %d min %02d sec.\n"
            "Local ranking: TOP %d",
            stage + 1,
            minutes,
            seconds,
            rankingPlace
        );
    }
    else
    {
        std::snprintf(
            message,
            sizeof(message),
            "You conquered all %d LightQuest stages!\n"
            "Total clear time: %d min %02d sec.\n"
            "Check the Rank screen for Local Top 5.",
            stage + 1,
            minutes,
            seconds
        );
    }

#if !LIGHTQUEST_HAS_SDL_TTF
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "LightQuest - Campaign Complete", message, nullptr);
    return false;
#else
    bool ttfInitedHere = false;
    if (TTF_WasInit() == 0)
    {
        if (TTF_Init() != 0)
            return false;
        ttfInitedHere = true;
    }

    TTF_Font* titleFont = TTF_OpenFont("assets/fonts/Times New Roman.ttf", 62);
    TTF_Font* bodyFont = TTF_OpenFont("assets/fonts/Times New Roman.ttf", 30);
    TTF_Font* subFont = TTF_OpenFont("assets/fonts/Times New Roman.ttf", 24);
    if (!titleFont || !bodyFont || !subFont)
    {
        if (titleFont) TTF_CloseFont(titleFont);
        if (bodyFont) TTF_CloseFont(bodyFont);
        if (subFont) TTF_CloseFont(subFont);
        if (ttfInitedHere)
            TTF_Quit();
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "LightQuest - Campaign Complete", message, nullptr);
        return false;
    }

    TTF_SetFontStyle(titleFont, TTF_STYLE_BOLD);

    Uint32 startedAt = SDL_GetTicks();
    const Uint32 minShowMs = 1600;
    const Uint32 maxShowMs = 12000;
    bool shouldQuit = false;
    bool done = false;

    char timeText[80];
    std::snprintf(timeText, sizeof(timeText), "TOTAL TIME: %d:%02d", minutes, seconds);

    char rankText[96];
    if (rankingPlace > 0)
        std::snprintf(rankText, sizeof(rankText), "LOCAL RANK: TOP %d", rankingPlace);
    else
        std::snprintf(rankText, sizeof(rankText), "LOCAL RANK: OUTSIDE TOP 5");

    while (!done)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                shouldQuit = true;
                done = true;
                break;
            }

            if ((event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN) &&
                SDL_GetTicks() - startedAt >= minShowMs)
            {
                done = true;
            }
        }

        Uint32 elapsed = SDL_GetTicks() - startedAt;
        if (elapsed >= maxShowMs)
            done = true;

        float t = static_cast<float>(elapsed) / static_cast<float>(maxShowMs);
        if (t < 0.0f)
            t = 0.0f;
        if (t > 1.0f)
            t = 1.0f;

        SDL_SetRenderDrawColor(renderer, 6, 8, 14, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 165);
        SDL_Rect full = {0, 0, 1280, 720};
        SDL_RenderFillRect(renderer, &full);

        int pulse = static_cast<int>(16.0f * std::sin(static_cast<float>(elapsed) * 0.0085f));
        SDL_Rect panel = {170 - pulse / 2, 120 - pulse / 2, 940 + pulse, 470 + pulse};
        SDL_SetRenderDrawColor(renderer, 14, 18, 34, 228);
        SDL_RenderFillRect(renderer, &panel);
        SDL_SetRenderDrawColor(renderer, 110, 220, 255, 230);
        SDL_RenderDrawRect(renderer, &panel);

        SDL_SetRenderDrawColor(renderer, 245, 208, 98, 220);
        SDL_Rect crownBar = {panel.x + 120, panel.y + 28, panel.w - 240, 10};
        SDL_RenderFillRect(renderer, &crownBar);

        SDL_Rect titleRect = {panel.x, panel.y + 58, panel.w, 84};
        drawCenteredText(renderer, titleFont, "CAMPAIGN CONQUERED", SDL_Color{255, 232, 152, 255}, titleRect);

        SDL_Rect stagesRect = {panel.x, panel.y + 172, panel.w, 42};
        char stageText[96];
        std::snprintf(stageText, sizeof(stageText), "ALL %d STAGES CLEARED", stage + 1);
        drawCenteredText(renderer, bodyFont, stageText, SDL_Color{220, 230, 242, 255}, stagesRect);

        SDL_Rect timeRect = {panel.x, panel.y + 228, panel.w, 40};
        drawCenteredText(renderer, bodyFont, timeText, SDL_Color{133, 243, 195, 255}, timeRect);

        SDL_Rect rankRect = {panel.x, panel.y + 274, panel.w, 40};
        drawCenteredText(renderer, bodyFont, rankText, SDL_Color{255, 220, 128, 255}, rankRect);

        SDL_Rect descRect = {panel.x + 90, panel.y + 332, panel.w - 180, 34};
        drawCenteredText(renderer, subFont, "Your legend is now written in the local hall of fame.", SDL_Color{190, 198, 220, 255}, descRect);

        int remainMs = static_cast<int>((maxShowMs > elapsed) ? (maxShowMs - elapsed) : 0U);
        char continueText[96];
        std::snprintf(continueText, sizeof(continueText), "Press any key to continue (%0.1fs)", static_cast<float>(remainMs) / 1000.0f);
        SDL_Rect continueRect = {panel.x, panel.y + panel.h - 54, panel.w, 30};
        drawCenteredText(renderer, subFont, continueText, SDL_Color{152, 165, 188, 255}, continueRect);

        SDL_Rect barBg = {panel.x + 90, panel.y + panel.h - 20, panel.w - 180, 8};
        SDL_SetRenderDrawColor(renderer, 46, 56, 80, 230);
        SDL_RenderFillRect(renderer, &barBg);
        SDL_SetRenderDrawColor(renderer, 245, 208, 98, 240);
        SDL_Rect barFill = {barBg.x, barBg.y, static_cast<int>(static_cast<float>(barBg.w) * t), barBg.h};
        SDL_RenderFillRect(renderer, &barFill);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    TTF_CloseFont(titleFont);
    TTF_CloseFont(bodyFont);
    TTF_CloseFont(subFont);
    if (ttfInitedHere)
        TTF_Quit();

    return shouldQuit;
#endif
}

// Hiển thị màn hình Game Over (như nh thế).
// Hiển thị tối thiểu 1.4s, tự đóng sau 4.5s hoặc khi nhấn bất kỳ phím/click.
static bool showGameOverScreen(SDL_Renderer* renderer)
{
    if (!renderer)
        return false;

    SDL_Texture* gameOverTexture = IMG_LoadTexture(renderer, "assets/images/background/game_over.png");
    Uint32 startedAt = SDL_GetTicks();
    const Uint32 minShowMs = 1400;
    const Uint32 maxShowMs = 4500;
    bool shouldQuit = false;
    bool done = false;

    // Keep a minimum display time so players can perceive failure feedback,
    // then allow skip via key/mouse input.
    while (!done)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                shouldQuit = true;
                done = true;
                break;
            }

            if (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN)
            {
                if (SDL_GetTicks() - startedAt >= minShowMs)
                    done = true;
            }
        }

        Uint32 elapsed = SDL_GetTicks() - startedAt;
        if (elapsed >= maxShowMs)
            done = true;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        if (gameOverTexture)
        {
            SDL_RenderCopy(renderer, gameOverTexture, nullptr, nullptr);
        }
        else
        {
            // Dự phòng khi nh không tìm thấy file game_over.png.
            SDL_ShowSimpleMessageBox(
                SDL_MESSAGEBOX_ERROR,
                "Defeat",
                "You lost!",
                nullptr
            );
            done = true;
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    if (gameOverTexture)
        SDL_DestroyTexture(gameOverTexture);

    return shouldQuit;
}

void Game::run()
{
    // ===== Khởi tạo cửa sổ =====
    window.init("LightQuest", 1280, 720);

    SoundManager& sound = SoundManager::instance();
    sound.init();
    sound.loadAssets();

    // ===== Load các Scene =====
    loading.load(window.getRenderer());
    menu.load(window.getRenderer());
    sound.playMenuBgm();

    SDL_Event event;
    lastFrameTime = SDL_GetTicks();

    int campaignStage = 0;
    const int finalCampaignStage = 4; // tổng 5 stage (0..4)
    Uint32 campaignStartTick = 0;
    int campaignElapsedSeconds = 0;

    // ===== Vòng lặp chính =====
    // Mỗi frame: xử lý sự kiện → update logic → render → giới hạn FPS.
    while (running)
    {
        Uint32 frameStart = SDL_GetTicks();
        float deltaTime = (frameStart - lastFrameTime) / 1000.0f;
        lastFrameTime = frameStart;

        // =========================
        // XỬ LÝ SỰ KIỆN
        // =========================
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
                running = false;

            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F11)
            {
                window.toggleFullscreen();
                continue;
            }

            // LOADING: Skip with SPACE
            if (currentState == GameState::LOADING &&
                event.type == SDL_KEYDOWN &&
                event.key.keysym.sym == SDLK_SPACE)
            {
                currentState = GameState::MENU;
                sound.playMenuBgm();
            }

            // MENU
            if (currentState == GameState::MENU)
            {
                menu.handleEvent(event);

                if (menu.getState() == MenuState::EXIT)
                    running = false;

                if (menu.getState() == MenuState::PLAY)
                {
                    // Starting gameplay: stop menu BGM so background music stays in menu only.
                    campaignStage = 0;
                    campaignStartTick = SDL_GetTicks();
                    campaignElapsedSeconds = 0;
                    sound.stopMusic(180);
                    currentState = GameState::PLAYING;
                    startCampaignFromBeginning(play, window.getRenderer());
                    play.setCampaignElapsedSeconds(campaignElapsedSeconds);
                }
            }

            // PLAYING
            if (currentState == GameState::PLAYING)
            {
                play.handleEvent(event);
            }
        }

        // =========================
        // CẬP NHẬT LOGIC
        // =========================
        switch (currentState)
        {
        case GameState::LOADING:
            loading.update(deltaTime);
            if (loading.isComplete())
            {
                currentState = GameState::MENU;
                sound.playMenuBgm();
            }
            break;

        case GameState::MENU:
            menu.update();
            break;

        case GameState::PLAYING:
            campaignElapsedSeconds = static_cast<int>((SDL_GetTicks() - campaignStartTick) / 1000U);
            play.setCampaignElapsedSeconds(campaignElapsedSeconds);
            play.update(deltaTime);
            if (play.shouldReturnToMenu())
            {
                PlayOutcome outcome = play.getOutcome();

                if (outcome == PlayOutcome::CLEARED)
                {
                    if (campaignStage < finalCampaignStage)
                    {
                        // Phát nhạc chuyển cảnh và hiển thị popup kết thúc stage.
                        sound.playTransition();
                        bool quitFromStagePopup = showStageClearMessage(window.getRenderer(), campaignStage);
                        if (quitFromStagePopup)
                        {
                            running = false;
                            break;
                        }
                    }
                    campaignStage++;

                    if (campaignStage > finalCampaignStage)
                    {
                        // Hoàn thành toàn bộ campaign: phát nhạc thắng, lưu ranking, hiển thị popup.
                        sound.playWin();
                        int rankingPlace = addCampaignClearToRanking(campaignElapsedSeconds);
                        bool quitFromComplete = showCampaignCompleteMessage(window.getRenderer(), campaignStage - 1, campaignElapsedSeconds, rankingPlace);
                        if (quitFromComplete)
                        {
                            running = false;
                            break;
                        }
                        menu.refreshRankings();
                        currentState = GameState::MENU;
                        sound.playMenuBgm();
                        menu.resetToMain();
                        play.clean();
                        campaignStage = 0;
                        campaignElapsedSeconds = 0;
                        break;
                    }

                    play.clean();
                    play.setCampaignStage(campaignStage);
                    play.setDifficulty(difficultyForCampaignStage(campaignStage));
                    play.setCampaignElapsedSeconds(campaignElapsedSeconds);
                    play.load(window.getRenderer());
                }
                else
                {
                    if (outcome == PlayOutcome::FAILED)
                    {
                        // Luồng thất bại gồm 2 bước:
                        // 1) Hiệu ứng nổ tại chỗ player chết
                        // 2) Tiếng thua + màn hình Game Over
                        sound.playBomb();
                        bool quitFromDeath = play.playDeathSequence(window.getRenderer());
                        if (quitFromDeath)
                        {
                            running = false;
                            break;
                        }
                        sound.playLoss();
                        bool quitRequested = showGameOverScreen(window.getRenderer());
                        sound.stopLoss();
                        if (quitRequested)
                        {
                            running = false;
                            break;
                        }
                    }

                    currentState = GameState::MENU;
                    sound.playMenuBgm();
                    menu.resetToMain();
                    play.clean();
                    campaignStage = 0;
                    campaignElapsedSeconds = 0;
                }
            }
            break;
        }

        // =========================
        // VỜ MÀN HÌNH
        // =========================
        window.clear();

        switch (currentState)
        {
        case GameState::LOADING:
            loading.render(window.getRenderer());
            break;

        case GameState::MENU:
            menu.render(window.getRenderer());
            break;

        case GameState::PLAYING:
            play.render(window.getRenderer());
            break;
        }

        window.present();

        // =========================
        // GIỚI HẠN FPS (60 FPS)
        // =========================
        Uint32 frameTime = SDL_GetTicks() - frameStart;
        if (frameTime < FRAME_TIME)
        {
            SDL_Delay(static_cast<Uint32>(FRAME_TIME - frameTime));
        }
    }

    // ===== Dọn sạch khi thoát =====
    play.clean();
    menu.clean();
    loading.clean();
    sound.shutdown();
    window.clean();
}
