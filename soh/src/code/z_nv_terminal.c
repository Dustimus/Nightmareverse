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
 * Drawn from RE1's own pictures (re1/ui/computer: umb00, umb01 and umbrella.tim from the player's
 * Data folder, soh_ui.computer_blob) where the decomp's tables put them: the windows' frames and
 * their lines ("Login:", "Basement Doorlock Operation", "B2"...), the keyboard and its lit key, the
 * typed letters and the caret, the status plates ("Login incorrect.", "No Match!", "Verified!"),
 * the floor menu's bar and "Quit? Yes / No". Not here: the hands typing (two entities RE1 moves),
 * the windows opening and closing, the logo's boot animation, the monitor's pictures. Without the
 * pictures (an older re1.o2r) the same steps as plain text.
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
    NV_TERM_RELEASED, // the lock released, back to the menu
    NV_TERM_QUIT      // "Quit?" (cl_menu_confirm_exit)
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
static s32 sNvTermQuitSel; // "Quit?": 0 Yes, 1 No
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
                if (sNvTermSel == 2) { // Cancel: "Quit?"
                    sNvTerm = NV_TERM_QUIT;
                    sNvTermQuitSel = 0;
                } else {
                    sNvTermDoor = sNvTermSel;
                    sNvTerm = NV_TERM_CMD;
                    NvTerm_Field(0);
                    sNvTermRow = 0;
                    sNvTermCol = 1;
                }
            } else if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
                sNvTerm = NV_TERM_QUIT;
                sNvTermQuitSel = 0;
            }
            break;
        }
        case NV_TERM_QUIT: // Yes leaves the computer, No (or B) back to the floors
            if (CHECK_BTN_ALL(input->press.button, BTN_DUP)) {
                sNvTermQuitSel = 0;
            } else if (CHECK_BTN_ALL(input->press.button, BTN_DDOWN)) {
                sNvTermQuitSel = 1;
            }
            if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
                if (sNvTermQuitSel == 0) {
                    NvTerm_Finish(play);
                } else {
                    sNvTerm = NV_TERM_MENU;
                }
            } else if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
                sNvTerm = NV_TERM_MENU;
            }
            break;
        default:
            break;
    }
}

static void NvTerm_Print(GfxPrint* p, s32 col, s32 row, u8 r, u8 g, u8 b, const char* s) {
    GfxPrint_SetColor(p, r, g, b, 255);
    GfxPrint_SetPos(p, col, row);
    GfxPrint_Printf(p, "%s", s);
}

static void NvTerm_DrawText(PlayState* play) {
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

/* ---- RE1's pictures ---- */

#define NV_TERM_CX 160 // RE1's screen space is centred: x -160..160, y -120..120
#define NV_TERM_CY 120

static const u16* sNvTermPic[3]; // umb00, umb01, umbrella.tim
static u16 sNvTermPicW[3];
static u16 sNvTermPicH[3];
static s32 sNvTermPicsTried;
static const u16* sNvTermFont; // RE1's 8x14 font (fontus.tim, from re1/ui/result): the password's stars
static u16 sNvTermFontW;

static void NvTerm_LoadPics(void) {
    const u8* d = NULL;
    s32 off = 6;
    s32 i;

    sNvTermPicsTried = true;
    if (ResourceMgr_FileExists("re1/ui/computer")) {
        d = (const u8*)ResourceGetDataByName("re1/ui/computer");
    }
    if ((d == NULL) || (memcmp(d, "RCMP", 4) != 0)) {
        return;
    }
    for (i = 0; i < 3; i++) {
        memcpy(&sNvTermPicW[i], d + off, 2);
        memcpy(&sNvTermPicH[i], d + off + 2, 2);
        sNvTermPic[i] = (const u16*)(d + off + 4);
        off += 4 + sNvTermPicW[i] * sNvTermPicH[i] * 2;
    }
    if (ResourceMgr_FileExists("re1/ui/result")) { // (its fifth picture)
        const u8* r = (const u8*)ResourceGetDataByName("re1/ui/result");

        if ((r != NULL) && (memcmp(r, "RRES", 4) == 0) && ((r[4] | (r[5] << 8)) >= 5)) {
            u16 w;
            u16 h;

            off = 6;
            for (i = 0; i < 5; i++) {
                memcpy(&w, r + off, 2);
                memcpy(&h, r + off + 2, 2);
                if (i == 4) {
                    sNvTermFont = (const u16*)(r + off + 4);
                    sNvTermFontW = w;
                }
                off += 4 + w * h * 2;
            }
        }
    }
}

/**
 * A '*' of RE1's 8x8 font (fontus.tim's top rows: PrintText8x8, ch at column ch % 32, row (ch - 32) / 32)
 * at RE1's (x, y): cl_draw_text's masked letters.
 */
static Gfx* NvTerm_Star(Gfx* gfx, s32 x, s32 y) {
    static u16 sStar[8 * 8];
    static s32 sCut;
    s32 r;

    if (sNvTermFont == NULL) {
        return gfx;
    }
    if (!sCut) {
        for (r = 0; r < 8; r++) {
            memcpy(sStar + r * 8, sNvTermFont + r * sNvTermFontW + (('*' * 8) & 0xFF), 16);
        }
        sCut = true;
    }
    x += NV_TERM_CX;
    y += NV_TERM_CY;
    gDPLoadTextureBlock(gfx++, sStar, G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 8, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gSPTextureRectangle(gfx++, x << 2, y << 2, (x + 8) << 2, (y + 8) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
    return gfx;
}

// Each sprite's pixels cut out of its picture once, into a buffer of its own: drawn as plain
// texture blocks (this renderer's tile loads off a shared picture came out at the wrong place)
#define NV_TERM_CUTS 96
#define NV_TERM_POOL (320 * 1024)
typedef struct {
    s16 pic;
    u8 u;
    u8 v;
    u16 w;
    u16 h;
    u16* px;
} NvTermCut;
static NvTermCut sNvTermCuts[NV_TERM_CUTS];
static s32 sNvTermCutCount;
static u16 sNvTermPool[NV_TERM_POOL / 2];
static u32 sNvTermPoolUsed; // (in u16s)

static const u16* NvTerm_Cut(s32 pic, s32 u, s32 v, s32 w, s32 h) {
    NvTermCut* c;
    s32 i;
    s32 y;

    for (i = 0; i < sNvTermCutCount; i++) {
        c = &sNvTermCuts[i];
        if ((c->pic == pic) && (c->u == u) && (c->v == v) && (c->w == w) && (c->h == h)) {
            return c->px;
        }
    }
    if ((sNvTermCutCount >= NV_TERM_CUTS) || (sNvTermPoolUsed + (u32)(w * h) > ARRAY_COUNT(sNvTermPool)) ||
        (u + w > sNvTermPicW[pic]) || (v + h > sNvTermPicH[pic])) {
        return NULL;
    }
    c = &sNvTermCuts[sNvTermCutCount++];
    c->pic = pic;
    c->u = u;
    c->v = v;
    c->w = w;
    c->h = h;
    c->px = &sNvTermPool[sNvTermPoolUsed];
    sNvTermPoolUsed += w * h;
    for (y = 0; y < h; y++) {
        memcpy(c->px + y * w, sNvTermPic[pic] + (v + y) * sNvTermPicW[pic] + u, w * 2);
    }
    return c->px;
}

/**
 * One of RE1's sprites: texture page `page` (7-8 umb00, 9-10 umb01: 128 wide each; 0 umbrella.tim),
 * its (u, v) w x h, at RE1's (x, y).
 */
static Gfx* NvTerm_Sprite(Gfx* gfx, s32 page, s32 u, s32 v, s32 w, s32 h, s32 x, s32 y) {
    s32 pic = (page == 0) ? 2 : (page <= 8) ? 0 : 1;
    const u16* px;
    s32 r;
    s32 rows;
    s32 pw;

    if ((sNvTermPic[pic] == NULL) || (w <= 0) || (h <= 0)) {
        return gfx;
    }
    if ((page == 8) || (page == 10)) {
        u += 128;
    }
    pw = (w + 3) & ~3; // (a texture row a multiple of 8 bytes: cut a little wider, drawn w wide)
    if (u + pw > sNvTermPicW[pic]) {
        pw = w;
    }
    px = NvTerm_Cut(pic, u, v, pw, h);
    if (px == NULL) {
        return gfx;
    }
    x += NV_TERM_CX;
    y += NV_TERM_CY;
    rows = MAX(1, 2048 / pw);
    for (r = 0; r < h; r += rows) {
        s32 n = MIN(rows, h - r);

        gDPLoadTextureBlock(gfx++, px + r * pw, G_IM_FMT_RGBA, G_IM_SIZ_16b, pw, n, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                            G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gSPTextureRectangle(gfx++, x << 2, (y + r) << 2, (x + w) << 2, (y + r + n) << 2, G_TX_RENDERTILE, 0, 0,
                            1 << 10, 1 << 10);
    }
    return gfx;
}

typedef struct {
    u8 u;
    u8 v;
    u8 w; // in 8s
    u8 h; // in 16s
    u8 page;
} NvTermRow;

// the windows' lines (ComputerLab.cpp's ClRow tables)
static const NvTermRow sNvTermRowsLogin[3] = { { 0x00, 0x90, 32, 2, 9 }, { 0x00, 0xE0, 7, 1, 7 }, { 0x00, 0xF0, 12, 1, 7 } };
static const NvTermRow sNvTermRowsPanelA[3] = { { 0x00, 0xB0, 32, 1, 9 }, { 0, 0, 0, 1, 7 }, { 0x60, 0xF0, 13, 1, 7 } };
static const NvTermRow sNvTermRowsSide[3] = { { 0x38, 0xE0, 3, 1, 7 }, { 0x50, 0xE0, 3, 1, 7 }, { 0xC8, 0xE0, 7, 1, 7 } };
static const NvTermRow sNvTermRowsMenu[4] = { { 0xC8, 0xF0, 6, 1, 7 }, { 0, 0, 0, 1, 7 }, { 0xE0, 0x68, 4, 1, 7 },
                                              { 0xE0, 0x78, 4, 1, 7 } };
static const NvTermRow sNvTermRowsPanelB[4] = { { 0x00, 0xC0, 21, 1, 9 }, { 0x00, 0xD0, 21, 1, 9 }, { 0, 0, 0, 1, 7 },
                                                { 0x00, 0xF0, 12, 1, 7 } };

/** cl_draw_windows: the 9-slice frame (page 10), then its lines, each padded out with the blank cell. */
static Gfx* NvTerm_Window(Gfx* gfx, s32 x, s32 y, s32 cols, s32 rows, const NvTermRow* lines, s32 n) {
    s32 fw = (cols + 1) * 16;
    s32 fh = (rows + 1) * 16;
    s32 i;
    s32 ly;

    gfx = NvTerm_Sprite(gfx, 10, 0x50, 0x20, 16, 16, x, y);
    gfx = NvTerm_Sprite(gfx, 10, 0x70, 0x20, 16, 16, x + fw, y);
    gfx = NvTerm_Sprite(gfx, 10, 0x50, 0x40, 16, 16, x, y + fh);
    gfx = NvTerm_Sprite(gfx, 10, 0x70, 0x40, 16, 16, x + fw, y + fh);
    for (i = 0; i < cols; i++) {
        gfx = NvTerm_Sprite(gfx, 10, 0x60, 0x20, 16, 16, x + 16 + 16 * i, y);
        gfx = NvTerm_Sprite(gfx, 10, 0x60, 0x40, 16, 16, x + 16 + 16 * i, y + fh);
    }
    for (i = 0; i < rows; i++) {
        gfx = NvTerm_Sprite(gfx, 10, 0x50, 0x30, 16, 16, x, y + 16 + 16 * i);
        gfx = NvTerm_Sprite(gfx, 10, 0x70, 0x30, 16, 16, x + fw, y + 16 + 16 * i);
    }
    ly = y + 16;
    for (i = 0; i < n; i++) {
        s32 lh = lines[i].h * 16;
        s32 k;

        if (lines[i].w != 0) {
            gfx = NvTerm_Sprite(gfx, lines[i].page, lines[i].u, lines[i].v, lines[i].w * 8, lh, x + 16, ly);
        }
        for (k = lines[i].w; k < cols * 2; k++) { // (the blank cell)
            gfx = NvTerm_Sprite(gfx, 9, 0xE0, 0x30, 8, 16, x + 16 + 8 * k, ly);
        }
        ly += lh;
    }
    return gfx;
}

/** cl_draw_text: a field's letters (umb01's 16x16 cells: A-P, Q-Z), or its stars; and the caret. */
static Gfx* NvTerm_Field2(Gfx* gfx, s32 wx, s32 wy, s32 fx, s32 fy, const char* text, s32 masked, s32 caret) {
    s32 sx = fx * 8 + 16 + wx;
    s32 sy = (fy + 1) * 16 + wy;
    s32 i;

    for (i = 0; i < NV_TERM_FIELD; i++) {
        if (text[i] != '\0') {
            if (masked) {
                gfx = NvTerm_Star(gfx, sx + 16 * i + 4, sy + 4); // (PrintText8x8 at the cell + 0xA4, 0x7C)
            } else {
                s32 g = text[i] - 'A';

                gfx = NvTerm_Sprite(gfx, 9, (g * 16) & 0xFF, g & 0x10, 16, 16, sx + 16 * i, sy);
            }
        }
    }
    if (caret && ((sNvTermFrames & 16) == 0)) { // cl_draw_caret: an underline where the next letter goes
        s32 pos = MIN(sNvTermCaret, NV_TERM_FIELD - 1);

        gfx = NvTerm_Sprite(gfx, 9, 0xB0, 0x10, 16, 16, sx + 16 * pos, sy);
    }
    return gfx;
}

/** cl_draw_keyboard: the keys (and their frame), the one under the cursor lit. */
static Gfx* NvTerm_DrawKeys(Gfx* gfx) {
    static const s16 sFrame[10][8] = { // s_keyboardFrame: x, y, u, v, w, h, vertical, repeat
        { 16, 16, 0x10, 0x20, 192, 96, 0, 0 }, { 8, 8, 0x00, 0x48, 16, 8, 0, 12 }, { 200, 8, 0x00, 0x48, 8, 8, 0, 0 },
        { 8, 16, 0x08, 0x50, 8, 16, 0, 0 },    { 8, 32, 0x08, 0x60, 8, 16, 1, 5 },  { 8, 112, 0x08, 0x70, 8, 8, 0, 0 },
        { 16, 112, 0x20, 0x80, 16, 8, 0, 11 }, { 192, 112, 0x30, 0x80, 24, 8, 0, 0 }, { 208, 8, 0x50, 0x80, 8, 16, 1, 6 },
        { 208, 104, 0x50, 0x80, 8, 8, 0, 0 },
    };
    s32 i;
    s32 k;
    s32 cell = sNvTermRow * 8 + sNvTermCol;
    s32 w = 24;
    s32 h = 24;

    for (i = 0; i < 10; i++) {
        s32 n = MAX(1, sFrame[i][7]);

        for (k = 0; k < n; k++) {
            gfx = NvTerm_Sprite(gfx, 9, sFrame[i][2], sFrame[i][3], sFrame[i][4], sFrame[i][5],
                                -0x68 + sFrame[i][0] + (sFrame[i][6] ? 0 : k * sFrame[i][4]),
                                -0x0E + sFrame[i][1] + (sFrame[i][6] ? k * sFrame[i][5] : 0));
        }
    }
    if (cell == 0x17) { // ENTER, two keys tall; DEL two wide
        h = 48;
    }
    if (cell == 0x1D) {
        w = 48;
    }
    if (((sNvTermFrames & 16) == 0) || (sNvTermRepeat > 0)) { // (the lit key: drawn again, brighter)
        gDPPipeSync(gfx++);
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 160, 255);
        gfx = NvTerm_Sprite(gfx, 9, sNvTermCol * 24 + 16, sNvTermRow * 24 + 32, w, h, sNvTermCol * 24 - 0x58,
                            sNvTermRow * 24 + 2);
        gDPPipeSync(gfx++);
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    }
    return gfx;
}

void NvTerm_Draw(PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    Gfx* gfx;
    s32 status = 0;

    if (sNvTerm == NV_TERM_OFF) {
        return;
    }
    if (!sNvTermPicsTried) {
        NvTerm_LoadPics();
    }
    if (sNvTermPic[0] == NULL) {
        NvTerm_DrawText(play);
        return;
    }
    OPEN_DISPS(gfxCtx);
    gfx = OVERLAY_DISP;
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPSetCombineLERP(gfx++, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0,
                      PRIMITIVE, 0);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    switch (sNvTerm) {
        case NV_TERM_USER:
        case NV_TERM_PASS:
        case NV_TERM_REFUSED:
        case NV_TERM_GRANTED: // the log-in window (its password line once the name's in) and the keyboard
        {
            NvTermRow login[3];

            memcpy(login, sNvTermRowsLogin, sizeof(login));
            if (sNvTerm == NV_TERM_USER) { // (the password line blank until the name's in: s_rowsLogin[2].width 0)
                login[2].w = 0;
            }
            gfx = NvTerm_Window(gfx, -0x90, -0x68, 0x10, 4, login, 3);
        }
            gfx = NvTerm_Field2(gfx, -0x90, -0x68, 7, 2, sNvTermText[0], false, sNvTerm == NV_TERM_USER);
            if (sNvTerm != NV_TERM_USER) {
                gfx = NvTerm_Field2(gfx, -0x90, -0x68, 0x0C, 3, sNvTermText[1], true, sNvTerm == NV_TERM_PASS);
            }
            if ((sNvTerm == NV_TERM_USER) || (sNvTerm == NV_TERM_PASS)) {
                gfx = NvTerm_DrawKeys(gfx);
            }
            status = (sNvTerm == NV_TERM_REFUSED) ? 4 : 0;
            break;
        case NV_TERM_MENU:
        case NV_TERM_QUIT: // "Basement Doorlock Operation / Select Floor", B2 / B3 / Cancel and its bar
            gfx = NvTerm_Window(gfx, -0x90, -0x68, 0x10, 3, sNvTermRowsPanelA, 3);
            gfx = NvTerm_Window(gfx, -0x10, -0x38, 4, 3, sNvTermRowsSide, 3);
            // (the bar is see-through over its line: RE1 draws it blended, flags 0x40000000)
            if (sNvTerm == NV_TERM_MENU) {
                gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 110);
                gfx = NvTerm_Sprite(gfx, 0, 0, 0xC6, 0x49, 0x11, -0x10 + 8, sNvTermSel * 0x12 - 0x38 + 0x0C);
            } else { // "Quit? Yes / No"
                gfx = NvTerm_Window(gfx, 0x10, -0x28, 3, 4, sNvTermRowsMenu, 4);
                gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 110);
                gfx = NvTerm_Sprite(gfx, 0, 0, 0xC6, 0x3B, 0x11, 0x10 + 8, (sNvTermQuitSel + 2) * 16 - 0x28 + 0x0E);
            }
            gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
            break;
        case NV_TERM_CMD:
        case NV_TERM_BADCMD:
        case NV_TERM_RELEASED: // "Permission Denied! Lock / Try Enter Password", its field and the keyboard
            gfx = NvTerm_Window(gfx, -0x90, -0x68, 0x10, 4, sNvTermRowsPanelB, 4);
            gfx = NvTerm_Field2(gfx, -0x90, -0x68, 0x0C, 3, sNvTermText[0], true, sNvTerm == NV_TERM_CMD);
            if (sNvTerm == NV_TERM_CMD) {
                gfx = NvTerm_DrawKeys(gfx);
            }
            status = (sNvTerm == NV_TERM_BADCMD) ? 2 : (sNvTerm == NV_TERM_RELEASED) ? 3 : 0;
            break;
        default:
            break;
    }
    // cl_draw_status: the plate over the windows (2 "No Match!", 3 "Verified!", 4 "Login incorrect.")
    if (status == 2) {
        gfx = NvTerm_Sprite(gfx, 7, 0x78, 0x88, 100, 32, -0x30, -0x3D);
    } else if (status == 3) {
        gfx = NvTerm_Sprite(gfx, 7, 0x78, 0xA8, 100, 32, -0x30, -0x3D);
    } else if (status == 4) {
        gfx = NvTerm_Sprite(gfx, 7, 0x78, 0xC8, 100, 24, -0x30, -0x3D);
        gfx = NvTerm_Sprite(gfx, 7, 0x78, 0xA0, 100, 8, -0x30, -0x3D + 24);
    }
    if ((sNvTerm == NV_TERM_RELEASED) && (sNvTermTimer < 60)) { // cl_draw_door_picture: the floor's map
        gfx = NvTerm_Sprite(gfx, 0, 0, (sNvTermDoor == 0) ? 100 : 0, 0x54, 0x61, (sNvTermDoor == 0) ? -0x26 : -0x28,
                            (sNvTermDoor == 0) ? -0x5D : -0x5C);
    }
    gDPPipeSync(gfx++);
    OVERLAY_DISP = gfx;
    CLOSE_DISPS(gfxCtx);
}
