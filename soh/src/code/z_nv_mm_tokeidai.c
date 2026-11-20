/*
 * Nightmareverse: Majora's Mask's clock tower (MM's Obj_Tokeidai), our own code to MM's rules
 * (zeldaret/mm, ovl_Obj_Tokeidai). Its model is MM's own, object_obj_tokeidai, drawn from the
 * player's mm.o2r (mm/tools/mm_objects.py).
 *
 * Its pieces, by params >> 12 (the ones Clock Town places first):
 *   0  the exterior gears either side of the clock (Clock Town), 4 Termina Field's;
 *   2  the tower clock: minute ring, hand, clock face, sun and moon panel (Clock Town), 5 Termina
 *      Field's (small, 0.15);
 *   3  the counterweight on top with its night spotlight (Clock Town), 6 Termina Field's;
 *   8  the tower walls (Termina Field), 1 an unused wall;
 *   9, 10  the wall clocks indoors (big, small);
 *   11 the staircase to the rooftop, drawn once the tower is open.
 *
 * They follow Termina's clock (z_nv_mm_clock.c), as MM's follow the save's time:
 * - the minute ring (and the gears) has 30 places an hour; at each it wiggles 8 frames, then
 *   turns on to the next, speeding up;
 * - the clock face turns a step each hour, after 12 frames of wiggling;
 * - the sun and moon panel turns over at 6:00 and 18:00;
 * - the counterweight turns slowly all the time, its spotlight fading in at night and out by day.
 * From midnight on the final night the tower stands open, as MM places it when the time is
 * already past that (the gear and counterweight moved, the clock face pushed in and tilted up,
 * the staircase there).
 *
 * Not yet: the tower opening itself (its cutscene, the fireworks), the moon's fall, and the
 * minute ring's tick (MM's own sound, not in OoT's sound banks).
 */
#include "global.h"
#include <math.h>

#define OBJ_TOKEIDAI_TYPE(thisx) (((thisx)->params & 0xF000) >> 12)

enum {
    TOKEIDAI_EXTERIOR_GEAR_CLOCK_TOWN = 0,
    TOKEIDAI_UNUSED_WALL = 1,
    TOKEIDAI_TOWER_CLOCK_CLOCK_TOWN = 2,
    TOKEIDAI_COUNTERWEIGHT_CLOCK_TOWN = 3,
    TOKEIDAI_EXTERIOR_GEAR_TERMINA_FIELD = 4,
    TOKEIDAI_TOWER_CLOCK_TERMINA_FIELD = 5,
    TOKEIDAI_COUNTERWEIGHT_TERMINA_FIELD = 6,
    TOKEIDAI_TOWER_WALLS_TERMINA_FIELD = 8,
    TOKEIDAI_WALL_CLOCK = 9,
    TOKEIDAI_SMALL_WALL_CLOCK = 10,
    TOKEIDAI_STAIRCASE_TO_ROOFTOP = 11,
};

#define MM_DL(name) ((Gfx*)"__OTR__mm/objects/object_obj_tokeidai/" name)
#define DL_TERMINA_FIELD_WALLS MM_DL("gClockTowerTerminaFieldWallsDL")
#define DL_SPOTLIGHT MM_DL("gClockTowerSpotlightDL")
#define DL_COUNTERWEIGHT MM_DL("gClockTowerCounterweightDL")
#define DL_EXTERIOR_GEAR MM_DL("gClockTowerExteriorGearDL")
#define DL_CLOCK_CENTER_AND_HAND MM_DL("gClockTowerClockCenterAndHandDL")
#define DL_SUN_AND_MOON_PANEL MM_DL("gClockTowerSunAndMoonPanelDL")
#define DL_MINUTE_RING MM_DL("gClockTowerMinuteRingDL")
#define DL_UNUSED_TOWER_WALL MM_DL("gClockTowerUnusedTowerWallDL")
#define DL_EMPTY MM_DL("gClockTowerEmptyDL")
#define DL_STAIRCASE_TO_ROOFTOP MM_DL("gClockTowerStaircaseToRooftopDL")
#define DL_CLOCK_FACE MM_DL("gClockTowerClockFaceDL")
#define DL_WALL_CLOCK_FACE MM_DL("gWallClockClockFaceDL")

struct NvMmTokeidai;
typedef void (*NvMmTokeidaiFunc)(struct NvMmTokeidai*, PlayState*);

typedef struct NvMmTokeidai {
    Actor actor;
    Gfx* opaDList;
    Gfx* xluDList;
    s16 ringRot;   // the minute ring's (or the gear's) angle
    s16 ringVel;
    s16 ringTimer;
    s16 faceRot;   // the clock face's angle (the hour)
    s16 faceVel;
    s16 faceTimer;
    s32 panelRot;  // the sun and moon panel's angle
    s16 panelVel;
    s16 faceZ;     // how far the clock face is pushed in once the tower opens
    s32 minute;    // 30 to the hour, as MM's clock has
    s32 hour;
    s32 spotlight; // 0-100
    u16 clockTime;
    NvMmTokeidaiFunc func;
} NvMmTokeidai;

const size_t gNvMmTokeidaiSize = sizeof(NvMmTokeidai);

s32 NvMmClock_Day(void);
u16 NvMmClock_Time(void);

static s32 NvMmTokeidai_Night(void) {
    u16 t = NvMmClock_Time();

    return (t >= 0xC000) || (t < 0x4000);
}

/** The tower stands open from midnight on the final night (MM: day 3 before 6:00, or later). */
static s32 NvMmTokeidai_Open(void) {
    return ((NvMmClock_Day() == 3) && (NvMmClock_Time() < 0x4000)) || (NvMmClock_Day() >= 4);
}

static s32 NvMmTokeidai_Hour(NvMmTokeidai* this) {
    return (s32)(this->clockTime * (24.0f / 0x10000));
}

static s32 NvMmTokeidai_Minute(NvMmTokeidai* this) {
    return (s32)(this->clockTime * (720.0f / 0x10000)) % 30;
}

static s16 NvMmTokeidai_FaceAngle(s32 hour) {
    return (s16)(s32)(hour * (0x10000 / 24.0f));
}

static s16 NvMmTokeidai_RingAngle(s32 minute) {
    return (s16)(s32)(minute * (0x10000 * 12.0f / 360));
}

static s32 NvMmTokeidai_PanelTarget(void) {
    return NvMmTokeidai_Night() ? 0x8000 : 0;
}

static void NvMmTokeidai_SetupRing(NvMmTokeidai* this) {
    this->minute = NvMmTokeidai_Minute(this);
    this->ringRot = NvMmTokeidai_RingAngle(this->minute);
    this->ringVel = 0x3C;
    this->ringTimer = 0;
}

static void NvMmTokeidai_SetupClock(NvMmTokeidai* this) {
    NvMmTokeidai_SetupRing(this);
    this->hour = NvMmTokeidai_Hour(this);
    this->faceRot = NvMmTokeidai_FaceAngle(this->hour);
    this->faceVel = 0;
    this->faceTimer = 0;
    this->panelVel = 0;
    this->panelRot = NvMmTokeidai_PanelTarget();
}

/** The minute ring or gear: a wiggle, then a turn to the next minute's place. */
static void NvMmTokeidai_TurnMinute(NvMmTokeidai* this) {
    s32 minute = NvMmTokeidai_Minute(this);

    if (minute == this->minute) {
        return;
    }
    if (this->ringTimer > 8) {
        this->ringVel += 0x3C;
        this->ringRot += this->ringVel;
    } else if ((this->ringTimer & 3) == 0) {
        this->ringRot += 0x5A;
    } else if ((this->ringTimer & 3) == 1) {
        this->ringRot -= 0x5A;
    }
    this->ringTimer++;
    if (((minute == 15) && (this->ringRot < 0)) || ((minute != 15) && (this->ringRot > NvMmTokeidai_RingAngle(minute)))) {
        this->ringRot = NvMmTokeidai_RingAngle(minute);
        this->minute = minute;
        this->ringVel = 0x5A;
        this->ringTimer = 0;
    }
}

/** The clock face: a wiggle, then a turn to the next hour; and the sun and moon panel. */
static void NvMmTokeidai_TurnHour(NvMmTokeidai* this) {
    s32 hour = NvMmTokeidai_Hour(this);

    if (hour != this->hour) {
        if (this->faceTimer > 12) {
            this->faceVel += 0xA;
            this->faceRot += this->faceVel;
        } else if ((this->faceTimer & 3) == 0) {
            this->faceRot += 0x3C;
        } else if ((this->faceTimer & 3) == 1) {
            this->faceRot -= 0x3C;
        }
        this->faceTimer++;
        if (((hour == 12) && (this->faceRot < 0)) || ((hour != 12) && (this->faceRot > NvMmTokeidai_FaceAngle(hour)))) {
            this->faceRot = NvMmTokeidai_FaceAngle(hour);
            this->hour = hour;
            this->faceVel = 0;
            this->faceTimer = 0;
        }
    }
    if (this->panelRot != NvMmTokeidai_PanelTarget()) {
        if ((this->hour == 6) || (this->hour == 18)) {
            s32 end = (this->hour == 6) ? 0x10000 : 0x8000;

            this->panelVel += 0x222;
            this->panelRot += this->panelVel;
            if (this->panelRot > end) {
                this->panelRot = NvMmTokeidai_PanelTarget();
                this->panelVel = 0;
            }
        }
    }
}

static void NvMmTokeidai_DoNothing(NvMmTokeidai* this, PlayState* play) {
}

static void NvMmTokeidai_TowerClockIdle(NvMmTokeidai* this, PlayState* play) {
    this->clockTime = NvMmClock_Time();
    if ((NvMmClock_Day() != 3) || (NvMmClock_Time() >= 0x4000)) {
        NvMmTokeidai_TurnMinute(this);
    }
    NvMmTokeidai_TurnHour(this);
}

static void NvMmTokeidai_WallClockIdle(NvMmTokeidai* this, PlayState* play) {
    this->clockTime = NvMmClock_Time();
    NvMmTokeidai_TurnMinute(this);
    NvMmTokeidai_TurnHour(this);
}

static void NvMmTokeidai_GearIdle(NvMmTokeidai* this, PlayState* play) {
    this->clockTime = NvMmClock_Time();
    NvMmTokeidai_TurnMinute(this);
}

static void NvMmTokeidai_CounterweightIdle(NvMmTokeidai* this, PlayState* play) {
    this->actor.shape.rot.y -= 0x40;
    if (NvMmTokeidai_Night()) {
        if (this->spotlight < 100) {
            this->spotlight += 4;
        }
    } else if (this->spotlight > 0) {
        this->spotlight -= 4;
    }
}

static void NvMmTokeidai_Draw(Actor* thisx, PlayState* play);
static void NvMmTokeidai_ClockDraw(Actor* thisx, PlayState* play);
static void NvMmTokeidai_CounterweightDraw(Actor* thisx, PlayState* play);
static void NvMmTokeidai_GearDraw(Actor* thisx, PlayState* play);

static void NvMmTokeidai_GearInit(NvMmTokeidai* this) {
    this->actor.draw = NvMmTokeidai_GearDraw;
    this->opaDList = DL_EXTERIOR_GEAR;
    NvMmTokeidai_SetupRing(this);
    if (NvMmTokeidai_Open()) { // moved up out of the way, as MM places it when already open
        this->actor.world.pos.y += this->actor.scale.y * 1900.0f;
        this->actor.shape.yOffset = 1500.0f;
        this->func = NvMmTokeidai_DoNothing;
    } else {
        this->func = NvMmTokeidai_GearIdle;
    }
}

static void NvMmTokeidai_TowerClockInit(NvMmTokeidai* this) {
    this->actor.draw = NvMmTokeidai_ClockDraw;
    NvMmTokeidai_SetupClock(this);
    if (NvMmTokeidai_Open()) { // pushed in and tilted up
        this->actor.world.pos.y += (this->actor.scale.y * 5191.0f) - 50.0f;
        this->actor.world.pos.x += Math_SinS(this->actor.world.rot.y) * this->actor.scale.z * 1791.0f;
        this->actor.world.pos.z += -Math_CosS(this->actor.world.rot.y) * this->actor.scale.z * 1791.0f;
        this->faceZ = -0x140;
        this->actor.shape.rot.x = -0x4000;
        this->func = NvMmTokeidai_DoNothing;
    } else {
        this->func = NvMmTokeidai_TowerClockIdle;
    }
}

static void NvMmTokeidai_CounterweightInit(NvMmTokeidai* this) {
    this->actor.draw = NvMmTokeidai_CounterweightDraw;
    this->opaDList = DL_COUNTERWEIGHT;
    this->xluDList = DL_SPOTLIGHT;
    this->spotlight = NvMmTokeidai_Night() ? 100 : 0;
    if (NvMmTokeidai_Open()) { // dropped down the front, lying flat
        this->spotlight = 0;
        this->actor.world.pos.y += this->actor.scale.y * -2160.0f;
        this->actor.world.pos.x += Math_SinS(this->actor.world.rot.y) * this->actor.scale.z * 5400.0f;
        this->actor.world.pos.z += -Math_CosS(this->actor.world.rot.y) * this->actor.scale.z * 5400.0f;
        this->actor.shape.rot.x = -0x4000;
        this->func = NvMmTokeidai_DoNothing;
    } else {
        this->func = NvMmTokeidai_CounterweightIdle;
    }
}

static InitChainEntry sInitChain[] = {
    ICHAIN_VEC3F_DIV1000(scale, 100, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneForward, 4000, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneScale, 3300, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneDownward, 1100, ICHAIN_STOP),
};

void NvMmTokeidai_Init(Actor* thisx, PlayState* play) {
    NvMmTokeidai* this = (NvMmTokeidai*)thisx;

    Actor_ProcessInitChain(&this->actor, sInitChain);
    this->func = NvMmTokeidai_DoNothing;
    this->opaDList = NULL;
    this->xluDList = NULL;
    this->faceZ = 0;
    this->clockTime = NvMmClock_Time();
    this->actor.draw = NvMmTokeidai_Draw;
    switch (OBJ_TOKEIDAI_TYPE(&this->actor)) {
        case TOKEIDAI_EXTERIOR_GEAR_TERMINA_FIELD:
            Actor_SetScale(&this->actor, 0.15f);
            // fallthrough
        case TOKEIDAI_EXTERIOR_GEAR_CLOCK_TOWN:
            NvMmTokeidai_GearInit(this);
            break;
        case TOKEIDAI_UNUSED_WALL:
            this->opaDList = DL_UNUSED_TOWER_WALL;
            break;
        case TOKEIDAI_TOWER_WALLS_TERMINA_FIELD:
            Actor_SetScale(&this->actor, 1.0f);
            this->opaDList = DL_TERMINA_FIELD_WALLS;
            break;
        case TOKEIDAI_TOWER_CLOCK_TERMINA_FIELD:
            Actor_SetScale(&this->actor, 0.15f);
            // fallthrough
        case TOKEIDAI_TOWER_CLOCK_CLOCK_TOWN:
            NvMmTokeidai_TowerClockInit(this);
            break;
        case TOKEIDAI_WALL_CLOCK:
        case TOKEIDAI_SMALL_WALL_CLOCK:
            Actor_SetScale(&this->actor, (OBJ_TOKEIDAI_TYPE(&this->actor) == TOKEIDAI_WALL_CLOCK) ? 0.02f : 0.01f);
            this->actor.draw = NvMmTokeidai_ClockDraw;
            NvMmTokeidai_SetupClock(this);
            this->func = NvMmTokeidai_WallClockIdle;
            break;
        case TOKEIDAI_COUNTERWEIGHT_TERMINA_FIELD:
            Actor_SetScale(&this->actor, 0.15f);
            // fallthrough
        case TOKEIDAI_COUNTERWEIGHT_CLOCK_TOWN:
            NvMmTokeidai_CounterweightInit(this);
            break;
        case TOKEIDAI_STAIRCASE_TO_ROOFTOP:
            this->opaDList = DL_STAIRCASE_TO_ROOFTOP;
            this->xluDList = DL_EMPTY;
            break;
        default:
            break;
    }
}

void NvMmTokeidai_Destroy(Actor* thisx, PlayState* play) {
}

void NvMmTokeidai_Update(Actor* thisx, PlayState* play) {
    NvMmTokeidai* this = (NvMmTokeidai*)thisx;

    this->func(this, play);
}

static void NvMmTokeidai_LoadMtx(GraphicsContext* gfxCtx, Gfx** disp) {
    gSPMatrix((*disp)++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
}

/** The plain pieces: walls, and the staircase once the tower is open. */
static void NvMmTokeidai_Draw(Actor* thisx, PlayState* play) {
    NvMmTokeidai* this = (NvMmTokeidai*)thisx;

    if ((OBJ_TOKEIDAI_TYPE(&this->actor) == TOKEIDAI_STAIRCASE_TO_ROOFTOP) && !NvMmTokeidai_Open()) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    if (this->opaDList != NULL) {
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        NvMmTokeidai_LoadMtx(play->state.gfxCtx, &POLY_OPA_DISP);
        gSPDisplayList(POLY_OPA_DISP++, this->opaDList);
    }
    if (this->xluDList != NULL) {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        NvMmTokeidai_LoadMtx(play->state.gfxCtx, &POLY_XLU_DISP);
        gSPDisplayList(POLY_XLU_DISP++, this->xluDList);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

/** The clock: minute ring, hand, face and the sun and moon panel. */
static void NvMmTokeidai_ClockDraw(Actor* thisx, PlayState* play) {
    NvMmTokeidai* this = (NvMmTokeidai*)thisx;
    s32 type = OBJ_TOKEIDAI_TYPE(&this->actor);

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Push();
    Matrix_RotateZ(BINANG_TO_RAD(-this->ringRot), MTXMODE_APPLY);
    NvMmTokeidai_LoadMtx(play->state.gfxCtx, &POLY_OPA_DISP);
    gSPDisplayList(POLY_OPA_DISP++, DL_MINUTE_RING);
    Matrix_Pop();
    Matrix_Translate(0.0f, 0.0f, this->faceZ, MTXMODE_APPLY);
    NvMmTokeidai_LoadMtx(play->state.gfxCtx, &POLY_OPA_DISP);
    gSPDisplayList(POLY_OPA_DISP++, DL_CLOCK_CENTER_AND_HAND);
    Matrix_RotateZ(BINANG_TO_RAD(-this->faceRot * 2), MTXMODE_APPLY);
    NvMmTokeidai_LoadMtx(play->state.gfxCtx, &POLY_OPA_DISP);
    gSPDisplayList(POLY_OPA_DISP++,
                   ((type == TOKEIDAI_WALL_CLOCK) || (type == TOKEIDAI_SMALL_WALL_CLOCK)) ? DL_WALL_CLOCK_FACE
                                                                                         : DL_CLOCK_FACE);
    Matrix_Translate(0.0f, -1112.0f, -19.6f, MTXMODE_APPLY);
    Matrix_RotateY(BINANG_TO_RAD((s16)this->panelRot), MTXMODE_APPLY);
    NvMmTokeidai_LoadMtx(play->state.gfxCtx, &POLY_OPA_DISP);
    gSPDisplayList(POLY_OPA_DISP++, DL_SUN_AND_MOON_PANEL);
    CLOSE_DISPS(play->state.gfxCtx);
}

/** The counterweight, turning, and its spotlight (its mask texture scrolling down the beam). */
static void NvMmTokeidai_CounterweightDraw(Actor* thisx, PlayState* play) {
    NvMmTokeidai* this = (NvMmTokeidai*)thisx;
    u32 frames = play->gameplayFrames;

    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 0x20, 0x40, 1, -frames, 0, 0x20, 0x20));
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    NvMmTokeidai_LoadMtx(play->state.gfxCtx, &POLY_OPA_DISP);
    gSPDisplayList(POLY_OPA_DISP++, this->opaDList);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    NvMmTokeidai_LoadMtx(play->state.gfxCtx, &POLY_XLU_DISP);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 255, 255, 235, 180, (s32)(this->spotlight * 2.55f));
    gSPDisplayList(POLY_XLU_DISP++, this->xluDList);
    CLOSE_DISPS(play->state.gfxCtx);
}

/** An exterior gear, turning with the minutes. */
static void NvMmTokeidai_GearDraw(Actor* thisx, PlayState* play) {
    NvMmTokeidai* this = (NvMmTokeidai*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    Matrix_RotateZ(BINANG_TO_RAD(this->ringRot), MTXMODE_APPLY);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    NvMmTokeidai_LoadMtx(play->state.gfxCtx, &POLY_OPA_DISP);
    gSPDisplayList(POLY_OPA_DISP++, DL_EXTERIOR_GEAR);
    CLOSE_DISPS(play->state.gfxCtx);
}
