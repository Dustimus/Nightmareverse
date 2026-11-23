/*
 * Nightmareverse: Majora's Mask's songs on Link's ocarina, registered with the shared song listener
 * (z_nv_songs.c, RE1's side's; NvSongs_Init calls NvSongs_RegisterMm after RE1's own).
 *
 * The Inverted Song of Time (MM's notes: C-Down, A, C-Right, C-Down, A, C-Right): in Termina, once
 * played, the ocarina is put away and time's speed list opens (z_nv_mm_clock.c: a third, half,
 * normal, double, triple; the user's list, 2026-11-23). It borrows no OoT song slot: OoT's
 * "correct" chime, then the list. Outside Termina it isn't a song (the clock runs only there).
 * The Song of Double Time is left for its 3DS behaviour later (mm/README.md "Decisions").
 */
#include "global.h"
#include <stdio.h>
#include <stdlib.h>
#include "z_nv_songs.h"

void NvMmClock_OpenSpeedMenu(PlayState* play);

static const u8 sInvertedSongOfTime[] = { OCARINA_BTN_C_DOWN, OCARINA_BTN_A, OCARINA_BTN_C_RIGHT,
                                          OCARINA_BTN_C_DOWN, OCARINA_BTN_A, OCARINA_BTN_C_RIGHT };
static s32 sInvertedPending; // played: the list opens once the ocarina is put away
static s32 sInvertedClosing; // the ocarina's being put away

static s32 NvMmSongs_InTermina(PlayState* play) {
    return NvMm_IsScene(play->sceneNum);
}

static void NvMmSongs_Inverted(PlayState* play) {
    Sfx_PlaySfxCentered(NA_SE_SY_CORRECT_CHIME);
    sInvertedPending = true;
    if (getenv("NV_LOG") != NULL) {
        printf("NVSONG the Inverted Song of Time\n");
        fflush(stdout);
    }
}

static const NvSongDef sInvertedDef = {
    "the Inverted Song of Time", sInvertedSongOfTime, ARRAY_COUNT(sInvertedSongOfTime), -1,
    NvMmSongs_InTermina,         NvMmSongs_Inverted,  NULL,                              NULL,
};

void NvSongs_RegisterMm(void) {
    NvSongs_Register(&sInvertedDef);
}

/** Every frame in Termina (z_nv_mm.c): a played Inverted Song of Time puts the ocarina away and
 * opens the speed list. */
void NvMmSongs_Update(PlayState* play) {
    if (!sInvertedPending) {
        return;
    }
    if (play->msgCtx.msgMode != MSGMODE_NONE) {
        if (!sInvertedClosing) { // the ocarina put away, as B does in free play (z_message_PAL.c)
            sInvertedClosing = true;
            AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_OFF);
            play->msgCtx.ocarinaMode = OCARINA_MODE_04;
            Message_CloseTextbox(play);
        }
        return;
    }
    sInvertedClosing = false;
    sInvertedPending = false;
    NvMmClock_OpenSpeedMenu(play);
}
