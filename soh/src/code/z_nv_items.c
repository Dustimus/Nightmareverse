/*
 * Nightmareverse: RE1's items as RE1 keeps them, for the mansion's pickups.
 *
 * One pool (re1/README.md, "Items"): the player's slots (Jill 8, Chris 6), RE1's linked item box,
 * and the room items' flags (RE1's g_roomItemsFlags, the scripts' flag bank 7).
 * Picking up follows RE1's room_event_item_pickup (RoomEvents.cpp 0x00451700): ammo and ink
 * ribbons merge into a stack already carried, up to 250, the rest into a new slot; anything else
 * takes a new slot. With no room for it, nothing changes and the item stays where it lies.
 *
 * The prompt and item names are our own plain wording (item names as a guide would give them),
 * shown in OoT's text box with its yes/no choice. Everything lasts for the session; saving it
 * comes with the save system.
 */
#include "global.h"
#include <stdio.h>
#include <string.h>

#define NV_SLOTS_MAX 8
#define NV_BOX_SLOTS 48
#define NV_STACK_MAX 250 // RE1's cap on a merged stack (0xFA)

typedef struct {
    u8 id;
    u8 qty;
} NvItemSlot;

static NvItemSlot sNvSlots[NV_SLOTS_MAX];
static NvItemSlot sNvBox[NV_BOX_SLOTS];
static u8 sNvMapsOwned; // bit n: map item 0x4E + n
static s8 sNvEquipped;  // RE1's g_EquippedItemId: the equipped slot, one-based (0: none)

// The items' plain names, by RE1's item id (Types.h ITEM_*)
static const char* sNvItemNames[0x54] = {
    "",
    "Knife",
    "Beretta",
    "Colt Python",
    "Colt Python",
    "Colt Python",
    "Flamethrower",
    "Grenade Launcher",
    "Grenade Launcher",
    "Grenade Launcher",
    "Rocket Launcher",
    "Clip",
    "Shotgun Shells",
    "DumDum Rounds",
    "Magnum Rounds",
    "Fuel",
    "Explosive Rounds",
    "Acid Rounds",
    "Flame Rounds",
    "Empty Bottle",
    "Water",
    "UMB No. 2",
    "UMB No. 4",
    "UMB No. 7",
    "UMB No. 13",
    "Yellow-6",
    "NP-003",
    "V-Jolt",
    "Broken Shotgun",
    "Square Crank",
    "Hex Crank",
    "Wooden Emblem",
    "Gold Emblem",
    "Blue Jewel",
    "Red Jewel",
    "Music Notes",
    "Wolf Medal",
    "Eagle Medal",
    "Chemical",
    "Battery",
    "MO Disk",
    "Wind Crest",
    "Flare",
    "Slides",
    "Moon Crest",
    "Star Crest",
    "Sun Crest",
    "Ink Ribbon",
    "Lighter",
    "Lockpick",
    "Oil",
    "Sword Key",
    "Armor Key",
    "Shield Key",
    "Helmet Key",
    "Lab Key",
    "Special Key",
    "Dorm Key",
    "Dorm Key",
    "Control Room Key",
    "Lab Key",
    "Desk Key",
    "Red Book",
    "Doom Book",
    "Doom Book",
    "First Aid Spray",
    "Serum",
    "Red Herb",
    "Green Herb",
    "Blue Herb",
    "Mixed Herb",
    "Mixed Herb",
    "Mixed Herb",
    "Mixed Herb",
    "Mixed Herb",
    "Mixed Herb",
    "Pick Axe",
    "Radio",
    "Map",
    "Map",
    "Map",
    "Map",
    "Map",
    "Map",
};

#define NV_ITEM_SHOTGUN 0x03
#define NV_ITEM_ROCKET_LAUNCHER 0x0A
#define NV_ITEM_EMPTY_BOTTLE 0x13
#define NV_ITEM_INK_RIBBONS 0x2F
#define NV_ITEM_MAP_FIRST 0x4E
#define NV_ITEM_MAP_LAST 0x53

const char* NvItems_Name(s32 id) {
    if (id == NV_ITEM_SHOTGUN) {
        return "Shotgun";
    }
    return ((id > 0) && (id < (s32)ARRAY_COUNT(sNvItemNames))) ? sNvItemNames[id] : "item";
}

/** The slots the player has: RE1's (4 - (player != Jill)) * 2. Link plays Jill. */
static s32 NvItems_SlotCount(void) {
    return 8;
}

static s32 NvItems_Stacks(s32 id) {
    return ((id > NV_ITEM_ROCKET_LAUNCHER) && (id < NV_ITEM_EMPTY_BOTTLE)) || (id == NV_ITEM_INK_RIBBONS);
}

static s32 NvItems_Held(void) {
    s32 n = 0;

    while ((n < NvItems_SlotCount()) && (sNvSlots[n].id != 0)) {
        n++;
    }
    return n;
}

/** Whether qty of item id fits (RE1 doesn't take what it can't hold whole). */
s32 NvItems_CanTake(s32 id, s32 qty) {
    s32 room = 0;
    s32 i;

    if ((id >= NV_ITEM_MAP_FIRST) && (id <= NV_ITEM_MAP_LAST)) {
        return true; // maps go to the map screen, not the slots
    }
    if (NvItems_Held() < NvItems_SlotCount()) {
        return true;
    }
    if (!NvItems_Stacks(id)) {
        return false;
    }
    for (i = 0; i < NvItems_SlotCount(); i++) {
        if (sNvSlots[i].id == id) {
            room += NV_STACK_MAX - sNvSlots[i].qty;
        }
    }
    return room >= qty;
}

/** room_event_item_pickup's give: false (and nothing changed) when it doesn't fit. */
s32 NvItems_Give(s32 id, s32 qty) {
    s32 i;

    if (!NvItems_CanTake(id, qty)) {
        return false;
    }
    if ((id >= NV_ITEM_MAP_FIRST) && (id <= NV_ITEM_MAP_LAST)) {
        sNvMapsOwned |= 1 << (id - NV_ITEM_MAP_FIRST);
        return true;
    }
    if (NvItems_Stacks(id)) {
        for (i = 0; (i < NvItems_SlotCount()) && (qty > 0); i++) {
            if (sNvSlots[i].id == id) {
                s32 add = MIN(qty, NV_STACK_MAX - sNvSlots[i].qty);

                sNvSlots[i].qty += add;
                qty -= add;
            }
        }
        if (qty == 0) {
            return true;
        }
    }
    i = NvItems_Held();
    sNvSlots[i].id = id;
    sNvSlots[i].qty = qty;
    return true;
}

/** RE1's room item flags (flag bank 7, the scripts' own: set while the item lies there). */
s32 NvFlags_Test(s32 bank, s32 bit);
void NvFlags_Op(s32 bank, s32 bit, s32 mode);

s32 NvItems_InRoom(s32 n) {
    return NvFlags_Test(7, n);
}

void NvItems_SetTaken(s32 n) {
    NvFlags_Op(7, n, 1);
}

/** For the menus to come: slot i's item (0: none) and count. */
s32 NvItems_Slot(s32 i, s32* qty) {
    if ((i < 0) || (i >= NvItems_SlotCount())) {
        return 0;
    }
    if (qty != NULL) {
        *qty = sNvSlots[i].qty;
    }
    return sNvSlots[i].id;
}

void NvItems_Log(void) {
    s32 i;

    printf("NVITEMS");
    for (i = 0; i < NvItems_SlotCount(); i++) {
        printf(" %02X:%d", sNvSlots[i].id, sNvSlots[i].qty);
    }
    printf("\n");
    fflush(stdout);
}

/* ---- the items lying in the room: RE1's item models ---- */

#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>
#include <stdlib.h>

#define NV_RE1_SCALE 0.0217f // OoT units per RE1 unit (re1/tools/rdt2scene.py SCALE)
#define NV_IT_CELL_W 64
#define NV_IT_CELL_H 32
#define NV_IT_MODELS 16

typedef struct {
    u16 cell;
    u16 numVerts;
    Vtx* vtx;
} NvItBatch;

typedef struct {
    u16* cells;
    s32 numBatches;
    NvItBatch* batches;
} NvItModel;

static NvItModel sNvItModels[NV_IT_MODELS];
static s32 sNvItCount;

s32 NvAot_Item(s32 i, s32* model, s32* parent, s16* pos);
s32 NvActor_ItemsId(void);
s32 NvDoorScene_Covering(void);

/**
 * The room's "<name>_items" (re1/tools/soh_scene.py items_blob) into sNvItModels, and the actor
 * that draws them. Called as the room loads (z_nv_script.c), before its scripts place the items.
 */
/** Testing: NV_GIVE="id,qty;id,qty..." (RE1 item ids, decimal or 0x) given once, on the first room. */
static void NvItems_DevGive(void) {
    static s32 sDone;
    const char* s = getenv("NV_GIVE");
    unsigned int id, qty;
    int used;

    if (sDone || (s == NULL)) {
        return;
    }
    sDone = true;
    while (sscanf(s, "%i,%i%n", &id, &qty, &used) == 2) {
        NvItems_Give(id, qty);
        s += used;
        if (*s != ';') {
            break;
        }
        s++;
    }
}

void NvItems_RoomLoad(PlayState* play) {
    char path[256];
    const char* file;
    size_t len;
    const u8* d;
    u16 count;
    s32 i;

    NvItems_DevGive();

    for (i = 0; i < sNvItCount; i++) {
        s32 b;

        for (b = 0; b < sNvItModels[i].numBatches; b++) {
            free(sNvItModels[i].batches[b].vtx);
        }
        free(sNvItModels[i].batches);
    }
    memset(sNvItModels, 0, sizeof(sNvItModels));
    sNvItCount = 0;
    if ((play->loadedScene == NULL) || (play->loadedScene->sceneFile.fileName == NULL)) {
        return;
    }
    file = play->loadedScene->sceneFile.fileName;
    len = strlen(file);
    if ((len <= 6) || (strcmp(file + len - 6, "_scene") != 0)) {
        return;
    }
    snprintf(path, sizeof(path), "scenes/shared/%s/%.*s_items", file, (int)(len - 6), file);
    if (!ResourceMgr_FileExists(path)) {
        return;
    }
    d = (const u8*)ResourceGetDataByName(path);
    if ((d == NULL) || (memcmp(d, "RITM", 4) != 0)) {
        return;
    }
    memcpy(&count, d + 4, 2);
    d += 6;
    for (i = 0; (i < count) && (i < NV_IT_MODELS); i++) {
        NvItModel* m = &sNvItModels[i];
        u16 cw;
        u16 ch;
        u16 nb;
        s32 b;

        memcpy(&cw, d, 2);
        memcpy(&ch, d + 2, 2);
        d += 4;
        m->cells = (u16*)d;
        d += cw * ch * NV_IT_CELL_W * NV_IT_CELL_H * 2;
        memcpy(&nb, d, 2);
        d += 2;
        m->numBatches = nb;
        m->batches = calloc(MAX(nb, 1), sizeof(NvItBatch));
        for (b = 0; b < nb; b++) {
            NvItBatch* batch = &m->batches[b];

            memcpy(&batch->cell, d, 2);
            memcpy(&batch->numVerts, d + 2, 2);
            d += 4;
            batch->vtx = malloc(batch->numVerts * sizeof(Vtx));
            memcpy(batch->vtx, d, batch->numVerts * sizeof(Vtx));
            d += batch->numVerts * sizeof(Vtx);
        }
    }
    sNvItCount = MIN(count, NV_IT_MODELS);
    if (sNvItCount > 0) {
        Actor_Spawn(&play->actorCtx, play, NvActor_ItemsId(), 0.0f, 0.0f, 0.0f, 0, 0, 0, 0);
    }
}

void NvItemsActor_Init(Actor* thisx, PlayState* play) {
}

void NvItemsActor_Destroy(Actor* thisx, PlayState* play) {
}

void NvItemsActor_Update(Actor* thisx, PlayState* play) {
}

/** Every item still lying in the room, at RE1's position: RE1 x, -y, -z, scaled. */
void NvItemsActor_Draw(Actor* thisx, PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    s32 i;

    if (NvDoorScene_Covering()) {
        return;
    }
    OPEN_DISPS(gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCycleType(POLY_OPA_DISP++, G_CYC_1CYCLE);
    gDPSetDepthSource(POLY_OPA_DISP++, G_ZS_PIXEL);
    gDPSetRenderMode(POLY_OPA_DISP++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
    gDPSetTextureFilter(POLY_OPA_DISP++, G_TF_POINT);
    gDPSetTexturePersp(POLY_OPA_DISP++, G_TP_PERSP);
    gDPSetTextureLUT(POLY_OPA_DISP++, G_TT_NONE);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, 255);
    gSPClearGeometryMode(POLY_OPA_DISP++, G_LIGHTING | G_FOG | G_CULL_BOTH | G_TEXTURE_GEN);
    gSPSetGeometryMode(POLY_OPA_DISP++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gSPTexture(POLY_OPA_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    for (i = 0; i < 32; i++) {
        s32 model;
        s32 parent;
        s16 pos[3];
        NvItModel* m;
        s32 b;

        if (!NvAot_Item(i, &model, &parent, pos) || (model >= sNvItCount) || (parent != 0xFF)) {
            continue; // (items on a room object or the player: not yet)
        }
        m = &sNvItModels[model];
        Matrix_Push();
        Matrix_Translate(pos[0] * NV_RE1_SCALE, -pos[1] * NV_RE1_SCALE, -pos[2] * NV_RE1_SCALE, MTXMODE_NEW);
        Matrix_Scale(NV_RE1_SCALE, -NV_RE1_SCALE, -NV_RE1_SCALE, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        Matrix_Pop();
        for (b = 0; b < m->numBatches; b++) {
            NvItBatch* batch = &m->batches[b];
            s32 v;

            gDPLoadTextureBlock(POLY_OPA_DISP++, m->cells + batch->cell * NV_IT_CELL_W * NV_IT_CELL_H, G_IM_FMT_RGBA,
                                G_IM_SIZ_16b, NV_IT_CELL_W, NV_IT_CELL_H, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                                G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
            for (v = 0; v + 3 <= batch->numVerts; v += 30) {
                s32 cnt = MIN(30, batch->numVerts - v);
                s32 q;

                gSPVertex(POLY_OPA_DISP++, batch->vtx + v, cnt, 0);
                for (q = 0; q + 3 <= cnt; q += 3) {
                    gSP1Triangle(POLY_OPA_DISP++, q, q + 1, q + 2, 0);
                }
            }
        }
    }
    CLOSE_DISPS(gfxCtx);
}

/* ---- Use and Combine (RE1's status screen, z_nv_status.c) ---- */

// RE1's healing per item (MenuData.cpp g_ItemHealTable): low nibble 1 a third of the player's
// health, 2 two thirds, 3 all of it; 0x10 / 0x20 cures (the serum's, the blue herb's poison)
static u8 NvItems_Heal(s32 id) {
    switch (id) {
        case 0x41: // first aid spray
            return 0x03;
        case 0x44: // green herb
            return 0x01;
        case 0x45: // blue herb
            return 0x20;
        case 0x46: // green + red
        case 0x4A: // green x3
            return 0x03;
        case 0x47: // green x2
            return 0x02;
        case 0x48: // green + blue
            return 0x21;
        case 0x49: // green + red + blue
            return 0x23;
        case 0x4B: // green x2 + blue
            return 0x22;
        default:
            return 0;
    }
}

/** Remove one of slot i's item, closing the gap as RE1 does (rearrange_item_slots). */
static void NvItems_TakeOne(s32 i) {
    s32 k;

    if (sNvSlots[i].qty > 1) {
        sNvSlots[i].qty--;
        return;
    }
    for (k = i; k < NvItems_SlotCount() - 1; k++) {
        sNvSlots[k] = sNvSlots[k + 1];
    }
    sNvSlots[NvItems_SlotCount() - 1].id = sNvSlots[NvItems_SlotCount() - 1].qty = 0;
    // the equipped item moves up with the rest, or is gone
    if (sNvEquipped == i + 1) {
        sNvEquipped = 0;
    } else if (sNvEquipped > i + 1) {
        sNvEquipped--;
    }
}

/** Whether item id can be equipped: RE1's weapons, the knife to the rocket launcher. */
s32 NvItems_Equippable(s32 id) {
    return (id > 0) && (id <= NV_ITEM_ROCKET_LAUNCHER);
}

/** The equipped slot (0-based), or -1. */
s32 NvItems_Equipped(void) {
    return sNvEquipped - 1;
}

/**
 * Equip (or, already equipped, unequip) slot i's weapon, as RE1's menu_item_toggle_equip. Its use
 * in the rooms comes with the weapons. True when the slot holds a weapon.
 */
s32 NvItems_ToggleEquip(s32 i) {
    if (!NvItems_Equippable(NvItems_Slot(i, NULL))) {
        return false;
    }
    sNvEquipped = (sNvEquipped == i + 1) ? 0 : i + 1;
    return true;
}

/**
 * Use slot i's item (menu_item_use_heal): a heal fills Link's hearts by RE1's thirds; a cure has
 * nothing to cure in Hyrule (no poison). As RE1, an item that would do nothing isn't used up.
 * Returns 1 used, 0 nothing happened, -1 not usable here.
 */
s32 NvItems_Use(PlayState* play, s32 i) {
    s32 id = NvItems_Slot(i, NULL);
    u8 heal = NvItems_Heal(id);
    s32 cap = gSaveContext.healthCapacity;
    s32 amount;

    if (heal == 0) {
        return -1;
    }
    if (((heal & 0x0F) == 0) || (gSaveContext.health >= cap)) {
        return 0;
    }
    amount = ((heal & 0x0F) == 1) ? cap / 3 : ((heal & 0x0F) == 2) ? cap * 2 / 3 : cap;
    Health_ChangeBy(play, amount);
    NvItems_TakeOne(i);
    return 1;
}

/** RE1's herb mixes: the item made by combining a with b (either order), or 0. */
static s32 NvItems_Mix(s32 a, s32 b) {
    static const u8 sMixes[][3] = {
        { 0x44, 0x44, 0x47 }, // green + green
        { 0x44, 0x43, 0x46 }, // green + red
        { 0x44, 0x45, 0x48 }, // green + blue
        { 0x47, 0x44, 0x4A }, // green x2 + green
        { 0x47, 0x45, 0x4B }, // green x2 + blue
        { 0x48, 0x44, 0x4B }, // green + blue + green
        { 0x46, 0x45, 0x49 }, // green + red + blue
        { 0x48, 0x43, 0x49 }, // green + blue + red
    };
    s32 k;

    for (k = 0; k < (s32)ARRAY_COUNT(sMixes); k++) {
        if (((sMixes[k][0] == a) && (sMixes[k][1] == b)) || ((sMixes[k][0] == b) && (sMixes[k][1] == a))) {
            return sMixes[k][2];
        }
    }
    return 0;
}

/** Combine slot a with slot b: the mix replaces a, b is used up. True when they combined. */
s32 NvItems_Combine(s32 a, s32 b) {
    s32 mix;

    if ((a == b) || (NvItems_Slot(a, NULL) == 0) || (NvItems_Slot(b, NULL) == 0)) {
        return false;
    }
    mix = NvItems_Mix(sNvSlots[a].id, sNvSlots[b].id);
    if (mix == 0) {
        return false;
    }
    sNvSlots[a].id = mix;
    sNvSlots[a].qty = 1;
    NvItems_TakeOne(b);
    return true;
}
