#pragma once
#include "Window.h"
#include "../scenes/MenuScene.h"

class Game {
public:
    void run();

private:
    bool running = true;
    Window window;
    MenuScene menu;
};
