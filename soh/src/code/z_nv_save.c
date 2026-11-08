/*
 * Nightmareverse: the RE1 run's save - one slot, written at a typewriter.
 *
 * A typewriter (a room action with handler 0x10: PlayerAnimations.cpp check_typewriter,
 * RoomEvents.cpp check_typewriter_state): with an INK RIBBON, "Will you use the INK RIBBON?" and
 * one is spent; without one, "If I had an INK RIBBON...". Jill needs them: the console (and PC
 * second-run) rule, the user's choice (re1/README.md, 2026-10-31) over the PC's first-run Jill, who
 * saves without. Yes writes the slot, RE1's save screen being just this one place.
 *
 * The slot is the run: the script flags and state block (z_nv_script.c: every story, lock, enemy
 * and item flag), the items and the box (z_nv_items.c), the files found (z_nv_files.c), the rooms'
 * music (z_nv_sound.c), the clock and the saves (z_nv_ending.c), and where she stood (the room's
 * scene, the spot, the facing) with Link's health. It lives next to the Ship save it belongs to
 * (Save/nv_re1_<file>.sav: NvSaveFile.cpp).
 *
 * Going into the mansion with the song loads it: back in the room it was written in, where she
 * stood; with no slot, a new run from the foyer. A finished run (the ending) clears it.
 */
#include "global.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

s32 NvSaveFile_Write(s32 fileNum, const void* data, u32 size);
u32 NvSaveFile_Read(s32 fileNum, void* buf, u32 max);
void NvSaveFile_Delete(s32 fileNum);

u32 NvScript_SaveState(u8* p);
void NvScript_LoadState(const u8* p);
void NvScript_NewRun(void);
u32 NvItems_SaveState(u8* p);
void NvItems_LoadState(const u8* p);
void NvItems_NewRun(void);
u32 NvFiles_SaveState(u8* p);
void NvFiles_LoadState(const u8* p);
void NvFiles_NewRun(void);
u32 NvBgm_SaveState(u8* p);
void NvBgm_LoadState(const u8* p);
void NvBgm_NewRun(void);
u32 NvEnding_SaveState(u8* p);
void NvEnding_LoadState(const u8* p);
void NvEnding_NewRun(void);
void NvEnding_CountSave(void);

s32 NvFlags_Test(s32 bank, s32 bit);
s32 NvItems_Find(s32 id);
s32 NvItems_Slot(s32 i, s32* qty);
void NvItems_SpendRound(s32 i);
s32 NvItems_Remove(s32 id);
void NvScript_Ask(PlayState* play, s32 id);
void NvScript_SayItem(PlayState* play, s32 id, s32 item);
void NvMsg_Show(PlayState* play, const char* text);
s16 NvDoor_Entrance(const char* sceneFile, s32 spawn);
void NvSnd_At(s32 slot, s32 x, s32 z, s32 lx, s32 lz);

#define NV_SAVE_MAGIC "NVS1"
#define NV_SAVE_MAX 0x4000
#define NV_ITEM_INK_RIBBON 0x2F

typedef struct {
    char scene[32]; // the room's scene file
    f32 pos[3];
    s16 yaw;
    s16 health;
} NvSaveWhere;

static u8 sNvSaveBuf[NV_SAVE_MAX];
static s32 sNvTypewriter;     // 0 idle, 1 asked, 2 answered
static s32 sNvTypewriterRibbon; // the question was the ribbon's (one is spent: always, as she needs them)
static s32 sNvSavePending;    // a loaded slot's spot, for Link once he's in the room
static NvSaveWhere sNvSaveWhere;

/** The run, to the slot. */
static s32 NvSave_Write(PlayState* play) {
    Player* player = GET_PLAYER(play);
    NvSaveWhere where;
    u8* p = sNvSaveBuf;
    u32 n;

    memset(&where, 0, sizeof(where));
    if ((play->loadedScene != NULL) && (play->loadedScene->sceneFile.fileName != NULL)) {
        snprintf(where.scene, sizeof(where.scene), "%s", play->loadedScene->sceneFile.fileName);
    }
    if (player != NULL) {
        where.pos[0] = player->actor.world.pos.x;
        where.pos[1] = player->actor.world.pos.y;
        where.pos[2] = player->actor.world.pos.z;
        where.yaw = player->actor.shape.rot.y;
    }
    where.health = gSaveContext.health;
    memcpy(p, NV_SAVE_MAGIC, 4);
    p += 4;
    memcpy(p, &where, sizeof(where));
    p += sizeof(where);
    n = NvScript_SaveState(NULL) + NvItems_SaveState(NULL) + NvFiles_SaveState(NULL) + NvBgm_SaveState(NULL) +
        NvEnding_SaveState(NULL);
    if ((u32)(p - sNvSaveBuf) + n > sizeof(sNvSaveBuf)) {
        return false;
    }
    p += NvScript_SaveState(p);
    p += NvItems_SaveState(p);
    p += NvFiles_SaveState(p);
    p += NvBgm_SaveState(p);
    p += NvEnding_SaveState(p);
    n = (u32)(p - sNvSaveBuf);
    printf("NVSAVE wrote %u bytes: %s at %.0f,%.0f,%.0f\n", (unsigned)n, where.scene, where.pos[0], where.pos[1],
           where.pos[2]);
    fflush(stdout);
    return NvSaveFile_Write(gSaveContext.fileNum, sNvSaveBuf, n);
}

/** A new run: a new game's flags, nothing carried, the clock at 0. */
static void NvSave_NewRun(void) {
    NvScript_NewRun();
    NvItems_NewRun();
    NvFiles_NewRun();
    NvBgm_NewRun();
    NvEnding_NewRun();
    sNvSavePending = false;
}

/**
 * Going into the mansion (z_nv_script.c NvWarp_SongEntrance): the slot's room's entrance, the run
 * loaded from it; or -1 when there's no slot (a new run: the foyer).
 */
s16 NvSave_Enter(void) {
    u32 n = NvSaveFile_Read(gSaveContext.fileNum, sNvSaveBuf, sizeof(sNvSaveBuf));
    const u8* p = sNvSaveBuf + 4;
    u32 need = 4 + sizeof(NvSaveWhere) + NvScript_SaveState(NULL) + NvItems_SaveState(NULL) +
               NvFiles_SaveState(NULL) + NvBgm_SaveState(NULL) + NvEnding_SaveState(NULL);
    s16 entrance;

    if ((n < need) || (memcmp(sNvSaveBuf, NV_SAVE_MAGIC, 4) != 0)) {
        printf("NVSAVE none for file %d: a new run\n", (int)gSaveContext.fileNum);
        fflush(stdout);
        NvSave_NewRun();
        return -1;
    }
    memcpy(&sNvSaveWhere, p, sizeof(sNvSaveWhere));
    p += sizeof(sNvSaveWhere);
    sNvSaveWhere.scene[sizeof(sNvSaveWhere.scene) - 1] = '\0';
    entrance = NvDoor_Entrance(sNvSaveWhere.scene, 0);
    if (entrance < 0) {
        printf("NVSAVE room %s isn't in this build: a new run\n", sNvSaveWhere.scene);
        fflush(stdout);
        NvSave_NewRun();
        return -1;
    }
    NvScript_LoadState(p);
    p += NvScript_SaveState(NULL);
    NvItems_LoadState(p);
    p += NvItems_SaveState(NULL);
    NvFiles_LoadState(p);
    p += NvFiles_SaveState(NULL);
    NvBgm_LoadState(p);
    p += NvBgm_SaveState(NULL);
    NvEnding_LoadState(p);
    sNvSavePending = true;
    printf("NVSAVE loaded: %s\n", sNvSaveWhere.scene);
    fflush(stdout);
    return entrance;
}

/** The run's over (the ending): its slot goes, the next time in is a new run. */
void NvSave_Finished(void) {
    NvSaveFile_Delete(gSaveContext.fileNum);
}

/** The room action's handler 0x10 (check_typewriter): A at a typewriter. */
void NvSave_Typewriter(PlayState* play) {
    if (sNvTypewriter != 0) {
        return;
    }
    if (NvItems_Find(NV_ITEM_INK_RIBBON) >= 0) {
        NvScript_Ask(play, 0xC0 | 0x1F); // "You can save your progress with this. Will you use the INK RIBBON?"
        sNvTypewriterRibbon = true;
    } else {
        NvScript_SayItem(play, 0xC0 | 0x1E, 0); // "It's an old typewriter. If I had an INK RIBBON..."
        return;
    }
    sNvTypewriter = 1;
}

/** Each frame (z_nv_script.c NvScript_Update): the typewriter's answer, and a loaded slot's spot. */
void NvSave_Update(PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (sNvSavePending && (player != NULL) && (play->sceneNum >= SCENE_RE1_100)) {
        sNvSavePending = false;
        player->actor.world.pos.x = player->actor.home.pos.x = sNvSaveWhere.pos[0];
        player->actor.world.pos.y = player->actor.home.pos.y = sNvSaveWhere.pos[1];
        player->actor.world.pos.z = player->actor.home.pos.z = sNvSaveWhere.pos[2];
        player->actor.prevPos = player->actor.world.pos;
        player->actor.shape.rot.y = player->actor.world.rot.y = sNvSaveWhere.yaw;
        player->yaw = sNvSaveWhere.yaw;
        if (sNvSaveWhere.health > 0) {
            gSaveContext.health = sNvSaveWhere.health;
        }
    }
    switch (sNvTypewriter) {
        case 1:
            if ((Message_GetState(&play->msgCtx) == TEXT_STATE_CHOICE) && Message_ShouldAdvance(play)) {
                s32 yes = (play->msgCtx.choiceIndex == 0);

                Message_CloseTextbox(play);
                sNvTypewriter = 0;
                if (!yes) {
                    break;
                }
                if (sNvTypewriterRibbon) { // one ribbon spent
                    s32 slot = NvItems_Find(NV_ITEM_INK_RIBBON);
                    s32 qty = 0;

                    NvItems_Slot(slot, &qty);
                    if (qty > 1) {
                        NvItems_SpendRound(slot);
                    } else {
                        NvItems_Remove(NV_ITEM_INK_RIBBON);
                    }
                }
                NvEnding_CountSave();
                if (NvSave_Write(play)) {
                    sNvTypewriter = 2;
                } else {
                    NvMsg_Show(play, "The save couldn't be written.");
                }
            } else if (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE) {
                sNvTypewriter = 0; // (closed without an answer)
            }
            break;
        case 2: // (RE1's save screen: here a line)
            if (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE) {
                NvMsg_Show(play, "Your progress has been saved.");
                sNvTypewriter = 0;
            }
            break;
    }
}
