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
    bool playDeathSequence(SDL_Renderer* renderer);
    void setDifficulty(Difficulty value) { difficulty = value; }
    void setCampaignStage(int value) { campaignStage = (value < 0) ? 0 : value; }
    void setCampaignElapsedSeconds(int value) { campaignElapsedSeconds = (value < 0) ? 0 : value; }
    bool shouldReturnToMenu() const { return returnToMenu; }
    PlayOutcome getOutcome() const { return outcome; }

private:
    MapManager map;
    Player player;
    QuestionManager questionManager;

    SDL_Renderer* rendererRef = nullptr;
    Difficulty difficulty = Difficulty::MEDIUM;
    int campaignStage = 0;
    bool returnToMenu = false;
    int torchActivated = 0;
    int checkpointRow = 0;
    int checkpointCol = 0;
    bool testerShowMines = false;
    bool testerShowTorches = false;
    bool testerShowAnswer = false;
    PlayOutcome outcome = PlayOutcome::NONE;
    int campaignElapsedSeconds = 0;
    Uint32 levelStartTick = 0;
    Uint32 levelIntroUntilTick = 0;
    void* uiFontRaw = nullptr;
    bool ttfInitByScene = false;

    void startLevel(Difficulty newDifficulty);
    void onPlayerMoved(int oldRow, int oldCol);
    void openTesterPanel();
    void syncTesterOverlay();
    void renderOverlay(SDL_Renderer* renderer);
};
