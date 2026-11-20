/*
 * Nightmareverse: Majora's Mask in OoT, the per-frame entry for Termina (z_play.c calls these two).
 * Everything here acts in MM's scenes only (NvMm_IsScene).
 *
 * - the 3-day clock (z_nv_mm_clock.c), and MM's half-days in the rooms (z_nv_mm_halfday.c);
 * - exits into places not built yet: mm_scene.py points them at a "back" entrance
 *   (ENTR_MM_BACK_FIRST..LAST, nv_mm_scene_range.h), which puts Link at the spot that place's own
 *   exit would bring him to, and here a note says the way isn't built, as RE1's unbuilt doors do.
 *
 * Dev: NV_MM_SAVETEST=save saves to file 1 (SoH's own save, sections and all) 100 frames into
 * a Termina scene; NV_MM_SAVETEST=load loads file 1 on the first Termina frame; "load,save" does
 * both, the save in a later scene (after an NV_WALK entr: step). The "termina" section's round
 * trip, headless. Logs "NVMM ...".
 */
#include "global.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void NvMmClock_Update(PlayState* play);
void NvMmClock_Draw(PlayState* play);
void NvMmHalfDay_Update(PlayState* play);
void NvMsg_Show(PlayState* play, const char* text);
void Save_SaveFile(void);
void Save_LoadFile(void);
void NvMmClock_LogState(const char* when);

static void NvMm_SaveTest(PlayState* play) {
    static s32 sLoaded;
    static s32 sSaved;
    const char* t = getenv("NV_MM_SAVETEST");

    if (t == NULL) {
        return;
    }
    if ((strstr(t, "load") != NULL) && !sLoaded) {
        sLoaded = true;
        gSaveContext.fileNum = 0;
        Save_LoadFile();
        { // a load mid-scene: the scene's live flags from the loaded save, as a scene load would
            SavedSceneFlags* f = NvScene_SavedFlags(play->sceneNum);

            play->actorCtx.flags.chest = f->chest;
            play->actorCtx.flags.swch = f->swch;
            play->actorCtx.flags.clear = f->clear;
            play->actorCtx.flags.collect = f->collect;
        }
        NvMmClock_LogState("after loading file 1");
        printf("NVMM loaded: Hyrule's time in the base save is %04X\n", (unsigned)gSaveContext.dayTime);
        fflush(stdout);
    } else if ((strstr(t, "save") != NULL) && !sSaved && (play->state.frames == 100)) {
        sSaved = true;
        gSaveContext.fileNum = 0;
        NvMmClock_LogState("saving to file 1");
        Play_SaveSceneFlags(play); // (as the pause menu's save does first)
        Save_SaveFile();
    }
}

static u32 sNvMmFrames;       // play->state.frames last frame (it starts over with each scene load)
static s32 sNvMmBackNote;     // a "back" arrival waiting for the fade to finish

void NvMm_Update(PlayState* play) {
    if (!NvMm_IsScene(play->sceneNum)) {
        sNvMmFrames = 0;
        NvMmClock_Update(play);
        return;
    }
    if ((sNvMmFrames == 0) || (play->state.frames + 1 < sNvMmFrames)) { // a new scene load
        sNvMmBackNote = (gSaveContext.entranceIndex >= ENTR_MM_BACK_FIRST) &&
                        (gSaveContext.entranceIndex <= ENTR_MM_BACK_LAST);
        if (sNvMmBackNote && getenv("NV_LOG")) {
            printf("NVMM back: entrance 0x%X (a way not built yet)\n", (unsigned)gSaveContext.entranceIndex);
            fflush(stdout);
        }
    }
    if (sNvMmBackNote && (play->transitionMode == TRANS_MODE_OFF) && (play->msgCtx.msgMode == MSGMODE_NONE) &&
        (play->state.frames > 20)) {
        sNvMmBackNote = false;
        if (getenv("NV_LOG")) {
            printf("NVMM back: note shown\n");
            fflush(stdout);
        }
        NvMsg_Show(play, "The rest of Termina isn't built yet.&You turn back.");
    }
    sNvMmFrames = play->state.frames + 1; // (+1: never 0 once in Termina)
    NvMmClock_Update(play);
    NvMmHalfDay_Update(play);
    NvMm_SaveTest(play);
}

void NvMm_Draw(PlayState* play) {
    NvMmClock_Draw(play);
}
