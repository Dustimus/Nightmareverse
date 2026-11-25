/*
 * Nightmareverse: Majora's Mask's Bombers, the blue-hatted kids, from MM's En_Bombers (zeldaret/mm,
 * ovl_En_Bombers), ported closely (mm/README.md). Their model is MM's object_cs, drawn by path; their
 * eyes and numbers are its textures, set in segments 8 and 9; their bandana's colour is MM's own two
 * one-line display lists (ovl_En_Bombers: red for number 1, blue for the rest), rebuilt here.
 *
 * Two kinds (params >> 4):
 *   0  out in town (East and West Clock Town, a different spot each half-day): wandering within
 *      75 of their spot, looking around, and talking. Their number is params & 0xF, plus one (the
 *      town's are 2-5; number 1 is Jim). MM's lines for a human Link: "Do you know about the Bombers
 *      Secret Society of Justice?" and on, with its yes/no; with the Bombers' Notebook, another set.
 *      They're gone once the Bombers' game has been played (week events 73_10, 85_02).
 *   1  sitting at the hideout after the game (North Clock Town), only for the ones Link caught
 *      (76_01-76_10); until then not there.
 *
 * Adapted from MM:
 * - Link is MM's human or Deku form (z_nv_mm_form.c: Deku Link gets MM's Deku lines, where Jim is;
 *   the Goron and Zora ones wait for those forms);
 *   MM's mask reactions (Text_GetFaceReaction) wait for MM's masks;
 * - the hideout's bowling-man handover (75_40) isn't in: those sit as in the other case;
 * - their footsteps are MM's own sound, not in OoT's banks; OoT's collider, movement and
 *   ground-check calls for MM's; MM's Scene_SetRenderModeXlu(play, 0, 1) as segment 0x0C pointed
 *   at an empty list, which is all that call does.
 */
#include "global.h"
#include <math.h>

#define ENBOMBERS_GET_NUMBER(thisx) ((thisx)->params & 0xF)
#define ENBOMBERS_GET_TYPE(thisx) (((thisx)->params >> 4) & 0xF)
#define ENBOMBERS_TYPE_NPC 0
#define BOMBER_LIMB_0F 0x0F
#define BOMBER_LIMB_HEAD 0x11
#define BOMBER_LIMB_13 0x13
#define BOMBER_LIMB_MAX 21
#define NV_MM_TEXT(id) (0xB000 + (id))
#define MM_WE(byte, bit) (((byte) << 8) | (bit))
#define WEEKEVENTREG_73_10 MM_WE(73, 0x10)
#define WEEKEVENTREG_85_02 MM_WE(85, 0x02)
#define WEEKEVENTREG_84_80 MM_WE(84, 0x80) // Link has the Bombers' Notebook

#define MM_CS(name) ("__OTR__mm/objects/object_cs/" name)
static const char sSkel[] = MM_CS("gBomberSkel");

enum {
    ANIM_IDLE, ANIM_TALK_HANDS_ON_HIPS, ANIM_WALK, ANIM_SURPRISE, ANIM_HIDE_UNDER_BOX, ANIM_DAZED, ANIM_SIT,
    ANIM_HOLD_CUCCO, ANIM_TURN_AROUND, ANIM_SHOW_NUMBER, ANIM_TALK_SHOWING_NUMBER, ANIM_TAUNT, ANIM_JUMP,
    ANIM_ARM_SWIPE, ANIM_SALUTE, ANIM_RUN, ANIM_LOOK_AROUND, ANIM_MAX
};

static const char* sAnimations[ANIM_MAX] = {
    MM_CS("gBomberIdleAnim"),       MM_CS("gBomberTalkHandsOnHipsAnim"), MM_CS("gBomberWalkAnim"),
    MM_CS("gBomberSurpriseAnim"),   MM_CS("gBomberHideUnderBoxAnim"),    MM_CS("gBomberDazedAnim"),
    MM_CS("gBomberSitAnim"),        MM_CS("gBomberHoldCuccoAnim"),       MM_CS("gBomberTurnAroundAnim"),
    MM_CS("gBomberShowNumberAnim"), MM_CS("gBomberTalkShowingNumberAnim"), MM_CS("gBomberTauntAnim"),
    MM_CS("gBomberJumpAnim"),       MM_CS("gBomberArmSwipeAnim"),        MM_CS("gBomberSaluteAnim"),
    MM_CS("gBomberRunAnim"),        MM_CS("gBomberLookAroundAnim"),
};

static u8 sAnimationModes[ANIM_MAX] = {
    ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_ONCE, ANIMMODE_ONCE, ANIMMODE_LOOP,
    ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_ONCE, ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_LOOP,
    ANIMMODE_ONCE, ANIMMODE_LOOP, ANIMMODE_ONCE, ANIMMODE_LOOP, ANIMMODE_LOOP,
};

static const char* sEyeTextures[] = { MM_CS("gBomberEyeOpenTex"), MM_CS("gBomberEyeHalfTex"),
                                      MM_CS("gBomberEyeClosedTex") };
static const char* sNumberTextures[] = { MM_CS("gBomberNumber1Tex"), MM_CS("gBomberNumber2Tex"),
                                         MM_CS("gBomberNumber3Tex"), MM_CS("gBomberNumber4Tex"),
                                         MM_CS("gBomberNumber5Tex") };

// ovl_En_Bombers' own two lists: the bandana's colour (prim colour), red for number 1, blue for the rest
static Gfx sRedBandanaDL[] = { gsDPSetPrimColor(0, 0, 0x84, 0x00, 0x00, 0xFF), gsSPEndDisplayList() };
static Gfx sBlueBandanaDL[] = { gsDPSetPrimColor(0, 0, 0x00, 0x5A, 0xFF, 0xFF), gsSPEndDisplayList() };
static Gfx* sBandanas[] = { sRedBandanaDL, sBlueBandanaDL, sBlueBandanaDL, sBlueBandanaDL, sBlueBandanaDL };
static Gfx sEmptyDL[] = { gsSPEndDisplayList() };

enum { NPC_WALKING, NPC_STANDING };

struct NvMmBombers;
typedef void (*NvMmBombersFunc)(struct NvMmBombers*, PlayState*);

typedef struct NvMmBombers {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[BOMBER_LIMB_MAX];
    Vec3s morphTable[BOMBER_LIMB_MAX];
    NvMmBombersFunc func;
    s16 headRotZ;
    s16 headRotX;
    s16 headRotTargetZ;
    s16 headRotTargetX;
    Vec3f walkTarget;
    s16 npcState;
    s16 targetYaw;
    s16 talkState;
    s16 idleTimer;
    s16 walkTimer;
    f32 animEndFrame;
    f32 walkMaxStep;
    s16 eyeIndex;
    s16 blinkTimer;
    s16 type;
    s16 number;
    s32 animIndex;
    ColliderCylinder collider;
} NvMmBombers;

const size_t gNvMmBombersSize = sizeof(NvMmBombers);

s32 NvMmClock_Day(void);
s32 NvMm_CheckWeekEvent(s32 flag);

static ColliderCylinderInit sCylinderInit = {
    { COLTYPE_NONE, AT_NONE, AC_NONE, OC1_ON | OC1_TYPE_PLAYER, OC2_TYPE_2, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0xF7CFFFFF, 0x00, 0x00 }, TOUCH_NONE | TOUCH_SFX_NORMAL,
      BUMP_NONE, OCELEM_ON },
    { 10, 30, 0, { 0, 0, 0 } },
};

static void NvMmBombers_ChangeAnim(NvMmBombers* this, s32 anim, f32 speed) {
    this->animIndex = anim;
    this->animEndFrame = Animation_GetLastFrame((AnimationHeader*)sAnimations[anim]);
    Animation_Change(&this->skelAnime, (AnimationHeader*)sAnimations[anim], speed, 0.0f, this->animEndFrame,
                     sAnimationModes[anim], -10.0f);
}

static void NvMmBombers_Npc(NvMmBombers* this, PlayState* play);
static void NvMmBombers_Talk(NvMmBombers* this, PlayState* play);
static void NvMmBombers_Sitting(NvMmBombers* this, PlayState* play);
static void NvMmBombers_TalkSitting(NvMmBombers* this, PlayState* play);

static void NvMmBombers_SetupNpc(NvMmBombers* this) {
    this->collider.dim.radius = 10;
    this->collider.dim.height = 30;
    this->func = NvMmBombers_Npc;
}

static void NvMmBombers_SetupSitting(NvMmBombers* this) {
    if (this->animIndex != ANIM_SIT) {
        NvMmBombers_ChangeAnim(this, ANIM_SIT, 1.0f);
    }
    this->actor.textId = NV_MM_TEXT(0x72D);
    this->headRotTargetZ = 0x1F40;
    this->func = NvMmBombers_Sitting;
}

void NvMmBombers_Init(Actor* thisx, PlayState* play) {
    NvMmBombers* this = (NvMmBombers*)thisx;
    s32 played = NvMm_CheckWeekEvent(WEEKEVENTREG_73_10) || NvMm_CheckWeekEvent(WEEKEVENTREG_85_02);

    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 19.0f);
    this->actor.gravity = -1.0f;
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sSkel, (AnimationHeader*)sAnimations[ANIM_IDLE],
                       this->jointTable, this->morphTable, BOMBER_LIMB_MAX);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    this->actor.targetMode = 0;
    Actor_SetScale(&this->actor, 0.01f);
    this->type = ENBOMBERS_GET_TYPE(&this->actor);
    this->number = ENBOMBERS_GET_NUMBER(&this->actor);
    if (this->type == ENBOMBERS_TYPE_NPC) {
        if (played) {
            Actor_Kill(&this->actor);
            return;
        }
        this->number++;
        NvMmBombers_SetupNpc(this);
    } else if (played && (this->number >= 0) && (this->number < 5) &&
               NvMm_CheckWeekEvent(MM_WE(76, 1 << this->number))) { // caught: sitting at the hideout
        NvMmBombers_SetupSitting(this);
    } else {
        Actor_Kill(&this->actor);
    }
    if (this->number > 4) {
        this->number = 4;
    }
}

void NvMmBombers_Destroy(Actor* thisx, PlayState* play) {
    NvMmBombers* this = (NvMmBombers*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

#define WEEKEVENTREG_73_20 MM_WE(73, 0x20)
#define WEEKEVENTREG_DEKU_LEARNED_WHERE_BOMBER_JIM_IS MM_WE(73, 0x40)
#define WEEKEVENTREG_DEKU_TALKED_TO_BOMBER(n) MM_WE(74, 1 << ((n) - 1)) // (MM's numbers 2-5: 0x01-0x08)

s32 NvMmForm_Get(void);
void NvMm_SetWeekEvent(s32 flag);

/** Their first line (MM's EnBombers_SetTextId): a human Link, the club's question or with the Notebook
 * its own; Deku Link, where Jim is (once told, a shorter line). */
static void NvMmBombers_SetTextId(NvMmBombers* this) {
    this->talkState = TEXT_STATE_EVENT;
    if (NvMmForm_Get() == 3) { // Deku
        u16 id = 0x744;

        if (NvMm_CheckWeekEvent(WEEKEVENTREG_73_20)) {
            id = 0x75A;
        } else if (NvMm_CheckWeekEvent(WEEKEVENTREG_DEKU_LEARNED_WHERE_BOMBER_JIM_IS)) {
            id = 0x749;
            if ((this->number >= 1) && (this->number <= 4) &&
                NvMm_CheckWeekEvent(WEEKEVENTREG_DEKU_TALKED_TO_BOMBER(this->number))) {
                id = 0x74A;
            }
        }
        this->actor.textId = NV_MM_TEXT(id);
        return;
    }
    this->actor.textId = NV_MM_TEXT(NvMm_CheckWeekEvent(WEEKEVENTREG_84_80) ? 0x74B : 0x73D);
}

/** Out in town: wander near their spot (not into walls), look about, and offer to talk. */
static void NvMmBombers_Npc(NvMmBombers* this, PlayState* play) {
    f32 frame = this->skelAnime.curFrame;
    CollisionPoly* poly;
    Vec3f endPoint;
    Vec3f target;
    s32 bgId;

    switch (this->npcState) {
        case NPC_WALKING: // (MM's name: choosing where to walk, while standing)
            if (this->idleTimer == 0) {
                s16 off;

                Math_Vec3f_Copy(&target, &this->actor.home.pos);
                target.x += Rand_CenteredFloat(150.0f);
                target.z += Rand_CenteredFloat(150.0f);
                off = ABS(BINANG_SUB(this->actor.world.rot.y, Math_Vec3f_Yaw(&this->actor.world.pos, &target)));
                if ((off < 0x4000) && !BgCheck_EntityLineTest1(&play->colCtx, &this->actor.world.pos, &target,
                                                               &endPoint, &poly, true, false, false, true, &bgId)) {
                    NvMmBombers_ChangeAnim(this, ANIM_WALK, 1.0f);
                    Math_Vec3f_Copy(&this->walkTarget, &target);
                    this->walkTimer = Rand_S16Offset(30, 50);
                    this->npcState++;
                }
            }
            break;
        case NPC_STANDING: { // (MM's name: walking to it)
            f32 x;
            f32 z;

            if (frame >= 0.0f) {
                this->targetYaw = Math_Vec3f_Yaw(&this->actor.world.pos, &this->walkTarget);
                Math_SmoothStepToS(&this->actor.world.rot.y, this->targetYaw, 0xA, 0x7D0, 0x14);
            }
            if ((s16)ABS(BINANG_SUB(this->actor.world.rot.y, this->targetYaw)) < 0x100) {
                Vec3f ahead;

                Math_Vec3f_Copy(&ahead, &this->actor.world.pos);
                ahead.x += Math_SinS(this->actor.world.rot.y) * 60.0f;
                ahead.z += Math_CosS(this->actor.world.rot.y) * 60.0f;
                if (BgCheck_EntityLineTest1(&play->colCtx, &this->actor.world.pos, &ahead, &endPoint, &poly, true,
                                            false, false, true, &bgId)) {
                    this->idleTimer = 0;
                    NvMmBombers_ChangeAnim(this, (Rand_ZeroOne() < 0.5f) ? ANIM_LOOK_AROUND : ANIM_IDLE, 1.0f);
                    this->npcState = NPC_WALKING;
                    this->walkMaxStep = 0.0f;
                    break;
                }
            }
            x = this->walkTarget.x - this->actor.world.pos.x;
            z = this->walkTarget.z - this->actor.world.pos.z;
            if ((this->walkTimer == 0) || (sqrtf(SQ(x) + SQ(z)) < 4.0f)) {
                this->idleTimer = Rand_S16Offset(20, 20);
                NvMmBombers_ChangeAnim(this, !(this->idleTimer & 1) ? ANIM_LOOK_AROUND : ANIM_IDLE, 1.0f);
                this->npcState = NPC_WALKING;
                this->walkMaxStep = 0.0f;
            } else if (frame >= 0.0f) {
                Math_ApproachF(&this->actor.world.pos.x, this->walkTarget.x, 0.3f, this->walkMaxStep);
                Math_ApproachF(&this->actor.world.pos.z, this->walkTarget.z, 0.3f, this->walkMaxStep);
                Math_ApproachF(&this->walkMaxStep, 1.0f, 0.3f, 0.5f);
            }
            break;
        }
        default:
            break;
    }
    NvMmBombers_SetTextId(this);
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        this->targetYaw = this->actor.yawTowardsPlayer;
        this->collider.dim.radius = 20;
        this->collider.dim.height = 60;
        NvMmBombers_ChangeAnim(this, ANIM_TALK_HANDS_ON_HIPS, 1.0f);
        this->func = NvMmBombers_Talk;
    } else {
        Actor_OfferTalk(&this->actor, play, 70.0f);
    }
}

/** A conversation: MM's chain of lines for a human Link, its yes/no, and the salute; Deku Link's. */
static void NvMmBombers_Talk(NvMmBombers* this, PlayState* play) {
    u16 id = this->actor.textId - NV_MM_TEXT(0);
    s32 more = false;

    Math_SmoothStepToS(&this->actor.world.rot.y, this->targetYaw, 1, 0x1388, 0);
    if (NvMmForm_Get() == 4) { // (MM: a human Link's head turn; Deku Link, none)
        this->headRotTargetZ = -0xFA0;
    }
    if ((this->talkState == Message_GetState(&play->msgCtx)) && Message_ShouldAdvance(play)) {
        Message_CloseTextbox(play);
        this->talkState = TEXT_STATE_EVENT;
        if (id == 0x73D) { // the club: "Do you know...?" then its yes/no
            id = 0x740;
            this->talkState = TEXT_STATE_CHOICE;
            more = true;
        } else if (id == 0x740) {
            if (play->msgCtx.choiceIndex == 0) {
                Sfx_PlaySfxCentered(NA_SE_SY_DECIDE);
                id = 0x742;
            } else {
                Sfx_PlaySfxCentered(NA_SE_SY_MESSAGE_PASS);
                id = 0x741;
            }
            more = true;
        } else if (id == 0x742) {
            id = 0x737;
            more = true;
        } else if (id == 0x737) {
            id = 0x743;
            more = true;
        } else if (id == 0x74B) { // with the Notebook: its own question, its yes/no
            id = 0x74C;
            this->talkState = TEXT_STATE_CHOICE;
            more = true;
        } else if (id == 0x74C) {
            if (play->msgCtx.choiceIndex == 1) {
                id = 0x737;
            } else {
                id = 0x74D;
                NvMmBombers_ChangeAnim(this, ANIM_SALUTE, 1.0f);
            }
            more = true;
        } else if (id == 0x744) { // Deku Link: where Jim is
            id = (NvMmClock_Day() == 3) ? 0x746 : 0x745;
            more = true;
        } else if ((id == 0x745) || (id == 0x746)) {
            id = 0x747;
            more = true;
        } else if (id == 0x747) {
            id = 0x748;
            more = true;
        } else if (id == 0x748) {
            if ((this->number >= 1) && (this->number <= 4)) {
                NvMm_SetWeekEvent(WEEKEVENTREG_DEKU_TALKED_TO_BOMBER(this->number));
            }
            NvMm_SetWeekEvent(WEEKEVENTREG_DEKU_LEARNED_WHERE_BOMBER_JIM_IS);
        }
        if (more) {
            this->actor.textId = NV_MM_TEXT(id);
            Message_ContinueTextbox(play, this->actor.textId);
        } else {
            this->headRotTargetZ = 0;
            NvMmBombers_SetupNpc(this);
        }
    }
}

static void NvMmBombers_Sitting(NvMmBombers* this, PlayState* play) {
    Math_SmoothStepToS(&this->headRotZ, this->headRotTargetZ, 1, 0x3E8, 0);
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        this->headRotTargetZ = 0;
        this->func = NvMmBombers_TalkSitting;
    } else {
        Actor_OfferTalk(&this->actor, play, 70.0f);
    }
}

static void NvMmBombers_TalkSitting(NvMmBombers* this, PlayState* play) {
    Math_SmoothStepToS(&this->headRotZ, this->headRotTargetZ, 1, 0x3E8, 0);
    if ((Message_GetState(&play->msgCtx) == TEXT_STATE_EVENT) && Message_ShouldAdvance(play)) {
        Message_CloseTextbox(play);
        NvMmBombers_SetupSitting(this);
    }
}

void NvMmBombers_Update(Actor* thisx, PlayState* play) {
    NvMmBombers* this = (NvMmBombers*)thisx;

    if (this->walkTimer != 0) {
        this->walkTimer--;
    }
    if (this->idleTimer != 0) {
        this->idleTimer--;
    }
    SkelAnime_Update(&this->skelAnime);
    this->actor.shape.rot.y = this->actor.world.rot.y;
    Actor_SetFocus(&this->actor, 20.0f);
    this->func(this, play);
    Actor_MoveXZGravity(&this->actor);
    Math_SmoothStepToS(&this->headRotX, this->headRotTargetX, 1, 0x1388, 0);
    Math_SmoothStepToS(&this->headRotZ, this->headRotTargetZ, 1, 0x1388, 0);
    if (this->blinkTimer == 0) {
        this->eyeIndex++;
        if (this->eyeIndex >= 3) {
            this->eyeIndex = 0;
            this->blinkTimer = (s16)Rand_ZeroFloat(60.0f) + 20;
        }
    } else {
        this->blinkTimer--;
    }
    Actor_UpdateBgCheckInfo(play, &this->actor, 20.0f, 20.0f, 50.0f, 0x1D);
    this->actor.uncullZoneForward = 500.0f;
    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

static s32 NvMmBombers_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                        void* thisx) {
    NvMmBombers* this = (NvMmBombers*)thisx;

    if ((limbIndex == BOMBER_LIMB_0F) || (limbIndex == BOMBER_LIMB_13)) {
        *dList = NULL; // (MM hides these two: the box and the blowgun)
    }
    if (limbIndex == BOMBER_LIMB_HEAD) {
        rot->x += this->headRotX;
        rot->z += this->headRotZ;
    }
    return false;
}

void NvMmBombers_Draw(Actor* thisx, PlayState* play) {
    NvMmBombers* this = (NvMmBombers*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sEyeTextures[this->eyeIndex]));
    gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(sNumberTextures[this->number]));
    gSPSegment(POLY_OPA_DISP++, 0x0A, sBandanas[this->number]);
    gSPSegment(POLY_OPA_DISP++, 0x0C, sEmptyDL);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          NvMmBombers_OverrideLimbDraw, NULL, &this->actor);
    CLOSE_DISPS(play->state.gfxCtx);
}

/*
 * ---- En_Bombers2: the hideout's guard (MM id 0x281), MM's ovl_En_Bombers2 ----
 * The Bomber in East Clock Town who stands in front of the hideout's way in, asking for the Bombers'
 * code: MM's code box (z_nv_mm_msginput.c: five digits, 1-5); right, he steps aside (week event
 * 73_80) and stays aside. As MM, a correct digit stays correct between tries while he's talked to.
 * Adapted: MM's cutscene camera as he steps aside is left out (the box closes, he walks, then
 * salutes and speaks again); the masks' reactions (Text_GetFaceReaction) wait for MM's masks.
 */
#define WEEKEVENTREG_ENTERED_BOMBERS_CODE MM_WE(73, 0x80)
#define WEEKEVENTREG_TALKED_TO_BOMBERS_GUARD MM_WE(86, 0x02)
#define GUARD_TEXT_BLOCKED_GORON_ZORA 1 // (MM's sTextIds' indices)
#define GUARD_TEXT_ASK 2
#define GUARD_TEXT_INPUT 3
#define GUARD_TEXT_WRONG 4
#define GUARD_TEXT_WRONG_BYE 5
#define GUARD_TEXT_RIGHT 6
#define GUARD_TEXT_ASIDE 7

void NvMm_SetWeekEvent(s32 flag);
s32 NvMmMsgInput_Digit(s32 i);
u8* NvMmMsg_BomberCode(void);

static u16 sGuardTextIds[] = { 0x0727, 0x0728, 0x0726, 0x0729, 0x072B, 0x0725, 0x072A, 0x073C };
static Gfx sGuardBandanaDL[] = { gsDPSetPrimColor(0, 0xFF, 198, 140, 0, 255), gsSPEndDisplayList() };

enum { GUARD_WAITING, GUARD_TALKING, GUARD_WALKING };

struct NvMmBombers2;
typedef void (*NvMmBombers2Func)(struct NvMmBombers2*, PlayState*);

typedef struct NvMmBombers2 {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[BOMBER_LIMB_MAX];
    Vec3s morphTable[BOMBER_LIMB_MAX];
    NvMmBombers2Func func;
    s16 headRotX, headRotZ, headRotTargetX, headRotTargetZ;
    s16 textIdIndex;
    s16 animPhase;
    s16 action;
    s16 eyeIndex;
    s16 blinkTimer;
    s16 bodyRotTargetAngle;
    s32 animIndex;
    s32 inputting;          // MM's talkState == TEXT_STATE_INPUT_BOMBER_CODE
    s32 hasMovedAside;
    s32 correctDigitSlots[5];
    f32 lastAnimFrame;
    Vec3f walkTarget;
    ColliderCylinder collider;
} NvMmBombers2;

const size_t gNvMmBombers2Size = sizeof(NvMmBombers2);

static ColliderCylinderInit sGuardCylinderInit = {
    { COLTYPE_NONE, AT_NONE, AC_NONE, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_2, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0xF7CFFFFF, 0x00, 0x00 }, TOUCH_NONE | TOUCH_SFX_NORMAL,
      BUMP_NONE, OCELEM_ON },
    { 35, 30, 0, { 0, 0, 0 } },
};

static void NvMmBombers2_AwaitTalk(NvMmBombers2* this, PlayState* play);
static void NvMmBombers2_Talking(NvMmBombers2* this, PlayState* play);
static void NvMmBombers2_Walking(NvMmBombers2* this, PlayState* play);

static void NvMmBombers2_ChangeAnim(NvMmBombers2* this, s32 anim) {
    this->animIndex = anim;
    this->lastAnimFrame = Animation_GetLastFrame((AnimationHeader*)sAnimations[anim]);
    Animation_Change(&this->skelAnime, (AnimationHeader*)sAnimations[anim], 1.0f, 0.0f, this->lastAnimFrame,
                     sAnimationModes[anim], -10.0f);
}

static void NvMmBombers2_SetupAwaitTalk(NvMmBombers2* this) {
    NvMmBombers2_ChangeAnim(this, this->hasMovedAside ? ANIM_IDLE : ANIM_TAUNT);
    this->action = GUARD_WAITING;
    this->func = NvMmBombers2_AwaitTalk;
}

void NvMmBombers2_Init(Actor* thisx, PlayState* play) {
    NvMmBombers2* this = (NvMmBombers2*)thisx;

    thisx->colChkInfo.mass = MASS_IMMOVABLE;
    ActorShape_Init(&thisx->shape, 0.0f, ActorShadow_DrawCircle, 19.0f);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sSkel, (AnimationHeader*)sAnimations[ANIM_IDLE],
                       this->jointTable, this->morphTable, BOMBER_LIMB_MAX);
    thisx->targetMode = 6;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sGuardCylinderInit);
    if (NvMm_CheckWeekEvent(WEEKEVENTREG_ENTERED_BOMBERS_CODE) || (gSaveContext.entranceIndex == ENTR_MM_Z2_TOWN_2)) {
        thisx->world.pos.x += Math_SinS(thisx->home.rot.y + 0xC100) * 50.0f;
        thisx->world.pos.z += Math_CosS(thisx->home.rot.y + 0xC100) * 50.0f;
        this->hasMovedAside = true;
    }
    NvMmBombers2_SetupAwaitTalk(this);
}

void NvMmBombers2_Destroy(Actor* thisx, PlayState* play) {
    Collider_DestroyCylinder(play, &((NvMmBombers2*)thisx)->collider);
}

static void NvMmBombers2_SetupTalking(NvMmBombers2* this) {
    if (this->textIdIndex <= GUARD_TEXT_BLOCKED_GORON_ZORA) {
        NvMmBombers2_ChangeAnim(this, ANIM_ARM_SWIPE);
    } else {
        NvMmBombers2_ChangeAnim(this, ANIM_TALK_HANDS_ON_HIPS);
    }
    this->animPhase = 0;
    if (this->hasMovedAside) {
        NvMmBombers2_ChangeAnim(this, ANIM_SALUTE);
    }
    this->action = GUARD_TALKING;
    this->func = NvMmBombers2_Talking;
}

/** MM's EnBombers2_AwaitTalk: his line by Link's form. */
static void NvMmBombers2_AwaitTalk(NvMmBombers2* this, PlayState* play) {
    s16 yawDiff = this->actor.yawTowardsPlayer - this->actor.world.rot.y;
    s32 form = NvMmForm_Get();

    this->textIdIndex = ((form == 3) || (form == 4)) ? GUARD_TEXT_ASK : GUARD_TEXT_BLOCKED_GORON_ZORA;
    this->inputting = false;
    if (this->hasMovedAside) {
        this->textIdIndex = GUARD_TEXT_ASIDE;
    }
    this->actor.textId = NV_MM_TEXT(sGuardTextIds[this->textIdIndex]);
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        this->bodyRotTargetAngle = this->actor.world.rot.y;
        NvMm_SetWeekEvent(WEEKEVENTREG_TALKED_TO_BOMBERS_GUARD);
        NvMmBombers2_SetupTalking(this);
        return;
    }
    if (ABS(yawDiff) < 0x3BB5) {
        Actor_OfferTalk(&this->actor, play, 70.0f);
    }
}

static void NvMmBombers2_Say(NvMmBombers2* this, PlayState* play, s32 index) {
    this->textIdIndex = index;
    this->actor.textId = NV_MM_TEXT(sGuardTextIds[index]);
    Message_ContinueTextbox(play, this->actor.textId);
}

/** MM's EnBombers2_StartWalking: aside, away from Link's side. */
static void NvMmBombers2_StartWalking(NvMmBombers2* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s16 homeYawToPlayer = Math_Vec3f_Yaw(&this->actor.home.pos, &player->actor.world.pos);
    s16 side = (this->actor.home.rot.y < homeYawToPlayer) ? 0xC100 : 0x3F00;

    NvMmBombers2_ChangeAnim(this, ANIM_WALK);
    this->walkTarget = this->actor.world.pos;
    this->walkTarget.x += Math_SinS(this->actor.home.rot.y + side) * 50.0f;
    this->walkTarget.z += Math_CosS(this->actor.home.rot.y + side) * 50.0f;
    this->bodyRotTargetAngle = Math_Vec3f_Yaw(&this->actor.home.pos, &this->walkTarget);
    this->action = GUARD_WALKING;
    this->func = NvMmBombers2_Walking;
}

/** MM's EnBombers2_Talking. */
static void NvMmBombers2_Talking(NvMmBombers2* this, PlayState* play) {
    s32 i;

    if (!this->hasMovedAside && (this->skelAnime.curFrame >= this->lastAnimFrame)) {
        NvMmBombers2_ChangeAnim(this, !(this->animPhase & 1) ? ANIM_TALK_HANDS_ON_HIPS : ANIM_ARM_SWIPE);
        this->animPhase = (this->animPhase + 1) & 1;
    }
    this->headRotTargetZ = (NvMmForm_Get() == 4) ? -0xFA0 : (NvMmForm_Get() == 3) ? this->headRotTargetZ : -0x1770;
    if ((Message_GetState(&play->msgCtx) != TEXT_STATE_EVENT) || !Message_ShouldAdvance(play)) {
        return;
    }
    if (this->inputting) {
        u8* code = NvMmMsg_BomberCode();
        s32 correct = 0;

        for (i = 0; i < 5; i++) {
            if (!this->correctDigitSlots[i] && (NvMmMsgInput_Digit(i) == code[i])) {
                this->correctDigitSlots[i] = true;
            }
            correct += this->correctDigitSlots[i];
        }
        if (correct >= 5) {
            this->inputting = false;
            NvMmBombers2_Say(this, play, GUARD_TEXT_RIGHT);
            Audio_PlayActorSound2(&this->actor, NA_SE_SY_PIECE_OF_HEART);
            return;
        }
    }
    switch (this->textIdIndex) {
        case 0:
        case GUARD_TEXT_BLOCKED_GORON_ZORA:
        case GUARD_TEXT_WRONG_BYE:
        case GUARD_TEXT_ASIDE:
            this->headRotTargetZ = 0;
            Message_CloseTextbox(play);
            NvMmBombers2_SetupAwaitTalk(this);
            break;
        case GUARD_TEXT_ASK:
            NvMmBombers2_Say(this, play, GUARD_TEXT_INPUT);
            this->inputting = true;
            break;
        case GUARD_TEXT_INPUT:
            for (i = 0; i < 5; i++) {
                this->correctDigitSlots[i] = false;
            }
            this->inputting = false;
            Audio_PlayActorSound2(&this->actor, NA_SE_SY_ERROR);
            NvMmBombers2_Say(this, play, GUARD_TEXT_WRONG);
            break;
        case GUARD_TEXT_WRONG:
            NvMmBombers2_Say(this, play, GUARD_TEXT_WRONG_BYE);
            break;
        case GUARD_TEXT_RIGHT:
            Message_CloseTextbox(play);
            NvMmBombers2_StartWalking(this, play);
            break;
    }
}

/** MM's EnBombers2_Walking (its cutscene camera left out). */
static void NvMmBombers2_Walking(NvMmBombers2* this, PlayState* play) {
    Math_SmoothStepToS(&this->actor.world.rot.y, this->bodyRotTargetAngle, 1, 0xBB8, 0);
    if ((fabsf(this->walkTarget.x - this->actor.world.pos.x) < 3.0f) &&
        (fabsf(this->walkTarget.z - this->actor.world.pos.z) < 3.0f)) {
        this->bodyRotTargetAngle = this->actor.yawTowardsPlayer;
        this->actor.speedXZ = 0.0f;
        if (ABS((s16)(this->actor.world.rot.y - this->actor.yawTowardsPlayer)) < 100) {
            this->textIdIndex = GUARD_TEXT_ASIDE;
            NvMmBombers2_ChangeAnim(this, ANIM_SALUTE);
            this->animPhase = 0;
            this->action = GUARD_TALKING;
            NvMm_SetWeekEvent(WEEKEVENTREG_ENTERED_BOMBERS_CODE);
            this->hasMovedAside = true;
            this->actor.textId = NV_MM_TEXT(sGuardTextIds[this->textIdIndex]);
            Message_StartTextbox(play, this->actor.textId, &this->actor);
            this->func = NvMmBombers2_Talking;
        }
    } else {
        Math_ApproachF(&this->actor.world.pos.x, this->walkTarget.x, 0.3f, 1.0f);
        Math_ApproachF(&this->actor.world.pos.z, this->walkTarget.z, 0.3f, 1.0f);
    }
}

void NvMmBombers2_Update(Actor* thisx, PlayState* play) {
    NvMmBombers2* this = (NvMmBombers2*)thisx;
    Vec3f pos;

    SkelAnime_Update(&this->skelAnime);
    thisx->shape.rot.y = thisx->world.rot.y;
    if (this->action != GUARD_WALKING) {
        s16 d = thisx->yawTowardsPlayer - thisx->world.rot.y;

        this->headRotTargetX = 0;
        if (ABS(d) < 0x3A98) {
            this->headRotTargetX = CLAMP(d, -0x2710, 0x2710);
        }
    }
    Actor_SetFocus(thisx, 20.0f);
    Actor_SetScale(thisx, 0.008f);
    this->func(this, play);
    Actor_MoveXZGravity(thisx);
    Math_SmoothStepToS(&this->headRotX, this->headRotTargetX, 1, 0xBB8, 0);
    Math_SmoothStepToS(&this->headRotZ, this->headRotTargetZ, 1, 0xBB8, 0);
    if (this->blinkTimer == 0) {
        if (++this->eyeIndex >= 3) {
            this->eyeIndex = 0;
            this->blinkTimer = (s16)Rand_ZeroFloat(60.0f) + 20;
        }
    } else {
        this->blinkTimer--;
    }
    // (MM: in front of the way in, his collider stands 26 behind him, wide; aside, small)
    pos = thisx->world.pos;
    if (!this->hasMovedAside) {
        thisx->world.pos.x = pos.x + Math_SinS(thisx->world.rot.y + 0x8000) * 26.0f;
        thisx->world.pos.z = pos.z + Math_CosS(thisx->world.rot.y + 0x8000) * 26.0f;
        this->collider.dim.radius = 35;
        this->collider.dim.height = 30;
    } else {
        this->collider.dim.radius = 10;
        this->collider.dim.height = 25;
    }
    Collider_UpdateCylinder(thisx, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    if (!this->hasMovedAside) {
        Actor_UpdateBgCheckInfo(play, thisx, 20.0f, 20.0f, 50.0f, 0x1D);
    }
    thisx->world.pos = pos;
}

static s32 NvMmBombers2_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                         void* thisx) {
    NvMmBombers2* this = (NvMmBombers2*)thisx;

    if (limbIndex == BOMBER_LIMB_HEAD) {
        rot->x += this->headRotX;
        rot->z += this->headRotZ;
    }
    if ((limbIndex == BOMBER_LIMB_0F) || (limbIndex == BOMBER_LIMB_13) || (limbIndex == 0x14)) {
        *dList = NULL;
    }
    return false;
}

void NvMmBombers2_Draw(Actor* thisx, PlayState* play) {
    NvMmBombers2* this = (NvMmBombers2*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sEyeTextures[this->eyeIndex]));
    gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(MM_CS("gBomberWhiteShirtBackTex")));
    gSPSegment(POLY_OPA_DISP++, 0x0A, sGuardBandanaDL);
    gSPSegment(POLY_OPA_DISP++, 0x0C, sEmptyDL);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          NvMmBombers2_OverrideLimbDraw, NULL, this);
    CLOSE_DISPS(play->state.gfxCtx);
}
