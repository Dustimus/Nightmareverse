/*
 * Nightmareverse: the lab's computer (506; the decomp's ComputerLab.cpp display_computer_lab).
 *
 * Put up by the room's script as the other interactive screens are (z_nv_panel.c: scenario flag
 * 0x20 with the system flag 0x1E). The room's camera 4, the monitor; RE1's virtual keyboard, an
 * 8 x 4 grid (cl_keyboard_input): EXIT top left, the letters A-Z, ENTER (two keys tall) and
 * DEL (two wide), moved over with the stick or the D-pad, A presses. The log-in: the user name
 * and then the password (JOHN, ADA: s_expectUser / s_expectPass), wrong ones refused; then the
 * menu of the two locks and leaving, and for a lock its release command (MOLE: s_expectCmd).
 * Released: the locks bank's bit (2: 0x25 the first, 0x26 the second, cl_cmd_unlock) and the
 * lab's map (the room bank's 0x7C + 5).
 *
 * RE1 draws it all from its own pictures (the hands typing, the windows, the logo); here it is
 * the plain text of the same steps over the monitor.
 */
#include "global.h"
#include <stdio.h>
#include <string.h>

s32 NvFlags_Test(s32 bank, s32 bit);
void NvFlags_Op(s32 bank, s32 bit, s32 mode);
void NvCamera_CutLock(s32 n);
void NvCamera_CutRestore(void);
void NvSnd_At(s32 slot, s32 x, s32 z, s32 lx, s32 lz);

#define NV_TERM_FIELD 8

typedef enum {
    NV_TERM_OFF,
    NV_TERM_USER,    // typing the user name
    NV_TERM_PASS,    // the password
    NV_TERM_REFUSED, // "access denied", then the log-in again
    NV_TERM_GRANTED, // "access granted", then the menu
    NV_TERM_MENU,    // the two locks, and leaving
    NV_TERM_CMD,     // a lock's release command
    NV_TERM_BADCMD,  // refused, back to the menu
    NV_TERM_RELEASED // the lock released, back to the menu
} NvTermState;

static const char* const sNvTermUser = "JOHN";
static const char* const sNvTermPass = "ADA";
static const char* const sNvTermCmd = "MOLE";

static s32 sNvTerm = NV_TERM_OFF;
static char sNvTermText[2][NV_TERM_FIELD + 1]; // the user name / the command; the password
static s32 sNvTermCaret;
static s32 sNvTermRow;
static s32 sNvTermCol;
static s32 sNvTermSel; // the menu's line
static s32 sNvTermDoor;
static s32 sNvTermTimer;
static s32 sNvTermRepeat;
static u16 sNvTermLatch;
static s32 sNvTermFrames;
static ActorFunc sNvTermLinkDraw; // Link's draw, off while it's up (RE1 parks the player under the floor)

s32 NvTerm_Up(void) {
    return sNvTerm != NV_TERM_OFF;
}

static void NvTerm_Field(s32 n) {
    memset(sNvTermText[n], 0, sizeof(sNvTermText[n]));
    sNvTermCaret = 0;
}

void NvTerm_Start(PlayState* play) {
    NvTerm_Field(0);
    NvTerm_Field(1);
    sNvTermRow = 0;
    sNvTermCol = 1;
    sNvTermSel = 0;
    sNvTermLatch = 0;
    sNvTerm = NV_TERM_USER;
    NvCamera_CutLock(4);
    Player_SetCsAction(play, NULL, 8);
    sNvTermLinkDraw = GET_PLAYER(play)->actor.draw;
    GET_PLAYER(play)->actor.draw = NULL;
    printf("NVTERM up\n");
    fflush(stdout);
}

static void NvTerm_Finish(PlayState* play) {
    sNvTerm = NV_TERM_OFF;
    NvCamera_CutRestore();
    if (sNvTermLinkDraw != NULL) {
        GET_PLAYER(play)->actor.draw = sNvTermLinkDraw;
    }
    NvFlags_Op(0, 0x20, 1);
    Player_SetCsAction(play, NULL, 7);
    printf("NVTERM done\n");
    fflush(stdout);
}

/** The keyboard's key at a cell: 'A'-'Z', or -1 ENTER, -2 EXIT, -3 DEL (cl_keyboard_input). */
static s32 NvTerm_Key(s32 cell) {
    if (cell == 0) {
        return -2;
    }
    if (cell == 0x17) {
        return -1;
    }
    if (cell == 0x1D) {
        return -3;
    }
    if (cell > 0x16) {
        cell--;
    }
    return (cell <= 26) ? ('A' + cell - 1) : ' ';
}

static const char* NvTerm_KeyName(s32 cell) {
    static char sOne[2];
    s32 k = NvTerm_Key(cell);

    if (k == -1) {
        return "ENT";
    }
    if (k == -2) {
        return "EXT";
    }
    if (k == -3) {
        return "DEL";
    }
    sOne[0] = (char)k;
    sOne[1] = '\0';
    return sOne;
}

/** The D-pad / stick on the keyboard, held keys repeating (8 frames, then every 15); A types. */
static s32 NvTerm_Keyboard(Input* input) {
    u16 dir = 0;
    s32 cell = sNvTermRow * 8 + sNvTermCol;

    if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
        return NvTerm_Key(cell);
    }
    if (CHECK_BTN_ALL(input->cur.button, BTN_DUP) || (input->rel.stick_y > 30)) {
        dir = 1;
    } else if (CHECK_BTN_ALL(input->cur.button, BTN_DDOWN) || (input->rel.stick_y < -30)) {
        dir = 2;
    } else if (CHECK_BTN_ALL(input->cur.button, BTN_DLEFT) || (input->rel.stick_x < -30)) {
        dir = 3;
    } else if (CHECK_BTN_ALL(input->cur.button, BTN_DRIGHT) || (input->rel.stick_x > 30)) {
        dir = 4;
    }
    if ((dir != 0) && (dir == sNvTermLatch)) {
        if (sNvTermRepeat > 0) {
            sNvTermRepeat--;
            return 0;
        }
        sNvTermRepeat = 15;
    } else {
        sNvTermRepeat = 8;
    }
    sNvTermLatch = dir;
    if ((dir == 1) && (sNvTermRow > 0)) {
        sNvTermRow--;
    } else if ((dir == 2) && (sNvTermRow < 3)) {
        sNvTermRow++;
    } else if (dir == 3) {
        sNvTermCol = (sNvTermCol + 7) % 8;
    } else if (dir == 4) {
        sNvTermCol += (cell == 0x1D) ? 2 : 1;
        sNvTermCol %= 8;
    }
    cell = sNvTermRow * 8 + sNvTermCol;
    if (cell == 0x1F) { // (inside ENTER)
        sNvTermRow--;
    }
    if (cell == 0x1E) { // (inside DEL)
        sNvTermCol--;
    }
    return 0;
}

/** A key into a field (cl_text_append: the caret stops at the eighth, which is overwritten). */
static void NvTerm_Type(char* buf, s32 key) {
    if (key == -3) {
        if ((sNvTermCaret != 0) && ((sNvTermCaret < NV_TERM_FIELD - 1) || (buf[sNvTermCaret] == '\0'))) {
            sNvTermCaret--;
        }
        buf[sNvTermCaret] = '\0';
        return;
    }
    if (key > 0) {
        buf[sNvTermCaret] = (char)key;
        if (sNvTermCaret < NV_TERM_FIELD - 1) {
            sNvTermCaret++;
        }
    }
}

void NvTerm_Update(PlayState* play) {
    Input* input = &play->state.input[0];
    s32 key;

    sNvTermFrames++;
    switch (sNvTerm) {
        case NV_TERM_USER:
        case NV_TERM_PASS:
        case NV_TERM_CMD: {
            char* buf = sNvTermText[(sNvTerm == NV_TERM_PASS) ? 1 : 0];

            key = NvTerm_Keyboard(input);
            if (key == -2) {
                if (sNvTerm == NV_TERM_CMD) {
                    sNvTerm = NV_TERM_MENU;
                } else {
                    NvTerm_Finish(play);
                }
            } else if (key == -1) {
                if (sNvTerm == NV_TERM_USER) {
                    sNvTerm = NV_TERM_PASS;
                    NvTerm_Field(1);
                } else if (sNvTerm == NV_TERM_PASS) { // cl_login_validate
                    s32 ok = (strcmp(sNvTermText[0], sNvTermUser) == 0) && (strcmp(sNvTermText[1], sNvTermPass) == 0);

                    printf("NVTERM login %s/%s: %s\n", sNvTermText[0], sNvTermText[1], ok ? "in" : "refused");
                    fflush(stdout);
                    sNvTerm = ok ? NV_TERM_GRANTED : NV_TERM_REFUSED;
                    sNvTermTimer = 90;
                    if (!ok) {
                        NvSnd_At(0x19, 0, 0, 0, 0);
                    }
                } else { // cl_cmd_validate
                    s32 ok = strcmp(sNvTermText[0], sNvTermCmd) == 0;

                    printf("NVTERM command %s: %s\n", sNvTermText[0], ok ? "released" : "refused");
                    fflush(stdout);
                    sNvTermTimer = 90;
                    if (ok) { // cl_cmd_unlock
                        s32 bit = (sNvTermDoor == 0) ? 0x25 : 0x26;

                        NvFlags_Op(8, 0x7C + 5, 0);
                        if (NvFlags_Test(2, bit)) {
                            sNvTermTimer = 32;
                        }
                        NvFlags_Op(2, bit, 0);
                        sNvTerm = NV_TERM_RELEASED;
                    } else {
                        NvSnd_At(0x19, 0, 0, 0, 0);
                        sNvTerm = NV_TERM_BADCMD;
                    }
                }
            } else if (key != 0) {
                NvTerm_Type(buf, key);
            }
            break;
        }
        case NV_TERM_REFUSED:
            if (--sNvTermTimer <= 0) {
                sNvTerm = NV_TERM_USER;
                NvTerm_Field(0);
                NvTerm_Field(1);
            }
            break;
        case NV_TERM_GRANTED:
        case NV_TERM_BADCMD:
        case NV_TERM_RELEASED:
            if (--sNvTermTimer <= 0) {
                sNvTerm = NV_TERM_MENU;
                sNvTermLatch = 0;
            }
            break;
        case NV_TERM_MENU: { // cl_menu_select: the two locks, or leaving
            s32 up = CHECK_BTN_ALL(input->press.button, BTN_DUP) || ((input->rel.stick_y > 30) && (sNvTermLatch != 1));
            s32 down =
                CHECK_BTN_ALL(input->press.button, BTN_DDOWN) || ((input->rel.stick_y < -30) && (sNvTermLatch != 2));

            sNvTermLatch = (input->rel.stick_y > 30) ? 1 : (input->rel.stick_y < -30) ? 2 : 0;
            if (up && (sNvTermSel > 0)) {
                sNvTermSel--;
            } else if (down && (sNvTermSel < 2)) {
                sNvTermSel++;
            }
            if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
                if (sNvTermSel == 2) {
                    NvTerm_Finish(play);
                } else {
                    sNvTermDoor = sNvTermSel;
                    sNvTerm = NV_TERM_CMD;
                    NvTerm_Field(0);
                    sNvTermRow = 0;
                    sNvTermCol = 1;
                }
            } else if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
                NvTerm_Finish(play);
            }
            break;
        }
        default:
            break;
    }
}

static void NvTerm_Print(GfxPrint* p, s32 col, s32 row, u8 r, u8 g, u8 b, const char* s) {
    GfxPrint_SetColor(p, r, g, b, 255);
    GfxPrint_SetPos(p, col, row);
    GfxPrint_Printf(p, "%s", s);
}

void NvTerm_Draw(PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    GfxPrint printer;
    Gfx* gfx;
    char line[48];
    s32 blink = (sNvTermFrames & 16) != 0;
    s32 typing;
    s32 r;
    s32 c;

    if (sNvTerm == NV_TERM_OFF) {
        return;
    }
    typing = (sNvTerm == NV_TERM_USER) || (sNvTerm == NV_TERM_PASS) || (sNvTerm == NV_TERM_CMD);
    OPEN_DISPS(gfxCtx);
    // the screen's glass: a dark green over the monitor
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCycleType(OVERLAY_DISP++, G_CYC_1CYCLE);
    gDPSetRenderMode(OVERLAY_DISP++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 0, 24, 8, 200);
    gDPFillRectangle(OVERLAY_DISP++, 32, 48, 288, 200);
    gDPPipeSync(OVERLAY_DISP++);
    gfx = OVERLAY_DISP;
    GfxPrint_Init(&printer);
    GfxPrint_Open(&printer, gfx);
    NvTerm_Print(&printer, 5, 7, 120, 255, 120, "UMBRELLA  RESEARCH TERMINAL");
    if ((sNvTerm == NV_TERM_USER) || (sNvTerm == NV_TERM_PASS) || (sNvTerm == NV_TERM_REFUSED) ||
        (sNvTerm == NV_TERM_GRANTED)) {
        snprintf(line, sizeof(line), "USER NAME: %s", sNvTermText[0]);
        NvTerm_Print(&printer, 6, 10, 160, 255, 160, line);
        if (sNvTerm != NV_TERM_USER) {
            char stars[NV_TERM_FIELD + 1];
            s32 k;

            for (k = 0; (k < NV_TERM_FIELD) && (sNvTermText[1][k] != '\0'); k++) {
                stars[k] = '*';
            }
            stars[k] = '\0';
            snprintf(line, sizeof(line), "PASSWORD:  %s", stars);
            NvTerm_Print(&printer, 6, 12, 160, 255, 160, line);
        }
        if (sNvTerm == NV_TERM_REFUSED) {
            NvTerm_Print(&printer, 12, 14, 255, 90, 90, "ACCESS DENIED");
        } else if (sNvTerm == NV_TERM_GRANTED) {
            NvTerm_Print(&printer, 12, 14, 120, 255, 120, "ACCESS GRANTED");
        }
    } else if (sNvTerm == NV_TERM_MENU) {
        static const char* const sItems[3] = { "RELEASE LOCK 1", "RELEASE LOCK 2", "LOG OFF" };

        for (r = 0; r < 3; r++) {
            snprintf(line, sizeof(line), "%c %s", (r == sNvTermSel) ? '>' : ' ', sItems[r]);
            if (r < 2 && NvFlags_Test(2, (r == 0) ? 0x25 : 0x26)) {
                strncat(line, "  (OPEN)", sizeof(line) - strlen(line) - 1);
            }
            NvTerm_Print(&printer, 8, 11 + 2 * r, (r == sNvTermSel) ? 255 : 140, 255, (r == sNvTermSel) ? 160 : 140,
                         line);
        }
    } else {
        snprintf(line, sizeof(line), "LOCK %d  COMMAND: %s", sNvTermDoor + 1, sNvTermText[0]);
        NvTerm_Print(&printer, 6, 10, 160, 255, 160, line);
        if (sNvTerm == NV_TERM_BADCMD) {
            NvTerm_Print(&printer, 12, 14, 255, 90, 90, "INVALID COMMAND");
        } else if (sNvTerm == NV_TERM_RELEASED) {
            NvTerm_Print(&printer, 12, 14, 120, 255, 120, "LOCK RELEASED");
        }
    }
    if (typing) { // the keyboard, 8 x 4 (ENTER two keys tall, DEL two wide)
        for (r = 0; r < 4; r++) {
            for (c = 0; c < 8; c++) {
                s32 cell = r * 8 + c;
                s32 here = (r == sNvTermRow) && (c == sNvTermCol);

                if ((cell == 0x1E) || (cell == 0x1F)) {
                    continue;
                }
                if (here && blink) {
                    GfxPrint_SetColor(&printer, 255, 255, 80, 255);
                } else if (here) {
                    GfxPrint_SetColor(&printer, 200, 200, 60, 255);
                } else {
                    GfxPrint_SetColor(&printer, 90, 200, 90, 255);
                }
                GfxPrint_SetPos(&printer, 6 + c * 4, 17 + r * 2);
                GfxPrint_Printf(&printer, "%s", NvTerm_KeyName(cell));
            }
        }
    }
    gfx = GfxPrint_Close(&printer);
    GfxPrint_Destroy(&printer);
    OVERLAY_DISP = gfx;
    CLOSE_DISPS(gfxCtx);
}
