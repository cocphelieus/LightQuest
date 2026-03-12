#pragma once
#include <SDL2/SDL.h>
#include "../map/MapManager.h"
#include "../entities/Player.h"
#include "../core/QuestionManager.h"

enum class PlayOutcome
{
    // Player is still in active gameplay.
    NONE,
    // Stage objective reached.
    CLEARED,
    // Player failed (usually mine contact).
    FAILED,
    // Player pressed ESC to leave gameplay.
    EXITED
};

class PlayScene
{
public:
    PlayScene();

    // Full scene setup: map, player, question pool, fonts.
    void load(SDL_Renderer* renderer);
    // Discrete input events (ESC/tester shortcuts).
    void handleEvent(SDL_Event& event);
    // Frame update loop (movement + gameplay resolution).
    void update(float deltaTime);
    // Render map, player, and HUD overlay.
    void render(SDL_Renderer* renderer);
    // Release scene resources.
    void clean();
    // Play special visual sequence before game-over screen.
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
    // Number of successfully solved torch events in current stage.
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
    // Prevent accidental movement carry-over right after stage load.
    bool blockMovementUntilRelease = false;
    void* uiFontRaw = nullptr;
    bool ttfInitByScene = false;

    // Reset map/player/tester flags for a new stage.
    void startLevel(Difficulty newDifficulty);
    // Resolve gameplay consequences when player changes tile.
    void onPlayerMoved(int oldRow, int oldCol);
    void openTesterPanel();
    void syncTesterOverlay();
    // Top-left runtime HUD (stage/difficulty/time/help text).
    void renderOverlay(SDL_Renderer* renderer);
};
