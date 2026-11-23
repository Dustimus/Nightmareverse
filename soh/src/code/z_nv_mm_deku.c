/*
 * Nightmareverse: Deku Link's moves (mm/README.md "The forms: Deku Link"), MM's player code for
 * the Deku form brought onto OoT's Link: MM's animations (mm/player_anim, all of MM's player
 * animations now in mm.o2r), MM's numbers, MM's order of things. OoT's player code runs Link as
 * ever; these are actions of our own set as his current action (OoT's actionFunc), started from
 * three hooks in z_player.c, each one line:
 * - A (OoT's roll handler, the last of its lists: talking and the rest come first): the spin
 *   (MM's func_80839A84, func_808373A4 and Player_Action_95);
 * - R (OoT's shield handler): the shell guard (MM's Player_ActionHandler_11 with pn_gurd);
 * - the water deep enough to swim (where OoT starts swimming): the hops across it, and with none
 *   left, sinking and being put back where he last stood (MM's func_8083868C, func_808373F8,
 *   func_8083784C, Player_Action_1).
 * And each frame (NvMmDeku_Update, from z_nv_mm_form.c): the hops counted (five from the ground,
 * MM's remainingHopsCounter), the last hop's spin in the air, MM's Deku speeds (MM's boot data for
 * PLAYER_BOOTS_DEKU on OoT's movement registers).
 *
 * The spin hits as OoT's Deku Stick does, for 1 (MM: DMG_DEKU_SPIN, 1, a 30-unit cylinder), through
 * OoT's own way of making Link's body cylinder hit (invincibilityTimer below 0). The guard keeps hits
 * off Link's body while R is held (MM blocks with the shell; OoT has no shell collider).
 *
 * Sounds: MM's Deku sounds aren't in OoT's sound banks (MM's audio isn't brought over yet); OoT's
 * nearest stand in (its Deku Scrubs', its jumps, its water).
 *
 * Not yet: the Deku bubble on B (needs magic and MM's bubble), the Deku flowers (MM's
 * Obj_Etcetera: none are ported yet; MM's func_80836DC0 and the flight, Player_Action_93/94, wait
 * for them), the hop counter's A-button label, the sinking's room change (put back in another room:
 * MM loads it; here he's put back only within the room he's in).
 */
#include "global.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include <stdio.h>
#include <stdlib.h>

#define MMANIM(name) ((LinkAnimationHeader*)("__OTR__mm/player_anim/" name))
#define ANIM_ATTACK MMANIM("gPlayerAnim_pn_attack")
#define ANIM_GUARD MMANIM("gPlayerAnim_pn_gurd")

#define DEKU_HOPS 5

// z_player.c (OoT's player code, as it is)
s32 Player_SetupAction(PlayState* play, Player* this, PlayerActionFunc actionFunc, s32 flags);
void func_80839F90(Player* this, PlayState* play); // back to standing (idle, or the lock-on stances)
void func_80838940(Player* this, LinkAnimationHeader* anim, f32 velY, PlayState* play, u16 sfxId); // a jump
s32 Player_GetMovementSpeedAndYaw(Player* this, f32* outSpeedTarget, s16* outYawTarget, f32 speedMode,
                                  PlayState* play);
void func_8083DF68(Player* this, f32 speed, s16 yaw); // steps speed and yaw towards the stick's
void Player_AnimChangeOnceMorph(PlayState* play, Player* this, LinkAnimationHeader* anim);
s32 Player_DecelerateToZero(Player* this);
LinkAnimationHeader* Player_GetIdleAnim(Player* this);
void Player_PlayVoiceSfx(Player* this, u16 sfxId);
void Player_DetachHeldActor(PlayState* play, Player* this);
void Player_Action_8084411C(Player* this, PlayState* play); // OoT's midair

s32 NvMmForm_Get(void);
#define MM_FORM_DEKU 3

static s32 sHops = DEKU_HOPS;
static f32 sSpinSpeed;  // MM's unk_B10[0]: the spin's turn a frame, slowing
static f32 sSpinLeft;   // MM's unk_B10[1]: how much turning is left
static s32 sAirSpin;    // the last hop's spin, in the air
static s32 sSinkStep;   // the sinking: 0 none, 1 to black, 2 back from black
static s32 sSinkAlpha;
static s32 sSavedAc = -1; // the body's hurt flags put aside while guarding

static void NvMmDeku_Log(const char* what) {
    if (getenv("NV_LOG") != NULL) {
        printf("NVMM deku: %s\n", what);
        fflush(stdout);
    }
}

static s32 NvMmDeku_Is(Player* player) {
    return (NvMmForm_Get() == MM_FORM_DEKU) && (player->actor.category == ACTORCAT_PLAYER);
}

/* ---- the spin (MM's Player_Action_95) ---- */

static u32 sSavedDmgFlags;
static u8 sSavedDamage;
static s16 sSavedRadius;
static s32 sCylinderSet;

static void NvMmDeku_SetSpinCylinder(Player* this) {
    if (!sCylinderSet) {
        sSavedDmgFlags = this->cylinder.info.toucher.dmgFlags;
        sSavedDamage = this->cylinder.info.toucher.damage;
        sSavedRadius = this->cylinder.dim.radius;
        sCylinderSet = true;
    }
    this->cylinder.info.toucher.dmgFlags = DMG_DEKU_STICK; // (MM's DMG_DEKU_SPIN)
    this->cylinder.info.toucher.damage = 1;
    this->cylinder.info.toucherFlags = TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL;
    this->cylinder.base.atFlags = AT_ON | AT_TYPE_PLAYER;
    this->cylinder.dim.radius = 30;
    if (this->invincibilityTimer >= -1) { // (OoT: below 0, the body's cylinder hits)
        this->invincibilityTimer = -2;
    }
}

static void NvMmDeku_ResetCylinder(Player* this) {
    if (sCylinderSet) {
        this->cylinder.info.toucher.dmgFlags = sSavedDmgFlags;
        this->cylinder.info.toucher.damage = sSavedDamage;
        this->cylinder.dim.radius = sSavedRadius;
        sCylinderSet = false;
    }
    if (this->invincibilityTimer < 0) {
        this->invincibilityTimer = 0;
    }
}

static void NvMmDeku_StartSpinTurn(PlayState* play, Player* this) { // MM's func_808373A4
    Player_AnimChangeOnceMorph(play, this, ANIM_ATTACK);
    sSpinSpeed = 20000.0f;
    sSpinLeft = 0x30000;
    Player_PlaySfx(&this->actor, NA_SE_EN_NUTS_UP); // (MM: NA_SE_PL_DEKUNUTS_ATTACK)
}

static void NvMmDeku_Spin(Player* this, PlayState* play) {
    s16 prevYaw = this->yaw;
    f32 speedTarget;
    s16 yawTarget;

    LinkAnimation_Update(play, &this->skelAnime);
    NvMmDeku_SetSpinCylinder(this);

    Player_GetMovementSpeedAndYaw(this, &speedTarget, &yawTarget, 0.018f, play); // (SPEED_MODE_CURVED)
    speedTarget *= 1.0f - (0.9f * ((11100.0f - sSpinSpeed) / 11100.0f));
    func_8083DF68(this, speedTarget, yawTarget);

    sSpinSpeed += -800.0f;
    this->actor.shape.rot.y += (s16)((s16)sSpinSpeed + (s16)(this->yaw - prevYaw));

    if (Math_StepToF(&sSpinLeft, 0.0f, sSpinSpeed)) {
        this->actor.shape.rot.y = this->yaw;
        NvMmDeku_ResetCylinder(this);
        NvMmDeku_Log("spin end");
        func_80839F90(this, play);
    } else if ((this->skelAnime.animation == ANIM_ATTACK) && (sSpinLeft < 0.0f)) {
        Player_AnimChangeOnceMorph(play, this, Player_GetIdleAnim(this));
    }
}

/* ---- the guard (MM's Player_ActionHandler_11 for Deku, and Player_Action_18 holding it) ---- */

static void NvMmDeku_Guard(Player* this, PlayState* play) {
    LinkAnimation_Update(play, &this->skelAnime);
    this->stateFlags1 |= PLAYER_STATE1_SHIELDING;
    Player_DecelerateToZero(this);
    if (sSavedAc < 0) {
        sSavedAc = this->cylinder.base.acFlags;
    }
    this->cylinder.base.acFlags &= ~AC_ON; // (the shell takes the hits)
    if (!CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_R) || !NvMmDeku_Is(this)) {
        this->stateFlags1 &= ~PLAYER_STATE1_SHIELDING;
        this->cylinder.base.acFlags = sSavedAc;
        sSavedAc = -1;
        NvMmDeku_Log("guard end");
        func_80839F90(this, play);
    }
}


/* ---- the Deku flowers (MM's func_80836DC0, Player_Action_93 and Player_Action_94) ---- */

#define ANIM_KAKKU MMANIM("gPlayerAnim_pn_kakku")
#define ANIM_KAKKUFINISH MMANIM("gPlayerAnim_pn_kakkufinish")
#define ANIM_BATABATA MMANIM("gPlayerAnim_pn_batabata")
#define ANIM_RAKKAFINISH MMANIM("gPlayerAnim_pn_rakkafinish")
#define NUTS(name) ("__OTR__mm/objects/object_link_nuts/" name)

s32 NvMmEtcetera_IsFlower(Actor* actor, s32* gold);

/** MM's Math_AsymStepToS (not in OoT): towards target, by incrStep going up, decrStep going down. */
static s32 NvMm_AsymStepToS(s16* pValue, s16 target, s16 incrStep, s16 decrStep) {
    s16 step = (target >= *pValue) ? incrStep : decrStep;

    if (step != 0) {
        if (target < *pValue) {
            step = -step;
        }
        *pValue += step;
        if (((*pValue - target) * step) >= 0) {
            *pValue = target;
            return true;
        }
    } else if (target == *pValue) {
        return true;
    }
    return false;
}
#define Math_AsymStepToS NvMm_AsymStepToS

/** MM's Math_Vec3f_StepToXZ (not in OoT): x and z towards target's, `speed` at most. */
static void NvMm_Vec3f_StepToXZ(Vec3f* start, Vec3f* target, f32 speed) {
    f32 dx = target->x - start->x;
    f32 dz = target->z - start->z;
    f32 dist = sqrtf(SQ(dx) + SQ(dz));

    if (dist > speed) {
        start->x += dx * (speed / dist);
        start->z += dz * (speed / dist);
    } else {
        start->x = target->x;
        start->z = target->z;
    }
}
#define Math_Vec3f_StepToXZ NvMm_Vec3f_StepToXZ
void Player_AnimPlayLoop(PlayState* play, Player* this, LinkAnimationHeader* anim);

static f32 sDepth;       // MM's unk_ABC: how deep in the flower (model units, below 0)
static f32 sDepthVel;    // MM's unk_B48
static s32 sStage;       // MM's av1.actionVar1 burrowing: 0 in, 1 down, 2 waiting (A held), 3 out
static s32 sCharge;      // MM's av2.actionVar2: A held this long (10: a high launch, and hovering)
static s32 sBudSound;    // MM's unk_B86[0]
static s32 sLaunching;   // MM's PLAYER_STATE3_200
static s32 sHovering;    // MM's PLAYER_STATE3_2000
static s32 sFlyFrom;     // MM's av1.actionVar1 flying: -1 not from a flower, 0 a pink, 1 a gold
static s32 sFlyTimer;    // MM's av2.actionVar2 flying
static Vec3f sLaunchPos; // MM's unk_AF0[0]
static s16 sPropSpeed;   // MM's unk_B86[1]: the hand flowers' turn
static s16 sPropAngle;   // MM's unk_B8A
static s16 sTiltX;       // MM's unk_B8C
static s16 sTiltZ;       // MM's unk_B8E
static u8 sSparkle;      // MM's unk_B66

s32 NvMmDeku_Launching(void) {
    return sLaunching;
}

s32 NvMmDeku_Hovering(void) {
    return sHovering;
}

s32 NvMmDeku_StartingBurrow(void) { // (MM: unk_ABC < 0 and not yet PLAYER_STATE3_100)
    return (sDepth < 0.0f) && (sDepth >= -1500.0f);
}

static Actor* NvMmDeku_FloorFlower(PlayState* play, Player* this, s32* gold) {
    DynaPolyActor* dyna;

    if (this->actor.floorBgId == BGCHECK_SCENE) {
        return NULL;
    }
    dyna = DynaPoly_GetActor(&play->colCtx, this->actor.floorBgId);
    return ((dyna != NULL) && NvMmEtcetera_IsFlower(&dyna->actor, gold)) ? &dyna->actor : NULL;
}

/** MM's func_808566C0: sparkles from a hand (the flowers' pollen). */
static void NvMmDeku_Sparkle(PlayState* play, Player* this, s32 part, f32 base, f32 range, f32 accel, s32 life) {
    Color_RGBA8 prim = { 255, 200, 200, 0 };
    Color_RGBA8 env = { 255, 255, 0, 0 };
    Vec3f vel = { 0.0f, 0.3f, 0.0f };
    Vec3f acc = { 0.0f, -0.025f, 0.0f };
    Vec3f pos;
    f32 sign = (Rand_ZeroOne() < 0.5f) ? -1.0f : 1.0f;

    vel.x = (Rand_ZeroFloat(range) + base) * sign;
    acc.x = accel * sign;
    sign = (Rand_ZeroOne() < 0.5f) ? -1.0f : 1.0f;
    vel.z = (Rand_ZeroFloat(range) + base) * sign;
    acc.z = accel * sign;
    pos.x = this->bodyPartsPos[part].x;
    pos.y = Rand_ZeroFloat(15.0f) + this->bodyPartsPos[part].y;
    pos.z = this->bodyPartsPos[part].z;
    EffectSsKiraKira_SpawnDispersed(play, &pos, &vel, &acc, &prim, &env, (Rand_ZeroOne() < 0.5f) ? 2000 : -150, life);
}

static void NvMmDeku_Fly(Player* this, PlayState* play);

static void NvMmDeku_EndFlower(Player* this) {
    this->stateFlags1 &= ~(PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL);
    sLaunching = sHovering = false;
    sDepth = 0.0f;
    this->actor.shape.yOffset = 0.0f;
    Actor_SetScale(&this->actor, 0.01f);
}

/** MM's func_80833AA0: out of the air, falling (OoT's midair). */
static void NvMmDeku_Fall(Player* this, PlayState* play, LinkAnimationHeader* anim) {
    NvMmDeku_EndFlower(this);
    NvMmDeku_ResetCylinder(this);
    this->actor.gravity = REG(68) / 100.0f;
    this->actor.minVelocityY = -20.0f;
    Player_SetupAction(play, this, Player_Action_8084411C, 0);
    if (anim != NULL) { // (MM's func_808355D8: the flowers close, falling)
        LinkAnimation_Change(play, &this->skelAnime, anim, 1.0f, 0.0f, Animation_GetLastFrame(anim), ANIMMODE_ONCE,
                             0.0f);
        Player_PlaySfx(&this->actor, NA_SE_EN_NUTS_DOWN); // (MM: NA_SE_IT_DEKUNUTS_FLOWER_CLOSE)
    } else {
        LinkAnimation_PlayLoop(play, &this->skelAnime, &gPlayerAnim_link_normal_landing_wait);
    }
}

/** MM's Player_Action_93: into the flower, waiting there while A is held, and out. */
static void NvMmDeku_Burrow(Player* this, PlayState* play) {
    Input* input = &play->state.input[0];
    f32 f;

    LinkAnimation_Update(play, &this->skelAnime);
    if (sStage == 0) {
        sDepth += sDepthVel;
        if (sDepth < -1000.0f) {
            sDepth = -1000.0f;
            sStage = 1;
            sDepthVel = 0.0f;
        }
    } else if (sStage == 1) {
        sDepthVel += -22.0f;
        if (sDepthVel < -170.0f) {
            sDepthVel = -170.0f;
        }
        sDepth += sDepthVel;
        if (sDepth < -3900.0f) {
            sDepth = -3900.0f;
            sStage = 2;
            this->actor.shape.rot.y = Camera_GetInputDirYaw(GET_ACTIVE_CAM(play));
            this->actor.scale.y = 0.01f;
            this->yaw = this->actor.world.rot.y = this->actor.shape.rot.y;
        } else {
            Actor* flower;

            f = Math_SinS((1000.0f + sDepth) * (-30.0f)) * 0.004f;
            this->actor.scale.y = 0.01f + f;
            this->actor.scale.z = this->actor.scale.x = 0.01f - (sDepthVel * -0.000015f);
            this->actor.shape.rot.y += (s16)(sDepthVel * 130.0f);
            flower = NvMmDeku_FloorFlower(play, this, NULL);
            if (flower != NULL) {
                Math_Vec3f_StepToXZ(&this->actor.world.pos, &flower->world.pos, 1.0f);
            }
        }
    } else if (sStage == 2) {
        if (!CHECK_BTN_ALL(input->cur.button, BTN_A)) {
            CollisionPoly* poly;
            s32 bgId;
            f32 ceilY;
            Vec3f pos = this->actor.world.pos;

            pos.y -= 20.0f;
            if (BgCheck_EntityCheckCeiling(&play->colCtx, &ceilY, &pos, 30.0f, &poly, &bgId, &this->actor)) {
                sCharge = 0; // (a ceiling above: no launch)
            } else {
                sStage = 3;
                sDepthVel = (sCharge >= 10) ? 2700.0f : 1450.0f;
                Player_PlaySfx(&this->actor, NA_SE_EN_NUTS_UP); // (MM: NA_SE_PL_DEKUNUTS_OUT_GRD)
            }
        } else if (sCharge < 15) {
            sCharge++;
        }
        {
            f32 speedTarget;
            s16 yawTarget;

            Player_GetMovementSpeedAndYaw(this, &speedTarget, &yawTarget, 0.018f, play);
            Math_ScaledStepToS(&this->yaw, yawTarget, 0x258);
            this->actor.shape.rot.y = this->yaw;
        }
    } else {
        sDepth += sDepthVel;
        if (sDepth >= 0.0f) { // out: launched
            s32 gold = false;
            f32 speed = sDepthVel * this->actor.scale.y;
            f32 depth = sDepth;
            Actor* flower = NvMmDeku_FloorFlower(play, this, &gold);

            sFlyFrom = (flower == NULL) ? -1 : (gold ? 1 : 0);
            if (gold) {
                speed *= 174 / 100.0f;
            }
            Math_Vec3f_Copy(&sLaunchPos, &this->actor.world.pos);
            sDepth = 0.0f;
            this->actor.shape.yOffset = 0.0f;
            this->actor.world.pos.y += depth * this->actor.scale.y;
            func_80838940(this, ANIM_KAKKU, speed, play, NA_SE_VO_LI_SWORD_N);
            Player_SetupAction(play, this, NvMmDeku_Fly, 1);
            this->stateFlags3 |= PLAYER_STATE3_MIDAIR;
            sLaunching = true;
            sHovering = (sCharge >= 10);
            sFlyTimer = 9999;
            sPropSpeed = 0;
            NvMmDeku_SetSpinCylinder(this); // (MM: DMG_DEKU_LAUNCH, 2, radius 20)
            this->cylinder.info.toucher.damage = 2;
            this->cylinder.dim.radius = 20;
            NvMmDeku_Log(sHovering ? "launch (hover)" : "launch");
            return;
        }
    }
    if (sDepth < -1500.0f) {
        if (sBudSound < 8) {
            sBudSound++;
            if (sBudSound == 8) {
                Player_PlaySfx(&this->actor, NA_SE_EN_NUTS_FAINT); // (MM: NA_SE_PL_DEKUNUTS_BUD)
            }
        }
    }
    this->actor.shape.yOffset = sDepth;
}

/** MM's func_80836DC0: A on a Deku flower dives in; true if it did. */
static s32 NvMmDeku_TryBurrow(PlayState* play, Player* this) {
    if (NvMmDeku_FloorFlower(play, this, NULL) == NULL) {
        return false;
    }
    Player_SetupAction(play, this, NvMmDeku_Burrow, 0);
    this->stateFlags1 &= ~(PLAYER_STATE1_PARALLEL | PLAYER_STATE1_LOCK_ON_FORCED_TO_RELEASE);
    Player_AnimChangeOnceMorph(play, this, ANIM_ATTACK);
    this->linearVelocity = 0.0f;
    this->actor.speedXZ = 0.0f;
    this->actor.focus.rot.x = this->actor.focus.rot.z = 0;
    this->headLimbRot.x = this->headLimbRot.y = this->headLimbRot.z = 0;
    this->upperLimbRot.x = this->upperLimbRot.y = this->upperLimbRot.z = 0;
    this->actor.shape.rot.y = this->yaw = this->actor.focus.rot.y;
    sDepth = 0.0f;
    sDepthVel = -2000.0f;
    sStage = 0;
    sCharge = 0;
    sBudSound = 0;
    Player_PlaySfx(&this->actor, NA_SE_EN_NUTS_DOWN); // (MM: NA_SE_PL_DEKUNUTS_IN_GRD)
    NvMmDeku_Log("into the flower");
    return true;
}

/** MM's Player_Action_94: up from the flower and, hovering, flying on the flowers in his hands. */
static void NvMmDeku_Fly(Player* this, PlayState* play) {
    Input* input = &play->state.input[0];

    this->stateFlags3 |= PLAYER_STATE3_MIDAIR; // (OoT: airborne in an action of its own, not a fall)
    this->stateFlags1 |= PLAYER_STATE1_JUMPING; // (OoT's jump camera, following him: MM has its own, CAM_MODE_DEKUFLY)

    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) { // (MM's func_80837134)
        NvMmDeku_EndFlower(this);
        NvMmDeku_ResetCylinder(this);
        if (CHECK_BTN_ALL(input->cur.button, BTN_A) && NvMmDeku_TryBurrow(play, this)) {
            return;
        }
        NvMmDeku_Log("landed");
        func_80839F90(this, play);
        return;
    }
    if ((this->actor.velocity.y > 0.0f) && sLaunching) { // going up
        this->actor.minVelocityY = -20.0f;
        this->actor.gravity = -5.5f;
        NvMmDeku_SetSpinCylinder(this);
        this->cylinder.info.toucher.damage = 2;
        this->cylinder.dim.radius = 20;
        if (this->actor.bgCheckFlags & BGCHECKFLAG_CEILING) {
            NvMmDeku_Fall(this, play, NULL);
        }
    } else if (!sHovering) {
        NvMmDeku_Fall(this, play, NULL);
    } else if (sLaunching) { // at the top: the flowers open
        if (this->actor.velocity.y < 0.0f) {
            if (sFlyFrom < 0) {
                NvMmDeku_Fall(this, play, NULL);
                return;
            }
            LinkAnimation_Update(play, &this->skelAnime);
            if (this->skelAnime.curFrame > 6.0f) {
                this->actor.velocity.y = 6.0f;
                sLaunching = false;
                Player_PlaySfx(&this->actor, NA_SE_EN_NUTS_THROW); // (MM: NA_SE_IT_DEKUNUTS_FLOWER_OPEN)
                NvMmDeku_Log("flying");
            }
        }
        this->actor.minVelocityY = -10.0f;
        this->actor.gravity = -0.5f;
        NvMmDeku_ResetCylinder(this);
    } else if (CHECK_BTN_ALL(input->press.button, BTN_A)) { // let go
        NvMmDeku_Fall(this, play, ANIM_KAKKUFINISH);
    } else {
        static const f32 sRange[] = { 600.0f, 960.0f }; // (MM's D_8085D958: pink, gold)
        f32 left;
        f32 speedTarget;
        f32 step;
        s16 yawTarget;
        s16 propTarget;
        s16 lean;
        s32 t;

        this->linearVelocity = sqrtf(SQ(this->actor.velocity.x) + SQ(this->actor.velocity.z));
        if (this->linearVelocity != 0.0f) {
            s16 dir = Math_Atan2S(this->actor.velocity.x, this->actor.velocity.z);

            if (ABS((s16)(this->actor.shape.rot.y - dir)) > 0x4000) {
                this->linearVelocity = -this->linearVelocity;
                dir += 0x8000;
            }
            this->yaw = dir;
        }
        if (sFlyTimer != 0) {
            sFlyTimer--;
        }
        left = sRange[sFlyFrom] - Math_Vec3f_DistXZ(&this->actor.world.pos, &sLaunchPos);
        LinkAnimation_Update(play, &this->skelAnime);
        if ((sFlyTimer != 0) && (left > 300.0f)) {
            propTarget = 0x1770;
            if (this->skelAnime.animation != ANIM_KAKKU) {
                LinkAnimation_Change(play, &this->skelAnime, ANIM_KAKKUFINISH, 1.0f, 0.0f,
                                     Animation_GetLastFrame(ANIM_KAKKUFINISH), ANIMMODE_ONCE, 0.0f);
            } else if (LinkAnimation_OnFrame(&this->skelAnime, 8.0f)) {
                s32 i;

                for (i = 0; i < 13; i++) {
                    NvMmDeku_Sparkle(play, this, PLAYER_BODYPART_L_HAND, 0.6f, 1.0f, 0.8f, 17);
                    NvMmDeku_Sparkle(play, this, PLAYER_BODYPART_R_HAND, 0.6f, 1.0f, 0.8f, 17);
                }
            }
        } else if ((sFlyTimer == 0) || (left < 0.0f)) { // (out of reach: dropping)
            NvMmDeku_Fall(this, play, ANIM_RAKKAFINISH);
            return;
        } else {
            propTarget = 0x1770 - (s32)((300.0f - left) * 10.0f);
            if (this->skelAnime.animation != ANIM_BATABATA) {
                Player_AnimPlayLoop(play, this, ANIM_BATABATA);
            } else if (LinkAnimation_OnFrame(&this->skelAnime, 6.0f)) {
                Player_PlaySfx(&this->actor, NA_SE_EN_NUTS_WALK); // (MM: NA_SE_PL_DEKUNUTS_STRUGGLE)
            }
        }
        Math_AsymStepToS(&sPropSpeed, propTarget, 0x190, 0x190);
        sPropAngle += sPropSpeed;
        t = ABS(sPropSpeed);
        if (t > 0xFA0) {
            sSparkle += (u8)(ABS(sPropSpeed) * 0.01f);
        }
        if (sSparkle > 200) {
            sSparkle -= 200;
            NvMmDeku_Sparkle(play, this, PLAYER_BODYPART_L_HAND, 0.0f, 1.0f, 0.0f, 32);
            NvMmDeku_Sparkle(play, this, PLAYER_BODYPART_R_HAND, 0.0f, 1.0f, 0.0f, 32);
        }
        if (this->actor.velocity.y < 0.0f) {
            if (propTarget != 0) {
                this->actor.minVelocityY = -0.38f;
                this->actor.gravity = -0.2f;
            } else {
                this->actor.minVelocityY = (sPropSpeed * 0.0033f) + -20.0f;
                this->actor.gravity = (sPropSpeed * 0.00004f) + (REG(68) / 100.0f);
            }
        }
        this->fallDistance = 0;
        Player_GetMovementSpeedAndYaw(this, &speedTarget, &yawTarget, 0.0f, play); // (SPEED_MODE_LINEAR)
        if (speedTarget == 0.0f) {
            step = 0.1f;
        } else {
            if (ABS((s16)(this->yaw - yawTarget)) > 0x4000) {
                speedTarget = -speedTarget;
                yawTarget += 0x8000;
            }
            step = 0.25f;
        }
        Math_SmoothStepToS(&sTiltX, speedTarget * 600.0f, 8, 0xFA0, 0x64);
        Math_ScaledStepToS(&this->yaw, yawTarget, 0xFA);
        lean = (s16)(yawTarget - this->yaw) * -2.0f;
        lean = CLAMP(lean, -0x1F40, 0x1F40);
        Math_SmoothStepToS(&sTiltZ, lean, 0x14, 0x320, 0x14);
        speedTarget = (speedTarget * (sPropSpeed * 0.0004f)) * fabsf(Math_SinS(sTiltX));
        Math_StepToF(&this->linearVelocity, speedTarget, step); // (MM's func_80856888)
        speedTarget = sqrtf(SQ(this->linearVelocity) + SQ(this->actor.velocity.y));
        if (speedTarget > 8.0f) {
            speedTarget = 8.0f / speedTarget;
            this->linearVelocity *= speedTarget;
            this->actor.velocity.y *= speedTarget;
        }
        this->actor.shape.rot.y = this->yaw;
    }
}

/** z_player_lib.c's hand hook (MM's func_801271B0): the flowers in his hands while he flies. */
void NvMmDeku_DrawHandFlower(PlayState* play, Player* this, s32 limbIndex) {
    typedef struct {
        s16 frame;
        Vec3s scale;
    } Key; // MM's struct_80124618
    static const Key sLaunchStem[] = { { 0, { 0, 0, 0 } }, { 6, { 0, 0, 0 } }, { 8, { 100, 100, 100 } }, { 11, { 100, 100, 100 } } };
    static const Key sLaunchHead[] = { { 0, { 0, 0, 0 } }, { 7, { 1120, 112, 112 } }, { 8, { 140, 168, 168 } }, { 11, { 140, 140, 140 } } };
    static const Key sLetGoStem[] = { { 0, { 100, 100, 100 } }, { 7, { 100, 100, 100 } }, { 8, { 70, 30, 70 } }, { 10, { 0, 0, 0 } }, { 14, { 0, 0, 0 } } };
    static const Key sLetGoHead[] = { { 0, { 140, 140, 140 } }, { 1, { 0, 0, 0 } }, { 14, { 0, 0, 0 } } };
    static const Key sDropStem[] = { { 0, { 100, 100, 100 } }, { 5, { 100, 100, 100 } }, { 6, { 0, 0, 0 } }, { 10, { 0, 0, 0 } } };
    static const Key sDropHead[] = { { 0, { 140, 140, 140 } }, { 1, { 0, 0, 0 } }, { 10, { 0, 0, 0 } } };
    static const Key sFlyStem[] = { { 0, { 100, 100, 100 } }, { 13, { 100, 100, 100 } } };
    static const Key sFlyHead[] = { { 0, { 140, 140, 140 } }, { 13, { 140, 140, 140 } } };
    const Key* keys[2] = { sLaunchStem, sLaunchHead };
    LinkAnimationHeader* anim = this->skelAnime.animation;
    s32 side = (limbIndex == PLAYER_LIMB_L_HAND) ? 0 : 1;
    s32 k;
    Vec3f scale;

    if (!NvMmDeku_Is(this) || ((anim != ANIM_KAKKU) && (anim != ANIM_KAKKUFINISH) && (anim != ANIM_RAKKAFINISH) &&
                               (anim != ANIM_BATABATA))) {
        return;
    }
    if (anim == ANIM_KAKKUFINISH) {
        keys[0] = sLetGoStem, keys[1] = sLetGoHead;
    } else if (anim == ANIM_BATABATA) {
        keys[0] = sFlyStem, keys[1] = sFlyHead;
    } else if (anim == ANIM_RAKKAFINISH) {
        keys[0] = sDropStem, keys[1] = sDropHead;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate(0.0f, 150.0f, 0.0f, MTXMODE_APPLY);
    for (k = 0; k < 2; k++) {
        const Key* a = keys[k];
        s32 frame = (s32)this->skelAnime.curFrame;
        f32 p;

        while (a[1].frame < frame) { // (MM's func_80124618: the keyframes' scale)
            a++;
        }
        p = (this->skelAnime.curFrame - a[0].frame) / (f32)(a[1].frame - a[0].frame);
        scale.x = (a[0].scale.x + (a[1].scale.x - a[0].scale.x) * p) * 0.01f;
        scale.y = (a[0].scale.y + (a[1].scale.y - a[0].scale.y) * p) * 0.01f;
        scale.z = (a[0].scale.z + (a[1].scale.z - a[0].scale.z) * p) * 0.01f;
        if (k == 1) {
            Matrix_Translate(2150.0f, 0.0f, 0.0f, MTXMODE_APPLY);
            Matrix_RotateX(BINANG_TO_RAD(sPropAngle), MTXMODE_APPLY);
        }
        Matrix_Push();
        Matrix_Scale(scale.x, scale.y, scale.z, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        Matrix_Pop();
        if (k == 0) {
            gSPDisplayList(POLY_OPA_DISP++, (Gfx*)(side ? NUTS("gLinkDekuRightFlowerStemDL") : NUTS("gLinkDekuLeftFlowerStemDL")));
        } else {
            gSPDisplayList(POLY_OPA_DISP++, (Gfx*)((this->actor.velocity.y < -6.0f) ? NUTS("gLinkDekuClosedFlowerDL")
                                                                                   : NUTS("gLinkDekuOpenFlowerDL")));
        }
    }
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}


/* ---- the bubble on B (MM's Player_UpperAction_7/8 for Deku, func_808306F8; the bubble: z_nv_mm_bubble.c) ---- */

#define ANIM_TAMAHAKIDF MMANIM("gPlayerAnim_pn_tamahakidf")
#define ANIM_TAMAHAKI MMANIM("gPlayerAnim_pn_tamahaki")

s32 NvMmActor_Id(s32 mmId);
void NvMmBubble_Fire(Actor* thisx, PlayState* play);
s32 NvMmBubble_Blowing(Actor* thisx);

static Actor* sBubble;

static void NvMmDeku_Blow(Player* this, PlayState* play) {
    Input* input = &play->state.input[0];
    f32 speedTarget;
    s16 yawTarget;

    LinkAnimation_Update(play, &this->skelAnime);
    Player_DecelerateToZero(this);
    if (Player_GetMovementSpeedAndYaw(this, &speedTarget, &yawTarget, 0.018f, play) && (speedTarget > 0.0f)) {
        Math_ScaledStepToS(&this->yaw, yawTarget, 0x258); // (turning to aim, as MM lets him)
    }
    this->actor.shape.rot.y = this->yaw;
    if ((sBubble != NULL) && (sBubble->update == NULL)) {
        sBubble = NULL;
    }
    if ((sBubble != NULL) && NvMmBubble_Blowing(sBubble)) {
        if (!CHECK_BTN_ALL(input->cur.button, BTN_B)) { // let go: off it goes
            NvMmBubble_Fire(sBubble, play);
            LinkAnimation_PlayOnce(play, &this->skelAnime, ANIM_TAMAHAKI);
            NvMmDeku_Log("bubble");
        }
    } else if ((this->skelAnime.animation != ANIM_TAMAHAKI) ||
               (this->skelAnime.curFrame >= Animation_GetLastFrame(ANIM_TAMAHAKI))) {
        if ((sBubble != NULL) && (this->skelAnime.animation != ANIM_TAMAHAKI)) { // (it went on its own)
            LinkAnimation_PlayOnce(play, &this->skelAnime, ANIM_TAMAHAKI);
            return;
        }
        sBubble = NULL;
        func_80839F90(this, play);
    }
}

/** z_nv_mm_mask.c's head hook: the bubble being blown sits at his mouth (MM's D_801C0EA0, in the
 * head's space, turned a quarter as MM does). */
void NvMmDeku_HeadPostLimb(PlayState* play, Player* this) {
    static Vec3f sMouth = { 1300.0f, -400.0f, 0.0f };
    MtxF mf;

    if ((sBubble == NULL) || (sBubble->update == NULL) || !NvMmBubble_Blowing(sBubble)) {
        return;
    }
    Matrix_Push();
    Matrix_MultVec3f(&sMouth, &sBubble->world.pos);
    Matrix_RotateZYX(0, 0x4000, 0, MTXMODE_APPLY);
    Matrix_Get(&mf);
    Matrix_MtxFToYXZRotS(&mf, &sBubble->shape.rot, 0);
    sBubble->world.rot.y = sBubble->shape.rot.y;
    Matrix_Pop();
}

/** B as Deku, standing: a bubble blown (MM's 2 magic). True if he started. */
static s32 NvMmDeku_TryBubble(PlayState* play, Player* this) {
    if (!CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B)) {
        return false;
    }
    if ((gSaveContext.magicLevel == 0) || !Magic_RequestChange(play, 2, MAGIC_CONSUME_NOW)) {
        Sfx_PlaySfxCentered(NA_SE_SY_ERROR); // (no magic: as MM)
        return true;
    }
    sBubble = Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, NvMmActor_Id(0x00F), this->actor.world.pos.x,
                                 this->actor.world.pos.y, this->actor.world.pos.z, 0, this->actor.shape.rot.y, 0, 0);
    if (sBubble == NULL) {
        return true;
    }
    Player_SetupAction(play, this, NvMmDeku_Blow, 0);
    LinkAnimation_Change(play, &this->skelAnime, ANIM_TAMAHAKIDF, 1.0f, 0.0f, Animation_GetLastFrame(ANIM_TAMAHAKIDF),
                         ANIMMODE_LOOP, -6.0f);
    this->yaw = this->actor.shape.rot.y;
    NvMmDeku_Log("blowing a bubble");
    return true;
}

/* ---- the hooks (z_player.c) ---- */

/** OoT's roll handler (A): MM's Deku spin, on the ground and out of the water. */
s32 NvMmGoron_ActionAB(PlayState* play, Player* this); // (z_nv_mm_goron.c)
s32 NvMmZora_ActionAB(PlayState* play, Player* this);  // (z_nv_mm_zora.c)
s32 NvMmZora_InFastSwim(Player* this);

s32 NvMmDeku_ActionA(PlayState* play, Player* this) {
    if (NvMmGoron_ActionAB(play, this) || NvMmZora_ActionAB(play, this)) { // (the other forms share this hook)
        return true;
    }
    if (NvMmDeku_Is(this) && (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) &&
        !(this->stateFlags1 & (PLAYER_STATE1_IN_WATER | PLAYER_STATE1_ON_HORSE)) && NvMmDeku_TryBubble(play, this)) {
        return true; // (B: the bubble; this hook is OoT's last free-standing handler, A's and B's)
    }
    if (!NvMmDeku_Is(this) || !CHECK_BTN_ALL(play->state.input[0].press.button, BTN_A) ||
        !(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) || (this->stateFlags1 & PLAYER_STATE1_IN_WATER) ||
        (this->stateFlags1 & PLAYER_STATE1_ON_HORSE)) {
        return false;
    }
    if (NvMmDeku_TryBurrow(play, this)) { // (a Deku flower under him: into it)
        return true;
    }
    Player_SetupAction(play, this, NvMmDeku_Spin, 0);
    this->stateFlags1 &= ~(PLAYER_STATE1_PARALLEL | PLAYER_STATE1_LOCK_ON_FORCED_TO_RELEASE);
    NvMmDeku_StartSpinTurn(play, this);
    NvMmDeku_Log("spin");
    return true;
}

/** OoT's shield handler (R): MM's Deku guard, held while R is. */
s32 NvMmDeku_ActionR(PlayState* play, Player* this) {
    if (!NvMmDeku_Is(this)) {
        return false;
    }
    if (!CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_R) || (this->actionFunc == NvMmDeku_Guard)) {
        return this->actionFunc == NvMmDeku_Guard;
    }
    Player_DetachHeldActor(play, this);
    if (Player_SetupAction(play, this, NvMmDeku_Guard, 0)) {
        this->stateFlags1 |= PLAYER_STATE1_SHIELDING;
        this->upperLimbRot.x = this->upperLimbRot.y = this->upperLimbRot.z = 0;
        LinkAnimation_Change(play, &this->skelAnime, ANIM_GUARD, 1.0f, 0.0f, Animation_GetLastFrame(ANIM_GUARD),
                             ANIMMODE_ONCE, 0.0f);
        Player_PlaySfx(&this->actor, NA_SE_EN_NUTS_DOWN); // (MM: NA_SE_PL_DEKUNUTS_IN_GRD)
        NvMmDeku_Log("guard");
    }
    return true;
}

static void NvMmDeku_Sink(Player* this, PlayState* play);

/** Where OoT would start Link swimming: Deku Link hops off the water, or sinks; Goron Link sinks
 * (MM: he can't swim either; MM's own walk on the bottom isn't in: put back on land); true if handled. */
s32 NvMmDeku_IntoWater(PlayState* play, Player* this) {
    LinkAnimationHeader* anim;
    f32 speed;
    s16 yawDiff;

    if (!NvMmDeku_Is(this)) {
        if (NvMmZora_InFastSwim(this)) { // (Zora Link gliding: z_nv_mm_zora.c's own swimming)
            return true;
        }
        if ((NvMmForm_Get() == 1) && (this->actor.category == ACTORCAT_PLAYER)) { // Goron
            if (this->actionFunc != NvMmDeku_Sink) {
                Player_SetupAction(play, this, NvMmDeku_Sink, 0);
                this->stateFlags1 |= PLAYER_STATE1_IN_CUTSCENE;
                sSinkStep = 1;
                sSinkAlpha = 0;
                Player_PlaySfx(&this->actor, NA_SE_EV_DIVE_INTO_WATER_L);
                NvMmDeku_Log("sink (Goron)");
            }
            return true;
        }
        return false;
    }
    if (this->actionFunc == NvMmDeku_Sink) {
        return true;
    }
    if ((sHops != 0) && (gSaveContext.health != 0)) { // MM's func_808373F8, the Deku part
        yawDiff = this->yaw - this->actor.shape.rot.y;
        if ((IREG(66) / 100.0f) < this->linearVelocity) {
            speed = IREG(67) / 100.0f;
        } else {
            speed = (IREG(68) / 100.0f) + ((IREG(69) * this->linearVelocity) / 1000.0f);
        }
        if (speed < 8.0f) {
            speed = 8.0f;
        }
        anim = ((ABS(yawDiff) >= 0x1000) || (this->linearVelocity <= 4.0f)) ? &gPlayerAnim_link_normal_jump
                                                                            : &gPlayerAnim_link_normal_run_jump;
        speed *= 0.3f + ((DEKU_HOPS - sHops) * 0.18f);
        if (speed < 4.0f) {
            speed = 4.0f;
        }
        this->actor.world.pos.y += this->actor.yDistToWater;
        func_80838940(this, anim, speed, play, NA_SE_VO_LI_AUTO_JUMP);
        this->stateFlags1 &= ~PLAYER_STATE1_IN_WATER;
        Player_PlaySfx(&this->actor, NA_SE_PL_JUMP_WATER0 + ((DEKU_HOPS - sHops) % 3)); // (MM: DEKUNUTS_JUMP1-5)
        sHops--;
        NvMmDeku_Log("hop");
        if (sHops == 0) { // the last: spinning (MM: func_808373A4, in the air)
            NvMmDeku_StartSpinTurn(play, this);
            sAirSpin = true;
        }
        return true;
    }
    // MM's Player_Action_1: sinks, the screen goes black, back where he last stood
    Player_SetupAction(play, this, NvMmDeku_Sink, 0);
    this->stateFlags1 |= PLAYER_STATE1_IN_CUTSCENE;
    sSinkStep = 1;
    sSinkAlpha = 0;
    NvMmDeku_Log("sink");
    Player_PlaySfx(&this->actor, NA_SE_EV_DIVE_INTO_WATER); // (MM: NA_SE_SY_DEKUNUTS_JUMP_FAILED)
    return true;
}

static void NvMmDeku_Sink(Player* this, PlayState* play) {
    LinkAnimation_Update(play, &this->skelAnime);
    this->linearVelocity = 0.0f;
    if (sSinkStep == 1) {
        this->actor.velocity.y = -1.0f;
        sSinkAlpha += 20; // (MM's R_PLAY_FILL_SCREEN_ON 20, to black)
        if (sSinkAlpha >= 255) {
            sSinkAlpha = 255;
            sSinkStep = 2;
            Math_Vec3f_Copy(&this->actor.world.pos, &gSaveContext.respawn[RESPAWN_MODE_DOWN].pos);
            Math_Vec3f_Copy(&this->actor.prevPos, &this->actor.world.pos);
            this->actor.shape.rot.y = this->yaw = gSaveContext.respawn[RESPAWN_MODE_DOWN].yaw;
            this->actor.velocity.y = 0.0f;
            this->stateFlags1 &= ~PLAYER_STATE1_IN_WATER;
            this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
            LinkAnimation_PlayOnce(play, &this->skelAnime, Player_GetIdleAnim(this));
        }
    } else {
        sSinkAlpha -= 20;
        if (sSinkAlpha <= 0) {
            sSinkAlpha = 0;
            sSinkStep = 0;
            this->stateFlags1 &= ~PLAYER_STATE1_IN_CUTSCENE;
            NvMmDeku_Log("back on land");
            if (this->invincibilityTimer >= 0) {
                this->invincibilityTimer = -40; // (MM: func_808339B4(this, -40), untouchable a while)
            }
            this->actor.bgCheckFlags |= BGCHECKFLAG_GROUND;
            func_80839F90(this, play);
        }
    }
    play->envCtx.fillScreen = (sSinkAlpha > 0);
    play->envCtx.screenFillColor[0] = play->envCtx.screenFillColor[1] = play->envCtx.screenFillColor[2] = 0;
    play->envCtx.screenFillColor[3] = sSinkAlpha;
}

/* ---- each frame ---- */

s32 NvMmForm_SetBootRegs(PlayState* play); // (z_nv_mm_form.c: the form's speeds, MM's boot data)
void Player_SetBootData(PlayState* play, Player* this);

/** z_nv_mm_form.c, each frame: the form's speeds (any form), and Deku Link's hops and last hop's spin. */
void NvMmDeku_Update(PlayState* play, Player* this) {
    static s32 sWasForm;
    static s32 sWasDeku;

    if ((this->actor.category == ACTORCAT_PLAYER) && NvMmForm_SetBootRegs(play)) {
        sWasForm = true;
    } else if (sWasForm) { // (human again: OoT's speeds)
        sWasForm = false;
        Player_SetBootData(play, this);
    }
    if (!NvMmDeku_Is(this)) {
        if (sWasDeku) { // (Deku no more: OoT's cylinder back)
            sWasDeku = false;
            NvMmDeku_ResetCylinder(this);
            if (sSavedAc >= 0) {
                this->cylinder.base.acFlags = sSavedAc;
                sSavedAc = -1;
            }
        }
        return;
    }
    sWasDeku = true;
    if ((this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && !(this->stateFlags1 & PLAYER_STATE1_IN_WATER)) {
        sHops = (getenv("NV_DEKU_NOHOPS") != NULL) ? 0 : DEKU_HOPS; // (MM: remainingHopsCounter, back to 5
        // on the ground; dev: NV_DEKU_NOHOPS, none, to try the sinking)
        if (sAirSpin) {
            sAirSpin = false;
            this->actor.shape.rot.y = this->yaw;
            NvMmDeku_ResetCylinder(this);
        }
    }
    if (sAirSpin && (this->actionFunc == Player_Action_8084411C)) { // MM's Player_Action_25, Deku
        s16 prevYaw = this->yaw;

        if (this->skelAnime.animation == ANIM_ATTACK) {
            NvMmDeku_SetSpinCylinder(this);
            sSpinSpeed += -800.0f;
            this->actor.shape.rot.y += (s16)((s16)sSpinSpeed + (s16)(this->yaw - prevYaw));
            Math_StepToF(&sSpinLeft, 0.0f, sSpinSpeed);
        }
    } else if (sAirSpin && (this->actionFunc != NvMmDeku_Sink)) {
        sAirSpin = false;
        NvMmDeku_ResetCylinder(this);
    }
}
