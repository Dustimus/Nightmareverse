/*
 * Nightmareverse: the Mayor's meeting (mm/README.md "The Mayor's meeting"), from MM's En_Dt (Mayor
 * Dotour, object_dt), En_Baisen (Viscen, object_bai) and En_Heishi (the soldier, MM's object_sdn),
 * zeldaret/mm, ported closely; Mutoh (z_nv_mm_townprops.c) takes his part as in MM.
 *
 * Talking to any of the three in the Mayor's office starts the meeting's talk, the mayor leading:
 * MM's lines in order, each looked at by the others (sCsFocusTargets), his look and animation by the
 * stage (before, after, the resolution), week event 60_08 once heard. Wearing the Couple's Mask: the
 * resolution (everyone's reactions), Mutoh and Viscen leave (63_80 and back in through the door,
 * MM's entrance 1), the mayor thanks Link with the piece of heart (60_10) and the notebook events.
 * After that Viscen talks with the soldier in the lobby, the soldier only stands. On the final night the
 * mayor is alone at his desk (60_40 once heard).
 *
 * Adapted from MM: MM shows each speaker with a cutscene camera (the meeting's CutsceneManager
 * entries); here OoT's talk camera stays and the others turn their heads to the one speaking. MM's
 * office music and crowd sound aren't in OoT's banks.
 */
#include "global.h"
#include "z_nv_mm_meeting.h"
#include <stdio.h>
#include <stdlib.h>

#define NV_MM_TEXT(id) (0xB000 + (id))
#define MM_WE(byte, bit) (((byte) << 8) | (bit))
#define WE_ATTENDED_MAYOR_MEETING MM_WE(60, 0x08)
#define WE_RECEIVED_MAYOR_HEART_PIECE MM_WE(60, 0x10)
#define WE_TALKED_MAYOR_NIGHT_3 MM_WE(60, 0x40)
#define WE_RESOLVED_MAYOR_MEETING MM_WE(63, 0x80)
#define MM_PLAYER_MASK_KAFEIS_MASK 2
#define MM_PLAYER_MASK_COUPLE 0x0A
#define MM_GI_HEART_PIECE 0x0C
#define NOTEBOOK_PERSON_MAYOR_DOTOUR 0x07
#define NOTEBOOK_EVENT_RECEIVED_MAYOR_HP 0x26
#define MM_EN_MUTO 0x26B
#define MM_EN_BAISEN 0x26C
#define MM_EN_HEISHI 0x26D

s32 NvMm_CheckWeekEvent(s32 flag);
void NvMm_SetWeekEvent(s32 flag);
s32 NvMmClock_Day(void);
s32 NvMmClock_IsNight(void);
s32 NvMmMask_Worn(void);
s32 NvMmActor_Id(s32 mmId);
void NvMm_NotebookEvent(u16 event);
GetItemEntry NvMmGetItem_Entry(s32 mmGi);

static s32 NvMmMayor_FinalNight(void) {
    return (NvMmClock_Day() == 3) && NvMmClock_IsNight();
}

static Actor* NvMmMayor_Find(PlayState* play, s32 mmId) {
    Actor* a;

    for (a = play->actorCtx.actorLists[ACTORCAT_NPC].head; a != NULL; a = a->next) {
        if (a->id == NvMmActor_Id(mmId)) {
            return a;
        }
    }
    return NULL;
}

static s16 NvMmMayor_HeadTarget(Actor* actor, s16 yawTo) {
    s16 d = yawTo - actor->world.rot.y;

    return (ABS(d) < 0x4E20) ? CLAMP(d, -0x2710, 0x2710) : 0;
}

/* ---------------------------------------------------------------------------------------------- */
/* Viscen (En_Baisen)                                                                             */

#define BAI(name) ("__OTR__mm/objects/object_bai/" name)
#define OBJECT_BAI_LIMB_09 9
#define OBJECT_BAI_LIMB_MAX 20

static const u16 sBaisenTextIds[] = { 0x2ABD, 0x2ABB, 0x2AD5, 0x2AD6, 0x2AD7, 0x2AD8, 0x2AC6 };
static const char* sBaisenAnims[] = { BAI("object_bai_Anim_0011C0"), BAI("object_bai_Anim_0008B4"),
                                      BAI("object_bai_Anim_008198") };

struct NvMmBaisen;
typedef void (*NvMmBaisenFunc)(struct NvMmBaisen*, PlayState*);

typedef struct NvMmBaisen {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[OBJECT_BAI_LIMB_MAX];
    Vec3s morphTable[OBJECT_BAI_LIMB_MAX];
    NvMmBaisenFunc func;
    s16 headRotX; // (MM's names: X turns the head side to side on this skeleton)
    s16 headRotXTarget;
    s32 animIndex;
    s32 paramCopy;
    s32 lookAround; // MM's unk290
    s16 yawTarget;  // MM's unk29E
    Actor* heishi;
    NvMmMeetingRole role;
    ColliderCylinder collider;
} NvMmBaisen;

size_t gNvMmBaisenSize = sizeof(NvMmBaisen);

NvMmMeetingRole* NvMmBaisen_Role(Actor* baisen) {
    return &((NvMmBaisen*)baisen)->role;
}

static ColliderCylinderInit sNpcCylinderInit = {
    { COLTYPE_NONE, AT_NONE, AC_NONE, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_2, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0, 0, 0 }, { 0xF7CFFFFF, 0, 0 }, TOUCH_NONE | TOUCH_SFX_NORMAL, BUMP_NONE, OCELEM_ON },
    { 20, 60, 0, { 0, 0, 0 } },
};

static void NvMmBaisen_FindHeishi(NvMmBaisen* this, PlayState* play);
static void NvMmBaisen_Wait(NvMmBaisen* this, PlayState* play);

static void NvMmBaisen_ChangeAnim(NvMmBaisen* this, s32 anim) {
    AnimationHeader* a = (AnimationHeader*)sBaisenAnims[anim];

    this->animIndex = anim;
    Animation_Change(&this->skelAnime, a, 1.0f, 0.0f, Animation_GetLastFrame(a), ANIMMODE_LOOP, -10.0f);
}

void NvMmBaisen_Init(Actor* thisx, PlayState* play) {
    NvMmBaisen* this = (NvMmBaisen*)thisx;

    ActorShape_Init(&thisx->shape, 0.0f, ActorShadow_DrawCircle, 25.0f);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)BAI("object_bai_Skel_007908"),
                       (AnimationHeader*)sBaisenAnims[0], this->jointTable, this->morphTable, OBJECT_BAI_LIMB_MAX);
    thisx->colChkInfo.mass = MASS_IMMOVABLE;
    this->paramCopy = thisx->params;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sNpcCylinderInit);
    if (this->paramCopy == 0) { // in the lobby with the soldier, once the meeting's settled
        this->lookAround = true;
        if (!NvMm_CheckWeekEvent(WE_RESOLVED_MAYOR_MEETING) && !NvMmMayor_FinalNight()) {
            Actor_Kill(thisx);
            return;
        }
    } else { // at the meeting
        this->collider.dim.radius = 30;
        if (NvMm_CheckWeekEvent(WE_RESOLVED_MAYOR_MEETING) || NvMmMayor_FinalNight()) {
            Actor_Kill(thisx);
            return;
        }
    }
    thisx->targetMode = 6;
    thisx->gravity = -3.0f;
    this->func = (this->paramCopy == 0) ? NvMmBaisen_FindHeishi : NvMmBaisen_Wait;
    if (this->paramCopy != 0) {
        NvMmBaisen_ChangeAnim(this, 0);
    }
}

void NvMmBaisen_Destroy(Actor* thisx, PlayState* play) {
    Collider_DestroyCylinder(play, &((NvMmBaisen*)thisx)->collider);
}

/** MM's func_80BE87FC. */
static void NvMmBaisen_SetupWait(NvMmBaisen* this) {
    if (this->paramCopy == 0) {
        this->role.textIdIndex = 2;
        NvMmBaisen_ChangeAnim(this, 2);
        this->yawTarget = this->actor.world.rot.y;
    } else {
        NvMmBaisen_ChangeAnim(this, 0);
    }
    this->actor.textId = NV_MM_TEXT(sBaisenTextIds[this->role.textIdIndex]);
    this->func = NvMmBaisen_Wait;
}

static void NvMmBaisen_FindHeishi(NvMmBaisen* this, PlayState* play) {
    this->heishi = NvMmMayor_Find(play, MM_EN_HEISHI);
    NvMmBaisen_SetupWait(this);
}

static void NvMmBaisen_InMeeting(NvMmBaisen* this, PlayState* play);
static void NvMmBaisen_WithSoldier(NvMmBaisen* this, PlayState* play);

/** MM's func_80BE895C. */
static void NvMmBaisen_SetupDialogue(NvMmBaisen* this, PlayState* play) {
    if (this->role.targetActor != NULL) {
        this->lookAround = true;
        this->role.cutsceneState = 1;
    }
    if (this->paramCopy == 0) {
        this->role.targetActor = this->heishi;
        this->func = NvMmBaisen_WithSoldier;
    } else {
        this->func = NvMmBaisen_InMeeting;
    }
}

/** MM's func_80BE887C. */
static void NvMmBaisen_Wait(NvMmBaisen* this, PlayState* play) {
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        NvMmBaisen_SetupDialogue(this, play);
        return;
    }
    if (this->paramCopy != 0) {
        this->role.textIdIndex = NvMm_CheckWeekEvent(WE_ATTENDED_MAYOR_MEETING) ? 1 : 0;
        if (NvMmMask_Worn() == MM_PLAYER_MASK_COUPLE) {
            this->role.textIdIndex = 6;
        }
        if (this->role.cutsceneState == 1) {
            NvMmBaisen_SetupDialogue(this, play);
            return;
        }
    }
    this->actor.textId = NV_MM_TEXT(sBaisenTextIds[this->role.textIdIndex]);
    Actor_OfferTalk(&this->actor, play, 70.0f);
}

/** MM's func_80BE89D8: at the meeting, looking at whoever speaks. */
static void NvMmBaisen_InMeeting(NvMmBaisen* this, PlayState* play) {
    u16 cur = play->msgCtx.textId;

    if ((this->role.targetActor == &this->actor) || (this->role.targetActor == NULL)) {
        this->yawTarget = this->actor.world.rot.y;
        if (this->animIndex == 0) {
            NvMmBaisen_ChangeAnim(this, 1);
        }
    } else {
        this->yawTarget = Math_Vec3f_Yaw(&this->actor.world.pos, &this->role.targetActor->world.pos);
        if (this->animIndex != 0) {
            NvMmBaisen_ChangeAnim(this, 0);
        }
    }
    if ((cur == NV_MM_TEXT(0x2AC6)) || (cur == NV_MM_TEXT(0x2AC7)) || (cur == NV_MM_TEXT(0x2AC8))) {
        this->skelAnime.playSpeed = 0.0f;
        this->yawTarget = this->actor.yawTowardsPlayer;
    }
    if (this->role.cutsceneState == 2) { // (the mayor ended it)
        NvMmBaisen_SetupWait(this);
    }
}

/** MM's func_80BE8AAC: in the lobby, he and the soldier take turns. */
static void NvMmBaisen_WithSoldier(NvMmBaisen* this, PlayState* play) {
    if ((this->role.textIdIndex % 2) != 0) {
        this->yawTarget = this->actor.world.rot.y;
        if (this->animIndex == 0) {
            NvMmBaisen_ChangeAnim(this, 1);
        }
    } else {
        if (this->role.targetActor != NULL) {
            this->yawTarget = Math_Vec3f_Yaw(&this->actor.world.pos, &this->role.targetActor->world.pos);
        }
        if (this->animIndex != 0) {
            NvMmBaisen_ChangeAnim(this, 0);
        }
    }
    if ((Message_GetState(&play->msgCtx) == TEXT_STATE_EVENT) && Message_ShouldAdvance(play)) {
        this->role.textIdIndex++;
        if (this->role.textIdIndex < 6) {
            Message_ContinueTextbox(play, NV_MM_TEXT(sBaisenTextIds[this->role.textIdIndex]));
            this->role.targetActor = ((this->role.textIdIndex % 2) == 0) ? this->heishi : &this->actor;
        } else {
            Message_CloseTextbox(play);
            NvMmBaisen_SetupWait(this);
        }
    }
}

void NvMmBaisen_Update(Actor* thisx, PlayState* play) {
    NvMmBaisen* this = (NvMmBaisen*)thisx;

    SkelAnime_Update(&this->skelAnime);
    thisx->shape.rot.y = thisx->world.rot.y;
    if ((this->paramCopy != 0) && NvMmMayor_FinalNight()) {
        Actor_Kill(thisx);
        return;
    }
    this->func(this, play);
    Actor_MoveXZGravity(thisx);
    Actor_UpdateBgCheckInfo(play, thisx, 20.0f, 20.0f, 50.0f, 0x1D);
    Actor_SetScale(thisx, 0.01f);
    if (this->lookAround) {
        this->headRotXTarget = (thisx->xzDistToPlayer < 200.0f) ? NvMmMayor_HeadTarget(thisx, this->yawTarget) : 0;
    }
    Actor_SetFocus(thisx, 60.0f);
    Math_SmoothStepToS(&this->headRotX, this->headRotXTarget, 1, 0xBB8, 0);
    Collider_UpdateCylinder(thisx, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

static s32 NvMmBaisen_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                       void* thisx) {
    if (limbIndex == OBJECT_BAI_LIMB_09) {
        rot->x += ((NvMmBaisen*)thisx)->headRotX;
    }
    return false;
}

void NvMmBaisen_Draw(Actor* thisx, PlayState* play) {
    NvMmBaisen* this = (NvMmBaisen*)thisx;

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          NvMmBaisen_OverrideLimbDraw, NULL, this);
}

/* ---------------------------------------------------------------------------------------------- */
/* The soldier (En_Heishi)                                                                        */

#define SDN(name) ("__OTR__mm/objects/object_sdn/" name)
#define SOLDIER_LIMB_HEAD 16
#define SOLDIER_LIMB_MAX 17

typedef struct NvMmHeishi {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[SOLDIER_LIMB_MAX];
    Vec3s morphTable[SOLDIER_LIMB_MAX];
    s16 headRotX;
    s16 headRotXTarget;
    s32 paramsCopy;
    s32 shouldSetHeadRotation;
    ColliderCylinder collider;
} NvMmHeishi;

size_t gNvMmHeishiSize = sizeof(NvMmHeishi);

void NvMmHeishi_Init(Actor* thisx, PlayState* play) {
    NvMmHeishi* this = (NvMmHeishi*)thisx;
    AnimationHeader* idle = (AnimationHeader*)SDN("gSoldierStandHandOnHipAnim");

    ActorShape_Init(&thisx->shape, 0.0f, ActorShadow_DrawCircle, 25.0f);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)SDN("gSoldierSkel"), idle, this->jointTable,
                       this->morphTable, SOLDIER_LIMB_MAX);
    thisx->colChkInfo.mass = MASS_IMMOVABLE;
    this->paramsCopy = thisx->params;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sNpcCylinderInit);
    if (this->paramsCopy == 0) {
        this->shouldSetHeadRotation = true;
        if (!NvMm_CheckWeekEvent(WE_RESOLVED_MAYOR_MEETING) && !NvMmMayor_FinalNight()) {
            Actor_Kill(thisx);
            return;
        }
    } else {
        this->collider.dim.radius = 30;
        if (NvMm_CheckWeekEvent(WE_RESOLVED_MAYOR_MEETING) || NvMmMayor_FinalNight()) {
            Actor_Kill(thisx);
            return;
        }
    }
    thisx->targetMode = 6;
    thisx->gravity = -3.0f;
    thisx->flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    Animation_Change(&this->skelAnime, idle, 1.0f, 0.0f, Animation_GetLastFrame(idle), ANIMMODE_LOOP, -10.0f);
}

void NvMmHeishi_Destroy(Actor* thisx, PlayState* play) {
    Collider_DestroyCylinder(play, &((NvMmHeishi*)thisx)->collider);
}

void NvMmHeishi_Update(Actor* thisx, PlayState* play) {
    NvMmHeishi* this = (NvMmHeishi*)thisx;

    SkelAnime_Update(&this->skelAnime);
    thisx->shape.rot.y = thisx->world.rot.y;
    if ((this->paramsCopy != 0) && NvMmMayor_FinalNight()) {
        Actor_Kill(thisx);
        return;
    }
    Actor_MoveXZGravity(thisx);
    Actor_UpdateBgCheckInfo(play, thisx, 20.0f, 20.0f, 50.0f, 0x1D);
    Actor_SetScale(thisx, 0.01f);
    if (this->shouldSetHeadRotation) { // (MM's: his yawTowardsPlayer is fixed at his facing, so ahead)
        this->headRotXTarget = 0;
    }
    Actor_SetFocus(thisx, 60.0f);
    Math_SmoothStepToS(&this->headRotX, this->headRotXTarget, 1, 0xBB8, 0);
    Collider_UpdateCylinder(thisx, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

static s32 NvMmHeishi_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                       void* thisx) {
    if (limbIndex == SOLDIER_LIMB_HEAD) {
        rot->x += ((NvMmHeishi*)thisx)->headRotX;
    }
    return false;
}

void NvMmHeishi_Draw(Actor* thisx, PlayState* play) {
    NvMmHeishi* this = (NvMmHeishi*)thisx;

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          NvMmHeishi_OverrideLimbDraw, NULL, this);
}

/* ---------------------------------------------------------------------------------------------- */
/* Mayor Dotour (En_Dt)                                                                           */

#define DT(name) ("__OTR__mm/objects/object_dt/" name)
#define OBJECT_DT_LIMB_0C 12
#define OBJECT_DT_LIMB_MAX 15

enum { DT_STATE_DEFAULT, DT_STATE_IDLE, DT_STATE_VIEWING_MEETING, DT_STATE_OFFERED_REWARD, DT_STATE_WAIT_FINAL_NIGHT,
       DT_STATE_DONE_FINAL_NIGHT };
enum { FOCUS_MUTO, FOCUS_BAISEN, FOCUS_MAYOR };
enum { APPEAR_DEFAULT, APPEAR_PRE_MEETING, APPEAR_POST_MEETING, APPEAR_UNUSED, APPEAR_RESOLUTION_TALK,
       APPEAR_RESOLVED_MEETING, APPEAR_MAX };
enum { DT_ANIM_WAIT, DT_ANIM_DISTRESS, DT_ANIM_SHOCK, DT_ANIM_TAP_DESK, DT_ANIM_UPRIGHT, DT_ANIM_SIT_UP };
enum { EYE_SHOCK, EYE_OPEN, EYE_CLOSED, EYE_LOOK_DOWN, EYE_SQUINT };

static const u16 sDtTextIds[] = {
    0x2ABD, 0x2ABE, 0x2ABF, 0x2AC0, 0x2ABE, 0x2AC1, 0x2AC2, 0x2AC3, 0x2AC4, 0x2ABB, 0x2ABC, 0x2AC6, 0x2AC7, 0x2AC8,
    0x2AC9, 0x2ACA, 0x2ACB, 0x2ACC, 0x2ACD, 0x2ACE, 0x2ACF, 0x2AD0, 0x2AD1, 0x2AC5, 0x2AD3, 0x2AD4, 0x2AD2,
};
static const u8 sCsFocusTargets[] = {
    FOCUS_BAISEN, FOCUS_MAYOR, FOCUS_MUTO,   FOCUS_MUTO,  FOCUS_MAYOR,  FOCUS_BAISEN, FOCUS_MAYOR, FOCUS_MUTO,
    FOCUS_MAYOR,  // the first meeting
    FOCUS_BAISEN, FOCUS_MUTO, // again
    FOCUS_BAISEN, FOCUS_MAYOR, FOCUS_MUTO,   FOCUS_MAYOR, FOCUS_MUTO,   FOCUS_BAISEN, FOCUS_MAYOR, FOCUS_BAISEN,
    FOCUS_MAYOR,  FOCUS_MUTO,  FOCUS_MAYOR,  FOCUS_MAYOR, // the resolution
    FOCUS_MAYOR,  // the final night
};
static const char* sDtAnims[] = { DT("gDotourWaitAnim"),     DT("gDotourDistressAnim"), DT("gDotourShockAnim"),
                                  DT("gDotourTapDeskAnim"),  DT("gDotourUprightAnim"),  DT("gDotourSitUpAnim") };
static const u8 sDtAnimModes[] = { ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_ONCE, ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_ONCE };
static const s32 sAppearances[APPEAR_MAX][3] = {
    // animation, eyes, no blinking
    { DT_ANIM_DISTRESS, EYE_LOOK_DOWN, true },  { DT_ANIM_SHOCK, EYE_LOOK_DOWN, true },
    { DT_ANIM_TAP_DESK, EYE_LOOK_DOWN, true },  { DT_ANIM_TAP_DESK, EYE_SHOCK, false },
    { DT_ANIM_SIT_UP, EYE_SHOCK, false },       { DT_ANIM_UPRIGHT, EYE_SHOCK, false },
};
static const char* sDtEyes[] = { DT("gDotourEyeShockTex"), DT("gDotourEyeOpenTex"), DT("gDotourEyeClosedTex"),
                                 DT("gDotourEyeLookDownTex"), DT("gDotourEyeSquintTex") };
static const char* sDtBrows[] = { DT("gDotourEyebrowHighTex"), DT("gDotourEyebrowMidTex"), DT("gDotourEyebrowLowTex") };

struct NvMmDt;
typedef void (*NvMmDtFunc)(struct NvMmDt*, PlayState*);

typedef struct NvMmDt {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[OBJECT_DT_LIMB_MAX];
    Vec3s morphTable[OBJECT_DT_LIMB_MAX];
    NvMmDtFunc func;
    Actor* muto;
    Actor* baisen;
    Actor* targetActor;
    s16 headRotY;
    s16 headRotTargetY;
    s32 animIndex;
    f32 animEndFrame;
    s16 textIdIndex;
    s16 timer;
    s16 blinkTimer;
    s16 eyeTexIndex;
    s32 disableBlinking;
    s32 appearancePhase;
    s32 state;
    s32 meetingFinished;
    s32 showedCouplesMask;
    ColliderCylinder collider;
    s16 csIds[12]; // MM's: its cutscene and the additional ones after it (z_nv_mm_csmgr.c)
    s16 csIdIndex; // into sStringIdCsIndexTable
    s32 csState;   // MM's EnDtCutsceneState
} NvMmDt;

enum { DT_CS_NONE, DT_CS_WAITING, DT_CS_PLAYING };

// MM's sStringIdCsIndexTable: which of his cutscenes (the shot) shows each line
static const s16 sStringIdCsIndexTable[] = {
    0x2ABB, 0, 0x2ABD, 0, 0x2AC0, 1, 0x2AC1, 2, 0x2AC4, 3, 0x2AC6, 4,
    0x2AC7, 5, 0x2AC8, 6, 0x2AC9, 7, 0x2ACC, 8, 0x2ACF, 9, 0x2AD0, 10,
};

s32 NvMmCs_ActorCsId(Actor* actor);
void NvMmCs_SetTargetActor(Actor* actor);
s16 NvMmCsMgr_GetAdditionalCsId(s16 csId);
void NvMmCsMgr_Queue(s16 csId);
s16 NvMmCsMgr_IsNext(s16 csId);
s16 NvMmCsMgr_StartWithPlayerCs(s16 csId, Actor* actor);
s16 NvMmCsMgr_Stop(s16 csId);

size_t gNvMmDtSize = sizeof(NvMmDt);

static void NvMmDt_SetupCutsceneNpcs(NvMmDt* this, PlayState* play);
static void NvMmDt_OfferRegularTalk(NvMmDt* this, PlayState* play);
static void NvMmDt_UpdateMeeting(NvMmDt* this, PlayState* play);
static void NvMmDt_SetupFinalNightState(NvMmDt* this, PlayState* play);

static void NvMmDt_ChangeAnim(NvMmDt* this, s32 anim) {
    AnimationHeader* a = (AnimationHeader*)sDtAnims[anim];

    this->animIndex = anim;
    this->animEndFrame = Animation_GetLastFrame(a);
    Animation_Change(&this->skelAnime, a, 1.0f, 0.0f, this->animEndFrame, sDtAnimModes[anim],
                     ((anim == DT_ANIM_SHOCK) || (anim == DT_ANIM_SIT_UP)) ? 0.0f : -4.0f);
}

static void NvMmDt_UpdateAppearance(NvMmDt* this) {
    NvMmDt_ChangeAnim(this, sAppearances[this->appearancePhase][0]);
    this->eyeTexIndex = sAppearances[this->appearancePhase][1];
    this->disableBlinking = sAppearances[this->appearancePhase][2];
}

static void NvMmDt_SetText(NvMmDt* this) {
    this->actor.textId = NV_MM_TEXT(sDtTextIds[this->textIdIndex]);
}

/** MM's loop over sStringIdCsIndexTable: the shot for the line showing (or about to). */
static s32 NvMmDt_FindCsIdIndex(NvMmDt* this, PlayState* play) {
    s32 i;

    for (i = 0; i < ARRAY_COUNT(sStringIdCsIndexTable); i += 2) {
        if ((play->msgCtx.textId == NV_MM_TEXT(sStringIdCsIndexTable[i])) ||
            (this->actor.textId == NV_MM_TEXT(sStringIdCsIndexTable[i]))) {
            this->csIdIndex = i;
            return true;
        }
    }
    return false;
}

/** After a line changes: the shot changes with it (MM: stop the one playing, queue the next). */
static void NvMmDt_UpdateShot(NvMmDt* this, PlayState* play, s32 prevIndex) {
    s32 index;

    NvMmDt_FindCsIdIndex(this, play);
    index = sStringIdCsIndexTable[this->csIdIndex + 1];
    if ((this->csState == DT_CS_PLAYING) && (index != prevIndex)) {
        this->csState = DT_CS_WAITING;
        NvMmCsMgr_Stop(this->csIds[prevIndex]);
        NvMmCsMgr_Queue(this->csIds[index]);
    }
    NvMmCs_SetTargetActor(this->targetActor); // (MM's Camera_SetTargetActor on the cutscene's camera)
}

/** The meeting's shot over. */
static void NvMmDt_StopShot(NvMmDt* this) {
    if (this->csState == DT_CS_PLAYING) {
        NvMmCsMgr_Stop(this->csIds[sStringIdCsIndexTable[this->csIdIndex + 1]]);
    }
    this->csState = DT_CS_NONE;
}

/** MM's EnDt_UpdateCutsceneFocusTarget: who everyone looks at for this line. */
static void NvMmDt_UpdateFocusTarget(NvMmDt* this) {
    Actor* target;

    if ((this->muto == NULL) || (this->baisen == NULL)) {
        return;
    }
    switch (sCsFocusTargets[CLAMP(this->textIdIndex, 0, ARRAY_COUNT(sCsFocusTargets) - 1)]) {
        case FOCUS_MUTO:
            target = this->muto;
            break;
        case FOCUS_BAISEN:
            target = this->baisen;
            break;
        default:
            target = &this->actor;
            break;
    }
    NvMmMuto_Role(this->muto)->targetActor = target;
    NvMmBaisen_Role(this->baisen)->targetActor = target;
    this->targetActor = target;
}

void NvMmDt_Init(Actor* thisx, PlayState* play) {
    NvMmDt* this = (NvMmDt*)thisx;

    thisx->colChkInfo.mass = MASS_IMMOVABLE;
    ActorShape_Init(&thisx->shape, 0.0f, ActorShadow_DrawCircle, 19.0f);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)DT("object_dt_Skel_00B0CC"),
                       (AnimationHeader*)sDtAnims[DT_ANIM_WAIT], this->jointTable, this->morphTable,
                       OBJECT_DT_LIMB_MAX);
    thisx->targetMode = 6;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sNpcCylinderInit);
    this->collider.dim.radius = 25;
    this->collider.dim.height = 70;
    if (NvMmMayor_FinalNight()) {
        NvMmDt_SetupFinalNightState(this, play);
    }
    this->func = NvMmDt_SetupCutsceneNpcs;
}

void NvMmDt_Destroy(Actor* thisx, PlayState* play) {
    Collider_DestroyCylinder(play, &((NvMmDt*)thisx)->collider);
}

/** MM's EnDt_SetupRegularState: his line and look by the meeting's stage. */
static void NvMmDt_SetupRegularState(NvMmDt* this, PlayState* play) {
    this->textIdIndex = 0;
    if (!NvMm_CheckWeekEvent(WE_RECEIVED_MAYOR_HEART_PIECE)) {
        if (NvMm_CheckWeekEvent(WE_RESOLVED_MAYOR_MEETING)) { // back through the door after the resolution
            this->textIdIndex = 21;
            this->appearancePhase = APPEAR_RESOLVED_MEETING;
            NvMmDt_UpdateAppearance(this);
            NvMmDt_SetText(this);
            Message_StartTextbox(play, this->actor.textId, &this->actor);
            Player_SetCsActionWithHaltedActors(play, &this->actor, 1); // (PLAYER_CSACTION_WAIT)
            this->state = DT_STATE_IDLE;
            this->func = NvMmDt_UpdateMeeting;
            return;
        }
    } else if (NvMm_CheckWeekEvent(WE_RESOLVED_MAYOR_MEETING)) {
        this->textIdIndex = 23;
        this->appearancePhase = APPEAR_RESOLVED_MEETING;
        this->meetingFinished = true;
        NvMmDt_UpdateAppearance(this);
        NvMm_NotebookEvent(NOTEBOOK_PERSON_MAYOR_DOTOUR);
    }
    if (this->appearancePhase == APPEAR_DEFAULT) {
        if (NvMm_CheckWeekEvent(WE_ATTENDED_MAYOR_MEETING)) {
            this->textIdIndex = 9;
            this->appearancePhase = APPEAR_POST_MEETING;
        }
        NvMmDt_UpdateAppearance(this);
        if (!NvMm_CheckWeekEvent(WE_ATTENDED_MAYOR_MEETING)) {
            this->appearancePhase = APPEAR_PRE_MEETING;
        }
    }
    NvMmDt_SetText(this);
    if (!NvMm_CheckWeekEvent(WE_RESOLVED_MAYOR_MEETING)) {
        NvMmDt_UpdateFocusTarget(this);
    }
    this->state = DT_STATE_IDLE;
    this->func = NvMmDt_OfferRegularTalk;
}

static void NvMmDt_SetupCutsceneNpcs(NvMmDt* this, PlayState* play) {
    if (this->state == DT_STATE_WAIT_FINAL_NIGHT) {
        return;
    }
    this->muto = NvMmMayor_Find(play, MM_EN_MUTO);
    this->baisen = NvMmMayor_Find(play, MM_EN_BAISEN);
    {
        s16 csId = NvMmCs_ActorCsId(&this->actor);
        s32 i;

        for (i = 0; i < ARRAY_COUNT(this->csIds); i++) {
            this->csIds[i] = csId;
            if (csId != -1) {
                csId = NvMmCsMgr_GetAdditionalCsId(csId);
            }
        }
    }
    NvMmDt_SetupRegularState(this, play);
}

/** MM's EnDt_SetupMeetingCutscene (the cutscene camera left out). */
static void NvMmDt_SetupMeeting(NvMmDt* this, PlayState* play) {
    if (!NvMm_CheckWeekEvent(WE_RESOLVED_MAYOR_MEETING) && (this->muto != NULL) && (this->baisen != NULL)) {
        NvMmMuto_Role(this->muto)->cutsceneState = 1;
        NvMmBaisen_Role(this->baisen)->cutsceneState = 1;
        if (NvMmMask_Worn() == MM_PLAYER_MASK_COUPLE) {
            NvMmMuto_Role(this->muto)->textIdIndex = 4;
            NvMmBaisen_Role(this->baisen)->textIdIndex = 6;
            this->appearancePhase = NvMm_CheckWeekEvent(WE_ATTENDED_MAYOR_MEETING) ? APPEAR_RESOLUTION_TALK
                                                                                   : APPEAR_RESOLVED_MEETING;
            NvMmDt_UpdateAppearance(this);
        }
    }
    this->csState = NvMmDt_FindCsIdIndex(this, play) ? DT_CS_WAITING : DT_CS_NONE;
    if (getenv("NV_LOG") != NULL) {
        printf("NVMM mayor: the meeting's talk starts at line %d\n", (int)this->textIdIndex);
        fflush(stdout);
    }
    this->state = DT_STATE_VIEWING_MEETING;
    this->func = NvMmDt_UpdateMeeting;
}

/** MM's EnDt_OfferRegularTalk. */
static void NvMmDt_OfferRegularTalk(NvMmDt* this, PlayState* play) {
    s32 resolved = NvMm_CheckWeekEvent(WE_RESOLVED_MAYOR_MEETING);

    if (!resolved) {
        this->showedCouplesMask = false;
        this->textIdIndex = 0;
        if (NvMmMask_Worn() == MM_PLAYER_MASK_COUPLE) {
            this->textIdIndex = 11;
            this->showedCouplesMask = true;
            if ((this->muto != NULL) && (this->baisen != NULL)) {
                NvMmMuto_Role(this->muto)->textIdIndex = 4;
                NvMmBaisen_Role(this->baisen)->textIdIndex = 6;
            }
        } else if (NvMm_CheckWeekEvent(WE_ATTENDED_MAYOR_MEETING)) {
            this->textIdIndex = 9;
        }
    }
    NvMmDt_SetText(this);
    if (resolved && (NvMmMask_Worn() == MM_PLAYER_MASK_KAFEIS_MASK)) {
        this->actor.textId = NV_MM_TEXT(0x2368); // "My wife hired you?"
    }
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        NvMmDt_SetupMeeting(this, play);
        return;
    }
    // talking to Mutoh or Viscen starts it too: their opening line is the meeting's
    if (!resolved && (this->muto != NULL) && (this->baisen != NULL) &&
        ((NvMmMuto_Role(this->muto)->cutsceneState == 1) || (NvMmBaisen_Role(this->baisen)->cutsceneState == 1))) {
        if (play->msgCtx.textId == NV_MM_TEXT(sDtTextIds[9])) {
            this->textIdIndex = 9;
        } else if (play->msgCtx.textId == NV_MM_TEXT(sDtTextIds[11])) {
            this->textIdIndex = 11;
        }
        NvMmDt_SetupMeeting(this, play);
        return;
    }
    Actor_OfferTalk(&this->actor, play, 150.0f);
}

static void NvMmDt_FinishMeeting(NvMmDt* this, PlayState* play);
static void NvMmDt_OfferReward(NvMmDt* this, PlayState* play);

/** MM's EnDt_UpdateMeetingCutscene: the mayor leads the talk line by line. */
static void NvMmDt_UpdateMeeting(NvMmDt* this, PlayState* play) {
    NvMmMeetingRole* muto = ((this->muto != NULL) && (this->baisen != NULL)) ? NvMmMuto_Role(this->muto) : NULL;
    NvMmMeetingRole* baisen = (muto != NULL) ? NvMmBaisen_Role(this->baisen) : NULL;
    s32 index = sStringIdCsIndexTable[this->csIdIndex + 1];

    if (this->csState == DT_CS_WAITING) { // MM's: wait for his shot's turn, then it plays
        if (!NvMmCsMgr_IsNext(this->csIds[index])) {
            NvMmCsMgr_Queue(this->csIds[index]);
            return;
        }
        NvMmCsMgr_StartWithPlayerCs(this->csIds[index], this->targetActor);
        NvMmCs_SetTargetActor(this->targetActor);
        this->csState = DT_CS_PLAYING;
    }
    if ((this->timer != 0) || (Message_GetState(&play->msgCtx) != TEXT_STATE_EVENT) || !Message_ShouldAdvance(play)) {
        return;
    }
    if (this->textIdIndex == 21) { // thanks: the reward
        Player_SetCsActionWithHaltedActors(play, &this->actor, 7); // (PLAYER_CSACTION_END)
        NvMmDt_StopShot(this);
        Message_CloseTextbox(play);
        NvMmDt_OfferReward(this, play);
        return;
    }
    if (this->textIdIndex == 20) { // Mutoh's last word: he and Viscen leave, Link back in through the door
        Message_CloseTextbox(play);
        play->nextEntranceIndex = ENTR_MM_Z2_SONCHONOIE_1;
        play->transitionType = TRANS_TYPE_FADE_BLACK;
        play->transitionTrigger = TRANS_TRIGGER_START;
        NvMm_SetWeekEvent(WE_RESOLVED_MAYOR_MEETING);
        this->meetingFinished = true;
        NvMmDt_StopShot(this);
        if (getenv("NV_LOG") != NULL) {
            printf("NVMM mayor: the meeting's settled\n");
            fflush(stdout);
        }
        return;
    }
    if ((this->textIdIndex == 8) || (this->textIdIndex == 10) || (this->textIdIndex == 22) ||
        (this->textIdIndex == 23)) { // the end of a talk
        NvMm_SetWeekEvent(WE_ATTENDED_MAYOR_MEETING);
        Message_CloseTextbox(play);
        if (!NvMm_CheckWeekEvent(WE_RESOLVED_MAYOR_MEETING) && (muto != NULL)) {
            muto->cutsceneState = 2;
            baisen->cutsceneState = 2;
            muto->targetActor = this->muto;
            baisen->targetActor = this->baisen;
        }
        this->targetActor = &this->actor;
        NvMmDt_StopShot(this);
        if (getenv("NV_LOG") != NULL) {
            printf("NVMM mayor: the meeting's talk ends (line %d)\n", (int)this->textIdIndex);
            fflush(stdout);
        }
        NvMmDt_SetupRegularState(this, play);
        return;
    }
    this->textIdIndex++;
    if (getenv("NV_LOG") != NULL) {
        printf("NVMM mayor: line %d (%04X)\n", (int)this->textIdIndex, sDtTextIds[this->textIdIndex]);
        fflush(stdout);
    }
    if (this->textIdIndex == 8) { // the first meeting's end: he slumps before his last word
        Message_CloseTextbox(play);
        NvMmDt_UpdateFocusTarget(this);
        NvMmCs_SetTargetActor(this->targetActor);
        this->timer = 2;
        this->func = NvMmDt_FinishMeeting;
        return;
    }
    if (this->textIdIndex == 12) { // the mayor sees the Couple's Mask
        if (NvMm_CheckWeekEvent(WE_ATTENDED_MAYOR_MEETING)) {
            NvMmDt_UpdateAppearance(this);
            this->timer = 25;
        } else {
            NvMmDt_ChangeAnim(this, DT_ANIM_UPRIGHT);
        }
    }
    if (this->textIdIndex == 13) {
        NvMmDt_ChangeAnim(this, DT_ANIM_UPRIGHT);
    }
    NvMmDt_UpdateFocusTarget(this);
    NvMmDt_SetText(this);
    Message_ContinueTextbox(play, this->actor.textId);
    NvMmDt_UpdateShot(this, play, index);
    if ((this->textIdIndex == 3) && (muto != NULL)) { // "Isn't that right, Mayor?": Mutoh looks to him
        muto->targetActor = &this->actor;
    }
}

/** MM's EnDt_FinishMeetingCutscene: his look changes, then his line. */
static void NvMmDt_FinishMeeting(NvMmDt* this, PlayState* play) {
    if (this->timer != 0) {
        if (this->timer == 1) {
            if ((this->textIdIndex == 8) || (this->appearancePhase == APPEAR_RESOLUTION_TALK)) {
                NvMmDt_UpdateAppearance(this);
            }
            this->appearancePhase++;
            if (this->appearancePhase >= APPEAR_MAX) {
                this->appearancePhase = APPEAR_RESOLVED_MEETING;
            }
        }
    } else if (this->skelAnime.curFrame >= this->animEndFrame) {
        NvMmDt_UpdateAppearance(this);
        NvMmDt_SetText(this);
        Message_StartTextbox(play, this->actor.textId, &this->actor);
        if (this->textIdIndex == 8) {
            NvMm_NotebookEvent(NOTEBOOK_PERSON_MAYOR_DOTOUR);
        }
        NvMmDt_UpdateFocusTarget(this);
        NvMmDt_UpdateShot(this, play, sStringIdCsIndexTable[this->csIdIndex + 1]);
        this->func = NvMmDt_UpdateMeeting;
    }
}

static void NvMmDt_TriggerRewardEvent(NvMmDt* this, PlayState* play);

static void NvMmDt_OfferReward(NvMmDt* this, PlayState* play) {
    GiveItemEntryFromActor(&this->actor, play, NvMmGetItem_Entry(MM_GI_HEART_PIECE), 300.0f, 300.0f);
    this->state = DT_STATE_OFFERED_REWARD;
    this->func = NvMmDt_TriggerRewardEvent;
}

/** MM's EnDt_TriggerMeetingRewardEvent and EnDt_TriggerMeetingNotebookEvent. */
static void NvMmDt_TriggerRewardEvent(NvMmDt* this, PlayState* play) {
    if (this->actor.parent != NULL) {
        if (Actor_HasParent(&this->actor, play)) {
            this->actor.parent = NULL;
            this->textIdIndex = 22;
            NvMm_SetWeekEvent(WE_RECEIVED_MAYOR_HEART_PIECE);
            this->timer = -1; // (the get-item message first, then his line)
        }
        return;
    }
    if (this->timer == -1) {
        if (play->msgCtx.msgMode == MSGMODE_NONE) {
            this->timer = 0;
            NvMmDt_SetText(this);
            Message_StartTextbox(play, this->actor.textId, &this->actor);
            NvMm_NotebookEvent(NOTEBOOK_EVENT_RECEIVED_MAYOR_HP);
            NvMm_NotebookEvent(NOTEBOOK_PERSON_MAYOR_DOTOUR);
            this->func = NvMmDt_UpdateMeeting;
        }
        return;
    }
    GiveItemEntryFromActor(&this->actor, play, NvMmGetItem_Entry(MM_GI_HEART_PIECE), 300.0f, 300.0f);
}

static void NvMmDt_OfferFinalNightTalk(NvMmDt* this, PlayState* play);

static void NvMmDt_SetupFinalNightState(NvMmDt* this, PlayState* play) {
    NvMmDt_ChangeAnim(this, DT_ANIM_TAP_DESK);
    this->textIdIndex = NvMm_CheckWeekEvent(WE_TALKED_MAYOR_NIGHT_3) ? 26 : 24;
    NvMm_NotebookEvent(NOTEBOOK_PERSON_MAYOR_DOTOUR);
    NvMmDt_SetText(this);
    this->eyeTexIndex = EYE_LOOK_DOWN;
    this->disableBlinking = true;
    this->state = DT_STATE_WAIT_FINAL_NIGHT;
    this->func = NvMmDt_OfferFinalNightTalk;
}

static void NvMmDt_FinalNightTalk(NvMmDt* this, PlayState* play) {
    if ((Message_GetState(&play->msgCtx) == TEXT_STATE_EVENT) && Message_ShouldAdvance(play)) {
        if (!NvMm_CheckWeekEvent(WE_TALKED_MAYOR_NIGHT_3)) {
            this->textIdIndex = 25;
            Message_ContinueTextbox(play, NV_MM_TEXT(sDtTextIds[25]));
            NvMm_SetWeekEvent(WE_TALKED_MAYOR_NIGHT_3);
        } else {
            Message_CloseTextbox(play);
            NvMmDt_SetupFinalNightState(this, play);
        }
    }
}

static void NvMmDt_OfferFinalNightTalk(NvMmDt* this, PlayState* play) {
    NvMmDt_SetText(this);
    if (NvMm_CheckWeekEvent(WE_RESOLVED_MAYOR_MEETING) && (NvMmMask_Worn() == MM_PLAYER_MASK_KAFEIS_MASK)) {
        this->actor.textId = NV_MM_TEXT(0x2368);
    }
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        this->state = DT_STATE_DONE_FINAL_NIGHT;
        this->func = NvMmDt_FinalNightTalk;
        return;
    }
    Actor_OfferTalk(&this->actor, play, 150.0f);
}

void NvMmDt_Update(Actor* thisx, PlayState* play) {
    NvMmDt* this = (NvMmDt*)thisx;

    SkelAnime_Update(&this->skelAnime);
    Actor_SetScale(thisx, 0.01f);
    if ((this->state != DT_STATE_WAIT_FINAL_NIGHT) && (this->state != DT_STATE_DONE_FINAL_NIGHT) &&
        NvMmMayor_FinalNight()) {
        NvMmDt_SetupFinalNightState(this, play);
    }
    DECR(this->blinkTimer);
    if (this->timer > 0) {
        this->timer--;
    }
    if (this->meetingFinished || (this->state >= DT_STATE_WAIT_FINAL_NIGHT)) {
        this->headRotTargetY = NvMmMayor_HeadTarget(thisx, thisx->yawTowardsPlayer);
    }
    if (!this->disableBlinking && (this->blinkTimer == 0)) {
        if (++this->eyeTexIndex >= EYE_LOOK_DOWN) {
            this->eyeTexIndex = EYE_SHOCK;
            this->blinkTimer = (s32)Rand_ZeroFloat(60.0f) + 20;
        }
    }
    this->func(this, play);
    thisx->shape.rot.y = thisx->world.rot.y;
    Math_SmoothStepToS(&this->headRotY, this->headRotTargetY, 1, 3000, 0);
    Actor_SetFocus(thisx, 60.0f);
    Actor_MoveXZGravity(thisx);
    Collider_UpdateCylinder(thisx, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

static s32 NvMmDt_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    if (limbIndex == OBJECT_DT_LIMB_0C) {
        rot->y -= ((NvMmDt*)thisx)->headRotY;
    }
    return false;
}

void NvMmDt_Draw(Actor* thisx, PlayState* play) {
    NvMmDt* this = (NvMmDt*)thisx;
    s32 brow = (this->eyeTexIndex < 3) ? this->eyeTexIndex : 0;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sDtEyes[this->eyeTexIndex]));
    gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(sDtBrows[brow]));
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          NvMmDt_OverrideLimbDraw, NULL, this);
    CLOSE_DISPS(play->state.gfxCtx);
}
