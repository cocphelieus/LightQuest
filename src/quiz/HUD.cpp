#include "HUD.h"
#include <iostream>
#include "../core/Asset.h"

HUD::HUD(SDL_Renderer* renderer): renderer(renderer) {}

HUD::~HUD() { clean(); }

bool HUD::init()
{
    if (!renderer) return false;

    // primary UI assets (may not exist)
    bgTexture = IMG_LoadTexture(renderer, "assets/images/background/question_bg.png");
    iconTexture = IMG_LoadTexture(renderer, "assets/images/background/question.png");

    // additional entity textures for nicer look/fallbacks
    tileDark = IMG_LoadTexture(renderer, "assets/images/entities/tile_dark.png");
    tileLight = IMG_LoadTexture(renderer, "assets/images/entities/tile_light.png");
    bombTex  = IMG_LoadTexture(renderer, "assets/images/entities/bom.png");
    fireTex  = IMG_LoadTexture(renderer, "assets/images/entities/fire.png");

    font = TTF_OpenFont("assets/fonts/arial.ttf", 20);
    if (!font)
    {
        std::cout << "HUD: failed to load arial.ttf: " << TTF_GetError() << std::endl;
        // try alternative font present in assets
        font = TTF_OpenFont("assets/fonts/Times New Roman.ttf", 20);
        if (font)
            std::cout << "HUD: using Times New Roman.ttf as fallback\n";
        else
            std::cout << "HUD: no font available, text will not render\n";
    }
    // do not fail initialization if font is missing; HUD can still draw buttons
    return true;
}

void HUD::clean()
{
    if (bgTexture) { SDL_DestroyTexture(bgTexture); bgTexture = nullptr; }
    if (iconTexture) { SDL_DestroyTexture(iconTexture); iconTexture = nullptr; }
    if (tileDark) { SDL_DestroyTexture(tileDark); tileDark = nullptr; }
    if (tileLight) { SDL_DestroyTexture(tileLight); tileLight = nullptr; }
    if (bombTex) { SDL_DestroyTexture(bombTex); bombTex = nullptr; }
    if (fireTex) { SDL_DestroyTexture(fireTex); fireTex = nullptr; }
    if (font) { TTF_CloseFont(font); font = nullptr; }
}

void HUD::showQuestion(const Question& q)
{
    currentQuestion = q;
    active = true;
    // layout buttons centered
    int cx = 1280/2;
    int cy = 720/2;
    int w = 220, h = 50;
    choiceRects[0] = {cx - w - 10, cy - h - 10, w, h};
    choiceRects[1] = {cx + 10,       cy - h - 10, w, h};
    choiceRects[2] = {cx - w - 10, cy + 10, w, h};
    choiceRects[3] = {cx + 10,       cy + 10, w, h};
}

void HUD::hide()
{
    active = false;
}

void HUD::render()
{
    if (!active) return;
    // overlay
    SDL_SetRenderDrawColor(renderer, 0,0,0,160);
    SDL_Rect full = {0,0,1280,720};
    SDL_RenderFillRect(renderer,&full);

    // background image or fallback colour
    if (bgTexture)
    {
        SDL_Rect bg = {100,100,1080,520};
        SDL_RenderCopy(renderer, bgTexture, nullptr, &bg);
    }
    else
    {
        SDL_SetRenderDrawColor(renderer, 50,50,50,200);
        SDL_Rect bg = {100,100,1080,520};
        SDL_RenderFillRect(renderer,&bg);
    }

    if (iconTexture)
    {
        SDL_Rect icon = {120,120,64,64};
        SDL_RenderCopy(renderer, iconTexture, nullptr, &icon);
    }

    // decorative entity icons (if available)
    if (bombTex)
    {
        SDL_Rect b = {1080, 100, 64, 64};
        SDL_RenderCopy(renderer, bombTex, nullptr, &b);
    }
    if (fireTex)
    {
        SDL_Rect f = {1080, 160, 64, 64};
        SDL_RenderCopy(renderer, fireTex, nullptr, &f);
    }

    // question text
    if (font)
    {
        int tw,th;
        SDL_Texture* tve = createTextTexture(currentQuestion.text, {255,255,255,255}, tw, th);
        if (tve)
        {
            SDL_Rect dst = {200,120,tw,th};
            SDL_RenderCopy(renderer,tve,nullptr,&dst);
            SDL_DestroyTexture(tve);
        }
    }
    // draw buttons with text
    int mx, my;
    SDL_GetMouseState(&mx,&my);
    for(int i=0;i<4;i++){
        bool hovered = (mx>=choiceRects[i].x && mx<=choiceRects[i].x+choiceRects[i].w &&
                        my>=choiceRects[i].y && my<=choiceRects[i].y+choiceRects[i].h);
        // draw tile background if available
        if (hovered && tileLight)
        {
            SDL_RenderCopy(renderer, tileLight, nullptr, &choiceRects[i]);
        }
        else if (!hovered && tileDark)
        {
            SDL_RenderCopy(renderer, tileDark, nullptr, &choiceRects[i]);
        }
        else
        {
            SDL_SetRenderDrawColor(renderer, hovered ? 240:200, hovered?240:200, hovered?100:200, 255);
            SDL_RenderFillRect(renderer,&choiceRects[i]);
        }
        if(font){
            int tw,th;
            SDL_Texture* t = createTextTexture(currentQuestion.choices[i],{0,0,0,255},tw,th);
            if(t){
                SDL_Rect dst={choiceRects[i].x+10, choiceRects[i].y + (choiceRects[i].h-th)/2, tw, th};
                SDL_RenderCopy(renderer,t,nullptr,&dst);
                SDL_DestroyTexture(t);
            }
        }
    }
}

bool HUD::handleEvent(const SDL_Event& event, bool& correct)
{
    if (!active) return false;
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button==SDL_BUTTON_LEFT)
    {
        int mx=event.button.x, my=event.button.y;
        for(int i=0;i<4;i++){
            if(mx>=choiceRects[i].x && mx<=choiceRects[i].x+choiceRects[i].w &&
               my>=choiceRects[i].y && my<=choiceRects[i].y+choiceRects[i].h)
            {
                char sel='a'+i;
                correct = (sel==currentQuestion.correct);
                active=false;
                return true;
            }
        }
    }
    if (event.type==SDL_KEYDOWN)
    {
        char sel=0;
        switch(event.key.keysym.sym){
            case SDLK_a: sel='a'; break;
            case SDLK_b: sel='b'; break;
            case SDLK_c: sel='c'; break;
            case SDLK_d: sel='d'; break;
        }
        if(sel){
            correct = (sel==currentQuestion.correct);
            active=false;
            return true;
        }
    }
    return false;
}

SDL_Texture* HUD::createTextTexture(const std::string& text, SDL_Color color, int& outW, int& outH)
{
    if(!font||!renderer) return nullptr;
    SDL_Surface* surf=TTF_RenderText_Blended_Wrapped(font, text.c_str(), color, 900);
    if(!surf) return nullptr;
    SDL_Texture* tex=SDL_CreateTextureFromSurface(renderer,surf);
    outW=surf->w; outH=surf->h;
    SDL_FreeSurface(surf);
    return tex;
}