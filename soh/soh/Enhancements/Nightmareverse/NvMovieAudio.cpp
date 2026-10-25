// Nightmareverse: an RE1 movie's sound (soh/src/code/z_nv_movie.c), its PCM queued on an SDL audio
// device of its own beside the game's.
#include <SDL2/SDL.h>
#include <cstdint>

static SDL_AudioDeviceID sDevice = 0;

extern "C" void NvMovieAudio_Play(const void* pcm, uint32_t bytes, int32_t rate, int32_t channels) {
    SDL_AudioSpec want = {};
    SDL_AudioSpec have = {};

    if ((SDL_WasInit(SDL_INIT_AUDIO) == 0) && (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)) {
        return;
    }
    want.freq = rate;
    want.format = AUDIO_S16LSB;
    want.channels = (Uint8)channels;
    want.samples = 2048;
    sDevice = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (sDevice == 0) {
        return;
    }
    SDL_QueueAudio(sDevice, pcm, bytes);
    SDL_PauseAudioDevice(sDevice, 0);
}

extern "C" void NvMovieAudio_Stop(void) {
    if (sDevice != 0) {
        SDL_ClearQueuedAudio(sDevice);
        SDL_CloseAudioDevice(sDevice);
        sDevice = 0;
    }
}

extern "C" uint32_t NvMovieAudio_Ticks(void) {
    return SDL_GetTicks();
}
