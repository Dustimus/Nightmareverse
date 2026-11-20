/*
 * Nightmareverse: Majora's Mask's moon, from MM's En_Fall (zeldaret/mm, ovl_En_Fall), ported
 * closely (mm/README.md: MM's actors keep MM's logic, adapted only where OoT's engine differs).
 * Its models are MM's own, object_lodmoon and object_fall, drawn by path from mm.o2r.
 *
 * Types (params >> 7 & 0x1F), the ones Termina places so far:
 *   6  the far, low-detail moon over Clock Town (object_lodmoon). It grows each day, and on the
 *      final day sinks towards the town as the hours pass (EnFall_Moon_AdjustScaleAndPosition).
 *      Past 9000 units from the camera it's drawn pulled in to 9000, so the view's far plane
 *      never clips it (EnFall_LodMoon_DrawWithLerp);
 *   0  the Termina Field moon, and any other default (object_fall's full moon);
 *   9  the moon over the clock tower's rooftop (object_fall), drawn still for now;
 *   7  the stopped moon after the giants (object_fall), only after its event; not yet here.
 * Its scale: params & 0x7F, 1-4 small ones, anything else the usual 0.16.
 *
 * Adapted from MM:
 * - the day and time are Termina's clock (z_nv_mm_clock.c);
 * - OoT's matrix calls (radians) for MM's binary-angle ones;
 * - 2Ship's "silly moon scale" cosmetic and the cutscene cues (the rooftop's, the crash's) are
 *   left out;
 * - the crash's fire balls, debris and fire ring (types 3, 4, 11), the Moon's Tear (8) and the
 *   open-mouthed moon (10) aren't ported yet.
 */
#include "global.h"

#define EN_FALL_SCALE(thisx) ((thisx)->params & 0x7F)
#define EN_FALL_TYPE(thisx) (((thisx)->params & 0xF80) >> 7)

enum {
    EN_FALL_TYPE_TERMINA_FIELD_MOON = 0,
    EN_FALL_TYPE_LODMOON_NO_LERP = 5,
    EN_FALL_TYPE_LODMOON = 6,
    EN_FALL_TYPE_STOPPED_MOON_CLOSED_MOUTH = 7,
    EN_FALL_TYPE_CLOCK_TOWER_MOON = 9,
    EN_FALL_TYPE_LODMOON_INVERTED_STONE_TOWER = 12,
};

#define DL_LODMOON_EYES ((Gfx*)"__OTR__mm/objects/object_lodmoon/gLodmoonEyesDL")
#define DL_LODMOON_MOON ((Gfx*)"__OTR__mm/objects/object_lodmoon/gLodmoonMoonDL")
#define DL_MOON ((Gfx*)"__OTR__mm/objects/object_fall/gMoonDL")

struct NvMmFall;
typedef void (*NvMmFallFunc)(struct NvMmFall*, PlayState*);

typedef struct NvMmFall {
    Actor actor;
    f32 scale;
    f32 eyeGlowIntensity;
    u16 dayStartTime;
    s16 currentDay;
    NvMmFallFunc func;
} NvMmFall;

const size_t gNvMmFallSize = sizeof(NvMmFall);

s32 NvMmClock_Day(void);
u16 NvMmClock_Time(void);

/** The moon's size by the day; on the final day it also comes down as the hours pass. */
static void NvMmFall_AdjustScaleAndPosition(NvMmFall* this) {
    u16 now = NvMmClock_Time();
    u16 start = this->dayStartTime;
    f32 height;

    if (now < start) {
        height = 1.0f - (((f32)start - (f32)now) * (1.0f / 0x10000));
    } else {
        height = ((f32)now - (f32)start) * (1.0f / 0x10000);
    }
    switch (NvMmClock_Day()) {
        case 0:
            Actor_SetScale(&this->actor, this->scale * 1.2f);
            this->actor.world.pos.y = this->actor.home.pos.y;
            break;
        case 1:
            Actor_SetScale(&this->actor, this->scale * 2.4f);
            this->actor.world.pos.y = this->actor.home.pos.y;
            break;
        case 2:
            Actor_SetScale(&this->actor, this->scale * 3.6f);
            this->actor.world.pos.y = this->actor.home.pos.y;
            break;
        case 3:
            Actor_SetScale(&this->actor, this->scale * 3.6f);
            if (EN_FALL_TYPE(&this->actor) == EN_FALL_TYPE_LODMOON_INVERTED_STONE_TOWER) {
                this->actor.world.pos.y = this->actor.home.pos.y + (height * 6700.0f * (this->scale * 6.25f));
            } else {
                this->actor.world.pos.y = this->actor.home.pos.y - (height * 6700.0f * (this->scale * 6.25f));
            }
            break;
        default:
            break;
    }
}

static void NvMmFall_DoNothing(NvMmFall* this, PlayState* play) {
}

/** The moon's every frame outside cutscenes: a new day starts its descent clock, then its size. */
static void NvMmFall_MoonDefault(NvMmFall* this, PlayState* play) {
    s16 day = NvMmClock_Day();

    if (this->currentDay != day) {
        this->currentDay = day;
        this->dayStartTime = NvMmClock_Time();
    }
    NvMmFall_AdjustScaleAndPosition(this);
}

static void NvMmFall_MoonDraw(Actor* thisx, PlayState* play);
static void NvMmFall_LodMoonDrawWithLerp(Actor* thisx, PlayState* play);
static void NvMmFall_LodMoonDrawWithoutLerp(Actor* thisx, PlayState* play);

void NvMmFall_Init(Actor* thisx, PlayState* play) {
    NvMmFall* this = (NvMmFall*)thisx;

    this->eyeGlowIntensity = 0.0f;
    switch (EN_FALL_SCALE(&this->actor)) {
        case 1:
            this->scale = 0.08f;
            break;
        case 2:
            this->scale = 0.04f;
            break;
        case 3:
            this->scale = 0.02f;
            break;
        case 4:
            this->scale = 0.01f;
            break;
        default:
            this->scale = 0.16f;
            break;
    }
    this->func = NvMmFall_MoonDefault;
    switch (EN_FALL_TYPE(&this->actor)) {
        case EN_FALL_TYPE_STOPPED_MOON_CLOSED_MOUTH: // after the giants stop it: no such event yet
            Actor_Kill(&this->actor);
            return;
        case EN_FALL_TYPE_CLOCK_TOWER_MOON:
            this->func = NvMmFall_DoNothing;
            Actor_SetScale(&this->actor, this->scale * 3.0f);
            this->actor.draw = NvMmFall_MoonDraw;
            break;
        case EN_FALL_TYPE_LODMOON_NO_LERP:
            this->actor.draw = NvMmFall_LodMoonDrawWithoutLerp;
            this->dayStartTime = 0x4000;
            this->currentDay = NvMmClock_Day();
            NvMmFall_AdjustScaleAndPosition(this);
            break;
        case EN_FALL_TYPE_LODMOON:
        case EN_FALL_TYPE_LODMOON_INVERTED_STONE_TOWER:
            this->actor.draw = NvMmFall_LodMoonDrawWithLerp;
            this->dayStartTime = 0x4000;
            this->currentDay = NvMmClock_Day();
            NvMmFall_AdjustScaleAndPosition(this);
            break;
        case EN_FALL_TYPE_TERMINA_FIELD_MOON:
        default:
            this->actor.draw = NvMmFall_MoonDraw;
            this->dayStartTime = 0x4000;
            this->currentDay = NvMmClock_Day();
            NvMmFall_AdjustScaleAndPosition(this);
            break;
    }
}

void NvMmFall_Destroy(Actor* thisx, PlayState* play) {
}

void NvMmFall_Update(Actor* thisx, PlayState* play) {
    NvMmFall* this = (NvMmFall*)thisx;

    this->func(this, play);
}

/** The full moon (object_fall): its eyes glow by the prim colour. */
static void NvMmFall_MoonDraw(Actor* thisx, PlayState* play) {
    static Vec3f sFocusOffset = { 1800.0f, 1000.0f, 4250.0f }; // (the Moon's Tear falls from its eye)
    NvMmFall* this = (NvMmFall*)thisx;
    s32 prim = (this->eyeGlowIntensity * 200.0f) + 40.0f;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    Matrix_MultVec3f(&sFocusOffset, &this->actor.focus.pos);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0x80, prim, prim, prim, 255);
    gSPDisplayList(POLY_OPA_DISP++, DL_MOON);
    CLOSE_DISPS(play->state.gfxCtx);
}

/** The far moon (object_lodmoon): its own fog, eyes unlit, then the moon lit. */
static void NvMmFall_LodMoonDraw(Actor* thisx, PlayState* play) {
    NvMmFall* this = (NvMmFall*)thisx;
    s32 prim = (this->eyeGlowIntensity * 200.0f) + 40.0f;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    POLY_OPA_DISP = Gfx_SetFog(POLY_OPA_DISP, 20, 25, 30, 0, 0x3E7, 0x3200);
    gDPSetRenderMode(POLY_OPA_DISP++, G_RM_PASS, G_RM_AA_ZB_OPA_SURF2);
    gSPLoadGeometryMode(POLY_OPA_DISP++, G_ZBUFFER | G_SHADE | G_CULL_BACK | G_SHADING_SMOOTH);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0x80, prim, prim, prim, 255);
    gSPDisplayList(POLY_OPA_DISP++, DL_LODMOON_EYES);
    gSPLoadGeometryMode(POLY_OPA_DISP++, G_ZBUFFER | G_SHADE | G_CULL_BACK | G_LIGHTING | G_SHADING_SMOOTH);
    gSPDisplayList(POLY_OPA_DISP++, DL_LODMOON_MOON);
    POLY_OPA_DISP = Play_SetFog(play, POLY_OPA_DISP);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void NvMmFall_LodMoonDrawWithoutLerp(Actor* thisx, PlayState* play) {
    NvMmFall_LodMoonDraw(thisx, play);
}

/** Past 9000 units from the camera, the moon is drawn pulled in to 9000 along the same line. */
static void NvMmFall_LodMoonDrawWithLerp(Actor* thisx, PlayState* play) {
    f32 dist = Actor_WorldDistXYZToPoint(thisx, &play->view.eye);

    if (dist > 9000.0f) {
        f32 s = 9000.0f / dist;
        Vec3f t;

        t.x = (-(play->view.eye.x - thisx->world.pos.x) * s) + play->view.eye.x;
        t.y = (-(play->view.eye.y - thisx->world.pos.y) * s) + play->view.eye.y;
        t.z = (-(play->view.eye.z - thisx->world.pos.z) * s) + play->view.eye.z;
        Matrix_Translate(t.x, t.y, t.z, MTXMODE_NEW);
        Matrix_Scale(thisx->scale.x, thisx->scale.y, thisx->scale.z, MTXMODE_APPLY);
        Matrix_RotateY(BINANG_TO_RAD(thisx->shape.rot.y), MTXMODE_APPLY);
        Matrix_RotateX(BINANG_TO_RAD(thisx->shape.rot.x), MTXMODE_APPLY);
        Matrix_RotateZ(BINANG_TO_RAD(thisx->shape.rot.z), MTXMODE_APPLY);
    }
    NvMmFall_LodMoonDraw(thisx, play);
}
