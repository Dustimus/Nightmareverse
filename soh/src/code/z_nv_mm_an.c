/*
 * Nightmareverse: Anju, from MM's En_An (zeldaret/mm, ovl_En_An), ported closely (mm/README.md). Her
 * model is MM's object_an1 (her face: eyes and mouth set in segments 8 and 9 as in MM, by MM's face
 * logic), with object_an2's umbrella and object_an3's broom, and her tray and chopsticks.
 *
 * Her schedule is MM's (mm/schedules/En_An.schl): at the Stock Pot Inn's front desk, cooking,
 * sweeping, waiting for closing time, walking the inn's halls and through its doors (with her tray,
 * her broom), out to the Laundry Pool on Day 2 under her umbrella (sitting there after, crying once
 * Link has talked to her), and the midnight meeting. She talks through MM's message scripts
 * (nv_mm_msgscripts/En_An.msg.inc, the decomp's, by mm_msgscripts.py), with MM's callbacks for
 * cooking (tasting), the Laundry Pool and the midnight meeting.
 *
 * Adapted from MM:
 * - her results with Granny (lunch), the Goron and the Postman at the desk: MM's, the talk camera
 *   left to OoT's (MM's cutscene cameras per line are left out);
 * - Link is MM's human or Deku form (z_nv_mm_form.c; Deku Link gets MM's Deku lines), without MM's
 *   masks (Kafei's Mask's lines wait); MM's items
 *   she gives (the Room Key, the Letter to Kafei) aren't in yet (the script goes on without them);
 * - the midnight meeting's scene (MM's cutscene at the inn's entrance 3) isn't ported: Link is taken
 *   to that entrance as in MM, without the cutscene;
 * - MM's object slots for her other animations: here they're drawn by path, no slots; her head
 *   turn toward Link (MM's transform draw) and the faces MM sets line by line (EnAn_HandleDialogue)
 *   are left out for now; her footsteps are MM's own sound;
 * - the Couples' Mask ending (after it she's gone from town) is kept: MM's week event 51_40.
 */
#include "global.h"
#include "z_nv_mm_schedule.h"
#include "z_nv_mm_subs.h"
#include <stdio.h>
#include <stdlib.h>

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

enum {
    ENAN_ANIM_IDLE, ENAN_ANIM_IDLE_MORPH, ENAN_ANIM_BOW, ENAN_ANIM_BOW_MORPH, ENAN_ANIM_SURPRISED,
    ENAN_ANIM_LOOK_UP_RELIEVED, ENAN_ANIM_WALK, ENAN_ANIM_SIT, ENAN_ANIM_SITTING_IN_DISBELIEF, ENAN_ANIM_COOK,
    ENAN_ANIM_TASTE, ENAN_ANIM_TASTE_END, ENAN_ANIM_WAIT_WITH_TRAY, ENAN_ANIM_WALK_WITH_TRAY, ENAN_ANIM_UMBRELLA_IDLE,
    ENAN_ANIM_UMBRELLA_WALK, ENAN_ANIM_UMBRELLA_SIT, ENAN_ANIM_UMBRELLA_CRY, ENAN_ANIM_BROOM_IDLE, ENAN_ANIM_BROOM_WALK,
    ENAN_ANIM_BROOM_SWEEP, ENAN_ANIM_MAX
};
#define AN(o, n) ("__OTR__mm/objects/object_" o "/" n)
// MM's sAnimationInfo (the ones used here): animation, mode, morph
static struct {
    const char* anim;
    u8 mode;
    s8 morph;
} sAnimations[ENAN_ANIM_MAX] = {
    { AN("an1", "gAnju1IdleAnim"), ANIMMODE_LOOP, 0 },
    { AN("an1", "gAnju1IdleAnim"), ANIMMODE_LOOP, -6 },
    { AN("an1", "gAnju1BowAnim"), ANIMMODE_ONCE, 0 },
    { AN("an1", "gAnju1BowAnim"), ANIMMODE_ONCE, -6 },
    { AN("an1", "gAnju1SurprisedAnim"), ANIMMODE_ONCE, 0 },
    { AN("an1", "gAnju1LookUpRelievedAnim"), ANIMMODE_ONCE, 0 },
    { AN("an1", "gAnju1WalkAnim"), ANIMMODE_LOOP, 0 },
    { AN("an1", "gAnju1SitAnim"), ANIMMODE_LOOP, 0 },
    { AN("an1", "gAnju1SittingInDisbeliefAnim"), ANIMMODE_LOOP, 0 },
    { AN("an1", "gAnju1CookAnim"), ANIMMODE_LOOP, 0 },
    { AN("an1", "gAnju1TasteAnim"), ANIMMODE_ONCE, 0 },
    { AN("an1", "gAnju1TasteEndAnim"), ANIMMODE_ONCE, 0 },
    { AN("an1", "gAnju1WaitWithTrayAnim"), ANIMMODE_LOOP, 0 },
    { AN("an1", "gAnju1WalkWithTrayAnim"), ANIMMODE_LOOP, 0 },
    { AN("an2", "gAnju2UmbrellaIdleAnim"), ANIMMODE_LOOP, 0 },
    { AN("an2", "gAnju2UmbrellaWalkAnim"), ANIMMODE_LOOP, 0 },
    { AN("an2", "gAnju2UmbrellaSitAnim"), ANIMMODE_ONCE, 0 },
    { AN("an2", "gAnju2UmbrellaCryAnim"), ANIMMODE_LOOP, 0 },
    { AN("an3", "gAnju3BroomIdleAnim"), ANIMMODE_LOOP, 0 },
    { AN("an3", "gAnju3BroomWalkAnim"), ANIMMODE_LOOP, 0 },
    { AN("an3", "gAnju3BroomSweepAnim"), ANIMMODE_LOOP, 0 },
};
#define DL_TRAY ((Gfx*)AN("an1", "gAnju1FoodTrayDL"))
#define DL_CHOPSTICKS ((Gfx*)AN("an1", "gAnju1ChopsticksDL"))
#define DL_BROOM ((Gfx*)AN("an3", "gAnju3BroomDL"))
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
#define STATE_DRAW_TRAY 0x10
#define STATE_DRAW_BROOM 0x20
#define STATE_DRAW_CHOPSTICKS 0x40
#define STATE_IGNORE_GRAVITY 0x80
#define STATE_OFFER 0x100 // (MM's offer mode: on screen)

#include "z_nv_mm_msgevent.h"
#include "nv_mm_msgscripts/En_An.msg.inc"
#define WEEKEVENTREG_TALKED_ANJU_IN_LAUNDRY_POOL MM_WE(55, 0x20)
#define WEEKEVENTREG_PROMISED_MIDNIGHT_MEETING MM_WE(50, 0x08)

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
    f32 offerRange;
    s16 laundryPoolState;
    s16 msgEventState;
    MsgScript* msgScript;
    s32 msgScriptPos;
    MsgScriptCallback msgScriptCallback;
    s16 trayScroll1;
    s16 trayScroll2;
    Actor* lookAtActor; // (MM's: the one she's with at the desk, else Link)
} NvMmAn;

const size_t gNvMmAnSize = sizeof(NvMmAn);

s32 NvMm_CheckWeekEvent(s32 flag);
s32 NvMmClock_Speed(void);
s32 NvMmClock_Day(void);
s32 NvMmActor_Id(s32 mmId);
s32 NvMmDoor_ScheduleType(Actor* thisx);
void NvMmDoor_SetOpenTimer(Actor* thisx, s8 frames);

static ColliderCylinderInit sCylinderInit = {
    { COLTYPE_HIT1, AT_NONE, AC_NONE, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK1, { 0x00000000, 0x00, 0x00 }, { 0x00000000, 0x00, 0x00 }, TOUCH_NONE, BUMP_NONE, OCELEM_ON },
    { 14, 62, 0, { 0, 0, 0 } },
};

/** MM's EnAn_ChangeAnim: idle/idle-morph count as one, as do the two sits. */
static void NvMmAn_ChangeAnim(NvMmAn* this, s32 animIndex) {
    s32 same = (this->animIndex == animIndex) ||
               (((animIndex == ENAN_ANIM_IDLE) || (animIndex == ENAN_ANIM_IDLE_MORPH)) &&
                ((this->animIndex == ENAN_ANIM_IDLE) || (this->animIndex == ENAN_ANIM_IDLE_MORPH)));

    if (!same) {
        AnimationHeader* anim = (AnimationHeader*)sAnimations[animIndex].anim;

        this->animIndex = animIndex;
        Animation_Change(&this->skelAnime, anim, 1.0f, 0.0f, Animation_GetLastFrame(anim), sAnimations[animIndex].mode,
                         sAnimations[animIndex].morph);
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

/** MM's EnAn_FindScheduleDoor: the inn's doors she uses, by their schedule type. */
static Actor* NvMmAn_FindScheduleDoor(PlayState* play, s32 result) {
    s32 doorId = NvMmActor_Id(0x005);
    s32 schType;
    Actor* a;

    switch (result) {
        case ANJU_SCH_DOOR_26:
        case ANJU_SCH_DOOR_27:
            schType = 13; // INN_GRANNYS
            break;
        case ANJU_SCH_DOOR_33:
        case ANJU_SCH_DOOR_34:
        case ANJU_SCH_DOOR_37:
        case ANJU_SCH_DOOR_38:
            schType = 11; // INN_MAIN_ENTRANCE
            break;
        case ANJU_SCH_DOOR_36:
            schType = 16; // INN_LARGE_SUITE
            break;
        case ANJU_SCH_DOOR_28:
        case ANJU_SCH_DOOR_29:
        case ANJU_SCH_DOOR_30:
        case ANJU_SCH_DOOR_31:
        case ANJU_SCH_DOOR_32:
        case ANJU_SCH_DOOR_35:
        case ANJU_SCH_DOOR_39:
            schType = 14; // INN_STAFF_ROOM
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
    switch (out->result) {
        case ANJU_SCH_DOOR_27:
        case ANJU_SCH_DOOR_28:
        case ANJU_SCH_DOOR_32:
        case ANJU_SCH_DOOR_33:
        case ANJU_SCH_DOOR_34:
        case ANJU_SCH_DOOR_35:
        case ANJU_SCH_DOOR_37:
        case ANJU_SCH_DOOR_38:
            this->faceIndex = 2;
            this->eyeTimer = 8;
            break;
    }
    switch (out->result) {
        case ANJU_SCH_DOOR_26:
        case ANJU_SCH_DOOR_27:
        case ANJU_SCH_DOOR_28:
            this->stateFlags |= STATE_DRAW_TRAY | STATE_UPDATE_EYES;
            NvMmAn_ChangeAnim(this, ENAN_ANIM_WALK_WITH_TRAY);
            break;
        case ANJU_SCH_DOOR_36:
        case ANJU_SCH_DOOR_39:
            this->stateFlags |= STATE_UPDATE_EYES | STATE_DRAW_BROOM;
            NvMmAn_ChangeAnim(this, ENAN_ANIM_BROOM_WALK);
            break;
        default:
            this->stateFlags |= STATE_UPDATE_EYES;
            NvMmAn_ChangeAnim(this, ENAN_ANIM_WALK);
            break;
    }
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
    switch (out->result) {
        case ANJU_SCH_WALKING_44:
        case ANJU_SCH_WALKING_45:
        case ANJU_SCH_WALKING_50:
        case ANJU_SCH_WALKING_51:
            this->faceIndex = 2;
            this->eyeTimer = 8;
            break;
        default:
            if (out->result >= ANJU_SCH_WALKING_54) {
                this->faceIndex = 2;
                this->eyeTimer = 8;
            }
            break;
    }
    switch (out->result) {
        case ANJU_SCH_WALKING_42:
        case ANJU_SCH_WALKING_43:
            this->stateFlags |= STATE_OFFER;
            // fallthrough
        case ANJU_SCH_WALKING_44:
        case ANJU_SCH_WALKING_45:
            NvMmAn_ChangeAnim(this, ENAN_ANIM_WALK_WITH_TRAY);
            this->stateFlags |= STATE_UPDATE_EYES | STATE_DRAW_TRAY;
            break;
        case ANJU_SCH_WALKING_52:
        case ANJU_SCH_WALKING_53:
            NvMmAn_ChangeAnim(this, ENAN_ANIM_BROOM_WALK);
            this->stateFlags |= STATE_OFFER | STATE_UPDATE_EYES | STATE_DRAW_BROOM;
            break;
        case ANJU_SCH_WALKING_50:
        case ANJU_SCH_WALKING_51:
            NvMmAn_ChangeAnim(this, ENAN_ANIM_WALK);
            this->stateFlags |= STATE_UPDATE_EYES;
            break;
        case ANJU_SCH_WALKING_40:
        case ANJU_SCH_WALKING_47:
        case ANJU_SCH_WALKING_48:
        case ANJU_SCH_WALKING_49:
            this->stateFlags |= STATE_OFFER | STATE_UPDATE_EYES;
            NvMmAn_ChangeAnim(this, ENAN_ANIM_WALK);
            break;
        default:
            if (out->result >= ANJU_SCH_WALKING_54) { // (out in town: her umbrella, no talk offer)
                NvMmAn_ChangeAnim(this, ENAN_ANIM_UMBRELLA_WALK);
                this->stateFlags |= STATE_UPDATE_EYES | STATE_DRAW_UMBRELLA;
            } else {
                NvMmAn_ChangeAnim(this, ENAN_ANIM_WALK);
            }
            break;
    }
    this->actor.gravity = -1.0f;
    return true;
}

/** MM's EnAn_ProcessSchedule_Sweeping: at her path's start, sweeping. */
static s32 NvMmAn_SetupSweeping(NvMmAn* this, PlayState* play, NvMmScheduleOutput* out) {
    Vec3s* points;
    Vec3f first;
    Vec3f second;

    this->timePath = NvMmAn_Path(this, play, out->result);
    if ((this->timePath == NULL) || (this->timePath->count < 2)) {
        return false;
    }
    points = SEGMENTED_TO_VIRTUAL(this->timePath->points);
    first = (Vec3f){ points[0].x, points[0].y, points[0].z };
    second = (Vec3f){ points[1].x, points[1].y, points[1].z };
    this->actor.world.rot.y = Math_Vec3f_Yaw(&first, &second);
    this->actor.shape.rot = this->actor.world.rot;
    this->actor.world.pos = this->actor.prevPos = first;
    NvMmAn_ChangeAnim(this, ENAN_ANIM_BROOM_SWEEP);
    this->stateFlags |= STATE_OFFER | STATE_UPDATE_EYES | STATE_DRAW_BROOM;
    return true;
}

/** MM's EnAn_ProcessSchedule_80B56880: at her path's end, standing at the desk, cooking, sitting at
 * the Laundry Pool, waiting at midnight. */
static s32 NvMmAn_SetupStand(NvMmAn* this, PlayState* play, NvMmScheduleOutput* out) {
    Vec3s* points;
    Vec3f last;
    Vec3f before;

    this->timePath = NvMmAn_Path(this, play, out->result);
    if ((this->timePath == NULL) || (this->timePath->count < 2)) {
        return false;
    }
    points = SEGMENTED_TO_VIRTUAL(this->timePath->points);
    last = (Vec3f){ points[this->timePath->count - 1].x, points[this->timePath->count - 1].y,
                    points[this->timePath->count - 1].z };
    before = (Vec3f){ points[this->timePath->count - 2].x, points[this->timePath->count - 2].y,
                      points[this->timePath->count - 2].z };
    this->actor.world.rot.y = Math_Vec3f_Yaw(&before, &last);
    this->actor.shape.rot = this->actor.home.rot = this->actor.world.rot;
    this->actor.world.pos = this->actor.prevPos = last;
    switch (out->result) {
        case ANJU_SCH_WAITING_CLOSING_TIME:
            this->actor.world.rot.y += 0x7FF8;
            // fallthrough
        case ANJU_SCH_RECEPTIONIST_IDLE:
        case ANJU_SCH_MIDNIGHT_MEETING:
            NvMmAn_ChangeAnim(this, ENAN_ANIM_IDLE);
            this->stateFlags |= STATE_OFFER | STATE_UPDATE_EYES;
            if (out->result == ANJU_SCH_RECEPTIONIST_IDLE) {
                this->offerRange = 70.0f;
            }
            break;
        case ANJU_SCH_LAUNDRY_POOL_SIT:
            this->stateFlags |= STATE_UPDATE_EYES | STATE_DRAW_UMBRELLA;
            if (NvMm_CheckWeekEvent(WEEKEVENTREG_TALKED_ANJU_IN_LAUNDRY_POOL)) {
                NvMmAn_ChangeAnim(this, ENAN_ANIM_UMBRELLA_CRY);
                this->stateFlags |= STATE_IGNORE_GRAVITY;
                this->actor.world.rot.y += 0x7FF8;
                this->actor.shape.rot.y = this->actor.world.rot.y;
                this->laundryPoolState = 4;
            } else {
                NvMmAn_ChangeAnim(this, ENAN_ANIM_UMBRELLA_WALK);
                this->laundryPoolState = 0;
            }
            this->faceIndex = 2;
            this->eyeTimer = 8;
            break;
        case ANJU_SCH_COOKING:
            NvMmAn_ChangeAnim(this, ENAN_ANIM_COOK);
            this->stateFlags |= STATE_OFFER | STATE_UPDATE_EYES | STATE_DRAW_CHOPSTICKS;
            break;
    }
    return true;
}

/** MM's EnAn_ProcessSchedule_StaffRoom: seated in the staff room, waiting for Kafei, or hearing
 * her mother out (Day 2's night). */
static s32 NvMmAn_SetupStaffRoom(NvMmAn* this, PlayState* play, NvMmScheduleOutput* out) {
    this->actor.world.pos = (Vec3f){ -508.0f, 210.0f, -162.0f };
    this->actor.shape.rot = (Vec3s){ 0, 0x3FFC, 0 };
    this->actor.world.rot = this->actor.shape.rot;
    if (out->result == ANJU_SCH_WAITING_FOR_KAFEI) {
        this->stateFlags |= STATE_OFFER;
        NvMmAn_ChangeAnim(this, ENAN_ANIM_SIT);
    } else {
        NvMmAn_ChangeAnim(this, ENAN_ANIM_SITTING_IN_DISBELIEF);
        this->faceIndex = 5;
        this->eyeTimer = 8;
    }
    this->stateFlags |= STATE_UPDATE_EYES | STATE_IGNORE_GRAVITY;
    this->actor.gravity = 0.0f;
    return true;
}

/** MM's EnAn_FindLookAtActor's people: the Goron, Granny, the Postman (MM's ids), else none. */
static s32 NvMmAn_InteractMmId(s32 result) {
    switch (result) {
        case ANJU_SCH_ATTEND_GORON:
            return 0x276; // En_Ig
        case ANJU_SCH_GIVE_LUNCH_TO_GRANNY:
            return 0x243; // En_Nb
        case ANJU_SCH_RECEIVE_LETTER_FROM_POSTMAN:
            return 0x1D5; // En_Pm
        default:
            return -1;
    }
}

static Actor* NvMmAn_FindNpc(NvMmAn* this, PlayState* play, s32 mmId) {
    s32 id = NvMmActor_Id(mmId);
    Actor* a;

    for (a = play->actorCtx.actorLists[ACTORCAT_NPC].head; a != NULL; a = a->next) {
        if ((a != &this->actor) && (a->id == id) && (a->update != NULL)) {
            return a;
        }
    }
    return NULL;
}

/** MM's func_80B55D98 and EnAn_ProcessSchedule_AttendGoron / _GiveLunchToGranny /
 * _ReceiveLetterFromPostman: at her path's end, with the other one there. */
static s32 NvMmAn_SetupInteract(NvMmAn* this, PlayState* play, NvMmScheduleOutput* out) {
    Actor* other = NvMmAn_FindNpc(this, play, NvMmAn_InteractMmId(out->result));
    Vec3s* points;
    Vec3f before;

    this->timePath = NvMmAn_Path(this, play, out->result);
    if ((other == NULL) || (this->timePath == NULL) || (this->timePath->count < 2)) {
        return false;
    }
    points = SEGMENTED_TO_VIRTUAL(this->timePath->points);
    before = (Vec3f){ points[this->timePath->count - 2].x, points[this->timePath->count - 2].y,
                      points[this->timePath->count - 2].z };
    this->actor.world.pos = (Vec3f){ points[this->timePath->count - 1].x, points[this->timePath->count - 1].y,
                                     points[this->timePath->count - 1].z };
    this->actor.world.rot.y = Math_Vec3f_Yaw(&before, &this->actor.world.pos);
    this->actor.shape.shadowDraw = NULL;
    if (out->result == ANJU_SCH_GIVE_LUNCH_TO_GRANNY) {
        NvMmAn_ChangeAnim(this, ENAN_ANIM_WAIT_WITH_TRAY);
        this->stateFlags |= STATE_DRAW_TRAY;
    } else {
        NvMmAn_ChangeAnim(this, ENAN_ANIM_IDLE);
    }
    this->stateFlags |= STATE_OFFER | STATE_UPDATE_EYES;
    return true;
}

/** MM's EnAn_ProcessScheduleOutput (without the people not ported yet). */
static s32 NvMmAn_SetupResult(NvMmAn* this, PlayState* play, NvMmScheduleOutput* out) {
    this->stateFlags = 0;
    this->faceIndex = 0;
    this->eyeTimer = 8;
    this->offerRange = 40.0f;
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    switch (out->result) {
        case ANJU_SCH_LAUNDRY_POOL_SIT:
        case ANJU_SCH_COOKING:
        case ANJU_SCH_RECEPTIONIST_IDLE:
        case ANJU_SCH_WAITING_CLOSING_TIME:
        case ANJU_SCH_MIDNIGHT_MEETING:
            return NvMmAn_SetupStand(this, play, out);
        case ANJU_SCH_SWEEPING:
            return NvMmAn_SetupSweeping(this, play, out);
        case ANJU_SCH_WAITING_FOR_KAFEI:
        case ANJU_SCH_TALKING_WITH_MOM:
            return NvMmAn_SetupStaffRoom(this, play, out);
        case ANJU_SCH_ATTEND_GORON:
        case ANJU_SCH_GIVE_LUNCH_TO_GRANNY:
        case ANJU_SCH_RECEIVE_LETTER_FROM_POSTMAN:
            return NvMmAn_SetupInteract(this, play, out);
        default:
            if ((out->result >= ANJU_SCH_DOOR_26) && (out->result <= ANJU_SCH_DOOR_39)) {
                return NvMmAn_SetupDoor(this, play, out);
            }
            if ((out->result >= ANJU_SCH_WALKING_40) && (out->result <= ANJU_SCH_WALKING_63) &&
                (out->result != ANJU_SCH_41)) {
                return NvMmAn_SetupWalking(this, play, out);
            }
            // (the ranch, with Kafei: not here yet)
            return false;
    }
}

/** MM's EnAn_HandleSch_LaundryPool: turning to the water and sitting down. */
static void NvMmAn_LaundryPool(NvMmAn* this, PlayState* play) {
    switch (this->laundryPoolState) {
        case 0:
            this->actor.world.rot.y += 0x7FF8;
            this->laundryPoolState++;
            break;
        case 1:
            if (((this->actor.world.rot.y / 0xB6) * 0xB6) == ((this->actor.shape.rot.y / 0xB6) * 0xB6)) {
                this->actor.shape.rot = this->actor.world.rot;
                NvMmAn_ChangeAnim(this, ENAN_ANIM_UMBRELLA_SIT);
                this->stateFlags |= STATE_IGNORE_GRAVITY;
                this->laundryPoolState++;
            }
            break;
        case 2:
            if (Animation_OnFrame(&this->skelAnime, this->skelAnime.endFrame)) {
                this->laundryPoolState++;
            }
            break;
        case 3:
            if (ABS((s16)(this->actor.yawTowardsPlayer - this->actor.shape.rot.y)) < 0x3000) {
                this->stateFlags |= STATE_OFFER;
            } else {
                this->stateFlags &= ~STATE_OFFER;
            }
            break;
        case 4:
            this->stateFlags |= STATE_OFFER;
            this->laundryPoolState++;
            break;
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
    if ((this->scheduleResult >= ANJU_SCH_DOOR_26) && (this->scheduleResult <= ANJU_SCH_DOOR_39)) {
        NvMmAn_DoorWalk(this, play);
    } else if ((this->scheduleResult >= ANJU_SCH_WALKING_40) && (this->timePath != NULL)) {
        NvMmAn_FollowTimePath(this, play);
    } else if (this->scheduleResult == ANJU_SCH_LAUNDRY_POOL_SIT) {
        NvMmAn_LaundryPool(this, play);
    }
    // MM's EnAn_FindLookAtActor, and EnAn_HandleSch_InteractActor: turned to whoever she's with
    this->lookAtActor = (NvMmAn_InteractMmId(this->scheduleResult) >= 0)
                            ? NvMmAn_FindNpc(this, play, NvMmAn_InteractMmId(this->scheduleResult))
                            : &GET_PLAYER(play)->actor;
    if ((this->lookAtActor != NULL) && (this->lookAtActor != &GET_PLAYER(play)->actor)) {
        this->actor.world.rot.y = Actor_WorldYawTowardActor(&this->actor, this->lookAtActor);
    }
    Math_ApproachS(&this->actor.shape.rot.y, this->actor.world.rot.y, 3, 0x2AA8);
}

/** MM's EnAn_MsgEvent_Cooking: she tastes, then goes on. */
static s32 NvMmAn_MsgEventCooking(Actor* thisx, PlayState* play) {
    NvMmAn* this = (NvMmAn*)thisx;
    s32 ret = false;

    switch (this->msgEventState) {
        case 0:
            NvMmAn_ChangeAnim(this, ENAN_ANIM_TASTE);
            this->msgEventState++;
            break;
        case 1:
        case 3:
            if (Animation_OnFrame(&this->skelAnime, this->skelAnime.endFrame)) {
                this->msgEventState++;
                ret = true;
            }
            break;
        case 2:
            NvMmAn_ChangeAnim(this, ENAN_ANIM_TASTE_END);
            this->msgEventState++;
            break;
    }
    return ret;
}

/** MM's EnAn_MsgEvent_LaundryPool: she breaks down crying (MM first shows Kafei's mask she holds:
 * MM's masks aren't in yet). */
static s32 NvMmAn_MsgEventLaundryPool(Actor* thisx, PlayState* play) {
    NvMmAn* this = (NvMmAn*)thisx;

    if (this->msgEventState == 0) {
        this->msgEventState++;
        if (!NvMm_CheckWeekEvent(WEEKEVENTREG_TALKED_ANJU_IN_LAUNDRY_POOL)) {
            return true;
        }
    }
    if (this->msgEventState == 1) {
        NvMmAn_ChangeAnim(this, ENAN_ANIM_UMBRELLA_CRY);
        this->msgEventState++;
        return true;
    }
    return false;
}

/** MM's EnAn_MsgEvent_MidnightMeeting: to the inn's entrance 3 (MM's meeting there; its cutscene
 * isn't ported). */
static s32 NvMmAn_MsgEventMidnightMeeting(Actor* thisx, PlayState* play) {
    NvMmAn* this = (NvMmAn*)thisx;

    if (this->msgEventState == 0) {
        play->nextEntranceIndex = ENTR_MM_Z2_YADOYA_3;
        play->transitionTrigger = TRANS_TRIGGER_START;
        play->transitionType = TRANS_TYPE_FADE_BLACK;
        gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK_SLOW;
        this->msgEventState++;
    }
    return false;
}

/** MM's EnAn_MsgEvent_AttendGoron / _GiveLunchToGranny / _ReceiveLetterFromPostman: in MM they
 * turn the talk camera between the two speakers, line by line; OoT's talk camera follows whom Link
 * listens to (the script's FOCUS_TO_CHILD / FOCUS_TO_SELF), so each step just goes on. */
static s32 NvMmAn_MsgEventExchange(Actor* thisx, PlayState* play) {
    return true;
}

s32 NvMmForm_Get(void); // (MM's player->transformation: z_nv_mm_form.c; 3 Deku, 4 human)

/** MM's EnAn_GetMsgScript: a human or Deku Link (Kafei's Mask's branch waits for that mask). */
static MsgScript* NvMmAn_GetMsgScript(NvMmAn* this, PlayState* play) {
    s32 human = (NvMmForm_Get() == 4);

    switch (this->scheduleResult) {
        case ANJU_SCH_GIVE_LUNCH_TO_GRANNY:
            this->msgScriptCallback = NvMmAn_MsgEventExchange;
            return sAnjuMsgScript_SchGiveLunchToGranny;
        case ANJU_SCH_RECEIVE_LETTER_FROM_POSTMAN:
            this->msgScriptCallback = NvMmAn_MsgEventExchange;
            return sAnjuMsgScript_SchReceiveLetterFromPostman;
        case ANJU_SCH_ATTEND_GORON:
            this->msgScriptCallback = NvMmAn_MsgEventExchange;
            return sAnjuMsgScript_SchAttendGoron;
        case ANJU_SCH_COOKING:
            this->msgScriptCallback = NvMmAn_MsgEventCooking;
            return sAnjuMsgScript_SchCooking;
        default:
            break;
    }
    if (NvMmForm_Get() == 3) { // Deku Link
        switch (this->scheduleResult) {
            case ANJU_SCH_WAITING_FOR_KAFEI:
                return sAnjuMsgScript_SchWaitingForKafei;
            case ANJU_SCH_WALKING_40:
            case ANJU_SCH_WALKING_42:
            case ANJU_SCH_WALKING_43:
            case ANJU_SCH_WALKING_44:
            case ANJU_SCH_WALKING_45:
            case ANJU_SCH_WALKING_47:
                return sAnjuMsgScript_80B58B7C;
            case ANJU_SCH_LAUNDRY_POOL_SIT:
                return sAnjuMsgScript_SchLaundryPoolDeku;
            case ANJU_SCH_WALKING_49:
                return sAnjuMsgScript_80B589FC;
            default:
                return sAnjuMsgScript_DekuDefault;
        }
    }
    if (human && NvMm_CheckWeekEvent(WEEKEVENTREG_PROMISED_MIDNIGHT_MEETING)) {
        if (this->scheduleResult == ANJU_SCH_MIDNIGHT_MEETING) {
            this->msgScriptCallback = NvMmAn_MsgEventMidnightMeeting;
            return sAnjuMsgScript_SchMidnightMeeting;
        }
        if (this->scheduleResult == ANJU_SCH_LAUNDRY_POOL_SIT) {
            this->msgScriptCallback = NvMmAn_MsgEventLaundryPool;
            return sAnjuMsgScript_SchLaundryPoolDefault;
        }
        if (NvMmClock_Day() >= 2) {
            return sAnjuMsgScript_80B58A44;
        }
    }
    switch (this->scheduleResult) {
        case ANJU_SCH_WAITING_FOR_KAFEI:
            return sAnjuMsgScript_SchWaitingForKafei;
        case ANJU_SCH_WALKING_40:
            return sAnjuMsgScript_80B5894C;
        case ANJU_SCH_WALKING_42:
        case ANJU_SCH_WALKING_43:
        case ANJU_SCH_WALKING_44:
        case ANJU_SCH_WALKING_45:
            return sAnjuMsgScript_80B58980;
        case ANJU_SCH_LAUNDRY_POOL_SIT:
            this->msgScriptCallback = NvMmAn_MsgEventLaundryPool;
            return sAnjuMsgScript_SchLaundryPoolDefault;
        case ANJU_SCH_RECEPTIONIST_IDLE:
            return (NvMmClock_Day() >= 2) ? sAnjuMsgScript_Receptionist : sAnjuMsgScript_ReceptionistDay1;
        case ANJU_SCH_WAITING_CLOSING_TIME:
        case ANJU_SCH_WALKING_47:
            return sAnjuMsgScript_InnCloseTime;
        case ANJU_SCH_SWEEPING:
            return sAnjuMsgScript_SchSweeping;
        case ANJU_SCH_MIDNIGHT_MEETING:
            return sAnjuMsgScript_80B589FC;
        case ANJU_SCH_WALKING_48:
            return sAnjuMsgScript_80B58944;
        case ANJU_SCH_WALKING_49:
            return human ? sAnjuMsgScript_80B58B88 : sAnjuMsgScript_80B589FC;
        case ANJU_SCH_WALKING_52:
        case ANJU_SCH_WALKING_53:
            return sAnjuMsgScript_80B58994;
        default:
            return NULL;
    }
}

/** MM's EnAn_ChooseAnimAfterTalking. */
static void NvMmAn_AnimAfterTalking(NvMmAn* this) {
    switch (this->scheduleResult) {
        case ANJU_SCH_RECEIVE_LETTER_FROM_POSTMAN:
        case ANJU_SCH_ATTEND_GORON:
        case ANJU_SCH_RECEPTIONIST_IDLE:
        case ANJU_SCH_WAITING_CLOSING_TIME:
        case ANJU_SCH_MIDNIGHT_MEETING:
            NvMmAn_ChangeAnim(this, ENAN_ANIM_IDLE);
            break;
        case ANJU_SCH_GIVE_LUNCH_TO_GRANNY:
            NvMmAn_ChangeAnim(this, ENAN_ANIM_WAIT_WITH_TRAY);
            break;
        case ANJU_SCH_SWEEPING:
            NvMmAn_ChangeAnim(this, ENAN_ANIM_BROOM_SWEEP);
            break;
        case ANJU_SCH_COOKING:
            NvMmAn_ChangeAnim(this, ENAN_ANIM_COOK);
            break;
        case ANJU_SCH_WALKING_47:
        case ANJU_SCH_WALKING_49:
            NvMmAn_ChangeAnim(this, ENAN_ANIM_WALK);
            break;
        case ANJU_SCH_WALKING_52:
        case ANJU_SCH_WALKING_53:
            NvMmAn_ChangeAnim(this, ENAN_ANIM_BROOM_WALK);
            break;
    }
}

/** MM's EnAn_Talk. */
static void NvMmAn_Talk(NvMmAn* this, PlayState* play) {
    if (NvMmMsgEvent_RunScript(&this->actor, play, this->msgScript, this->msgScriptCallback, &this->msgScriptPos)) {
        this->stateFlags |= STATE_OFFER;
        this->msgScriptPos = 0;
        NvMmAn_AnimAfterTalking(this);
        this->func = NvMmAn_FollowSchedule;
        return;
    }
    if ((this->scheduleResult != ANJU_SCH_LAUNDRY_POOL_SIT) && (this->scheduleResult != ANJU_SCH_COOKING) &&
        (this->scheduleResult != ANJU_SCH_WAITING_FOR_KAFEI)) {
        Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 4, 0x2AA8);
    }
}

/** MM's EnAn_CheckTalk. */
static void NvMmAn_CheckTalk(NvMmAn* this, PlayState* play) {
    MsgScript* script;

    if ((this->func != NvMmAn_FollowSchedule) || !(this->stateFlags & STATE_OFFER) ||
        !Actor_ProcessTalkRequest(&this->actor, play)) {
        return;
    }
    this->msgEventState = 0;
    this->msgScriptCallback = NULL;
    script = NvMmAn_GetMsgScript(this, play);
    if (script == NULL) {
        return;
    }
    this->stateFlags &= ~STATE_OFFER;
    this->actor.child = this->lookAtActor; // (MM: whom the script's FOCUS_TO_CHILD turns to)
    this->msgScript = script;
    this->msgScriptPos = 0;
    this->func = NvMmAn_Talk;
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

    NvMmAn_CheckTalk(this, play);
    this->func(this, play);
    if ((getenv("NV_LOG") != NULL) && ((play->gameplayFrames % 20) == 0)) { // (dev: where she is)
        printf("NVMM anju result=%d pos=%.0f,%.0f,%.0f\n", this->scheduleResult, this->actor.world.pos.x,
               this->actor.world.pos.y, this->actor.world.pos.z);
        fflush(stdout);
    }
    if (this->scheduleResult == ANJU_SCH_NONE) {
        return;
    }
    this->skelAnime.playSpeed = this->animPlaySpeed;
    SkelAnime_Update(&this->skelAnime);
    NvMmAn_UpdateFace(this);
    // (MM's SubS_Offer, on screen: her talk offer)
    if ((this->func == NvMmAn_FollowSchedule) && (this->stateFlags & STATE_OFFER) &&
        (this->actor.flags & ACTOR_FLAG_ATTENTION_ENABLED)) {
        Actor_OfferTalkExchange(&this->actor, play, this->offerRange, 30.0f, EXCH_ITEM_NONE);
    }
    if (!(this->stateFlags & STATE_IGNORE_GRAVITY) && (this->actor.gravity != 0.0f)) {
        Actor_MoveXZGravity(&this->actor);
        Actor_UpdateBgCheckInfo(play, &this->actor, 30.0f, 12.0f, 0.0f, 4);
    }
    Actor_SetFocus(&this->actor, 60.0f);
    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

/** MM's EnAn_PostLimbDraw / EnAn_DrawAccessory: what she holds, in her right hand. */
static void NvMmAn_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    NvMmAn* this = (NvMmAn*)thisx;

    if (limbIndex != ANJU1_LIMB_RIGHT_HAND) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    if (this->stateFlags & STATE_DRAW_TRAY) {
        this->trayScroll1++;
        this->trayScroll2 -= 2;
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPSegment(POLY_XLU_DISP++, 0x08,
                   Gfx_TwoTexScroll(play->state.gfxCtx, 0, this->trayScroll1, 0, 16, 16, 1, 0, this->trayScroll2, 16,
                                    16));
        gSPDisplayList(POLY_XLU_DISP++, DL_TRAY);
    }
    if (this->stateFlags & STATE_DRAW_CHOPSTICKS) {
        gSPDisplayList(POLY_OPA_DISP++, DL_CHOPSTICKS);
    }
    if (this->stateFlags & STATE_DRAW_UMBRELLA) {
        gSPDisplayList(POLY_OPA_DISP++, DL_UMBRELLA);
    }
    if (this->stateFlags & STATE_DRAW_BROOM) {
        gSPDisplayList(POLY_OPA_DISP++, DL_BROOM);
    }
    CLOSE_DISPS(play->state.gfxCtx);
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
