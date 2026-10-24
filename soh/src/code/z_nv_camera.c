/*
 * Nightmareverse: RE1's camera switching.
 *
 * RE1 doesn't pick its camera from the floor under the player (OoT's way). Its switch-zone
 * table (RDT+0x48) is grouped by camera: the first record of each group is the camera's own,
 * the rest are quads that, while that camera is on, cut to their camTo when the player enters
 * them (check_camera_switch / is_entity_in_switch_zone, RE1's PC code 0x00462cc0 / 0x00462d90).
 * Coming through a door, the room opens on the camera in the door's byte 13 (low 6 bits).
 * z_camera.c asks NvCamera_Select for the camera in rooms that have the table; it carries the
 * current camera from frame to frame as RE1 does.
 *
 * Data: "scenes/shared/<name>_scene/<name>_camzones" (re1/tools/soh_scene.py): "RCZN", u32
 * count, then RE1's own 20-byte records (s16 camTo, camFrom, then 4 corners as u16 x, z in
 * RE1 units).
 */
#include "global.h"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>
#include <stdio.h>
#include <string.h>

#define NV_RE1_SCALE 0.0217f // OoT units per RE1 unit (re1/tools/rdt2scene.py SCALE)

typedef struct {
    s16 camTo;
    s16 camFrom;
    u16 x[4];
    u16 z[4];
} NvCamZone;

static s16 sNvCamScene = -1;
static s32 sNvCamCurrent = -1;
static s32 sNvCamArrival = -1; // set by the door Link came through (z_nv_door.c)

/** The camera the next room opens on (a door's byte 13, low 6 bits); -1: the start position's. */
void NvCamera_SetArrival(s32 cam) {
    sNvCamArrival = cam;
}

static u8* NvCamera_Zones(PlayState* play, u32* count) {
    char path[256];
    const char* file;
    size_t len;
    u8* data;

    if ((play->loadedScene == NULL) || (play->loadedScene->sceneFile.fileName == NULL)) {
        return NULL;
    }
    file = play->loadedScene->sceneFile.fileName;
    len = strlen(file);
    if ((len <= 6) || (strcmp(file + len - 6, "_scene") != 0)) {
        return NULL;
    }
    snprintf(path, sizeof(path), "scenes/shared/%s/%.*s_camzones", file, (int)(len - 6), file);
    if (!ResourceMgr_FileExists(path)) {
        return NULL;
    }
    data = (u8*)ResourceGetDataByName(path);
    if ((data == NULL) || (memcmp(data, "RCZN", 4) != 0)) {
        return NULL;
    }
    memcpy(count, data + 4, sizeof(*count));
    return data + 8;
}

// is_entity_in_switch_zone: the quad's corners zero-extended, the point a signed RE1 position
static s32 NvCamera_InZone(const u8* rec, s32 px, s32 pz) {
    u16 c[8];
    s32 x0;
    s32 z0;
    s32 x2;
    s32 z2;

    memcpy(c, rec + 4, sizeof(c));
    x0 = c[0];
    z0 = c[1];
    if ((c[2] - x0) * (pz - z0) > (c[3] - z0) * (px - x0)) {
        return false;
    }
    if ((c[6] - x0) * (pz - z0) < (c[7] - z0) * (px - x0)) {
        return false;
    }
    x2 = c[4];
    z2 = c[5];
    if ((c[2] - x2) * (pz - z2) < (c[3] - z2) * (px - x2)) {
        return false;
    }
    if ((c[6] - x2) * (pz - z2) > (c[7] - z2) * (px - x2)) {
        return false;
    }
    return true;
}

/**
 * The camera for this frame. floorCam is OoT's pick (the floor under Link); returned unchanged
 * outside RE1 rooms. Called from z_camera.c.
 */
s32 NvCamera_Select(PlayState* play, s32 floorCam) {
    Player* player = GET_PLAYER(play);
    u8* zones;
    u32 count;
    u32 i;
    s32 px;
    s32 pz;
    s32 inGroup = false;

    zones = NvCamera_Zones(play, &count);
    if ((zones == NULL) || (player == NULL)) {
        sNvCamScene = -1;
        return floorCam;
    }
    if (play->sceneNum != sNvCamScene) { // a new room: the door's camera, else the start position's
        sNvCamScene = play->sceneNum;
        sNvCamCurrent = (sNvCamArrival >= 0) ? sNvCamArrival : floorCam;
        sNvCamArrival = -1;
    }
    if (sNvCamCurrent < 0) {
        sNvCamCurrent = floorCam;
    }
    // Link in RE1 units (OoT = (x, -y, -z) * scale)
    px = (s32)(player->actor.world.pos.x / NV_RE1_SCALE);
    pz = (s32)(-player->actor.world.pos.z / NV_RE1_SCALE);
    for (i = 0; i < count; i++) {
        const u8* rec = zones + i * 20;
        s16 camTo;
        s16 camFrom;

        memcpy(&camTo, rec, 2);
        memcpy(&camFrom, rec + 2, 2);
        if (camFrom != sNvCamCurrent) {
            if (inGroup) {
                break; // past this camera's group
            }
            continue;
        }
        if (!inGroup) { // the group's first record is the camera's own: not a switch zone
            inGroup = true;
            continue;
        }
        if (NvCamera_InZone(rec, px, pz)) {
            sNvCamCurrent = camTo;
            break;
        }
    }
    return sNvCamCurrent;
}
