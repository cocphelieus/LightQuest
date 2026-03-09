#pragma once
#include <SDL2/SDL.h>
#include <string>

class Button {
public:
    Button(int x, int y, int width, int height, const std::string& imagePath);
    ~Button();
    
    bool load(SDL_Renderer* renderer);
    void update(int mouseX, int mouseY);
    void render(SDL_Renderer* renderer);
    void clean();
    
    bool isClicked(int mouseX, int mouseY) const;
    void setPosition(int x, int y);
    
    SDL_Rect getRect() const { return rect; }
    bool getHovered() const { return isHovered; }

private:
    SDL_Rect baseRect;
    SDL_Rect rect;
    SDL_Texture* texture = nullptr;
    std::string imagePath;
    bool isHovered = false;
    float hoverScale = 1.0f;
};
