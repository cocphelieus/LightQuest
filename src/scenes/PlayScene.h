#pragma once
#include <SDL2/SDL.h>
#include "../map/MapManager.h"
#include "../entities/Player.h"
#include "../core/QuestionManager.h"

enum class PlayOutcome
{
    NONE,
    CLEARED,
    FAILED,
    EXITED
};

class PlayScene
{
public:
    PlayScene();

    void load(SDL_Renderer* renderer);
    void handleEvent(SDL_Event& event);
    void update(float deltaTime);
    void render(SDL_Renderer* renderer);
    void clean();
    void setDifficulty(Difficulty value) { difficulty = value; }
    bool shouldReturnToMenu() const { return returnToMenu; }
    PlayOutcome getOutcome() const { return outcome; }

private:
    MapManager map;
    Player player;
    QuestionManager questionManager;

    SDL_Renderer* rendererRef = nullptr;
    Difficulty difficulty = Difficulty::MEDIUM;
    bool returnToMenu = false;
    int torchActivated = 0;
    int checkpointRow = 0;
    int checkpointCol = 0;
    bool testerShowMines = false;
    bool testerShowTorches = false;
    bool testerShowAnswer = false;
    PlayOutcome outcome = PlayOutcome::NONE;

    void startLevel(Difficulty newDifficulty);
    void onPlayerMoved(int oldRow, int oldCol);
    void openTesterPanel();
    void syncTesterOverlay();
};
