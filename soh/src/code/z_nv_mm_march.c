/*
 * Nightmareverse: the Bremen Mask's march and Kamaro's Mask's dance (mm/README.md "The Bremen march
 * and Kamaro's dance"), MM's Player_Action_11 and Player_Action_12.
 *
 * Worn by human Link, these masks turn B into MM's: holding B on the ground he marches (MM's
 * clink_normal_okarina_walk, walkB when moving; his steps rise and fall with the march's beat) or
 * dances (MM's alink_dance_loop, standing); letting go stands him back up. MM sets its
 * PLAYER_STATE3_20000000 (marching) and PLAYER_STATE2_2000000 (dancing) for others to see: here
 * NvMmMask_Marching and NvMmMask_Dancing, asked by whoever follows or watches (MM's dogs and Grog's
 * chicks follow the march; the Rosa sisters learn the dance; in Nightmareverse OoT's animals follow
 * the march too). Hooked from OoT's item buttons (z_player.c, one line): B with one of these masks.
 * Not here: MM's music for each (NA_BGM_BREMEN_MARCH, NA_BGM_KAMARO_DANCE), not in OoT's banks.
 */
#include "global.h"
#include <stdio.h>
#include <stdlib.h>

#define MMANIM(name) ((LinkAnimationHeader*)("__OTR__mm/player_anim/" name))
#define MM_PLAYER_MASK_KAMARO 0x0E
#define MM_PLAYER_MASK_BREMEN 0x11
#define MM_FORM_HUMAN 4

s32 NvMmMask_Worn(void);
s32 NvMmForm_Get(void);
s32 Player_SetupAction(PlayState* play, Player* this, PlayerActionFunc actionFunc, s32 flags);
void func_80839F90(Player* this, PlayState* play);
s32 Player_GetMovementSpeedAndYaw(Player* this, f32* outSpeedTarget, s16* outYawTarget, f32 speedMode,
                                  PlayState* play);
s32 Player_DecelerateToZero(Player* this);

static f32 sMarchSpeed; // MM's unk_B48: the speed before the beat's rise and fall

static void NvMmMarch_Log(const char* what) {
    if (getenv("NV_LOG") != NULL) {
        printf("NVMM %s\n", what);
        fflush(stdout);
    }
}

static s32 NvMmMarch_Stop(Player* this, PlayState* play) {
    if (!CHECK_BTN_ALL(play->state.input[0].cur.button, BTN_B) || !(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) ||
        (this->stateFlags1 & (PLAYER_STATE1_IN_WATER | PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_TALKING))) {
        this->linearVelocity = 0.0f;
        func_80839F90(this, play); // (MM's func_80839E74: standing again)
        NvMmMarch_Log("march/dance end");
        return true;
    }
    return false;
}

/** MM's Player_Action_11: marching. */
static void NvMmMarch_March(Player* this, PlayState* play) {
    f32 speedTarget;
    s16 yawTarget;
    f32 beat;
    f32 half;

    this->skelAnime.animation = (sMarchSpeed < 1.0f) ? MMANIM("gPlayerAnim_clink_normal_okarina_walk")
                                                     : MMANIM("gPlayerAnim_clink_normal_okarina_walkB");
    LinkAnimation_Update(play, &this->skelAnime);
    if (NvMmMarch_Stop(this, play)) {
        return;
    }
    Player_GetMovementSpeedAndYaw(this, &speedTarget, &yawTarget, 0.018f, play); // (SPEED_MODE_CURVED)
    Math_AsymStepToF(&sMarchSpeed, speedTarget, 1.5f, 1.5f);
    Math_ScaledStepToS(&this->yaw, yawTarget, 0x3E8);
    this->actor.shape.rot.y = this->yaw;
    // the beat (MM: effectively an fmodf over half the animation)
    beat = this->skelAnime.curFrame + 5.0f;
    half = this->skelAnime.animLength / 2.0f;
    beat -= half * (s32)(beat / half);
    this->linearVelocity = sMarchSpeed * Math_CosS(beat * 1000.0f) * 0.4f;
}

/** MM's Player_Action_12: dancing. */
static void NvMmMarch_Dance(Player* this, PlayState* play) {
    LinkAnimation_Update(play, &this->skelAnime);
    Player_DecelerateToZero(this);
    NvMmMarch_Stop(this, play);
}

/** B with the Bremen Mask or Kamaro's Mask on (MM's ITEM_F1, ITEM_F2): true if it started. */
s32 NvMmMask_ActionB(PlayState* play, Player* this) {
    s32 mask = NvMmMask_Worn();

    if ((NvMmForm_Get() != MM_FORM_HUMAN) || ((mask != MM_PLAYER_MASK_BREMEN) && (mask != MM_PLAYER_MASK_KAMARO)) ||
        !CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B) || !(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) ||
        (this->stateFlags1 & (PLAYER_STATE1_IN_WATER | PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_CARRYING_ACTOR))) {
        return false;
    }
    if (mask == MM_PLAYER_MASK_BREMEN) { // MM's func_80839978
        Player_SetupAction(play, this, NvMmMarch_March, 0);
        LinkAnimation_PlayLoop(play, &this->skelAnime, MMANIM("gPlayerAnim_clink_normal_okarina_walk"));
        sMarchSpeed = this->linearVelocity;
        NvMmMarch_Log("march");
    } else { // MM's func_80839A10
        Player_SetupAction(play, this, NvMmMarch_Dance, 0);
        LinkAnimation_PlayLoop(play, &this->skelAnime, MMANIM("gPlayerAnim_alink_dance_loop"));
        NvMmMarch_Log("dance");
    }
    this->yaw = this->actor.shape.rot.y;
    return true;
}

/** MM's PLAYER_STATE3_20000000: Link marching with the Bremen Mask. */
s32 NvMmMask_Marching(PlayState* play) {
    Player* player = GET_PLAYER(play);

    return (player != NULL) && (player->actionFunc == NvMmMarch_March);
}

/** MM's PLAYER_STATE2_2000000: Link dancing Kamaro's dance. */
s32 NvMmMask_Dancing(PlayState* play) {
    Player* player = GET_PLAYER(play);

    return (player != NULL) && (player->actionFunc == NvMmMarch_Dance);
}
