/*
 * Nightmareverse: MM's dogs (En_Dg, MM id 0x0E2), ported closely for Clock Town (mm/README.md "The
 * dogs"): MM's model (object_dog), South Clock Town's dog on its path (params: & 0x3E0 its index,
 * & 0xFC00 its path).
 *
 * As MM: it runs its path, stopping to bark now and then. To Link's forms: Deku Link it chases and
 * jumps at (biting); Goron Link it backs away from, runs from and barks at; Zora Link it runs up to
 * and sits by. Human Link can pick it up and throw it (OoT's carrying); with the Mask of Truth on,
 * picked up, it speaks its mind (MM's 0x627). The Bremen march: it falls in behind Link with OoT's
 * animals (z_nv_march_follow.c's line, in place of MM's own one-dog following).
 * Left out: the swimming (no water on its path in town), the dog race's dogs (Romani Ranch's
 * racetrack: their own colours and lines, with the race), MM's own dog sounds (its angry bark, whine, cry
 * and steps: OoT's dog's bark, steps and evening cry stand in).
 */
#include "global.h"
#include <stdio.h>
#include <stdlib.h>

#define DOG(name) ("__OTR__mm/objects/object_dog/" name)
#define NV_MM_TEXT(id) (0xB000 + (id))
#define DOG_LIMB_HEAD 5
#define DOG_LIMB_MAX 13
#define MM_PLAYER_MASK_TRUTH 0x01
#define ENDG_GET_INDEX(thisx) (((thisx)->params & 0x3E0) >> 5)
#define ENDG_GET_PATH_INDEX(thisx) (((thisx)->params & 0xFC00) >> 10)
#define ENDG_PATH_INDEX_NONE 0x3F

s32 NvMmForm_Get(void); // 0 FD, 1 Goron, 2 Zora, 3 Deku, 4 human
s32 NvMmMask_Worn(void);
s32 NvMarch_Follow(PlayState* play, Actor* actor, f32 gap);

enum { FORM_FD, FORM_GORON, FORM_ZORA, FORM_DEKU, FORM_HUMAN };
enum { GRAB_NONE, GRAB_HELD, GRAB_THROWN };
enum { BEHAVIOR_INITIAL, BEHAVIOR_HUMAN, BEHAVIOR_GORON, BEHAVIOR_GORON_WAIT, BEHAVIOR_ZORA, BEHAVIOR_ZORA_WAIT,
       BEHAVIOR_DEKU, BEHAVIOR_DEKU_WAIT, BEHAVIOR_DEFAULT };
enum { ANIM_WALK_AFTER_TALKING, ANIM_WALK, ANIM_RUN, ANIM_BARK, ANIM_SIT_DOWN, ANIM_WALK_BACKWARDS, ANIM_JUMP_ATTACK,
       ANIM_MAX };

typedef struct {
    const char* anim;
    f32 speed;
    s16 start;
    u8 mode;
    s8 morph;
} NvMmDgAnim;

static NvMmDgAnim sAnims[ANIM_MAX] = {
    { DOG("gDogWalkAnim"), 1.0f, 0, ANIMMODE_LOOP, 0 },          { DOG("gDogWalkAnim"), 1.0f, 0, ANIMMODE_LOOP, -6 },
    { DOG("gDogRunAnim"), 1.0f, 0, ANIMMODE_LOOP, 0 },           { DOG("gDogBarkAnim"), 1.0f, 0, ANIMMODE_LOOP, -6 },
    { DOG("gDogSitAnim"), 1.0f, 0, ANIMMODE_LOOP_PARTIAL, -6 }, { DOG("gDogWalkAnim"), -1.5f, -1, ANIMMODE_LOOP, -6 },
    { DOG("gDogJumpAttackAnim"), 1.2f, 0, ANIMMODE_ONCE, 0 },
};

struct NvMmDg;
typedef void (*NvMmDgFunc)(struct NvMmDg*, PlayState*);

typedef struct NvMmDg {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[DOG_LIMB_MAX];
    Vec3s morphTable[DOG_LIMB_MAX];
    NvMmDgFunc func;
    Path* path;
    s16 currentPoint;
    s16 timer;
    s16 attackTimer;
    s16 sitAfterThrowTimer;
    s16 index;
    s16 behavior;
    s16 grabState;
    s16 jumpAttacking;
    s16 thrown;
    Vec3f curRot;
    ColliderCylinder collider;
} NvMmDg;

const size_t gNvMmDgSize = sizeof(NvMmDg);

static ColliderCylinderInit sCylinderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_ENEMY, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1,
      COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK1, { 0xF7CFFFFF, 0x04, 0x00 }, { 0xF7CFFFFF, 0x00, 0x00 }, TOUCH_ON | TOUCH_SFX_NORMAL, BUMP_ON,
      OCELEM_ON },
    { 13, 19, 0, { 0, 0, 0 } },
};

static void NvMmDg_IdleMove(NvMmDg* this, PlayState* play);
static void NvMmDg_IdleBark(NvMmDg* this, PlayState* play);
static void NvMmDg_Fall(NvMmDg* this, PlayState* play);
static void NvMmDg_Held(NvMmDg* this, PlayState* play);
static void NvMmDg_Thrown(NvMmDg* this, PlayState* play);
static void NvMmDg_SetupTalk(NvMmDg* this, PlayState* play);
static void NvMmDg_Talk(NvMmDg* this, PlayState* play);
static void NvMmDg_BackAwayFromGoron(NvMmDg* this, PlayState* play);
static void NvMmDg_RunAwayFromGoron(NvMmDg* this, PlayState* play);
static void NvMmDg_BarkAtGoron(NvMmDg* this, PlayState* play);
static void NvMmDg_ApproachPlayerToAttack(NvMmDg* this, PlayState* play);
static void NvMmDg_JumpAttack(NvMmDg* this, PlayState* play);
static void NvMmDg_RunAfterAttacking(NvMmDg* this, PlayState* play);
static void NvMmDg_SlowlyBackUpBeforeAttacking(NvMmDg* this, PlayState* play);
static void NvMmDg_ApproachPlayer(NvMmDg* this, PlayState* play);
static void NvMmDg_SitNextToPlayer(NvMmDg* this, PlayState* play);

static void NvMmDg_ChangeAnim(NvMmDg* this, s32 i) {
    AnimationHeader* a = (AnimationHeader*)sAnims[i].anim;
    f32 last = Animation_GetLastFrame(a);

    if (sAnims[i].start < 0) { // (walking backwards: from the end)
        Animation_Change(&this->skelAnime, a, sAnims[i].speed, last, 0.0f, sAnims[i].mode, sAnims[i].morph);
    } else {
        Animation_Change(&this->skelAnime, a, sAnims[i].speed, sAnims[i].start, last, sAnims[i].mode,
                         sAnims[i].morph);
    }
}

static void NvMmDg_Sfx(NvMmDg* this, u16 sfx, f32 frame) {
    if (Animation_OnFrame(&this->skelAnime, frame)) {
        Audio_PlayActorSound2(&this->actor, sfx);
    }
}

/** MM's EnDg_HasReachedPoint: past the plane through the point, across the path. */
static s32 NvMmDg_HasReachedPoint(NvMmDg* this, Path* path, s32 i) {
    Vec3s* p = SEGMENTED_TO_VIRTUAL(path->points);
    s32 n = path->count;
    f32 dx;
    f32 dz;

    if (i == 0) {
        dx = p[1].x - p[0].x;
        dz = p[1].z - p[0].z;
    } else if (i == (n - 1)) {
        dx = p[n - 1].x - p[n - 2].x;
        dz = p[n - 1].z - p[n - 2].z;
    } else {
        dx = p[i + 1].x - p[i - 1].x;
        dz = p[i + 1].z - p[i - 1].z;
    }
    return ((this->actor.world.pos.x - p[i].x) * dx + (this->actor.world.pos.z - p[i].z) * dz) > 0.0f;
}

/** MM's EnDg_MoveAlongPath (South Clock Town's pace). */
static void NvMmDg_MoveAlongPath(NvMmDg* this, PlayState* play) {
    Vec3s* p;
    s16 yaw;

    if (this->path == NULL) {
        Actor_Kill(&this->actor);
        return;
    }
    Vec3f point;

    p = &((Vec3s*)SEGMENTED_TO_VIRTUAL(this->path->points))[this->currentPoint];
    point.x = p->x;
    point.y = p->y;
    point.z = p->z;
    yaw = Math_Vec3f_Yaw(&this->actor.world.pos, &point);
    if (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
        yaw = this->actor.wallYaw;
    }
    Math_SmoothStepToS(&this->actor.world.rot.y, yaw, 4, 0x3E8, 1);
    this->actor.shape.rot.y = this->actor.world.rot.y;
    if (NvMmDg_HasReachedPoint(this, this->path, this->currentPoint)) {
        this->currentPoint = (this->currentPoint >= (this->path->count - 1)) ? 0 : (this->currentPoint + 1);
    }
    Math_ApproachF(&this->actor.speedXZ, 3.5f, 0.2f, 1.0f);
}

static void NvMmDg_SetupIdleMove(NvMmDg* this, PlayState* play) {
    NvMmDg_ChangeAnim(this, ANIM_RUN); // (MM: running, in South Clock Town)
    this->func = NvMmDg_IdleMove;
}

/** MM's EnDg_TryPickUp: OoT's carrying. */
static void NvMmDg_TryPickUp(NvMmDg* this, PlayState* play) {
    if (Actor_HasParent(&this->actor, play)) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EV_SMALL_DOG_BARK);
        this->grabState = GRAB_HELD;
        NvMmDg_ChangeAnim(this, ANIM_SIT_DOWN);
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        this->actor.speedXZ = 0.0f;
        if (NvMmMask_Worn() == MM_PLAYER_MASK_TRUTH) { // it speaks its mind
            this->func = NvMmDg_SetupTalk;
        } else {
            this->func = NvMmDg_Held;
        }
    } else {
        Actor_OfferCarry(&this->actor, play);
    }
}

static void NvMmDg_IdleMove(NvMmDg* this, PlayState* play) {
    this->actor.velocity.y = 0.0f;
    this->actor.gravity = -3.0f;
    NvMmDg_MoveAlongPath(this, play);
    Actor_MoveXZGravity(&this->actor);
    if (NvMmForm_Get() == FORM_HUMAN) {
        NvMmDg_TryPickUp(this, play);
    }
    NvMmDg_Sfx(this, NA_SE_EV_DOG_WALK, 1.0f);
    if (!(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        this->func = NvMmDg_Fall;
    }
    if (DECR(this->timer) == 0) {
        this->timer = Rand_S16Offset(20, 20);
        NvMmDg_ChangeAnim(this, ANIM_BARK);
        this->func = NvMmDg_IdleBark;
    }
}

static void NvMmDg_IdleBark(NvMmDg* this, PlayState* play) {
    if (NvMmForm_Get() == FORM_HUMAN) {
        NvMmDg_TryPickUp(this, play);
    }
    NvMmDg_Sfx(this, NA_SE_EV_SMALL_DOG_BARK, 13.0f);
    NvMmDg_Sfx(this, NA_SE_EV_SMALL_DOG_BARK, 19.0f);
    if (!(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        this->func = NvMmDg_Fall;
    }
    if (DECR(this->timer) == 0) {
        this->timer = Rand_S16Offset(60, 60);
        NvMmDg_SetupIdleMove(this, play);
    }
}

static void NvMmDg_Fall(NvMmDg* this, PlayState* play) {
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        NvMmDg_SetupIdleMove(this, play);
    }
    Actor_MoveXZGravity(&this->actor);
}

/* ---- Goron Link: backing away, running, barking ---- */

static void NvMmDg_BackAwayFromGoron(NvMmDg* this, PlayState* play) {
    this->actor.velocity.y = 0.0f;
    this->actor.gravity = -3.0f;
    if (DECR(this->timer) == 0) {
        this->timer = 50;
        NvMmDg_ChangeAnim(this, ANIM_RUN);
        this->func = NvMmDg_RunAwayFromGoron;
    } else {
        Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 4, 0x3E8, 1);
        if (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
            this->actor.shape.rot.y = this->actor.wallYaw;
        }
        this->actor.world.rot.y = this->actor.shape.rot.y;
        Math_ApproachF(&this->actor.speedXZ, -1.5f, 0.2f, 1.0f);
        Actor_MoveXZGravity(&this->actor);
    }
}

static void NvMmDg_RunAwayFromGoron(NvMmDg* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (this->actor.xzDistToPlayer < 250.0f) {
        Math_ApproachS(&this->actor.shape.rot.y, -this->actor.yawTowardsPlayer, 4, 0xC00);
        this->actor.world.rot.y = this->actor.shape.rot.y;
        Math_ApproachF(&this->actor.speedXZ, (player->linearVelocity != 0.0f) ? player->linearVelocity : 3.5f, 0.2f,
                       1.0f);
    } else {
        NvMmDg_ChangeAnim(this, ANIM_BARK);
        this->func = NvMmDg_BarkAtGoron;
    }
    if (this->actor.speedXZ > 7.0f) {
        this->actor.speedXZ = 7.0f;
    }
    Actor_MoveXZGravity(&this->actor);
    NvMmDg_Sfx(this, NA_SE_EV_DOG_CRY_EVENING, 3.0f);
}

static void NvMmDg_BarkAtGoron(NvMmDg* this, PlayState* play) {
    Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 4, 0xC00);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    if (this->actor.xzDistToPlayer < 250.0f) {
        NvMmDg_ChangeAnim(this, ANIM_WALK_BACKWARDS);
        this->timer = 50;
        this->func = NvMmDg_BackAwayFromGoron;
    }
    NvMmDg_Sfx(this, NA_SE_EV_SMALL_DOG_BARK, 13.0f);
    NvMmDg_Sfx(this, NA_SE_EV_SMALL_DOG_BARK, 19.0f);
}

/* ---- Deku Link: chased and jumped at ---- */

static void NvMmDg_ApproachPlayerToAttack(NvMmDg* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    this->actor.velocity.y = 0.0f;
    this->actor.gravity = -3.0f;
    if (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
        this->actor.shape.rot.y = this->actor.wallYaw;
    }
    this->actor.world.rot.y = this->actor.shape.rot.y;
    if (this->actor.xzDistToPlayer < 70.0f) {
        Math_ApproachZeroF(&this->actor.speedXZ, 0.2f, 1.0f);
        if (Animation_OnFrame(&this->skelAnime, 7.0f)) {
            s16 d = player->actor.shape.rot.y - this->actor.shape.rot.y;

            this->attackTimer = 20;
            if (ABS(d) < 0x4000) {
                NvMmDg_ChangeAnim(this, ANIM_JUMP_ATTACK);
                this->func = NvMmDg_JumpAttack;
            } else {
                NvMmDg_ChangeAnim(this, ANIM_WALK_BACKWARDS);
                this->skelAnime.playSpeed = -1.0f;
                this->func = NvMmDg_SlowlyBackUpBeforeAttacking;
            }
        }
    } else {
        Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 4, 0xC00);
        Math_ApproachF(&this->actor.speedXZ, 5.0f, 0.2f, 1.0f);
    }
    Actor_MoveXZGravity(&this->actor);
    NvMmDg_Sfx(this, NA_SE_EV_SMALL_DOG_BARK, 5.0f);
}

static void NvMmDg_RunAfterAttacking(NvMmDg* this, PlayState* play) {
    Math_ApproachF(&this->actor.speedXZ, 3.5f, 0.1f, 0.5f);
    Actor_MoveXZGravity(&this->actor);
    if (DECR(this->attackTimer) == 0) {
        this->attackTimer = 20;
        this->func = NvMmDg_ApproachPlayerToAttack;
    }
}

static void NvMmDg_JumpAttack(NvMmDg* this, PlayState* play) {
    s16 frame = this->skelAnime.curFrame;

    if ((this->actor.xyzDistToPlayerSq < 800.0f) && (this->collider.base.atFlags & AT_BOUNCED)) {
        this->attackTimer = 60;
        this->jumpAttacking = false;
        this->collider.base.atFlags &= ~AT_BOUNCED;
        this->actor.speedXZ *= -1.0f;
        NvMmDg_ChangeAnim(this, ANIM_RUN);
        this->func = NvMmDg_RunAfterAttacking;
        return;
    }
    if (DECR(this->attackTimer) == 0) {
        this->attackTimer = 60;
        this->jumpAttacking = false;
        NvMmDg_ChangeAnim(this, ANIM_RUN);
        this->func = NvMmDg_RunAfterAttacking;
    }
    if (frame < 9) {
        if (Animation_OnFrame(&this->skelAnime, 0.0f)) {
            this->skelAnime.playSpeed = Rand_CenteredFloat(1.0f) + 3.0f;
        }
    } else {
        this->jumpAttacking = true;
        if (Animation_OnFrame(&this->skelAnime, 9.0f)) {
            f32 r = Rand_CenteredFloat(1.5f);

            this->skelAnime.playSpeed = 1.2f;
            this->actor.velocity.y = 2.0f * r + 3.0f;
            this->actor.speedXZ = 8.0f + r;
        } else if (frame > 20) {
            Math_ApproachF(&this->actor.speedXZ, 2.5f, 0.2f, 1.0f);
        }
        Actor_MoveXZGravity(&this->actor);
    }
}

static void NvMmDg_SlowlyBackUpBeforeAttacking(NvMmDg* this, PlayState* play) {
    if (this->actor.xzDistToPlayer > 72.0f) {
        NvMmDg_ChangeAnim(this, ANIM_JUMP_ATTACK);
        this->func = NvMmDg_JumpAttack;
    }
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 4, 0x3E8, 1);
    if (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
        this->actor.shape.rot.y = this->actor.wallYaw;
        NvMmDg_ChangeAnim(this, ANIM_JUMP_ATTACK);
        this->func = NvMmDg_JumpAttack;
    }
    this->actor.world.rot.y = this->actor.shape.rot.y;
    Math_ApproachF(&this->actor.speedXZ, -1.0f, 0.2f, 1.0f);
    Actor_MoveXZGravity(&this->actor);
}

/* ---- Zora Link: run up to him and sit ---- */

static void NvMmDg_ApproachPlayer(NvMmDg* this, PlayState* play) {
    this->actor.velocity.y = 0.0f;
    this->actor.gravity = -3.0f;
    if ((this->actor.xzDistToPlayer < 60.0f) && (this->collider.base.ocFlags1 & OC1_HIT)) {
        this->actor.shape.rot.y += 0x71C;
    } else {
        Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 4, 0xC00);
    }
    this->actor.world.rot.y = this->actor.shape.rot.y;
    if (this->actor.xzDistToPlayer < 40.0f) {
        NvMmDg_ChangeAnim(this, ANIM_SIT_DOWN);
        this->func = NvMmDg_SitNextToPlayer;
    } else {
        Math_ApproachF(&this->actor.speedXZ, 3.5f, 0.2f, 1.0f);
    }
    Actor_MoveXZGravity(&this->actor);
    NvMmDg_Sfx(this, NA_SE_EV_SMALL_DOG_BARK, 3.0f);
}

static void NvMmDg_SitNextToPlayer(NvMmDg* this, PlayState* play) {
    if (!(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
        this->func = NvMmDg_Fall;
    }
    if (this->actor.xzDistToPlayer < 50.0f) {
        Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 4, 0xC00);
        this->actor.world.rot.y = this->actor.shape.rot.y;
    } else {
        NvMmDg_ChangeAnim(this, ANIM_RUN);
        this->func = NvMmDg_ApproachPlayer;
    }
    NvMmDg_Sfx(this, NA_SE_EV_DOG_CRY_EVENING, 23.0f);
}

/* ---- picked up, thrown, the Mask of Truth ---- */

static void NvMmDg_Held(NvMmDg* this, PlayState* play) {
    if (Actor_HasNoParent(&this->actor, play)) {
        this->grabState = GRAB_THROWN;
        this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
        this->sitAfterThrowTimer = 100;
        Actor_MoveXZGravity(&this->actor);
        this->thrown = true;
        this->func = NvMmDg_Thrown;
    }
}

static void NvMmDg_Thrown(NvMmDg* this, PlayState* play) {
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        if (this->thrown) {
            this->thrown = false;
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_DOG_WALK);
        }
        this->actor.speedXZ = 0.0f;
        this->actor.gravity = -3.0f;
        if (NvMmForm_Get() == FORM_HUMAN) {
            NvMmDg_TryPickUp(this, play);
        }
    }
    if (DECR(this->sitAfterThrowTimer) == 0) {
        this->grabState = GRAB_NONE;
        NvMmDg_SetupIdleMove(this, play);
    }
    Actor_MoveXZGravity(&this->actor);
}

static void NvMmDg_SetupTalk(NvMmDg* this, PlayState* play) {
    this->actor.textId = NV_MM_TEXT(0x627); // "Where did that Deku Scrub go?" (South Clock Town's dog)
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        this->func = NvMmDg_Talk;
    } else {
        this->actor.flags |= ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
        Actor_OfferTalk(&this->actor, play, 100.0f);
    }
}

static void NvMmDg_Talk(NvMmDg* this, PlayState* play) {
    if (Message_GetState(&play->msgCtx) == TEXT_STATE_CLOSING) {
        this->actor.flags &= ~ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
        NvMmDg_ChangeAnim(this, ANIM_WALK_AFTER_TALKING);
        this->func = NvMmDg_Held;
    }
}

/** MM's EnDg_ChooseActionForForm: once Link moves, the dog reacts to his form. */
static void NvMmDg_ChooseActionForForm(NvMmDg* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s32 moving = player->linearVelocity > 1.0f;

    switch (NvMmForm_Get()) {
        case FORM_HUMAN:
            if (this->behavior != BEHAVIOR_HUMAN) {
                this->behavior = BEHAVIOR_HUMAN;
                this->jumpAttacking = false;
                NvMmDg_SetupIdleMove(this, play);
            }
            break;
        case FORM_ZORA:
            this->jumpAttacking = false;
            if ((this->behavior != BEHAVIOR_ZORA) && moving) {
                this->behavior = BEHAVIOR_ZORA;
                NvMmDg_ChangeAnim(this, ANIM_RUN);
                this->func = NvMmDg_ApproachPlayer;
            }
            if ((this->behavior != BEHAVIOR_ZORA_WAIT) && (this->behavior != BEHAVIOR_ZORA)) {
                this->behavior = BEHAVIOR_ZORA_WAIT;
                NvMmDg_SetupIdleMove(this, play);
            }
            break;
        case FORM_GORON:
            this->jumpAttacking = false;
            if ((this->behavior != BEHAVIOR_GORON) && moving) {
                this->behavior = BEHAVIOR_GORON;
                NvMmDg_ChangeAnim(this, ANIM_WALK_BACKWARDS);
                this->timer = 50;
                this->func = NvMmDg_BackAwayFromGoron;
            }
            if ((this->behavior != BEHAVIOR_GORON_WAIT) && (this->behavior != BEHAVIOR_GORON)) {
                this->behavior = BEHAVIOR_GORON_WAIT;
                NvMmDg_SetupIdleMove(this, play);
            }
            break;
        case FORM_DEKU:
            this->jumpAttacking = false;
            if ((this->behavior != BEHAVIOR_DEKU) && moving) {
                this->behavior = BEHAVIOR_DEKU;
                NvMmDg_ChangeAnim(this, ANIM_RUN);
                this->func = NvMmDg_ApproachPlayerToAttack;
            }
            if ((this->behavior != BEHAVIOR_DEKU_WAIT) && (this->behavior != BEHAVIOR_DEKU)) {
                this->behavior = BEHAVIOR_DEKU_WAIT;
                NvMmDg_SetupIdleMove(this, play);
            }
            break;
    }
}

/** MM's EnDg_ShouldReactToNonHumanPlayer. */
static s32 NvMmDg_ShouldReact(NvMmDg* this) {
    switch (NvMmForm_Get()) {
        case FORM_GORON:
        case FORM_ZORA:
            return this->actor.xzDistToPlayer < 300.0f;
        case FORM_DEKU:
            return this->actor.xzDistToPlayer < 250.0f;
    }
    return false;
}

void NvMmDg_Init(Actor* thisx, PlayState* play) {
    NvMmDg* this = (NvMmDg*)thisx;

    ActorShape_Init(&thisx->shape, 0.0f, ActorShadow_DrawCircle, 24.0f);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)DOG("gDogSkel"), NULL, this->jointTable,
                       this->morphTable, DOG_LIMB_MAX);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sCylinderInit);
    thisx->colChkInfo.mass = 1;
    thisx->uncullZoneForward = 1000.0f;
    this->path = Path_GetByIndex(play, ENDG_GET_PATH_INDEX(thisx), ENDG_PATH_INDEX_NONE);
    Actor_SetScale(thisx, 0.0075f);
    thisx->targetMode = 1;
    thisx->gravity = -3.0f;
    this->timer = Rand_S16Offset(60, 60);
    this->attackTimer = 20;
    this->index = ENDG_GET_INDEX(thisx);
    this->behavior = BEHAVIOR_INITIAL;
    this->grabState = GRAB_NONE;
    NvMmDg_SetupIdleMove(this, play);
}

void NvMmDg_Destroy(Actor* thisx, PlayState* play) {
    Collider_DestroyCylinder(play, &((NvMmDg*)thisx)->collider);
}

void NvMmDg_Update(Actor* thisx, PlayState* play) {
    NvMmDg* this = (NvMmDg*)thisx;
    s32 march;

    if (NvMmDg_ShouldReact(this)) {
        NvMmDg_ChooseActionForForm(this, play);
    } else if (this->behavior != BEHAVIOR_DEFAULT) {
        this->behavior = BEHAVIOR_DEFAULT;
        NvMmDg_SetupIdleMove(this, play);
    }
    // the Bremen march: free on the ground, it falls in with the others (z_nv_march_follow.c)
    march = ((this->grabState == GRAB_NONE) && ((this->func == NvMmDg_IdleMove) || (this->func == NvMmDg_IdleBark)))
                ? NvMarch_Follow(play, thisx, 45.0f)
                : 0;
    if (march != 0) {
        if ((march == 1) && (this->skelAnime.animation != (AnimationHeader*)sAnims[ANIM_WALK].anim)) {
            NvMmDg_ChangeAnim(this, ANIM_WALK);
        } else if ((march == 2) && (this->skelAnime.animation != (AnimationHeader*)sAnims[ANIM_SIT_DOWN].anim)) {
            NvMmDg_ChangeAnim(this, ANIM_SIT_DOWN);
        }
        this->func = NvMmDg_IdleMove;
        this->timer = 60;
        Actor_MoveXZGravity(thisx);
    } else {
        this->func(this, play);
    }
    // MM's EnDg_UpdateCollision
    Collider_UpdateCylinder(thisx, &this->collider);
    if ((NvMmForm_Get() == FORM_DEKU) && (this->func == NvMmDg_JumpAttack)) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
    if ((this->grabState != GRAB_HELD) && !this->jumpAttacking) {
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    }
    Actor_UpdateBgCheckInfo(play, thisx, 26.0f, 10.0f, 0.0f, 5);
    // MM's EnDg_GetFloorRot: leaning with the ground
    if (thisx->floorPoly != NULL) {
        f32 nx = COLPOLY_GET_NORMAL(thisx->floorPoly->normal.x);
        f32 ny = COLPOLY_GET_NORMAL(thisx->floorPoly->normal.y);
        f32 nz = COLPOLY_GET_NORMAL(thisx->floorPoly->normal.z);

        Math_ApproachF(&this->curRot.x, -Math_Atan2F(1.0f, -nz * ny), 0.2f, 0.1f); // (MM's Math_Atan2F_XY)
        Math_ApproachF(&this->curRot.z, Math_Atan2F(1.0f, -nx * ny), 0.2f, 0.1f);
    }
    SkelAnime_Update(&this->skelAnime);
}

static void NvMmDg_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    NvMmDg* this = (NvMmDg*)thisx;
    Vec3f offset = { 0.0f, 20.0f, 0.0f };

    if ((limbIndex == DOG_LIMB_HEAD) && (this->func != NvMmDg_Talk)) {
        if (this->func == NvMmDg_SetupTalk) {
            offset.x = 5000.0f;
        }
        Matrix_MultVec3f(&offset, &this->actor.focus.pos);
    }
}

void NvMmDg_Draw(Actor* thisx, PlayState* play) {
    NvMmDg* this = (NvMmDg*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetEnvColor(POLY_OPA_DISP++, 255, 255, 200, 0); // (MM's default: beige)
    Matrix_Translate(thisx->world.pos.x, thisx->world.pos.y, thisx->world.pos.z, MTXMODE_NEW);
    Matrix_RotateX(this->curRot.x, MTXMODE_APPLY);
    Matrix_RotateZ(this->curRot.z, MTXMODE_APPLY);
    Matrix_RotateY(BINANG_TO_RAD(thisx->shape.rot.y), MTXMODE_APPLY);
    Matrix_Scale(thisx->scale.x, thisx->scale.y, thisx->scale.z, MTXMODE_APPLY);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          NULL, NvMmDg_PostLimbDraw, this);
    CLOSE_DISPS(play->state.gfxCtx);
}
