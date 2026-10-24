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
 * per door f32 x0, z0, x1, z1, u8 flags, u8 anim, u16 spawn, u8 type, handle, 2 bytes padding,
 * char target[24].
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
    u8 type;   // Door_set byte 13: the door scene's variable 0
    u8 handle; // Door_set byte 10: its variable 3
    u8 pad[2];
    char target[24];
} NvDoor;

s32 NvDoorScene_Exists(s32 anim);
void NvDoorScene_Queue(s32 anim, s32 type, s32 handle);

#define NV_DOOR_ACTION 0x80   // opens with the action button
#define NV_DOOR_AT_PLAYER 0x40 // tested at the player's position, not in front of him
// RE1's 600 units in front of the player (13 OoT units), plus 5 for Link's wider body: OoT
// keeps him further from walls and furniture than RE1 keeps Jill (the foyer's door to 111
// sits in a gap beside a column that Jill fits into and Link does not; from where Link stops,
// 13 units misses the door by about one). Jill's exact radius is still to be read from Bio.exe.
#define NV_DOOR_REACH 18.0f

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
    f32 fx;
    f32 fz;

    if ((player == NULL) || !CHECK_BTN_ALL(input->press.button, BTN_A) ||
        (play->transitionTrigger != TRANS_TRIGGER_OFF) || (play->transitionMode != TRANS_MODE_OFF) ||
        Player_InBlockingCsMode(play, player) || (play->msgCtx.msgMode != MSGMODE_NONE) ||
        !(player->actor.bgCheckFlags & 1) || (player->actor.parent != NULL)) {
        return;
    }
    data = NvDoor_Data(play, &count);
    if (data == NULL) {
        return;
    }
    fx = player->actor.world.pos.x + Math_SinS(player->actor.shape.rot.y) * NV_DOOR_REACH;
    fz = player->actor.world.pos.z + Math_CosS(player->actor.shape.rot.y) * NV_DOOR_REACH;
    for (i = 0; i < count; i++) {
        NvDoor door;
        f32 px;
        f32 pz;
        s16 entrance;

        memcpy(&door, data + i * sizeof(NvDoor), sizeof(NvDoor));
        if (!(door.flags & NV_DOOR_ACTION)) {
            continue;
        }
        px = (door.flags & NV_DOOR_AT_PLAYER) ? player->actor.world.pos.x : fx;
        pz = (door.flags & NV_DOOR_AT_PLAYER) ? player->actor.world.pos.z : fz;
        if ((px < door.x0) || (px > door.x1) || (pz < door.z0) || (pz > door.z1)) {
            continue;
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
        play->nextEntranceIndex = entrance;
        play->transitionTrigger = TRANS_TRIGGER_START;
        if (NvDoorScene_Exists(door.anim)) {
            NvDoorScene_Queue(door.anim, door.type, door.handle);
            play->transitionType = TRANS_TYPE_INSTANT;
            gSaveContext.nextTransitionType = TRANS_TYPE_INSTANT;
        } else {
            play->transitionType = TRANS_TYPE_FADE_BLACK;
            gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
        }
        return;
    }
}
