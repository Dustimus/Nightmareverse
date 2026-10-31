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

// the room's object models (cmd_omodel_set's slots: model i is object i's)
#define NV_OBJ_MODELS 64
static NvItModel sNvObjModels[NV_OBJ_MODELS];
static s32 sNvObjCount;

s32 NvAot_Item(s32 i, s32* model, s32* parent, s16* pos);
s32 NvObj_Get(s32 i, s32* parent, s16* pos, s16* rot);
s32 NvObj_Placed(s32 i, s32* parent, s16* pos, s16* rot);
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

    if (sDone) {
        return;
    }
    sDone = true;
    if (getenv("NV_FILE") != NULL) { // NV_FILE=n: take RE1 file n, as if found in the room
        s32 NvFiles_Take(s32 id);

        NvFiles_Take(0x5F + atoi(getenv("NV_FILE")));
    }
    if (s == NULL) {
        return;
    }
    while (sscanf(s, "%i,%i%n", &id, &qty, &used) == 2) {
        NvItems_Give(id, qty);
        s += used;
        if (*s != ';') {
            break;
        }
        s++;
    }
}

static void NvIt_FreeSet(NvItModel* models, s32* count) {
    s32 i;

    for (i = 0; i < *count; i++) {
        s32 b;

        for (b = 0; b < models[i].numBatches; b++) {
            free(models[i].batches[b].vtx);
        }
        free(models[i].batches);
    }
    memset(models, 0, sizeof(NvItModel) * *count);
    *count = 0;
}

/** One of the room's model sets ("<name>_items" / "<name>_objs", soh_scene.py items_blob). */
static s32 NvIt_LoadSet(const char* path, const char* magic, NvItModel* models, s32 max) {
    const u8* d;
    u16 count;
    s32 i;

    if (!ResourceMgr_FileExists(path)) {
        return 0;
    }
    d = (const u8*)ResourceGetDataByName(path);
    if ((d == NULL) || (memcmp(d, magic, 4) != 0)) {
        return 0;
    }
    memcpy(&count, d + 4, 2);
    d += 6;
    for (i = 0; (i < count) && (i < max); i++) {
        NvItModel* m = &models[i];
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
    return MIN(count, max);
}

void NvItems_RoomLoad(PlayState* play) {
    char path[256];
    const char* file;
    size_t len;

    NvItems_DevGive();

    NvIt_FreeSet(sNvItModels, &sNvItCount);
    NvIt_FreeSet(sNvObjModels, &sNvObjCount);
    if ((play->loadedScene == NULL) || (play->loadedScene->sceneFile.fileName == NULL)) {
        return;
    }
    file = play->loadedScene->sceneFile.fileName;
    len = strlen(file);
    if ((len <= 6) || (strcmp(file + len - 6, "_scene") != 0)) {
        return;
    }
    snprintf(path, sizeof(path), "scenes/shared/%s/%.*s_items", file, (int)(len - 6), file);
    sNvItCount = NvIt_LoadSet(path, "RITM", sNvItModels, NV_IT_MODELS);
    snprintf(path, sizeof(path), "scenes/shared/%s/%.*s_objs", file, (int)(len - 6), file);
    sNvObjCount = NvIt_LoadSet(path, "ROBJ", sNvObjModels, NV_OBJ_MODELS);
    if ((sNvItCount > 0) || (sNvObjCount > 0)) {
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
static void NvIt_DrawModel(GraphicsContext* gfxCtx, NvItModel* m) {
    s32 b;

    OPEN_DISPS(gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
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
    CLOSE_DISPS(gfxCtx);
}

#define NV_PSX_ANGLE(a) ((a) * (2.0f * M_PI / 4096.0f))

/**
 * Apply room object i's place in RE1's space to the current matrix, its parents' first
 * (InitScaMatrix: an object on another object moves with it). Objects on the player or an enemy
 * are placed in the room for now. False when it isn't placed.
 */
static s32 NvIt_ApplyObject(s32 i, s32 depth) {
    s32 parent;
    s16 pos[3];
    s16 rot[3];

    // (a hidden parent still carries its children)
    if ((depth > 4) || !NvObj_Placed(i, &parent, pos, rot)) {
        return false;
    }
    if (parent < 0x80) {
        NvIt_ApplyObject(parent, depth + 1);
    }
    Matrix_Translate(pos[0], pos[1], pos[2], MTXMODE_APPLY);
    // RotMatrix: x, then y, then z
    Matrix_RotateX(NV_PSX_ANGLE(rot[0]), MTXMODE_APPLY);
    Matrix_RotateY(NV_PSX_ANGLE(rot[1]), MTXMODE_APPLY);
    Matrix_RotateZ(NV_PSX_ANGLE(rot[2]), MTXMODE_APPLY);
    return true;
}

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
    CLOSE_DISPS(gfxCtx);

    // the room's objects (render_room_objects' first pass: those with bit 0 set and a model);
    // NV_NO_OBJS hides them, for comparisons
    for (i = 0; (i < sNvObjCount) && (getenv("NV_NO_OBJS") == NULL); i++) {
        s32 parent;
        s16 pos[3];
        s16 rot[3];

        if (!NvObj_Get(i, &parent, pos, rot) || (sNvObjModels[i].numBatches == 0)) {
            continue;
        }
        Matrix_Push();
        Matrix_Scale(NV_RE1_SCALE, -NV_RE1_SCALE, -NV_RE1_SCALE, MTXMODE_NEW);
        NvIt_ApplyObject(i, 0);
        NvIt_DrawModel(gfxCtx, &sNvObjModels[i]);
        Matrix_Pop();
    }
    // the items: in the room, or on a room object (its offset in the object's space)
    for (i = 0; i < 32; i++) {
        s32 model;
        s32 parent;
        s16 pos[3];

        if (!NvAot_Item(i, &model, &parent, pos) || (model >= sNvItCount)) {
            continue;
        }
        Matrix_Push();
        Matrix_Scale(NV_RE1_SCALE, -NV_RE1_SCALE, -NV_RE1_SCALE, MTXMODE_NEW);
        if (parent == 0xFF) {
            Matrix_Translate(pos[0], pos[1], pos[2], MTXMODE_APPLY);
        } else if ((parent < 0x80) && NvIt_ApplyObject(parent, 0)) {
            Matrix_Translate(pos[0], pos[1], pos[2], MTXMODE_APPLY);
        } else {
            Matrix_Pop();
            continue; // (on the player: not yet)
        }
        NvIt_DrawModel(gfxCtx, &sNvItModels[model]);
        Matrix_Pop();
    }
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

/* ---- the item box (RE1's g_itemboxSlots: one box, linked across every box in the mansion) ---- */

/** Box slot i's item (0: empty) and count. */
s32 NvItems_BoxSlot(s32 i, s32* qty) {
    if ((i < 0) || (i >= NV_BOX_SLOTS)) {
        return 0;
    }
    if (qty != NULL) {
        *qty = sNvBox[i].qty;
    }
    return sNvBox[i].id;
}

s32 NvItems_BoxSize(void) {
    return NV_BOX_SLOTS;
}

/**
 * Swap Jill's slot a with box slot b (menu_itembox_interaction's exchange). Jill's slots stay
 * packed as RE1 keeps them (an item put away closes the gap); the box keeps its places. The
 * equipped weapon is unequipped once it goes in the box. False when there was nothing to move.
 */
s32 NvItems_BoxSwap(s32 a, s32 b) {
    NvItemSlot t;
    s32 held = NvItems_Held();
    s32 k;

    if ((a < 0) || (a >= NvItems_SlotCount()) || (b < 0) || (b >= NV_BOX_SLOTS)) {
        return false;
    }
    if (a >= held) {
        a = held; // an empty slot of Jill's: the first free one
    }
    if ((a >= NvItems_SlotCount()) || ((sNvSlots[a].id == 0) && (sNvBox[b].id == 0))) {
        return false;
    }
    if (sNvEquipped == a + 1) {
        sNvEquipped = 0;
    }
    t = sNvSlots[a];
    sNvSlots[a] = sNvBox[b];
    sNvBox[b] = t;
    if (sNvSlots[a].id == 0) { // put away with nothing taken: close the gap
        for (k = a; k < NvItems_SlotCount() - 1; k++) {
            sNvSlots[k] = sNvSlots[k + 1];
        }
        sNvSlots[NvItems_SlotCount() - 1].id = sNvSlots[NvItems_SlotCount() - 1].qty = 0;
        if (sNvEquipped > a + 1) {
            sNvEquipped--;
        }
    }
    return true;
}

/*
 * Opening the box in a room (RoomEvents.cpp check_itembox_state): the lid (a room object) swings
 * open, faster and faster, past -199, then eases to a stop; the box menu then opens. When the menu
 * closes the lid snaps shut.
 */
void NvObj_SetRotZ(s32 i, s16 z);
static s8 sNvBoxState;   // g_itembox_state: 0 idle, 1 start, 2 opening, 3 settling, 4 menu open
static s16 sNvBoxTimer;  // g_short_itembox_open_timer
static s16 sNvBoxStep;   // g_counter_increase
static s16 sNvBoxAngle;  // the lid's +0x76
static s8 sNvBoxLid;     // the lid's object
static s8 sNvBoxWantPause;

/** open_itembox: start opening the box whose lid is room object lid. */
void NvBox_Open(s32 lid) {
    if (sNvBoxState == 0) {
        sNvBoxState = 1;
        sNvBoxLid = lid;
    }
}

/** Whether the box menu is (to be) open: the status screen shows the box. */
s32 NvBox_MenuOpen(void) {
    return sNvBoxState == 4;
}

/** Whether the pause menu should open now on the box (once). */
s32 NvBox_WantPause(void) {
    s32 want = sNvBoxWantPause;

    sNvBoxWantPause = false;
    if (want) {
        printf("NVBOX pause opens\n");
        fflush(stdout);
    }
    return want;
}

/** Each frame (z_nv_script.c). */
void NvBox_Update(PlayState* play) {
    switch (sNvBoxState) {
        case 1:
            sNvBoxTimer = 1;
            sNvBoxStep = 1;
            sNvBoxAngle = 0;
            sNvBoxState = 2;
            // fall through
        case 2:
            sNvBoxAngle -= sNvBoxTimer;
            sNvBoxTimer += sNvBoxStep;
            if (sNvBoxAngle < -199) {
                sNvBoxState = 3;
                sNvBoxStep = -sNvBoxStep;
            }
            break;
        case 3:
            sNvBoxAngle -= sNvBoxTimer;
            sNvBoxTimer += sNvBoxStep;
            if (sNvBoxTimer < 1) {
                sNvBoxState = 4; // settled: the box menu
                sNvBoxWantPause = true;
                printf("NVBOX lid open (%d), menu\n", sNvBoxAngle);
                fflush(stdout);
            }
            break;
        case 4:
            if (!sNvBoxWantPause && (play->pauseCtx.state == 0)) { // the menu closed
                sNvBoxState = 0;
                sNvBoxAngle = 0;
            }
            break;
    }
    if (sNvBoxState != 0 || sNvBoxAngle != 0) {
        NvObj_SetRotZ(sNvBoxLid, sNvBoxAngle);
    } else {
        NvObj_SetRotZ(sNvBoxLid, 0);
    }
}

/** A new room: no box open. */
void NvBox_Reset(void) {
    sNvBoxState = 0;
    sNvBoxAngle = 0;
    sNvBoxWantPause = false;
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
