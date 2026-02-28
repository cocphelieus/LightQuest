#include "PlayScene.h"
#include <iostream>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include "../quiz/HUD.h"
#include "../core/Asset.h"

PlayScene::PlayScene()
{
}

void PlayScene::resetLevel()
{
    map = MapManager(mapRows, mapCols, mapTorches, mapMines);
    map.load(renderer, ASSET("background/map_level_1.png"));
    player.clean(); // destroy existing texture if any
    player = Player();
    if (renderer)
    {
        player.load(renderer, ASSET("entities/bot.png"));
    }
    int tileSize = MapManager::TILE_SIZE;
    player.setPosition(1, 1, tileSize);
    gameOver = false;
    hasWon = false;
    if (hud) hud->hide();
}
void PlayScene::load(SDL_Renderer *renderer)
{
    std::cout << "PlayScene Loaded\n";
    this->renderer = renderer;

    // recreate map with chosen parameters
    map = MapManager(mapRows, mapCols, mapTorches, mapMines);
    map.load(renderer, ASSET("background/map_level_1.png"));

    player.load(renderer, ASSET("entities/bot.png"));   

    int tileSize = MapManager::TILE_SIZE;
    player.setPosition(1, 1, tileSize);
    gameOver = false;

    // initialize HUD for questions
    if (hud) { hud->clean(); delete hud; }
    hud = new HUD(renderer);
    if (!hud->init()) {
        std::cout << "PlayScene: HUD init failed, disabling in-game questions\n";
        delete hud;
        hud = nullptr;
    }
}
void PlayScene::handleEvent(SDL_Event &event)
{
    // if HUD is active process it first
    if (hud && hud->isActive())
    {
        bool correct = false;
        if (hud->handleEvent(event, correct))
        {
            int pr = player.getRow();
            int pc = player.getCol();
            map.activateTorch(pr, pc, correct);
        }
        return; // consume all other events while question shown
    }

    if (event.type == SDL_KEYDOWN)
    {
        if (event.key.keysym.sym == SDLK_ESCAPE)
        {
            if (gameOver)
            {
                std::cout << "Returning to main menu...\n";
                // TODO: signal scene manager to switch to menu
            }
            else
            {
                std::cout << "ESC pressed\n";
            }
        }
        else if (gameOver && (event.key.keysym.sym == SDLK_r))
        {
            std::cout << "Restarting level...\n";
            resetLevel();
        }
    }

    if (!gameOver)
        player.handleEvent(event, map);
}

void PlayScene::update(float deltaTime)
{
    if (gameOver)
        return;

    // update player only if HUD not blocking
    if (!(hud && hud->isActive()))
        player.update(deltaTime, map);

    // if player is alive and on a torch, trigger HUD question
    if (player.isAlive() && hud && !hud->isActive())
    {
        int pr = player.getRow();
        int pc = player.getCol();
        if (map.isTorch(pr, pc) && !map.torchActivatedAt(pr, pc))
        {
            hud->showQuestion(qManager.getRandomQuestion());
        }
    }

    if (!player.isAlive())
    {
        gameOver = true;
        hasWon = false;
        std::cout << "Game over! Press ESC to return to menu or R to restart.\n";
    }
    // check if reached goal
    if (player.isAlive() && player.getRow() == map.getRows()-2 && player.getCol() == map.getCols()-2)
    {
        gameOver = true;
        hasWon = true;
        std::cout << "Congratulations! You reached the goal! Press ESC or R to restart.\n";
    }
}

void PlayScene::render(SDL_Renderer *renderer)
{
    int mapWidth = map.getCols() * MapManager::TILE_SIZE;
    int mapHeight = map.getRows() * MapManager::TILE_SIZE;

    int screenWidth = 1280;
    int screenHeight = 720;

    int offsetX = (screenWidth - mapWidth) / 2;
    int offsetY = (screenHeight - mapHeight) / 2;

    map.render(renderer);

    player.render(renderer, offsetX, offsetY);

    if (hud && hud->isActive())
    {
        hud->render();
    }

    if (gameOver)
    {
        // draw a semi-transparent overlay
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
        SDL_Rect cover = {0,0,1280,720};
        SDL_RenderFillRect(renderer,&cover);
        // future: render message based on hasWon/gameOver
    }
}

void PlayScene::clean()
{
    player.clean();
    if (hud) { hud->clean(); delete hud; hud = nullptr; }
    // old textures/fonts removed when HUD cleaned
}

