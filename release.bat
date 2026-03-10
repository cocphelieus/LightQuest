@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
pushd "%SCRIPT_DIR%" >nul

echo [1/3] Building LightQuest...

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

echo [3/4] Locking question data and preparing runtime...
call :lock_question_file
if errorlevel 1 (
  echo.
  echo Failed to lock data\question.txt for release.
  popd >nul
  exit /b 1
)

call :bundle_runtime_dlls
if errorlevel 1 (
  echo.
  echo Failed to copy required SDL runtime DLLs.
  popd >nul
  exit /b 1
)

call :build_setup_exe
if errorlevel 1 (
  echo.
  echo Failed to create setup installer.
  popd >nul
  exit /b 1
)

echo Writing README...

(
  echo LightQuest Player Build
  echo ======================
  echo Output setup file: dist\LightQuest-Setup.exe
  echo This build includes game files and runtime DLLs.
  echo Player PC does NOT need SDL2 pre-installed.
  echo.
  echo Installer lets player choose install directory like typical game setup.
  echo All required SDL runtime DLLs are bundled into installer.
  echo.
  echo How to install on player PC:
  echo 1. Run LightQuest-Setup.exe
  echo 2. Choose install location.
  echo 3. Launch from desktop/start menu shortcut.
  echo.
  echo Question file is locked for anti-cheat in release build.
) > "%RELEASE_DIR%\README.txt"

echo.
echo Package folder created at: %RELEASE_DIR%
echo Installer created at: dist\LightQuest-Setup.exe
echo Share only LightQuest-Setup.exe to players.
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
$payload = 'LQPLAIN1' + [System.Environment]::NewLine + $plain; ^
$bytes = [System.Text.Encoding]::UTF8.GetBytes($payload); ^
$keyBytes = [System.Text.Encoding]::UTF8.GetBytes($key); ^
for ($i = 0; $i -lt $bytes.Length; $i++) { $bytes[$i] = $bytes[$i] -bxor $keyBytes[$i %% $keyBytes.Length]; } ^
$hexBuilder = New-Object System.Text.StringBuilder; ^
foreach ($b in $bytes) { [void]$hexBuilder.AppendFormat('{0:X2}', $b); } ^
$hex = $hexBuilder.ToString(); ^
[System.IO.File]::WriteAllText($file, ('LQLOCK1' + [System.Environment]::NewLine + $hex), [System.Text.Encoding]::UTF8);"

if errorlevel 1 exit /b 1
exit /b 0

:bundle_runtime_dlls
set "SDL_BIN="
for %%D in ("%MINGW_BIN%" "C:\msys64\mingw64\bin" "C:\msys64\ucrt64\bin" "C:\MinGW\bin") do (
  if not defined SDL_BIN if exist "%%~D\SDL2.dll" set "SDL_BIN=%%~D"
)

if not defined SDL_BIN (
  echo Could not find SDL runtime folder. Checked MINGW_BIN and common paths.
  exit /b 1
)

echo Using SDL runtime from: %SDL_BIN%

call :copy_required_dll SDL2.dll || exit /b 1
call :copy_required_dll SDL2_image.dll || exit /b 1
call :copy_required_dll SDL2_ttf.dll || exit /b 1

call :copy_optional_dll libfreetype-6.dll
call :copy_optional_dll libharfbuzz-0.dll
call :copy_optional_dll libpng16-16.dll
call :copy_optional_dll zlib1.dll
call :copy_optional_dll libjpeg-8.dll
call :copy_optional_dll libwebp-7.dll
call :copy_optional_dll libwebpdecoder-3.dll
call :copy_optional_dll libtiff-6.dll
call :copy_optional_dll libavif-16.dll
call :copy_optional_dll libbz2-1.dll
call :copy_optional_dll libbrotlidec.dll
call :copy_optional_dll libbrotlicommon.dll
call :copy_optional_dll libdeflate.dll
call :copy_optional_dll libiconv-2.dll
call :copy_optional_dll libintl-8.dll
call :copy_optional_dll liblzma-5.dll
call :copy_optional_dll libwinpthread-1.dll
call :copy_optional_dll libgcc_s_seh-1.dll
call :copy_optional_dll libstdc++-6.dll

exit /b 0

:copy_required_dll
if not exist "%SDL_BIN%\%~1" (
  echo Missing required runtime DLL: %~1
  exit /b 1
)
copy /y "%SDL_BIN%\%~1" "%RELEASE_DIR%\" >nul
if errorlevel 1 exit /b 1
exit /b 0

:copy_optional_dll
if exist "%SDL_BIN%\%~1" (
  copy /y "%SDL_BIN%\%~1" "%RELEASE_DIR%\" >nul
)
exit /b 0

:find_iscc
set "ISCC_PATH="
for %%I in (
  "%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"
  "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
  "%ProgramFiles%\Inno Setup 6\ISCC.exe"
) do (
  if not defined ISCC_PATH if exist "%%~I" set "ISCC_PATH=%%~I"
)

if not defined ISCC_PATH (
  for /f "delims=" %%I in ('where ISCC.exe 2^>nul') do (
    if not defined ISCC_PATH set "ISCC_PATH=%%~fI"
  )
)
exit /b 0

:build_setup_exe
call :find_iscc
if not defined ISCC_PATH (
  echo Inno Setup compiler not found. Attempting install via winget...
  winget install --id JRSoftware.InnoSetup -e --accept-package-agreements --accept-source-agreements --silent >nul 2>&1
  call :find_iscc
)

if not defined ISCC_PATH (
  echo Could not find ISCC.exe. Install Inno Setup 6 and run release.bat again.
  exit /b 1
)

set "ISS_FILE=%RELEASE_DIR%\LightQuest-Setup.iss"
set "SETUP_OUT_DIR=%CD%\dist"

(
  echo [Setup]
  echo AppId={{9A66A4D5-2C27-4F7D-8D14-09D6D56D53A9}
  echo AppName=LightQuest
  echo AppVersion=1.0.0
  echo DefaultDirName={localappdata}\Programs\LightQuest
  echo DisableProgramGroupPage=yes
  echo OutputDir=%SETUP_OUT_DIR%
  echo OutputBaseFilename=LightQuest-Setup
  echo Compression=lzma
  echo SolidCompression=yes
  echo WizardStyle=modern
  echo PrivilegesRequired=lowest
  echo UninstallDisplayIcon={app}\LightQuest.exe
  echo.
  echo [Languages]
  echo Name: "english"; MessagesFile: "compiler:Default.isl"
  echo.
  echo [Tasks]
  echo Name: "desktopicon"; Description: "Create desktop shortcut"; GroupDescription: "Additional icons:"; Flags: unchecked
  echo.
  echo [Files]
  echo Source: "%CD%\%RELEASE_DIR%\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
  echo.
  echo [Icons]
  echo Name: "{autoprograms}\LightQuest"; Filename: "{app}\LightQuest.exe"
  echo Name: "{autodesktop}\LightQuest"; Filename: "{app}\LightQuest.exe"; Tasks: desktopicon
  echo.
  echo [Run]
  echo Filename: "{app}\LightQuest.exe"; Description: "Launch LightQuest"; Flags: nowait postinstall skipifsilent
) > "%ISS_FILE%"

if errorlevel 1 exit /b 1

"%ISCC_PATH%" "%ISS_FILE%" >nul
if errorlevel 1 exit /b 1
if not exist "dist\LightQuest-Setup.exe" exit /b 1

exit /b 0
