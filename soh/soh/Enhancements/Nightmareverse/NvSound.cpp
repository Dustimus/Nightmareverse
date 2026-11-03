// Nightmareverse: RE1's sounds (soh/src/code/z_nv_sound.c decides what plays; this mixes it).
//
// One SDL audio device of our own beside the game's, as the movies have: effects (RE1's banks,
// played by name: "re1/sound/<name>", or a voice line "re1/voice/<name>", re1/tools/soh_sound.py)
// on a pool of voices, and RE1's three music channels (g_SndBank: each its WAV, looping or not,
// started and stopped by the rooms' scripts). RE1's WAVs (mono, 22050 Hz, 8 or 16 bit) are
// converted to the device's format once, the first time each is played, and kept.
#include <SDL2/SDL.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>

namespace {

struct Pcm {
    std::vector<int16_t> samples; // stereo, interleaved, at the device's rate
    bool ok = false;
};

struct Voice {
    const Pcm* pcm = nullptr;
    size_t pos = 0; // in samples (both channels)
    int vol = 128;  // 0-128
    bool loop = false;
    bool playing = false;
};

constexpr int kRate = 44100;
constexpr int kEffects = 24;
constexpr int kMusic = 3;

SDL_AudioDeviceID sDevice = 0;
std::unordered_map<std::string, Pcm> sCache;
Voice sEffects[kEffects];
Voice sMusic[kMusic];
int sMusicVol = 128;

void Mix(void*, Uint8* stream, int len) {
    int16_t* out = reinterpret_cast<int16_t*>(stream);
    int n = len / 2;

    std::vector<int32_t> acc(n, 0);
    auto add = [&](Voice& v) {
        if (!v.playing || (v.pcm == nullptr)) {
            return;
        }
        const std::vector<int16_t>& s = v.pcm->samples;
        for (int i = 0; i < n; i++) {
            if (v.pos >= s.size()) {
                if (!v.loop || s.empty()) {
                    v.playing = false;
                    return;
                }
                v.pos = 0;
            }
            acc[i] += s[v.pos++] * v.vol / 128;
        }
    };
    for (Voice& v : sEffects) {
        add(v);
    }
    for (Voice& v : sMusic) {
        int keep = v.vol;
        v.vol = v.vol * sMusicVol / 128;
        add(v);
        v.vol = keep;
    }
    for (int i = 0; i < n; i++) {
        out[i] = (int16_t)SDL_clamp(acc[i], -32768, 32767);
    }
}

bool Open() {
    if (sDevice != 0) {
        return true;
    }
    if ((SDL_WasInit(SDL_INIT_AUDIO) == 0) && (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)) {
        return false;
    }
    SDL_AudioSpec want = {};
    SDL_AudioSpec have = {};
    want.freq = kRate;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = Mix;
    sDevice = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (sDevice == 0) {
        return false;
    }
    SDL_PauseAudioDevice(sDevice, 0);
    return true;
}

// a sound by name, loaded and converted the first time
const Pcm* Get(const char* name) {
    std::string key(name);
    for (char& c : key) {
        c = (char)tolower((unsigned char)c);
    }
    auto it = sCache.find(key);
    if (it != sCache.end()) {
        return it->second.ok ? &it->second : nullptr;
    }
    Pcm& pcm = sCache[key];
    std::string path = "re1/sound/" + key;
    if (!ResourceMgr_FileExists(path.c_str())) {
        path = "re1/voice/" + key;
        if (!ResourceMgr_FileExists(path.c_str())) {
            printf("NVSOUND missing %s\n", key.c_str());
            return nullptr;
        }
    }
    const uint8_t* d = static_cast<const uint8_t*>(ResourceGetDataByName(path.c_str()));
    if ((d == nullptr) || (memcmp(d, "RIFF", 4) != 0)) {
        return nullptr;
    }
    uint32_t riff;
    memcpy(&riff, d + 4, 4);
    SDL_AudioSpec spec;
    Uint8* buf = nullptr;
    Uint32 bytes = 0;
    if (SDL_LoadWAV_RW(SDL_RWFromConstMem(d, (int)riff + 8), 1, &spec, &buf, &bytes) == nullptr) {
        return nullptr;
    }
    SDL_AudioCVT cvt;
    if (SDL_BuildAudioCVT(&cvt, spec.format, spec.channels, spec.freq, AUDIO_S16SYS, 2, kRate) < 0) {
        SDL_FreeWAV(buf);
        return nullptr;
    }
    std::vector<Uint8> work((size_t)bytes * cvt.len_mult);
    memcpy(work.data(), buf, bytes);
    SDL_FreeWAV(buf);
    cvt.buf = work.data();
    cvt.len = (int)bytes;
    if (cvt.needed && (SDL_ConvertAudio(&cvt) != 0)) {
        return nullptr;
    }
    int outBytes = cvt.needed ? cvt.len_cvt : (int)bytes;
    pcm.samples.resize(outBytes / 2);
    memcpy(pcm.samples.data(), work.data(), outBytes);
    pcm.ok = true;
    return &pcm;
}

} // namespace

/** Play a sound once (an effect or a voice line), at vol 0-128. */
extern "C" void NvSound_Play(const char* name, int32_t vol) {
    if (!Open()) {
        return;
    }
    const Pcm* pcm = Get(name);
    if (pcm == nullptr) {
        return;
    }
    SDL_LockAudioDevice(sDevice);
    Voice* slot = &sEffects[0];
    for (Voice& v : sEffects) { // a free voice, else the one furthest along
        if (!v.playing) {
            slot = &v;
            break;
        }
        if (v.pos > slot->pos) {
            slot = &v;
        }
    }
    slot->pcm = pcm;
    slot->pos = 0;
    slot->vol = SDL_clamp(vol, 0, 128);
    slot->loop = false;
    slot->playing = true;
    SDL_UnlockAudioDevice(sDevice);
}

/** Music channel ch (0-2): load its WAV (NULL: none), stopped; loop as RE1's flag. */
extern "C" void NvSound_MusicLoad(int32_t ch, const char* name, int32_t loop) {
    if ((ch < 0) || (ch >= kMusic) || !Open()) {
        return;
    }
    const Pcm* pcm = (name != nullptr) ? Get(name) : nullptr;
    SDL_LockAudioDevice(sDevice);
    sMusic[ch].pcm = pcm;
    sMusic[ch].pos = 0;
    sMusic[ch].vol = 128;
    sMusic[ch].loop = loop != 0;
    sMusic[ch].playing = false;
    SDL_UnlockAudioDevice(sDevice);
}

/** Start (from its beginning) or stop music channel ch. */
extern "C" void NvSound_MusicPlay(int32_t ch, int32_t play) {
    if ((ch < 0) || (ch >= kMusic) || (sDevice == 0)) {
        return;
    }
    SDL_LockAudioDevice(sDevice);
    if (play && (sMusic[ch].pcm != nullptr)) {
        sMusic[ch].pos = 0;
        sMusic[ch].playing = true;
    } else {
        sMusic[ch].playing = false;
    }
    SDL_UnlockAudioDevice(sDevice);
}

/** Whether music channel ch has a WAV loaded / is playing. */
extern "C" int32_t NvSound_MusicState(int32_t ch, int32_t* playing) {
    if ((ch < 0) || (ch >= kMusic)) {
        return 0;
    }
    if (playing != nullptr) {
        *playing = sMusic[ch].playing;
    }
    return sMusic[ch].pcm != nullptr;
}

/** Everything stops (leaving the mansion). */
extern "C" void NvSound_StopAll(void) {
    if (sDevice == 0) {
        return;
    }
    SDL_LockAudioDevice(sDevice);
    for (Voice& v : sEffects) {
        v.playing = false;
    }
    for (Voice& v : sMusic) {
        v.playing = false;
        v.pcm = nullptr;
    }
    SDL_UnlockAudioDevice(sDevice);
}

/** The music's volume, 0-128 (the pause menu turns it down, as OoT's does). */
extern "C" void NvSound_MusicVolume(int32_t vol) {
    sMusicVol = SDL_clamp(vol, 0, 128);
}
