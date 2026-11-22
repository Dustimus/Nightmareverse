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

void NvMmScene_SongOfTime(PlayState* play) {
    s32 i;

    NvMm_WeekEventsNewCycle();

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
    memset(sMmWeekEventReg, 0, sizeof(sMmWeekEventReg));
}

/** After the scene's draw config (Scene_Draw, z_scene_table.c): MM's rooms call display lists in
 * segments 8-0xD, which MM's scenes fill with their animated materials (MM's
 * Scene_DrawConfigMatAnim); where a scene sets none, MM leaves them an empty list. Set here for
 * Termina's scenes, before the rooms draw. */
void NvMmScene_Draw(PlayState* play) {
    static Gfx sEmptyDL[] = { gsSPEndDisplayList() };
    s32 seg;

    if (!NvMm_IsScene(play->sceneNum)) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    for (seg = 0x08; seg <= 0x0D; seg++) {
        gSPSegment(POLY_OPA_DISP++, seg, sEmptyDL);
        gSPSegment(POLY_XLU_DISP++, seg, sEmptyDL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}
