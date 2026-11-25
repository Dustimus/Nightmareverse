/*
 * Nightmareverse: West Clock Town's bank teller (MM's En_Ginko_Man, MM id 0x177), ported closely
 * (mm/README.md "The bank"): MM's model (object_boj: the jugglers' troupe's body, his green colour
 * through segments 8 and 9, his own head), MM's lines, MM's bank.
 *
 * The bank: MM's (HS_GET_BANK_RUPEES): rupees deposited stay through the Song of Time; up to 5000.
 * Amounts are chosen in MM's bank box (z_nv_mm_msginput.c: three digits). The first deposit stamps
 * Link's hand, MM's account. Rewards, MM's: 200 in the bank, the next wallet (MM's Adult's Wallet,
 * OoT's wallets shared; with the Giant's already, a blue rupee); 1000, a blue rupee; 5000, a piece
 * of heart (then blue rupees). At night a withdrawal costs MM's 4 rupee fee. Withdrawals fit the
 * wallet (OoT's capacity).
 * In Nightmareverse rupees aren't lost to the Song of Time (mm/README.md), so the bank keeps them
 * the same as a wallet would; it's here for MM's rewards and as MM has it.
 * Adapted: MM's sound for his hand on his knee and the stamp are left out (not in OoT's banks).
 */
#include "global.h"
#include <stdio.h>
#include <stdlib.h>

#define BOJ(name) ("__OTR__mm/objects/object_boj/" name)
#define NV_MM_TEXT(id) (0xB000 + (id))
#define MM_WE(byte, bit) (((byte) << 8) | (bit))
s32 NvMm_CheckWeekEvent(s32 flag);
void NvMm_SetWeekEvent(s32 flag);
s32 NvMmClock_Day(void);
s32 NvMmClock_IsNight(void);
GetItemEntry NvMmGetItem_Entry(s32 mmGi);
s32 NvMmMsg_RupeesSelected(void);
void NvMmMsg_SetRupeesTotal(s32 n);

#define WE_RECEIVED_BANK_WALLET_UPGRADE MM_WE(10, 0x08)
#define WE_RECEIVED_BANK_HEART_PIECE MM_WE(59, 0x08)
#define WE_59_40 MM_WE(59, 0x40) // told of the 200 reward
#define WE_59_80 MM_WE(59, 0x80) // the 1000
#define WE_60_01 MM_WE(60, 0x01) // the 5000
#define MM_GI_RUPEE_BLUE 0x02
#define MM_GI_WALLET_ADULT 0x08
#define MM_GI_HEART_PIECE 0x0C
#define OBJECT_BOJ_LIMB_08 8
#define OBJECT_BOJ_LIMB_0F 15
#define OBJECT_BOJ_LIMB_MAX 17
#define NIGHT_FEE 4 // (MM's message 0x477's price)

enum { CHOICE_YES = 0, CHOICE_NO = 1 };
enum { CHOICE_DEPOSIT = 0, CHOICE_WITHDRAW = 1, CHOICE_CANCEL = 2, CHOICE_RESET = 3 };
enum { ANIM_LEGSMACKING, ANIM_SITTING, ANIM_REACHING, ANIM_AMAZED, ANIM_ADVERTISING, ANIM_MAX };

static const char* sAnims[ANIM_MAX] = { BOJ("object_boj_Anim_0008C0"), BOJ("object_boj_Anim_0043F0"),
                                         BOJ("object_boj_Anim_004F40"), BOJ("object_boj_Anim_000AC4"),
                                         BOJ("object_boj_Anim_004A7C") };
static const u8 sAnimModes[ANIM_MAX] = { ANIMMODE_LOOP, ANIMMODE_LOOP, ANIMMODE_ONCE, ANIMMODE_LOOP, ANIMMODE_LOOP };

/* ---- MM's bank (its high score slot): saved with the file, through the cycles ---- */
static s32 sBankRupees;

s32* NvMmBank_Rupees(void) {
    return &sBankRupees;
}

struct NvMmGinko;
typedef void (*NvMmGinkoFunc)(struct NvMmGinko*, PlayState*);

typedef struct NvMmGinko {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[OBJECT_BOJ_LIMB_MAX];
    Vec3s morphTable[OBJECT_BOJ_LIMB_MAX];
    NvMmGinkoFunc func;
    Vec3s headRot;
    Vec3s torsoRot;
    s16 curTextId;
    s16 animTimer;
    s16 serviceFee;
    s16 previousBankValue;
    s32 choice;
    s32 isNewAccount;
    s32 isStampChecked;
    s32 animIndex;
} NvMmGinko;

const size_t gNvMmGinkoSize = sizeof(NvMmGinko);

static void NvMmGinko_Idle(NvMmGinko* this, PlayState* play);
static void NvMmGinko_Dialogue(NvMmGinko* this, PlayState* play);
static void NvMmGinko_BankAward(NvMmGinko* this, PlayState* play);
static void NvMmGinko_BankAward2(NvMmGinko* this, PlayState* play);
static void NvMmGinko_Stamp(NvMmGinko* this, PlayState* play);

static void NvMmGinko_Anim(NvMmGinko* this, s32 i) {
    AnimationHeader* a = (AnimationHeader*)sAnims[i];

    this->animIndex = i;
    Animation_Change(&this->skelAnime, a, 1.0f, 0.0f, Animation_GetLastFrame(a), sAnimModes[i], -4.0f);
}

static void NvMmGinko_Say(NvMmGinko* this, PlayState* play, u16 id) {
    Message_StartTextbox(play, NV_MM_TEXT(id), &this->actor);
    this->curTextId = id;
}

static s32 NvMmGinko_LastNight(void) {
    return (NvMmClock_Day() == 3) && NvMmClock_IsNight();
}

static void NvMmGinko_SetupIdle(NvMmGinko* this) {
    this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    NvMmGinko_Anim(this, ANIM_SITTING);
    this->func = NvMmGinko_Idle;
}

void NvMmGinko_Init(Actor* thisx, PlayState* play) {
    NvMmGinko* this = (NvMmGinko*)thisx;

    thisx->targetMode = 1;
    thisx->uncullZoneForward = 400.0f;
    Actor_SetScale(thisx, 0.01f);
    thisx->colChkInfo.cylRadius = 100;
    this->choice = CHOICE_RESET;
    if ((getenv("NV_MM_BANK") != NULL) && (sBankRupees == 0)) { // (dev: a balance already banked)
        sBankRupees = atoi(getenv("NV_MM_BANK"));
    }
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)BOJ("object_boj_Skel_00C240"),
                       (AnimationHeader*)sAnims[ANIM_SITTING], this->jointTable, this->morphTable,
                       OBJECT_BOJ_LIMB_MAX);
    NvMmGinko_SetupIdle(this);
}

void NvMmGinko_Destroy(Actor* thisx, PlayState* play) {
}

/** MM's EnGinkoMan_SwitchAnimation: looking for customers, amazed when Link comes near. */
static void NvMmGinko_SwitchAnimation(NvMmGinko* this) {
    if (this->actor.xzDistToPlayer > 160.0f) {
        if ((this->animTimer == 0) && (this->animIndex != ANIM_ADVERTISING)) {
            this->animTimer = 40;
            NvMmGinko_Anim(this, ANIM_ADVERTISING);
        }
    } else if ((this->animTimer == 0) && (this->animIndex != ANIM_AMAZED)) {
        this->animTimer = 40;
        NvMmGinko_Anim(this, ANIM_AMAZED);
    }
    DECR(this->animTimer);
}

static void NvMmGinko_Idle(NvMmGinko* this, PlayState* play) {
    s16 yaw = this->actor.yawTowardsPlayer - this->actor.shape.rot.y;

    u16 id = (sBankRupees == 0) ? 0x44C : NvMmGinko_LastNight() ? 0x467 : 0x466; // (0x44C: an account?)

    NvMmGinko_SwitchAnimation(this);
    if (Actor_ProcessTalkRequest(&this->actor, play)) { // (OoT's talk opens the actor's text: his line)
        NvMmGinko_Anim(this, (id == 0x44C) ? ANIM_LEGSMACKING : ANIM_SITTING);
        this->curTextId = id;
        this->func = NvMmGinko_Dialogue;
    } else if (ABS(yaw) < 0x1555) {
        this->actor.textId = NV_MM_TEXT(id);
        Actor_OfferTalk(&this->actor, play, 100.0f);
    }
}

/** MM's EnGinkoMan_DepositDialogue: an event box gone on. */
static void NvMmGinko_EventDialogue(NvMmGinko* this, PlayState* play) {
    switch (this->curTextId) {
        case 0x44C:
            NvMmGinko_Anim(this, ANIM_SITTING);
            NvMmGinko_Say(this, play, NvMm_CheckWeekEvent(WE_RECEIVED_BANK_WALLET_UPGRADE) ? 0x44E : 0x44D);
            break;
        case 0x44D:
        case 0x456:
        case 0x459:
            NvMmGinko_Say(this, play, 0x44E);
            break;
        case 0x44F:
            NvMmGinko_Anim(this, ANIM_LEGSMACKING);
            NvMmGinko_Say(this, play, 0x450);
            break;
        case 0x453: // deposited a little
        case 0x454: // a fair amount
        case 0x455: // a lot
            if (this->isNewAccount) {
                this->isNewAccount = false;
                if (this->curTextId != 0x453) {
                    NvMmGinko_Anim(this, ANIM_SITTING);
                }
                NvMmGinko_Say(this, play, 0x461);
            } else {
                if (this->curTextId == 0x453) {
                    NvMmGinko_Anim(this, ANIM_LEGSMACKING);
                }
                NvMmMsg_SetRupeesTotal(sBankRupees);
                NvMmGinko_Say(this, play, 0x45A);
            }
            break;
        case 0x45A: // the balance: a reward reached?
            if ((sBankRupees >= 200) && (this->previousBankValue < 200) && !NvMm_CheckWeekEvent(WE_59_40)) {
                NvMm_SetWeekEvent(WE_59_40);
                NvMmGinko_Say(this, play, 0x45B);
            } else if ((sBankRupees >= 1000) && (this->previousBankValue < 1000) && !NvMm_CheckWeekEvent(WE_59_80)) {
                NvMm_SetWeekEvent(WE_59_80);
                NvMmGinko_Say(this, play, 0x45C);
            } else if (sBankRupees >= 5000) {
                if ((this->previousBankValue < 5000) && !NvMm_CheckWeekEvent(WE_60_01)) {
                    NvMm_SetWeekEvent(WE_60_01);
                    NvMmGinko_Say(this, play, 0x45D);
                } else if (this->previousBankValue < sBankRupees) {
                    NvMmGinko_Anim(this, ANIM_SITTING);
                    NvMmGinko_Say(this, play, 0x45E);
                } else {
                    NvMmGinko_Anim(this, ANIM_LEGSMACKING);
                    NvMmGinko_Say(this, play, 0x460);
                }
            } else {
                NvMmGinko_Anim(this, ANIM_LEGSMACKING);
                NvMmGinko_Say(this, play, 0x460);
            }
            break;
        case 0x45B: // the 200 reward
        case 0x45C: // the 1000
        case 0x45D: // the 5000
            this->isStampChecked = false;
            Message_CloseTextbox(play);
            this->func = NvMmGinko_BankAward;
            NvMmGinko_BankAward(this, play);
            break;
        case 0x461:
            NvMmGinko_Say(this, play, 0x462);
            break;
        case 0x462:
            NvMmGinko_Say(this, play, 0x463);
            break;
        case 0x463:
            NvMmGinko_Say(this, play, 0x464);
            break;
        case 0x464:
            play->msgCtx.msgMode = MSGMODE_PAUSED;
            NvMmGinko_Anim(this, ANIM_REACHING); // the stamp
            this->func = NvMmGinko_Stamp;
            break;
        case 0x465:
            NvMmGinko_Anim(this, ANIM_LEGSMACKING);
            NvMmMsg_SetRupeesTotal(sBankRupees);
            NvMmGinko_Say(this, play, 0x45A);
            break;
        case 0x466:
        case 0x467:
            NvMmGinko_Say(this, play, 0x468);
            break;
        case 0x469:
            NvMmGinko_Anim(this, ANIM_REACHING); // checking the stamp
            this->func = NvMmGinko_Stamp;
            break;
        case 0x46A:
        case 0x46C:
        case 0x47E:
            if (this->choice == CHOICE_DEPOSIT) {
                if (sBankRupees >= 5000) {
                    NvMmGinko_Say(this, play, 0x45F);
                } else if (gSaveContext.rupees == 0) {
                    NvMmGinko_Say(this, play, 0x458);
                } else {
                    NvMmGinko_Say(this, play, 0x479);
                }
            } else if (NvMmGinko_LastNight()) {
                NvMmGinko_Say(this, play, 0x46D);
            } else {
                NvMmGinko_Say(this, play, 0x46B);
            }
            this->choice = CHOICE_RESET;
    if ((getenv("NV_MM_BANK") != NULL) && (sBankRupees == 0)) { // (dev: a balance already banked)
        sBankRupees = atoi(getenv("NV_MM_BANK"));
    }
            break;
        case 0x46B:
            NvMmGinko_Say(this, play, 0x46E);
            break;
        case 0x46D:
            NvMmGinko_Say(this, play, 0x46B);
            break;
        case 0x476:
            NvMmGinko_Anim(this, ANIM_SITTING);
            // fallthrough
        case 0x475:
        case 0x47C:
        case 0x47D:
            NvMmGinko_Say(this, play, 0x468);
            break;
        case 0x472:
        case 0x473:
        case 0x474:
            if (sBankRupees == 0) {
                NvMmGinko_Say(this, play, 0x478);
            } else {
                NvMmMsg_SetRupeesTotal(sBankRupees);
                NvMmGinko_Say(this, play, 0x45A);
            }
            break;
        case 0x477:
            NvMmGinko_Say(this, play, 0x471);
            this->serviceFee = NIGHT_FEE;
            break;
        case 0x479:
            NvMmGinko_Say(this, play, 0x44F);
            break;
        default: // (the rest end the talk)
            Message_CloseTextbox(play);
            this->isStampChecked = false;
            NvMmGinko_SetupIdle(this);
            break;
    }
}

/** MM's EnGinkoMan_WaitForRupeeCount: an amount chosen in MM's bank box. */
static void NvMmGinko_AmountDialogue(NvMmGinko* this, PlayState* play) {
    s32 amount = NvMmMsg_RupeesSelected();

    if (this->curTextId == 0x450) {
        if (amount == 0) {
            NvMmGinko_Anim(this, ANIM_SITTING);
            NvMmGinko_Say(this, play, 0x457);
        } else {
            NvMmGinko_Say(this, play, 0x452);
        }
    } else { // 0x46E
        if (amount == 0) {
            NvMmGinko_Say(this, play, 0x46F);
        } else if (NvMmClock_IsNight()) {
            NvMmGinko_Say(this, play, 0x477);
        } else {
            NvMmGinko_Say(this, play, 0x471);
            this->serviceFee = 0;
        }
    }
}

/** MM's EnGinkoMan_WaitForDialogueInput: a choice made. */
static void NvMmGinko_ChoiceDialogue(NvMmGinko* this, PlayState* play) {
    u8 choice = play->msgCtx.choiceIndex;
    s32 amount = NvMmMsg_RupeesSelected();

    switch (this->curTextId) {
        case 0x44E: // deposit?
            if (choice == CHOICE_YES) {
                if (sBankRupees >= 5000) {
                    Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
                    NvMmGinko_Say(this, play, 0x45F); // full
                } else if (gSaveContext.rupees > 0) {
                    Sfx_PlaySfxCentered(NA_SE_SY_DECIDE);
                    NvMmGinko_Say(this, play, 0x44F);
                } else {
                    Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
                    NvMmGinko_Say(this, play, 0x458); // not a single rupee
                }
            } else {
                Sfx_PlaySfxCentered(NA_SE_SY_CANCEL);
                NvMmGinko_Say(this, play, 0x451);
            }
            break;
        case 0x452: // really deposit this much?
            if (choice == CHOICE_YES) {
                if (gSaveContext.rupees < amount) {
                    Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
                    NvMmGinko_Anim(this, ANIM_SITTING);
                    NvMmGinko_Say(this, play, 0x459);
                } else {
                    Sfx_PlaySfxCentered(NA_SE_SY_DECIDE);
                    if (amount >= 100) {
                        NvMmGinko_Say(this, play, 0x455);
                    } else if (amount >= 10) {
                        NvMmGinko_Say(this, play, 0x454);
                    } else {
                        NvMmGinko_Anim(this, ANIM_SITTING);
                        NvMmGinko_Say(this, play, 0x453);
                    }
                    if (sBankRupees == 0) {
                        this->isNewAccount = true;
                    }
                    Rupees_ChangeBy(-amount);
                    this->previousBankValue = sBankRupees;
                    sBankRupees += amount;
                }
            } else {
                Sfx_PlaySfxCentered(NA_SE_SY_CANCEL);
                NvMmGinko_Anim(this, ANIM_SITTING);
                NvMmGinko_Say(this, play, (sBankRupees == 0) ? 0x456 : 0x47D);
            }
            break;
        case 0x468: // deposit, withdraw or not
            if (choice == CHOICE_CANCEL) {
                Sfx_PlaySfxCentered(NA_SE_SY_CANCEL);
                NvMmGinko_Say(this, play, 0x470);
            } else {
                Sfx_PlaySfxCentered(NA_SE_SY_DECIDE);
                this->choice = choice;
                if (!this->isStampChecked) {
                    this->isStampChecked = true;
                    NvMmGinko_Say(this, play, 0x469);
                } else {
                    NvMmGinko_Say(this, play, 0x47E);
                }
            }
            break;
        case 0x471: // really withdraw?
            if (choice == CHOICE_YES) {
                if (sBankRupees < (amount + this->serviceFee)) {
                    Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
                    NvMmGinko_Anim(this, ANIM_LEGSMACKING);
                    NvMmGinko_Say(this, play, 0x476);
                } else if (CUR_CAPACITY(UPG_WALLET) < (amount + gSaveContext.rupees)) {
                    Sfx_PlaySfxCentered(NA_SE_SY_ERROR); // (the wallet can't hold it)
                    NvMmGinko_Say(this, play, 0x475);
                } else {
                    Sfx_PlaySfxCentered(NA_SE_SY_DECIDE);
                    NvMmGinko_Say(this, play, (amount >= 100) ? 0x474 : (amount >= 10) ? 0x473 : 0x472);
                    this->previousBankValue = sBankRupees;
                    sBankRupees -= amount + this->serviceFee;
                    Rupees_ChangeBy(amount);
                }
            } else {
                Sfx_PlaySfxCentered(NA_SE_SY_CANCEL);
                NvMmGinko_Say(this, play, 0x47C);
            }
            break;
        default:
            break;
    }
}

/** MM's EnGinkoMan_Dialogue. */
static void NvMmGinko_Dialogue(NvMmGinko* this, PlayState* play) {
    NvMmGinko_SwitchAnimation(this);
    switch (Message_GetState(&play->msgCtx)) {
        case TEXT_STATE_CLOSING:
            NvMmGinko_SetupIdle(this);
            break;
        case TEXT_STATE_CHOICE:
            if (Message_ShouldAdvance(play)) {
                NvMmGinko_ChoiceDialogue(this, play);
            }
            break;
        case TEXT_STATE_EVENT: // (MM's TEXT_STATE_INPUT_RUPEES too: its boxes wait as an event here)
            if (Message_ShouldAdvance(play)) {
                if ((this->curTextId == 0x450) || (this->curTextId == 0x46E)) {
                    NvMmGinko_AmountDialogue(this, play);
                } else {
                    NvMmGinko_EventDialogue(this, play);
                }
            }
            break;
        case TEXT_STATE_DONE:
            if (Message_ShouldAdvance(play)) {
                this->isStampChecked = false;
                NvMmGinko_SetupIdle(this);
            }
            break;
        default:
            break;
    }
}

/** MM's EnGinkoMan_BankAward: the reward offered. */
static void NvMmGinko_BankAward(NvMmGinko* this, PlayState* play) {
    s32 gi = MM_GI_RUPEE_BLUE;

    if (Actor_HasParent(&this->actor, play)) {
        this->actor.parent = NULL;
        this->func = NvMmGinko_BankAward2;
        return;
    }
    if (this->curTextId == 0x45B) {
        if (!NvMm_CheckWeekEvent(WE_RECEIVED_BANK_WALLET_UPGRADE) && (CUR_UPG_VALUE(UPG_WALLET) < 2)) {
            gi = MM_GI_WALLET_ADULT + CUR_UPG_VALUE(UPG_WALLET); // (the next wallet: OoT's, shared)
        }
    } else if ((this->curTextId == 0x45D) && !NvMm_CheckWeekEvent(WE_RECEIVED_BANK_HEART_PIECE)) {
        gi = MM_GI_HEART_PIECE;
    }
    GiveItemEntryFromActor(&this->actor, play, NvMmGetItem_Entry(gi), 500.0f, 100.0f);
}

/** MM's EnGinkoMan_BankAward2: after the reward, his line. */
static void NvMmGinko_BankAward2(NvMmGinko* this, PlayState* play) {
    s32 wallet = !NvMm_CheckWeekEvent(WE_RECEIVED_BANK_WALLET_UPGRADE) && (this->curTextId == 0x45B);

    if (Actor_ProcessTalkRequest(&this->actor, play)) { // (OoT's talk opens the actor's text)
        if (wallet) {
            NvMm_SetWeekEvent(WE_RECEIVED_BANK_WALLET_UPGRADE);
            this->curTextId = 0x47A;
        } else {
            NvMmGinko_Anim(this, ANIM_SITTING);
            this->curTextId = 0x47B;
        }
        this->func = NvMmGinko_Dialogue;
    } else if (this->curTextId == 0x45D) { // (the piece of heart: its own message ends it)
        if ((Message_GetState(&play->msgCtx) == TEXT_STATE_DONE) && Message_ShouldAdvance(play)) {
            NvMm_SetWeekEvent(WE_RECEIVED_BANK_HEART_PIECE);
            NvMmGinko_SetupIdle(this);
        } else if (play->msgCtx.msgMode == MSGMODE_NONE) {
            NvMm_SetWeekEvent(WE_RECEIVED_BANK_HEART_PIECE);
            NvMmGinko_SetupIdle(this);
        }
    } else {
        this->actor.textId = NV_MM_TEXT(wallet ? 0x47A : 0x47B);
        Actor_OfferTalk(&this->actor, play, 500.0f); // (MM's Actor_OfferTalkExchangeEquiCylinder: he speaks next)
    }
}

/** MM's EnGinkoMan_Stamp: stamping Link's hand (a new account), or checking it. */
static void NvMmGinko_Stamp(NvMmGinko* this, PlayState* play) {
    if (Animation_OnFrame(&this->skelAnime, this->skelAnime.endFrame)) {
        NvMmGinko_Anim(this, ANIM_SITTING);
        if (this->curTextId == 0x464) {
            NvMmGinko_Say(this, play, 0x465);
        } else if (this->curTextId == 0x469) {
            NvMmMsg_SetRupeesTotal(sBankRupees);
            NvMmGinko_Say(this, play, NvMmGinko_LastNight() ? 0x46C : 0x46A);
        }
        this->func = NvMmGinko_Dialogue;
    }
}

void NvMmGinko_Update(Actor* thisx, PlayState* play) {
    NvMmGinko* this = (NvMmGinko*)thisx;

    this->func(this, play);
    thisx->focus.pos = thisx->world.pos;
    thisx->focus.pos.y += 30.0f;
    // MM's EnGinkoMan_FacePlayer
    SkelAnime_Update(&this->skelAnime);
    if (this->animIndex != ANIM_ADVERTISING) {
        Actor_TrackPlayer(play, thisx, &this->headRot, &this->torsoRot, thisx->focus.pos);
    } else {
        Actor_TrackNone(&this->headRot, &this->torsoRot);
    }
}

static s32 NvMmGinko_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                      void* thisx) {
    NvMmGinko* this = (NvMmGinko*)thisx;

    if (limbIndex == OBJECT_BOJ_LIMB_0F) { // his own head
        *dList = (Gfx*)BOJ("object_boj_DL_00B1D8");
        Matrix_Translate(1500.0f, 0.0f, 0.0f, MTXMODE_APPLY);
        Matrix_RotateX(BINANG_TO_RAD(this->headRot.y), MTXMODE_APPLY);
        Matrix_RotateZ(BINANG_TO_RAD(this->headRot.x), MTXMODE_APPLY);
        Matrix_Translate(-1500.0f, 0.0f, 0.0f, MTXMODE_APPLY);
    } else if (limbIndex == OBJECT_BOJ_LIMB_08) {
        Matrix_RotateX(BINANG_TO_RAD(-this->torsoRot.y), MTXMODE_APPLY);
        Matrix_RotateZ(BINANG_TO_RAD(-this->torsoRot.x), MTXMODE_APPLY);
    }
    return false;
}

void NvMmGinko_Draw(Actor* thisx, PlayState* play) {
    NvMmGinko* this = (NvMmGinko*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, Gfx_EnvColor(play->state.gfxCtx, 50, 80, 0, 0));
    gSPSegment(POLY_OPA_DISP++, 0x09, Gfx_EnvColor(play->state.gfxCtx, 50, 80, 0, 0));
    gDPPipeSync(POLY_OPA_DISP++);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          NvMmGinko_OverrideLimbDraw, NULL, this);
    CLOSE_DISPS(play->state.gfxCtx);
}
