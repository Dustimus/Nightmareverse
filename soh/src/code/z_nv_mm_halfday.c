/*
 * Nightmareverse: MM's half-days in Termina's rooms (mm/README.md, "MM's half-days").
 *
 * Every actor MM places has ten half-day bits (day 0 night to day 4 night): MM spawns it only in
 * those half-days, and as the half-day turns (6:00, 18:00) spawns the entries newly due and removes
 * the ones whose time is up (z_actor.c, Actor_SpawnSetupActors / the half-day check). Our own
 * code to MM's rules:
 * - mm_scene.py writes each room's masks beside it, "<room>_halfdays" ("MHDY", a count, then one
 *   u16 per entry of the room's actor list, 0 = always);
 * - OoT's room spawn (z_actor.c, one hook) asks NvMmHalfDay_Skip for each entry;
 * - NvMmHalfDay_Update (z_nv_mm.c, every frame in Termina) swaps them as the half-day turns:
 *   it removes the room's actors whose entry isn't due any more (found by SoH's actor list index)
 *   and spawns the entries due now that weren't before, as MM does (one broken pot stays broken).
 */
#include "global.h"
#include "soh/ObjectExtension/ActorListIndex.h"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

u16 NvMmClock_HalfDayBit(void);

static const u16* sMasks;    // the room's masks, one per actor list entry
static s32 sCount;
static s32 sRoom = -1;       // the room they belong to
static PlayState* sPlay;
static s16 sScene = -1;      // ...and their scene (the play state's address is reused between scenes)
static u16 sHalfDay;         // the half-day the room's actors were last placed for

static void NvMmHalfDay_Load(PlayState* play) {
    char path[256];
    const char* file;
    size_t len;
    const u8* data;

    sMasks = NULL;
    sCount = 0;
    sRoom = play->roomCtx.curRoom.num;
    sPlay = play;
    sScene = play->sceneNum;
    if ((play->loadedScene == NULL) || ((file = play->loadedScene->sceneFile.fileName) == NULL)) {
        return;
    }
    len = strlen(file);
    if ((len <= 6) || (strcmp(file + len - 6, "_scene") != 0)) {
        return;
    }
    snprintf(path, sizeof(path), "scenes/shared/%s/%.*s_room_%02d_halfdays", file, (int)(len - 6), file, sRoom);
    if (!ResourceMgr_FileExists(path)) {
        return;
    }
    data = (const u8*)ResourceGetDataByName(path);
    if ((data == NULL) || (memcmp(data, "MHDY", 4) != 0)) {
        return;
    }
    memcpy(&sCount, data + 4, sizeof(sCount));
    sMasks = (const u16*)(data + 8);
}

static s32 NvMmHalfDay_Due(s32 i, u16 bit) {
    u16 mask = ((sMasks != NULL) && (i < sCount)) ? sMasks[i] : 0;

    return (mask == 0) || (mask & bit);
}

/** OoT's room spawn (z_actor.c): true to leave entry i out, as not due in this half-day. */
s32 NvMmHalfDay_Skip(PlayState* play, s32 i) {
    if (!NvMm_IsScene(play->sceneNum)) {
        return false;
    }
    if ((i == 0) || (sPlay != play) || (sScene != play->sceneNum) || (sRoom != play->roomCtx.curRoom.num)) {
        NvMmHalfDay_Load(play);
        sHalfDay = NvMmClock_HalfDayBit();
    }
    return !NvMmHalfDay_Due(i, sHalfDay);
}

/** Every frame in Termina (z_nv_mm.c): the swap as the half-day turns. */
void NvMmHalfDay_Update(PlayState* play) {
    u16 now = NvMmClock_HalfDayBit();
    u16 before = sHalfDay;
    s32 cat;
    s32 i;
    s32 removed = 0;
    s32 spawned = 0;

    if ((sMasks == NULL) || (sPlay != play) || (sScene != play->sceneNum) || (sRoom != play->roomCtx.curRoom.num) ||
        (now == before) ||
        (play->setupActorList == NULL)) {
        sHalfDay = now;
        return;
    }
    sHalfDay = now;
    for (cat = 0; cat < ACTORCAT_MAX; cat++) { // out of their time
        Actor* actor;

        for (actor = play->actorCtx.actorLists[cat].head; actor != NULL; actor = actor->next) {
            s16 i = GetActorListIndex(actor);

            if ((i >= 0) && (i < sCount) && !NvMmHalfDay_Due(i, now)) {
                Actor_Kill(actor);
                removed++;
            }
        }
    }
    for (i = 0; i < sCount; i++) { // newly due
        if (NvMmHalfDay_Due(i, now) && !NvMmHalfDay_Due(i, before)) {
            Actor* a = Actor_SpawnEntry(&play->actorCtx, &play->setupActorList[i], play);

            SetActorListIndex(a, (s16)i);
            spawned += (a != NULL);
        }
    }
    if (getenv("NV_LOG")) {
        printf("NVMM half-day %03X -> %03X (room %d, %d entries): %d removed, %d spawned\n", (unsigned)before,
               (unsigned)now, (int)sRoom, (int)sCount, (int)removed, (int)spawned);
        fflush(stdout);
    }
}
