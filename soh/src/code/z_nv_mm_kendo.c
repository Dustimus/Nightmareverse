/*
 * Nightmareverse: the Swordsman's School (mm/README.md "The Swordsman's School"), from MM's
 * En_Kendo_Js (the swordsman, MM's object_js) and En_Maruta (the practice logs, object_maruta),
 * zeldaret/mm, ported closely.
 *
 * His lines by day, form and mask; the Novice Course (1 Rupee: side jump, roll, back flip, horizontal
 * cut, vertical cut, thrust, jump attack, each checked against Link's moves, with MM's retries and
 * hints) and the Expert Course (10 Rupees: five rounds of two logs rising at random from the path's
 * six points, 1 point a cut, 2 a thrust, 3 a jump attack; all 30 for the piece of heart, a red rupee
 * after). On the final night he cowers in the back room and a sign stands at the counter. The logs
 * rise through the floor, turn to face Link, and are cut by the swing that hit them into MM's pieces
 * (eight chunks, nine shapes), which fly, spin, bounce and flatten as MM's.
 *
 * Adapted from MM:
 * - Link's moves read OoT's player: MM's side jump and back flip are OoT's hop (PLAYER_STATE2_HOPPING,
 *   its direction), MM's roll flag OoT's roll action (NvPlayer_IsRolling, a line in z_player.c);
 *   MM's sword swings (meleeWeaponAnimation) are OoT's of the same names;
 * - MM walks Link to his starting spot with the control stick (Actor_SetControlStickData); here the
 *   stick is set for him the same way (NvMmInput_SetStick, applied with the dev walker's input);
 * - MM's minigame score counter is a line over the HUD (z_nv_mm_clock.c: gNvMmMinigameScore);
 * - the Expert Course's camera is MM's: his cutscene's camera as the main camera (z_nv_mm_csmgr.c);
 *   the logs' bounce is OoT's wooden thud.
 */
#include "global.h"
#include <stdio.h>
#include <stdlib.h>

#define NV_MM_TEXT(id) (0xB000 + (id))
#define MM_WE(byte, bit) (((byte) << 8) | (bit))
#define WEEKEVENTREG_RECEIVED_SWORDSMANS_SCHOOL_HEART_PIECE MM_WE(63, 0x20)
#define WEEKEVENTREG_PLAYING_SWORDSMAN_MINIGAME MM_WE(82, 0x08)
#define MM_GI_RUPEE_RED 0x04
#define MM_GI_HEART_PIECE 0x0C
#define MM_EN_KENDO_JS 0x1EF
#define MM_EN_MARUTA 0x1F8
#define MM_PLAYER_MASK_GIANT 0x14

s32 NvMm_CheckWeekEvent(s32 flag);
void NvMm_SetWeekEvent(s32 flag);
void NvMm_ClearWeekEvent(s32 flag);
s32 NvMmClock_Day(void);
s32 NvMmClock_Time(void); // (MM's 0x10000 a day)
s32 NvMmForm_Get(void);   // MM's PLAYER_FORM_*: 0 the Fierce Deity, 1 Goron, 2 Zora, 3 Deku, 4 human
s32 NvMmMask_Worn(void);  // MM's PLAYER_MASK_*, 0 none
s32 NvMmActor_Id(s32 mmId);
GetItemEntry NvMmGetItem_Entry(s32 mmGi);
s32 NvPlayer_IsRolling(Player* player);

/* ---------------------------------------------------------------------------------------------- */
/* MM's minigame score (Interface_InitMinigame, interfaceCtx.minigamePoints) and the stick set for   */
/* Link (Actor_SetControlStickData)                                                                */

s32 gNvMmMinigameOn;
s32 gNvMmMinigameScore;

static s32 sStickSet;
static f32 sStickMag;
static s16 sStickYaw;

/** MM's Actor_SetControlStickData: the stick pushed `mag` toward world yaw `yaw`, next frame. */
static void NvMmInput_SetStick(f32 mag, s16 yaw) {
    sStickSet = true;
    sStickMag = mag;
    sStickYaw = yaw;
}

/** With the dev walker's input (z_nv_dev.c, each frame before the actors): the stick set above. */
void NvMmInput_Apply(PlayState* play) {
    Input* input = &play->state.input[0];
    s16 rel;

    if (!sStickSet) {
        return;
    }
    sStickSet = false;
    rel = sStickYaw - Camera_GetInputDirYaw(GET_ACTIVE_CAM(play));
    input->cur.stick_x = input->rel.stick_x = (s8)(-Math_SinS(rel) * sStickMag);
    input->cur.stick_y = input->rel.stick_y = (s8)(Math_CosS(rel) * sStickMag);
    input->cur.button = input->press.button = 0;
}

/* ---------------------------------------------------------------------------------------------- */
/* The swordsman (En_Kendo_Js)                                                                     */

#define JS(name) ("__OTR__mm/objects/object_js/" name)
#define OBJECT_JS_LIMB_0C 12
#define OBJECT_JS_LIMB_MAX 13
#define IN_BACK_ROOM 1

static const char sJsSkel[] = JS("gSwordmasterSkel");
static const char sAnimFlailing[] = JS("gSwordmasterFlailingAnim");
static const char sAnimStopFlailing[] = JS("gSwordmasterStopFlailingAnim");
static const char sAnimCowering[] = JS("gSwordmasterCoweringAnim");
static const char sAnimDefault[] = JS("gSwordmasterDefaultAnim");

enum { ANIM_DEFAULT, ANIM_FLAILING, ANIM_STOP_FLAILING, ANIM_COWERING };
static const char* sAnims[] = { sAnimDefault, sAnimFlailing, sAnimStopFlailing, sAnimCowering };
static const u8 sAnimModes[] = { ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_ONCE, ANIMMODE_LOOP };

static const s16 sNonHumanTextIds[3][3] = {
    { 0x2731, 0x2732, 0x2733 }, // Deku
    { 0x2734, 0x2735, 0x2736 }, // Goron
    { 0x2737, 0x2738, 0x2739 }, // Zora
};
static const s16 sHumanTextIds[2][3] = { { 0x2710, 0x2712, 0x2714 }, { 0x2711, 0x2713, 0x2715 } };
static const s16 sNoviceProgressTextIds[] = { 0x271C, 0x271E, 0x2720, 0x2722, 0x2724, 0x2726, 0x2728 };
static const s16 sNoviceTryAgainTextIds[] = { 0x271B, 0x271D, 0x271F, 0x2721, 0x2723, 0x2725, 0x2727 };
static const s16 sPrices[2] = { 1, 10 }; // (MM's message 0x2716: the Novice and Expert Courses)

enum { COURSE_PAUSED, COURSE_ACTIVE, COURSE_RETRY };
enum {
    NOVICE_SIDE_JUMP,
    NOVICE_ROLL,
    NOVICE_BACKFLIP,
    NOVICE_SWORD_ATTACK,
    NOVICE_SWORD_TARGET,
    NOVICE_SWORD_THRUST,
    NOVICE_JUMP_ATTACK,
    NOVICE_FINISHED
};
enum { RESULT_CORRECT, RESULT_INCORRECT, RESULT_PENDING };

struct NvMmKendo;
typedef void (*NvMmKendoFunc)(struct NvMmKendo*, PlayState*);

typedef struct NvMmKendo {
    Actor actor;
    ColliderCylinder collider;
    SkelAnime skelAnime;
    NvMmKendoFunc func;
    Vec3s jointTable[OBJECT_JS_LIMB_MAX];
    Vec3s morphTable[OBJECT_JS_LIMB_MAX];
    Vec3s* pathPoints;
    Vec3s headRot;
    Vec3s torsoRot;
    s16 round;
    s16 courseState;
    s16 curTextId;
    s16 hasSpoken;
    s16 numLogs;
    s16 isSlashingLog;
    s16 timer;
    u8 isPlayerLockedOn;
} NvMmKendo;

size_t gNvMmKendoJsSize = sizeof(NvMmKendo);

static ColliderCylinderInit sJsCylinderInit = {
    { COLTYPE_NONE, AT_NONE, AC_NONE, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_2, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0, 0, 0 }, { 0xF7CFFFFF, 0, 0 }, TOUCH_NONE | TOUCH_SFX_NORMAL, BUMP_NONE, OCELEM_ON },
    { 18, 30, 0, { 0, 0, 0 } },
};

static void NvMmKendo_AwaitTalk(NvMmKendo* this, PlayState* play);
static void NvMmKendo_HandleMessageState(NvMmKendo* this, PlayState* play);
static void NvMmKendo_SpawnLog(NvMmKendo* this, PlayState* play, s32 position);

static void NvMmKendo_ChangeAnim(NvMmKendo* this, s32 anim) {
    AnimationHeader* a = (AnimationHeader*)sAnims[anim];

    Animation_Change(&this->skelAnime, a, 1.0f, 0.0f, Animation_GetLastFrame(a), sAnimModes[anim], -8.0f);
}

static void NvMmKendo_StartMsg(NvMmKendo* this, PlayState* play, s16 id) {
    Message_StartTextbox(play, NV_MM_TEXT(id), &this->actor);
    this->curTextId = id;
}

static s32 NvMmKendo_IsFinalNight(void) {
    s32 t = NvMmClock_Time();

    return (NvMmClock_Day() == 3) && ((t > 0xF555) || (t < 0x4000)); // after 23:00, before 6:00
}

void NvMmKendoJs_Init(Actor* thisx, PlayState* play) {
    NvMmKendo* this = (NvMmKendo*)thisx;

    ActorShape_Init(&thisx->shape, 0.0f, ActorShadow_DrawCircle, 36.0f);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sJsCylinderInit);
    thisx->colChkInfo.mass = MASS_IMMOVABLE;
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sJsSkel, (AnimationHeader*)sAnimDefault,
                       this->jointTable, this->morphTable, OBJECT_JS_LIMB_MAX);
    if (NvMmKendo_IsFinalNight()) {
        if ((thisx->params & 0xFF) != IN_BACK_ROOM) { // (his sign at the counter)
            Actor_Spawn(&play->actorCtx, play, NvMmActor_Id(0x0A8), thisx->home.pos.x, thisx->home.pos.y,
                        thisx->home.pos.z - 10.0f, thisx->home.rot.x, thisx->home.rot.y, thisx->home.rot.z, 0x10);
            Actor_Kill(thisx);
            return;
        }
        NvMmKendo_ChangeAnim(this, ANIM_COWERING);
    } else if ((thisx->params & 0xFF) == IN_BACK_ROOM) {
        Actor_Kill(thisx);
        return;
    }
    Actor_UpdateBgCheckInfo(play, thisx, 0.0f, 0.0f, 0.0f, 4); // (the floor)
    if ((thisx->params & 0xFF) != IN_BACK_ROOM) {
        Path* path = Path_GetByIndex(play, (thisx->params >> 8) & 0xFF, 0xFF);

        this->pathPoints = (path != NULL) ? SEGMENTED_TO_VIRTUAL(path->points) : NULL;
    }
    thisx->flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    thisx->focus.pos = thisx->world.pos;
    thisx->focus.pos.y += 30.0f;
    thisx->child = NULL;
    this->courseState = COURSE_PAUSED;
    this->func = NvMmKendo_AwaitTalk;
}

void NvMmKendoJs_Destroy(Actor* thisx, PlayState* play) {
    NvMmKendo* this = (NvMmKendo*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
    NvMm_ClearWeekEvent(WEEKEVENTREG_PLAYING_SWORDSMAN_MINIGAME);
    gNvMmMinigameOn = false;
}

/** MM's EnKendoJs_AwaitTalk: the opening line by place, form, mask and day. */
static void NvMmKendo_AwaitTalk(NvMmKendo* this, PlayState* play) {
    s32 day = CLAMP(NvMmClock_Day(), 1, 3) - 1;
    s32 form = NvMmForm_Get();
    s32 mask = NvMmMask_Worn();
    s16 id;

    if ((this->actor.params & 0xFF) == IN_BACK_ROOM) {
        id = 0x273C;
    } else if (form != 4) {
        id = sNonHumanTextIds[(form == 1) ? 1 : (form == 2) ? 2 : 0][day];
    } else if ((mask != 0) && (mask < MM_PLAYER_MASK_GIANT)) {
        id = mask + 0x273C;
    } else {
        id = sHumanTextIds[this->hasSpoken ? 1 : 0][day];
    }
    this->actor.textId = NV_MM_TEXT(id); // (OoT's talk opens it)
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        this->curTextId = id;
        if ((id == sHumanTextIds[0][day]) && (form == 4)) {
            this->hasSpoken = true;
        }
        this->func = NvMmKendo_HandleMessageState;
    } else {
        Actor_OfferTalk(&this->actor, play, 100.0f);
    }
}

static void NvMmKendo_MovePlayerToStartNovice(NvMmKendo* this, PlayState* play);
static void NvMmKendo_StartNovice(NvMmKendo* this);
static void NvMmKendo_SetupExpert(NvMmKendo* this);
static void NvMmKendo_AwardPrize(NvMmKendo* this, PlayState* play);

/** MM's EnKendoJs_HandleMessageChoices: Novice, Expert, no. */
static void NvMmKendo_HandleMessageChoices(NvMmKendo* this, PlayState* play) {
    s32 choice;

    if (!Message_ShouldAdvance(play) || (this->curTextId != 0x2716)) {
        return;
    }
    choice = play->msgCtx.choiceIndex;
    if (getenv("NV_LOG") != NULL) {
        printf("NVMM swordsman: choice %d (frame %d)\n", (int)choice, (int)play->gameplayFrames);
        fflush(stdout);
    }
    if (choice == 2) {
        Sfx_PlaySfxCentered(NA_SE_SY_CANCEL);
        NvMmKendo_StartMsg(this, play, 0x2717);
    } else if (CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_NONE) {
        Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
        NvMmKendo_StartMsg(this, play, 0x272C);
        NvMmKendo_ChangeAnim(this, ANIM_FLAILING);
    } else if (gSaveContext.rupees < sPrices[choice]) {
        Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
        NvMmKendo_StartMsg(this, play, 0x2718);
    } else {
        Sfx_PlaySfxCentered(NA_SE_SY_DECIDE);
        Rupees_ChangeBy(-sPrices[choice]);
        NvMmKendo_StartMsg(this, play, (choice == 0) ? 0x2719 : 0x273A);
    }
}

/** MM's EnKendoJs_HandleMessageEvents. */
static void NvMmKendo_HandleMessageEvents(NvMmKendo* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (!Message_ShouldAdvance(play)) {
        return;
    }
    switch (this->curTextId) {
        case 0x2710:
        case 0x2711:
        case 0x2712:
        case 0x2713:
        case 0x2714:
        case 0x2715:
            NvMmKendo_StartMsg(this, play, 0x2716);
            break;
        case 0x2719:
            Message_CloseTextbox(play);
            this->func = NvMmKendo_MovePlayerToStartNovice;
            break;
        case 0x271A:
            Message_CloseTextbox(play);
            NvMmKendo_StartNovice(this);
            break;
        case 0x273A:
            NvMmKendo_StartMsg(this, play, 0x273B);
            break;
        case 0x273B:
            Message_CloseTextbox(play);
            gNvMmMinigameOn = true; // (MM's Interface_InitMinigame)
            gNvMmMinigameScore = 0;
            NvMmKendo_SetupExpert(this);
            break;
        case 0x272D:
            Message_CloseTextbox(play);
            gNvMmMinigameOn = false;
            this->func = NvMmKendo_AwardPrize;
            NvMmKendo_AwardPrize(this, play);
            break;
        default:
            break;
    }
    (void)player;
}

/** MM's EnKendoJs_HandleMessageState. */
static void NvMmKendo_HandleMessageState(NvMmKendo* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (getenv("NV_LOG") != NULL) {
        static s32 sLast = -1;
        s32 now = (Message_GetState(&play->msgCtx) << 16) | (u16)this->curTextId;

        if (now != sLast) {
            sLast = now;
            printf("NVMM swordsman: text %04X state %d mode %d\n", (u16)this->curTextId,
                   (int)Message_GetState(&play->msgCtx), (int)play->msgCtx.msgMode);
            fflush(stdout);
        }
    }

    switch (Message_GetState(&play->msgCtx)) {
        case TEXT_STATE_CHOICE:
            NvMmKendo_HandleMessageChoices(this, play);
            break;
        case TEXT_STATE_EVENT:
            NvMmKendo_HandleMessageEvents(this, play);
            break;
        case TEXT_STATE_DONE:
            if (Message_ShouldAdvance(play)) {
                if (this->curTextId == 0x272C) {
                    NvMmKendo_ChangeAnim(this, ANIM_STOP_FLAILING);
                }
                if ((this->curTextId == 0x272E) || (this->curTextId == 0x272F) || (this->curTextId == 0x2730)) {
                    gNvMmMinigameOn = false;
                }
                player->stateFlags1 &= ~PLAYER_STATE1_INPUT_DISABLED;
                this->func = NvMmKendo_AwaitTalk;
            }
            break;
        case TEXT_STATE_CLOSING:
            player->stateFlags1 &= ~PLAYER_STATE1_INPUT_DISABLED;
            this->func = NvMmKendo_AwaitTalk;
            break;
        default:
            break;
    }
}

/** MM's EnKendoJs_GetNoviceCourseActionResult: Link's move against the lesson. */
static s32 NvMmKendo_NoviceResult(NvMmKendo* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s32 hopping = (player->stateFlags2 & PLAYER_STATE2_HOPPING) != 0;
    s32 sideJumping = hopping && ((player->av1.actionVar1 == 1) || (player->av1.actionVar1 == 3));
    s32 backJumping = hopping && (player->av1.actionVar1 == 2);
    s32 rolling = NvPlayer_IsRolling(player);
    s32 mwa = player->meleeWeaponAnimation;

    switch (this->round) {
        case NOVICE_SIDE_JUMP:
            if (sideJumping) {
                return RESULT_CORRECT;
            }
            if ((player->meleeWeaponState != 0) || rolling || hopping) {
                return RESULT_INCORRECT;
            }
            break;
        case NOVICE_ROLL:
            if (rolling) {
                return RESULT_CORRECT;
            }
            if ((player->meleeWeaponState != 0) || hopping) {
                return RESULT_INCORRECT;
            }
            break;
        case NOVICE_BACKFLIP:
            if (backJumping) {
                return RESULT_CORRECT;
            }
            if ((player->meleeWeaponState != 0) || rolling || hopping) {
                return RESULT_INCORRECT;
            }
            this->isSlashingLog = false;
            break;
        case NOVICE_SWORD_ATTACK:
        case NOVICE_SWORD_TARGET:
        case NOVICE_SWORD_THRUST:
        case NOVICE_JUMP_ATTACK: {
            s32 right;

            if (this->round == NOVICE_SWORD_ATTACK) {
                right = (mwa == PLAYER_MWA_RIGHT_SLASH_1H) || (mwa == PLAYER_MWA_RIGHT_COMBO_1H);
            } else if (this->round == NOVICE_SWORD_TARGET) {
                right = (mwa == PLAYER_MWA_FORWARD_SLASH_1H) || (mwa == PLAYER_MWA_FORWARD_COMBO_1H);
            } else if (this->round == NOVICE_SWORD_THRUST) {
                right = (mwa == PLAYER_MWA_STAB_1H);
            } else {
                right = (mwa == PLAYER_MWA_JUMPSLASH_START) || (mwa == PLAYER_MWA_JUMPSLASH_FINISH);
            }
            if (this->isSlashingLog && right) {
                this->isSlashingLog = false;
                return RESULT_CORRECT;
            }
            if (this->isSlashingLog || rolling || hopping) {
                this->isSlashingLog = false;
                return RESULT_INCORRECT;
            }
            break;
        }
        default:
            break;
    }
    return RESULT_PENDING;
}

/** MM's EnKendoJs_HasMoreAdviceToTryAgain. */
static s32 NvMmKendo_MoreAdvice(NvMmKendo* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if ((this->curTextId == 0x271D) && (Player_GetMeleeWeaponHeld(player) != 0)) {
        NvMmKendo_StartMsg(this, play, 0x272A); // put the sword away
        return true;
    }
    if ((this->curTextId == 0x2721) && this->isPlayerLockedOn) {
        NvMmKendo_StartMsg(this, play, 0x272B); // don't Z-Target
        return true;
    }
    return false;
}

/** MM's EnKendoJs_MovePlayerToPos. */
static s32 NvMmKendo_MovePlayerToPos(PlayState* play, Vec3f target) {
    Player* player = GET_PLAYER(play);
    f32 dist = Math_Vec3f_DistXZ(&player->actor.world.pos, &target);

    NvMmInput_SetStick((dist < 20.0f) ? 10.0f : (dist < 40.0f) ? 40.0f : 80.0f,
                       Math_Vec3f_Yaw(&player->actor.world.pos, &target));
    return dist < 20.0f;
}

static void NvMmKendo_MovePlayerToStartNovice(NvMmKendo* this, PlayState* play) {
    Vec3f start = this->actor.world.pos;

    start.z += 200.0f;
    if (NvMmKendo_MovePlayerToPos(play, start)) {
        this->actor.flags |= ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
        this->actor.textId = NV_MM_TEXT(0x271A);
        if (Actor_ProcessTalkRequest(&this->actor, play)) {
            this->actor.flags &= ~ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
            NvMmKendo_SpawnLog(this, play, 0);
            this->curTextId = 0x271A;
            this->func = NvMmKendo_HandleMessageState;
        } else {
            Actor_OfferTalk(&this->actor, play, 800.0f);
        }
    }
}

static void NvMmKendo_NoviceCourse(NvMmKendo* this, PlayState* play);

static void NvMmKendo_StartNovice(NvMmKendo* this) {
    NvMm_SetWeekEvent(WEEKEVENTREG_PLAYING_SWORDSMAN_MINIGAME);
    this->numLogs = 1;
    this->timer = 0;
    this->round = NOVICE_SIDE_JUMP;
    this->courseState = COURSE_ACTIVE;
    this->func = NvMmKendo_NoviceCourse;
}

/** MM's EnKendoJs_NoviceCourse. */
static void NvMmKendo_NoviceCourse(NvMmKendo* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if ((Message_GetState(&play->msgCtx) == TEXT_STATE_EVENT) && Message_ShouldAdvance(play)) {
        if (this->curTextId == 0x2729) { // wrong!
            NvMmKendo_StartMsg(this, play, sNoviceTryAgainTextIds[this->round]);
        } else if (!NvMmKendo_MoreAdvice(this, play)) {
            if (this->skelAnime.animation == (AnimationHeader*)sAnimFlailing) {
                NvMmKendo_ChangeAnim(this, ANIM_STOP_FLAILING);
            }
            this->courseState = COURSE_RETRY;
            Message_CloseTextbox(play);
            player->stateFlags1 &= ~PLAYER_STATE1_INPUT_DISABLED;
        }
    } else if (this->courseState == COURSE_RETRY) {
        this->courseState = COURSE_ACTIVE;
    }
    if (this->courseState != COURSE_ACTIVE) {
        return;
    }
    switch (NvMmKendo_NoviceResult(this, play)) {
        case RESULT_CORRECT:
            this->courseState = COURSE_PAUSED;
            Audio_PlayActorSound2(&this->actor, NA_SE_SY_TRE_BOX_APPEAR);
            player->stateFlags1 |= PLAYER_STATE1_INPUT_DISABLED;
            NvMmKendo_StartMsg(this, play, sNoviceProgressTextIds[this->round]);
            this->round++;
            break;
        case RESULT_INCORRECT:
            Audio_PlayActorSound2(&this->actor, NA_SE_SY_ERROR);
            this->courseState = COURSE_PAUSED;
            player->stateFlags1 |= PLAYER_STATE1_INPUT_DISABLED;
            NvMmKendo_StartMsg(this, play, 0x2729);
            NvMmKendo_ChangeAnim(this, ANIM_FLAILING);
            break;
        default:
            if ((this->numLogs == 0) && (++this->timer == 30)) {
                this->timer = 0;
                NvMmKendo_SpawnLog(this, play, 0);
            }
            break;
    }
    if (getenv("NV_LOG") != NULL) {
        static s16 sLastRound = -1;

        if (this->round != sLastRound) {
            sLastRound = this->round;
            printf("NVMM swordsman: novice lesson %d\n", (int)this->round);
            fflush(stdout);
        }
    }
    if ((this->skelAnime.animation == (AnimationHeader*)sAnimStopFlailing) &&
        Animation_OnFrame(&this->skelAnime, this->skelAnime.endFrame)) {
        NvMmKendo_ChangeAnim(this, ANIM_DEFAULT);
    }
    if (this->round == NOVICE_FINISHED) {
        NvMm_ClearWeekEvent(WEEKEVENTREG_PLAYING_SWORDSMAN_MINIGAME);
        this->func = NvMmKendo_HandleMessageState;
    }
}

static void NvMmKendo_MovePlayerToStartExpert(NvMmKendo* this, PlayState* play);
static void NvMmKendo_ExpertCourse(NvMmKendo* this, PlayState* play);

static void NvMmKendo_SetupExpert(NvMmKendo* this) {
    NvMm_SetWeekEvent(WEEKEVENTREG_PLAYING_SWORDSMAN_MINIGAME);
    this->timer = 120;
    this->round = 0;
    this->courseState = COURSE_ACTIVE;
    this->func = NvMmKendo_MovePlayerToStartExpert;
}

static void NvMmKendo_MovePlayerToStartExpert(NvMmKendo* this, PlayState* play) {
    Vec3f start = this->actor.world.pos;

    start.z += 300.0f;
    if (NvMmKendo_MovePlayerToPos(play, start)) {
        this->numLogs = 0;
        this->func = NvMmKendo_ExpertCourse;
    }
}

static void NvMmKendo_RetractLogs(NvMmKendo* this, PlayState* play);

/** MM's EnKendoJs_ExpertCourse: five rounds of two logs. */
static void NvMmKendo_ExpertCourse(NvMmKendo* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (this->timer >= 140) {
        if (this->round == 5) {
            NvMmKendo_StartMsg(this, play, (gNvMmMinigameScore == 30) ? 0x272D : 0x272E);
            player->stateFlags1 |= PLAYER_STATE1_INPUT_DISABLED;
            NvMm_ClearWeekEvent(WEEKEVENTREG_PLAYING_SWORDSMAN_MINIGAME);
            if (getenv("NV_LOG") != NULL) {
                printf("NVMM swordsman: expert course over, %d points\n", (int)gNvMmMinigameScore);
                fflush(stdout);
            }
            this->func = NvMmKendo_HandleMessageState;
            return;
        }
        Sfx_PlaySfxCentered(NA_SE_SY_FOUND);
        NvMmKendo_SpawnLog(this, play, ((u8)Rand_Next() % 3) + 1);
        NvMmKendo_SpawnLog(this, play, ((u8)Rand_Next() % 3) + 4);
        this->timer = 0;
        this->round++;
    } else if (this->timer == 120) {
        NvMmKendo_RetractLogs(this, play);
        this->timer++;
    } else {
        this->timer++;
    }
    { // MM's EnKendoJs_SetExpertCourseCamera: his cutscene's camera as the main camera through the course
        void NvMmCs_MainCameraFromCutscene(PlayState * play, s16 csId, Actor * target);
        s32 NvMmCs_ActorCsId(Actor * actor);

        NvMmCs_MainCameraFromCutscene(play, NvMmCs_ActorCsId(&this->actor), &this->actor);
    }
    if (this->isSlashingLog) {
        s32 mwa = player->meleeWeaponAnimation;
        s32 points;

        if ((mwa == PLAYER_MWA_JUMPSLASH_START) || (mwa == PLAYER_MWA_JUMPSLASH_FINISH)) {
            points = 3;
        } else if (mwa == PLAYER_MWA_STAB_1H) {
            points = 2;
        } else {
            points = 1;
        }
        gNvMmMinigameScore += points;
        Audio_PlayActorSound2(&this->actor, NA_SE_SY_TRE_BOX_APPEAR);
        this->isSlashingLog = false;
    }
}

/** MM's EnKendoJs_AwardPrize, then EnKendoJs_AwaitFinalMessage. */
static void NvMmKendo_AwaitFinalMessage(NvMmKendo* this, PlayState* play);

static void NvMmKendo_AwardPrize(NvMmKendo* this, PlayState* play) {
    if (Actor_HasParent(&this->actor, play)) {
        this->actor.parent = NULL;
        this->func = NvMmKendo_AwaitFinalMessage;
    } else {
        s32 gi = NvMm_CheckWeekEvent(WEEKEVENTREG_RECEIVED_SWORDSMANS_SCHOOL_HEART_PIECE) ? MM_GI_RUPEE_RED
                                                                                         : MM_GI_HEART_PIECE;

        GiveItemEntryFromActor(&this->actor, play, NvMmGetItem_Entry(gi), 800.0f, 100.0f);
    }
}

static void NvMmKendo_AwaitFinalMessage(NvMmKendo* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s16 id = NvMm_CheckWeekEvent(WEEKEVENTREG_RECEIVED_SWORDSMANS_SCHOOL_HEART_PIECE) ? 0x2730 : 0x272F;

    if (play->msgCtx.msgMode != MSGMODE_NONE) { // (the item's own message first)
        return;
    }
    NvMm_SetWeekEvent(WEEKEVENTREG_RECEIVED_SWORDSMANS_SCHOOL_HEART_PIECE);
    NvMmKendo_StartMsg(this, play, id);
    player->stateFlags1 &= ~PLAYER_STATE1_INPUT_DISABLED;
    this->func = NvMmKendo_HandleMessageState;
}

static void NvMmKendo_SpawnLog(NvMmKendo* this, PlayState* play, s32 position) {
    if (this->pathPoints == NULL) {
        return;
    }
    Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, NvMmActor_Id(MM_EN_MARUTA), this->pathPoints[position].x,
                       this->pathPoints[position].y, this->pathPoints[position].z, 0, 0, 0, 0);
    this->numLogs++;
}

void NvMmKendoJs_Update(Actor* thisx, PlayState* play) {
    NvMmKendo* this = (NvMmKendo*)thisx;
    Player* player = GET_PLAYER(play);

    this->func(this, play);
    SkelAnime_Update(&this->skelAnime);
    Actor_TrackPlayer(play, &this->actor, &this->headRot, &this->torsoRot, this->actor.focus.pos);
    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    // MM's EnKendoJs_KeepPlayerInTrainingArea
    if (((thisx->params & 0xFF) != IN_BACK_ROOM) && (player->actor.world.pos.z < (thisx->world.pos.z + 70.0f))) {
        player->actor.world.pos.z = thisx->world.pos.z + 70.0f;
    }
}

static s32 NvMmKendo_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                      void* thisx) {
    NvMmKendo* this = (NvMmKendo*)thisx;

    if (limbIndex == OBJECT_JS_LIMB_0C) {
        rot->y -= this->headRot.y;
    }
    return false;
}

void NvMmKendoJs_Draw(Actor* thisx, PlayState* play) {
    NvMmKendo* this = (NvMmKendo*)thisx;

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          NvMmKendo_OverrideLimbDraw, NULL, &this->actor);
}

/* ---------------------------------------------------------------------------------------------- */
/* The practice logs (En_Maruta)                                                                   */

#define MARUTA(name) ((Gfx*)"__OTR__mm/objects/object_maruta/" name)

enum {
    SHAPE_WHOLE,
    SHAPE_DIAGONAL_LEFT_CUT_BOTTOM_HALF,
    SHAPE_DIAGONAL_LEFT_CUT_TOP_HALF,
    SHAPE_DIAGONAL_RIGHT_CUT_BOTTOM_HALF,
    SHAPE_DIAGONAL_RIGHT_CUT_TOP_HALF,
    SHAPE_VERTICAL_CUT_LEFT_HALF,
    SHAPE_VERTICAL_CUT_RIGHT_HALF,
    SHAPE_HORIZONTAL_CUT_BOTTOM_HALF,
    SHAPE_HORIZONTAL_CUT_TOP_HALF,
};

static Gfx* sChunkDLs[] = {
    MARUTA("gPracticeLogBottomLeftBigChunkDL"),     MARUTA("gPracticeLogBottomRightBigChunkDL"),
    MARUTA("gPracticeLogBottomLeftCornerChunkDL"),  MARUTA("gPracticeLogBottomRightCornerChunkDL"),
    MARUTA("gPracticeLogTopLeftCornerChunkDL"),     MARUTA("gPracticeLogTopRightCornerChunkDL"),
    MARUTA("gPracticeLogTopLeftBigChunkDL"),        MARUTA("gPracticeLogTopRightBigChunkDL"),
};
static const u8 sFragmentsByShape[] = { 0xFF, 0x2B, 0xD4, 0x17, 0xE8, 0x55, 0xAA, 0x0F, 0xF0 };

/** MM's sShapesAfterMeleeWeaponAnimations, by OoT's swings (OoT's list, without MM's Goron and Zora ones). */
static const u8 sShapesAfterSwing[PLAYER_MWA_MAX] = {
    SHAPE_VERTICAL_CUT_LEFT_HALF,         // FORWARD_SLASH_1H
    SHAPE_VERTICAL_CUT_LEFT_HALF,         // FORWARD_SLASH_2H
    SHAPE_DIAGONAL_RIGHT_CUT_BOTTOM_HALF, // FORWARD_COMBO_1H
    SHAPE_DIAGONAL_RIGHT_CUT_BOTTOM_HALF, // FORWARD_COMBO_2H
    SHAPE_HORIZONTAL_CUT_BOTTOM_HALF,     // RIGHT_SLASH_1H
    SHAPE_HORIZONTAL_CUT_BOTTOM_HALF,     // RIGHT_SLASH_2H
    SHAPE_HORIZONTAL_CUT_BOTTOM_HALF,     // RIGHT_COMBO_1H
    SHAPE_HORIZONTAL_CUT_BOTTOM_HALF,     // RIGHT_COMBO_2H
    SHAPE_DIAGONAL_RIGHT_CUT_BOTTOM_HALF, // LEFT_SLASH_1H
    SHAPE_DIAGONAL_RIGHT_CUT_BOTTOM_HALF, // LEFT_SLASH_2H
    SHAPE_DIAGONAL_RIGHT_CUT_BOTTOM_HALF, // LEFT_COMBO_1H
    SHAPE_DIAGONAL_RIGHT_CUT_BOTTOM_HALF, // LEFT_COMBO_2H
    SHAPE_HORIZONTAL_CUT_BOTTOM_HALF,     // STAB_1H
    SHAPE_HORIZONTAL_CUT_BOTTOM_HALF,     // STAB_2H
    SHAPE_DIAGONAL_RIGHT_CUT_BOTTOM_HALF, // STAB_COMBO_1H
    SHAPE_DIAGONAL_RIGHT_CUT_BOTTOM_HALF, // STAB_COMBO_2H
    SHAPE_WHOLE,                          // FLIPSLASH_START
    SHAPE_WHOLE,                          // JUMPSLASH_START
    SHAPE_WHOLE,                          // FLIPSLASH_FINISH
    SHAPE_VERTICAL_CUT_LEFT_HALF,         // JUMPSLASH_FINISH
    SHAPE_WHOLE,                          // BACKSLASH_RIGHT
    SHAPE_WHOLE,                          // BACKSLASH_LEFT
    SHAPE_WHOLE,                          // HAMMER_FORWARD
    SHAPE_WHOLE,                          // HAMMER_SIDE
    SHAPE_HORIZONTAL_CUT_BOTTOM_HALF,     // SPIN_ATTACK_1H
    SHAPE_HORIZONTAL_CUT_BOTTOM_HALF,     // SPIN_ATTACK_2H
    SHAPE_HORIZONTAL_CUT_BOTTOM_HALF,     // BIG_SPIN_1H
    SHAPE_HORIZONTAL_CUT_BOTTOM_HALF,     // BIG_SPIN_2H
};

static Vec3f sDiagonalLeftCutTopHalfVelocity = { -2.0f, 3.0f, 0.0f };
static Vec3f sDiagonalRightCutTopHalfVelocitySlideOff = { 2.0f, 3.0f, 0.0f };
static Vec3f sDiagonalRightCutTopHalfVelocityUpAndOver = { -2.0f, 10.0f, 0.0f };
static Vec3f sVerticalCutLeftHalfVelocityForwardSlash = { -2.5f, 5.0f, 0.0f };
static Vec3f sVerticalCutLeftHalfVelocityJumpSlash = { -3.0f, 10.0f, 0.0f };
static Vec3f sVerticalCutRightHalfVelocityForwardSlash = { 2.5f, 5.0f, 0.0f };
static Vec3f sVerticalCutRightHalfVelocityJumpSlash = { 3.0f, 10.0f, 0.0f };
static Vec3f sHorizontalCutTopHalfVelocityLeft = { -1.0f, 7.0f, -1.0f };
static Vec3f sHorizontalCutTopHalfVelocityRight = { 1.0f, 7.0f, -1.0f };
static Vec3f sHorizontalCutBottomHalfVelocityLeft = { 0.0f, 8.0f, -1.5f };

static Vec3f sTranslationVectorsToRecoilAxis[] = {
    { 0.0f, 0.0f, 0.0f },    { 0.0f, 0.0f, 0.0f },    { -10.0f, 460.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f },    { 10.0f, 460.0f, 0.0f }, { -40.0f, 315.0f, 0.0f },
    { 40.0f, 315.0f, 0.0f }, { 0.0f, 140.0f, 0.0f },  { 0.0f, 445.0f, 0.0f },
};
static Vec3f sRecoilSpinAxesForShapes[] = {
    { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f },
    { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f },
};
static Vec3f sDiagonalLeftCutTopHalfHitboxPoints[] = {
    { 90.0f, 630.0f, -78.0f }, { -90.0f, 630.0f, -78.0f }, { -90.0f, 630.0f, 78.0f }, { 90.0f, 630.0f, 78.0f },
    { 90.0f, 350.0f, -78.0f }, { -90.0f, 170.0f, -78.0f }, { -90.0f, 170.0f, 78.0f }, { 90.0f, 350.0f, 78.0f },
};
static Vec3f sDiagonalRightCutTopHalfHitboxPoints[] = {
    { 90.0f, 630.0f, -78.0f }, { -90.0f, 630.0f, -78.0f }, { -90.0f, 630.0f, 78.0f }, { 90.0f, 630.0f, 78.0f },
    { 90.0f, 170.0f, -78.0f }, { -90.0f, 350.0f, -78.0f }, { -90.0f, 350.0f, 78.0f }, { 90.0f, 170.0f, 78.0f },
};
static Vec3f sVerticalCutLeftHalfHitboxPoints[] = {
    { 0.0f, 630.0f, -104.0f }, { -90.0f, 630.0f, -52.0f }, { -90.0f, 630.0f, 52.0f }, { 0.0f, 630.0f, 104.0f },
    { 0.0f, 0.0f, -104.0f },   { -90.0f, 0.0f, -52.0f },   { -90.0f, 0.0f, 52.0f },   { 0.0f, 0.0f, 104.0f },
};
static Vec3f sVerticalCutRightHalfHitboxPoints[] = {
    { 0.0f, 630.0f, -104.0f }, { 90.0f, 630.0f, -52.0f }, { 90.0f, 630.0f, 52.0f }, { 0.0f, 630.0f, 104.0f },
    { 0.0f, 0.0f, -104.0f },   { 90.0f, 0.0f, -52.0f },   { 90.0f, 0.0f, 52.0f },   { 0.0f, 0.0f, 104.0f },
};
static Vec3f sHorizontalCutTopHalfHitboxPoints[] = {
    { 90.0f, 630.0f, -78.0f }, { -90.0f, 630.0f, -78.0f }, { -90.0f, 630.0f, 78.0f }, { 90.0f, 630.0f, 78.0f },
    { 90.0f, 260.0f, -78.0f }, { -90.0f, 260.0f, -78.0f }, { -90.0f, 260.0f, 78.0f }, { 90.0f, 260.0f, 78.0f },
};
static Vec3f sHorizontalCutBottomHalfHitboxPoints[] = {
    { 90.0f, 260.0f, -78.0f }, { -90.0f, 260.0f, -78.0f }, { -90.0f, 260.0f, 78.0f }, { 90.0f, 260.0f, 78.0f },
    { 90.0f, 20.0f, -78.0f },  { -90.0f, 20.0f, -78.0f },  { -90.0f, 20.0f, 78.0f },  { 90.0f, 20.0f, 78.0f },
};

static ColliderCylinderInit sMarutaCylinderInit = {
    { COLTYPE_NONE, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK1, { 0, 0, 0 }, { 0xF7CFFFFF, 0, 0 }, TOUCH_NONE | TOUCH_SFX_NORMAL, BUMP_ON | BUMP_HOOKABLE,
      OCELEM_ON },
    { 12, 65, 0, { 0, 0, 0 } },
};

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };

struct NvMmMaruta;
typedef void (*NvMmMarutaFunc)(struct NvMmMaruta*, PlayState*);

typedef struct NvMmMaruta {
    Actor actor;
    NvMmMarutaFunc func;
    ColliderCylinder collider;
    Vec3f recoilSpinAxis;
    Vec3f* relativeHitboxPoints;
    Vec3f hitboxPoints[8];
    Vec3f centerPos;
    s32 shape;
    s16 recoilSpinAngle;
    s16 recoilSpinRate;
    s16 endingBounces;
    s16 timer;
    s16 isRetracting;
} NvMmMaruta;

size_t gNvMmMarutaSize = sizeof(NvMmMaruta);

static void NvMmMaruta_SittingWhole(NvMmMaruta* this, PlayState* play);
static void NvMmMaruta_RiseThroughFloor(NvMmMaruta* this, PlayState* play);
static void NvMmMaruta_RetractWhole(NvMmMaruta* this, PlayState* play);
static void NvMmMaruta_RetractAfterCut(NvMmMaruta* this, PlayState* play);
static void NvMmMaruta_SetupRecoilAfterCut(NvMmMaruta* this, PlayState* play);
static void NvMmMaruta_Flatten(NvMmMaruta* this, PlayState* play);

static NvMmKendo* NvMmMaruta_Swordsman(NvMmMaruta* this) {
    Actor* parent = this->actor.parent;

    return ((parent != NULL) && (parent->id == NvMmActor_Id(MM_EN_KENDO_JS))) ? (NvMmKendo*)parent : NULL;
}

void NvMmMaruta_Init(Actor* thisx, PlayState* play) {
    NvMmMaruta* this = (NvMmMaruta*)thisx;
    s32 i;

    Actor_SetScale(thisx, 0.1f);
    thisx->targetMode = 6;
    thisx->focus.pos = thisx->world.pos;
    thisx->focus.pos.y += 50.0f;
    thisx->colChkInfo.health = 1;
    thisx->world.pos.y -= 4.0f;
    thisx->home.pos.y -= 4.0f;
    this->shape = (thisx->params >> 8) & 0xFF;
    this->recoilSpinAxis.z = 1.0f;
    for (i = 0; i < ARRAY_COUNT(this->hitboxPoints); i++) {
        this->hitboxPoints[i] = sZeroVec;
    }
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sMarutaCylinderInit);
    thisx->colChkInfo.mass = MASS_HEAVY;
    if (this->shape == SHAPE_WHOLE) {
        if ((thisx->params & 0xFF) == 0xFF) {
            this->func = NvMmMaruta_SittingWhole;
        } else {
            thisx->world.pos.y = thisx->home.pos.y - 630.0f;
            this->func = NvMmMaruta_RiseThroughFloor;
        }
    } else {
        NvMmMaruta_SetupRecoilAfterCut(this, play);
    }
}

void NvMmMaruta_Destroy(Actor* thisx, PlayState* play) {
    NvMmMaruta* this = (NvMmMaruta*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

static void NvMmMaruta_SittingWhole(NvMmMaruta* this, PlayState* play) {
    s16 yawDiff = this->actor.yawTowardsPlayer - this->actor.shape.rot.y;
    NvMmKendo* js = NvMmMaruta_Swordsman(this);

    if (yawDiff > 0x1555) {
        this->actor.shape.rot.y += 0x2AAA;
    } else if (yawDiff < -0x1555) {
        this->actor.shape.rot.y -= 0x2AAA;
    }
    if (this->isRetracting) {
        this->collider.base.acFlags |= AC_HIT;
        this->actor.velocity.y = 0.0f;
        this->actor.gravity = -2.0f;
        this->func = NvMmMaruta_RetractWhole;
    }
    if (js != NULL) {
        js->isPlayerLockedOn = this->actor.isTargeted;
    }
}

static void NvMmMaruta_RiseThroughFloor(NvMmMaruta* this, PlayState* play) {
    if (Math_SmoothStepToF(&this->actor.world.pos.y, this->actor.home.pos.y, 0.4f, 100.0f, 10.0f) == 0.0f) {
        this->func = NvMmMaruta_SittingWhole;
    }
}

static void NvMmMaruta_RetractWhole(NvMmMaruta* this, PlayState* play) {
    if ((this->actor.floorHeight - 630.0f) < this->actor.world.pos.y) {
        this->actor.velocity.y += this->actor.gravity;
        this->actor.world.pos.y += this->actor.velocity.y;
    } else {
        NvMmKendo* js = NvMmMaruta_Swordsman(this);

        if (js != NULL) {
            js->numLogs--;
        }
        Actor_Kill(&this->actor);
    }
}

static void NvMmMaruta_BottomHalfSittingOnFloor(NvMmMaruta* this) {
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    if (this->func != NvMmMaruta_RetractWhole) {
        this->timer = 0;
        this->actor.gravity = -2.0f;
        this->func = NvMmMaruta_RetractAfterCut;
    }
}

static void NvMmMaruta_RetractAfterCut(NvMmMaruta* this, PlayState* play) {
    if (this->timer == 40) {
        Actor_Kill(&this->actor);
        return;
    }
    if (((this->actor.floorHeight - 630.0f) < this->actor.world.pos.y) && (this->timer > 30)) {
        this->actor.velocity.y += this->actor.gravity;
        this->actor.world.pos.y += this->actor.velocity.y;
    }
    this->timer++;
}

static void NvMmMaruta_RotateVector(Vec3f* src, Vec3f* dest, s16 rotX, s16 rotY, s16 rotZ) {
    Matrix_Push();
    Matrix_RotateZYX(rotX, rotY, rotZ, MTXMODE_NEW);
    Matrix_MultVec3f(src, dest);
    Matrix_Pop();
}

static void NvMmMaruta_Recoil(NvMmMaruta* this, PlayState* play);

/** MM's EnMaruta_SetupRecoilAfterCut: a cut piece flies off. */
static void NvMmMaruta_SetupRecoilAfterCut(NvMmMaruta* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Vec3f v;
    Vec3f spinAxis;
    s16 rotY;

    this->timer = 0;
    this->endingBounces = 0;
    this->actor.gravity = -2.0f;
    switch (this->shape) {
        case SHAPE_DIAGONAL_LEFT_CUT_TOP_HALF:
            v = sDiagonalLeftCutTopHalfVelocity;
            this->relativeHitboxPoints = sDiagonalLeftCutTopHalfHitboxPoints;
            break;
        case SHAPE_DIAGONAL_RIGHT_CUT_TOP_HALF:
            v = (player->meleeWeaponAnimation == PLAYER_MWA_LEFT_SLASH_1H) ? sDiagonalRightCutTopHalfVelocitySlideOff
                                                                          : sDiagonalRightCutTopHalfVelocityUpAndOver;
            this->relativeHitboxPoints = sDiagonalRightCutTopHalfHitboxPoints;
            break;
        case SHAPE_VERTICAL_CUT_LEFT_HALF:
            v = (player->meleeWeaponAnimation == PLAYER_MWA_FORWARD_SLASH_1H) ? sVerticalCutLeftHalfVelocityForwardSlash
                                                                             : sVerticalCutLeftHalfVelocityJumpSlash;
            this->relativeHitboxPoints = sVerticalCutLeftHalfHitboxPoints;
            break;
        case SHAPE_VERTICAL_CUT_RIGHT_HALF:
            v = (player->meleeWeaponAnimation == PLAYER_MWA_FORWARD_SLASH_1H)
                    ? sVerticalCutRightHalfVelocityForwardSlash
                    : sVerticalCutRightHalfVelocityJumpSlash;
            this->relativeHitboxPoints = sVerticalCutRightHalfHitboxPoints;
            break;
        case SHAPE_HORIZONTAL_CUT_TOP_HALF:
            v = (player->meleeWeaponAnimation == PLAYER_MWA_RIGHT_SLASH_1H) ? sHorizontalCutTopHalfVelocityLeft
                                                                           : sHorizontalCutTopHalfVelocityRight;
            this->relativeHitboxPoints = sHorizontalCutTopHalfHitboxPoints;
            break;
        case SHAPE_HORIZONTAL_CUT_BOTTOM_HALF:
            v = sHorizontalCutBottomHalfVelocityLeft;
            this->relativeHitboxPoints = sHorizontalCutBottomHalfHitboxPoints;
            break;
        default:
            v = sZeroVec;
            break;
    }
    rotY = this->actor.shape.rot.y;
    this->actor.velocity.x = (v.x * Math_CosS(rotY)) + (Math_SinS(rotY) * v.z);
    this->actor.velocity.y = v.y;
    this->actor.velocity.z = (-v.x * Math_SinS(rotY)) + (Math_CosS(rotY) * v.z);
    spinAxis = sRecoilSpinAxesForShapes[this->shape];
    NvMmMaruta_RotateVector(&spinAxis, &this->recoilSpinAxis, Rand_Next() & 0xFFF, Rand_Next() & 0xFFF, 0);
    this->recoilSpinRate = Rand_Next() & 0x7FF;
    this->recoilSpinRate |= (this->shape == SHAPE_HORIZONTAL_CUT_BOTTOM_HALF) ? 0x3F : 0xFF;
    this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    this->func = NvMmMaruta_Recoil;
}

static void NvMmMaruta_FindRestingOrientation(NvMmMaruta* this, PlayState* play);
static void NvMmMaruta_WaitToFlatten(NvMmMaruta* this, PlayState* play);

static void NvMmMaruta_LerpSpinAngleToFlat(s16* angle) {
    s16 flat = *angle & 0xC000;

    if ((*angle & 0x3FFF) > 0x2000) {
        flat += 0x4000;
    }
    Math_SmoothStepToS(angle, flat, 1, 0xAAA, 0xB6);
}

/** MM's EnMaruta_AdjustBounceAngle. */
static void NvMmMaruta_AdjustBounceAngle(NvMmMaruta* this, Vec3f* lowestPoint) {
    Vec3f spinAxis;
    f32 det;
    f32 bound;

    NvMmMaruta_RotateVector(&this->recoilSpinAxis, &spinAxis, 0, this->actor.shape.rot.y, 0);
    det = (spinAxis.z * (lowestPoint->x - this->centerPos.x)) - (spinAxis.x * (lowestPoint->z - this->centerPos.z));
    if ((this->shape == SHAPE_VERTICAL_CUT_LEFT_HALF) || (this->shape == SHAPE_VERTICAL_CUT_RIGHT_HALF)) {
        bound = 8.0f;
    } else if (this->shape == SHAPE_DIAGONAL_RIGHT_CUT_TOP_HALF) {
        bound = 5.0f;
    } else {
        bound = 2.0f;
    }
    bound = ((det < bound) && (-bound < det)) ? 3.0f : 1.2f;
    if (det > 0.0f) {
        this->recoilSpinRate *= (this->recoilSpinRate > 0) ? bound : -0.8f;
    } else {
        this->recoilSpinRate *= (this->recoilSpinRate > 0) ? -0.8f : bound;
    }
    if ((ABS(this->recoilSpinRate) < 0x38E) &&
        (((this->recoilSpinAngle & 0x3FFF) < 0x71C) || ((this->recoilSpinAngle & 0x3FFF) >= 0x38E4))) {
        this->actor.gravity *= 0.8f;
        if (++this->endingBounces == 2) { // MM's EnMaruta_StopBouncing
            this->timer = 0;
            this->actor.gravity = 0.0f;
            this->actor.velocity.y = 0.0f;
            this->recoilSpinRate = 0;
            this->func = NvMmMaruta_FindRestingOrientation;
        }
    } else if (ABS(this->recoilSpinRate) > 0x38E) {
        this->recoilSpinRate = (this->recoilSpinRate < 0) ? -0x38E : 0x38E;
    }
}

/** MM's EnMaruta_Bounce. */
static void NvMmMaruta_Bounce(NvMmMaruta* this, PlayState* play) {
    static Color_RGBA8 sPrim = { 170, 130, 90, 255 };
    static Color_RGBA8 sEnv = { 100, 60, 20, 255 };
    Vec3f lowest = this->centerPos;
    s32 i;

    for (i = 0; i < ARRAY_COUNT(this->hitboxPoints); i++) {
        if (this->hitboxPoints[i].y < lowest.y) {
            lowest = this->hitboxPoints[i];
        }
    }
    if (lowest.y < this->actor.floorHeight) {
        this->recoilSpinAngle -= this->recoilSpinRate;
        this->actor.velocity.y -= this->actor.gravity;
        this->actor.velocity.x *= 0.6f;
        this->actor.velocity.z *= 0.6f;
        this->actor.world.pos.y += this->actor.floorHeight - lowest.y;
        if (this->actor.velocity.y < -this->actor.gravity) {
            Vec3f pos = lowest;
            Vec3f vel;
            Vec3f accel;

            pos.y += 15.0f;
            accel.y = 0.0f;
            for (i = 0; i < 10; i++) { // MM's EnMaruta_SpawnDustClouds
                vel.x = Rand_CenteredFloat(10.0f);
                vel.y = 2.0f * Rand_ZeroOne();
                vel.z = Rand_CenteredFloat(10.0f);
                accel.x = -0.2f * vel.x;
                accel.z = -0.2f * vel.z;
                EffectSsDust_Spawn(play, 0, &pos, &vel, &accel, &sPrim, &sEnv, 60, 20, 10, 0);
            }
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_WOOD_BOUND);
            this->actor.velocity.y *= -0.6f;
            NvMmMaruta_AdjustBounceAngle(this, &lowest);
        }
    }
}

static void NvMmMaruta_Recoil(NvMmMaruta* this, PlayState* play) {
    this->recoilSpinAngle += this->recoilSpinRate;
    this->actor.velocity.y += this->actor.gravity;
    NvMmMaruta_Bounce(this, play);
    Actor_UpdatePos(&this->actor);
}

static void NvMmMaruta_FindRestingOrientation(NvMmMaruta* this, PlayState* play) {
    if (this->timer == 40) {
        // MM's EnMaruta_SetupWaitToFlatten: a vertical half resting on its flat side waits less
        this->timer = (((this->shape == SHAPE_VERTICAL_CUT_LEFT_HALF) || (this->shape == SHAPE_VERTICAL_CUT_RIGHT_HALF)) &&
                       !(this->recoilSpinAngle & 0x7FFF))
                          ? 100
                          : 0;
        this->func = NvMmMaruta_WaitToFlatten;
    } else {
        this->timer++;
    }
    NvMmMaruta_LerpSpinAngleToFlat(&this->recoilSpinAngle);
}

static void NvMmMaruta_WaitToFlatten(NvMmMaruta* this, PlayState* play) {
    if (this->timer > 100) {
        this->actor.colChkInfo.health = 0;
    } else {
        this->timer++;
    }
}

static void NvMmMaruta_Flatten(NvMmMaruta* this, PlayState* play) {
    if (this->actor.scale.y == 0.0f) {
        if (this->actor.scale.x == 0.0f) {
            Actor_Kill(&this->actor);
            return;
        }
        Math_SmoothStepToF(&this->actor.scale.x, 0.0f, 0.2f, 0.01f, 0.001f);
        Math_SmoothStepToF(&this->actor.scale.z, 0.0f, 0.2f, 0.01f, 0.001f);
    } else {
        Math_SmoothStepToF(&this->actor.scale.y, 0.0f, 0.2f, 0.003f, 0.001f);
    }
}

/** MM's EnMaruta_IsHittable: facing it, or a swing that reaches round. */
static s32 NvMmMaruta_IsHittable(NvMmMaruta* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s16 yawDiff = (s16)(this->actor.yawTowardsPlayer - 0x8000) - player->actor.shape.rot.y;
    s32 mwa = player->meleeWeaponAnimation;

    return (ABS(yawDiff) < 0x1555) ||
           ((player->meleeWeaponState != 0) && ((mwa == PLAYER_MWA_RIGHT_SLASH_1H) || (mwa == PLAYER_MWA_RIGHT_COMBO_1H) ||
                                                (mwa == PLAYER_MWA_SPIN_ATTACK_1H) || (mwa == PLAYER_MWA_BIG_SPIN_1H)));
}

/** MM's EnMaruta_UpdateCollider: a sword's swing cuts the log by its shape. */
static void NvMmMaruta_UpdateCollider(NvMmMaruta* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (!((this->func == NvMmMaruta_SittingWhole) || (this->func == NvMmMaruta_RiseThroughFloor) ||
          (this->func == NvMmMaruta_RetractAfterCut) || (this->func == NvMmMaruta_Flatten) ||
          ((this->func == NvMmMaruta_RetractWhole) && !(this->actor.world.pos.y < (this->actor.floorHeight - 20.0f))))) {
        return;
    }
    if ((this->collider.base.acFlags & AC_HIT) && (this->func == NvMmMaruta_SittingWhole)) {
        s32 mwa = CLAMP(player->meleeWeaponAnimation, 0, PLAYER_MWA_MAX - 1);

        this->collider.base.acFlags &= ~AC_HIT;
        Audio_PlayActorSound2(&this->actor, NA_SE_IT_SWORD_STRIKE);
        if ((player->meleeWeaponState != 0) && (sShapesAfterSwing[mwa] != SHAPE_WHOLE)) {
            NvMmKendo* js = NvMmMaruta_Swordsman(this);

            this->shape = sShapesAfterSwing[mwa];
            // the other half, its shape one on
            Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, this->actor.id, this->actor.world.pos.x,
                               this->actor.world.pos.y, this->actor.world.pos.z, 0, this->actor.shape.rot.y, 0,
                               ((this->shape + 1) << 8) & 0xFF00);
            this->actor.world.rot.y = this->actor.shape.rot.y;
            if ((this->shape == SHAPE_VERTICAL_CUT_LEFT_HALF) ||
                ((this->shape == SHAPE_HORIZONTAL_CUT_BOTTOM_HALF) && (mwa == PLAYER_MWA_STAB_1H))) {
                NvMmMaruta_SetupRecoilAfterCut(this, play);
            } else {
                NvMmMaruta_BottomHalfSittingOnFloor(this);
            }
            if (js != NULL) {
                js->isSlashingLog = true;
                js->numLogs--;
            }
            return;
        }
    }
    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    if (NvMmMaruta_IsHittable(this, play) && (this->func == NvMmMaruta_SittingWhole)) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
}

void NvMmMaruta_Update(Actor* thisx, PlayState* play) {
    NvMmMaruta* this = (NvMmMaruta*)thisx;

    this->func(this, play);
    NvMmMaruta_UpdateCollider(this, play);
    if ((this->func != NvMmMaruta_Flatten) && (thisx->colChkInfo.health == 0)) {
        this->func = NvMmMaruta_Flatten;
    }
}

void NvMmMaruta_Draw(Actor* thisx, PlayState* play) {
    NvMmMaruta* this = (NvMmMaruta*)thisx;
    s32 i;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    if (this->shape == SHAPE_WHOLE) {
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, MARUTA("gPracticeLogWholeDL"));
    } else {
        Vec3f t = sTranslationVectorsToRecoilAxis[this->shape];

        Matrix_Push();
        Matrix_Translate(t.x, t.y, t.z, MTXMODE_APPLY);
        Matrix_RotateAxis(BINANG_TO_RAD(this->recoilSpinAngle), &this->recoilSpinAxis, MTXMODE_APPLY);
        Matrix_Translate(-t.x, -t.y, -t.z, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        for (i = 0; i < 8; i++) {
            if (sFragmentsByShape[this->shape] & (1 << i)) {
                gSPDisplayList(POLY_OPA_DISP++, sChunkDLs[i]);
            }
        }
        if (this->relativeHitboxPoints != NULL) {
            for (i = 0; i < ARRAY_COUNT(this->hitboxPoints); i++) {
                Matrix_MultVec3f(&this->relativeHitboxPoints[i], &this->hitboxPoints[i]);
            }
            Matrix_MultVec3f(&t, &this->centerPos);
        }
        Matrix_Pop();
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

/** MM's EnKendoJs_RetractLogs. */
static void NvMmKendo_RetractLogs(NvMmKendo* this, PlayState* play) {
    Actor* a;

    for (a = play->actorCtx.actorLists[ACTORCAT_PROP].head; a != NULL; a = a->next) {
        if (a->id == NvMmActor_Id(MM_EN_MARUTA)) {
            ((NvMmMaruta*)a)->isRetracting = true;
        }
    }
    this->numLogs = 0;
}
