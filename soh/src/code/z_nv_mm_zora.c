/*
 * Nightmareverse: Zora Link's moves (mm/README.md "Zora Link"), MM's player code for the Zora form brought
 * onto OoT's Link as our own actions, as Deku Link's and Goron Link's are:
 * - B on land, the punches and kick (MM's pz_attackA, B, C: z_nv_mm_goron.c's shared chain, MM's frames);
 * - A held swimming, the fast swim (MM's func_80850734 and Player_Action_56): the dash (MM's water roll, a
 *   burst of 16 at its 13th frame), then gliding at 9 while A is held, the stick's up and down pitching
 *   him (MM's airplane way: up dives), its left and right turning him and rolling him over, kept under
 *   near the surface; let go, he slows into OoT's own swimming (MM's pz_swimtowait). His back fins are
 *   out while he glides (MM's D_801C0ABC, at his forearms);
 * - R held, the barrier (MM's func_8082F1AC), on land or in water, while there's magic: MM's light
 *   circling him (100, 200, 255), the hum; what touches him is stunned (OoT's Deku Nut, its nearest:
 *   MM's DMG_ZORA_BARRIER) and nothing hurts him through it. Its magic: 1 every 6 frames (MM drains it
 *   its own way; OoT's lens drain stops without the lens).
 * Started from z_nv_mm_deku.c's hooks (OoT's last free-standing handler; where OoT would start him
 * swimming, which our fast swim keeps it from) and each frame (z_nv_mm_form.c's Update).
 *
 * Not yet: the fins thrown (MM's boomerang, on B held: MM's En_Boom isn't ported), walking the bottom
 * (MM's Zora boots: B in water), the jump out of the water, the barrier's colour on the scene (MM's
 * sZoraBarrierEnvLighting), the guitar.
 */
#include "global.h"
#include <stdio.h>
#include <stdlib.h>

#define MMANIM(name) ((LinkAnimationHeader*)("__OTR__mm/player_anim/" name))
#define ANIM_WATERROLL MMANIM("gPlayerAnim_pz_waterroll")
#define ANIM_FISHSWIM MMANIM("gPlayerAnim_pz_fishswim")
#define ANIM_SWIMTOWAIT MMANIM("gPlayerAnim_pz_swimtowait")
#define MM_FORM_ZORA 2

s32 NvMmForm_Get(void);
s32 NvMmForm_StartPunch(PlayState* play, Player* this, s32 form);
void NvMmForm_BodyAttack(Player* this, u32 dmgFlags, u8 damage, s16 radius);
void NvMmForm_EndBodyAttack(Player* this);
s32 Player_SetupAction(PlayState* play, Player* this, PlayerActionFunc actionFunc, s32 flags);
s32 Player_GetMovementSpeedAndYaw(Player* this, f32* outSpeedTarget, s16* outYawTarget, f32 speedMode,
                                  PlayState* play);
void func_80838F18(PlayState* play, Player* this); // (OoT: back to swimming)

static s32 sDash;      // MM's av2.actionVar2: the dash's frames left
static s32 sEnding;    // MM's unk_B86[0]
static f32 sSpeed;     // MM's unk_B48
static s16 sPitch;     // MM's unk_AAA
static s16 sTurn;      // MM's unk_B8A
static s16 sRoll;      // MM's unk_B86[1]
static s16 sRollDraw;  // MM's unk_B8E
static s16 sBarrier;   // MM's unk_B62: 0-255
static s32 sBarrierDrain;
static LightInfo sBarrierLight;
static LightNode* sBarrierNode;
static PlayState* sBarrierPlay;
static s32 sBarrierWasOn;

static void NvMmZora_Log(const char* what) {
    if (getenv("NV_LOG") != NULL) {
        printf("NVMM zora: %s\n", what);
        fflush(stdout);
    }
}

static s32 NvMmZora_Is(Player* player) {
    return (NvMmForm_Get() == MM_FORM_ZORA) && (player->actor.category == ACTORCAT_PLAYER);
}

/* ---- the fast swim (MM's Player_Action_56) ---- */

static void NvMmZora_FastSwim(Player* this, PlayState* play);

/** z_nv_mm_deku.c's water hook: OoT would restart his swimming; not while he's gliding. */
s32 NvMmZora_InFastSwim(Player* this) {
    return this->actionFunc == NvMmZora_FastSwim;
}

static void NvMmZora_EndSwim(Player* this, PlayState* play) {
    this->actor.shape.rot.x = 0;
    this->actor.shape.rot.z = 0;
    sPitch = sRoll = sRollDraw = sTurn = 0;
    func_80838F18(play, this);
    NvMmZora_Log("swim end");
}

static void NvMmZora_FastSwim(Player* this, PlayState* play) {
    Input* input = &play->state.input[0];
    s32 held = CHECK_BTN_ALL(input->cur.button, BTN_A);
    f32 speedTarget = 0.0f;
    f32 c;
    s16 turn;

    if (!(this->stateFlags1 & PLAYER_STATE1_IN_WATER)) { // (out of the water: OoT's own takes over)
        this->actor.shape.rot.x = this->actor.shape.rot.z = 0;
        sPitch = sRoll = sRollDraw = 0;
        func_80838F18(play, this);
        return;
    }
    if (sDash != 0) { // the dash (MM's water roll)
        if (!held) {
            sEnding = true;
        }
        if (LinkAnimation_Update(play, &this->skelAnime) && (--sDash == 0)) {
            if (sEnding) {
                LinkAnimation_PlayOnce(play, &this->skelAnime, ANIM_SWIMTOWAIT);
            } else {
                LinkAnimation_PlayLoop(play, &this->skelAnime, ANIM_FISHSWIM);
            }
        } else {
            f32 st;
            s16 yawTarget;

            Player_GetMovementSpeedAndYaw(this, &st, &yawTarget, 0.0f, play);
            if (st > 0.0f) {
                Math_ScaledStepToS(&this->yaw, yawTarget, 0x640);
            }
            if (this->skelAnime.curFrame >= 13.0f) {
                speedTarget = 12.0f;
                if (LinkAnimation_OnFrame(&this->skelAnime, 13.0f)) {
                    sSpeed = 16.0f;
                }
            }
        }
        Math_SmoothStepToS(&sRoll, input->rel.stick_x * 0xC8, 0xA, 0x3E8, 0x64);
        Math_SmoothStepToS(&sRollDraw, sRoll, 1, 0x3E8, 0x64);
    } else if (!sEnding) { // gliding (MM's pz_fishswim)
        s16 pitchTarget = input->rel.stick_y * 0xC8;
        s16 turnTarget = input->rel.stick_x * 0x64;

        LinkAnimation_Update(play, &this->skelAnime);
        if (!held) {
            LinkAnimation_PlayOnce(play, &this->skelAnime, ANIM_SWIMTOWAIT);
            sEnding = true;
        } else {
            speedTarget = 9.0f;
            Actor_PlaySfx_Flagged2(&this->actor, NA_SE_PL_SWIM - SFX_FLAG); // (MM: NA_SE_PL_ZORA_SWIM_LV)
        }
        if ((sPitch >= -0x1555) && (this->actor.yDistToWater < (this->ageProperties->unk_24 + 10.0f))) {
            pitchTarget = CLAMP_MIN(pitchTarget, 0x7D0); // (near the surface: kept under)
        }
        Math_SmoothStepToS(&sPitch, pitchTarget, 4, 0xFA0, 0x190);
        if (Math_ScaledStepToS(&sTurn, turnTarget, 0x384) && (turnTarget == 0)) {
            Math_SmoothStepToS(&sRoll, 0, 4, 0x5DC, 0x64);
        } else {
            sRoll += sTurn; // (turning hard, he rolls over)
        }
        Math_SmoothStepToS(&sRollDraw, sRoll, 1, 0x3E8, 0x64);
    } else { // slowing into OoT's swimming
        Math_SmoothStepToS(&sRoll, 0, 4, 0xFA0, 0x190);
        sRollDraw = sRoll;
        Math_SmoothStepToS(&sPitch, 0, 4, 0xFA0, 0x190);
        if (LinkAnimation_Update(play, &this->skelAnime)) {
            NvMmZora_EndSwim(this, play);
            return;
        }
    }
    if (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) { // (off the bottom: MM's bounce up)
        sPitch += (s16)((-this->floorPitch - sPitch) * 2);
    }
    // MM's func_80850BF8 and func_80850BA8: the speed, the stick's turn, the speed along his pitch
    Math_AsymStepToF(&sSpeed, speedTarget, 1.0f, (fabsf(sSpeed) * 0.01f) + 0.4f);
    c = Math_CosS(input->rel.stick_x * 0x10E);
    turn = ((input->rel.stick_x >= 0) ? 1 : -1) * (1.0f - c) * -1100.0f;
    turn = CLAMP(turn, -0x1F40, 0x1F40);
    this->yaw += turn;
    this->linearVelocity = Math_CosS(sPitch) * sSpeed;
    this->actor.velocity.y = -Math_SinS(sPitch) * sSpeed;
    this->actor.gravity = 0.0f;
    this->actor.shape.rot.y = this->yaw;
    this->actor.shape.rot.x = sPitch;
    this->actor.shape.rot.z = sRollDraw;
}

/** MM's func_80850734: A held swimming, the dash starts. */
static void NvMmZora_StartFastSwim(PlayState* play, Player* this) {
    Player_SetupAction(play, this, NvMmZora_FastSwim, 0);
    LinkAnimation_Change(play, &this->skelAnime, ANIM_WATERROLL, 1.0f, 4.0f, Animation_GetLastFrame(ANIM_WATERROLL),
                         ANIMMODE_ONCE, -6.0f);
    sDash = 5;
    sEnding = false;
    sSpeed = this->linearVelocity;
    sPitch = sTurn = sRoll = sRollDraw = 0;
    this->actor.velocity.y = 0.0f;
    Player_PlaySfx(&this->actor, NA_SE_PL_DIVE_BUBBLE); // (MM: NA_SE_PL_ZORA_SWIM_DASH)
    NvMmZora_Log("fast swim");
}

/* ---- the barrier (MM's func_8082F1AC) ---- */

static void NvMmZora_Barrier(PlayState* play, Player* this) {
    s32 on = CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_R) && (gSaveContext.magic != 0) &&
             !(this->stateFlags1 & PLAYER_STATE1_IN_CUTSCENE);
    s32 target;

    if (sBarrierPlay != play) { // (a new scene: the old light went with it)
        sBarrierPlay = play;
        sBarrierNode = NULL;
    }
    if (on) {
        target = (gSaveContext.magic >= 16) ? 255 : (s32)((gSaveContext.magic / 16.0f) * 255.0f);
        Math_StepToS(&sBarrier, target, 50);
        if (++sBarrierDrain >= 6) {
            sBarrierDrain = 0;
            gSaveContext.magic = MAX(gSaveContext.magic - 1, 0);
        }
        if (!sBarrierWasOn) {
            sBarrierWasOn = true;
            NvMmZora_Log("barrier");
        }
    } else {
        Math_StepToS(&sBarrier, 0, 50);
        sBarrierWasOn = false;
    }
    if (sBarrier != 0) {
        s16 a = play->gameplayFrames * 7000;
        s16 b = play->gameplayFrames * 14000;
        f32 r = Math_SinS(b) * 40.0f;

        if (sBarrierNode == NULL) {
            sBarrierNode = LightContext_InsertLight(play, &play->lightCtx, &sBarrierLight);
        }
        Lights_PointNoGlowSetInfo(&sBarrierLight, this->actor.world.pos.x + Math_CosS(b) * 40.0f,
                                  this->actor.world.pos.y + Math_SinS(a) * r, this->actor.world.pos.z + Math_CosS(a) * r,
                                  100, 200, 255, 600 * sBarrier / 255);
        Actor_PlaySfx_Flagged2(&this->actor, NA_SE_IT_MAGIC_ARROW_SHOT - SFX_FLAG); // (MM: ZORA_SPARK_BARRIER)
        NvMmForm_BodyAttack(this, DMG_DEKU_NUT, 1, 40);
        this->cylinder.base.acFlags &= ~AC_ON; // (nothing hurts him through it)
    } else if (sBarrierNode != NULL) {
        LightContext_RemoveLight(play, &play->lightCtx, sBarrierNode);
        sBarrierNode = NULL;
        NvMmForm_EndBodyAttack(this);
        this->cylinder.base.acFlags |= AC_ON;
    }
}

/* ---- the hooks ---- */

/** z_nv_mm_deku.c's hook (OoT's last free-standing handler): B on land, the punches; true if started. */
s32 NvMmZora_ActionAB(PlayState* play, Player* this) {
    if (!NvMmZora_Is(this) || !(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) ||
        (this->stateFlags1 & (PLAYER_STATE1_IN_WATER | PLAYER_STATE1_ON_HORSE))) {
        return false;
    }
    if (CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B) && !CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_R)) {
        return NvMmForm_StartPunch(play, this, MM_FORM_ZORA);
    }
    return false;
}

/** z_nv_mm_form.c, each frame: the barrier, and the fast swim starting (A held, swimming). */
void NvMmZora_Update(PlayState* play, Player* this) {
    if (!NvMmZora_Is(this)) {
        if (sBarrier != 0) {
            sBarrier = 0;
            if ((sBarrierNode != NULL) && (sBarrierPlay == play)) {
                LightContext_RemoveLight(play, &play->lightCtx, sBarrierNode);
            }
            sBarrierNode = NULL;
            NvMmForm_EndBodyAttack(this);
            this->cylinder.base.acFlags |= AC_ON;
        }
        return;
    }
    NvMmZora_Barrier(play, this);
    if ((this->stateFlags1 & PLAYER_STATE1_IN_WATER) && !(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) &&
        (this->actor.yDistToWater > this->ageProperties->unk_2C) && (this->actionFunc != NvMmZora_FastSwim) &&
        !(this->stateFlags1 & PLAYER_STATE1_IN_CUTSCENE) && CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_A)) {
        NvMmZora_StartFastSwim(play, this);
    }
}

/** z_nv_mm_form.c's limb hook: his back fins out while he glides (MM's D_801C0ABC). */
s32 NvMmZora_SwimFinsOut(Player* this) {
    return NvMmZora_Is(this) && (this->actionFunc == NvMmZora_FastSwim) &&
           ((this->skelAnime.animation == ANIM_FISHSWIM) || (this->skelAnime.animation == ANIM_WATERROLL));
}
