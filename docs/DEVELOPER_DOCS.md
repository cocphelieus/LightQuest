# LightQuest Developer Docs

This document is a practical walkthrough for teammates who need to read, modify, or debug LightQuest quickly.

## 1) Runtime Architecture

LightQuest follows a scene-driven loop inside Game::run:

1. Handle input events
2. Update current scene logic
3. Render current scene
4. Cap frame rate to target FPS

Main state machine:

- LOADING: Intro/loading screen.
- MENU: Main menu, rank, story/guide overlays, exit confirmation.
- PLAYING: Core gameplay with map, movement, torch-question logic.

Core coordinator:

- src/core/Game.cpp
- src/core/Game.h

## 2) Module Responsibilities

### Core

- src/core/Window.h, src/core/Window.cpp
  - SDL window/renderer lifecycle.
  - Logical render size management for consistent mouse mapping.
  - Fullscreen toggle.

- src/core/SoundManager.h, src/core/SoundManager.cpp
  - All BGM and SFX loading/playback.
  - Low-latency mixer setup with fallback device configs.
  - Reserved channels for rapid click SFX and dedicated loss SFX channel.

- src/core/QuestionManager.h, src/core/QuestionManager.cpp
  - Parse question file.
  - Display question overlay and evaluate answer.
  - Random question selection.

- src/core/Button.h, src/core/Button.cpp
  - Texture button with hover animation and click hit-test.

### Scenes

- src/scenes/LoadingScene.cpp
  - Loading transition before entering menu.

- src/scenes/MenuScene.h, src/scenes/MenuScene.cpp
  - Main buttons (Start, Story, Guide, Rank, Exit).
  - Overlay handling and cursor changes.
  - Ranking board rendering.
  - Exit confirmation popup logic.

- src/scenes/PlayScene.h, src/scenes/PlayScene.cpp
  - Gameplay scene lifecycle.
  - Player movement, mine/failure detection, goal detection.
  - Torch/question flow.
  - HUD rendering.
  - Death sequence.

### Gameplay Data + Entities

- src/map/MapManager.h, src/map/MapManager.cpp
  - Stage generation/reset.
  - Fog/shadow visibility and reveal logic.
  - Torch placement, progression, hints.
  - Mine/goal state.

- src/entities/Player.h, src/entities/Player.cpp
  - Tile + pixel movement state.
  - Sprite rendering.

## 3) Data File Formats

### Questions

File: data/question.txt

Current format is 6 lines per question block:

1. Question text
2. Option A
3. Option B
4. Option C
5. Option D
6. Correct answer letter (A/B/C/D)

Then a blank line (optional separator).

### Rankings

File: data/ranking.txt

Each line:

seconds|timestamp

Example:

8|2026-03-12 09:12:38

Rank board keeps top 5 fastest times.

## 4) Audio Behavior (Current)

Audio assets loaded from assets/sound:

- background-music.mp3
- transition-music.mp3
- Win-sound-music.mp3
- click-sound-music.mp3
- bomb-sound-music.mp3
- Loss.mp3

Current gameplay/menu policy:

- Menu BGM is menu-only.
- Entering gameplay stops menu BGM.
- Stage-to-stage transitions can play transition cue.
- Failure flow: bomb SFX -> death sequence -> loss SFX -> game-over screen.
- After closing game-over, loop waits briefly for loss SFX tail to avoid abrupt cut.

## 5) Exit Popup Troll Logic

Implemented in MenuScene:

- YES button requires 4-5 clicks to confirm exit.
- Before reaching required click count, YES button jumps position.
- NO or closing popup resets troll counter and button position.

Key functions:

- MenuScene::resetExitConfirmTroll
- MenuScene::shuffleExitYesButton

## 6) Campaign Flow

Managed in Game::run:

- Start game from menu -> stage index reset to 0.
- Clear stage:
  - If not final stage: transition cue + stage-clear message.
  - If final stage completed: win cue + campaign complete popup + ranking write.
- Failure:
  - Death sequence + game-over flow + return to menu.

## 7) Build Notes

Build script:

- build.bat

Highlights:

- Auto-kills existing LightQuest.exe to reduce linker permission issues.
- Detects SDL2_mixer and enables sound by flag.
- Produces LightQuest.exe in project root.

Useful command:

cmd /c "echo.|build.bat"

(Automatically sends Enter for pause prompts in scripted checks.)

## 8) Tricky Areas (Read Before Editing)

### A) Coordinate Mapping in Menu

Menu overlay hit-tests use renderer logical coordinates. If UI click zones feel off after resolution/fullscreen tweaks, inspect:

- MenuScene::mapMouseToLogical

### B) Audio Channel Conflicts

Rapid UI clicks and loss cue rely on reserved channels. If SFX cut each other unexpectedly, inspect:

- SoundManager channel constants and Mix_ReserveChannels call.

### C) Stage Progression Side Effects

Campaign stage, elapsed time, and play scene reset are coordinated across multiple branches in Game::run. Keep state transitions symmetric when changing flow.

### D) Rank Overlay Layout

Recent UI changes removed rank panel border/fill for a cleaner look. If visual style changes again, check rank rendering block in MenuScene::render.

## 9) Suggested Team Conventions

- Keep new assets names stable and match exact case in code.
- For new scene-level behavior, add high-level comments in both .h and .cpp.
- For any flow with special timing (audio tails, minimum popup duration), document constants in-place.
- Run build.bat after each cross-module change (Game + Scene + Audio).
