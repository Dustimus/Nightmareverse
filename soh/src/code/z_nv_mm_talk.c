/*
 * Nightmareverse: MM's talk spots (what Link can read or check: a book, a picture), from MM's
 * En_Talk (zeldaret/mm, ovl_En_Talk), ported as it is (mm/README.md): its line is MM's text
 * 0x1C00 + (params & 0x3F) (ours 0xB000 + MM's id), offered when Link stands close facing it, or
 * has it locked on.
 */
#include "global.h"

#define NV_MM_TEXT(id) (0xB000 + (id))

typedef struct NvMmTalk {
    Actor actor;
    s32 talking;
} NvMmTalk;

const size_t gNvMmTalkSize = sizeof(NvMmTalk);

void NvMmTalk_Init(Actor* thisx, PlayState* play) {
    s8 range = thisx->home.rot.x - 1; // (MM's ENTALK_GET_ATTENTION_RANGE_TYPE)

    if ((range >= 0) && (range < 7)) {
        thisx->targetMode = range;
    }
    Actor_SetScale(thisx, 1.0f);
    thisx->textId = NV_MM_TEXT((thisx->params & 0x3F) + 0x1C00);
}

void NvMmTalk_Destroy(Actor* thisx, PlayState* play) {
}

void NvMmTalk_Update(Actor* thisx, PlayState* play) {
    NvMmTalk* this = (NvMmTalk*)thisx;

    if (this->talking) {
        if (Actor_TextboxIsClosing(thisx, play)) {
            this->talking = false;
        }
        return;
    }
    if (Actor_ProcessTalkRequest(thisx, play)) {
        this->talking = true;
        return;
    }
    if (((thisx->xzDistToPlayer < 40.0f) && Player_IsFacingActor(thisx, 0x3000, play)) ||
        thisx->isTargeted) { // (MM's isLockedOn)
        Actor_OfferTalk(thisx, play, 120.0f);
    }
}
