/*
 * Nightmareverse: Anju, from MM's En_An (zeldaret/mm, ovl_En_An), ported closely where she is so far:
 * outdoors (mm/README.md). Her model is MM's object_an1 (her face: eyes and mouth set in segments
 * 8 and 9 as in MM), with object_an2's umbrella and umbrella walk.
 *
 * Her schedule is MM's (mm/schedules/En_An.schl). Most of her days are inside the Stock Pot Inn,
 * not built yet; outdoors she goes out of the inn's front door (East Clock Town) at lunchtime on
 * Day 2 under her umbrella, through East and South Clock Town to the Laundry Pool, and back in the
 * afternoon (MM's results DOOR_37, WALKING_54/55/58/59 in East Clock Town, WALKING_60-63 in South,
 * DOOR_38 back in). Walking, she doesn't talk (as in MM: no offer while she walks there).
 *
 * Adapted from MM:
 * - the inn and the Laundry Pool aren't built: her results there come to nothing (MM's script
 *   checks the scene); her other objects (the tray, broom, masks) wait with those;
 * - MM's object slots for her other animations: here they're drawn by path, no slots;
 * - the Couples' Mask ending (after it she's gone from town) is kept: MM's week event 51_40;
 * - her head turn (MM's transform draw) is left out while walking, as MM's is then (she's lost
 *   attention: no head tracking).
 */
#include "global.h"
#include "z_nv_mm_schedule.h"
#include "z_nv_mm_subs.h"

#define ENAN_GET_PATH_INDEX(thisx) ((thisx)->params & 0xFF)
#define ENAN_8000 0x8000
#define ANJU1_LIMB_RIGHT_HAND 8
#define ANJU1_LIMB_MAX 21
#define MM_WE(byte, mask) (((byte) << 8) | (mask))
#define WEEKEVENTREG_COUPLES_MASK_CUTSCENE_FINISHED MM_WE(51, 0x40)

typedef enum {
    ANJU_SCH_NONE, ANJU_SCH_WAITING_FOR_KAFEI, ANJU_SCH_2, ANJU_SCH_LAUNDRY_POOL_SIT, ANJU_SCH_4, ANJU_SCH_5,
    ANJU_SCH_6, ANJU_SCH_7, ANJU_SCH_8, ANJU_SCH_9, ANJU_SCH_10, ANJU_SCH_11, ANJU_SCH_RANCH, ANJU_SCH_13,
    ANJU_SCH_COOKING, ANJU_SCH_15, ANJU_SCH_RECEIVE_LETTER_FROM_POSTMAN, ANJU_SCH_ATTEND_GORON,
    ANJU_SCH_RECEPTIONIST_IDLE, ANJU_SCH_WAITING_CLOSING_TIME, ANJU_SCH_20, ANJU_SCH_GIVE_LUNCH_TO_GRANNY,
    ANJU_SCH_SWEEPING, ANJU_SCH_MIDNIGHT_MEETING, ANJU_SCH_TALKING_WITH_MOM, ANJU_SCH_WITH_KAFEI, ANJU_SCH_DOOR_26,
    ANJU_SCH_DOOR_27, ANJU_SCH_DOOR_28, ANJU_SCH_DOOR_29, ANJU_SCH_DOOR_30, ANJU_SCH_DOOR_31, ANJU_SCH_DOOR_32,
    ANJU_SCH_DOOR_33, ANJU_SCH_DOOR_34, ANJU_SCH_DOOR_35, ANJU_SCH_DOOR_36, ANJU_SCH_DOOR_37, ANJU_SCH_DOOR_38,
    ANJU_SCH_DOOR_39, ANJU_SCH_WALKING_40, ANJU_SCH_41, ANJU_SCH_WALKING_42, ANJU_SCH_WALKING_43,
    ANJU_SCH_WALKING_44, ANJU_SCH_WALKING_45, ANJU_SCH_WALKING_46, ANJU_SCH_WALKING_47, ANJU_SCH_WALKING_48,
    ANJU_SCH_WALKING_49, ANJU_SCH_WALKING_50, ANJU_SCH_WALKING_51, ANJU_SCH_WALKING_52, ANJU_SCH_WALKING_53,
    ANJU_SCH_WALKING_54, ANJU_SCH_WALKING_55, ANJU_SCH_WALKING_56, ANJU_SCH_WALKING_57, ANJU_SCH_WALKING_58,
    ANJU_SCH_WALKING_59, ANJU_SCH_WALKING_60, ANJU_SCH_WALKING_61, ANJU_SCH_WALKING_62, ANJU_SCH_WALKING_63,
    ANJU_SCH_MAX
} AnjuScheduleResult;

#include "nv_mm_schedules/En_An.schl.inc"

// MM's sSearchTimePathLimit: how far along her path chain each result's path is
static s32 sSearchTimePathLimit[ANJU_SCH_MAX] = {
    -1, 10, -1, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  -1, 10, 10, 10, 11, -1, 4, 22, 15, -1, -1,
    3,  6,  8,  9, 13, 14, 16, 18, 19, 21, 23, 0,  5,  25, 0,  1,  2,  4,  5,  7,  10, 11, 12, 15, 17, 20,
    22, 24, 1,  2, 0,  1,  3,  4,  0,  1,  2,  3,
};

enum { ENAN_ANIM_IDLE, ENAN_ANIM_WALK, ENAN_ANIM_UMBRELLA_WALK, ENAN_ANIM_MAX };
static const char* sAnimations[ENAN_ANIM_MAX] = {
    "__OTR__mm/objects/object_an1/gAnju1IdleAnim",
    "__OTR__mm/objects/object_an1/gAnju1WalkAnim",
    "__OTR__mm/objects/object_an2/gAnju2UmbrellaWalkAnim",
};
static const char sSkel[] = "__OTR__mm/objects/object_an1/gAnju1Skel";
#define DL_UMBRELLA ((Gfx*)"__OTR__mm/objects/object_an2/gAnju2UmbrellaDL")

enum { EYES_OPEN, EYES_HALF1, EYES_CLOSED, EYES_HALF2, EYES_COMFORT, EYES_SAD, EYES_RELIEVED_CLOSED, EYES_MAX };
enum { MOUTH_CLOSED, MOUTH_HAPPY, MOUTH_OPEN, MOUTH_MAX };
#define AN1(name) ("__OTR__mm/objects/object_an1/" name)
static const char* sEyeTextures[EYES_MAX] = {
    AN1("gAnju1EyeOpenTex"), AN1("gAnju1EyeHalfTex"),    AN1("gAnju1EyeClosedTex"),       AN1("gAnju1EyeHalfTex"),
    AN1("gAnju1EyeComfortTex"), AN1("gAnju1EyeSadTex"), AN1("gAnju1EyeRelievedClosedTex"),
};
static const char* sMouthTextures[MOUTH_MAX] = { AN1("gAnju1MouthClosedTex"), AN1("gAnju1MouthHappyTex"),
                                                 AN1("gAnju1MouthOpenTex") };

#define STATE_UPDATE_EYES 1
#define STATE_DRAW_UMBRELLA 2
#define STATE_REACHED_PATH_END 4
#define STATE_8 8

struct NvMmAn;
typedef void (*NvMmAnFunc)(struct NvMmAn*, PlayState*);

typedef struct NvMmAn {
    Actor actor;
    SkelAnime skelAnime;
    NvMmAnFunc func;
    ColliderCylinder collider;
    Path* timePath;
    Vec3f timePathTargetPos;
    f32 timePathProgress;
    s32 timePathTotalTime;
    s32 timePathWaypointTime;
    s32 timePathWaypoint;
    s32 timePathElapsedTime;
    s16 timePathTimeSpeed;
    u8 scheduleResult;
    Vec3f doorEntrancePos;
    Vec3f doorExitPos;
    s8 doorOpenTimer;
    s16 doorTimeTotalDiff;
    s16 doorTimeProgress;
    Vec3s jointTable[ANJU1_LIMB_MAX];
    Vec3s morphTable[ANJU1_LIMB_MAX];
    s32 animIndex;
    f32 animPlaySpeed;
    u16 stateFlags;
    s16 faceIndex;
    s16 eyeTexIndex;
    s16 mouthTexIndex;
    s16 eyeTimer;
} NvMmAn;

const size_t gNvMmAnSize = sizeof(NvMmAn);

s32 NvMm_CheckWeekEvent(s32 flag);
s32 NvMmClock_Speed(void);
s32 NvMmActor_Id(s32 mmId);
s32 NvMmDoor_ScheduleType(Actor* thisx);
void NvMmDoor_SetOpenTimer(Actor* thisx, s8 frames);

static ColliderCylinderInit sCylinderInit = {
    { COLTYPE_HIT1, AT_NONE, AC_NONE, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK1, { 0x00000000, 0x00, 0x00 }, { 0x00000000, 0x00, 0x00 }, TOUCH_NONE, BUMP_NONE, OCELEM_ON },
    { 14, 62, 0, { 0, 0, 0 } },
};

static void NvMmAn_ChangeAnim(NvMmAn* this, s32 animIndex) {
    if (this->animIndex != animIndex) {
        AnimationHeader* anim = (AnimationHeader*)sAnimations[animIndex];

        this->animIndex = animIndex;
        Animation_Change(&this->skelAnime, anim, 1.0f, 0.0f, Animation_GetLastFrame(anim), ANIMMODE_LOOP, 0.0f);
        this->animPlaySpeed = this->skelAnime.playSpeed;
    }
}

/** MM's EnAn_UpdateFace: blinking, and the mouth for her face. */
static void NvMmAn_UpdateFace(NvMmAn* this) {
    static s16 sMouthIndices[10] = { MOUTH_CLOSED, MOUTH_OPEN, MOUTH_CLOSED, MOUTH_HAPPY, MOUTH_OPEN,
                                     MOUTH_CLOSED, MOUTH_HAPPY, MOUTH_OPEN, MOUTH_HAPPY, MOUTH_HAPPY };
    s32 skipBlink = false;

    if ((this->stateFlags & STATE_UPDATE_EYES) && (DECR(this->eyeTimer) == 0)) {
        switch (this->faceIndex) {
            case 2:
            case 4:
            case 6:
                if ((this->eyeTexIndex == EYES_SAD) || (this->eyeTexIndex == EYES_CLOSED)) {
                    skipBlink = true;
                    this->eyeTexIndex = EYES_SAD;
                }
                break;
            case 5:
            case 7:
                if ((this->eyeTexIndex == EYES_RELIEVED_CLOSED) || (this->eyeTexIndex == EYES_HALF1)) {
                    skipBlink = true;
                    this->eyeTexIndex = EYES_RELIEVED_CLOSED;
                }
                break;
            case 8:
                if ((this->eyeTexIndex == EYES_COMFORT) || (this->eyeTexIndex == EYES_CLOSED)) {
                    skipBlink = true;
                    this->eyeTexIndex = EYES_COMFORT;
                }
                break;
        }
        if (!skipBlink) {
            if ((this->eyeTexIndex == EYES_COMFORT) || (this->eyeTexIndex == EYES_SAD)) {
                this->eyeTexIndex = EYES_OPEN;
            } else if (this->eyeTexIndex == EYES_RELIEVED_CLOSED) {
                this->eyeTexIndex = EYES_HALF1;
            }
            this->eyeTexIndex++;
            if (this->eyeTexIndex > EYES_HALF2) {
                this->eyeTimer = ((this->faceIndex == 0) || (this->faceIndex == 1) || (this->faceIndex == 3))
                                     ? Rand_S16Offset(30, 30)
                                     : 8;
                this->eyeTexIndex = EYES_OPEN;
            }
        }
    }
    this->mouthTexIndex = sMouthIndices[this->faceIndex % 10];
}

static Path* NvMmAn_Path(NvMmAn* this, PlayState* play, s32 result) {
    s32 limit = ((result >= 0) && (result < ANJU_SCH_MAX)) ? sSearchTimePathLimit[result] : -1;

    return (limit >= 0) ? NvMmSubS_GetAdditionalPath(play, ENAN_GET_PATH_INDEX(&this->actor), limit) : NULL;
}

/** MM's EnAn_FindScheduleDoor, the outdoor part: the inn's front door (37, 38). */
static Actor* NvMmAn_FindScheduleDoor(PlayState* play, s32 result) {
    s32 doorId = NvMmActor_Id(0x005);
    Actor* a;

    if ((result != ANJU_SCH_DOOR_37) && (result != ANJU_SCH_DOOR_38)) {
        return NULL; // (the inn's inside doors: not built)
    }
    for (a = play->actorCtx.actorLists[ACTORCAT_DOOR].head; a != NULL; a = a->next) {
        if ((a->id == doorId) && (a->update != NULL) && (NvMmDoor_ScheduleType(a) == 11)) { // INN_MAIN_ENTRANCE
            return a;
        }
    }
    return NULL;
}

/** MM's EnAn_ProcessSchedule_Door. */
static s32 NvMmAn_SetupDoor(NvMmAn* this, PlayState* play, NvMmScheduleOutput* out) {
    u16 now = NvMmSchedule_Now();
    Actor* door = NvMmAn_FindScheduleDoor(play, out->result);
    Vec3s* points;

    this->timePath = NvMmAn_Path(this, play, out->result);
    if ((door == NULL) || (this->timePath == NULL) || (this->timePath->count < 2)) {
        return false;
    }
    points = SEGMENTED_TO_VIRTUAL(this->timePath->points);
    this->doorEntrancePos = (Vec3f){ points[0].x, points[0].y, points[0].z };
    this->doorExitPos = (Vec3f){ points[1].x, points[1].y, points[1].z };
    this->actor.world.rot.y = Math_Vec3f_Yaw(&this->doorEntrancePos, &this->doorExitPos);
    this->actor.world.pos = this->doorEntrancePos;
    this->doorOpenTimer = (ABS((s16)(this->actor.world.rot.y - door->shape.rot.y)) <= 0x4000) ? -75 : 75;
    this->doorTimeTotalDiff = out->time1 - out->time0;
    this->doorTimeProgress = now - out->time0;
    this->faceIndex = 2;
    this->eyeTimer = 8;
    this->stateFlags |= STATE_UPDATE_EYES;
    NvMmAn_ChangeAnim(this, ENAN_ANIM_WALK);
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->actor.gravity = 0.0f;
    return true;
}

/** MM's EnAn_ProcessSchedule_Walking, the outdoor results (her umbrella walk). */
static s32 NvMmAn_SetupWalking(NvMmAn* this, PlayState* play, NvMmScheduleOutput* out) {
    u16 now = NvMmSchedule_Now();
    u16 startTime;

    this->timePath = NvMmAn_Path(this, play, out->result);
    if ((this->timePath != NULL) && (this->timePath->count < 3)) {
        this->timePath = NULL;
    }
    if (this->timePath == NULL) {
        return false;
    }
    if ((this->scheduleResult < ANJU_SCH_WALKING_40) && (this->scheduleResult != ANJU_SCH_NONE) &&
        (this->timePathTimeSpeed >= 0)) {
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
    this->stateFlags &= ~(STATE_8 | STATE_REACHED_PATH_END);
    this->faceIndex = 2;
    this->eyeTimer = 8;
    NvMmAn_ChangeAnim(this, ENAN_ANIM_UMBRELLA_WALK);
    this->stateFlags |= STATE_UPDATE_EYES | STATE_DRAW_UMBRELLA;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED; // (MM: no talk offer while she walks out here)
    this->actor.gravity = -1.0f;
    return true;
}

/** MM's EnAn_ProcessScheduleOutput, the outdoor part. */
static s32 NvMmAn_SetupResult(NvMmAn* this, PlayState* play, NvMmScheduleOutput* out) {
    this->stateFlags = 0;
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    switch (out->result) {
        case ANJU_SCH_DOOR_37:
        case ANJU_SCH_DOOR_38:
            return NvMmAn_SetupDoor(this, play, out);
        default:
            if ((out->result >= ANJU_SCH_WALKING_54) && (out->result <= ANJU_SCH_WALKING_63)) {
                return NvMmAn_SetupWalking(this, play, out);
            }
            return false; // (the inn, the Laundry Pool, the ranch: not built)
    }
}

/** MM's EnAn_HandleSch_Door: through the inn's door, it held open for her. */
static void NvMmAn_DoorWalk(NvMmAn* this, PlayState* play) {
    Actor* door = NvMmAn_FindScheduleDoor(play, this->scheduleResult);
    f32 step;
    f32 dist;

    if (Player_InCsMode(play) || (this->timePathTimeSpeed == 0) || (this->doorTimeTotalDiff <= 0)) {
        return;
    }
    if (door != NULL) {
        NvMmDoor_SetOpenTimer(door, (((f32)this->doorTimeProgress / this->doorTimeTotalDiff) <= 0.9f)
                                        ? this->doorOpenTimer
                                        : 0);
    }
    this->doorTimeProgress = CLAMP(this->doorTimeProgress, 0, this->doorTimeTotalDiff);
    step = Math_Vec3f_DistXZ(&this->doorEntrancePos, &this->doorExitPos) / this->doorTimeTotalDiff;
    dist = this->doorTimeProgress * step;
    this->actor.world.pos.x = this->doorEntrancePos.x + Math_SinS(this->actor.world.rot.y) * dist;
    this->actor.world.pos.y = this->doorEntrancePos.y;
    this->actor.world.pos.z = this->doorEntrancePos.z + Math_CosS(this->actor.world.rot.y) * dist;
    this->doorTimeProgress += this->timePathTimeSpeed;
}

/** MM's EnAn_HandleSch_FollowTimePath. */
static void NvMmAn_FollowTimePath(NvMmAn* this, PlayState* play) {
    static f32 knots[265];
    Vec3f target;

    NvMmSubS_TimePathing_FillKnots(knots, NV_MM_TIME_PATHING_ORDER, this->timePath->count + NV_MM_TIME_PATHING_ORDER);
    if (!(this->stateFlags & STATE_8)) {
        target = (Vec3f){ 0.0f, 0.0f, 0.0f };
        NvMmSubS_TimePathing_Update(this->timePath, &this->timePathProgress, &this->timePathElapsedTime,
                                    this->timePathWaypointTime, this->timePathTotalTime, &this->timePathWaypoint,
                                    knots, &target, this->timePathTimeSpeed);
        NvMmSubS_TimePathing_ComputeInitialY(play, this->timePath, this->timePathWaypoint, &target);
        this->actor.world.pos.y = target.y;
        this->stateFlags |= STATE_8;
    } else {
        target = this->timePathTargetPos;
    }
    this->actor.world.pos.x = target.x;
    this->actor.world.pos.z = target.z;
    this->timePathTargetPos = (Vec3f){ 0.0f, 0.0f, 0.0f };
    if (NvMmSubS_TimePathing_Update(this->timePath, &this->timePathProgress, &this->timePathElapsedTime,
                                    this->timePathWaypointTime, this->timePathTotalTime, &this->timePathWaypoint,
                                    knots, &this->timePathTargetPos, this->timePathTimeSpeed)) {
        this->stateFlags |= STATE_REACHED_PATH_END;
    } else {
        this->actor.world.rot.y = Math_Vec3f_Yaw(&this->actor.world.pos, &this->timePathTargetPos);
    }
}

/** MM's EnAn_FollowSchedule. */
static void NvMmAn_FollowSchedule(NvMmAn* this, PlayState* play) {
    NvMmScheduleOutput out;

    this->timePathTimeSpeed = NvMmClock_Speed();
    if (!(this->actor.params & ENAN_8000) && NvMm_CheckWeekEvent(WEEKEVENTREG_COUPLES_MASK_CUTSCENE_FINISHED)) {
        Actor_Kill(&this->actor);
        return;
    }
    out.result = ANJU_SCH_NONE;
    if (!NvMmSchedule_RunScript(play, sScheduleScript, &out) ||
        ((this->scheduleResult != out.result) && !NvMmAn_SetupResult(this, play, &out))) {
        this->actor.shape.shadowDraw = NULL;
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        out.result = ANJU_SCH_NONE;
    } else {
        this->actor.shape.shadowDraw = ActorShadow_DrawCircle;
    }
    this->scheduleResult = out.result;
    if ((this->scheduleResult == ANJU_SCH_DOOR_37) || (this->scheduleResult == ANJU_SCH_DOOR_38)) {
        NvMmAn_DoorWalk(this, play);
    } else if (this->scheduleResult != ANJU_SCH_NONE) {
        NvMmAn_FollowTimePath(this, play);
    }
    Math_ApproachS(&this->actor.shape.rot.y, this->actor.world.rot.y, 3, 0x2AA8);
}

void NvMmAn_Init(Actor* thisx, PlayState* play) {
    NvMmAn* this = (NvMmAn*)thisx;

    // (MM: ENAN_8000 marks the inn's Anju after the Couples' Mask; out here it's cleared)
    this->actor.params &= (s16)~ENAN_8000;
    if (NvMm_CheckWeekEvent(WEEKEVENTREG_COUPLES_MASK_CUTSCENE_FINISHED)) {
        Actor_Kill(&this->actor);
        return;
    }
    ActorShape_Init(&this->actor.shape, 0.0f, NULL, 14.0f);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sSkel, NULL, this->jointTable, this->morphTable,
                       ANJU1_LIMB_MAX);
    this->animIndex = -1;
    NvMmAn_ChangeAnim(this, ENAN_ANIM_IDLE);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    Actor_SetScale(&this->actor, 0.01f);
    this->actor.room = -1; // (MM: she walks between rooms)
    this->func = NvMmAn_FollowSchedule;
    this->func(this, play);
}

void NvMmAn_Destroy(Actor* thisx, PlayState* play) {
    NvMmAn* this = (NvMmAn*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

void NvMmAn_Update(Actor* thisx, PlayState* play) {
    NvMmAn* this = (NvMmAn*)thisx;

    this->func(this, play);
    if (this->scheduleResult == ANJU_SCH_NONE) {
        return;
    }
    this->skelAnime.playSpeed = this->animPlaySpeed;
    SkelAnime_Update(&this->skelAnime);
    NvMmAn_UpdateFace(this);
    if (this->actor.gravity != 0.0f) {
        Actor_MoveXZGravity(&this->actor);
        Actor_UpdateBgCheckInfo(play, &this->actor, 30.0f, 12.0f, 0.0f, 4);
    }
    Actor_SetFocus(&this->actor, 60.0f);
    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

static void NvMmAn_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    NvMmAn* this = (NvMmAn*)thisx;

    if ((limbIndex == ANJU1_LIMB_RIGHT_HAND) && (this->stateFlags & STATE_DRAW_UMBRELLA)) {
        OPEN_DISPS(play->state.gfxCtx);
        gSPDisplayList(POLY_OPA_DISP++, DL_UMBRELLA);
        CLOSE_DISPS(play->state.gfxCtx);
    }
}

void NvMmAn_Draw(Actor* thisx, PlayState* play) {
    NvMmAn* this = (NvMmAn*)thisx;

    if (this->scheduleResult == ANJU_SCH_NONE) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sEyeTextures[this->eyeTexIndex % EYES_MAX]));
    gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(sMouthTextures[this->mouthTexIndex % MOUTH_MAX]));
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          NULL, NvMmAn_PostLimbDraw, &this->actor);
    CLOSE_DISPS(play->state.gfxCtx);
}
