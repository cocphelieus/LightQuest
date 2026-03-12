# LightQuest

A C++ quiz game built with SDL2, designed to create an interactive learning experience through engaging gameplay mechanics.

## Developer Documentation

For code walkthrough, module responsibilities, data formats, and maintenance notes, see:

- docs/DEVELOPER_DOCS.md

## About the Project

**LightQuest** is an educational quiz game where players navigate through game scenes, answer questions, and compete for high scores. The game features:

- **Multiple Game States**: Loading, Menu, and Playing scenes
- **Interactive Quiz System**: Dynamic question loading from external files
- **Player Progression**: Track scores in a ranking system
- **Rich Media Assets**: Supports images (AVIF, PNG) and fonts for enhanced visual experience
- **Sound Support**: Integrated audio capabilities for immersive gameplay
- **Responsive UI**: Custom button system with HUD (Heads-Up Display)
- **Cross-Platform Building**: Uses MSYS2 and MinGW for Windows compilation

## Technologies & Libraries

### Core Technology Stack
- **Language**: C++ (C++11 and above)
- **Build System**: Batch scripts (build.bat, release.bat)
- **Compiler**: MinGW (GCC) via MSYS2

### Graphics & Rendering
- **SDL2** (Simple DirectMedia Layer 2) - Core graphics, input handling, and windowing
- **SDL2_image** - Image loading and manipulation (supports PNG, JPG, BMP, etc.)

### Text Rendering
- **SDL2_ttf** - TrueType font rendering for in-game text

### Additional Features
- **Static Linking**: Libraries are statically linked for portability
- **Unicode Support**: Full support for text rendering via SDL2_ttf

## Project Structure

```
LightQuest/
├── src/                          # Source code
│   ├── main.cpp                  # Entry point
│   ├── config/
│   │   └── TesterConfig.h       # Testing configuration
│   ├── core/                     # Core engine components
│   │   ├── Game.cpp/.h          # Main game loop and state management
│   │   ├── Window.cpp/.h        # SDL2 window setup and rendering
│   │   ├── Button.cpp/.h        # UI button implementation
│   │   ├── HUD.cpp/.h           # Heads-Up Display system
│   │   ├── QuestionManager.cpp/.h  # Quiz question loading and display
│   ├── entities/                 # Game entities
│   │   ├── Player.cpp/.h        # Player character class
│   ├── map/                      # Map system
│   │   ├── MapManager.cpp/.h    # Map/level management
│   ├── scenes/                   # Game scenes
│   │   ├── LoadingScene.cpp/.h  # Loading screen
│   │   ├── MenuScene.cpp/.h     # Main menu interface
│   │   └── PlayScene.cpp/.h     # Main gameplay scene
│
├── assets/                       # Game resources
│   ├── fonts/                    # TrueType font files (.ttf)
│   ├── images/
│   │   ├── background/          # Background images
│   │   ├── button/              # Button UI graphics
│   │   └── entities/            # Entity sprites (player, logo, etc.)
│   └── sound/                    # Audio files
│
├── data/                         # Game data files
│   ├── question.txt             # Quiz questions database
│   └── ranking.txt              # Player rankings/scores
│
├── local/                        # Local development files
│   └── Tester.local.h           # Local testing header
│
├── tools/                        # Build and utility tools
│   ├── BundleRuntimeDlls.ps1   # Bundle DLLs for release
│   └── MakeIcoFromPng.ps1      # Convert PNG to ICO format
│
├── build.bat                     # Development build script
└── release.bat                   # Release build script
```

## Getting Started

### Prerequisites

Before building LightQuest, ensure you have MSYS2 with MinGW and SDL2 libraries installed.

### Step 1: Install MSYS2

1. Download MSYS2 from [https://www.msys2.org/](https://www.msys2.org/)
2. Install to a location like `C:\msys64`
3. Run MSYS2 and update the package database:
   ```bash
   pacman -Syu
   ```

### Step 2: Install SDL2 and Dependencies

Open MSYS2 MinGW 64-bit terminal and install the required libraries:

```bash
# Install GCC compiler toolchain
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-make

# Install SDL2 libraries
pacman -S mingw-w64-x86_64-SDL2
pacman -S mingw-w64-x86_64-SDL2_image
pacman -S mingw-w64-x86_64-SDL2_ttf
```

**Alternative for 32-bit:**
```bash
pacman -S mingw-w64-i686-gcc mingw-w64-i686-make
pacman -S mingw-w64-i686-SDL2
pacman -S mingw-w64-i686-SDL2_image
pacman -S mingw-w64-i686-SDL2_ttf
```

### Step 3: Configure PATH (Optional)

To build from regular PowerShell/Command Prompt (not MSYS2 terminal), add MSYS2 to your PATH:

1. Open Environment Variables
2. Add `C:\msys64\mingw64\bin` (or `mingw32\bin` for 32-bit) to your PATH
3. Restart your terminal/IDE


### Step 5: Build and Run

#### Development Build

Open Command Prompt or PowerShell in the LightQuest directory and run:

```cmd
build.bat
```

This will:
- Terminate any running LightQuest instance
- Compile all source files with g++ compiler
- Link SDL2 libraries
- Generate `LightQuest.exe`
- Display build status

#### Release Build

For a release build (typically with optimizations):

```cmd
release.bat
```

#### Running the Game

After successful build:

```cmd
LightQuest.exe
```

## Library Usage Details

### SDL2 (Graphics & Input)

SDL2 is initialized in `src/core/Window.cpp`:

```cpp
#include <SDL2/SDL.h>

SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
SDL_Window* window = SDL_CreateWindow("LightQuest", /* ... */);
SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, /* ... */);
```

**Usage in Build:**
```
-lSDL2main     # Main entry point
-lSDL2         # Core library
```

### SDL2_image (Image Loading)

Used in rendering sprites and backgrounds:

```cpp
#include <SDL2/SDL_image.h>

IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
SDL_Texture* texture = IMG_LoadTexture(renderer, "assets/images/button/play.png");
```

**Usage in Build:**
```
-lSDL2_image   # Image handling
```

### SDL2_ttf (Font Rendering)

Used by HUD and UI for text display:

```cpp
#include <SDL2/SDL_ttf.h>

TTF_Init();
TTF_Font* font = TTF_OpenFont("assets/fonts/arial.ttf", 24);
SDL_Surface* surface = TTF_RenderText_Solid(font, "Score: 1000", color);
```

**Usage in Build:**
```
-lSDL2_ttf     # Font rendering
```

## Build System Overview

### build.bat

Compiles the project with these flags:

```batch
g++ ^                          # GCC compiler
  [source files] ^             # All .cpp files
  -o LightQuest.exe ^          # Output executable
  -lmingw32 ^                  # MinGW library
  -lSDL2main ^                 # SDL2 entry point
  -lSDL2 -lSDL2_image ^        # Graphics libraries
  -lSDL2_ttf ^                 # Font rendering
  -static-libgcc ^             # Static linking
  -static-libstdc++ ^          # No external runtime dependencies
  -mwindows ^                  # Windows subsystem (no console)
  -Wall                        # All warnings
```

### Linker Flags Explained

| Flag | Purpose |
|------|---------|
| `-static-libgcc` | Statically link GCC runtime libraries |
| `-static-libstdc++` | Statically link C++ standard library |
| `-mwindows` | Remove console window (GUI only) |
| `-Wall` | Show all warnings |
| `-lSDL2main` | Automatically handles Windows app entry point |

## Troubleshooting

### "gcc: not found" or "g++: not found"
- Ensure MSYS2 MinGW terminal is open, OR
- Add `C:\msys64\mingw64\bin` to your system PATH

### SDL2 libraries not found
```bash
# Verify installation in MSYS2:
pacman -Q | grep SDL2

# Reinstall if needed:
pacman -S mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_image mingw-w64-x86_64-SDL2_ttf
```

### "Permission denied" when building
- Another instance of LightQuest.exe is running
- The build.bat script automatically kills the process

### Game window not appearing
- Ensure `-mwindows` flag is present in build.bat
- Check that all asset paths are correct
- Verify fonts directory contains at least one .ttf file

