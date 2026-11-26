/*
 * Nightmareverse: MM's message values and number inputs in OoT's text box (mm/README.md "MM's message
 * values and number inputs").
 *
 * Values: tools/mm_text.py keeps MM's value codes (counters, the time, the Bombers' code, rupee amounts)
 * as a marker (0x7F, then MM's code); NvMmText.cpp expands each when the message opens, through
 * NvMmMsg_Expand here, as MM's decoder would (z_message_nes.c): digits written as text.
 *
 * Inputs (MM's bank amount, the Bombers' code, the dog race's bet, the lottery's numbers): the digits
 * are written as text, each behind its own colour code; the message ends in OoT's event (the box
 * waits, as MM's input boxes do, for the actor to go on when A is pressed). While it waits, the stick
 * (or D-pad) changes the selected digit and moves between them, as MM's Message_Control*Input: the
 * digit's character and its glyph (OoT's font slot) are rewritten in place, its colour code red for
 * the selected one (MM draws a flashing box behind it instead). The actor reads the digits
 * (NvMmMsgInput_Digit, NvMmMsgInput_Value) when A goes on.
 * Hooked from Message_Update (z_message_PAL.c, one line).
 */
#include "global.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

s32 NvMmClock_Day(void);
u16 NvMmClock_Time(void);
s32 NvMmClock_IsNight(void);

enum {
    NVMM_INPUT_NONE,
    NVMM_INPUT_BANK,    // MM's MESSAGE_INPUT_BANK: 3 digits 0-9, starting 000, the cursor on the last
    NVMM_INPUT_DOGGY,   // MM's MESSAGE_INPUT_DOGGY_RACETRACK_BET: 2 digits (tens of rupees)
    NVMM_INPUT_BOMBER,  // MM's MESSAGE_INPUT_BOMBER_CODE: 5 digits 1-5, starting 11111
    NVMM_INPUT_LOTTERY, // MM's MESSAGE_INPUT_LOTTERY_CODE: 3 digits 0-9, starting 111
};

#define COL_DIGIT 0x40    // (OoT's colour codes: white)
#define COL_SELECTED 0x41 // (red)

static struct {
    s32 kind;
    s32 count;
    s32 cursor;
    s32 located;
    s32 digits[5];
    s16 pos[5];  // each digit's place in msgBufDecoded
    s16 slot[5]; // its glyph's slot in the font's charTexBuf
    s16 colPos[5];
    s32 heldX;
    s32 repeatY;
} sInput;

static u8 sBomberCode[5];
static u8 sLotteryCodes[3][3]; // MM's saveInfo.lotteryCodes: each day's winning numbers, made with a new file
static u16 sLotteryGuess;      // MM's HS_GET_LOTTERY_CODE_GUESS: the numbers Link picked, 0xABC

s32 NvMmMsgInput_Digit(s32 i);

u8* NvMmMsg_LotteryCodes(void) {
    return &sLotteryCodes[0][0];
}
u16* NvMmMsg_LotteryGuess(void) {
    return &sLotteryGuess;
}

/** MM's Sram_GenerateRandomSaveFields' lottery part: three digits 0-9 for each day. */
void NvMmMsg_NewLottery(void) {
    s32 d;
    s32 i;

    for (d = 0; d < 3; d++) {
        for (i = 0; i < 3; i++) {
            sLotteryCodes[d][i] = CLAMP((s32)Rand_ZeroFloat(10.0f), 0, 9);
        }
    }
    sLotteryGuess = 0;
}

/** The day's numbers against the guess (MM's CHECK_LOTTERY_NUMBERS). */
s32 NvMmMsg_LotteryWon(void) {
    s32 day = CLAMP(NvMmClock_Day(), 1, 3) - 1;

    return (sLotteryCodes[day][0] == ((sLotteryGuess >> 8) & 0xF)) &&
           (sLotteryCodes[day][1] == ((sLotteryGuess >> 4) & 0xF)) && (sLotteryCodes[day][2] == (sLotteryGuess & 0xF));
}

/** MM's Inventory_SaveLotteryCodeGuess: the digits entered. */
void NvMmMsg_SaveLotteryGuess(void) {
    sLotteryGuess = (NvMmMsgInput_Digit(0) << 8) | (NvMmMsgInput_Digit(1) << 4) | NvMmMsgInput_Digit(2);
}
static s32 sRupeesSelected;
static s32 sRupeesTotal;
static s32 sPrice;

/** MM's bomberCode (Sram_GenerateRandomSaveFields: five different digits 1-5, made with a new file). */
u8* NvMmMsg_BomberCode(void) {
    return sBomberCode;
}

void NvMmMsg_NewBomberCode(void) {
    s32 i = 0;
    const char* dev = getenv("NV_MM_BOMBERCODE"); // (dev: a known code, e.g. "12345")

    if ((dev != NULL) && (strlen(dev) == 5)) {
        for (i = 0; i < 5; i++) {
            sBomberCode[i] = dev[i] - '0';
        }
        return;
    }

    while (i < 5) {
        s32 d = 1 + (s32)Rand_ZeroFloat(5.0f);
        s32 j;

        if (d > 5) {
            d = 5;
        }
        for (j = 0; j < i; j++) {
            if (sBomberCode[j] == d) {
                break;
            }
        }
        if (j == i) {
            sBomberCode[i++] = d;
        }
    }
}

void NvMmMsg_SetRupeesSelected(s32 n) {
    sRupeesSelected = n;
}
s32 NvMmMsg_RupeesSelected(void) {
    return sRupeesSelected;
}
void NvMmMsg_SetRupeesTotal(s32 n) {
    sRupeesTotal = n;
}
void NvMmMsg_SetPrice(s32 n) {
    sPrice = n;
}

s32 NvMmMsgInput_Digit(s32 i) {
    return ((i >= 0) && (i < 5)) ? sInput.digits[i] : 0;
}

/** The digits as one number (the bank's amount, the bet). */
s32 NvMmMsgInput_Value(void) {
    s32 v = 0;
    s32 i;

    for (i = 0; i < sInput.count; i++) {
        v = v * 10 + sInput.digits[i];
    }
    return v;
}

static s32 NvMmMsg_Put(u8* out, s32 n, s32 max, u8 c) {
    if (n < max) {
        out[n] = c;
    }
    return n + 1;
}

static s32 NvMmMsg_PutStr(u8* out, s32 n, s32 max, const char* s) {
    while (*s != '\0') {
        n = NvMmMsg_Put(out, n, max, (u8)*s++);
    }
    return n;
}

/** A number, without leading zeros past the last `minDigits`. */
static s32 NvMmMsg_PutNum(u8* out, s32 n, s32 max, s32 v, s32 minDigits) {
    char buf[16];

    snprintf(buf, sizeof(buf), "%0*d", minDigits, (int)v);
    return NvMmMsg_PutStr(out, n, max, buf);
}

static s32 NvMmMsg_PutRupees(u8* out, s32 n, s32 max, s32 v) { // (MM's Message_LoadRupeesNES)
    return NvMmMsg_PutStr(out, n, max, (v == 1) ? " Rupee" : " Rupees");
}

static s32 NvMmMsg_PutClock(u8* out, s32 n, s32 max, s32 minutes) { // HH:MM
    n = NvMmMsg_PutNum(out, n, max, minutes / 60, 2);
    n = NvMmMsg_Put(out, n, max, ':');
    return NvMmMsg_PutNum(out, n, max, minutes % 60, 2);
}

/** MM's minutes until the moon falls (the dawn of day 4) or until the next dawn. */
static s32 NvMmMsg_MinutesUntil(s32 moon) {
    s32 now = (NvMmClock_Time() * 24 * 60) >> 16; // minutes since midnight
    s32 sinceDawn = (now - 6 * 60 + 24 * 60) % (24 * 60);
    s32 day = NvMmClock_Day(); // (MM's day turns at dawn: after midnight it's still the day's night)

    if (moon) {
        return (3 - day) * 24 * 60 + (24 * 60 - sinceDawn);
    }
    return 24 * 60 - sinceDawn;
}

static void NvMmMsg_BeginInput(s32 kind, s32 count, s32 start, s32 cursor) {
    s32 i;

    memset(&sInput, 0, sizeof(sInput));
    sInput.kind = kind;
    sInput.count = count;
    sInput.cursor = cursor;
    for (i = 0; i < count; i++) {
        sInput.digits[i] = start;
    }
}

static s32 NvMmMsg_PutInput(u8* out, s32 n, s32 max) {
    s32 i;

    for (i = 0; i < sInput.count; i++) {
        n = NvMmMsg_Put(out, n, max, 0x05); // (OoT's colour code: each digit its own)
        n = NvMmMsg_Put(out, n, max, (i == sInput.cursor) ? COL_SELECTED : COL_DIGIT);
        n = NvMmMsg_Put(out, n, max, '0' + sInput.digits[i]);
    }
    n = NvMmMsg_Put(out, n, max, 0x05);
    return NvMmMsg_Put(out, n, max, 0x40);
}

/**
 * MM's value code `code` as OoT text, in `out` (at most `max` bytes). Returns the length; *isInput
 * set for an input (the message then waits, ending in OoT's event).
 */
s32 NvMmMsg_Expand(u8 code, u8* out, s32 max, s32* isInput) {
    s32 n = 0;
    s32 i;

    *isInput = false;
    switch (code) {
        case 0xCC: // MESSAGE_INPUT_BANK
            NvMmMsg_BeginInput(NVMM_INPUT_BANK, 3, 0, 2);
            sRupeesSelected = 0;
            *isInput = true;
            n = NvMmMsg_PutInput(out, n, max);
            n = NvMmMsg_PutRupees(out, n, max, 2);
            break;
        case 0xD0: // MESSAGE_INPUT_DOGGY_RACETRACK_BET
            NvMmMsg_BeginInput(NVMM_INPUT_DOGGY, 2, 0, 0);
            sRupeesSelected = 0;
            *isInput = true;
            n = NvMmMsg_PutInput(out, n, max);
            break;
        case 0xD1: // MESSAGE_INPUT_BOMBER_CODE
            NvMmMsg_BeginInput(NVMM_INPUT_BOMBER, 5, 1, 0);
            if (getenv("NV_LOG") != NULL) {
                printf("NVMM bombers' code %d%d%d%d%d\n", sBomberCode[0], sBomberCode[1], sBomberCode[2],
                       sBomberCode[3], sBomberCode[4]);
                fflush(stdout);
            }
            *isInput = true;
            n = NvMmMsg_PutInput(out, n, max);
            break;
        case 0xD5: // MESSAGE_INPUT_LOTTERY_CODE
            NvMmMsg_BeginInput(NVMM_INPUT_LOTTERY, 3, 1, 0);
            *isInput = true;
            n = NvMmMsg_PutInput(out, n, max);
            break;
        case 0xCD: // MESSAGE_RUPEES_SELECTED
            n = NvMmMsg_PutNum(out, n, max, sRupeesSelected, 1);
            n = NvMmMsg_PutRupees(out, n, max, sRupeesSelected);
            break;
        case 0xCE: // MESSAGE_RUPEES_TOTAL
            n = NvMmMsg_PutNum(out, n, max, sRupeesTotal, 1);
            n = NvMmMsg_PutRupees(out, n, max, sRupeesTotal);
            break;
        case 0xDE: // MESSAGE_HELD_ITEM_PRICE
            n = NvMmMsg_PutNum(out, n, max, sPrice, 1);
            n = NvMmMsg_PutRupees(out, n, max, sPrice);
            break;
        case 0xDF: // MESSAGE_BOMBER_CODE
            for (i = 0; i < 5; i++) {
                n = NvMmMsg_Put(out, n, max, '0' + sBomberCode[i]);
            }
            break;
        case 0xDC: { // MESSAGE_LOTTERY_CODE: today's winning numbers
            s32 day = CLAMP(NvMmClock_Day(), 1, 3) - 1;

            for (i = 0; i < 3; i++) {
                n = NvMmMsg_Put(out, n, max, '0' + sLotteryCodes[day][i]);
            }
            break;
        }
        case 0xDD: // MESSAGE_LOTTERY_CODE_GUESS: Link's
            n = NvMmMsg_Put(out, n, max, '0' + ((sLotteryGuess >> 8) & 0xF));
            n = NvMmMsg_Put(out, n, max, '0' + ((sLotteryGuess >> 4) & 0xF));
            n = NvMmMsg_Put(out, n, max, '0' + (sLotteryGuess & 0xF));
            break;
        case 0xCA: // MESSAGE_TIME
            n = NvMmMsg_PutClock(out, n, max, (NvMmClock_Time() * 24 * 60) >> 16);
            break;
        case 0xCF: // MESSAGE_TIME_UNTIL_MOON_CRASH
            n = NvMmMsg_PutClock(out, n, max, NvMmMsg_MinutesUntil(true));
            break;
        case 0xE8: // MESSAGE_TIME_UNTIL_NEW_DAY
            n = NvMmMsg_PutClock(out, n, max, NvMmMsg_MinutesUntil(false));
            break;
        case 0xE7: { // MESSAGE_HOURS_UNTIL_MOON_CRASH
            s32 h = NvMmMsg_MinutesUntil(true) / 60;

            n = NvMmMsg_PutNum(out, n, max, h, 1);
            n = NvMmMsg_PutStr(out, n, max, (h == 1) ? " hour" : " hours");
            break;
        }
        default: // (the rest: not yet; shown as nothing, as before)
            break;
    }
    return (n < max) ? n : max;
}

/** Each digit's place in the decoded text, and its glyph slot: walked as OoT's Message_Decode loads them. */
static s32 NvMmMsgInput_Locate(MessageContext* msgCtx) {
    s32 found = 0;
    s32 slot = 0;
    s32 i;

    for (i = 0; (i < msgCtx->decodedTextLen) && (found < sInput.count); i++) {
        u8 c = msgCtx->msgBufDecoded[i];

        switch (c) {
            case 0x05: // colour: its argument; ours precede the digits
                if (((msgCtx->msgBufDecoded[i + 1] == COL_DIGIT) || (msgCtx->msgBufDecoded[i + 1] == COL_SELECTED)) &&
                    (msgCtx->msgBufDecoded[i + 2] >= '0') && (msgCtx->msgBufDecoded[i + 2] <= '9') &&
                    ((found > 0) || (msgCtx->msgBufDecoded[i + 3] == 0x05))) {
                    sInput.colPos[found] = i + 1;
                    sInput.pos[found] = i + 2;
                    sInput.slot[found] = slot;
                    found++;
                    slot += FONT_CHAR_TEX_SIZE;
                    i += 2;
                } else {
                    i++;
                }
                break;
            case 0x06: // shift, text speed, fade, delayed break: one argument
            case 0x14:
            case 0x0E:
            case 0x0C:
                i++;
                break;
            case 0x07: // next text id, fade2, sound: two
            case 0x11:
            case 0x12:
                i += 2;
                break;
            case 0x01: // newline, quick text on and off, persistent, event, choices, unskippable: none,
            case 0x08: // no glyph
            case 0x09:
            case 0x0A:
            case 0x0B:
            case 0x0D:
            case 0x10:
            case 0x1A:
            case 0x1B:
            case 0x1C:
            case ' ':
                break;
            default:
                if (c >= ' ') {
                    slot += FONT_CHAR_TEX_SIZE;
                }
                break;
        }
    }
    return found == sInput.count;
}

static void NvMmMsgInput_Show(MessageContext* msgCtx, s32 i) {
    u8 ch = '0' + sInput.digits[i];

    msgCtx->msgBufDecoded[sInput.pos[i]] = ch;
    Font_LoadChar(&msgCtx->font, ch - ' ', sInput.slot[i]);
    msgCtx->msgBufDecoded[sInput.colPos[i]] = (i == sInput.cursor) ? COL_SELECTED : COL_DIGIT;
}

/** MM's Message_ControlBankInput and the others: each frame the input's box waits. */
void NvMmMsgInput_Update(PlayState* play) {
    MessageContext* msgCtx = &play->msgCtx;
    Input* input = &play->state.input[0];
    s32 x;
    s32 y;
    s32 lo;
    s32 hi;
    s32 i;

    if (sInput.kind == NVMM_INPUT_NONE) {
        return;
    }
    if ((msgCtx->msgMode == MSGMODE_NONE) || (msgCtx->msgMode != MSGMODE_TEXT_DONE) ||
        (Message_GetState(msgCtx) != TEXT_STATE_EVENT)) {
        if (msgCtx->msgMode == MSGMODE_NONE) {
            sInput.located = false;
        }
        return;
    }
    if (!sInput.located) {
        if (!NvMmMsgInput_Locate(msgCtx)) {
            return;
        }
        sInput.located = true;
    }
    x = input->rel.stick_x;
    y = input->rel.stick_y;
    if (CHECK_BTN_ALL(input->cur.button, BTN_DLEFT)) {
        x = -60;
    } else if (CHECK_BTN_ALL(input->cur.button, BTN_DRIGHT)) {
        x = 60;
    }
    if (CHECK_BTN_ALL(input->cur.button, BTN_DDOWN)) {
        y = -60;
    } else if (CHECK_BTN_ALL(input->cur.button, BTN_DUP)) {
        y = 60;
    }
    lo = (sInput.kind == NVMM_INPUT_BOMBER) ? 1 : 0;
    hi = (sInput.kind == NVMM_INPUT_BOMBER) ? 5 : 9;
    // up and down: MM's stickAdjY, repeating while held
    if ((y >= 30) || (y <= -30)) {
        if ((sInput.repeatY == 0) || ((sInput.repeatY > 10) && ((sInput.repeatY % 4) == 0))) {
            i = sInput.cursor;
            sInput.digits[i] += (y > 0) ? 1 : -1;
            if (sInput.digits[i] > hi) {
                sInput.digits[i] = lo;
            } else if (sInput.digits[i] < lo) {
                sInput.digits[i] = hi;
            }
            NvMmMsgInput_Show(msgCtx, i);
            Audio_PlaySfxGeneral(NA_SE_SY_RUPY_COUNT, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                   &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }
        sInput.repeatY++;
    } else {
        sInput.repeatY = 0;
        // left and right: once per push (the dog race's bet has none: MM's moves only its tens)
        if (((x >= 30) || (x <= -30)) && !sInput.heldX && (sInput.kind != NVMM_INPUT_DOGGY)) {
            s32 old = sInput.cursor;

            sInput.heldX = true;
            sInput.cursor += (x > 0) ? 1 : -1;
            sInput.cursor = CLAMP(sInput.cursor, 0, sInput.count - 1);
            if (sInput.cursor != old) {
                NvMmMsgInput_Show(msgCtx, old);
                NvMmMsgInput_Show(msgCtx, sInput.cursor);
                Audio_PlaySfxGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            }
        } else if ((x < 30) && (x > -30)) {
            sInput.heldX = false;
        }
    }
    if (sInput.kind == NVMM_INPUT_BANK) {
        sRupeesSelected = NvMmMsgInput_Value();
    } else if (sInput.kind == NVMM_INPUT_DOGGY) {
        sRupeesSelected = sInput.digits[0] * 10;
    }
}
