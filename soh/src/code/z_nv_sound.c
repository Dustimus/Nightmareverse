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
#include <stdio.h>
#include <string.h>

void NvSound_Play(const char* name, s32 vol);
void NvSound_MusicLoad(s32 ch, const char* name, s32 loop);
void NvSound_MusicPlay(s32 ch, s32 play);
void NvSound_StopAll(void);

extern const char* const gNvBgmNames[57][3];
extern const u8 gNvBgmLoop[57][3];
extern const u8 gNvBgmRoom[7][32][4];

static u8 sNvRoomBgmState[7 * 32]; // g_roomBgmState: all clear at a new game
static u8 sNvBgmState = 0xFF;      // g_BGM_STATE: what's playing
static s32 sNvBgmStage = -1;       // the room that state belongs to
static s32 sNvBgmRoom = -1;

static u8 NvBgm_Group(s32 stage, s32 room, u8 state) {
    if ((stage < 0) || (stage >= 7) || (room < 0) || (room >= 32) || (state == 0xFF)) {
        return 0xFF;
    }
    return gNvBgmRoom[stage][room][state & 7];
}

/** bgm_load_and_start: load the group's channels, stopped. */
static void NvBgm_Load(u8 group) {
    s32 ch;

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
