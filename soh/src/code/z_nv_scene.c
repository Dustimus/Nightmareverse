/*
 * Nightmareverse: scenes added after OoT's own.
 *
 * OoT's save keeps per-scene flags (chests, switches, clears, collectibles) for its 124 scene
 * slots only. Added scenes past them get a scratch set instead, cleared on each scene load and
 * never saved: their games keep their own state (RE1's story flags and locks come with the
 * core systems).
 */
#include "global.h"
#include <string.h>

static SavedSceneFlags sNvScratchFlags;

SavedSceneFlags* NvScene_SavedFlags(s16 sceneNum) {
    if ((sceneNum >= 0) && (sceneNum < ARRAY_COUNT(gSaveContext.sceneFlags))) {
        return &gSaveContext.sceneFlags[sceneNum];
    }
    memset(&sNvScratchFlags, 0, sizeof(sNvScratchFlags));
    return &sNvScratchFlags;
}
