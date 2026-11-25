// Nightmareverse: Majora's Mask's text in OoT's text box. MM's messages come from the player's own
// ROM, converted to OoT's text codes when the mod is made (mm/tools/mm_text.py), one resource,
// mm/text/messages. Termina's actors ask for MM text id N as NV_MM_TEXT_BASE + N; this fills the box
// from that resource (SoH's OnOpenText hook, as NvMessages.cpp does for RE1's).
#include <cstring>
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/ShipInit.hpp"
#include <libultraship/bridge/resourcebridge.h>

#define NV_MM_TEXT_BASE 0xB000 // (OoT's own ids are below; RE1's NV_TEXT_ID is 0xA1E0)
#define NV_MM_TEXT_LAST 0xEFFF

struct NvMmTextEntry {
    uint16_t id;
    uint8_t box;
    uint8_t pos;
    uint16_t next;
    uint16_t len;
    uint32_t offset;
};

static const uint8_t* NvMmText_Data(uint32_t* count) {
    static const char* path = "mm/text/messages";

    if (!ResourceMgr_FileExists(path)) {
        return nullptr;
    }
    const uint8_t* data = (const uint8_t*)ResourceGetDataByName(path);
    if ((data == nullptr) || (memcmp(data, "MTXT", 4) != 0)) {
        return nullptr;
    }
    memcpy(count, data + 4, sizeof(*count));
    return data;
}

/** MM's message `id`, or null (binary search: the resource is sorted by id). */
static const NvMmTextEntry* NvMmText_Find(uint16_t id, const uint8_t** base) {
    uint32_t count;
    const uint8_t* data = NvMmText_Data(&count);

    if (data == nullptr) {
        return nullptr;
    }
    const NvMmTextEntry* e = (const NvMmTextEntry*)(data + 8);
    uint32_t lo = 0, hi = count;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        if (e[mid].id < id) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if ((lo < count) && (e[lo].id == id)) {
        *base = data;
        return &e[lo];
    }
    return nullptr;
}

/** MM's next text id after message `id` (its header's), or 0xFFFF. */
extern "C" uint16_t NvMmText_Next(uint16_t id) {
    const uint8_t* base;
    const NvMmTextEntry* e = NvMmText_Find(id, &base);

    return (e != nullptr) ? e->next : 0xFFFF;
}

extern "C" int32_t NvMmMsg_Expand(uint8_t code, uint8_t* out, int32_t max, int32_t* isInput);

/**
 * MM's value codes (tools/mm_text.py's marker: 0x7F, MM's code) written out as MM's decoder would, as
 * the message opens (z_nv_mm_msginput.c). A message with a number input waits at its end, as MM's:
 * OoT's event code before its end.
 */
static std::string NvMmText_Expand(const char* text, size_t len) {
    std::string out;
    bool input = false;

    for (size_t i = 0; i < len; i++) {
        uint8_t c = (uint8_t)text[i];

        if ((c == 0x7F) && (i + 1 < len)) {
            uint8_t buf[64];
            int32_t isInput = 0;
            int32_t n = NvMmMsg_Expand((uint8_t)text[++i], buf, sizeof(buf), &isInput);

            out.append((const char*)buf, n);
            input = input || isInput;
            continue;
        }
        out.push_back((char)c);
        // (OoT's codes with arguments: copied as they are, never read as a marker)
        int args = ((c == 0x05) || (c == 0x06) || (c == 0x0C) || (c == 0x0E) || (c == 0x14)) ? 1
                   : ((c == 0x07) || (c == 0x11) || (c == 0x12))                               ? 2
                                                                                               : 0;
        for (; (args > 0) && (i + 1 < len); args--) {
            out.push_back(text[++i]);
        }
    }
    if (input && (out.find('\x0B') == std::string::npos)) {
        size_t end = out.rfind('\x02');

        if (end != std::string::npos) {
            out.insert(end, 1, '\x0B');
        }
    }
    return out;
}

static void NvMmText_OnOpenText(uint16_t* textId, bool* loadFromMessageTable) {
    const uint8_t* base;

    if ((*textId < NV_MM_TEXT_BASE) || (*textId > NV_MM_TEXT_LAST)) {
        return;
    }
    const NvMmTextEntry* e = NvMmText_Find(*textId - NV_MM_TEXT_BASE, &base);
    if (e == nullptr) {
        return;
    }
    // MM's box types follow OoT's where they share them (black, wooden, blue, ...); others as black
    TextBoxType type = (e->box <= TEXTBOX_TYPE_NONE_NO_SHADOW) ? (TextBoxType)e->box : TEXTBOX_TYPE_BLACK;
    TextBoxPosition pos = (e->pos <= TEXTBOX_POS_BOTTOM) ? (TextBoxPosition)e->pos : TEXTBOX_POS_BOTTOM;
    CustomMessage msg(NvMmText_Expand((const char*)base + e->offset, e->len), type, pos);
    msg.LoadIntoFont(); // (already OoT's codes: no reformatting)
    *loadFromMessageTable = false;
}

static void NvMmText_Register() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnOpenText>(NvMmText_OnOpenText);
}

static RegisterShipInitFunc sNvMmTextInit(NvMmText_Register);
