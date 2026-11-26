/*
 * Nightmareverse: Termina's per-scene flags, saved (mm/README.md, "Saved pickups").
 *
 * OoT keeps per-scene flags (chests, switches, cleared rooms, collectibles, visited rooms) for its own
 * 124 scenes; scenes added after them get scratch flags (z_nv_scene.c). Termina's scenes get these
 * instead, kept in the "termina" save section (NvMmSave.cpp), in its two parts:
 *   permanent  collectibles (heart pieces and the like), visited rooms and floors: never reset;
 *   cycle      chests, switches, cleared rooms: reset by the Song of Time, as MM resets them.
 * A chest the Song of Time refills will give something else once its randomizer check is taken
 * (decided with the user, 2026-11-19); for now chests aren't brought over.
 */
#include "global.h"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>

#define NV_MM_SCENES (SCENE_MM_LAST - SCENE_MM_FIRST + 1)

static SavedSceneFlags sMmFlags[NV_MM_SCENES];

/** z_nv_scene.c: a Termina scene's saved flags. */
SavedSceneFlags* NvMmScene_SavedFlags(s16 sceneNum) {
    return &sMmFlags[sceneNum - SCENE_MM_FIRST];
}

static void NvMmScene_ClearCycle(SavedSceneFlags* f) {
    f->chest = 0;
    f->swch = 0;
    f->clear = 0;
    f->unk = 0;
}

/** The Song of Time (and the moon's fall): every Termina scene's cycle flags go, and the scene Link
 * is in drops its own live ones too, so leaving it doesn't write them back. */
void NvMm_WeekEventsNewCycle(void);
void NvMm_WeekEventsNewGame(void);

void NvMmItem_NewCycle(void);
void NvMmItem_NewGame(void);
void NvMmMask_NewGame(void);
void NvMmForm_NewGame(void);

void NvMmScene_SongOfTime(PlayState* play) {
    s32 i;

    NvMm_WeekEventsNewCycle();
    NvMmItem_NewCycle(); // (z_nv_mm_getitem.c: MM's items the cycle takes)

    for (i = 0; i < NV_MM_SCENES; i++) {
        NvMmScene_ClearCycle(&sMmFlags[i]);
    }
    if ((play != NULL) && NvMm_IsScene(play->sceneNum)) {
        play->actorCtx.flags.chest = 0;
        play->actorCtx.flags.swch = 0;
        play->actorCtx.flags.clear = 0;
        play->actorCtx.flags.tempSwch = 0;
        play->actorCtx.flags.tempClear = 0;
    }
}

/** A new game, or a save from before Termina. */
void NvMmScene_NewGame(void) {
    NvMm_WeekEventsNewGame();
    NvMmItem_NewGame();
    NvMmMask_NewGame(); // (no MM mask worn)
    NvMmForm_NewGame(); // (human)
    memset(sMmFlags, 0, sizeof(sMmFlags));
}

/* NvMmSave.cpp: the scenes by name (the table's file names), so saves don't depend on numbering */
s32 NvMmScene_Count(void) {
    return NV_MM_SCENES;
}

const char* NvMmScene_Name(s32 i) {
    return gSceneTable[SCENE_MM_FIRST + i].sceneFile.fileName;
}

SavedSceneFlags* NvMmScene_At(s32 i) {
    return &sMmFlags[i];
}

/*
 * MM's week event flags (the save's weekEventReg: 100 bytes of bits, the decomp's WEEKEVENTREG_*,
 * byte and bit as MM numbers them, e.g. WEEKEVENTREG_12_20 = byte 12, bit 0x20). Termina's actors set
 * and test them as MM's do. Kept in the "termina" save section's cycle part, so the Song of Time
 * clears them, as MM clears most; the few MM keeps across cycles get kept here as they come up.
 */
static u8 sMmWeekEventReg[100];

s32 NvMm_CheckWeekEvent(s32 flag) { // flag = (byte << 8) | bit, as MM's WEEKEVENTREG_* values are
    return (sMmWeekEventReg[(flag >> 8) % 100] & (flag & 0xFF)) != 0;
}

void NvMm_SetWeekEvent(s32 flag) {
    sMmWeekEventReg[(flag >> 8) % 100] |= (flag & 0xFF);
}

void NvMm_ClearWeekEvent(s32 flag) {
    sMmWeekEventReg[(flag >> 8) % 100] &= ~(flag & 0xFF);
}

u8* NvMm_WeekEventReg(void) { // (NvMmSave.cpp)
    return sMmWeekEventReg;
}

// MM's sPersistentCycleWeekEventRegs (z_sram_NES.c): the week events a new cycle keeps, as a mask per
// byte (MM packs two bits a flag; any set keeps it): the Bombers' Notebook's, the bosses beaten,
// the owl statues, ... (from the decomp's table, by its flags' names)
static const u8 sPersistentWeekEventRegs[100] = {
    0xFE, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x80, 0x20, 0x00, 0x08, 0x00, 0x10, 0x40, 0x80, 0x20, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x82, 0x82, 0x82, 0x01, 0x40, 0x00, 0x00, 0x00, 0xF0, 0x27, 0x07, 0x00, 0x00, 0xBF,
    0x00, 0x00, 0x20, 0x20, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x04, 0x00, 0x42,
    0x40, 0x00, 0x02, 0x08, 0x00, 0x3C, 0x10, 0x00, 0x00, 0x20, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x08, 0x00, 0x80, 0x00, 0x01, 0x00, 0x80, 0x00, 0x08, 0x10, 0x00, 0x00, 0x00, 0x80, 0xFC, 0x00, 0x00,
    0x10, 0x00, 0x80, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/** The Song of Time: MM's week events, but for those MM keeps across cycles. */
void NvMm_WeekEventsNewCycle(void) {
    s32 i;

    for (i = 0; i < 100; i++) {
        sMmWeekEventReg[i] &= sPersistentWeekEventRegs[i];
    }
}

/** A new game: none. */
void NvMm_WeekEventsNewGame(void) {
    const char* dev = getenv("NV_MM_WE"); // dev: NV_MM_WE="85:02,73:80" week events set with a new game
    int byte;
    int bit;

    memset(sMmWeekEventReg, 0, sizeof(sMmWeekEventReg));
    while ((dev != NULL) && (sscanf(dev, "%d:%x", &byte, &bit) == 2)) {
        if ((byte >= 0) && (byte < 100)) {
            sMmWeekEventReg[byte] |= bit;
        }
        dev = strchr(dev, ',');
        dev = (dev != NULL) ? dev + 1 : NULL;
    }
}

// MM's animated materials (z_scene_proc.c AnimatedMat_*), from the scene's "_matanim" resource
// (mm_scene.py): read once per scene.
static s16 sMatScene = -1;
static const u8* sMat; // "MMAT", a count, then the entries
static s32 sMatCount;

static void NvMmScene_LoadMatAnim(PlayState* play) {
    char path[160];
    const char* file;
    const u8* data;

    if (sMatScene == play->sceneNum) {
        return;
    }
    sMatScene = play->sceneNum;
    sMat = NULL;
    sMatCount = 0;
    if ((play->loadedScene == NULL) || ((file = play->loadedScene->sceneFile.fileName) == NULL)) {
        return;
    }
    snprintf(path, sizeof(path), "scenes/shared/%s/%s_matanim", file, file);
    if (!ResourceMgr_FileExists(path)) {
        return;
    }
    data = (const u8*)ResourceGetDataByName(path);
    if ((data == NULL) || (memcmp(data, "MMAT", 4) != 0)) {
        return;
    }
    memcpy(&sMatCount, data + 4, sizeof(sMatCount));
    sMat = data + 8;
}

static void NvMmScene_SetSegment(PlayState* play, s32 segment, void* ptr) {
    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, segment, ptr);
    gSPSegment(POLY_XLU_DISP++, segment, ptr);
    CLOSE_DISPS(play->state.gfxCtx);
}

/** MM's AnimatedMat_SetColor: a list setting the prim (and env) color, into the segment. */
static void NvMmScene_SetColor(PlayState* play, s32 segment, const u8* prim, const u8* env) {
    Gfx* gfx = Graph_Alloc(play->state.gfxCtx, 3 * sizeof(Gfx));

    NvMmScene_SetSegment(play, segment, gfx);
    gDPSetPrimColor(gfx++, 0, prim[4], prim[0], prim[1], prim[2], prim[3]);
    if (env != NULL) {
        gDPSetEnvColor(gfx++, env[0], env[1], env[2], env[3]);
    }
    gSPEndDisplayList(gfx++);
}

static s32 NvMmScene_Lerp(s32 min, s32 max, f32 norm) {
    return (s32)((max - min) * norm) + min;
}

/** MM's Scene_LagrangeInterp. */
static f32 NvMmScene_Lagrange(s32 n, f32 x[], f32 fx[], f32 xp) {
    f32 weights[50];
    f32 intp = 0.0f;
    s32 i;
    s32 j;

    for (i = 0; i < n; i++) {
        f32 m = 1.0f;

        for (j = 0; j < n; j++) {
            if (j != i) {
                m *= x[i] - x[j];
            }
        }
        weights[i] = fx[i] / m;
    }
    for (i = 0; i < n; i++) {
        f32 m = 1.0f;

        for (j = 0; j < n; j++) {
            if (j != i) {
                m *= xp - x[j];
            }
        }
        intp += weights[i] * m;
    }
    return intp;
}

/** The color kinds (MM's AnimatedMat_DrawColor, _DrawColorLerp, _DrawColorNonLinearInterp). */
static void NvMmScene_DrawColor(PlayState* play, s32 segment, s32 type, const u8* d, u32 step) {
    u16 klen;
    u16 kcount;
    s32 hasEnv = d[4];
    const u8* prims = d + 5;
    const u8* envs;
    const u8* keyBytes;
    s32 n;
    s32 curFrame;
    u8 prim[5];
    u8 env[4];
    s32 c;

    memcpy(&klen, d, 2);
    memcpy(&kcount, d + 2, 2);
    if ((klen == 0) || ((type != 2) && (kcount == 0))) { // (type 2, one colour a frame, has no key frames)
        return;
    }
    n = (type == 2) ? klen : kcount;
    envs = hasEnv ? (prims + 5 * n) : NULL;
    keyBytes = prims + 5 * n + (hasEnv ? 4 * n : 0);
    curFrame = step % klen;
    if (type == 2) {
        NvMmScene_SetColor(play, segment, prims + 5 * curFrame, hasEnv ? (envs + 4 * curFrame) : NULL);
    } else if (type == 3) {
        s32 i = 1;
        u16 key;
        u16 prevKey;
        f32 norm;

        while (i < kcount) {
            memcpy(&key, keyBytes + 2 * i, 2);
            if (curFrame < key) {
                break;
            }
            i++;
        }
        i = MIN(i, kcount - 1);
        memcpy(&key, keyBytes + 2 * i, 2);
        memcpy(&prevKey, keyBytes + 2 * (i - 1), 2);
        norm = (key != prevKey) ? ((f32)(curFrame - prevKey) / (f32)(key - prevKey)) : 0.0f;
        for (c = 0; c < 5; c++) {
            prim[c] = NvMmScene_Lerp(prims[5 * (i - 1) + c], prims[5 * i + c], norm);
        }
        if (hasEnv) {
            for (c = 0; c < 4; c++) {
                env[c] = NvMmScene_Lerp(envs[4 * (i - 1) + c], envs[4 * i + c], norm);
            }
        }
        NvMmScene_SetColor(play, segment, prim, hasEnv ? env : NULL);
    } else {
        f32 x[50];
        f32 fx[50];
        s32 k;

        n = MIN(kcount, 50);
        for (k = 0; k < n; k++) {
            u16 key;

            memcpy(&key, keyBytes + 2 * k, 2);
            x[k] = key;
        }
        for (c = 0; c < 5; c++) {
            for (k = 0; k < n; k++) {
                fx[k] = prims[5 * k + c];
            }
            prim[c] = CLAMP((s32)NvMmScene_Lagrange(n, x, fx, curFrame), 0, 255);
        }
        if (hasEnv) {
            for (c = 0; c < 4; c++) {
                for (k = 0; k < n; k++) {
                    fx[k] = envs[4 * k + c];
                }
                env[c] = CLAMP((s32)NvMmScene_Lagrange(n, x, fx, curFrame), 0, 255);
            }
        }
        NvMmScene_SetColor(play, segment, prim, hasEnv ? env : NULL);
    }
}

/** MM's AnimatedMat_Draw for a list of "MMAT" entries (mm_scene.py matanim_blob): the step is the
 * frame count. */
static void NvMmMatAnim_DrawEntriesStep(PlayState* play, const u8* entries, s32 count, u32 step);

static void NvMmMatAnim_DrawEntries(PlayState* play, const u8* entries, s32 count) {
    NvMmMatAnim_DrawEntriesStep(play, entries, count, play->gameplayFrames);
}

/** The same at a given step (MM's AnimatedMat_DrawStep: an actor picking its frame, e.g. its colours). */
static void NvMmMatAnim_DrawEntriesStep(PlayState* play, const u8* entries, s32 count, u32 step) {
    const u8* e;
    s32 i;

    for (i = 0, e = entries; (e != NULL) && (i < count); i++) {
        s8 segment = (s8)e[0];
        u8 type = e[1];
        u16 len;
        const u8* d = e + 4;
        s32 segAbs = ABS(segment) + 7;

        memcpy(&len, e + 2, 2);
        switch (type) {
            case 0: // MM's AnimatedMat_DrawTexScroll
                NvMmScene_SetSegment(play, segAbs,
                                     Gfx_TexScroll(play->state.gfxCtx, (s8)d[0] * step, -((s8)d[1] * step), d[2],
                                                   d[3]));
                break;
            case 1: // MM's AnimatedMat_DrawTwoTexScroll
                NvMmScene_SetSegment(play, segAbs,
                                     Gfx_TwoTexScroll(play->state.gfxCtx, 0, (s8)d[0] * step, -((s8)d[1] * step),
                                                      d[2], d[3], 1, (s8)d[4] * step, -((s8)d[5] * step), d[6],
                                                      d[7]));
                break;
            case 2:
            case 3:
            case 4:
                NvMmScene_DrawColor(play, segAbs, type, d, step);
                break;
            default: // (5, the texture cycle: none of our scenes has one yet)
                break;
        }
        e = d + len;
    }
}

/** An object's animated materials (MM's AnimatedMat_Draw in an actor's draw): `path` its "MMAT"
 * blob (mm_objects.py "matanim"; the resource manager keeps it loaded). */
void NvMmMatAnim_Draw(PlayState* play, const char* path) {
    const u8* data;
    s32 count;

    if (!ResourceMgr_FileExists(path) || ((data = (const u8*)ResourceGetDataByName(path)) == NULL) ||
        (memcmp(data, "MMAT", 4) != 0)) {
        return;
    }
    memcpy(&count, data + 4, sizeof(count));
    NvMmMatAnim_DrawEntries(play, data + 8, count);
}

/** An object's animated materials at a fixed step (MM's AnimatedMat_DrawStepOpa). */
void NvMmMatAnim_DrawStep(PlayState* play, const char* path, u32 step) {
    const u8* data;
    s32 count;

    if (!ResourceMgr_FileExists(path) || ((data = (const u8*)ResourceGetDataByName(path)) == NULL) ||
        (memcmp(data, "MMAT", 4) != 0)) {
        return;
    }
    memcpy(&count, data + 4, sizeof(count));
    NvMmMatAnim_DrawEntriesStep(play, data + 8, count, step);
}

/** After the scene's draw config (Scene_Draw, z_scene_table.c): MM's rooms call display lists in
 * segments 8-0xD, which MM's scenes fill with their animated materials (MM's
 * Scene_DrawConfigMatAnim, AnimatedMat_Draw: the step is the frame count); the ones a scene sets
 * none in are an empty list, as in MM. Set here for Termina's scenes, before the rooms draw. */
void NvMmScene_Draw(PlayState* play) {
    static Gfx sEmptyDL[] = { gsSPEndDisplayList() };
    s32 seg;

    if (!NvMm_IsScene(play->sceneNum)) {
        return;
    }
    for (seg = 0x08; seg <= 0x0D; seg++) {
        NvMmScene_SetSegment(play, seg, sEmptyDL);
    }
    NvMmScene_LoadMatAnim(play);
    NvMmMatAnim_DrawEntries(play, sMat, sMatCount);
}
