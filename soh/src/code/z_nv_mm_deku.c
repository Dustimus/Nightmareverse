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

/* ---- the hooks (z_player.c) ---- */

/** OoT's roll handler (A): MM's Deku spin, on the ground and out of the water. */
s32 NvMmDeku_ActionA(PlayState* play, Player* this) {
    if (!NvMmDeku_Is(this) || !CHECK_BTN_ALL(play->state.input[0].press.button, BTN_A) ||
        !(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) || (this->stateFlags1 & PLAYER_STATE1_IN_WATER) ||
        (this->stateFlags1 & PLAYER_STATE1_ON_HORSE)) {
        return false;
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

/** Where OoT would start Link swimming: Deku Link hops off the water, or sinks; true if handled. */
s32 NvMmDeku_IntoWater(PlayState* play, Player* this) {
    LinkAnimationHeader* anim;
    f32 speed;
    s16 yawDiff;

    if (!NvMmDeku_Is(this)) {
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

// MM's boot data for PLAYER_BOOTS_DEKU, on OoT's registers (OoT has no REG(39): MM's 9th left out)
static const s16 sDekuBootRegs[] = { 200, 1000, 300, 700, 550, 270, 600, 1000, 800, 600, -100, 600, 590, 750, 125, 200, 130 };

static void NvMmDeku_SetBootRegs(PlayState* play) {
    const s16* r = sDekuBootRegs;

    REG(27) = 2000;
    REG(48) = 370;
    REG(19) = r[0];
    REG(30) = r[1];
    REG(32) = r[2];
    REG(34) = r[3];
    REG(35) = r[4];
    REG(36) = r[5];
    REG(37) = r[6];
    REG(38) = r[7];
    REG(43) = r[8]; // (R_DECELERATE_RATE)
    REG(45) = r[9]; // (R_RUN_SPEED_LIMIT)
    REG(68) = r[10];
    REG(69) = r[11];
    IREG(66) = r[12];
    IREG(67) = r[13];
    IREG(68) = r[14];
    IREG(69) = r[15];
    MREG(95) = r[16];
    if (play->roomCtx.curRoom.behaviorType1 == ROOM_BEHAVIOR_TYPE1_2) { // (indoors, as OoT and MM)
        REG(45) = 500;
    }
}

void Player_SetBootData(PlayState* play, Player* this);

/** z_nv_mm_form.c, each frame: the hops, the last hop's spin, MM's Deku speeds. */
void NvMmDeku_Update(PlayState* play, Player* this) {
    static s32 sWasDeku;

    if (!NvMmDeku_Is(this)) {
        if (sWasDeku) { // (human again: OoT's speeds, OoT's cylinder)
            sWasDeku = false;
            NvMmDeku_ResetCylinder(this);
            if (sSavedAc >= 0) {
                this->cylinder.base.acFlags = sSavedAc;
                sSavedAc = -1;
            }
            Player_SetBootData(play, this);
        }
        return;
    }
    sWasDeku = true;
    NvMmDeku_SetBootRegs(play);
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
