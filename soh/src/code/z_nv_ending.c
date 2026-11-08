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
 * both survivors, else Jill's own, 25) and, after the ones with the staff roll, 22.
 *
 * Then the results (ending_result_update): on the endings without the staff roll Jill's backdrop,
 * RE1's plates "TOTAL TIME" and "NUMBER OF SAVES" fading in a pair at a time with the time and the
 * count, held until a button (or 76 seconds), faded out; after ending 7 the special key's epilogue
 * (its picture and two plates, held nine seconds). The pictures are the player's own (re1/ui/result:
 * soh_ui.result_blob). RE1 then saves the cleared game and goes to its title; here, for now, Link
 * is put back in Hyrule at Kakariko's front gate (z_nv_save.c NvSave_Home). The time is the run's
 * RE1 ticks; the saves, the typewriter's (z_nv_save.c).
 */
#include "global.h"
#include "soh/OTRGlobals.h"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

s32 NvFlags_Test(s32 bank, s32 bit);
void NvFlags_Op(s32 bank, s32 bit, s32 mode);
u16* NvScript_StateWord(s32 i);
void NvSave_Finished(void);
s16 NvSave_Home(void);
void NvMovie_Start(s32 id);
s32 NvMovie_Playing(void);

#define NV_MSF_PLAYER_DEAD 0x07       // bank 5 (g_main_state_flags: 0x01000000)
#define NV_MSF2_DEATH_VARIANT 0x20    // (g_main_state_flags2: 0x80000000)
#define NV_MSF2_COUNTDOWN_ACTIVE 0x24 // (0x08000000)
#define NV_COUNTDOWN_END 180

static s32 sNvCountTicks; // DAT_004d2294: ticks into the second

typedef enum { NV_END_NONE, NV_END_BLAST, NV_END_MOVIES, NV_END_RESULT, NV_END_EPILOGUE, NV_END_LEAVE, NV_END_GONE } NvEndState;
static s32 sNvEnd = NV_END_NONE;
static s8 sNvEndMovies[5];
static s32 sNvEndMovie;
static s32 sNvEndCount;
static s32 sNvEndId;
static u32 sNvRunTicks; // the run's clock (Game_timer: 30 a second)
static u16 sNvRunSaves; // the times saved at a typewriter (the results' count)

/** The run's clock and saves, for the typewriter's save (z_nv_save.c). */
u32 NvEnding_SaveState(u8* p) {
    if (p != NULL) {
        memcpy(p, &sNvRunTicks, 4);
        memcpy(p + 4, &sNvRunSaves, 2);
    }
    return 6;
}

void NvEnding_LoadState(const u8* p) {
    memcpy(&sNvRunTicks, p, 4);
    memcpy(&sNvRunSaves, p + 4, 2);
}

void NvEnding_NewRun(void) {
    sNvRunTicks = 0;
    sNvRunSaves = 0;
}

/** A typewriter save: one more for the results. */
void NvEnding_CountSave(void) {
    sNvRunSaves++;
}
static s32 sNvTickPhase;

/* the results' pictures: 0 Jill's backdrop, 1 en05's plates, 2 en07's, 3 the epilogue's picture,
   4 RE1's 8x14 font (fontus.tim) */
#define NV_RES_PICS 5
static const u8* sNvResPic[NV_RES_PICS];
static u16 sNvResW[NV_RES_PICS];
static u16 sNvResH[NV_RES_PICS];

typedef struct {
    s16 x;
    s16 y;
    u8 kind; // 2 a plate, 3 the time, 5 the saves
    u8 v;    // a plate's rows in the sheet
    u8 h;
} NvResSlot;

// s_creditEntries: 0-4 on the backdrop, 7-11 the plain layout, 5-6 the epilogue's plates
static const NvResSlot sNvResOnBackdrop[5] = {
    { 0x00, 0x30, 2, 0x00, 0x18 }, { 0x30, 0x50, 3, 0, 0 }, { 0x00, 0x74, 2, 0x18, 0x18 },
    { 0x48, 0x94, 5, 0, 0 },       { 0x25, 0x66, 2, 0x30, 0x28 },
};
static const NvResSlot sNvResPlain[5] = {
    { 0x20, 0x16, 2, 0x00, 0x36 }, { 0x80, 0x58, 3, 0, 0 }, { 0x20, 0x7A, 2, 0x36, 0x36 },
    { 0x98, 0xB8, 5, 0, 0 },       { 0, 0, 0, 0, 0 },
};
static const NvResSlot sNvResEpilogue[2] = { { 0x20, 0x20, 2, 0x58, 0x30 }, { 0x20, 0xC0, 2, 0x88, 0x20 } };

static s32 sNvResStep;
static s32 sNvResTimer;
static s32 sNvResFade[6]; // 0-4 the slots, 5 the backdrop (0..0x8000)
static s32 sNvResVel[6];
static s32 sNvResOn[6];

/** game_loop's countdown, one RE1 tick (z_nv_script.c's tick loop). */
void NvCountdown_Tick(PlayState* play) {
    u16* w = NvScript_StateWord(4);

    sNvRunTicks++;
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
    sNvEndId = id;
    NvSave_Finished(); // (the run's done: next time in is a new one)
    sNvEnd = NV_END_MOVIES;
    printf("NVEND ending %d\n", (int)id);
    fflush(stdout);
}

static s32 NvResult_Backdrop(void) { // s_endingTable's background: the endings without the staff roll
    return sNvEndId >= 4;
}

static const NvResSlot* NvResult_Slots(void) {
    return NvResult_Backdrop() ? sNvResOnBackdrop : sNvResPlain;
}

static void NvResult_Load(void) {
    const u8* d = NULL;
    s32 i;
    s32 off;

    memset(sNvResPic, 0, sizeof(sNvResPic));
    if (ResourceMgr_FileExists("re1/ui/result")) {
        d = (const u8*)ResourceGetDataByName("re1/ui/result");
    }
    if ((d == NULL) || (memcmp(d, "RRES", 4) != 0)) {
        return;
    }
    off = 6;
    for (i = 0; i < MIN(NV_RES_PICS, d[4] | (d[5] << 8)); i++) {
        memcpy(&sNvResW[i], d + off, 2);
        memcpy(&sNvResH[i], d + off + 2, 2);
        sNvResPic[i] = d + off + 4;
        off += 4 + sNvResW[i] * sNvResH[i] * 2;
    }
}

static void NvResult_Show(s32 i, s32 vel) {
    sNvResOn[i] = true;
    sNvResVel[i] = vel;
}

/** One RE1 tick of ending_result_update; true when it's done. */
static s32 NvResult_Tick(Input* input) {
    s32 i;

    for (i = 0; i < 6; i++) { // ending_result_draw's ramps
        sNvResFade[i] = CLAMP(sNvResFade[i] + sNvResVel[i], 0, 0x8000);
        if ((sNvResFade[i] == 0) || (sNvResFade[i] == 0x8000)) {
            sNvResVel[i] = 0;
        }
    }
    switch (sNvResStep) {
        case 0:
            memset(sNvResFade, 0, sizeof(sNvResFade));
            memset(sNvResVel, 0, sizeof(sNvResVel));
            memset(sNvResOn, 0, sizeof(sNvResOn));
            NvResult_Show(5, 0x800);
            sNvResTimer = 0;
            sNvResStep = 1;
            break;
        case 1:
        case 2: // the plates a pair at a time
            if (++sNvResTimer > 0x3C) {
                sNvResTimer = 0;
                NvResult_Show((sNvResStep - 1) * 2, 0x800);
                NvResult_Show((sNvResStep - 1) * 2 + 1, 0x800);
                sNvResStep++;
            }
            break;
        case 3:
            if (++sNvResTimer > 0x5A) {
                sNvResTimer = 0;
                sNvResStep = 4;
            }
            break;
        case 4: // held until a button or ~76 seconds
            if ((++sNvResTimer > 0x708) || ((getenv("NV_END_AUTO") != NULL) && (sNvResTimer > 60)) ||
                CHECK_BTN_ANY(input->press.button, BTN_A | BTN_B | BTN_START | BTN_CRIGHT | BTN_CLEFT)) {
                for (i = 0; i < 6; i++) {
                    sNvResVel[i] = -0x800;
                }
                sNvResTimer = 0;
                sNvResStep = 10;
            }
            break;
        case 10:
            if (++sNvResTimer > 0x1E) {
                return true;
            }
            break;
    }
    return false;
}

/** ending_epilogue_update: the special key's picture and plates, nine seconds, faded out. */
static s32 NvEpilogue_Tick(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        sNvResFade[i] = CLAMP(sNvResFade[i] + sNvResVel[i], 0, 0x8000);
    }
    if (++sNvResTimer == 0x10D) {
        for (i = 0; i < 6; i++) {
            sNvResVel[i] = -0x800;
        }
    }
    return sNvResTimer > 0x10D + 0x14;
}

/** Each frame from Play_Update, ahead of the movies (it runs between them); true while it holds the game. */
s32 NvEnding_Update(PlayState* play) {
    Input* input = &play->state.input[0];
    s32 ticks;
    s32 t;

    if (play->sceneNum < SCENE_RE1_100) {
        if (sNvEnd == NV_END_GONE) { // (back in Hyrule)
            sNvEnd = NV_END_NONE;
        }
        return false;
    }
    ticks = (sNvTickPhase ^= 1) ? 1 : 2; // (RE1's 30 a second at OoT's 20)
    switch (sNvEnd) {
        case NV_END_NONE:
            if (NvFlags_Test(5, NV_MSF_PLAYER_DEAD) && NvFlags_Test(5, NV_MSF2_DEATH_VARIANT)) {
                NvFlags_Op(5, NV_MSF_PLAYER_DEAD, 1);
                NvFlags_Op(5, NV_MSF2_DEATH_VARIANT, 1);
                NvEnding_Begin();
                return true;
            }
            return false;
        case NV_END_BLAST: // the explosion over: she's dead (OoT's game over)
            if (!NvMovie_Playing()) {
                gSaveContext.health = 0;
                sNvEnd = NV_END_NONE;
            }
            return false;
        case NV_END_MOVIES:
            if (NvMovie_Playing()) {
                return true;
            }
            if (sNvEndMovies[sNvEndMovie] < 0) {
                NvResult_Load();
                sNvResStep = 0;
                sNvEnd = NV_END_RESULT;
                printf("NVEND results: %u ticks\n", (unsigned)sNvRunTicks);
                fflush(stdout);
                return true;
            }
            if (getenv("NV_END_NO_MOVIES") == NULL) { // dev: straight to the results
                NvMovie_Start(sNvEndMovies[sNvEndMovie]);
            }
            sNvEndMovie++;
            return true;
        case NV_END_RESULT:
            for (t = 0; t < ticks; t++) {
                if (NvResult_Tick(input)) {
                    if (sNvEndId == 7) { // the special key's epilogue
                        memset(sNvResOn, 0, sizeof(sNvResOn));
                        sNvResOn[0] = sNvResOn[1] = sNvResOn[5] = true;
                        sNvResFade[0] = sNvResFade[1] = sNvResFade[5] = 0x8000;
                        memset(sNvResVel, 0, sizeof(sNvResVel));
                        sNvResTimer = 0;
                        sNvEnd = NV_END_EPILOGUE;
                        printf("NVEND epilogue\n");
                    } else {
                        sNvEnd = NV_END_LEAVE;
                    }
                    fflush(stdout);
                    break;
                }
            }
            return true;
        case NV_END_EPILOGUE:
            for (t = 0; t < ticks; t++) {
                if (NvEpilogue_Tick()) {
                    sNvEnd = NV_END_LEAVE;
                    break;
                }
            }
            return true;
        case NV_END_GONE: // (black until the scene changes)
            return false;
        case NV_END_LEAVE: // for now: back to Hyrule, at Kakariko
            sNvEnd = NV_END_GONE;
            printf("NVEND back to Hyrule\n");
            fflush(stdout);
            gSaveContext.health = MAX(gSaveContext.health, 0x30);
            play->nextEntranceIndex = NvSave_Home();
            play->transitionTrigger = TRANS_TRIGGER_START;
            play->transitionType = TRANS_TYPE_FADE_BLACK;
            gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
            return false; // (Play_Update carries the transition out)
    }
    return false;
}

/** A picture's rows y0..y0+h at (x, y), tinted (shade 0..255) and see-through by its alpha. */
static Gfx* NvResult_Pic(Gfx* gfx, s32 pic, s32 v, s32 h, s32 x, s32 y, s32 shade) {
    const u16* px = (const u16*)sNvResPic[pic];
    s32 w = sNvResW[pic];
    s32 rows = 4096 / (w * 2);
    s32 r;

    shade = MIN(shade, 255);
    if ((px == NULL) || (shade <= 0)) {
        return gfx;
    }
    gDPSetPrimColor(gfx++, 0, 0, shade, shade, shade, 255);
    for (r = 0; r < h; r += rows) {
        s32 n = MIN(rows, h - r);

        gDPLoadTextureBlock(gfx++, px + (v + r) * w, G_IM_FMT_RGBA, G_IM_SIZ_16b, w, n, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                            G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gSPTextureRectangle(gfx++, x << 2, (y + r) << 2, (x + w) << 2, (y + r + n) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10,
                            1 << 10);
    }
    return gfx;
}

/** RE1's PrintText8x14: ASCII in fontus.tim's 8x14 cells (column ch % 18, row ch / 18). */
static Gfx* NvResult_Text(Gfx* gfx, s32 x, s32 y, const char* str, s32 shade) {
    const u16* font = (const u16*)sNvResPic[4];

    shade = MIN(shade, 255);
    if ((font == NULL) || (shade <= 0)) {
        return gfx;
    }
    gDPSetPrimColor(gfx++, 0, 0, shade, shade, shade, 255);
    for (; *str != '\0'; str++, x += 8) {
        u8 ch = (u8)*str;
        s32 u = (ch % 18) * 8;
        s32 v = (ch / 18) * 14;

        if (ch == ' ') {
            continue;
        }
        gDPLoadTextureTile(gfx++, font, G_IM_FMT_RGBA, G_IM_SIZ_16b, sNvResW[4], sNvResH[4], u, v, u + 7, v + 13, 0,
                           G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                           G_TX_NOLOD, G_TX_NOLOD);
        gSPTextureRectangle(gfx++, x << 2, y << 2, (x + 8) << 2, (y + 14) << 2, G_TX_RENDERTILE, u << 5, v << 5,
                            1 << 10, 1 << 10);
    }
    return gfx;
}

/** The results over everything (z_play.c, with the movies). */
void NvEnding_Draw(PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    const NvResSlot* slots;
    s32 sheet;
    s32 n;
    s32 i;
    s32 dx;
    Gfx* gfx;

    if ((sNvEnd != NV_END_RESULT) && (sNvEnd != NV_END_EPILOGUE) && (sNvEnd != NV_END_LEAVE) &&
        ((sNvEnd != NV_END_GONE) || (play->sceneNum < SCENE_RE1_100))) {
        return;
    }
    OPEN_DISPS(gfxCtx);
    gfx = OVERLAY_DISP;
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetFillColor(gfx++, GPACK_RGBA5551(0, 0, 0, 1) << 16 | GPACK_RGBA5551(0, 0, 0, 1));
    gDPFillWideRectangle(gfx++, OTRGetRectDimensionFromLeftEdge(0), 0, OTRGetRectDimensionFromRightEdge(SCREEN_WIDTH),
                         SCREEN_HEIGHT);
    gDPPipeSync(gfx++);
    if ((sNvEnd != NV_END_LEAVE) && (sNvEnd != NV_END_GONE)) {
        gDPSetCycleType(gfx++, G_CYC_1CYCLE);
        gDPSetTexturePersp(gfx++, G_TP_NONE);
        gDPSetTextureFilter(gfx++, G_TF_POINT);
        gDPSetTextureLUT(gfx++, G_TT_NONE);
        gDPSetCombineLERP(gfx++, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0,
                          PRIMITIVE, 0);
        gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
        sheet = NvResult_Backdrop() ? 2 : 1;
        if (sNvEnd == NV_END_EPILOGUE) {
            gfx = NvResult_Pic(gfx, 3, 0, 240, 0, 0, sNvResFade[5] >> 7);
            slots = sNvResEpilogue;
            n = 2;
            dx = 0;
        } else {
            if (NvResult_Backdrop()) {
                gfx = NvResult_Pic(gfx, 0, 0, 240, 0, 0, sNvResFade[5] >> 7);
            }
            slots = NvResult_Slots();
            n = 4; // (the fifth plate is Chris's ending 6's)
            dx = NvResult_Backdrop() ? 0x9C : 0; // (Jill's layout: a plate width right, off her picture)
        }
        for (i = 0; i < n; i++) {
            if (sNvResOn[i] && (slots[i].kind == 2)) {
                gfx = NvResult_Pic(gfx, sheet, slots[i].v, slots[i].h, slots[i].x + dx,
                                   slots[i].y, sNvResFade[i] >> 7);
            }
        }
        gDPPipeSync(gfx++);
        if (sNvEnd == NV_END_RESULT) { // the time and the count, in RE1's font (GfxPrint's without it)
            u32 t = sNvRunTicks;
            char line[16];

            for (i = 0; i < n; i++) {
                s32 shade = MIN(sNvResFade[i] >> 7, 255);

                if (!sNvResOn[i] || (shade <= 0) || ((slots[i].kind != 3) && (slots[i].kind != 5))) {
                    continue;
                }
                if (slots[i].kind == 3) { // "%02d:%02d%c%02d": hours, minutes, RE1's separator (0x5E), seconds
                    snprintf(line, sizeof(line), "%02d:%02d%c%02d", (int)(t / 108000), (int)((t % 108000) / 1800),
                             0x5E, (int)((t % 1800) / 30));
                } else {
                    snprintf(line, sizeof(line), "%02d", (int)MIN(sNvRunSaves, 99));
                }
                if (sNvResPic[4] != NULL) {
                    gfx = NvResult_Text(gfx, slots[i].x + dx, slots[i].y, line, shade);
                } else {
                    GfxPrint printer;

                    GfxPrint_Init(&printer);
                    GfxPrint_Open(&printer, gfx);
                    GfxPrint_SetColor(&printer, shade, shade, shade, 255);
                    GfxPrint_SetPosPx(&printer, slots[i].x + dx, slots[i].y);
                    GfxPrint_Printf(&printer, "%s", line);
                    gfx = GfxPrint_Close(&printer);
                    GfxPrint_Destroy(&printer);
                }
            }
        }
    }
    OVERLAY_DISP = gfx;
    CLOSE_DISPS(gfxCtx);
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
