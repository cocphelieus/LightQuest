#include "PlayScene.h"
#include <iostream>
#include <cstdlib>
#include <cstdio>

void PlayScene::syncTesterOverlay()
{
    map.setTesterOverlay(testerShowMines, testerShowTorches);
}

void PlayScene::openTesterPanel()
{
    bool keepOpen = true;

    while (keepOpen)
    {
        char statusText[512];
        std::snprintf(
            statusText,
            sizeof(statusText),
            "TESTER PANEL\n\n"
            "Trang thai hien tai:\n"
            "- Hien min: %s\n"
            "- Hien duoc: %s\n"
            "- Hien dap an cau hoi: %s\n\n"
            "Chon nut de bat/tat tung muc.",
            testerShowMines ? "BAT" : "TAT",
            testerShowTorches ? "BAT" : "TAT",
            testerShowAnswer ? "BAT" : "TAT"
        );

        SDL_MessageBoxButtonData buttons[4] = {
            {0, 0, "Toggle min"},
            {0, 1, "Toggle duoc"},
            {0, 2, "Toggle dap an"},
            {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 3, "Dong"}
        };

        SDL_MessageBoxData boxdata;
        boxdata.flags = SDL_MESSAGEBOX_INFORMATION;
        boxdata.window = nullptr;
        boxdata.title = "Tester";
        boxdata.message = statusText;
        boxdata.numbuttons = 4;
        boxdata.buttons = buttons;
        boxdata.colorScheme = nullptr;

        int buttonid = -1;
        if (SDL_ShowMessageBox(&boxdata, &buttonid) < 0)
            return;

        if (buttonid == 0)
            testerShowMines = !testerShowMines;
        else if (buttonid == 1)
            testerShowTorches = !testerShowTorches;
        else if (buttonid == 2)
            testerShowAnswer = !testerShowAnswer;
        else
            keepOpen = false;

        syncTesterOverlay();
    }
}

PlayScene::PlayScene()
{
}

void PlayScene::startLevel(Difficulty newDifficulty)
{
    difficulty = newDifficulty;
    map.reset(difficulty);
    map.activateStarterTorch();
    player.setPosition(map.getStartRow(), map.getStartCol());
    checkpointRow = map.getStartRow();
    checkpointCol = map.getStartCol();
    torchActivated = 0;
    testerShowMines = false;
    testerShowTorches = false;
    testerShowAnswer = false;
    syncTesterOverlay();
    outcome = PlayOutcome::NONE;
}

void PlayScene::load(SDL_Renderer* renderer)
{
    rendererRef = renderer;
    returnToMenu = false;

    std::cout << "PlayScene Loaded\n";
    map.load(renderer, "assets/images/background/map_level_1.png", difficulty);
    bool loadedQuestions = questionManager.loadFromFile("data/question.txt");
    startLevel(difficulty);

    if (!loadedQuestions)
    {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_WARNING,
            "Canh bao",
            "Khong doc duoc data/question.txt. He thong dang dung bo cau hoi du phong.",
            nullptr
        );
    }

    char introMessage[512];
    std::snprintf(
        introMessage,
        sizeof(introMessage),
        "WASD/Phim mui ten de di chuyen.\nBat dau chi mo rat it map + 1 duoc khoi dong chua mo.\nCac duoc duoc dat co dinh tu dau man.\nBan phai tu do tim duoc tiep theo.\nDung thi mo duoc, sai thi duoc bi khoa.\nMuc tieu man nay: toi dich de chien thang."
    );

    SDL_ShowSimpleMessageBox(
        SDL_MESSAGEBOX_INFORMATION,
        "LightQuest",
        introMessage,
        nullptr
    );
}

void PlayScene::handleEvent(SDL_Event &event)
{
    if (event.type == SDL_KEYDOWN)
    {
        if (event.key.keysym.sym == SDLK_t)
        {
            openTesterPanel();
            return;
        }

        if (event.key.keysym.sym == SDLK_ESCAPE)
        {
            outcome = PlayOutcome::EXITED;
            returnToMenu = true;
            return;
        }
    }

    int oldRow = player.getRow();
    int oldCol = player.getCol();

    player.handleEvent(event, map);

    if (oldRow != player.getRow() || oldCol != player.getCol())
    {
        onPlayerMoved(oldRow, oldCol);
    }
}

void PlayScene::onPlayerMoved(int oldRow, int oldCol)
{
    int row = player.getRow();
    int col = player.getCol();

    bool enteredDarkTile = !map.isKnown(row, col);

    if (map.isMine(row, col))
    {
        outcome = PlayOutcome::FAILED;
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR,
            "That bai",
            "Ban dam trung min. Thu lai tu dau!",
            nullptr
        );
        returnToMenu = true;
        return;
    }

    if (map.isGoal(row, col))
    {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_INFORMATION,
            "Chuc mung",
            "Ban da den dich va chien thang!",
            nullptr
        );
        outcome = PlayOutcome::CLEARED;
        returnToMenu = true;
        return;
    }

    if (enteredDarkTile)
    {
        map.revealAt(row, col);

        if (!map.canAttemptTorch(row, col))
        {
            checkpointRow = row;
            checkpointCol = col;
            return;
        }
    }

    if (map.canAttemptTorch(row, col))
    {
        bool correct = false;
        bool answeredQuestion = false;

        if (questionManager.hasQuestions())
        {
            int questionIndex = questionManager.randomIndex();
            correct = questionManager.askQuestion(questionIndex, testerShowAnswer);
            answeredQuestion = true;
        }

        if (!answeredQuestion)
        {
            map.resolveTorch(row, col, false);
            player.setPosition(oldRow, oldCol);
            SDL_ShowSimpleMessageBox(
                SDL_MESSAGEBOX_WARNING,
                "Khong co cau hoi",
                "Duoc nay bi khoa vi he thong cau hoi khong hop le.",
                nullptr
            );
            return;
        }

        map.resolveTorch(row, col, correct);

        if (correct)
        {
            torchActivated++;
            map.clearTorchHint();
            map.revealPathToNextTorch(row, col, torchActivated);
            checkpointRow = row;
            checkpointCol = col;
        }
        else
        {
            map.hintNearestTorch(row, col);
            player.setPosition(oldRow, oldCol);
        }

    }
    else
    {
        checkpointRow = row;
        checkpointCol = col;
    }
}

void PlayScene::update(float deltaTime)
{
    (void)deltaTime;
}

void PlayScene::render(SDL_Renderer *renderer)
{
    int offsetX = map.getRenderOffsetX();
    int offsetY = map.getRenderOffsetY();

    map.render(renderer);
    player.render(renderer, offsetX, offsetY, map.getTileSize());
}

void PlayScene::clean()
{
    map.clean();
    rendererRef = nullptr;
}
