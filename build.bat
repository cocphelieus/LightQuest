@echo off

echo Building LightQuest...

set "SOUND_FLAGS="
set "MIXER_LIB_DIR="
for %%D in (
    "C:\msys64\mingw64\lib"
    "C:\msys64\ucrt64\lib"
    "C:\msys64\clang64\lib"
) do (
    if exist "%%~fD\libSDL2_mixer.a" (
        set "MIXER_LIB_DIR=%%~fD"
    )
)

if defined MIXER_LIB_DIR (
    set "SOUND_FLAGS=-DLIGHTQUEST_ENABLE_SOUND -L\"%MIXER_LIB_DIR%\" -lSDL2_mixer"
)

if defined SOUND_FLAGS (
    echo SDL2_mixer detected. Sound system: ENABLED.
) else (
    echo SDL2_mixer not found. Sound system: DISABLED.
)

REM Avoid linker "Permission denied" when previous game instance is still running.
taskkill /f /im LightQuest.exe >nul 2>&1
if exist "LightQuest.exe" del /f /q "LightQuest.exe" >nul 2>&1

g++ ^
src/main.cpp ^
src/core/Game.cpp ^
src/core/SoundManager.cpp ^
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
%SOUND_FLAGS% ^
-static-libgcc ^
-static-libstdc++ ^
-mwindows ^
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
