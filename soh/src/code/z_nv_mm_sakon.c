/*
 * Nightmareverse: the Bomb Shop lady and Sakon on the first night, MM's own, ported closely (mm/README.md
 * "The Bomb Shop lady and Sakon").
 *
 *   En_Baba (0x236)     the Bomb Shop lady (object_bba). In North Clock Town on the first night she walks
 *                       home with the shop's new Big Bomb Bags (MM's time path, 0:00-0:30); at 0:30 Sakon
 *                       knocks her over. Afterwards (North Clock Town's spawn 7) she thanks Link: the bag
 *                       got back, MM's Blast Mask; or her lines if it wasn't. In the Bomb Shop, her lines
 *                       with her son by what's happened.
 *   En_Suttari (0x237)  Sakon (object_boj, his own head). In North Clock Town on the first night: waiting,
 *                       then walking up behind her (MM's time path, 0:20-0:30); at 0:30 he snatches the bag
 *                       and runs (MM's chase); hit with the sword (or a Goron's or Zora's punch, a spin) he
 *                       drops it; an arrow, the boomerang, the hookshot or an explosion finishes him (week
 *                       event 79_40, MM's "Sakon's dead"). At his path's end the night goes on at spawn 7
 *                       (the bag got back: week event 33_08).
 * Adapted from MM: MM's cutscene cameras are left out (the snatch's two lines play as plain text); the
 * chase music (MM's NA_BGM_CHASE) and their own voices aren't in OoT's banks; the dropped bag (MM's
 * En_Nimotsu, falling) isn't drawn, it's simply gone from his hands. Sakon elsewhere (the Curiosity
 * Shop, West Clock Town with the Sun's Mask, Ikana) waits for Kafei's story.
 */
#include "global.h"
#include <stdio.h>
#include <stdlib.h>
#include "z_nv_mm_schedule.h"
#include "z_nv_mm_subs.h"

#define BBA(name) ("__OTR__mm/objects/object_bba/" name)
#define BOJ(name) ("__OTR__mm/objects/object_boj/" name)
#define NV_MM_TEXT(id) (0xB000 + (id))
#define MM_WE(byte, bit) (((byte) << 8) | (bit))

s32 NvMm_CheckWeekEvent(s32 flag);
void NvMm_SetWeekEvent(s32 flag);
void NvMm_NotebookEvent(u16 event);
s32 NvMmForm_Get(void); // 0 FD, 1 Goron, 2 Zora, 3 Deku, 4 human
s32 NvMmItem_Has(s32 item);
GetItemEntry NvMmGetItem_Entry(s32 mmGi);
s32 NvMmClock_Speed(void);
u16 NvMmClock_Time(void);
s32 NvMmClock_Day(void);
s32 NvMmActor_Id(s32 mmId);
void NvMmSubS_TimePathing_FillKnots(f32 knots[], s32 order, s32 numPoints);
s32 NvMmSubS_TimePathing_Update(Path* path, f32* progress, s32* elapsedTime, s32 waypointTime, s32 totalTime,
                                s32* waypoint, f32 knots[], Vec3f* targetPos, s32 timeSpeed);
void NvMmSubS_TimePathing_ComputeInitialY(PlayState* play, Path* path, s32 waypoint, Vec3f* targetPos);
Path* NvMmSubS_GetAdditionalPath(PlayState* play, u8 pathIndex, s32 limit);
u8 NvMmSubS_AdditionalPathIndex(PlayState* play, s32 pathIndex);

#include "nv_mm_schedules/En_Baba.schl.inc"
#include "nv_mm_schedules/En_Suttari.schl.inc"

#define WE_RECOVERED_STOLEN_BOMB_BAG MM_WE(33, 0x08)
#define WE_SAKON_DEAD MM_WE(79, 0x40)
#define WE_58_40 MM_WE(58, 0x40) // the lady set off home
#define WE_58_80 MM_WE(58, 0x80)
#define WE_73_01 MM_WE(73, 0x01)
#define WE_73_02 MM_WE(73, 0x02)
#define WE_79_20 MM_WE(79, 0x20)
#define WE_81_02 MM_WE(81, 0x02) // her thanks heard (the night goes on)
#define NOTEBOOK_MET_BOMB_SHOP_LADY 0x04
#define NOTEBOOK_RECEIVED_BLAST_MASK 0x33
#define MM_GI_MASK_BLAST 0x8D
#define MM_ITEM_MASK_BLAST 0x47

static s32 NvMmSakon_After(u8 h, u8 m) { // the time is at or past h:m tonight (before the dawn)
    u16 now = NvMmClock_Time();
    u16 t = (u16)((h * 60 + m) * 0x10000 / (24 * 60));

    return (now >= t) && (now < 0x4000);
}

static void NvMmSakon_ToSpawn7(PlayState* play) { // MM's ENTRANCE(NORTH_CLOCK_TOWN, 7)
    play->nextEntranceIndex = ENTR_MM_Z2_BACKTOWN_7;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
}

typedef struct {
    Path* path;
    f32 progress;
    s32 totalTime, waypointTime, waypoint, elapsedTime;
    s32 set;
    s32 ended;
    Vec3f target;
} NvMmTimePath;

/** MM's time path set up from a schedule's two times (EnBaba_InitTimePath, func_80BABC48). */
static s32 NvMmTimePath_Init(NvMmTimePath* tp, PlayState* play, u8 pathIndex, NvMmScheduleOutput* out,
                             s32 continuing) {
    u16 now = NvMmSchedule_Now();
    u16 start;
    s32 n;

    tp->path = NvMmSubS_GetAdditionalPath(play, pathIndex, 0);
    if ((tp->path == NULL) || (tp->path->count < 3)) {
        return false;
    }
    start = continuing ? now : out->time0;
    tp->totalTime = (out->time1 < start) ? ((start - out->time1) + 0xFFFF) : (out->time1 - start);
    tp->elapsedTime = now - start;
    n = tp->path->count - (NV_MM_TIME_PATHING_ORDER - 1);
    tp->waypointTime = MAX(tp->totalTime / n, 1);
    tp->waypoint = (tp->elapsedTime / tp->waypointTime) + (NV_MM_TIME_PATHING_ORDER - 1);
    tp->set = false;
    tp->ended = false;
    return true;
}

/** MM's EnBaba_FollowTimePath (func_80BABFD4): along the path by the time. */
static void NvMmTimePath_Follow(NvMmTimePath* tp, Actor* actor, PlayState* play) {
    static f32 knots[265];
    Vec3f target;
    s32 speed = NvMmClock_Speed();

    NvMmSubS_TimePathing_FillKnots(knots, NV_MM_TIME_PATHING_ORDER, tp->path->count + NV_MM_TIME_PATHING_ORDER);
    if (!tp->set) {
        target = (Vec3f){ 0.0f, 0.0f, 0.0f };
        NvMmSubS_TimePathing_Update(tp->path, &tp->progress, &tp->elapsedTime, tp->waypointTime, tp->totalTime,
                                    &tp->waypoint, knots, &target, speed);
        NvMmSubS_TimePathing_ComputeInitialY(play, tp->path, tp->waypoint, &target);
        actor->world.pos.y = target.y;
        tp->set = true;
    } else {
        target = tp->target;
    }
    actor->world.pos.x = target.x;
    actor->world.pos.z = target.z;
    tp->target = (Vec3f){ 0.0f, 0.0f, 0.0f };
    if (NvMmSubS_TimePathing_Update(tp->path, &tp->progress, &tp->elapsedTime, tp->waypointTime, tp->totalTime,
                                    &tp->waypoint, knots, &tp->target, speed)) {
        tp->ended = true;
    } else {
        actor->world.rot.y = Math_Vec3f_Yaw(&actor->world.pos, &tp->target);
    }
}

/* ============================== En_Baba: the Bomb Shop lady ============================== */

enum { BABA_ANIM_IDLE_HOLDING_BAG, BABA_ANIM_IDLE, BABA_ANIM_WALKING_HOLDING_BAG, BABA_ANIM_KNOCKED_OVER,
       BABA_ANIM_LYING_DOWN, BABA_ANIM_SWAY, BABA_ANIM_MAX };
static const char* sBabaAnims[BABA_ANIM_MAX] = {
    BBA("gBombShopLadyIdleHoldingBagAnim"), BBA("gBombShopLadyIdleAnim"), BBA("gBombShopLadyWalkingHoldingBagAnim"),
    BBA("gBombShopLadyKnockedOverAnim"),    BBA("gBombShopLadyLyingDownAnim"), BBA("gBombShopLadySwayAnim"),
};
static const u8 sBabaModes[BABA_ANIM_MAX] = { ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_ONCE,
                                              ANIMMODE_LOOP, ANIMMODE_LOOP };
#define BABA_LIMB_UPPER_ROOT 5
#define BABA_LIMB_NECK 6
#define BABA_LIMB_HEAD 7
#define BABA_LIMB_BAG 8
#define BABA_LIMB_MAX 18

#define BABA_END_CONVERSATION (1 << 0)
#define BABA_VISIBLE (1 << 1)
#define BABA_KNOCKED_OVER (1 << 2)
#define BABA_AUTOTALK (1 << 3)
#define BABA_GIVE_BLAST_MASK (1 << 5)
#define BABA_GAVE_BLAST_MASK (1 << 6)

struct NvMmBaba;
typedef void (*NvMmBabaFunc)(struct NvMmBaba*, PlayState*);

typedef struct NvMmBaba {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[BABA_LIMB_MAX];
    Vec3s morphTable[BABA_LIMB_MAX];
    NvMmBabaFunc func;
    Vec3s headRot, torsoRot;
    s32 animIndex;
    u16 textId;
    s32 flags;
    s16 timer;
    u8 scheduleResult;
    NvMmTimePath tp;
    ColliderCylinder collider;
} NvMmBaba;

const size_t gNvMmBabaSize = sizeof(NvMmBaba);

static ColliderCylinderInit sBabaCylinderInit = {
    { COLTYPE_NONE, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK1, { 0x00000000, 0x00, 0x00 }, { 0xF7CFFFFF, 0x00, 0x00 }, TOUCH_NONE | TOUCH_SFX_NORMAL, BUMP_ON,
      OCELEM_ON },
    { 18, 64, 0, { 0, 0, 0 } },
};

#define BABA_TYPE(thisx) (((thisx)->params & 0xC000) >> 14)
#define BABA_PATH(thisx) (((thisx)->params & 0x3F00) >> 8)

static void NvMmBaba_Idle(NvMmBaba* this, PlayState* play);
static void NvMmBaba_Talk(NvMmBaba* this, PlayState* play);
static void NvMmBaba_FollowSchedule(NvMmBaba* this, PlayState* play);
static void NvMmBaba_KnockedOver(NvMmBaba* this, PlayState* play);
static void NvMmBaba_DoNothing(NvMmBaba* this, PlayState* play) {
}

static void NvMmBaba_Anim(NvMmBaba* this, s32 i) {
    AnimationHeader* a = (AnimationHeader*)sBabaAnims[i];

    this->animIndex = i;
    Animation_Change(&this->skelAnime, a, 1.0f, 0.0f, Animation_GetLastFrame(a), sBabaModes[i], 0.0f);
}

/** MM's EnBaba_HandleConversation: her next line. */
static void NvMmBaba_Converse(NvMmBaba* this, PlayState* play) {
    switch (this->textId) {
        case 0:
            if (this->flags & BABA_AUTOTALK) { // afterwards, North Clock Town
                if (NvMm_CheckWeekEvent(WE_RECOVERED_STOLEN_BOMB_BAG)) {
                    this->textId = 0x2A34; // thanks; Big Bomb Bags tomorrow
                } else if (NvMm_CheckWeekEvent(WE_SAKON_DEAD)) {
                    this->flags |= BABA_END_CONVERSATION;
                    this->textId = 0x2A33;
                } else {
                    this->flags |= BABA_END_CONVERSATION;
                    this->textId = 0x2A32;
                }
                break;
            }
            if (NvMmForm_Get() == 3) { // Deku Link
                this->flags |= BABA_END_CONVERSATION;
                this->textId = NvMm_CheckWeekEvent(WE_79_20) ? 0x2A38 : 0x2A37;
                NvMm_SetWeekEvent(WE_79_20);
                break;
            }
            if (!NvMm_CheckWeekEvent(WE_RECOVERED_STOLEN_BOMB_BAG)) {
                this->textId = NvMm_CheckWeekEvent(WE_73_01) ? 0x662 : 0x660;
            } else {
                this->textId = NvMm_CheckWeekEvent(WE_73_02) ? 0x65E : 0x65A;
            }
            break;
        case 0x660:
        case 0x661:
        case 0x65A:
        case 0x65B:
            this->textId++;
            break;
        case 0x662:
            this->textId = 0x663;
            NvMm_SetWeekEvent(WE_73_01);
            this->flags |= BABA_END_CONVERSATION;
            break;
        case 0x65C:
            this->textId = 0x65D;
            NvMm_SetWeekEvent(WE_73_02);
            this->flags |= BABA_END_CONVERSATION;
            break;
        case 0x65E:
            this->textId = 0x65F;
            this->flags |= BABA_END_CONVERSATION;
            break;
        case 0x2A34:
            if (NvMmItem_Has(MM_ITEM_MASK_BLAST)) {
                this->flags |= BABA_END_CONVERSATION;
                this->textId = 0x2A36;
            } else {
                this->flags |= BABA_GIVE_BLAST_MASK;
                this->textId = 0x2A35; // a dangerous mask...
            }
            break;
        case 0x2A35:
            this->flags |= BABA_END_CONVERSATION;
            this->textId = 0x2A36;
            break;
        default:
            this->flags |= BABA_END_CONVERSATION;
            break;
    }
    Message_StartTextbox(play, NV_MM_TEXT(this->textId), &this->actor);
    if (this->flags & BABA_END_CONVERSATION) {
        if (this->flags & BABA_GAVE_BLAST_MASK) {
            this->flags &= ~BABA_GAVE_BLAST_MASK;
            NvMm_NotebookEvent(NOTEBOOK_RECEIVED_BLAST_MASK);
        }
        NvMm_NotebookEvent(NOTEBOOK_MET_BOMB_SHOP_LADY);
    }
}

void NvMmBaba_Init(Actor* thisx, PlayState* play) {
    NvMmBaba* this = (NvMmBaba*)thisx;

    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sBabaCylinderInit);
    thisx->colChkInfo.mass = MASS_IMMOVABLE;
    Actor_SetScale(thisx, 0.01f);
    thisx->targetMode = 0;
    thisx->gravity = -4.0f;
    ActorShape_Init(&thisx->shape, 0.0f, ActorShadow_DrawCircle, 20.0f);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)BBA("gBombShopLadySkel"),
                       (AnimationHeader*)sBabaAnims[BABA_ANIM_WALKING_HOLDING_BAG], this->jointTable, this->morphTable,
                       BABA_LIMB_MAX);
    if (play->sceneNum == SCENE_MM_Z2_BACKTOWN) {
        if ((BABA_TYPE(thisx) == 0) && (gSaveContext.entranceIndex != ENTR_MM_Z2_BACKTOWN_7) &&
            (BABA_PATH(thisx) != 0x3F)) { // walking home tonight
            if (NvMm_CheckWeekEvent(WE_58_40) || NvMmSakon_After(0, 20)) {
                Actor_Kill(thisx);
                return;
            }
            this->timer = 50;
            NvMmBaba_Anim(this, BABA_ANIM_WALKING_HOLDING_BAG);
            this->func = NvMmBaba_FollowSchedule;
        } else if ((BABA_TYPE(thisx) == 1) && (gSaveContext.entranceIndex == ENTR_MM_Z2_BACKTOWN_7)) { // afterwards
            if (NvMm_CheckWeekEvent(WE_81_02)) {
                Actor_Kill(thisx);
                return;
            }
            this->flags |= BABA_VISIBLE | BABA_AUTOTALK;
            NvMmBaba_Anim(this, NvMm_CheckWeekEvent(WE_RECOVERED_STOLEN_BOMB_BAG) ? BABA_ANIM_IDLE_HOLDING_BAG
                                                                                   : BABA_ANIM_IDLE);
            this->func = NvMmBaba_Idle;
        } else {
            Actor_Kill(thisx);
        }
        return;
    }
    this->flags |= BABA_VISIBLE; // (the Bomb Shop; elsewhere)
    if (BABA_TYPE(thisx) == 2) {
        thisx->flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        NvMmBaba_Anim(this, BABA_ANIM_SWAY);
        this->func = NvMmBaba_DoNothing;
    } else {
        NvMmBaba_Anim(this, BABA_ANIM_IDLE);
        this->func = NvMmBaba_Idle;
    }
}

void NvMmBaba_Destroy(Actor* thisx, PlayState* play) {
    Collider_DestroyCylinder(play, &((NvMmBaba*)thisx)->collider);
}

/** MM's EnBaba_Idle: in the shop, or waiting to thank Link (she speaks first). */
static void NvMmBaba_Idle(NvMmBaba* this, PlayState* play) {
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        this->textId = 0;
        this->actor.flags &= ~ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
        NvMmBaba_Converse(this, play);
        this->func = NvMmBaba_Talk;
    } else if ((this->actor.xzDistToPlayer < 100.0f) || (this->flags & BABA_AUTOTALK)) {
        if (this->flags & BABA_AUTOTALK) {
            this->actor.flags |= ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
        }
        Actor_OfferTalk(&this->actor, play, (this->flags & BABA_AUTOTALK) ? 1000.0f : 100.0f);
    }
}

static void NvMmBaba_GiveBlastMask(NvMmBaba* this, PlayState* play) {
    if (Actor_HasParent(&this->actor, play)) {
        this->actor.parent = NULL;
        this->flags |= BABA_GAVE_BLAST_MASK;
        this->func = NvMmBaba_Talk;
        this->textId = 0x2A35;
        this->timer = -1; // (her next line once the mask's message is gone)
    } else {
        GiveItemEntryFromActor(&this->actor, play, NvMmGetItem_Entry(MM_GI_MASK_BLAST), 300.0f, 300.0f);
    }
}

/** MM's EnBaba_Talk. */
static void NvMmBaba_Talk(NvMmBaba* this, PlayState* play) {
    u8 state = Message_GetState(&play->msgCtx);

    if (this->timer == -1) { // after the Blast Mask: her thanks (MM's talk exchange: she speaks on)
        if (play->msgCtx.msgMode == MSGMODE_NONE) {
            this->timer = 0;
            NvMmBaba_Converse(this, play);
        }
        return;
    }
    if (((state == TEXT_STATE_EVENT) || (state == TEXT_STATE_DONE)) && Message_ShouldAdvance(play)) {
        if (this->flags & BABA_END_CONVERSATION) {
            this->flags &= ~BABA_END_CONVERSATION;
            Message_CloseTextbox(play);
            if (this->flags & BABA_AUTOTALK) { // the night goes on (MM: North Clock Town, spawn 7 again)
                NvMm_SetWeekEvent(WE_81_02);
                NvMmSakon_ToSpawn7(play);
                this->func = NvMmBaba_DoNothing;
            } else {
                this->textId = 0;
                this->func = NvMmBaba_Idle;
            }
        } else if (this->flags & BABA_GIVE_BLAST_MASK) {
            this->flags &= ~BABA_GIVE_BLAST_MASK;
            Message_CloseTextbox(play);
            this->func = NvMmBaba_GiveBlastMask;
        } else {
            NvMmBaba_Converse(this, play);
        }
    }
}

/** MM's EnBaba_FollowSchedule: walking home (0:00-0:30), then knocked over. */
static void NvMmBaba_FollowSchedule(NvMmBaba* this, PlayState* play) {
    NvMmScheduleOutput out;

    if (!NvMmSchedule_RunScript(play, sSchedule, &out) ||
        ((this->scheduleResult != out.result) &&
         !((out.result == 1) ||
           ((out.result == 2) &&
            NvMmTimePath_Init(&this->tp, play, BABA_PATH(&this->actor), &out, this->scheduleResult != 0))))) {
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        out.result = 0;
    } else {
        this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    }
    this->scheduleResult = out.result;
    if (out.result == 2) {
        NvMm_SetWeekEvent(WE_58_40);
        this->flags |= BABA_VISIBLE;
        NvMmTimePath_Follow(&this->tp, &this->actor, play);
    } else if (out.result == 1) { // knocked over
        this->textId = 0x2A30;
        this->actor.speedXZ = 0.0f;
        this->flags |= BABA_KNOCKED_OVER;
        NvMmBaba_Anim(this, BABA_ANIM_KNOCKED_OVER);
        this->func = NvMmBaba_KnockedOver;
        return;
    }
    Math_ApproachS(&this->actor.shape.rot.y, this->actor.world.rot.y, 4, 0x1554);
    if (this->flags & BABA_VISIBLE) {
        if (Actor_ProcessTalkRequest(&this->actor, play)) {
            Message_StartTextbox(play, NV_MM_TEXT(0x2A39), &this->actor); // "I'm sorry"
            this->textId = 0x2A39;
            this->func = NvMmBaba_Talk;
        } else if (this->actor.xzDistToPlayer < 100.0f) {
            Actor_OfferTalk(&this->actor, play, 100.0f);
        }
    }
    Actor_MoveXZGravity(&this->actor);
}

static void NvMmBaba_KnockedOver(NvMmBaba* this, PlayState* play) {
    this->collider.dim.height = 37;
    this->collider.dim.radius = 23;
    if (this->animIndex == BABA_ANIM_KNOCKED_OVER) {
        if (this->skelAnime.curFrame >= Animation_GetLastFrame((AnimationHeader*)sBabaAnims[BABA_ANIM_KNOCKED_OVER])) {
            NvMmBaba_Anim(this, BABA_ANIM_LYING_DOWN);
        }
    } else if (NvMm_CheckWeekEvent(WE_SAKON_DEAD) && (DECR(this->timer) == 0)) {
        NvMmSakon_ToSpawn7(play);
    }
    Actor_MoveXZGravity(&this->actor);
}

void NvMmBaba_Update(Actor* thisx, PlayState* play) {
    NvMmBaba* this = (NvMmBaba*)thisx;
    Player* player = GET_PLAYER(play);

    this->func(this, play);
    Actor_UpdateBgCheckInfo(play, thisx, 0.0f, 0.0f, 0.0f, 4);
    SkelAnime_Update(&this->skelAnime);
    if (!(this->flags & BABA_KNOCKED_OVER)) {
        Actor_TrackPlayer(play, thisx, &this->headRot, &this->torsoRot, thisx->focus.pos);
    }
    if (this->flags & BABA_VISIBLE) {
        this->collider.dim.pos.x = thisx->world.pos.x + 5.0f;
        this->collider.dim.pos.y = thisx->world.pos.y;
        this->collider.dim.pos.z = thisx->world.pos.z + 22.0f;
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    }
}

static s32 NvMmBaba_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                     void* thisx) {
    NvMmBaba* this = (NvMmBaba*)thisx;

    if (limbIndex == BABA_LIMB_NECK) {
        Matrix_Translate(1500.0f, 0.0f, 0.0f, MTXMODE_APPLY);
        Matrix_RotateX(BINANG_TO_RAD(this->headRot.y), MTXMODE_APPLY);
        Matrix_RotateZ(BINANG_TO_RAD(-this->headRot.x), MTXMODE_APPLY);
        Matrix_Translate(-1500.0f, 0.0f, 0.0f, MTXMODE_APPLY);
    }
    if (limbIndex == BABA_LIMB_UPPER_ROOT) {
        Matrix_RotateX(BINANG_TO_RAD(-this->torsoRot.y), MTXMODE_APPLY);
        Matrix_RotateZ(BINANG_TO_RAD(-this->torsoRot.x), MTXMODE_APPLY);
    }
    if ((limbIndex == BABA_LIMB_BAG) &&
        ((this->animIndex == BABA_ANIM_IDLE) || (this->animIndex == BABA_ANIM_KNOCKED_OVER) ||
         (this->animIndex == BABA_ANIM_LYING_DOWN) || (BABA_TYPE(&this->actor) == 2))) {
        *dList = NULL; // (the bag gone)
    }
    return false;
}

static void NvMmBaba_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    Vec3f zero = { 0.0f, 0.0f, 0.0f };

    if (limbIndex == BABA_LIMB_HEAD) {
        Matrix_MultVec3f(&zero, &((Actor*)thisx)->focus.pos);
    }
}

void NvMmBaba_Draw(Actor* thisx, PlayState* play) {
    NvMmBaba* this = (NvMmBaba*)thisx;

    if (!(this->flags & BABA_VISIBLE)) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_37Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(BBA("gBombShopLadyEyeTex")));
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          NvMmBaba_OverrideLimbDraw, NvMmBaba_PostLimbDraw, this);
    CLOSE_DISPS(play->state.gfxCtx);
}

/* ============================== En_Suttari: Sakon ============================== */

enum { SAKON_ANIM_0, SAKON_ANIM_IDLE, SAKON_ANIM_RUN, SAKON_ANIM_HIT, SAKON_ANIM_4, SAKON_ANIM_RUN_BAG,
       SAKON_ANIM_RUN_FAST, SAKON_ANIM_MAX };
static const char* sSakonAnims[SAKON_ANIM_MAX] = {
    BOJ("object_boj_Anim_00071C"), BOJ("object_boj_Anim_0128F4"), BOJ("object_boj_Anim_011F84"),
    BOJ("object_boj_Anim_012E84"), BOJ("object_boj_Anim_01139C"), BOJ("object_boj_Anim_00071C"),
    BOJ("object_boj_Anim_011F84"),
};
static const f32 sSakonSpeeds[SAKON_ANIM_MAX] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 2.0f, 2.0f };
#define OBJECT_BOJ_LIMB_08 8
#define OBJECT_BOJ_LIMB_0F 15
#define OBJECT_BOJ_LIMB_MAX 17

#define SAKON_HAS_BAG (1 << 0)
#define SAKON_HITTABLE (1 << 1)
#define SAKON_DROPPED (1 << 2)
#define SAKON_SNATCHED (1 << 3)

struct NvMmSuttari;
typedef void (*NvMmSuttariFunc)(struct NvMmSuttari*, PlayState*);

typedef struct NvMmSuttari {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[OBJECT_BOJ_LIMB_MAX];
    Vec3s morphTable[OBJECT_BOJ_LIMB_MAX];
    NvMmSuttariFunc func;
    Vec3s headRot, torsoRot;
    s32 animIndex;
    s32 flags;
    u16 textId;
    u8 scheduleResult;
    Path* escapePath;
    s16 escapePoint;
    NvMmTimePath tp;
    ColliderCylinder collider;
} NvMmSuttari;

const size_t gNvMmSuttariSize = sizeof(NvMmSuttari);

static ColliderCylinderInit sSakonCylinderInit = {
    { COLTYPE_HIT0, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK1, { 0x00000000, 0x00, 0x00 }, { 0xF7CFFFFF, 0x00, 0x00 }, TOUCH_NONE | TOUCH_SFX_NORMAL, BUMP_ON,
      OCELEM_ON },
    { 14, 62, 0, { 0, 0, 0 } },
};

#define SAKON_PATH(thisx) (((thisx)->params & 0x7E00) >> 9)

static void NvMmSuttari_Schedule(NvMmSuttari* this, PlayState* play);
static void NvMmSuttari_Snatch(NvMmSuttari* this, PlayState* play);
static void NvMmSuttari_Chase(NvMmSuttari* this, PlayState* play);
static void NvMmSuttari_TalkWait(NvMmSuttari* this, PlayState* play);

static void NvMmSuttari_Anim(NvMmSuttari* this, s32 i) {
    AnimationHeader* a = (AnimationHeader*)sSakonAnims[i];

    this->animIndex = i;
    Animation_Change(&this->skelAnime, a, sSakonSpeeds[i], 0.0f, Animation_GetLastFrame(a),
                     (i == SAKON_ANIM_HIT) ? ANIMMODE_ONCE : ANIMMODE_LOOP, (i == SAKON_ANIM_HIT) ? -6.0f : 0.0f);
}

static s32 NvMmSuttari_AnimDone(NvMmSuttari* this) {
    return this->skelAnime.curFrame >= Animation_GetLastFrame((AnimationHeader*)sSakonAnims[this->animIndex]);
}

void NvMmSuttari_Init(Actor* thisx, PlayState* play) {
    NvMmSuttari* this = (NvMmSuttari*)thisx;
    s32 extra;

    if (NvMm_CheckWeekEvent(WE_SAKON_DEAD) || (play->sceneNum != SCENE_MM_Z2_BACKTOWN) ||
        NvMmSakon_After(0, 20) || (gSaveContext.entranceIndex == ENTR_MM_Z2_BACKTOWN_7) ||
        NvMm_CheckWeekEvent(WE_58_40)) {
        Actor_Kill(thisx);
        return;
    }
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sSakonCylinderInit);
    thisx->colChkInfo.mass = MASS_IMMOVABLE;
    Actor_SetScale(thisx, 0.01f);
    thisx->gravity = -4.0f;
    ActorShape_Init(&thisx->shape, 0.0f, ActorShadow_DrawCircle, 20.0f);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)BOJ("object_boj_Skel_00C240"),
                       (AnimationHeader*)sSakonAnims[SAKON_ANIM_IDLE], this->jointTable, this->morphTable,
                       OBJECT_BOJ_LIMB_MAX);
    // his escape: the path after his own (MM's paths[1], the additional path)
    extra = NvMmSubS_AdditionalPathIndex(play, SAKON_PATH(thisx));
    this->escapePath = (extra != 0xFF) ? Path_GetByIndex(play, extra, 0x3F) : NULL;
    NvMmSuttari_Anim(this, SAKON_ANIM_IDLE);
    this->func = NvMmSuttari_Schedule;
}

void NvMmSuttari_Destroy(Actor* thisx, PlayState* play) {
    Collider_DestroyCylinder(play, &((NvMmSuttari*)thisx)->collider);
}

/** MM's func_80BAD004 with func_80BAC2FC: North Clock Town, his schedule. */
static void NvMmSuttari_Schedule(NvMmSuttari* this, PlayState* play) {
    NvMmScheduleOutput out;
    Player* player = GET_PLAYER(play);

    if (!NvMmSchedule_RunScript(play, D_80BAE820, &out) ||
        ((this->scheduleResult != out.result) &&
         !((out.result == 3) || (out.result == 4) ||
           ((out.result == 15) &&
            NvMmTimePath_Init(&this->tp, play, SAKON_PATH(&this->actor), &out, this->scheduleResult != 0))))) {
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        out.result = 0;
    } else {
        this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    }
    this->scheduleResult = out.result;
    switch (out.result) {
        case 15: // after her
            if ((this->animIndex == SAKON_ANIM_IDLE) && NvMmSuttari_AnimDone(this)) {
                NvMmSuttari_Anim(this, SAKON_ANIM_RUN);
            }
            NvMmTimePath_Follow(&this->tp, &this->actor, play);
            break;
        case 4: // the snatch (MM: its cutscene; here her "Ouch!", then his line)
            if (!NvMm_CheckWeekEvent(WE_RECOVERED_STOLEN_BOMB_BAG)) {
                NvMmSuttari_Anim(this, SAKON_ANIM_RUN_BAG);
                this->flags |= SAKON_HAS_BAG;
                this->textId = 0x2A30;
                Message_StartTextbox(play, NV_MM_TEXT(this->textId), &this->actor);
                player->actor.freezeTimer = 3;
                this->func = NvMmSuttari_Snatch;
                if (getenv("NV_LOG") != NULL) {
                    printf("NVMM Sakon: the snatch at %d,%d (escape path %s)\n", (int)this->actor.world.pos.x,
                           (int)this->actor.world.pos.z, (this->escapePath != NULL) ? "found" : "none");
                    fflush(stdout);
                }
                return;
            }
            break;
    }
    Math_ApproachS(&this->actor.shape.rot.y, this->actor.world.rot.y, 4, 0x1554);
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        Message_StartTextbox(play, NV_MM_TEXT(0x2A3A), &this->actor);
        this->func = NvMmSuttari_TalkWait;
    } else if (this->actor.xzDistToPlayer < 200.0f) {
        Actor_OfferTalk(&this->actor, play, 200.0f);
    }
    Actor_MoveXZGravity(&this->actor);
}

static void NvMmSuttari_TalkWait(NvMmSuttari* this, PlayState* play) {
    u8 state = Message_GetState(&play->msgCtx);

    if (((state == TEXT_STATE_EVENT) || (state == TEXT_STATE_DONE)) && Message_ShouldAdvance(play)) {
        Message_CloseTextbox(play);
        this->func = NvMmSuttari_Schedule;
    }
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 5, 0x1000, 0x100);
}

/** MM's func_80BAD2B4, func_80BAD230: her line, then his; then the chase. */
static void NvMmSuttari_Snatch(NvMmSuttari* this, PlayState* play) {
    GET_PLAYER(play)->actor.freezeTimer = 3;
    if ((Message_GetState(&play->msgCtx) == TEXT_STATE_EVENT) && Message_ShouldAdvance(play)) {
        if (this->textId == 0x2A30) {
            this->textId = 0x2A31;
            Message_ContinueTextbox(play, NV_MM_TEXT(this->textId));
        } else {
            Message_CloseTextbox(play);
            this->flags |= SAKON_HITTABLE;
            this->escapePoint = 0;
            this->func = NvMmSuttari_Chase;
        }
    }
}

/** MM's func_80BAD380: running his escape; hit, the bag dropped; finished; or away, the night goes on. */
static void NvMmSuttari_Chase(NvMmSuttari* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (player->stateFlags1 & PLAYER_STATE1_TALKING) {
        this->actor.speedXZ = 0.0f;
        return;
    }
    if (this->animIndex == SAKON_ANIM_HIT) { // MM's func_80BAAF1C
        if (NvMmSuttari_AnimDone(this)) {
            NvMmSuttari_Anim(this, SAKON_ANIM_RUN_FAST);
        }
    }
    if ((this->escapePath != NULL) && (this->escapePoint >= 0)) { // MM's func_80BABA90(this, 1, 1)
        Vec3s* p = &((Vec3s*)SEGMENTED_TO_VIRTUAL(this->escapePath->points))[this->escapePoint];
        Vec3f to = { p->x, p->y, p->z };
        s16 yaw = Math_Vec3f_Yaw(&this->actor.world.pos, &to);

        if (this->actor.bgCheckFlags & BGCHECKFLAG_WALL) {
            yaw = this->actor.wallYaw;
        }
        Math_SmoothStepToS(&this->actor.world.rot.y, yaw, 4, 0x3E8, 1);
        this->actor.shape.rot.y = this->actor.world.rot.y;
        { // MM's EnSuttari_HasReachedPointForward: past the plane through the point, across the path
            Vec3s* pts = SEGMENTED_TO_VIRTUAL(this->escapePath->points);
            s32 n = this->escapePath->count;
            s32 i = this->escapePoint;
            f32 dx;
            f32 dz;

            if (i == 0) {
                dx = pts[1].x - pts[0].x;
                dz = pts[1].z - pts[0].z;
            } else if (i == (n - 1)) {
                dx = pts[n - 1].x - pts[n - 2].x;
                dz = pts[n - 1].z - pts[n - 2].z;
            } else {
                dx = pts[i + 1].x - pts[i - 1].x;
                dz = pts[i + 1].z - pts[i - 1].z;
            }
            if (((this->actor.world.pos.x - to.x) * dx + (this->actor.world.pos.z - to.z) * dz) > 0.0f) {
                this->escapePoint = (i >= (n - 1)) ? -0x63 : (i + 1);
                if (getenv("NV_LOG") != NULL) {
                    printf("NVMM Sakon: escape point %d of %d at %d,%d\n", i, n, (int)this->actor.world.pos.x,
                           (int)this->actor.world.pos.z);
                    fflush(stdout);
                }
            }
        }
    } else {
        this->escapePoint = -0x63;
    }
    if (this->escapePoint == -0x63) { // away
        if (this->flags & SAKON_DROPPED) {
            NvMm_SetWeekEvent(WE_RECOVERED_STOLEN_BOMB_BAG);
        }
        this->actor.speedXZ = 0.0f;
        NvMmSakon_ToSpawn7(play);
        this->func = NvMmSuttari_TalkWait; // (nothing more)
        if (getenv("NV_LOG") != NULL) {
            printf("NVMM Sakon: away (the bag %s)\n", (this->flags & SAKON_DROPPED) ? "got back" : "gone");
            fflush(stdout);
        }
        return;
    }
    Math_ApproachF(&this->actor.speedXZ, 4.0f, 0.2f, 0.5f);
    Actor_MoveXZGravity(&this->actor);
}

/** MM's EnSuttari_UpdateCollider: hit while running with the bag. */
static void NvMmSuttari_CheckHit(NvMmSuttari* this, PlayState* play) {
    if ((this->flags & SAKON_HITTABLE) && (this->collider.base.acFlags & AC_HIT)) {
        ColliderInfo* by = this->collider.info.acHitInfo;
        u32 dmg = (by != NULL) ? by->toucher.dmgFlags : 0;

        this->collider.base.acFlags &= ~AC_HIT;
        if (dmg & (DMG_EXPLOSIVE | DMG_ARROW | DMG_HOOKSHOT | DMG_BOOMERANG | DMG_MAGIC_FIRE | DMG_MAGIC_ICE |
                   DMG_MAGIC_LIGHT | DMG_SLINGSHOT)) { // (MM's damage effect 14: he's finished)
            Vec3f pos = this->actor.world.pos;
            Vec3f zero = { 0.0f, 0.0f, 0.0f };

            NvMm_SetWeekEvent(WE_SAKON_DEAD);
            EffectSsBomb2_SpawnLayered(play, &pos, &zero, &zero, 80, 19);
            SoundSource_PlaySfxAtFixedWorldPos(play, &pos, 30, NA_SE_IT_BOMB_EXPLOSION);
            Actor_Kill(&this->actor);
            if (getenv("NV_LOG") != NULL) {
                printf("NVMM Sakon: finished\n");
                fflush(stdout);
            }
        } else if (dmg != 0) { // (15: the sword, punches, the spin) the bag dropped
            this->flags &= ~(SAKON_HITTABLE | SAKON_HAS_BAG);
            this->flags |= SAKON_DROPPED;
            NvMmSuttari_Anim(this, SAKON_ANIM_HIT);
            if (getenv("NV_LOG") != NULL) {
                printf("NVMM Sakon: hit, the bag dropped\n");
                fflush(stdout);
            }
        }
    }
}

void NvMmSuttari_Update(Actor* thisx, PlayState* play) {
    NvMmSuttari* this = (NvMmSuttari*)thisx;

    this->func(this, play);
    if (thisx->update == NULL) {
        return;
    }
    SkelAnime_Update(&this->skelAnime);
    Actor_TrackPlayer(play, thisx, &this->headRot, &this->torsoRot, thisx->focus.pos);
    Collider_UpdateCylinder(thisx, &this->collider);
    NvMmSuttari_CheckHit(this, play);
    if (thisx->update == NULL) {
        return;
    }
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    Actor_UpdateBgCheckInfo(play, thisx, 35.0f, 30.0f, 30.0f, 4);
}

static s32 NvMmSuttari_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                        void* thisx) {
    NvMmSuttari* this = (NvMmSuttari*)thisx;

    if (limbIndex == OBJECT_BOJ_LIMB_0F) { // his own head
        *dList = (Gfx*)BOJ("object_boj_DL_00AF90");
        Matrix_Translate(1500.0f, 0.0f, 0.0f, MTXMODE_APPLY);
        Matrix_RotateX(BINANG_TO_RAD(this->headRot.y), MTXMODE_APPLY);
        Matrix_RotateZ(BINANG_TO_RAD(-this->headRot.x), MTXMODE_APPLY);
        Matrix_Translate(-1500.0f, 0.0f, 0.0f, MTXMODE_APPLY);
    }
    if (limbIndex == OBJECT_BOJ_LIMB_08) {
        Matrix_RotateX(BINANG_TO_RAD(-this->torsoRot.y), MTXMODE_APPLY);
        Matrix_RotateZ(BINANG_TO_RAD(-this->torsoRot.x), MTXMODE_APPLY);
    }
    return false;
}

static void NvMmSuttari_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    NvMmSuttari* this = (NvMmSuttari*)thisx;
    Vec3f zero = { 0.0f, 0.0f, 0.0f };

    if ((limbIndex == OBJECT_BOJ_LIMB_08) && (this->flags & SAKON_HAS_BAG)) { // the stolen bag
        OPEN_DISPS(play->state.gfxCtx);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)BOJ("gBombShopBagDL"));
        CLOSE_DISPS(play->state.gfxCtx);
    }
    if (limbIndex == OBJECT_BOJ_LIMB_0F) {
        Matrix_MultVec3f(&zero, &this->actor.focus.pos);
    }
}

void NvMmSuttari_Draw(Actor* thisx, PlayState* play) {
    NvMmSuttari* this = (NvMmSuttari*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, Gfx_EnvColor(play->state.gfxCtx, 255, 255, 255, 0));
    gSPSegment(POLY_OPA_DISP++, 0x09, Gfx_EnvColor(play->state.gfxCtx, 55, 55, 255, 0));
    gDPPipeSync(POLY_OPA_DISP++);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          NvMmSuttari_OverrideLimbDraw, NvMmSuttari_PostLimbDraw, this);
    CLOSE_DISPS(play->state.gfxCtx);
}
