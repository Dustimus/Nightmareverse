/*
 * Nightmareverse: the bomb layer. RE1's rooms have walls only as tall as a door (re1_floor.py
 * WALL_HEIGHT), so a thrown bomb or an arrow shot upward can leave the room over them. Taller
 * walls would poke up through a room's upper floor and get in the way of everything else, so
 * instead each room has its floor plan ("<name>_bombwall", re1/tools/re1_floor.py bombwall_blob:
 * the floor with its holes filled, grown out to where the walls stand), and only bombs, bomb
 * flowers and arrows test it: one about to leave it, at any height, is stopped there. A bomb is
 * handed OoT's own wall bounce (its wall flag and the wall's facing, back into the room); an arrow
 * sticks, as into a wall. Link, the enemies and the camera never see it.
 */
#include "global.h"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define NV_RE1_SCALE 0.0217f
#define NV_BW_CELL 100 // (re1_floor.py CELL, RE1 units)

static const u8* sNvBwBits;
static s32 sNvBwX0;
static s32 sNvBwZ0;
static u16 sNvBwW;
static u16 sNvBwH;

/** The room's layer (z_nv_script.c, as the room loads): its "RBWL" blob, or NULL for none. */
void NvBombWall_RoomLoad(const u8* d) {
    sNvBwBits = NULL;
    if ((d == NULL) || (memcmp(d, "RBWL", 4) != 0)) {
        return;
    }
    memcpy(&sNvBwX0, d + 4, 4);
    memcpy(&sNvBwZ0, d + 8, 4);
    memcpy(&sNvBwW, d + 12, 2);
    memcpy(&sNvBwH, d + 14, 2);
    if ((sNvBwW != 0) && (sNvBwH != 0)) {
        sNvBwBits = d + 16;
    }
}

/** Whether an OoT position (x, z) is in the room's floor plan. */
static s32 NvBw_In(f32 x, f32 z) {
    s32 cx = (s32)floorf((x / NV_RE1_SCALE - sNvBwX0) / NV_BW_CELL);
    s32 cz = (s32)floorf((-z / NV_RE1_SCALE - sNvBwZ0) / NV_BW_CELL);
    s32 i;

    if ((cx < 0) || (cz < 0) || (cx >= sNvBwW) || (cz >= sNvBwH)) {
        return false;
    }
    i = cz * sNvBwW + cx;
    return (sNvBwBits[i >> 3] >> (i & 7)) & 1;
}

/**
 * After a bomb's or an arrow's own collision this frame: if it just crossed out of the floor plan,
 * it's put back at the edge on its way out and given a wall facing back into the room (its wall flag and
 * wallYaw, which OoT's bombs bounce off). True if it was stopped.
 */
s32 NvBombWall_Check(PlayState* play, Actor* a) {
    Vec3f* p = &a->world.pos;
    Vec3f* q = &a->prevPos;
    s32 outX;
    s32 outZ;
    s16 yaw;

    if ((sNvBwBits == NULL) || (play == NULL) || !NvRe1_IsScene(play->sceneNum)) {
        return false;
    }
    if (!NvBw_In(q->x, q->z) || NvBw_In(p->x, p->z)) {
        return false; // (still inside, or never was: a carried bomb, one spawned outside)
    }
    outX = !NvBw_In(p->x, q->z); // (which way it crossed: the face it met)
    outZ = !NvBw_In(q->x, p->z);
    if (outX && !outZ) {
        yaw = (p->x > q->x) ? -0x4000 : 0x4000; // (facing -x or +x, back in)
    } else if (outZ && !outX) {
        yaw = (p->z > q->z) ? -0x8000 : 0;
    } else { // (a corner: straight back)
        yaw = (s16)(Math_Atan2S(q->z - p->z, q->x - p->x));
    }
    printf("NVBOMBWALL actor %04X stopped at %.0f,%.0f,%.0f (wall facing %04X)\n", a->id, p->x, p->y, p->z,
           (u16)yaw);
    fflush(stdout);
    {
        // back to the last place inside along its way (an arrow goes 150 a frame: it stops at the
        // wall, not short of it)
        f32 lo = 0.0f;
        f32 hi = 1.0f;
        s32 k;

        for (k = 0; k < 10; k++) {
            f32 m = (lo + hi) * 0.5f;

            if (NvBw_In(q->x + (p->x - q->x) * m, q->z + (p->z - q->z) * m)) {
                lo = m;
            } else {
                hi = m;
            }
        }
        p->x = q->x + (p->x - q->x) * lo;
        p->y = q->y + (p->y - q->y) * lo;
        p->z = q->z + (p->z - q->z) * lo;
    }
    a->wallYaw = yaw;
    a->bgCheckFlags |= BGCHECKFLAG_WALL;
    return true;
}
