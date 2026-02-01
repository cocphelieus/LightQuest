@echo off

g++ ^
src/main.cpp ^
src/core/Game.cpp ^
src/core/Window.cpp ^
src/scenes/MenuScene.cpp ^
-Ithird_party/include ^
-Lthird_party/lib ^
-lmingw32 ^
-lSDL2main ^
-lSDL2 ^
-o DarkMaze.exe

echo Build done!
pause
