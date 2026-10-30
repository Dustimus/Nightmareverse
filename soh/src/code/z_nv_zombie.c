/*
 * Nightmareverse: RE1's zombie, ported as RE1 runs it.
 *
 * The reference is the PC decomp (ecruells/resident-evil-pc-decomp): src/game/entities/Zombie.cpp
 * (zombie_update and everything it reaches), EntityCommon.cpp (the shared entity helpers:
 * turning, sight, path finding, entity pushes), RoomCollision.cpp (check_room_collision and its
 * shape handlers, the sight test) and PlayerAnimations.cpp (Joint_move, Add_speedXZ, the grab
 * vertex). Everything is in RE1's units (y down), angles (4096 a turn) and 30 Hz ticks; the
 * enemy actor (z_nv_enemy.c) runs a tick when RE1 would and only converts the result for OoT.
 * Room collision and walk zones are RE1's own ("<name>_re1col", re1/tools/soh_scene.py): no OoT
 * floor, wall or gravity touches an RE1 enemy.
 *
 * Left out, each a note where it would be: sounds (Snd_em), effects (billboards, blood, fade
 * sprites, joint tints), the Director's Cut and Saturn branches, severed limbs (joint flags), and
 * zombie_body_part_physics (the crawl's speed from the leg joints: here the speed it starts at).
 * The player's side is a stand-in fed from Link each tick (gNvRe1Player).
 */
#include "global.h"
#include "z_nv_re1.h"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NV_PI 3.14159265358979

enum {
    ZST_INIT = 0,
    ZST_IDLE = 1,
    ZST_DAMAGED = 2,
    ZST_DIE = 3,
    ZST_NO_ACTION = 4,
    ZST_ATTACK = 5,
    ZST_ACTION_UPDATE = 8,
};
#define ZF_LAYING_DOWN 0x02
#define ZF_VOMITING 0x40
#define ZF_SCD_CONTROLLED 0x80
#define ST_PLAYER_ABOVE 0x20
#define ST_PLAYER_BELOW 0x80
#define ST_ALIGNED 0x40

NvRe1Player gNvRe1Player;

/* ---- RE1's shared scratch, as the original shares it between these routines ---- */

static NvRe1Entity* E; // ENTITY
#define P (&gNvRe1Player)
static s32 sDisp;           // g_playerDisplacement
static s32 sEntityBkp;      // g_entity_bkp
static s32 sPushZLo;        // g_collPushDepthZLo
static s32 sFrameIdSave;    // g_animFrameIdSave
static s32 sTempVar;        // g_tempVar
static s32 sScratch[3];     // g_playerPosScratch
static u16 sRandSeed;       // g_RandSeed

/* ---- the room: RE1's boundaries and walk zones ---- */

typedef struct {
    u16 xMax;
    u16 zMax;
    u16 xMin;
    u16 zMin;
    u16 type;
    u16 flags;
} NvRe1Bound;

static NvRe1Bound* sBounds; // a copy: the sight test writes the flags back, as the original does
static s32 sBoundGroup[5];  // quadrant q's records are [group[q], group[q+1])
static s16 sCellX;
static s16 sCellZ;
static u8* sZones;          // count byte, pad, then 12-byte zones

#define NV_RE1_MAX_ENTS 16
static NvRe1Entity* sEnts[NV_RE1_MAX_ENTS];

void NvRe1_Register(NvRe1Entity* e) {
    s32 i;

    for (i = 0; i < NV_RE1_MAX_ENTS; i++) {
        if (sEnts[i] == NULL) {
            sEnts[i] = e;
            return;
        }
    }
}

void NvRe1_Unregister(NvRe1Entity* e) {
    s32 i;

    for (i = 0; i < NV_RE1_MAX_ENTS; i++) {
        if (sEnts[i] == e) {
            sEnts[i] = NULL;
        }
    }
}

/** Play_Init: the room's collision and walk zones. */
void NvRe1_RoomLoad(void* playArg) {
    PlayState* play = playArg;
    char path[256];
    const char* file;
    size_t len;
    u8* d;
    u32 bndLen;
    u32 zoneLen;

    free(sBounds);
    free(sZones);
    sBounds = NULL;
    sZones = NULL;
    memset(sEnts, 0, sizeof(sEnts));
    if ((play->loadedScene == NULL) || (play->loadedScene->sceneFile.fileName == NULL)) {
        return;
    }
    file = play->loadedScene->sceneFile.fileName;
    len = strlen(file);
    if ((len <= 6) || (strcmp(file + len - 6, "_scene") != 0)) {
        return;
    }
    snprintf(path, sizeof(path), "scenes/shared/%s/%.*s_re1col", file, (int)(len - 6), file);
    if (!ResourceMgr_FileExists(path)) {
        return;
    }
    d = (u8*)ResourceGetDataByName(path);
    if ((d == NULL) || (memcmp(d, "RCOL", 4) != 0)) {
        return;
    }
    memcpy(&bndLen, d + 4, 4);
    memcpy(&zoneLen, d + 8, 4);
    if (bndLen >= 0x18) {
        u32 counts[5];
        s32 q;
        s32 run = 0;
        u32 n = (bndLen - 0x18) / 12;

        memcpy(&sCellX, d + 12, 2);
        memcpy(&sCellZ, d + 14, 2);
        memcpy(counts, d + 16, sizeof(counts));
        sBounds = malloc(MAX(n, 1) * sizeof(NvRe1Bound));
        memcpy(sBounds, d + 12 + 0x18, n * sizeof(NvRe1Bound));
        // Room_SetupCollisionCallbacks: counts to ranges
        for (q = 0; q < 5; q++) {
            sBoundGroup[q] = run;
            if (q < 4) {
                run += counts[q];
            }
        }
        sBoundGroup[4] = MIN(sBoundGroup[4], (s32)n);
    }
    if (zoneLen >= 2) {
        sZones = malloc(zoneLen);
        memcpy(sZones, d + 12 + bndLen, zoneLen);
    }
}

/** Once per tick: RE1's g_RandSeed moves on every frame. */
void NvRe1_TickBegin(void) {
    sRandSeed = (u16)(Rand_Next() >> 8);
}

/* ---- maths (the GTE's fixed point, 4096 = 1.0) ---- */

static s32 SquareRoot0(s32 v) {
    return (v <= 0) ? 0 : (s32)sqrt((double)v);
}

static s32 Rsin(s32 a) {
    return (s32)lround(sin((a & 0xFFF) * (NV_PI / 2048.0)) * 4096.0);
}

static s32 Rcos(s32 a) {
    return (s32)lround(cos((a & 0xFFF) * (NV_PI / 2048.0)) * 4096.0);
}

/** RotMatrixY(a) applied to (x, 0, z): x' = c x + s z, z' = -s x + c z. */
static void RotY(s32 a, s32 x, s32 z, s32* ox, s32* oz) {
    s32 c = Rcos(a);
    s32 s = Rsin(a);

    *ox = (c * x + s * z) >> 12;
    *oz = (-s * x + c * z) >> 12;
}

/** CalculateAngleBetweenPointsXZ (as shorts: positions past 32767 wrap, as in RE1). */
static u16 AngleBetween(s32 x1, s32 z1, s32 x2, s32 z2) {
    s16 dx = (s16)x2 - (s16)x1;
    s16 dz = (s16)z2 - (s16)z1;

    if (dx != 0) {
        s32 slope = ((s32)dz * 4096) / (s32)dx;
        s16 a = (slope == 0) ? 0 : (s16)(s32)(atan((double)slope / 4096.0) * (2048.0 / NV_PI));

        return (u16)(-(((u16)(dx < 0)) * 0x800 + a)) & 0xFFF;
    }
    return (u16)((0 < dz) * 0x800 + 0x400);
}

static u16 getAngleTowardsTarget(s32 px, s32 pz) {
    return AngleBetween(E->t[0], E->t[2], px, pz);
}

/** turn_toward_target: a step toward the target, 0 within twice the step. */
static s32 turn_toward_target(s32 x, s32 z, s16 step) {
    u16 target = getAngleTowardsTarget(x, z);
    s32 delta = (((s16)target - (s16)(u16)E->angle) + step) & 0xFFF;

    if (delta < (s32)(u16)(step * 2)) {
        return 0;
    }
    return (delta < 0x801) ? step : -step;
}

static void entity_rotate_toward_target(s32 x, s32 z, u16 step) {
    s16 target = getAngleTowardsTarget(x, z);
    u16 base = (u16)target;
    u16 delta;

    if (step & 0x8000) {
        step = -step;
        base = (base + 0x800) & 0xFFF;
    }
    delta = ((u16)(step - E->angle) + base) & 0xFFF;
    if ((s32)delta < (s32)(s16)step * 2) {
        E->angle = base;
        return;
    }
    E->angle -= step;
    if (delta < 0x801) {
        E->angle += step * 2;
    }
}

/** Add_speedXZ: move_speed_current along the facing (+ an offset). */
static void Add_speedXZ(s16 offset) {
    s32 x;
    s32 z;

    RotY(E->angle + offset, E->move_speed_current, 0, &x, &z);
    E->speed[0] = x;
    E->speed[1] = 0;
    E->speed[2] = z;
    E->t[0] += x;
    E->t[2] += z;
}

static s32 Manhattan(s32 x, s32 z) {
    s32 dx = x - E->t[0];
    s32 dz = z - E->t[2];

    return ABS(dx) + ABS(dz);
}

/* ---- the room's collision (RoomCollision.cpp) ---- */

static u32 ChkOutsideCell(s32 x, s32 z) {
    u32 bitX = (u32)(x - sCellX) >> 31;
    u32 bitZ = ((u32)(z - sCellZ) & 0xbfffffffu) >> 30;

    return bitZ | bitX;
}

static s32 BoundInside(const NvRe1Bound* r, s32 px, s32 pz, s32 radius) {
    s32 xHi = r->xMax + radius;
    s32 zHi = r->zMax + radius;
    s32 xLo = r->xMin - radius;
    s32 zLo = r->zMin - radius;

    return (px >= xLo) && (px <= xHi) && (pz >= zLo) && (pz <= zHi);
}

static s16 EntRadius(void) {
    return E->sca[5];
}

static void collision_push_rect(const NvRe1Bound* rec, s32* pos, const s16* prev) {
    s16 radius = EntRadius();
    s16 posX = (s16)pos[0];
    s16 posZ = (s16)pos[2];
    s16 pushXHi = (s16)((radius - posX) + (s16)rec->xMax + 0x12);
    s16 pushXLo = (s16)((((s16)rec->xMin - radius) - posX) - 0x12);
    s16 pushX = pushXHi;
    s16 pushZHi = (s16)(((radius + (s16)rec->zMax) - posZ) + 0x12);
    s16 pushZLo = (s16)((((s16)rec->zMin - radius) - posZ) - 0x12);
    s16 pushZ = pushZHi;
    s32 prevX = prev[0];
    s32 prevZ = prev[2];
    u8 sel;
    s32 absX;
    s32 absZ;

    if (-pushXLo < pushXHi) {
        pushX = pushXLo;
    }
    if (-pushZLo < pushZHi) {
        pushZ = pushZLo;
    }
    sel = (u8)(((((u8)((pos[2] - prevZ) >> 14)) ^ (u8)(pushZ >> 14)) & 2) |
               ((((u8)((pos[0] - prevX) >> 15)) ^ (u8)(pushX >> 15)) & 1));
    if ((prevX == pos[0]) && (prevZ == pos[2])) {
        sFrameIdSave = 0;
    }
    switch (sel) {
        case 0:
            pos[0] = prevX;
            pos[2] = prevZ;
            return;
        case 1:
            if ((u16)(pushX + 0x190) <= 0x320) {
                pos[0] += pushX;
                E->status_flags &= 0xEF;
                return;
            }
            break;
        case 2:
            if ((u16)(pushZ + 0x190) <= 0x320) {
                pos[2] += pushZ;
                E->status_flags |= 0x10;
                return;
            }
            break;
        default:
            break;
    }
    absX = ABS(pushX);
    absZ = ABS(pushZ);
    if (absZ > absX) {
        pos[0] += pushX;
        E->status_flags &= 0xEF;
    } else {
        pos[2] += pushZ;
        E->status_flags |= 0x10;
    }
}

static void collision_push_circle(const NvRe1Bound* rec, s32* pos) {
    u32 radius = (u16)EntRadius();
    s32 reach = (s32)(((u32)rec->xMax - (u32)rec->xMin) + radius * 2) / 2;
    s32 dz = ((pos[2] - (s32)rec->zMin) - reach) + (s32)radius;
    s32 dx = ((pos[0] - (s32)rec->xMin) - reach) + (s32)radius;
    s32 dist = SquareRoot0(dx * dx + dz * dz);
    s32 pen = reach - dist;
    s32 px;
    s32 pz;

    if (pen < 1) {
        return;
    }
    if (dist == 0) {
        pos[0] += pen;
        E->status_flags &= 0xEF;
        return;
    }
    px = (pen * dx) / dist;
    pz = (pen * dz) / dist;
    pos[2] += pz;
    pos[0] += px;
    if (ABS(px) < ABS(pz)) {
        E->status_flags |= 0x10;
    } else {
        E->status_flags &= 0xEF;
    }
}

static void Bound_Handle(const NvRe1Bound* rec, s32 shape, s32* pos, const s16* prev) {
    switch (shape) {
        case 1:
        case 5:
            collision_push_rect(rec, pos, prev);
            break;
        case 3:
            collision_push_circle(rec, pos);
            break;
        case 4:
            E->collisionFlags |= 0x08;
            break;
        default:
            break;
    }
}

static u8 collision_accept(s16 floorStep, u8 clearResult) {
    s32 dx = E->t[0] - E->pos[0];
    s32 dz = E->t[2] - E->pos[2];

    sTempVar = SquareRoot0(dz * dz + dx * dx);
    E->pos[0] = (s16)E->t[0];
    E->pos[1] = (s16)E->t[1];
    E->pos[2] = (s16)E->t[2];
    if (floorStep != 0) {
        sFrameIdSave = floorStep;
        return 3;
    }
    return clearResult;
}

/** check_room_collision: push out, re-test, roll back if still inside. */
static u8 check_room_collision(void) {
    u16 hitBits = 0;
    s16 floorStep = 0;
    s16 radius = EntRadius();
    s32 cell;
    s32 i;
    s32 sx;
    s32 sz;

    E->collisionFlags &= 0xF7;
    if ((E->status_flags & 4) || (sBounds == NULL)) {
        return 0;
    }
    cell = ChkOutsideCell(E->t[0], E->t[2]);
    sFrameIdSave = 0;
    // pass 1 tests every record against where it was coming in (g_playerPosScratch), not as pushed
    sx = E->t[0];
    sz = E->t[2];
    for (i = sBoundGroup[cell]; i < sBoundGroup[cell + 1]; i++) {
        NvRe1Bound* rec = &sBounds[i];
        u8 cf = E->collisionFlags;

        if ((cf & 4) && !(rec->flags & 0x100)) {
            s16 step = (s16)(((((rec->type & 0x7F00) >> 8) * 10) + (rec->flags & 0xFF)) * 100);

            floorStep = (s16)(((rec->type & 0x8000) ? -(s32)step : 0) | 1);
            continue;
        }
        if (((rec->type & 0xFF) == 5) && (cf & 0x10)) {
            continue;
        }
        if (!BoundInside(rec, sx, sz, radius)) {
            continue;
        }
        Bound_Handle(rec, rec->type & 0xFF, E->t, E->pos);
        hitBits |= (rec->flags & 0x300) >> 8;
    }
    if (hitBits == 0) {
        return collision_accept(floorStep, 0);
    }
    hitBits = 0;
    sFrameIdSave = 0;
    for (i = sBoundGroup[cell]; i < sBoundGroup[cell + 1]; i++) {
        NvRe1Bound* rec = &sBounds[i];
        u16 t = rec->type & 0xFF;
        u16 bits;

        if (t == 4) {
            continue;
        }
        if ((t == 5) && (E->collisionFlags & 0x10)) {
            continue;
        }
        if ((E->collisionFlags & 4) && !(rec->flags & 0x100)) {
            continue;
        }
        if (!BoundInside(rec, E->t[0], E->t[2], radius)) {
            continue;
        }
        bits = rec->flags & 0x300;
        hitBits |= bits >> 8;
        if ((t == 3) && (bits != 0)) {
            sFrameIdSave = 1;
        }
    }
    if (sFrameIdSave != 0) {
        return collision_accept(floorStep, 0);
    }
    if (hitBits == 0) {
        return collision_accept(floorStep, 1);
    }
    sTempVar = 0;
    E->t[0] = E->pos[0];
    E->pos[1] = (s16)E->t[1];
    E->t[2] = E->pos[2];
    if (floorStep != 0) {
        sFrameIdSave = floorStep;
        return 3;
    }
    return 2;
}

/** check_room_collision_two_point: both ends of a body on the floor (offsets along its x). */
static u8 check_room_collision_two_point(s16 endA, s16 endB) {
    s32 ends[2][3];
    s16 world[2][3];
    u16 bits[2] = { 0, 0 };
    u32 cells[2] = { 0, 0 };
    s32 which;
    s32 i;

    if ((E->status_flags & 4) || (sBounds == NULL)) {
        return 0;
    }
    for (which = 0; which < 2; which++) {
        s16 off = which ? endB : endA;
        s32 x;
        s32 z;

        RotY(E->angle, off, 0, &x, &z);
        ends[which][0] = x + E->t[0];
        ends[which][1] = 0;
        ends[which][2] = z + E->t[2];
        RotY(E->angleBackup, off, 0, &x, &z);
        world[which][0] = (s16)(x + E->pos[0]);
        world[which][1] = 0;
        world[which][2] = (s16)(z + E->pos[2]);
    }
    for (which = 1; which >= 0; which--) { // end B first, as the original
        cells[which] = ChkOutsideCell(ends[which][0], ends[which][2]);
        for (i = sBoundGroup[cells[which]]; i < sBoundGroup[cells[which] + 1]; i++) {
            NvRe1Bound* rec = &sBounds[i];
            u8 shape = rec->type & 0xFF;

            if ((shape == 4) || (shape == 5)) {
                continue;
            }
            if (!BoundInside(rec, ends[which][0], ends[which][2], EntRadius())) {
                continue;
            }
            Bound_Handle(rec, shape, ends[which], world[which]);
            bits[which] |= (rec->flags & 0x300) >> 8;
        }
    }
    if (bits[0] == 0) {
        E->pos[0] = (s16)E->t[0];
        E->pos[1] = (s16)E->t[1];
        E->pos[2] = (s16)E->t[2];
        E->angleBackup = E->angle;
        return (u8)bits[1];
    }
    if (cells[1] > 0) { // (the original re-walks the quadrant before end B's)
        for (i = sBoundGroup[cells[1] - 1]; i < sBoundGroup[cells[1]]; i++) {
            NvRe1Bound* rec = &sBounds[i];
            u8 shape = rec->type & 0xFF;

            if ((shape == 4) || (shape == 5)) {
                continue;
            }
            if (BoundInside(rec, ends[1][0], ends[1][2], EntRadius())) {
                bits[1] |= (rec->flags & 0x300) >> 8;
            }
        }
    }
    if (bits[1] != 0) {
        E->t[0] = E->pos[0];
        E->pos[1] = (s16)E->t[1];
        E->t[2] = E->pos[2];
        E->angle = E->angleBackup;
        return 0x80;
    }
    E->pos[0] = (s16)E->t[0];
    E->pos[1] = (s16)E->t[1];
    E->pos[2] = (s16)E->t[2];
    E->angleBackup = E->angle;
    return 1;
}

static s32 Cross2(s32 ax, s32 az, s32 bx, s32 bz) {
    return az * bx - ax * bz; // vectorMul3's .y
}

static s32 ray_crosses_diagonal(s32 dx, s32 dz, s32 entX, s32 entZ, s32 dirX, s32 dirZ, s32 ax, s32 az, s32 bx,
                                s32 bz) {
    s32 ex = bx - ax;
    s32 ez = bz - az;
    s32 c1 = Cross2(ex, ez, (entX + dirX) - ax, (entZ + dirZ) - az);
    s32 c0 = Cross2(ex, ez, entX - ax, entZ - az);

    if ((((u32)c0 ^ (u32)c1) & 0x80000000u) == 0) {
        return 0;
    }
    c1 = Cross2(dx, dz, bx - entX, bz - entZ);
    c0 = Cross2(dx, dz, ax - entX, az - entZ);
    return (((u32)c0 ^ (u32)c1) & 0x80000000u) != 0;
}

/** room_check_sight_blocked: does entity -> entity + delta cross a sight-blocking box? */
static u32 room_check_sight_blocked(s32 dx, s32 dz, u8 cell) {
    s32 entX;
    s32 entZ;
    s32 dirX;
    s32 dirZ;
    s32 i;

    cell &= 0x1F;
    if ((sBounds == NULL) || (cell > 3)) {
        return 0;
    }
    entX = E->t[0] / 18;
    entZ = E->t[2] / 18;
    dirX = dx / 18;
    dirZ = dz / 18;
    for (i = sBoundGroup[cell]; i < sBoundGroup[cell + 1]; i++) {
        NvRe1Bound* rec = &sBounds[i];
        s32 xMax = rec->xMax / 18;
        s32 zMax = rec->zMax / 18;
        s32 xMin = rec->xMin / 18;
        s32 zMin = rec->zMin / 18;
        u16 blocking = rec->flags & 0x300;

        rec->flags = blocking; // (the original writes this back)
        if ((blocking != 0x300) && (E->id != 13) && (E->id != 18)) {
            continue;
        }
        if ((rec->type == 4) || (rec->type == 5)) {
            continue;
        }
        if (ray_crosses_diagonal(dx, dz, entX, entZ, dirX, dirZ, xMax, zMin, xMin, zMax) ||
            ray_crosses_diagonal(dx, dz, entX, entZ, dirX, dirZ, xMin, zMin, xMax, zMax)) {
            return 1;
        }
    }
    return 0;
}

/* ---- the entity helpers (EntityCommon.cpp) ---- */

static u8 check_line_of_sight(s32 x, s32 z) {
    u32 cell = ChkOutsideCell(x, z);

    return (u8)room_check_sight_blocked(x - E->t[0], z - E->t[2], (u8)cell);
}

static u32 entity_check_angular_los(s16 fovHalf, s32 x, s32 z) {
    s16 target = getAngleTowardsTarget(x, z);
    u16 delta = ((u16)(fovHalf - E->angle) + (u16)target) & 0xFFF;

    if (fovHalf * 2 < (s32)delta) {
        return 1;
    }
    return room_check_sight_blocked(x - E->t[0], z - E->t[2], E->pathfind_state);
}

/** checkAngularViewAndDistance: within maxDist (Manhattan) and between the two edges. */
static u8 checkAngularViewAndDistance(s16 fovHalf, s16 maxDist, s32 x, s32 z) {
    s32 dx = x - E->t[0];
    s32 dz = z - E->t[2];
    s32 lx;
    s32 lz;
    s32 rx;
    s32 rz;
    s32 cl;
    s32 cr;

    if ((s32)(u16)maxDist < ABS(dx) + ABS(dz)) {
        return 0;
    }
    RotY(E->angle - fovHalf, 2000, 0, &lx, &lz);
    RotY(E->angle + fovHalf, 2000, 0, &rx, &rz);
    cl = Cross2(lx, lz, dx, dz);
    cr = Cross2(rx, rz, dx, dz);
    return ((u32)cl & 0x80000000u) < ((u32)cr & 0x80000000u);
}

static u32 entity_check_visual_range(u32 range) {
    s32 dx = P->t[0] - E->t[0];
    s32 dz = P->t[2] - E->t[2];
    u32 d = SquareRoot0(dx * dx + dz * dz);

    if (d < range) {
        E->status_flags |= ST_PLAYER_ABOVE;
    }
    return d;
}

static u32 entity_check_alert_range(u32 range) {
    s32 dx = P->t[0] - E->t[0];
    s32 dz = P->t[2] - E->t[2];
    u32 d = SquareRoot0(dx * dx + dz * dz);

    if (d < range) {
        E->status_flags |= ST_PLAYER_BELOW;
    }
    return d;
}

/** entity_pathfind_update: every few ticks, if it sees the player, make her the waypoint. */
static u32 entity_pathfind_update(void) {
    u8* state = &E->pathfind_state;
    u8 val = *state;
    u8 counter = val & 0x1F;
    s8 result;

    if (counter > 3) {
        ++*state;
        if ((*state & 0x1F) > 0x0F) {
            *state &= 0xC0;
        }
        return 2;
    }
    result = (s8)entity_check_angular_los(1512, P->t[0], P->t[2]);
    *state = (result << 5) | val;
    val = *state;
    counter = val & 0x1F;
    if (counter == 3) {
        if ((val & 0x20) == 0) {
            E->player_pos_x = (s16)P->t[0];
            E->player_pos_z = (s16)P->t[2];
            ++*state;
            *state &= ~0x20;
            return 1;
        }
        ++*state;
        *state &= ~0x20;
        return 0;
    }
    ++*state;
    return 2;
}

/** entity_update_wander_turn: turn off at random when stuck; otherwise steer to the waypoint. */
static u32 entity_update_wander_turn(u32 dist, u8* control, u8* counter, u16 step, u8 limit) {
    u16* ang = (u16*)&E->angle;
    s16 speedDiv = E->move_speed_current;
    u16 target;
    u16 delta;

    if (*control & 0x80) {
        s32 s3 = speedDiv * 3;
        s32 threshold = (s3 + ((s3 >> 31) & 3)) >> 2;

        *ang = *ang + (1 - (u16)((*control & 0x40) >> 5)) * step;
        if ((u32)threshold < dist) {
            u8 n = *counter - 1;

            *counter = n;
            if (n == 0) {
                *control = 0;
                *counter = 0;
            }
        }
        return 1;
    }
    if (dist < (u32)((speedDiv * 2) / 3)) {
        u8 n = *counter + 1;

        *counter = n;
        if (limit < n) {
            u8 flags = *control;

            *control = (sRandSeed & 0x40) | flags | 0x80;
            *counter = limit / 6;
        }
    } else {
        *control = 0;
        *counter = 0;
    }
    target = getAngleTowardsTarget(E->player_pos_x, E->player_pos_z);
    if (step & 0x8000) {
        step = -step;
        target = (target + 0x800) & 0xFFF;
    }
    delta = ((u16)(step - *ang) + target) & 0xFFF;
    if ((s32)delta < (s16)(step * 2)) {
        *ang = target;
        return 0;
    }
    *ang = *ang - step;
    if (delta < 0x801) {
        *ang = *ang + step * 2;
    }
    return 0;
}

/* walk zones (zone_path_find and its walk) */

static u8 sZoneIdx[0x20];
static u8 sZoneBest[0x20];
static s16 sZonePt[0x20][2];
static s32 sEdgeX; // walk_zone_shared_edge's results (g_playerDisplacement / player_distance_z)
static s32 sEdgeZ;

static u32 walk_zone_find(s16 x, s16 z) {
    s32 i;

    for (i = sZones[0] - 1; i >= 0; i--) {
        u8* e = sZones + 2 + i * 12;
        s16 x1;
        s16 z1;
        s16 x2;
        s16 z2;

        memcpy(&x1, e, 2);
        memcpy(&z1, e + 2, 2);
        memcpy(&x2, e + 4, 2);
        memcpy(&z2, e + 6, 2);
        if (((u16)(x - x1) < (u16)(x2 - x1)) && ((u16)(z - z1) < (u16)(z2 - z1))) {
            return i;
        }
    }
    return 0xFF;
}

static u16 ZoneField(u32 zone, s32 k) {
    u16 v;

    memcpy(&v, sZones + 2 + (zone & 0xFFFF) * 12 + k * 2, 2);
    return v;
}

static u8 walk_zone_shared_edge(u32 a, u32 b) {
    u16 lo;
    u16 hi;

    if (ZoneField(b, 2) == ZoneField(a, 0)) {
        sEdgeX = ZoneField(a, 0);
    } else if (ZoneField(a, 2) == ZoneField(b, 0)) {
        sEdgeX = ZoneField(b, 0);
    } else {
        if (ZoneField(b, 3) == ZoneField(a, 1)) {
            sEdgeZ = ZoneField(a, 1);
        } else if (ZoneField(a, 3) == ZoneField(b, 1)) {
            sEdgeZ = ZoneField(b, 1);
        }
        lo = MAX(ZoneField(a, 0), ZoneField(b, 0));
        hi = MIN(ZoneField(a, 2), ZoneField(b, 2));
        sEdgeX = (lo + hi) >> 1;
        return 1;
    }
    lo = MAX(ZoneField(a, 1), ZoneField(b, 1));
    hi = MIN(ZoneField(a, 3), ZoneField(b, 3));
    sEdgeZ = (lo + hi) >> 1;
    return 0;
}

#define ZW_DIST(ax, az, bx, bz) \
    (u32)SquareRoot0(((s32)(ax) - (s32)(bx)) * ((s32)(ax) - (s32)(bx)) + ((s32)(az) - (s32)(bz)) * ((s32)(az) - (s32)(bz)))

static u8 zone_walk(u8 zoneTarget, s16 targetX, s16 targetZ, s32 cw) {
    const u8 seed = cw ? 0xFF : sZones[0];
    const u32 targetBit = 1u << (zoneTarget & 0x1F);
    u8* idx = sZoneIdx;
    s16(*pt)[2] = sZonePt;
    s8 step = 0;
    u32 best = 0xFFFFFFFF;
    u32 dist = 0;
    s32 guard = 0;

    for (;;) {
        const s32 i = step;
        const u16 flags = ZoneField(idx[i], 5);

        if (++guard > 20000) {
            return 0xFF;
        }
        if (flags & targetBit) {
            idx[i + 1] = zoneTarget;
            walk_zone_shared_edge(idx[i], zoneTarget);
            dist += ZW_DIST(pt[i + 1][0], pt[i + 1][1], sEdgeX, sEdgeZ);
            dist += ZW_DIST(sEdgeX, sEdgeZ, targetX, targetZ);
            if (dist < best) {
                s8 n = step + 1;

                best = dist;
                do {
                    sZoneBest[n] = idx[n];
                    n--;
                } while (n != 0);
            }
            if (step != 0) {
                dist -= ZW_DIST(sEdgeX, sEdgeZ, targetX, targetZ);
                dist -= ZW_DIST(pt[i + 1][0], pt[i + 1][1], sEdgeX, sEdgeZ);
                dist -= ZW_DIST(pt[i][0], pt[i][1], pt[i + 1][0], pt[i + 1][1]);
            }
        } else {
            const u32 mask = cw ? ~((1u << ((u8)(idx[i + 1] + 1) & 0x1F)) - 1u) : ((1u << (idx[i + 1] & 0x1F)) - 1u);

            if ((flags & mask) == 0) {
                if (step != 0) {
                    dist -= ZW_DIST(pt[i][0], pt[i][1], pt[i + 1][0], pt[i + 1][1]);
                }
            } else if (step + 2 >= (s32)sizeof(sZoneIdx)) {
                if (step != 0) {
                    dist -= ZW_DIST(pt[i][0], pt[i][1], pt[i + 1][0], pt[i + 1][1]);
                }
            } else {
                s32 s;
                u8 zone;
                s32 onPath = false;
                s8 b;

                step++;
                s = step;
                do {
                    zone = (u8)(cw ? idx[s] + 1 : idx[s] - 1);
                    idx[s] = zone;
                } while ((flags & (1u << (zone & 0x1F))) == 0);
                for (b = s - 2; b >= 0; b--) {
                    if (idx[b] == zone) {
                        onPath = true;
                        break;
                    }
                }
                if (!onPath) {
                    walk_zone_shared_edge(idx[s], idx[s - 1]);
                    dist += ZW_DIST(pt[s][0], pt[s][1], sEdgeX, sEdgeZ);
                    if (dist < best) {
                        pt[s + 1][0] = (s16)sEdgeX;
                        pt[s + 1][1] = (s16)sEdgeZ;
                        idx[s + 1] = seed;
                        continue;
                    }
                    dist -= ZW_DIST(pt[s][0], pt[s][1], sEdgeX, sEdgeZ);
                }
            }
        }
        step--;
        if (step < 0) {
            return (best == 0xFFFFFFFF) ? 0xFF : sZoneBest[1];
        }
    }
}

/** zone_path_find: the waypoint toward (x, z) across the room's walk zones. */
static u8 zone_path_find(s32 x, s32 z, s16* outX, s16* outZ) {
    u8 count;
    u8 startZone;
    u8 targetZone;
    u16 targetZ = (u16)z;
    s32 delta;
    s8 dir;
    u8 first;

    if (sZones == NULL) {
        *outX = (s16)x;
        *outZ = (s16)z;
        return 0x10;
    }
    count = sZones[0];
    startZone = (u8)walk_zone_find((s16)E->t[0], (s16)E->t[2]);
    targetZone = (u8)walk_zone_find((s16)x, (s16)targetZ);
    if (targetZone == startZone) {
        *outX = (s16)x;
        *outZ = (s16)targetZ;
        return startZone | 0x10;
    }
    if ((startZone == 0xFF) || (targetZone == 0xFF)) {
        return 0xFF; // (the original walks from zone 0xFF too; its adjacency read is past the table)
    }
    sZoneIdx[1] = count;
    delta = (s32)targetZone - (s32)startZone;
    if (delta < 0) {
        delta += count;
    }
    dir = (s8)((((count >> 1) < delta) * 2) - 1);
    if (startZone == 0) {
        dir = -dir;
    }
    sZonePt[1][0] = (s16)E->t[0];
    sZonePt[1][1] = (s16)E->t[2];
    sZoneIdx[0] = startZone;
    sZoneBest[0] = startZone;
    if (dir < 1) {
        first = zone_walk(targetZone, (s16)x, (s16)targetZ, 0);
    } else {
        sZoneIdx[1] = 0xFF;
        first = zone_walk(targetZone, (s16)x, (s16)targetZ, 1);
    }
    if (first == 0xFF) {
        return 0xFF;
    }
    walk_zone_shared_edge(startZone, first);
    *outX = (s16)sEdgeX;
    *outZ = (s16)sEdgeZ;
    return first;
}

static u32 is_facing_toward_entity(void) {
    s32 diff = (s32)P->angle - (s32)E->angle;

    return ((u32)(diff + 0x400) & 0xFFF) < 0x800;
}

static s8 reduce_attack_time_by_btn_press(void) {
    s8 r = 0;

    if (P->padHeld & 1) {
        r = 3;
    }
    if (P->padHeld & 2) {
        r += 2;
    }
    return r;
}

/** SetEntityScaHitData: the one volume, turned by the facing. */
static void SetScaWorld(const s16* sca, s16 angle, s16* out) {
    s32 x;
    s32 z;

    RotY(angle, sca[1], sca[3], &x, &z);
    out[0] = (s16)x;
    out[1] = sca[2];
    out[2] = (s16)z;
}

/** ResolveEntityScaCollision: push B out of A (single volumes, as zombies and the player have). */
static u32 ResolveScaPush(const s16* scaA, const s16* wA, const s32* tA, NvRe1Entity* b) {
    s32 dx;
    s32 dz;
    u16 rA = scaA[5];
    u16 rB = b->sca[5];
    u16 hA = scaA[4];
    u16 hB = b->sca[4];
    s32 dist;
    s32 pen;

    if (b->state == 4) {
        return 0;
    }
    dx = ((s32)b->scaWorld[0] - (s32)wA[0]) - tA[0] + b->t[0];
    dz = ((s32)b->scaWorld[2] - (s32)wA[2]) - tA[2] + b->t[2];
    dist = SquareRoot0(dz * dz + dx * dx);
    pen = (s32)(rA + rB) - (dist + 1);
    if (pen > 0) {
        s32 dy = (s32)b->scaWorld[1] + (b->t[1] - (s32)wA[1]) - tA[1];
        s32 maxH = (s32)hA + (s32)hB;

        if ((-maxH < dy) && (dy < maxH)) {
            s32 px = (pen * dx) / (dist + 1);
            s32 pz = (pen * dz) / (dist + 1);
            s32 dy2 = (s32)b->scaWorld[1] + ((s32)b->pos[1] - (s32)wA[1]) - tA[1];

            if ((dy2 <= -maxH) || (maxH <= dy2)) {
                if (((b->pos[0] < tA[0]) && (tA[0] < b->t[0])) || ((tA[0] < b->pos[0]) && (b->t[0] < tA[0]))) {
                    px = (-px < 1) ? -(-px + rA * 2) : (rA * 2 + px);
                }
                if (((b->pos[2] < tA[2]) && (tA[2] < b->t[2])) || ((tA[2] < b->pos[2]) && (b->t[2] < tA[2]))) {
                    pz = (-pz < 1) ? -(-pz + rA * 2) : (rA * 2 + pz);
                }
            }
            b->t[0] += px;
            b->t[2] += pz;
            return 1;
        }
    }
    return 0;
}

/** HandleEnemyPlayerCollisions: the other enemies push this one. */
static void HandleEnemyCollisions(void) {
    s32 i;

    for (i = 0; i < NV_RE1_MAX_ENTS; i++) {
        NvRe1Entity* o = sEnts[i];

        if ((o != NULL) && (o != E) && (o->status_flags != 0) && !((o->status_flags | E->status_flags) & 2)) {
            ResolveScaPush(o->sca, o->scaWorld, o->t, E);
        }
    }
}

/* ---- Joint_move (PlayerAnimations.cpp) ---- */

static const u16* AnimStep(NvRe1Entity* e, s32 reverse) {
    const NvRe1Anim* a = e->anim;
    s32 n;
    s32 f;

    if ((a == NULL) || (e->animationId >= a->numAnims)) {
        return NULL;
    }
    n = a->numSteps[e->animationId];
    if (n == 0) {
        return NULL;
    }
    f = reverse ? (n - 1 - e->animation_frame_id) : e->animation_frame_id;
    f = CLAMP(f, 0, n - 1);
    return a->steps[e->animationId] + f * 2;
}

static const s16* AnimData(NvRe1Entity* e, const u16* step) {
    if ((step == NULL) || (step[0] >= e->anim->numFrames)) {
        return NULL;
    }
    return e->anim->frames + step[0] * e->anim->frameShorts;
}

s32 NvRe1_JointMove(NvRe1Entity* e, s32 reverse, s16 blendStep) {
    const u16* step;
    const s16* data;
    s32 j;

    if (e->timing_control > 1) {
        e->timing_control--;
        return 0;
    }
    step = AnimStep(e, reverse);
    data = AnimData(e, step);
    if (data == NULL) {
        return 0;
    }
    e->rootT[0] = data[0];
    if (e->blend_counter == 0) {
        e->rootT[1] = data[1];
        e->rootT[2] = data[2];
        for (j = 0; j < e->anim->numJoints && j < NV_RE1_JOINTS; j++) {
            e->rot[j][0] = data[6 + j * 3];
            e->rot[j][1] = data[6 + j * 3 + 1];
            e->rot[j][2] = data[6 + j * 3 + 2];
        }
    } else {
        s32 stepW = blendStep;
        u32 blend = e->blend_counter;
        s32 inv = (s32)(0x1000 / stepW) - (s32)blend;
        s32 ty = data[1] * inv * stepW;
        s32 cy = e->rootT[1] * (s32)blend * stepW;

        e->rootT[1] = (s16)(((ty + ((ty >> 31) & 0xFFF)) >> 12) + ((cy + ((cy >> 31) & 0xFFF)) >> 12));
        e->rootT[2] = data[2];
        for (j = 0; j < e->anim->numJoints && j < NV_RE1_JOINTS; j++) {
            s32 k;

            if (inv == 1) { // the last step: take the shorter way round
                for (k = 2; k >= 0; k--) {
                    s16 cur = e->rot[j][k];
                    u16 diff = (u16)((data[6 + j * 3 + k] - cur) + 0x800);

                    if (0x1000 < diff) {
                        e->rot[j][k] = (s16)((((diff & 0x8000) == 0) * 0x2000) + cur - 0x1000);
                    }
                }
            }
            for (k = 0; k < 3; k++) { // fp_lerp
                s32 t = data[6 + j * 3 + k] * (inv * stepW);
                s32 c = e->rot[j][k] * (s32)(blend * stepW);

                e->rot[j][k] = (s16)(((t + ((t >> 31) & 0xFFF)) >> 12) + ((c + ((c >> 31) & 0xFFF)) >> 12));
            }
        }
        e->blend_counter--;
    }
    e->timing_control = (u8)step[1];
    e->animation_frame_id++;
    if ((s32)(e->anim->numSteps[e->animationId] - 1) < (s32)e->animation_frame_id) {
        e->animation_frame_id = 0;
        return 1;
    }
    return 0;
}

static s32 Joint_move(s32 reverse, s16 blendStep) {
    return NvRe1_JointMove(E, reverse, blendStep);
}

/** entity_extract_anim_vertex: the current frame's second vector (its root speed slot). */
static void ExtractAnimVertex(s32* vx, s32* vz) {
    u8 saved = E->animation_frame_id;
    const s16* data;

    if (E->timing_control > 1) {
        E->animation_frame_id--;
        if (E->animation_frame_id == 0xFF) {
            E->animation_frame_id = (u8)(E->anim->numSteps[E->animationId] - 1);
        }
    }
    data = AnimData(E, AnimStep(E, 0));
    *vx = data ? data[3] : 0;
    *vz = data ? data[5] : 0;
    E->animation_frame_id = saved;
}

static void entity_apply_anim_vertex(void) {
    s32 vx;
    s32 vz;
    s32 x;
    s32 z;

    ExtractAnimVertex(&vx, &vz);
    RotY(E->angle, vx, vz, &x, &z);
    E->t[0] = E->unk_c6 + x;
    E->t[2] = E->unk_c8 + z;
}

static void snap_player_to_grab_position(void) {
    s32 vx;
    s32 vz;
    s32 x;
    s32 z;

    ExtractAnimVertex(&vx, &vz);
    RotY(E->angle, vx, vz, &x, &z);
    E->unk_c6 = (s16)E->t[0] - (s16)x;
    E->unk_c8 = (s16)E->t[2] - (s16)z;
}

/* ---- the zombie (Zombie.cpp) ---- */

static const s16 sZombieSca[2][8] = {
    { (s16)0x8001, 0, (s16)0xFA06, 0, 0x05FA, 0x01A6, 0, 0 }, // standard: r 422
    { (s16)0x8001, 0, (s16)0xFA06, 0, 0x05FA, 0x0142, 0, 0 }, // naked: r 322
};
static const u8 sHealth[16] = { 59, 59, 79, 59, 59, 39, 59, 79, 79, 99, 59, 79, 59, 59, 79, 59 };
static const u8 sAnimId[16] = { 0, 0, 9, 9, 0, 12, 9, 29, 0, 0, 9, 0, 0, 0, 0, 0 };
static const u8 sStagger[32] = { 4, 3, 5, 3, 4, 4, 3, 4, 3, 5, 4, 4, 5, 3, 4, 5,
                                 4, 3, 3, 4, 4, 3, 4, 4, 5, 3, 5, 3, 4, 3, 3, 4 };
static const u8 sThresholdInit[64] = {
    3, 2, 3, 3, 2, 3, 4, 3, 2, 3, 3, 4, 3, 2, 3, 3, 3, 2, 3, 3, 3, 3, 2, 3, 3, 2, 3, 4, 3, 4, 3, 2,
    1, 2, 3, 2, 3, 3, 2, 3, 3, 2, 2, 3, 2, 3, 2, 3, 2, 3, 2, 4, 2, 2, 2, 2, 3, 2, 2, 3, 2, 2, 2, 3,
};
static const u8 sThresholdNormal[32] = { 1, 2, 3, 2, 3, 3, 2, 3, 3, 2, 2, 3, 2, 3, 2, 3,
                                         2, 3, 2, 4, 2, 2, 2, 2, 3, 2, 2, 3, 2, 2, 2, 3 };
static const u8 sDamageActionTbl[14] = { 0, 0, 5, 0, 2, 0, 0, 4, 4, 0, 0, 0, 0, 0 };
static const u8 sAttackAnim[12] = { 0x11, 0x00, 0x00, 0x14, 0x03, 0x00, 0x17, 0x06, 0x06, 0x1A, 0x09, 0x0E };
static const u8 sDamageNormal[16] = { 10, 10, 6, 6, 10, 10, 10, 10, 10, 0, 0, 0, 0, 0, 0, 0 };
static const u16 sAttackData[12] = { 0x0004, 0x0020, 0x0020, 0x0004, 0x0055, 0x000C,
                                     0x0233, 0x0041, 0xFFE0, 0x0000, 0x0A0A, 0x090C };
static const u8 sRecovery[16] = { 2, 4, 4, 4, 4, 4, 4, 6, 6, 6, 6, 6, 6, 9, 9, 9 };

// RE1's bite takes the player's health; the actor turns it into hearts (z_nv_enemy.c)
s16 gNvRe1BiteDamage;

static void SetState(u8 state, u8 ignore, u8 action, u8 actionState) {
    E->state = state;
    E->ignore_player_flag = ignore;
    E->action_behavior = action;
    E->action_state = actionState;
}

static void SetAnim(u8 id, u8 blend) {
    E->animation_frame_id = 0;
    E->timing_control = 0;
    E->animationId = id;
    E->blend_counter = blend;
}

static void zombie_dead_animation(void);
static void zombie_attack(void);
static void update_zombie_action(void);
static void zombie_update_player_distance(void);
static void zombie_pushback_action(void);
static void short_push_back(void);

void NvZombie_Init(NvRe1Entity* e) {
    E = e;
    SetState(ZST_IDLE, 0, 0, 0);
    E->action_ticks_counter = 0;
    E->death_timer = 0;
    E->hit_state = 0;
    E->sca = sZombieSca[0];
    E->health = sHealth[Rand_Next() & 0xF] - (s16)(Rand_Next() % 22);
    if ((E->behavior_flags & 0xF) == 5) {
        E->action_state = 2;
    }
    if (E->id == 1) {
        E->sca = sZombieSca[1];
    }
    E->is_moving = 0;
    E->move_max_steps = 0;
    E->dir_control_flags = 0;
    E->turn_counter = 0;
    E->action_speed = 0;
    E->hit_threshold = sThresholdInit[(sRandSeed & 0x1F) + 32]; // (first playthrough)
    E->stagger_timer = sStagger[sRandSeed & 0x1F];
    E->behavior_step = 0;
    E->move_speed = 45;
    E->turn_speed = 24;
    E->action_counter = 0;
    E->internal_timer = 0;
    E->blend_counter = 0;
    E->animationId = sAnimId[E->behavior_flags & 0xF];
    E->animation_frame_id = 0;
    E->timing_control = 0;
    Joint_move(0, 0x400);
    if (E->behavior_flags & ZF_LAYING_DOWN) {
        E->t[1] = 1;
    }
    if (E->behavior_flags == 6) { // dead on the floor
        E->status_flags |= 0x0A;
        E->health = -1;
        SetState(ZST_DIE, 1, 4, 0);
    }
    if (E->behavior_flags == 10) {
        E->status_flags |= 0x04;
    }
    if (E->behavior_flags & ZF_VOMITING) {
        E->state = ZST_ACTION_UPDATE;
    }
    E->pos[0] = (s16)E->t[0];
    E->pos[1] = (s16)E->t[1];
    E->pos[2] = (s16)E->t[2];
    E->angleBackup = E->angle;
    SetScaWorld(E->sca, E->angle, E->scaWorld);
}

/* the action table */

static s32 TickExpired(void) {
    s16 t = (s16)E->action_ticks_counter;

    E->action_ticks_counter = (u16)(t - 1);
    return t == 0;
}

static void zombie_idle(void) {
    switch (E->action_state) {
        case 0:
            E->action_state = 1;
            SetAnim(0, 3);
            E->action_ticks_counter = (sRandSeed & 0x7F) + 50;
            // fall through
        case 1:
            Joint_move(0, 0x400);
            if (--E->action_ticks_counter == 0) {
                E->action_behavior = 1;
                E->action_state = 0;
                return;
            }
            break;
        case 2:
            E->action_state = 3;
            SetAnim(0, 3);
            E->action_ticks_counter = (sRandSeed & 0xF) + 8;
            // fall through
        case 3:
            Joint_move(0, 0x400);
            if (TickExpired()) {
                E->action_state = 4;
                E->action_ticks_counter = (sRandSeed & 0xF) + 8;
            }
            E->angle += 12;
            break;
        case 4:
            Joint_move(0, 0x400);
            if (TickExpired()) {
                E->action_state = 5;
                E->action_ticks_counter = (sRandSeed & 0xF) + 4;
            }
            E->angle -= 24;
            break;
        case 5:
            Joint_move(0, 0x400);
            if (TickExpired()) {
                E->action_state = 6;
                E->action_ticks_counter = (sRandSeed & 0xF) + 8;
            }
            E->angle -= 32;
            break;
        case 6:
            Joint_move(0, 0x400);
            if (TickExpired()) {
                E->action_state = 7;
                E->action_ticks_counter = (sRandSeed & 0xF) + 4;
            }
            E->angle += 24;
            break;
        case 7:
            E->action_behavior = 0;
            E->action_state = 0;
            break;
    }
}

static void zombie_slow_walk(void) {
    if (E->action_state == 0) {
        s32 x;
        s32 z;

        E->move_speed_current = 20;
        E->action_state = 1;
        SetAnim(1, 3);
        E->action_ticks_counter = (sRandSeed & 0x7F) + 300;
        RotY(E->angle + 8, 5000, 0, &x, &z); // a waypoint 5000 ahead
        E->player_pos_x = (s16)((s16)E->t[0] + x);
        E->player_pos_z = (s16)((s16)E->t[2] + z);
    }
    Joint_move(0, 0x400);
    if (--E->action_ticks_counter == 0) {
        E->action_behavior = 0;
        E->action_state = 2;
    }
    sFrameIdSave = 7;
    entity_update_wander_turn(E->move_dist, &E->dir_control_flags, &E->turn_counter, E->turn_speed, 60);
    if (E->animation_frame_id > 15) {
        Add_speedXZ(0);
    }
}

static void zombie_chase_walk(void) {
    switch (E->action_state) {
        case 0:
            E->action_state = 1;
            E->move_speed_current = 0;
            SetAnim(2, 3);
            // fall through
        case 1:
            E->action_state += (u8)Joint_move(0, 0x400);
            break;
        case 2:
            E->move_speed_current = E->move_speed;
            E->action_state = 3;
            E->timing_control = 0;
            E->animationId = 3;
            E->blend_counter = 3;
            E->action_ticks_counter = (sRandSeed & 0x7F) + 10;
            E->next_turn_timer = 0;
            // fall through
        case 3:
            if (TickExpired()) {
                E->next_turn_timer = (sRandSeed & 0x0F) + 0x14;
                E->action_ticks_counter = E->next_turn_timer + (sRandSeed & 0x7F);
            }
            Joint_move(0, 0x400);
            E->move_speed_current = 45;
            if (E->next_turn_timer != 0) {
                sFrameIdSave = 7;
                entity_update_wander_turn(E->move_dist, &E->dir_control_flags, &E->turn_counter,
                                          (u16)(E->turn_speed * -2), 60);
                E->move_speed_current = 10;
                E->next_turn_timer--;
            }
            sDisp = Manhattan(P->t[0], P->t[2]);
            if ((P->isBeingAttackedFlag != 0) && (sDisp < 800)) {
                E->ignore_player_flag = 1;
                E->action_state++;
                E->action_ticks_counter = 60;
                SetAnim(0, 3);
                E->move_speed_current = 0;
            }
            break;
        case 4:
            Joint_move(0, 0x400);
            if (--E->action_ticks_counter == 0) {
                E->ignore_player_flag = 0;
                E->action_state = 0;
            }
            break;
    }
    Add_speedXZ(0);
}

static void fast_player_facing(void) {
    s32 step;
    s16 ticks;

    if (E->action_state == 0) {
        SetAnim(3, 3);
        E->hit_state = 0;
        E->action_state = 1;
        E->action_ticks_counter = (sRandSeed & 0x3F) + 0x1E;
    }
    step = turn_toward_target(P->t[0], P->t[2], 0x54);
    ticks = (s16)E->action_ticks_counter;
    E->action_ticks_counter = (u16)(ticks - 1);
    if ((ticks != 0) && (step != 0)) {
        Joint_move(0, 0x400);
        E->angle += (s16)step;
        return;
    }
    SetState(ZST_IDLE, 0, 2, 2);
    E->player_pos_x = (s16)P->t[0];
    E->player_pos_z = (s16)P->t[2];
}

static void benddown_and_eat(void) {
    u8 st = E->action_state;

    if (st == 0) {
        E->action_state = 3;
        SetAnim(0x0B, 3);
    }
    if (st == 1) {
        Joint_move(0, 0x400);
        if (--E->action_ticks_counter == 0) {
            E->action_state++;
        }
    }
    if (st == 2) {
        E->animation_frame_id = 0;
        E->timing_control = 0;
        E->action_state++;
        E->blend_counter = 3;
        E->animationId = 0x0B;
        st = 3;
    }
    if (st == 3) {
        if (Joint_move(0, 0x400)) {
            E->action_state++;
        }
        E->angle += (s16)turn_toward_target(P->t[0], P->t[2], 0x20);
        return;
    }
    if (st == 4) {
        E->action_state++;
        E->animationId++;
        E->timing_control = 0;
        E->action_ticks_counter = 120;
        st = 5;
    }
    if (st == 5) {
        Joint_move(0, 0x400);
        if (TickExpired()) {
            E->action_state++;
            E->animationId++;
            E->animation_frame_id = 0;
            E->timing_control = 0;
            E->blend_counter = 3;
        }
    }
    if (st == 6) {
        if (Joint_move(0, 0x400)) {
            SetState(ZST_IDLE, 0, 0, 0);
        }
    }
}

static void turn_towards_player(void) {
    s16 ticks;

    if (E->action_state == 0) {
        E->action_state = 1;
        SetAnim(3, 3);
        E->move_speed_current = 45;
        Add_speedXZ((s16)sAttackData[E->attacking_direction * 3]);
        E->action_ticks_counter = sAttackData[E->attacking_direction * 3 + 1];
    }
    E->angle -= (s16)sAttackData[E->attacking_direction * 3 + 2];
    Joint_move(0, 0x400);
    ticks = (s16)E->action_ticks_counter;
    E->action_ticks_counter = (u16)(ticks - 1);
    if (ticks == 0) {
        E->ignore_player_flag = 1;
        E->action_behavior = 4;
        E->action_state = 0;
    }
    E->t[0] += E->speed[0];
    E->t[1] += E->speed[1];
    E->t[2] += E->speed[2];
}

static void zombie_vomiting(void) {
    switch (E->action_state) {
        case 0:
            E->action_state = 1;
            SetAnim(5, E->blend_counter); // (vomit billboard and sound: effects not ported)
            // fall through
        case 1:
            E->action_state += (u8)Joint_move(0, 0x400);
            return;
        case 2:
            E->action_state = 3;
            SetAnim(0, 3);
            E->action_ticks_counter = (sRandSeed & 0x1F) + 20;
            // fall through
        case 3:
            Joint_move(0, 0x400);
            if (--E->action_ticks_counter == 0) {
                E->ignore_player_flag = 0;
                E->action_behavior = 0;
                E->action_state = 0;
            }
    }
}

static void zombie_falldown(void) {
    switch (E->action_state) {
        case 0:
            E->action_state = 1;
            SetAnim(8, 7);
            E->move_speed_current = 20;
            E->t[1] = 1;
            break;
        case 1:
            E->behavior_step &= ~0x04;
            E->hit_state = 1;
            E->action_speed |= 0x80;
            if (E->animation_frame_id < 4) {
                E->action_speed &= ~0x80;
                E->behavior_step |= 0x04;
                E->hit_state = 0;
            }
            if (Joint_move(0, 0x200)) {
                E->hit_state = 0;
                E->action_ticks_counter = (u16)(sRecovery[sRandSeed & 0x0F] * 30);
                E->action_state = 2;
            }
            Add_speedXZ(0);
            break;
        case 2:
            if (--E->action_ticks_counter == 0) {
                E->action_state = 3;
                E->blend_counter = 0;
                E->hit_state = 1;
                return;
            }
            break;
        case 3:
            E->behavior_step &= ~0x04;
            E->hit_state = 1;
            E->action_speed |= 0x80;
            if (E->animation_frame_id > 0x1A) {
                E->action_speed &= ~0x7F;
                E->behavior_step |= 0x04;
                E->hit_state = 0;
            }
            if (Joint_move(1, 0x200)) {
                if (E->stagger_timer == 0) {
                    E->stagger_timer = sStagger[sRandSeed & 0x1F];
                    E->action_speed &= ~0x80;
                } else {
                    E->action_speed = 0;
                }
                E->hit_state = 0;
                SetState(ZST_IDLE, 1, 3, 0);
                E->behavior_step &= ~0x04;
                E->t[1] = 0;
            }
            Add_speedXZ(0x800);
            break;
    }
}

static void update_zombie_action(void);

/* the move behaviours (zombie_update_player_distance) */

static void zombie_walk1(void) {
    sFrameIdSave = 6;
    sPushZLo = E->action_behavior;
    if (((E->is_moving | (E->move_max_steps << 8)) & 0xFFFF) == 0) {
        u8 r = (u8)entity_pathfind_update();

        sEntityBkp = r;
        if ((r & 0xFE) == 0) {
            E->is_moving = (E->is_moving & 0xFE) | (sEntityBkp & 1);
        }
    }
    if (E->action_speed & 0x80) {
        SetState(ZST_IDLE, 1, 7, 0);
        return;
    }
    if ((E->action_behavior != 0) && (E->action_behavior != 1)) {
        sFrameIdSave = 7;
        entity_update_wander_turn(E->move_dist, &E->dir_control_flags, &E->turn_counter, E->turn_speed, 60);
    }
    if (sDisp < 3000) {
        if ((s16)turn_toward_target(P->t[0], P->t[2], 1024) != 0) {
            E->ignore_player_flag = 1;
            E->action_behavior = 3;
            E->action_state = 0;
            return;
        }
    }
    if (((E->is_moving | (E->move_max_steps << 8)) != 0) && (P->isBeingAttackedFlag == 0)) {
        if (sPushZLo != 2) {
            E->action_state = 0;
            E->blend_counter = 3;
        }
        E->action_behavior = 2;
        E->ignore_player_flag = 0;
    }
    if (P->isBeingAttackedFlag & 0x80) {
        if (sEntityBkp & 1) {
            if (sPushZLo != 2) {
                E->action_state = 0;
                E->blend_counter = 3;
            }
            E->action_behavior = 2;
            E->ignore_player_flag = 0;
        }
        if (sDisp < 1200) {
            if ((s16)turn_toward_target(P->t[0], P->t[2], 712) == 0) {
                E->ignore_player_flag = 1;
                E->action_behavior = 4;
                E->action_state = 0;
            }
        }
    }
}

static void zombie_check_player_distance(void) {
    u32 r;

    E->action_ticks_counter++;
    E->angle += (s16)turn_toward_target(E->player_pos_x, E->player_pos_z, 4);
    r = entity_pathfind_update();
    if (((r & 1) == 0) || (sDisp > 8999)) {
        if ((E->action_behavior != 0) || (sDisp < 3000)) {
            E->behavior_flags--;
            if (E->behavior_flags == 7) {
                E->behavior_flags = 4;
            }
        }
    } else {
        E->behavior_flags--;
        if (E->behavior_flags == 7) {
            E->behavior_flags = 4;
        }
    }
}

static void zombie_slow_walk_alt(void) {
    s32 prevAction = E->action_behavior;

    if (((E->is_moving | (E->move_max_steps << 8)) & 0xFFFF) == 0) {
        u8 r = (u8)entity_pathfind_update();

        sEntityBkp = r;
        if ((r & 0xFE) == 0) {
            E->is_moving = (E->is_moving & 0xFE) | (sEntityBkp & 1);
        }
        return;
    }
    if (prevAction != 1) {
        E->action_state = 0;
        E->blend_counter = 3;
    }
    E->action_behavior = 1;
    E->ignore_player_flag = 0;
}

static void zombie_idling(void) {
    if (E->action_behavior != 0) {
        E->action_behavior = 0;
        E->action_state = 0;
    }
    entity_pathfind_update();
}

static void zombie_walk2(void) {
    sFrameIdSave = 6;
    sTempVar = E->action_behavior;
    if (((E->is_moving | (E->move_max_steps << 8)) & 0xFFFF) == 0) {
        u32 r = entity_pathfind_update();

        sEntityBkp = r;
        if ((r & 0xFE) == 0) {
            E->is_moving = (E->is_moving & 0xFE) | (r & 1);
        }
    }
    if ((E->action_behavior != 0) && (E->action_behavior != 1)) {
        sFrameIdSave = 7;
        entity_update_wander_turn(E->move_dist, &E->dir_control_flags, &E->turn_counter, E->turn_speed, 60);
    }
    if (E->action_speed & 0x80) {
        SetState(ZST_IDLE, 1, 7, 0);
        return;
    }
    if (sDisp < 3000) {
        if ((s16)turn_toward_target(P->t[0], P->t[2], 0x400) != 0) {
            E->ignore_player_flag = 1;
            E->action_behavior = 3;
            E->action_state = 0;
            return;
        }
    }
    if ((E->collisionFlags & 8) && (sDisp < 2000)) {
        if ((s16)turn_toward_target(P->t[0], P->t[2], 0x200) == 0) {
            E->ignore_player_flag = 1;
            E->action_behavior = 6;
            E->action_state = 0;
            if (P->isBeingAttackedFlag != 0) {
                E->action_state = 2;
            }
            return;
        }
    }
    if (((E->is_moving | (E->move_max_steps << 8)) != 0) && (P->isBeingAttackedFlag == 0)) {
        if (sTempVar != 2) {
            E->action_state = 0;
            E->blend_counter = 3;
        }
        E->action_behavior = 2;
        E->ignore_player_flag = 0;
    }
    if (P->isBeingAttackedFlag & 0x80) {
        if (sEntityBkp & 1) {
            if (sTempVar != 2) {
                E->action_state = 0;
                E->blend_counter = 3;
            }
            E->action_behavior = 2;
            E->ignore_player_flag = 0;
        }
        if (!(E->collisionFlags & 8) && (sDisp < 1200)) {
            if ((s16)turn_toward_target(P->t[0], P->t[2], 0x2C8) == 0) {
                E->ignore_player_flag = 1;
                E->action_behavior = 4;
                E->action_state = 0;
            }
        }
    }
}

static void zombie_move_behavior(s32 idx) {
    switch (idx) {
        case 0:
            zombie_walk1();
            break;
        case 1:
        case 5:
        case 6:
        case 7:
        case 8:
            zombie_check_player_distance();
            break;
        case 2:
            zombie_slow_walk_alt();
            break;
        case 3:
        case 10:
            zombie_idling();
            break;
        case 4:
            zombie_walk2();
            break;
        default:
            break; // NULL in the original
    }
}

static void zombie_update_player_distance(void) {
    if ((E->is_moving | (E->move_max_steps << 8)) != 0) {
        zone_path_find(P->t[0], P->t[2], &E->player_pos_x, &E->player_pos_z);
    }
    sDisp = Manhattan(P->t[0], P->t[2]);
    zombie_move_behavior(E->behavior_flags & 0x0F);
}

static void update_zombie_action(void) {
    switch (E->action_behavior) {
        case 0:
            zombie_idle();
            break;
        case 1:
            zombie_slow_walk();
            break;
        case 2:
            zombie_chase_walk();
            break;
        case 3:
            fast_player_facing();
            break;
        case 4:
            benddown_and_eat();
            break;
        case 5:
            turn_towards_player();
            break;
        case 6:
            zombie_vomiting();
            break;
        case 7:
            zombie_falldown();
            break;
        default:
            if (E->action_behavior <= 12) {
                zombie_move_behavior(E->action_behavior - 8);
            }
            break;
    }
}

/* the damage reactions */

static void short_push_back(void) {
    if (!(E->behavior_flags & ZF_VOMITING)) {
        E->animationId = 5 - (E->action_behavior == 0);
    }
    E->move_speed_current = 15;
    if (E->action_state == 0) {
        E->action_state = 1;
        E->move_speed_current = 0;
        E->animation_frame_id = 0;
        E->timing_control = 0;
        E->blend_counter = 3;
        if (E->internal_timer == 0) {
            E->internal_timer = 150; // (the moan: sounds not ported)
        }
    }
    if (Joint_move(0, 1024)) {
        E->action_state++;
        if (!(E->behavior_flags & ZF_VOMITING)) {
            if ((E->behavior_flags & 0x0F) == 5) {
                E->behavior_flags = 0;
            }
            SetState(ZST_IDLE, 1, 3, 0);
        }
        E->player_pos_x = (s16)P->t[0];
        E->player_pos_z = (s16)P->t[2];
        E->hit_state = 0;
    }
    if (E->hit_state & 1) {
        E->action_counter++;
        if (E->action_counter == 2) {
            E->move_speed = 20;
            E->turn_speed = 14;
        }
    }
    Add_speedXZ(0);
}

static void push_and_stagger(void) {
    u16 speedArg;

    if (E->action_behavior == 2) {
        E->animationId = 7;
        speedArg = 0x800;
    } else {
        E->animationId = 6;
        speedArg = 0;
    }
    E->move_speed_current = 15;
    if (E->action_state == 0) {
        E->action_state = 1;
        E->animation_frame_id = 0;
        E->timing_control = 0;
        E->blend_counter = 3;
        E->animationId = 6;
        if (E->internal_timer == 0) {
            E->internal_timer = 150;
        }
    }
    if (Joint_move(0, 1024)) {
        if ((E->behavior_flags & 0x0F) == 5) {
            E->behavior_flags = 0;
        }
        SetState(ZST_IDLE, 1, 3, 0);
        E->player_pos_x = (s16)P->t[0];
        E->player_pos_z = (s16)P->t[2];
        E->hit_state = 0;
    } else {
        if (E->animation_frame_id < 5) {
            E->move_speed_current += 90;
        }
        if (E->animation_frame_id == 0x14) {
            E->hit_state = 0;
        }
    }
    Add_speedXZ(speedArg);
}

static void push_and_drop(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        E->animation_frame_id = 0;
        E->timing_control = 0;
        E->animationId = 15;
        if (E->internal_timer == 0) {
            E->internal_timer = 150;
        }
    }
    if (Joint_move(0, 1024)) {
        SetState(ZST_IDLE, 0, 1, 0);
        E->player_pos_x = (s16)P->t[0];
        E->player_pos_z = (s16)P->t[2];
        E->hit_state = 0;
        if (!(E->collisionFlags & 8)) {
            if (!(E->behavior_flags & 0x20) && ((E->behavior_flags & 0x0F) == 3)) {
                E->behavior_flags = 2;
            }
        }
    }
}

static void explode_leg_and_drop(void) {
    switch (E->action_state) {
        case 0:
            E->action_state = 1;
            E->death_timer = 70;
            SetAnim(8, 3);
            E->move_speed_current = 0x14;
            if (E->internal_timer == 0) {
                E->internal_timer = 150;
            }
            if (E->health < 0) {
                E->health = 1;
            }
            E->t[1] = 1;
            // fall through
        case 1:
            E->action_state += (u8)Joint_move(0, 1024);
            break;
        case 2: {
            s32 saved[3] = { E->t[0], E->t[1], E->t[2] };
            u8 blocked;

            E->status_flags |= 0x0E;
            blocked = check_room_collision_two_point(-800, 800);
            E->t[0] = saved[0];
            E->t[1] = saved[1];
            E->t[2] = saved[2];
            if ((E->behavior_flags != 4) && (blocked == 0) && (E->health > 0)) {
                E->behavior_flags = (u8)(((sRandSeed & 3) == 0) + 2);
                SetState(ZST_IDLE, 0, 0, 0);
                E->hit_state = 0;
                E->player_pos_x = (s16)P->t[0];
                E->player_pos_z = (s16)P->t[2];
                E->status_flags &= 0xF1;
                return;
            }
            E->health = -1;
            if (E->death_event_id != 0xFF) {
                void NvFlags_Op(s32 bank, s32 bit, s32 mode);

                NvFlags_Op(3, E->death_event_id, 0);
            }
            E->action_state = 3;
            E->status_flags |= 0x0E;
            E->move_speed_current = 0;
        }
            // fall through
        case 3:
            E->death_timer--;
            if (E->death_timer == 0) {
                E->action_state = 4;
            }
            break;
    }
    Add_speedXZ(0);
}

static void long_push_back(void) {
    u8 st = E->action_state;

    if (st == 0) {
        E->action_state = 1;
        E->animation_frame_id = 0x0C;
        E->timing_control = 1;
        E->hit_state = 1;
        E->animationId = 7;
        E->move_speed_current = 0x1E;
    } else if (st != 1) {
        if (st == 2) {
            if (TickExpired()) {
                SetState(ZST_IDLE, 0, 2, 2);
                E->player_pos_x = (s16)P->t[0];
                E->player_pos_z = (s16)P->t[2];
                E->hit_state = 0;
            }
        }
        Add_speedXZ(2048);
        return;
    }
    if (!Joint_move(0, 0x400)) {
        if (E->animation_frame_id == 0x14) {
            E->hit_state = 0;
        }
    } else {
        E->action_state = 2;
        E->action_ticks_counter = sRandSeed & 7;
        E->move_speed_current = 15;
    }
    Add_speedXZ(2048);
}

static void benddown_and_standup(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        E->animation_frame_id = 0;
        E->timing_control = 0;
        E->animationId = 0x0D;
    } else if (E->action_state != 1) {
        return;
    }
    if (Joint_move(0, 0x400)) {
        SetState(ZST_IDLE, 1, 3, 0);
        E->behavior_flags = 0;
        E->hit_state = 0;
    }
}

static void zombie_damage_behavior(void) {
    switch (E->action_behavior) {
        case 0:
        case 1:
            short_push_back();
            break;
        case 2:
        case 3:
            push_and_stagger();
            break;
        case 4:
            push_and_drop();
            break;
        case 5:
            explode_leg_and_drop();
            break;
        case 6:
            long_push_back();
            break;
        case 7:
            benddown_and_standup();
            break;
        case 8:
            zombie_idle();
            break;
        case 9:
            zombie_slow_walk();
            break;
        case 10:
            zombie_chase_walk();
            break;
        case 11:
            fast_player_facing();
            break;
    }
}

static void zombie_damaged(void) {
    if (E->ignore_player_flag == 0) {
        u8 beh = E->behavior_flags & 0x0F;

        if (E->behavior_step & 0x04) {
            E->state = E->state_mirror[0];
            E->ignore_player_flag = E->state_mirror[1];
            E->action_behavior = E->state_mirror[2];
            E->action_state = E->state_mirror[3];
            E->hit_state = 0;
            return;
        }
        if (E->action_speed & 0x80) {
            SetState(ZST_IDLE, 1, 7, 3);
            E->blend_counter = 0;
            E->hit_state = 1;
            return;
        }
        if ((E->hit_state & 0x78) == 0x08) {
            E->action_speed++;
            if ((s8)E->hit_threshold <= (s8)E->action_speed) {
                if (!(E->behavior_flags & ZF_LAYING_DOWN)) {
                    E->action_speed = 0x80;
                    E->hit_threshold = sThresholdNormal[sRandSeed & 0x1F];
                }
            }
        }
        if (((E->hit_state & 0x78) == 0x10) && (E->action_state == 0) && (--E->stagger_timer == 0) &&
            !(E->behavior_flags & ZF_LAYING_DOWN)) {
            E->action_speed = 0x80;
        }
        E->action_behavior = sDamageActionTbl[(E->hit_state & 7) + (E->behavior_flags & ZF_LAYING_DOWN) * 3];
        if ((E->hit_state - 1) & 0x02) {
            u32 a = (u32)turn_toward_target(P->t[0], P->t[2], 1024);

            E->action_behavior += (a >> 10) & 1;
        }
        if ((beh == 7) || (beh == 5)) {
            E->action_behavior = 7;
            if (E->animationId == 13) {
                E->action_state = 1;
            }
        }
        E->ignore_player_flag = 1;
    }
    zombie_damage_behavior();
}

static void zombie_dead_animation(void) {
    switch (E->action_state) {
        case 0:
            E->action_state = 1;
            E->animation_frame_id = 0;
            E->timing_control = 0;
            E->death_timer = 70;
            E->blend_counter = 3;
            E->hit_state = 1;
            // fall through
        case 1:
            E->action_state += (u8)Joint_move(0, 0x400);
            break;
        case 2: {
            s16 savedAngle = E->angle;
            s32 saved[3] = { E->t[0], E->t[1], E->t[2] };
            u8 blocked = check_room_collision_two_point(-800, 800);

            E->t[0] = saved[0];
            E->t[1] = saved[1];
            E->t[2] = saved[2];
            E->angle = savedAngle;
            // the "it gets back up" path
            if (!(E->behavior_flags & 0x40) && (E->behavior_flags != 0x04) && (E->animationId == 8) &&
                (blocked == 0) && ((sRandSeed & 3) == 0)) {
                E->behavior_flags = 3;
                E->health = 1;
                SetState(ZST_IDLE, 0, 0, 0);
                E->hit_state = 0;
                E->t[1] = 1;
                E->behavior_step &= ~0x04;
                E->status_flags &= 0x1F;
                return;
            }
            E->health = -1;
            if (E->death_event_id != 0xFF) {
                void NvFlags_Op(s32 bank, s32 bit, s32 mode);

                NvFlags_Op(3, E->death_event_id, 0);
            }
            E->action_state = 3;
            E->status_flags |= 0x0E;
        }
            // fall through
        case 3:
            E->move_speed_current = 0;
            E->death_timer--;
            if (E->death_timer == 0) {
                E->action_state = 4;
                return;
            }
            break;
        case 4:
            E->move_speed_current = 0;
            if (E->behavior_flags & 0x40) { // a script's corpse: tell the script (g_SysFlags)
                void NvFlags_Op(s32 bank, s32 bit, s32 mode);

                NvFlags_Op(4, E->scd_anim_param, 0);
                E->action_behavior = 0;
                E->action_state = 0;
            }
            return;
    }
}

static void magnum_shot_pushback(void) {
    if (E->action_state == 0) {
        E->behavior_step |= 0x01;
        E->move_speed_current = 45;
        E->action_state = 1;
        SetAnim(3, 3);
        return;
    }
    if (E->action_state == 1) {
        entity_rotate_toward_target(P->t[0], P->t[2], 32);
        if (Joint_move(0, 0x400)) {
            SetState(ZST_DIE, 1, 0, 3);
        }
    }
    Add_speedXZ(0);
}

static void zombie_die(void) {
    void NvFlags_Op(s32 bank, s32 bit, s32 mode);

    if (E->ignore_player_flag == 0) {
        E->ignore_player_flag = 1;
        E->action_behavior = 0;
        if ((E->behavior_flags & ZF_LAYING_DOWN) || (E->action_speed & 0x80) || (E->behavior_flags == 5)) {
            E->action_behavior = 2;
        }
        if ((E->hit_state & 7) == 4) {
            if ((s16)turn_toward_target(P->t[0], P->t[2], 1024) == 0) {
                E->action_behavior = 1;
            }
        }
        E->behavior_step &= ~0x04;
        if (E->death_event_id != 0xFF) {
            NvFlags_Op(3, E->death_event_id, 0);
        }
    }
    switch (E->action_behavior) {
        case 0:
            E->animationId = 8;
            E->move_speed_current = 20;
            zombie_dead_animation();
            Add_speedXZ(0);
            return;
        case 1:
            E->animationId = 10;
            E->move_speed_current = 20;
            if (E->animation_frame_id < 5) {
                E->move_speed_current += 90;
            }
            zombie_dead_animation();
            Add_speedXZ(2048);
            return;
        case 2:
            E->animationId = 16;
            zombie_dead_animation();
            return;
        case 3:
            magnum_shot_pushback();
            return;
    }
}

/* the attack */

static void zombie_attack_withdraw(void) {
    entity_apply_anim_vertex();
    if (Joint_move(0, 0x400)) {
        SetState(ZST_DAMAGED, 1, 6, 0);
        E->status_flags &= 0xF5;
        E->hit_state = 1;
    }
}

static void zombie_attack_head_bite(void) {
    entity_apply_anim_vertex();
    E->action_state += (u8)Joint_move(0, 0x400);
    if (sAttackAnim[E->attacking_direction * 3 + 2] == E->animation_frame_id) {
        E->action_state = 10;
        E->headless = true; // the head bursts (z_nv_enemy.c draws it gone, with the burst)
    }
}

static void zombie_attack_vomit(void) {
    entity_apply_anim_vertex();
    E->action_state += (u8)Joint_move(0, 0x400);
}

static void zombie_attack_headless_death(void) {
    void NvFlags_Op(s32 bank, s32 bit, s32 mode);

    if (E->death_event_id != 0xFF) {
        NvFlags_Op(3, E->death_event_id, 0);
    }
    E->death_timer = 0x46;
    SetState(ZST_DIE, 1, 2, 3);
    E->status_flags |= 0x0E;
    E->health = -1;
    E->hit_state = 1;
    E->headless = true; // (cases 7 and 9 both park a headless corpse: the vomit path's too)
}

static void zombie_attack(void) {
    s8 reduce;

    switch (E->action_state) {
        case 0:
            SetAnim(0, 3);
            E->action_state = 1;
            E->status_flags |= 0x01 | 0x08;
            E->attacking_direction = (E->behavior_flags & ZF_LAYING_DOWN) + (u8)is_facing_toward_entity();
            E->animationId = sAttackAnim[E->attacking_direction * 3];
            E->hit_state = 1;
            E->animation_frame_id = 0;
            E->timing_control = 0;
            snap_player_to_grab_position();
            P->isBeingAttackedFlag = 1;
            break;
        case 1:
            entity_apply_anim_vertex();
            E->action_state += (u8)Joint_move(0, 0x400);
            break;
        case 2:
            E->action_state = 3;
            E->animationId++;
            E->timing_control = 0;
            E->action_ticks_counter = 0;
            E->attack_timer = 105;
            if (P->attackTimer != 0) {
                E->attack_timer = 30;
            }
            break;
        case 3: {
            s16 tick = (s16)E->action_ticks_counter;

            E->action_ticks_counter = (u16)(tick + 1);
            if ((tick % 19) == 0) {
                u8 damage = sDamageNormal[E->behavior_flags & 0x0F];

                P->health -= damage;
                gNvRe1BiteDamage += damage;
                if ((P->health < 0) && (E->attacking_direction & 2)) {
                    P->health = 1;
                }
            }
            entity_apply_anim_vertex();
            Joint_move(0, 0x400);
            reduce = reduce_attack_time_by_btn_press();
            E->attack_timer -= (u8)(reduce + 1);
            if ((s8)E->attack_timer < 0) {
                E->action_state = 4;
            }
            if (P->health < 0) {
                SetState(ZST_IDLE, 1, 5, 0);
                E->action_behavior += (E->behavior_flags & ZF_LAYING_DOWN) ? -5 : 0;
            }
            break;
        }
        case 4:
            E->action_state = 5;
            E->animationId++;
            E->animation_frame_id = 0;
            E->timing_control = 0;
            E->blend_counter = 3;
            if (!(E->behavior_flags & ZF_LAYING_DOWN)) {
                zombie_attack_withdraw();
                break;
            }
            if (E->attacking_direction != 2) { // ((player id & 1) == 0: Jill is id 1)
                E->action_state = 6;
                zombie_attack_head_bite();
                break;
            }
            E->action_state = 8;
            zombie_attack_vomit();
            break;
        case 5:
            zombie_attack_withdraw();
            break;
        case 6:
            zombie_attack_head_bite();
            break;
        case 8:
            zombie_attack_vomit();
            break;
        case 7:
        case 9:
            zombie_attack_headless_death();
            break;
        case 10:
            entity_apply_anim_vertex();
            if (Joint_move(0, 0x400)) {
                E->action_state = 7;
            }
            break;
    }
}

static s32 TryAttack(s16 fov, s16 dist) {
    if (checkAngularViewAndDistance(fov, dist, P->t[0], P->t[2])) {
        if (!check_line_of_sight(P->t[0], P->t[2]) && (P->isBeingAttackedFlag == 0)) {
            E->angle = getAngleTowardsTarget(P->t[0], P->t[2]);
            SetState(ZST_ATTACK, 0, E->action_behavior, 0);
            E->action_state = 0;
            zombie_attack();
            return true;
        }
    }
    return false;
}

static void zombie_chase_player(void) {
    if (!(E->behavior_step & 0x04) && !(E->action_speed & 0x80) && TryAttack(700, 1500)) {
        return;
    }
    if (E->ignore_player_flag == 0) {
        zombie_update_player_distance();
    }
    update_zombie_action();
}

static void zombie_pushback_idle(void) {
    E->animationId = 9;
    if (E->action_state == 0) {
        E->action_state = 1;
        E->animation_frame_id = 0;
        E->timing_control = 0;
        E->blend_counter = 3;
    }
    Joint_move(0, 0x400);
}

static void zombie_pushback_stagger(void) {
    switch (E->action_state) {
        case 0:
            E->action_state = 1;
            SetAnim(0x0E, 3);
            break;
        case 1:
            Joint_move(0, 0x400);
            if (E->animation_frame_id == 5) {
                E->action_state = 2;
                E->blend_counter = 3;
                E->move_speed_current = 20;
                E->action_ticks_counter = 1;
            }
            break;
        case 2:
            Joint_move(0, 0x400); // (zombie_body_part_physics: the crawl keeps the speed it started at)
            E->turn_delta = (s16)turn_toward_target(E->player_pos_x, E->player_pos_z, 8);
            if (E->dir_control_flags & 0x80) {
                E->turn_delta = -E->turn_delta;
                E->action_state = 3;
                E->action_ticks_counter = (sRandSeed & 0x1F) + 30;
            }
            E->angle += E->turn_delta;
            Add_speedXZ(0);
            break;
        case 3:
            Joint_move(0, 0x400);
            E->angle += E->turn_delta;
            Add_speedXZ(0);
            if (--E->action_ticks_counter == 0) {
                E->action_state = 3;
            }
            break;
    }
    if (E->collisionFlags & 8) {
        E->behavior_flags++;
    }
}

static void zombie_pushback_action(void) {
    if (E->action_behavior == 0) {
        zombie_pushback_idle();
    } else {
        zombie_pushback_stagger();
    }
}

static void zombie_pushed_back(void) {
    if (TryAttack(512, 2200)) {
        return;
    }
    if (E->ignore_player_flag == 0) {
        zombie_update_player_distance();
    }
    zombie_pushback_action();
}

static void zombie_random_chase(void) {
    if (!(E->behavior_step & 0x04) && !(E->action_speed & 0x80) && !(E->collisionFlags & 0x08) &&
        TryAttack(700, 1500)) {
        return;
    }
    if (E->ignore_player_flag == 0) {
        zombie_update_player_distance();
    }
    update_zombie_action();
}

static void zombie_eating(void) {
    switch (E->action_state) {
        case 0:
            E->action_state = 1;
            E->animation_frame_id = sRandSeed & 0x1F;
            E->timing_control = 0;
            E->blend_counter = 3;
            E->animationId = 0x1E;
            E->action_ticks_counter = (sRandSeed & 0xF) + 0x2D;
            // fall through
        case 1:
            if (Joint_move(0, 0x400)) {
                E->animationId = (sRandSeed & 1) ? 0x1D : 0x1E;
            }
            sDisp = Manhattan(P->t[0], P->t[2]);
            if (sDisp < 3000) {
                E->action_state++;
                return;
            }
            break;
        case 2:
            E->action_state = 3;
            E->animation_frame_id = 0;
            E->timing_control = 0;
            E->animationId = 0x0D;
            // fall through
        case 3:
            if (Joint_move(0, 0x400)) {
                E->behavior_flags = 0;
                SetState(ZST_IDLE, 1, 3, 0);
                E->t[1] = 0;
            }
            break;
    }
}

static void zombie_behavior(s32 idx) {
    switch (idx) {
        case 0:
        case 1:
        case 6:
        case 8:
            zombie_chase_player();
            break;
        case 2:
        case 3:
        case 10:
            zombie_pushed_back();
            break;
        case 4:
            zombie_random_chase();
            break;
        case 5:
        case 7:
            zombie_eating();
            break;
        default:
            break; // NULL in the original
    }
}

static void zombie_state_check(void) {
    s32 dy;

    if (E->behavior_flags & ZF_SCD_CONTROLLED) {
        return;
    }
    zombie_behavior(E->behavior_flags & 0x0F);
    E->status_flags &= 0x1F;
    if (!(E->behavior_flags & ZF_LAYING_DOWN) && !(E->action_speed & 0x80)) {
        E->status_flags |= ST_ALIGNED;
        entity_check_alert_range(3000);
    }
    entity_check_visual_range(4500);
    dy = P->t[1] - E->t[1];
    if ((dy < -100) || (dy > 100)) {
        E->status_flags &= 0x1F;
        E->status_flags |= (dy < 0) ? ST_PLAYER_ABOVE : ST_PLAYER_BELOW;
    }
    if ((E->behavior_flags == (ZF_LAYING_DOWN | 0x05)) || (E->behavior_flags == 5)) {
        E->status_flags &= 0x1F;
        entity_check_visual_range(4000);
    }
    if (E->behavior_step & 0x04) {
        E->status_flags |= ST_ALIGNED;
    }
}

/* the SCD-driven actions (state 8) */

static void zombie_aggresive_roar(void) {
    switch ((s8)E->action_state) {
        case 0:
            E->action_state++;
            E->move_speed_current = 0;
            SetAnim(2, 3);
            // fall through
        case 1:
            if (Joint_move(0, 1024)) {
                E->action_state++;
                E->move_speed_current = 0x23;
                SetAnim(3, 3);
            }
            break;
        case 2:
            entity_rotate_toward_target(E->unk_c6, E->unk_c8, 64);
            Joint_move(E->scd_entity_flags & 1, 0x400);
            {
                s32 dz = E->t[2] - E->unk_c8;
                s32 dx = E->t[0] - E->unk_c6;

                if (SquareRoot0(dz * dz + dx * dx) < 150) {
                    E->action_state++;
                }
            }
            break;
        case 3: {
            void NvFlags_Op(s32 bank, s32 bit, s32 mode);

            NvFlags_Op(4, E->scd_anim_param, 0);
            if (!(E->collisionFlags & 0x80)) {
                E->action_behavior = 0;
                E->action_state = 0;
            } else {
                E->action_state = 2;
            }
            break;
        }
    }
    Add_speedXZ(0);
}

static void zombie_action_update(void) {
    void NvFlags_Op(s32 bank, s32 bit, s32 mode);

    switch (E->action_behavior) {
        case 2:
            zombie_aggresive_roar();
            break;
        case 10:
            E->animationId = 8;
            E->move_speed_current = 0x14;
            zombie_dead_animation();
            Add_speedXZ(0);
            break;
        case 11:
            E->animationId = 10;
            E->move_speed_current = 0x14;
            if (E->animation_frame_id < 5) {
                E->move_speed_current += 90;
            }
            zombie_dead_animation();
            Add_speedXZ(2048);
            break;
        case 12:
        case 13:
            E->animationId = (E->action_behavior == 12) ? 4 : 5;
            short_push_back();
            if (E->action_state & 2) {
                NvFlags_Op(4, E->scd_anim_param, 0);
                E->action_behavior = 0;
                E->action_state = 0;
            }
            break;
        default:
            break;
    }
}

/** zombie_update: one tick. */
void NvZombie_Update(NvRe1Entity* e) {
    E = e;
    switch (E->state) {
        case ZST_INIT:
            NvZombie_Init(e);
            break;
        case ZST_IDLE:
            zombie_state_check();
            break;
        case ZST_DAMAGED:
            zombie_damaged();
            break;
        case ZST_DIE:
            zombie_die();
            break;
        case ZST_ATTACK:
            zombie_attack();
            break;
        case ZST_ACTION_UPDATE:
            zombie_action_update();
            break;
        default:
            break; // ZST_NO_ACTION, and the NULL slots
    }
    E->state_mirror[0] = E->state;
    E->state_mirror[1] = E->ignore_player_flag;
    E->state_mirror[2] = E->action_behavior;
    E->state_mirror[3] = E->action_state;
    if (E->internal_timer != 0) {
        E->internal_timer--;
    }
    if (E->state != ZST_ATTACK) {
        u8 r;

        SetScaWorld(E->sca, E->angle, E->scaWorld);
        SetScaWorld(P->sca, P->angle, P->scaWorld);
        // the player pushes it (ResolveEntityScaCollision(player, ENTITY)), then the other enemies
        if (!((E->status_flags) & 2)) {
            ResolveScaPush(P->sca, P->scaWorld, P->t, E);
            HandleEnemyCollisions();
        }
        E->collisionFlags &= ~0x08;
        r = check_room_collision();
        E->dir_control_flags |= r;
        E->move_dist = (u16)sTempVar;
        if (E->behavior_flags & ZF_LAYING_DOWN) {
            E->dir_control_flags |= check_room_collision_two_point(-600, 600);
        }
    }
}

/**
 * apply_weapon_damage's end, for a hit OoT's collision found: the damage, the hit state (RE1's
 * record's, with the weapon id at bits 3-6) and the state change. A zombie already reacting
 * (hit_state set) isn't hit again, as in RE1.
 */
void NvZombie_Hit(NvRe1Entity* e, s16 damage, u8 hit, u8 weaponId) {
    e->health -= damage;
    e->hit_state = hit | (u8)(weaponId << 3);
    e->state = ZST_DIE;
    e->ignore_player_flag = 0;
    e->action_behavior = 0;
    e->action_state = 0;
    if (e->health >= 0) {
        e->state = ZST_DAMAGED;
    }
}
