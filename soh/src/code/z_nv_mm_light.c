/*
 * Nightmareverse: MM's flames and lamps, from MM's En_Light (zeldaret/mm, ovl_En_Light), ported
 * closely (mm/README.md): a flame drawn facing the camera (MM's gEffFire1DL, or for negative params
 * the small flame gameplay_keep_DL_01ACF0: candles and lamps), with its point light flickering.
 * MM's params: & 0xF the color, 0x800 lit by a switch flag ((params & 0x7F0) >> 4; with 0x1000 lit
 * while it's set, else while it's clear), 0x2000 a light only (no flame, no glow), 0x4000 no light.
 */
#include "global.h"

#define ENLIGHT_GET_1(thisx) ((thisx)->params & 1)
#define ENLIGHT_GET_F(thisx) ((thisx)->params & 0xF)
#define ENLIGHT_GET_800(thisx) ((thisx)->params & 0x800)
#define ENLIGHT_SWITCH_FLAG(thisx) (((thisx)->params & 0x7F0) >> 4)
#define ENLIGHT_GET_1000(thisx) ((thisx)->params & 0x1000)
#define ENLIGHT_GET_2000(thisx) ((thisx)->params & 0x2000)
#define ENLIGHT_GET_4000(thisx) ((thisx)->params & 0x4000)

#define DL_FIRE ((Gfx*)"__OTR__mm/objects/gameplay_keep/gEffFire1DL")
#define DL_SMALL_FLAME ((Gfx*)"__OTR__mm/objects/gameplay_keep/gameplay_keep_DL_01ACF0")

typedef struct NvMmLight {
    Actor actor;
    u8 timer; // (MM's unk_144)
    LightNode* lightNode;
    LightInfo lightInfo;
} NvMmLight;

const size_t gNvMmLightSize = sizeof(NvMmLight);

// MM's D_808666D0: prim color, env color, scale (x 0.0001)
static struct {
    Color_RGBA8 prim;
    Color_RGB8 env;
    u8 scale;
} sColors[] = {
    { { 255, 200, 0, 255 }, { 255, 0, 0 }, 75 },     { { 255, 235, 175, 255 }, { 255, 0, 0 }, 75 },
    { { 0, 170, 255, 255 }, { 0, 0, 255 }, 75 },     { { 170, 255, 0, 255 }, { 0, 150, 0 }, 75 },
    { { 255, 200, 0, 255 }, { 255, 0, 0 }, 40 },     { { 255, 200, 0, 255 }, { 255, 0, 0 }, 75 },
    { { 170, 255, 0, 255 }, { 0, 150, 0 }, 75 },     { { 0, 170, 255, 255 }, { 0, 0, 255 }, 75 },
    { { 255, 0, 170, 255 }, { 200, 0, 0 }, 75 },     { { 255, 255, 170, 255 }, { 255, 50, 0 }, 75 },
    { { 255, 255, 170, 255 }, { 255, 255, 0 }, 75 }, { { 255, 255, 170, 255 }, { 100, 255, 0 }, 75 },
    { { 255, 170, 255, 255 }, { 255, 0, 100 }, 75 }, { { 255, 170, 255, 255 }, { 100, 0, 255 }, 75 },
    { { 170, 255, 255, 255 }, { 0, 0, 255 }, 75 },   { { 170, 255, 255, 255 }, { 0, 150, 255 }, 75 },
};

static void NvMmLight_SwitchedUpdate(Actor* thisx, PlayState* play);

void NvMmLight_Init(Actor* thisx, PlayState* play) {
    NvMmLight* this = (NvMmLight*)thisx;

    if (!ENLIGHT_GET_4000(thisx)) {
        if (ENLIGHT_GET_2000(thisx)) {
            Lights_PointNoGlowSetInfo(&this->lightInfo, thisx->world.pos.x,
                                      ((thisx->params < 0) ? 1 : 40) + (s32)thisx->world.pos.y, thisx->world.pos.z,
                                      255, 255, 180, -1);
        } else {
            Lights_PointGlowSetInfo(&this->lightInfo, thisx->world.pos.x,
                                    ((thisx->params < 0) ? 1 : 40) + (s32)thisx->world.pos.y, thisx->world.pos.z, 255,
                                    255, 180, -1);
        }
        this->lightNode = LightContext_InsertLight(play, &play->lightCtx, &this->lightInfo);
    }
    Actor_SetScale(thisx, sColors[ENLIGHT_GET_F(thisx)].scale * 0.0001f);
    this->timer = (s8)(Rand_ZeroOne() * 255.0f);
    if (ENLIGHT_GET_800(thisx)) {
        thisx->update = NvMmLight_SwitchedUpdate;
        if (ENLIGHT_GET_1000(thisx) && Flags_GetSwitch(play, ENLIGHT_SWITCH_FLAG(thisx))) {
            Actor_SetScale(thisx, 0.0f);
        }
    } else if (ENLIGHT_GET_2000(thisx)) {
        thisx->draw = NULL;
    }
}

void NvMmLight_Destroy(Actor* thisx, PlayState* play) {
    NvMmLight* this = (NvMmLight*)thisx;

    if (!ENLIGHT_GET_4000(thisx)) {
        LightContext_RemoveLight(play, &play->lightCtx, this->lightNode);
    }
}

/** MM's func_80865BF8: facing the camera. */
static void NvMmLight_Face(NvMmLight* this, PlayState* play) {
    this->actor.shape.rot.y = Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)) + 0x8000;
    if (this->actor.parent != NULL) {
        this->actor.world.pos = this->actor.parent->world.pos;
        this->actor.world.pos.y += 17.0f;
    }
    this->timer++;
}

void NvMmLight_Update(Actor* thisx, PlayState* play) {
    NvMmLight* this = (NvMmLight*)thisx;

    if (!ENLIGHT_GET_4000(thisx)) {
        f32 flicker = (Rand_ZeroOne() * 0.5f) + 0.5f;
        s32 radius = (thisx->params < 0) ? 100 : ENLIGHT_GET_2000(thisx) ? 730 : 300;
        Color_RGBA8* c = &sColors[ENLIGHT_GET_F(thisx)].prim;

        Lights_PointSetColorAndRadius(&this->lightInfo, (u8)(c->r * flicker), (u8)(c->g * flicker),
                                      (u8)(c->b * flicker), radius);
    }
    NvMmLight_Face(this, play);
    if ((thisx->params >= 0) && !ENLIGHT_GET_4000(thisx)) {
        Audio_PlayActorSound2(thisx, NA_SE_EV_TORCH - SFX_FLAG);
    }
}

/** MM's func_80865F38: lit by a switch flag, growing and shrinking. */
static void NvMmLight_SwitchedUpdate(Actor* thisx, PlayState* play) {
    NvMmLight* this = (NvMmLight*)thisx;
    f32 base = sColors[ENLIGHT_GET_F(thisx)].scale * 0.0001f;
    f32 size = thisx->scale.x / base;
    s32 lit = false;

    if (ENLIGHT_GET_1000(thisx) ? Flags_GetSwitch(play, ENLIGHT_SWITCH_FLAG(thisx))
                                : !Flags_GetSwitch(play, ENLIGHT_SWITCH_FLAG(thisx))) {
        Math_StepToF(&size, 1.0f, 0.05f);
        lit = true;
    } else {
        if (size < 0.1f) {
            size = 0.0f;
        }
        Math_StepToF(&size, 0.0f, 0.05f);
    }
    Actor_SetScale(thisx, base * size);
    if (!ENLIGHT_GET_4000(thisx)) {
        f32 flicker = (Rand_ZeroOne() * 0.5f) + 0.5f;
        Color_RGBA8* c = &sColors[ENLIGHT_GET_F(thisx)].prim;

        Lights_PointSetColorAndRadius(&this->lightInfo, (u8)(c->r * flicker), (u8)(c->g * flicker),
                                      (u8)(c->b * flicker), 300.0f * size);
    }
    NvMmLight_Face(this, play);
    if ((thisx->params >= 0) && lit) {
        Audio_PlayActorSound2(thisx, NA_SE_EV_TORCH - SFX_FLAG);
    }
}

void NvMmLight_Draw(Actor* thisx, PlayState* play) {
    NvMmLight* this = (NvMmLight*)thisx;
    Gfx* dl;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    if (thisx->params >= 0) {
        Color_RGBA8* p = &sColors[ENLIGHT_GET_F(thisx)].prim;
        Color_RGB8* e = &sColors[ENLIGHT_GET_F(thisx)].env;

        gSPSegment(POLY_XLU_DISP++, 0x08,
                   Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 0x20, 0x40, 1, 0, (this->timer * -20) & 0x1FF,
                                      0x20, 0x80, 0, 0, 0, -20));
        dl = DL_FIRE;
        gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, p->r, p->g, p->b, p->a);
        gDPSetEnvColor(POLY_XLU_DISP++, e->r, e->g, e->b, 0);
    } else {
        gSPSegment(POLY_XLU_DISP++, 0x08,
                   Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 0x10, 0x20, 1, (this->timer * 2) & 0x3F,
                                      (this->timer * -6) & 0x7F, 0x10, 0x20, 0, 0, 2, -6));
        dl = DL_SMALL_FLAME;
        gDPSetPrimColor(POLY_XLU_DISP++, 0xC0, 0xC0, 255, 200, 0, 0);
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 0, 0, 0);
    }
    Matrix_RotateY(BINANG_TO_RAD((s16)(Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)) - thisx->shape.rot.y + 0x8000)),
                   MTXMODE_APPLY);
    if (ENLIGHT_GET_1(thisx)) {
        Matrix_RotateY(M_PI, MTXMODE_APPLY);
    }
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, dl);
    CLOSE_DISPS(play->state.gfxCtx);
}
