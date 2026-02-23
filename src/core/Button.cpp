#include "Button.h"
#include <SDL2/SDL_image.h>

Button::Button(int x, int y, int width, int height, const std::string& imagePath)
    : imagePath(imagePath) {
    rect.x = x;
    rect.y = y;
    rect.w = width;
    rect.h = height;
}

Button::~Button() {
    clean();
}

bool Button::load(SDL_Renderer* renderer) {

    texture = IMG_LoadTexture(renderer, imagePath.c_str());

    if (!texture) {
        SDL_Log("Failed to load button: %s - %s",
                imagePath.c_str(),
                IMG_GetError());
        return false;
    }

    return true;
}

void Button::render(SDL_Renderer* renderer) {
    if (texture) {
        SDL_RenderCopy(renderer, texture, nullptr, &rect);
    }
}

void Button::update(int mouseX, int mouseY) {
    isHovered = isClicked(mouseX, mouseY);
}

void Button::clean() {
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
}

bool Button::isClicked(int mouseX, int mouseY) const {
    return mouseX >= rect.x && mouseX <= rect.x + rect.w &&
           mouseY >= rect.y && mouseY <= rect.y + rect.h;
}

void Button::setPosition(int x, int y) {
    rect.x = x;
    rect.y = y;
}
