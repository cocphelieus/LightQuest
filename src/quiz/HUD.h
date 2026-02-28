#pragma once

#include "QuestionManager.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <string>

// Graphical HUD for questions.  Supports showing a question with four choices,
// rendering a semi-transparent background, and handling mouse/keyboard input.
class HUD
{
public:
    HUD(SDL_Renderer* renderer);
    ~HUD();

    bool init();               // load fonts/textures
    void clean();              // free resources

    void showQuestion(const Question& q);
    void hide();
    bool isActive() const { return active; }

    void render();
    // process event; if question answered, returns true and sets 'correct'
    bool handleEvent(const SDL_Event& event, bool& correct);

private:
    SDL_Renderer* renderer = nullptr;

    SDL_Texture* bgTexture = nullptr;
    SDL_Texture* iconTexture = nullptr;
    // optional entity textures for UI styling
    SDL_Texture* tileDark = nullptr;
    SDL_Texture* tileLight = nullptr;
    SDL_Texture* bombTex = nullptr;
    SDL_Texture* fireTex = nullptr;
    TTF_Font* font = nullptr;

    Question currentQuestion;
    bool active = false;

    SDL_Rect choiceRects[4];

    SDL_Texture* createTextTexture(const std::string& text, SDL_Color color, int& outW, int& outH);
};
