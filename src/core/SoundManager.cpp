#include "SoundManager.h"

#include <SDL2/SDL.h>
#include <array>

#if defined(LIGHTQUEST_ENABLE_SOUND) && __has_include(<SDL2/SDL_mixer.h>)
#include <SDL2/SDL_mixer.h>
#define LIGHTQUEST_HAS_SDL_MIXER 1
#elif defined(LIGHTQUEST_ENABLE_SOUND) && __has_include(<SDL_mixer.h>)
#include <SDL_mixer.h>
#define LIGHTQUEST_HAS_SDL_MIXER 1
#else
#define LIGHTQUEST_HAS_SDL_MIXER 0
#endif

#if LIGHTQUEST_HAS_SDL_MIXER
// Struct nội bộ chứa toàn bộ dữ liệu SDL_mixer (Pimpl idiom)
// để tránh rò rỉ header SDL_mixer ra ngoài.
struct SoundManager::Impl
{
    // Track nhạc đang phát hiện tại.
    enum class MusicTrack
    {
        NONE,
        MENU,
        TRANSITION,
        WIN
    };

    bool initialized = false;
    bool assetsLoaded = false;
    bool audioInitByManager = false;

    Mix_Music* menuBgm = nullptr;
    Mix_Music* transitionBgm = nullptr;
    Mix_Music* winBgm = nullptr;

    Mix_Chunk* clickSfx = nullptr;
    Mix_Chunk* bombSfx = nullptr;
    Mix_Chunk* lossSfx = nullptr;

    static constexpr int kClickChannelStart = 0;
    static constexpr int kClickChannelCount = 4;
    // Kênh riêng cho tiếng thua để Game có thể kiểm tra trạng thái phát một cách chính xác.
    static constexpr int kLossChannel = kClickChannelStart + kClickChannelCount;
    int clickChannelCursor = 0;

    MusicTrack currentTrack = MusicTrack::NONE;

    // Giải phóng tất cả music và SFX đã load, đặt lại trạng thái.
    void freeAssets()
    {
        if (menuBgm)
        {
            Mix_FreeMusic(menuBgm);
            menuBgm = nullptr;
        }

        if (transitionBgm)
        {
            Mix_FreeMusic(transitionBgm);
            transitionBgm = nullptr;
        }

        if (winBgm)
        {
            Mix_FreeMusic(winBgm);
            winBgm = nullptr;
        }

        if (clickSfx)
        {
            Mix_FreeChunk(clickSfx);
            clickSfx = nullptr;
        }

        if (bombSfx)
        {
            Mix_FreeChunk(bombSfx);
            bombSfx = nullptr;
        }

        if (lossSfx)
        {
            Mix_FreeChunk(lossSfx);
            lossSfx = nullptr;
        }

        assetsLoaded = false;
        currentTrack = MusicTrack::NONE;
    }

    // Phát nhạc nền với hiệu ứng fade-in 240ms.
    void playMusic(Mix_Music* music, int loops, MusicTrack track)
    {
        if (!initialized || !music)
            return;

        if (Mix_FadeInMusic(music, loops, 240) != 0)
            return;

        currentTrack = track;
    }

    // Phát SFX trên kênh chỉ định.
    // restartIfPlaying = true: ngắt âm thanh cũ rồi phát lại ngay (dùng cho click).
    void playSfx(Mix_Chunk* chunk, int channel = -1, bool restartIfPlaying = false)
    {
        if (!initialized || !chunk)
            return;

        if (restartIfPlaying && channel >= 0 && Mix_Playing(channel) != 0)
            Mix_HaltChannel(channel);

        Mix_PlayChannel(channel, chunk, 0);
    }
};
#endif

SoundManager& SoundManager::instance()
{
    static SoundManager manager;
    return manager;
}

// Khởi tạo SDL_mixer: thử các cấu hình sample rate/buffer size khác nhau
// từ thấp nhất đến cao nhất để tìm cấu hình ổn định nhất với driver âm thanh.
bool SoundManager::init()
{
#if !LIGHTQUEST_HAS_SDL_MIXER
    return false;
#else
    if (!impl)
        impl = new Impl();

    if (impl->initialized)
        return true;

    if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0)
    {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
            return false;
        impl->audioInitByManager = true;
    }

    int initFlags = MIX_INIT_MP3 | MIX_INIT_OGG;
    Mix_Init(initFlags);

    // Thử lần lượt các cấu hình latency thấp đến an toàn hơn.
    // Đảm bảo click/SFX reagep trên nhiều loại driver khác nhau.
    const std::array<int, 4> sampleRates = {48000, 44100, 44100, 22050};
    const std::array<int, 4> chunkSizes  = {256,   256,   512,   1024};
    bool opened = false;
    for (size_t i = 0; i < sampleRates.size(); i++)
    {
        if (Mix_OpenAudio(sampleRates[i], MIX_DEFAULT_FORMAT, 2, chunkSizes[i]) == 0)
        {
            opened = true;
            break;
        }
    }

    if (!opened)
        return false;

    Mix_AllocateChannels(24);
    // Reserve click channels and one loss channel so these cues are not stolen by random SFX.
    Mix_ReserveChannels(Impl::kClickChannelCount + 1);
    Mix_VolumeMusic(MIX_MAX_VOLUME / 2);
    impl->initialized = true;
    return true;
#endif
}

// Load tất cả file nhạc/SFX từ thư mục assets/sound.
// Log cảnh báo nếu file nào bị thiếu nhưng không dừng lại.
bool SoundManager::loadAssets()
{
#if !LIGHTQUEST_HAS_SDL_MIXER
    return false;
#else
    if (!impl || !impl->initialized)
        return false;

    impl->freeAssets();

    impl->menuBgm = Mix_LoadMUS("assets/sound/background-music.mp3");
    impl->transitionBgm = Mix_LoadMUS("assets/sound/transition-music.mp3");
    impl->winBgm = Mix_LoadMUS("assets/sound/Win-sound-music.mp3");

    impl->clickSfx = Mix_LoadWAV("assets/sound/click-sound-music.mp3");
    impl->bombSfx = Mix_LoadWAV("assets/sound/bomb-sound-music.mp3");
    impl->lossSfx = Mix_LoadWAV("assets/sound/Loss.mp3");

    if (!impl->menuBgm)
        SDL_Log("Sound load failed: assets/sound/background-music.mp3 (%s)", Mix_GetError());
    if (!impl->transitionBgm)
        SDL_Log("Sound load failed: assets/sound/transition-music.mp3 (%s)", Mix_GetError());
    if (!impl->winBgm)
        SDL_Log("Sound load failed: assets/sound/Win-sound-music.mp3 (%s)", Mix_GetError());
    if (!impl->clickSfx)
        SDL_Log("Sound load failed: assets/sound/click-sound-music.mp3 (%s)", Mix_GetError());
    if (!impl->bombSfx)
        SDL_Log("Sound load failed: assets/sound/bomb-sound-music.mp3 (%s)", Mix_GetError());
    if (!impl->lossSfx)
        SDL_Log("Sound load failed: assets/sound/Loss.mp3 (%s)", Mix_GetError());

    if (impl->clickSfx)
        Mix_VolumeChunk(impl->clickSfx, MIX_MAX_VOLUME);
    if (impl->bombSfx)
        Mix_VolumeChunk(impl->bombSfx, MIX_MAX_VOLUME);
    if (impl->lossSfx)
        Mix_VolumeChunk(impl->lossSfx, MIX_MAX_VOLUME);

    impl->assetsLoaded = (impl->menuBgm || impl->transitionBgm || impl->winBgm || impl->clickSfx || impl->bombSfx || impl->lossSfx);
    return impl->assetsLoaded;
#endif
}

// Dọn sạch toàn bộ tài nguyên âm thanh và tắt SDL_mixer.
void SoundManager::shutdown()
{
#if LIGHTQUEST_HAS_SDL_MIXER
    if (!impl)
        return;

    impl->freeAssets();

    if (impl->initialized)
    {
        Mix_HaltChannel(-1);
        Mix_HaltMusic();
        Mix_CloseAudio();
        Mix_Quit();
        impl->initialized = false;
    }

    if (impl->audioInitByManager)
    {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        impl->audioInitByManager = false;
    }

    delete impl;
    impl = nullptr;
#endif
}

// Phát nhạc nền menu (phát lặp vô tận). Bỏ qua nếu đang sẵn phát rồi.
void SoundManager::playMenuBgm()
{
#if LIGHTQUEST_HAS_SDL_MIXER
    if (!impl || !impl->initialized)
        return;

    if (impl->currentTrack == Impl::MusicTrack::MENU && Mix_PlayingMusic())
        return;

    impl->playMusic(impl->menuBgm, -1, Impl::MusicTrack::MENU);
#endif
}

// Phát nhạc chuyển cảnh giữa các stage (phát 1 lần).
void SoundManager::playTransition()
{
#if LIGHTQUEST_HAS_SDL_MIXER
    if (!impl || !impl->initialized)
        return;

    impl->playMusic(impl->transitionBgm, 0, Impl::MusicTrack::TRANSITION);
#endif
}

// Phát tiếng click UI. Xoay vòng qua 4 kênh riêng
// để các click liên tiếp không cắt nhau.
void SoundManager::playClick()
{
#if LIGHTQUEST_HAS_SDL_MIXER
    if (!impl || !impl->initialized)
        return;

    // Xoay vòng qua các kênh click để click nhanh liên tiếp không cắt nhau.
    const int channel = Impl::kClickChannelStart + impl->clickChannelCursor;
    impl->clickChannelCursor = (impl->clickChannelCursor + 1) % Impl::kClickChannelCount;
    impl->playSfx(impl->clickSfx, channel, true);
#endif
}

// Phát tiếng nổ khi player bước vào mìn.
void SoundManager::playBomb()
{
#if LIGHTQUEST_HAS_SDL_MIXER
    if (!impl || !impl->initialized)
        return;

    impl->playSfx(impl->bombSfx);
#endif
}

// Phát tiếng thua game trên kênh cố định để Game có thể kiểm tra isLossPlaying().
void SoundManager::playLoss()
{
#if LIGHTQUEST_HAS_SDL_MIXER
    if (!impl || !impl->initialized)
        return;

    impl->playSfx(impl->lossSfx, Impl::kLossChannel, true);
#endif
}

void SoundManager::stopLoss()
{
#if LIGHTQUEST_HAS_SDL_MIXER
    if (!impl || !impl->initialized)
        return;

    Mix_HaltChannel(Impl::kLossChannel);
#endif
}

bool SoundManager::isLossPlaying() const
{
#if !LIGHTQUEST_HAS_SDL_MIXER
    return false;
#else
    if (!impl || !impl->initialized)
        return false;

    return Mix_Playing(Impl::kLossChannel) != 0;
#endif
}

void SoundManager::playWin()
{
#if LIGHTQUEST_HAS_SDL_MIXER
    if (!impl || !impl->initialized)
        return;

    impl->playMusic(impl->winBgm, 0, Impl::MusicTrack::WIN);
#endif
}

void SoundManager::stopMusic(int fadeOutMs)
{
#if LIGHTQUEST_HAS_SDL_MIXER
    if (!impl || !impl->initialized)
        return;

    if (fadeOutMs > 0)
        Mix_FadeOutMusic(fadeOutMs);
    else
        Mix_HaltMusic();

    impl->currentTrack = Impl::MusicTrack::NONE;
#else
    (void)fadeOutMs;
#endif
}
