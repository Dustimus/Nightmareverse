/*
 * Nightmareverse: the Bombers' hide-and-seek, MM's own, ported closely (mm/README.md "The Bombers'
 * game"): four of MM's actors, the Bombers' model (object_cs) as in z_nv_mm_bombers.c.
 *
 *   En_Bombal (0x282)       the Majora balloon over North Clock Town, Jim shooting at it with his
 *                           blowgun (missing); Link pops it with an arrow, a seed or Deku Link's bubble.
 *   En_Bomjima (0x27E)      Jim: with the balloon popped, MM's lines by Link's form; yes to the game,
 *                           the four others run in, then the game starts (North Clock Town again,
 *                           spawn 6). Params >> 4: 0 Jim, 2 one of the four running in (& 0xF its number).
 *   En_Bomjimb (0x27F)      the five hidden around town while the game's on (week events 73_10 for
 *                           Deku Link's game, 85_02 for a human Link's): wandering, running from Link
 *                           once he's near, taunting from afar, hiding under MM's crates (Obj_Kibako),
 *                           one riding a cucco down from a roof; caught by touching them (MM's lines
 *                           by the count, MM's caught order kept); the fifth takes Link back to the
 *                           hideout (the room's exit, week events 75_40, 83_04). Hit, they're dazed.
 *   En_Bom_Bowl_Man (0x11C) after the game, at the hideout: the five line up by the Bombers' code,
 *                           each showing its number in turn, Jim gives the notebook (a human Link: MM's
 *                           Bombers' Notebook) and says the code; Deku Link is told the code only
 *                           (73_20). Spawned by the sitting Bomber number 1 (z_nv_mm_bombers.c, 75_40).
 *                           In East Clock Town (params >> 4 == 1): coming out of the hideout with the
 *                           code entered and no notebook, one runs up and gives it, then runs off.
 * Adapted from MM: MM's camera cutscenes (the balloon popping, the four running in, the line-up, the
 * run-off) are left out, their actors' parts played as they are; MM's Bombers' sounds aren't in
 * OoT's banks (OoT's stand in or none); Deku Link hiding in a flower (MM's PLAYER_STATE3_1000000,
 * which makes them stop running) waits for that; the cucco a Bomber rides is OoT's.
 */
#include "global.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define MM_CS(name) ("__OTR__mm/objects/object_cs/" name)
#define FUSEN(name) ("__OTR__mm/objects/object_fusen/" name)
#define NV_MM_TEXT(id) (0xB000 + (id))
#define MM_WE(byte, bit) (((byte) << 8) | (bit))

s32 NvMm_CheckWeekEvent(s32 flag);
void NvMm_SetWeekEvent(s32 flag);
void NvMm_ClearWeekEvent(s32 flag);
void NvMm_NotebookEvent(u16 event);
s32 NvMmForm_Get(void); // 0 FD, 1 Goron, 2 Zora, 3 Deku, 4 human
s32 NvMmActor_Id(s32 mmId);
s32 NvMmItem_Has(s32 item);
GetItemEntry NvMmGetItem_Entry(s32 mmGi);
u8* NvMmMsg_BomberCode(void);

enum { FORM_FD, FORM_GORON, FORM_ZORA, FORM_DEKU, FORM_HUMAN };

void NvMmBomjimb_Draw(Actor* thisx, PlayState* play);
void NvMmBomBowl_Draw(Actor* thisx, PlayState* play);

#define WE_11_01 MM_WE(11, 0x01) // (caught, this game: numbers 1-5)
#define WE_73_10 MM_WE(73, 0x10) // Deku Link's game on
#define WE_73_20 MM_WE(73, 0x20) // Deku Link told the code
#define WE_75_40 MM_WE(75, 0x40) // the game won: to the hideout
#define WE_76_01 MM_WE(76, 0x01) // (caught: they sit at the hideout)
#define WE_77_02 MM_WE(77, 0x02) // Deku Link's game played
#define WE_83_04 MM_WE(83, 0x04)
#define WE_84_80 MM_WE(84, 0x80) // the Bombers' Notebook
#define WE_85_01 MM_WE(85, 0x01) // a human Link's game played
#define WE_85_02 MM_WE(85, 0x02) // a human Link's game on
#define WE_ENTERED_BOMBERS_CODE MM_WE(73, 0x80)
#define NOTEBOOK_MET_BOMBERS 0x00
#define NOTEBOOK_LEARNED_SECRET_CODE 0x24
#define NOTEBOOK_RECEIVED_BOMBERS_NOTEBOOK 0x25
#define MM_GI_BOMBERS_NOTEBOOK 0x50
#define MM_ITEM_BOMBERS_NOTEBOOK 0x6D

static Vec3f sZero = { 0.0f, 0.0f, 0.0f };

#define BOMBER_LIMB_08 0x08
#define BOMBER_LIMB_0F 0x0F
#define BOMBER_LIMB_HEAD 0x11
#define BOMBER_LIMB_13 0x13
#define BOMBER_LIMB_14 0x14
#define BOMBER_LIMB_MAX 21

/* ---- the game's state (MM's saveInfo.bombersCaughtNum, bombersCaughtOrder): saved with the cycle ---- */
static u8 sCaught[6]; // [0] the count, [1..5] the order

u8* NvMmBomGame_Caught(void) {
    return sCaught;
}

static s32 NvMmBomGame_On(void) {
    return NvMm_CheckWeekEvent(WE_73_10) || NvMm_CheckWeekEvent(WE_85_02);
}

static void NvMmBomGame_Log(const char* what, s32 n) {
    if (getenv("NV_LOG") != NULL) {
        printf("NVMM bombers' game: %s %d\n", what, (int)n);
        fflush(stdout);
    }
}

/* ---- shared: the Bombers' model ---- */

enum {
    BA_IDLE, BA_HANDS_ON_HIPS, BA_TALK_NORMAL, BA_TALK_HANDS_ON_HIPS, BA_BLOWGUN, BA_WALK, BA_SURPRISE,
    BA_HIDE_UNDER_BOX, BA_DAZED, BA_SIT, BA_CLIMB, BA_HOLD_CUCCO, BA_TURN_AROUND, BA_SHOW_NUMBER,
    BA_TALK_SHOWING_NUMBER, BA_TAUNT, BA_JUMP, BA_ARM_SWIPE, BA_SALUTE, BA_RUN, BA_LOOK_AROUND, BA_CAUGHT, BA_MAX
};

static const char* sBAnims[BA_MAX] = {
    MM_CS("gBomberIdleAnim"),         MM_CS("gBomberHandsOnHipsAnim"),   MM_CS("gBomberTalkNormalAnim"),
    MM_CS("gBomberTalkHandsOnHipsAnim"), MM_CS("gBomberBlowgunAnim"),    MM_CS("gBomberWalkAnim"),
    MM_CS("gBomberSurpriseAnim"),     MM_CS("gBomberHideUnderBoxAnim"),  MM_CS("gBomberDazedAnim"),
    MM_CS("gBomberSitAnim"),          MM_CS("gBomberClimbAnim"),         MM_CS("gBomberHoldCuccoAnim"),
    MM_CS("gBomberTurnAroundAnim"),   MM_CS("gBomberShowNumberAnim"),    MM_CS("gBomberTalkShowingNumberAnim"),
    MM_CS("gBomberTauntAnim"),        MM_CS("gBomberJumpAnim"),          MM_CS("gBomberArmSwipeAnim"),
    MM_CS("gBomberSaluteAnim"),       MM_CS("gBomberRunAnim"),           MM_CS("gBomberLookAroundAnim"),
    MM_CS("gBomberCaughtAnim"),
};
static const u8 sBModes[BA_MAX] = {
    ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_ONCE,
    ANIMMODE_ONCE, ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_ONCE, ANIMMODE_LOOP,
    ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_ONCE, ANIMMODE_LOOP, ANIMMODE_ONCE, ANIMMODE_LOOP, ANIMMODE_LOOP,
    ANIMMODE_ONCE,
};
static const char* sBEyes[] = { MM_CS("gBomberEyeOpenTex"), MM_CS("gBomberEyeHalfTex"), MM_CS("gBomberEyeClosedTex") };
static const char* sBNumbers[] = { MM_CS("gBomberNumber1Tex"), MM_CS("gBomberNumber2Tex"), MM_CS("gBomberNumber3Tex"),
                                   MM_CS("gBomberNumber4Tex"), MM_CS("gBomberNumber5Tex") };
// (the overlays' own two lists: the bandana, red for number 1, blue for the rest)
static Gfx sBRedDL[] = { gsDPSetPrimColor(0, 0, 0x84, 0x00, 0x00, 0xFF), gsSPEndDisplayList() };
static Gfx sBBlueDL[] = { gsDPSetPrimColor(0, 0, 0x00, 0x5A, 0xFF, 0xFF), gsSPEndDisplayList() };
static Gfx sBEmptyDL[] = { gsSPEndDisplayList() };

typedef struct {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[BOMBER_LIMB_MAX];
    Vec3s morphTable[BOMBER_LIMB_MAX];
    s32 animIndex;
    f32 animEndFrame;
    s16 eyeIndex;
    s16 blinkTimer;
    s16 headRotX, headRotZ, headRotTargetX, headRotTargetZ;
    s16 torsoRotZ, torsoRotTargetZ;
    s16 number;     // 0-4: the number shown, the bandana
    u8 hideLimb13;  // (MM's: the blowgun, the box... by kind)
    u8 hideLimb14;
    u8 hideLimb0F;
    ColliderCylinder collider;
} NvMmBomber;

static ColliderCylinderInit sBCylinderInit = {
    { COLTYPE_NONE, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_2, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0xF7CFFFFF, 0x00, 0x00 }, TOUCH_NONE | TOUCH_SFX_NORMAL, BUMP_ON,
      OCELEM_ON },
    { 20, 30, 0, { 0, 0, 0 } },
};

static void NvMmBomber_Init(NvMmBomber* b, PlayState* play) {
    b->actor.colChkInfo.mass = MASS_IMMOVABLE;
    ActorShape_Init(&b->actor.shape, 0.0f, ActorShadow_DrawCircle, 19.0f);
    SkelAnime_InitFlex(play, &b->skelAnime, (FlexSkeletonHeader*)MM_CS("gBomberSkel"),
                       (AnimationHeader*)sBAnims[BA_IDLE], b->jointTable, b->morphTable, BOMBER_LIMB_MAX);
    Collider_InitCylinder(play, &b->collider);
    Collider_SetCylinder(play, &b->collider, &b->actor, &sBCylinderInit);
    b->actor.targetMode = 6;
    Actor_SetScale(&b->actor, 0.01f);
}

static void NvMmBomber_Anim(NvMmBomber* b, s32 i, f32 speed) {
    b->animIndex = i;
    b->animEndFrame = Animation_GetLastFrame((AnimationHeader*)sBAnims[i]);
    Animation_Change(&b->skelAnime, (AnimationHeader*)sBAnims[i], speed, 0.0f, b->animEndFrame, sBModes[i], -4.0f);
}

static s32 NvMmBomber_AnimDone(NvMmBomber* b) {
    return b->skelAnime.curFrame >= b->animEndFrame;
}

static void NvMmBomber_Common(NvMmBomber* b, PlayState* play) {
    if (b->blinkTimer == 0) {
        if (++b->eyeIndex >= 3) {
            b->eyeIndex = 0;
            b->blinkTimer = (s16)Rand_ZeroFloat(60.0f) + 20;
        }
    } else {
        b->blinkTimer--;
    }
    Math_SmoothStepToS(&b->headRotX, b->headRotTargetX, 1, 0x1388, 0);
    Math_SmoothStepToS(&b->headRotZ, b->headRotTargetZ, 1, 0x3E8, 0);
    Math_SmoothStepToS(&b->torsoRotZ, b->torsoRotTargetZ, 1, 0x3E8, 0);
    Actor_UpdateBgCheckInfo(play, &b->actor, 20.0f, 20.0f, 50.0f, 0x1D);
    b->actor.uncullZoneForward = 500.0f;
}

static s32 NvMmBomber_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                       void* thisx) {
    NvMmBomber* b = (NvMmBomber*)thisx;

    if (limbIndex == BOMBER_LIMB_08) {
        rot->z += b->torsoRotZ;
    }
    if (limbIndex == BOMBER_LIMB_HEAD) {
        rot->x += b->headRotX;
        rot->z += b->headRotZ;
    }
    if (((limbIndex == BOMBER_LIMB_0F) && b->hideLimb0F) || ((limbIndex == BOMBER_LIMB_13) && b->hideLimb13) ||
        ((limbIndex == BOMBER_LIMB_14) && b->hideLimb14)) {
        *dList = NULL;
    }
    return false;
}

static void NvMmBomber_Draw(NvMmBomber* b, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sBEyes[b->eyeIndex]));
    gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(sBNumbers[b->number]));
    gSPSegment(POLY_OPA_DISP++, 0x0A, (b->number == 0) ? sBRedDL : sBBlueDL);
    gSPSegment(POLY_OPA_DISP++, 0x0C, sBEmptyDL);
    SkelAnime_DrawFlexOpa(play, b->skelAnime.skeleton, b->skelAnime.jointTable, b->skelAnime.dListCount,
                          NvMmBomber_OverrideLimbDraw, NULL, b);
    CLOSE_DISPS(play->state.gfxCtx);
}

static s32 NvMmBomber_TextEvent(PlayState* play) {
    return (Message_GetState(&play->msgCtx) == TEXT_STATE_EVENT) && Message_ShouldAdvance(play);
}

static void NvMmBomber_Continue(Actor* actor, PlayState* play, u16 id) {
    actor->textId = NV_MM_TEXT(id);
    Message_ContinueTextbox(play, actor->textId);
}

/* ============================== En_Bombal: the balloon ============================== */

typedef struct {
    Actor actor;
    ColliderCylinder collider;
    f32 scale;
    f32 oscillationAngle;
    s16 timer;
    s16 isPopped;
} NvMmBombal;

const size_t gNvMmBombalSize = sizeof(NvMmBombal);

static ColliderCylinderInit sBombalCylinderInit = {
    { COLTYPE_NONE, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_NONE, OC2_TYPE_2, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 },
      { DMG_ARROW | DMG_SLINGSHOT | DMG_HOOKSHOT | DMG_BOOMERANG | DMG_MAGIC_FIRE | DMG_MAGIC_ICE | DMG_MAGIC_LIGHT,
        0x00, 0x00 }, // (MM's: arrows, the hookshot, Zora's boomerang, Deku Link's bubble; OoT's ranged kinds)
      TOUCH_NONE | TOUCH_SFX_NORMAL, BUMP_ON, OCELEM_NONE },
    { 60, 90, -50, { 0, 0, 0 } },
};

void NvMmBombal_Init(Actor* thisx, PlayState* play) {
    NvMmBombal* this = (NvMmBombal*)thisx;

    ActorShape_Init(&thisx->shape, 0.0f, ActorShadow_DrawCircle, 25.0f);
    thisx->colChkInfo.mass = 0;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sBombalCylinderInit);
    thisx->colChkInfo.health = 1;
    if (thisx->params != 0x7F) { // (respawned by Jim: grows from nothing)
        this->scale = 0.1f;
    }
}

void NvMmBombal_Destroy(Actor* thisx, PlayState* play) {
    Collider_DestroyCylinder(play, &((NvMmBombal*)thisx)->collider);
}

void NvMmBombal_Update(Actor* thisx, PlayState* play) {
    NvMmBombal* this = (NvMmBombal*)thisx;

    DECR(this->timer);
    thisx->shape.rot.y = thisx->world.rot.y;
    Actor_SetFocus(thisx, 30.0f);
    Actor_SetScale(thisx, this->scale);
    if (!this->isPopped) { // MM's func_80C05B3C: bobbing, popped by a hit
        this->oscillationAngle += 1500.0f;
        thisx->velocity.y = Math_SinS(this->oscillationAngle);
        Math_ApproachF(&this->scale, 0.1f, 0.3f, 0.01f);
        if ((getenv("NV_MM_POP") != NULL) && (play->gameplayFrames > (u32)atoi(getenv("NV_MM_POP")))) { // (dev: popped, as if shot)
            this->collider.base.acFlags |= AC_HIT;
        }
        if ((this->collider.base.acFlags & AC_HIT) && (play->msgCtx.msgMode == MSGMODE_NONE)) {
            Vec3f pos = thisx->world.pos;
            s32 i;

            this->collider.base.acFlags &= ~AC_HIT;
            pos.y += 60.0f;
            EffectSsBomb2_SpawnLayered(play, &pos, &sZero, &sZero, 100, 19);
            for (i = 0; i < 20; i++) { // (MM's sparkles: OoT's)
                Vec3f vel = { Rand_CenteredFloat(8.0f), Rand_CenteredFloat(8.0f), Rand_CenteredFloat(8.0f) };

                EffectSsKiraKira_SpawnDispersed(play, &pos, &vel, &sZero, &(Color_RGBA8){ 255, 255, 255, 255 },
                                                &(Color_RGBA8){ 250, 180, 255, 255 }, 3000, 20);
            }
            NvMm_SetWeekEvent(WE_83_04);
            Audio_PlayActorSound2(thisx, NA_SE_IT_BOMB_EXPLOSION); // (MM's balloon pop: not in OoT's banks)
            this->timer = 30;
            this->isPopped = true;
            NvMmBomGame_Log("balloon popped", 0);
        }
    } else {
        if (this->timer < 10) {
            thisx->colChkInfo.health = 0;
        }
        if (this->timer == 0) {
            Actor_Kill(thisx);
            return;
        }
    }
    Actor_MoveXZGravity(thisx);
    if (!this->isPopped) {
        Collider_UpdateCylinder(thisx, &this->collider);
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
}

void NvMmBombal_Draw(Actor* thisx, PlayState* play) {
    NvMmBombal* this = (NvMmBombal*)thisx;

    if (!this->isPopped) {
        Gfx_DrawDListOpa(play, (Gfx*)FUSEN("gMajoraBalloonDL"));
        Gfx_DrawDListOpa(play, (Gfx*)FUSEN("gMajoraBalloonKnotDL"));
    }
}

/* ============================== En_Bomjima: Jim ============================== */

static u16 sJimBefore[] = { 0x719, 0x71A, 0x71B, 0x708 };   // before the balloon's popped
static u16 sJimAfterPop[] = { 0x739, 0x73A, 0x73B, 0x000 }; // after, Goron and Zora Link
static u16 sJimDeku[] = { 0x739, 0x73A, 0x73B, 0x714, 0x709, 0x70A, 0x70B, 0x70C, 0x70D, 0x70E, 0x70F, 0x712, 0x713 };
static u16 sJimHuman[] = { 0x739, 0x73A, 0x73B, 0x759, 0x753, 0x754, 0x755, 0x756, 0x70D, 0x757, 0x758, 0x712, 0x713 };
static u16 sJimAfterGame[] = { 0x736, 0x737, 0x738, 0x74E };
static s16 sRunInOffsets[] = { 0x4000, 60, 0x4000, 30, 0xC000, 30, 0xC000, 60 };

static s32 sJimArrived; // MM's D_80C009F0: the four come; then 100+ as the game starts
static s32 sJimDazed;   // MM's D_80C009F4

enum { JIM_SHOOTING, JIM_SURPRISED, JIM_ASKED, JIM_STARTING, JIM_TALK, JIM_RUN_IN };

struct NvMmBomjima;
typedef void (*NvMmBomjimaFunc)(struct NvMmBomjima*, PlayState*);

typedef struct NvMmBomjima {
    NvMmBomber b;
    NvMmBomjimaFunc func;
    s16 type;     // MM's unk_2E6: 0 Jim, 2 one running in
    s16 textKind; // MM's unk_2CA: 0 before, 1 after (Goron/Zora), 2 the game's lines, 3 after the game
    s16 textIndex; // MM's unk_2C8
    s16 state;    // MM's action
    s16 shots;
    s16 timer, timer2;
    s16 saidNoDeku, saidNoHuman;
    s16 targetYaw;
    Vec3f target;
    Vec3f balloonPos;
    Actor* balloon;
} NvMmBomjima;

const size_t gNvMmBomjimaSize = sizeof(NvMmBomjima);

static void NvMmBomjima_Shoot(NvMmBomjima* this, PlayState* play);
static void NvMmBomjima_Surprised(NvMmBomjima* this, PlayState* play);
static void NvMmBomjima_Talk(NvMmBomjima* this, PlayState* play);
static void NvMmBomjima_Choice(NvMmBomjima* this, PlayState* play);
static void NvMmBomjima_Jumped(NvMmBomjima* this, PlayState* play);
static void NvMmBomjima_WaitRunIn(NvMmBomjima* this, PlayState* play);
static void NvMmBomjima_GameText(NvMmBomjima* this, PlayState* play);
static void NvMmBomjima_Starting(NvMmBomjima* this, PlayState* play);
static void NvMmBomjima_RespawnBalloon(NvMmBomjima* this, PlayState* play);
static void NvMmBomjima_RunIn(NvMmBomjima* this, PlayState* play);
static void NvMmBomjima_RunInWait(NvMmBomjima* this, PlayState* play);
static void NvMmBomjima_RunInJump(NvMmBomjima* this, PlayState* play);

/** MM's func_80BFE32C: his line by the talk's kind and Link's form. */
static void NvMmBomjima_SetText(NvMmBomjima* this, s32 kind) {
    s32 form = NvMmForm_Get();
    u16 id = 0;

    this->textKind = kind;
    if (form == FORM_GORON) {
        this->textIndex = 1;
    } else if ((form == FORM_ZORA) || (form == FORM_FD)) {
        this->textIndex = 2;
    }
    switch (kind) {
        case 0:
            if (form == FORM_DEKU) {
                id = NvMm_CheckWeekEvent(WE_73_20) ? 0x759 : 0x708;
            } else if (form == FORM_HUMAN) {
                id = NvMm_CheckWeekEvent(WE_84_80) ? 0x75A : 0x719;
            } else {
                id = sJimBefore[this->textIndex];
            }
            break;
        case 1:
            id = sJimAfterPop[this->textIndex];
            break;
        case 2:
            id = (form == FORM_DEKU) ? sJimDeku[this->textIndex] : sJimHuman[this->textIndex];
            break;
        case 3:
            id = sJimAfterGame[this->textIndex];
            break;
    }
    this->b.actor.textId = NV_MM_TEXT(id);
}

static Actor* NvMmBomjima_FindBalloon(PlayState* play) {
    Actor* a = play->actorCtx.actorLists[ACTORCAT_PROP].head;
    s16 id = NvMmActor_Id(0x282);

    for (; a != NULL; a = a->next) {
        if (a->id == id) {
            return a;
        }
    }
    return NULL;
}

static s32 NvMmBomjima_BalloonGone(NvMmBomjima* this, PlayState* play) {
    return (this->balloon == NULL) || (NvMmBomjima_FindBalloon(play) != this->balloon) ||
           (this->balloon->colChkInfo.health == 0);
}

static void NvMmBomjima_SetupShoot(NvMmBomjima* this) {
    NvMmBomber_Anim(&this->b, BA_HANDS_ON_HIPS, 1.0f);
    this->shots = 0;
    this->timer = 0;
    this->state = JIM_SHOOTING;
    this->func = NvMmBomjima_Shoot;
}

void NvMmBomjima_Init(Actor* thisx, PlayState* play) {
    NvMmBomjima* this = (NvMmBomjima*)thisx;

    NvMmBomber_Init(&this->b, play);
    thisx->gravity = -3.0f;
    thisx->targetMode = 0;
    NvMm_ClearWeekEvent(WE_83_04);
    this->type = (thisx->params >> 4) & 0xF;
    this->b.number = CLAMP(thisx->params & 0xF, 0, 4);
    this->b.hideLimb0F = (this->type == 2);
    this->b.hideLimb13 = (this->type == 2);
    this->b.hideLimb14 = (this->type == 0);
    if (this->type == 2) { // one of the four running in
        NvMmBomber_Anim(&this->b, BA_RUN, 1.0f);
        this->state = JIM_RUN_IN;
        this->func = NvMmBomjima_RunIn;
        return;
    }
    if (NvMm_CheckWeekEvent(WE_75_40) || NvMmBomGame_On()) {
        Actor_Kill(thisx);
        return;
    }
    NvMmBomjima_SetupShoot(this); // (the balloon found once it's there: MM's func_80BFEA94)
}

void NvMmBomjima_Destroy(Actor* thisx, PlayState* play) {
    Collider_DestroyCylinder(play, &((NvMmBomjima*)thisx)->b.collider);
}

static void NvMmBomjima_SetupTalk(NvMmBomjima* this) {
    NvMmBomber_Anim(&this->b, BA_TALK_HANDS_ON_HIPS, 1.0f);
    this->b.collider.dim.radius = 15;
    this->b.collider.dim.height = 40;
    this->func = NvMmBomjima_Talk;
}

/** MM's func_80BFEB64: shooting at the balloon (and missing); talked to. */
static void NvMmBomjima_Shoot(NvMmBomjima* this, PlayState* play) {
    s32 form = NvMmForm_Get();

    if (this->balloon == NULL) { // MM's func_80BFEA94: waiting for the balloon
        this->balloon = NvMmBomjima_FindBalloon(play);
        if (this->balloon == NULL) {
            return;
        }
        this->balloonPos = this->balloon->world.pos;
    }

    NvMmBomjima_SetText(this, 0);
    if (form == FORM_DEKU) {
        if (NvMm_CheckWeekEvent(WE_73_20)) {
            this->textIndex = 3;
            NvMmBomjima_SetText(this, 3);
        } else if (NvMm_CheckWeekEvent(WE_77_02)) {
            this->textIndex = 11;
            NvMmBomjima_SetText(this, 2);
        }
    } else if (form == FORM_HUMAN) {
        if (NvMm_CheckWeekEvent(WE_84_80)) {
            this->textIndex = 0;
            NvMmBomjima_SetText(this, 3);
        } else if (NvMm_CheckWeekEvent(WE_85_01)) {
            this->textIndex = 11;
            NvMmBomjima_SetText(this, 2);
        }
    }
    if (Actor_ProcessTalkRequest(&this->b.actor, play)) {
        this->targetYaw = this->b.actor.yawTowardsPlayer;
        NvMmBomjima_SetupTalk(this);
        return;
    }
    Actor_OfferTalk(&this->b.actor, play, 70.0f);
    if (NvMmBomjima_BalloonGone(this, play)) { // MM's func_80BFEFF0: popped
        this->balloon = NULL;
        NvMmBomber_Anim(&this->b, BA_SURPRISE, 1.0f);
        this->shots = 0;
        this->timer = 30;
        this->state = JIM_SURPRISED;
        this->targetYaw = this->b.actor.yawTowardsPlayer;
        this->func = NvMmBomjima_Surprised;
        NvMmBomGame_Log("Jim sees the balloon popped", 0);
        return;
    }
    switch (this->timer2) {
        case 0:
            NvMmBomber_Anim(&this->b, BA_BLOWGUN, 1.0f);
            this->b.torsoRotTargetZ = -7000;
            this->timer2 = 1;
            break;
        case 1:
            this->targetYaw = Math_Vec3f_Yaw(&this->b.actor.world.pos, &this->balloon->world.pos);
            if (Animation_OnFrame(&this->b.skelAnime, 19.0f)) {
                this->timer = 5;
            }
            if (this->timer == 1) { // a dart, missing (MM's hitmark beside the balloon)
                s16 yaw = this->targetYaw + ((Rand_ZeroOne() < 0.5f) ? 0x4000 : 0xC000);
                Vec3f p;

                p.x = Math_SinS(yaw) * (Rand_ZeroFloat(20.0f) + 40.0f) + this->balloon->world.pos.x;
                p.y = this->balloon->world.pos.y - Rand_CenteredFloat(40.0f);
                p.z = Math_CosS(yaw) * (Rand_ZeroFloat(20.0f) + 40.0f) + this->balloon->world.pos.z;
                EffectSsHitMark_SpawnFixedScale(play, 0, &p);
                this->shots++;
                if (((s32)Rand_ZeroFloat(2.0f) + 3) < this->shots) {
                    NvMmBomber_Anim(&this->b, BA_WALK, 1.0f);
                    this->b.torsoRotTargetZ = 0;
                    this->target = this->b.actor.home.pos;
                    this->target.x += Rand_CenteredFloat(150.0f);
                    this->target.z += Rand_CenteredFloat(150.0f);
                    this->timer2 = 2;
                }
            }
            break;
        case 2:
            this->targetYaw = Math_Vec3f_Yaw(&this->b.actor.world.pos, &this->target);
            Math_ApproachF(&this->b.actor.world.pos.x, this->target.x, 0.3f, 2.0f);
            Math_ApproachF(&this->b.actor.world.pos.z, this->target.z, 0.3f, 2.0f);
            if (Math_Vec3f_DistXZ(&this->b.actor.world.pos, &this->target) < 4.0f) {
                this->shots = 0;
                this->timer2 = 0;
            }
            break;
    }
    Math_SmoothStepToS(&this->b.actor.world.rot.y, this->targetYaw, 1, 0x1388, 0);
}

/** MM's func_80BFF174: the balloon popped, he turns; talked to, the game's lines. */
static void NvMmBomjima_Surprised(NvMmBomjima* this, PlayState* play) {
    s32 form = NvMmForm_Get();

    if (Actor_ProcessTalkRequest(&this->b.actor, play)) {
        this->targetYaw = this->b.actor.yawTowardsPlayer;
        NvMmBomjima_SetupTalk(this);
        return;
    }
    Math_SmoothStepToS(&this->b.actor.world.rot.y, this->targetYaw, 1, 0x1388, 0);
    if (NvMmBomber_AnimDone(&this->b) && (this->shots < 5)) {
        this->shots++;
        if (this->b.animIndex != BA_LOOK_AROUND) {
            NvMmBomber_Anim(&this->b, BA_LOOK_AROUND, 1.0f);
        }
    }
    if (form == FORM_DEKU) {
        if (NvMm_CheckWeekEvent(WE_73_20)) {
            this->textIndex = 3;
            NvMmBomjima_SetText(this, 3);
        } else {
            this->textIndex = !NvMm_CheckWeekEvent(WE_77_02) ? (this->saidNoDeku ? 3 : 4) : 11;
            NvMmBomjima_SetText(this, 2);
        }
    } else if (form == FORM_HUMAN) {
        if (NvMm_CheckWeekEvent(WE_84_80)) {
            this->textIndex = 0;
            NvMmBomjima_SetText(this, 3);
        } else {
            this->textIndex = !NvMm_CheckWeekEvent(WE_85_01) ? (this->saidNoHuman ? 3 : 4) : 11;
            NvMmBomjima_SetText(this, 2);
        }
    } else if (this->textKind != 1) {
        NvMmBomjima_SetText(this, 1);
    }
    Actor_OfferTalk(&this->b.actor, play, 70.0f);
}

/** MM's func_80C00284: his lines, one after another. */
static void NvMmBomjima_Talk(NvMmBomjima* this, PlayState* play) {
    s32 form = NvMmForm_Get();
    u16* lines = (form == FORM_DEKU) ? sJimDeku : sJimHuman;

    Math_SmoothStepToS(&this->b.actor.world.rot.y, this->targetYaw, 1, 0x1388, 0);
    this->b.headRotTargetZ = (form == FORM_HUMAN) ? -0xFA0 : ((form == FORM_GORON) || (form == FORM_ZORA)) ? -0x1770 : 0;
    if (NvMmBomber_AnimDone(&this->b)) {
        NvMmBomber_Anim(&this->b, (Rand_ZeroOne() < 0.5f) ? BA_TALK_HANDS_ON_HIPS : BA_ARM_SWIPE, 1.0f);
    }
    if (!NvMmBomber_TextEvent(play)) {
        return;
    }
    this->b.collider.dim.radius = 10;
    this->b.collider.dim.height = 30;
    if ((this->textKind == 1) || ((this->textKind == 3) && (this->textIndex >= 2))) { // the talk's end
        this->b.headRotTargetZ = 0;
        Message_CloseTextbox(play);
        if (this->balloon == NULL) { // (popped: he jumps, and blows another)
            NvMmBomber_Anim(&this->b, BA_JUMP, 1.0f);
            this->func = NvMmBomjima_RespawnBalloon;
        } else {
            NvMmBomjima_SetupShoot(this);
        }
        return;
    }
    switch (this->textKind) {
        case 0:
            Message_CloseTextbox(play);
            this->b.headRotTargetZ = 0;
            NvMmBomjima_SetupShoot(this);
            break;
        case 2:
            if ((this->textIndex == 10) || (this->textIndex >= 12)) { // (no to the game; or played)
                Message_CloseTextbox(play);
                this->b.headRotTargetZ = 0;
                NvMmBomber_Anim(&this->b, BA_HANDS_ON_HIPS, 1.0f);
                this->func = NvMmBomjima_Surprised;
                break;
            }
            this->textIndex++;
            if ((this->textIndex == 4) && ((form == FORM_DEKU) ? this->saidNoDeku : this->saidNoHuman)) {
                this->textIndex = 6;
            }
            NvMmBomber_Continue(&this->b.actor, play, lines[this->textIndex]);
            if ((this->textIndex == 7) || (this->textIndex == 12)) { // the choice: play?
                this->func = NvMmBomjima_Choice;
            }
            break;
        case 3:
            this->textIndex++;
            NvMmBomber_Continue(&this->b.actor, play, sJimAfterGame[this->textIndex]);
            if (this->textIndex >= 2) {
                NvMmBomber_Anim(&this->b, BA_SALUTE, 1.0f);
            }
            break;
    }
}

/** MM's func_80BFF52C: yes, the game; no, his line. */
static void NvMmBomjima_Choice(NvMmBomjima* this, PlayState* play) {
    s32 form = NvMmForm_Get();
    u16* lines = (form == FORM_DEKU) ? sJimDeku : sJimHuman;

    if ((Message_GetState(&play->msgCtx) != TEXT_STATE_CHOICE) || !Message_ShouldAdvance(play)) {
        return;
    }
    if (play->msgCtx.choiceIndex == 0) {
        Sfx_PlaySfxCentered(NA_SE_SY_DECIDE);
        this->textIndex = 8;
        NvMmBomber_Continue(&this->b.actor, play, lines[this->textIndex]);
        Sfx_PlaySfxCentered(NA_SE_SY_CORRECT_CHIME); // (MM's NA_SE_SY_FOUND)
        NvMmBomber_Anim(&this->b, BA_JUMP, 1.0f);
        this->state = JIM_STARTING;
        this->func = NvMmBomjima_Jumped;
    } else {
        Sfx_PlaySfxCentered(NA_SE_SY_CANCEL);
        this->textIndex = 10;
        if (form == FORM_DEKU) {
            this->saidNoDeku = true;
        } else {
            this->saidNoHuman = true;
        }
        NvMmBomber_Continue(&this->b.actor, play, lines[this->textIndex]);
        NvMmBomjima_SetupTalk(this);
    }
}

/** MM's func_80BFF6CC and func_80BFF754: the four called in, running up around him. */
static void NvMmBomjima_Jumped(NvMmBomjima* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s32 i;

    if (!NvMmBomber_AnimDone(&this->b) || !NvMmBomber_TextEvent(play)) {
        return;
    }
    Message_CloseTextbox(play);
    NvMmBomber_Anim(&this->b, BA_HANDS_ON_HIPS, 1.0f);
    for (i = 1; i < 5; i++) {
        Vec3f p = player->actor.world.pos;
        f32 k = 2.0f + i * 0.2f;
        NvMmBomjima* one;

        p.x += (player->actor.world.pos.x - this->b.actor.world.pos.x) * k;
        p.y += (player->actor.world.pos.y - this->b.actor.world.pos.y) * k;
        p.z += (player->actor.world.pos.z - this->b.actor.world.pos.z) * k;
        one = (NvMmBomjima*)Actor_SpawnAsChild(&play->actorCtx, &this->b.actor, play, NvMmActor_Id(0x27E), p.x,
                                               this->b.actor.world.pos.y, p.z, 0, 0, 0, i + 32);
        if (one != NULL) {
            s32 idx = (i * 2) - 2;

            one->target = this->b.actor.world.pos;
            one->target.x += Math_SinS(sRunInOffsets[idx] + this->b.actor.world.rot.y) * sRunInOffsets[idx + 1];
            one->target.z += Math_CosS(sRunInOffsets[idx] + this->b.actor.world.rot.y) * sRunInOffsets[idx + 1];
        }
    }
    sJimArrived = 0;
    sJimDazed = 0;
    player->actor.freezeTimer = 3;
    this->func = NvMmBomjima_WaitRunIn;
}

/** MM's func_80BFF9B0: all four here, the game begins (its week events, the count cleared). */
static void NvMmBomjima_WaitRunIn(NvMmBomjima* this, PlayState* play) {
    s32 form = NvMmForm_Get();
    s32 i;

    GET_PLAYER(play)->actor.freezeTimer = 3;
    if (sJimArrived < 4) {
        return;
    }
    sJimArrived = 0;
    if (form == FORM_DEKU) {
        NvMm_SetWeekEvent(WE_73_10);
        NvMm_SetWeekEvent(WE_77_02);
    } else {
        NvMm_SetWeekEvent(WE_85_02);
        NvMm_SetWeekEvent(WE_85_01);
    }
    for (i = 0; i < 5; i++) {
        NvMm_ClearWeekEvent(MM_WE(11, 1 << i));
        NvMm_ClearWeekEvent(MM_WE(76, 1 << i));
    }
    for (i = 0; i < 6; i++) {
        sCaught[i] = 0;
    }
    NvMmBomGame_Log("the game begins", form);
    NvMmBomber_Anim(&this->b, BA_TALK_HANDS_ON_HIPS, 1.0f);
    this->textIndex = 9;
    this->b.actor.textId = NV_MM_TEXT(((form == FORM_DEKU) ? sJimDeku : sJimHuman)[9]);
    Message_StartTextbox(play, this->b.actor.textId, &this->b.actor);
    this->func = NvMmBomjima_GameText;
}

static void NvMmBomjima_GameText(NvMmBomjima* this, PlayState* play) {
    GET_PLAYER(play)->actor.freezeTimer = 3;
    if (NvMmBomber_TextEvent(play)) {
        Message_CloseTextbox(play);
        NvMmBomber_Anim(&this->b, BA_JUMP, 1.0f);
        sJimArrived = 100;
        this->b.headRotTargetX = 0;
        this->func = NvMmBomjima_Starting;
    }
}

/** MM's func_80BFFBC4: they scatter (number 5 falls over); then the game, North Clock Town afresh. */
static void NvMmBomjima_Starting(NvMmBomjima* this, PlayState* play) {
    GET_PLAYER(play)->actor.freezeTimer = 3;
    if ((this->b.animIndex != BA_HANDS_ON_HIPS) && NvMmBomber_AnimDone(&this->b)) {
        NvMmBomber_Anim(&this->b, BA_HANDS_ON_HIPS, 1.0f);
    }
    if (sJimArrived >= 104) {
        sJimArrived = 0;
        play->nextEntranceIndex = ENTR_MM_Z2_BACKTOWN_6; // (MM's Entrance_CreateFromSpawn(6))
        play->transitionTrigger = TRANS_TRIGGER_START;
        play->transitionType = TRANS_TYPE_FADE_WHITE;
        gSaveContext.nextTransitionType = TRANS_TYPE_FADE_WHITE;
    }
}

/** MM's func_80BFF430: after a talk with the balloon gone, he jumps and blows another. */
static void NvMmBomjima_RespawnBalloon(NvMmBomjima* this, PlayState* play) {
    if (NvMmBomber_AnimDone(&this->b)) {
        this->balloon = Actor_Spawn(&play->actorCtx, play, NvMmActor_Id(0x282), this->balloonPos.x, this->balloonPos.y,
                                    this->balloonPos.z, 0, 0, 0, 0x7F);
        NvMm_ClearWeekEvent(WE_83_04);
        NvMmBomjima_SetupShoot(this);
    }
}

/* the four running in (MM's func_80BFFD48, func_80BFFE48, func_80BFFF54) */
static void NvMmBomjima_RunIn(NvMmBomjima* this, PlayState* play) {
    this->targetYaw = Math_Vec3f_Yaw(&this->b.actor.world.pos, &this->target);
    Math_SmoothStepToS(&this->b.actor.world.rot.y, this->targetYaw, 1, 0x1388, 0);
    Math_ApproachF(&this->b.actor.world.pos.x, this->target.x, 0.3f, 4.0f);
    Math_ApproachF(&this->b.actor.world.pos.z, this->target.z, 0.3f, 4.0f);
    if (Math_Vec3f_DistXZ(&this->b.actor.world.pos, &this->target) < 4.0f) {
        sJimArrived++;
        this->targetYaw = (this->b.actor.parent != NULL) ? this->b.actor.parent->world.rot.y : 0;
        NvMmBomber_Anim(&this->b, BA_IDLE, 1.0f);
        this->func = NvMmBomjima_RunInWait;
    }
}

static void NvMmBomjima_RunInWait(NvMmBomjima* this, PlayState* play) {
    Math_SmoothStepToS(&this->b.actor.world.rot.y, this->targetYaw, 1, 0x1388, 0);
    if (sJimArrived >= 100) {
        if ((this->b.number != 4) || (sJimArrived >= 103)) {
            NvMmBomber_Anim(&this->b, BA_JUMP, 1.0f);
            this->func = NvMmBomjima_RunInJump;
        } else {
            Math_SmoothStepToS(&this->b.headRotX, 10000, 1, 0x1388, 0);
        }
    }
}

static void NvMmBomjima_RunInJump(NvMmBomjima* this, PlayState* play) {
    if (this->b.number != 4) {
        if ((this->b.animIndex != BA_IDLE) && NvMmBomber_AnimDone(&this->b)) {
            sJimArrived++;
            NvMmBomber_Anim(&this->b, BA_IDLE, 1.0f);
        }
    } else if ((this->b.animIndex != BA_DAZED) && NvMmBomber_AnimDone(&this->b)) {
        NvMmBomber_Anim(&this->b, BA_DAZED, 1.0f); // (number 5: over he goes)
        sJimDazed = 1;
        this->timer = 10;
    } else if ((this->b.animIndex == BA_DAZED) && (this->timer == 1)) {
        sJimArrived++;
    }
}

void NvMmBomjima_Update(Actor* thisx, PlayState* play) {
    NvMmBomjima* this = (NvMmBomjima*)thisx;

    DECR(this->timer);
    SkelAnime_Update(&this->b.skelAnime);
    thisx->shape.rot.y = thisx->world.rot.y;
    Actor_SetFocus(thisx, 20.0f);
    this->func(this, play);
    Actor_MoveXZGravity(thisx);
    NvMmBomber_Common(&this->b, play);
    Collider_UpdateCylinder(thisx, &this->b.collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->b.collider.base);
}

void NvMmBomjima_Draw(Actor* thisx, PlayState* play) {
    NvMmBomber_Draw(&((NvMmBomjima*)thisx)->b, play);
}

/* ============================== En_Bomjimb: the five hidden ============================== */

static u16 sCaughtText[] = { 0x0721, 0x0722, 0x0723, 0x0724, 0x072C };
static Actor* sHidingBox; // MM's D_80C03170: one Bomber under a crate at a time

enum { KID_WANDER, KID_CUCCO, KID_HIDING, KID_SITTING, KID_RUN, KID_TAUNT, KID_DAZED, KID_CAUGHT, KID_NOT_PLAYING,
       KID_LEAVING };

struct NvMmBomjimb;
typedef void (*NvMmBomjimbFunc)(struct NvMmBomjimb*, PlayState*);

typedef struct NvMmBomjimb {
    NvMmBomber b;
    NvMmBomjimbFunc func;
    s16 state;      // MM's unk_2CA
    s16 kind;       // MM's unk_2C6: 0-1 wandering, 2 riding a cucco down
    s16 exitIndex;  // MM's unk_2B2: the room's exit to the hideout
    s16 timer, timer2, step;
    s16 runYaw, runYawOffset, blocked;
    s16 bounced;
    f32 speed;
    Vec3f target;
    Actor* held; // the cucco ridden, or the crate hidden under
} NvMmBomjimb;

const size_t gNvMmBomjimbSize = sizeof(NvMmBomjimb);

static void NvMmBomjimb_SetupWander(NvMmBomjimb* this);
static void NvMmBomjimb_SetupRun(NvMmBomjimb* this);
static void NvMmBomjimb_SetupTaunt(NvMmBomjimb* this);
static void NvMmBomjimb_SetupSit(NvMmBomjimb* this);
static void NvMmBomjimb_Caught(NvMmBomjimb* this, PlayState* play);
static void NvMmBomjimb_Hiding(NvMmBomjimb* this, PlayState* play);

void NvMmBomjimb_Init(Actor* thisx, PlayState* play) {
    NvMmBomjimb* this = (NvMmBomjimb*)thisx;
    s32 n;

    NvMmBomber_Init(&this->b, play);
    thisx->gravity = -2.0f;
    this->kind = (thisx->params >> 4) & 0xF;
    this->exitIndex = (thisx->params >> 8) & 0xFF;
    n = (this->kind != 0) ? ((thisx->params & 0xF) + 1) : 0;
    this->b.number = CLAMP(n, 0, 4);
    this->b.hideLimb0F = true;
    this->b.hideLimb13 = (this->b.number != 0);
    this->b.hideLimb14 = (this->b.number == 0);
    // only while the game's on, and not those caught already
    if (!NvMmBomGame_On() || NvMm_CheckWeekEvent(WE_75_40) ||
        NvMm_CheckWeekEvent(MM_WE(11, 1 << this->b.number))) {
        NvMmBomGame_Log("a hider not out (game on?)", NvMmBomGame_On());
        Actor_Kill(thisx);
        return;
    }
    if (this->kind == 2) { // riding a cucco down from the roof (OoT's cucco)
        this->held = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_NIW, thisx->world.pos.x, thisx->world.pos.y + 30.0f,
                                 thisx->world.pos.z, 0, thisx->world.rot.y, 0, 0);
        if (this->held != NULL) {
            NvMmBomber_Anim(&this->b, BA_HOLD_CUCCO, 1.0f);
        }
        this->state = KID_CUCCO;
    } else {
        NvMmBomjimb_SetupWander(this);
    }
}

void NvMmBomjimb_Destroy(Actor* thisx, PlayState* play) {
    NvMmBomjimb* this = (NvMmBomjimb*)thisx;

    if (sHidingBox == this->held) {
        sHidingBox = NULL;
    }
    Collider_DestroyCylinder(play, &this->b.collider);
}

/** MM's func_80C012FC: touched, caught. */
static s32 NvMmBomjimb_CheckCaught(NvMmBomjimb* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if ((this->b.actor.xzDistToPlayer < 40.0f) && (fabsf(player->actor.world.pos.y - this->b.actor.world.pos.y) < 50.0f) &&
        (play->msgCtx.msgMode == MSGMODE_NONE)) {
        this->b.actor.speedXZ = 0.0f;
        NvMmBomjimb_Caught(this, play);
        return true;
    }
    return false;
}

/** MM's func_80C013B4, func_80C0267C: hit, dazed a while. */
static s32 NvMmBomjimb_CheckHit(NvMmBomjimb* this) {
    if (this->b.collider.base.acFlags & AC_HIT) {
        this->b.collider.base.acFlags &= ~AC_HIT;
        NvMmBomber_Anim(&this->b, BA_DAZED, 1.0f);
        this->b.actor.speedXZ = 0.0f;
        this->b.actor.world.rot.y = this->b.actor.yawTowardsPlayer;
        this->timer = 40;
        this->state = KID_DAZED;
        return true;
    }
    return false;
}

/** MM's func_80C013F0: bumping into a crate (MM's Obj_Kibako), under it he goes. */
static s32 NvMmBomjimb_CheckBox(NvMmBomjimb* this, PlayState* play) {
    Actor* box = this->b.collider.base.oc;

    if ((this->b.collider.base.ocFlags1 & OC1_HIT) && (box != NULL) && (box->id == NvMmActor_Id(0x081)) &&
        (sHidingBox == NULL) && (box->update != NULL) && !Actor_HasParent(box, play)) {
        sHidingBox = box;
        this->held = box;
        box->velocity.y = 8.0f; // (MM: the crate hops up, he slips under)
        this->target.x = box->world.pos.x;
        this->target.y = this->b.actor.world.pos.y;
        this->target.z = box->world.pos.z;
        this->b.actor.speedXZ = 0.0f;
        this->step = 0;
        this->state = KID_HIDING;
        NvMmBomGame_Log("under a crate", this->b.number + 1);
        return true;
    }
    return false;
}

static void NvMmBomjimb_SetupWander(NvMmBomjimb* this) {
    this->timer = this->timer2 = this->step = 0;
    this->target = this->b.actor.world.pos;
    this->state = KID_WANDER;
}

/** MM's func_80C014E4: wandering near his spot; Link near, he runs. */
static void NvMmBomjimb_Wander(NvMmBomjimb* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    CollisionPoly* poly;
    Vec3f hit;
    s32 bgId;
    f32 reach;
    s16 d;

    switch (this->step) {
        case 0:
            if (this->timer == 0) {
                Vec3f to = this->b.actor.home.pos;

                to.x += Rand_CenteredFloat(150.0f);
                to.z += Rand_CenteredFloat(150.0f);
                d = this->b.actor.world.rot.y - Math_Vec3f_Yaw(&this->b.actor.world.pos, &to);
                if ((ABS(d) < 0x4000) && !BgCheck_EntityLineTest1(&play->colCtx, &this->b.actor.world.pos, &to, &hit,
                                                                 &poly, true, false, false, true, &bgId)) {
                    NvMmBomber_Anim(&this->b, BA_WALK, 1.0f);
                    this->target = to;
                    this->timer2 = Rand_S16Offset(30, 50);
                    this->step = 1;
                }
            }
            break;
        case 1: {
            s16 yaw = Math_Vec3f_Yaw(&this->b.actor.world.pos, &this->target);

            Math_SmoothStepToS(&this->b.actor.world.rot.y, yaw, 10, 0x7D0, 0x14);
            if ((this->timer2 == 0) || (Math_Vec3f_DistXZ(&this->b.actor.world.pos, &this->target) < 4.0f)) {
                this->timer = Rand_S16Offset(20, 20);
                NvMmBomber_Anim(&this->b, (this->timer & 1) ? BA_IDLE : BA_LOOK_AROUND, 1.0f);
                this->step = 0;
                this->speed = 0.0f;
            } else {
                Math_ApproachF(&this->b.actor.world.pos.x, this->target.x, 0.3f, this->speed);
                Math_ApproachF(&this->b.actor.world.pos.z, this->target.z, 0.3f, this->speed);
                Math_ApproachF(&this->speed, 1.0f, 0.3f, 0.5f);
            }
            break;
        }
    }
    reach = 200.0f;
    d = this->b.actor.yawTowardsPlayer - this->b.actor.world.rot.y;
    if (ABS(d) > 0x2890) {
        reach = 150.0f;
    }
    if (this->b.actor.xzDistToPlayer < reach) {
        NvMmBomjimb_SetupRun(this);
    }
}

/** MM's func_80C01A24, func_80C01B74: riding the cucco down; Link near, off it goes. */
static void NvMmBomjimb_Cucco(NvMmBomjimb* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    this->b.actor.gravity = -0.1f;
    if (this->b.collider.base.acFlags & AC_HIT) {
        this->b.collider.base.acFlags &= ~AC_HIT;
        this->held = NULL;
        this->b.actor.speedXZ = 0.0f;
        this->b.actor.gravity = -2.0f;
        NvMmBomjimb_SetupRun(this);
        return;
    }
    if ((this->b.actor.xzDistToPlayer < 200.0f) && (fabsf(player->actor.world.pos.y - this->b.actor.world.pos.y) < 40.0f)) {
        this->step = 1;
    }
    if (this->step != 0) {
        Math_ApproachF(&this->b.actor.speedXZ, 6.0f, 0.5f, 2.0f);
        if (this->b.actor.bgCheckFlags & BGCHECKFLAG_GROUND) {
            this->held = NULL;
            this->b.actor.speedXZ = 0.0f;
            this->b.actor.gravity = -2.0f;
            NvMmBomjimb_SetupRun(this);
        }
    }
}

/** MM's func_80C01CD0: under the crate; the crate lifted, he's out and running. */
static void NvMmBomjimb_Hiding(NvMmBomjimb* this, PlayState* play) {
    Actor* box = this->held;

    if ((box == NULL) || (box->update == NULL) || Actor_HasParent(box, play)) {
        this->b.actor.draw = (ActorFunc)NvMmBomjimb_Draw;
        sHidingBox = NULL;
        this->held = NULL;
        NvMmBomjimb_SetupRun(this);
        return;
    }
    if (this->b.animIndex != BA_HIDE_UNDER_BOX) {
        Math_SmoothStepToS(&this->b.actor.world.rot.y, Math_Vec3f_Yaw(&this->b.actor.world.pos, &this->target), 1,
                           0xBB8, 0);
        Math_ApproachF(&this->b.actor.world.pos.x, this->target.x, 0.3f, 2.0f);
        Math_ApproachF(&this->b.actor.world.pos.z, this->target.z, 0.3f, 2.0f);
        if (Math_Vec3f_DistXZ(&this->b.actor.world.pos, &this->target) < 3.0f) {
            NvMmBomber_Anim(&this->b, BA_HIDE_UNDER_BOX, 1.0f);
            this->b.actor.world.pos = this->target;
        }
    } else if (NvMmBomber_AnimDone(&this->b)) {
        this->b.actor.draw = NULL; // (under it)
    }
    if (this->b.actor.draw == NULL) { // the crate shakes now and then
        if (this->timer == 0) {
            this->timer = Rand_S16Offset(20, 30);
            box->shape.rot.z = 1000;
        } else {
            box->shape.rot.z = -box->shape.rot.z * 0.9f;
            if (ABS(box->shape.rot.z) < 100) {
                box->shape.rot.z = 0;
            }
        }
    }
}

static void NvMmBomjimb_SetupRun(NvMmBomjimb* this) {
    NvMmBomber_Anim(&this->b, BA_SURPRISE, 1.0f);
    this->runYaw = this->b.actor.yawTowardsPlayer + 0x8000;
    this->b.actor.world.rot.y = this->b.actor.yawTowardsPlayer;
    this->runYawOffset = 0;
    this->timer = this->timer2 = this->step = 0;
    this->state = KID_RUN;
}

/** MM's func_80C0217C: running from Link, turning from walls. */
static void NvMmBomjimb_Run(NvMmBomjimb* this, PlayState* play) {
    CollisionPoly* poly;
    Vec3f ahead;
    Vec3f hit;
    s32 bgId;
    s32 wall = false;

    if (this->b.animIndex == BA_SURPRISE) {
        if (NvMmBomber_AnimDone(&this->b)) {
            NvMmBomber_Anim(&this->b, BA_RUN, 2.0f);
        }
        return;
    }
    Math_ApproachF(&this->b.actor.speedXZ, 8.0f, 0.5f, 2.0f);
    ahead = this->b.actor.world.pos;
    ahead.x += Math_SinS(this->b.actor.world.rot.y) * 50.0f;
    ahead.y += 20.0f;
    ahead.z += Math_CosS(this->b.actor.world.rot.y) * 50.0f;
    if (BgCheck_EntityLineTest1(&play->colCtx, &this->b.actor.world.pos, &ahead, &hit, &poly, true, false, false,
                                true, &bgId)) {
        s16 t = (this->b.actor.world.rot.y - this->b.actor.yawTowardsPlayer) - 0x8000;
        s16 turn = (s16)Rand_ZeroFloat(0x200) + 0x4000;

        this->runYaw = t;
        if (t < 0) {
            this->runYaw += turn + (this->blocked ? turn : 0);
        } else {
            this->runYaw -= turn + (this->blocked ? turn : 0);
        }
        this->blocked = true;
        wall = true;
    }
    if (!wall) {
        this->blocked = false;
        if (this->b.actor.xzDistToPlayer < 60.0f) {
            this->runYaw = this->b.actor.yawTowardsPlayer + 0x8000;
        }
    }
    this->b.actor.world.rot.y = this->runYaw + this->runYawOffset;
    if (SurfaceType_GetSceneExitIndex(&play->colCtx, this->b.actor.floorPoly, this->b.actor.floorBgId) != 0) {
        // (an exit underfoot: not out of the area)
        s16 t = this->b.actor.world.rot.y - (this->b.actor.yawTowardsPlayer - 0x8000);

        this->runYawOffset += (t < 0) ? -0x1000 : 0x1000;
        this->b.actor.world.rot.y = this->runYaw + this->runYawOffset;
        return;
    }
    this->runYawOffset = 0;
    if (!wall && (this->b.actor.xzDistToPlayer > 200.0f)) {
        NvMmBomjimb_SetupTaunt(this);
    }
}

static void NvMmBomjimb_SetupTaunt(NvMmBomjimb* this) {
    NvMmBomber_Anim(&this->b, BA_TAUNT, 1.0f);
    this->b.actor.speedXZ = 0.0f;
    this->state = KID_TAUNT;
}

/** MM's func_80C02570: taunting from afar; near again, running; far, wandering. */
static void NvMmBomjimb_Taunt(NvMmBomjimb* this, PlayState* play) {
    Math_SmoothStepToS(&this->b.actor.world.rot.y, this->b.actor.yawTowardsPlayer, 1, 0x1388, 0);
    if (this->b.actor.xzDistToPlayer < 200.0f) {
        this->runYaw = this->b.actor.yawTowardsPlayer + 0x8000;
        NvMmBomber_Anim(&this->b, BA_RUN, 2.0f);
        this->state = KID_RUN;
    } else if (this->b.actor.xzDistToPlayer > 410.0f) {
        NvMmBomjimb_SetupWander(this);
    }
}

/** MM's func_80C02740: caught (Deku or human Link, his own game; others, MM's "you're not playing"). */
static void NvMmBomjimb_Caught(NvMmBomjimb* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s32 form = NvMmForm_Get();
    s32 n = this->b.number;

    NvMmBomber_Anim(&this->b, BA_CAUGHT, 1.0f);
    player->actor.freezeTimer = 3;
    if (((form != FORM_DEKU) && (form != FORM_HUMAN)) || ((form == FORM_DEKU) && !NvMm_CheckWeekEvent(WE_73_10)) ||
        ((form == FORM_HUMAN) && !NvMm_CheckWeekEvent(WE_85_02))) {
        NvMmBomber_Anim(&this->b, BA_ARM_SWIPE, 1.0f);
        Message_StartTextbox(play, NV_MM_TEXT(0x72E), &this->b.actor);
        this->step = 0;
        this->state = KID_NOT_PLAYING;
        return;
    }
    Message_StartTextbox(play, NV_MM_TEXT(sCaughtText[CLAMP(sCaught[0], 0, 4)]), &this->b.actor);
    if (sCaught[0] < 5) {
        sCaught[1 + sCaught[0]] = n + 1;
        sCaught[0]++;
    }
    Audio_PlayActorSound2(&this->b.actor, (sCaught[0] > 4) ? NA_SE_SY_CORRECT_CHIME : NA_SE_SY_PIECE_OF_HEART);
    NvMm_SetWeekEvent(MM_WE(76, 1 << n));
    NvMm_SetWeekEvent(MM_WE(11, 1 << n));
    NvMmBomGame_Log("caught", n + 1);
    this->state = KID_CAUGHT;
}

static void NvMmBomjimb_SetupSit(NvMmBomjimb* this) {
    this->b.actor.textId = NV_MM_TEXT(0x72D);
    NvMmBomber_Anim(&this->b, BA_SIT, 1.0f);
    this->state = KID_SITTING;
}

/** MM's func_80C02A14: his line, sitting; the fifth, to the hideout. */
static void NvMmBomjimb_CaughtText(NvMmBomjimb* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (this->b.animIndex == BA_CAUGHT) {
        player->actor.freezeTimer = 3;
        if (NvMmBomber_AnimDone(&this->b)) {
            NvMmBomber_Anim(&this->b, BA_SIT, 1.0f);
        }
        return;
    }
    if (this->state == KID_CAUGHT) {
        player->actor.freezeTimer = 3;
    }
    if (NvMmBomber_TextEvent(play)) {
        Message_CloseTextbox(play);
        if ((this->state == KID_CAUGHT) && (sCaught[0] >= 5)) { // MM's func_80C02CA4
            play->nextEntranceIndex = play->setupExitList[this->exitIndex];
            play->transitionTrigger = TRANS_TRIGGER_START;
            play->transitionType = TRANS_TYPE_FADE_WHITE;
            gSaveContext.nextTransitionType = TRANS_TYPE_FADE_WHITE;
            NvMm_SetWeekEvent(WE_75_40);
            NvMm_SetWeekEvent(WE_83_04);
            NvMmBomGame_Log("all five: to the hideout", 0);
            this->state = KID_LEAVING;
        } else {
            NvMmBomjimb_SetupSit(this);
        }
    }
}

void NvMmBomjimb_Update(Actor* thisx, PlayState* play) {
    NvMmBomjimb* this = (NvMmBomjimb*)thisx;
    Player* player = GET_PLAYER(play);

    DECR(this->timer);
    DECR(this->timer2);
    SkelAnime_Update(&this->b.skelAnime);
    thisx->shape.rot.y = thisx->world.rot.y;
    Actor_SetFocus(thisx, 20.0f);
    switch (this->state) {
        case KID_WANDER:
        case KID_RUN:
        case KID_TAUNT:
        case KID_DAZED:
            if (NvMmBomjimb_CheckCaught(this, play) ||
                ((this->state != KID_DAZED) && (NvMmBomjimb_CheckHit(this) || NvMmBomjimb_CheckBox(this, play)))) {
                break;
            }
            if (this->state == KID_WANDER) {
                NvMmBomjimb_Wander(this, play);
            } else if (this->state == KID_RUN) {
                NvMmBomjimb_Run(this, play);
            } else if (this->state == KID_TAUNT) {
                NvMmBomjimb_Taunt(this, play);
            } else if (this->timer == 0) {
                NvMmBomjimb_SetupTaunt(this);
            }
            break;
        case KID_CUCCO:
            NvMmBomjimb_Cucco(this, play);
            break;
        case KID_HIDING:
            NvMmBomjimb_Hiding(this, play);
            break;
        case KID_CAUGHT:
            NvMmBomjimb_CaughtText(this, play);
            break;
        case KID_SITTING: { // MM's func_80C0201C: his head turned to Link, talked to
            s16 d = thisx->yawTowardsPlayer - thisx->world.rot.y;

            this->b.headRotTargetX = ((thisx->xzDistToPlayer < 200.0f) && (ABS(d) < 0x61A8)) ? CLAMP(d, -10000, 10000) : 0;
            if (Actor_ProcessTalkRequest(thisx, play)) {
                this->state = KID_DAZED + 100; // (talking)
            } else {
                Actor_OfferTalk(thisx, play, 40.0f);
            }
            break;
        }
        case KID_NOT_PLAYING: // MM's func_80C02BCC
            Math_SmoothStepToS(&thisx->world.rot.y, thisx->yawTowardsPlayer, 1, 0x1388, 0);
            if (this->step == 0) {
                player->actor.freezeTimer = 3;
                if (NvMmBomber_TextEvent(play)) {
                    Message_CloseTextbox(play);
                    this->step = 1;
                }
            } else if (thisx->xzDistToPlayer > 200.0f) {
                NvMmBomjimb_SetupWander(this);
            }
            break;
        case KID_LEAVING:
            player->actor.freezeTimer = 3;
            break;
        default: // talking, sitting
            if (NvMmBomber_TextEvent(play)) {
                Message_CloseTextbox(play);
                this->state = KID_SITTING;
            }
            break;
    }
    Actor_MoveXZGravity(thisx);
    if ((this->state == KID_CUCCO) && (this->held != NULL) && (this->held->update != NULL)) { // the cucco overhead
        this->held->world.pos = thisx->world.pos;
        this->held->world.pos.y += (thisx->bgCheckFlags & BGCHECKFLAG_GROUND) ? 35.0f : 25.0f;
    }
    NvMmBomber_Common(&this->b, play);
    Collider_UpdateCylinder(thisx, &this->b.collider);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->b.collider.base);
    if (this->state != KID_HIDING) {
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->b.collider.base);
    }
}

void NvMmBomjimb_Draw(Actor* thisx, PlayState* play) {
    NvMmBomber_Draw(&((NvMmBomjimb*)thisx)->b, play);
}

/* ============================== En_Bom_Bowl_Man: the notebook ============================== */

static u16 sLineUpHuman[] = { 0x710, 0x711, 0x715, 0x716, 0x717, 0x718 };
static u16 sLineUpDeku[] = { 0x74F, 0x750, 0x751, 0x752 };
static Vec3f sLineUpPos[] = {
    { -730.0f, 200.0f, -2350.0f }, { -690.0f, 200.0f, -2350.0f }, { -650.0f, 200.0f, -2350.0f },
    { -610.0f, 200.0f, -2350.0f }, { -570.0f, 200.0f, -2350.0f },
};
static s32 sShowing;  // MM's D_809C6100: the line-up showing their numbers in turn
static s32 sSaluting; // MM's D_809C6104

struct NvMmBomBowl;
typedef void (*NvMmBomBowlFunc)(struct NvMmBomBowl*, PlayState*);

typedef struct NvMmBomBowl {
    NvMmBomber b;
    NvMmBomBowlFunc func;
    s16 type;     // MM's unk_2F6: 0 the line-up, 1 East Clock Town's
    s16 place;    // MM's unk_2F4: in the line, 0 Jim
    s16 step;     // MM's unk_2C0
    s16 timer;
    s16 talked;
    s16 pathPoint;
    Path* path;
    Vec3f target;
} NvMmBomBowl;

const size_t gNvMmBomBowlSize = sizeof(NvMmBomBowl);

static void NvMmBomBowl_LineUpTalk(NvMmBomBowl* this, PlayState* play);
static void NvMmBomBowl_Showing(NvMmBomBowl* this, PlayState* play);
static void NvMmBomBowl_Leave(NvMmBomBowl* this, PlayState* play);
static void NvMmBomBowl_GiveNotebook(NvMmBomBowl* this, PlayState* play);
static void NvMmBomBowl_AfterNotebook(NvMmBomBowl* this, PlayState* play);
static void NvMmBomBowl_Waiting(NvMmBomBowl* this, PlayState* play);
static void NvMmBomBowl_RunUp(NvMmBomBowl* this, PlayState* play);
static void NvMmBomBowl_EastTalk(NvMmBomBowl* this, PlayState* play);
static void NvMmBomBowl_RunOff(NvMmBomBowl* this, PlayState* play);

static s32 NvMmBomBowl_HasNotebook(void) {
    return NvMmItem_Has(MM_ITEM_BOMBERS_NOTEBOOK) || NvMm_CheckWeekEvent(WE_84_80);
}

static void NvMmBomBowl_SetupShowing(NvMmBomBowl* this) {
    NvMmBomber_Anim(&this->b, BA_IDLE, 1.0f);
    this->func = NvMmBomBowl_Showing;
}

/** MM's func_809C4BC4: Jim, with the others spawned in the line by the code. */
static void NvMmBomBowl_SetupLineUp(NvMmBomBowl* this, PlayState* play) {
    u8* code = NvMmMsg_BomberCode();
    s32 form = NvMmForm_Get();
    s32 i;

    for (i = 0; i < 5; i++) { // (the place in the line spells the code: the Bomber with number d stands at i)
        if (code[i] == 1) {
            this->b.actor.world.pos = sLineUpPos[i];
        } else {
            Actor_SpawnAsChild(&play->actorCtx, &this->b.actor, play, NvMmActor_Id(0x11C), sLineUpPos[i].x,
                               sLineUpPos[i].y, sLineUpPos[i].z, 0, this->b.actor.world.rot.y, 0, code[i]);
        }
    }
    NvMm_ClearWeekEvent(WE_73_10);
    NvMm_ClearWeekEvent(WE_85_02);
    { // (MM's cutscene camera shows them: here Link turned to face them)
        Player* player = GET_PLAYER(play);

        player->actor.shape.rot.y = player->actor.world.rot.y =
            Math_Vec3f_Yaw(&player->actor.world.pos, &sLineUpPos[2]);
        player->yaw = player->actor.shape.rot.y;
        player->actor.freezeTimer = 3;
    }
    NvMmBomber_Anim(&this->b, BA_TALK_HANDS_ON_HIPS, 1.0f);
    this->b.headRotTargetX = 0;
    this->step = 0;
    this->b.actor.textId = NV_MM_TEXT((form == FORM_HUMAN) ? sLineUpHuman[0] : sLineUpDeku[0]);
    Message_StartTextbox(play, this->b.actor.textId, &this->b.actor);
    NvMm_NotebookEvent(NOTEBOOK_MET_BOMBERS);
    this->func = NvMmBomBowl_LineUpTalk;
}

void NvMmBomBowl_Init(Actor* thisx, PlayState* play) {
    NvMmBomBowl* this = (NvMmBomBowl*)thisx;

    NvMmBomber_Init(&this->b, play);
    thisx->gravity = -3.0f;
    this->type = (thisx->params >> 4) & 0xF;
    this->place = thisx->params & 0xF;
    this->b.hideLimb0F = true;
    this->b.hideLimb13 = true;
    if (this->type == 0) {
        if (this->place == 0) { // Jim, number 1
            this->b.number = 0;
            NvMmBomBowl_SetupLineUp(this, play);
        } else { // the others (MM: params the number shown, 1-5)
            this->b.number = CLAMP(this->place - 1, 0, 4);
            this->place--;
            NvMmBomBowl_SetupShowing(this);
        }
        return;
    }
    // East Clock Town: out of the hideout, the code entered, no notebook yet
    this->b.number = 0;
    this->path = Path_GetByIndex(play, (thisx->params >> 8) & 0x3F, 0x3F);
    if ((gSaveContext.entranceIndex == ENTR_MM_Z2_TOWN_2) && NvMm_CheckWeekEvent(WE_ENTERED_BOMBERS_CODE) &&
        !NvMmBomBowl_HasNotebook()) {
        thisx->draw = NULL;
        thisx->flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        thisx->world.pos.x = 1340.0f;
        thisx->world.pos.z = -1795.0f;
        this->func = NvMmBomBowl_Waiting;
    } else {
        Actor_Kill(thisx);
    }
}

void NvMmBomBowl_Destroy(Actor* thisx, PlayState* play) {
    Collider_DestroyCylinder(play, &((NvMmBomBowl*)thisx)->b.collider);
}

/** MM's func_809C4DA4: Jim's lines in the line-up; the notebook (human Link) and the code. */
static void NvMmBomBowl_LineUpTalk(NvMmBomBowl* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s32 human = NvMmForm_Get() == FORM_HUMAN;

    player->actor.freezeTimer = 3;
    if ((this->timer != 0) || !NvMmBomber_TextEvent(play)) {
        return;
    }
    Message_CloseTextbox(play);
    switch (this->step) {
        case 0: // they salute
            this->step = 1;
            sSaluting = 1;
            this->timer = 10;
            NvMmBomber_Anim(&this->b, BA_SALUTE, 1.0f);
            break;
        case 1:
            sSaluting = 0;
            NvMmBomber_Anim(&this->b, BA_TALK_HANDS_ON_HIPS, 1.0f);
            this->step = (human && NvMmBomBowl_HasNotebook()) ? 4 : 2;
            break;
        case 2:
            if (human) { // the notebook
                this->func = NvMmBomBowl_GiveNotebook;
                return;
            }
            this->step = 3;
            // fallthrough: (Deku Link) the numbers shown
        case 4:
            NvMmBomber_Anim(&this->b, BA_HANDS_ON_HIPS, 1.0f);
            sShowing = 1;
            this->func = NvMmBomBowl_Showing;
            return;
        case 3:
            if (!human) {
                this->func = NvMmBomBowl_Leave;
                return;
            }
            this->step = 4;
            break;
        case 5: // the code learnt
            NvMm_NotebookEvent(NOTEBOOK_LEARNED_SECRET_CODE);
            NvMm_NotebookEvent(NOTEBOOK_RECEIVED_BOMBERS_NOTEBOOK);
            this->func = NvMmBomBowl_Leave;
            return;
    }
    this->b.actor.textId = NV_MM_TEXT(human ? sLineUpHuman[this->step] : sLineUpDeku[this->step]);
    Message_StartTextbox(play, this->b.actor.textId, &this->b.actor);
}

/** MM's func_809C5BF4: in turn each turns round and shows his number; then Jim speaks the code. */
static void NvMmBomBowl_Showing(NvMmBomBowl* this, PlayState* play) {
    if ((this->place == 0) && (sShowing != 0)) {
        GET_PLAYER(play)->actor.freezeTimer = 3;
    }
    if (sSaluting && (this->b.animIndex != BA_JUMP)) {
        NvMmBomber_Anim(&this->b, BA_JUMP, 1.0f);
    }
    if ((this->b.animIndex == BA_JUMP) && NvMmBomber_AnimDone(&this->b)) {
        NvMmBomber_Anim(&this->b, BA_IDLE, 1.0f);
    }
    if (sShowing == 0) {
        return;
    }
    if (((this->b.animIndex == BA_IDLE) || (this->b.animIndex == BA_HANDS_ON_HIPS)) && (sShowing == (this->place + 1))) {
        NvMmBomber_Anim(&this->b, BA_TURN_AROUND, 1.0f);
    }
    if ((this->b.animIndex == BA_TURN_AROUND) && NvMmBomber_AnimDone(&this->b)) {
        NvMmBomber_Anim(&this->b, BA_SHOW_NUMBER, 1.0f);
        sShowing++;
    }
    if ((this->place == 0) && (sShowing > 5)) { // (Jim) all shown: the code
        s32 human = NvMmForm_Get() == FORM_HUMAN;

        NvMmBomber_Anim(&this->b, BA_TALK_SHOWING_NUMBER, 1.0f);
        sShowing = 0;
        this->step = human ? 5 : 3;
        this->b.actor.textId = NV_MM_TEXT(human ? sLineUpHuman[this->step] : sLineUpDeku[this->step]);
        Message_StartTextbox(play, this->b.actor.textId, &this->b.actor);
        this->func = NvMmBomBowl_LineUpTalk;
    }
}

/** MM's func_809C51B4: done, North Clock Town afresh (the notebook kept, the game over). */
static void NvMmBomBowl_Leave(NvMmBomBowl* this, PlayState* play) {
    if ((play->msgCtx.msgMode == MSGMODE_NONE) || (Message_GetState(&play->msgCtx) == TEXT_STATE_DONE)) {
        NvMm_ClearWeekEvent(WE_75_40);
        if (NvMmForm_Get() == FORM_HUMAN) {
            NvMm_SetWeekEvent(WE_84_80);
            NvMm_ClearWeekEvent(WE_77_02);
        } else {
            NvMm_SetWeekEvent(WE_73_20);
            NvMm_ClearWeekEvent(WE_85_01);
        }
        play->nextEntranceIndex = ENTR_MM_Z2_BACKTOWN_6;
        play->transitionTrigger = TRANS_TRIGGER_START;
        play->transitionType = TRANS_TYPE_FADE_WHITE;
        gSaveContext.nextTransitionType = TRANS_TYPE_FADE_WHITE;
        this->func = NvMmBomBowl_Waiting;
        this->type = 2; // (nothing more)
    }
}

/** MM's func_809C59A4, func_809C59F0: the Bombers' Notebook. */
static void NvMmBomBowl_GiveNotebook(NvMmBomBowl* this, PlayState* play) {
    if (Actor_HasParent(&this->b.actor, play)) {
        this->b.actor.parent = NULL;
        if (this->type == 0) {
            this->step = 3;
            this->b.actor.textId = NV_MM_TEXT(sLineUpHuman[3]);
        } else {
            this->b.actor.textId = NV_MM_TEXT(0x716);
        }
        this->func = NvMmBomBowl_AfterNotebook;
    } else {
        GiveItemEntryFromActor(&this->b.actor, play, NvMmGetItem_Entry(MM_GI_BOMBERS_NOTEBOOK), 300.0f, 300.0f);
    }
}

static void NvMmBomBowl_AfterNotebook(NvMmBomBowl* this, PlayState* play) {
    if (play->msgCtx.msgMode == MSGMODE_NONE) { // (MM's talk exchange at 400: he speaks on, at once)
        NvMm_SetWeekEvent(WE_84_80);
        Message_StartTextbox(play, this->b.actor.textId, &this->b.actor);
        this->func = (this->type == 0) ? NvMmBomBowl_LineUpTalk : NvMmBomBowl_EastTalk;
    }
}

/* East Clock Town's (MM's func_809C5310 ...) */
static void NvMmBomBowl_Waiting(NvMmBomBowl* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if ((this->type == 1) && (player->actor.world.pos.x < 1510.0f)) {
        if (NvMmForm_Get() == FORM_DEKU) {
            Actor_Kill(&this->b.actor);
        } else if (this->b.actor.xzDistToPlayer < 80.0f) {
            this->b.actor.draw = (ActorFunc)NvMmBomBowl_Draw;
            this->b.actor.world.pos.x = 1360.0f;
            this->b.actor.world.pos.z = -1870.0f;
            NvMmBomber_Anim(&this->b, BA_RUN, 1.0f);
            this->func = NvMmBomBowl_RunUp;
        }
    }
}

static void NvMmBomBowl_RunUp(NvMmBomBowl* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Vec3f to = { 1350.0f, 0.0f, -1800.0f };

    player->actor.freezeTimer = 3;
    Math_SmoothStepToS(&this->b.actor.world.rot.y, Math_Vec3f_Yaw(&this->b.actor.world.pos, &player->actor.world.pos), 1,
                       0x1388, 0);
    Math_SmoothStepToS(&player->actor.shape.rot.y, Math_Vec3f_Yaw(&player->actor.world.pos, &this->b.actor.world.pos), 1,
                       0x1388, 0);
    player->actor.world.rot.y = player->actor.shape.rot.y;
    Math_ApproachF(&this->b.actor.world.pos.x, to.x, 0.3f, 2.0f);
    Math_ApproachF(&this->b.actor.world.pos.z, to.z, 0.3f, 2.0f);
    if (Math_Vec3f_DistXZ(&this->b.actor.world.pos, &to) < 4.0f) {
        this->b.actor.textId = NV_MM_TEXT(NvMm_CheckWeekEvent(WE_85_02) ? 0x730 : 0x72F);
        NvMmBomber_Anim(&this->b, BA_TALK_HANDS_ON_HIPS, 1.0f);
        Message_StartTextbox(play, this->b.actor.textId, &this->b.actor);
        this->func = NvMmBomBowl_EastTalk;
    }
}

/** MM's func_809C5598: his lines; the notebook; then off he runs. */
static void NvMmBomBowl_EastTalk(NvMmBomBowl* this, PlayState* play) {
    u16 id = this->b.actor.textId - NV_MM_TEXT(0);

    GET_PLAYER(play)->actor.freezeTimer = 3;
    if (!NvMmBomber_TextEvent(play)) {
        return;
    }
    Message_CloseTextbox(play);
    switch (id) {
        case 0x72F:
        case 0x730:
            id = 0x731;
            break;
        case 0x731:
        case 0x732:
        case 0x733:
            id++;
            break;
        case 0x734:
            id = 0x715;
            break;
        case 0x715:
            NvMmBomber_Anim(&this->b, BA_SALUTE, 1.0f);
            this->func = NvMmBomBowl_GiveNotebook;
            return;
        case 0x716:
            id = 0x735;
            break;
        default: // 0x735: the code learnt; off he runs
            NvMmBomber_Anim(&this->b, BA_RUN, 1.0f);
            NvMm_NotebookEvent(NOTEBOOK_LEARNED_SECRET_CODE);
            NvMm_NotebookEvent(NOTEBOOK_RECEIVED_BOMBERS_NOTEBOOK);
            NvMm_NotebookEvent(NOTEBOOK_MET_BOMBERS);
            this->pathPoint = 0;
            this->func = NvMmBomBowl_RunOff;
            return;
    }
    this->b.actor.textId = NV_MM_TEXT(id);
    Message_StartTextbox(play, this->b.actor.textId, &this->b.actor);
}

static void NvMmBomBowl_RunOff(NvMmBomBowl* this, PlayState* play) {
    Vec3s* p;
    Vec3f to;

    if ((this->path == NULL) || (this->pathPoint >= this->path->count)) {
        NvMm_SetWeekEvent(WE_84_80);
        NvMm_ClearWeekEvent(WE_83_04);
        Actor_Kill(&this->b.actor);
        return;
    }
    p = &((Vec3s*)SEGMENTED_TO_VIRTUAL(this->path->points))[this->pathPoint];
    to.x = p->x;
    to.y = p->y;
    to.z = p->z;
    Math_SmoothStepToS(&this->b.actor.world.rot.y, Math_Vec3f_Yaw(&this->b.actor.world.pos, &to), 1, 0x7D0, 0xA);
    Math_ApproachF(&this->b.actor.world.pos.x, to.x, 0.5f, 6.0f);
    Math_ApproachF(&this->b.actor.world.pos.z, to.z, 0.5f, 6.0f);
    if (Math_Vec3f_DistXZ(&this->b.actor.world.pos, &to) < 4.0f) {
        this->pathPoint++;
    }
}

void NvMmBomBowl_Update(Actor* thisx, PlayState* play) {
    NvMmBomBowl* this = (NvMmBomBowl*)thisx;

    DECR(this->timer);
    SkelAnime_Update(&this->b.skelAnime);
    thisx->shape.rot.y = thisx->world.rot.y;
    Actor_SetFocus(thisx, 20.0f);
    this->func(this, play);
    Actor_MoveXZGravity(thisx);
    NvMmBomber_Common(&this->b, play);
}

void NvMmBomBowl_Draw(Actor* thisx, PlayState* play) {
    NvMmBomber_Draw(&((NvMmBomBowl*)thisx)->b, play);
}
