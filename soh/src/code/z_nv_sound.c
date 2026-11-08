/*
 * Nightmareverse: RE1's sounds in the mansion, played through
 * soh/soh/Enhancements/Nightmareverse/NvSound.cpp.
 *
 * Room music as RE1's (SoundSystem.cpp): each room has a music state byte (g_roomBgmState, 7
 * stages x 32 rooms; RE1 starts a game with it all clear, and the rooms' scripts set it with
 * opcode 0x37). Entering a room, update_room_bgm reads it: bits 0-2 pick the room's music group
 * (gNvBgmRoom), which loads up to three channels (gNvBgmNames, looping per gNvBgmLoop); bits 3-5
 * say which channels start now; bits 6-7 say how (0: keep the same group playing across rooms,
 * toggling channels; 1: reload, start nothing; 2: reload and start). Scripts start and stop
 * channels themselves with opcodes 0x15 and 0x16. Most rooms load a group and stay quiet until
 * their script starts it, as RE1's mansion does.
 *
 * Effects: RE1's names, played where our ports of RE1's code make RE1's sounds (NvSound_Effect).
 */
#include "global.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

void NvSound_Play(const char* name, s32 vol);
void NvSound_MusicLoad(s32 ch, const char* name, s32 loop);
void NvSound_MusicPlay(s32 ch, s32 play);
void NvSound_StopAll(void);
s32 NvSound_MusicState(s32 ch, s32* playing);

extern const char* const gNvBgmNames[57][3];
extern const u8 gNvBgmLoop[57][3];
extern const u8 gNvBgmRoom[7][32][4];
extern const char* const gNvRoomSnd[203][48];

static u8 sNvRoomBgmState[7 * 32]; // g_roomBgmState: all clear at a new game
static u8 sNvBgmState = 0xFF;      // g_BGM_STATE: what's playing
static s32 sNvBgmStage = -1;       // the room that state belongs to
static s32 sNvBgmRoom = -1;
// the music channels' volumes, as RE1 keeps them: DirectSound's hundredths of a dB (0 full,
// -10000 silent), moved by the scripts' fade (0x27), set (0x2F) and ramp (0x43)
static s32 sNvBgmDb[3];
static s32 sNvBgmFadeStep; // BuildSndFadeTbl / UpdateSoundFade: dB a tick, all channels down
static s32 sNvBgmRampCh = -1;
static s32 sNvBgmRampDir; // FUN_004804a0 / UpdateSoundDecay
static s32 sNvBgmRampLeft;

/** The rooms' music states, for the typewriter's save (z_nv_save.c): its size, written to p if not NULL. */
u32 NvBgm_SaveState(u8* p) {
    if (p != NULL) {
        memcpy(p, sNvRoomBgmState, sizeof(sNvRoomBgmState));
    }
    return sizeof(sNvRoomBgmState);
}

void NvBgm_LoadState(const u8* p) {
    memcpy(sNvRoomBgmState, p, sizeof(sNvRoomBgmState));
}

void NvBgm_NewRun(void) {
    memset(sNvRoomBgmState, 0, sizeof(sNvRoomBgmState));
}

static u8 NvBgm_Group(s32 stage, s32 room, u8 state) {
    if ((stage < 0) || (stage >= 7) || (room < 0) || (room >= 32) || (state == 0xFF)) {
        return 0xFF;
    }
    return gNvBgmRoom[stage][room][state & 7];
}

/** bgm_load_and_start: load the group's channels, stopped. */
static void NvBgm_Load(u8 group) {
    s32 ch;

    sNvBgmFadeStep = 0;
    sNvBgmRampCh = -1;
    for (ch = 0; ch < 3; ch++) {
        sNvBgmDb[ch] = 0;
    }

    for (ch = 0; ch < 3; ch++) {
        const char* name = (group < 57) ? gNvBgmNames[group][ch] : NULL;

        NvSound_MusicLoad(ch, name, (group < 57) ? gNvBgmLoop[group][ch] : 0);
    }
    printf("NVBGM group %d: %s %s %s\n", (int)group, (group < 57 && gNvBgmNames[group][0]) ? gNvBgmNames[group][0] : "-",
           (group < 57 && gNvBgmNames[group][1]) ? gNvBgmNames[group][1] : "-",
           (group < 57 && gNvBgmNames[group][2]) ? gNvBgmNames[group][2] : "-");
    fflush(stdout);
}

/** bgm_start_secondary_slots: the channels whose bits (3-5) are set start. */
static void NvBgm_StartBits(u8 state) {
    s32 ch;

    for (ch = 0; ch < 3; ch++) {
        if (state & (8 << ch)) {
            NvSound_MusicPlay(ch, true);
        }
    }
}

/** update_room_bgm: entering stage's room (RE1's numbering: stage 0 is the mansion's first floor). */
void NvBgm_RoomEnter(s32 stage, s32 room) {
    u8 target = sNvRoomBgmState[(stage * 32 + room) % (7 * 32)];
    u8 prev = sNvBgmState;
    u8 newGroup = NvBgm_Group(stage, room, target);
    u8 oldGroup = NvBgm_Group(sNvBgmStage, sNvBgmRoom, prev);
    s32 ch;

    sNvBgmStage = stage;
    sNvBgmRoom = room;
    if (target == 0xFF) { // no music here
        if (prev != 0xFF) {
            for (ch = 0; ch < 3; ch++) {
                NvSound_MusicPlay(ch, false);
            }
        }
        sNvBgmState = 0xFF;
        return;
    }
    switch (target >> 6) {
        case 0:
            if ((prev == 0xFF) || (newGroup != oldGroup)) {
                NvBgm_Load(newGroup);
                NvBgm_StartBits(target);
            } else { // the same music goes on: only the channels whose bits changed
                u8 changed = target ^ prev;

                for (ch = 0; ch < 3; ch++) {
                    if (changed & (8 << ch)) {
                        NvSound_MusicPlay(ch, (target & (8 << ch)) != 0);
                    }
                }
            }
            break;
        case 1: // reload, start nothing
            NvBgm_Load(newGroup);
            break;
        case 2: // reload and start
            target &= 0x7F;
            NvBgm_Load(NvBgm_Group(stage, room, target));
            NvBgm_StartBits(target);
            break;
        default:
            break;
    }
    sNvBgmState = target;
}

/** Leaving the mansion: RE1's sounds stop. */
void NvBgm_Leave(void) {
    if (sNvBgmStage >= 0) {
        NvSound_StopAll();
    }
    sNvBgmState = 0xFF;
    sNvBgmStage = sNvBgmRoom = -1;
}

/** Script opcode 0x15 (cmd_bgm_play): channel ch starts. */
void NvBgm_ScdPlay(s32 ch) {
    if ((ch >= 0) && (ch < 3)) {
        NvSound_MusicPlay(ch, true);
        if (sNvBgmState != 0xFF) {
            sNvBgmState |= 8 << ch;
        }
    }
}

/** Script opcode 0x16 (cmd_bgm_stop): channel ch stops. */
void NvBgm_ScdStop(s32 ch) {
    if ((ch >= 0) && (ch < 3)) {
        NvSound_MusicPlay(ch, false);
        if (sNvBgmState != 0xFF) {
            sNvBgmState &= ~(8 << ch);
        }
    }
}

void NvSound_MusicChannelVolume(s32 ch, s32 vol);


static void NvBgm_ApplyDb(s32 ch) {
    f32 lin = powf(10.0f, sNvBgmDb[ch] / 2000.0f);

    NvSound_MusicChannelVolume(ch, (s32)(lin * 128.0f + 0.5f));
}

/** Script opcode 0x27 (cmd_snd_fade_set): the music fades out, n * 78 hundredths of a dB a tick. */
void NvBgm_ScdFade(s32 n) {
    sNvBgmFadeStep = n * 0x4E;
}

/** Script opcode 0x2F (cmd_snd_pan_vol_set): a channel's volume from its pan / volume pair (CalcPanVolume). */
void NvBgm_ScdPanVol(s32 ch, s32 pan, s32 vol) {
    s32 avg = (pan + vol) / 2;

    if ((ch < 0) || (ch >= 3)) {
        return;
    }
    sNvBgmDb[ch] = (avg < 0x20) ? (avg * 0x103 - 10000) : (avg * 0x12 - 0x8E4);
    NvBgm_ApplyDb(ch);
}

/** Script opcode 0x43 (cmd_bgm_volume_ramp): a playing channel slides by dir over n steps, then stops. */
void NvBgm_ScdRamp(s32 ch, s32 dir, s32 n) {
    s32 playing = 0;

    if ((ch < 0) || (ch >= 3) || (n <= 0) || !(sNvBgmState != 0xFF && (sNvBgmState & (8 << ch)))) {
        return;
    }
    NvSound_MusicState(ch, &playing);
    if (!playing) {
        return;
    }
    sNvBgmRampCh = ch;
    sNvBgmRampDir = (dir / n) * 0x4E;
    sNvBgmRampLeft = n * 2;
}

/** A tick of RE1's sound update: the fade and the ramp (UpdateSoundFade, UpdateSoundDecay). */
void NvBgm_Tick(void) {
    s32 ch;

    if (sNvBgmFadeStep > 0) {
        s32 any = false;

        for (ch = 0; ch < 3; ch++) {
            if (sNvBgmDb[ch] > -10000) {
                sNvBgmDb[ch] = MAX(sNvBgmDb[ch] - sNvBgmFadeStep, -10000);
                NvBgm_ApplyDb(ch);
                any = true;
            }
        }
        if (!any) {
            sNvBgmFadeStep = 0;
        }
    }
    if ((sNvBgmRampCh >= 0) && (sNvBgmRampLeft > 0)) {
        ch = sNvBgmRampCh;
        sNvBgmRampLeft--;
        sNvBgmDb[ch] += (400 / (sNvBgmRampLeft + 1) * sNvBgmRampDir) / 100;
        NvBgm_ApplyDb(ch);
        if ((sNvBgmDb[ch] < -10000) || (sNvBgmRampLeft <= 0)) { // (ends stopped, its volume back)
            NvSound_MusicPlay(ch, false);
            sNvBgmDb[ch] = 0;
            NvBgm_ApplyDb(ch);
            sNvBgmRampCh = -1;
        }
    }
}

static u8 sNvBgmSaved = 0xFF; // what cmd_bgm_stop_all put aside (RE1: g_BGM_STATE << 8)

/** Script opcode 0x4B (cmd_bgm_stop_all): the music stops, its state put aside. */
void NvBgm_ScdStopAll(void) {
    s32 ch;

    for (ch = 0; ch < 3; ch++) {
        NvSound_MusicPlay(ch, false);
    }
    sNvBgmSaved = sNvBgmState;
    if (sNvBgmState != 0xFF) {
        sNvBgmState &= ~0x38;
    }
}

/** Script opcode 0x4A (cmd_bgm_restore): the channels that were playing start again. */
void NvBgm_ScdRestore(void) {
    if ((sNvBgmSaved == 0xFF) || (sNvBgmState == 0xFF)) {
        return;
    }
    NvBgm_StartBits(sNvBgmSaved);
    sNvBgmState = sNvBgmSaved;
    sNvBgmSaved = 0xFF;
}

/** Script opcode 0x37 (cmd_room_bgm_state_set): a stage's room's music state. */
void NvBgm_ScdState(s32 stage, s32 room, u8 state) {
    sNvRoomBgmState[(stage * 32 + room) % (7 * 32)] = state;
}

/** An effect by RE1's name (its WAV in the player's SOUND folder), at vol 0-128. */
void NvSound_Effect(const char* name, s32 vol) {
    NvSound_Play(name, vol);
}

/**
 * A menu's sound (RE1's status, box and file screens): RE1's own cursor, decide and cancel
 * (load_character_sfx's shared entries), else OoT's.
 */
void NvSound_Menu(u16 sfx) {
    const char* name = (sfx == NA_SE_SY_CURSOR) ? "cursor"
                       : (sfx == NA_SE_SY_DECIDE) ? "decide"
                       : (sfx == NA_SE_SY_ERROR)  ? "cancel"
                                                  : NULL;

    if (name != NULL) {
        NvSound_Play(name, 128);
    } else {
        Audio_PlaySfxGeneral(sfx, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultReverb);
    }
}

/** A weapon's shot (SoundApi.cpp's per-weapon banks: the fire sound, slot 7), by RE1's item id. */
void NvSound_Shot(s32 item) {
    static const char* const sShot[11] = {
        NULL, "knife01", "gun01", "shot01", "magnum01", "magnum01", NULL, "grbomb02", "gracid02", "grfire02", "rocket01",
    };

    if ((item > 0) && (item < 11) && (sShot[item] != NULL)) {
        NvSound_Play(sShot[item], 128);
    }
}

/**
 * Slot of the room's sound bank (Room_LoadEnemySoundBanks' 48: an enemy's sounds by its group,
 * doors, items, the floors' footsteps), heard from (x, z) in RE1's units: quieter with distance from
 * Link, as Calc3DSndPan / CalcPanVolume make RE1's. (lx, lz): Link, RE1's units.
 */
void NvSnd_At(s32 slot, s32 x, s32 z, s32 lx, s32 lz) {
    s32 row = sNvBgmStage * 29 + sNvBgmRoom;
    const char* name;
    f32 d;
    s32 vol;

    if ((sNvBgmStage < 0) || (row < 0) || (row >= 203) || (slot < 0) || (slot >= 48)) {
        return;
    }
    name = gNvRoomSnd[row][slot];
    if (name == NULL) {
        return;
    }
    d = sqrtf((f32)(x - lx) * (x - lx) + (f32)(z - lz) * (z - lz));
    vol = 128 - (s32)(d / 120.0f);
    if (vol < 24) {
        vol = 24;
    }
    NvSound_Play(name, vol);
}

/* ---- Link's footsteps: RE1's for the floor (PlayEntitySnd 0 at the walk's footfalls) ---- */

static const u8* sNvFtZones; // the room's "RFTZ" (re1/tools/soh_scene.py ftzones_blob), or NULL

void NvSnd_SetFootstepZones(const u8* d) {
    sNvFtZones = ((d != NULL) && (memcmp(d, "RFTZ", 4) == 0)) ? d : NULL;
}

/**
 * Link's step (z_player.c Player_PlaySteppingSfx) in RE1's rooms: the room bank's footstep for the
 * zone he's in (LookupFootstepZone: the first zone holding him), alternating its two sounds. False
 * outside the mansion (OoT's own step then).
 */
s32 NvSnd_LinkStep(Player* player) {
    static u8 sAlt;
    u16 n;
    s32 x, z, k;

    if ((sNvBgmStage < 0) || (sNvFtZones == NULL)) {
        return false;
    }
    memcpy(&n, sNvFtZones + 4, 2);
    x = (s32)(player->actor.world.pos.x / 0.0217f);
    z = (s32)(-player->actor.world.pos.z / 0.0217f);
    for (k = 0; k < n; k++) {
        u16 e[5];

        memcpy(e, sNvFtZones + 6 + k * 10, 10);
        if (((u32)(x - e[0]) < e[2]) && ((u32)(z - e[1]) < e[3])) {
            sAlt ^= 1;
            NvSnd_At((e[4] & 0xFF) + sAlt, x, z, x, z);
            return true;
        }
    }
    return true; // (no zone: RE1 plays nothing)
}

/* ---- doors: RE1's room sounds (load_room_sfx: the door's record byte 9 picks the pair) ---- */

static const char* const sNvDoorSnd[15][2] = {
    { "dr_wd01", "dr_wd02" },  { "dr_mtl01", "dr_mtl02" }, { NULL, "dr_brk01" },        { "dr_reb01", "dr_reb02" },
    { "st_wcp01", NULL },      { "st_wd01", NULL },        { "ev_mv", NULL },           { "ev_new01", "ev_new02" },
    { "dr_gat01", "dr_gat02" }, { "st_mtl01", NULL },      { "ev_old01", "ev_old02" },  { "ladder01", NULL },
    { "dr_air01", "dr_air02" }, { "ev_lab01", NULL },      { "ev_ftn01", "ev_ftn02" },
};

/** A door's sound (z_nv_doorscene.c): which = 0 as it opens, 1 as it closes behind (play_sfx(0, 1)). */
void NvSnd_Door(s32 set, s32 which) {
    if ((set >= 0) && (set < 15) && (which >= 0) && (which < 2) && (sNvDoorSnd[set][which] != NULL)) {
        NvSound_Play(sNvDoorSnd[set][which], 128);
    }
}
