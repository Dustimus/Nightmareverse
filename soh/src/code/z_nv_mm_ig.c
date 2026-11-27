/*
 * Nightmareverse: the Goron at the Stock Pot Inn ("Link the Goron"), from MM's En_Ig (zeldaret/mm,
 * ovl_En_Ig), ported closely (mm/README.md). MM's model (object_dai; his eyes set in segment 8 by
 * MM's blinking), MM's schedule (mm/schedules/En_Ig.schl) and message scripts
 * (nv_mm_msgscripts/En_Ig.msg.inc). Day 1 only, as in MM: he walks into East Clock Town at 15:25,
 * through the inn's door at 15:45, to the desk, where he asks Anju for the room he booked
 * (16:10-16:30). If Link took the room key first, there's no room: he walks back out and sleeps in
 * East Clock Town (18:00 to dawn); else he goes to the Knife Chamber.
 *
 * Adapted from MM:
 * - the talk at the desk is MM's, with Anju (MM's FOCUS_TO_CHILD / _SELF); in Anju's cutscene,
 *   the shot turning to each speaker as MM's (z_nv_mm_csmgr.c);
 * - his head turn toward whom he talks to (MM's transform draw, SubS_UpdateLimb) is left out;
 * - Link is MM's human form: the Goron Mask lines wait for MM's masks;
 * - MM's sounds for his steps and his snoring aren't in OoT's sound bank: left out.
 */
#include "global.h"
#include "z_nv_mm_csmgr.h"
#include "z_nv_mm_schedule.h"
#include "z_nv_mm_subs.h"
#include "z_nv_mm_msgevent.h"
#include <stdio.h>
#include <stdlib.h>

#define ENIG_GET_PATH_INDEX(thisx) ((thisx)->params & 0xFF)
#define OBJECT_DAI_LIMB_09 9
#define OBJECT_DAI_LIMB_0A 10
#define OBJECT_DAI_LIMB_0B 11
#define OBJECT_DAI_LIMB_0C 12
#define OBJECT_DAI_LIMB_MAX 19

#include "nv_mm_schedules/En_Ig.schl.inc"
#include "nv_mm_msgscripts/En_Ig.msg.inc"

// MM's D_80BF3318: how far along his path chain each result's path is
static s32 sPathLimit[] = { -1, -1, 3, 1, 3, 1, 2, 0, 3, 5, 0, 3, 1, 2, 4 };

#define DAI(name) ("__OTR__mm/objects/object_dai/" name)
static const char sSkel[] = DAI("object_dai_Skel_0130D0");
enum { ENIG_ANIM_0, ENIG_ANIM_1, ENIG_ANIM_2, ENIG_ANIM_3, ENIG_ANIM_4, ENIG_ANIM_5, ENIG_ANIM_6, ENIG_ANIM_7,
       ENIG_ANIM_8, ENIG_ANIM_9, ENIG_ANIM_MAX };
static struct {
    const char* anim;
    u8 mode;
    s8 morph;
} sAnimationInfo[ENIG_ANIM_MAX] = {
    { DAI("object_dai_Anim_0048B4"), ANIMMODE_LOOP, 0 },  { DAI("object_dai_Anim_0048B4"), ANIMMODE_LOOP, -4 },
    { DAI("object_dai_Anim_005100"), ANIMMODE_LOOP, 0 },  { DAI("object_dai_Anim_005100"), ANIMMODE_LOOP, -4 },
    { DAI("object_dai_Anim_0010F8"), ANIMMODE_ONCE, 0 },  { DAI("object_dai_Anim_001E44"), ANIMMODE_LOOP, -4 },
    { DAI("object_dai_Anim_0014BC"), ANIMMODE_ONCE, 0 },  { DAI("object_dai_Anim_003CAC"), ANIMMODE_ONCE, -4 },
    { DAI("object_dai_Anim_0040E0"), ANIMMODE_LOOP, 0 },  { DAI("object_dai_Anim_0040E0"), ANIMMODE_LOOP, -4 },
};
static const char* sEyeTextures[] = { DAI("object_dai_Tex_0107B0"), DAI("object_dai_Tex_010FB0"),
                                      DAI("object_dai_Tex_0117B0"), DAI("object_dai_Tex_011FB0"),
                                      DAI("object_dai_Tex_0127B0") };

#define FLAG_OFFER 1          // (MM's SubS offer mode: on screen)
#define FLAG_PATH_STARTED 8
#define FLAG_PATH_END 0x10
#define FLAG_BLINK 0x100
#define FLAG_TALKING 0x400

struct NvMmIg;
typedef void (*NvMmIgFunc)(struct NvMmIg*, PlayState*);

typedef struct NvMmIg {
    Actor actor;
    SkelAnime skelAnime;
    NvMmIgFunc func;
    NvMmIgFunc lineFunc; // (MM's unk_18C: what a line has him do)
    MtxF headMtx;        // (MM's unk_190: limb 0x0A's matrix, his head drawn after the skeleton)
    ColliderCylinder collider;
    Path* timePath;
    Vec3f timePathTargetPos;
    f32 timePathProgress;
    s32 timePathTotalTime;
    s32 timePathWaypointTime;
    s32 timePathWaypoint;
    s32 timePathElapsedTime;
    u8 scheduleResult;
    MsgScript* msgScript;
    s32 msgScriptPos;
    s8 doorOpenTimer;
    Actor* lookAt; // (MM's unk_2A8: Anju at the desk, else Link)
    Vec3f doorEntrancePos;
    Vec3f doorExitPos;
    Vec3s jointTable[OBJECT_DAI_LIMB_MAX];
    Vec3s morphTable[OBJECT_DAI_LIMB_MAX];
    u16 flags;
    u16 lastTextId;
    f32 animPlaySpeed;
    s16 doorTimeTotal;
    s16 doorTimeProgress;
    s16 timePathTimeSpeed;
    s16 blinkTimer;
    s16 eyeIndex;
    s16 lineState;
    s32 animIndex;
    s32 snore;
    s16 csStep; // MM's unk_3F6: the desk talk's cutscene, step by step
} NvMmIg;

const size_t gNvMmIgSize = sizeof(NvMmIg);

s32 NvMmClock_Speed(void);
s32 NvMmActor_Id(s32 mmId);
s32 NvMmDoor_ScheduleType(Actor* thisx);
void NvMmDoor_SetOpenTimer(Actor* thisx, s8 frames);

static ColliderCylinderInit sCylinderInit = {
    { COLTYPE_HIT1, AT_NONE, AC_NONE, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK1, { 0x00000000, 0x00, 0x00 }, { 0x00000000, 0x00, 0x00 }, TOUCH_NONE, BUMP_NONE, OCELEM_ON },
    { 28, 62, 0, { 0, 0, 0 } },
};

static void NvMmIg_FollowSchedule(NvMmIg* this, PlayState* play);

static Actor* NvMmIg_FindAnju(NvMmIg* this, PlayState* play) {
    s32 id = NvMmActor_Id(0x202);
    Actor* a;

    for (a = play->actorCtx.actorLists[ACTORCAT_NPC].head; a != NULL; a = a->next) {
        if ((a->id == id) && (a->update != NULL)) {
            return a;
        }
    }
    return NULL;
}

/** MM's EnIg_FindScheduleDoor: the inn's front door, or the Knife Chamber's. */
static Actor* NvMmIg_FindScheduleDoor(PlayState* play, s32 result) {
    s32 doorId = NvMmActor_Id(0x005);
    s32 schType;
    Actor* a;

    switch (result) {
        case 5:
        case 6:
        case 7:
        case 9:
            schType = 11; // INN_MAIN_ENTRANCE
            break;
        case 8:
            schType = 15; // INN_KNIFE_CHAMBER
            break;
        default:
            return NULL;
    }
    for (a = play->actorCtx.actorLists[ACTORCAT_DOOR].head; a != NULL; a = a->next) {
        if ((a->id == doorId) && (a->update != NULL) && (NvMmDoor_ScheduleType(a) == schType)) {
            return a;
        }
    }
    return NULL;
}

static Path* NvMmIg_Path(NvMmIg* this, PlayState* play, s32 result) {
    s32 limit = ((result >= 0) && (result < ARRAY_COUNT(sPathLimit))) ? sPathLimit[result] : -1;

    return (limit >= 0) ? NvMmSubS_GetAdditionalPath(play, ENIG_GET_PATH_INDEX(&this->actor), limit) : NULL;
}

static void NvMmIg_ChangeAnim(NvMmIg* this, s32 animIndex) {
    s32 change;

    if ((animIndex == ENIG_ANIM_0) || (animIndex == ENIG_ANIM_1)) {
        change = (this->animIndex != ENIG_ANIM_0) && (this->animIndex != ENIG_ANIM_1);
    } else if ((animIndex == ENIG_ANIM_2) || (animIndex == ENIG_ANIM_3)) {
        change = (this->animIndex != ENIG_ANIM_2) && (this->animIndex != ENIG_ANIM_3);
    } else {
        change = (this->animIndex != animIndex);
    }
    if (change) {
        AnimationHeader* anim = (AnimationHeader*)sAnimationInfo[animIndex].anim;

        this->animIndex = animIndex;
        Animation_Change(&this->skelAnime, anim, 1.0f, 0.0f, Animation_GetLastFrame(anim),
                         sAnimationInfo[animIndex].mode, sAnimationInfo[animIndex].morph);
        this->animPlaySpeed = this->skelAnime.playSpeed;
    }
}

/** MM's func_80BF13E4: blinking. */
static void NvMmIg_Blink(NvMmIg* this) {
    if ((this->flags & FLAG_BLINK) && (DECR(this->blinkTimer) == 0)) {
        this->eyeIndex++;
        if (this->eyeIndex >= 4) {
            this->blinkTimer = Rand_S16Offset(30, 30);
            this->eyeIndex = 0;
        }
    }
}

/** MM's func_80BF1A60: the lines 0x28B0 and 0x28B7 (he bows his head, then waits). */
static void NvMmIg_LineBow(NvMmIg* this, PlayState* play) {
    if (this->lineState == 0) {
        NvMmIg_ChangeAnim(this, ENIG_ANIM_4);
        this->lineState++;
    } else if ((this->lineState == 1) && Animation_OnFrame(&this->skelAnime, this->skelAnime.endFrame)) {
        NvMmIg_ChangeAnim(this, ENIG_ANIM_5);
        this->lineState++;
    }
}

/** MM's func_80BF1AE0: after talking, back to what he was doing. */
static void NvMmIg_AnimAfterTalking(NvMmIg* this) {
    if (this->scheduleResult == 3) {
        NvMmIg_ChangeAnim(this, ENIG_ANIM_0);
    } else if ((this->scheduleResult >= 10) && (this->scheduleResult <= 14)) {
        NvMmIg_ChangeAnim(this, ENIG_ANIM_2);
    }
}

/** MM's func_80BF1B40: what he does by the line being shown (his own talk, or Anju's). */
static void NvMmIg_ByLine(NvMmIg* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    u16 textId = play->msgCtx.textId - 0xB000; // (MM's id: ours are 0xB000 + MM's)

    if (player->stateFlags1 & PLAYER_STATE1_TALKING) {
        this->flags |= FLAG_TALKING;
        if (this->lastTextId != textId) {
            if ((this->animIndex == ENIG_ANIM_2) || (this->animIndex == ENIG_ANIM_3)) {
                NvMmIg_ChangeAnim(this, ENIG_ANIM_0);
            }
            if ((textId == 0x28B0) || (textId == 0x28B7)) {
                this->lineFunc = NvMmIg_LineBow;
                this->lineState = 0;
            }
        }
        this->lastTextId = textId;
    } else if (this->flags & FLAG_TALKING) {
        this->lastTextId = 0;
        this->flags &= ~FLAG_TALKING;
        NvMmIg_AnimAfterTalking(this);
    }
    if (this->lineFunc != NULL) {
        this->lineFunc(this, play);
    }
}

/** MM's func_80BF1744: Anju's cutscene, or one chained after it. */
static s16 NvMmIg_ChildCsId(NvMmIg* this, s32 numCutscenes) {
    s16 csId = NV_MM_CS_ID_NONE;
    s32 i;

    if ((this->actor.child != NULL) && (this->actor.child->update != NULL)) {
        csId = NvMmCs_ActorCsId(this->actor.child);
        for (i = 0; (i < numCutscenes) && (csId != NV_MM_CS_ID_NONE); i++) {
            csId = NvMmCsMgr_GetAdditionalCsId(csId);
        }
    }
    return csId;
}

/** MM's func_80BF16C8: queue it until it's next, then start it. */
static s32 NvMmIg_StartCs(NvMmIg* this, s16 csId) {
    if (NvMmCsMgr_IsNext(csId)) {
        NvMmCsMgr_StartWithPlayerCs(csId, &this->actor);
        return true;
    }
    NvMmCsMgr_Queue(csId);
    return false;
}

/** MM's func_80BF17BC: the talk at the desk with Anju, the shot turning to each of them in turn. */
static s32 NvMmIg_MsgEventDesk(Actor* thisx, PlayState* play) {
    NvMmIg* this = (NvMmIg*)thisx;
    s16 csId = NvMmIg_ChildCsId(this, 0);
    s32 ret = false;

    if (csId == NV_MM_CS_ID_NONE) {
        return true;
    }
    switch (this->csStep) {
        case 0:
            if (!NvMmIg_StartCs(this, csId)) {
                break;
            }
            // fallthrough
        case 2:
        case 4:
            if ((this->actor.child != NULL) && (this->actor.child->update != NULL)) {
                NvMmCs_SetTargetActor(this->actor.child);
            }
            this->csStep++;
            ret = true;
            break;
        case 1:
        case 3:
            if (!NvMm_CheckWeekEvent((75 << 8) | 0x10) && (this->csStep == 3)) { // (RECEIVED_ROOM_KEY)
                NvMmCsMgr_Stop(csId);
                this->csStep = 5;
            } else {
                NvMmCs_SetTargetActor(&this->actor);
            }
            this->csStep++;
            ret = true;
            break;
        case 5:
            NvMmCsMgr_Stop(csId);
            this->csStep++;
            ret = true;
            break;
        default:
            break;
    }
    return ret;
}

/** MM's EnIg_GetMsgScript. */
static MsgScript* NvMmIg_GetMsgScript(NvMmIg* this) {
    switch (this->scheduleResult) {
        case 3:
            return D_80BF335C;
        case 4:
            return D_80BF33F0;
        case 2:
        case 11:
        case 14:
            return D_80BF33E0;
        case 10:
        case 12:
            return D_80BF3354;
        case 13:
            return D_80BF33E8;
        default:
            return NULL;
    }
}

static void NvMmIg_Talk(NvMmIg* this, PlayState* play) {
    MsgScript* script = NvMmIg_GetMsgScript(this);

    if ((script == NULL) ||
        NvMmMsgEvent_RunScript(&this->actor, play, script, (this->scheduleResult == 3) ? NvMmIg_MsgEventDesk : NULL,
                               &this->msgScriptPos)) {
        this->flags |= FLAG_OFFER;
        this->msgScriptPos = 0;
        this->actor.child = NULL;
        this->func = NvMmIg_FollowSchedule;
    } else if ((this->scheduleResult != 2) && (this->scheduleResult != 4) && (this->lookAt != NULL) &&
               (this->lookAt->update != NULL)) {
        Math_ApproachS(&this->actor.shape.rot.y, Actor_WorldYawTowardActor(&this->actor, this->lookAt), 4, 0x2AA8);
    }
}

/** MM's func_80BF1D78: at the desk, Anju there. */
static s32 NvMmIg_SetupDesk(NvMmIg* this, PlayState* play, NvMmScheduleOutput* out) {
    Vec3s* points;
    Vec3f before;

    this->timePath = NvMmIg_Path(this, play, out->result);
    if ((NvMmIg_FindAnju(this, play) == NULL) || (this->timePath == NULL) || (this->timePath->count < 2)) {
        return false;
    }
    points = SEGMENTED_TO_VIRTUAL(this->timePath->points);
    before = (Vec3f){ points[this->timePath->count - 2].x, points[this->timePath->count - 2].y,
                      points[this->timePath->count - 2].z };
    this->actor.world.pos = (Vec3f){ points[this->timePath->count - 1].x, points[this->timePath->count - 1].y,
                                     points[this->timePath->count - 1].z };
    this->actor.world.rot.y = Math_Vec3f_Yaw(&before, &this->actor.world.pos);
    NvMmIg_ChangeAnim(this, ENIG_ANIM_0);
    this->flags |= FLAG_OFFER | FLAG_BLINK;
    return true;
}

/** MM's func_80BF1DF4: through a door, it held open for him. */
static s32 NvMmIg_SetupDoor(NvMmIg* this, PlayState* play, NvMmScheduleOutput* out) {
    u16 now = NvMmSchedule_Now();
    Actor* door = NvMmIg_FindScheduleDoor(play, out->result);
    Vec3s* points;

    this->timePath = NvMmIg_Path(this, play, out->result);
    if ((door == NULL) || (this->timePath == NULL) || (this->timePath->count < 2)) {
        return false;
    }
    points = SEGMENTED_TO_VIRTUAL(this->timePath->points);
    this->doorEntrancePos = (Vec3f){ points[0].x, points[0].y, points[0].z };
    this->doorExitPos = (Vec3f){ points[1].x, points[1].y, points[1].z };
    this->actor.world.rot.y = Math_Vec3f_Yaw(&this->doorEntrancePos, &this->doorExitPos);
    this->actor.world.pos = this->doorEntrancePos;
    this->doorOpenTimer = (ABS((s16)(this->actor.world.rot.y - door->shape.rot.y)) <= 0x4000) ? -75 : 75;
    this->doorTimeTotal = out->time1 - out->time0;
    this->doorTimeProgress = now - out->time0;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->flags |= FLAG_BLINK;
    NvMmIg_ChangeAnim(this, ENIG_ANIM_3);
    this->actor.gravity = 0.0f;
    return true;
}

/** MM's func_80BF1FA8: walking his time path. */
static s32 NvMmIg_SetupWalking(NvMmIg* this, PlayState* play, NvMmScheduleOutput* out) {
    u16 now = NvMmSchedule_Now();
    u16 startTime;

    this->timePath = NvMmIg_Path(this, play, out->result);
    if ((this->timePath != NULL) && (this->timePath->count < 3)) {
        this->timePath = NULL;
    }
    if (this->timePath == NULL) {
        return false;
    }
    if ((this->scheduleResult < 10) && (this->scheduleResult != 0) && (this->timePathTimeSpeed >= 0)) {
        startTime = now;
    } else {
        startTime = out->time0;
    }
    if (out->time1 < startTime) {
        this->timePathTotalTime = (startTime - out->time1) + 0xFFFF;
    } else {
        this->timePathTotalTime = out->time1 - startTime;
    }
    this->timePathElapsedTime = now - startTime;
    this->timePathWaypointTime = MAX(this->timePathTotalTime / (this->timePath->count - 2), 1);
    this->timePathWaypoint = (this->timePathElapsedTime / this->timePathWaypointTime) + 2;
    this->flags &= ~(FLAG_PATH_STARTED | FLAG_PATH_END);
    this->flags |= FLAG_OFFER | FLAG_BLINK;
    NvMmIg_ChangeAnim(this, ENIG_ANIM_2);
    this->actor.gravity = -1.0f;
    return true;
}

/** MM's func_80BF219C: at his path's end, standing (2) or asleep (4). */
static s32 NvMmIg_SetupStand(NvMmIg* this, PlayState* play, NvMmScheduleOutput* out) {
    Vec3s* points;
    Vec3f before;

    this->timePath = NvMmIg_Path(this, play, out->result);
    if ((this->timePath == NULL) || (this->timePath->count < 2)) {
        return false;
    }
    points = SEGMENTED_TO_VIRTUAL(this->timePath->points);
    before = (Vec3f){ points[this->timePath->count - 2].x, points[this->timePath->count - 2].y,
                      points[this->timePath->count - 2].z };
    this->actor.world.pos = this->actor.prevPos = (Vec3f){
        points[this->timePath->count - 1].x, points[this->timePath->count - 1].y, points[this->timePath->count - 1].z
    };
    this->actor.world.rot.y = Math_Vec3f_Yaw(&before, &this->actor.world.pos);
    this->actor.shape.rot = this->actor.world.rot;
    if (out->result == 2) {
        this->actor.home.rot.y = this->actor.world.rot.y + 0x8000;
        NvMmIg_ChangeAnim(this, ENIG_ANIM_1);
    } else {
        this->actor.world.rot.y += 0x8000;
        this->actor.shape.rot.y = this->actor.world.rot.y;
        NvMmIg_ChangeAnim(this, ENIG_ANIM_8);
    }
    this->flags |= FLAG_OFFER | FLAG_BLINK;
    return true;
}

/** MM's func_80BF2368. */
static s32 NvMmIg_SetupResult(NvMmIg* this, PlayState* play, NvMmScheduleOutput* out) {
    this->flags = 0;
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    switch (out->result) {
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
            return NvMmIg_SetupDoor(this, play, out);
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
            return NvMmIg_SetupWalking(this, play, out);
        case 2:
        case 4:
            return NvMmIg_SetupStand(this, play, out);
        case 3:
            return NvMmIg_SetupDesk(this, play, out);
        default:
            return false;
    }
}

/** MM's func_80BF2470: through the door. */
static void NvMmIg_DoorWalk(NvMmIg* this, PlayState* play) {
    Actor* door = NvMmIg_FindScheduleDoor(play, this->scheduleResult);
    f32 dist;

    if (Player_InCsMode(play) || (this->timePathTimeSpeed == 0) || (this->doorTimeTotal <= 0)) {
        return;
    }
    if (door != NULL) {
        NvMmDoor_SetOpenTimer(door, (((f32)this->doorTimeProgress / this->doorTimeTotal) <= 0.9f)
                                        ? this->doorOpenTimer
                                        : 0);
    }
    this->doorTimeProgress = CLAMP(this->doorTimeProgress, 0, this->doorTimeTotal);
    dist = this->doorTimeProgress * (Math_Vec3f_DistXZ(&this->doorEntrancePos, &this->doorExitPos) / this->doorTimeTotal);
    this->actor.world.pos.x = this->doorEntrancePos.x + Math_SinS(this->actor.world.rot.y) * dist;
    this->actor.world.pos.y = this->doorEntrancePos.y;
    this->actor.world.pos.z = this->doorEntrancePos.z + Math_CosS(this->actor.world.rot.y) * dist;
    this->doorTimeProgress += this->timePathTimeSpeed;
}

/** MM's func_80BF25E8: along his time path. */
static void NvMmIg_FollowTimePath(NvMmIg* this, PlayState* play) {
    static f32 knots[265];
    Vec3f target;

    NvMmSubS_TimePathing_FillKnots(knots, NV_MM_TIME_PATHING_ORDER, this->timePath->count + NV_MM_TIME_PATHING_ORDER);
    if (!(this->flags & FLAG_PATH_STARTED)) {
        target = (Vec3f){ 0.0f, 0.0f, 0.0f };
        NvMmSubS_TimePathing_Update(this->timePath, &this->timePathProgress, &this->timePathElapsedTime,
                                    this->timePathWaypointTime, this->timePathTotalTime, &this->timePathWaypoint,
                                    knots, &target, this->timePathTimeSpeed);
        NvMmSubS_TimePathing_ComputeInitialY(play, this->timePath, this->timePathWaypoint, &target);
        this->actor.world.pos.y = target.y;
        this->flags |= FLAG_PATH_STARTED;
    } else {
        target = this->timePathTargetPos;
    }
    this->actor.world.pos.x = target.x;
    this->actor.world.pos.z = target.z;
    this->timePathTargetPos = (Vec3f){ 0.0f, 0.0f, 0.0f };
    if (NvMmSubS_TimePathing_Update(this->timePath, &this->timePathProgress, &this->timePathElapsedTime,
                                    this->timePathWaypointTime, this->timePathTotalTime, &this->timePathWaypoint,
                                    knots, &this->timePathTargetPos, this->timePathTimeSpeed)) {
        this->flags |= FLAG_PATH_END;
    } else {
        this->actor.world.rot.y = Math_Vec3f_Yaw(&this->actor.world.pos, &this->timePathTargetPos);
    }
}

/** MM's func_80BF2890: asleep, snoring. */
static void NvMmIg_Snore(NvMmIg* this, PlayState* play) {
    if ((this->flags & FLAG_BLINK) && (this->eyeIndex == 2)) {
        this->flags &= ~FLAG_BLINK; // (his eyes stay shut)
    }
    if (!(this->flags & FLAG_BLINK) && Animation_OnFrame(&this->skelAnime, this->skelAnime.endFrame)) {
        this->snore ^= 1; // (MM's two snores, NA_SE_EN_GOLON_SNORE1/2: not in OoT's sound bank)
    }
}

/** MM's func_80BF293C: turning to face the street, and sitting down. */
static void NvMmIg_Stand(NvMmIg* this, PlayState* play) {
    if (this->actor.world.rot.y != this->actor.home.rot.y) {
        if ((this->actor.world.rot.y / 182) != (this->actor.home.rot.y / 182)) {
            Math_ApproachS(&this->actor.world.rot.y, this->actor.home.rot.y, 3, 0x2AA8);
        } else {
            this->actor.world.rot.y = this->actor.home.rot.y;
            NvMmIg_ChangeAnim(this, ENIG_ANIM_7);
        }
    } else if ((this->animIndex == ENIG_ANIM_7) && Animation_OnFrame(&this->skelAnime, this->skelAnime.endFrame)) {
        this->flags |= FLAG_OFFER;
        NvMmIg_ChangeAnim(this, ENIG_ANIM_9);
    }
}

/** MM's func_80BF2AF8 and func_80BF2A50. */
static void NvMmIg_FollowSchedule(NvMmIg* this, PlayState* play) {
    NvMmScheduleOutput out;

    this->timePathTimeSpeed = NvMmClock_Speed();
    out.result = 0;
    if (!NvMmSchedule_RunScript(play, D_80BF3260, &out) ||
        ((this->scheduleResult != out.result) && !NvMmIg_SetupResult(this, play, &out))) {
        this->actor.shape.shadowDraw = NULL;
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        out.result = 0;
    } else {
        this->actor.shape.shadowDraw = ActorShadow_DrawCircle;
        this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    }
    this->lookAt = (out.result == 3) ? NvMmIg_FindAnju(this, play) : &GET_PLAYER(play)->actor;
    this->scheduleResult = out.result;
    switch (this->scheduleResult) {
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
            if (this->timePath != NULL) {
                NvMmIg_FollowTimePath(this, play);
            }
            break;
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
            NvMmIg_DoorWalk(this, play);
            break;
        case 2:
            NvMmIg_Stand(this, play);
            break;
        case 3:
            if ((this->lookAt != NULL) && (this->lookAt->update != NULL)) {
                this->actor.world.rot.y = Actor_WorldYawTowardActor(&this->actor, this->lookAt);
            }
            break;
        case 4:
            NvMmIg_Snore(this, play);
            break;
    }
    Math_ApproachS(&this->actor.shape.rot.y, this->actor.world.rot.y, 3, 0x2AA8);
}

void NvMmIg_Init(Actor* thisx, PlayState* play) {
    NvMmIg* this = (NvMmIg*)thisx;

    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 28.0f);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sSkel, NULL, this->jointTable, this->morphTable,
                       OBJECT_DAI_LIMB_MAX);
    this->animIndex = -1;
    NvMmIg_ChangeAnim(this, ENIG_ANIM_0);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    Actor_SetScale(&this->actor, 0.01f);
    this->actor.room = -1; // (MM: he walks between rooms)
    this->actor.gravity = 0.0f;
    this->func = NvMmIg_FollowSchedule;
    this->func(this, play);
}

void NvMmIg_Destroy(Actor* thisx, PlayState* play) {
    NvMmIg* this = (NvMmIg*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

void NvMmIg_Update(Actor* thisx, PlayState* play) {
    NvMmIg* this = (NvMmIg*)thisx;

    // MM's func_80BF19A0: his talk taken up
    if ((this->func == NvMmIg_FollowSchedule) && (this->flags & FLAG_OFFER) &&
        Actor_ProcessTalkRequest(&this->actor, play)) {
        this->flags &= ~FLAG_OFFER;
        this->actor.child = this->lookAt;
        this->msgScriptPos = 0;
        this->csStep = 0;
        this->func = NvMmIg_Talk;
    }
    this->func(this, play);
    if ((getenv("NV_LOG") != NULL) && ((play->gameplayFrames % 20) == 0)) { // (dev: where he is)
        printf("NVMM goron result=%d pos=%.0f,%.0f,%.0f\n", this->scheduleResult, this->actor.world.pos.x,
               this->actor.world.pos.y, this->actor.world.pos.z);
        fflush(stdout);
    }
    NvMmIg_ByLine(this, play);
    if (this->scheduleResult == 0) {
        return;
    }
    this->skelAnime.playSpeed = this->animPlaySpeed;
    SkelAnime_Update(&this->skelAnime);
    NvMmIg_Blink(this);
    if ((this->func == NvMmIg_FollowSchedule) && (this->flags & FLAG_OFFER) &&
        (this->actor.flags & ACTOR_FLAG_ATTENTION_ENABLED)) { // (MM's SubS_Offer, on screen)
        Actor_OfferTalkExchange(&this->actor, play, 60.0f, 30.0f, EXCH_ITEM_NONE);
    }
    if (this->actor.gravity != 0.0f) {
        Actor_MoveXZGravity(&this->actor);
    }
    Actor_UpdateBgCheckInfo(play, &this->actor, 30.0f, 12.0f, 0.0f, 4);
    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

static s32 NvMmIg_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                   void* thisx) {
    if (limbIndex == OBJECT_DAI_LIMB_0A) {
        *dList = NULL;
    }
    return false;
}

/** MM's EnIg_PostLimbDraw: his focus, the parts drawn on his limbs, his head's matrix. */
static void NvMmIg_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    static Vec3f sZero = { 0.0f, 0.0f, 0.0f };
    NvMmIg* this = (NvMmIg*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    if (limbIndex == OBJECT_DAI_LIMB_0B) {
        Matrix_MultVec3f(&sZero, &this->actor.focus.pos);
        this->actor.focus.rot = this->actor.world.rot;
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)DAI("object_dai_DL_008710"));
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)DAI("object_dai_DL_0087B8"));
    }
    if (limbIndex == OBJECT_DAI_LIMB_0C) {
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)DAI("object_dai_DL_0089D8"));
    }
    if (limbIndex == OBJECT_DAI_LIMB_09) {
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)DAI("object_dai_DL_008B00"));
    }
    if (limbIndex == OBJECT_DAI_LIMB_0A) {
        Matrix_Get(&this->headMtx);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

void NvMmIg_Draw(Actor* thisx, PlayState* play) {
    static Gfx sEmptyDL[] = { gsSPEndDisplayList(), gsSPEndDisplayList(), gsSPEndDisplayList(), gsSPEndDisplayList() }; // (MM's renderModeSetNoneDL: called at +0x10 too)
    NvMmIg* this = (NvMmIg*)thisx;

    if (this->scheduleResult == 0) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0C, sEmptyDL); // (MM's Scene_SetRenderModeXlu(play, 0, 1))
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sEyeTextures[this->eyeIndex % 5]));
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          NvMmIg_OverrideLimbDraw, NvMmIg_PostLimbDraw, &this->actor);
    Matrix_Put(&this->headMtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)DAI("object_dai_DL_00C538"));
    CLOSE_DISPS(play->state.gfxCtx);
}
