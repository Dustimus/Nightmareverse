/*
 * Nightmareverse: the Lottery Shop (MM's En_Kujiya, MM id 0x239), ported closely (mm/README.md "The
 * Lottery Shop"): MM's turning booth (object_kujiya, its scrolling rainbow MM's animated material),
 * MM's lines.
 * By day (6:00-18:00) a ticket is 10 rupees: three numbers in MM's lottery box (z_nv_mm_msginput.c).
 * One ticket a day (week events 33_10, 33_20, 33_40); the booth turns closed. From 18:00 it turns
 * open again and announces the day's winning numbers (MM's, made with a new file); a match wins
 * MM's 50 (a purple rupee).
 * The booth turning is shown by its actor cutscene (z_nv_mm_csmgr.c), with MM's windmill-level sound.
 */
#include "global.h"
#include "z_nv_mm_csmgr.h"
#include <stdio.h>
#include <stdlib.h>

#define KUJIYA(name) ("__OTR__mm/objects/object_kujiya/" name)
#define NV_MM_TEXT(id) (0xB000 + (id))
#define MM_WE(byte, bit) (((byte) << 8) | (bit))
#define MM_GI_RUPEE_PURPLE 0x05

s32 NvMm_CheckWeekEvent(s32 flag);
void NvMm_SetWeekEvent(s32 flag);
void NvMm_ClearWeekEvent(s32 flag);
s32 NvMmClock_Day(void);
s32 NvMmClock_IsNight(void);
s32 NvMmMsg_LotteryWon(void);
void NvMmMsg_SaveLotteryGuess(void);
u16* NvMmMsg_LotteryGuess(void);
GetItemEntry NvMmGetItem_Entry(s32 mmGi);
void NvMmMatAnim_Draw(PlayState* play, const char* path);

struct NvMmKujiya;
typedef void (*NvMmKujiyaFunc)(struct NvMmKujiya*, PlayState*);

typedef struct NvMmKujiya {
    Actor actor;
    NvMmKujiyaFunc func;
    s16 timer;
    u16 textId;
} NvMmKujiya;

const size_t gNvMmKujiyaSize = sizeof(NvMmKujiya);

static void NvMmKujiya_Wait(NvMmKujiya* this, PlayState* play);
static void NvMmKujiya_Talk(NvMmKujiya* this, PlayState* play);
static void NvMmKujiya_Turn(NvMmKujiya* this, PlayState* play);

static s32 NvMmKujiya_DayFlag(void) {
    s32 day = CLAMP(NvMmClock_Day(), 1, 3);

    return MM_WE(33, 0x10 << (day - 1)); // (33_10, 33_20, 33_40)
}

static s32 NvMmKujiya_Bought(void) {
    return NvMm_CheckWeekEvent(NvMmKujiya_DayFlag());
}

static void NvMmKujiya_Say(NvMmKujiya* this, PlayState* play, u16 id) {
    Message_StartTextbox(play, NV_MM_TEXT(id), &this->actor);
    this->textId = id;
    if (getenv("NV_LOG") != NULL) {
        printf("NVMM lottery says %04X f=%d\n", id, play->gameplayFrames);
        fflush(stdout);
    }
}

void NvMmKujiya_Init(Actor* thisx, PlayState* play) {
    NvMmKujiya* this = (NvMmKujiya*)thisx;

    Actor_SetScale(thisx, 0.1f);
    thisx->flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    thisx->targetMode = 6;
    thisx->focus.pos = thisx->world.pos;
    thisx->focus.pos.y += 30.0f;
    thisx->shape.rot.y = (NvMmKujiya_Bought() && !NvMmClock_IsNight()) ? 0 : 0x7555; // (closed, or open)
    this->func = NvMmKujiya_Wait;
}

void NvMmKujiya_Destroy(Actor* thisx, PlayState* play) {
}

/** MM's EnKujiya_Wait. */
static void NvMmKujiya_Wait(NvMmKujiya* this, PlayState* play) {
    u16 id;

    if (!NvMmClock_IsNight()) {
        id = NvMmKujiya_Bought() ? 0x2B61 : 0x2B5C; // come back tomorrow; pick 3 numbers
    } else {
        id = NvMmKujiya_Bought() ? 0x2B64 : 0x2B63; // the winning numbers; exchanging
    }
    this->actor.textId = NV_MM_TEXT(id); // (OoT's talk opens this itself)
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        this->textId = id;
        this->func = NvMmKujiya_Talk;
    } else if (NvMmClock_IsNight() && NvMmKujiya_Bought() && (this->actor.shape.rot.y == 0)) {
        this->timer = 0;
        this->func = NvMmKujiya_Turn; // (open for the announcement)
    } else if (this->actor.xzDistToPlayer < 100.0f) {
        Actor_OfferTalk(&this->actor, play, 100.0f);
    }
}

static void NvMmKujiya_GivePrize(NvMmKujiya* this, PlayState* play) {
    if (Actor_HasParent(&this->actor, play)) {
        this->actor.parent = NULL;
        this->func = NvMmKujiya_Wait;
    } else {
        GiveItemEntryFromActor(&this->actor, play, NvMmGetItem_Entry(MM_GI_RUPEE_PURPLE), 500.0f, 100.0f);
    }
}

/** MM's EnKujiya_Talk. */
static void NvMmKujiya_Talk(NvMmKujiya* this, PlayState* play) {
    switch (Message_GetState(&play->msgCtx)) {
        case TEXT_STATE_CHOICE: // buy?
            if (Message_ShouldAdvance(play)) {
                if (play->msgCtx.choiceIndex == 0) {
                    if (gSaveContext.rupees < 10) {
                        Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
                        NvMmKujiya_Say(this, play, 0x2B62);
                    } else {
                        Sfx_PlaySfxCentered(NA_SE_SY_DECIDE);
                        Rupees_ChangeBy(-10);
                        NvMmKujiya_Say(this, play, 0x2B5F); // (the numbers: MM's lottery box)
                    }
                } else {
                    Sfx_PlaySfxCentered(NA_SE_SY_CANCEL);
                    NvMmKujiya_Say(this, play, 0x2B5E);
                }
            }
            break;
        case TEXT_STATE_EVENT:
            if ((this->textId == 0x2B65) && (this->timer != 0)) { // (the suspense)
                this->timer--;
                break;
            }
            if (!Message_ShouldAdvance(play)) {
                break;
            }
            switch (this->textId) {
                case 0x2B5C:
                    NvMmKujiya_Say(this, play, 0x2B5D);
                    break;
                case 0x2B5F: // (MM's TEXT_STATE_INPUT_LOTTERY_CODE: its box waits as an event here)
                    NvMmMsg_SaveLotteryGuess();
                    if (getenv("NV_LOG") != NULL) {
                        printf("NVMM lottery guess %03X\n", *NvMmMsg_LotteryGuess());
                        fflush(stdout);
                    }
                    NvMmKujiya_Say(this, play, 0x2B60);
                    break;
                case 0x2B60:
                    NvMm_SetWeekEvent(NvMmKujiya_DayFlag());
                    Message_CloseTextbox(play);
                    this->timer = 0;
                    this->func = NvMmKujiya_Turn; // (closed till the evening)
                    break;
                case 0x2B64:
                    NvMm_ClearWeekEvent(NvMmKujiya_DayFlag());
                    this->timer = 20;
                    NvMmKujiya_Say(this, play, 0x2B65); // your numbers, the winning numbers
                    break;
                case 0x2B65:
                    NvMmKujiya_Say(this, play, NvMmMsg_LotteryWon() ? 0x2B66 : 0x2B67);
                    break;
                case 0x2B66:
                    Message_CloseTextbox(play);
                    this->func = NvMmKujiya_GivePrize;
                    NvMmKujiya_GivePrize(this, play);
                    break;
                default:
                    Message_CloseTextbox(play);
                    this->func = NvMmKujiya_Wait;
                    break;
            }
            break;
        case TEXT_STATE_DONE:
            if (Message_ShouldAdvance(play)) {
                this->func = NvMmKujiya_Wait;
            }
            break;
        case TEXT_STATE_CLOSING:
            this->func = NvMmKujiya_Wait;
            break;
        default:
            break;
    }
}

/** MM's EnKujiya_TurnToOpen and TurnToClosed. */
static void NvMmKujiya_Turn(NvMmKujiya* this, PlayState* play) {
    s16 target = NvMmKujiya_Bought() ? 0 : 0x7555;
    s16 csId = NvMmCs_ActorCsId(&this->actor);

    if (csId != NV_MM_CS_ID_NONE) { // (its shot of the turning booth: z_nv_mm_csmgr.c)
        if (NvMmCsMgr_IsNext(csId)) {
            NvMmCsMgr_StartWithPlayerCs(csId, &this->actor);
        } else {
            NvMmCsMgr_Queue(csId);
        }
    }
    if (!Math_SmoothStepToS(&this->actor.shape.rot.y, target, 0xA, 0x16C, 0x16C)) {
        if (this->timer > 20) {
            if (NvMmCsMgr_GetCurrentCsId() == csId) {
                NvMmCsMgr_Stop(csId);
            }
            this->func = NvMmKujiya_Wait;
        } else {
            this->timer++;
        }
    } else {
        Actor_PlaySfx_Flagged(&this->actor, NA_SE_EV_WINDMILL_LEVEL - SFX_FLAG);
    }
}

void NvMmKujiya_Update(Actor* thisx, PlayState* play) {
    ((NvMmKujiya*)thisx)->func((NvMmKujiya*)thisx, play);
}

void NvMmKujiya_Draw(Actor* thisx, PlayState* play) {
    static const char* sDLs[] = { KUJIYA("gLotteryShopCylinderDL"), KUJIYA("gLotteryShopBackSignDL"),
                                  KUJIYA("gLotteryShopOpenBoxDL"), KUJIYA("gLotteryShopScrollingEffectsDL"),
                                  KUJIYA("gLotteryShopRedRupeesDL"), KUJIYA("gLotteryShopGreenRupeesDL"),
                                  KUJIYA("gLotteryShopBlueRupeesDL"), KUJIYA("gLotteryShopTargetArrowAndSignDL"),
                                  KUJIYA("gLotteryShopScrollingArrowDL"), KUJIYA("gLotteryShopMaskDL") };
    s32 i;

    NvMmMatAnim_Draw(play, KUJIYA("gLotteryShopTexAnim"));
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    for (i = 0; i < ARRAY_COUNT(sDLs); i++) {
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sDLs[i]);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}
