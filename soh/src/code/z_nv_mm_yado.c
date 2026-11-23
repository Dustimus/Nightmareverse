/*
 * Nightmareverse: the Stock Pot Inn's upstairs windows, from MM's Obj_Yado (zeldaret/mm,
 * ovl_Obj_Yado), ported as it is (mm/README.md): MM's model (object_yado_obj), its light shaft by day
 * and by night (MM's colors in segments 9 and 0xA), and its animated material (MM's AnimatedMat_Draw:
 * our NvMmMatAnim_Draw).
 */
#include "global.h"

#define YADO(name) ("__OTR__mm/objects/object_yado_obj/" name)

typedef struct NvMmYado {
    Actor actor;
    s32 isNight;
} NvMmYado;

const size_t gNvMmYadoSize = sizeof(NvMmYado);

void NvMmMatAnim_Draw(PlayState* play, const char* path);
s32 NvMmClock_IsNight(void);

/** MM's Gfx_PrimColor: a list setting the prim color. */
static Gfx* NvMmYado_PrimColor(GraphicsContext* gfxCtx, s32 lodfrac, s32 r, s32 g, s32 b, s32 a) {
    Gfx* dl = Graph_Alloc(gfxCtx, 2 * sizeof(Gfx));

    gDPSetPrimColor(&dl[0], 0, lodfrac, r, g, b, a);
    gSPEndDisplayList(&dl[1]);
    return dl;
}

void NvMmYado_Init(Actor* thisx, PlayState* play) {
    Actor_SetScale(thisx, 0.1f);
    ((NvMmYado*)thisx)->isNight = NvMmClock_IsNight();
}

void NvMmYado_Destroy(Actor* thisx, PlayState* play) {
}

void NvMmYado_Update(Actor* thisx, PlayState* play) {
    ((NvMmYado*)thisx)->isNight = NvMmClock_IsNight();
}

void NvMmYado_Draw(Actor* thisx, PlayState* play) {
    NvMmYado* this = (NvMmYado*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    if (this->isNight) {
        gSPSegment(POLY_XLU_DISP++, 0x09, NvMmYado_PrimColor(play->state.gfxCtx, 128, 95, 95, 70, 155));
        gSPSegment(POLY_OPA_DISP++, 0x0A, NvMmYado_PrimColor(play->state.gfxCtx, 128, 0, 40, 40, 255));
    } else {
        gSPSegment(POLY_XLU_DISP++, 0x09, NvMmYado_PrimColor(play->state.gfxCtx, 128, 255, 255, 215, 110));
        gSPSegment(POLY_OPA_DISP++, 0x0A, NvMmYado_PrimColor(play->state.gfxCtx, 128, 255, 255, 215, 255));
    }
    NvMmMatAnim_Draw(play, "mm/objects/object_yado_obj/object_yado_obj_Matanimheader_0012E8");
    Gfx_DrawDListOpa(play, (Gfx*)YADO("object_yado_obj_DL_000430"));
    Gfx_DrawDListXlu(play, (Gfx*)YADO("object_yado_obj_DL_000320"));
    CLOSE_DISPS(play->state.gfxCtx);
}
