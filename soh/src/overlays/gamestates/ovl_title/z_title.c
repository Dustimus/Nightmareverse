/*
 * File: z_title.c
 * Overlay: ovl_title
 * Description: Displays the Nintendo Logo
 */

#include "global.h"
#include "alloca.h"
#include "textures/nintendo_rogo_static/nintendo_rogo_static.h"
#include "assets/objects/gameplay_keep/gameplay_keep.h"
#include <soh/Enhancements/bootcommands.h>
#include <soh/GameVersions.h>
#include <soh/SaveManager.h>
#include "soh/ResourceManagerHelpers.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "time.h"

// Note: In other rom versions this function also updates unk_1D4, coverAlpha, addAlpha, visibleDuration to calculate
// the fade-in/fade-out + the duration of the n64 logo animation
void Title_Calc(TitleContext* this) {
    if ((this->coverAlpha == 0) && (this->visibleDuration != 0)) {
        this->visibleDuration--;
        this->unk_1D4--;
        if (this->unk_1D4 == 0) {
            this->unk_1D4 = 400;
        }
    } else {
        this->coverAlpha += this->addAlpha;
        if (this->coverAlpha <= 0) {
            this->coverAlpha = 0;
            this->addAlpha = 3;
        } else if (this->coverAlpha >= 0xFF) {
            this->coverAlpha = 0xFF;
            this->exit = 1;
        }
    }
    this->uls = this->ult & 0x7F;
    this->ult++;
}

void Title_SetupView(TitleContext* this, f32 x, f32 y, f32 z) {
    View* view = &this->view;
    Vec3f eye;
    Vec3f lookAt;
    Vec3f up;

    eye.x = x;
    eye.y = y;
    eye.z = z;
    up.x = up.z = 0.0f;
    lookAt.x = lookAt.y = lookAt.z = 0.0f;
    up.y = 1.0f;

    func_800AA460(view, 30.0f, 10.0f, 12800.0f);
    func_800AA358(view, &eye, &lookAt, &up);
    func_800AAA50(view, 0xF);
}

void Title_Draw(TitleContext* this) {
    static s16 sTitleRotY = 0;
    static Lights1 sTitleLights = gdSPDefLights1(0x64, 0x64, 0x64, 0xFF, 0xFF, 0xFF, 0x45, 0x45, 0x45);

    u16 y;
    u16 idx;
    s32 pad1;
    Vec3f v3;
    Vec3f v1;
    Vec3f v2;
    s32 pad2[2];

    OPEN_DISPS(this->state.gfxCtx);

    v3.x = 69;
    v3.y = 69;
    v3.z = 69;
    v2.x = -4949.148;
    v2.y = 4002.5417;
    v1.x = 0;
    v1.y = 0;
    v1.z = 0;
    v2.z = 1119.0837;

    func_8002EABC(&v1, &v2, &v3, this->state.gfxCtx);
    gSPSetLights1(POLY_OPA_DISP++, sTitleLights);
    Title_SetupView(this, 0, 150.0, 300.0);
    Gfx_SetupDL_25Opa(this->state.gfxCtx);
    Matrix_Translate(-53.0, -5.0, 0, MTXMODE_NEW);
    Matrix_Scale(1.0, 1.0, 1.0, MTXMODE_APPLY);
    Matrix_RotateZYX(0, sTitleRotY, 0, MTXMODE_APPLY);

    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(this->state.gfxCtx), G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, gNintendo64LogoDL);
    Gfx_SetupDL_39Opa(this->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCycleType(POLY_OPA_DISP++, G_CYC_2CYCLE);
    gDPSetRenderMode(POLY_OPA_DISP++, G_RM_XLU_SURF2, G_RM_OPA_CI | CVG_DST_WRAP);
    gDPSetCombineLERP(POLY_OPA_DISP++, TEXEL1, PRIMITIVE, ENV_ALPHA, TEXEL0, 0, 0, 0, TEXEL0, PRIMITIVE, ENVIRONMENT,
                      COMBINED, ENVIRONMENT, COMBINED, 0, PRIMITIVE, 0);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 255, 128);

    gDPLoadMultiBlock(POLY_OPA_DISP++, nintendo_rogo_static_Tex_001800, 0x100, 1, G_IM_FMT_I, G_IM_SIZ_8b, 32, 32, 0,
                      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 5, 5, 2, 11);

    for (idx = 0, y = 94; idx < 16; idx++, y += 2) {
        gDPLoadMultiTile(POLY_OPA_DISP++, nintendo_rogo_static_Tex_000000, 0, G_TX_RENDERTILE, G_IM_FMT_I, G_IM_SIZ_8b,
                         192, 32, 0, idx * 2, 192 - 1, (idx + 1) * 2 - 1, 0, G_TX_NOMIRROR | G_TX_WRAP,
                         G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);

        gDPSetTileSize(POLY_OPA_DISP++, 0, 0, 0, (192 - 1) << G_TEXTURE_IMAGE_FRAC, (2 - 1) << G_TEXTURE_IMAGE_FRAC);

        gDPSetTileSize(POLY_OPA_DISP++, 1, this->uls, (this->ult & 0x7F) - idx * 4, 0, 0);
        gSPTextureRectangle(POLY_OPA_DISP++, 388, y << 2, 1156, (y + 2) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
    }

    Environment_FillScreen(this->state.gfxCtx, 0, 0, 0, (s16)this->coverAlpha, FILL_SCREEN_XLU);

    sTitleRotY += 300;

    CLOSE_DISPS(this->state.gfxCtx);
}

void Sram_InitDebugSave(void);

// Nightmareverse (dev): NV_BOOT_ENTRANCE=<entrance index> skips the title and file select and
// loads straight into that entrance on a debug save (headless room checks, quick testing)
s16 NvDoor_Entrance(const char* sceneFile, s32 spawn);

// Nightmareverse dev boot: straight into a room, from NV_BOOT_ENTRANCE (an entrance index) or,
// easier on Windows, a file nv_boot.txt in the SoH folder holding a room ("re1_106", its start
// position 0) or an entrance index ("0x670")
static s32 Title_NvDevBoot(TitleContext* this) {
    static char sLine[64];
    const char* entr = getenv("NV_BOOT_ENTRANCE");
    s32 entrance;

    if ((entr == NULL) || (entr[0] == '\0')) {
        FILE* f = fopen("nv_boot.txt", "r");

        if (f == NULL) {
            return false;
        }
        if (fscanf(f, "%63s", sLine) != 1) {
            sLine[0] = '\0';
        }
        fclose(f);
        entr = sLine;
    }
    if ((entr[0] >= '0') && (entr[0] <= '9')) {
        entrance = (s32)strtol(entr, NULL, 0);
    } else {
        char scene[80];

        snprintf(scene, sizeof(scene), "%s_scene", entr);
        entrance = NvDoor_Entrance(scene, 0);
    }
    if (entrance < 0) {
        return false;
    }
    Sram_InitDebugSave();
    gSaveContext.fileNum = 0xFF;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.entranceIndex = entrance;
    gSaveContext.dayTime = 0x8000;
    gSaveContext.respawnFlag = 0;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].entranceIndex = ENTR_LOAD_OPENING;
    gSaveContext.showTitleCard = false;
    this->state.running = false;
    SET_NEXT_GAMESTATE(&this->state, Play_Init, PlayState);
    return true;
}

void Title_Main(GameState* thisx) {
    TitleContext* this = (TitleContext*)thisx;

    OPEN_DISPS(this->state.gfxCtx);

    gSPSegment(POLY_OPA_DISP++, 0, NULL);
    gSPSegment(POLY_OPA_DISP++, 1, this->staticSegment);
    Gfx_SetupFrame(this->state.gfxCtx, 0, 0, 0);
    Title_Calc(this);
    Title_Draw(this);

    if (this->exit) {
        gSaveContext.seqId = (u8)NA_BGM_DISABLED;
        gSaveContext.natureAmbienceId = 0xFF;
        gSaveContext.gameMode = GAMEMODE_TITLE_SCREEN;
        this->state.running = false;
        SET_NEXT_GAMESTATE(&this->state, Opening_Init, OpeningContext);
    }

    CLOSE_DISPS(this->state.gfxCtx);
}

void Title_Destroy(GameState* thisx) {
    TitleContext* this = (TitleContext*)thisx;

    Sram_InitSram(&this->state);
}

void Title_Init(GameState* thisx) {
    TitleContext* this = (TitleContext*)thisx;

    this->staticSegment = NULL;
    osSyncPrintf("z_title.c\n");

    R_UPDATE_RATE = 1;
    Matrix_Init(&this->state);
    View_Init(&this->view, this->state.gfxCtx);
    this->state.main = Title_Main;
    this->state.destroy = Title_Destroy;
    this->exit = false;
    gSaveContext.fileNum = 0xFF;
    this->ult = 0;
    this->unk_1D4 = 0x14;
    this->coverAlpha = 255;
    this->addAlpha = -3;
    this->visibleDuration = 0x3C;

    GameInteractor_ExecuteOnZTitleInit(this);
    Title_NvDevBoot(this); // (after SoH's own title hooks, so it wins)
}
