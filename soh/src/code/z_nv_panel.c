/*
 * Nightmareverse: RE1's interactive screens (InteractiveScreen.cpp check_and_display_interactive_screen).
 *
 * A room's script puts one up by raising the scenario flag 0x20 ("an interactive screen is up")
 * with a system flag naming which: 0x1D the numeric panel (408), 0x1E the lab's computer (506),
 * 0x1F the slide projector (504; the decomp's "4070" counts the stages from 0). The computer is
 * z_nv_terminal.c.
 *
 * The panel (display_passcode_panel): the room cuts to its camera 5, the close-up of a box of
 * nine lights over nine number keys; "Will you push the keys?" (the room's message 14), no
 * backing out. Yes: a cursor on the keys (the stick / D-pad), A presses the one under it, which
 * flips its light and the ones beside it (above, below, left, right); B backs out. All nine lit,
 * the lights go out one by one in RE1's order and the panel is solved (scenario flag 0x21: the
 * room's script opens its door). The lights are the camera's sprite groups 1-9 (z_nv_mask.c, the
 * room drawn with their pixels: soh_scene.sprite_room), on while lit. Link is held meanwhile.
 *
 * The projector (LabSlides.cpp): its camera 4, the screen; each of the six slides slides in from
 * the right into the projector's light, its caption (the room's messages 0-5) is read, and it
 * slides out to the left. After the sixth the light comes back on empty once more, then the
 * staff photograph with message 6, and the room has her back. The slides are the player's
 * Data/slide.tim (re1/ui/slides: soh_ui.slides_blob).
 */
#include "global.h"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>
#include <stdio.h>
#include <string.h>

s32 NvFlags_Test(s32 bank, s32 bit);
void NvFlags_Op(s32 bank, s32 bit, s32 mode);
void NvCamera_CutLock(s32 n);
void NvCamera_CutRestore(void);
void NvSpr_Set(s32 id, s32 on);
void NvScript_Ask(PlayState* play, s32 id);
void NvSnd_At(s32 slot, s32 x, s32 z, s32 lx, s32 lz);
s32 NvScript_Say(PlayState* play, s32 id);
s32 NvTerm_Up(void);
void NvTerm_Start(PlayState* play);
void NvTerm_Update(PlayState* play);
void NvTerm_Draw(PlayState* play);

#define NV_SCN_SCREEN_UP 0x20 // scenario flags (bank 0)
#define NV_SCN_PANEL_SOLVED 0x21
#define NV_SCN_PANEL_A 0x1E
#define NV_SCN_PANEL_B 0x1F

// the lights' first states (0x004ba960): by the panel's variant and the character (Jill: the 3rd, 4th)
static const u8 sNvPanelStart[4][9] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 0, 0, 1, 1, 0, 1, 0, 1 },
    { 0, 1, 0, 1, 1, 1, 0, 0, 1 },
    { 1, 0, 1, 0, 1, 1, 0, 0, 1 },
};
// the success: lights out in this order, each after its delay (0x004ba9a0: light, delay, ...)
static const u8 sNvPanelOut[18] = { 0, 30, 1, 25, 2, 16, 5, 8, 8, 6, 7, 4, 6, 2, 3, 1, 4, 0 };

typedef enum { NV_PANEL_OFF, NV_PANEL_ASK, NV_PANEL_INPUT, NV_PANEL_WIN, NV_PANEL_DONE } NvPanelState;

static s32 sNvPanel = NV_PANEL_OFF;
static u8 sNvPanelLit[9];
static s32 sNvPanelX;
static s32 sNvPanelY;
static s32 sNvPanelTimer;
static s32 sNvPanelStep;
static s32 sNvPanelAsked;
static s8 sNvPanelHeld;

static void NvPanel_Lights(void) {
    s32 i;

    for (i = 0; i < 9; i++) {
        NvSpr_Set(i + 1, sNvPanelLit[i]);
    }
}

static void NvPanel_Finish(PlayState* play) {
    sNvPanel = NV_PANEL_OFF;
    NvCamera_CutRestore();
    NvFlags_Op(0, NV_SCN_SCREEN_UP, 1); // (the room gets her back)
    Player_SetCsAction(play, NULL, 7);
}

/** One push of the stick or the D-pad: 1 left, 2 right, 3 up, 4 down. */
static s8 NvPanel_Dir(Input* input) {
    s8 dir = 0;

    if ((input->rel.stick_x < -30) || CHECK_BTN_ALL(input->cur.button, BTN_DLEFT)) {
        dir = 1;
    } else if ((input->rel.stick_x > 30) || CHECK_BTN_ALL(input->cur.button, BTN_DRIGHT)) {
        dir = 2;
    } else if ((input->rel.stick_y > 30) || CHECK_BTN_ALL(input->cur.button, BTN_DUP)) {
        dir = 3;
    } else if ((input->rel.stick_y < -30) || CHECK_BTN_ALL(input->cur.button, BTN_DDOWN)) {
        dir = 4;
    }
    if (dir == sNvPanelHeld) {
        return 0;
    }
    sNvPanelHeld = dir;
    return dir;
}

/** passcode_panel_toggle: the key under the cursor flips its light and its neighbours'. */
static void NvPanel_Press(void) {
    s32 i = sNvPanelY * 3 + sNvPanelX;
    s32 lit = 0;
    s32 k;

    NvSnd_At(0x17, 0, 0, 0, 0); // (the room's sound for a key)
    sNvPanelLit[i] ^= 1;
    if (sNvPanelY > 0) {
        sNvPanelLit[i - 3] ^= 1;
    }
    if (sNvPanelY < 2) {
        sNvPanelLit[i + 3] ^= 1;
    }
    if (sNvPanelX > 0) {
        sNvPanelLit[i - 1] ^= 1;
    }
    if (sNvPanelX < 2) {
        sNvPanelLit[i + 1] ^= 1;
    }
    for (k = 0; k < 9; k++) {
        lit += sNvPanelLit[k];
    }
    printf("NVPANEL key %d: %d lit\n", i + 1, lit);
    fflush(stdout);
    if (lit == 9) {
        sNvPanel = NV_PANEL_WIN;
        sNvPanelTimer = 30;
        sNvPanelStep = -1;
    }
}

/* ---- the slide projector ---- */

typedef enum { NV_SLD_OFF, NV_SLD_WAIT, NV_SLD_IN, NV_SLD_CAPTION, NV_SLD_OUT } NvSlidesState;

static s32 sNvSld = NV_SLD_OFF;
static s32 sNvSldIndex;
static s32 sNvSldLoopDone;
static s32 sNvSldMsg;
static s32 sNvSldCount;
static s32 sNvSldX; // the slide's left edge (RE1's screen space, x -160..160)
static s32 sNvSldShown;
static const u8* sNvSldData;

static void NvSlides_Finish(PlayState* play) {
    sNvSld = NV_SLD_OFF;
    NvCamera_CutRestore();
    NvFlags_Op(0, NV_SCN_SCREEN_UP, 1);
    Player_SetCsAction(play, NULL, 7);
    printf("NVSLIDES done\n");
    fflush(stdout);
}

static void NvSlides_Start(PlayState* play) {
    sNvSldData = NULL;
    if (ResourceMgr_FileExists("re1/ui/slides")) {
        const u8* d = (const u8*)ResourceGetDataByName("re1/ui/slides");

        if ((d != NULL) && (memcmp(d, "RSLD", 4) == 0)) {
            sNvSldData = d;
        }
    }
    sNvSldIndex = 0;
    sNvSldLoopDone = 0;
    sNvSldMsg = 0;
    sNvSldCount = 16;
    sNvSldX = -96;
    sNvSld = NV_SLD_WAIT;
    NvCamera_CutLock(4);
    Player_SetCsAction(play, NULL, 8);
    printf("NVSLIDES up (%s)\n", sNvSldData ? "slides" : "no slides");
    fflush(stdout);
}

/** lab_slides_update's animation, a step a frame. */
static void NvSlides_Update(PlayState* play) {
    switch (sNvSld) {
        case NV_SLD_WAIT:
            if (--sNvSldCount != 0) {
                break;
            }
            sNvSld = NV_SLD_IN;
            sNvSldX = 0x66;
            // fall through
        case NV_SLD_IN:
            sNvSldX -= 0x12;
            if (sNvSldX == 0x1E) {
                NvSnd_At(0x1B, 0, 0, 0, 0);
            }
            if (sNvSldX == -0x60) {
                sNvSld = NV_SLD_CAPTION;
                sNvSldShown = false;
            }
            break;
        case NV_SLD_CAPTION: // its caption read (an empty one goes straight on)
            if (!sNvSldShown) {
                if (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE) {
                    printf("NVSLIDES slide %d (message %d)\n", sNvSldIndex, sNvSldMsg);
                    fflush(stdout);
                    sNvSldShown = true;
                    if (NvScript_Say(play, sNvSldMsg)) {
                        break;
                    }
                } else {
                    break;
                }
            }
            if (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE) {
                sNvSld = NV_SLD_OUT;
                NvSnd_At(0x1A, 0, 0, 0, 0);
            }
            break;
        case NV_SLD_OUT:
            sNvSldX -= 0x12;
            if (sNvSldX < -0x127) {
                sNvSld = NV_SLD_WAIT;
                sNvSldCount = 16;
                if (sNvSldIndex < 5) {
                    sNvSldIndex++;
                    sNvSldMsg++;
                } else if (!sNvSldLoopDone) { // the light once more, empty, then the photograph
                    sNvSldLoopDone = true;
                    sNvSldMsg++;
                } else {
                    NvSlides_Finish(play);
                }
            }
            break;
    }
}

/** The projector's light on the screen: the slide, clipped to its window (-103..102, -72..66). */
static void NvSlides_Draw(PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    s32 x0 = 160 + sNvSldX;
    s32 lx0 = 160 - 103;
    s32 lx1 = 160 + 102;
    s32 y0 = 120 - 66;
    u16 w;
    u16 h;
    u16 n;
    s32 dim;
    s32 y;

    if ((sNvSld == NV_SLD_OFF) || (sNvSld == NV_SLD_WAIT)) {
        return;
    }
    dim = !sNvSldLoopDone && (sNvSldIndex == 5); // (its first showing: the light alone)
    OPEN_DISPS(gfxCtx);
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetScissor(OVERLAY_DISP++, G_SC_NON_INTERLACE, lx0, 120 - 72, lx1, 120 + 66);
    if (dim || (sNvSldData == NULL)) {
        gDPSetCycleType(OVERLAY_DISP++, G_CYC_1CYCLE);
        gDPSetRenderMode(OVERLAY_DISP++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
        gDPSetCombineMode(OVERLAY_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 240, 56);
        gDPFillRectangle(OVERLAY_DISP++, MAX(x0, lx0), y0, MIN(x0 + 191, lx1), y0 + 127);
    } else {
        const u16* px;

        memcpy(&n, sNvSldData + 4, 2);
        memcpy(&w, sNvSldData + 6, 2);
        memcpy(&h, sNvSldData + 8, 2);
        px = (const u16*)(sNvSldData + 10) + (size_t)MIN(sNvSldIndex, n - 1) * w * h;
        gDPSetCycleType(OVERLAY_DISP++, G_CYC_1CYCLE);
        gDPSetRenderMode(OVERLAY_DISP++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
        gDPSetCombineLERP(OVERLAY_DISP++, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, 1, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, 1);
        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 224, 224, 224, 255); // (RE1's tint 0x70 of 0x80)
        gDPSetTexturePersp(OVERLAY_DISP++, G_TP_NONE);
        gDPSetTextureFilter(OVERLAY_DISP++, G_TF_POINT);
        gDPSetTextureLUT(OVERLAY_DISP++, G_TT_NONE);
        for (y = 0; y < h; y += 8) {
            s32 rows = MIN(8, h - y);

            gDPLoadTextureBlock(OVERLAY_DISP++, px + y * w, G_IM_FMT_RGBA, G_IM_SIZ_16b, w, rows, 0,
                                G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                                G_TX_NOLOD, G_TX_NOLOD);
            gSPTextureRectangle(OVERLAY_DISP++, x0 << 2, (y0 + y) << 2, (x0 + w) << 2, (y0 + y + rows) << 2,
                                G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
        }
    }
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetScissor(OVERLAY_DISP++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    CLOSE_DISPS(gfxCtx);
}

/* ---- the panel ---- */

/** Each frame (z_nv_script.c NvScript_Update): the screen the room's script has put up. */
void NvPanel_Update(PlayState* play) {
    Input* input = &play->state.input[0];
    s8 dir;

    if (sNvSld != NV_SLD_OFF) {
        NvSlides_Update(play);
        return;
    }
    if (NvTerm_Up()) {
        NvTerm_Update(play);
        return;
    }
    // check_and_display_interactive_screen: the system flag picks the screen, 0x1D first
    if ((sNvPanel == NV_PANEL_OFF) && NvFlags_Test(0, NV_SCN_SCREEN_UP) && !NvFlags_Test(4, 0x1D)) {
        if (NvFlags_Test(4, 0x1E)) {
            NvTerm_Start(play);
        } else if (NvFlags_Test(4, 0x1F)) {
            NvSlides_Start(play);
        }
        return;
    }
    if (sNvPanel == NV_PANEL_OFF) {
        const u8* start = sNvPanelStart[0];
        s32 k;

        if (!NvFlags_Test(0, NV_SCN_SCREEN_UP) || !NvFlags_Test(4, 0x1D)) {
            return;
        }
        // passcode_panel_init: its camera, the lights as they start, the question
        if (NvFlags_Test(0, NV_SCN_PANEL_A)) {
            start = sNvPanelStart[2]; // (Jill's)
        } else if (NvFlags_Test(0, NV_SCN_PANEL_B)) {
            start = sNvPanelStart[3];
        }
        for (k = 0; k < 9; k++) {
            sNvPanelLit[k] = start[k];
        }
        sNvPanelX = sNvPanelY = 0;
        sNvPanelHeld = 0;
        sNvPanelAsked = false;
        NvCamera_CutLock(5);
        Player_SetCsAction(play, NULL, 8);
        sNvPanel = NV_PANEL_ASK;
        printf("NVPANEL up %d%d%d %d%d%d %d%d%d\n", start[0], start[1], start[2], start[3], start[4], start[5],
               start[6], start[7], start[8]);
        fflush(stdout);
    }
    NvPanel_Lights();
    switch (sNvPanel) {
        case NV_PANEL_ASK:
            if (!sNvPanelAsked) {
                if (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE) {
                    NvScript_Ask(play, 0x0E);
                    sNvPanelAsked = true;
                }
                break;
            }
            if ((Message_GetState(&play->msgCtx) == TEXT_STATE_CHOICE) && Message_ShouldAdvance(play)) {
                s32 yes = (play->msgCtx.choiceIndex == 0);

                Message_CloseTextbox(play);
                if (yes) {
                    sNvPanel = NV_PANEL_INPUT;
                    sNvPanelHeld = 3; // (the press that answered is still down)
                } else {
                    NvPanel_Finish(play);
                }
            } else if (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE) {
                NvPanel_Finish(play); // (closed without answering)
            }
            break;
        case NV_PANEL_INPUT:
            dir = NvPanel_Dir(input);
            if ((dir == 1) && (sNvPanelX > 0)) {
                sNvPanelX--;
            } else if ((dir == 2) && (sNvPanelX < 2)) {
                sNvPanelX++;
            } else if ((dir == 3) && (sNvPanelY > 0)) {
                sNvPanelY--;
            } else if ((dir == 4) && (sNvPanelY < 2)) {
                sNvPanelY++;
            }
            if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
                NvPanel_Press();
            } else if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
                NvPanel_Finish(play);
            }
            break;
        case NV_PANEL_WIN: // the lights out, one by one
            if (--sNvPanelTimer > 0) {
                break;
            }
            sNvPanelStep += (sNvPanelStep < 0) ? 1 : 2;
            if ((sNvPanelStep >= 0) && (sNvPanelStep < 18)) {
                sNvPanelLit[sNvPanelOut[sNvPanelStep]] = 0;
                sNvPanelTimer = (sNvPanelStep + 1 < 18) ? sNvPanelOut[sNvPanelStep + 1] : 0;
            }
            if (sNvPanelTimer == 0) {
                sNvPanel = NV_PANEL_DONE;
                sNvPanelTimer = 30;
            }
            break;
        case NV_PANEL_DONE: // passcode_panel_animation_finish: the room's script sees it solved
            if (--sNvPanelTimer == 0) {
                NvSnd_At(0x18, 0, 0, 0, 0);
                NvFlags_Op(0, NV_SCN_PANEL_SOLVED, 0);
                printf("NVPANEL solved\n");
                fflush(stdout);
                NvPanel_Finish(play);
            }
            break;
    }
}

s32 NvPanel_Up(void) {
    return (sNvPanel != NV_PANEL_OFF) || (sNvSld != NV_SLD_OFF) || NvTerm_Up();
}

/** Over the room (z_play.c): the keys' numbers, the one under the cursor lit (passcode_panel_draw). */
void NvPanel_Draw(PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    GfxPrint printer;
    Gfx* gfx;
    s32 r;
    s32 c;

    NvSlides_Draw(play);
    NvTerm_Draw(play);
    if (sNvPanel != NV_PANEL_INPUT) {
        return;
    }
    OPEN_DISPS(gfxCtx);
    gfx = OVERLAY_DISP;
    GfxPrint_Init(&printer);
    GfxPrint_Open(&printer, gfx);
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 3; c++) {
            s32 here = (sNvPanelX == c) && (sNvPanelY == r);

            if (here) {
                GfxPrint_SetColor(&printer, 255, 220, 60, 255);
            } else {
                GfxPrint_SetColor(&printer, 120, 120, 120, 255);
            }
            // (RE1 prints them at x 0x8b + 16 * column, y 14 * (row + 11), over the keys' art)
            GfxPrint_SetPosPx(&printer, 0x8B + 16 * c, 14 * (r + 11));
            GfxPrint_Printf(&printer, "%d", r * 3 + c + 1);
        }
    }
    gfx = GfxPrint_Close(&printer);
    GfxPrint_Destroy(&printer);
    OVERLAY_DISP = gfx;
    CLOSE_DISPS(gfxCtx);
}
