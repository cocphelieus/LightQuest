@echo off

echo Building LightQuest...

set "TESTER_DEFINE="
if "%ENABLE_TESTER%"=="1" (
    echo Tester mode: ON
    set "TESTER_DEFINE=-DLQ_ENABLE_TESTER"
) else (
    echo Tester mode: OFF
)

g++ ^
src/main.cpp ^
src/core/Game.cpp ^
src/core/Window.cpp ^
src/core/Button.cpp ^
src/core/HUD.cpp ^
src/scenes/MenuScene.cpp ^
src/scenes/LoadingScene.cpp ^
src/scenes/PlayScene.cpp ^
src/map/MapManager.cpp ^
src/entities/Player.cpp ^
src/core/QuestionManager.cpp ^
%TESTER_DEFINE% ^
-o LightQuest.exe ^
-lmingw32 ^
-lSDL2main ^
-lSDL2 ^
-lSDL2_image ^
-lSDL2_ttf ^
-Wall

if %errorlevel% neq 0 (
    echo.
    echo Build failed!
    pause
    exit /b
)

echo.
echo Build successful!
pause
