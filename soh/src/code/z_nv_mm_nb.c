/*
 * Nightmareverse: Anju's grandmother, from MM's En_Nb (zeldaret/mm, ovl_En_Nb), ported closely
 * (mm/README.md). MM's model (object_nb: Granny in her wheelchair), MM's schedule
 * (mm/schedules/En_Nb.schl: in her room at the Stock Pot Inn) and MM's message scripts
 * (nv_mm_msgscripts/En_Nb.msg.inc): she offers to tell a story, the Carnival of Time or the Four
 * Giants. Without the All-Night Mask Link falls asleep, as in MM: the short story costs him two
 * hours, the long one the night (he wakes at 8:00 the next day), back at her side (MM's inn entrance
 * 2), where she says he dozed off. With the mask (MM's story pictures and her quizzes, and the two
 * heart pieces) waits for MM's masks: the scripts' mask branches are MM's, checked as not worn.
 *
 * Adapted from MM:
 * - the day of the story and her lunch with Anju (MM's camera cutscene between them, 12:00-12:15 on
 *   Day 1): the lunch scene isn't in yet, she sits as at other times;
 * - MM's story screen (the pictures over a black fill) is for the mask path: not ported yet;
 * - the time jump and the night's sleep are Termina's clock (NvMmClock_Jump, _SleepToMorning);
 * - her head turn and her talking animations by line (MM's transform draw, func_80BC08E0...) are
 *   left out; MM's "auto" talk offer after waking (she speaks first) is OoT's auto-accepted offer.
 */
#include "global.h"
#include "z_nv_mm_schedule.h"
#include "z_nv_mm_msgevent.h"

enum { EN_NB_SCH_NONE, EN_NB_SCH_1, EN_NB_SCH_2, EN_NB_SCH_3, EN_NB_SCH_4 };
#include "nv_mm_schedules/En_Nb.schl.inc"
#include "nv_mm_msgscripts/En_Nb.msg.inc"

#define NB_LIMB_MAX 8
#define NB(name) ("__OTR__mm/objects/object_nb/" name)
static const char sSkel[] = NB("gNbSkel");
static const char sIdleAnim[] = NB("gNbIdleAnim");
#define EVENTINF_42 0x42 // she's told the short story (MM's)
#define EVENTINF_43 0x43 // Link has just woken from her story (MM's)

struct NvMmNb;
typedef void (*NvMmNbFunc)(struct NvMmNb*, PlayState*);

typedef struct NvMmNb {
    Actor actor;
    SkelAnime skelAnime;
    NvMmNbFunc func;
    ColliderCylinder collider;
    u8 scheduleResult;
    MsgScript* msgScript;
    s32 msgScriptPos;
    MsgScriptCallback msgScriptCallback;
    Vec3s jointTable[NB_LIMB_MAX];
    Vec3s morphTable[NB_LIMB_MAX];
    u16 behaviour;
    s16 offer; // 0 none, 1 on screen, 2 auto (she speaks first)
} NvMmNb;

const size_t gNvMmNbSize = sizeof(NvMmNb);

s32 NvMmMsgEvent_EventInf(s32 flag);
void NvMmMsgEvent_SetEventInf(s32 flag, s32 on);
void NvMmClock_Jump(PlayState* play, u32 minutes);
void NvMmClock_SleepToMorning(PlayState* play);

static ColliderCylinderInit sCylinderInit = {
    { COLTYPE_HIT1, AT_NONE, AC_NONE, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK1, { 0x00000000, 0x00, 0x00 }, { 0x00000000, 0x00, 0x00 }, TOUCH_NONE, BUMP_NONE, OCELEM_ON },
    { 10, 68, 0, { 0, 0, 0 } },
};

static void NvMmNb_FollowSchedule(NvMmNb* this, PlayState* play);

/** MM's func_80BC01DC, without the All-Night Mask: Link falls asleep. The short story (EVENTINF_42)
 * costs two hours, the long one the night (8:00 next day); he's taken back to her side (MM's inn
 * entrance 2), where she speaks first. */
static s32 NvMmNb_MsgEventStory(Actor* thisx, PlayState* play) {
    NvMmNb* this = (NvMmNb*)thisx;

    if (this->behaviour == 0) {
        this->behaviour = 5; // (MM: 1 with the mask: the story told, its pictures shown)
    }
    if (this->behaviour == 5) {
        if (!NvMmMsgEvent_EventInf(EVENTINF_42)) {
            NvMmClock_SleepToMorning(play);
        } else {
            NvMmClock_Jump(play, 120);
        }
        this->behaviour++;
        play->nextEntranceIndex = ENTR_MM_Z2_YADOYA_2;
        play->transitionTrigger = TRANS_TRIGGER_START;
        play->transitionType = TRANS_TYPE_FADE_BLACK;
        gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK_SLOW;
        NvMmMsgEvent_SetEventInf(EVENTINF_43, true);
    }
    return false;
}

static void NvMmNb_Talk(NvMmNb* this, PlayState* play) {
    if (NvMmMsgEvent_RunScript(&this->actor, play, this->msgScript, this->msgScriptCallback, &this->msgScriptPos)) {
        if (NvMmMsgEvent_EventInf(EVENTINF_43)) {
            NvMmMsgEvent_SetEventInf(EVENTINF_42, false);
            NvMmMsgEvent_SetEventInf(EVENTINF_43, false);
        }
        this->offer = 1;
        this->msgScriptPos = 0;
        this->func = NvMmNb_FollowSchedule;
    }
}

/** MM's EnNb_ProcessScheduleOutput: in her room (her lunch, SCH_2, as the others for now). */
static s32 NvMmNb_SetupResult(NvMmNb* this, PlayState* play, NvMmScheduleOutput* out) {
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    switch (out->result) {
        case EN_NB_SCH_1:
        case EN_NB_SCH_2:
        case EN_NB_SCH_3:
        case EN_NB_SCH_4:
            this->offer = NvMmMsgEvent_EventInf(EVENTINF_43) ? 2 : 1;
            return true;
        default:
            return false;
    }
}

static void NvMmNb_FollowSchedule(NvMmNb* this, PlayState* play) {
    NvMmScheduleOutput out;

    out.result = EN_NB_SCH_NONE;
    if (NvMmMsgEvent_EventInf(EVENTINF_43)) { // (just woken: she's there, whatever the time)
        out.result = EN_NB_SCH_1;
        NvMmNb_SetupResult(this, play, &out);
        this->actor.shape.shadowDraw = ActorShadow_DrawCircle;
    } else if (!NvMmSchedule_RunScript(play, sScheduleScript, &out) ||
               ((this->scheduleResult != out.result) && !NvMmNb_SetupResult(this, play, &out))) {
        this->actor.shape.shadowDraw = NULL;
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        out.result = EN_NB_SCH_NONE;
    } else {
        this->actor.shape.shadowDraw = ActorShadow_DrawCircle;
    }
    this->scheduleResult = out.result;
    Math_ApproachS(&this->actor.shape.rot.y, this->actor.world.rot.y, 3, 0x2AA8);
}

void NvMmNb_Init(Actor* thisx, PlayState* play) {
    NvMmNb* this = (NvMmNb*)thisx;
    AnimationHeader* anim = (AnimationHeader*)sIdleAnim;

    ActorShape_Init(&this->actor.shape, 0.0f, NULL, 0.0f);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sSkel, NULL, this->jointTable, this->morphTable,
                       NB_LIMB_MAX);
    Animation_Change(&this->skelAnime, anim, 1.0f, 0.0f, Animation_GetLastFrame(anim), ANIMMODE_LOOP, 0.0f);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    Actor_SetScale(&this->actor, 0.01f);
    if (!NvMmMsgEvent_EventInf(EVENTINF_43)) {
        NvMmMsgEvent_SetEventInf(EVENTINF_42, false);
        NvMmMsgEvent_SetEventInf(EVENTINF_43, false);
    }
    this->func = NvMmNb_FollowSchedule;
    this->func(this, play);
}

void NvMmNb_Destroy(Actor* thisx, PlayState* play) {
    NvMmNb* this = (NvMmNb*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

void NvMmNb_Update(Actor* thisx, PlayState* play) {
    NvMmNb* this = (NvMmNb*)thisx;

    if ((this->func == NvMmNb_FollowSchedule) && (this->offer != 0) && Actor_ProcessTalkRequest(&this->actor, play)) {
        this->offer = 0;
        this->behaviour = 0;
        this->actor.flags &= ~ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
        this->msgScriptCallback = NvMmNb_MsgEventStory; // (MM's EnNb_GetMsgScript: the Kafei's Mask line waits)
        this->msgScript = D_80BC1464;
        this->msgScriptPos = 0;
        this->func = NvMmNb_Talk;
    }
    this->func(this, play);
    if (this->scheduleResult == EN_NB_SCH_NONE) {
        return;
    }
    SkelAnime_Update(&this->skelAnime);
    if ((this->func == NvMmNb_FollowSchedule) && (this->offer != 0) && Actor_IsFacingPlayer(&this->actor, 0x38E0)) {
        if (this->offer == 2) { // (MM's SubS_Offer, auto: she speaks first)
            this->actor.flags |= ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
            Actor_OfferTalkExchange(&this->actor, play, this->actor.xzDistToPlayer + 1.0f,
                                    fabsf(this->actor.yDistToPlayer) + 1.0f, EXCH_ITEM_NONE);
        } else {
            Actor_OfferTalkExchange(&this->actor, play, 40.0f, 30.0f, EXCH_ITEM_NONE);
        }
    }
    Actor_SetFocus(&this->actor, 40.0f);
    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

void NvMmNb_Draw(Actor* thisx, PlayState* play) {
    NvMmNb* this = (NvMmNb*)thisx;

    if (this->scheduleResult == EN_NB_SCH_NONE) {
        return;
    }
    Gfx_SetupDL_37Opa(play->state.gfxCtx);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          NULL, NULL, &this->actor);
}
