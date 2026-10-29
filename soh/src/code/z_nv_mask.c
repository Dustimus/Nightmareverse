/*
 * Nightmareverse: RE1's foreground masks (the cut-outs of a prerendered background that
 * hide characters behind furniture), drawn exactly.
 *
 * RE1 keeps, per camera, a list of mask pieces: a screen rectangle, its shape and an ordering
 * key (view-space depth / 16). The PS1's ordering table sorts each piece against each body
 * part of a character, every part entering at its own view depth (t[2] >> 4 per part,
 * FUN_00483250; DrawRoomSpr 0x00475b80), so a pillar can hide a head and not the legs. Here
 * each piece is drawn right after the background as an invisible quad that writes only depth,
 * placed in the scene at its own view depth over its rectangle on screen (not libultraship's
 * prim depth: that register is applied when a batch is flushed, so each piece got the next
 * one's); the depth buffer then tests Link against it per pixel, a little finer than RE1's per
 * part, and the background shows through where he's behind.
 *
 * Data: re1/tools/soh_scene.py writes "<background>_mask" next to each camera background
 * (re1/tools/re1_masks.py): "RMSK", u32 count; per piece s16 x, y, u16 w, h, stride, group,
 * f32 depth (RE1's ordering key as view-space depth, OoT units); then stride * h bytes of I8
 * texels (255 solid, 0 a hole).
 */
#include "global.h"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct NvMaskPiece {
    s16 x;
    s16 y;
    u16 w;
    u16 h;
    u16 stride;
    u16 group; // RE1's sprite group: what room scripts switch off and on
    f32 depth;
} NvMaskPiece;

#define NV_MASK_TMEM 4096 // bytes of texture loaded at once (rows per load are cut to fit)

/**
 * Called after a prerendered room's background (z_room.c), bgCamIndex being the camera whose
 * background was drawn. The scene "<name>_scene" keeps camera n's mask, if it has one, at
 * scenes/shared/<name>_scene/<name>_bg<n>_mask (re1/tools/soh_scene.py).
 */
#define NV_MASK_STEPS 4096.0f // mask vertices: steps per piece depth (|offsets| stay < 32767)
#define NV_MASK_OVERLAP 0.5f  // pixels each piece reaches past its rectangle
#define NV_MASK_NEAR 1.5f     // nearest a piece is placed, in near-plane distances

s32 NvDoorScene_Covering(void);

void NvMask_Draw(PlayState* play, Gfx** gfxP, s32 bgCamIndex) {
    char path[256];
    const char* file;
    size_t len;
    u8* data;
    u32 count;
    u32 i;
    Gfx* gfx = *gfxP;

    if (NvDoorScene_Covering()) {
        return;
    }
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
    {
        // the camera's frame: a screen point (sx, sy) at view depth d is
        // eye + d * (fwd + right * (sx - 160) / k + up * (120 - sy) / k)
        Vec3f fwd;
        Vec3f right;
        Vec3f up;
        f32 l;
        f32 k = 120.0f / tanf(DEG_TO_RAD(play->view.fovy) * 0.5f);

        fwd.x = play->view.lookAt.x - play->view.eye.x;
        fwd.y = play->view.lookAt.y - play->view.eye.y;
        fwd.z = play->view.lookAt.z - play->view.eye.z;
        l = sqrtf(SQ(fwd.x) + SQ(fwd.y) + SQ(fwd.z));
        if (l < 1.0f) {
            return;
        }
        fwd.x /= l, fwd.y /= l, fwd.z /= l;
        right.x = fwd.y * play->view.up.z - fwd.z * play->view.up.y;
        right.y = fwd.z * play->view.up.x - fwd.x * play->view.up.z;
        right.z = fwd.x * play->view.up.y - fwd.y * play->view.up.x;
        l = sqrtf(SQ(right.x) + SQ(right.y) + SQ(right.z));
        right.x /= l, right.y /= l, right.z /= l;
        up.x = right.y * fwd.z - right.z * fwd.y;
        up.y = right.z * fwd.x - right.x * fwd.z;
        up.z = right.x * fwd.y - right.y * fwd.x;

        gDPPipeSync(gfx++);
        // invisible: the blender keeps the framebuffer's color (CLR_MEM), so only depth is written;
        // CVG_X_ALPHA + the alpha threshold drop the pieces' holes
        gDPSetOtherMode(gfx++,
                        G_AD_DISABLE | G_CD_DISABLE | G_CK_NONE | G_TC_FILT | G_TF_POINT | G_TT_NONE | G_TL_TILE |
                            G_TD_CLAMP | G_TP_NONE | G_CYC_1CYCLE | G_PM_NPRIMITIVE,
                        G_AC_THRESHOLD | G_ZS_PIXEL | Z_CMP | Z_UPD | ZMODE_OPA | CVG_X_ALPHA | ALPHA_CVG_SEL |
                            GBL_c1(G_BL_CLR_IN, G_BL_0, G_BL_CLR_MEM, G_BL_1) |
                            GBL_c2(G_BL_CLR_IN, G_BL_0, G_BL_CLR_MEM, G_BL_1));
        gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, 0, 0, 0, TEXEL0, 0, 0, 0, TEXEL0, 0, 0, 0, TEXEL0);
        gDPSetBlendColor(gfx++, 0, 0, 0, 1);
        gSPClearGeometryMode(gfx++, G_CULL_BOTH | G_LIGHTING | G_FOG | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR);
        gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE);
        gSPTexture(gfx++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
        for (i = 0; i < count; i++) {
            NvMaskPiece piece;
            f32 d;
            u8* texels;
            s32 rowsPerLoad;
            s32 row;

            memcpy(&piece, data, sizeof(piece));
            texels = data + sizeof(piece);
            data = texels + piece.stride * piece.h;
            if ((piece.w == 0) || (piece.h == 0) || (piece.stride == 0)) {
                continue;
            }
            // RE1 keys "always in front" pieces (a chandelier seen from above) at ~128 units,
            // nearer than the camera's near plane: kept just past it, or they'd be clipped
            d = MAX(piece.depth, play->view.zNear * NV_MASK_NEAR);
            // vertices relative to the eye, in steps of the piece's own depth / NV_MASK_STEPS (a
            // pixel is ~20 steps at any depth): fixed steps (quarter units) were several pixels
            // wide for the near pieces, and Link showed through the seams between them (208)
            Matrix_Push();
            Matrix_Translate(play->view.eye.x, play->view.eye.y, play->view.eye.z, MTXMODE_NEW);
            Matrix_Scale(d / NV_MASK_STEPS, d / NV_MASK_STEPS, d / NV_MASK_STEPS, MTXMODE_APPLY);
            gSPMatrix(gfx++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            Matrix_Pop();
            rowsPerLoad = NV_MASK_TMEM / piece.stride;
            if (rowsPerLoad < 1) {
                rowsPerLoad = 1;
            }
            for (row = 0; row < piece.h; row += rowsPerLoad) {
                s32 rows = MIN(rowsPerLoad, piece.h - row);
                Vtx* vtx = Graph_Alloc(play->state.gfxCtx, 4 * sizeof(Vtx));
                s32 c;

                // a quad in the scene at the piece's own depth, covering its rectangle on screen:
                // the depth buffer tests Link against it per pixel (RE1 sorts each body part at
                // its own view depth, t[2] >> 4 per part)
                for (c = 0; c < 4; c++) {
                    // half a pixel past the rectangle each way, so pieces overlap at their seams
                    // (the texture's clamped edge fills it)
                    f32 u = (c == 1 || c == 2) ? piece.w + NV_MASK_OVERLAP : -NV_MASK_OVERLAP;
                    f32 v = (c >= 2) ? rows + NV_MASK_OVERLAP : -NV_MASK_OVERLAP;
                    f32 dx = (piece.x + u - 160.0f) / k;
                    f32 dy = (120.0f - (piece.y + row + v)) / k;
                    vtx[c].v.ob[0] = NV_MASK_STEPS * (fwd.x + right.x * dx + up.x * dy);
                    vtx[c].v.ob[1] = NV_MASK_STEPS * (fwd.y + right.y * dx + up.y * dy);
                    vtx[c].v.ob[2] = NV_MASK_STEPS * (fwd.z + right.z * dx + up.z * dy);
                    vtx[c].v.flag = 0;
                    vtx[c].v.tc[0] = (s16)(u * 32.0f);
                    vtx[c].v.tc[1] = (s16)(v * 32.0f);
                    vtx[c].v.cn[0] = vtx[c].v.cn[1] = vtx[c].v.cn[2] = vtx[c].v.cn[3] = 255;
                }
                gDPLoadTextureBlock(gfx++, texels + row * piece.stride, G_IM_FMT_I, G_IM_SIZ_8b, piece.stride, rows, 0,
                                    G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                                    G_TX_NOLOD, G_TX_NOLOD);
                gSPVertex(gfx++, vtx, 4, 0);
                gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
            }
        }
        gDPPipeSync(gfx++);
    }
    *gfxP = gfx;
}
