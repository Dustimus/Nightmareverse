/*
 * Nightmareverse: the end of RE1's run - the lab's self-destruct countdown and the endings.
 *
 * The countdown (GameLoop.cpp game_loop): state word 4 (g_CountdownTimer) counts seconds, one
 * every 30 RE1 ticks, always (a script that sets it to 0x7FFF restarts it from 0: 501's does,
 * when the lab's self-destruct is armed). While the main state's countdown bit is up (bank 5,
 * 0x24: MSF2_COUNTDOWN_ACTIVE) its time left shows at the top of the screen, "%2d:%02d:%d%d" from
 * three minutes (red in the last one); at 180 seconds the lab goes up: the explosion movie (2) and
 * she dies. The scripts read the word to hurry things on (cmd_state_word_test 4).
 *
 * The endings (EndingScreen.cpp ending_state): game_loop leaves for them when a script marks her
 * "dead" (bank 5 bit 7, MSF_PLAYER_DEAD) with the ending variant (bank 5 bit 0x20,
 * MSF2_DEATH_VARIANT): the helicopter at 303 does. ending_select_id picks the ending from the
 * second scenario bank's 0xC0 (Chris saved) and 0x4B (Barry: the second survivor); then the
 * movies: 14 (the mansion going up), 14 + the ending, the closing one (27 after the endings with
 * both survivors, else Jill's own, 25) and, after the ones with the staff roll, 22. Then the title.
 * RE1's results screen (the clear time, the saves) and its unlocks aren't here yet.
 */
#include "global.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include <stdio.h>
#include <stdlib.h>

s32 NvFlags_Test(s32 bank, s32 bit);
void NvFlags_Op(s32 bank, s32 bit, s32 mode);
u16* NvScript_StateWord(s32 i);
void NvMovie_Start(s32 id);
s32 NvMovie_Playing(void);

#define NV_MSF_PLAYER_DEAD 0x07       // bank 5 (g_main_state_flags: 0x01000000)
#define NV_MSF2_DEATH_VARIANT 0x20    // (g_main_state_flags2: 0x80000000)
#define NV_MSF2_COUNTDOWN_ACTIVE 0x24 // (0x08000000)
#define NV_COUNTDOWN_END 180

static s32 sNvCountTicks; // DAT_004d2294: ticks into the second

typedef enum { NV_END_NONE, NV_END_BLAST, NV_END_MOVIES, NV_END_TITLE } NvEndState;
static s32 sNvEnd = NV_END_NONE;
static s8 sNvEndMovies[5];
static s32 sNvEndMovie;
static s32 sNvEndCount;

/** game_loop's countdown, one RE1 tick (z_nv_script.c's tick loop). */
void NvCountdown_Tick(PlayState* play) {
    u16* w = NvScript_StateWord(4);

    if ((sNvCountTicks > 29) || (*w == 0x7FFF)) {
        sNvCountTicks = 0;
        if (*w < 0x7FFE) {
            if (gSaveContext.health > 0) {
                (*w)++;
            }
        } else {
            *w = 0;
        }
    }
    if (NvFlags_Test(5, NV_MSF2_COUNTDOWN_ACTIVE) && (*w == NV_COUNTDOWN_END) && (sNvEnd == NV_END_NONE)) {
        // the time's up: the lab goes, and she with it
        NvFlags_Op(5, NV_MSF2_COUNTDOWN_ACTIVE, 1);
        NvMovie_Start(2);
        sNvEnd = NV_END_BLAST;
        printf("NVEND the lab went up\n");
        fflush(stdout);
    }
    if (gSaveContext.health > 0) {
        sNvCountTicks++;
    }
}

/** ending_select_id: 2 both saved, 3 Barry too (with Chris), 5 Jill alone, 7 Barry without Chris. */
static s32 NvEnding_Id(void) {
    if (!NvFlags_Test(1, 0xC0)) {
        return NvFlags_Test(1, 0x4B) ? 7 : 5;
    }
    return NvFlags_Test(1, 0x4B) ? 3 : 2;
}

static void NvEnding_Begin(void) {
    s32 id = NvEnding_Id();
    s32 n = 0;

    sNvEndMovies[n++] = 14;
    sNvEndMovies[n++] = 14 + id;
    sNvEndMovies[n++] = (id <= 3) ? 27 : 25;
    if (id <= 3) {
        sNvEndMovies[n++] = 22; // the staff roll
    }
    sNvEndMovies[n] = -1;
    sNvEndMovie = 0;
    sNvEndCount = 0;
    sNvEnd = NV_END_MOVIES;
    printf("NVEND ending %d\n", (int)id);
    fflush(stdout);
}

/** Each frame from Play_Update, ahead of the movies (it runs between them). */
void NvEnding_Update(PlayState* play) {
    if (play->sceneNum < SCENE_RE1_100) {
        return;
    }
    switch (sNvEnd) {
        case NV_END_NONE:
            if (NvFlags_Test(5, NV_MSF_PLAYER_DEAD) && NvFlags_Test(5, NV_MSF2_DEATH_VARIANT)) {
                NvFlags_Op(5, NV_MSF_PLAYER_DEAD, 1);
                NvFlags_Op(5, NV_MSF2_DEATH_VARIANT, 1);
                NvEnding_Begin();
            }
            break;
        case NV_END_BLAST: // the explosion over: she's dead (OoT's game over)
            if (!NvMovie_Playing()) {
                gSaveContext.health = 0;
                sNvEnd = NV_END_NONE;
            }
            break;
        case NV_END_MOVIES:
            if (NvMovie_Playing()) {
                break;
            }
            if (sNvEndMovies[sNvEndMovie] < 0) {
                sNvEnd = NV_END_TITLE;
                break;
            }
            NvMovie_Start(sNvEndMovies[sNvEndMovie++]);
            break;
        case NV_END_TITLE: // ending_state's last step: back to the title
            sNvEnd = NV_END_NONE;
            printf("NVEND title\n");
            fflush(stdout);
            play->state.running = false;
            SET_NEXT_GAMESTATE(&play->state, Opening_Init, OpeningContext);
            GameInteractor_ExecuteOnExitGame(gSaveContext.fileNum);
            break;
    }
}

/** The time left at the top (game_loop's PrintText8x14 at 0x84, 0x20), while the countdown runs. */
void NvCountdown_Draw(PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    u16 w = *NvScript_StateWord(4);
    GfxPrint printer;
    Gfx* gfx;

    if ((play->sceneNum < SCENE_RE1_100) || !NvFlags_Test(5, NV_MSF2_COUNTDOWN_ACTIVE) || (w >= NV_COUNTDOWN_END) ||
        NvMovie_Playing()) {
        return;
    }
    OPEN_DISPS(gfxCtx);
    gfx = OVERLAY_DISP;
    GfxPrint_Init(&printer);
    GfxPrint_Open(&printer, gfx);
    if (w > 0x77) {
        GfxPrint_SetColor(&printer, 255, 60, 40, 255);
    } else {
        GfxPrint_SetColor(&printer, 255, 255, 255, 255);
    }
    GfxPrint_SetPosPx(&printer, 0x84, 0x40); // (RE1: y 0x20, under OoT's buttons here)
    GfxPrint_Printf(&printer, "%2d:%02d:%d%d", 2 - w / 60, 59 - w % 60, MAX(0, 9 - sNvCountTicks / 3),
                    (sNvCountTicks + 1) % 10);
    gfx = GfxPrint_Close(&printer);
    GfxPrint_Destroy(&printer);
    OVERLAY_DISP = gfx;
    CLOSE_DISPS(gfxCtx);
}
