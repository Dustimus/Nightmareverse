/*
 * Nightmareverse: Majora's Mask's Postman, from MM's En_Pm (zeldaret/mm, ovl_En_Pm), ported closely
 * where he is so far: his rounds through Clock Town (mm/README.md). His model is MM's object_mm.
 *
 * As in MM he's placed in every Clock Town scene (params & 0xFF: his first path there) and his
 * schedule (mm/schedules/En_Pm.schl: five scripts, one per stage of his day's story) says where he
 * is: walking a path between two times (results 38-91: MM's time pathing, z_nv_mm_subs.c),
 * going through a door (1, 2, 10-13, 32-37: he swings it open, our ported door's openTimer), or
 * standing at a mailbox (3-7, 19); nothing, and he isn't here. He talks while about.
 *
 * Adapted from MM:
 * - indoors (the Post Office, the inn, the Milk Bar, the mayor's) isn't built: those results
 *   need people there (Anju, the mayor...) and come to nothing here, as in MM without them;
 * - checking a mailbox (z_nv_mm_pst.c) sets MM's week events, the Letter to Kafei ones too;
 * - talking: MM's message scripts for him outdoors, run by our port of MM's (z_nv_mm_msgevent.c);
 *   the Kafei's Mask line waits for MM's masks;
 * - his footsteps are MM's own sound (not in OoT's banks); his head turns to Link by OoT's
 *   limb override, where MM uses its own transform draw (SubS_DrawTransformFlex);
 * - MM's cutscene checks (SubS_InCsMode) are OoT's Player_InCsMode.
 */
#include "global.h"
#include "z_nv_mm_schedule.h"
#include "z_nv_mm_subs.h"
#include "z_nv_mm_msgevent.h"
#include <stdio.h>
#include <stdlib.h>

#define ENPM_GET_PATH_INDEX(thisx) ((thisx)->params & 0xFF)
#define OBJECT_MM_LIMB_08 8
#define OBJECT_MM_LIMB_0B 0x0B
#define OBJECT_MM_LIMB_0F 0x0F
#define OBJECT_MM_LIMB_MAX 0x10
#define NV_MM_TEXT(id) (0xB000 + (id))
#define MM_WE(byte, mask) (((byte) << 8) | (mask))

#define MM_OBJ(name) ("__OTR__mm/objects/object_mm/" name)
static const char sSkel[] = MM_OBJ("object_mm_Skel_0096E8");

enum {
    ENPM_ANIM_0, ENPM_ANIM_1, ENPM_ANIM_2, ENPM_ANIM_3, ENPM_ANIM_4, ENPM_ANIM_5, ENPM_ANIM_6, ENPM_ANIM_7,
    ENPM_ANIM_8, ENPM_ANIM_9, ENPM_ANIM_10, ENPM_ANIM_11, ENPM_ANIM_12, ENPM_ANIM_13, ENPM_ANIM_MAX
};

typedef struct {
    const char* anim;
    u8 mode;
    s8 morph;
} NvMmPmAnim;

// MM's sAnimationInfo (speed 1, the whole animation)
static NvMmPmAnim sAnimationInfo[ENPM_ANIM_MAX] = {
    { MM_OBJ("object_mm_Anim_002238"), ANIMMODE_LOOP, 0 },  { MM_OBJ("object_mm_Anim_002238"), ANIMMODE_LOOP, -4 },
    { MM_OBJ("object_mm_Anim_00A4E0"), ANIMMODE_ONCE, 0 },  { MM_OBJ("object_mm_Anim_00B09C"), ANIMMODE_LOOP, 0 },
    { MM_OBJ("object_mm_Anim_00B09C"), ANIMMODE_LOOP, -4 }, { MM_OBJ("object_mm_Anim_00BA78"), ANIMMODE_ONCE, 0 },
    { MM_OBJ("object_mm_Anim_00C32C"), ANIMMODE_LOOP, -4 }, { MM_OBJ("object_mm_Anim_0099B4"), ANIMMODE_LOOP, 0 },
    { MM_OBJ("object_mm_Anim_000FC4"), ANIMMODE_LOOP, 0 },  { MM_OBJ("object_mm_Anim_00A8D8"), ANIMMODE_LOOP, 0 },
    { MM_OBJ("object_mm_Anim_00099C"), ANIMMODE_LOOP, 0 },  { MM_OBJ("object_mm_Anim_001F84"), ANIMMODE_ONCE, 0 },
    { MM_OBJ("object_mm_Anim_000468"), ANIMMODE_LOOP, 0 },  { MM_OBJ("object_mm_Anim_00C640"), ANIMMODE_LOOP, 0 },
};

#define DL_LETTER ((Gfx*)MM_OBJ("object_mm_DL_008348"))
#define DL_BAG ((Gfx*)MM_OBJ("object_mm_DL_0083E0"))
#define DL_CAP ((Gfx*)MM_OBJ("object_mm_DL_0085C8"))
static const char sEyeTex[] = MM_OBJ("object_mm_Tex_002950");

// the path along his chain for each schedule result (MM's D_80AFB430)
static s32 sPathIndices[] = {
    -1, 0, 4, 1,  0,  0, 1, 4,  -1, -1, 15, 5, 0, 3, -1, -1, 1, 2, 11, 3, -1, -1, -1, 0, 0, 0, 0, 0, 1,  12, -1,
    -1, 2, 0, 10, 11, 0, 3, 1,  0,  1,  2,  0, 1, 0, 1,  2,  3, 4, 3,  5, 14, 3,  1,  0, 6, 4, 1, 2, 6,  7,  8,
    2,  1, 0, 6,  7,  9, 8, -1, 0,  1,  10, 3, 1, 0, 11, 12, 8, 2, 2,  3, 0,  1,  1,  0, 6, 9, 1, 2, 12, 13,
};

#include "nv_mm_schedules/En_Pm.schl.inc"
static u8* sSchedules[] = { D_80AFAD80, D_80AFB30C, D_80AFB3C0, D_80AFB3FC, D_80AFB41C };

#define WEEKEVENTREG_86_01 MM_WE(86, 0x01)
#define WEEKEVENTREG_89_08 MM_WE(89, 0x08)
#define WEEKEVENTREG_89_40 MM_WE(89, 0x40)
#define WEEKEVENTREG_90_01 MM_WE(90, 0x01)
#define WEEKEVENTREG_90_04 MM_WE(90, 0x04)
#define WEEKEVENTREG_88_02 MM_WE(88, 0x02)
#define WEEKEVENTREG_60_04 MM_WE(60, 0x04)
#define WEEKEVENTREG_POSTMAN_RECEIVED_LETTER_TO_KAFEI MM_WE(90, 0x08)
#define WEEKEVENTREG_LETTER_TO_KAFEI_SCHEDULED_DAY_2 MM_WE(28, 0x08)
#define WEEKEVENTREG_LETTER_TO_KAFEI_SCHEDULED_DAY_3 MM_WE(28, 0x10)
#define WEEKEVENTREG_LETTER_TO_KAFEI_SCHEDULED_TODAY MM_WE(51, 0x02)

s32 NvMm_CheckWeekEvent(s32 flag);
void NvMm_SetWeekEvent(s32 flag);
s32 NvMmClock_Day(void);
u16 NvMmClock_Time(void);
s32 NvMmClock_Speed(void);
s32 NvMmActor_Id(s32 mmId);

struct NvMmPm;
typedef void (*NvMmPmFunc)(struct NvMmPm*, PlayState*);

typedef struct NvMmPm {
    Actor actor;
    SkelAnime skelAnime;
    NvMmPmFunc func;
    ColliderCylinder collider;
    Path* timePath;
    Vec3f timePathTargetPos;
    f32 timePathProgress;
    s32 timePathTotalTime;
    s32 timePathWaypointTime;
    s32 timePathWaypoint;
    s32 timePathElapsedTime;
    u8 scheduleResult;
    s8 doorOpenTimer;   // (MM's unk_260: how a door is opened for him, and which way)
    Actor* lookAt;      // (MM's unk_268)
    Vec3f doorPathStart; // (MM's unk_26C, unk_278)
    Vec3f doorPathEnd;
    Vec3s jointTable[OBJECT_MM_LIMB_MAX];
    Vec3s morphTable[OBJECT_MM_LIMB_MAX];
    u16 flags; // (MM's unk_356)
    f32 animPlaySpeed;
    s16 headRotX; // (MM's unk_370, unk_372)
    s16 headRotY;
    s16 doorTime; // (MM's unk_36C, unk_36E: a door walk's length and how far in)
    s16 doorElapsed;
    s16 timePathTimeSpeed;
    s16 lookTimer;
    f32 talkRange;
    s32 animIndex;
    s32 stage;    // (MM's unk_38C: which of his five schedules)
    MsgScript* msgScript;
    s32 msgScriptPos;
    u16 stageTimeOffset;
} NvMmPm;

const size_t gNvMmPmSize = sizeof(NvMmPm);

static u16 sStageTime; // (MM's D_801F4E78)

static ColliderCylinderInit sCylinderInit = {
    { COLTYPE_HIT1, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK1, { 0x00000000, 0x00, 0x00 }, { 0x00000000, 0x00, 0x00 }, TOUCH_NONE, BUMP_NONE, OCELEM_ON },
    { 14, 62, 0, { 0, 0, 0 } },
};

static void NvMmPm_Schedule(NvMmPm* this, PlayState* play);
static void NvMmPm_Talk(NvMmPm* this, PlayState* play);

static s32 NvMmPm_Stage(void) {
    if (NvMm_CheckWeekEvent(WEEKEVENTREG_90_01)) {
        return 4;
    }
    if (NvMm_CheckWeekEvent(WEEKEVENTREG_89_40)) {
        return 3;
    }
    if (NvMm_CheckWeekEvent(WEEKEVENTREG_89_08)) {
        return 2;
    }
    if (NvMm_CheckWeekEvent(WEEKEVENTREG_86_01)) {
        return 1;
    }
    return 0;
}

/** MM's func_80AF7BAC: on to the next of his schedules as his story moves on. */
static void NvMmPm_UpdateStage(NvMmPm* this) {
    static const s32 sNext[] = { WEEKEVENTREG_86_01, WEEKEVENTREG_89_08, WEEKEVENTREG_89_40, WEEKEVENTREG_90_01 };

    if ((this->stage < 4) && NvMm_CheckWeekEvent(sNext[this->stage])) {
        sStageTime = (this->stage == 2) ? 0 : NvMmClock_Time();
        this->stage++;
    }
}

static s32 NvMmPm_ChangeAnim(NvMmPm* this, s32 animIndex) {
    s32 change = false;

    switch (animIndex) {
        case ENPM_ANIM_0:
        case ENPM_ANIM_1:
            change = (this->animIndex != ENPM_ANIM_0) && (this->animIndex != ENPM_ANIM_1);
            break;
        case ENPM_ANIM_3:
        case ENPM_ANIM_4:
            change = (this->animIndex != ENPM_ANIM_3) && (this->animIndex != ENPM_ANIM_4);
            break;
        default:
            change = this->animIndex != animIndex;
            break;
    }
    if (change) {
        NvMmPmAnim* info = &sAnimationInfo[animIndex];

        this->animIndex = animIndex;
        Animation_Change(&this->skelAnime, (AnimationHeader*)info->anim, 1.0f, 0.0f,
                         Animation_GetLastFrame((AnimationHeader*)info->anim), info->mode, info->morph);
        this->animPlaySpeed = this->skelAnime.playSpeed;
    }
    return change;
}

s32 NvMmDoor_ScheduleType(Actor* thisx);
void NvMmDoor_SetOpenTimer(Actor* thisx, s8 frames);

/** MM's EnPm_FindScheduleDoor: the door his walk goes through (the Post Office's, the Milk Bar's,
 * the inn's), by its schedule type (MM's SubS_FindScheduleDoor). */
static Actor* NvMmPm_FindScheduleDoor(PlayState* play, s32 result) {
    s32 schType;
    s32 doorId = NvMmActor_Id(0x005);
    Actor* a;

    switch (result) {
        case 1:
        case 2:
        case 32:
        case 33:
            schType = 1; // ENDOOR_SCH_TYPE_POST_OFFICE
            break;
        case 34:
        case 35:
        case 36:
        case 37:
            schType = 10; // ENDOOR_SCH_TYPE_MILK_BAR
            break;
        case 10:
        case 11:
        case 12:
        case 13:
            schType = 11; // ENDOOR_SCH_TYPE_INN_MAIN_ENTRANCE
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

static Path* NvMmPm_Path(NvMmPm* this, PlayState* play, s32 result) {
    s32 n = ((result >= 0) && (result < ARRAY_COUNT(sPathIndices))) ? sPathIndices[result] : -1;

    return (n >= 0) ? NvMmSubS_GetAdditionalPath(play, ENPM_GET_PATH_INDEX(&this->actor), n) : NULL;
}

/** MM's func_80AF9008: through a door, along the path's first two points, between two times. */
static s32 NvMmPm_SetupDoorWalk(NvMmPm* this, PlayState* play, NvMmScheduleOutput* out) {
    u16 now = NvMmSchedule_Now();
    Actor* door = NvMmPm_FindScheduleDoor(play, out->result);
    Vec3s* points;
    s16 diff;

    this->timePath = NvMmPm_Path(this, play, out->result);
    if ((door == NULL) || (this->timePath == NULL) || (this->timePath->count < 2)) {
        return false;
    }
    points = SEGMENTED_TO_VIRTUAL(this->timePath->points);
    this->doorPathStart = (Vec3f){ points[0].x, points[0].y, points[0].z };
    this->doorPathEnd = (Vec3f){ points[1].x, points[1].y, points[1].z };
    this->actor.world.rot.y = Math_Vec3f_Yaw(&this->doorPathStart, &this->doorPathEnd);
    this->actor.world.pos = this->doorPathStart;
    diff = this->actor.world.rot.y - door->shape.rot.y;
    this->doorOpenTimer = (ABS(diff) <= 0x4000) ? -0x4B : 0x4B;
    this->doorTime = out->time1 - out->time0;
    this->doorElapsed = now - out->time0;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    if (NvMm_CheckWeekEvent(WEEKEVENTREG_POSTMAN_RECEIVED_LETTER_TO_KAFEI)) {
        this->flags |= 0x800;
    }
    this->flags |= 0x9000 | 0x200;
    NvMmPm_ChangeAnim(this, ENPM_ANIM_0);
    this->actor.gravity = 0.0f;
    return true;
}

/** MM's func_80AF91E8: a path walked between two times (time pathing). */
static s32 NvMmPm_SetupTimePath(NvMmPm* this, PlayState* play, NvMmScheduleOutput* out) {
    u16 now = NvMmSchedule_Now();
    u16 start;
    u16 n;

    this->timePath = NvMmPm_Path(this, play, out->result);
    if ((this->timePath != NULL) && (this->timePath->count < 3)) {
        this->timePath = NULL;
    }
    if (this->timePath == NULL) {
        return false;
    }
    if ((this->scheduleResult < 38) && (this->scheduleResult != 0) && (this->timePathTimeSpeed >= 0)) {
        start = now;
    } else {
        start = out->time0;
    }
    if (out->time1 < start) {
        this->timePathTotalTime = (start - out->time1) + 0xFFFF;
    } else {
        this->timePathTotalTime = out->time1 - start;
    }
    this->timePathElapsedTime = now - start;
    n = this->timePath->count - (NV_MM_TIME_PATHING_ORDER - 1);
    this->timePathWaypointTime = this->timePathTotalTime / n;
    if (this->timePathWaypointTime <= 0) {
        this->timePathWaypointTime = 1;
    }
    this->timePathWaypoint = (this->timePathElapsedTime / this->timePathWaypointTime) + (NV_MM_TIME_PATHING_ORDER - 1);
    this->flags &= ~(8 | 0x10);
    switch (out->result) {
        case 83:
        case 84:
        case 85:
        case 86:
        case 87:
        case 88:
        case 89:
        case 90:
            this->flags |= 0x9000 | 0x200;
            // fallthrough
        case 82:
            NvMmPm_ChangeAnim(this, ENPM_ANIM_0);
            break;
        case 91:
            this->flags |= 0x9000;
            NvMmPm_ChangeAnim(this, ENPM_ANIM_12);
            break;
        default:
            NvMmPm_ChangeAnim(this, ENPM_ANIM_0);
            if (NvMm_CheckWeekEvent(WEEKEVENTREG_POSTMAN_RECEIVED_LETTER_TO_KAFEI)) {
                this->flags |= 0x800;
            }
            this->flags |= 0x9000 | 0x200;
            break;
    }
    this->actor.gravity = -1.0f;
    return true;
}

/** MM's func_80AF7DC4: the mailbox `box` in this scene (MM's En_Pst, params the box). */
static Actor* NvMmPm_FindMailbox(PlayState* play, s32 box) {
    s32 id = NvMmActor_Id(0x1F2);
    Actor* a;

    for (a = play->actorCtx.actorLists[ACTORCAT_PROP].head; a != NULL; a = a->next) {
        if ((a->id == id) && (a->update != NULL) && (a->params == box)) {
            return a;
        }
    }
    return NULL;
}

/** MM's func_80AF95E8, the outdoor part: standing at the path's end (at a mailbox, if it's there). */
static s32 NvMmPm_SetupStand(NvMmPm* this, PlayState* play, NvMmScheduleOutput* out) {
    Vec3s* points;
    Vec3f end;
    Vec3f before;
    s32 box = ((out->result >= 3) && (out->result <= 7)) ? (out->result - 3) : (out->result == 19) ? 4 : -1;

    if ((box >= 0) && (NvMmPm_FindMailbox(play, box) == NULL)) {
        return false;
    }

    this->timePath = NvMmPm_Path(this, play, out->result);
    if ((this->timePath == NULL) || (this->timePath->count < 2)) {
        return false;
    }
    points = SEGMENTED_TO_VIRTUAL(this->timePath->points);
    end = (Vec3f){ points[this->timePath->count - 1].x, points[this->timePath->count - 1].y,
                   points[this->timePath->count - 1].z };
    before = (Vec3f){ points[this->timePath->count - 2].x, points[this->timePath->count - 2].y,
                      points[this->timePath->count - 2].z };
    this->actor.world.rot.y = Math_Vec3f_Yaw(&before, &end);
    this->actor.shape.rot = this->actor.world.rot;
    this->actor.world.pos = end;
    this->actor.prevPos = end;
    switch (out->result) {
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 19:
            this->flags |= 0x9000;
            NvMmPm_ChangeAnim(this, ENPM_ANIM_11);
            break;
        case 18:
            this->flags |= 0x9000 | 0x800;
            NvMmPm_ChangeAnim(this, ENPM_ANIM_5);
            break;
        case 23:
            if (NvMm_CheckWeekEvent(WEEKEVENTREG_POSTMAN_RECEIVED_LETTER_TO_KAFEI)) {
                this->flags |= 0x800;
            }
            NvMm_SetWeekEvent(WEEKEVENTREG_60_04);
            // fallthrough
        default:
            this->flags |= 0x9000;
            NvMmPm_ChangeAnim(this, ENPM_ANIM_3);
            break;
    }
    return true;
}

/** MM's func_80AF9BF8: set up for a new schedule result; false if it can't be (he isn't here). */
static s32 NvMmPm_SetupResult(NvMmPm* this, PlayState* play, NvMmScheduleOutput* out) {
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    this->flags = 0;
    this->talkRange = 40.0f;
    switch (out->result) {
        case 1:
        case 2:
        case 10:
        case 11:
        case 12:
        case 13:
        case 32:
        case 33:
        case 34:
        case 35:
        case 36:
        case 37:
            return NvMmPm_SetupDoorWalk(this, play, out);
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 18:
        case 19:
        case 23:
        case 25:
            return NvMmPm_SetupStand(this, play, out);
        default:
            if ((out->result >= 38) && (out->result <= 91)) {
                return NvMmPm_SetupTimePath(this, play, out);
            }
            return false; // (indoors, with people not here yet: MM's 8, 9, 14, 16, 17, 20-22, 24, 27-29)
    }
}

/** MM's func_80AF8BA8: a mailbox checked (and a Letter to Kafei in it picked up). */
static void NvMmPm_CheckMailbox(s32 box) {
    static const u16 sDeposited[] = { MM_WE(27, 0x02), MM_WE(27, 0x04), MM_WE(27, 0x08), MM_WE(27, 0x10),
                                      MM_WE(27, 0x20) };
    static const u16 sChecked[] = { MM_WE(27, 0x40), MM_WE(27, 0x80), MM_WE(28, 0x01), MM_WE(28, 0x02),
                                    MM_WE(28, 0x04) };

    if (!NvMm_CheckWeekEvent(WEEKEVENTREG_88_02) && NvMm_CheckWeekEvent(sDeposited[box])) {
        if (NvMmClock_Day() == 2) {
            NvMm_SetWeekEvent(WEEKEVENTREG_LETTER_TO_KAFEI_SCHEDULED_DAY_2);
        } else if (NvMmClock_Day() == 3) {
            NvMm_SetWeekEvent(WEEKEVENTREG_LETTER_TO_KAFEI_SCHEDULED_DAY_3);
        }
        NvMm_SetWeekEvent(WEEKEVENTREG_LETTER_TO_KAFEI_SCHEDULED_TODAY);
        NvMm_SetWeekEvent(WEEKEVENTREG_POSTMAN_RECEIVED_LETTER_TO_KAFEI);
    }
    NvMm_SetWeekEvent(sChecked[box]);
}

/** MM's func_80AF9D04: through the door, it held open for him. */
static void NvMmPm_DoorWalk(NvMmPm* this, PlayState* play) {
    Actor* door = NvMmPm_FindScheduleDoor(play, this->scheduleResult);
    Vec3f offset;
    f32 step;

    if (Player_InCsMode(play) || (this->timePathTimeSpeed == 0) || (this->doorTime <= 0)) {
        return;
    }
    if (door != NULL) {
        NvMmDoor_SetOpenTimer(door, (((f32)this->doorElapsed / this->doorTime) <= 0.9f) ? this->doorOpenTimer : 0);
    }
    this->doorElapsed = CLAMP(this->doorElapsed, 0, this->doorTime);
    step = Math_Vec3f_DistXZ(&this->doorPathStart, &this->doorPathEnd) / this->doorTime;
    offset.x = 0.0f;
    offset.y = 0.0f;
    offset.z = this->doorElapsed * step;
    this->actor.world.pos.x = this->doorPathStart.x + Math_SinS(this->actor.world.rot.y) * offset.z;
    this->actor.world.pos.y = this->doorPathStart.y;
    this->actor.world.pos.z = this->doorPathStart.z + Math_CosS(this->actor.world.rot.y) * offset.z;
    this->doorElapsed += this->timePathTimeSpeed;
}

/** MM's func_80AF9E7C: along his path by the time. */
static void NvMmPm_FollowTimePath(NvMmPm* this, PlayState* play) {
    static f32 knots[265];
    Vec3f target;
    s32 savedElapsed = 0;
    s32 savedWaypoint = 0;

    NvMmSubS_TimePathing_FillKnots(knots, NV_MM_TIME_PATHING_ORDER, this->timePath->count + NV_MM_TIME_PATHING_ORDER);
    if (!(this->flags & 8)) {
        target = (Vec3f){ 0.0f, 0.0f, 0.0f };
        NvMmSubS_TimePathing_Update(this->timePath, &this->timePathProgress, &this->timePathElapsedTime,
                                    this->timePathWaypointTime, this->timePathTotalTime, &this->timePathWaypoint,
                                    knots, &target, this->timePathTimeSpeed);
        NvMmSubS_TimePathing_ComputeInitialY(play, this->timePath, this->timePathWaypoint, &target);
        this->actor.world.pos.y = target.y;
        this->flags |= 8;
    } else {
        target = this->timePathTargetPos;
    }
    this->actor.world.pos.x = target.x;
    this->actor.world.pos.z = target.z;
    if (Player_InCsMode(play)) {
        savedElapsed = this->timePathElapsedTime;
        savedWaypoint = this->timePathWaypoint;
        target = this->actor.world.pos;
    }
    this->timePathTargetPos = (Vec3f){ 0.0f, 0.0f, 0.0f };
    if (NvMmSubS_TimePathing_Update(this->timePath, &this->timePathProgress, &this->timePathElapsedTime,
                                    this->timePathWaypointTime, this->timePathTotalTime, &this->timePathWaypoint,
                                    knots, &this->timePathTargetPos, this->timePathTimeSpeed)) {
        this->flags |= 0x10;
    } else {
        this->actor.world.rot.y = Math_Vec3f_Yaw(&this->actor.world.pos, &this->timePathTargetPos);
    }
    if (Player_InCsMode(play)) {
        this->timePathElapsedTime = savedElapsed;
        this->timePathWaypoint = savedWaypoint;
        this->timePathTargetPos = target;
    }
    if ((this->flags & 0x10) && (this->scheduleResult == 90) && !NvMm_CheckWeekEvent(WEEKEVENTREG_89_40)) {
        NvMm_SetWeekEvent(WEEKEVENTREG_89_40);
    }
}

/** MM's func_80AFA170, the outdoor part: at a mailbox, opening it, then waiting. */
static void NvMmPm_AtMailbox(NvMmPm* this, PlayState* play) {
    if ((this->animIndex == ENPM_ANIM_11) && Animation_OnFrame(&this->skelAnime, this->skelAnime.endFrame)) {
        NvMmPm_ChangeAnim(this, ENPM_ANIM_4);
    }
    NvMmPm_CheckMailbox((this->scheduleResult == 19) ? 4 : (this->scheduleResult - 3));
}

/** MM's func_80AFA4D0: what his schedule says now, and doing it. */
static void NvMmPm_Schedule(NvMmPm* this, PlayState* play) {
    NvMmScheduleOutput out;

    this->timePathTimeSpeed = NvMmClock_Speed();
    out.result = 0;
    if (!NvMmSchedule_RunScript(play, sSchedules[this->stage], &out) ||
        ((this->scheduleResult != out.result) && !NvMmPm_SetupResult(this, play, &out))) {
        this->actor.shape.shadowDraw = NULL;
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        out.result = 0;
    } else {
        this->actor.shape.shadowDraw = ActorShadow_DrawCircle;
        this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    }
    this->scheduleResult = out.result;
    this->lookAt = &GET_PLAYER(play)->actor;
    switch (this->scheduleResult) {
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 19:
            NvMmPm_AtMailbox(this, play);
            break;
        case 1:
        case 2:
        case 10:
        case 11:
        case 12:
        case 13:
        case 32:
        case 33:
        case 34:
        case 35:
        case 36:
        case 37:
            NvMmPm_DoorWalk(this, play);
            break;
        default:
            if ((this->scheduleResult >= 38) && (this->scheduleResult <= 91)) {
                NvMmPm_FollowTimePath(this, play);
            }
            break;
    }
    Math_ApproachS(&this->actor.shape.rot.y, this->actor.world.rot.y, 3, 0x2AA8);
}

#define BOMBERS_NOTEBOOK_EVENT_MET_POSTMAN 0x0B

// MM's message scripts for him outdoors (ovl_En_Pm's D_80AFB60C ... D_80AFB63C)
#define PM_LINE(text) \
    MSCRIPT_CMD_BEGIN_TEXT(text), MSCRIPT_CMD_NOTEBOOK_EVENT(BOMBERS_NOTEBOOK_EVENT_MET_POSTMAN), \
        MSCRIPT_CMD_AWAIT_TEXT(), MSCRIPT_CMD_DONE()
static MsgScript D_80AFB60C[] = { PM_LINE(0x2774) };
static MsgScript D_80AFB614[] = { PM_LINE(0x2775) };
static MsgScript D_80AFB61C[] = { PM_LINE(0x2777) };
static MsgScript D_80AFB624[] = { PM_LINE(0x2778) };
static MsgScript D_80AFB62C[] = { PM_LINE(0x2779) };
static MsgScript D_80AFB634[] = { PM_LINE(0x2781) };
static MsgScript D_80AFB63C[] = { PM_LINE(0x277A) };

/** MM's EnPm_GetMsgScript, its outdoor part (the indoor ones go with the people there; the Kafei's
 * Mask line with MM's masks). */
static MsgScript* NvMmPm_GetMsgScript(NvMmPm* this) {
    switch (this->scheduleResult) {
        case 9:
            return D_80AFB614;
        case 14:
            return D_80AFB624;
        case 20:
            return D_80AFB61C;
        case 21:
            return D_80AFB634;
        case 22:
            return D_80AFB63C;
        case 18:
        case 74:
        case 75:
        case 76:
        case 77:
        case 78:
        case 79:
            return D_80AFB62C;
        default:
            return D_80AFB60C;
    }
}

/** MM's func_80AFA5FC: the conversation, run by his message script until it's done. */
static void NvMmPm_Talk(NvMmPm* this, PlayState* play) {
    if (NvMmMsgEvent_RunScript(&this->actor, play, this->msgScript, NULL, &this->msgScriptPos)) {
        this->lookTimer = 20;
        this->msgScriptPos = 0;
        this->func = NvMmPm_Schedule;
        return;
    }
    Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 4, 0x2AA8);
}

void NvMmPm_Init(Actor* thisx, PlayState* play) {
    NvMmPm* this = (NvMmPm*)thisx;

    ActorShape_Init(&this->actor.shape, 0.0f, NULL, 14.0f);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sSkel, NULL, this->jointTable, this->morphTable,
                       OBJECT_MM_LIMB_MAX);
    this->animIndex = -1;
    NvMmPm_ChangeAnim(this, ENPM_ANIM_0);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    Actor_SetScale(&this->actor, 0.01f);
    this->actor.targetMode = 0;
    this->stage = NvMmPm_Stage();
    this->scheduleResult = 0;
    this->flags = 0;
    this->talkRange = 40.0f;
    this->func = NvMmPm_Schedule;
    this->func(this, play);
}

void NvMmPm_Destroy(Actor* thisx, PlayState* play) {
    NvMmPm* this = (NvMmPm*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

void NvMmPm_Update(Actor* thisx, PlayState* play) {
    NvMmPm* this = (NvMmPm*)thisx;

    this->func(this, play);
    NvMmPm_UpdateStage(this);
    if ((getenv("NV_LOG") != NULL) && ((play->gameplayFrames % 20) == 0)) { // (dev: where he is)
        printf("NVMM postman result=%d pos=%.0f,%.0f,%.0f\n", this->scheduleResult, this->actor.world.pos.x,
               this->actor.world.pos.y, this->actor.world.pos.z);
        fflush(stdout);
    }
    if (this->scheduleResult == 0) {
        return;
    }
    if (this->func == NvMmPm_Schedule) {
        if (Actor_ProcessTalkRequest(&this->actor, play)) {
            this->msgScript = NvMmPm_GetMsgScript(this);
            this->msgScriptPos = 0;
            this->func = NvMmPm_Talk;
        } else if ((this->actor.flags & ACTOR_FLAG_ATTENTION_ENABLED) && (this->scheduleResult != 91)) {
            Actor_OfferTalk(&this->actor, play, this->talkRange);
        }
    }
    this->skelAnime.playSpeed = this->animPlaySpeed;
    SkelAnime_Update(&this->skelAnime);
    // head to Link while near (MM's func_80AF898C)
    if ((this->func == NvMmPm_Talk) || (this->actor.xzDistToPlayer < 120.0f)) {
        s16 target = this->actor.yawTowardsPlayer - this->actor.shape.rot.y;

        Math_ApproachS(&this->headRotY, CLAMP(target, -0x1FFE, 0x1FFE), 4, 0x2AA8);
    } else {
        Math_ApproachS(&this->headRotY, 0, 4, 0x2AA8);
    }
    Actor_MoveXZGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 30.0f, 12.0f, 0.0f, 4);
    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    Actor_SetFocus(&this->actor, 60.0f);
}

static s32 NvMmPm_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                   void* thisx) {
    NvMmPm* this = (NvMmPm*)thisx;

    if (limbIndex == OBJECT_MM_LIMB_0F) {
        rot->x += this->headRotY; // (the head's local axes: its x turns it side to side)
    }
    return false;
}

static void NvMmPm_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    NvMmPm* this = (NvMmPm*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    switch (limbIndex) {
        case OBJECT_MM_LIMB_0F:
            if ((this->flags & 0x8000) && !NvMm_CheckWeekEvent(WEEKEVENTREG_90_04) && (this->flags & 0x1000)) {
                gSPDisplayList(POLY_OPA_DISP++, DL_CAP);
            }
            break;
        case OBJECT_MM_LIMB_0B:
            if (this->flags & 0x800) {
                gSPDisplayList(POLY_OPA_DISP++, DL_LETTER);
            }
            break;
        case OBJECT_MM_LIMB_08:
            if (this->flags & 0x1000) {
                gSPDisplayList(POLY_OPA_DISP++, DL_BAG);
            }
            break;
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

void NvMmPm_Draw(Actor* thisx, PlayState* play) {
    static Gfx sEmptyDL[] = { gsSPEndDisplayList() };
    NvMmPm* this = (NvMmPm*)thisx;

    if (this->scheduleResult == 0) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0C, sEmptyDL); // (MM's Scene_SetRenderModeXlu(play, 0, 1))
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sEyeTex));
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          NvMmPm_OverrideLimbDraw, NvMmPm_PostLimbDraw, &this->actor);
    CLOSE_DISPS(play->state.gfxCtx);
}
