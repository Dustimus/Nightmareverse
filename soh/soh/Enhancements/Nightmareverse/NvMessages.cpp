// Nightmareverse: RE1's room messages in OoT's text box. z_nv_script.c picks the message an
// examine spot shows (RE1's display_msg_room_action) and opens text NV_TEXT_ID; this fills it, in
// the blue box Navi speaks in. The text comes from the player's own RE1 files, decoded when the
// mod is made (re1/tools/soh_scene.py decode_message).
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"

extern "C" {
const char* NvMsg_PendingText(void);
}

#define NV_TEXT_ID 0xA1E0 // (z_nv_script.c)

static void NvMsg_OnOpenText(uint16_t* textId, bool* loadFromMessageTable) {
    const char* text = NvMsg_PendingText();

    if ((*textId != NV_TEXT_ID) || (text == nullptr) || (text[0] == '\0')) {
        return;
    }
    CustomMessage msg(text, TEXTBOX_TYPE_BLUE);
    msg.AutoFormat();
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

extern "C" void NvMsg_EnsureHook(void) {
    static bool registered = false;

    if (!registered) {
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnOpenText>(NvMsg_OnOpenText);
        registered = true;
    }
}
