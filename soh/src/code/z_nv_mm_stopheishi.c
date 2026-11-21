/*
 * Nightmareverse: Majora's Mask's gate guards in Clock Town, from MM's En_Stop_heishi (zeldaret/mm,
 * ovl_En_Stop_heishi), ported closely (mm/README.md). Their model is MM's object_sdn, drawn by path.
 *
 * A guard stands in a gate. While Link may not leave, he steps sideways to stay in Link's way
 * (within his reach, rotZ * 40 + 50 from where he stands, his collider tall and wide), and turns
 * back to his post when Link walks off. Talking to him with a sword (his second line) lets Link
 * through: MM's week event 12_20, after which he stands aside with a small collider. His lines are
 * by gate (params >> 12), day, night, and whether Link has a sword. On the final day he stands
 * hand on chest; outside that, hand on hip by day and looking down at night.
 *
 * Adapted from MM:
 * - the day and night are Termina's clock; the week event is Termina's (z_nv_mm_scene.c);
 * - Link is always MM's human form here (no transformations yet), and there's no Takkuri to have
 *   stolen his sword, so those branches are left out;
 * - on the final day MM turns his head about at each earthquake; there are none yet, so he looks
 *   about once (MM's own start) and then waits;
 * - his footstep sound is MM's own, not in OoT's banks; OoT's collider, movement and ground-check
 *   calls for MM's.
 */
#include "global.h"
#include <math.h>

#define ENSTOPHEISHI_GET_SWITCH_FLAG(thisx) (((thisx)->params) & 0x3F)
#define ENSTOPHEISHI_GET_GATE(thisx) ((((thisx)->params) >> 0xC) & 0xF)
#define ENSTOPHEISHI_SWITCH_FLAG_NONE 0x7F
#define SOLDIER_LIMB_HEAD 16
#define SOLDIER_LIMB_MAX 17
#define WEEKEVENTREG_12_20 ((12 << 8) | 0x20) // talked to a gate guard with a sword: he lets Link out
#define NV_MM_TEXT(id) (0xB000 + (id))

#define MM_SDN(name) ("__OTR__mm/objects/object_sdn/" name)
static const char sSkel[] = MM_SDN("gSoldierSkel");

enum {
    SOLDIER_ANIM_LOOK_DOWN,
    SOLDIER_ANIM_COME_UP_HERE,
    SOLDIER_ANIM_STAND_HAND_ON_HIP,
    SOLDIER_ANIM_STAND_LOOK_DOWN,
    SOLDIER_ANIM_4, // (stepping into the way)
    SOLDIER_ANIM_5, // (sidestepping)
    SOLDIER_ANIM_6, // (planted in the way)
    SOLDIER_ANIM_STAND_HAND_ON_CHEST,
    SOLDIER_ANIM_MAX
};

static const char* sAnimations[SOLDIER_ANIM_MAX] = {
    MM_SDN("gSoldierLookDownAnim"),       MM_SDN("gSoldierComeUpHereAnim"),  MM_SDN("gSoldierStandHandOnHipAnim"),
    MM_SDN("gSoldierStandAndLookDownAnim"), MM_SDN("object_sdn_Anim_0057BC"), MM_SDN("object_sdn_Anim_005D28"),
    MM_SDN("object_sdn_Anim_0064C0"),     MM_SDN("gSoldierStandWithHandOnChestAnim"),
};

// MM's lines, a pair (the line, and the one after it) per gate x night x what Link is
static u16 sLeaveMessages[] = {
    0x0516, 0x0517, 0x051A, 0x0000, 0x051C, 0x0000, 0x051E, 0x0000, 0x0520, 0x0521, 0x0524, 0x0000, 0x0526, 0x0000,
    0x0528, 0x0000, 0x052A, 0x052B, 0x052E, 0x0000, 0x0530, 0x0000, 0x0532, 0x0000, 0x0534, 0x0535, 0x0538, 0x0000,
    0x053A, 0x0000, 0x053C, 0x0000, 0x0518, 0x0519, 0x051B, 0x0000, 0x051D, 0x0000, 0x051F, 0x0000, 0x0522, 0x0523,
    0x0525, 0x0000, 0x0527, 0x0000, 0x0529, 0x0000, 0x052C, 0x052D, 0x052F, 0x0000, 0x0531, 0x0000, 0x0533, 0x0000,
    0x0536, 0x0537, 0x0539, 0x0000, 0x053B, 0x0000, 0x053D, 0x0000, 0x0514, 0x0000, 0x0560, 0x0000, 0x0562, 0x0000,
    0x0564, 0x0000, 0x0515, 0x0000, 0x0561, 0x0000, 0x0563, 0x0000, 0x0565, 0x0000,
};

static u16 sThirdDayLeaveMessages[] = {
    0x0540, 0x0541, 0x0542, 0x0000, 0x0543, 0x0000, 0x0543, 0x0000, 0x0547, 0x0548, 0x0549, 0x0000, 0x054A, 0x0000,
    0x054B, 0x0000, 0x054F, 0x0550, 0x0551, 0x0000, 0x0552, 0x0000, 0x0553, 0x0000, 0x0557, 0x0558, 0x0559, 0x0000,
    0x055A, 0x0000, 0x055A, 0x0000, 0x0544, 0x0545, 0x0546, 0x0000, 0x0546, 0x0000, 0x0546, 0x0000, 0x054C, 0x054D,
    0x054E, 0x0000, 0x054E, 0x0000, 0x054E, 0x0000, 0x0554, 0x0555, 0x0556, 0x0000, 0x0556, 0x0000, 0x0556, 0x0000,
    0x055B, 0x055C, 0x055D, 0x0000, 0x055D, 0x0000, 0x055D, 0x0000, 0x053E, 0x0000, 0x053E, 0x0000, 0x053E, 0x0000,
    0x053E, 0x0000, 0x053F, 0x0000, 0x053F, 0x0000, 0x053F, 0x0000, 0x053F, 0x0000,
};

struct NvMmStopheishi;
typedef void (*NvMmStopheishiFunc)(struct NvMmStopheishi*, PlayState*);

typedef struct NvMmStopheishi {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[SOLDIER_LIMB_MAX];
    Vec3s morphTable[SOLDIER_LIMB_MAX];
    NvMmStopheishiFunc func;
    s16 headRotZ;
    s16 headRotX;
    s16 pitchToPlayer;
    s16 headRotXTarget;
    u8 atPost;       // back at his post (MM's unk_264)
    u8 letThrough;   // this guard has let Link through (MM's unk_265)
    s32 animIndex;
    f32 animEndFrame;
    s16 timer;
    s16 headTurnTimer1;
    s16 blockState;  // MM's unk_274: the steps of getting in the way
    s16 gate;        // MM's unk_276
    s16 disableCollider;
    s16 switchFlag;
    s16 messageIndex;
    u16 nextText;    // MM's unk_27E: the line after this one
    s16 rotYTarget;
    s32 headTurnTimer2;
    f32 reach;       // MM's unk_288: how far he steps aside
    f32 maxMoveStep;
    Vec3f headWorldPos;
    ColliderCylinder collider;
} NvMmStopheishi;

const size_t gNvMmStopheishiSize = sizeof(NvMmStopheishi);

s32 NvMmClock_Day(void);
u16 NvMmClock_Time(void);
s32 NvMm_CheckWeekEvent(s32 flag);
void NvMm_SetWeekEvent(s32 flag);

static s32 NvMmStopheishi_Night(void) {
    u16 t = NvMmClock_Time();

    return (t >= 0xC000) || (t < 0x4000);
}

static ColliderCylinderInit sCylinderInit = {
    { COLTYPE_NONE, AT_NONE, AC_NONE, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_2, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0xF7CFFFFF, 0x00, 0x00 }, TOUCH_NONE | TOUCH_SFX_NORMAL,
      BUMP_NONE, OCELEM_ON },
    { 50, 260, 0, { 0, 0, 0 } },
};

static void NvMmStopheishi_ChangeAnim(NvMmStopheishi* this, s32 anim) {
    s32 mode = ANIMMODE_ONCE;
    f32 morph = -10.0f;

    this->animIndex = anim;
    this->animEndFrame = Animation_GetLastFrame((AnimationHeader*)sAnimations[anim]);
    if ((anim >= SOLDIER_ANIM_STAND_HAND_ON_HIP) && (anim != SOLDIER_ANIM_4)) {
        mode = ANIMMODE_LOOP;
    }
    if (anim == SOLDIER_ANIM_5) {
        morph = 0.0f;
    }
    Animation_Change(&this->skelAnime, (AnimationHeader*)sAnimations[anim], 1.0f, 0.0f, this->animEndFrame, mode,
                     morph);
}

/** His standing pose for the time: hand on hip, looking down at night, hand on chest on the final day. */
static void NvMmStopheishi_StandPose(NvMmStopheishi* this) {
    NvMmStopheishi_ChangeAnim(this, SOLDIER_ANIM_STAND_HAND_ON_HIP);
    if ((NvMmClock_Day() != 3) && NvMmStopheishi_Night()) {
        NvMmStopheishi_ChangeAnim(this, SOLDIER_ANIM_STAND_LOOK_DOWN);
    }
    if (NvMmClock_Day() == 3) {
        NvMmStopheishi_ChangeAnim(this, SOLDIER_ANIM_STAND_HAND_ON_CHEST);
    }
}

static void NvMmStopheishi_Guard(NvMmStopheishi* this, PlayState* play);
static void NvMmStopheishi_Talking(NvMmStopheishi* this, PlayState* play);

static void NvMmStopheishi_SetupGuard(NvMmStopheishi* this) {
    NvMmStopheishi_StandPose(this);
    this->blockState = 0;
    this->disableCollider = false;
    this->func = NvMmStopheishi_Guard;
    this->actor.speedXZ = 0.0f;
}

void NvMmStopheishi_Init(Actor* thisx, PlayState* play) {
    NvMmStopheishi* this = (NvMmStopheishi*)thisx;

    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 25.0f);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sSkel,
                       (AnimationHeader*)sAnimations[SOLDIER_ANIM_STAND_HAND_ON_HIP], this->jointTable,
                       this->morphTable, SOLDIER_LIMB_MAX);
    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    this->gate = ENSTOPHEISHI_GET_GATE(&this->actor);
    this->switchFlag = ENSTOPHEISHI_GET_SWITCH_FLAG(&this->actor);
    this->reach = (this->actor.world.rot.z * 40.0f) + 50.0f;
    this->actor.world.rot.z = 0;
    this->actor.shape.rot.z = 0;
    if (this->switchFlag == ENSTOPHEISHI_SWITCH_FLAG_NONE) {
        this->switchFlag = -1;
    }
    if ((this->switchFlag >= 0) && Flags_GetSwitch(play, this->switchFlag)) {
        Actor_Kill(&this->actor);
        return;
    }
    this->actor.targetMode = 0;
    this->actor.gravity = -3.0f;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    this->rotYTarget = this->actor.world.rot.y;
    this->headTurnTimer2 = 6;
    NvMmStopheishi_SetupGuard(this);
}

void NvMmStopheishi_Destroy(Actor* thisx, PlayState* play) {
    NvMmStopheishi* this = (NvMmStopheishi*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

static void NvMmStopheishi_HeadNormal(NvMmStopheishi* this, PlayState* play) {
    s32 yawDiff = ABS(BINANG_SUB(this->actor.yawTowardsPlayer, this->actor.world.rot.y));

    this->headRotXTarget = 0;
    if (this->actor.xzDistToPlayer < 200.0f) {
        Player* player = GET_PLAYER(play);
        Vec3f playerPos;

        if (yawDiff < 0x4E20) {
            this->headRotXTarget = CLAMP((s16)(this->actor.yawTowardsPlayer - this->actor.world.rot.y), -0x2710, 0x2710);
        }
        Math_Vec3f_Copy(&playerPos, &player->actor.world.pos);
        playerPos.y += 57.0f; // (MM's human or Deku height; the forms aren't in yet)
        this->pitchToPlayer = Math_Vec3f_Pitch(&this->actor.focus.pos, &playerPos);
    }
}

/** The final day: he looks about, nervous (MM: again at each earthquake; there are none yet). */
static void NvMmStopheishi_HeadThirdDay(NvMmStopheishi* this) {
    if (this->headTurnTimer1 != 0) {
        this->headTurnTimer1--;
    }
    this->pitchToPlayer = -0xBB8;
    this->headRotXTarget = 0;
    if (this->headTurnTimer2 < 6) {
        this->headRotXTarget = 0x1770;
        if (((u32)this->headTurnTimer2 % 8) == 0) {
            this->headRotXTarget = -0x1770;
        }
        if (this->headTurnTimer1 == 0) {
            this->headTurnTimer2++;
            this->headTurnTimer1 = 10;
            if (this->headTurnTimer2 >= 6) {
                this->headTurnTimer1 = 50;
            }
        }
    }
}

/** Link has walked off: back to his post, and his standing pose. */
static void NvMmStopheishi_ReturnToPost(NvMmStopheishi* this) {
    this->blockState = 0;
    this->maxMoveStep = 0.0f;
    this->rotYTarget = this->actor.home.rot.y;
    if (this->timer == 1) {
        this->pitchToPlayer = 0;
        NvMmStopheishi_ChangeAnim(this, SOLDIER_ANIM_5);
    }
    if ((this->timer == 0) && (this->atPost == 0)) {
        if ((fabsf(this->actor.world.pos.x - this->actor.home.pos.x) > 3.0f) ||
            (fabsf(this->actor.world.pos.z - this->actor.home.pos.z) > 3.0f)) {
            this->skelAnime.playSpeed = 3.0f;
            Math_ApproachF(&this->actor.world.pos.x, this->actor.home.pos.x, 0.3f, 5.0f);
            Math_ApproachF(&this->actor.world.pos.z, this->actor.home.pos.z, 0.3f, 5.0f);
        } else {
            this->atPost = 1;
        }
    }
    if ((this->animIndex != SOLDIER_ANIM_STAND_HAND_ON_HIP) && (this->animIndex != SOLDIER_ANIM_STAND_LOOK_DOWN) &&
        (this->animIndex != SOLDIER_ANIM_STAND_HAND_ON_CHEST) && (this->atPost != 0)) {
        this->skelAnime.playSpeed = 1.0f;
        NvMmStopheishi_StandPose(this);
    }
}

/** Link is near the gate: step sideways along it to stay in his way, within reach of the post. */
static void NvMmStopheishi_Block(NvMmStopheishi* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 frame = this->skelAnime.curFrame;
    Vec3f newPos;
    f32 xDiff;
    f32 zDiff;
    f32 speed;
    s16 yaw;
    s16 rotY;

    if (ABS(BINANG_SUB(this->actor.yawTowardsPlayer, this->actor.home.rot.y)) < 0x2000) {
        this->rotYTarget = this->actor.yawTowardsPlayer;
    }
    speed = fabsf((f32)this->actor.yawTowardsPlayer - Math_Vec3f_Yaw(&this->actor.home.pos, &player->actor.world.pos)) *
            0.01f * 5.0f;
    if (this->blockState != 4) {
        this->skelAnime.playSpeed = CLAMP(speed, 1.0f, 5.0f);
    }
    yaw = Math_Vec3f_Yaw(&this->actor.home.pos, &player->actor.world.pos);
    xDiff = fabsf(this->actor.home.pos.x - player->actor.world.pos.x);
    zDiff = fabsf(this->actor.home.pos.z - player->actor.world.pos.z);
    Math_Vec3f_Copy(&newPos, &this->actor.home.pos);
    rotY = this->actor.home.rot.y;
    if (rotY == -0x8000) {
        if (yaw > 0) {
            xDiff = Math_SinS(this->actor.home.rot.y + 0xC000) * xDiff;
            zDiff = Math_CosS(this->actor.home.rot.y + 0xC000) * zDiff;
        } else {
            xDiff = Math_SinS(this->actor.home.rot.y + 0x4000) * xDiff;
            zDiff = Math_CosS(this->actor.home.rot.y + 0x4000) * zDiff;
        }
    } else if (yaw < rotY) {
        xDiff = Math_SinS(this->actor.home.rot.y + 0xC000) * xDiff;
        zDiff = Math_CosS(this->actor.home.rot.y + 0xC000) * zDiff;
    } else {
        xDiff = Math_SinS(this->actor.home.rot.y + 0x4000) * xDiff;
        zDiff = Math_CosS(this->actor.home.rot.y + 0x4000) * zDiff;
    }
    xDiff = CLAMP(xDiff, -this->reach, this->reach);
    zDiff = CLAMP(zDiff, -this->reach, this->reach);
    newPos.x += xDiff;
    newPos.z += zDiff;
    Math_ApproachF(&this->actor.world.pos.x, newPos.x, 0.5f, this->maxMoveStep);
    Math_ApproachF(&this->actor.world.pos.z, newPos.z, 0.5f, this->maxMoveStep);
    Math_ApproachF(&this->maxMoveStep, 50.0f, 0.3f, 1.0f);
    switch (this->blockState) {
        case 0:
            if (NvMmClock_Day() != 3) {
                NvMmStopheishi_ChangeAnim(this, SOLDIER_ANIM_4);
                this->blockState = 2;
            } else {
                NvMmStopheishi_ChangeAnim(this, SOLDIER_ANIM_STAND_HAND_ON_HIP);
                this->blockState = 1;
            }
            break;
        case 1:
            NvMmStopheishi_ChangeAnim(this, SOLDIER_ANIM_4);
            this->blockState = 2;
            break;
        case 2:
            if (frame >= this->animEndFrame) {
                NvMmStopheishi_ChangeAnim(this, SOLDIER_ANIM_5);
                this->blockState = 3;
            }
            break;
        case 3:
            if ((fabsf(this->actor.world.pos.x - newPos.x) < 2.0f) && (fabsf(this->actor.world.pos.z - newPos.z) < 2.0f)) {
                this->skelAnime.playSpeed = 1.0f;
                NvMmStopheishi_ChangeAnim(this, SOLDIER_ANIM_6);
                this->blockState = 4;
            }
            break;
        case 4:
            if ((fabsf(this->actor.world.pos.x - newPos.x) > 4.0f) || (fabsf(this->actor.world.pos.z - newPos.z) > 4.0f)) {
                NvMmStopheishi_ChangeAnim(this, SOLDIER_ANIM_5);
                this->blockState = 3;
            }
            break;
    }
    this->atPost = 0;
    this->timer = 20;
    this->pitchToPlayer = 0;
    if (this->gate == 3) {
        Math_Vec3f_Copy(&newPos, &player->actor.world.pos);
        newPos.y += 77.0f;
        this->pitchToPlayer = Math_Vec3f_Pitch(&this->actor.focus.pos, &newPos);
    }
}

static void NvMmStopheishi_Guard(NvMmStopheishi* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s16 night = NvMmStopheishi_Night() ? 0x20 : 0;
    s16 who = 0; // MM: 0 a human Link who may not leave yet, 1 one who may, 2-4 the forms
    s32 yawDiffAbs;

    SkelAnime_Update(&this->skelAnime);
    if (NvMmClock_Day() != 3) {
        NvMmStopheishi_HeadNormal(this, play);
    } else {
        NvMmStopheishi_HeadThirdDay(this);
    }
    Math_SmoothStepToS(&this->actor.world.rot.y, this->rotYTarget, 1, 0x1388, 0);
    if (NvMm_CheckWeekEvent(WEEKEVENTREG_12_20)) {
        who = 1;
    }
    if ((who == 1) && ((this->animIndex == SOLDIER_ANIM_4) || (this->animIndex == SOLDIER_ANIM_5) ||
                       (this->animIndex == SOLDIER_ANIM_6))) {
        NvMmStopheishi_StandPose(this);
    }
    if (who == 0) {
        f32 xDiff = this->actor.home.pos.x - player->actor.world.pos.x;
        f32 zDiff = this->actor.home.pos.z - player->actor.world.pos.z;

        this->collider.dim.radius = 50;
        this->collider.dim.height = 260;
        if (sqrtf(SQ(xDiff) + SQ(zDiff)) < 240.0f) {
            NvMmStopheishi_Block(this, play);
        } else {
            NvMmStopheishi_ReturnToPost(this);
        }
    } else {
        this->collider.dim.radius = 15;
        this->collider.dim.height = 60;
    }
    this->messageIndex = this->gate * 8 + night + who * 2;
    if (this->messageIndex > (s32)ARRAY_COUNT(sLeaveMessages) - 2) {
        this->messageIndex = 0;
    }
    if (NvMmClock_Day() != 3) {
        this->actor.textId = NV_MM_TEXT(sLeaveMessages[this->messageIndex]);
        this->nextText = sLeaveMessages[this->messageIndex + 1];
    } else {
        this->actor.textId = NV_MM_TEXT(sThirdDayLeaveMessages[this->messageIndex]);
        this->nextText = sThirdDayLeaveMessages[this->messageIndex + 1];
    }
    if ((this->nextText != 0) && (CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_NONE)) {
        // no sword: MM's lines for a boy without one
        static const u16 sNoSword[][2] = { { 0x516, 0x55E }, { 0x520, 0x566 }, { 0x52A, 0x568 }, { 0x534, 0x56A },
                                           { 0x518, 0x55F }, { 0x522, 0x567 }, { 0x52C, 0x569 }, { 0x536, 0x56B } };
        s32 k;

        for (k = 0; k < ARRAY_COUNT(sNoSword); k++) {
            if (this->actor.textId == NV_MM_TEXT(sNoSword[k][0])) {
                this->actor.textId = NV_MM_TEXT(sNoSword[k][1]);
                break;
            }
        }
    }
    yawDiffAbs = ABS((s16)(this->actor.yawTowardsPlayer - this->actor.world.rot.y));
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        this->skelAnime.playSpeed = 1.0f;
        if ((this->letThrough || NvMm_CheckWeekEvent(WEEKEVENTREG_12_20)) &&
            (this->animIndex != SOLDIER_ANIM_STAND_HAND_ON_HIP)) {
            NvMmStopheishi_ChangeAnim(this, SOLDIER_ANIM_STAND_HAND_ON_HIP);
        }
        this->pitchToPlayer = 0;
        this->blockState = 0;
        this->disableCollider = true;
        this->func = NvMmStopheishi_Talking;
    } else if (yawDiffAbs < 0x4BB9) {
        Actor_OfferTalk(&this->actor, play, 70.0f);
    }
}

/** Talking: his second line (with a sword) lets Link out from now on. */
static void NvMmStopheishi_Talking(NvMmStopheishi* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    NvMmStopheishi_HeadNormal(this, play);
    if ((Message_GetState(&play->msgCtx) == TEXT_STATE_EVENT) && Message_ShouldAdvance(play)) {
        if ((this->nextText != 0) && (CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) != EQUIP_VALUE_SWORD_NONE)) {
            this->actor.textId = NV_MM_TEXT(this->nextText);
            Message_ContinueTextbox(play, this->actor.textId);
            NvMmStopheishi_ChangeAnim(this, SOLDIER_ANIM_STAND_HAND_ON_HIP);
            NvMm_SetWeekEvent(WEEKEVENTREG_12_20);
            this->letThrough = 1;
            this->nextText = 0;
        } else {
            Message_CloseTextbox(play);
            NvMmStopheishi_SetupGuard(this);
        }
    }
}

void NvMmStopheishi_Update(Actor* thisx, PlayState* play) {
    NvMmStopheishi* this = (NvMmStopheishi*)thisx;

    if (this->timer != 0) {
        this->timer--;
    }
    this->actor.shape.rot.y = this->actor.world.rot.y;
    Math_SmoothStepToS(&this->headRotX, this->headRotXTarget, 1, 0x7D0, 0);
    Math_SmoothStepToS(&this->headRotZ, this->pitchToPlayer, 1, 0x7D0, 0);
    this->func(this, play);
    Actor_MoveXZGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 20.0f, 20.0f, 50.0f, 0x1D);
    Actor_SetScale(&this->actor, 0.01f);
    this->actor.uncullZoneForward = 500.0f;
    Math_Vec3f_Copy(&this->actor.focus.pos, &this->headWorldPos);
    this->actor.focus.rot = this->actor.world.rot;
    if (!this->disableCollider) {
        Collider_UpdateCylinder(&this->actor, &this->collider);
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    }
}

static s32 NvMmStopheishi_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                           void* thisx) {
    NvMmStopheishi* this = (NvMmStopheishi*)thisx;

    if (limbIndex == SOLDIER_LIMB_HEAD) {
        rot->x += this->headRotX;
        rot->z += this->headRotZ;
    }
    return false;
}

static void NvMmStopheishi_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    static Vec3f sZero = { 0.0f, 0.0f, 0.0f };
    NvMmStopheishi* this = (NvMmStopheishi*)thisx;

    if (limbIndex == SOLDIER_LIMB_HEAD) {
        Matrix_MultVec3f(&sZero, &this->headWorldPos);
    }
}

void NvMmStopheishi_Draw(Actor* thisx, PlayState* play) {
    NvMmStopheishi* this = (NvMmStopheishi*)thisx;

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          NvMmStopheishi_OverrideLimbDraw, NvMmStopheishi_PostLimbDraw, &this->actor);
}
