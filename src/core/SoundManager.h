#pragma once

class SoundManager
{
public:
    static SoundManager& instance();

    // Initialize SDL_mixer and low-latency channel configuration.
    bool init();
    // Load music/SFX assets from assets/sound.
    bool loadAssets();
    // Stop playback and free all audio resources.
    void shutdown();

    // Background music for menu screen only.
    void playMenuBgm();
    // Short transition cue between campaign stages.
    void playTransition();
    // UI click feedback. Supports rapid clicking.
    void playClick();
    // Immediate explosion cue when stepping on a mine.
    void playBomb();
    // Loss cue played at game-over flow.
    void playLoss();
    // Immediately stop the loss cue (e.g. when leaving game-over screen).
    void stopLoss();
    // Used by Game loop to avoid cutting off the tail of loss cue too early.
    bool isLossPlaying() const;
    // Win cue when finishing all campaign stages.
    void playWin();
    // Fade or halt currently playing music track.
    void stopMusic(int fadeOutMs = 250);

private:
    SoundManager() = default;
    ~SoundManager() = default;
    SoundManager(const SoundManager&) = delete;
    SoundManager& operator=(const SoundManager&) = delete;

    struct Impl;
    Impl* impl = nullptr;
};
