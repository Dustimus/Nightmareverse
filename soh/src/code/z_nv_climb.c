/*
 * Nightmareverse: RE1's stairs.
 *
 * RE1 has no sloped floors. A flight of stairs is a solid box with an Aot_set zone over it, type
 * 5 (the stairs run along x) or 6 (along z). With the action button pressed, the point 600
 * units in front of the player in the zone (check_action_object 0x0041c150, the door probe) and
 * the player facing along the axis into it (check_door / check_door_side 0x0041b6d0 /
 * 0x0041b790), she climbs: player_door_open_sequence (0x00457390) turns her square to the
 * axis, plays the climb and moves her 4350 units along the axis, away from the side she came
 * from, and 2885 up, or down when (she came from the high side) xor (the zone's first data
 * word's bit 0); 2114 and 1402 in rooms 0x05 and 0x0E.
 *
 * Here A does the same (z_nv_door.c asks NvClimb_TryStart when no door is in reach), and
 * Link is carried there over NV_CLIMB_FRAMES, through the box, with his own update held
 * (z_player.c asks NvClimb_UpdatePlayer, as SoH's noclip does).
 *
 * Data: "scenes/shared/<name>_scene/<name>_climbs" (re1/tools/soh_scene.py): "RCLB", u32
 * count, then per zone u8 type, u8 Aot_set flags, u8 d0, pad, u16 x, z, w, d, s16 along, up
 * (RE1 units).
 */
#include "global.h"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>
#include <stdio.h>
#include <string.h>

#define NV_RE1_SCALE 0.0217f // OoT units per RE1 unit (re1/tools/rdt2scene.py SCALE)
#define NV_CLIMB_FRAMES 30
#define NV_CLIMB_AT_PLAYER 0x40 // tested at the player's position, not in front of her

typedef struct {
    u8 type;
    u8 flags;
    u8 d0;
    u8 pad;
    u16 x;
    u16 z;
    u16 w;
    u16 d;
    s16 along;
    s16 up;
} NvClimb;

static s32 sNvClimbTimer = 0;
static Vec3f sNvClimbFrom;
static Vec3f sNvClimbTo;

s32 NvClimb_Active(void) {
    return sNvClimbTimer > 0;
}

static u8* NvClimb_Data(PlayState* play, u32* count) {
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
    snprintf(path, sizeof(path), "scenes/shared/%s/%.*s_climbs", file, (int)(len - 6), file);
    if (!ResourceMgr_FileExists(path)) {
        return NULL;
    }
    data = (u8*)ResourceGetDataByName(path);
    if ((data == NULL) || (memcmp(data, "RCLB", 4) != 0)) {
        return NULL;
    }
    memcpy(count, data + 4, sizeof(*count));
    return data + 8;
}

static s32 NvClimb_In(const NvClimb* c, f32 x, f32 z) {
    return (x >= c->x) && (x <= c->x + c->w) && (z >= c->z) && (z <= c->z + c->d);
}

/**
 * On an A press with no door in reach: start a climb if a stairs zone is in front of Link (the
 * same points z_nv_door.c tests, from nearD to farD OoT units ahead). Returns true if he climbs
 * (with start false, whether he would: the A button's label).
 */
s32 NvClimb_TryStart(PlayState* play, Player* player, f32 nearD, f32 farD, s32 start) {
    u8* data;
    u32 count;
    u32 i;
    f32 fx = Math_SinS(player->actor.shape.rot.y);
    f32 fz = Math_CosS(player->actor.shape.rot.y);
    // Link and his facing in RE1's frame (OoT = (x, -y, -z) * scale)
    f32 rx = player->actor.world.pos.x / NV_RE1_SCALE;
    f32 rz = -player->actor.world.pos.z / NV_RE1_SCALE;
    f32 ry = -player->actor.world.pos.y / NV_RE1_SCALE;
    f32 rfx = fx;
    f32 rfz = -fz;

    if (sNvClimbTimer > 0) {
        return false;
    }
    data = NvClimb_Data(play, &count);
    if (data == NULL) {
        return false;
    }
    for (i = 0; i < count; i++) {
        NvClimb c;
        s32 in = false;
        s32 low;
        s32 dir;
        s32 otherWay;
        f32 tx = rx;
        f32 tz = rz;
        f32 d;

        memcpy(&c, data + i * sizeof(NvClimb), sizeof(NvClimb));
        if (c.flags & NV_CLIMB_AT_PLAYER) {
            in = NvClimb_In(&c, rx, rz);
        } else {
            for (d = nearD; d <= farD + 0.01f; d += 1.0f) {
                if (NvClimb_In(&c, rx + rfx * d / NV_RE1_SCALE, rz + rfz * d / NV_RE1_SCALE)) {
                    in = true;
                    break;
                }
            }
        }
        if (!in) {
            continue;
        }
        // the side she's on, and facing into the zone from it
        if (c.type == 5) {
            low = (rx - c.x) < (c.w >> 1);
        } else {
            low = (rz - c.z) < (c.d >> 1);
        }
        dir = low ? 1 : -1;
        if (((c.type == 5) ? rfx : rfz) * dir <= 0.0f) {
            continue;
        }
        if (!start) {
            return true;
        }
        if (c.type == 5) {
            tx += dir * c.along;
        } else {
            tz += dir * c.along;
        }
        otherWay = (!low) ^ (c.d0 & 1);
        ry += otherWay ? c.up : -c.up; // RE1's y points down

        sNvClimbFrom = player->actor.world.pos;
        sNvClimbTo.x = tx * NV_RE1_SCALE;
        sNvClimbTo.y = -ry * NV_RE1_SCALE;
        sNvClimbTo.z = -tz * NV_RE1_SCALE;
        sNvClimbTimer = NV_CLIMB_FRAMES;
        // square to the axis, facing on along it
        if (c.type == 5) {
            player->actor.shape.rot.y = (dir > 0) ? 0x4000 : -0x4000;
        } else {
            player->actor.shape.rot.y = (dir > 0) ? -0x8000 : 0;
        }
        player->actor.world.rot.y = player->yaw = player->actor.shape.rot.y;
        printf("NVCLIMB %d from %d,%d,%d to %d,%d,%d\n", (int)i, (int)sNvClimbFrom.x, (int)sNvClimbFrom.y,
               (int)sNvClimbFrom.z, (int)sNvClimbTo.x, (int)sNvClimbTo.y, (int)sNvClimbTo.z);
        fflush(stdout);
        return true;
    }
    return false;
}

/** Called first thing in Player_Update: while climbing, carry Link and skip his own update. */
s32 NvClimb_UpdatePlayer(Player* player, PlayState* play) {
    f32 t;

    if (sNvClimbTimer <= 0) {
        return false;
    }
    if (play->transitionTrigger != TRANS_TRIGGER_OFF) {
        sNvClimbTimer = 0;
        return false;
    }
    sNvClimbTimer--;
    t = 1.0f - (f32)sNvClimbTimer / NV_CLIMB_FRAMES;
    player->actor.world.pos.x = sNvClimbFrom.x + (sNvClimbTo.x - sNvClimbFrom.x) * t;
    player->actor.world.pos.y = sNvClimbFrom.y + (sNvClimbTo.y - sNvClimbFrom.y) * t;
    player->actor.world.pos.z = sNvClimbFrom.z + (sNvClimbTo.z - sNvClimbFrom.z) * t;
    player->actor.speedXZ = 0.0f;
    player->linearVelocity = 0.0f;
    player->actor.velocity.x = player->actor.velocity.y = player->actor.velocity.z = 0.0f;
    player->actor.prevPos = player->actor.world.pos;
    player->actor.home.pos = player->actor.world.pos;
    return true;
}
