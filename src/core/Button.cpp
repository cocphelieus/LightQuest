#include "Button.h"
#include <SDL2/SDL_image.h>

Button::Button(int x, int y, int width, int height, const std::string& imagePath)
    : imagePath(imagePath) {
    baseRect.x = x;
    baseRect.y = y;
    baseRect.w = width;
    baseRect.h = height;

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
        // Scale around the center to make hover states feel more alive.
        int scaledW = static_cast<int>(static_cast<float>(baseRect.w) * hoverScale);
        int scaledH = static_cast<int>(static_cast<float>(baseRect.h) * hoverScale);
        rect.w = scaledW;
        rect.h = scaledH;
        rect.x = baseRect.x + (baseRect.w - scaledW) / 2;
        rect.y = baseRect.y + (baseRect.h - scaledH) / 2;
        SDL_RenderCopy(renderer, texture, nullptr, &rect);
    }
}

void Button::update(int mouseX, int mouseY) {
    isHovered = (mouseX >= baseRect.x && mouseX <= baseRect.x + baseRect.w &&
                 mouseY >= baseRect.y && mouseY <= baseRect.y + baseRect.h);

    float targetScale = isHovered ? 1.08f : 1.0f;
    float speed = 0.20f;
    hoverScale += (targetScale - hoverScale) * speed;
}

void Button::clean() {
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
}

bool Button::isClicked(int mouseX, int mouseY) const {
    return mouseX >= baseRect.x && mouseX <= baseRect.x + baseRect.w &&
           mouseY >= baseRect.y && mouseY <= baseRect.y + baseRect.h;
}

void Button::setPosition(int x, int y) {
    baseRect.x = x;
    baseRect.y = y;
    rect.x = x;
    rect.y = y;
}
