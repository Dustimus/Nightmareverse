/*
 * Nightmareverse: RE1's foreground masks (the cut-outs of a prerendered background that
 * hide characters behind furniture), drawn exactly.
 *
 * RE1 keeps, per camera, a list of mask pieces: a screen rectangle, its shape and a
 * camera-space depth. The PS1 repaints those pixels over the characters; here each piece
 * is drawn right after the background as an invisible screen-space rectangle that writes
 * only depth, at the piece's own depth (libultraship's prim depth, which the renderer takes
 * as the fragment's depth). Characters behind it then fail the depth test and the
 * background shows through, untouched: nothing has to match the background's colors, and
 * the rectangles land on RE1's own pixels at any resolution.
 *
 * Data: re1/tools/soh_scene.py writes "<background>_mask" next to each camera background:
 * "RMSK", u32 count; per piece s16 x, y, u16 w, h, stride, unused, f32 depth (camera-space,
 * OoT units); then stride * h bytes of I8 texels (255 solid, 0 a hole).
 */
#include "global.h"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>
#include <stdio.h>
#include <string.h>

typedef struct NvMaskPiece {
    s16 x;
    s16 y;
    u16 w;
    u16 h;
    u16 stride;
    u16 unused;
    f32 depth;
} NvMaskPiece;

#define NV_MASK_TMEM 4096 // bytes of texture loaded at once (rows per load are cut to fit)

// Window depth (0 near, 1 far) of a camera-space distance under the view's perspective
// projection, as 15-bit prim depth (libultraship: prim_depth / 0x7FFF becomes the depth)
static u16 NvMask_PrimDepth(f32 dist, f32 zNear, f32 zFar) {
    f32 ndc;
    f32 win;

    if (dist < zNear) {
        dist = zNear;
    }
    ndc = ((zFar + zNear) * dist - 2.0f * zFar * zNear) / ((zFar - zNear) * dist);
    win = (ndc + 1.0f) * 0.5f;
    if (win < 0.0f) {
        win = 0.0f;
    } else if (win > 1.0f) {
        win = 1.0f;
    }
    return (u16)(win * 0x7FFF);
}

/**
 * Called after a prerendered room's background (z_room.c), bgCamIndex being the camera whose
 * background was drawn. The scene "<name>_scene" keeps camera n's mask, if it has one, at
 * scenes/shared/<name>_scene/<name>_bg<n>_mask (re1/tools/soh_scene.py).
 */
void NvMask_Draw(PlayState* play, Gfx** gfxP, s32 bgCamIndex) {
    char path[256];
    const char* file;
    size_t len;
    u8* data;
    u32 count;
    u32 i;
    Gfx* gfx = *gfxP;

    if ((play->loadedScene == NULL) || (play->loadedScene->sceneFile.fileName == NULL)) {
        return;
    }
    file = play->loadedScene->sceneFile.fileName;
    len = strlen(file);
    if ((len <= 6) || (strcmp(file + len - 6, "_scene") != 0)) {
        return;
    }
    snprintf(path, sizeof(path), "scenes/shared/%s/%.*s_bg%d_mask", file, (int)(len - 6), file, (int)bgCamIndex);
    if (!ResourceMgr_FileExists(path)) {
        return;
    }
    data = (u8*)ResourceGetDataByName(path);
    if ((data == NULL) || (memcmp(data, "RMSK", 4) != 0)) {
        return;
    }
    memcpy(&count, data + 4, sizeof(count));
    data += 8;

    gDPPipeSync(gfx++);
    // invisible: the blender keeps the framebuffer's color (CLR_MEM), so only depth is written;
    // CVG_X_ALPHA + the alpha threshold drop the pieces' holes
    gDPSetOtherMode(gfx++,
                    G_AD_DISABLE | G_CD_DISABLE | G_CK_NONE | G_TC_FILT | G_TF_POINT | G_TT_NONE | G_TL_TILE |
                        G_TD_CLAMP | G_TP_NONE | G_CYC_1CYCLE | G_PM_NPRIMITIVE,
                    G_AC_THRESHOLD | G_ZS_PRIM | Z_CMP | Z_UPD | ZMODE_OPA | CVG_X_ALPHA | ALPHA_CVG_SEL |
                        GBL_c1(G_BL_CLR_IN, G_BL_0, G_BL_CLR_MEM, G_BL_1) |
                        GBL_c2(G_BL_CLR_IN, G_BL_0, G_BL_CLR_MEM, G_BL_1));
    gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, 0, 0, 0, TEXEL0, 0, 0, 0, TEXEL0, 0, 0, 0, TEXEL0);
    gDPSetBlendColor(gfx++, 0, 0, 0, 1);

    for (i = 0; i < count; i++) {
        NvMaskPiece piece;
        u8* texels;
        s32 rowsPerLoad;
        s32 row;

        memcpy(&piece, data, sizeof(piece));
        texels = data + sizeof(piece);
        data = texels + piece.stride * piece.h;
        if ((piece.w == 0) || (piece.h == 0) || (piece.stride == 0)) {
            continue;
        }
        gDPPipeSync(gfx++);
        gDPSetPrimDepth(gfx++, NvMask_PrimDepth(piece.depth, play->view.zNear, play->view.zFar), 0);
        rowsPerLoad = NV_MASK_TMEM / piece.stride;
        if (rowsPerLoad < 1) {
            rowsPerLoad = 1;
        }
        for (row = 0; row < piece.h; row += rowsPerLoad) {
            s32 rows = MIN(rowsPerLoad, piece.h - row);

            gDPLoadTextureBlock(gfx++, texels + row * piece.stride, G_IM_FMT_I, G_IM_SIZ_8b, piece.stride, rows, 0,
                                G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                                G_TX_NOLOD, G_TX_NOLOD);
            gSPTextureRectangle(gfx++, piece.x << 2, (piece.y + row) << 2, (piece.x + piece.w) << 2,
                                (piece.y + row + rows) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
        }
    }
    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PIXEL); // what follows sets its render mode, not its depth source
    *gfxP = gfx;
}
