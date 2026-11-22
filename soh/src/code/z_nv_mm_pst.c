/*
 * Nightmareverse: Clock Town's mailboxes, from MM's En_Pst (zeldaret/mm, ovl_En_Pst), ported closely
 * (mm/README.md). MM's model (object_pst: the box, and its mail slot, which drops while it's being
 * used), MM's schedule (mm/schedules/En_Pst.schl: each box is out of reach the four minutes the
 * Postman empties it) and MM's message scripts: "Deposit mail here.", the item prompt, then a
 * letter taken, anything else refused, or "Thank you for using the postal system."
 *
 * Params: which box (0 South Clock Town upper, 1 North, 2 East upper, 3 East lower, 4 South lower).
 *
 * Adapted from MM:
 * - MM's letters (the Letter to Kafei, the Express Mail to Mama) aren't in yet, nor MM's pause-menu
 *   item showing; so MM's item prompt is skipped and the box goes on as when it's closed in MM
 *   (its callback's branch 3, "Thank you for using the postal system.").
 *   The letter scripts are here as MM's, for when the letters come; the deposit's day/late week
 *   events too;
 * - the Postman's Hat scene (a heart piece for it) waits for MM's masks;
 * - MM's offer mode and facing check are OoT's talk offer, with MM's ranges.
 */
#include "global.h"
#include "z_nv_mm_schedule.h"
#include "z_nv_mm_msgevent.h"

enum { POSTBOX_SCH_NONE, POSTBOX_SCH_AVAILABLE, POSTBOX_SCH_CHECKED_BY_POSTMAN };
enum { POSTBOX_BEHAVIOUR_WAIT_FOR_ITEM, POSTBOX_BEHAVIOUR_TAKE_ITEM };
#define POSTBOX_LIMB_MAIL_SLOT 2
#define POSTBOX_LIMB_MAX 3

#include "nv_mm_schedules/En_Pst.schl.inc"
static u8* sScheduleScripts[] = { D_80B2C200, D_80B2C20C, D_80B2C218, D_80B2C224, D_80B2C230 };

#define MM_WE(byte, mask) (((byte) << 8) | (mask))
#define WEEKEVENTREG_LETTER_TO_KAFEI_SENT_IN_TIME MM_WE(91, 0x04)
#define WEEKEVENTREG_LETTER_TO_KAFEI_SENT_LATE MM_WE(91, 0x08)
#define BOMBERS_NOTEBOOK_EVENT_DEPOSITED_LETTER_TO_KAFEI 0x18
#define ITEM_LETTER_TO_KAFEI_MM 0x2F // (MM's item id: no such item here yet; its delete does nothing)

// by box: MM's WEEKEVENTREG_DEPOSITED_LETTER_TO_KAFEI_* and WEEKEVENTREG_POSTMAN_CHECKED_*
static const u16 sDeposited[] = { MM_WE(27, 0x02), MM_WE(27, 0x04), MM_WE(27, 0x08), MM_WE(27, 0x10), MM_WE(27, 0x20) };
static const u16 sChecked[] = { MM_WE(27, 0x40), MM_WE(27, 0x80), MM_WE(28, 0x01), MM_WE(28, 0x02), MM_WE(28, 0x04) };

// MM's main script (D_80B2C23C ... D_80B2C36C: the same but for the box's two flags)
#define POSTBOX_SCRIPT(deposited, checked)                                                                    \
    {                                                                                                          \
        MSCRIPT_CMD_BEGIN_TEXT(0x2784), MSCRIPT_CMD_AWAIT_TEXT(), MSCRIPT_CMD_BEGIN_TEXT(0x00FF),              \
            MSCRIPT_CMD_CHECK_CALLBACK_MULTI(0x0, 0x003F - 0x000E, 0x0047 - 0x000E),                           \
            MSCRIPT_CMD_CHECK_CALLBACK_CONTINUE(0x001B - 0x0011), MSCRIPT_CMD_CLOSE_TEXT(),                    \
            MSCRIPT_CMD_PLAYER_TALK(0x27A4), MSCRIPT_CMD_AWAIT_TEXT(), MSCRIPT_CMD_JUMP_3(0x0),                \
            MSCRIPT_CMD_AWAIT_TEXT(), MSCRIPT_CMD_DONE(), MSCRIPT_CMD_CLOSE_TEXT(),                            \
            MSCRIPT_CMD_PLAYER_TALK(0x2787), MSCRIPT_CMD_AWAIT_TEXT(), MSCRIPT_CMD_JUMP_3(0x0),                \
            MSCRIPT_CMD_AWAIT_TEXT(), MSCRIPT_CMD_SET_WEEK_EVENT_REG(deposited),                               \
            MSCRIPT_CMD_DELETE_ITEM(ITEM_LETTER_TO_KAFEI_MM),                                                  \
            MSCRIPT_CMD_CHECK_WEEK_EVENT_REG(checked, 0x0037 - 0x002F), MSCRIPT_CMD_CONTINUE_TEXT(0x2788),     \
            MSCRIPT_CMD_NOTEBOOK_EVENT(BOMBERS_NOTEBOOK_EVENT_DEPOSITED_LETTER_TO_KAFEI),                      \
            MSCRIPT_CMD_AWAIT_TEXT(), MSCRIPT_CMD_DONE(), MSCRIPT_CMD_CONTINUE_TEXT(0x2789),                   \
            MSCRIPT_CMD_NOTEBOOK_EVENT(BOMBERS_NOTEBOOK_EVENT_DEPOSITED_LETTER_TO_KAFEI),                      \
            MSCRIPT_CMD_AWAIT_TEXT(), MSCRIPT_CMD_DONE(), MSCRIPT_CMD_CLOSE_TEXT(),                            \
            MSCRIPT_CMD_PLAYER_TALK(0x2785), MSCRIPT_CMD_AWAIT_TEXT(), MSCRIPT_CMD_JUMP_3(0x0019 - 0x0047),    \
            MSCRIPT_CMD_BEGIN_TEXT(0x2786), MSCRIPT_CMD_AWAIT_TEXT(), MSCRIPT_CMD_DONE(),                      \
    }
static MsgScript sScripts[5][76] = {
    POSTBOX_SCRIPT(MM_WE(27, 0x02), MM_WE(27, 0x40)), POSTBOX_SCRIPT(MM_WE(27, 0x04), MM_WE(27, 0x80)),
    POSTBOX_SCRIPT(MM_WE(27, 0x08), MM_WE(28, 0x01)), POSTBOX_SCRIPT(MM_WE(27, 0x10), MM_WE(28, 0x02)),
    POSTBOX_SCRIPT(MM_WE(27, 0x20), MM_WE(28, 0x04)),
};

#define MM_PST(name) ("__OTR__mm/objects/object_pst/" name)
static const char sSkel[] = MM_PST("gPostboxSkel");
static const char sIdleAnim[] = MM_PST("gPostboxIdleAnim");

struct NvMmPst;
typedef void (*NvMmPstFunc)(struct NvMmPst*, PlayState*);

typedef struct NvMmPst {
    Actor actor;
    SkelAnime skelAnime;
    NvMmPstFunc func;
    ColliderCylinder collider;
    u8 scheduleResult;
    MsgScript* msgScript;
    s32 msgScriptPos;
    Vec3s jointTable[POSTBOX_LIMB_MAX];
    Vec3s morphTable[POSTBOX_LIMB_MAX];
    u16 stateFlags; // 0x40: in use (the slot drops)
    s16 behaviour;
    MsgScriptCallback msgScriptCallback;
    s32 isLetterToKafeiDeposited;
} NvMmPst;

const size_t gNvMmPstSize = sizeof(NvMmPst);

s32 NvMm_CheckWeekEvent(s32 flag);
void NvMm_SetWeekEvent(s32 flag);
s32 NvMmClock_Day(void);

static ColliderCylinderInit sCylinderInit = {
    { COLTYPE_HIT1, AT_NONE, AC_NONE, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK1, { 0x00000000, 0x00, 0x00 }, { 0x00000000, 0x00, 0x00 }, TOUCH_NONE | TOUCH_SFX_NORMAL,
      BUMP_NONE, OCELEM_ON },
    { 28, 72, 0, { 0, 0, 0 } },
};

static void NvMmPst_FollowSchedule(NvMmPst* this, PlayState* play);

static s32 NvMmPst_Box(NvMmPst* this) {
    return CLAMP(this->actor.params, 0, 4);
}

/** MM's EnPst_ChooseBehaviour: at the item prompt, which way the script goes. Nothing can be shown
 * here yet (MM's letters and item showing aren't in; the prompt isn't shown, z_nv_mm_msgevent.c): as
 * MM's "closed without showing" (3). */
static s32 NvMmPst_ChooseBehaviour(Actor* thisx, PlayState* play) {
    NvMmPst* this = (NvMmPst*)thisx;

    if (this->behaviour == POSTBOX_BEHAVIOUR_WAIT_FOR_ITEM) {
        this->behaviour++;
        return 3; // (MM: the prompt closed without an item shown)
    }
    return 0;
}

static void NvMmPst_Talk(NvMmPst* this, PlayState* play) {
    if (NvMmMsgEvent_RunScript(&this->actor, play, this->msgScript, this->msgScriptCallback, &this->msgScriptPos)) {
        if (NvMm_CheckWeekEvent(sDeposited[NvMmPst_Box(this)]) != this->isLetterToKafeiDeposited) {
            switch (NvMmClock_Day()) {
                case 1:
                    NvMm_SetWeekEvent(WEEKEVENTREG_LETTER_TO_KAFEI_SENT_IN_TIME);
                    break;
                case 2:
                    NvMm_SetWeekEvent(NvMm_CheckWeekEvent(sChecked[NvMmPst_Box(this)])
                                          ? WEEKEVENTREG_LETTER_TO_KAFEI_SENT_LATE
                                          : WEEKEVENTREG_LETTER_TO_KAFEI_SENT_IN_TIME);
                    break;
                default:
                    NvMm_SetWeekEvent(WEEKEVENTREG_LETTER_TO_KAFEI_SENT_LATE);
                    break;
            }
        }
        this->stateFlags &= ~0x40;
        this->msgScriptPos = 0;
        this->func = NvMmPst_FollowSchedule;
    }
}

/** MM's EnPst_FollowSchedule: there unless the Postman is emptying it. */
static void NvMmPst_FollowSchedule(NvMmPst* this, PlayState* play) {
    NvMmScheduleOutput out;

    out.result = POSTBOX_SCH_NONE;
    if (!NvMmSchedule_RunScript(play, sScheduleScripts[NvMmPst_Box(this)], &out)) {
        out.result = POSTBOX_SCH_NONE;
    }
    if (out.result == POSTBOX_SCH_NONE) {
        this->actor.shape.shadowDraw = NULL;
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    } else {
        this->actor.shape.shadowDraw = ActorShadow_DrawCircle;
        this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    }
    this->scheduleResult = out.result;
}

void NvMmPst_Init(Actor* thisx, PlayState* play) {
    NvMmPst* this = (NvMmPst*)thisx;

    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 18.0f);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sSkel, NULL, this->jointTable, this->morphTable,
                       POSTBOX_LIMB_MAX);
    Animation_Change(&this->skelAnime, (AnimationHeader*)sIdleAnim, 1.0f, 0.0f,
                     Animation_GetLastFrame((AnimationHeader*)sIdleAnim), ANIMMODE_ONCE, 0.0f);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    this->actor.targetMode = 0;
    Actor_SetScale(&this->actor, 0.02f);
    this->func = NvMmPst_FollowSchedule;
    Actor_UpdateBgCheckInfo(play, &this->actor, 0.0f, 0.0f, 0.0f, 4);
}

void NvMmPst_Destroy(Actor* thisx, PlayState* play) {
    NvMmPst* this = (NvMmPst*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

void NvMmPst_Update(Actor* thisx, PlayState* play) {
    NvMmPst* this = (NvMmPst*)thisx;

    if ((this->func == NvMmPst_FollowSchedule) && (this->scheduleResult == POSTBOX_SCH_AVAILABLE) &&
        Actor_ProcessTalkRequest(&this->actor, play)) { // (MM's EnPst_CheckTalk, no item shown)
        this->isLetterToKafeiDeposited = NvMm_CheckWeekEvent(sDeposited[NvMmPst_Box(this)]);
        this->behaviour = POSTBOX_BEHAVIOUR_WAIT_FOR_ITEM;
        this->stateFlags |= 0x40;
        this->msgScript = sScripts[NvMmPst_Box(this)];
        this->msgScriptCallback = NvMmPst_ChooseBehaviour;
        this->msgScriptPos = 0;
        this->func = NvMmPst_Talk;
    }
    this->func(this, play);
    if (this->scheduleResult != POSTBOX_SCH_NONE) {
        if ((this->func == NvMmPst_FollowSchedule) && (this->scheduleResult == POSTBOX_SCH_AVAILABLE) &&
            Actor_IsFacingPlayer(&this->actor, 0x1FFE)) {
            Actor_OfferTalkExchange(&this->actor, play, 60.0f, 20.0f, EXCH_ITEM_NONE);
        }
        Actor_SetFocus(&this->actor, 20.0f);
        Collider_UpdateCylinder(&this->actor, &this->collider);
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    }
}

static s32 NvMmPst_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                    void* thisx) {
    NvMmPst* this = (NvMmPst*)thisx;

    if (limbIndex == POSTBOX_LIMB_MAIL_SLOT) {
        Matrix_Translate(0.0f, (this->stateFlags & 0x40) ? -100.0f : 0.0f, 0.0f, MTXMODE_APPLY);
    }
    return false;
}

void NvMmPst_Draw(Actor* thisx, PlayState* play) {
    NvMmPst* this = (NvMmPst*)thisx;

    if (this->scheduleResult != POSTBOX_SCH_NONE) {
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable,
                              this->skelAnime.dListCount, NvMmPst_OverrideLimbDraw, NULL, &this->actor);
    }
}
