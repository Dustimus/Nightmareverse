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
void NvMmScene_SongOfTime(PlayState* play) {
    s32 i;

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
