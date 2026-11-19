/*
 * Nightmareverse: RE1's map screen, the status screen's Map tab (z_nv_status.c), as RE1's
 * MainMenu.cpp runs it (the PC decomp, ecruells/resident-evil-pc-decomp: menu_init_map_screen
 * 0x00488160, menu_map_state_machine 0x00487100, map_tab_* 0x00487200.., map_display_* 0x00487640..).
 *
 * Two views. The browser (map_tab_update): the section overview (item_m2/map0d, 0c, 00 or 0e: the
 * highest section she knows of), the known sections' markers (the current one bright), the section's
 * name and the floor's label, and blinking arrows to the floors above and below; up/down change the
 * floor, left/right the section, A opens the floor's map, B leaves. The display (map_display_update):
 * the floor's map (item_m2/map01..0b, by RE1's area) over the blue grid (Map_blue.tim), opened and
 * closed with RE1's stretch, with a scan line sweeping it at rest; B closes it, A plays it again.
 *
 * What it shows is RE1's: a floor's picture has every room in it, and each room owns six colours of
 * its palette (from entry 13; 7-12 are the floor's own). A room not visited is hidden by clearing
 * them: wholly when the area's map isn't owned, its fill only (the outline stays) when it is
 * (map_display_load_textures). The room she's in is blanked there and drawn again, red, on a second
 * layer that blinks. Visited is bank 8's g_StageRoomFlagOffset[group] + room (z_nv_script.c raises it
 * as a room loads), owned 8:0x7C + the map's index (taking one: pickup_key_event). The objective
 * highlight (map_update_objective_highlight): Jill's serum, while it's still to fetch, blinks on the
 * 1F map with RE1's marker.
 *
 * Drawn in RE1's status-screen coordinates moved into the page (NV_MAP_X, NV_MAP_Y), the pictures
 * built as RGBA32 from RE1's palettes (colour 0 clear; the fills, RE1's semi-transparent colours,
 * see-through). Ticks are RE1's 30 Hz: three every two of the pause menu's frames.
 */
#include "global.h"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>
#include <stdio.h>
#include <string.h>

s32 NvFlags_Test(s32 bank, s32 bit);
void NvScript_Where(s32* stage, s32* room, s32* camera);
void NvSound_Play(const char* name, s32 vol);

#define NV_MAP_X 48 // RE1's status-screen coordinates, moved into the page under the tabs
#define NV_MAP_Y 64

// the pictures ("re1/ui/map", re1/tools/soh_ui.py MAP_TIMS order)
#define NV_MAP_PIC_TAB 0   // + layout (0-3)
#define NV_MAP_PIC_AREA 4  // + area (0-10)
#define NV_MAP_PIC_BLUE 15
#define NV_MAP_PICS 16

typedef struct {
    u16 w, h;
    const u8* clut; // 256 PS1 colours, little-endian
    const u8* px;   // w * h indices
} NvMapPic;

static NvMapPic sPics[NV_MAP_PICS];

// ---- RE1's tables (MainMenu.cpp, the original's .data) ----

static const u8 sRoomCounts[6] = { 32, 31, 19, 18, 24, 0 };          // 0x004d31e8, per area group
static const u8 sRoomFlagBase[5] = { 0, 32, 63, 82, 100 };           // g_StageRoomFlagOffset
static const u8 sAreaTable[16] = { 2, 0, 1, 0xFF, 4, 3, 0xFF, 0xFF, // 0x004d31f0, (layout, floor)
                                   6, 5, 0xFF, 0xFF, 10, 9, 8, 7 };
static const u8 sAreaLayouts[11] = { 0, 0, 0, 1, 1, 2, 2, 3, 3, 3, 3 }; // 0x004d3200
static const u8 sAreaGroups[11] = { 0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 4 };  // 0x004d3210
static const u8 sLayoutFloors[4] = { 3, 2, 2, 4 };                    // 0x004d321c
static const u8 sLayoutOffset[4] = { 4, 4, 4, 1 };                    // 0x004d3220

// the browser's pieces (0x004d324c: screen x, y, w, h, texture u, v): the four section markers,
// the section name (v by layout), the floor label (v by floor), the up and down arrows
typedef struct {
    s16 x, y, w, h;
    u8 u, v;
} NvMapSpr;

static const NvMapSpr sBase = { 33, 32, 160, 72, 0x00, 0x00 }; // 0x004d3228
static const NvMapSpr sMarkers[4] = {
    { 121, 32, 56, 64, 0xA0, 0x00 },
    { 73, 32, 80, 64, 0x00, 0x48 },
    { 49, 32, 40, 64, 0x50, 0x48 },
    { 121, 32, 28, 64, 0xC8, 0x48 },
};
static const NvMapSpr sName = { 57, 104, 80, 16, 0x78, 0x48 };
static const NvMapSpr sFloor = { 137, 104, 24, 16, 0xE8, 0x00 };
static const NvMapSpr sUp = { 145, 96, 8, 8, 0xA0, 0x40 };
static const NvMapSpr sDown = { 145, 120, 8, 8, 0xB0, 0x40 };

// ---- the state (DAT_00ac9890's block) ----

static struct {
    u8 state, substate, blink, zoom;
    u8 layout, group, area, room;
    u8 floor[4]; // MAP_ROOM_IDX: the floor shown, per layout
    u8 move;     // MAP_MOVE_FLAGS: 1 a floor above, 2 one below
    u8 layouts;  // MAP_LAYOUT_MASK
    u8 mode;     // MAP_MODE: the objective highlight (2: the 1F map)
    u8 hlState, hlCount, timer, hlFrame;
    u16 areas; // MAP_AREA_MASK
} sMap;

static s16 sScaleX = 1, sScaleY = 200; // the zoom sprites' scale (fix 4.12)
static s32 sSettled;                    // their 0x10000000 flag: at rest, the fills see-through
static s16 sDot;                        // DAT_00ac368c: the blink's level (0x50..0xA0)
static u8 sDotBlink;                    // g_MapDotBlink
static s16 sWait;                       // DAT_00ac3694
static u8 sTabFrame;                    // state[0x11]: the arrows' blink
static s16 sScan;                       // state[0xe]: the scan line's counter (half a row a tick)
static s32 sTicks;

// the pictures as built: the overview in use, the floor's two layers, the grid
static u8 sTabTex[256 * 160 * 4];
static s32 sTabTexPic = -1;
static u8 sMapTex[2][128 * 112 * 4];
static s32 sMapTexArea = -1;
static u8 sBlueTex[128 * 104 * 4];
static s32 sBlueReady;

static s32 NvMap_Pics(void) {
    static s32 sState; // 0 untried, 1 loaded, -1 missing

    if (sState == 0) {
        const u8* d;
        u16 count;
        s32 k;
        s32 p = 6;

        sState = -1;
        if (!ResourceMgr_FileExists("re1/ui/map")) {
            return false;
        }
        d = (const u8*)ResourceGetDataByName("re1/ui/map");
        if ((d == NULL) || (memcmp(d, "RMAP", 4) != 0)) {
            return false;
        }
        memcpy(&count, d + 4, 2);
        if (count < NV_MAP_PICS) {
            return false;
        }
        for (k = 0; k < NV_MAP_PICS; k++) {
            memcpy(&sPics[k].w, d + p, 2);
            memcpy(&sPics[k].h, d + p + 2, 2);
            sPics[k].clut = d + p + 4;
            sPics[k].px = d + p + 4 + 512;
            p += 4 + 512 + sPics[k].w * sPics[k].h;
        }
        sState = 1;
    }
    return sState > 0;
}

/** Whether the map screen can be shown (the mod has its pictures). */
s32 NvMap_Ready(void) {
    return NvMap_Pics();
}

static u16 NvMap_Clut(const u8* clut, s32 i) {
    return (u16)(clut[i * 2] | (clut[i * 2 + 1] << 8));
}

/** A PS1 colour as RGBA32: 0 clear; RE1's semi-transparent colours (STP) see-through when `stp`. */
static void NvMap_Rgba(u8* o, u16 c, s32 stp) {
    if (c == 0) {
        o[0] = o[1] = o[2] = o[3] = 0;
        return;
    }
    o[0] = (u8)(((c & 31) << 3) | ((c & 31) >> 2));
    o[1] = (u8)((((c >> 5) & 31) << 3) | (((c >> 5) & 31) >> 2));
    o[2] = (u8)((((c >> 10) & 31) << 3) | (((c >> 10) & 31) >> 2));
    o[3] = (stp && (c & 0x8000)) ? 0xA0 : 0xFF;
}

static void NvMap_Build(u8* out, const NvMapPic* pic, const u16* clut, s32 rows, s32 stp) {
    s32 n = pic->w * MIN(rows, pic->h);
    s32 k;

    for (k = 0; k < n; k++) {
        NvMap_Rgba(out + k * 4, clut[pic->px[k]], stp);
    }
}

// ---- RE1's logic ----

/** map_area_known (0x004885c0): whether the area's map is owned. Areas 2, 7, 10 have none. */
static s32 NvMap_AreaKnown(s32 area) {
    static const s8 sIdx[11] = { 0, 1, -1, 2, 3, 4, 4, -1, 5, 5, -1 };

    if ((area < 0) || (area > 10) || (sIdx[area] < 0)) {
        return false;
    }
    return NvFlags_Test(8, 0x7C + sIdx[area]);
}

static s32 NvMap_Visited(s32 group, s32 room) {
    return NvFlags_Test(8, sRoomFlagBase[group] + room);
}

/** map_build_area_mask (0x00488430): the areas known (by a room visited, or a map), the sections. */
static void NvMap_BuildMasks(void) {
    s32 group;
    s32 room;
    s32 area;

    sMap.areas = 0;
    for (group = 0; group < 5; group++) {
        for (room = 0; room < sRoomCounts[group]; room++) {
            if (!NvMap_Visited(group, room)) {
                continue;
            }
            switch (group) {
                case 0:
                    sMap.areas |= 1;
                    break;
                case 1:
                    sMap.areas |= ((room < 0x1A) || (room == 0x1D)) ? 2 : 4;
                    break;
                case 2:
                    sMap.areas |= ((room < 0x06) || (room == 0x10)) ? 8 : 0x10;
                    break;
                case 3:
                    sMap.areas |= (room < 0x0D) ? 0x20 : 0x40;
                    break;
                default:
                    sMap.areas |= ((room < 0x02) || (room == 0x16)) ? 0x80
                                  : (room < 0x05)                   ? 0x100
                                  : (room < 0x13)                   ? 0x200
                                                                    : 0x400;
                    break;
            }
        }
    }
    for (area = 0; area < 11; area++) {
        if (NvMap_AreaKnown(area)) {
            sMap.areas |= 1 << area;
        }
    }
    sMap.layouts = 0;
    for (area = 0; area < 11; area++) {
        if (sMap.areas & (1 << area)) {
            sMap.layouts |= 1 << sAreaLayouts[area];
        }
    }
}

/**
 * map_update_objective_highlight (0x00488950), Jill's half (MSF_CHAR_VARIANT): her serum, objective
 * 1 (bank 1's 0x23) not yet acknowledged (bank 0's 0x49): the 1F map's highlight.
 */
static void NvMap_Objective(void) {
    sMap.mode = (NvFlags_Test(1, 0x23) && !NvFlags_Test(0, 0x49)) ? 1 : 0;
}

/** menu_init_map_screen (0x00488160): as the pause menu opens, where she is. */
void NvMap_Open(void) {
    s32 stage;
    s32 room;
    s32 cam;
    s32 i;
    s32 j;

    NvScript_Where(&stage, &room, &cam);
    memset(&sMap, 0, sizeof(sMap));
    sMap.floor[0] = sMap.floor[1] = sMap.floor[2] = 1;
    sMap.floor[3] = 3;
    sMap.room = (u8)room;
    sMap.group = (u8)(stage % 5);
    switch (sMap.group) {
        case 0:
            if ((room == 0x07) && (cam > 2) && (cam < 7)) { // the gallery's balcony
                sMap.area = 0, sMap.room = 0x1D;
            } else if ((room == 0x0F) && ((cam == 3) || (cam == 4))) { // the bar's back
                sMap.area = 0, sMap.room = 0x1E;
            } else if ((room == 0x10) && (cam == 0)) { // the elevator's stairway (stage 6)
                sMap.area = 2, sMap.room = 0x1E, sMap.group = 1;
            } else {
                sMap.area = 0;
            }
            break;
        case 1:
            if ((room == 0x0F) && ((cam == 2) || (cam == 5))) {
                sMap.area = 1, sMap.room = 0x1D;
            } else if ((room == 0x0C) && (cam > 3) && (cam < 8)) {
                sMap.area = 0, sMap.room = 0x1F, sMap.group = 0;
            } else {
                sMap.area = (room < 0x1A) ? 1 : 2;
            }
            break;
        case 2:
            if ((room == 0x0B) && (cam > 4) && (cam < 7)) {
                sMap.area = 4, sMap.room = 0x11;
            } else if ((room == 0x0F) && (cam == 6)) {
                sMap.area = 4, sMap.room = 0x12;
            } else {
                sMap.area = (room < 0x06) ? 3 : 4;
            }
            break;
        case 3:
            sMap.area = (room < 0x0D) ? 5 : 6;
            break;
        default:
            if ((room == 0x07) && (cam < 4)) {
                sMap.area = 9, sMap.room = 0x16;
            } else {
                sMap.area = (room < 0x02) ? 7 : (room < 0x05) ? 8 : (room > 0x12) ? 10 : 9;
            }
            break;
    }
    NvMap_BuildMasks();
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            if (sAreaTable[i * 4 + j] == sMap.area) {
                sMap.floor[i] = (u8)j;
            }
        }
    }
    NvMap_Objective();
    if (sMap.mode != 0) {
        sMap.mode++;
    }
    sMap.layout = sAreaLayouts[sMap.area];
    sMap.hlState = 0;
    sScaleX = 1;
    sScaleY = 200;
    sSettled = false;
    sTicks = 0;
}

/** map_update_layout_state (0x00487430): the floor shown, and whether there are floors beside it. */
static void NvMap_LayoutState(void) {
    s32 layout = sMap.layout;
    s32 floor = sMap.floor[layout];

    sMap.move = 0;
    sMap.area = sAreaTable[layout * 4 + floor];
    if (sLayoutFloors[layout] - floor != 1) {
        u8 next = sAreaTable[layout * 4 + floor + 1];

        if ((sMap.areas & (1 << (next & 0x1F))) || NvMap_AreaKnown(next)) {
            sMap.move |= 1;
        }
    }
    if (floor != 0) {
        u8 prev = sAreaTable[layout * 4 + floor - 1];

        if ((sMap.areas & (1 << (prev & 0x1F))) || NvMap_AreaKnown(prev)) {
            sMap.move |= 2;
        }
    }
}

/** map_tab_load_map (0x00487230): the overview of the highest section known. */
static void NvMap_LoadTab(void) {
    s32 hi = 0;
    s32 k;

    for (k = 0; k < 4; k++) {
        if (sMap.layouts & (1 << k)) {
            hi = k;
        }
    }
    if (sTabTexPic != hi) {
        u16 clut[256];

        for (k = 0; k < 256; k++) {
            clut[k] = NvMap_Clut(sPics[NV_MAP_PIC_TAB + hi].clut, k);
        }
        NvMap_Build(sTabTex, &sPics[NV_MAP_PIC_TAB + hi], clut, 160, false);
        sTabTexPic = hi;
    }
    NvMap_LayoutState();
}

/** map_tab_navigate (0x004872d0): up/down a floor, left/right a section. */
static void NvMap_Navigate(s8 dir) {
    if (sMap.mode & 1) {
        return;
    }
    if ((dir == 3) && (sMap.move & 1)) {
        sMap.floor[sMap.layout]++;
        NvSound_Play("cursor", 128);
    } else if ((dir == 4) && (sMap.move & 2)) {
        sMap.floor[sMap.layout]--;
        NvSound_Play("cursor", 128);
    } else if (dir == 2) {
        if (sMap.layouts & 8) { // with the lab: mansion -> lab -> courtyard -> guardhouse
            static const u8 sRight[4] = { 3, 2, 2, 1 };

            sMap.layout = sRight[sMap.layout];
            NvSound_Play("cursor", 128);
        } else if ((sMap.layout != 2) && (sMap.layouts & (1 << (sMap.layout + 1)))) {
            sMap.layout++;
            NvSound_Play("cursor", 128);
        }
    } else if (dir == 1) {
        if (sMap.layouts & 8) {
            static const u8 sLeft[4] = { 0, 3, 1, 0 };

            sMap.layout = sLeft[sMap.layout];
            NvSound_Play("cursor", 128);
        } else if ((sMap.layout != 0) && (sMap.layouts & (1 << (sMap.layout - 1)))) {
            sMap.layout--;
            NvSound_Play("cursor", 128);
        }
    }
    NvMap_LayoutState();
}

/** map_display_load_textures (0x004876b0): the floor's map, its rooms shown as RE1 shows them. */
static void NvMap_LoadArea(void) {
    const NvMapPic* pic;
    u16 clut[256];
    s32 group;
    s32 count;
    s32 known;
    s32 room;
    s32 k;
    s32 layer;

    if (sMap.area > 10) {
        return;
    }
    if (sMapTexArea != sMap.area) {
        pic = &sPics[NV_MAP_PIC_AREA + sMap.area];
        group = sAreaGroups[sMap.area];
        count = sRoomCounts[group];
        known = NvMap_AreaKnown(sMap.area);
        for (layer = 0; layer < 2; layer++) {
            for (k = 0; k < 256; k++) {
                clut[k] = NvMap_Clut(pic->clut, k);
            }
            if (!known) { // (the floor's own colours 7-12: its outline, without the map)
                memset(&clut[7], 0, 6 * sizeof(u16));
            }
            for (room = 0; room < count; room++) {
                u16* e = &clut[13 + room * 6];
                s32 here = (group == sMap.group) && (room == sMap.room) && NvMap_Visited(group, room);

                if (layer == 0) {
                    if (!NvMap_Visited(group, room)) {
                        if (!known) {
                            memset(e, 0, 6 * sizeof(u16)); // unseen and unmapped: gone
                        } else {
                            e[1] = e[2] = e[3] = 0; // mapped but unseen: the outline, no fill
                        }
                    } else if (here) {
                        e[1] = e[2] = e[3] = 0; // here: the blinking layer has it
                    }
                } else if (!here) {
                    memset(e, 0, 6 * sizeof(u16));
                } else {
                    e[1] = e[2] = e[3] = 0x001F; // red
                }
            }
            if (layer == 1) { // (the floor's outline, doors and ladders, colours 1-12: the base layer's)
                memset(&clut[1], 0, 12 * sizeof(u16));
            }
            NvMap_Build(sMapTex[layer], pic, clut, 112, layer == 0);
        }
        sMapTexArea = sMap.area;
    }
    if (!sBlueReady) {
        for (k = 0; k < 256; k++) {
            clut[k] = NvMap_Clut(sPics[NV_MAP_PIC_BLUE].clut, k);
        }
        NvMap_Build(sBlueTex, &sPics[NV_MAP_PIC_BLUE], clut, 104, false);
        sBlueReady = true;
    }
    // the objective's highlight on the maps that show it
    sMap.hlState = ((sMap.mode == 2) && (sMap.area == 0)) || ((sMap.mode == 4) && (sMap.area == 6)) ? 1 : 0;
}

/** map_display_animate (0x00487a30): the blink, and the zoom's states. */
static void NvMap_Animate(void) {
    if (sAreaGroups[sMap.area] == sMap.group) { // (she's on this map: her room blinks)
        switch (sMap.blink) {
            case 0:
                sDot = 0x50;
                sMap.blink = 1;
                break;
            case 1:
                sDot += 2;
                if (sDot > 0xA0) {
                    sMap.blink = 2;
                }
                break;
            default:
                sDot -= 2;
                if (sDot < 0x50) {
                    sMap.blink = 1;
                }
                break;
        }
        sDotBlink = (u8)((sDot >> 3) & 0x1F);
    }
    switch (sMap.zoom) {
        case 0:
            sScaleX = 1;
            sScaleY = 200;
            sSettled = false;
            sMap.zoom = 1;
            NvSound_Play("chris09", 128);
            // fallthrough
        case 1:
            sMap.zoom = 2;
            break;
        case 2:
            sScaleX = (s16)MIN(sScaleX * 3, 0x7FFF);
            if (sScaleX >= 0xFFF) {
                sScaleX = 0xFFF;
                sMap.zoom = 3;
            }
            break;
        case 3:
            sScaleY += 900;
            if (sScaleY >= 0x15DC) {
                sScaleY = 0x15DC;
                sMap.zoom = 4;
            }
            break;
        case 4:
            sScaleY -= 300;
            if (sScaleY <= 0x1000) {
                sScaleX = sScaleY = 0x1000;
                sWait = 8;
                sMap.zoom = 5;
            }
            break;
        case 5:
            if (--sWait == 0) {
                sSettled = true;
                sMap.zoom = 6;
                sWait = 10;
            }
            break;
        case 6:
            if ((sMap.hlState == 1) && (--sWait == 0)) {
                sMap.hlState = 2;
            }
            break;
        case 7:
            sSettled = false;
            sScaleX = 0xFFF;
            sScan = 0;
            if (sMap.hlState != 0) {
                sMap.hlState = 1;
            }
            sMap.zoom = 8;
            // fallthrough
        case 8:
            sScaleY -= 0x28A;
            if (sScaleY <= 1) {
                sScaleY = 200;
                sMap.zoom = 9;
            }
            break;
        case 9:
            sScaleX /= 2;
            if (sScaleX <= 1) {
                sScaleX = 1;
                sMap.state = 0; // back to the browser
                sMap.substate = 1;
                sMap.blink = 0;
                sMap.zoom = 0;
            }
            break;
    }
    if ((sMap.zoom == 6) && (++sScan > 0x118)) { // (a sweep of 0xD0 ticks, then a pause)
        sScan = 0;
    }
    // the objective highlight's marker
    if ((sMap.hlState > 1) && (sMap.mode != 0)) {
        switch (sMap.hlState) {
            case 2:
                NvSound_Play("mapled", 128);
                sMap.hlState = 3;
                // fallthrough
            case 3:
                sMap.hlFrame += 2;
                if (sMap.hlFrame >= 0x17) {
                    sMap.hlState = 4;
                    sMap.hlFrame = 0x17;
                }
                break;
            case 4:
                if (--sMap.hlFrame == 0) {
                    sMap.hlState = 5;
                    sMap.hlFrame = 0x14;
                }
                break;
            case 5:
                if (--sMap.hlFrame == 0) {
                    sMap.hlState = 2;
                }
                break;
        }
    }
}

/** One RE1 tick of the map tab (menu_map_state_machine); input on the frame's first. -1: it's left. */
static s32 NvMap_Tick(s8 dir, s32 pressA, s32 pressB) {
    if (pressB && (sMap.state == 1) && (sMap.zoom >= 7)) { // (B again while it closes: back out)
        sMap.state = 0;
    }
    switch (sMap.state) {
        case 0: // the browser
            if (pressB) {
                NvSound_Play("cancel", 128);
                return -1;
            }
            if (sMap.substate == 0) {
                NvMap_LoadTab();
                sMap.substate = 1;
            } else {
                NvMap_Navigate(dir);
            }
            if (pressA && (sMap.area <= 10)) {
                sMap.state = 1;
                sMap.substate = 0;
                sMap.blink = 0;
                sMap.zoom = 0;
            }
            break;
        case 1: // the floor's map
            if (sMap.substate == 0) {
                NvMap_LoadArea();
                sMap.substate = 1;
            }
            if (pressA && ((sMap.zoom == 0) || (sMap.zoom > 6))) {
                if (sMap.zoom == 8) {
                    sMap.zoom = 3;
                }
                if (sMap.zoom == 9) {
                    sMap.zoom = 2;
                }
                NvSound_Play("chris09", 128);
            }
            if (pressB && (sMap.zoom < 7)) {
                sMap.zoom = 7;
                NvSound_Play("cancel", 128);
            }
            NvMap_Animate();
            break;
    }
    return 0;
}

/** Each pause-menu frame the tab is open: RE1's ticks. -1 when it's left (back to the tabs). */
s32 NvMap_Update(s8 dir, s32 pressA, s32 pressB) {
    s32 n = (sTicks++ & 1) ? 2 : 1;
    s32 k;

    if (!NvMap_Pics()) {
        return -1;
    }
    for (k = 0; k < n; k++) {
        if (NvMap_Tick((k == 0) ? dir : 0, (k == 0) && pressA, (k == 0) && pressB) < 0) {
            return -1;
        }
    }
    return 0;
}

// ---- drawing ----

/**
 * Rows [v, v + h) and columns [u, u + w) of an RGBA32 picture `texW` wide, with its top left at
 * (x, y) in RE1's coordinates, scaled (1.0: as it is), tinted.
 */
static void NvMap_Blit(Gfx** gfxP, const u8* tex, s32 texW, s32 u, s32 v, s32 w, s32 h, f32 x, f32 y, f32 sx,
                       f32 sy, u8 r, u8 g, u8 b, u8 a) {
    Gfx* gfx = *gfxP;
    s32 strip = 4096 / (texW * 4); // rows per load: 4 KB, RGBA32's 1024 texels
    s32 k;

    if ((w * sx < 0.5f) || (h * sy < 0.5f)) {
        return;
    }
    x += NV_MAP_X;
    y += NV_MAP_Y;
    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, r, g, b, a);
    for (k = 0; k < h; k += strip) {
        s32 n = MIN(strip, h - k);
        s32 y0 = (s32)((y + k * sy) * 4.0f);
        s32 y1 = (s32)((y + (k + n) * sy) * 4.0f);

        if (y1 <= y0) {
            continue;
        }
        gDPLoadTextureBlock(gfx++, tex + (v + k) * texW * 4, G_IM_FMT_RGBA, G_IM_SIZ_32b, texW, n, 0,
                            G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                            G_TX_NOLOD, G_TX_NOLOD);
        gSPTextureRectangle(gfx++, (s32)(x * 4.0f), y0, (s32)((x + w * sx) * 4.0f), y1, G_TX_RENDERTILE, u << 5, 0,
                            (s32)(1024.0f / sx), (s32)(1024.0f / sy));
    }
    *gfxP = gfx;
}

static void NvMap_Spr(Gfx** gfxP, const NvMapSpr* s, s32 u, s32 v, u8 shade) {
    NvMap_Blit(gfxP, sTabTex, 256, u, v, s->w, s->h, s->x, s->y, 1.0f, 1.0f, shade, shade, shade, 255);
}

/** map_draw_browser (0x00487540): the overview, the markers, the labels, the arrows. */
static void NvMap_DrawBrowser(Gfx** gfxP, s32 dim) {
    s32 blink;
    s32 k;

    if (sTabTexPic < 0) {
        return;
    }
    NvMap_Spr(gfxP, &sBase, sBase.u, sBase.v, dim ? 128 : 255);
    for (k = 0; k < 4; k++) {
        if (sMap.layouts & (1 << k)) {
            NvMap_Spr(gfxP, &sMarkers[k], sMarkers[k].u, sMarkers[k].v, dim ? 96 : (sMap.layout == k) ? 255 : 128);
        }
    }
    NvMap_Spr(gfxP, &sName, sName.u, sMap.layout * 16 + 0x48, dim ? 96 : 255);
    NvMap_Spr(gfxP, &sFloor, sFloor.u, (6 - sMap.floor[sMap.layout] - sLayoutOffset[sMap.layout]) * 16,
              dim ? 96 : 255);
    sTabFrame++;
    blink = (sTabFrame & 0x30) != 0;
    if (sMap.move & 1) {
        NvMap_Spr(gfxP, &sUp, sUp.u + blink * 8, sUp.v, dim ? 96 : 255);
    }
    if (sMap.move & 2) {
        NvMap_Spr(gfxP, &sDown, sDown.u + blink * 8, sDown.v, dim ? 96 : 255);
    }
}

/** map_display_draw (0x00487ec0): the grid, the floor's map, the blinking room, the scan line, the marker. */
static void NvMap_DrawDisplay(Gfx** gfxP) {
    f32 sx = sScaleX / 4096.0f;
    f32 sy = sScaleY / 4096.0f;
    u8 red = (u8)MIN(255, sDotBlink * 8 + 40);

    if ((sMapTexArea < 0) || !sBlueReady) {
        return;
    }
    // the grid, two halves meeting at (112, 76); the map pinned at (128, 78) by its (80, 50)
    NvMap_Blit(gfxP, sBlueTex, 128, 0, 0, 80, 104, 112 - 80 * sx, 76 - 52 * sy, sx, sy, 255, 255, 255, 255);
    NvMap_Blit(gfxP, sBlueTex, 128, 0, 0, 80, 104, 112, 76 - 52 * sy, sx, sy, 255, 255, 255, 255);
    NvMap_Blit(gfxP, sMapTex[0], 128, 0, 0, 128, 100, 128 - 80 * sx, 78 - 50 * sy, sx, sy, 255, 255, 255,
               sSettled ? 255 : 230);
    if (sAreaGroups[sMap.area] == sMap.group) {
        NvMap_Blit(gfxP, sMapTex[1], 128, 0, 0, 128, 100, 128 - 80 * sx, 78 - 50 * sy, sx, sy, red, 255, 255, 255);
    }
    // the scan line at rest: one row of the grid and the map again, brighter
    if ((sMap.zoom == 6) && (sScan < 0xD0)) {
        s32 a = sScan >> 1; // (RE1 draws a row when its texture row is past 5)

        if ((a > 5) && (a < 104)) {
            NvMap_Blit(gfxP, sBlueTex, 128, 0, a, 80, 1, 32, 24 + a, 1.0f, 1.0f, 255, 255, 255, 120);
            NvMap_Blit(gfxP, sBlueTex, 128, 0, a, 80, 1, 112, 24 + a, 1.0f, 1.0f, 255, 255, 255, 120);
        }
        if ((a > 9) && (a - 4 < 100)) {
            NvMap_Blit(gfxP, sMapTex[0], 128, 0, a - 4, 128, 1, 48, 24 + a, 1.0f, 1.0f, 255, 255, 255, 160);
        }
    }
    // the objective's marker: RE1's frames in the map's own picture (row 104)
    if ((sMap.hlState > 1) && (sMap.mode != 0) && (sMap.zoom == 6)) {
        s32 frame = (sMap.hlState == 5) ? 0 : ((sMap.hlFrame / 3) & 7);
        s32 x = (sMap.mode / 3 == 0) ? 0x38 : 0x86;
        s32 y = (sMap.mode / 3 == 0) ? 0x34 : 0x43;

        NvMap_Blit(gfxP, sMapTex[0], 128, frame * 8, 104, 8, 8, x, y, 1.0f, 1.0f, 255, 255, 255, 255);
    }
}

/** The tab's picture (z_nv_status.c, under the tabs): the browser, and the floor's map over it. */
Gfx* NvMap_Draw(Gfx* gfx) {
    if (!NvMap_Pics()) {
        return gfx;
    }
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineLERP(gfx++, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0,
                      PRIMITIVE, 0);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    NvMap_DrawBrowser(&gfx, sMap.state == 1);
    if (sMap.state == 1) {
        NvMap_DrawDisplay(&gfx);
    }
    gDPPipeSync(gfx++);
    return gfx;
}
