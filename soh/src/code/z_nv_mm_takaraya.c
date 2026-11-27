/*
 * Nightmareverse: the Treasure Chest Shop (mm/README.md "The Treasure Chest Shop"), from MM's
 * En_Takaraya (the gal, MM's object_bg) and Obj_Takaraya_Wall (the board: MM's
 * object_takaraya_objects), zeldaret/mm, ported closely.
 *
 * The gal: her greeting and price by Link's form (Deku 10, human and the Fierce Deity 20, Goron 30,
 * Zora 5; MM's message prices), paying, the board, the 45-second timer, and back to the counter
 * (MM's entrance 1) with "Did you have fun?" or "Time's up!". The prize by form, the second time a
 * smaller one (MM's switch flags, params >> 8 plus the form). The board: a random path carved from
 * the chest's square in the back row to the front, as MM's; the other squares' walls rise around
 * Link as he walks the board and sink behind him, and push him back (two OC cylinders, as MM's).
 * The chest is OoT's En_Box (MM's switch-flag big chest, numbered as OoT's: type 11, its switch flag
 * in rot.z), in MM's model (z_nv_mm_box.c), giving MM's get-item.
 *
 * Adapted from MM:
 * - MM's camera sweep over the board as the chest appears is her actor cutscene (z_nv_mm_csmgr.c),
 *   started by her as MM's chest would start it (OoT's chest has no MM cutscene);
 * - MM's timer is OoT's main timer (its count to 0 stops harmlessly); MM's success state is kept here;
 * - the masks' reactions (Text_GetFaceReaction) wait for MM's face-reaction tables;
 * - the closing-time kick-out (week events KICKOUT_*) isn't followed;
 * - MM's minigame music isn't in OoT's banks.
 */
#include "global.h"
#include <stdio.h>
#include <stdlib.h>
#include "soh/frame_interpolation.h"

#define NV_MM_TEXT(id) (0xB000 + (id))
#define MM_GI_RUPEE_RED 0x04
#define MM_GI_RUPEE_PURPLE 0x05
#define MM_GI_HEART_PIECE 0x0C
#define MM_GI_DEKU_NUTS_5 0x29
#define MM_GI_DEKU_NUTS_10 0x2A
#define ENBOX_TYPE_SWITCH_FLAG_BIG_MM 11
#define MM_TAKARAYA_WALL 0x1BB

s32 NvMmActor_Id(s32 mmId);
s32 NvMmForm_Get(void); // MM's PLAYER_FORM_*: 0 the Fierce Deity, 1 Goron, 2 Zora, 3 Deku, 4 human
uint16_t NvMmText_Next(uint16_t id);
s32 Object_Spawn(ObjectContext* objectCtx, s16 objectId);

/* ---------------------------------------------------------------------------------------------- */
/* The gal (En_Takaraya)                                                                          */

#define BG(name) ("__OTR__mm/objects/object_bg/" name)
#define GAL_LIMB_HEAD 5
#define GAL_LIMB_MAX 0x18

static const char sGalSkel[] = BG("gTreasureChestShopGalSkel");
static const char sAnimIdle[] = BG("object_bg_Anim_009890");
static const char sAnimTalkLoop[] = BG("object_bg_Anim_001384");
static const char sAnimBow[] = BG("object_bg_Anim_00AD98");
static const char sAnimStartTalk[] = BG("object_bg_Anim_00A280");
static const char sAnimPoint[] = BG("object_bg_Anim_000968");
static const char* sEyesUp[] = { BG("gTreasureChestShopGalEyeOpenUpTex"), BG("gTreasureChestShopGalEyeHalfUpTex"),
                                 BG("gTreasureChestShopGalEyeClosedTex"), BG("gTreasureChestShopGalEyeHalfUpTex") };
static const char* sEyesDown[] = { BG("gTreasureChestShopGalEyeOpenDownTex"), BG("gTreasureChestShopGalEyeHalfDownTex"),
                                   BG("gTreasureChestShopGalEyeClosedTex"), BG("gTreasureChestShopGalEyeHalfDownTex") };

static const u16 sGreetings[5] = { 0x76D, 0x76E, 0x76F, 0x76C, 0x76D }; // (MM's form order)
static const u8 sPrizes[5][2] = {
    { MM_GI_RUPEE_PURPLE, MM_GI_RUPEE_PURPLE }, { MM_GI_HEART_PIECE, MM_GI_RUPEE_PURPLE },
    { MM_GI_RUPEE_RED, MM_GI_RUPEE_RED },       { MM_GI_DEKU_NUTS_10, MM_GI_DEKU_NUTS_5 },
    { MM_GI_RUPEE_PURPLE, MM_GI_RUPEE_PURPLE },
};
static const u16 sDeclined[5] = { 0x775, 0x776, 0x777, 0x774, 0x775 };
static const u16 sPriceLines[5] = { 0x771, 0x772, 0x773, 0x770, 0x771 };
static const s16 sPrices[5] = { 20, 30, 5, 10, 20 }; // (MM's message headers' first choice price)

enum { NV_TAKARAYA_NONE, NV_TAKARAYA_PLAYING, NV_TAKARAYA_WON, NV_TAKARAYA_LOST };
static s32 sGameState; // MM's TIMER_ID_MINIGAME_2 state across the reload (TIMER_STATE_6: won)

struct NvMmTakaraya;
typedef void (*NvMmTakarayaFunc)(struct NvMmTakaraya*, PlayState*);

typedef struct NvMmTakaraya {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[GAL_LIMB_MAX];
    Vec3s morphTable[GAL_LIMB_MAX];
    NvMmTakarayaFunc func;
    u8 eyeTexIndex;
    u8 formSwitchFlag;
    u8 switchFlag;
    u8 endingGame; // MM's unk2AD
    s16 timer;
    Vec3s headRot;
    u16 textId; // MM's
    s32 csStarted;
} NvMmTakaraya;

s32 NvMmCs_ActorCsId(Actor* actor);
void NvMmCs_SetActorCsId(Actor* actor, s32 csId);
void NvMmCsMgr_Queue(s16 csId);
s16 NvMmCsMgr_IsNext(s16 csId);
s16 NvMmCsMgr_StartWithPlayerCs(s16 csId, Actor* actor);
s16 NvMmCsMgr_GetCurrentCsId(void);
s16 NvMmCsMgr_GetCurrentSubCamId(s16 csId);

size_t gNvMmTakarayaSize = sizeof(NvMmTakaraya);

static void NvMmTakaraya_Wait(NvMmTakaraya* this, PlayState* play);
static void NvMmTakaraya_Talk(NvMmTakaraya* this, PlayState* play);
static void NvMmTakaraya_SetupReturned(NvMmTakaraya* this);

static s32 NvMmTakaraya_Form(void) {
    return CLAMP(NvMmForm_Get(), 0, 4);
}

static void NvMmTakaraya_SetText(NvMmTakaraya* this, u16 id) {
    this->textId = id;
    this->actor.textId = NV_MM_TEXT(id);
}

static void NvMmTakaraya_Continue(NvMmTakaraya* this, PlayState* play, u16 id) {
    NvMmTakaraya_SetText(this, id);
    Message_ContinueTextbox(play, this->actor.textId);
}

void NvMmTakaraya_Init(Actor* thisx, PlayState* play) {
    NvMmTakaraya* this = (NvMmTakaraya*)thisx;

    thisx->targetMode = 6;
    ActorShape_Init(&thisx->shape, -60.0f, NULL, 0.0f);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sGalSkel, (AnimationHeader*)sAnimIdle,
                       this->jointTable, this->morphTable, GAL_LIMB_MAX);
    this->switchFlag = (thisx->params >> 8) & 0xFF;
    thisx->params &= 0xFF;
    this->eyeTexIndex = 2;
    if (Object_GetIndex(&play->objectCtx, OBJECT_BOX) < 0) { // (the prize chest: OoT's, its object)
        Object_Spawn(&play->objectCtx, OBJECT_BOX);
    }
    this->func = NvMmTakaraya_Wait;
    if (gSaveContext.entranceIndex == ENTR_MM_Z2_TAKARAYA_1) { // back at the counter after a game
        NvMmTakaraya_SetText(this, (sGameState == NV_TAKARAYA_WON) ? 0x77A : 0x77C);
        NvMmTakaraya_SetupReturned(this);
    }
    sGameState = NV_TAKARAYA_NONE;
}

void NvMmTakaraya_Destroy(Actor* thisx, PlayState* play) {
    NvMmTakaraya* this = (NvMmTakaraya*)thisx;

    Flags_UnsetSwitch(play, 5);
    if (!this->endingGame) {
        gSaveContext.timerState = TIMER_STATE_OFF;
    }
}

static void NvMmTakaraya_SetupWait(NvMmTakaraya* this) {
    if (this->skelAnime.animation == (AnimationHeader*)sAnimTalkLoop) {
        Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sAnimBow, 5.0f);
    }
    this->func = NvMmTakaraya_Wait;
}

/** MM's EnTakaraya_Wait. */
static void NvMmTakaraya_Wait(NvMmTakaraya* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        if (this->skelAnime.animation == (AnimationHeader*)sAnimStartTalk) {
            Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sAnimBow, 5.0f);
        } else {
            Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)sAnimIdle, -4.0f);
        }
    }
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sAnimStartTalk, -4.0f);
        this->func = NvMmTakaraya_Talk;
    } else if (Actor_IsFacingPlayer(&this->actor, 0x2000)) {
        NvMmTakaraya_SetText(this, sGreetings[NvMmTakaraya_Form()]);
        this->formSwitchFlag = NvMmTakaraya_Form() + this->switchFlag;
        Actor_OfferTalk(&this->actor, play, 120.0f);
    }
}

/** MM's EnTakaraya_SpawnWalls: the board, and the prize in its chest. */
static void NvMmTakaraya_SpawnWalls(NvMmTakaraya* this, PlayState* play) {
    u8 gi = sPrizes[NvMmTakaraya_Form()][Flags_GetSwitch(play, this->formSwitchFlag) ? 1 : 0];

    Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, NvMmActor_Id(MM_TAKARAYA_WALL), 0.0f, 0.0f, 0.0f, 0, 0, 5,
                       (ENBOX_TYPE_SWITCH_FLAG_BIG_MM << 12) + (gi << 5) + this->actor.params);
}

static void NvMmTakaraya_StartGame(NvMmTakaraya* this, PlayState* play);
static void NvMmTakaraya_StartTimer(NvMmTakaraya* this, PlayState* play);

/** MM's EnTakaraya_Talk. */
static void NvMmTakaraya_Talk(NvMmTakaraya* this, PlayState* play) {
    u8 talkState;
    s32 form = NvMmTakaraya_Form();

    if (SkelAnime_Update(&this->skelAnime)) {
        if (this->skelAnime.animation == (AnimationHeader*)sAnimBow) {
            Animation_PlayOnce(&this->skelAnime, (AnimationHeader*)sAnimPoint);
        } else if (this->skelAnime.animation == (AnimationHeader*)sAnimStartTalk) {
            Animation_PlayLoop(&this->skelAnime, (AnimationHeader*)sAnimTalkLoop);
        } else {
            Animation_PlayLoop(&this->skelAnime, (AnimationHeader*)sAnimIdle);
        }
    }
    talkState = Message_GetState(&play->msgCtx);
    if ((talkState == TEXT_STATE_DONE) && (this->textId != 0x778) && (NvMmText_Next(this->textId) != 0xFFFF)) {
        // (MM's TEXT_STATE_NEXT: the message's header names the next one)
        if (Message_ShouldAdvance(play)) {
            Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sAnimBow, 5.0f);
            NvMmTakaraya_Continue(this, play, NvMmText_Next(this->textId));
        }
    } else if ((talkState == TEXT_STATE_DONE) && (this->textId == 0x778) && (NvMmText_Next(0x778) != 0xFFFF) &&
               (play->msgCtx.textId == NV_MM_TEXT(0x778))) {
        if (Message_ShouldAdvance(play)) { // (on to "Are you ready?", still the game's line)
            Message_ContinueTextbox(play, NV_MM_TEXT(NvMmText_Next(0x778)));
        }
    } else if ((talkState == TEXT_STATE_CLOSING) || (talkState == TEXT_STATE_DONE)) {
        if ((talkState == TEXT_STATE_DONE) && !Message_ShouldAdvance(play)) {
            return;
        }
        if (talkState == TEXT_STATE_DONE) {
            Message_CloseTextbox(play);
        }
        if (this->textId == 0x778) {
            NvMmTakaraya_StartGame(this, play);
        } else {
            NvMmTakaraya_SetupWait(this);
        }
    } else if ((talkState == TEXT_STATE_CHOICE) && Message_ShouldAdvance(play)) {
        if (play->msgCtx.choiceIndex == 0) {
            if (gSaveContext.rupees < sPrices[form]) {
                NvMmTakaraya_SetText(this, 0x77B);
                if (this->skelAnime.animation == (AnimationHeader*)sAnimIdle) {
                    Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sAnimPoint, 5.0f);
                }
                Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
            } else {
                Sfx_PlaySfxCentered(NA_SE_SY_DECIDE);
                Rupees_ChangeBy(-sPrices[form]);
                NvMmTakaraya_SpawnWalls(this, play);
                NvMmTakaraya_SetText(this, 0x778);
                if (this->skelAnime.animation != (AnimationHeader*)sAnimIdle) {
                    Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)sAnimIdle, 5.0f);
                }
            }
        } else {
            Sfx_PlaySfxCentered(NA_SE_SY_CANCEL);
            NvMmTakaraya_SetText(this, sDeclined[form]);
            if (this->skelAnime.animation == (AnimationHeader*)sAnimIdle) {
                Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sAnimPoint, 5.0f);
            }
        }
        Message_ContinueTextbox(play, this->actor.textId);
    }
}

static void NvMmTakaraya_Playing(NvMmTakaraya* this, PlayState* play);
static void NvMmTakaraya_Ending(NvMmTakaraya* this, PlayState* play);


static void NvMmTakaraya_Sweep(NvMmTakaraya* this, PlayState* play);

/** MM's func_80ADF2D4: the board's shown first (her cutscene, MM's queued by the chest as it appears). */
static void NvMmTakaraya_StartGame(NvMmTakaraya* this, PlayState* play) {
    Sfx_PlaySfxCentered(NA_SE_SY_CORRECT_CHIME);
    this->timer = 145;
    this->csStarted = false;
    this->func = NvMmTakaraya_Sweep;
}

/** MM's func_80ADF338: the camera sweeps from the chest back toward Link. */
static void NvMmTakaraya_Sweep(NvMmTakaraya* this, PlayState* play) {
    Actor* chest = play->actorCtx.actorLists[ACTORCAT_CHEST].head;
    Player* player = GET_PLAYER(play);
    s16 csId = NvMmCs_ActorCsId(&this->actor);

    SkelAnime_Update(&this->skelAnime);
    if (!this->csStarted && (csId != -1) && (chest != NULL)) { // (MM's EnBox starts it as it appears)
        if (!NvMmCsMgr_IsNext(csId)) {
            NvMmCsMgr_Queue(csId);
            return;
        }
        NvMmCsMgr_StartWithPlayerCs(csId, chest);
        this->csStarted = true;
    }
    if ((csId != -1) && (NvMmCsMgr_GetCurrentCsId() == csId) && (chest != NULL)) {
        Vec3f eye;
        Vec3f at;
        f32 dist;

        this->timer--;
        dist = (this->timer >= 26) ? 250.0f : ((chest->xzDistToPlayer - 250.0f) * (25 - this->timer) * 0.04f) + 250.0f;
        eye.x = chest->world.pos.x + (Math_SinS(chest->yawTowardsPlayer) * dist);
        eye.y = player->actor.world.pos.y + 120.0f;
        eye.z = chest->world.pos.z + (Math_CosS(chest->yawTowardsPlayer) * dist);
        at.x = eye.x - (Math_SinS(chest->yawTowardsPlayer) * 250.0f);
        at.y = eye.y - 90.0f;
        at.z = eye.z - (Math_CosS(chest->yawTowardsPlayer) * 250.0f);
        Play_CameraSetAtEye(play, NvMmCsMgr_GetCurrentSubCamId(csId), &at, &eye);
    } else if ((this->timer < 145) || (csId == -1)) {
        NvMmTakaraya_StartTimer(this, play);
    }
}

/** MM's func_80ADF4E0. */
static void NvMmTakaraya_StartTimer(NvMmTakaraya* this, PlayState* play) {
    Interface_SetTimer(45);
    sGameState = NV_TAKARAYA_PLAYING;
    this->timer = 1;
    this->func = NvMmTakaraya_Playing;
    if (getenv("NV_LOG") != NULL) {
        printf("NVMM treasure chest shop: the game starts (form %d)\n", (int)NvMmTakaraya_Form());
        fflush(stdout);
    }
}

/** MM's func_80ADF520: the chest opened, or the time's up. */
static void NvMmTakaraya_Playing(NvMmTakaraya* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (Player_InCsMode(play)) {
        return;
    }
    if (Flags_GetTreasure(play, this->actor.params)) {
        Flags_SetSwitch(play, this->formSwitchFlag);
        play->actorCtx.flags.chest &= ~(1 << this->actor.params);
        this->timer = 0;
        gSaveContext.timerState = TIMER_STATE_OFF;
        sGameState = NV_TAKARAYA_WON;
        Player_SetCsActionWithHaltedActors(play, &this->actor, 1); // (PLAYER_CSACTION_WAIT)
        this->endingGame = true;
        this->func = NvMmTakaraya_Ending;
    } else if ((gSaveContext.timerSeconds <= 0) &&
               ((gSaveContext.timerState == TIMER_STATE_STOP) || (gSaveContext.timerState == TIMER_STATE_OFF))) {
        this->timer = 50;
        Message_StartTextbox(play, NV_MM_TEXT(0x77D), &this->actor); // time's up
        gSaveContext.timerState = TIMER_STATE_OFF;
        sGameState = NV_TAKARAYA_LOST;
        Player_SetCsActionWithHaltedActors(play, &this->actor, 1);
        this->endingGame = true;
        this->func = NvMmTakaraya_Ending;
    }
}

/** MM's func_80ADF654: back to the counter. */
static void NvMmTakaraya_Ending(NvMmTakaraya* this, PlayState* play) {
    if (this->timer > 0) {
        this->timer--;
        return;
    }
    if (play->transitionTrigger != TRANS_TRIGGER_OFF) {
        return;
    }
    play->transitionType = (sGameState == NV_TAKARAYA_WON) ? TRANS_TYPE_FADE_WHITE : TRANS_TYPE_FADE_BLACK;
    play->nextEntranceIndex = ENTR_MM_Z2_TAKARAYA_1;
    play->transitionTrigger = TRANS_TRIGGER_START;
}

static void NvMmTakaraya_ReturnedTalk(NvMmTakaraya* this, PlayState* play);
static void NvMmTakaraya_Returned(NvMmTakaraya* this, PlayState* play);

/** MM's func_80ADF6DC: she speaks first. */
static void NvMmTakaraya_SetupReturned(NvMmTakaraya* this) {
    Animation_PlayLoop(&this->skelAnime, (AnimationHeader*)sAnimTalkLoop);
    this->eyeTexIndex = 0;
    this->actor.flags |= ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
    this->func = NvMmTakaraya_Returned;
}

/** MM's func_80ADF730. */
static void NvMmTakaraya_Returned(NvMmTakaraya* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        this->actor.flags &= ~ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
        this->func = NvMmTakaraya_ReturnedTalk;
    } else {
        this->formSwitchFlag = NvMmTakaraya_Form() + this->switchFlag;
        Actor_OfferTalk(&this->actor, play, 120.0f);
    }
}

/** MM's func_80ADF7CC: "Did you have fun?", "Won't you try again?", then the price. */
static void NvMmTakaraya_ReturnedTalk(NvMmTakaraya* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if ((Message_GetState(&play->msgCtx) == TEXT_STATE_EVENT) && Message_ShouldAdvance(play)) {
        if (this->textId == 0x77A) {
            NvMmTakaraya_Continue(this, play, 0x77C);
        } else {
            NvMmTakaraya_Continue(this, play, sPriceLines[NvMmTakaraya_Form()]);
            Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sAnimBow, 5.0f);
            this->func = NvMmTakaraya_Talk;
        }
    }
}

void NvMmTakaraya_Update(Actor* thisx, PlayState* play) {
    NvMmTakaraya* this = (NvMmTakaraya*)thisx;
    Vec3s torsoRot;

    this->func(this, play);
    Actor_TrackPlayer(play, &this->actor, &this->headRot, &torsoRot, this->actor.focus.pos);
    if (this->eyeTexIndex != 0) { // MM's EnTakaraya_Blink
        if (++this->eyeTexIndex == 4) {
            this->eyeTexIndex = 0;
        }
    } else if (Rand_ZeroOne() < 0.02f) {
        this->eyeTexIndex++;
    }
}

static s32 NvMmTakaraya_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                         void* thisx) {
    NvMmTakaraya* this = (NvMmTakaraya*)thisx;

    if (limbIndex == GAL_LIMB_HEAD) {
        rot->x += this->headRot.y;
        rot->y += 0x400 - this->headRot.x; // (MM's transform draw)
    }
    return false;
}

static void NvMmTakaraya_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    static Vec3f sFocusOffset = { 500.0f, 0.0f, 0.0f };
    NvMmTakaraya* this = (NvMmTakaraya*)thisx;

    if (limbIndex == GAL_LIMB_HEAD) {
        Matrix_MultVec3f(&sFocusOffset, &this->actor.focus.pos);
    }
}

void NvMmTakaraya_Draw(Actor* thisx, PlayState* play) {
    NvMmTakaraya* this = (NvMmTakaraya*)thisx;
    s32 form = NvMmTakaraya_Form();
    const char* eyes = ((form == 3) || (form == 4)) ? sEyesDown[this->eyeTexIndex] : sEyesUp[this->eyeTexIndex];

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(eyes));
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          NvMmTakaraya_OverrideLimbDraw, NvMmTakaraya_PostLimbDraw, &this->actor);
    CLOSE_DISPS(play->state.gfxCtx);
}

/* ---------------------------------------------------------------------------------------------- */
/* The board (Obj_Takaraya_Wall)                                                                  */

#define TKO(name) ((Gfx*)"__OTR__mm/objects/object_takaraya_objects/" name)
#define WALL_ROWS 11
#define WALL_COLUMNS 8

enum { WALL_INACTIVE, WALL_RISING, WALL_FALLING };
enum { DIR_BACK, DIR_RIGHT, DIR_FRONT, DIR_LEFT };

typedef struct {
    u8 row;
    u8 column;
} NvWallCell;

static s32 sPathDepth;
static NvWallCell sPath[WALL_ROWS * WALL_COLUMNS];
static s32 sPathReachedFront;
static u8 sWallStates[WALL_ROWS][WALL_COLUMNS];
static f32 sWallHeights[WALL_ROWS][WALL_COLUMNS]; // -10: the path (never rises)

typedef struct NvMmTakarayaWall {
    Actor actor;
    ColliderCylinder columnCollider;
    ColliderCylinder rowCollider;
} NvMmTakarayaWall;

size_t gNvMmTakarayaWallSize = sizeof(NvMmTakarayaWall);

static ColliderCylinderInit sWallCylinderInit = {
    { COLTYPE_NONE, AT_NONE, AC_NONE, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_2, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0, 0, 0 }, { 0, 0, 0 }, TOUCH_NONE | TOUCH_SFX_NORMAL, BUMP_NONE, OCELEM_ON },
    { 40, 120, 0, { 0, 0, 0 } },
};

static s32 NvWall_PosToCell(Vec3f* pos, s32* row, s32* column) {
    *row = ((s32)pos->x - -1680) / 120;
    *column = ((s32)pos->z - 0) / 120;
    return (*row >= 0) && (*row < WALL_ROWS) && (*column >= 0) && (*column < WALL_COLUMNS);
}

static s32 NvWall_IsCellOpen(s32 row, s32 column) {
    return !((row < 0) || (row >= WALL_ROWS) || (column < 0) || (column >= WALL_COLUMNS) ||
             (sWallHeights[row][column] == 0.0f));
}

/** MM's ObjTakarayaWall_CanCarvePath. */
static s32 NvWall_CanCarvePath(s32 row, s32 column, s32 direction) {
    s32 adjRow, adjColumn, row1, column1, row2, column2, row3, column3;

    if ((direction == DIR_FRONT) || (direction == DIR_BACK)) {
        adjRow = (direction == DIR_BACK) ? row - 1 : row + 1;
        row3 = (direction == DIR_BACK) ? row - 2 : row + 2;
        adjColumn = column3 = column;
        row1 = row2 = adjRow;
        column1 = column + 1;
        column2 = column - 1;
    } else {
        adjColumn = (direction == DIR_RIGHT) ? column - 1 : column + 1;
        column3 = (direction == DIR_RIGHT) ? column - 2 : column + 2;
        adjRow = row3 = row;
        row1 = row + 1;
        row2 = row - 1;
        column1 = column2 = adjColumn;
    }
    if ((adjRow < 0) || (adjRow >= WALL_ROWS) || (adjColumn < 0) || (adjColumn >= WALL_COLUMNS) ||
        (sWallHeights[adjRow][adjColumn] == -10.0f) || NvWall_IsCellOpen(row1, column1) ||
        NvWall_IsCellOpen(row2, column2) || NvWall_IsCellOpen(row3, column3)) {
        return false;
    }
    return true;
}

/** MM's ObjTakarayaWall_CarvePath. */
static void NvWall_CarvePath(s32 row, s32 column) {
    s32 flags = 0;
    s32 num = 0;
    s32 randMode;
    s32 direction;
    s32 d;

    for (d = DIR_BACK; d <= DIR_LEFT; d++) {
        if (NvWall_CanCarvePath(row, column, d)) {
            num++;
            flags |= 1 << d;
        }
    }
    if (num == 0) {
        sPathDepth--;
        return;
    }
    randMode = (s32)Rand_ZeroFloat(num) % num;
    if (randMode == 0) {
        direction = DIR_BACK;
        while (!((1 << direction) & flags)) {
            direction++;
        }
    } else if (randMode == 1) {
        direction = DIR_LEFT;
        while (!((1 << direction) & flags)) {
            direction--;
        }
    } else if (randMode == 2) {
        direction = DIR_BACK;
        while (!((1 << direction) & flags)) {
            direction++;
        }
        direction++;
        while (!((1 << direction) & flags)) {
            direction++;
        }
    } else {
        direction = DIR_LEFT;
        while (!((1 << direction) & flags)) {
            direction--;
        }
        direction--;
        while (!((1 << direction) & flags)) {
            direction--;
        }
    }
    if (direction == DIR_BACK) {
        row--;
    } else if (direction == DIR_FRONT) {
        row++;
    } else if (direction == DIR_RIGHT) {
        column--;
    } else {
        column++;
    }
    sWallHeights[row][column] = -10.0f;
    sPath[sPathDepth].row = row;
    sPath[sPathDepth].column = column;
    sPathDepth++;
    if (!sPathReachedFront && (row == (WALL_ROWS - 1))) {
        sPathReachedFront = true;
    }
}

void NvMmTakarayaWall_Init(Actor* thisx, PlayState* play) {
    NvMmTakarayaWall* this = (NvMmTakarayaWall*)thisx;
    Actor* chest;
    s32 column;
    s32 i;
    s32 j;

    Actor_SetScale(thisx, 0.1f);
    for (i = 0; i < WALL_ROWS; i++) {
        for (j = 0; j < WALL_COLUMNS; j++) {
            sWallHeights[i][j] = 0.0f;
            sWallStates[i][j] = WALL_INACTIVE;
        }
    }
    sPathDepth = 0;
    sPathReachedFront = false;
    Collider_InitCylinder(play, &this->columnCollider);
    Collider_SetCylinder(play, &this->columnCollider, thisx, &sWallCylinderInit);
    Collider_InitCylinder(play, &this->rowCollider);
    Collider_SetCylinder(play, &this->rowCollider, thisx, &sWallCylinderInit);
    thisx->colChkInfo.mass = MASS_IMMOVABLE;
    column = (s32)Rand_ZeroFloat(WALL_COLUMNS) % WALL_COLUMNS;
    if (getenv("NV_MM_CHEST_COLUMN") != NULL) { // (dev: the chest's column, for testing)
        column = CLAMP(atoi(getenv("NV_MM_CHEST_COLUMN")), 0, WALL_COLUMNS - 1);
    }
    // (MM's 0x4000, half round: OoT's En_Box faces and opens the other way from MM's, mm_actors.py)
    chest = Actor_SpawnAsChild(&play->actorCtx, thisx, play, ACTOR_EN_BOX, -1635.0f, 0.0f, (column * 120) + 60, 0,
                               0x4000 + 0x8000, thisx->shape.rot.z, thisx->params);
    Flags_SetSwitch(play, thisx->shape.rot.z);
    thisx->shape.rot.z = thisx->world.rot.z = 0;
    if (chest != NULL) {
        chest->uncullZoneForward = 2000.0f;
    }
    if (getenv("NV_LOG") != NULL) {
        printf("NVMM treasure chest shop: chest in column %d (%s), params %04X\n", (int)column,
               (chest != NULL) ? "spawned" : "NOT spawned", (u16)thisx->params);
        fflush(stdout);
    }
    sWallHeights[0][column] = -10.0f;
    sPath[sPathDepth].row = 0;
    sPath[sPathDepth].column = column;
    sPathDepth++;
    do {
        NvWall_CarvePath(sPath[sPathDepth - 1].row, sPath[sPathDepth - 1].column);
    } while (sPathDepth > 1);
    for (j = 1; j < WALL_COLUMNS - 1; j++) {
        if (sWallHeights[WALL_ROWS - 1][j] == -10.0f) {
            break;
        }
    }
    // (a front corner on the path with no other front opening: its neighbour opened, the scene blocking
    // the corners, as MM's)
    if (j == (WALL_COLUMNS - 1)) {
        if (sWallHeights[WALL_ROWS - 1][0] == -10.0f) {
            sWallHeights[WALL_ROWS - 1][1] = -10.0f;
        } else {
            sWallHeights[WALL_ROWS - 1][WALL_COLUMNS - 2] = -10.0f;
        }
    }
}

void NvMmTakarayaWall_Destroy(Actor* thisx, PlayState* play) {
    NvMmTakarayaWall* this = (NvMmTakarayaWall*)thisx;

    Collider_DestroyCylinder(play, &this->columnCollider);
    Collider_DestroyCylinder(play, &this->rowCollider);
}

/** MM's ObjTakarayaWall_Manage. */
void NvMmTakarayaWall_Update(Actor* thisx, PlayState* play) {
    NvMmTakarayaWall* this = (NvMmTakarayaWall*)thisx;
    Player* player = GET_PLAYER(play);
    ColliderCylinder* collider;
    s32 rowBehind = -1, rowInFront = -1, columnLeft = -1, columnRight = -1;
    s32 row, column, cellX, cellZ, i, j;
    s32 columnActive = false, rowActive = false, sounding = 0;

    NvWall_PosToCell(&player->actor.world.pos, &row, &column);
    cellX = (((s32)player->actor.world.pos.x - -1800) % 120) - 60;
    cellZ = (((s32)player->actor.world.pos.z - -120) % 120) - 60;
    if (cellX > 20) {
        rowBehind = row + 1;
    } else if (cellX < -20) {
        rowInFront = row - 1;
    }
    if (cellZ > 20) {
        columnLeft = column + 1;
    } else if (cellZ < -20) {
        columnRight = column - 1;
    }
    for (i = 0; i < WALL_ROWS; i++) {
        for (j = 0; j < WALL_COLUMNS; j++) {
            if (sWallHeights[i][j] < 0.0f) {
                continue;
            }
            if (((j == column) && ((i == rowInFront) || (i == rowBehind))) ||
                ((i == row) && ((j == columnRight) || (j == columnLeft)))) {
                sWallStates[i][j] = Math_StepToF(&sWallHeights[i][j], 120.0f, 15.0f) ? WALL_INACTIVE : WALL_RISING;
                if (j == column) {
                    columnActive = true;
                } else if (i == row) {
                    rowActive = true;
                }
            } else if ((i != row) || (j != column)) {
                Math_SmoothStepToF(&sWallHeights[i][j], 0.0f, 0.2f, 5.0f, 1.0f);
                sWallStates[i][j] = (sWallHeights[i][j] > 0.0f) ? WALL_FALLING : WALL_INACTIVE;
            }
            if (sWallStates[i][j] == WALL_RISING) {
                sounding = 1;
            }
        }
    }
    collider = &this->columnCollider;
    collider->dim.radius = 200;
    if (!columnActive) {
        collider->dim.pos.y = (s32)thisx->world.pos.y - 240;
    } else {
        collider->dim.pos.y = (s32)thisx->world.pos.y;
        collider->dim.pos.z = (s32)player->actor.world.pos.z;
        collider->dim.pos.x = ((((s32)player->actor.world.pos.x - -1680) / 120) * 120) + ((cellX > 0) ? -1380 : -1860);
    }
    collider = &this->rowCollider;
    collider->dim.radius = 200;
    if (!rowActive) {
        collider->dim.pos.y = (s32)thisx->world.pos.y - 240;
    } else {
        collider->dim.pos.y = (s32)thisx->world.pos.y;
        collider->dim.pos.x = (s32)player->actor.world.pos.x;
        collider->dim.pos.z = (((s32)player->actor.world.pos.z / 120) * 120) + ((cellZ > 0) ? 300 : -180);
    }
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->columnCollider.base);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->rowCollider.base);
    if (sounding) { // (MM's rock cubes' sounds aren't OoT's: its block's)
        Actor_PlaySfx_Flagged(thisx, NA_SE_EV_BLOCK_RISING - SFX_FLAG);
    }
}

void NvMmTakarayaWall_Draw(Actor* thisx, PlayState* play) {
    s32 i;
    s32 j;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, 255);
    for (i = 0; i < WALL_ROWS; i++) {
        for (j = 0; j < WALL_COLUMNS; j++) {
            if (sWallHeights[i][j] <= 0.0f) {
                continue;
            }
            FrameInterpolation_RecordOpenChild(thisx, i * WALL_COLUMNS + j);
            Matrix_Push();
            Matrix_Translate((i * 120) - 1620, sWallHeights[i][j] + (thisx->world.pos.y - 120.0f), (j * 120) + 60,
                             MTXMODE_NEW);
            Matrix_Scale(0.1f, 0.1f, 0.1f, MTXMODE_APPLY);
            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, (((i + j) % 2) != 0) ? TKO("gTreasureChestShopWallWhiteDL")
                                                                 : TKO("gTreasureChestShopWallBlackDL"));
            Matrix_Pop();
            FrameInterpolation_RecordCloseChild();
        }
    }
    CLOSE_DISPS(play->state.gfxCtx);
}
