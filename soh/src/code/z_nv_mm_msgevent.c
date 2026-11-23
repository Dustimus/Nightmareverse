/*
 * Nightmareverse: Majora's Mask's message scripts, from MM's z_msgevent.c (zeldaret/mm), ported
 * closely (mm/README.md). MM's people talk through small scripts (MsgScript, the MSCRIPT_CMD_*
 * macros in z_nv_mm_msgevent.h, written in each actor's file as in MM): show a line, wait for it,
 * branch on a week event, the day, Link's form, a choice, rupees, an item..., set flags. The
 * actor runs its script each frame while talking; it stops where it must wait and carries on
 * from there the next frame (pos), and says when it's done.
 *
 * Adapted from MM:
 * - text ids are MM's: shown as ours, 0xB000 + id (NvMmText.cpp); MM's item prompt (text 0xFF: an
 *   item shown from the pause menu mid-talk) isn't shown, OoT has no such step: the actor's
 *   callback then goes on as if the prompt was closed;
 * - Link is always MM's human form for now (no Deku, Goron, Zora yet): the form checks follow that;
 * - MM's items: given as MM's get-items (z_nv_mm_getitem.c: into OoT's inventory where OoT has the
 *   same thing, else kept as MM's), checked and taken the same way; MM's songs and quest items are
 *   MM's own. Wearing masks isn't in yet: the worn-mask check is false;
 * - week events are Termina's (z_nv_mm_scene.c); MM's eventInf (temporary flags) are kept here,
 *   for the cycle's session, not saved, as in MM; switch and collectible flags are OoT's own;
 * - the Bombers' Notebook isn't in yet: its events set MM's week events for them (NvMm_NotebookEvent);
 * - MM's Actor_ChangeFocus (the conversation camera turning to the other person) is OoT's
 *   talkActor: the text camera follows whoever it names.
 */
#include "global.h"
#include "z_nv_mm_msgevent.h"
#include "z_nv_mm_schedule.h"
#include <stdio.h>
#include <stdlib.h>

#define NV_MM_TEXT(id) (0xB000 + (id))
#define PACK_16(h, l) ((((h) & 0xFF) << 8) | ((l) & 0xFF))

s32 NvMm_CheckWeekEvent(s32 flag);
void NvMm_SetWeekEvent(s32 flag);
s32 NvMmClock_Day(void);
u16 NvMmClock_HalfDayBit(void);

static u8 sEventInf[8]; // MM's eventInf: a session's temporary flags

#define MM_WE(byte, mask) (((byte) << 8) | (mask))

// MM's notebook table (tables/notebook_table.h): the week event each of its 55 events sets, the 20
// people's "met" entries first, then the rest (from the decomp's table, by its flags' names)
static const u16 sNotebookEventFlags[] = {
    MM_WE(68, 0x08), MM_WE(66, 0x01), MM_WE(66, 0x02), MM_WE(66, 0x04), MM_WE(66, 0x08),
    MM_WE(66, 0x10), MM_WE(66, 0x20), MM_WE(66, 0x40), MM_WE(66, 0x80), MM_WE(67, 0x01),
    MM_WE(67, 0x02), MM_WE(67, 0x04), MM_WE(67, 0x08), MM_WE(67, 0x10), MM_WE(67, 0x20),
    MM_WE(67, 0x40), MM_WE(67, 0x80), MM_WE(68, 0x01), MM_WE(68, 0x02), MM_WE(68, 0x04),
    MM_WE(68, 0x10), MM_WE(68, 0x20), MM_WE(68, 0x40), MM_WE(68, 0x80), MM_WE(69, 0x01),
    MM_WE(69, 0x02), MM_WE(69, 0x04), MM_WE(69, 0x08), MM_WE(69, 0x10), MM_WE(69, 0x20),
    MM_WE(69, 0x40), MM_WE(69, 0x80), MM_WE(70, 0x01), MM_WE(70, 0x02), MM_WE(70, 0x04),
    MM_WE(70, 0x08), MM_WE(70, 0x10), MM_WE(70, 0x20), MM_WE(70, 0x40), MM_WE(70, 0x80),
    MM_WE(71, 0x01), MM_WE(71, 0x02), MM_WE(71, 0x04), MM_WE(71, 0x08), MM_WE(71, 0x10),
    MM_WE(71, 0x20), MM_WE(71, 0x40), MM_WE(71, 0x80), MM_WE(72, 0x01), MM_WE(72, 0x02),
    MM_WE(72, 0x04), MM_WE(72, 0x08), MM_WE(72, 0x10), MM_WE(72, 0x20), MM_WE(72, 0x40),
};

/** A Bombers' Notebook event (MM's Message_BombersNotebookQueueEvent). The notebook itself isn't in
 * yet: the event's week event is set (MM's, kept across cycles as MM keeps them), so it'll be
 * there when the notebook comes. */
void NvMm_NotebookEvent(u16 event) {
    if (event < ARRAY_COUNT(sNotebookEventFlags)) {
        NvMm_SetWeekEvent(sNotebookEventFlags[event]);
    }
    if (getenv("NV_LOG") != NULL) {
        printf("NVMM notebook event %d\n", event);
        fflush(stdout);
    }
}


/** MM's eventInf (flag: byte << 4 | bit), for the actors that check it themselves (Granny). */
s32 NvMmMsgEvent_EventInf(s32 flag) {
    return (sEventInf[(flag >> 4) & 7] & (1 << (flag & 0xF))) != 0;
}

void NvMmMsgEvent_SetEventInf(s32 flag, s32 on) {
    if (on) {
        sEventInf[(flag >> 4) & 7] |= (1 << (flag & 0xF));
    } else {
        sEventInf[(flag >> 4) & 7] &= ~(1 << (flag & 0xF));
    }
}

static s32 NvMmMsgEvent_IsNight(void) {
    return (NvMmClock_HalfDayBit() & 0x155) != 0; // (the nights' bits)
}

GetItemEntry NvMmGetItem_Entry(s32 mmGi); // (z_nv_mm_getitem.c)
s32 NvMmItem_Has(s32 item);
s32 NvMmItem_HasQuest(s32 quest);
s32 NvMmItem_Check(s32 item);
void NvMmItem_Delete(s32 item);

static s32 NvMmMsgEvent_TextAdvance(PlayState* play) {
    switch (Message_GetState(&play->msgCtx)) {
        case TEXT_STATE_CHOICE:
        case TEXT_STATE_EVENT:
            return Message_ShouldAdvance(play) ? 1 : 0;
        case TEXT_STATE_CLOSING:
            return 1;
        default:
            return 0;
    }
}

static u8 sCmdSizes[MSCRIPT_CMD_ID_MAX] = {
    5, 3, 3, 3, 3, 7, 5, 3, 5, 3, 13, 3, 1, 1, 3, 3, 1, 3, 1, 3, 3, 1, 1, 1, 1, 3,
    5, 5, 3, 3, 9, 5, 5, 5, 5, 5, 3, 5, 7, 5, 3, 3, 3, 7, 3, 3, 1, 3, 1, 1, 1,
};

/** One command: true to stop for this frame. *done is set when the script has finished. */
static s32 NvMmMsgEvent_Command(Actor* actor, PlayState* play, u8** script, MsgScriptCallback callback, s32* done) {
    u8* c = *script;
    Player* player = GET_PLAYER(play);
    s16 skip;

    switch (c[0]) {
        case MSCRIPT_CMD_ID_CHECK_WEEK_EVENT_REG:
            if (NvMm_CheckWeekEvent(PACK_16(c[1], c[2]))) {
                *script += (s16)PACK_16(c[3], c[4]);
            }
            return false;
        case MSCRIPT_CMD_ID_CHECK_GORON:
        case MSCRIPT_CMD_ID_CHECK_ZORA:
        case MSCRIPT_CMD_ID_CHECK_DEKU:
            return false; // (Link is human)
        case MSCRIPT_CMD_ID_CHECK_HUMAN:
            *script += (s16)PACK_16(c[1], c[2]);
            return false;
        case MSCRIPT_CMD_ID_CHECK_TEXT_CHOICE:
            skip = 0;
            if (play->msgCtx.choiceIndex <= 2) {
                skip = (s16)PACK_16(c[1 + 2 * play->msgCtx.choiceIndex], c[2 + 2 * play->msgCtx.choiceIndex]);
            }
            *script += skip;
            return false;
        case MSCRIPT_CMD_ID_OFFER_ITEM: { // (MM's get-item, as MM's: z_nv_mm_getitem.c)
            GetItemEntry e = NvMmGetItem_Entry(PACK_16(c[1], c[2]));

            if ((e.getItemId == GI_NONE) || Actor_HasParent(actor, play)) {
                *script += (s16)PACK_16(c[3], c[4]);
                return false;
            }
            GiveItemEntryFromActor(actor, play, e, actor->xzDistToPlayer + 1.0f, fabsf(actor->yDistToPlayer) + 1.0f);
            return true;
        }
        case MSCRIPT_CMD_ID_AUTOTALK:
            if (Actor_ProcessTalkRequest(actor, play)) {
                *script += (s16)PACK_16(c[1], c[2]);
                return false;
            } else {
                f32 xzDist = actor->xzDistToPlayer;

                actor->flags |= ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
                actor->xzDistToPlayer = 0.0f;
                Actor_OfferTalkExchange(actor, play, xzDist + 1.0f, fabsf(actor->yDistToPlayer) + 1.0f,
                                        EXCH_ITEM_NONE);
                actor->xzDistToPlayer = xzDist;
            }
            return true;
        case MSCRIPT_CMD_ID_CHECK_RUPEES:
            if (gSaveContext.rupees >= (s16)PACK_16(c[1], c[2])) {
                *script += (s16)PACK_16(c[3], c[4]);
            }
            return false;
        case MSCRIPT_CMD_ID_CHECK_CALLBACK:
            if ((callback == NULL) || (callback(actor, play) != 0)) {
                *script += (s16)PACK_16(c[1], c[2]);
                return false;
            }
            return true;
        case MSCRIPT_CMD_ID_CHECK_DAY: {
            s32 day = NvMmClock_Day();

            skip = 0;
            if ((day >= 1) && (day <= 3)) {
                s32 i = 1 + 4 * (day - 1) + (NvMmMsgEvent_IsNight() ? 2 : 0);

                skip = (s16)PACK_16(c[i], c[i + 1]);
            }
            *script += skip;
            return false;
        }
        case MSCRIPT_CMD_ID_AWAIT_TEXT_JUMP:
            if (!NvMmMsgEvent_TextAdvance(play)) {
                return true;
            }
            *script += (s16)PACK_16(c[1], c[2]);
            return false;
        case MSCRIPT_CMD_ID_AWAIT_TEXT:
            return !NvMmMsgEvent_TextAdvance(play);
        case MSCRIPT_CMD_ID_AWAIT_TEXT_END:
            *done = NvMmMsgEvent_TextAdvance(play);
            return true;
        case MSCRIPT_CMD_ID_BEGIN_TEXT:
            if (PACK_16(c[1], c[2]) == 0xFF) {
                return false; // MM's item prompt (show an item from the pause menu): none in OoT's talk yet
            }
            Message_StartTextbox(play, NV_MM_TEXT(PACK_16(c[1], c[2])), NULL);
            return false;
        case MSCRIPT_CMD_ID_CONTINUE_TEXT:
            Message_ContinueTextbox(play, NV_MM_TEXT(PACK_16(c[1], c[2])));
            return false;
        case MSCRIPT_CMD_ID_DONE:
            *done = true;
            return true;
        case MSCRIPT_CMD_ID_SET_WEEK_EVENT_REG:
            NvMm_SetWeekEvent(PACK_16(c[1], c[2]));
            return false;
        case MSCRIPT_CMD_ID_CLOSE_TEXT:
            Message_CloseTextbox(play);
            return false;
        case MSCRIPT_CMD_ID_SET_COLLECTIBLE:
            if (!Flags_GetCollectible(play, PACK_16(c[1], c[2]))) {
                Flags_SetCollectible(play, PACK_16(c[1], c[2]));
            }
            actor->parent = NULL;
            return false;
        case MSCRIPT_CMD_ID_CHANGE_RUPEES:
            Rupees_ChangeBy((s16)PACK_16(c[1], c[2]));
            return false;
        case MSCRIPT_CMD_ID_PAUSE:
            play->msgCtx.msgMode = MSGMODE_PAUSED;
            return false;
        case MSCRIPT_CMD_ID_UNSET_AUTOTALK:
            actor->flags &= ~ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
            return false;
        case MSCRIPT_CMD_ID_FOCUS_TO_CHILD:
            if (actor->child != NULL) {
                player->talkActor = actor->child;
            }
            return false;
        case MSCRIPT_CMD_ID_FOCUS_TO_SELF:
            if (actor->child != NULL) {
                player->talkActor = actor;
            }
            return false;
        case MSCRIPT_CMD_ID_JUMP:
            *script += (s16)PACK_16(c[1], c[2]);
            return false;
        case MSCRIPT_CMD_ID_CHECK_QUEST_ITEM:
            if (NvMmItem_HasQuest(PACK_16(c[1], c[2]))) {
                *script += (s16)PACK_16(c[3], c[4]);
            }
            return false;
        case MSCRIPT_CMD_ID_CHECK_HAS_SONG:
            if (NvMmItem_HasQuest(6 + PACK_16(c[1], c[2]))) { // (MM's QUEST_SONG_SONATA + song)
                *script += (s16)PACK_16(c[3], c[4]);
            }
            return false;
        case MSCRIPT_CMD_ID_CHECK_ITEM:
            if (NvMmItem_Check(PACK_16(c[1], c[2]))) {
                *script += (s16)PACK_16(c[3], c[4]);
            }
            return false;
        case MSCRIPT_CMD_ID_CHECK_HAS_POWDER_KEG:
            if (NvMmItem_Has(0x0C)) { // ITEM_POWDER_KEG
                *script += (s16)PACK_16(c[1], c[2]);
            }
            return false;
        case MSCRIPT_CMD_ID_DELETE_ITEM:
            NvMmItem_Delete(PACK_16(c[1], c[2]));
            return false;
        case MSCRIPT_CMD_ID_CHECK_WORN_MASK:
            return false; // (wearing MM's masks: not in yet)
        case MSCRIPT_CMD_ID_CHECK_EVENT_INF:
            if (sEventInf[c[1] & 7] & c[2]) {
                *script += (s16)PACK_16(c[3], c[4]);
            }
            return false;
        case MSCRIPT_CMD_ID_SET_EVENT_INF:
            sEventInf[c[1] & 7] |= c[2];
            return false;
        case MSCRIPT_CMD_ID_UNSET_EVENT_INF:
            sEventInf[c[1] & 7] &= c[2] ^ 0xFF;
            return false;
        case MSCRIPT_CMD_ID_CHECK_ITEM_ACTION:
            // MM: an item shown from the pause screen; none of MM's here yet: as if it isn't the one
            if (!NvMmMsgEvent_TextAdvance(play)) {
                return true;
            }
            *script += (s16)PACK_16(c[5], c[6]);
            return false;
        case MSCRIPT_CMD_ID_CHECK_AFTER_TIME:
            if (NvMmSchedule_Time(c[1], c[2]) < NvMmSchedule_Now()) {
                *script += (s16)PACK_16(c[3], c[4]);
            }
            return false;
        case MSCRIPT_CMD_ID_CHECK_BEFORE_TIME:
            if (NvMmSchedule_Time(c[1], c[2]) >= NvMmSchedule_Now()) {
                *script += (s16)PACK_16(c[3], c[4]);
            }
            return false;
        case MSCRIPT_CMD_ID_CHECK_SWITCH_FLAG:
            if (Flags_GetSwitch(play, PACK_16(c[1], c[2]))) {
                *script += (s16)PACK_16(c[3], c[4]);
            }
            return false;
        case MSCRIPT_CMD_ID_SET_SWITCH_FLAG:
            Flags_SetSwitch(play, PACK_16(c[1], c[2]));
            return false;
        case MSCRIPT_CMD_ID_CHECK_BETWEEN_TIME: {
            u16 start = NvMmSchedule_Time(c[1], c[2]);
            u16 end = NvMmSchedule_Time(c[3], c[4]) - 1;
            u16 now = NvMmSchedule_Now();

            if ((start >= now) || (now >= end)) {
                *script += (s16)PACK_16(c[5], c[6]);
            }
            return false;
        }
        case MSCRIPT_CMD_ID_CHECK_ON_DAY:
            if (NvMmClock_Day() == (s16)PACK_16(c[1], c[2])) {
                *script += (s16)PACK_16(c[3], c[4]);
            }
            return false;
        case MSCRIPT_CMD_ID_CHECK_CALLBACK_CONTINUE:
            if ((callback != NULL) && (callback(actor, play) != 0)) {
                *script += (s16)PACK_16(c[1], c[2]);
            }
            return false;
        case MSCRIPT_CMD_ID_CHECK_CALLBACK_MULTI: {
            s32 ret = (callback != NULL) ? callback(actor, play) : 1;

            if ((ret < 1) || (ret > 3)) {
                return true;
            }
            *script += (s16)PACK_16(c[1 + 2 * (ret - 1)], c[2 + 2 * (ret - 1)]);
            return false;
        }
        case MSCRIPT_CMD_ID_PLAYER_TALK:
            player->actor.textId = NV_MM_TEXT(PACK_16(c[1], c[2]));
            Message_CloseTextbox(play);
            return false;
        case MSCRIPT_CMD_ID_NOTEBOOK_EVENT:
            NvMm_NotebookEvent(PACK_16(c[1], c[2]));
            return false;
        case MSCRIPT_CMD_ID_AWAIT_TEXT_DONE:
            switch (Message_GetState(&play->msgCtx)) {
                case TEXT_STATE_CLOSING:
                    return false;
                case TEXT_STATE_CHOICE:
                case TEXT_STATE_EVENT:
                case TEXT_STATE_DONE:
                    return !Message_ShouldAdvance(play);
                default:
                    return true;
            }
        case MSCRIPT_CMD_ID_JUMP_3:
            *script += (s16)PACK_16(c[1], c[2]);
            *script += 3;
            return true;
        case MSCRIPT_CMD_ID_PLAY_DECIDE:
            Sfx_PlaySfxCentered(NA_SE_SY_DECIDE);
            return false;
        case MSCRIPT_CMD_ID_PLAY_CANCEL:
            Sfx_PlaySfxCentered(NA_SE_SY_CANCEL);
            return false;
        case MSCRIPT_CMD_ID_PLAY_ERROR:
            Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
            return false;
        default: // (not MM's: a bad command ends the script rather than running off)
            *done = true;
            return true;
    }
}

/** MM's MsgEvent_RunScript: runs from *pos until it must wait; true once the script is done. */
s32 NvMmMsgEvent_RunScript(Actor* actor, PlayState* play, MsgScript* script, MsgScriptCallback callback, s32* pos) {
    u8* start = script;
    s32 done = false;
    s32 len = 0;
    s32 stop;

    script += *pos;
    do {
        script += len;
        len = (*script < MSCRIPT_CMD_ID_MAX) ? sCmdSizes[*script] : 0;
        stop = NvMmMsgEvent_Command(actor, play, &script, callback, &done);
    } while (!stop);
    *pos = done ? 0 : (s32)(script - start);
    return done;
}
