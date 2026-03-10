@echo off

echo Building LightQuest...

REM Avoid linker "Permission denied" when previous game instance is still running.
taskkill /f /im LightQuest.exe >nul 2>&1
if exist "LightQuest.exe" del /f /q "LightQuest.exe" >nul 2>&1

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
-o LightQuest.exe ^
-lmingw32 ^
-lSDL2main ^
-lSDL2 ^
-lSDL2_image ^
-lSDL2_ttf ^
-static-libgcc ^
-static-libstdc++ ^
-Wall

if %errorlevel% neq 0 (
    echo.
    echo Build failed!
    pause
    exit /b
)

echo.
echo Build successful!
echo Run: LightQuest.exe
pause
