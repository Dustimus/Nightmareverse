/*
 * Nightmareverse: Goron Link's moves (mm/README.md "Goron Link"), MM's player code for the Goron
 * form brought onto OoT's Link as our own actions, as z_nv_mm_deku.c does Deku Link's, started from the
 * same one-line hook in z_player.c (OoT's last free-standing handler: A and B):
 * - A, the roll (MM's func_80839F98, func_80836B3C, func_80857A44 and Player_Action_96): he curls up
 *   (MM's pg_maru_change, or straight from a run), rolls while A is held, steered (MM's speed from the
 *   stick times 2.6, his turn quicker the faster he goes), bouncing off walls at speed (MM's 12 and
 *   up), and after rolling hard for MM's 54 frames, with magic, grows spikes and goes MM's 18;
 * - B while rolling, the pound (MM's func_80857640, func_80857AEC): a hop (MM's 14), a hang at the top,
 *   a slam (MM's gravity -10), a shockwave and a quake where he lands, hitting all round (MM's 4,
 *   radius 60);
 * - B standing, the punches (MM's pg_punchA, B, C: B again in time chains the next).
 * Curled, he's drawn as MM draws him: the ball (gLinkGoronCurledDL) at his middle, turning as he rolls,
 * squashed and stretched, the spikes (MM's lg_spike_model) on it when charged, in MM's colours.
 *
 * Adapted from MM: the hits are OoT's nearest kinds (OoT has no Goron damage): the roll as the Deku
 * Stick, the spikes and the punches as the hammer's swing, the pound as the hammer's jump; the spikes'
 * magic goes 2 at once and 1 every 10 frames after (MM drains it its own way, OoT's lens drain stops
 * without the lens); the roll's slope physics, its dust and footprints, MM's punch effect and the
 * fists' hit spots (here his body's cylinder, reaching out), and the sounds (OoT's nearest) are simpler.
 */
#include "global.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include <stdio.h>
#include <stdlib.h>

#define MMANIM(name) ((LinkAnimationHeader*)("__OTR__mm/player_anim/" name))
#define GORON(name) ((Gfx*)("__OTR__mm/objects/object_link_goron/" name))
#define ANIM_MARU_CHANGE MMANIM("gPlayerAnim_pg_maru_change")
#define MM_FORM_GORON 1

s32 NvMmForm_Get(void);
s32 Player_SetupAction(PlayState* play, Player* this, PlayerActionFunc actionFunc, s32 flags);
void func_80839F90(Player* this, PlayState* play);
s32 Player_GetMovementSpeedAndYaw(Player* this, f32* outSpeedTarget, s16* outYawTarget, f32 speedMode,
                                  PlayState* play);
void Player_AnimChangeOnceMorph(PlayState* play, Player* this, LinkAnimationHeader* anim);
s32 Player_DecelerateToZero(Player* this);
LinkAnimationHeader* Player_GetIdleAnim(Player* this);

static s32 sStage;      // MM's av1.actionVar1: 0 curling, 1 pounding (up), 2 landed from the pound, 4 rolling
static s32 sRollRot;    // MM's av2.actionVar2: the ball's turn a frame
static s16 sBallPitch;  // the ball's turn (MM: shape.rot.x)
static f32 sSquash;     // MM's unk_ABC: the ball squashed (-) or stretched (+)
static f32 sSquashVel;  // MM's unk_B48
static s32 sCharge;     // MM's av1.actionVar1 counting up while rolling hard (54: the spikes)
static s32 sSpikes;     // MM's unk_B86[1]: the spikes, growing (1, 2) then out
static s32 sPoundBounces; // MM's unk_B8A
static s32 sNoSteer;    // MM's unk_B8C: frames he can't steer (a wall bounce)
static s32 sDrainTimer;
static u32 sSavedDmgFlags;
static u8 sSavedDamage;
static s16 sSavedRadius;
static s32 sCylinderSet;

static void NvMmGoron_Log(const char* what) {
    if (getenv("NV_LOG") != NULL) {
        printf("NVMM goron: %s\n", what);
        fflush(stdout);
    }
}

static s32 NvMmGoron_Is(Player* player) {
    return (NvMmForm_Get() == MM_FORM_GORON) && (player->actor.category == ACTORCAT_PLAYER);
}

/** MM's Player_SetCylinderForAttack, through OoT's own way of letting Link's body cylinder hit. */
static void NvMmGoron_Attack(Player* this, u32 dmgFlags, u8 damage, s16 radius) {
    if (!sCylinderSet) {
        sSavedDmgFlags = this->cylinder.info.toucher.dmgFlags;
        sSavedDamage = this->cylinder.info.toucher.damage;
        sSavedRadius = this->cylinder.dim.radius;
        sCylinderSet = true;
    }
    this->cylinder.info.toucher.dmgFlags = dmgFlags;
    this->cylinder.info.toucher.damage = damage;
    this->cylinder.info.toucherFlags = TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NORMAL;
    this->cylinder.base.atFlags = AT_ON | AT_TYPE_PLAYER;
    this->cylinder.dim.radius = radius;
    if (this->invincibilityTimer >= -1) {
        this->invincibilityTimer = -2;
    }
}

static void NvMmGoron_EndAttack(Player* this) {
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

static void NvMmGoron_EndSpikes(PlayState* play) {
    if (sSpikes != 0) {
        sSpikes = 0;
        Magic_Reset(play);
    }
}

/* ---- the roll (MM's Player_Action_96) ---- */

static void NvMmGoron_Roll(Player* this, PlayState* play);

/** MM's func_80857950: A let go (no spikes) or the pound's hop coming down: uncurled. */
static s32 NvMmGoron_TryUncurl(PlayState* play, Player* this) {
    if ((sSpikes == 0) && !CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_A) && (sStage != 1)) {
        NvMmGoron_EndAttack(this);
        this->stateFlags3 &= ~PLAYER_STATE3_MIDAIR;
        this->actor.shape.rot.x = this->actor.shape.rot.z = 0;
        this->actor.gravity = REG(68) / 100.0f;
        func_80839F90(this, play);
        LinkAnimation_Change(play, &this->skelAnime, ANIM_MARU_CHANGE, -1.0f, 7.0f, 0.0f, ANIMMODE_ONCE, 0.0f);
        Player_PlaySfx(&this->actor, NA_SE_PL_CHANGE_ARMS); // (MM: NA_SE_PL_BALL_TO_GORON)
        NvMmGoron_Log("uncurl");
        return true;
    }
    return false;
}

/** MM's func_808577E0: the ball squashing and stretching with its turn. */
static void NvMmGoron_Squash(void) {
    f32 target = ABS(sRollRot) * 0.00004f;

    sSquashVel += (sSquash < target) ? 0.08f : -0.07f;
    sSquashVel = CLAMP(sSquashVel, -0.2f, 0.14f);
    if (fabsf(sSquashVel) < 0.12f) {
        if (Math_StepToF(&sSquash, target, fabsf(sSquashVel))) {
            sSquashVel = 0.0f;
        }
    } else {
        sSquash += sSquashVel;
        sSquash = CLAMP(sSquash, -0.7f, 0.3f);
    }
}

/** MM's func_80857AEC: touching down; from the pound, the slam. */
static void NvMmGoron_Land(PlayState* play, Player* this) {
    if (!(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND_TOUCH)) {
        return;
    }
    if ((sSpikes == 0) && (sStage == 1)) {
        s16 quake = Quake_Add(GET_ACTIVE_CAM(play), 3);

        sStage = 2;
        sPoundBounces = 0;
        Quake_SetSpeed(quake, 25000); // (MM's Player_RequestQuakeAndRumble)
        Quake_SetQuakeValues(quake, 6, 0, 0, 0);
        Quake_SetCountdown(quake, 8);
        Player_PlaySfx(&this->actor, NA_SE_IT_HAMMER_HIT); // (MM: NA_SE_PL_GORON_PUNCH)
        {
            Vec3f zero = { 0.0f, 0.0f, 0.0f };

            EffectSsBlast_SpawnWhiteShockwave(play, &this->actor.world.pos, &zero, &zero);
        }
        sRollRot = 0;
        this->linearVelocity = 0.0f;
        NvMmGoron_Log("pound");
    } else if (sStage != 2) {
        sStage = 4;
    }
}

static void NvMmGoron_Roll(Player* this, PlayState* play) {
    Input* input = &play->state.input[0];
    f32 speedTarget = 0.0f;
    s16 yawTarget = this->yaw;
    s32 rotTarget;

    this->stateFlags3 |= PLAYER_STATE3_MIDAIR; // (OoT: not a fall of its own when he rolls off an edge)
    if (sStage == 0) { // curling up (MM's func_80857A44)
        if (!LinkAnimation_Update(play, &this->skelAnime)) {
            return;
        }
        sStage = 4;
        sRollRot = this->linearVelocity * 500.0f;
        NvMmGoron_Log("rolling");
    }
    NvMmGoron_Squash();
    if (NvMmGoron_TryUncurl(play, this)) {
        return;
    }
    if (sNoSteer == 0) {
        Player_GetMovementSpeedAndYaw(this, &speedTarget, &yawTarget, 0.0f, play); // (SPEED_MODE_LINEAR)
        speedTarget *= 2.6f;
    } else {
        sNoSteer--;
        yawTarget = this->yaw;
    }
    if ((this->actor.bgCheckFlags & BGCHECKFLAG_WALL) && (this->linearVelocity >= 12.0f)) { // off a wall
        s16 d = this->yaw - (this->actor.wallYaw + 0x8000);

        this->yaw += 0x8000 - 2 * d;
        this->actor.shape.rot.y = this->yaw;
        sNoSteer = 4;
        this->linearVelocity *= 0.8f;
        Player_PlaySfx(&this->actor, NA_SE_IT_SHIELD_REFLECT_SW); // (MM: NA_SE_IT_GORON_ROLLING_REFLECTION)
        if (sSpikes != 0) { // (MM: a hard bump takes the spikes off)
            NvMmGoron_EndSpikes(play);
            sCharge = 4;
        }
    }
    if (sSpikes != 0) { // spiked: MM's 18, A held, magic left
        speedTarget = 18.0f;
        if (!CHECK_BTN_ALL(input->cur.button, BTN_A) || (gSaveContext.magic == 0)) {
            NvMmGoron_EndSpikes(play);
            sCharge = 4;
            Player_PlaySfx(&this->actor, NA_SE_SY_ERROR); // (MM: NA_SE_PL_GORON_BALL_CHARGE_FAILED)
        } else {
            if (sSpikes < 3) {
                sSpikes++;
            }
            if (++sDrainTimer >= 10) {
                sDrainTimer = 0;
                gSaveContext.magic = MAX(gSaveContext.magic - 1, 0);
            }
        }
    }
    rotTarget = speedTarget * 900.0f;
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
        NvMmGoron_Land(play, this);
        if (sStage == 2) { // after the pound: a few small bounces, then rolling on
            if (++sPoundBounces > 10) {
                sStage = 4;
            }
        } else if ((sSpikes == 0) && CHECK_BTN_ALL(input->press.button, BTN_B)) { // the pound's hop
            this->actor.velocity.y = 14.0f;
            this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
            Player_PlaySfx(&this->actor, NA_SE_PL_JUMP); // (MM: NA_SE_PL_GORON_BALLJUMP)
            this->linearVelocity = 0.0f;
            sRollRot = MAX(sRollRot, 0x1F40);
            sStage = 1;
            sSquashVel = 1.0f;
            NvMmGoron_Log("hop");
        } else {
            s16 turn = (s16)(fabsf(this->linearVelocity) * 20.0f) + 300;

            if ((sSpikes == 0) && (gSaveContext.magicState == 0) && (gSaveContext.magic >= 2) &&
                ((sRollRot >= 0x36B0) || (getenv("NV_GORON_SPIKES") != NULL))) {
                // rolling hard: charging the spikes (MM's count, from 4 to 0x36; dev: NV_GORON_SPIKES, any roll)
                if (++sCharge >= 0x36) {
                    if (Magic_RequestChange(play, 2, MAGIC_CONSUME_NOW)) {
                        sSpikes = 1;
                        sDrainTimer = 0;
                        this->linearVelocity = 18.0f;
                        Player_PlaySfx(&this->actor, NA_SE_IT_SWORD_CHARGE); // (MM: GORON_BALL_CHARGE_DASH)
                        NvMmGoron_Log("spikes");
                    }
                    sCharge = 4;
                }
            } else if (sSpikes == 0) {
                sCharge = 4;
            }
            turn = MAX(turn, 100);
            Math_AsymStepToF(&this->linearVelocity, speedTarget, CLAMP_MIN(sRollRot * 0.0003f, 0.05f),
                             (Math_SinS(this->floorPitch) * 8.0f) + 0.6f);
            Math_ScaledStepToS(&this->yaw, yawTarget, turn);
        }
    } else { // in the air
        this->actor.shape.rot.z = 0;
        if (sStage == 1) { // the pound: a hang at the top, then down hard (MM's)
            if (this->actor.velocity.y > 0.0f) {
                if ((this->actor.velocity.y + this->actor.gravity) < 0.0f) {
                    this->actor.velocity.y = -this->actor.gravity;
                }
            } else if (this->actor.velocity.y > -1.0f) {
                this->actor.gravity = -0.2f;
            } else {
                this->actor.gravity = -10.0f;
            }
        }
    }
    // the ball's turn (MM's Math_AsymStepToS on actionVar2)
    if (sStage != 1) {
        if (rotTarget > sRollRot) {
            sRollRot = MIN(sRollRot + 0x7D0, rotTarget);
        } else {
            sRollRot = MAX(sRollRot - 0x4B0, rotTarget);
        }
    }
    sBallPitch += sRollRot;
    this->actor.shape.rot.y = this->yaw;
    // what he hits (MM: the pound 4 at 60, the spikes 1, the roll 1)
    if (sStage == 2) {
        NvMmGoron_Attack(this, DMG_HAMMER_JUMP, 4, 60);
    } else if (sSpikes != 0) {
        NvMmGoron_Attack(this, DMG_HAMMER_SWING, 2, 25);
    } else if (fabsf(this->linearVelocity) > 4.0f) {
        NvMmGoron_Attack(this, DMG_DEKU_STICK, 1, 25);
    } else {
        NvMmGoron_EndAttack(this);
    }
}

/** MM's func_80836AD8: curling up (from standing, MM's pg_maru_change; running, straight into it). */
static void NvMmGoron_StartRoll(PlayState* play, Player* this) {
    Player_SetupAction(play, this, NvMmGoron_Roll, 0);
    this->stateFlags1 &= ~(PLAYER_STATE1_PARALLEL | PLAYER_STATE1_LOCK_ON_FORCED_TO_RELEASE);
    sStage = 0;
    sSpikes = 0;
    sCharge = 4;
    sSquash = sSquashVel = 0.0f;
    sNoSteer = 0;
    if (this->linearVelocity != 0.0f) {
        this->yaw = this->actor.shape.rot.y;
        LinkAnimation_Change(play, &this->skelAnime, &gPlayerAnim_link_normal_landing_roll, 1.5f, 0.0f, 6.0f,
                             ANIMMODE_ONCE, 0.0f);
    } else {
        LinkAnimation_Change(play, &this->skelAnime, ANIM_MARU_CHANGE, 1.0f, 0.0f, 7.0f, ANIMMODE_ONCE, 0.0f);
    }
    Player_PlaySfx(&this->actor, NA_SE_PL_ROLL); // (MM: NA_SE_PL_GORON_TO_BALL)
    NvMmGoron_Log("curl");
}

/* ---- the punches (MM's Goron melee: pg_punchA, B, C) ---- */

static s32 sPunch;    // 0, 1, 2: A, B, C
static s32 sPunchNext;

static void NvMmGoron_Punch(Player* this, PlayState* play) {
    static LinkAnimationHeader* sPunches[] = { MMANIM("gPlayerAnim_pg_punchA"), MMANIM("gPlayerAnim_pg_punchB"),
                                                MMANIM("gPlayerAnim_pg_punchC") };
    static LinkAnimationHeader* sEnds[] = { MMANIM("gPlayerAnim_pg_punchAend"), MMANIM("gPlayerAnim_pg_punchBend"),
                                             MMANIM("gPlayerAnim_pg_punchCend") };
    s32 done = LinkAnimation_Update(play, &this->skelAnime);
    f32 frame = this->skelAnime.curFrame;

    Player_DecelerateToZero(this);
    if (CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B) && (sPunch < 2) && (frame > 3.0f)) {
        sPunchNext = true;
    }
    if ((frame >= 2.0f) && (frame <= 7.0f) && !done) { // (the fist out: MM's hit frames, about)
        NvMmGoron_Attack(this, DMG_HAMMER_SWING, 2, 40);
    } else {
        NvMmGoron_EndAttack(this);
    }
    if (done) {
        if (sPunchNext) { // the next punch in the chain
            sPunchNext = false;
            sPunch++;
            LinkAnimation_PlayOnce(play, &this->skelAnime, sPunches[sPunch]);
            Player_PlaySfx(&this->actor, NA_SE_IT_HAMMER_SWING);
            NvMmGoron_Log("punch");
        } else if (sPunch >= 0) { // the end of the swing, then standing
            LinkAnimation_PlayOnce(play, &this->skelAnime, sEnds[sPunch]);
            sPunch = -1;
        } else {
            NvMmGoron_EndAttack(this);
            func_80839F90(this, play);
        }
    }
}

/* ---- the hook (z_nv_mm_deku.c's: OoT's last free-standing handler) ---- */

/** A and B as Goron Link, standing or moving on the ground; true if something started. */
s32 NvMmGoron_ActionAB(PlayState* play, Player* this) {
    Input* input = &play->state.input[0];

    if (!NvMmGoron_Is(this) || !(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) ||
        (this->stateFlags1 & (PLAYER_STATE1_IN_WATER | PLAYER_STATE1_ON_HORSE))) {
        return false;
    }
    if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
        NvMmGoron_StartRoll(play, this);
        return true;
    }
    if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
        Player_SetupAction(play, this, NvMmGoron_Punch, 0);
        sPunch = 0;
        sPunchNext = false;
        LinkAnimation_PlayOnce(play, &this->skelAnime, MMANIM("gPlayerAnim_pg_punchA"));
        Player_PlaySfx(&this->actor, NA_SE_IT_HAMMER_SWING); // (MM: NA_SE_VO_LI_SWORD_N, the punch)
        NvMmGoron_Log("punch");
        return true;
    }
    return false;
}

/** z_nv_mm_form.c's limb hook: curled, his body isn't drawn (the ball is, below). */
s32 NvMmGoron_Curled(Player* this) {
    return NvMmGoron_Is(this) && (this->actionFunc == NvMmGoron_Roll) && (sStage != 0);
}

/** z_nv_mm_mask.c's head hook: the ball (MM's Player_Draw for PLAYER_STATE3_1000), in the world's space. */
void NvMmGoron_DrawBall(PlayState* play, Player* this) {
    f32 stretch = sSquash + 1.0f;
    f32 squash = 1.0f - (sSquash * 0.5f);
    f32 mix = (sStage == 2) ? 1.0f : 0.0f; // (MM's unk_B10[0]: blue after the pound, MM's 80,80,200)

    if (!NvMmGoron_Curled(this)) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Translate(this->actor.world.pos.x, this->actor.world.pos.y + (1200.0f * this->actor.scale.y * stretch),
                     this->actor.world.pos.z, MTXMODE_NEW);
    Matrix_RotateY(BINANG_TO_RAD(this->actor.shape.rot.y), MTXMODE_APPLY);
    Matrix_Scale(this->actor.scale.x * squash * 1.15f, this->actor.scale.y * stretch * 1.15f,
                 CLAMP_MIN(stretch, squash) * this->actor.scale.z * 1.15f, MTXMODE_APPLY);
    Matrix_RotateX(BINANG_TO_RAD(sBallPitch), MTXMODE_APPLY);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gDPSetEnvColor(POLY_OPA_DISP++, 255 - (s32)(175 * mix), 255 - (s32)(175 * mix), 255 - (s32)(55 * mix), 255);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, GORON("gLinkGoronCurledDL"));
    if (sSpikes != 0) {
        if (sSpikes < 3) { // (growing out)
            Matrix_Scale(sSpikes * 0.5f, sSpikes * 0.5f, sSpikes * 0.5f, MTXMODE_APPLY);
            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        }
        gSPDisplayList(POLY_OPA_DISP++, GORON("gLinkGoronSpikesDL"));
    }
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}
