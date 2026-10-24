/*
 * Nightmareverse: RE1's doors.
 *
 * RE1 (Bio.exe 0x456880) opens a door when the action button is pressed and the point 600
 * units in front of the player (13 OoT units) lies in the door's rectangle; a door flagged
 * 0x40 tests the player's own position instead. Nothing happens by walking into a door.
 * Here A does the same, and the door takes Link to the target room's start position for it
 * (re1/tools/soh_scene.py writes the doors, with RE1's arrival points as start positions).
 *
 * Data: "scenes/shared/<name>_scene/<name>_doors" (soh_scene.door_blob): "RDOR", u32 count;
 * per door f32 x0, z0, x1, z1, u8 flags, u8 anim, u16 spawn, u8 Door_set bytes 10, 11, 13, 15
 * (the door scene's inputs), char target[24].
 *
 * A door with a door scene (z_nv_doorscene.c) cuts straight to it, as RE1 does; any other
 * fades to black.
 *
 * Called from Play_Update (z_play.c) before the actors update, so the A press it takes is not
 * also Link's (no roll into the door).
 */
#include "global.h"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>
#include <stdio.h>
#include <string.h>

typedef struct NvDoor {
    f32 x0;
    f32 z0;
    f32 x1;
    f32 z1;
    u8 flags;
    u8 anim;
    u16 spawn;
    u8 b10; // Door_set bytes 10, 11, 13 and 15: what RE1's door scene reads (z_nv_doorscene.c)
    u8 b11;
    u8 b13;
    u8 b15;
    char target[24];
} NvDoor;

s32 NvDoorScene_Exists(s32 anim);
void NvCamera_SetArrival(s32 cam);
void NvDoorScene_Queue(s32 anim, s32 b10, s32 b11, s32 b13, s32 b15);
s32 NvClimb_Active(void);
s32 NvClimb_TryStart(PlayState* play, Player* player, f32 nearD, f32 farD);

#define NV_DOOR_ACTION 0x80   // opens with the action button
#define NV_DOOR_AT_PLAYER 0x40 // tested at the player's position, not in front of him
// How far in front of Link the door point is. RE1's 600 units in front of Jill reach about 150
// past her body (13 OoT units against a radius taken as ~450 units, ~10 OoT: RE2's player
// radius, Jill's own still to be read from Bio.exe). Measured from Link's body instead (his wall
// radius: 18 adult, 14 child), as RE1's door rectangles start at the wall's surface and Link
// stops his radius short of it.
#define NV_DOOR_REACH_PAST_BODY 4.0f
// RE1's own point: 600 units in front of the player. A door whose rectangle lies on open floor
// (a doorway, a corner) can be smaller than Link's reach; standing in it, RE1's point is inside
// while the far one is past it. So the door opens if any point from RE1's to Link's reach is in.
#define NV_DOOR_REACH_RE1 13.0f

// The entrance for a scene file's start position (soh_tables.py: four per start position, the
// first is setup 0), or -1
s16 NvDoor_Entrance(const char* sceneFile, s32 spawn) {
    s32 scene;
    s32 i;

    for (scene = 0; scene < SCENE_ID_MAX; scene++) {
        if ((gSceneTable[scene].sceneFile.fileName != NULL) &&
            (strcmp(gSceneTable[scene].sceneFile.fileName, sceneFile) == 0)) {
            break;
        }
    }
    if (scene == SCENE_ID_MAX) {
        return -1;
    }
    for (i = 0; i < ENTR_MAX; i++) {
        if ((gEntranceTable[i].scene == scene) && (gEntranceTable[i].spawn == spawn)) {
            return i;
        }
    }
    return -1;
}

static u8* NvDoor_Data(PlayState* play, u32* count) {
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
    snprintf(path, sizeof(path), "scenes/shared/%s/%.*s_doors", file, (int)(len - 6), file);
    if (!ResourceMgr_FileExists(path)) {
        return NULL;
    }
    data = (u8*)ResourceGetDataByName(path);
    if ((data == NULL) || (memcmp(data, "RDOR", 4) != 0)) {
        return NULL;
    }
    memcpy(count, data + 4, sizeof(*count));
    return data + 8;
}

void NvDoor_Update(PlayState* play) {
    Player* player = GET_PLAYER(play);
    Input* input = &play->state.input[0];
    u8* data;
    u32 count;
    u32 i;
    f32 sx;
    f32 sz;
    f32 reach;

    if ((player == NULL) || !CHECK_BTN_ALL(input->press.button, BTN_A) ||
        (play->transitionTrigger != TRANS_TRIGGER_OFF) || (play->transitionMode != TRANS_MODE_OFF) ||
        Player_InBlockingCsMode(play, player) || (play->msgCtx.msgMode != MSGMODE_NONE) ||
        !(player->actor.bgCheckFlags & 1) || (player->actor.parent != NULL) || NvClimb_Active()) {
        return;
    }
    reach = player->ageProperties->wallCheckRadius + NV_DOOR_REACH_PAST_BODY;
    data = NvDoor_Data(play, &count);
    if (data == NULL) {
        count = 0;
    }
    sx = Math_SinS(player->actor.shape.rot.y);
    sz = Math_CosS(player->actor.shape.rot.y);
    for (i = 0; i < count; i++) {
        NvDoor door;
        f32 px;
        f32 pz;
        s16 entrance;

        memcpy(&door, data + i * sizeof(NvDoor), sizeof(NvDoor));
        if (!(door.flags & NV_DOOR_ACTION)) {
            continue;
        }
        if (door.flags & NV_DOOR_AT_PLAYER) {
            px = player->actor.world.pos.x;
            pz = player->actor.world.pos.z;
            if ((px < door.x0) || (px > door.x1) || (pz < door.z0) || (pz > door.z1)) {
                continue;
            }
        } else {
            f32 d;
            s32 in = false;

            for (d = NV_DOOR_REACH_RE1; d <= reach + 0.01f; d += 1.0f) {
                px = player->actor.world.pos.x + sx * d;
                pz = player->actor.world.pos.z + sz * d;
                if ((px >= door.x0) && (px <= door.x1) && (pz >= door.z0) && (pz <= door.z1)) {
                    in = true;
                    break;
                }
            }
            if (!in) {
                continue;
            }
        }
        door.target[sizeof(door.target) - 1] = '\0';
        entrance = NvDoor_Entrance(door.target, door.spawn);
        printf("NVDOOR %d -> %s spawn %d entrance 0x%X\n", (int)i, door.target, (int)door.spawn, (int)entrance);
        fflush(stdout);
        if (entrance < 0) {
            return;
        }
        // the press is the door's, not Link's
        input->press.button &= ~BTN_A;
        input->cur.button &= ~BTN_A;
        NvCamera_SetArrival(door.b13 & 0x3F); // RE1: the next room opens on this camera
        play->nextEntranceIndex = entrance;
        play->transitionTrigger = TRANS_TRIGGER_START;
        if (NvDoorScene_Exists(door.anim)) {
            NvDoorScene_Queue(door.anim, door.b10, door.b11, door.b13, door.b15);
            play->transitionType = TRANS_TYPE_INSTANT;
            gSaveContext.nextTransitionType = TRANS_TYPE_INSTANT;
        } else {
            play->transitionType = TRANS_TYPE_FADE_BLACK;
            gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
        }
        return;
    }
    // no door: the room's stairs (z_nv_climb.c), with the same points
    if (NvClimb_TryStart(play, player, NV_DOOR_REACH_RE1, reach)) {
        input->press.button &= ~BTN_A;
        input->cur.button &= ~BTN_A;
    }
}
