/*
 * Nightmareverse: RE1's enemies.
 *
 * A room's enemies are what its init script's Enemy_set instructions place (SCD 0x1B,
 * cmd_enemy_set 0x004617d0; the scripts run in z_nv_script.c), in RE1's enemy slots. Each becomes
 * an Nv_Enemy actor (registered with the ActorDB by NvActors.cpp), drawn with RE1's own model and
 * animations, "re1/enemies/em<type>" (re1/tools/soh_enemy.py).
 *
 * The zombie (types 0, 1 and 17) is RE1's: zombie_update and its tables
 * (docs/ZOMBIE_STATE_MACHINE.md in the PC decomp), in RE1's units, angles (4096 a turn) and
 * 30 Hz ticks, three ticks every two of OoT's frames. Ported so far: eating (behaviour 5), getting
 * up, turning to Link, the walking chase, the grab and bite and the stagger back after it, the hit
 * reaction and the falling death. A zombie whose behaviour has bit 7 is held for its room's
 * script (zombie_state_check returns at once) until enemy_prop_set changes it. Not yet: the idle
 * head turns and wandering of the other behaviours, the knock-downs and leg shots, getting back
 * up, the events' own animation of enemies, and RE1's sounds.
 * Kenneth's corpse (37, 41) is posed and still.
 *
 * Link's side is OoT's: his weapons hurt it through its collider and his life is in hearts. The
 * numbers that don't come from RE1 come from mod1 (z_en_re_zombie.c): the collider's size, the
 * damage table and the bite (half a heart). RE1's health (17-99) is kept as health / 10, the
 * Kokiri Sword's 1 being about a handgun shot.
 */
#include "global.h"
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
    s32 numFrames;
    s16* frames; // per frame: root offset[3], root speed[3], angles[3 * bones]
    s32 numAnims;
    u16* steps[NV_EM_MAX_ANIMS]; // u16 frame, u16 flags (low byte: ticks to hold it)
    s32 numSteps[NV_EM_MAX_ANIMS];
} NvEmBank;

typedef struct {
    s32 type; // -1: free
    s32 numBones;
    s32 numBonesAll; // in the file (numBones is capped)
    s16* bones; // per bone: x, y, z (from its parent), parent
    s32 order[NV_EM_MAX_BONES]; // parents first
    NvEmBank bank;
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
    // RE1's entity, in its units (y down)
    f32 rx, ry, rz;
    s16 angle; // 4096 a turn
    u8 behaviour; // behavior_flags
    u8 deathFlag; // death_event_id (0xFF: none)
    u8 state;
    u8 ignorePlayer;
    u8 action;
    u8 actionState;
    u8 animId;
    u8 frameId;
    u8 timing;
    u8 blend;
    s16 ticks; // action_ticks_counter
    s16 deathTimer;
    s16 moveSpeed; // move_speed_current
    s16 nextTurn;
    s16 attackTimer;
    u8 attackDir;
    u8 tickPhase;
    s16 rootT[3];
    s16 rot[NV_EM_MAX_BONES][3];
    s16 grabCooldown;
    s16 biteTimer; // frames to the next bite
    s32 grabbing;
} NvEnemy;

enum { NV_ZST_IDLE = 1, NV_ZST_DAMAGED = 2, NV_ZST_DIE = 3, NV_ZST_ATTACK = 5 };

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

/* ---- flags ---- */

/** Flg_on(g_EnemiesFlags, death_event_id): bank 3, killed for good. */
static void NvEnemy_SetDeathFlag(u8 bit) {
    if (bit != 0xFF) {
        NvFlags_Op(3, bit, 0);
    }
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

/* ---- RE1's helpers, on the actor ---- */

static s16 NvEnemy_AngleTo(NvEnemy* this, f32 x, f32 z) {
    // RE1's facing: forward is +x turned by RotMatrixY, (cos a, -sin a) in x, z
    return (s16)((s32)(atan2f(-(z - this->rz), x - this->rx) * (2048.0f / NV_PI)) & 0xFFF);
}

/** turn_toward_target: a step toward the target's angle, 0 when within twice the step. */
static s32 NvEnemy_TurnStep(NvEnemy* this, f32 x, f32 z, s16 step) {
    s32 delta = ((NvEnemy_AngleTo(this, x, z) - this->angle) + step) & 0xFFF;

    if (delta < (s32)(u16)(step * 2)) {
        return 0;
    }
    return (delta < 0x801) ? step : -step;
}

/** checkAngularViewAndDistance: within a Manhattan distance and fovHalf of the facing. */
static s32 NvEnemy_InView(NvEnemy* this, f32 x, f32 z, s16 fovHalf, s32 maxDist) {
    s32 delta;

    if (fabsf(x - this->rx) + fabsf(z - this->rz) > maxDist) {
        return false;
    }
    delta = ((NvEnemy_AngleTo(this, x, z) - this->angle) + fovHalf) & 0xFFF;
    return delta <= fovHalf * 2;
}

static void NvEnemy_SetAnim(NvEnemy* this, s32 anim, s32 blend) {
    this->animId = anim;
    this->frameId = 0;
    this->timing = 0;
    this->blend = blend;
}

/** Joint_move: one tick of the animation; true when it wraps. */
static s32 NvEnemy_JointMove(NvEnemy* this) {
    NvEmModel* m = this->model;
    NvEmBank* bank;
    u16* step;
    s16* data;
    s32 i;

    if (this->timing > 1) {
        this->timing--;
        return false;
    }
    if (m == NULL) {
        return false;
    }
    bank = &m->bank;
    if ((this->animId >= bank->numAnims) || (bank->numSteps[this->animId] == 0)) {
        return false;
    }
    if (this->frameId >= bank->numSteps[this->animId]) {
        this->frameId = 0;
    }
    step = bank->steps[this->animId] + this->frameId * 2;
    if (step[0] >= bank->numFrames) {
        return false;
    }
    data = bank->frames + step[0] * (6 + 3 * m->numBonesAll);
    this->rootT[0] = data[0];
    this->rootT[2] = data[2];
    if (this->blend == 0) {
        this->rootT[1] = data[1];
        for (i = 0; i < m->numBones; i++) {
            this->rot[i][0] = data[6 + i * 3];
            this->rot[i][1] = data[6 + i * 3 + 1];
            this->rot[i][2] = data[6 + i * 3 + 2];
        }
    } else {
        // blend: blend/4 of the old pose with the rest of the new, the shorter way round
        s32 k;

        this->rootT[1] = (this->rootT[1] * this->blend + data[1] * (4 - this->blend)) / 4;
        for (i = 0; i < m->numBones; i++) {
            for (k = 0; k < 3; k++) {
                s32 cur = this->rot[i][k];
                s32 diff = ((data[6 + i * 3 + k] - cur + 0x800) & 0xFFF) - 0x800;

                this->rot[i][k] = cur + diff * (4 - this->blend) / 4;
            }
        }
        this->blend--;
    }
    this->timing = step[1] & 0xFF;
    this->frameId++;
    if (this->frameId > bank->numSteps[this->animId] - 1) {
        this->frameId = 0;
        return true;
    }
    return false;
}

/** Add_speedXZ: move moveSpeed along the facing (plus an offset). */
static void NvEnemy_AddSpeed(NvEnemy* this, s16 offset) {
    f32 a = (this->angle + offset) * (NV_PI / 2048.0f);

    this->rx += cosf(a) * this->moveSpeed;
    this->rz -= sinf(a) * this->moveSpeed;
}

static void NvEnemy_PlayerRe1(PlayState* play, f32* x, f32* z) {
    Player* player = GET_PLAYER(play);

    *x = player->actor.world.pos.x / NV_RE1_SCALE;
    *z = -player->actor.world.pos.z / NV_RE1_SCALE;
}

static s32 NvEnemy_Manhattan(NvEnemy* this, f32 x, f32 z) {
    return (s32)(fabsf(x - this->rx) + fabsf(z - this->rz));
}

/* ---- the zombie ---- */

static void NvZombie_SetState(NvEnemy* this, u8 state, u8 ignore, u8 action, u8 actionState) {
    this->state = state;
    this->ignorePlayer = ignore;
    this->action = action;
    this->actionState = actionState;
}

/** zombie_eating: feed until Link is within 3000, get up, then turn to him. */
static void NvZombie_Eating(NvEnemy* this, PlayState* play, f32 px, f32 pz) {
    switch (this->actionState) {
        case 0:
            this->actionState = 1;
            NvEnemy_SetAnim(this, 0x1E, 3);
            this->frameId = Rand_Next() & 0x1F;
            this->ticks = (Rand_Next() & 0xF) + 0x2D;
            // fall through
        case 1:
            if (NvEnemy_JointMove(this)) {
                this->animId = (Rand_Next() & 1) ? 0x1D : 0x1E;
            }
            if (NvEnemy_Manhattan(this, px, pz) < 3000) {
                this->actionState++;
            }
            break;
        case 2:
            this->actionState = 3;
            NvEnemy_SetAnim(this, 0x0D, this->blend);
            // fall through
        case 3:
            if (NvEnemy_JointMove(this)) {
                this->behaviour = 0;
                NvZombie_SetState(this, NV_ZST_IDLE, 1, 3, 0);
                this->ry = 0.0f;
            }
            break;
    }
}

/** fast_player_facing: turn to Link at 0x54 a tick for 30-93 ticks, then walk at him. */
static void NvZombie_FaceFast(NvEnemy* this, f32 px, f32 pz) {
    s32 turn;
    s16 ticks;

    if (this->actionState == 0) {
        NvEnemy_SetAnim(this, 3, 3);
        this->actionState = 1;
        this->ticks = (Rand_Next() & 0x3F) + 0x1E;
    }
    turn = NvEnemy_TurnStep(this, px, pz, 0x54);
    ticks = this->ticks--;
    if ((ticks != 0) && (turn != 0)) {
        NvEnemy_JointMove(this);
        this->angle = (this->angle + turn) & 0xFFF;
        return;
    }
    NvZombie_SetState(this, NV_ZST_IDLE, 0, 2, 2);
}

/** zombie_chase_walk: the shamble at 45 a tick, with a slower weave now and then. */
static void NvZombie_ChaseWalk(NvEnemy* this, f32 px, f32 pz) {
    s16 ticks;

    switch (this->actionState) {
        case 0:
            this->actionState = 1;
            this->moveSpeed = 0;
            NvEnemy_SetAnim(this, 2, 3);
            // fall through
        case 1:
            this->actionState += NvEnemy_JointMove(this);
            break;
        case 2:
            this->actionState = 3;
            this->timing = 0;
            this->animId = 3;
            this->blend = 3;
            this->ticks = (Rand_Next() & 0x7F) + 10;
            this->nextTurn = 0;
            // fall through
        case 3:
            ticks = this->ticks--;
            if (ticks == 0) {
                this->nextTurn = (Rand_Next() & 0x0F) + 0x14;
                this->ticks = this->nextTurn + (Rand_Next() & 0x7F);
            }
            NvEnemy_JointMove(this);
            this->moveSpeed = 45;
            if (this->nextTurn != 0) {
                // the weave (entity_update_wander_turn, not ported): slow, turning away
                this->angle = (this->angle - 24 * 2 / 4) & 0xFFF;
                this->moveSpeed = 10;
                this->nextTurn--;
            }
            break;
    }
    // zombie_check_player_distance / the move behaviours' steering: toward Link at turn_speed
    if (this->nextTurn == 0) {
        this->angle = (this->angle + NvEnemy_TurnStep(this, px, pz, 24)) & 0xFFF;
    }
    NvEnemy_AddSpeed(this, 0);
}

static void NvZombie_Action(NvEnemy* this, PlayState* play, f32 px, f32 pz) {
    switch (this->action) {
        case 3:
            NvZombie_FaceFast(this, px, pz);
            break;
        case 2:
        default:
            NvZombie_ChaseWalk(this, px, pz);
            break;
    }
}

static void NvZombie_StartAttack(NvEnemy* this, PlayState* play, f32 px, f32 pz) {
    Player* player = GET_PLAYER(play);
    s16 linkFacing;
    s32 facing;

    if ((this->grabCooldown > 0) || (player->stateFlags2 & PLAYER_STATE2_GRABBED_BY_ENEMY) ||
        !play->grabPlayer(play, player)) {
        return;
    }
    this->angle = NvEnemy_AngleTo(this, px, pz);
    NvZombie_SetState(this, NV_ZST_ATTACK, 0, this->action, 0);
    // is_facing_toward_entity: Link looking at it (within 90 degrees)
    linkFacing = player->actor.shape.rot.y;
    facing = ABS((s16)(Actor_WorldYawTowardActor(&player->actor, &this->actor) - linkFacing)) < 0x4000;
    this->attackDir = (this->behaviour & 2) + (facing ? 1 : 0);
    this->grabbing = true;
    this->biteTimer = 0;
    printf("NVENEMY grab (direction %d)\n", this->attackDir);
    fflush(stdout);
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
}

/** zombie_attack: wind up, bite for 105 ticks (Link mashing out ends it sooner), let go. */
static void NvZombie_Attack(NvEnemy* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (this->grabbing && !(player->stateFlags2 & PLAYER_STATE2_GRABBED_BY_ENEMY) && (this->actionState < 4)) {
        this->actionState = 4; // he got free
    }
    switch (this->actionState) {
        case 0:
            NvEnemy_SetAnim(this, sNvZombieAttackAnim[this->attackDir & 3], 3);
            this->actionState = 1;
            break;
        case 1:
            this->actionState += NvEnemy_JointMove(this);
            break;
        case 2:
            this->actionState = 3;
            this->animId++;
            this->timing = 0;
            this->attackTimer = 105;
            break;
        case 3:
            NvEnemy_JointMove(this);
            if (--this->attackTimer < 0) {
                this->actionState = 4;
                if (player->stateFlags2 & PLAYER_STATE2_GRABBED_BY_ENEMY) {
                    player->av2.actionVar2 = 101; // let him go (Player_Action_8084F308's mash count)
                }
            }
            break;
        case 4:
            this->actionState = 5;
            this->animId++;
            this->frameId = 0;
            this->timing = 0;
            this->blend = 3;
            this->grabbing = false;
            this->grabCooldown = NV_GRAB_COOLDOWN;
            this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
            // fall through
        case 5:
            if (NvEnemy_JointMove(this)) {
                // zombie_attack_withdraw: the long push back
                NvZombie_SetState(this, NV_ZST_DAMAGED, 1, 6, 0);
            }
            break;
    }
}

/** long_push_back: staggering back (animation 7 from its 12th step) after letting go. */
static void NvZombie_LongPushBack(NvEnemy* this) {
    if (this->actionState == 0) {
        this->actionState = 1;
        this->animId = 7;
        this->frameId = 0x0C;
        this->timing = 1;
        this->moveSpeed = 0x1E;
    } else if (this->actionState == 2) {
        s16 ticks = this->ticks--;

        if (ticks == 0) {
            NvZombie_SetState(this, NV_ZST_IDLE, 0, 2, 2);
        }
        NvEnemy_AddSpeed(this, 2048);
        return;
    }
    if (NvEnemy_JointMove(this)) {
        this->actionState = 2;
        this->ticks = Rand_Next() & 7;
        this->moveSpeed = 15;
    }
    NvEnemy_AddSpeed(this, 2048);
}

/** short_push_back: the flinch (animation 4 or 5) at 15 a tick, then turn back to Link. */
static void NvZombie_Damaged(NvEnemy* this) {
    if (this->action == 6) {
        NvZombie_LongPushBack(this);
        return;
    }
    if (this->actionState == 0) {
        this->actionState = 1;
        NvEnemy_SetAnim(this, 4, 3);
        this->moveSpeed = 0;
    } else {
        this->moveSpeed = 15;
    }
    if (NvEnemy_JointMove(this)) {
        if ((this->behaviour & 0x0F) == 5) {
            this->behaviour = 0;
        }
        NvZombie_SetState(this, NV_ZST_IDLE, 1, 3, 0);
    }
    NvEnemy_AddSpeed(this, 0);
}

/** zombie_die + zombie_dead_animation: fall (8, or 16 from the floor), then lie there. */
static void NvZombie_Die(NvEnemy* this) {
    if (this->ignorePlayer == 0) {
        this->ignorePlayer = 1;
        this->action = ((this->behaviour & 2) || (this->behaviour == 5)) ? 2 : 0;
        NvEnemy_SetDeathFlag(this->deathFlag);
        this->actionState = 0;
    }
    if (this->actionState == 0) {
        this->actionState = 1;
        NvEnemy_SetAnim(this, (this->action == 2) ? 16 : 8, 3);
        this->deathTimer = 70;
    }
    switch (this->actionState) {
        case 1:
            if (this->action == 0) {
                this->moveSpeed = 20;
                NvEnemy_AddSpeed(this, 0);
            }
            // (once it wraps it isn't moved again: the pose stays on the last frame)
            this->actionState += NvEnemy_JointMove(this);
            break;
        case 2:
            if (--this->deathTimer <= 0) {
                this->actionState = 3;
            }
            break;
        default:
            break;
    }
}

static void NvZombie_Tick(NvEnemy* this, PlayState* play) {
    f32 px;
    f32 pz;

    NvEnemy_PlayerRe1(play, &px, &pz);
    switch (this->state) {
        case NV_ZST_IDLE:
            if (this->behaviour & 0x80) { // zombie_state_check: held for the room's script
                break;
            }
            if ((this->behaviour & 0x0F) == 5) {
                NvZombie_Eating(this, play, px, pz);
                break;
            }
            // zombie_chase_player: in reach (700 either side, 1500 away) and nobody on him: grab
            if ((this->ignorePlayer == 0) && NvEnemy_InView(this, px, pz, 700, 1500)) {
                NvZombie_StartAttack(this, play, px, pz);
                if (this->state == NV_ZST_ATTACK) {
                    NvZombie_Attack(this, play);
                    break;
                }
            }
            NvZombie_Action(this, play, px, pz);
            break;
        case NV_ZST_DAMAGED:
            NvZombie_Damaged(this);
            break;
        case NV_ZST_DIE:
            NvZombie_Die(this);
            break;
        case NV_ZST_ATTACK:
            NvZombie_Attack(this, play);
            break;
    }
}

/** Link's hits, OoT's way; the reaction is RE1's (a flinch, or the fall at no health left). */
static void NvZombie_UpdateDamage(NvEnemy* this, PlayState* play) {
    if (!(this->collider.base.acFlags & AC_HIT)) {
        return;
    }
    this->collider.base.acFlags &= ~AC_HIT;
    if ((this->state == NV_ZST_DIE) || (this->actor.colChkInfo.damageEffect == 0)) {
        return;
    }
    Actor_SetDropFlag(&this->actor, &this->collider.info, true);
    Actor_SetColorFilter(&this->actor, 0x4000, 255, 0, 8);
    Actor_ApplyDamage(&this->actor);
    printf("NVENEMY hit: health %d\n", this->actor.colChkInfo.health);
    fflush(stdout);
    if (this->actor.colChkInfo.health == 0) {
        if (this->grabbing) {
            Player* player = GET_PLAYER(play);

            player->av2.actionVar2 = 101;
            this->grabbing = false;
        }
        NvZombie_SetState(this, NV_ZST_DIE, 0, 0, 0);
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        Item_DropCollectibleRandom(play, NULL, &this->actor.world.pos, 0x90);
    } else if (this->state != NV_ZST_ATTACK) {
        // zombie_damaged: the eating zombie gets up instead (benddown_and_standup, as getting up)
        if ((this->behaviour & 0x0F) == 5) {
            if (this->actionState < 2) {
                this->actionState = 2;
            }
        } else {
            NvZombie_SetState(this, NV_ZST_DAMAGED, 1, 0, 0);
        }
    }
}

/* ---- the actor ---- */

void NvEnemy_Init(Actor* thisx, PlayState* play) {
    NvEnemy* this = (NvEnemy*)thisx;

    this->type = thisx->params & 0xFF;
    this->behaviour = thisx->home.rot.x & 0xFF;
    this->deathFlag = thisx->home.rot.z & 0xFF;
    this->model = NvEnemy_Model(this->type);
    this->rx = thisx->world.pos.x / NV_RE1_SCALE;
    this->ry = -thisx->world.pos.y / NV_RE1_SCALE;
    this->rz = -thisx->world.pos.z / NV_RE1_SCALE;
    this->angle = thisx->home.rot.y & 0xFFF;
    thisx->world.rot.x = thisx->world.rot.z = thisx->shape.rot.x = thisx->shape.rot.z = 0;
    ActorShape_Init(&thisx->shape, 0.0f, NULL, 0.0f);
    thisx->gravity = -3.5f;
    thisx->targetMode = 0;
    thisx->colChkInfo.mass = MASS_HEAVY;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sNvEnemyCylinder);

    if (NvEnemy_IsZombie(this->type)) {
        // zombie_init: health 59/79/99/39 less 0-21, kept as tenths
        s32 health = sNvZombieHealth[Rand_Next() & 0xF] - (s32)(Rand_Next() % 22);

        thisx->colChkInfo.health = MAX(1, (health + 9) / 10);
        thisx->colChkInfo.damageTable = &sNvZombieDamage;
        thisx->naviEnemyId = 0x2A; // (a ReDead)
        NvZombie_SetState(this, NV_ZST_IDLE, 0, 0, 0);
        NvEnemy_SetAnim(this, sNvZombieAnimId[this->behaviour & 0xF], 0);
        if ((this->behaviour & 0x0F) == 5) {
            this->actionState = 2; // zombie_init: an eating zombie starts at getting up
        } else {
            this->action = 2; // the other behaviours' idling and wandering aren't ported: it comes at Link
        }
        if (this->behaviour == 6) { // lying dead
            NvZombie_SetState(this, NV_ZST_DIE, 1, 2, 2);
            NvEnemy_SetAnim(this, 16, 0);
            thisx->flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        }
    } else {
        thisx->flags &= ~(ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE);
        NvEnemy_SetAnim(this, 0, 0);
    }
    NvEnemy_JointMove(this);
    printf("NVENEMY type %d behaviour 0x%02x at %d,%d,%d angle %d model %s health %d\n", (int)this->type,
           this->behaviour, (int)this->rx, (int)this->ry, (int)this->rz, this->angle,
           (this->model != NULL) ? "ok" : "missing", thisx->colChkInfo.health);
    fflush(stdout);
}

void NvEnemy_Destroy(Actor* thisx, PlayState* play) {
    NvEnemy* this = (NvEnemy*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
    for (s32 i = 0; i < NV_ENEMY_SLOTS; i++) {
        if (sNvEnemySlots[i] == thisx) {
            sNvEnemySlots[i] = NULL;
        }
    }
}

s32 NvDoorScene_Covering(void);

void NvEnemy_Update(Actor* thisx, PlayState* play) {
    NvEnemy* this = (NvEnemy*)thisx;
    s32 ticks;
    s32 t;

    if (this->model == NULL) {
        return;
    }
    if (NvDoorScene_Covering()) {
        return;
    }
    // RE1 runs at 30 Hz: three ticks every two frames
    ticks = (this->tickPhase ^= 1) ? 1 : 2;
    if (this->grabCooldown > 0) {
        this->grabCooldown--;
    }
    // the bite (zombie_attack's damage loop: the first as it starts biting)
    if (this->grabbing && (this->state == NV_ZST_ATTACK) && (this->actionState == 3) && (--this->biteTimer <= 0)) {
        Player* player = GET_PLAYER(play);

        play->damagePlayer(play, NV_BITE_DAMAGE);
        Rumble_Request(thisx->xzDistToPlayer, 240, 1, 12);
        Player_PlaySfx(&player->actor, NA_SE_VO_LI_DAMAGE_S + player->ageProperties->unk_92);
        this->biteTimer = NV_BITE_FRAMES;
    }
    if (NvEnemy_IsZombie(this->type)) {
        NvZombie_UpdateDamage(this, play);
        for (t = 0; t < ticks; t++) {
            NvZombie_Tick(this, play);
        }
    } else {
        for (t = 0; t < ticks; t++) {
            NvEnemy_JointMove(this);
        }
    }

    if (getenv("NV_LOG") && ((play->gameplayFrames % 20) == 0)) {
        printf("NVENEMY f=%d state %d action %d/%d anim %d at %d,%d angle %d\n", (int)play->gameplayFrames,
               this->state, this->action, this->actionState, this->animId, (int)this->rx, (int)this->rz,
               this->angle);
        fflush(stdout);
    }
    // to OoT, OoT's walls and floor, and back
    thisx->world.pos.x = this->rx * NV_RE1_SCALE;
    thisx->world.pos.z = -this->rz * NV_RE1_SCALE;
    thisx->shape.rot.y = thisx->world.rot.y = (s16)(0x4000 - this->angle * 16);
    if (this->grabbing) {
        Player* player = GET_PLAYER(play);
        f32 d = (this->attackDir & 1) ? 22.0f : -25.0f; // in front of him, or behind

        Math_SmoothStepToF(&thisx->world.pos.x, player->actor.world.pos.x + Math_SinS(player->actor.shape.rot.y) * d,
                           1.0f, 10.0f, 0.0f);
        Math_SmoothStepToF(&thisx->world.pos.z, player->actor.world.pos.z + Math_CosS(player->actor.shape.rot.y) * d,
                           1.0f, 10.0f, 0.0f);
        Math_SmoothStepToF(&thisx->world.pos.y, player->actor.world.pos.y, 1.0f, 10.0f, 0.0f);
        thisx->shape.rot.y = thisx->world.rot.y =
            player->actor.shape.rot.y + ((this->attackDir & 1) ? 0x8000 : 0);
        this->angle = (0x400 - thisx->shape.rot.y / 16) & 0xFFF; // (OoT yaw = 0x4000 - 16 angle)
    } else {
        Actor_MoveXZGravity(thisx);
        thisx->velocity.x = thisx->velocity.z = 0.0f;
        Actor_UpdateBgCheckInfo(play, thisx, 20.0f, 422.0f * NV_RE1_SCALE, 40.0f, 0x1D);
    }
    this->rx = thisx->world.pos.x / NV_RE1_SCALE;
    this->rz = -thisx->world.pos.z / NV_RE1_SCALE;
    thisx->focus.pos = thisx->world.pos;
    thisx->focus.pos.y += 54.0f;

    if ((this->state != NV_ZST_DIE) || !NvEnemy_IsZombie(this->type)) {
        Collider_UpdateCylinder(thisx, &this->collider);
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
        if (NvEnemy_IsZombie(this->type)) {
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

    if ((m == NULL) || NvDoorScene_Covering()) {
        return;
    }
    if (thisx->colorFilterTimer != 0) {
        tint = 80;
    }
    // the entity: RotMatrixY(angle), then to OoT's axes and scale (x, -y, -z)
    ca = cosf(this->angle * (NV_PI / 2048.0f));
    sa = sinf(this->angle * (NV_PI / 2048.0f));
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
        f32 a = this->rot[i][0] * (NV_PI / 2048.0f);
        f32 b = this->rot[i][1] * (NV_PI / 2048.0f);
        f32 c = this->rot[i][2] * (NV_PI / 2048.0f);
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
                T[i][r] = this->rootT[r];
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
        mf.mf[3][1] += thisx->world.pos.y - this->ry * NV_RE1_SCALE;
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

/** cmd_enemy_prop_set: 0 the behaviour byte, 2 the action (the others aren't run yet). */
void NvEnemy_ScdProp(s32 slot, s32 subCmd, u16 param) {
    NvEnemy* this;

    if ((slot >= NV_ENEMY_SLOTS) || (sNvEnemySlots[slot] == NULL)) {
        return;
    }
    this = (NvEnemy*)sNvEnemySlots[slot];
    if (subCmd == 0) {
        this->behaviour = param & 0xFF;
    } else if (subCmd == 2) {
        this->action = param & 0xFF;
        this->actionState = 0;
    }
    printf("NVENEMY slot %d prop %d = %d\n", (int)slot, (int)subCmd, (int)param);
    fflush(stdout);
}

const size_t gNvEnemySize = sizeof(NvEnemy);
