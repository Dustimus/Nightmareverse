/*
 * Nightmareverse: Majora's Mask's carpenters, from MM's En_Daiku (zeldaret/mm, ovl_En_Daiku), ported
 * closely (mm/README.md: MM's actors keep MM's logic, adapted only where OoT's engine differs). Their
 * model is MM's own, object_daiku, drawn by path from mm.o2r.
 *
 * Types (params & 0xFF):
 *   0  up on the carnival scaffolding: idle, no talking; gone once the mayor's meeting is settled or
 *      on the final night (Clock Town's six, in its main layout);
 *   1  standing, idle;
 *   2  hammering (two animations in turn);
 *   3  walking a path (params >> 8, the scene's path), turning to face Link.
 * Each wears its own hat (a display list on limb 0x0F) and colour (the env colour); type 3 carries a
 * plank (on limb 8).
 *
 * Adapted from MM:
 * - the day and night are Termina's clock (z_nv_mm_clock.c);
 * - OoT's collider, movement and ground-check calls for MM's (the same values);
 * - their lines are MM's own text (NvMmText.cpp: MM text id + 0xB000); Kafei's mask's lines aren't
 *   in yet (no Kafei's Mask);
 * - paths: MM's SubS_GetPathByIndex / SubS_CopyPointFromPath as two small functions on OoT's
 *   setupPathList (the scene's paths, mm_scene.py);
 * - the mayor's meeting has no flag yet, so type 0 stays until the final night.
 */
#include "global.h"
#include <math.h>

#define ENDAIKU_GET_TYPE(thisx) ((thisx)->params & 0xFF)
#define ENDAIKU_GET_PATH_INDEX(thisx) (((thisx)->params >> 8) & 0xFF)
#define DAIKU_LIMB_HEAD 0x0F
#define DAIKU_LIMB_HAND 0x08
#define DAIKU_LIMB_MAX 0x11

#define MM_DAIKU(name) ("__OTR__mm/objects/object_daiku/" name)
static const char sSkel[] = MM_DAIKU("object_daiku_Skel_00A850");

enum {
    DAIKU_ANIM_0, DAIKU_ANIM_1, DAIKU_ANIM_2, DAIKU_ANIM_3, DAIKU_ANIM_4,
    DAIKU_ANIM_5, DAIKU_ANIM_6, DAIKU_ANIM_7, DAIKU_ANIM_8, DAIKU_ANIM_MAX
};

static const char* sAnimations[DAIKU_ANIM_MAX] = {
    MM_DAIKU("object_daiku_Anim_002FA0"), MM_DAIKU("object_daiku_Anim_00ACD0"), MM_DAIKU("object_daiku_Anim_00C92C"),
    MM_DAIKU("object_daiku_Anim_000C44"), MM_DAIKU("object_daiku_Anim_00C234"), MM_DAIKU("object_daiku_Anim_000600"),
    MM_DAIKU("object_daiku_Anim_001114"), MM_DAIKU("object_daiku_Anim_00B690"), MM_DAIKU("object_daiku_Anim_00BEAC"),
};

static u8 sAnimationModes[DAIKU_ANIM_MAX] = {
    ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_ONCE,
    ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_ONCE, ANIMMODE_ONCE,
};

static const char* sHats[] = { // by type, on the head
    MM_DAIKU("object_daiku_DL_0070C0"), MM_DAIKU("object_daiku_DL_006FB0"),
    MM_DAIKU("object_daiku_DL_006E80"), MM_DAIKU("object_daiku_DL_006D70"),
};
static const char sPlank[] = MM_DAIKU("object_daiku_DL_008EC8");

struct NvMmDaiku;
typedef void (*NvMmDaikuFunc)(struct NvMmDaiku*, PlayState*);

typedef struct NvMmDaiku {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[DAIKU_LIMB_MAX];
    Vec3s morphTable[DAIKU_LIMB_MAX];
    NvMmDaikuFunc func;
    ColliderCylinder collider;
    s16 headRotZ;    // the head's turn, eased towards its target (MM's unk_25E / unk_264)
    s16 headRotX;    // (unk_260 / unk_266)
    s16 headRotZTarget;
    s16 headRotXTarget;
    s16 animDelay;   // frames before the animation starts (type 0: 4)
    s16 yaw;         // the yaw it walks or faces towards
    s16 targetYaw;
    s32 type;
    f32 animEndFrame;
    Vec3f pathPoint;  // the point it walks to (at first, where it stands)
    Path* path;
    s16 pathIndex;
    s16 pathPointIndex;
} NvMmDaiku;

const size_t gNvMmDaikuSize = sizeof(NvMmDaiku);

s32 NvMmClock_Day(void);
u16 NvMmClock_Time(void);

static s32 NvMmDaiku_FinalNight(void) {
    u16 t = NvMmClock_Time();

    return (NvMmClock_Day() == 3) && ((t >= 0xC000) || (t < 0x4000));
}

#define ENDAIKU_PATH_INDEX_NONE 0x3F

/** MM's SubS_GetPathByIndex: the scene's path, or none. */
static Path* NvMmDaiku_GetPath(PlayState* play, s16 index, s16 none) {
    return ((index != none) && (play->setupPathList != NULL)) ? &play->setupPathList[index] : NULL;
}

/** MM's SubS_CopyPointFromPath: the path's point i, false past its end. */
static s32 NvMmDaiku_CopyPoint(Path* path, s32 i, Vec3f* dst) {
    Vec3s* points;

    if ((path == NULL) || (i >= path->count)) {
        return false;
    }
    points = (Vec3s*)SEGMENTED_TO_VIRTUAL(path->points);
    dst->x = points[i].x;
    dst->y = points[i].y;
    dst->z = points[i].z;
    return true;
}

/** The next point of its path (MM: a carpenter whose path runs out is removed). */
static void NvMmDaiku_NextPoint(NvMmDaiku* this) {
    if ((this->pathIndex != ENDAIKU_PATH_INDEX_NONE) && (this->path != NULL)) {
        if (!NvMmDaiku_CopyPoint(this->path, this->pathPointIndex, &this->pathPoint)) {
            Actor_Kill(&this->actor);
        }
    }
}

#define NV_MM_TEXT(id) (0xB000 + (id)) // MM's text id, as Termina's text box takes it (NvMmText.cpp)

// each day's line, by day (1-3) and type (MM's sTextIds)
static u16 sTextIds[] = {
    0x061C, 0x061A, 0x061B, 0x061C, 0x061C, 0x061D, 0x061E, 0x061F, 0x061C, 0x0620, 0x0621, 0x0622,
};

static ColliderCylinderInit sCylinderInit = {
    { COLTYPE_NONE, AT_NONE, AC_NONE, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_2, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0xF7CFFFFF, 0x00, 0x00 }, TOUCH_NONE | TOUCH_SFX_NORMAL,
      BUMP_NONE, OCELEM_ON },
    { 20, 60, 0, { 0, 0, 0 } },
};

static void NvMmDaiku_ChangeAnim(NvMmDaiku* this, s32 anim) {
    this->animEndFrame = Animation_GetLastFrame((AnimationHeader*)sAnimations[anim]);
    Animation_Change(&this->skelAnime, (AnimationHeader*)sAnimations[anim], 1.0f, 0.0f, this->animEndFrame,
                     sAnimationModes[anim], -4.0f);
}

static void NvMmDaiku_Idle(NvMmDaiku* this, PlayState* play);
static void NvMmDaiku_Talking(NvMmDaiku* this, PlayState* play);

static void NvMmDaiku_SetText(NvMmDaiku* this) {
    s32 i = (NvMmClock_Day() - 1) * 4 + this->type;

    if ((i >= 0) && (i < ARRAY_COUNT(sTextIds))) {
        this->actor.textId = NV_MM_TEXT(sTextIds[i]);
    }
}

static void NvMmDaiku_SetupIdle(NvMmDaiku* this) {
    switch (this->type) {
        case 0:
        case 1:
            NvMmDaiku_ChangeAnim(this, DAIKU_ANIM_0);
            break;
        case 2:
            NvMmDaiku_ChangeAnim(this, DAIKU_ANIM_7);
            break;
        case 3:
            NvMmDaiku_ChangeAnim(this, DAIKU_ANIM_6);
            break;
        default:
            break;
    }
    NvMmDaiku_SetText(this);
    this->yaw = this->actor.shape.rot.y;
    this->func = NvMmDaiku_Idle;
}

void NvMmDaiku_Init(Actor* thisx, PlayState* play) {
    NvMmDaiku* this = (NvMmDaiku*)thisx;
    const char* startAnim = sAnimations[DAIKU_ANIM_0];

    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 40.0f);
    this->actor.targetMode = 0;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    this->type = ENDAIKU_GET_TYPE(&this->actor);
    if (this->type == 3) {
        this->pathIndex = ENDAIKU_GET_PATH_INDEX(&this->actor);
        this->path = NvMmDaiku_GetPath(play, this->pathIndex, ENDAIKU_PATH_INDEX_NONE);
    } else if (this->type == 2) {
        this->headRotZTarget = -2000;
    }
    if (this->type == 0) {
        this->collider.dim.radius = 30;
        this->collider.dim.height = 60;
        this->collider.dim.yShift = 0;
        this->actor.flags |= ACTOR_FLAG_LOCK_ON_DISABLED;
    }
    if (NvMmDaiku_FinalNight()) { // (type 0 also leaves once the mayor's meeting is settled: no flag yet)
        Actor_Kill(&this->actor);
        return;
    }
    Math_Vec3f_Copy(&this->pathPoint, &this->actor.world.pos);
    this->targetYaw = this->actor.world.rot.y;
    this->actor.gravity = -3.0f;
    switch (this->type) {
        case 0:
            this->animDelay = this->type * 4 + 4;
            break;
        case 2:
            startAnim = sAnimations[DAIKU_ANIM_7];
            break;
        case 3:
            startAnim = sAnimations[DAIKU_ANIM_6];
            break;
        default:
            break;
    }
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sSkel, (AnimationHeader*)startAnim,
                       this->jointTable, this->morphTable, DAIKU_LIMB_MAX);
    NvMmDaiku_SetupIdle(this);
}

void NvMmDaiku_Destroy(Actor* thisx, PlayState* play) {
    NvMmDaiku* this = (NvMmDaiku*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

static void NvMmDaiku_Idle(NvMmDaiku* this, PlayState* play) {
    f32 frame = this->skelAnime.curFrame;

    NvMmDaiku_SetText(this);
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        this->func = NvMmDaiku_Talking;
        return;
    }

    if ((this->type == 2) && (frame >= this->animEndFrame)) { // hammering: one of two swings, in turn
        NvMmDaiku_ChangeAnim(this, (Rand_ZeroOne() < 0.5f) ? DAIKU_ANIM_7 : DAIKU_ANIM_8);
    }
    if (this->type == 3) { // walking its path: towards the point, then turning to the next
        f32 xzDist;
        f32 yawDiff;

        Math_ApproachF(&this->actor.world.pos.x, this->pathPoint.x, 0.5f,
                       fabsf(2.0f * Math_SinS(this->actor.world.rot.y)));
        Math_ApproachF(&this->actor.world.pos.z, this->pathPoint.z, 0.5f,
                       fabsf(2.0f * Math_CosS(this->actor.world.rot.y)));
        Math_SmoothStepToS(&this->actor.world.rot.y, this->targetYaw, 1, 0x7D0, 0xA);
        xzDist = sqrtf(SQ(this->actor.world.pos.x - this->pathPoint.x) + SQ(this->actor.world.pos.z - this->pathPoint.z));
        yawDiff = fabsf(this->actor.world.rot.y - this->targetYaw);
        if ((xzDist < 4.0f) && (this->path != NULL) && (yawDiff < 10.0f)) {
            this->pathPointIndex++;
            if (this->pathPointIndex >= this->path->count) {
                this->pathPointIndex = 0;
            }
            NvMmDaiku_NextPoint(this);
            this->targetYaw = Math_Vec3f_Yaw(&this->actor.world.pos, &this->pathPoint);
        }
    }
    if (this->type != 0) {
        s16 angle = ABS(BINANG_SUB(this->actor.yawTowardsPlayer, this->actor.world.rot.y));

        this->yaw = this->actor.yawTowardsPlayer;
        if ((this->type == 1) || (this->type == 2) || (angle <= 0x2890)) {
            Actor_OfferTalk(&this->actor, play, 100.0f);
        }
    }
}

/** Talking: the hammering goes on; the line's end (its event code) closes it. */
static void NvMmDaiku_Talking(NvMmDaiku* this, PlayState* play) {
    f32 frame = this->skelAnime.curFrame;

    if ((this->type == 2) && (frame >= this->animEndFrame)) {
        NvMmDaiku_ChangeAnim(this, (Rand_ZeroOne() < 0.5f) ? DAIKU_ANIM_7 : DAIKU_ANIM_8);
    }
    if ((Message_GetState(&play->msgCtx) == TEXT_STATE_EVENT) && Message_ShouldAdvance(play)) {
        Message_CloseTextbox(play);
        NvMmDaiku_SetupIdle(this);
    }
}

void NvMmDaiku_Update(Actor* thisx, PlayState* play) {
    NvMmDaiku* this = (NvMmDaiku*)thisx;

    if (this->animDelay == 0) {
        SkelAnime_Update(&this->skelAnime);
    }
    if ((this->type == 0) && NvMmDaiku_FinalNight()) {
        Actor_Kill(&this->actor);
        return;
    }
    this->func(this, play);
    if (this->animDelay != 0) {
        this->animDelay--;
    }
    Actor_SetScale(&this->actor, 0.01f);
    this->actor.shape.rot.y = this->actor.world.rot.y;
    Actor_SetFocus(&this->actor, 65.0f);
    Actor_MoveXZGravity(&this->actor);
    Math_SmoothStepToS(&this->headRotX, this->headRotXTarget, 1, 0xBB8, 0);
    Math_SmoothStepToS(&this->headRotZ, this->headRotZTarget, 1, 0xBB8, 0);
    Actor_UpdateBgCheckInfo(play, &this->actor, 20.0f, 20.0f, 50.0f, 0x1D);
    this->actor.uncullZoneForward = 650.0f;
    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

static s32 NvMmDaiku_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                      void* thisx) {
    NvMmDaiku* this = (NvMmDaiku*)thisx;

    if (limbIndex == DAIKU_LIMB_HEAD) {
        rot->x += this->headRotX;
        rot->z += this->headRotZ;
    }
    return false;
}

static void NvMmDaiku_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    NvMmDaiku* this = (NvMmDaiku*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    if ((limbIndex == DAIKU_LIMB_HEAD) && (this->type >= 0) && (this->type < 4)) {
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sHats[this->type]);
    }
    if ((this->type == 3) && (limbIndex == DAIKU_LIMB_HAND)) {
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sPlank);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

void NvMmDaiku_Draw(Actor* thisx, PlayState* play) {
    static const u8 sColours[5][3] = {
        { 170, 10, 70 }, { 170, 200, 255 }, { 0, 230, 70 }, { 200, 0, 150 }, { 200, 0, 0 },
    };
    NvMmDaiku* this = (NvMmDaiku*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    if ((this->type >= 0) && (this->type < 5)) {
        gDPSetEnvColor(POLY_OPA_DISP++, sColours[this->type][0], sColours[this->type][1], sColours[this->type][2],
                       255);
    }
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          NvMmDaiku_OverrideLimbDraw, NvMmDaiku_PostLimbDraw, &this->actor);
    CLOSE_DISPS(play->state.gfxCtx);
}
