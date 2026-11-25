/*
 * Nightmareverse: Guru-Guru, the music box man (MM's En_Guruguru, MM id 0x248), ported as it is
 * (mm/README.md "The townsfolk"): MM's model (object_fu: his skeleton, his two cranking animations, his
 * closed and angry eyes and mouths), his lines, his schedule by MM's params (0: the Laundry Pool by day;
 * 1: the inn at night, in his room; 2: the day's music box heard through the wall). By day he plays and
 * tells you about it; at night he tells his story, line by line, his music sped up with his telling, and
 * at its end gives the Bremen Mask (MM's get-item), once; Deku Link gets other lines. The Notebook's
 * events (met him, the mask) are set.
 * Adapted: his music box's Song of Storms (MM's sub-BGM by position) and its tempo change wait for MM's
 * audio; MM's angry face on the line it's for is kept.
 */
#include "global.h"
#include <stdio.h>
#include <stdlib.h>

#define FU(name) ("__OTR__mm/objects/object_fu/" name)
#define NV_MM_TEXT(id) (0xB000 + (id))
#define MM_WE(byte, bit) (((byte) << 8) | (bit))
s32 NvMm_CheckWeekEvent(s32 flag);
void NvMm_SetWeekEvent(s32 flag);
void NvMm_NotebookEvent(u16 event);
#define GURU_GURU_LIMB_HEAD 14
#define GURU_GURU_LIMB_MAX 16

#define WE_38_10 MM_WE(38, 0x10) // talked to him by day
#define WE_38_40 MM_WE(38, 0x40) // got the Bremen Mask (this cycle)
#define WE_77_04 MM_WE(77, 0x04) // (the music box's owner known: the Laundry Pool lines after)
#define WE_88_04 MM_WE(88, 0x04)
#define WE_79_04 MM_WE(79, 0x04) // Deku Link's talk heard
#define NOTEBOOK_MET_GURU_GURU 19 // (MM's notebook table: 0x13)
#define NOTEBOOK_RECEIVED_BREMEN_MASK 54 // (0x36)
#define MM_GI_MASK_BREMEN 0x8C
#define MM_ITEM_MASK_BREMEN 0x46

s32 NvMmClock_IsNight(void);
s32 NvMmForm_Get(void); // 0 FD, 1 Goron, 2 Zora, 3 Deku, 4 human
s32 NvMmItem_Has(s32 item);
GetItemEntry NvMmGetItem_Entry(s32 mmGi);

static u16 sTextIds[] = { 0x292A, 0x292B, 0x292C, 0x292D, 0x292E, 0x292F, 0x2930, 0x2931,
                          0x2932, 0x2933, 0x2934, 0x2935, 0x2936, 0x294D, 0x294E };
static const char* sAnims[] = { FU("gGuruGuruPlayStillAnim"), FU("gGuruGuruPlayAndMoveHeadAnim") };
static const char* sEyes[] = { FU("gGuruGuruEyeClosedTex"), FU("gGuruGuruEyeAngryTex") };
static const char* sMouths[] = { FU("gGuruGuruMouthOpenTex"), FU("gGuruGuruMouthAngryTex") };

struct NvMmGuruguru;
typedef void (*NvMmGuruguruFunc)(struct NvMmGuruguru*, PlayState*);

typedef struct NvMmGuruguru {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[GURU_GURU_LIMB_MAX];
    Vec3s morphTable[GURU_GURU_LIMB_MAX];
    NvMmGuruguruFunc actionFunc;
    s16 headZRot, headXRot, headZRotTarget, headXRotTarget;
    s16 headTimer;  // MM's unusedTimer (it paces the head's bob while talking)
    s16 justSpoke;  // MM's unk266
    s16 playing;    // MM's unk268: cranking (1) or still, as the telling goes
    u8 storyMode;   // MM's unk270: the night's story
    s16 textIdIndex;
    s16 texIndex;
    ColliderCylinder collider;
} NvMmGuruguru;

const size_t gNvMmGuruguruSize = sizeof(NvMmGuruguru);

static ColliderCylinderInit sCylinderInit = {
    { COLTYPE_NONE, AT_NONE, AC_NONE, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_2, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0xF7CFFFFF, 0x00, 0x00 }, TOUCH_NONE | TOUCH_SFX_NORMAL, BUMP_NONE,
      OCELEM_ON },
    { 15, 20, 0, { 0, 0, 0 } },
};

static void NvMmGuruguru_Idle(NvMmGuruguru* this, PlayState* play);
static void NvMmGuruguru_Talk(NvMmGuruguru* this, PlayState* play);
static void NvMmGuruguru_GiveMask(NvMmGuruguru* this, PlayState* play);
static void NvMmGuruguru_AfterMask(NvMmGuruguru* this, PlayState* play);
static void NvMmGuruguru_DoNothing(NvMmGuruguru* this, PlayState* play) {
}

static void NvMmGuruguru_ChangeAnim(NvMmGuruguru* this, s32 i) {
    AnimationHeader* a = (AnimationHeader*)sAnims[i];

    Animation_Change(&this->skelAnime, a, 1.0f, 0.0f, Animation_GetLastFrame(a), ANIMMODE_LOOP, -4.0f);
}

/** MM's func_80BC6E10: back to playing, his line chosen for the time and what's happened. */
static void NvMmGuruguru_SetupIdle(NvMmGuruguru* this) {
    NvMmGuruguru_ChangeAnim(this, 0);
    this->textIdIndex = 0;
    this->storyMode = 0;
    if (this->actor.params == 0) {
        if (NvMm_CheckWeekEvent(WE_38_10)) {
            this->textIdIndex = 1;
        }
    } else if (NvMm_CheckWeekEvent(WE_38_40)) {
        this->textIdIndex = 2;
    } else {
        this->textIdIndex = 3;
        this->storyMode = 1;
    }
    this->headZRotTarget = 0;
    this->playing = 1;
    this->actor.textId = NV_MM_TEXT(sTextIds[this->textIdIndex]);
    if ((this->textIdIndex <= 1) && NvMm_CheckWeekEvent(WE_77_04)) {
        this->actor.textId = NV_MM_TEXT(NvMm_CheckWeekEvent(WE_88_04) ? 0x2960 : 0x295F);
    }
    this->actionFunc = NvMmGuruguru_Idle;
}

void NvMmGuruguru_Init(Actor* thisx, PlayState* play) {
    NvMmGuruguru* this = (NvMmGuruguru*)thisx;
    s32 night = NvMmClock_IsNight();

    thisx->colChkInfo.mass = MASS_IMMOVABLE;
    ActorShape_Init(&thisx->shape, 0.0f, ActorShadow_DrawCircle, 19.0f);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)FU("gGuruGuruSkel"),
                       (AnimationHeader*)sAnims[0], this->jointTable, this->morphTable, GURU_GURU_LIMB_MAX);
    thisx->targetMode = 0;
    if (thisx->params != 2) {
        Collider_InitCylinder(play, &this->collider);
        Collider_SetCylinder(play, &this->collider, thisx, &sCylinderInit);
    }
    if (!night) {
        if (thisx->params == 0) {
            NvMmGuruguru_SetupIdle(this);
        } else if (thisx->params == 2) { // (only his music, through the wall)
            thisx->draw = NULL;
            thisx->flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
            this->actionFunc = NvMmGuruguru_DoNothing;
        } else {
            Actor_Kill(thisx);
        }
    } else if (thisx->params == 1) {
        NvMmGuruguru_SetupIdle(this);
    } else {
        Actor_Kill(thisx);
    }
}

void NvMmGuruguru_Destroy(Actor* thisx, PlayState* play) {
    if (thisx->params != 2) {
        Collider_DestroyCylinder(play, &((NvMmGuruguru*)thisx)->collider);
    }
}

static void NvMmGuruguru_Idle(NvMmGuruguru* this, PlayState* play) {
    s16 yaw;

    SkelAnime_Update(&this->skelAnime);
    if (this->storyMode) {
        this->textIdIndex = 3;
        if (NvMmForm_Get() == 3) { // Deku Link
            this->textIdIndex = NvMm_CheckWeekEvent(WE_79_04) ? 14 : 13;
        }
        this->actor.textId = NV_MM_TEXT(sTextIds[this->textIdIndex]);
    }
    yaw = ABS((s16)(this->actor.yawTowardsPlayer - this->actor.world.rot.y));
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        s32 form = NvMmForm_Get();

        if (this->playing && ((form == 4) || (form == 3))) {
            this->headZRotTarget = 5000;
        }
        this->justSpoke = 1;
        this->actionFunc = NvMmGuruguru_Talk;
    } else if (yaw <= 0x2890) {
        Actor_OfferTalk(&this->actor, play, 60.0f);
    }
}

/** MM's func_80BC7068: line after line; the story's lines change his cranking and his face. */
static void NvMmGuruguru_Talk(NvMmGuruguru* this, PlayState* play) {
    s32 form = NvMmForm_Get();

    if (this->playing) {
        SkelAnime_Update(&this->skelAnime);
    } else if (this->headTimer == 0) {
        this->headTimer = 6;
        if (Message_GetState(&play->msgCtx) != TEXT_STATE_EVENT) {
            if (this->justSpoke == 0) {
                this->headZRotTarget = (this->headZRotTarget != 0) ? 0 : 5000;
            }
        } else {
            this->headZRotTarget = ((form == 4) || (form == 3)) ? 5000 : 0;
        }
    }
    if ((Message_GetState(&play->msgCtx) != TEXT_STATE_EVENT) || !Message_ShouldAdvance(play)) {
        return;
    }
    Message_CloseTextbox(play);
    this->headZRotTarget = 0;
    if ((this->textIdIndex == 13) || (this->textIdIndex == 14)) {
        NvMm_NotebookEvent(NOTEBOOK_MET_GURU_GURU);
        NvMm_SetWeekEvent(WE_79_04);
        NvMmGuruguru_SetupIdle(this);
        return;
    }
    if (this->actor.params == 0) {
        if (this->actor.textId == NV_MM_TEXT(0x295F)) {
            NvMm_SetWeekEvent(WE_88_04);
        }
        if (this->actor.textId == NV_MM_TEXT(0x292A)) {
            NvMm_SetWeekEvent(WE_38_10);
        }
        NvMm_NotebookEvent(NOTEBOOK_MET_GURU_GURU);
        NvMmGuruguru_SetupIdle(this);
        return;
    }
    if (this->textIdIndex == 11) { // his story's end: the mask
        this->playing = 1;
        this->headZRotTarget = 0;
        this->actionFunc = NvMmGuruguru_GiveMask;
        return;
    }
    if (this->textIdIndex == 12) {
        NvMm_SetWeekEvent(WE_38_40);
        NvMm_NotebookEvent(NOTEBOOK_RECEIVED_BREMEN_MASK);
        NvMm_NotebookEvent(NOTEBOOK_MET_GURU_GURU);
        NvMmGuruguru_SetupIdle(this);
        return;
    }
    if (this->textIdIndex >= 3) {
        if ((this->textIdIndex == 10) && NvMmItem_Has(MM_ITEM_MASK_BREMEN)) {
            this->textIdIndex = 12; // (the mask's had already: his last line)
            this->playing = 0;
        } else {
            this->textIdIndex++;
            this->playing = (this->playing + 1) & 1;
            if (this->textIdIndex == 11) {
                this->playing = 0;
            }
        }
        this->texIndex = (this->textIdIndex == 7) ? 1 : 0; // (his angry face, as MM)
        this->skelAnime.playSpeed = (this->playing && (this->textIdIndex >= 7)) ? 2.0f : 1.0f;
        this->justSpoke = 1;
        Message_ContinueTextbox(play, NV_MM_TEXT(sTextIds[this->textIdIndex]));
        return;
    }
    NvMm_NotebookEvent(NOTEBOOK_MET_GURU_GURU);
    NvMmGuruguru_SetupIdle(this);
}

/** MM's func_80BC7440: the Bremen Mask handed over, then his last line. */
static void NvMmGuruguru_GiveMask(NvMmGuruguru* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (Actor_HasParent(&this->actor, play)) {
        this->actor.parent = NULL;
        this->textIdIndex++;
        this->actor.textId = NV_MM_TEXT(sTextIds[this->textIdIndex]);
        Actor_OfferTalk(&this->actor, play, 400.0f); // (MM's Actor_OfferTalkExchange: he speaks next)
        this->playing = 0;
        NvMm_SetWeekEvent(WE_38_40);
        this->actionFunc = NvMmGuruguru_AfterMask;
        if (getenv("NV_LOG") != NULL) {
            printf("NVMM Guru-Guru: the Bremen Mask given\n");
            fflush(stdout);
        }
    } else {
        GetItemEntry e = NvMmGetItem_Entry(MM_GI_MASK_BREMEN);

        GiveItemEntryFromActor(&this->actor, play, e, 300.0f, 300.0f);
    }
}

static void NvMmGuruguru_AfterMask(NvMmGuruguru* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        this->actionFunc = NvMmGuruguru_Talk;
    } else {
        Actor_OfferTalk(&this->actor, play, 400.0f);
    }
}

void NvMmGuruguru_Update(Actor* thisx, PlayState* play) {
    NvMmGuruguru* this = (NvMmGuruguru*)thisx;
    s32 night = NvMmClock_IsNight();
    s16 yaw;

    if ((!night && (thisx->params == 1)) || (night && (thisx->params != 1))) {
        Actor_Kill(thisx); // (his half of the day over)
        return;
    }
    this->actionFunc(this, play);
    if (thisx->params == 2) {
        return;
    }
    if (this->headTimer != 0) {
        this->headTimer--;
    }
    if (this->justSpoke != 0) {
        this->justSpoke--;
    }
    yaw = ABS((s16)(thisx->yawTowardsPlayer - thisx->world.rot.y));
    this->headXRotTarget = 0;
    if (yaw < 0x2AF8) {
        this->headXRotTarget = CLAMP((s16)(thisx->yawTowardsPlayer - thisx->world.rot.y), -0x1388, 0x1388);
    }
    Actor_SetScale(thisx, 0.01f);
    Actor_SetFocus(thisx, 50.0f);
    Actor_MoveXZGravity(thisx);
    Math_SmoothStepToS(&this->headXRot, this->headXRotTarget, 1, 0xBB8, 0);
    Math_SmoothStepToS(&this->headZRot, this->headZRotTarget, 1, 0x3E8, 0);
    Actor_UpdateBgCheckInfo(play, thisx, 20.0f, 20.0f, 50.0f, 0x1D);
    Collider_UpdateCylinder(thisx, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

static s32 NvMmGuruguru_OverrideLimb(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    NvMmGuruguru* this = (NvMmGuruguru*)thisx;

    if (limbIndex == GURU_GURU_LIMB_HEAD) {
        rot->x += this->headXRot;
        rot->z += this->headZRot;
    }
    return false;
}

void NvMmGuruguru_Draw(Actor* thisx, PlayState* play) {
    NvMmGuruguru* this = (NvMmGuruguru*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sEyes[this->texIndex]));
    gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(sMouths[this->texIndex]));
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          NvMmGuruguru_OverrideLimb, NULL, &this->actor);
    CLOSE_DISPS(play->state.gfxCtx);
}
