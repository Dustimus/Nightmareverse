/*
 * Nightmareverse: RE1's 2D effects - fire, smoke, sparks, glass, blood, drips, the items' glint.
 *
 * A port of the decomp's EffectSystem.cpp (update_2d_effects, EffectActor_UpdateAndRender, the
 * behaviours) and PlayerAnimations.cpp's Effect_CreateBillboard, on RE1's own data
 * (re1/tools/soh_effects.py): the core types (re1/effects/core), the room's (<name>_effects), its
 * four texture pages (the Effspr files re1/effspr/<i> with the room's sprites over them) and RE1's
 * blend and colour tables (re1/effects/tables, from the player's executable).
 *
 * A pool of 64 effects. Each runs a chain of 24-byte phase blocks: the block's two behaviour
 * numbers (animId, updateId) pick what it does each tick, its flags (byte 10 of the header) say
 * whether it moves (2: its velocities integrated through its yaw and its parent's matrix), whether
 * its frames run (1) and how it's drawn (0x40 of byte 11 blended, 0x80 hidden). Positions stay in
 * RE1's units; drawn here as a billboard in the room at RE1's size (the sprite's pixels times its
 * width times its light factor over 64, in RE1 units), with RE1's blend and tint.
 *
 * Left out for now: the weapon effects' hits (bullets, the rocket, the flamethrower: our weapons
 * have their own), the drops' hurting (the player's damage), effects hitting RE1's walls (no
 * ground probe: they stop at the floor, y 0), the camera-zone test, sounds, the mirror pass.
 */
#include "global.h"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define NV_RE1_SCALE 0.0217f
#define NV_FX_OPAQUE(c) (((c) >> 8) & 1) // (an RGBA5551 texel, big-endian in memory: its alpha bit)

extern PlayState* gPlayState;
s32 NvCamera_InView(PlayState* play, s32 x, s32 z);
#define NV_FX_SLOTS 64
#define NV_FX_TYPES 50

/* ---- RE1's math (PSX fixed point: 4096 = 1.0, angles 4096 = a turn) ---- */

typedef struct {
    s16 m[3][3];
    s16 pad;
    s32 t[3];
} NvMtx; // RE1's MATRIX (0x20 bytes: the rotation, then the translation)

static const NvMtx sNvFxIdentity = { { { 0x1000, 0, 0 }, { 0, 0x1000, 0 }, { 0, 0, 0x1000 } }, 0, { 0, 0, 0 } };
static NvMtx sNvFxPlayerMtx; // the player's (parent 1): Link where RE1's player would be
static NvMtx sNvFxEnemyMtx[16]; // the enemies' (parent 2 + slot): where each stands, turned as it faces
static NvMtx sNvFxObjMtx[64];   // the room objects' (parent 0x80 + object)

static s16 NvFx_Sin(s32 a) {
    return (s16)(Math_SinS((s16)(a * 16)) * 4096.0f);
}

static s16 NvFx_Cos(s32 a) {
    return (s16)(Math_CosS((s16)(a * 16)) * 4096.0f);
}

/** RotMatrixY: a turn of a about y. */
static void NvFx_RotY(s32 a, NvMtx* m) {
    s16 s = NvFx_Sin(a);
    s16 c = NvFx_Cos(a);

    *m = sNvFxIdentity;
    m->m[0][0] = c;
    m->m[0][2] = s;
    m->m[2][0] = -s;
    m->m[2][2] = c;
}

/** ApplyMatrixSV / ApplyMatrix: m times v. */
static void NvFx_Apply(const NvMtx* m, const s32* v, s32* out) {
    s32 i;

    for (i = 0; i < 3; i++) {
        out[i] = (m->m[i][0] * v[0] + m->m[i][1] * v[1] + m->m[i][2] * v[2]) >> 12;
    }
}

/* ---- the pool ---- */

typedef struct {
    // the phase header (24 bytes, copied from the data's blocks)
    u8 animId;
    u8 updateId;
    u8 type;
    u8 lightFactor;
    u8 animHeader[18];
    s16 yaw;
    // movement
    s16 rotSpeedX;
    s16 rotSpeedY;
    s16 rotSpeedZ;
    u8 frameDelay;
    u8 frameIndex;
    s16 posX;
    s16 posY;
    s16 posZ;
    u8 effectType;
    u8 depthGroup;
    s16 localOffsetX;
    s16 localOffsetY;
    s16 localOffsetZ;
    NvMtx xf; // its transform (RE1's transform[9] + spriteOffset: the parent's matrix it follows)
    s32 spawnPos[4];
    const NvMtx* parent; // spriteInfo: the matrix it's attached to
    const u8* info;      // its type's data (clutInfo)
    const u8* frames;    // vramInfo
    const u8* frame;     // vramInfoBackup: the frame showing (UV record, delay, width, height)
    const u8* uvs;       // uvData
    const u8* uv;        // uvDataBackup: that frame's UV record (u, v, 0x80 - pivot x, 0x80 - pivot y)
    const u8* animBase;  // animDataBase
    const u8* animFrame; // animDataFrame: the phase block now
    s32 drawn;           // (a position worked out: drawable)
} NvFx;

#define AH_S16(e, off) (*(s16*)&(e)->animHeader[(off)])
#define AH_U16(e, off) (*(u16*)&(e)->animHeader[(off)])

static NvFx sNvFx[NV_FX_SLOTS];
static u8 sNvFxFree = NV_FX_SLOTS; // g_freeEffectSlots
static u8 sNvFxCur;                // g_activeEffectIndex
static s32 sNvFxScratch[3];        // g_playerPosScratch
static s32 sNvFxDisplacement;      // g_playerDisplacement (a spawned slot, a few behaviours pass it on)

/* ---- the data ---- */

static const u8* sNvFxInfo[NV_FX_TYPES]; // g_effectSpriteInfo: a type's data, NULL when undeclared
static const u8* sNvFxAnim[NV_FX_TYPES]; // g_effectAnimData
static u8 sNvFxPage[NV_FX_TYPES];        // the page its art is on
static u8 sNvFxRoomArt[NV_FX_TYPES];     // the room's own (drawn untinted)
static const u8* sNvFxTables;            // RFXT
static u8 sNvFxRoomPages[4];             // the room's pages' Effspr files
static u16 sNvFxPages[4][256 * 256];     // the pages, as drawn
static s32 sNvFxReady;

static const u8* NvFx_Res(const char* path) {
    if (!ResourceMgr_FileExists(path)) {
        return NULL;
    }
    return (const u8*)ResourceGetDataByName(path);
}

/** A block of types (soh_effects._block): their data and pages into the tables. */
static const u8* NvFx_Block(const u8* p, s32 roomArt) {
    u8 n = *p++;
    const u8* types = p;
    const u8* data;
    u32 size;
    s32 i;

    p += n * 8;
    memcpy(&size, p, 4);
    data = p + 4;
    for (i = 0; i < n; i++) {
        u8 t = types[i * 8];
        u32 off;
        const u8* info;

        memcpy(&off, types + i * 8 + 4, 4);
        if ((t >= NV_FX_TYPES) || (off >= size)) {
            continue;
        }
        info = data + off;
        sNvFxInfo[t] = info;
        sNvFxAnim[t] = info + (info[2] + info[0]) * 4 + 8; // (load_effect_sprite_data)
        sNvFxPage[t] = types[i * 8 + 1];
        sNvFxRoomArt[t] = roomArt;
    }
    return data + size;
}

/** The room's effects (InitRoomEffSprite): the pool emptied, its types and pages set up. */
void NvFx_RoomLoad(const char* sceneFile) {
    char path[160];
    const u8* d;
    const u8* p;
    s32 i;
    s32 len = strlen(sceneFile);

    memset(sNvFx, 0, sizeof(sNvFx));
    sNvFxFree = NV_FX_SLOTS;
    memset(sNvFxInfo, 0, sizeof(sNvFxInfo));
    memset(sNvFxAnim, 0, sizeof(sNvFxAnim));
    sNvFxReady = false;
    sNvFxTables = NvFx_Res("re1/effects/tables");
    d = NvFx_Res("re1/effects/core");
    if ((sNvFxTables == NULL) || (memcmp(sNvFxTables, "RFXT", 4) != 0) || (d == NULL) ||
        (memcmp(d, "RFXC", 4) != 0)) {
        return;
    }
    NvFx_Block(d + 4, false);
    sNvFxRoomPages[0] = 0;
    sNvFxRoomPages[1] = sNvFxRoomPages[2] = sNvFxRoomPages[3] = 0xFF;
    p = NULL;
    if ((len > 6) && (strcmp(sceneFile + len - 6, "_scene") == 0)) {
        snprintf(path, sizeof(path), "scenes/shared/%s/%.*s_effects", sceneFile, len - 6, sceneFile);
        d = NvFx_Res(path);
        if ((d != NULL) && (memcmp(d, "RFXR", 4) == 0)) {
            memcpy(sNvFxRoomPages, d + 4, 4);
            p = NvFx_Block(d + 8, true);
        }
    }
    // the pages: each its Effspr file, then the room's sprites over them (load_effect_sprites)
    for (i = 0; i < 4; i++) {
        const u8* f;

        memset(sNvFxPages[i], 0, sizeof(sNvFxPages[i]));
        if (sNvFxRoomPages[i] == 0xFF) {
            continue;
        }
        snprintf(path, sizeof(path), "re1/effspr/%d", sNvFxRoomPages[i]);
        f = NvFx_Res(path);
        if ((f != NULL) && (memcmp(f, "RFXP", 4) == 0)) {
            memcpy(sNvFxPages[i], f + 4, sizeof(sNvFxPages[i]));
        }
    }
    if (p != NULL) {
        u8 n = *p++;

        for (i = 0; i < n; i++) {
            u8 page = p[0];
            u16 row;
            u16 w;
            u16 h;
            const u16* px;
            s32 x;
            s32 y;

            memcpy(&row, p + 2, 2);
            memcpy(&w, p + 4, 2);
            memcpy(&h, p + 6, 2);
            px = (const u16*)(p + 8);
            for (y = 0; (y < h) && (page < 4) && (row + y < 256); y++) {
                for (x = 0; (x < w) && (x < 256); x++) {
                    u16 c = px[y * w + x];

                    if (NV_FX_OPAQUE(c)) { // (its index 0 left as the page has it)
                        sNvFxPages[page][(row + y) * 256 + x] = c;
                    }
                }
            }
            p += 8 + w * h * 2;
        }
    }
    sNvFxReady = true;
    if (getenv("NV_LOG")) {
        printf("NVFX room pages %d %d %d %d; page 0 at (224+16, 176+16) %04X, (0,0) %04X\n", sNvFxRoomPages[0],
               sNvFxRoomPages[1], sNvFxRoomPages[2], sNvFxRoomPages[3], sNvFxPages[0][192 * 256 + 240],
               sNvFxPages[0][0]);
        fflush(stdout);
    }
}

/* ---- Effect_CreateBillboard ---- */

/**
 * Effect_CreateBillboard: type's animation for spawn group depthGroup (& 7 its table entry; >> 3
 * the tint), turned by yaw, attached to parent (NULL: the world), at pos, its light factor if
 * not 0. The slot of the last one placed (a group can place several), or 0xFF.
 */
u8 NvFx_Create(u8 type, u8 depthGroup, s16 yaw, const NvMtx* parent, const s32* pos, u8 lightFactor) {
    const u8* anim;
    const u32* table;
    u32 count;
    s32 k;

    if ((sNvFxFree == 0) || (type >= NV_FX_TYPES) || (sNvFxInfo[type] == NULL)) {
        if (getenv("NV_LOG")) {
            printf("NVFX type 0x%02X group %d: %s\n", type, depthGroup, (sNvFxFree == 0) ? "pool full" : "not declared");
            fflush(stdout);
        }
        return 0xFF;
    }
    if (getenv("NV_LOG")) {
        printf("NVFX type 0x%02X group %d at %d,%d,%d\n", type, depthGroup, (int)pos[0], (int)pos[1], (int)pos[2]);
        fflush(stdout);
    }
    anim = sNvFxAnim[type];
    table = (const u32*)(anim + anim[depthGroup & 7] * 4);
    count = table[0];
    for (k = (s32)count; k > 0; k--) {
        NvFx* e = NULL;
        const u32* blk = table + 1;
        s32 slot;
        s32 skip;

        for (slot = NV_FX_SLOTS - 1; slot >= 0; slot--) {
            if (sNvFx[slot].animId == 0) {
                e = &sNvFx[slot];
                sNvFxFree--;
                break;
            }
        }
        if (e == NULL) {
            break;
        }
        memset(e, 0, sizeof(*e));
        e->effectType = type;
        e->depthGroup = depthGroup;
        e->localOffsetX = (s16)pos[0];
        e->localOffsetY = (s16)pos[1];
        e->localOffsetZ = (s16)pos[2];
        e->parent = (parent != NULL) ? parent : &sNvFxIdentity;
        e->spawnPos[0] = pos[0];
        e->spawnPos[1] = pos[1];
        e->spawnPos[2] = pos[2];
        e->info = sNvFxInfo[type];
        e->frames = e->info + 8;
        e->frame = e->frames;
        e->uvs = e->info + 8 + (e->info[2] | (e->info[3] << 8)) * 4;
        e->uv = e->uvs;
        e->frameDelay = e->frames[1];
        e->frameIndex = e->frames[0];
        // the k-th of the group: past the chains before it (count, then that many 24-byte blocks)
        for (skip = k - 1; skip > 0; skip--) {
            blk += blk[0] * 6 + 1;
        }
        e->animBase = e->animFrame = (const u8*)(blk + 1);
        memcpy(&e->animId, e->animFrame, 24);
        if (lightFactor != 0) {
            e->lightFactor = lightFactor;
        }
        e->yaw += yaw;
        e->type = 1;
        if (k == 1) {
            return (u8)slot;
        }
    }
    return 0xFF;
}

/** FUN_0047cf80: every effect matching the masked criteria freed (1 type, 2 group, 4 header byte 2,
 * 8 parent). */
static void NvFx_ClearWhere(s32 mask, u8 type, u8 group, u8 b2, const NvMtx* parent) {
    s32 i;

    for (i = NV_FX_SLOTS - 1; i >= 0; i--) {
        NvFx* e = &sNvFx[i];
        s32 m = 0;

        if (e->animId == 0) {
            continue;
        }
        if ((mask & 1) && (e->effectType == type)) {
            m |= 1;
        }
        if ((mask & 2) && (e->depthGroup == group)) {
            m |= 2;
        }
        if ((mask & 4) && (e->animHeader[2] == b2)) {
            m |= 4;
        }
        if ((mask & 8) && (e->parent == parent)) {
            m |= 8;
        }
        if (m == mask) {
            sNvFxFree++;
            e->updateId = 0;
            e->animId = 0;
        }
    }
}

/* ---- the behaviours (EffectSystem.cpp's, by number) ---- */

#define CUR (&sNvFx[sNvFxCur])
#define RND() ((u32)(Rand_Next() >> 16))

static s32 NvFx_PlayerY(void) { // g_playerEntity.posY: Link's height in RE1's units
    Player* p = GET_PLAYER(gPlayState);

    return (p != NULL) ? (s32)(-p->actor.world.pos.y / NV_RE1_SCALE) : 0;
}

static void fx_next_phase(void) {
    NvFx* e = CUR;
    u8 lf = e->lightFactor;
    s16 yaw = e->yaw;

    e->animFrame += 0x18;
    memcpy(&e->animId, e->animFrame, 24);
    e->yaw = yaw;
    e->lightFactor = lf;
}

static void fx_set_frame(void) {
    NvFx* e = CUR;

    e->frame = e->frames + e->animHeader[0] * 4;
    e->frameDelay = e->frame[1];
    e->uv = e->uvs + e->frame[0] * 4;
}

static void fx_freeze(NvFx* e) { // memcpy(transform, spriteInfo): it stops following its parent
    e->xf = *e->parent;
}

static void fx_spawn_here(u8 type, u8 group, s16 yaw, u8 lf) {
    NvFx* e = CUR;

    sNvFxScratch[0] = e->posX;
    sNvFxScratch[1] = e->posY;
    sNvFxScratch[2] = e->posZ;
    sNvFxDisplacement = (s8)NvFx_Create(type, group, yaw, NULL, sNvFxScratch, lf);
}

static void fx_kill(void) {
    NvFx* e = CUR;

    sNvFxFree++;
    e->updateId = 0;
    e->animId = 0;
}

static void fx_ground_splat(void) {
    NvFx* e = CUR;

    e->frame = e->frames + 0x14;
    e->frameIndex = e->frame[0];
    e->frameDelay = e->frame[1];
    e->uv = e->uvs + e->frameIndex * 4;
    e->animId = 1;
    e->updateId = 1;
    e->animHeader[6] = e->animHeader[7] = 0;
    AH_S16(e, 4) = AH_S16(e, 6);
    AH_S16(e, 0xE) = AH_S16(e, 4);
    AH_S16(e, 0xC) = AH_S16(e, 0xE);
    if (e->animHeader[1] != 0) {
        fx_spawn_here(9, 1, 0, e->animHeader[3]);
    }
}

static void fx_idle(void) {
}

static void fx_timer_refresh(void) {
    NvFx* e = CUR;

    if (e->animHeader[3] == 0) {
        fx_next_phase();
        fx_freeze(e);
        return;
    }
    e->animHeader[3]--;
}

static void fx_hold_frame(void) {
    CUR->frameDelay = 0;
}

static void fx_ember(void) {
    NvFx* e = CUR;

    if (e->animHeader[3] == 0) {
        fx_next_phase();
        e->lightFactor = 0x1E;
        e->localOffsetX -= 0xFA;
        return;
    }
    e->animHeader[3]--;
}

static void fx_timer_phase(void) {
    NvFx* e = CUR;

    if (e->animHeader[3] == 0) {
        fx_next_phase();
        return;
    }
    e->animHeader[3]--;
}

static void fx_floor_reset(NvFx* e) { // (the velocities and accelerations stopped, on the floor)
    e->animHeader[8] = e->animHeader[9] = 0;
    AH_S16(e, 6) = AH_S16(e, 8);
    AH_S16(e, 4) = AH_S16(e, 6);
    e->animHeader[0x10] = e->animHeader[0x11] = 0;
    AH_S16(e, 0xE) = AH_S16(e, 0x10);
    AH_S16(e, 0xC) = AH_S16(e, 0xE);
    e->rotSpeedY = (s16)(-(e->xf.t[1] + e->localOffsetY));
}

static void fx_bounce(void) {
    NvFx* e = CUR;

    if (e->posY + AH_S16(e, 0xE) >= 0) {
        e->animId = 1;
        e->updateId = 1;
        AH_U16(e, 10) |= 0x4007;
        fx_floor_reset(e);
    }
}

static void fx_gravity_rise(void) {
    NvFx* e = CUR;

    e->animHeader[10] = 3;
    e->animHeader[11] = 0x40;
    if (NvFx_PlayerY() < e->posY + AH_S16(e, 0xE)) {
        AH_U16(e, 10) |= 0x400B;
        e->animId = 0x2F;
        e->updateId = 0;
        e->rotSpeedY = (s16)((NvFx_PlayerY() - e->xf.t[1]) - e->localOffsetY);
    }
}

static void fx_projectile(void) {
    NvFx* e = CUR;

    if (NvFx_PlayerY() < e->posY + AH_S16(e, 0xE)) {
        u32 r = RND() % MAX(1, e->animHeader[1]);

        AH_U16(e, 10) |= 0x400B;
        e->animFrame += 0x18 + r * 0x18;
        e->updateId = e->animFrame[1];
        AH_S16(e, 0xC) = *(const s16*)(e->animFrame + 0x10);
        AH_S16(e, 0xE) = *(const s16*)(e->animFrame + 0x12);
        AH_S16(e, 0x10) = (s16)(r * 10 + *(const s16*)(e->animFrame + 0x14));
        e->yaw += *(const s16*)(e->animFrame + 0x16);
        e->rotSpeedY = (s16)((NvFx_PlayerY() - e->xf.t[1]) - e->localOffsetY);
        if (e->animHeader[2] != 0) {
            AH_S16(e, 0xC) = -AH_S16(e, 0xC);
        }
    }
}

static void fx_rand_phase(void) {
    NvFx* e = CUR;
    u8 phase;
    s16 yaw;
    u8 lf;

    fx_set_frame();
    e->animFrame += 0x18;
    phase = e->animHeader[3];
    yaw = e->yaw;
    lf = e->lightFactor;
    if ((RND() & 1) == 0) {
        memcpy(&e->animId, e->animFrame, 24);
        e->animFrame += 0x18;
    } else {
        e->animFrame += 0x18;
        memcpy(&e->animId, e->animFrame, 24);
    }
    e->yaw = (s16)(e->yaw + yaw);
    e->animHeader[3] = phase;
    e->lightFactor = lf;
    fx_freeze(e);
}

static void fx_spawner(void) {
    NvFx* e = CUR;

    if (e->animHeader[2] == 0) {
        sNvFxScratch[0] = e->localOffsetX;
        sNvFxScratch[1] = e->localOffsetY;
        sNvFxScratch[2] = e->localOffsetZ;
        NvFx_Create(e->effectType, e->depthGroup, e->yaw, e->parent, sNvFxScratch, 0);
        fx_next_phase();
        return;
    }
    e->animHeader[2]--;
}

static void fx_count_phase(void) {
    NvFx* e = CUR;

    if (e->animHeader[2] == 0) {
        fx_next_phase();
        return;
    }
    e->animHeader[2]--;
}

static void fx_blink(void) {
    CUR->animHeader[11] ^= 0x80;
}

static void fx_spawn_from_header(void);

static void fx_floor_splash(void) {
    NvFx* e = CUR;

    if (e->posY + AH_S16(e, 0xE) >= 0) {
        e->animId = 0x2F;
        e->updateId = 0;
        e->rotSpeedY = (s16)(-(e->xf.t[1] + e->localOffsetY));
        fx_spawn_from_header();
        if ((sNvFxDisplacement >= 0) && (sNvFxDisplacement < NV_FX_SLOTS)) {
            sNvFx[sNvFxDisplacement].lightFactor += (u8)(e->lightFactor - 3);
        }
    }
}

static void fx_wobble_common(s32 spread, s32 lift) {
    NvFx* e = CUR;

    if (e->animHeader[3] != 0) {
        u32 r = RND();

        e->animHeader[3]--;
        AH_U16(e, 10) |= (r & 1) ? 0x80 : 0;
        AH_S16(e, 0xC) = (s16)((s16)(r % 6) * -spread + spread * 12 / 5) * (s16)(s8)e->animHeader[2];
        AH_S16(e, 6) = (s16)(-lift - (s16)(r % 6));
        e->lightFactor -= (r & 1) ? 1 : 0;
        e->yaw = (s16)(((r % 6) * 0x3C000) / 0x168);
        e->animHeader[0] = (u8)(r % 6);
        fx_set_frame();
        return;
    }
    e->animId = 0x11;
    e->animHeader[3] = 0;
    e->type = 2;
    fx_freeze(e);
}

static void fx_wobble(void) { // (0x32 a step: 0x78 - 0x32 * r)
    fx_wobble_common(0x32, 8);
}

static void fx_bounce_debris_common(s32 kick, s32 down) {
    NvFx* e = CUR;

    if (AH_S16(e, 0xE) < (s16)(s8)e->animHeader[3]) {
        u32 r = RND();

        AH_U16(e, 0xC) = (u16)((kick - (r & 3)) * (s16)(s8)e->animHeader[2]);
        AH_U16(e, 0xE) = (u16)((0x23 - (r & 3)) * 2);
        e->animHeader[3] = (u8)(AH_S16(e, 0xE) / -3);
        AH_S16(e, 0x10) = (s16)(AH_S16(e, 0x10) + (s16)(r % 5) * (s16)(s8)e->animHeader[2]);
        AH_S16(e, 4) = (s16)((s16)(r % 3) * (s16)(s8)e->animHeader[2]);
        AH_U16(e, 6) = (u16)(-down - (r & 1));
        e->animHeader[2] = (u8)(-e->animHeader[2]);
    }
}

static void fx_bounce_debris(void) {
    fx_bounce_debris_common(0x1E, 2);
}

static void fx_clone(void) {
    NvFx* e = CUR;
    s32 slot;

    if (e->animHeader[0] != 0) {
        e->animHeader[0]--;
        return;
    }
    e->animId = 1;
    if (sNvFxFree == 0) {
        return;
    }
    for (slot = NV_FX_SLOTS - 1; slot >= 0; slot--) {
        if (sNvFx[slot].animId == 0) {
            NvFx* d = &sNvFx[slot];

            sNvFxFree--;
            memset(d, 0, sizeof(*d));
            d->effectType = e->effectType;
            d->depthGroup = e->depthGroup;
            d->parent = e->parent;
            memcpy(d->spawnPos, e->spawnPos, sizeof(d->spawnPos));
            d->info = e->info;
            d->frames = d->frame = e->frames;
            d->uvs = d->uv = e->uvs;
            d->localOffsetX = (s16)e->spawnPos[0];
            d->localOffsetY = (s16)e->spawnPos[1];
            d->localOffsetZ = (s16)e->spawnPos[2];
            d->animBase = e->animBase;
            d->animFrame = e->animFrame + 0x18;
            memcpy(&d->animId, d->animFrame, 24);
            break;
        }
    }
}

static void fx_fall_stop(void) {
    NvFx* e = CUR;

    if (e->posY + AH_S16(e, 0xE) >= 0) {
        e->animId = 0x2F;
        e->animHeader[10] = 7;
        e->animHeader[11] = 0x40;
        fx_floor_reset(e);
    }
}

static void fx_phase_drop(void) {
    fx_next_phase();
    AH_S16(CUR, 0xE) -= 0x18;
}

static void fx_drop_splat(void) { // (RE1 also splats on its walls and hurts the player under it)
    NvFx* e = CUR;

    if (e->posY + AH_S16(e, 0xE) >= 0) {
        fx_ground_splat();
    }
}

static void fx_fade_fall(void) {
    NvFx* e = CUR;

    if (e->animHeader[0] == 0) {
        e->animId = 0x16;
        e->animHeader[0xE] = e->animHeader[0xF] = 0;
        AH_S16(e, 6) = (s16)e->animHeader[2];
    }
    fx_drop_splat();
}

static void fx_ground_splat_b(void) {
    NvFx* e = CUR;

    if (e->posY + AH_S16(e, 0xE) >= 0) {
        fx_ground_splat();
        AH_U16(e, 10) |= 0x4007;
    }
}

static void fx_jitter(void) {
    NvFx* e = CUR;
    u8 f = e->animHeader[3];

    if (f & 1) {
        AH_S16(e, 0xC) += (s16)(RND() % 5);
    }
    if (f & 2) {
        AH_S16(e, 0xE) += (s16)(RND() % 5);
    }
    if (f & 4) {
        AH_S16(e, 0x10) += (s16)(RND() % 5);
    }
    if (f & 8) {
        AH_S16(e, 4) += (s16)(RND() % 5);
    }
    if (f & 0x10) {
        AH_S16(e, 6) += (s16)(RND() % 5);
    }
    if (f & 0x20) {
        AH_S16(e, 8) += (s16)(RND() % 5);
    }
}

static void fx_drift(void) {
    NvFx* e = CUR;

    e->localOffsetX += (s8)e->animHeader[0] * 2;
    e->localOffsetY += (s8)e->animHeader[1] * 2;
    e->localOffsetZ += (s8)e->animHeader[2] * 2;
    fx_next_phase();
}

static void fx_spawn_from_header(void) {
    NvFx* e = CUR;

    fx_spawn_here(e->animHeader[0], e->animHeader[1], e->yaw, 0);
}

static void fx_next_frame(void) {
    CUR->frame += 4;
    CUR->frameDelay = 0;
}

static void fx_rand_frame(void) {
    NvFx* e = CUR;

    e->frame += (RND() % 3) * 4;
    e->frameDelay = e->frame[1];
    e->uv = e->uvs + e->frame[0] * 4;
}

static void fx_spawn_advance(void) {
    NvFx* e = CUR;

    fx_spawn_here(e->animHeader[0], e->animHeader[1], e->yaw, 0);
    fx_next_phase();
}

static void fx_floor_phase(void) {
    NvFx* e = CUR;

    if (e->posY + AH_S16(e, 0xE) >= 0) {
        fx_floor_reset(e);
        fx_set_frame();
        fx_next_phase();
    }
}

static void fx_frame_phase(void) {
    fx_set_frame();
    fx_next_phase();
}

static void fx_fire_wobble(void) { // (0x19 a step: 0x3C - 0x19 * r)
    fx_wobble_common(0x19, 10);
}

static void fx_fire_bounce(void) {
    fx_bounce_debris_common(10, 10);
}

static void fx_set_tint(void) {
    static const u8 sTint[10] = { 7, 0xD, 8, 8, 0xF, 0xF, 0xF, 0x11, 0x11, 0x11 };
    NvFx* e = CUR;

    e->updateId = 1;
    e->type = 2;
    fx_freeze(e);
    e->lightFactor = sTint[MIN(e->animHeader[0], 9)];
}

static void fx_pair_timer(void) {
    NvFx* e = CUR;

    e->animHeader[2]--;
    e->animHeader[3]--;
    if (e->animHeader[3] == 0) {
        fx_kill();
        return;
    }
    if (e->animHeader[2] == 0) {
        e->animHeader[2] = 4;
        NvFx_Create(e->animHeader[0], e->animHeader[1], e->yaw, e->parent, e->spawnPos, e->lightFactor);
    }
}

static void fx_timer_flags(void) {
    fx_timer_refresh();
    CUR->animHeader[2] = 0;
}

static void fx_rand_anim(void) {
    NvFx* e = CUR;

    AH_U16(e, 10) |= (u16)(RND() % 3) << 6;
    e->animId = e->animHeader[2];
}

static void fx_countdown_kill(void) {
    NvFx* e = CUR;

    if (e->animHeader[2] == 0) {
        fx_kill();
        return;
    }
    e->animHeader[2]--;
}

static void fx_fade(void) {
    NvFx* e = CUR;

    e->frameDelay = 0;
    if (e->lightFactor != 0) {
        e->lightFactor -= 2;
    }
}

static void fx_flash_marker(void) {
    NvFx* e = CUR;

    e->animHeader[10] = 6;
    e->animHeader[11] = 0x80;
    if (AH_S16(e, 10) != 6) {
        e->animHeader[10] = 6;
        e->animHeader[11] = 0;
    }
    if (e->animHeader[1] == 0) {
        fx_kill();
        return;
    }
    e->animHeader[1]--;
}

static void fx_floor_stop(void) {
    NvFx* e = CUR;

    if (e->posY + AH_S16(e, 0xE) >= 0) {
        e->animId = 0x2F;
        e->updateId = 0;
        e->animHeader[10] = 6;
        e->animHeader[11] = 0;
        fx_floor_reset(e);
    }
}

static void fx_sysflag(void) {
    s32 NvFlags_Test(s32 bank, s32 bit);

    if (NvFlags_Test(4, 0)) {
        CUR->animId = 1;
        fx_set_frame();
    }
}

static void fx_to_settle(void) {
    NvFx* e = CUR;

    fx_freeze(e);
    e->type = 2;
    e->animId = 5; // (gravity_settle: RE1 then probes its walls; here it stays where it is)
}

static void fx_floor_stop1(void) {
    NvFx* e = CUR;

    if (e->posY + AH_S16(e, 0xE) >= 0) {
        e->animId = 1;
        e->updateId = 0;
        fx_floor_reset(e);
    }
}

static void fx_random_spin(void) {
    NvFx* e = CUR;

    e->rotSpeedX = (s16)(((RND() & 1) ? 5 : 15) * ((RND() & 1) ? 1 : -1));
    e->rotSpeedY = (s16)(((RND() & 1) ? 5 : 15) * ((RND() & 1) ? 1 : -1));
    e->rotSpeedZ = (s16)(((RND() & 1) ? 5 : 15) * ((RND() & 1) ? 1 : -1));
}

static void fx_splash_timer(void) {
    NvFx* e = CUR;
    s32 y;

    e->animHeader[0]--;
    if (e->animHeader[0] == 0) {
        e->animHeader[0] = 4;
        e->frame = e->frames + 0x1C;
        e->frameIndex = e->frame[0];
        e->frameDelay = e->frame[1];
        e->uv = e->uvs + e->frameIndex * 4;
    }
    y = AH_S16(e, 0xE) + e->posY;
    if ((y > -0x961) && (y > -1)) {
        fx_ground_splat();
        AH_U16(e, 10) |= 0x4007;
    }
}

static void fx_fire_phases(void) {
    NvFx* e = CUR;

    switch (e->animHeader[0]) {
        case 0:
            e->frame = e->frames + 0x14;
            e->frameDelay = e->frame[1];
            e->uv = e->uvs + e->frame[0] * 4;
            e->animHeader[1] = 0xB;
            e->animHeader[0] = 1;
            // fall through
        case 1:
            e->animHeader[10] = 3;
            e->animHeader[11] = 0x40;
            e->lightFactor = e->animHeader[2];
            if (--e->animHeader[1] == 0) {
                e->animHeader[0] = 2;
                e->animHeader[10] = e->animHeader[11] = 0;
                e->lightFactor = 0;
            }
            break;
        case 2:
            e->frame = e->frames + 0x14;
            e->frameDelay = e->frame[1];
            e->uv = e->uvs + e->frame[0] * 4;
            e->animHeader[1] = (u8)(((s8)(RND() % 5) + 10) * 10);
            e->animHeader[0] = 3;
            e->animHeader[10] = e->animHeader[11] = 0;
            e->lightFactor = 0;
            break;
        case 3:
            e->animHeader[10] = e->animHeader[11] = 0;
            e->lightFactor = 0;
            if (--e->animHeader[1] == 0) {
                e->animHeader[0] = 0;
                e->animHeader[1] = 0xB;
            }
            break;
    }
}

static void fx_unported(void) { // (a weapon's hit tests: bullets, the rocket, the flamethrower, the
                                   // burning's wall probe, the muzzle, the charge - nothing here)
}

static void fx_fire_burst(void) { // (44: a burst of fire spawned a group at a time)
    NvFx* e = CUR;

    sNvFxScratch[0] = e->localOffsetX;
    sNvFxScratch[1] = e->localOffsetY;
    sNvFxScratch[2] = e->localOffsetZ;
    switch (e->animHeader[0]) {
        case 0:
            NvFx_Create(1, e->depthGroup + 1, e->yaw, e->parent, sNvFxScratch, 0);
            e->animHeader[0] = 1;
            break;
        case 1:
            NvFx_Create(1, e->depthGroup + 2, e->yaw, e->parent, sNvFxScratch, 0);
            NvFx_Create(1, e->depthGroup + 3, e->yaw, e->parent, sNvFxScratch, 0);
            e->animHeader[0] = 2;
            break;
        case 2:
            NvFx_Create(1, e->depthGroup + 4, e->yaw, e->parent, sNvFxScratch, 0);
            fx_kill();
            break;
    }
}

static void (*const sNvFxBehaviour[64])(void) = {
    fx_idle,           fx_idle,           fx_timer_refresh, fx_hold_frame,    fx_ember,
    fx_idle,           fx_timer_phase,    fx_bounce,        fx_idle,          fx_gravity_rise,
    fx_projectile,     fx_rand_phase,     fx_spawner,       fx_count_phase,   fx_blink,
    fx_floor_splash,   fx_wobble,         fx_bounce_debris, fx_clone,         fx_fall_stop,
    fx_phase_drop,     fx_fade_fall,      fx_drop_splat,    fx_ground_splat_b, fx_jitter,
    fx_drift,          fx_spawn_from_header, fx_next_frame, fx_next_phase,    fx_rand_frame,
    fx_spawn_advance,  fx_set_frame,      fx_floor_phase,   fx_frame_phase,   fx_unported,
    fx_fire_wobble,    fx_fire_bounce,    fx_unported,      fx_set_tint,      fx_pair_timer,
    fx_timer_flags,    fx_rand_anim,      fx_unported,      fx_unported,      fx_fire_burst,
    fx_unported,       fx_countdown_kill, fx_kill,          fx_fade,          fx_flash_marker,
    fx_floor_stop,     fx_sysflag,        fx_to_settle,     fx_floor_stop1,   fx_random_spin,
    fx_splash_timer,   fx_fire_phases,    fx_unported,      fx_idle,          fx_unported,
    fx_idle,           fx_idle,           fx_idle,          fx_idle,
};
// (5 gravity_settle and 8 gravity_impact wait on RE1's walls: idle here)

/** Effect_AnimateSprite: the frame's delay counted down; the next frame (0xFF: loop back), or the end. */
static void NvFx_Animate(NvFx* e) {
    if (e->frameDelay == 0) {
        const u8* next = e->frame + 4;

        e->frameIndex++;
        if ((next[0] == 0) && (next[1] == 0)) {
            if ((e->animId != 0) || (e->updateId != 0)) {
                sNvFxFree++;
                e->updateId = 0;
                e->animId = 0;
            }
            return;
        }
        if (next[1] == 0xFF) {
            next = e->frames + next[0] * 4;
            e->frameIndex = next[0];
        }
        e->frameDelay = next[1];
        e->uv = e->uvs + next[0] * 4;
        e->frame = next;
    }
    e->frameDelay--;
}

/** EffectActor_UpdateAndRender's update half, for slot sNvFxCur. */
static void NvFx_Step(void) {
    NvFx* e = CUR;

    sNvFxBehaviour[e->animId & 63]();
    if (e->animId == 0) {
        return;
    }
    if (e->animHeader[10] & 2) { // moving: its velocity through its yaw and its parent's matrix
        NvMtx rot;
        s32 v[3] = { e->rotSpeedX, e->rotSpeedY, e->rotSpeedZ };
        s32 sv[3];
        s32 w[3];

        NvFx_RotY(e->yaw, &rot);
        NvFx_Apply(&rot, v, sv);
        sv[0] += e->localOffsetX;
        sv[1] += e->localOffsetY;
        sv[2] += e->localOffsetZ;
        if (e->type == 1) {
            e->xf = *e->parent;
        }
        NvFx_Apply(&e->xf, sv, w);
        e->posX = (s16)(e->xf.t[0] + w[0]);
        e->posY = (s16)(e->xf.t[1] + w[1]);
        e->posZ = (s16)(e->xf.t[2] + w[2]);
        if (e->animHeader[10] & 4) {
            e->posY = 0;
        }
        if (e->animHeader[10] & 8) {
            e->posY = (s16)NvFx_PlayerY();
        }
        e->drawn = true;
        AH_S16(e, 0xC) += AH_S16(e, 4);
        AH_S16(e, 0xE) += AH_S16(e, 6);
        AH_S16(e, 0x10) += AH_S16(e, 8);
        e->rotSpeedX += AH_S16(e, 0xC);
        e->rotSpeedY += AH_S16(e, 0xE);
        e->rotSpeedZ += AH_S16(e, 0x10);
    }
    sNvFxBehaviour[e->updateId & 63]();
    if ((e->animId != 0) && (e->animHeader[10] & 1)) {
        NvFx_Animate(e);
    }
}

/** The player's matrix (parent 1): Link where RE1's player stands, turned as he faces. */
static void NvFx_PlayerMatrix(PlayState* play) {
    Player* p = GET_PLAYER(play);

    if (p == NULL) {
        return;
    }
    // (OoT yaw 0 faces +z, RE1's 0 faces +x with z flipped: RE1 angle = (0x4000 - yaw) / 16)
    NvFx_RotY(((0x4000 - p->actor.shape.rot.y) & 0xFFFF) >> 4, &sNvFxPlayerMtx);
    sNvFxPlayerMtx.t[0] = (s32)(p->actor.world.pos.x / NV_RE1_SCALE);
    sNvFxPlayerMtx.t[1] = (s32)(-p->actor.world.pos.y / NV_RE1_SCALE);
    sNvFxPlayerMtx.t[2] = (s32)(-p->actor.world.pos.z / NV_RE1_SCALE);
}

/** The enemies' and objects' matrices, for the effects attached to them (they follow as they move). */
static void NvFx_ParentMatrices(void) {
    s32 NvEnemy_Place(s32 slot, s32* t, s16* angle);
    s32 NvObj_Pose(s32 i, s16* pos, s16* rot);
    s32 i;

    for (i = 0; i < 16; i++) {
        s32 t[3];
        s16 a;

        if (NvEnemy_Place(i, t, &a)) {
            NvFx_RotY(a, &sNvFxEnemyMtx[i]);
            sNvFxEnemyMtx[i].t[0] = t[0];
            sNvFxEnemyMtx[i].t[1] = t[1];
            sNvFxEnemyMtx[i].t[2] = t[2];
        }
    }
    for (i = 0; i < 64; i++) {
        s16 pos[3];
        s16 rot[3];

        if (NvObj_Pose(i, pos, rot)) {
            NvFx_RotY(rot[1], &sNvFxObjMtx[i]);
            sNvFxObjMtx[i].t[0] = pos[0];
            sNvFxObjMtx[i].t[1] = pos[1];
            sNvFxObjMtx[i].t[2] = pos[2];
        }
    }
}

/** A tick of update_2d_effects (z_nv_script.c's tick loop). */
void NvFx_Tick(PlayState* play) {
    s32 i;

    if (!sNvFxReady) {
        return;
    }
    NvFx_PlayerMatrix(play);
    NvFx_ParentMatrices();
    if ((getenv("NV_FX_TEST") != NULL) && ((play->gameplayFrames % 60) == 30)) {
        // dev: NV_FX_TEST="type,group" spawns one a little in front of Link every few seconds
        Player* pl = GET_PLAYER(play);
        int t;
        int g;

        if ((pl != NULL) && (sscanf(getenv("NV_FX_TEST"), "%i,%i", &t, &g) == 2)) {
            s32 at[3];

            at[0] = (s32)((pl->actor.world.pos.x + Math_SinS(pl->actor.shape.rot.y) * 40.0f) / NV_RE1_SCALE);
            at[1] = (s32)(-(pl->actor.world.pos.y + 30.0f) / NV_RE1_SCALE);
            at[2] = (s32)(-(pl->actor.world.pos.z + Math_CosS(pl->actor.shape.rot.y) * 40.0f) / NV_RE1_SCALE);
            NvFx_Create((u8)t, (u8)g, 0, NULL, at, 0);
        }
    }
    for (i = NV_FX_SLOTS - 1; i >= 0; i--) {
        if (sNvFx[i].animId != 0) {
            NvFx* e = &sNvFx[i];

            sNvFxCur = (u8)i;
            NvFx_Step();
            if (getenv("NV_LOG") && ((play->gameplayFrames % 40) == 0)) {
                printf("NVFXTICK %d type %02X anim %d/%d hdr10 %02X hdr11 %02X lf %d drawn %d pos %d,%d,%d\n", i,
                       e->effectType, e->animId, e->updateId, e->animHeader[10], e->animHeader[11], e->lightFactor,
                       e->drawn, e->posX, e->posY, e->posZ);
                fflush(stdout);
            }
        }
    }
}

/* ---- the script's commands ---- */

/** cmd_effect_spawn (0x2A) / cmd_bullet_effect_spawn (0x3D): p is the command. */
void NvFx_ScdSpawn(const u8* p) {
    s32 pos[3];
    s16 v[3];
    u16 flags;
    const NvMtx* parent = NULL;
    u8 ptype = p[3];

    if (p[0] == 0x3D) { // (its positions read unsigned)
        u16 uv[3];

        memcpy(uv, p + 4, 6);
        pos[0] = uv[0];
        pos[1] = uv[1];
        pos[2] = uv[2];
    } else {
        memcpy(v, p + 4, 6);
        pos[0] = v[0];
        pos[1] = v[1];
        pos[2] = v[2];
    }
    memcpy(&flags, p + 10, 2);
    if (ptype == 1) {
        parent = &sNvFxPlayerMtx;
    } else if ((ptype >= 2) && !(ptype & 0x80) && (ptype - 2 < 16)) { // an enemy's
        parent = &sNvFxEnemyMtx[ptype - 2];
    } else if (ptype & 0x80) { // a room object's
        parent = &sNvFxObjMtx[ptype & 0x3F];
    }
    NvFx_ParentMatrices(); // (where they are now)
    NvFx_Create(p[1], p[2], (s16)flags, parent, pos, 0);
}

/** An effect spawned in the world (the items' glints). */
u8 NvFx_Spawn(u8 type, u8 group, const s32* pos) {
    return NvFx_Create(type, group, 0, NULL, pos, 0);
}

/** An effect's slot freed (an item's glint when it's taken); 0xFF nothing. */
void NvFx_Kill(u8 slot) {
    if ((slot < NV_FX_SLOTS) && (sNvFx[slot].animId != 0)) {
        sNvFxFree++;
        sNvFx[slot].animId = 0;
        sNvFx[slot].updateId = 0;
    }
}

/** cmd_effect_clear_typed (0x42): the type / group pair's freed. */
void NvFx_ScdClearTyped(u8 type, u8 group) {
    NvFx_ClearWhere(3, type, group, 0, NULL);
}

/** cmd_bullet_effect_clear (0x3E): the last 0x3D's type's freed (RE1 matches its parent too). */
void NvFx_ScdClearType(u8 type) {
    NvFx_ClearWhere(1, type, 0, 0, NULL);
}

/** cmd_effect_pool_clear (0x48): all of them. */
void NvFx_ScdClearAll(void) {
    memset(sNvFx, 0, sizeof(sNvFx));
    sNvFxFree = NV_FX_SLOTS;
}

/* ---- drawing ---- */

// each frame's pixels cut out of its page once (as z_nv_terminal.c does: a block of its own)
#define NV_FX_CUTS 160
#define NV_FX_POOL (192 * 1024)
typedef struct {
    u8 page;
    u8 u;
    u8 v;
    u8 w;
    u8 h;
    u16* px;
} NvFxCut;
static NvFxCut sNvFxCuts[NV_FX_CUTS];
static s32 sNvFxCutCount;
static u16 sNvFxPool[NV_FX_POOL / 2];
static u32 sNvFxPoolUsed;
static const u16* sNvFxCutFor; // (the pages the cuts were made from: a new room, new cuts)

static const u16* NvFx_Cut(s32 page, s32 u, s32 v, s32 w, s32 h) {
    NvFxCut* c;
    s32 i;
    s32 y;

    if (sNvFxCutFor != sNvFxPages[0] + sNvFxRoomPages[1] * 7 + sNvFxRoomPages[2] * 131) {
        sNvFxCutCount = 0;
        sNvFxPoolUsed = 0;
        sNvFxCutFor = sNvFxPages[0] + sNvFxRoomPages[1] * 7 + sNvFxRoomPages[2] * 131;
    }
    for (i = 0; i < sNvFxCutCount; i++) {
        c = &sNvFxCuts[i];
        if ((c->page == page) && (c->u == u) && (c->v == v) && (c->w == w) && (c->h == h)) {
            return c->px;
        }
    }
    if ((sNvFxCutCount >= NV_FX_CUTS) || (sNvFxPoolUsed + (u32)(w * h) > ARRAY_COUNT(sNvFxPool))) {
        sNvFxCutCount = 0; // (full: start again)
        sNvFxPoolUsed = 0;
    }
    c = &sNvFxCuts[sNvFxCutCount++];
    c->page = page;
    c->u = u;
    c->v = v;
    c->w = w;
    c->h = h;
    c->px = &sNvFxPool[sNvFxPoolUsed];
    sNvFxPoolUsed += w * h;
    for (y = 0; y < h; y++) {
        s32 x;

        for (x = 0; x < w; x++) {
            c->px[y * w + x] = sNvFxPages[page][((v + y) & 0xFF) * 256 + ((u + x) & 0xFF)];
        }
    }
    return c->px;
}

/** effect_submit_sprite's look-ups: its blend (0 opaque, 1 half, 2 added, 3 a quarter added) and colour. */
static void NvFx_Look(NvFx* e, s32* blend, u8* rgb) {
    const u8* t = sNvFxTables + 4 + 896;
    u8 rec;
    s32 i;
    s32 n;
    s32 band = 0;
    u8 colour = 0;
    u16 attr = AH_U16(e, 10);
    s32 tint = e->depthGroup >> 3;
    u8 page = sNvFxPage[e->effectType];

    rgb[0] = rgb[1] = rgb[2] = 255;
    *blend = (attr & 0x4000) ? (((attr >> 12) & 3) == 1 ? 2 : ((attr >> 12) & 3) == 3 ? 3 : 1) : 0;
    rec = (page < 4) ? sNvFxRoomPages[page] : 0xFF;
    if (rec >= 32) {
        return;
    }
    for (i = 0; i < rec; i++) { // (the bands: u8 count, rows of 4)
        t += 1 + t[0] * 4;
    }
    n = t[0];
    for (i = 0; i < n; i++) {
        const u8* row = t + 1 + i * 4;

        if (e->uv[1] < row[0] + row[1]) {
            band = i;
            break;
        }
    }
    if (n > 0) {
        colour = t[1 + band * 4 + 3];
    }
    if (sNvFxRoomArt[e->effectType]) { // (the room's own art: untinted)
        return;
    }
    t = sNvFxTables + 4 + 896;
    for (i = 0; i < 32; i++) {
        t += 1 + t[0] * 4;
    }
    if (colour >= t[0]) {
        return;
    }
    t++;
    for (i = 0; i < colour; i++) {
        t += 1 + t[0] * 3;
    }
    if (tint >= t[0]) {
        tint = 0;
    }
    if (t[0] > 0) {
        memcpy(rgb, t + 1 + tint * 3, 3);
    }
}

/** Every effect a billboard in the room (z_play.c, after the actors). */
void NvFx_Draw(PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    s32 i;

    MtxF sBill; // facing the view: its right, its up (from the view's eye and target, the rooms' fixed cameras)

    if (!sNvFxReady || (play->sceneNum < SCENE_RE1_100)) {
        return;
    }
    {
        Vec3f f = { play->view.lookAt.x - play->view.eye.x, play->view.lookAt.y - play->view.eye.y,
                    play->view.lookAt.z - play->view.eye.z };
        Vec3f r;
        Vec3f u;
        f32 n = sqrtf(SQ(f.x) + SQ(f.y) + SQ(f.z));

        if (n < 0.001f) {
            return;
        }
        f.x /= n;
        f.y /= n;
        f.z /= n;
        r.x = -f.z; // (right = forward x world up, flattened)
        r.y = 0.0f;
        r.z = f.x;
        n = sqrtf(SQ(r.x) + SQ(r.z));
        if (n < 0.001f) {
            r.x = 1.0f;
            r.z = 0.0f;
            n = 1.0f;
        }
        r.x /= n;
        r.z /= n;
        u.x = r.y * f.z - r.z * f.y; // up = right x forward
        u.y = r.z * f.x - r.x * f.z;
        u.z = r.x * f.y - r.y * f.x;
        SkinMatrix_Clear(&sBill);
        sBill.xx = r.x;
        sBill.yx = r.y;
        sBill.zx = r.z;
        sBill.xy = u.x;
        sBill.yy = u.y;
        sBill.zy = u.z;
        sBill.xz = -f.x;
        sBill.yz = -f.y;
        sBill.zz = -f.z;
    }
    OPEN_DISPS(gfxCtx);
    for (i = NV_FX_SLOTS - 1; i >= 0; i--) {
        NvFx* e = &sNvFx[i];
        s32 w;
        s32 h;
        s32 px;
        s32 py;
        f32 ps;
        s32 blend;
        u8 rgb[3];
        const u16* tex;
        Vtx* vtx;
        Mtx* mtx;
        MtxF mfT;
        MtxF mfR;
        MtxF mfS;
        MtxF mf;
        s32 rows;
        s32 r;
        s32 page;

        if ((e->animId == 0) || !e->drawn || (e->animHeader[11] & 0x80) || (e->frame == NULL)) {
            continue;
        }
        if (!NvCamera_InView(play, e->posX, e->posZ)) { // (RE1 draws it only in the camera's own zone)
            continue;
        }
        w = e->frame[2];
        h = e->frame[3];
        page = sNvFxPage[e->effectType];
        if ((w == 0) || (h == 0) || (page >= 4) || (e->lightFactor == 0)) {
            continue;
        }
        px = 0x80 - e->uv[2];
        py = 0x80 - e->uv[3];
        if (getenv("NV_LOG") && ((play->gameplayFrames % 40) == 0)) {
            printf("NVFXDRAW %d type %02X anim %d/%d frame %dx%d uv %d,%d page %d lf %d at %d,%d,%d\n", (int)i,
                   e->effectType, e->animId, e->updateId, w, h, e->uv[0], e->uv[1], page, e->lightFactor, e->posX,
                   e->posY, e->posZ);
            fflush(stdout);
        }
        // the size: w * lightFactor / 64 RE1 units a pixel (effect_submit_sprite's scale, as a world size)
        ps = (f32)w * e->lightFactor / 64.0f * NV_RE1_SCALE;
        if (getenv("NV_FX_BIG") != NULL) { // dev: drawn larger, to find them
            ps *= 8.0f;
        }
        NvFx_Look(e, &blend, rgb);
        tex = NvFx_Cut(page, e->uv[0], e->uv[1], (w + 3) & ~3, h);
        SkinMatrix_SetTranslate(&mfT, e->posX * NV_RE1_SCALE, -e->posY * NV_RE1_SCALE, -e->posZ * NV_RE1_SCALE);
        SkinMatrix_MtxFMtxFMult(&mfT, &sBill, &mfR);
        SkinMatrix_SetScale(&mfS, ps, ps, ps);
        SkinMatrix_MtxFMtxFMult(&mfR, &mfS, &mf);
        mtx = SkinMatrix_MtxFToNewMtx(gfxCtx, &mf);
        if (mtx == NULL) {
            continue;
        }
        gSPMatrix(POLY_XLU_DISP++, mtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        POLY_XLU_DISP = Gfx_SetupDL(POLY_XLU_DISP, 0x19); // (25: the see-through 3D setup)
        gDPPipeSync(POLY_XLU_DISP++);
        gSPClearGeometryMode(POLY_XLU_DISP++, G_LIGHTING | G_FOG | G_CULL_BOTH);
        gDPSetCycleType(POLY_XLU_DISP++, G_CYC_1CYCLE); // (setup 25 is two-cycle: TEXEL0 there reads tile 1)
        gDPSetTextureLUT(POLY_XLU_DISP++, G_TT_NONE);
        if (getenv("NV_FX_FLAT") != NULL) { // dev: untextured, to see the quads
            gDPSetCombineMode(POLY_XLU_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
        } else {
            gDPSetCombineLERP(POLY_XLU_DISP++, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE,
                              0, TEXEL0, 0, PRIMITIVE, 0);
        }
        gDPSetRenderMode(POLY_XLU_DISP++, G_RM_ZB_CLD_SURF, G_RM_ZB_CLD_SURF2);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, rgb[0], rgb[1], rgb[2],
                        (blend == 0) ? 255 : (blend == 1) ? 128 : (blend == 2) ? 200 : 90);
        gSPTexture(POLY_XLU_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
        gDPSetTexturePersp(POLY_XLU_DISP++, G_TP_PERSP);
        gDPSetTextureFilter(POLY_XLU_DISP++, G_TF_POINT);
        // in strips of a texture block each (4KB of texels at most)
        rows = MAX(1, 2048 / ((w + 3) & ~3));
        for (r = 0; r < h; r += rows) {
            s32 n = MIN(rows, h - r);
            s32 tw = (w + 3) & ~3;

            vtx = Graph_Alloc(gfxCtx, sizeof(Vtx) * 4);
            // (y up: the pivot's row at 0, the sprite's top above it)
            vtx[0] = (Vtx)VTX(-px, py - r, 0, 0, 0, 255, 255, 255, 255);
            vtx[1] = (Vtx)VTX(w - px, py - r, 0, w << 5, 0, 255, 255, 255, 255);
            vtx[2] = (Vtx)VTX(w - px, py - r - n, 0, w << 5, n << 5, 255, 255, 255, 255);
            vtx[3] = (Vtx)VTX(-px, py - r - n, 0, 0, n << 5, 255, 255, 255, 255);
            gDPLoadTextureBlock(POLY_XLU_DISP++, tex + r * tw, G_IM_FMT_RGBA, G_IM_SIZ_16b, tw, n, 0,
                                G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                                G_TX_NOLOD, G_TX_NOLOD);
            gSPVertex(POLY_XLU_DISP++, vtx, 4, 0);
            gSP2Triangles(POLY_XLU_DISP++, 0, 1, 2, 0, 0, 2, 3, 0);
        }
    }
    CLOSE_DISPS(gfxCtx);
}

/** How many are up (testing). */
s32 NvFx_Count(void) {
    s32 i;
    s32 n = 0;

    for (i = 0; i < NV_FX_SLOTS; i++) {
        n += sNvFx[i].animId != 0;
    }
    return n;
}
