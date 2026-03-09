@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
pushd "%SCRIPT_DIR%" >nul

echo [1/3] Building LightQuest...
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

if errorlevel 1 (
  echo.
  echo Build failed. Release package was not created.
  popd >nul
  exit /b 1
)

set "RELEASE_DIR=dist\LightQuest-Player"

echo [2/3] Preparing release folder...
if exist "%RELEASE_DIR%" rmdir /s /q "%RELEASE_DIR%"
mkdir "%RELEASE_DIR%"

copy /y "LightQuest.exe" "%RELEASE_DIR%\" >nul
xcopy /e /i /y "assets" "%RELEASE_DIR%\assets" >nul
xcopy /e /i /y "data" "%RELEASE_DIR%\data" >nul

echo [3/3] Locking question data and writing README...
call :lock_question_file
if errorlevel 1 (
  echo.
  echo Failed to lock data\question.txt for release.
  popd >nul
  exit /b 1
)

(
  echo LightQuest Player Build
  echo ======================
  echo This package includes only the game files.
  echo If player PC is missing libraries, install SDL2 runtime first.
  echo.
  echo Required DLL files for LightQuest.exe:
  echo - SDL2.dll
  echo - SDL2_image.dll
  echo - SDL2_ttf.dll
  echo.
  echo Optional dependencies sometimes required by SDL2_image:
  echo - zlib1.dll, libpng16-16.dll, libjpeg-9.dll
  echo - libwebp-7.dll, libtiff-6.dll, libavif-16.dll
  echo.
  echo How to install on player PC:
  echo 1. Install SDL2, SDL2_image, SDL2_ttf runtime DLLs.
  echo 2. Copy required DLLs into the same folder as LightQuest.exe.
  echo 3. Keep assets and data folders next to LightQuest.exe.
  echo.
  echo If game cannot start, Windows error will show missing DLL name.
) > "%RELEASE_DIR%\README.txt"

echo.
echo Package created successfully at: %RELEASE_DIR%
echo Creating zip archive...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
"$zip = Join-Path (Get-Location) 'dist\\LightQuest-Player.zip'; ^
if (Test-Path $zip) { Remove-Item -Force $zip; } ^
Compress-Archive -Path 'dist\\LightQuest-Player\\*' -DestinationPath $zip;"

if errorlevel 1 (
  echo Failed to create zip. Folder package is still available.
  popd >nul
  exit /b 3
)

echo Zip created: dist\LightQuest-Player.zip
echo Share this zip file with the README instructions.
popd >nul
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
$hexBuilder = New-Object System.Text.StringBuilder; ^
foreach ($b in $bytes) { [void]$hexBuilder.AppendFormat('{0:X2}', $b); } ^
$hex = $hexBuilder.ToString(); ^
[System.IO.File]::WriteAllText($file, 'LQLOCK1`n' + $hex, [System.Text.Encoding]::UTF8);"

if errorlevel 1 exit /b 1
exit /b 0
