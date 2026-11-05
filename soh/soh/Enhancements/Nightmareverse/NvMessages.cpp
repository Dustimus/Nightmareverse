// Nightmareverse: RE1's room messages in OoT's text box. z_nv_script.c picks the message an
// examine spot shows (RE1's display_msg_room_action) and opens text NV_TEXT_ID; this fills it, in
// the blue box Navi speaks in. The text comes from the player's own RE1 files, decoded when the
// mod is made (re1/tools/soh_scene.py decode_message).
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"

extern "C" {
const char* NvMsg_PendingText(void);
int32_t NvSong_Pending(void);
}

#define NV_NOCTURNE_PLAYED_TEXT 0x897 // the song's name, after its playback
#define NV_NOCTURNE_WARP_TEXT 0x891 // TEXT_WARP_NOCTURNE_OF_SHADOW, "Warp to the graveyard?" (Nocturne's slot, which the mansion's song rides)

#define NV_TEXT_ID 0xA1E0 // (z_nv_script.c)

static void NvMsg_OnOpenText(uint16_t* textId, bool* loadFromMessageTable) {
    const char* text = NvMsg_PendingText();

    if ((*textId == NV_NOCTURNE_PLAYED_TEXT) && NvSong_Pending()) { // the mansion's song: its name
        // all at once (OoT's quick-text code): the question replaces it after a second
        CustomMessage played(std::string("\x08") + "You played&%pClowns Farting in Basement%w.");
        played.AutoFormat();
        played.LoadIntoFont();
        *loadFromMessageTable = false;
        return;
    }
    if ((*textId == NV_NOCTURNE_WARP_TEXT) && NvSong_Pending()) { // the mansion's song: its own question
        CustomMessage warp("Warp to the&%pmansion%w?&" + CustomMessage::TWO_WAY_CHOICE() + "%gOK&No%w");
        warp.AutoFormat(); // its % and & codes into OoT's control bytes
        warp.LoadIntoFont();
        *loadFromMessageTable = false;
        return;
    }

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
