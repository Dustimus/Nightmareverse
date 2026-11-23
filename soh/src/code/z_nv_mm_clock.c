/*
 * Nightmareverse: Termina's 3-day clock (Majora's Mask), running in MM's scenes only.
 *
 * Decided with the user (mm/README.md, NV_GAMES.md): Termina keeps its own day and time.
 * Hyrule's time is put aside on the way in and given back on the way out, so each place's days
 * work its own way and the Song of Time resets only Termina.
 *
 * MM's rules (zeldaret/mm: the save's time and day, R_TIME_SPEED, timeSpeedOffset), our own code:
 * - time is OoT's and MM's shared unit: 0x10000 per day, 0x4000 = 6:00, 0x8000 = noon;
 * - it moves 3 units a frame (R_TIME_SPEED), plus the speed offset (-2 under the Inverted Song of
 *   Time), and stands still while paused, in a message, during a scene change or game over;
 * - the day counts up at 6:00: Day 1 starts at 6:00 on the first day, and 6:00 after the third
 *   night is the moon's fall;
 * - the Song of Time takes Link back to the dawn of the first day, in South Clock Town.
 *
 * Termina's rooms carry no time increment (mm_scene.py), so OoT's own clock doesn't move here.
 * Anything else that moves OoT's clock (the Sun's Song's fast-forward, RE1's Moonlight Sonata
 * setting its hour) moves Termina's forward by as much: the Sun's Song acts as MM's Song of
 * Double Time (to the next 6:00 or 18:00), and the Sonata skips to the hour it sets (midnight
 * today; the user wants 19:01 in Hyrule, an RE1-side change, and maybe midnight in Termina, 2026-11-19).
 *
 * Not yet: MM's clock on the HUD (a text line for now), its saving (nv_mm_<file>.sav), the
 * Song of Double Time's and the Inverted Song of Time's own notes, the final hours' music and
 * the moon's fall itself (for now it's a Song of Time).
 *
 * Dev: NV_MM_TIME="day,hhmm" sets the clock on the first Termina frame; NV_MM_SPEED=n
 * multiplies its speed (headless tests); NV_LOG prints its changes ("NVMM ...").
 */
#include "global.h"
#include <stdio.h>
#include <stdlib.h>

#define MM_DAWN 0x4000      // 6:00
#define MM_TIME_SPEED 3     // R_TIME_SPEED
#define MM_TELOP_FRAMES 60  // "Dawn of the ... Day" on screen
#define MM_ENTRANCE_CLOCK_TOWN 0xF64 // ENTR_MM_Z2_CLOCKTOWER_0: South Clock Town, the clock tower's door

static s32 sMmDay = 1;          // 1-3; 4 for an instant, as the moon falls
static u16 sMmTime = MM_DAWN;
// The clock's speed, in half steps a frame (MM's R_TIME_SPEED, 3, is 6): the user's speed list
// (the Inverted Song of Time's, 2026-11-23): a third (N64's inverted), half (3DS's), normal,
// double, triple. A half step carries to the next frame.
static s16 sMmSpeedHalves = 6;
static s16 sMmHalfCarry;
static s32 sMmFrameStep = MM_TIME_SPEED; // this frame's step, for the people walking by the time
static s32 sMmMenu;                      // the speed list is open
static s32 sMmMenuCursor;
static s8 sMmMenuHeld;
static const struct {
    s16 halves;
    const char* name;
    const char* tag; // on the clock while it's set ("" for normal)
} sMmSpeeds[] = {
    { 2, "Slow time to a third (N64)", "x1/3" }, { 3, "Slow time to half (3DS)", "x1/2" },
    { 6, "Normal speed", "" },                  { 12, "Double speed", "x2" },
    { 18, "Triple speed", "x3" },
};
static s32 sMmInTermina;        // the last frame was in Termina
static u16 sMmHyruleTime;       // Hyrule's time, put aside while in Termina
static u16 sMmWritten;          // the time this clock last gave OoT
static s32 sMmTelop;            // frames left of the day's title
static s32 sMmSongOfTime;       // the Song of Time was played: act when its message closes
static s32 sMmDevSet;           // NV_MM_TIME applied

void NvMmScene_SongOfTime(PlayState* play);
void NvMmScene_NewGame(void);
void NvMmClock_OpenSpeedMenu(PlayState* play);
static void NvMmClock_SpeedMenuInput(PlayState* play);
static s32 sMmMenuDevDone;

static s32 NvMm_Log(void) {
    return getenv("NV_LOG") != NULL;
}

static s32 NvMm_Hour(u16 t) {
    return (t * 24) >> 16;
}

static s32 NvMm_Minute(u16 t) {
    return ((t * 24 * 60) >> 16) % 60;
}

/** Back to the dawn of the first day (the Song of Time; for now, the moon's fall too). */
static void NvMmClock_Reset(PlayState* play, const char* why) {
    sMmDay = 1;
    sMmTime = MM_DAWN;
    sMmSpeedHalves = 6; // (MM: the Song of Time puts the speed back to normal)
    sMmTelop = MM_TELOP_FRAMES;
    if (NvMm_Log()) {
        printf("NVMM reset (%s): day 1 06:00\n", why);
        fflush(stdout);
    }
    NvMmScene_SongOfTime(play); // Termina's chests, switches and cleared rooms (z_nv_mm_scene.c)
    if (play != NULL) {
        play->nextEntranceIndex = MM_ENTRANCE_CLOCK_TOWN;
        play->transitionTrigger = TRANS_TRIGGER_START;
        play->transitionType = TRANS_TYPE_FADE_WHITE;
    }
}

/** Moves the clock forward by d units, counting a day at every 6:00 it passes. */
static void NvMmClock_Advance(PlayState* play, u32 d) {
    while (d > 0) {
        u32 toDawn = (u16)(MM_DAWN - sMmTime);

        if (toDawn == 0) {
            toDawn = 0x10000;
        }
        if (d < toDawn) {
            sMmTime += d;
            return;
        }
        d -= toDawn;
        sMmTime = MM_DAWN;
        sMmDay++;
        if (sMmDay > 3) {
            if (NvMm_Log()) {
                printf("NVMM dawn of the fourth day: the moon falls\n");
                fflush(stdout);
            }
            NvMmClock_Reset(play, "the moon fell");
            return;
        }
        sMmTelop = MM_TELOP_FRAMES;
        if (NvMm_Log()) {
            printf("NVMM dawn of day %d\n", (int)sMmDay);
            fflush(stdout);
        }
    }
}

static s32 NvMmClock_Running(PlayState* play) {
    return (play->pauseCtx.state == 0) && (play->gameOverCtx.state == GAMEOVER_INACTIVE) &&
           (play->msgCtx.msgLength == 0) && (play->msgCtx.msgMode == MSGMODE_NONE) &&
           (play->transitionMode == TRANS_MODE_OFF) && (play->transitionTrigger == TRANS_TRIGGER_OFF);
}

static void NvMmClock_DevSet(void) {
    const char* s = getenv("NV_MM_TIME");
    int day, hhmm;

    sMmDevSet = true;
    if ((s != NULL) && (sscanf(s, "%d,%d", &day, &hhmm) == 2) && (day >= 1) && (day <= 3)) {
        sMmDay = day;
        sMmTime = (u16)((((hhmm / 100) * 60 + (hhmm % 100)) << 16) / (24 * 60));
    }
}

/** Every frame of play (z_play.c). */
void NvMmClock_Update(PlayState* play) {
    s32 hour = NvMm_Hour(sMmTime);
    u16 now;

    if (!NvMm_IsScene(play->sceneNum)) {
        if (sMmInTermina) { // leaving Termina: Hyrule's time back as it was
            gSaveContext.dayTime = sMmHyruleTime;
            sMmInTermina = false;
        }
        return;
    }
    if (!sMmDevSet) {
        NvMmClock_DevSet();
    }
    if (!sMmInTermina) { // arriving: Hyrule's time aside, Termina's in its place
        sMmHyruleTime = gSaveContext.dayTime;
        sMmInTermina = true;
        gSaveContext.dayTime = gSaveContext.skyboxTime = sMmWritten = sMmTime;
        return;
    }

    // something else moved OoT's clock (the Sun's Song, the Sonata): Termina's goes with it
    now = gSaveContext.dayTime;
    if (now != sMmWritten) {
        u32 d = (u16)(now - sMmWritten);

        if (NvMm_Log()) {
            printf("NVMM skip +%04X\n", (unsigned)d);
            fflush(stdout);
        }
        NvMmClock_Advance(play, d);
    }
    if (getenv("NV_MM_SPEED_MENU") && !sMmMenuDevDone) { // (dev: the speed list on the first frame)
        sMmMenuDevDone = true;
        NvMmClock_OpenSpeedMenu(play);
    }
    if (sMmMenu) {
        NvMmClock_SpeedMenuInput(play);
    } else if (NvMmClock_Running(play)) {
        const char* mult = getenv("NV_MM_SPEED");
        s32 halves = sMmSpeedHalves * ((mult != NULL) ? atoi(mult) : 1) + sMmHalfCarry;

        sMmFrameStep = halves / 2;
        sMmHalfCarry = halves % 2;
        if (sMmFrameStep > 0) {
            NvMmClock_Advance(play, sMmFrameStep);
        }
    }
    // (OoT moves the sky's time only while its own clock runs, and here it doesn't: kankyo)
    gSaveContext.dayTime = gSaveContext.skyboxTime = sMmWritten = sMmTime;

    // the Song of Time: once its message has closed, back to the first dawn
    if ((play->msgCtx.msgMode == MSGMODE_SONG_PLAYED) && (play->msgCtx.lastPlayedSong == OCARINA_SONG_TIME)) {
        sMmSongOfTime = true;
    } else if (sMmSongOfTime && (play->msgCtx.msgMode == MSGMODE_NONE)) {
        sMmSongOfTime = false;
        NvMmClock_Reset(play, "the Song of Time");
    }

    if (sMmTelop > 0) {
        sMmTelop--;
    }
    if (NvMm_Log() && (NvMm_Hour(sMmTime) != hour)) {
        printf("NVMM day %d %02d:00\n", (int)sMmDay, (int)NvMm_Hour(sMmTime));
        fflush(stdout);
    }
}

/** The speed list (the Inverted Song of Time's, once the shared song listener has it): Link held
 * still while it's open. */
void NvMmClock_OpenSpeedMenu(PlayState* play) {
    s32 i;

    sMmMenu = true;
    sMmMenuHeld = 0;
    sMmMenuCursor = 2;
    for (i = 0; i < ARRAY_COUNT(sMmSpeeds); i++) {
        if (sMmSpeeds[i].halves == sMmSpeedHalves) {
            sMmMenuCursor = i;
        }
    }
    Player_SetCsActionWithHaltedActors(play, NULL, 8);
}

s32 NvMmClock_SpeedMenuOpen(void) {
    return sMmMenu;
}

static void NvMmClock_SpeedMenuInput(PlayState* play) {
    Input* input = &play->state.input[0];
    s8 dir = 0;

    if ((input->rel.stick_y > 30) || CHECK_BTN_ALL(input->cur.button, BTN_DUP)) {
        dir = -1;
    } else if ((input->rel.stick_y < -30) || CHECK_BTN_ALL(input->cur.button, BTN_DDOWN)) {
        dir = 1;
    }
    if ((dir != 0) && (dir != sMmMenuHeld)) {
        sMmMenuCursor = CLAMP(sMmMenuCursor + dir, 0, ARRAY_COUNT(sMmSpeeds) - 1);
        Sfx_PlaySfxCentered(NA_SE_SY_CURSOR);
    }
    sMmMenuHeld = dir;
    if (CHECK_BTN_ALL(input->press.button, BTN_A) || CHECK_BTN_ALL(input->press.button, BTN_B)) {
        if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
            sMmSpeedHalves = sMmSpeeds[sMmMenuCursor].halves;
            sMmHalfCarry = 0;
            Sfx_PlaySfxCentered(NA_SE_SY_DECIDE);
            if (NvMm_Log()) {
                printf("NVMM speed: %s\n", sMmSpeeds[sMmMenuCursor].name);
                fflush(stdout);
            }
        } else {
            Sfx_PlaySfxCentered(NA_SE_SY_CANCEL);
        }
        sMmMenu = false;
        Player_SetCsActionWithHaltedActors(play, NULL, 7);
    }
}

static const char* NvMmClock_SpeedTag(void) {
    s32 i;

    for (i = 0; i < ARRAY_COUNT(sMmSpeeds); i++) {
        if (sMmSpeeds[i].halves == sMmSpeedHalves) {
            return sMmSpeeds[i].tag;
        }
    }
    return "";
}

/** Over the HUD (z_play.c): the day and time, and the day's title as it dawns. */
void NvMmClock_Draw(PlayState* play) {
    static const char* sDayNames[] = { "", "First", "Second", "Final" };
    GfxPrint printer;
    Gfx* opa;
    Gfx* gfx;

    if (!NvMm_IsScene(play->sceneNum) || !sMmInTermina || (sMmDay < 1) || (sMmDay > 3) ||
        (gSaveContext.gameMode != GAMEMODE_NORMAL) || (play->pauseCtx.state != 0)) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    opa = POLY_OPA_DISP;
    gfx = Graph_GfxPlusOne(opa);
    gSPDisplayList(OVERLAY_DISP++, gfx);
    if (sMmMenu) { // the speed list's panel
        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_1CYCLE);
        gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
        gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
        gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 200);
        gDPFillRectangle(gfx++, 48, 64, 272, 64 + 16 + 12 * ARRAY_COUNT(sMmSpeeds) + 8);
        gDPPipeSync(gfx++);
    }
    GfxPrint_Init(&printer);
    GfxPrint_Open(&printer, gfx);
    GfxPrint_SetColor(&printer, 255, 255, 255, 255);
    GfxPrint_SetPos(&printer, 14, 27);
    GfxPrint_Printf(&printer, "Day %d  %02d:%02d %s", (int)sMmDay, (int)NvMm_Hour(sMmTime), (int)NvMm_Minute(sMmTime),
                    NvMmClock_SpeedTag());
    if (sMmMenu) {
        s32 i;

        GfxPrint_SetColor(&printer, 255, 255, 200, 255);
        GfxPrint_SetPos(&printer, 8, 9);
        GfxPrint_Printf(&printer, "The flow of time");
        for (i = 0; i < ARRAY_COUNT(sMmSpeeds); i++) {
            if (i == sMmMenuCursor) {
                GfxPrint_SetColor(&printer, 120, 255, 120, 255);
            } else {
                GfxPrint_SetColor(&printer, 255, 255, 255, 255);
            }
            GfxPrint_SetPos(&printer, 7, 11 + i);
            GfxPrint_Printf(&printer, "%s %s%s", (i == sMmMenuCursor) ? ">" : " ", sMmSpeeds[i].name,
                            (sMmSpeeds[i].halves == sMmSpeedHalves) ? " *" : "");
        }
    }
    if (sMmTelop > 0) {
        GfxPrint_SetColor(&printer, 255, 255, 200, 255);
        GfxPrint_SetPos(&printer, 9, 12);
        GfxPrint_Printf(&printer, "Dawn of the %s Day", sDayNames[sMmDay]);
    }
    gfx = GfxPrint_Close(&printer);
    GfxPrint_Destroy(&printer);
    gSPEndDisplayList(gfx++);
    Graph_BranchDlist(opa, gfx);
    POLY_OPA_DISP = gfx;
    CLOSE_DISPS(play->state.gfxCtx);
}

/*
 * Saving (NvMmSave.cpp): Termina lives in the "termina" section of SoH's own save file, saved and
 * loaded in the same moment as Hyrule's, so the two can never come from different saves.
 */
typedef struct {
    s32 day;
    u16 time;
    s16 speedOffset;
    s32 inTermina;  // the save was made in Termina: OoT's own time in the file is Termina's
    u16 hyruleTime; // Hyrule's time, put aside while in Termina
} NvMmClockSave;

void NvMmClock_GetSave(NvMmClockSave* s) {
    s->day = sMmDay;
    s->time = sMmTime;
    s->speedOffset = sMmSpeedHalves; // (since 2026-11-23: the speed in half steps; see SetSave)
    s->inTermina = sMmInTermina;
    s->hyruleTime = sMmHyruleTime;
}

/** A save's Termina, after its base section (Hyrule) has been read. */
void NvMmClock_SetSave(const NvMmClockSave* s) {
    sMmDay = ((s->day >= 1) && (s->day <= 3)) ? s->day : 1;
    sMmTime = s->time;
    // the speed: half steps a frame (2-18); an older save's MM offset (-2 or 0) read as one
    sMmSpeedHalves = (s->speedOffset >= 2) ? s->speedOffset : 2 * (MM_TIME_SPEED + s->speedOffset);
    sMmHalfCarry = 0;
    sMmMenu = false;
    if (s->inTermina) {
        // OoT saved Termina's time as its own; a loaded game starts in Hyrule (OoT's respawn), so
        // its time is put back
        gSaveContext.dayTime = s->hyruleTime;
    }
    sMmInTermina = false;
    sMmTelop = 0;
    sMmSongOfTime = false;
}

/** A new game, or a save from before Termina: the dawn of the first day. */
void NvMmClock_NewGame(void) {
    NvMmScene_NewGame();
    sMmDay = 1;
    sMmTime = MM_DAWN;
    sMmSpeedHalves = 6;
    sMmHalfCarry = 0;
    sMmMenu = false;
    sMmInTermina = false;
    sMmTelop = 0;
    sMmSongOfTime = false;
}

void NvMmClock_LogState(const char* when) {
    printf("NVMM state (%s): day %d %02d:%02d speed %d/6, in Termina %d, Hyrule %04X\n", when, (int)sMmDay,
           (int)NvMm_Hour(sMmTime), (int)NvMm_Minute(sMmTime), (int)sMmSpeedHalves, (int)sMmInTermina,
           (unsigned)sMmHyruleTime);
    fflush(stdout);
}

/** MM's half-day bit for the clock's day and time (z64actor.h HALFDAYBIT_*: day 0 dawn is bit 9,
 * then night, down to day 4 night at bit 0). The day turns at 6:00, so a night belongs to the day
 * before it, as in MM. */
u16 NvMmClock_HalfDayBit(void) {
    s32 night = (sMmTime >= 0xC000) || (sMmTime < MM_DAWN);
    s32 day = CLAMP(sMmDay, 0, 4);

    return 1 << (9 - 2 * day - night);
}

/** MM's gSaveContext.save.isNight: 18:00 to 6:00. */
s32 NvMmClock_IsNight(void) {
    return (sMmTime >= 0xC000) || (sMmTime < MM_DAWN);
}

/** MM's Environment_SetTimeJump: the clock jumped ahead (minutes), through any dawn on the way. */
void NvMmClock_Jump(PlayState* play, u32 minutes) {
    NvMmClock_Advance(play, (minutes * 0x10000) / (24 * 60));
}

/** MM's time = 8:00 with Sram_IncrementDay: asleep until the next morning at 8:00 (on through the
 * dawn: its day's title, or the moon's fall after the third). */
void NvMmClock_SleepToMorning(PlayState* play) {
    u32 toDawn = (u16)(MM_DAWN - sMmTime);

    if (toDawn == 0) {
        toDawn = 0x10000;
    }
    NvMmClock_Advance(play, toDawn + (2 * 0x10000 + 23) / 24);
}

/** How far the clock moves a frame (MM's R_TIME_SPEED + timeSpeedOffset; with NV_MM_SPEED's
 * multiplier, so people walking by the time keep up in a sped-up test). */
s32 NvMmClock_Speed(void) {
    return sMmMenu ? 0 : sMmFrameStep;
}

/** The clock, for Termina's actors (the day: 1-3; the time: OoT's and MM's units). */
s32 NvMmClock_Day(void) {
    return sMmDay;
}

u16 NvMmClock_Time(void) {
    return sMmTime;
}
