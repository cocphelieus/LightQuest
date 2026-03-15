#include "HUD.h"

#if __has_include(<SDL2/SDL_ttf.h>)
#include <SDL2/SDL_ttf.h>
#define LIGHTQUEST_HAS_SDL_TTF 1
#elif __has_include(<SDL_ttf.h>)
#include <SDL_ttf.h>
#define LIGHTQUEST_HAS_SDL_TTF 1
#else
#define LIGHTQUEST_HAS_SDL_TTF 0
#endif

#include <string>
#include <sstream>

namespace
{
    // Giới hạn giá trị trong khoảng [minValue, maxValue].
    int clampInt(int value, int minValue, int maxValue)
    {
        if (value < minValue) return minValue;
        if (value > maxValue) return maxValue;
        return value;
    }

    // Vẽ thanh trạng thái (HP, light, objective...) với nền tối trong suốt,
    // phần tô màu theo tỉ lệ ratio (0.0 - 1.0), và viền trắng bao ngoài.
    void drawBar(SDL_Renderer* renderer, int x, int y, int w, int h, float ratio, SDL_Color fillColor)
    {
        if (w <= 0 || h <= 0)
            return;

        float r = ratio;
        if (r < 0.0f) r = 0.0f;
        if (r > 1.0f) r = 1.0f;

        SDL_Rect bg = {x, y, w, h};
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 180);
        SDL_RenderFillRect(renderer, &bg);

        int fillW = static_cast<int>(r * static_cast<float>(w));
        if (fillW > 0)
        {
            SDL_Rect fill = {x, y, fillW, h};
            SDL_SetRenderDrawColor(renderer, fillColor.r, fillColor.g, fillColor.b, fillColor.a);
            SDL_RenderFillRect(renderer, &fill);
        }

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 230, 230, 230, 255);
        SDL_RenderDrawRect(renderer, &bg);
    }
}

// Khởi tạo HUD: load font chữ để render text trong game.
// Nếu SDL_ttf không có sẵn hoặc font thiếu, HUD vẫn chạy nhưng không hiện text.
bool HUD::init(SDL_Renderer* renderer)
{
    (void)renderer;

#if !LIGHTQUEST_HAS_SDL_TTF
    fontRaw = nullptr;
    return true; // no SDL_ttf available -> HUD text disabled
#else
    if (TTF_WasInit() == 0)
    {
        if (TTF_Init() != 0)
        {
            fontRaw = nullptr;
            return true; // fail gracefully
        }
        ttfInitByHUD = true;
    }

    const char* fontPath = "assets/fonts/FreeSans.ttf";
    TTF_Font* font = TTF_OpenFont(fontPath, 20);
    if (!font)
    {
        fontRaw = nullptr;
        return true; // font missing -> HUD text disabled
    }

    fontRaw = font;
    return true;
#endif
}

// Vẽ toàn bộ panel HUD góc trên trái mỗi frame:
// - 5 thanh: HP (đỏ), Light (vàng), Objective/Torch (xanh lá), Combo (hồng), Wave (xanh dương)
// - 1 dòng text tổng hợp tất cả chỉ số bên dưới panel
void HUD::render(
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
)
{
    if (!renderer)
        return;

    // Panel HUD nằm góc trên trái, kích thước cố định 270x154.
    const int panelX = 16;
    const int panelY = 16;
    const int panelW = 270;
    const int panelH = 154;

    SDL_Rect panel = {panelX, panelY, panelW, panelH};
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 145);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderDrawRect(renderer, &panel);

    int safeMaxHealth = (maxHealth <= 0) ? 1 : maxHealth;
    int safeMaxLight = (maxLight <= 0) ? 1 : maxLight;
    int safeRequiredTorches = (requiredTorches <= 0) ? 1 : requiredTorches;
    int safeWaveThreshold = (waveThreshold <= 0) ? 1 : waveThreshold;

    float healthRatio = static_cast<float>(clampInt(health, 0, safeMaxHealth)) / static_cast<float>(safeMaxHealth);
    float lightRatio = static_cast<float>(clampInt(light, 0, safeMaxLight)) / static_cast<float>(safeMaxLight);
    float objectiveRatio = static_cast<float>(clampInt(torchesActivated, 0, safeRequiredTorches)) / static_cast<float>(safeRequiredTorches);
    float waveRatio = static_cast<float>(clampInt(waveProgress, 0, safeWaveThreshold)) / static_cast<float>(safeWaveThreshold);

    int comboCap = 6;
    float comboRatio = static_cast<float>(clampInt(combo, 0, comboCap)) / static_cast<float>(comboCap);

    // Vẽ lần lượt: thanh HP, Light, tiến độ đốt đuốc, Combo, Wave.
    drawBar(renderer, panelX + 12, panelY + 16, 246, 18, healthRatio, SDL_Color{220, 70, 70, 255});
    drawBar(renderer, panelX + 12, panelY + 42, 246, 18, lightRatio, SDL_Color{255, 190, 50, 255});
    drawBar(renderer, panelX + 12, panelY + 68, 246, 16, objectiveRatio, SDL_Color{90, 230, 130, 255});
    drawBar(renderer, panelX + 12, panelY + 92, 246, 14, comboRatio, SDL_Color{255, 120, 210, 255});
    drawBar(renderer, panelX + 12, panelY + 114, 246, 12, waveRatio, SDL_Color{120, 170, 255, 255});

#if !LIGHTQUEST_HAS_SDL_TTF
    (void)score;
    (void)elapsedSec;
    return;
#else
    if (!fontRaw) return;

    TTF_Font* font = static_cast<TTF_Font*>(fontRaw);
    std::ostringstream ss;
    ss
        << "HP " << clampInt(health, 0, safeMaxHealth) << "/" << safeMaxHealth
        << "  Light " << clampInt(light, 0, safeMaxLight) << "/" << safeMaxLight
        << "  Score " << score
        << "  Torch " << clampInt(torchesActivated, 0, safeRequiredTorches) << "/" << safeRequiredTorches
        << "  Combo " << combo
        << "  Wave " << clampInt(waveProgress, 0, safeWaveThreshold) << "/" << safeWaveThreshold
        << "  Time " << static_cast<int>(elapsedSec) << "s";
    std::string s = ss.str();

    SDL_Color color = {255, 255, 255, 255};
    SDL_Surface* surf = TTF_RenderText_Solid(font, s.c_str(), color);
    if (!surf) return;

    if (textTexture) { SDL_DestroyTexture(textTexture); textTexture = nullptr; }
    textTexture = SDL_CreateTextureFromSurface(renderer, surf);
    texW = surf->w; texH = surf->h;
    SDL_FreeSurface(surf);

    if (textTexture)
    {
        SDL_Rect dst = {20, 174, texW, texH};
        SDL_RenderCopy(renderer, textTexture, nullptr, &dst);
    }
#endif
}

// Giải phóng texture text và font khi HUD không còn dùng nữa.
void HUD::clean()
{
    if (textTexture) { SDL_DestroyTexture(textTexture); textTexture = nullptr; }

#if LIGHTQUEST_HAS_SDL_TTF
    if (fontRaw) { TTF_CloseFont(static_cast<TTF_Font*>(fontRaw)); fontRaw = nullptr; }
    if (ttfInitByHUD) { TTF_Quit(); ttfInitByHUD = false; }
#else
    fontRaw = nullptr;
    ttfInitByHUD = false;
#endif
}