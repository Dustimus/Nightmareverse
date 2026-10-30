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

static s32 NvEnemy_IsCorpse(s32 type) {
    return (type == 37) || (type == 38) || (type == 41);
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
    if ((d == NULL) || (memcmp(d, "RENM", 4) != 0)) {
        return NULL;
    }
    memset(m, 0, sizeof(*m));
    m->numBonesAll = *(u16*)(d + 4);
    m->numBones = MIN(m->numBonesAll, NV_EM_MAX_BONES);
    numBanks = *(u16*)(d + 6);
    m->bones = (s16*)(d + 8);
    d += 8 + 8 * m->numBonesAll;
    for (b = 0; b < numBanks; b++) {
        s32 frameSize = 2 * (6 + 3 * m->numBonesAll);
        s32 numFrames = *(u16*)d;
        s32 numAnims;
        s32 a;

        if (b == 0) {
            m->bank.numFrames = numFrames;
            m->bank.frames = (s16*)(d + 2);
            m->bank.frameShorts = 6 + 3 * m->numBonesAll;
            m->bank.numJoints = m->numBones;
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

// Jill's collision volume (g_scaJillData): radius 372, half height 1350
static const s16 sNvJillSca[8] = { (s16)0x8000, 0, (s16)0xFABA, 0, 0x0546, 0x0174, 0, 0 };
// what cmd_enemy_set gives every entity before its own init: Chris's (g_scaDataTable[0]), r 422
static const s16 sNvChrisSca[8] = { (s16)0x8000, 0, (s16)0xFA06, 0, 0x05FA, 0x01A6, 0, 0 };

#define NV_RE1_HANDGUN_DAMAGE 9 // RE1's handgun on a zombie; a point of OoT damage is one shot
#define NV_BITE_HEARTS -8       // half a heart a bite (mod1's)

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
    p->health = (gSaveContext.health > 0) ? 0x7FFF : -1;
    p->attackTimer = (player->meleeWeaponState != 0) ? 1 : 0;
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
        // the attack is over: let him go (Player_Action_8084F308's mash count)
        if (player->stateFlags2 & PLAYER_STATE2_GRABBED_BY_ENEMY) {
            player->av2.actionVar2 = 101;
        }
        this->grabbing = false;
        gNvRe1Player.isBeingAttackedFlag = 0;
        return;
    }
    // RE1 decides when it ends: OoT's own mashing out stays off, RE1's mashing counts instead
    player->av2.actionVar2 = 0;
    // the player stands at RE1's grab point. is_facing_toward_entity is true when she faces the way
    // the zombie does, so directions 1 and 3 take her from behind, 0 and 2 face to face
    player->actor.world.pos.x = e->unk_c6 * NV_RE1_SCALE;
    player->actor.world.pos.z = -e->unk_c8 * NV_RE1_SCALE;
    player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw =
        (s16)(0x4000 - e->angle * 16) + ((e->attacking_direction & 1) ? 0 : 0x8000);
    while (gNvRe1BiteDamage > 0) {
        gNvRe1BiteDamage -= 10;
        play->damagePlayer(play, NV_BITE_HEARTS);
        Rumble_Request(this->actor.xzDistToPlayer, 240, 1, 12);
        Player_PlaySfx(&player->actor, NA_SE_VO_LI_DAMAGE_S + player->ageProperties->unk_92);
    }
}

/* ---- the actor ---- */

void NvEnemy_Init(Actor* thisx, PlayState* play) {
    NvEnemy* this = (NvEnemy*)thisx;
    NvRe1Entity* e = &this->e;

    this->type = thisx->params & 0xFF;
    this->model = NvEnemy_Model(this->type);
    memset(e, 0, sizeof(*e));
    e->id = this->type;
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
    } else {
        for (t = 0; t < ticks; t++) {
            NvRe1_JointMove(e, 0, 0x400);
        }
    }
    if (getenv("NV_LOG") && ((play->gameplayFrames % 20) == 0)) {
        printf("NVENEMY f=%d state %d %d/%d/%d anim %d at %d,%d angle %d health %d hit 0x%02x\n",
               (int)play->gameplayFrames, e->state, e->ignore_player_flag, e->action_behavior, e->action_state,
               e->animationId, (int)e->t[0], (int)e->t[2], e->angle, e->health, e->hit_state);
        fflush(stdout);
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
    if (NvEnemy_IsZombie(this->type)) {
        if (alive) {
            thisx->flags |= ACTOR_FLAG_ATTENTION_ENABLED;
        } else {
            thisx->flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        }
    }
    if ((alive || (!NvEnemy_IsZombie(this->type) && !NvEnemy_Parked(e))) && (thisx != sNvEnemySlots[15])) { // (15: the dev test one)
        Collider_UpdateCylinder(thisx, &this->collider);
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
        if (NvEnemy_IsZombie(this->type) && (e->state != NV_ZST_ATTACK)) {
            CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
        }
    }
}

void NvEnemy_Draw(Actor* thisx, PlayState* play) {
    NvEnemy* this = (NvEnemy*)thisx;
    NvEmModel* m = this->model;
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    // per bone, in RE1's space: rotation (3x3, column vectors) and translation
    f32 R[NV_EM_MAX_BONES][3][3];
    f32 T[NV_EM_MAX_BONES][3];
    f32 base[3][3];
    f32 ca;
    f32 sa;
    s32 n;
    u8 tint = 255;

    if ((m == NULL) || NvDoorScene_Covering() || NvEnemy_Parked(&this->e)) {
        return;
    }
    if (thisx->colorFilterTimer != 0) {
        tint = 80;
    }
    // the entity: RotMatrixY(angle), then to OoT's axes and scale (x, -y, -z)
    ca = cosf(this->e.angle * (NV_PI / 2048.0f));
    sa = sinf(this->e.angle * (NV_PI / 2048.0f));
    base[0][0] = ca * NV_RE1_SCALE;
    base[0][1] = 0.0f;
    base[0][2] = sa * NV_RE1_SCALE;
    base[1][0] = 0.0f;
    base[1][1] = -NV_RE1_SCALE;
    base[1][2] = 0.0f;
    base[2][0] = sa * NV_RE1_SCALE; // -(-sa)
    base[2][1] = 0.0f;
    base[2][2] = -ca * NV_RE1_SCALE;

    for (n = 0; n < m->numBones; n++) {
        s32 i = m->order[n];
        s16 p = m->bones[i * 4 + 3];
        f32 a = this->e.rot[i][0] * (NV_PI / 2048.0f);
        f32 b = this->e.rot[i][1] * (NV_PI / 2048.0f);
        f32 c = this->e.rot[i][2] * (NV_PI / 2048.0f);
        f32 sx = sinf(a), cx = cosf(a), sy = sinf(b), cy = cosf(b), sz = sinf(c), cz = cosf(c);
        f32 L[3][3];
        s32 r;
        s32 k;

        // RotMatrix: Rz * Ry * Rx
        L[0][0] = cz * cy;
        L[0][1] = cz * sy * sx - sz * cx;
        L[0][2] = cz * sy * cx + sz * sx;
        L[1][0] = sz * cy;
        L[1][1] = sz * sy * sx + cz * cx;
        L[1][2] = sz * sy * cx - cz * sx;
        L[2][0] = -sy;
        L[2][1] = cy * sx;
        L[2][2] = cy * cx;

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
    }

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

        if (m->numBatches[n] == 0) {
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
    if (!(NvEnemy_IsZombie(e[1]) || NvEnemy_IsCorpse(e[1]))) {
        return; // (the other types aren't ported)
    }
    memcpy(&angle, e + 8, 2);
    memcpy(&x, e + 12, 2);
    memcpy(&y, e + 14, 2);
    memcpy(&z, e + 16, 2);
    // (RE1's x and z are unsigned: rooms run past 32767) rot: behaviour, angle, death flag
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

/** cmd_enemy_prop_set: 0 the behaviour byte, 2 the action (the others aren't run yet). */
void NvEnemy_ScdProp(s32 slot, s32 subCmd, u16 param) {
    NvEnemy* this;

    if ((slot >= NV_ENEMY_SLOTS) || (sNvEnemySlots[slot] == NULL)) {
        return;
    }
    this = (NvEnemy*)sNvEnemySlots[slot];
    if (subCmd == 0) {
        this->e.behavior_flags = param & 0xFF;
    } else if (subCmd == 2) {
        this->e.action_behavior = param & 0xFF;
        this->e.action_state = 0;
    }
    printf("NVENEMY slot %d prop %d = %d\n", (int)slot, (int)subCmd, (int)param);
    fflush(stdout);
}

const size_t gNvEnemySize = sizeof(NvEnemy);
