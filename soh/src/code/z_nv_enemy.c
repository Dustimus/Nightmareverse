/*
 * Nightmareverse: RE1's enemies, as actors.
 *
 * A room's enemies are what its init script's Enemy_set instructions place (SCD 0x1B,
 * cmd_enemy_set 0x004617d0; the scripts run in z_nv_script.c), in RE1's enemy slots. Each becomes
 * an Nv_Enemy actor (registered with the ActorDB by NvActors.cpp), drawn with RE1's own model and
 * animations, "re1/enemies/em<type>" (re1/tools/soh_enemy.py).
 *
 * The enemy itself is RE1's: an RE1 entity run by RE1's code, ported (z_nv_zombie.c: the zombie,
 * types 0, 1 and 17), in RE1's units, 30 Hz ticks and room collision. This file is the Zelda
 * layer on top of it:
 *  - Link is RE1's player to it (gNvRe1Player), refreshed each tick: where he is, his facing,
 *    Jill's collision volume, whether he's mashing while held, whether he's swinging.
 *  - Link's weapons hit it through an OoT collider, and a hit enters RE1's own hit path
 *    (apply_weapon_damage's end: damage, hit state, the damaged or dying state) only when RE1
 *    would take one (hit_state clear). OoT's damage table is mod1's (its sizes, as the user
 *    allowed); each point of it is a handgun shot's 9 of RE1's damage, and the weapon's kind picks
 *    RE1's hit state: blades the knife's, shots the handgun's, blasts the grenade launcher's.
 *  - RE1's grab is OoT's grab: Link held where RE1 holds the player, RE1's timer and mashing
 *    deciding when it lets go, each RE1 bite half a heart (mod1's bite).
 *  - Targeting: OoT's usual enemy range, the focus at head height; not while it's dead.
 * Kenneth's corpse (37, 41) and the other characters are posed and still for now.
 */
#include "global.h"
#include "z_nv_re1.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NV_RE1_SCALE 0.0217f // OoT units per RE1 unit (re1/tools/rdt2scene.py SCALE)
#define NV_PI 3.14159265358979f
#define NV_GRAB_COOLDOWN 30 // frames after a grab before the next (mod1's)
#define NV_BITE_DAMAGE -8 // half a heart a bite (mod1's)
#define NV_BITE_FRAMES 25 // frames between bites (mod1's; RE1 bites every 19 ticks for 10 of Jill's 96)
#define NV_EM_MAX_BONES 24
#define NV_EM_MAX_ANIMS 64
#define NV_EM_MAX_BATCHES 384
#define NV_EM_MODELS 6
s32 NvLinkAnim_Play(s32 type, s32 anim, s32 hold);
void NvLinkAnim_Stop(void);
s32 NvLinkAnim_Playing(void);
s32 NvLinkAnim_Done(void);
#define NV_ZOMBIE_HEAD_BONE 2 // the zombie's head: the chest's child with none of its own

#define NV_EM_CELL_W 64
#define NV_EM_CELL_H 32
#define NV_ENEMY_SLOTS 16 // cmd_enemy_set's slot (its low nibble)

typedef struct {
    u16 cell;
    u16 numVerts;
    Vtx* vtx;
} NvEmBatch;

typedef struct {
    s32 type; // -1: free
    s32 numBones;
    s32 numBonesAll; // in the file (numBones is capped)
    s16* bones; // per bone: x, y, z (from its parent), parent
    s32 order[NV_EM_MAX_BONES]; // parents first
    NvRe1Anim bank;
    s32 cellsW;
    u16* cells;
    s32 firstBatch[NV_EM_MAX_BONES];
    s32 numBatches[NV_EM_MAX_BONES];
    // meshes past the skeleton's ("REN3": Plant 42's root ball and flower body, the Tyrant's heart)
    s32 numExtra;
    s32 extraFirstBatch[4];
    s32 extraNumBatches[4];
    NvEmBatch batches[NV_EM_MAX_BATCHES];
} NvEmModel;

typedef struct {
    Actor actor;
    ColliderCylinder collider;
    NvEmModel* model;
    s32 type;
    NvRe1Entity e; // RE1's entity
    u8 tickPhase;
    s32 grabbing;
    s16 sunStun; // frames left frozen by the Sun's Song, as a ReDead (En_Rd's 600)
    u8 wasHeadless;
    // the Tyrant's limbs blown off (z_nv_tyrant.inc): each one's pose when it went, and which bones
    // fly with them (the rest of the body isn't drawn)
    u8 limbCaught[5];
    f32 limbR[5][3][3];
    f32 limbT[5][3];
    u32 limbBones;
} NvEnemy;

s32 gNvSunSongSeen; // an enemy took this Sun's Song (z_nv_script.c ends it next frame)

enum { NV_ZST_IDLE = 1, NV_ZST_DAMAGED = 2, NV_ZST_DIE = 3, NV_ZST_ATTACK = 5 };

void NvRe1_Register(NvRe1Entity* e); // z_nv_zombie.c
void NvRe1_Unregister(NvRe1Entity* e);
extern s16 gNvRe1BiteDamage;

static NvEmModel sNvEmModels[NV_EM_MODELS];
static s32 sNvEmModelsInit = false;
static Actor* sNvEnemySlots[NV_ENEMY_SLOTS];

s32 NvFlags_Test(s32 bank, s32 bit); // z_nv_script.c
void NvFlags_Op(s32 bank, s32 bit, s32 mode);

s32 NvActor_EnemyId(void); // NvActors.cpp
static u8 sNvEnemySpawnSndGroup; // the sound group of the enemy being placed (NvEnemy_ScdSet)
static u8 sNvEnemySpawnAnim;  // its starting animation and frame (cmd_enemy_set bytes 0x13, 0x14)
static u8 sNvEnemySpawnFrame;

static ColliderCylinderInit sNvEnemyCylinder = {
    {
        COLTYPE_HIT0,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_PLAYER,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK1,
        { 0x00000000, 0x00, 0x00 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_ON | BUMP_HOOKABLE,
        OCELEM_ON,
    },
    { 20, 65, 0, { 0, 0, 0 } }, // mod1's: adult Link's height
};

// mod1's, from z_en_re_zombie.c: everything hurts it, the Kokiri Sword about a handgun shot
static DamageTable sNvZombieDamage = {
    /* Deku nut      */ DMG_ENTRY(0, 0x0),
    /* Deku stick    */ DMG_ENTRY(2, 0xF),
    /* Slingshot     */ DMG_ENTRY(1, 0xF),
    /* Explosive     */ DMG_ENTRY(4, 0xF),
    /* Boomerang     */ DMG_ENTRY(0, 0x0),
    /* Normal arrow  */ DMG_ENTRY(2, 0xF),
    /* Hammer swing  */ DMG_ENTRY(2, 0xF),
    /* Hookshot      */ DMG_ENTRY(0, 0x0),
    /* Kokiri sword  */ DMG_ENTRY(1, 0xF),
    /* Master sword  */ DMG_ENTRY(2, 0xF),
    /* Giant's Knife */ DMG_ENTRY(4, 0xF),
    /* Fire arrow    */ DMG_ENTRY(4, 0xF),
    /* Ice arrow     */ DMG_ENTRY(2, 0xF),
    /* Light arrow   */ DMG_ENTRY(4, 0xF),
    /* Unk arrow 1   */ DMG_ENTRY(6, 0xF),
    /* Unk arrow 2   */ DMG_ENTRY(1, 0xF),
    /* Unk arrow 3   */ DMG_ENTRY(3, 0xF),
    /* Fire magic    */ DMG_ENTRY(4, 0xF),
    /* Ice magic     */ DMG_ENTRY(2, 0xF),
    /* Light magic   */ DMG_ENTRY(4, 0xF),
    /* Shield        */ DMG_ENTRY(0, 0x0),
    /* Mirror Ray    */ DMG_ENTRY(0, 0x0),
    /* Kokiri spin   */ DMG_ENTRY(1, 0xF),
    /* Giant spin    */ DMG_ENTRY(4, 0xF),
    /* Master spin   */ DMG_ENTRY(2, 0xF),
    /* Kokiri jump   */ DMG_ENTRY(2, 0xF),
    /* Giant jump    */ DMG_ENTRY(8, 0xF),
    /* Master jump   */ DMG_ENTRY(4, 0xF),
    /* Unknown 1     */ DMG_ENTRY(0, 0x0),
    /* Unblockable   */ DMG_ENTRY(0, 0x0),
    /* Hammer jump   */ DMG_ENTRY(4, 0xF),
    /* Unknown 2     */ DMG_ENTRY(0, 0x0),
};

// RE1's tables (Zombie.cpp)
static const u8 sNvZombieHealth[16] = { 59, 59, 79, 59, 59, 39, 59, 79, 79, 99, 59, 79, 59, 59, 79, 59 };
static const u8 sNvZombieAnimId[16] = { 0, 0, 9, 9, 0, 12, 9, 29, 0, 0, 9, 0, 0, 0, 0, 0 };
// per attack direction (0 behind Link, 1 in front; 2, 3 the same from the floor): the attack's
// first animation (wind-up; +1 the bite, +2 letting go)
static const u8 sNvZombieAttackAnim[4] = { 0x11, 0x14, 0x17, 0x1A };

static s32 NvEnemy_IsZombie(s32 type) {
    return (type == 0) || (type == 1) || (type == 17);
}

static s32 NvEnemy_IsDog(s32 type) {
    return type == 2;
}

/** The enemies whose behaviour is ported (RE1 runs them; the rest stand in their first pose). */
static s32 NvEnemy_IsHunter(s32 type) {
    return type == 6;
}

static s32 NvEnemy_IsCrow(s32 type) {
    return type == 5;
}

static s32 NvEnemy_IsPlant(s32 type) {
    return type == 15;
}

static s32 NvEnemy_IsYawn(s32 type) {
    return (type == 13) || (type == 18);
}

static s32 NvEnemy_IsSpider(s32 type) {
    return type == 3;
}

static s32 NvEnemy_IsWasp(s32 type) {
    return type == 7;
}

static s32 NvEnemy_IsAdder(s32 type) {
    return type == 10;
}

static s32 NvEnemy_IsNeptune(s32 type) {
    return type == 11;
}

static s32 NvEnemy_IsBlackTiger(s32 type) {
    return type == 4;
}

static s32 NvEnemy_IsChimera(s32 type) {
    return type == 9;
}

static s32 NvEnemy_IsPlant42(s32 type) {
    return type == 8;
}

static s32 NvEnemy_IsWeb(s32 type) {
    return type == 19;
}

static s32 NvEnemy_IsTyrant(s32 type) {
    return (type == 12) || (type == 16);
}

static s32 NvEnemy_Acts(s32 type) {
    return NvEnemy_IsZombie(type) || NvEnemy_IsDog(type) || NvEnemy_IsHunter(type) || NvEnemy_IsCrow(type) ||
           NvEnemy_IsPlant(type) || NvEnemy_IsYawn(type) || NvEnemy_IsSpider(type) ||
           NvEnemy_IsWasp(type) || NvEnemy_IsAdder(type) || NvEnemy_IsNeptune(type) ||
           NvEnemy_IsBlackTiger(type) || NvEnemy_IsChimera(type) ||
           NvEnemy_IsPlant42(type) || NvEnemy_IsTyrant(type) || NvEnemy_IsWeb(type);
}

static s32 NvEnemy_IsCorpse(s32 type) {
    return (type == 37) || (type == 38) || (type == 41);
}

/** RE1's characters (Chris 32 ... Enrico 40, and the variants up to 46): z_nv_npc.inc. */
static s32 NvEnemy_IsNpc(s32 type) {
    return (type >= 32) && (type <= 46) && !NvEnemy_IsCorpse(type);
}

/* ---- models ---- */

static NvEmModel* NvEnemy_Model(s32 type) {
    char path[64];
    u8* d;
    NvEmModel* m = NULL;
    s32 i;
    s32 b;
    s32 nb = 0;
    s32 numBanks;
    s32 cellsH;

    if (!sNvEmModelsInit) {
        for (i = 0; i < NV_EM_MODELS; i++) {
            sNvEmModels[i].type = -1;
        }
        sNvEmModelsInit = true;
    }
    for (i = 0; i < NV_EM_MODELS; i++) {
        if (sNvEmModels[i].type == type) {
            return &sNvEmModels[i];
        }
        if ((m == NULL) && (sNvEmModels[i].type < 0)) {
            m = &sNvEmModels[i];
        }
    }
    if (m == NULL) {
        m = &sNvEmModels[type % NV_EM_MODELS]; // (the resources stay loaded: reparsing is cheap)
    }
    snprintf(path, sizeof(path), "re1/enemies/em%d", (int)type);
    if (!ResourceMgr_FileExists(path)) {
        return NULL;
    }
    d = (u8*)ResourceGetDataByName(path);
    if ((d == NULL) || ((memcmp(d, "REN2", 4) != 0) && (memcmp(d, "REN3", 4) != 0))) {
        return NULL;
    }
    memset(m, 0, sizeof(*m));
    m->numBonesAll = *(u16*)(d + 4);
    m->numBones = MIN(m->numBonesAll, NV_EM_MAX_BONES);
    numBanks = *(u16*)(d + 6);
    m->bones = (s16*)(d + 8);
    d += 8 + 8 * m->numBonesAll;
    for (b = 0; b < numBanks; b++) {
        s32 joints = *(u16*)d; // the bank's own (the Hunter's extra one has a joint fewer)
        s32 frameSize = 2 * (6 + 3 * joints);
        s32 numFrames;
        s32 numAnims;
        s32 a;

        d += 2;
        numFrames = *(u16*)d;
        if (b == 0) {
            m->bank.numFrames = numFrames;
            m->bank.frames = (s16*)(d + 2);
            m->bank.frameShorts = 6 + 3 * joints;
            m->bank.numJoints = MIN(joints, m->numBones);
        }
        d += 2 + numFrames * frameSize;
        numAnims = *(u16*)d;
        d += 2;
        for (a = 0; a < numAnims; a++) {
            s32 n = *(u16*)d;

            if ((b == 0) && (a < NV_EM_MAX_ANIMS)) {
                m->bank.steps[a] = (u16*)(d + 2);
                m->bank.numSteps[a] = n;
            }
            d += 2 + 4 * n;
        }
        if (b == 0) {
            m->bank.numAnims = MIN(numAnims, NV_EM_MAX_ANIMS);
        }
    }
    m->cellsW = *(u16*)d;
    cellsH = *(u16*)(d + 2);
    m->cells = (u16*)(d + 4);
    d += 4 + m->cellsW * cellsH * NV_EM_CELL_W * NV_EM_CELL_H * 2;
    for (i = 0; i < m->numBonesAll; i++) {
        s32 n = *(u16*)d;
        s32 k;

        d += 2;
        if (i < NV_EM_MAX_BONES) {
            m->firstBatch[i] = nb;
        }
        for (k = 0; k < n; k++) {
            u16 cell = *(u16*)d;
            u16 nv = *(u16*)(d + 2);

            if ((i < NV_EM_MAX_BONES) && (nb < NV_EM_MAX_BATCHES)) {
                m->batches[nb].cell = cell;
                m->batches[nb].numVerts = nv;
                m->batches[nb].vtx = (Vtx*)(d + 4);
                nb++;
                m->numBatches[i]++;
            }
            d += 4 + nv * sizeof(Vtx);
        }
    }
    if (memcmp(m->bones - 4, "REN3", 4) == 0) { // the extra meshes
        s32 count = *(u16*)d;

        d += 2;
        for (i = 0; i < count; i++) {
            s32 n = *(u16*)d;
            s32 k;

            d += 2;
            if (i < 4) {
                m->extraFirstBatch[i] = nb;
                m->numExtra = i + 1;
            }
            for (k = 0; k < n; k++) {
                u16 cell = *(u16*)d;
                u16 nv = *(u16*)(d + 2);

                if ((i < 4) && (nb < NV_EM_MAX_BATCHES)) {
                    m->batches[nb].cell = cell;
                    m->batches[nb].numVerts = nv;
                    m->batches[nb].vtx = (Vtx*)(d + 4);
                    nb++;
                    m->extraNumBatches[i]++;
                }
                d += 4 + nv * sizeof(Vtx);
            }
        }
    }
    // parents before children
    {
        s32 placed[NV_EM_MAX_BONES] = { 0 };
        s32 n = 0;
        s32 pass;

        for (pass = 0; (pass < m->numBones) && (n < m->numBones); pass++) {
            for (i = 0; i < m->numBones; i++) {
                s16 p = m->bones[i * 4 + 3];

                if (!placed[i] && ((p < 0) || (p >= m->numBones) || placed[p])) {
                    placed[i] = true;
                    m->order[n++] = i;
                }
            }
        }
    }
    m->type = type;
    return m;
}

/* ---- the Zelda layer ---- */

static u32 sNvDogFlinchUntil;

// Jill's collision volume (g_scaJillData): radius 372, half height 1350
static const s16 sNvJillSca[8] = { (s16)0x8000, 0, (s16)0xFABA, 0, 0x0546, 0x0174, 0, 0 };
// what cmd_enemy_set gives every entity before its own init: Chris's (g_scaDataTable[0]), r 422
static const s16 sNvChrisSca[8] = { (s16)0x8000, 0, (s16)0xFA06, 0, 0x05FA, 0x01A6, 0, 0 };

#define NV_RE1_HANDGUN_DAMAGE 9 // RE1's handgun on a zombie; a point of OoT damage is one shot
#define NV_RE1_JILL_HEALTH 96.0f // Jill's maximum health in RE1 (GameStart.cpp: 140 - 44 for her)

/** Link, as RE1's player: where he is (RE1's units), his facing, how he's pressing. */
static void NvEnemy_PlayerToRe1(PlayState* play) {
    Player* player = GET_PLAYER(play);
    NvRe1Player* p = &gNvRe1Player;
    Input* input = &play->state.input[0];

    p->pos[0] = (s16)p->t[0];
    p->pos[1] = (s16)p->t[1];
    p->pos[2] = (s16)p->t[2];
    p->t[0] = (s32)lroundf(player->actor.world.pos.x / NV_RE1_SCALE);
    p->t[1] = (s32)lroundf(-player->actor.world.pos.y / NV_RE1_SCALE);
    p->t[2] = (s32)lroundf(-player->actor.world.pos.z / NV_RE1_SCALE);
    p->angle = (0x400 - player->actor.shape.rot.y / 16) & 0xFFF; // (OoT yaw = 0x4000 - 16 angle)
    p->sca = sNvJillSca;
    // RE1's health as the same share of Jill's 96 as Link's hearts are of his (as the damage
    // goes the other way): the enemies' low-health moves (the dog's maul, the Hunter's
    // decapitation, Yawn's swallow) open when he's as hurt as she'd be
    p->health = (gSaveContext.health > 0)
                    ? (s16)MAX(1, (s32)lroundf(gSaveContext.health * NV_RE1_JILL_HEALTH / MAX(gSaveContext.healthCapacity, 1)))
                    : -1;
    p->attackTimer = (player->meleeWeaponState != 0) ? 1 : 0;
    gNvRe1DogFlinch = play->gameplayFrames < sNvDogFlinchUntil;
    {
        s32 NvWeapon_Holding(void);

        // aiming a gun (z_nv_weapon.c): RE1's player behaviour 0x14, which its enemies watch for
        p->flags = (p->flags & ~0xE0) | (NvWeapon_Holding() ? 0x20 : 0);
    }
    p->speed = (s16)((player->actor.speedXZ >= 5.0f) ? 230 : (player->actor.speedXZ / NV_RE1_SCALE * 2.0f / 3.0f));
    p->padHeld = 0;
    if ((ABS(input->rel.stick_x) > 30) || (ABS(input->rel.stick_y) > 30)) {
        p->padHeld |= 1;
    }
    if (CHECK_BTN_ANY(input->cur.button, BTN_A | BTN_B)) {
        p->padHeld |= 2;
    }
}

/** OoT's damage table entry to RE1's weapon: (hit state, weapon id). */
static void NvEnemy_HitKind(u8 effect, s32 dmgType, u8* hit, u8* weapon) {
    // blades and blunt: the knife's record (hit 1, weapon 1)
    *hit = 1;
    *weapon = 1;
    switch (dmgType) {
        case 2:  // slingshot
        case 5:  // arrows
        case 11: // fire arrow
        case 12: // ice arrow
        case 13: // light arrow
        case 14: // (unk arrows: the guns' rounds in mod1)
        case 15:
        case 16:
            *hit = 1; // the handgun's
            *weapon = 2;
            break;
        case 3: // explosive: the grenade launcher's (hit 2, weapon 7)
            *hit = 2;
            *weapon = 7;
            break;
    }
}

/** Which of OoT's damage kinds hit: the bit of the collider's acHitInfo->toucher.dmgFlags. */
static s32 NvEnemy_DamageType(NvEnemy* this) {
    u32 flags = (this->collider.info.acHitInfo != NULL) ? this->collider.info.acHitInfo->toucher.dmgFlags : 0;
    s32 i;

    for (i = 0; i < 32; i++) {
        if (flags & (1u << i)) {
            return i;
        }
    }
    return -1;
}

static void NvEnemy_Hits(NvEnemy* this, PlayState* play) {
    NvRe1Entity* e = &this->e;
    u8 hit;
    u8 weapon;
    s32 points;

    if (!(this->collider.base.acFlags & AC_HIT)) {
        return;
    }
    this->collider.base.acFlags &= ~AC_HIT;
    points = this->actor.colChkInfo.damage;
    // apply_weapon_damage takes a hit only on an enemy not already reacting (hit_state clear)
    if ((this->actor.colChkInfo.damageEffect == 0) || (points == 0) || (e->hit_state != 0) || (e->health < 0)) {
        return;
    }
    NvEnemy_HitKind(this->actor.colChkInfo.damageEffect, NvEnemy_DamageType(this), &hit, &weapon);
    Actor_SetColorFilter(&this->actor, 0x4000, 255, 0, 8);
    NvZombie_Hit(e, (s16)(points * NV_RE1_HANDGUN_DAMAGE), hit, weapon);
    printf("NVENEMY hit: %d points, health %d, hit state 0x%02x\n", (int)points, e->health, e->hit_state);
    fflush(stdout);
}

/*
 * RE1's guns on an enemy (WeaponDamage.cpp apply_weapon_damage, for z_nv_weapon.c): the
 * zombies' hit records (g_weaponHitRecordsFirstRun's zombie rows, first playthrough), by the
 * weapon's item id less one: the damage and the hit state.
 */
static const s16 sNvGunDamage[10] = { 8, 9, 53, 50, 130, 20, 201, 95, 95, 900 };
static const u8 sNvGunHitState[10] = { 1, 1, 2, 2, 2, 1, 2, 1, 1, 2 };

/*
 * RE1's hit records ("re1/hits", re1/tools/soh_weapons.py, from the player's executable):
 * apply_weapon_damage's first-playthrough table, per enemy type and weapon (type * 10 + weapon id
 * - 1), 12 bytes: knockback x, y, z, damage, the post-hit kind and data, the hit state. Without
 * them (a mod made without the executable) every enemy takes the zombie's row above.
 */
typedef struct {
    s16 kx, ky, kz;
    s16 dmg;
    u8 kind, data, hit, pad;
} NvHitRecord;

static const NvHitRecord* NvEnemy_HitRecord(s32 type, s32 item) {
    static const u8* sHits;
    static s32 sTried;
    u16 n;
    s32 k = type * 10 + item - 1;

    if (!sTried) {
        sTried = true;
        if (ResourceMgr_FileExists("re1/hits")) {
            sHits = (const u8*)ResourceGetDataByName("re1/hits");
            if ((sHits != NULL) && (memcmp(sHits, "RHIT", 4) != 0)) {
                sHits = NULL;
            }
        }
    }
    if ((sHits == NULL) || (item < 1) || (item > 10) || (type < 0)) {
        return NULL;
    }
    memcpy(&n, sHits + 4, 2);
    return (k < n) ? (const NvHitRecord*)(sHits + 6 + k * 12) : NULL;
}

/**
 * The per-enemy rules of RE1's hit tests for a shot (WeaponDamage.cpp: apply_weapon_damage's aim
 * filter, then weapon_hit_detect_knife / _gun / _projectile's own): whether actor a can be hit by
 * weapon `item` aimed `aim` (the player's flags: 0x20 up, 0x40 level, 0x80 down), and what its
 * reach changes by. False also for anything but RE1's enemies.
 */
s32 NvEnemy_ShotRules(Actor* a, s32 item, u8 aim, s32* reachAdj) {
    NvEnemy* this = (NvEnemy*)a;
    NvRe1Entity* e = &this->e;
    s32 t = this->type;
    s32 zombie = (t == 0x00) || (t == 0x01) || (t == 0x11);

    *reachAdj = 0;
    if ((a == NULL) || (a->id != NvActor_EnemyId())) {
        return false;
    }
    if ((e->status_flags & aim & 0xE0) == 0) { // (the heights it can be hit at: its status flags)
        return false;
    }
    if (item == 1) { // the knife
        if (zombie && (aim & 0x80)) {
            return false;
        }
        if ((((t == 0x02) && (e->t[1] > -400)) || (t == 0x03)) && (aim & 0x40)) {
            return false; // (a dog on the ground, a spider: not at the level)
        }
        if (((t == 0x05) || (t == 0x07) || (t == 0x09)) && (e->t[1] < -4800)) {
            return false; // (on the ceiling)
        }
        *reachAdj = (t == 0x04) ? -800 : (t == 0x08) ? 2000 : ((t == 0x07) || (t == 0x0A)) ? 100 : 0;
    } else if (item <= 5) { // the guns
        if (zombie && (aim & 0x80) && (item != 3)) {
            return false; // (a zombie aimed down at: only the shotgun)
        }
        *reachAdj = (t == 0x04) ? -1000 : (t == 0x08) ? 2000 : 0;
    } else { // the flame and the launchers
        *reachAdj = (t == 0x04) ? -1000 : 0;
    }
    return true;
}

static s32 NvEnemy_Parked(const NvRe1Entity* e);
static void NvEnemy_JointsWorld(NvEnemy* this);

/**
 * Whether actor a is one of RE1's enemies a gun can hit now (alive, out, not already reacting:
 * apply_weapon_damage takes only hit_state 0), and where it is (RE1's units) and how wide.
 */
s32 NvEnemy_GunTarget(Actor* a, s32* x, s32* z, s32* radius) {
    NvEnemy* this = (NvEnemy*)a;
    NvRe1Entity* e;

    if ((a == NULL) || (a->id != NvActor_EnemyId())) {
        return false;
    }
    e = &this->e;
    if ((e->health < 0) || (e->status_flags & 2) || NvEnemy_Parked(e) || (e->hit_state != 0)) {
        return false;
    }
    *x = e->t[0];
    *z = e->t[2];
    *radius = ((e->sca != NULL) && (e->sca[5] > 0)) ? e->sca[5] : 400;
    return true;
}

/**
 * A round from weapon item (RE1's item id), aimed `aim`, hits it (apply_weapon_damage): the damage
 * and hit state from RE1's record for its type and the weapon, the hit state raised by the aim
 * unless it was up (0x20); RE1's characters (type 0x14 and up) take no damage.
 */
void NvEnemy_GunHit(Actor* a, s32 item, u8 aim) {
    NvEnemy* this = (NvEnemy*)a;
    s32 k = CLAMP(item - 1, 0, 9);
    const NvHitRecord* rec = NvEnemy_HitRecord(this->type, item);

    // the spider web's row of the hit records: the knife cuts it, fire and the launchers burn it, bullets don't
    static const s16 sNvWebDamage[10] = { 10, 0, 0, 0, 0, 2, 30, 30, 30, 900 };
    s16 dmg;
    u8 hit;

    if (this->type >= 0x14) {
        return;
    }
    if (rec != NULL) {
        dmg = rec->dmg;
        hit = rec->hit;
    } else {
        dmg = NvEnemy_IsWeb(this->type) ? sNvWebDamage[k] : sNvGunDamage[k];
        hit = sNvGunHitState[k];
    }
    if ((aim & 0xE0) != 0x20) {
        hit += (u8)(aim >> 5);
    }
    Actor_SetColorFilter(&this->actor, 0x4000, 255, 0, 8);
    NvZombie_Hit(&this->e, dmg, hit, (u8)item);
    printf("NVENEMY shot (weapon %d, aim %02X): %d damage, hit %d, health %d\n", (int)item, aim, dmg, hit,
           this->e.health);
    fflush(stdout);
}

/** RE1's grab, as OoT's: held while RE1's attack lasts, let go when it ends. */
static void NvEnemy_Grab(NvEnemy* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    NvRe1Entity* e = &this->e;

    if ((e->state == NV_ZST_ATTACK) && !this->grabbing) {
        if (!(player->stateFlags2 & PLAYER_STATE2_GRABBED_BY_ENEMY) && play->grabPlayer(play, player)) {
            this->grabbing = true;
            printf("NVENEMY grab (direction %d)\n", e->attacking_direction);
            fflush(stdout);
        } else { // OoT won't hand Link over (a cutscene, another grab): no attack
            e->state = NV_ZST_IDLE;
            e->ignore_player_flag = 1;
            e->action_behavior = 3;
            e->action_state = 0;
            e->hit_state = 0;
            gNvRe1Player.isBeingAttackedFlag = 0;
        }
    }
    if (!this->grabbing) {
        return;
    }
    if (e->state != NV_ZST_ATTACK) {
        // the attack is over; RE1's last player animation (a crawler's: Jill's stomp) plays out first
        if (NvLinkAnim_Playing() && !NvLinkAnim_Done()) {
            player->av2.actionVar2 = 0;
            return;
        }
        NvLinkAnim_Stop();
        // let him go (Player_Action_8084F308's mash count)
        if (player->stateFlags2 & PLAYER_STATE2_GRABBED_BY_ENEMY) {
            player->av2.actionVar2 = 101;
        }
        this->grabbing = false;
        gNvRe1Player.isBeingAttackedFlag = 0;
        return;
    }
    // the player's side, as RE1 plays it from the zombie's EMD (z_nv_linkanim.c): per direction
    // three animations (zombie_attack_player_anim_tbl: 0, 3, 6, 9): the grab, the hold (the bites,
    // looping), then the end (pushed off, or a crawler's head stomped)
    {
        s32 phase = (e->action_state < 2) ? 0 : (e->action_state < 4) ? 1 : 2;

        NvLinkAnim_Play(this->type, e->attacking_direction * 3 + phase, phase != 1);
    }
    // RE1 decides when it ends: OoT's own mashing out stays off, RE1's mashing counts instead
    player->av2.actionVar2 = 0;
    // the player stands at RE1's grab point. is_facing_toward_entity is true when she faces the way
    // the zombie does, so directions 1 and 3 take her from behind, 0 and 2 face to face
    player->actor.world.pos.x = e->unk_c6 * NV_RE1_SCALE;
    player->actor.world.pos.z = -e->unk_c8 * NV_RE1_SCALE;
    player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw =
        (s16)(0x4000 - e->angle * 16) + ((e->attacking_direction & 1) ? 0 : 0x8000);
    // RE1's damage as the same share of Link's hearts as of Jill's 96, as the heals are (a herb
    // undoes what it would in RE1, however many hearts he has); the remainder carries over
    if (gNvRe1BiteDamage > 0) {
        static f32 sOwed;
        s32 whole;

        sOwed += gNvRe1BiteDamage * gSaveContext.healthCapacity / NV_RE1_JILL_HEALTH;
        gNvRe1BiteDamage = 0;
        whole = (s32)sOwed;
        if (whole > 0) {
            sOwed -= whole;
            play->damagePlayer(play, -whole);
        }
        Rumble_Request(this->actor.xzDistToPlayer, 240, 1, 12);
        Player_PlaySfx(&player->actor, NA_SE_VO_LI_DAMAGE_S + player->ageProperties->unk_92);
    }
}

/**
 * A dog's bite (z_nv_cerberus.inc): RE1's damage as the same share of Link's hearts as of Jill's
 * 96, and a knock back from the dog, as OoT's enemies hit.
 */
// sNvDogFlinchUntil: the frame Link's flinch from a bite ends (RE1 holds a dog off until then)
static void NvEnemy_DogBite(NvEnemy* this, PlayState* play) {
    static f32 sOwed;
    Player* player = GET_PLAYER(play);
    s32 whole;

    if (gNvRe1DogBite <= 0) {
        return;
    }
    sOwed += gNvRe1DogBite * gSaveContext.healthCapacity / NV_RE1_JILL_HEALTH;
    gNvRe1DogBite = 0;
    sNvDogFlinchUntil = play->gameplayFrames + 30;
    whole = (s32)sOwed;
    if (whole > 0) {
        sOwed -= whole;
        play->damagePlayer(play, -whole);
    }
    Actor_SetPlayerKnockbackLargeNoDamage(play, &this->actor, 6.0f, this->actor.yawTowardsPlayer, 4.0f);
    Rumble_Request(this->actor.xzDistToPlayer, 180, 1, 10);
    Player_PlaySfx(&player->actor, NA_SE_VO_LI_DAMAGE_S + player->ageProperties->unk_92);
    printf("NVENEMY dog bite\n");
    fflush(stdout);
}

/**
 * An enemy holding Link (the plant's drain, the dog's maul, the Hunter's and Yawn's kills): OoT's
 * grab while RE1's hold lasts, at the spot it pulled him to; a drain as hearts (the same share of
 * his as of Jill's 96), no knock back; a kill move's end, all of them.
 */
static void NvEnemy_Hold(NvEnemy* this, PlayState* play) {
    static f32 sOwed;
    Player* player = GET_PLAYER(play);
    NvRe1Entity* e = &this->e;

    if (gNvRe1Drain > 0) {
        s32 whole;

        sOwed += gNvRe1Drain * gSaveContext.healthCapacity / NV_RE1_JILL_HEALTH;
        gNvRe1Drain = 0;
        whole = (s32)sOwed;
        if (whole > 0) {
            sOwed -= whole;
            play->damagePlayer(play, -whole);
            Player_PlaySfx(&player->actor, NA_SE_VO_LI_DAMAGE_S + player->ageProperties->unk_92);
        }
    }
    if (gNvRe1Kill) { // a kill move: all of his hearts
        gNvRe1Kill = 0;
        if (gSaveContext.health > 0) {
            play->damagePlayer(play, -gSaveContext.health);
        }
    }
    if (e->holding && !this->grabbing) {
        if (!(player->stateFlags2 & PLAYER_STATE2_GRABBED_BY_ENEMY) && play->grabPlayer(play, player)) {
            this->grabbing = true;
            printf("NVENEMY hold (type %d)\n", (int)this->type);
            fflush(stdout);
        } else {
            e->holding = 0; // OoT won't hand him over: let go
        }
    }
    if (!this->grabbing) {
        return;
    }
    if (!e->holding) {
        if (player->stateFlags2 & PLAYER_STATE2_GRABBED_BY_ENEMY) {
            player->av2.actionVar2 = 101; // let him go
        }
        this->grabbing = false;
        gNvRe1Player.isBeingAttackedFlag = 0;
        return;
    }
    player->av2.actionVar2 = 0; // RE1 decides when it ends (mashing counts there)
    player->actor.world.pos.x = e->unk_c6 * NV_RE1_SCALE;
    player->actor.world.pos.z = -e->unk_c8 * NV_RE1_SCALE;
}

/* ---- the actor ---- */

void NvEnemy_Init(Actor* thisx, PlayState* play) {
    NvEnemy* this = (NvEnemy*)thisx;
    NvRe1Entity* e = &this->e;

    this->type = thisx->params & 0xFF;
    this->model = NvEnemy_Model(this->type);
    memset(e, 0, sizeof(*e));
    e->id = this->type;
    e->snd_group = sNvEnemySpawnSndGroup;
    sNvEnemySpawnSndGroup = 0;
    e->behavior_flags = thisx->home.rot.x & 0xFF;
    e->death_event_id = thisx->home.rot.z & 0xFF;
    e->angle = thisx->home.rot.y & 0xFFF;
    e->t[0] = (s32)lroundf(thisx->world.pos.x / NV_RE1_SCALE);
    e->t[1] = (s32)lroundf(-thisx->world.pos.y / NV_RE1_SCALE);
    e->t[2] = (s32)lroundf(-thisx->world.pos.z / NV_RE1_SCALE);
    e->status_flags = 1;
    e->anim = (this->model != NULL) ? &this->model->bank : NULL;
    e->sca = sNvChrisSca; // cmd_enemy_set's (the zombie's init gives it its own)
    thisx->world.rot.x = thisx->world.rot.z = thisx->shape.rot.x = thisx->shape.rot.z = 0;
    ActorShape_Init(&thisx->shape, 0.0f, NULL, 0.0f);
    thisx->targetMode = 3; // OoT's usual enemy range (350)
    thisx->colChkInfo.mass = MASS_IMMOVABLE; // RE1 moves it; OoT only pushes Link off it
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sNvEnemyCylinder);
    if (NvEnemy_IsZombie(this->type) && (this->model != NULL)) {
        thisx->colChkInfo.damageTable = &sNvZombieDamage;
        thisx->colChkInfo.health = 255; // (RE1's health decides)
        thisx->naviEnemyId = 0x2A; // (a ReDead's)
        NvRe1_TickBegin();
        NvZombie_Init(e);
    } else if (NvEnemy_IsDog(this->type) && (this->model != NULL)) {
        thisx->colChkInfo.damageTable = &sNvZombieDamage;
        thisx->colChkInfo.health = 255;
        thisx->naviEnemyId = 0x1E; // (a Wolfos's)
        NvRe1_TickBegin();
        NvCerberus_Init(e);
    } else if (NvEnemy_IsHunter(this->type) && (this->model != NULL)) {
        thisx->colChkInfo.damageTable = &sNvZombieDamage;
        thisx->colChkInfo.health = 255;
        thisx->naviEnemyId = 0x22; // (a Lizalfos's)
        NvRe1_TickBegin();
        NvHunter_Init(e);
    } else if (NvEnemy_IsCrow(this->type) && (this->model != NULL)) {
        thisx->colChkInfo.damageTable = &sNvZombieDamage;
        thisx->colChkInfo.health = 255;
        thisx->naviEnemyId = 0x3B; // (a Guay's)
        NvRe1_TickBegin();
        NvCrow_Init(e);
    } else if (NvEnemy_IsPlant(this->type) && (this->model != NULL)) {
        thisx->colChkInfo.damageTable = &sNvZombieDamage;
        thisx->colChkInfo.health = 255;
        thisx->naviEnemyId = 0x2E; // (a Deku Baba's)
        NvRe1_TickBegin();
        NvPlant_Init(e);
    } else if (NvEnemy_IsYawn(this->type) && (this->model != NULL)) {
        thisx->colChkInfo.damageTable = &sNvZombieDamage;
        thisx->colChkInfo.health = 255;
        thisx->naviEnemyId = 0x40; // (a Dodongo's: a big one)
        NvRe1_TickBegin();
        NvYawn_Init(e);
    } else if (NvEnemy_IsSpider(this->type) && (this->model != NULL)) {
        thisx->colChkInfo.damageTable = &sNvZombieDamage;
        thisx->colChkInfo.health = 255;
        thisx->naviEnemyId = 0x1F; // (a Skulltula's)
        NvRe1_TickBegin();
        NvSpider_Init(e);
    } else if (NvEnemy_IsWasp(this->type) && (this->model != NULL)) {
        thisx->colChkInfo.damageTable = &sNvZombieDamage;
        thisx->colChkInfo.health = 255;
        thisx->naviEnemyId = 0x3B; // (a Guay's, as the crows: a flier)
        NvRe1_TickBegin();
        NvWasp_Init(e);
    } else if (NvEnemy_IsAdder(this->type) && (this->model != NULL)) {
        thisx->colChkInfo.damageTable = &sNvZombieDamage;
        thisx->colChkInfo.health = 255;
        thisx->naviEnemyId = 0x2E; // (a Deku Baba's, as the plants: a striker from the ground)
        NvRe1_TickBegin();
        NvAdder_Init(e);
    } else if (NvEnemy_IsNeptune(this->type) && (this->model != NULL)) {
        thisx->colChkInfo.damageTable = &sNvZombieDamage;
        thisx->colChkInfo.health = 255;
        thisx->naviEnemyId = 0x40; // (a Dodongo's, as Yawn: a big one)
        NvRe1_TickBegin();
        NvNeptune_Init(e);
    } else if (NvEnemy_IsWeb(this->type) && (this->model != NULL)) {
        thisx->colChkInfo.damageTable = &sNvZombieDamage;
        thisx->colChkInfo.health = 255;
        NvRe1_TickBegin();
        NvWeb_Init(e);
    } else if (NvEnemy_IsBlackTiger(this->type) && (this->model != NULL)) {
        thisx->colChkInfo.damageTable = &sNvZombieDamage;
        thisx->colChkInfo.health = 255;
        thisx->naviEnemyId = 0x1F; // (a Skulltula's, as the Web Spinners)
        NvRe1_TickBegin();
        NvBlackTiger_Init(e);
    } else if (NvEnemy_IsChimera(this->type) && (this->model != NULL)) {
        thisx->colChkInfo.damageTable = &sNvZombieDamage;
        thisx->colChkInfo.health = 255;
        thisx->naviEnemyId = 0x22; // (a Lizalfos's, as the Hunters)
        NvRe1_TickBegin();
        NvChimera_Init(e);
    } else if (NvEnemy_IsPlant42(this->type) && (this->model != NULL)) {
        thisx->colChkInfo.damageTable = &sNvZombieDamage;
        thisx->colChkInfo.health = 255;
        thisx->naviEnemyId = 0x2E; // (a Deku Baba's, as the other plants)
        NvRe1_TickBegin();
        NvPlant42_Init(e);
    } else if (NvEnemy_IsTyrant(this->type) && (this->model != NULL)) {
        thisx->colChkInfo.damageTable = &sNvZombieDamage;
        thisx->colChkInfo.health = 255;
        thisx->naviEnemyId = 0x40; // (a Dodongo's, as the big ones)
        NvRe1_TickBegin();
        NvTyrant_Init(e);
    } else if (NvEnemy_IsNpc(this->type) && (this->model != NULL)) {
        thisx->flags &= ~(ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE);
        e->sca = sNvJillSca; // (a person's)
        e->animationId = sNvEnemySpawnAnim;
        e->animation_frame_id = sNvEnemySpawnFrame;
        e->timing_control = 1;
        NvRe1_TickBegin();
        NvNpc_Init(e);
    } else {
        thisx->flags &= ~(ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE);
        e->animationId = 0;
        if (this->model != NULL) {
            NvRe1_JointMove(e, 0, 0x400);
        }
    }
    NvRe1_Register(e);
    printf("NVENEMY type %d behaviour 0x%02x at %d,%d,%d angle %d model %s health %d\n", (int)this->type,
           e->behavior_flags, (int)e->t[0], (int)e->t[1], (int)e->t[2], e->angle,
           (this->model != NULL) ? "ok" : "missing", e->health);
    fflush(stdout);
}

void NvEnemy_Destroy(Actor* thisx, PlayState* play) {
    NvPlant42_Forget(&((NvEnemy*)thisx)->e);
    NvEnemy* this = (NvEnemy*)thisx;
    s32 i;

    Collider_DestroyCylinder(play, &this->collider);
    NvRe1_Unregister(&this->e);
    if (this->grabbing) {
        gNvRe1Player.isBeingAttackedFlag = 0;
    }
    for (i = 0; i < NV_ENEMY_SLOTS; i++) {
        if (sNvEnemySlots[i] == thisx) {
            sNvEnemySlots[i] = NULL;
        }
    }
}

s32 NvDoorScene_Covering(void);
s32 NvMovie_Playing(void);

static s32 NvEnemy_Parked(const NvRe1Entity* e) {
    // a dog the script holds (behaviour 0x80) waits outside, off the room (108's two, behind the
    // windows they come in through): not drawn, not hit, until its event lets it go
    if ((e->id == 2) && (e->behavior_flags == 0x80)) {
        return true;
    }
    // a Hunter the script holds for its entrance (behaviour flag 0x40) waits the same way
    if ((e->id == 6) && (e->behavior_flags & 0x40)) {
        return true;
    }
    // Yawn waiting for its entrance (behaviour 0x80), or gone out of the room after fleeing
    if (((e->id == 13) || (e->id == 18)) && (e->behavior_flags & 0x80)) {
        return true;
    }
    return (e->behavior_flags & 0x80) && (e->t[0] == 0) && (e->t[1] == 0) && (e->t[2] == 0);
}

void NvEnemy_Update(Actor* thisx, PlayState* play) {
    NvEnemy* this = (NvEnemy*)thisx;
    NvRe1Entity* e = &this->e;
    s32 ticks;
    s32 t;
    s32 alive;

    if ((this->model == NULL) || NvDoorScene_Covering()) {
        return;
    }
    {
        // g_message_flags: the enemies (bit 2) and the characters (bit 1) only think while the room's
        // scripts and messages let them
        u32 NvScript_MsgFlags(void);

        if (!(NvScript_MsgFlags() & (NvEnemy_IsNpc(this->type) ? 0x02 : 0x04))) {
            return;
        }
    }
    // RE1 runs at 30 Hz: three ticks every two frames
    ticks = (this->tickPhase ^= 1) ? 1 : 2;
    if (NvEnemy_IsZombie(this->type)) {
        // the Sun's Song freezes zombies as it does ReDeads: RE1's code waits, hits still land
        if ((gSaveContext.sunsSongState != SUNSSONG_INACTIVE) && (e->health >= 0) && !this->grabbing) {
            gNvSunSongSeen = true;
            if (this->sunStun == 0) {
                this->sunStun = 600;
                Audio_PlayActorSound2(thisx, NA_SE_EN_LIGHT_ARROW_HIT);
            }
        }
        NvEnemy_Hits(this, play);
        if ((this->sunStun > 0) && (e->hit_state != 0)) {
            this->sunStun = 0; // struck: it wakes into the hit
        }
        if (this->sunStun > 0) {
            this->sunStun--;
            Actor_SetColorFilter(thisx, -0x8000, 0xC8, 0, 0xFF);
            ticks = 0;
        }
        for (t = 0; t < ticks; t++) {
            NvEnemy_PlayerToRe1(play);
            NvRe1_TickBegin();
            NvZombie_Update(e);
            NvEnemy_Grab(this, play);
        }
    } else if (NvEnemy_IsDog(this->type)) {
        NvEnemy_Hits(this, play);
        for (t = 0; t < ticks; t++) {
            NvEnemy_PlayerToRe1(play);
            NvRe1_TickBegin();
            NvCerberus_Update(e);
            NvEnemy_DogBite(this, play);
            NvEnemy_Hold(this, play); // (the maul)
        }
    } else if (NvEnemy_IsHunter(this->type)) {
        NvEnemy_Hits(this, play);
        for (t = 0; t < ticks; t++) {
            NvEnemy_PlayerToRe1(play);
            NvRe1_TickBegin();
            NvHunter_Update(e);
            NvEnemy_DogBite(this, play); // (its claws: the same hurt and knock back)
            NvEnemy_Hold(this, play);     // (the decapitation)
        }
    } else if (NvEnemy_IsCrow(this->type)) {
        NvEnemy_Hits(this, play);
        for (t = 0; t < ticks; t++) {
            NvEnemy_PlayerToRe1(play);
            NvRe1_TickBegin();
            NvCrow_Update(e);
            NvEnemy_DogBite(this, play); // (its peck)
        }
    } else if (NvEnemy_IsPlant(this->type)) {
        NvEnemy_Hits(this, play);
        for (t = 0; t < ticks; t++) {
            NvEnemy_PlayerToRe1(play);
            NvRe1_TickBegin();
            NvPlant_Update(e);
            NvEnemy_DogBite(this, play); // (a lasher's whip)
            NvEnemy_Hold(this, play);
        }
    } else if (NvEnemy_IsYawn(this->type)) {
        NvEnemy_Hits(this, play);
        for (t = 0; t < ticks; t++) {
            NvEnemy_PlayerToRe1(play);
            NvRe1_TickBegin();
            NvYawn_Update(e);
            NvEnemy_DogBite(this, play); // (its bite)
            NvEnemy_Hold(this, play);     // (the swallow)
        }
    } else if (NvEnemy_IsSpider(this->type)) {
        NvEnemy_Hits(this, play);
        for (t = 0; t < ticks; t++) {
            NvEnemy_PlayerToRe1(play);
            NvRe1_TickBegin();
            NvSpider_Update(e);
            NvEnemy_DogBite(this, play); // (its bite and its venom)
        }
    } else if (NvEnemy_IsWasp(this->type)) {
        NvEnemy_Hits(this, play);
        for (t = 0; t < ticks; t++) {
            NvEnemy_PlayerToRe1(play);
            NvRe1_TickBegin();
            NvWasp_Update(e);
            NvEnemy_DogBite(this, play); // (its sting)
            NvEnemy_Hold(this, play);     // (the pin)
        }
    } else if (NvEnemy_IsAdder(this->type)) {
        NvEnemy_Hits(this, play);
        for (t = 0; t < ticks; t++) {
            NvEnemy_PlayerToRe1(play);
            NvRe1_TickBegin();
            NvAdder_Update(e);
            NvEnemy_DogBite(this, play); // (its strike)
        }
    } else if (NvEnemy_IsNeptune(this->type)) {
        NvEnemy_Hits(this, play);
        for (t = 0; t < ticks; t++) {
            NvEnemy_PlayerToRe1(play);
            NvRe1_TickBegin();
            NvNeptune_Update(e);
            NvEnemy_Hold(this, play); // (the bite's hold, the chewing, the kill)
        }
    } else if (NvEnemy_IsBlackTiger(this->type)) {
        NvEnemy_Hits(this, play);
        for (t = 0; t < ticks; t++) {
            NvEnemy_PlayerToRe1(play);
            NvRe1_TickBegin();
            NvBlackTiger_Update(e);
            NvEnemy_DogBite(this, play); // (its bite and its venom)
        }
    } else if (NvEnemy_IsWeb(this->type)) {
        NvEnemy_Hits(this, play);
        for (t = 0; t < ticks; t++) {
            NvEnemy_PlayerToRe1(play);
            NvRe1_TickBegin();
            NvWeb_Update(e);
        }
    } else if (NvEnemy_IsChimera(this->type)) {
        NvEnemy_Hits(this, play);
        for (t = 0; t < ticks; t++) {
            NvEnemy_PlayerToRe1(play);
            NvRe1_TickBegin();
            NvChimera_Update(e);
            NvEnemy_DogBite(this, play); // (its claws)
            NvEnemy_Hold(this, play);     // (the grab)
        }
    } else if (NvEnemy_IsPlant42(this->type)) {
        NvEnemy_Hits(this, play);
        for (t = 0; t < ticks; t++) {
            NvEnemy_PlayerToRe1(play);
            NvRe1_TickBegin();
            NvPlant42_Update(e);
            NvEnemy_DogBite(this, play); // (the sweep, the strike, the squeeze's end)
            NvEnemy_Hold(this, play);     // (the grab, the kill)
        }
    } else if (NvEnemy_IsTyrant(this->type)) {
        NvEnemy_Hits(this, play);
        for (t = 0; t < ticks; t++) {
            NvEnemy_PlayerToRe1(play);
            NvRe1_TickBegin();
            NvTyrant_Update(e);
            NvEnemy_DogBite(this, play); // (its claw)
            NvEnemy_Hold(this, play);     // (the impale)
        }
    } else if (NvEnemy_IsNpc(this->type)) {
        for (t = 0; t < ticks; t++) {
            NvEnemy_PlayerToRe1(play);
            NvRe1_TickBegin();
            NvNpc_Update(e); // (what the room's event scripts have it do)
        }
    } else {
        for (t = 0; t < ticks; t++) {
            NvRe1_JointMove(e, 0, 0x400);
        }
    }
    if (NvEnemy_IsTyrant(this->type) && e->ty.boom) { // the rocket's blasts
        Vec3f at = thisx->world.pos;
        Vec3f zero = { 0.0f, 0.0f, 0.0f };

        e->ty.boom = 0;
        at.x += Rand_CenteredFloat(30.0f);
        at.y += 20.0f + Rand_ZeroFloat(40.0f);
        at.z += Rand_CenteredFloat(30.0f);
        EffectSsBomb2_SpawnLayered(play, &at, &zero, &zero, 100, 19);
    }
    if (e->fx && (e->numJoints > 0)) { // its effect this tick
        // (a weapon's: from the hand, RE1's joint 14; blood: from the joint it names)
        s32 j = MIN((e->fx == 4) ? e->fxJoint : 14, e->numJoints - 1);
        Vec3f at = { e->joint[j][0] * NV_RE1_SCALE, -e->joint[j][1] * NV_RE1_SCALE, -e->joint[j][2] * NV_RE1_SCALE };

        if (e->fx == 1) {
            EffectSsHitMark_SpawnFixedScale(play, 0, &at);
        } else if (e->fx == 3) { // glass bursting (RE1's 53 shards: OoT's debris)
            Vec3f g = { e->fxAt[0] * NV_RE1_SCALE, -e->fxAt[1] * NV_RE1_SCALE, -e->fxAt[2] * NV_RE1_SCALE };

            EffectSsHahen_SpawnBurst(play, &g, 8.0f, 0, 6, 6, 40, -1, 10, NULL);
            EffectSsHahen_SpawnBurst(play, &g, 4.0f, 0, 4, 4, 20, -1, 10, NULL);
        } else if (e->fx == 4) { // blood
            EffectSsHitMark_SpawnCustomScale(play, 1, 120, &at);
        } else {
            f32 s = Math_SinS(thisx->shape.rot.y);
            f32 c = Math_CosS(thisx->shape.rot.y);
            Vec3f vel = { s * 6.0f, 0.5f, c * 6.0f };
            Vec3f accel = { 0.0f, 0.1f, 0.0f };

            at.x += s * 15.0f;
            at.z += c * 15.0f;
            EffectSsKFire_Spawn(play, &at, &vel, &accel, 30, 100); // (100: the orange fire)
        }
        e->fx = 0;
    }
    NvEnemy_JointsWorld(this); // its joints where the logic can reach with them, for the next frame
    if (getenv("NV_LOG") && ((play->gameplayFrames % 20) == 0)) {
        printf("NVENEMY f=%d state %d %d/%d/%d anim %d.%d.%d at %d,%d,%d angle %d health %d hit 0x%02x\n",
               (int)play->gameplayFrames, e->state, e->ignore_player_flag, e->action_behavior, e->action_state,
               e->animationId, e->animation_frame_id, e->timing_control, (int)e->t[0], (int)e->t[1], (int)e->t[2], e->angle, e->health, e->hit_state);
        fflush(stdout);
    }

    if (e->headless && !this->wasHeadless) { // the head bursts: a splash of red and the crunch
        Vec3f head = thisx->world.pos;

        this->wasHeadless = true;
        head.y += 10.0f;
        EffectSsHitMark_SpawnCustomScale(play, 2, 400, &head);
        Audio_PlayActorSound2(thisx, NA_SE_EN_DEADHAND_BITE);
    }
    // where RE1 put it, in OoT's terms (RE1 x, -y, -z)
    thisx->world.pos.x = e->t[0] * NV_RE1_SCALE;
    thisx->world.pos.y = -e->t[1] * NV_RE1_SCALE;
    thisx->world.pos.z = -e->t[2] * NV_RE1_SCALE;
    thisx->shape.rot.y = thisx->world.rot.y = (s16)(0x4000 - e->angle * 16);
    thisx->focus.pos = thisx->world.pos;
    thisx->focus.pos.y += 54.0f;

    // a placeholder: held for its script (behaviour bit 7) and still at the room's origin, where
    // the script parks an enemy until its event moves it in (105's zombie that follows Jill back)
    alive = (e->health >= 0) && !(e->status_flags & 2) && !NvEnemy_Parked(e);
    if (NvEnemy_Acts(this->type)) {
        if (alive) {
            thisx->flags |= ACTOR_FLAG_ATTENTION_ENABLED;
        } else {
            thisx->flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        }
    }
    if ((alive || (!NvEnemy_Acts(this->type) && !NvEnemy_Parked(e))) && (thisx != sNvEnemySlots[15])) { // (15: the dev test one)
        Collider_UpdateCylinder(thisx, &this->collider);
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
        if ((NvEnemy_IsZombie(this->type) && (e->state != NV_ZST_ATTACK)) || NvEnemy_IsDog(this->type) || NvEnemy_IsHunter(this->type) ||
            NvEnemy_IsCrow(this->type) || NvEnemy_IsYawn(this->type) || (NvEnemy_IsPlant(this->type) && (e->health >= 0))) {
            CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
        }
    }
}

/** RotMatrix (Rz * Ry * Rx) of RE1 angles (4096 a turn), rows. */
static void NvEnemy_RotMatrix(s16 ax, s16 ay, s16 az, f32 L[3][3]) {
    f32 a = ax * (NV_PI / 2048.0f);
    f32 b = ay * (NV_PI / 2048.0f);
    f32 c = az * (NV_PI / 2048.0f);
    f32 sx = sinf(a), cx = cosf(a), sy = sinf(b), cy = cosf(b), sz = sinf(c), cz = cosf(c);

    L[0][0] = cz * cy;
    L[0][1] = cz * sy * sx - sz * cx;
    L[0][2] = cz * sy * cx + sz * sx;
    L[1][0] = sz * cy;
    L[1][1] = sz * sy * sx + cz * cx;
    L[1][2] = sz * sy * cx - cz * sx;
    L[2][0] = -sy;
    L[2][1] = cy * sx;
    L[2][2] = cy * cx;
}

/** The skeleton posed (Joint_move's angles): per bone its rotation and translation in the entity's space. */
/** A Tyrant limb blown off: its pose when it went, tumbled, carried along its flight. */
static void NvEnemy_PoseLimb(NvEnemy* this, s32 i, s32 p, f32 R[][3][3], f32 T[][3]) {
    static const u8 sJoint[5] = { 2, 4, 7, 10, 12 };
    s32 k;
    s32 r;
    s32 c;

    if (i >= 32) {
        return;
    }
    if ((p >= 0) && (p < 32) && (this->limbBones & (1u << p))) {
        this->limbBones |= 1u << i; // (it goes with its limb)
    }
    for (k = 0; k < 5; k++) {
        f32 S[3][3];
        f32 Rn[3][3];

        if (sJoint[k] != i) {
            continue;
        }
        this->limbBones |= 1u << i;
        if (!this->limbCaught[k]) {
            this->limbCaught[k] = true;
            memcpy(this->limbR[k], R[i], sizeof(this->limbR[k]));
            memcpy(this->limbT[k], T[i], sizeof(this->limbT[k]));
        }
        NvEnemy_RotMatrix(this->e.ty.limbSpin[k][0], this->e.ty.limbSpin[k][1], this->e.ty.limbSpin[k][2], S);
        for (r = 0; r < 3; r++) {
            for (c = 0; c < 3; c++) {
                Rn[r][c] = this->limbR[k][r][0] * S[0][c] + this->limbR[k][r][1] * S[1][c] + this->limbR[k][r][2] * S[2][c];
            }
            T[i][r] = this->limbT[k][r] + this->e.ty.limbOff[k][r];
        }
        memcpy(R[i], Rn, sizeof(Rn));
    }
}

static void NvEnemy_Pose(NvEnemy* this, f32 R[][3][3], f32 T[][3]) {
    NvEmModel* m = this->model;
    s32 n;

    this->limbBones = 0;

    for (n = 0; n < m->numBones; n++) {
        s32 i = m->order[n];
        s16 p = m->bones[i * 4 + 3];
        f32 L[3][3];
        s32 r;
        s32 k;

        NvEnemy_RotMatrix(this->e.rot[i][0], (s16)(this->e.rot[i][1] + this->e.yawExtra[i]), this->e.rot[i][2], L);
        if ((p < 0) || (p >= m->numBones)) {
            for (r = 0; r < 3; r++) {
                for (k = 0; k < 3; k++) {
                    R[i][r][k] = L[r][k];
                }
                T[i][r] = this->e.rootT[r];
            }
        } else {
            for (r = 0; r < 3; r++) {
                for (k = 0; k < 3; k++) {
                    R[i][r][k] = R[p][r][0] * L[0][k] + R[p][r][1] * L[1][k] + R[p][r][2] * L[2][k];
                }
                T[i][r] = T[p][r] + R[p][r][0] * m->bones[i * 4] + R[p][r][1] * m->bones[i * 4 + 1] +
                          R[p][r][2] * m->bones[i * 4 + 2];
            }
        }
           if (NvEnemy_IsTyrant(this->type) && this->e.ty.blownUp) {
            NvEnemy_PoseLimb(this, i, p, R, T);
        }
    }
}

/**
 * Where its joints are now, in RE1's world (e->joint: the entity's place plus its turn, RotMatrix of
 * pitch, facing and roll, applied to each posed joint), for the logic that reaches with a hand, a
 * jaw or a vine's tip (RE1 reads its joints' world matrices the same way).
 */
static void NvEnemy_JointsWorld(NvEnemy* this) {
    NvEmModel* m = this->model;
    f32 R[NV_EM_MAX_BONES][3][3];
    f32 T[NV_EM_MAX_BONES][3];
    f32 M[3][3];
    s32 i;

    if (m == NULL) {
        return;
    }
    NvEnemy_Pose(this, R, T);
    NvEnemy_RotMatrix(this->e.pitch, this->e.angle, this->e.roll, M);
    for (i = 0; (i < m->numBones) && (i < NV_RE1_JOINTS); i++) {
        s32 r;

        for (r = 0; r < 3; r++) {
            this->e.joint[i][r] = this->e.t[r] + (s32)(M[r][0] * T[i][0] + M[r][1] * T[i][1] + M[r][2] * T[i][2]);
        }
    }
    this->e.numJoints = (u8)MIN(m->numBones, NV_RE1_JOINTS);
}

void NvEnemy_Draw(Actor* thisx, PlayState* play) {
    NvEnemy* this = (NvEnemy*)thisx;
    NvEmModel* m = this->model;
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    // per bone, in RE1's space: rotation (3x3, column vectors) and translation
    f32 R[NV_EM_MAX_BONES][3][3];
    f32 T[NV_EM_MAX_BONES][3];
    f32 base[3][3];
    f32 M[3][3];
    s32 n;
    u8 tint = 255;

    if ((m == NULL) || NvDoorScene_Covering() || NvEnemy_Parked(&this->e)) {
        return;
    }
    if (thisx->colorFilterTimer != 0) {
        tint = 80;
    }
    // the entity: RotMatrix(pitch, facing, roll), then to OoT's axes and scale (x, -y, -z)
    NvEnemy_RotMatrix(this->e.pitch, this->e.angle, this->e.roll, M);
    for (n = 0; n < 3; n++) {
        base[0][n] = M[0][n] * NV_RE1_SCALE;
        base[1][n] = -M[1][n] * NV_RE1_SCALE;
        base[2][n] = -M[2][n] * NV_RE1_SCALE;
    }
    NvEnemy_Pose(this, R, T);

    OPEN_DISPS(gfxCtx);
    // point sampled and unlit, as RE1's door scenes are here; colour 0 (alpha 0) is see-through
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCycleType(POLY_OPA_DISP++, G_CYC_1CYCLE);
    gDPSetDepthSource(POLY_OPA_DISP++, G_ZS_PIXEL);
    gDPSetRenderMode(POLY_OPA_DISP++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
    gDPSetTextureFilter(POLY_OPA_DISP++, G_TF_POINT);
    gDPSetTexturePersp(POLY_OPA_DISP++, G_TP_PERSP);
    gDPSetTextureLUT(POLY_OPA_DISP++, G_TT_NONE);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, tint, tint, 255);
    gSPClearGeometryMode(POLY_OPA_DISP++, G_LIGHTING | G_FOG | G_CULL_BOTH | G_TEXTURE_GEN);
    gSPSetGeometryMode(POLY_OPA_DISP++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gSPTexture(POLY_OPA_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);

    for (n = 0; n < m->numBones; n++) {
        MtxF mf;
        Mtx* mtx;
        s32 r;
        s32 k;
        s32 b;

        if ((m->numBatches[n] == 0) || (this->e.headless && (n == NV_ZOMBIE_HEAD_BONE)) ||
            (NvEnemy_IsTyrant(this->type) && this->e.ty.blownUp && !(this->limbBones & (1u << n))) ||
            (NvEnemy_IsWeb(this->type) && (n < 8) && (this->e.webGone & (1u << n)))) {
            continue;
        }
        // OoT = pos + base * (R p + T); mf[column][row]
        for (r = 0; r < 3; r++) {
            for (k = 0; k < 3; k++) {
                mf.mf[k][r] = base[r][0] * R[n][0][k] + base[r][1] * R[n][1][k] + base[r][2] * R[n][2][k];
            }
            mf.mf[3][r] = base[r][0] * T[n][0] + base[r][1] * T[n][1] + base[r][2] * T[n][2];
            mf.mf[r][3] = 0.0f;
        }
        mf.mf[3][0] += thisx->world.pos.x;
        mf.mf[3][1] += thisx->world.pos.y;
        mf.mf[3][2] += thisx->world.pos.z;
        mf.mf[3][3] = 1.0f;
        mtx = Graph_Alloc(gfxCtx, sizeof(Mtx));
        Matrix_MtxFToMtx(&mf, mtx);
        gSPMatrix(POLY_OPA_DISP++, mtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

        for (b = 0; b < m->numBatches[n]; b++) {
            NvEmBatch* batch = &m->batches[m->firstBatch[n] + b];
            s32 v;

            gDPLoadTextureBlock(POLY_OPA_DISP++, m->cells + batch->cell * NV_EM_CELL_W * NV_EM_CELL_H, G_IM_FMT_RGBA,
                                G_IM_SIZ_16b, NV_EM_CELL_W, NV_EM_CELL_H, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                                G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
            for (v = 0; v + 3 <= batch->numVerts; v += 30) {
                s32 cnt = MIN(30, batch->numVerts - v);
                s32 q;

                gSPVertex(POLY_OPA_DISP++, batch->vtx + v, cnt, 0);
                for (q = 0; q + 3 <= cnt; q += 3) {
                    gSP1Triangle(POLY_OPA_DISP++, q, q + 1, q + 2, 0);
                }
            }
        }
    }
    // the Tyrant's exposed heart (its model's mesh past the skeleton's): on joint 1 at (312, -760,
    // -287), beating (tyrant_draw_heart); gone with the body when the rocket blows it apart
    if (NvEnemy_IsTyrant(this->type) && (m->numExtra >= 1) && !this->e.ty.blownUp && (m->numBones > 1)) {
        static const f32 sOff[3] = { 312.0f, -760.0f, -287.0f };
        f32 sc = (this->e.ty.heartScale + 500) / 4096.0f;
        f32 Th[3];
        MtxF mf;
        Mtx* mtx;
        s32 r;
        s32 k;
        s32 b;

        for (r = 0; r < 3; r++) {
            Th[r] = T[1][r] + R[1][r][0] * sOff[0] + R[1][r][1] * sOff[1] + R[1][r][2] * sOff[2];
        }
        for (r = 0; r < 3; r++) {
            for (k = 0; k < 3; k++) {
                mf.mf[k][r] = (base[r][0] * R[1][0][k] + base[r][1] * R[1][1][k] + base[r][2] * R[1][2][k]) * sc;
            }
            mf.mf[3][r] = base[r][0] * Th[0] + base[r][1] * Th[1] + base[r][2] * Th[2];
            mf.mf[r][3] = 0.0f;
        }
        mf.mf[3][0] += thisx->world.pos.x;
        mf.mf[3][1] += thisx->world.pos.y;
        mf.mf[3][2] += thisx->world.pos.z;
        mf.mf[3][3] = 1.0f;
        mtx = Graph_Alloc(gfxCtx, sizeof(Mtx));
        Matrix_MtxFToMtx(&mf, mtx);
        gSPMatrix(POLY_OPA_DISP++, mtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        for (b = 0; b < m->extraNumBatches[0]; b++) {
            NvEmBatch* batch = &m->batches[m->extraFirstBatch[0] + b];
            s32 v;

            gDPLoadTextureBlock(POLY_OPA_DISP++, m->cells + batch->cell * NV_EM_CELL_W * NV_EM_CELL_H,
                                G_IM_FMT_RGBA, G_IM_SIZ_16b, NV_EM_CELL_W, NV_EM_CELL_H, 0,
                                G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                                G_TX_NOLOD, G_TX_NOLOD);
            for (v = 0; v + 3 <= batch->numVerts; v += 30) {
                s32 cnt = MIN(30, batch->numVerts - v);
                s32 q;

                gSPVertex(POLY_OPA_DISP++, batch->vtx + v, cnt, 0);
                for (q = 0; q + 3 <= cnt; q += 3) {
                    gSP1Triangle(POLY_OPA_DISP++, q, q + 1, q + 2, 0);
                }
            }
        }
    }
    // Plant 42's root ball and flower body (its model's two meshes past the vine's: RE1's copies
    // of the plant, drawn at its place; their own sway and the fallen core's pose left out)
    if (NvEnemy_IsPlant42(this->type) && !(this->e.behavior_flags & 1) && (m->numExtra >= 2) &&
        !(this->e.status_flags & 8)) {
        MtxF mf;
        Mtx* mtx;
        s32 r;
        s32 k;
        s32 x;

        for (r = 0; r < 3; r++) {
            for (k = 0; k < 3; k++) {
                mf.mf[k][r] = base[r][k];
            }
            mf.mf[3][r] = 0.0f;
            mf.mf[r][3] = 0.0f;
        }
        mf.mf[3][0] = thisx->world.pos.x;
        mf.mf[3][1] = thisx->world.pos.y;
        mf.mf[3][2] = thisx->world.pos.z;
        mf.mf[3][3] = 1.0f;
        mtx = Graph_Alloc(gfxCtx, sizeof(Mtx));
        Matrix_MtxFToMtx(&mf, mtx);
        gSPMatrix(POLY_OPA_DISP++, mtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        for (x = 0; x < 2; x++) {
            s32 b;

            for (b = 0; b < m->extraNumBatches[x]; b++) {
                NvEmBatch* batch = &m->batches[m->extraFirstBatch[x] + b];
                s32 v;

                gDPLoadTextureBlock(POLY_OPA_DISP++, m->cells + batch->cell * NV_EM_CELL_W * NV_EM_CELL_H,
                                    G_IM_FMT_RGBA, G_IM_SIZ_16b, NV_EM_CELL_W, NV_EM_CELL_H, 0,
                                    G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                                    G_TX_NOLOD, G_TX_NOLOD);
                for (v = 0; v + 3 <= batch->numVerts; v += 30) {
                    s32 cnt = MIN(30, batch->numVerts - v);
                    s32 q;

                    gSPVertex(POLY_OPA_DISP++, batch->vtx + v, cnt, 0);
                    for (q = 0; q + 3 <= cnt; q += 3) {
                        gSP1Triangle(POLY_OPA_DISP++, q, q + 1, q + 2, 0);
                    }
                }
            }
        }
    }
    CLOSE_DISPS(gfxCtx);
}

/* ---- the script's side ---- */

void NvEnemy_ClearSlots(void) {
    memset(sNvEnemySlots, 0, sizeof(sNvEnemySlots));
}

/** cmd_enemy_set: place one (its 22 bytes), unless its death flag is up or its slot is taken. */
void NvEnemy_ScdSet(PlayState* play, const u8* e) {
    s32 slot = e[0x12] & 0xF;
    s16 y;
    u16 angle;
    u16 x;
    u16 z;

    if (((e[3] != 0xFF) && NvFlags_Test(3, e[3])) || (sNvEnemySlots[slot] != NULL)) {
        return;
    }
    if (!(NvEnemy_Acts(e[1]) || NvEnemy_IsCorpse(e[1]) || NvEnemy_IsNpc(e[1]))) {
        return; // (the other types aren't ported)
    }
    memcpy(&angle, e + 8, 2);
    memcpy(&x, e + 12, 2);
    memcpy(&y, e + 14, 2);
    memcpy(&z, e + 16, 2);
    // (RE1's x and z are unsigned: rooms run past 32767) rot: behaviour, angle, death flag
    sNvEnemySpawnSndGroup = (e[0x15] >> 4) & 7; // its sound group (cmd_enemy_set), for NvEnemy_Init
    sNvEnemySpawnAnim = e[0x13];                // and its starting pose (a character's: its animation, frame)
    sNvEnemySpawnFrame = e[0x14];
    sNvEnemySlots[slot] = Actor_Spawn(&play->actorCtx, play, NvActor_EnemyId(), x * NV_RE1_SCALE, -y * NV_RE1_SCALE,
                                      -z * NV_RE1_SCALE, e[2], angle & 0xFFF, e[3], e[1]);
}

/** cmd_enemy_pos_set: put one somewhere (rooms place enemies by script). */
void NvEnemy_ScdPos(s32 slot, s16 angle, s16 x, s16 y, s16 z) {
    NvEnemy* this;

    if ((slot >= NV_ENEMY_SLOTS) || (sNvEnemySlots[slot] == NULL)) {
        return;
    }
    this = (NvEnemy*)sNvEnemySlots[slot];
    this->e.angle = angle;
    this->e.t[0] = x; // (signed here, as cmd_enemy_pos_set has it)
    this->e.t[1] = y;
    this->e.t[2] = z;
    this->e.pos[0] = x;
    this->e.pos[1] = y;
    this->e.pos[2] = z;
}

/** cmd_enemy_flags_get: the slot's behaviour flags, for the scripts' state byte 10. */
s32 NvEnemy_SlotFlags(s32 slot) {
    if ((slot < 0) || (slot >= NV_ENEMY_SLOTS) || (sNvEnemySlots[slot] == NULL)) {
        return 0;
    }
    return ((NvEnemy*)sNvEnemySlots[slot])->e.behavior_flags;
}

/**
 * cmd_enemy_prop_set: 0 the behaviour byte, 1 hurt it (its damaged state with this health and hit
 * state: the scripts' own shots), 2 the action, 3 its status bits, 5 its facing, 6 no blend into
 * the next pose, 8 the follow state, 10 the action's step (9, its joints' visibility, isn't run).
 */
void NvEnemy_ScdProp(s32 slot, s32 subCmd, u16 param, u8 hit) {
    NvEnemy* this;

    if ((slot >= NV_ENEMY_SLOTS) || (sNvEnemySlots[slot] == NULL)) {
        return;
    }
    this = (NvEnemy*)sNvEnemySlots[slot];
    switch (subCmd) {
        case 0:
            this->e.behavior_flags = param & 0xFF;
            break;
        case 1:
            this->e.state = 2;
            this->e.ignore_player_flag = 0;
            this->e.action_behavior = 0;
            this->e.action_state = 0;
            this->e.health = (s16)param;
            this->e.hit_state = hit;
            break;
        case 2:
            this->e.action_behavior = param & 0xFF;
            this->e.action_state = 0;
            break;
        case 3:
            if ((param >> 8) == 0) {
                this->e.status_flags = param & 0xFF;
            } else if ((param >> 8) == 1) {
                this->e.status_flags |= param & 0xFF;
            } else if ((param >> 8) == 2) {
                this->e.status_flags ^= param & 0xFF;
            }
            break;
        case 5:
            this->e.angle = param & 0xFFF;
            break;
        case 6:
            this->e.blend_counter = 0;
            break;
        case 8:
            this->e.state = 9;
            this->e.ignore_player_flag = 0;
            this->e.action_behavior = 0;
            this->e.action_state = 0;
            break;
        case 10:
            this->e.action_state = param & 0xFF;
            break;
    }
    printf("NVENEMY slot %d prop %d = %d\n", (int)slot, (int)subCmd, (int)param);
    fflush(stdout);
}

const size_t gNvEnemySize = sizeof(NvEnemy);

/** The entity in an enemy slot (an event script's character 1, n), or NULL. */
NvRe1Entity* NvEnemy_SlotEntity(s32 slot) {
    if ((slot < 0) || (slot >= NV_ENEMY_SLOTS) || (sNvEnemySlots[slot] == NULL)) {
        return NULL;
    }
    return &((NvEnemy*)sNvEnemySlots[slot])->e;
}

/** An enemy slot's place and facing in RE1's units (an effect attached to it); false if empty. */
s32 NvEnemy_Place(s32 slot, s32* t, s16* angle) {
    NvRe1Entity* e = NvEnemy_SlotEntity(slot);

    if (e == NULL) {
        return false;
    }
    t[0] = e->t[0];
    t[1] = e->t[1];
    t[2] = e->t[2];
    *angle = e->angle;
    return true;
}

/* ---- the player in a scripted scene ---- */

// RE1's player entity as the event scripts drive her (character 0): state 8 and a behaviour
// (z_nv_npc.inc), which walk Link; state 1 is her own control, and the stand-in follows Link.
static NvRe1Entity sNvCastPlayer;
static u8 sNvCastPlaced; // the stand-in is where Link is (or where a script put her) since the room loaded
s32 gNvCastHolding; // the cast holds Link: OoT doesn't freeze the enemies for it (z_actor.c)
static u8 sNvCastHeld;
static u8 sNvCastPhase;
static u16 sNvCastHeldFrames;
// a scene holding her this long is waiting on something not ported yet: she's let go
#define NV_CAST_HOLD_MAX (60 * 60)

NvRe1Entity* NvCast_Player(void) {
    return &sNvCastPlayer;
}

/** The room's start: she's her own again. */
void NvCast_RoomLoad(void) {
    memset(&sNvCastPlayer, 0, sizeof(sNvCastPlayer));
    sNvCastPlayer.state = 1;
    sNvCastPlayer.id = 0;
    sNvCastPlayer.sca = sNvJillSca;
    sNvCastHeld = false;
    gNvCastHolding = false;
    sNvCastHeldFrames = 0;
    sNvCastPlaced = false;
}

/** A script put her somewhere (cmd_player_pos_set): the stand-in starts from there. */
void NvCast_Placed(void) {
    sNvCastPlaced = true;
}

/** Every frame (NvScript_Update): Link walked by the script while it holds her, else followed. */
/*
 * The scenes' player animations (cmd 0x84's 0x10-0x3D: player_scd_behavior_01 plays them from the
 * room's own set, RDT+0x6C/0x70): the stand-in takes its timing from them, so a scene waits as long
 * as RE1's does, and Link plays the same one on his own skeleton (z_nv_linkanim.c), held at the
 * stand-in's step. NvCast_SetRoomSet swaps in another set for a while (10F's wrong notes: Chris's).
 */
const char* NvScript_RoomPlayerPath(s32 chris);
s32 NvLinkAnim_PlayFrom(const char* path, s32 skip, s32 anim, s32 hold, s32 mirror);
void NvLinkAnim_Stop(void);
void NvLinkAnim_Pin(s32 step);

static NvRe1Anim sNvCastBank;
static char sNvCastBankPath[40];
static s32 sNvCastBankOk;
static const char* sNvCastSetOverride; // another set's path (NULL: the room's own)
static s32 sNvCastShown = -1;          // the animation Link is playing for the scene (-1: none)
static char sNvCastShownPath[40];

void NvCast_SetRoomSet(const char* path) {
    sNvCastSetOverride = path;
}

static const char* NvCast_SetPath(void) {
    return (sNvCastSetOverride != NULL) ? sNvCastSetOverride : NvScript_RoomPlayerPath(false);
}

/** The set as RE1's animation bank ("RPLA": bones, then frames and the list), or NULL. */
static const NvRe1Anim* NvCast_Bank(const char* path) {
    const u8* d;
    u16 n, nf, na;
    s32 i;

    if (path == NULL) {
        return NULL;
    }
    if (strcmp(path, sNvCastBankPath) == 0) {
        return sNvCastBankOk ? &sNvCastBank : NULL;
    }
    snprintf(sNvCastBankPath, sizeof(sNvCastBankPath), "%s", path);
    sNvCastBankOk = false;
    memset(&sNvCastBank, 0, sizeof(sNvCastBank));
    d = (const u8*)ResourceGetDataByName(path);
    if ((d == NULL) || (memcmp(d, "RPLA", 4) != 0)) {
        return NULL;
    }
    memcpy(&n, d + 4, 2);
    d += 6 + n * 8;
    memcpy(&nf, d, 2);
    d += 2;
    sNvCastBank.numJoints = MIN(n, NV_RE1_JOINTS);
    sNvCastBank.frameShorts = 6 + 3 * n;
    sNvCastBank.numFrames = nf;
    sNvCastBank.frames = (const s16*)d;
    d += nf * sNvCastBank.frameShorts * 2;
    memcpy(&na, d, 2);
    d += 2;
    sNvCastBank.numAnims = MIN(na, 64);
    for (i = 0; i < sNvCastBank.numAnims; i++) {
        u16 ns;

        memcpy(&ns, d, 2);
        d += 2;
        sNvCastBank.numSteps[i] = ns;
        sNvCastBank.steps[i] = (const u16*)d;
        d += ns * 4;
    }
    sNvCastBankOk = true;
    return &sNvCastBank;
}

s32 NvWeapon_EquippedItem(void);

/*
 * Where a scene's animation comes from (player_scd_behavior_01): 0x10-0x3D the room's set, at that
 * number; below 0x10 g_ScdAnimRemap's pairs, 0-4 Jill's body set (EM1021's own animations) and
 * 5-9 her hand set (the weapon in her hand, unarmed W10), each at 0-4; 0x0A-0x0F the body's 0.
 * The file, and the animation in it (*src); NULL for one it has no set for (0x3E and up: the
 * model's own section, which Jill's has none of).
 */
static const char* NvCast_ScenePath(s32 anim, s32* src) {
    static const char* sHand[11] = { "re1/player/w10", "re1/player/w11", "re1/player/w12", "re1/player/w13",
                                     "re1/player/w14", "re1/player/w14", "re1/player/w15", "re1/player/w16",
                                     "re1/player/w16", "re1/player/w16", "re1/player/w17" };
    s32 item;

    *src = anim;
    if ((anim >= 0x10) && (anim < 0x3E)) {
        return NvCast_SetPath();
    }
    if ((anim < 0) || (anim >= 0x10)) {
        return NULL;
    }
    if ((anim >= 5) && (anim < 10)) {
        *src = anim - 5;
        item = NvWeapon_EquippedItem();
        return sHand[((item >= 0) && (item < 11)) ? item : 0];
    }
    *src = (anim < 5) ? anim : 0;
    return "re1/player/b1";
}

/** The bank for the stand-in's behaviour now: the set when it's playing one of the set's animations. */
static const NvRe1Anim* NvCast_SceneBank(NvRe1Entity* e) {
    const NvRe1Anim* b;
    const char* path;
    s32 src;

    if ((e->state != 8) || (e->action_behavior != 1) || getenv("NV_NOSCENEANIM")) {
        return NULL;
    }
    path = NvCast_ScenePath(e->animationId, &src);
    b = NvCast_Bank(path);
    if ((b == NULL) || (src >= b->numAnims) || (b->numSteps[src] == 0) || (e->animationId >= 64)) {
        return NULL;
    }
    if (src != e->animationId) { // (the stand-in steps it at the script's number: the remapped one there)
        sNvCastBank.steps[e->animationId] = sNvCastBank.steps[src];
        sNvCastBank.numSteps[e->animationId] = sNvCastBank.numSteps[src];
        sNvCastBank.numAnims = MAX(sNvCastBank.numAnims, e->animationId + 1);
    }
    return b;
}

/** Link shows the stand-in's scene animation, at its step; stops when it has none. */
static void NvCast_ShowScene(NvRe1Entity* e) {
    s32 src;
    const char* path = NvCast_ScenePath(e->animationId, &src);

    if ((e->anim != NULL) && (path != NULL)) {
        if ((sNvCastShown != e->animationId) || (strcmp(path, sNvCastShownPath) != 0)) {
            sNvCastShown = NvLinkAnim_PlayFrom(path, 0, src, true, false) ? e->animationId : -1;
            snprintf(sNvCastShownPath, sizeof(sNvCastShownPath), "%s", path);
            if (getenv("NV_LOG")) {
                printf("NVCAST Link plays %s animation 0x%02X\n", path, src);
                fflush(stdout);
            }
        }
        NvLinkAnim_Pin(e->animation_frame_id);
    } else if (sNvCastShown >= 0) {
        NvLinkAnim_Stop();
        sNvCastShown = -1;
    }
}

void NvCast_Update(PlayState* play, s32 eventsLive) {
    Player* player = GET_PLAYER(play);
    NvRe1Entity* e = &sNvCastPlayer;
    s32 ticks;
    s32 t;

    if (player == NULL) {
        return;
    }
    if ((e->state == 8) && (!eventsLive || (sNvCastHeldFrames > NV_CAST_HOLD_MAX))) {
        if (eventsLive) {
            printf("NVCAST the scene held her too long: let go\n");
            fflush(stdout);
        }
        e->state = 1; // (the scene ended with her still held: she's let go)
    }
    if (e->state != 8) {
        if (sNvCastShown >= 0) {
            NvLinkAnim_Stop();
            sNvCastShown = -1;
        }
        e->anim = NULL;
        sNvCastHeldFrames = 0;
        if (sNvCastHeld) {
            sNvCastHeld = false;
            gNvCastHolding = false;
            Player_SetCsAction(play, NULL, 7);
            Interface_ChangeHudVisibilityMode(HUD_VISIBILITY_ALL); // (the scene's over: the HUD back)
        }
        e->t[0] = (s32)lroundf(player->actor.world.pos.x / NV_RE1_SCALE);
        e->t[1] = (s32)lroundf(-player->actor.world.pos.y / NV_RE1_SCALE);
        e->t[2] = (s32)lroundf(-player->actor.world.pos.z / NV_RE1_SCALE);
        e->angle = ((0x4000 - (s32)(u16)player->actor.shape.rot.y) / 16) & 0xFFF;
        sNvCastPlaced = true;
        return;
    }
    if (!sNvCastPlaced) { // (a scene taking her as the room starts: from where Link is, not the room's 0,0)
        e->t[0] = (s32)lroundf(player->actor.world.pos.x / NV_RE1_SCALE);
        e->t[1] = (s32)lroundf(-player->actor.world.pos.y / NV_RE1_SCALE);
        e->t[2] = (s32)lroundf(-player->actor.world.pos.z / NV_RE1_SCALE);
        e->angle = ((0x4000 - (s32)(u16)player->actor.shape.rot.y) / 16) & 0xFFF;
        sNvCastPlaced = true;
    }
    if (!sNvCastHeld) {
        sNvCastHeld = true;
        gNvCastHolding = true;
        Player_SetCsAction(play, NULL, 8);
    }
    sNvCastHeldFrames++;
    // RE1 has no HUD: a scene that holds her plays without OoT's, as OoT's own cutscenes do; the
    // ocarina's modes keep theirs (the piano's staff and its "Stop")
    if ((play->msgCtx.msgMode < MSGMODE_OCARINA_STARTING) || (play->msgCtx.msgMode >= MSGMODE_TEXT_AWAIT_NEXT)) {
        Interface_ChangeHudVisibilityMode(HUD_VISIBILITY_NOTHING);
    }
    {
        s32 x0 = e->t[0];
        s32 z0 = e->t[2];
        f32 step;
        LinkAnimationHeader* want;

        ticks = (sNvCastPhase ^= 1) ? 1 : 2;
        for (t = 0; t < ticks; t++) {
            NvRe1_TickBegin();
            e->anim = NvCast_SceneBank(e); // (RE1's timing for a scene's animation; else the fixed hold)
            NvNpc_PlayerTick(e);
        }
        e->anim = NvCast_SceneBank(e);
        NvCast_ShowScene(e);
        if (getenv("NV_LOG") && ((sNvCastHeldFrames % 20) == 1)) {
            printf("NVCASTP held %d beh %d/%d at %d,%d target %d,%d flag %02X angle %d\n", (int)sNvCastHeldFrames,
                   e->action_behavior, e->action_state, (int)e->t[0], (int)e->t[2], e->unk_c6, e->unk_c8,
                   e->scd_anim_param, e->angle);
            fflush(stdout);
        }
        // Link moves as the script moves her (player_scd_behavior_02..06), with OoT's nearest
        // animation at about her pace: 2 walks, 3 runs, 4 and 5 step back (her back to the
        // target), and the turns on the spot (2 and 3 before they set off, 6) step as RE1's do,
        // on her walk cycle; standing once she stops (the hold loops whatever he's playing)
        step = sqrtf(SQ((f32)(e->t[0] - x0)) + SQ((f32)(e->t[2] - z0))) * NV_RE1_SCALE;
        {
            s32 beh = e->action_behavior;
            s32 turning = (e->state == 8) && (e->action_state == 1) && ((beh == 2) || (beh == 3) || (beh == 6));
            f32 speed;

            if ((step > 0.3f) && ((beh == 4) || (beh == 5))) {
                want = (LinkAnimationHeader*)gPlayerAnim_link_normal_back_walk;
                speed = CLAMP(step / 2.0f, 0.5f, 2.0f);
            } else if ((step > 0.3f) && (beh == 3)) {
                want = (LinkAnimationHeader*)gPlayerAnim_link_normal_run;
                speed = CLAMP(step / 6.0f, 0.7f, 1.5f);
            } else if ((step > 0.3f) || turning) {
                want = (LinkAnimationHeader*)gPlayerAnim_link_normal_walk;
                speed = (step > 0.3f) ? CLAMP(step / 3.0f, 0.5f, 2.0f) : 0.6f;
            } else {
                want = (LinkAnimationHeader*)gPlayerAnim_link_normal_wait;
                speed = 1.0f;
            }
            if ((sNvCastHeldFrames > 2) && (player->skelAnime.animation != want)) {
                LinkAnimation_Change(play, &player->skelAnime, want, 1.0f, 0.0f, Animation_GetLastFrame(want),
                                     ANIMMODE_LOOP, -4.0f);
            }
            player->skelAnime.playSpeed = speed;
        }
    }
    player->actor.world.pos.x = e->t[0] * NV_RE1_SCALE;
    player->actor.world.pos.z = -e->t[2] * NV_RE1_SCALE;
    player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw = (s16)(0x4000 - e->angle * 16);
}

/** Testing (NV_TEST_ENEMY): the slot's enemy goes, so the test one can take its place. */
void NvEnemy_DevFree(s32 slot) {
    if ((slot >= 0) && (slot < NV_ENEMY_SLOTS) && (sNvEnemySlots[slot] != NULL)) {
        Actor_Kill(sNvEnemySlots[slot]);
        sNvEnemySlots[slot] = NULL;
    }
}
