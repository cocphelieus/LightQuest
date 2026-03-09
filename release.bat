@echo off
setlocal EnableExtensions EnableDelayedExpansion

echo [1/4] Building LightQuest...
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
-Wall

if errorlevel 1 (
  echo.
  echo Build failed. Release package was not created.
  exit /b 1
)

set "RELEASE_DIR=dist\LightQuest-Player"

echo [2/4] Preparing release folder...
if exist "%RELEASE_DIR%" rmdir /s /q "%RELEASE_DIR%"
mkdir "%RELEASE_DIR%"

copy /y "LightQuest.exe" "%RELEASE_DIR%\" >nul
xcopy /e /i /y "assets" "%RELEASE_DIR%\assets" >nul
xcopy /e /i /y "data" "%RELEASE_DIR%\data" >nul

echo [3/5] Locking question data...
call :lock_question_file
if errorlevel 1 (
  echo.
  echo Failed to lock data\question.txt for release.
  exit /b 1
)

echo [4/5] Collecting runtime DLLs...
call :copy_from_where SDL2.dll
call :copy_from_where SDL2_image.dll
call :copy_from_where SDL2_ttf.dll
call :copy_from_where libgcc_s_seh-1.dll
call :copy_from_where libstdc++-6.dll
call :copy_from_where libwinpthread-1.dll
call :copy_from_where zlib1.dll
call :copy_from_where libpng16-16.dll
call :copy_from_where freetype-6.dll
call :copy_from_where harfbuzz-0.dll
call :copy_from_where bz2-1.dll

set "MISSING="
if not exist "%RELEASE_DIR%\SDL2.dll" set "MISSING=!MISSING! SDL2.dll"
if not exist "%RELEASE_DIR%\SDL2_image.dll" set "MISSING=!MISSING! SDL2_image.dll"
if not exist "%RELEASE_DIR%\SDL2_ttf.dll" set "MISSING=!MISSING! SDL2_ttf.dll"

echo [5/5] Writing quick start file...
(
  echo LightQuest Player Build
  echo ======================
  echo 1. Double-click LightQuest.exe to play.
  echo 2. If the game does not start, a runtime DLL is missing.
  echo 3. Keep assets and data folders next to the exe.
) > "%RELEASE_DIR%\README.txt"

if defined MISSING (
  echo.
  echo Package created at: %RELEASE_DIR%
  echo WARNING: Missing required DLLs:%MISSING%
  echo Please copy them manually next to LightQuest.exe before sharing.
  exit /b 2
)

echo.
echo Package created successfully at: %RELEASE_DIR%
echo Share the whole folder or zip it as LightQuest-Player.zip.
exit /b 0

:lock_question_file
set "QUESTION_FILE=%RELEASE_DIR%\data\question.txt"
if not exist "%QUESTION_FILE%" (
  echo Missing %QUESTION_FILE%
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
"$ErrorActionPreference = 'Stop'; ^
$file = '%QUESTION_FILE%'; ^
$key = 'LQ_2026_PlayerLock'; ^
$plain = [System.IO.File]::ReadAllText($file, [System.Text.Encoding]::UTF8); ^
$payload = 'LQPLAIN1`n' + $plain; ^
$bytes = [System.Text.Encoding]::UTF8.GetBytes($payload); ^
$keyBytes = [System.Text.Encoding]::UTF8.GetBytes($key); ^
for ($i = 0; $i -lt $bytes.Length; $i++) { $bytes[$i] = $bytes[$i] -bxor $keyBytes[$i %% $keyBytes.Length]; } ^
$hex = -join ($bytes | ForEach-Object { $_.ToString('X2') }); ^
[System.IO.File]::WriteAllText($file, 'LQLOCK1`n' + $hex, [System.Text.Encoding]::UTF8);"

if errorlevel 1 exit /b 1
exit /b 0

:copy_from_where
set "DLL_NAME=%~1"
for /f "delims=" %%I in ('where %DLL_NAME% 2^>nul') do (
  copy /y "%%~fI" "%RELEASE_DIR%\" >nul
  goto :eof
)
goto :eof
