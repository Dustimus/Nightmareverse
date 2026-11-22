/*
 * Nightmareverse: Anju's mother, from MM's En_Ah (zeldaret/mm, ovl_En_Ah), ported closely
 * (mm/README.md). MM's model (object_ah; her eyes and mouth set in segments 8 and 9 as in MM, with
 * MM's blinking), MM's schedule (mm/schedules/En_Ah.schl) and message scripts
 * (nv_mm_msgscripts/En_Ah.msg.inc): at the Stock Pot Inn's desk on Days 2 and 3, and with Anju in
 * the staff room on Day 2's night (21:00-23:00). Her Day 3 night at the ranch house (MM's Omoya)
 * waits for that scene.
 *
 * Adapted from MM: her head and body turning to whoever she talks to (MM's transform draw) are left
 * out; the Kafei's Mask line waits for MM's masks; her faces by line (0x28FD, 0x2954: the line where
 * she bows her head) are MM's, her bow is in.
 */
#include "global.h"
#include "z_nv_mm_schedule.h"
#include "z_nv_mm_msgevent.h"

#include "nv_mm_schedules/En_Ah.schl.inc"
#include "nv_mm_msgscripts/En_Ah.msg.inc"

#define OBJECT_AH_LIMB_MAX 17
#define AH(name) ("__OTR__mm/objects/object_ah/" name)
static const char sSkel[] = AH("object_ah_Skel_009E70");

enum { ENAH_ANIM_0, ENAH_ANIM_1, ENAH_ANIM_2, ENAH_ANIM_3, ENAH_ANIM_4, ENAH_ANIM_MAX };
static struct {
    const char* anim;
    u8 mode;
    s8 morph;
} sAnimationInfo[ENAH_ANIM_MAX] = {
    { AH("object_ah_Anim_001860"), ANIMMODE_LOOP, 0 },  { AH("object_ah_Anim_001860"), ANIMMODE_LOOP, -4 },
    { AH("object_ah_Anim_002280"), ANIMMODE_ONCE, 0 },  { AH("object_ah_Anim_000968"), ANIMMODE_LOOP, -4 },
    { AH("object_ah_Anim_000DDC"), ANIMMODE_LOOP, 0 },
};
static s16 sMouthByFace[] = { 0, 0, 1, 0 };
static PosRot sDeskPos = { { 100.0f, 0.0f, 67.0f }, { 0, 0x349C, 0 } };
static PosRot sStaffRoomPos = { { -395.0f, 210.0f, -162.0f }, { 0, (s16)0xBE98, 0 } };
static const char* sMouthTextures[] = { AH("object_ah_Tex_008D70"), AH("object_ah_Tex_009570") };
static const char* sEyeTextures[] = { AH("object_ah_Tex_006D70"), AH("object_ah_Tex_007570"),
                                      AH("object_ah_Tex_007D70"), AH("object_ah_Tex_007570"),
                                      AH("object_ah_Tex_008570") };

#define FLAG_OFFER 1
#define FLAG_BLINK 0x40
#define FLAG_NO_GRAVITY 0x10
#define FLAG_TALKING 0x100

struct NvMmAh;
typedef void (*NvMmAhFunc)(struct NvMmAh*, PlayState*);

typedef struct NvMmAh {
    Actor actor;
    SkelAnime skelAnime;
    NvMmAhFunc func;
    NvMmAhFunc lineFunc; // (MM's unk_18C: what a line has her do)
    ColliderCylinder collider;
    u8 scheduleResult;
    s32 msgScriptPos;
    Vec3s jointTable[OBJECT_AH_LIMB_MAX];
    Vec3s morphTable[OBJECT_AH_LIMB_MAX];
    u16 flags;
    u16 lastTextId;
    f32 animPlaySpeed;
    s16 face;
    s16 blinkTimer;
    s16 eyeIndex;
    s16 mouthIndex;
    s16 lineState;
    s32 animIndex;
} NvMmAh;

const size_t gNvMmAhSize = sizeof(NvMmAh);

s32 NvMmClock_Day(void);
s32 NvMmActor_Id(s32 mmId);

static ColliderCylinderInit sCylinderInit = {
    { COLTYPE_HIT1, AT_NONE, AC_NONE, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK1, { 0x00000000, 0x00, 0x00 }, { 0x00000000, 0x00, 0x00 }, TOUCH_NONE, BUMP_NONE, OCELEM_ON },
    { 10, 68, 0, { 0, 0, 0 } },
};

static void NvMmAh_FollowSchedule(NvMmAh* this, PlayState* play);

static void NvMmAh_ChangeAnim(NvMmAh* this, s32 animIndex) {
    s32 change = ((animIndex == ENAH_ANIM_0) || (animIndex == ENAH_ANIM_1))
                     ? ((this->animIndex != ENAH_ANIM_0) && (this->animIndex != ENAH_ANIM_1))
                     : (this->animIndex != animIndex);

    if (change) {
        AnimationHeader* anim = (AnimationHeader*)sAnimationInfo[animIndex].anim;

        this->animIndex = animIndex;
        Animation_Change(&this->skelAnime, anim, 1.0f, 0.0f, Animation_GetLastFrame(anim),
                         sAnimationInfo[animIndex].mode, sAnimationInfo[animIndex].morph);
        this->animPlaySpeed = this->skelAnime.playSpeed;
    }
}

/** MM's func_80BD2C6C: blinking (held shut on a sad face). */
static void NvMmAh_Blink(NvMmAh* this) {
    s32 done = false;

    if ((this->flags & FLAG_BLINK) && (DECR(this->blinkTimer) == 0)) {
        if (((this->face == 1) || (this->face == 2)) && ((this->eyeIndex == 4) || (this->eyeIndex == 2))) {
            done = true;
            this->eyeIndex = 4;
        }
        if (!done && (this->eyeIndex == 4)) {
            this->eyeIndex = 0;
        }
        if (!done) {
            this->eyeIndex++;
            if (this->eyeIndex >= 4) {
                this->blinkTimer = (this->face == 0) ? Rand_S16Offset(30, 30) : 8;
                this->eyeIndex = 0;
                done = true;
            }
        }
    }
    if (done) {
        this->mouthIndex = sMouthByFace[this->face & 3];
    }
}

/** MM's func_80BD3118: her bow (the line 0x2954). */
static void NvMmAh_Bow(NvMmAh* this, PlayState* play) {
    if (this->lineState == 0) {
        NvMmAh_ChangeAnim(this, ENAH_ANIM_2);
        this->lineState++;
    } else if ((this->lineState == 1) && Animation_OnFrame(&this->skelAnime, this->skelAnime.endFrame)) {
        NvMmAh_ChangeAnim(this, ENAH_ANIM_3);
        this->lineState++;
    }
}

/** MM's func_80BD3198: what she does by the line being shown. */
static void NvMmAh_ByLine(NvMmAh* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    u16 textId = play->msgCtx.textId - 0xB000; // (MM's id: ours are 0xB000 + MM's)

    if (player->stateFlags1 & PLAYER_STATE1_TALKING) {
        if (this->lastTextId != textId) {
            if (textId == 0x2954) {
                this->lineFunc = NvMmAh_Bow;
                this->lineState = 0;
            }
            if ((textId == 0x28FD) || (textId == 0x2954)) {
                this->face = (textId == 0x28FD) ? 1 : 2;
                this->blinkTimer = 8;
            }
        }
        this->lastTextId = textId;
        this->flags |= FLAG_TALKING;
    } else if (this->flags & FLAG_TALKING) {
        this->lineFunc = NULL;
        this->lastTextId = 0;
        this->flags &= ~FLAG_TALKING;
        this->face = 0;
        this->blinkTimer = 4;
        NvMmAh_ChangeAnim(this, (this->scheduleResult == 2) ? ENAH_ANIM_4 : ENAH_ANIM_0);
    }
    if (this->lineFunc != NULL) {
        this->lineFunc(this, play);
    }
}

static MsgScript* NvMmAh_GetMsgScript(NvMmAh* this) {
    switch (this->scheduleResult) {
        case 1:
            return (NvMmClock_Day() == 2) ? D_80BD3DF0 : D_80BD3DE8;
        case 2:
            return D_80BD3DF8;
        default:
            return NULL;
    }
}

static void NvMmAh_Talk(NvMmAh* this, PlayState* play) {
    MsgScript* script = NvMmAh_GetMsgScript(this);

    if ((script == NULL) || NvMmMsgEvent_RunScript(&this->actor, play, script, NULL, &this->msgScriptPos)) {
        this->flags |= FLAG_OFFER;
        this->msgScriptPos = 0;
        this->func = NvMmAh_FollowSchedule;
    } else if (this->scheduleResult != 2) {
        Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 4, 0x2AA8);
    }
}

/** MM's func_80BD3548: at the desk (1), at the ranch house (2: not built), with Anju (3). */
static s32 NvMmAh_SetupResult(NvMmAh* this, PlayState* play, NvMmScheduleOutput* out) {
    Actor* a;

    this->flags = 0;
    switch (out->result) {
        case 1:
            this->actor.world.pos = sDeskPos.pos;
            this->actor.world.rot = this->actor.shape.rot = sDeskPos.rot;
            NvMmAh_ChangeAnim(this, ENAH_ANIM_0);
            this->flags |= FLAG_OFFER | FLAG_BLINK;
            return true;
        case 3: // (with Anju in the staff room: MM looks for her)
            for (a = play->actorCtx.actorLists[ACTORCAT_NPC].head; a != NULL; a = a->next) {
                if ((a->id == NvMmActor_Id(0x202)) && (a->update != NULL)) {
                    break;
                }
            }
            if (a == NULL) {
                return false;
            }
            this->actor.child = a;
            this->actor.world.pos = sStaffRoomPos.pos;
            this->actor.world.rot = this->actor.shape.rot = sStaffRoomPos.rot;
            NvMmAh_ChangeAnim(this, ENAH_ANIM_4);
            this->flags |= FLAG_BLINK | FLAG_NO_GRAVITY;
            this->actor.gravity = 0.0f;
            return true;
        default:
            return false;
    }
}

static void NvMmAh_FollowSchedule(NvMmAh* this, PlayState* play) {
    NvMmScheduleOutput out;

    out.result = 0;
    if (!NvMmSchedule_RunScript(play, D_80BD3DB0, &out) ||
        ((this->scheduleResult != out.result) && !NvMmAh_SetupResult(this, play, &out))) {
        this->actor.shape.shadowDraw = NULL;
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        out.result = 0;
    } else {
        this->actor.shape.shadowDraw = ActorShadow_DrawCircle;
        this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    }
    this->scheduleResult = out.result;
    Math_ApproachS(&this->actor.shape.rot.y, this->actor.world.rot.y, 3, 0x2AA8);
}

void NvMmAh_Init(Actor* thisx, PlayState* play) {
    NvMmAh* this = (NvMmAh*)thisx;
    Actor* a;

    for (a = play->actorCtx.actorLists[ACTORCAT_NPC].head; a != NULL; a = a->next) { // (one of her only)
        if ((a != thisx) && (a->id == thisx->id) && (a->update != NULL)) {
            Actor_Kill(thisx);
            return;
        }
    }
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 20.0f);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sSkel, NULL, this->jointTable, this->morphTable,
                       OBJECT_AH_LIMB_MAX);
    this->animIndex = -1;
    NvMmAh_ChangeAnim(this, ENAH_ANIM_0);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    Actor_SetScale(&this->actor, 0.01f);
    this->func = NvMmAh_FollowSchedule;
    this->func(this, play);
}

void NvMmAh_Destroy(Actor* thisx, PlayState* play) {
    NvMmAh* this = (NvMmAh*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

void NvMmAh_Update(Actor* thisx, PlayState* play) {
    NvMmAh* this = (NvMmAh*)thisx;

    if ((this->func == NvMmAh_FollowSchedule) && (this->flags & FLAG_OFFER) &&
        Actor_ProcessTalkRequest(&this->actor, play)) {
        this->flags &= ~FLAG_OFFER;
        this->func = NvMmAh_Talk;
    }
    this->func(this, play);
    if (this->scheduleResult == 0) {
        return;
    }
    NvMmAh_ByLine(this, play);
    this->skelAnime.playSpeed = this->animPlaySpeed;
    SkelAnime_Update(&this->skelAnime);
    NvMmAh_Blink(this);
    if ((this->func == NvMmAh_FollowSchedule) && (this->flags & FLAG_OFFER)) { // (MM's SubS_Offer, on screen)
        Actor_OfferTalkExchange(&this->actor, play, this->collider.dim.radius + 60.0f,
                                this->collider.dim.height + 10.0f, EXCH_ITEM_NONE);
    }
    if (!(this->flags & FLAG_NO_GRAVITY)) {
        Actor_MoveXZGravity(&this->actor);
        Actor_UpdateBgCheckInfo(play, &this->actor, 30.0f, 12.0f, 0.0f, 4);
    }
    Actor_SetFocus(&this->actor, 60.0f);
    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

void NvMmAh_Draw(Actor* thisx, PlayState* play) {
    NvMmAh* this = (NvMmAh*)thisx;

    if (this->scheduleResult == 0) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sEyeTextures[this->eyeIndex % 5]));
    gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(sMouthTextures[this->mouthIndex & 1]));
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          NULL, NULL, &this->actor);
    CLOSE_DISPS(play->state.gfxCtx);
}
