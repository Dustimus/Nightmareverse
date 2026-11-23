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
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>

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
static u8 sNvGunBtn;    // the C button it's on: 0 C-Left, 1 C-Down, 2 C-Right (saved in the same byte's top bits)

/** The run's items for the typewriter's save (z_nv_save.c): its size, written to p if not NULL. */
u32 NvItems_SaveState(u8* p) {
    u32 n = sizeof(sNvSlots) + sizeof(sNvBox) + 2;

    if (p != NULL) {
        memcpy(p, sNvSlots, sizeof(sNvSlots));
        memcpy(p + sizeof(sNvSlots), sNvBox, sizeof(sNvBox));
        p[n - 2] = sNvMapsOwned;
        p[n - 1] = (u8)((sNvEquipped & 0x0F) | (sNvGunBtn << 4));
    }
    return n;
}

void NvItems_LoadState(const u8* p) {
    u32 n = sizeof(sNvSlots) + sizeof(sNvBox) + 2;

    memcpy(sNvSlots, p, sizeof(sNvSlots));
    memcpy(sNvBox, p + sizeof(sNvSlots), sizeof(sNvBox));
    sNvMapsOwned = p[n - 2];
    sNvEquipped = (s8)(p[n - 1] & 0x0F);
    sNvGunBtn = (p[n - 1] >> 4) & 3; // (a save from before: 0, C-Left)
    if (sNvGunBtn > 2) {
        sNvGunBtn = 0;
    }
}

/** A new run: nothing carried, the box empty. */
void NvItems_NewRun(void) {
    memset(sNvSlots, 0, sizeof(sNvSlots));
    memset(sNvBox, 0, sizeof(sNvBox));
    sNvMapsOwned = 0;
    sNvEquipped = 0;
    sNvGunBtn = 0;
}

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

/**
 * RE1's text from its English executable ("re1/exe_text", re1/tools/soh_text.py): kind 0 the shared
 * messages (by id & 0x3F), 1 the item names and 2 their descriptions (by item id - 1). NULL when
 * the mod was made without it.
 */
const char* NvText_Get(s32 kind, s32 i) {
    static const u8* sText;
    static s32 sTried;
    u16 counts[3];
    s32 base = 0;
    s32 k;
    u32 off;

    if (!sTried) {
        sTried = true;
        if (ResourceMgr_FileExists("re1/exe_text")) {
            sText = (const u8*)ResourceGetDataByName("re1/exe_text");
            if ((sText != NULL) && (memcmp(sText, "RXTX", 4) != 0)) {
                sText = NULL;
            }
        }
    }
    if ((sText == NULL) || (kind < 0) || (kind > 2)) {
        return NULL;
    }
    memcpy(counts, sText + 4, sizeof(counts));
    if ((i < 0) || (i >= counts[kind])) {
        return NULL;
    }
    for (k = 0; k < kind; k++) {
        base += counts[k];
    }
    memcpy(&off, sText + 10 + 4 * (base + i), 4);
    return (const char*)sText + 10 + 4 * (counts[0] + counts[1] + counts[2]) + off;
}

const char* NvItems_Name(s32 id) {
    const char* re1 = NvText_Get(1, id - 1); // (RE1's own name, from its executable)

    if ((re1 != NULL) && (re1[0] != '\0')) {
        return re1;
    }
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

/**
 * Check on slot i (the item viewer's action 3, MainMenu.cpp 0x0044e920): a Doom Book opens to its
 * medal, in the same slot - Doom Book 2 (0x3F) the Wolf Medal (0x24), Doom Book 1 (0x40) the Eagle
 * Medal (0x25). The new item, or 0 when checking it changes nothing.
 */
s32 NvItems_CheckOpen(s32 i) {
    if ((i < 0) || (i >= NvItems_SlotCount()) || (sNvSlots[i].id < 0x3F) || (sNvSlots[i].id > 0x40)) {
        return 0;
    }
    sNvSlots[i].id = sNvSlots[i].id - 0x3F + 0x24;
    sNvSlots[i].qty = 1;
    printf("NVITEM checked a Doom Book: %02X in slot %d\n", sNvSlots[i].id, (int)i);
    NvItems_Log();
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
    if (getenv("NV_EQUIP") != NULL) { // NV_EQUIP=n: equip slot n once given (testing)
        s32 NvItems_ToggleEquip(s32 i);

        while (sscanf(s, "%i,%i%n", &id, &qty, &used) == 2) {
            NvItems_Give(id, qty);
            s += used;
            if (*s != ';') {
                break;
            }
            s++;
        }
        NvItems_ToggleEquip(atoi(getenv("NV_EQUIP")));
        printf("NVGIVE equipped slot %d now %d\n", atoi(getenv("NV_EQUIP")), (int)sNvEquipped);
        NvItems_Log();
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

/** One model as soh_scene.py model_bytes packs it, at *pd (moved past it): the cells are read in place. */
static void NvIt_ParseModel(const u8** pd, NvItModel* m) {
    const u8* d = *pd;
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
    *pd = d;
}

/** For z_nv_weapon.c: a model at d (soh_scene.py model_bytes), kept until freed. */
void* NvModel_Parse(const u8* d) {
    NvItModel* m = calloc(1, sizeof(NvItModel));

    NvIt_ParseModel(&d, m);
    return m;
}

/** The model's extent: its vertices' bounds (the model's own units). */
void NvModel_Bounds(void* model, s16 mn[3], s16 mx[3]) {
    NvItModel* m = model;
    s32 b, v, k;

    for (k = 0; k < 3; k++) {
        mn[k] = 0x7FFF, mx[k] = -0x7FFF;
    }
    for (b = 0; b < m->numBatches; b++) {
        for (v = 0; v < m->batches[b].numVerts; v++) {
            for (k = 0; k < 3; k++) {
                s16 c = m->batches[b].vtx[v].v.ob[k];

                mn[k] = MIN(mn[k], c), mx[k] = MAX(mx[k], c);
            }
        }
    }
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
        NvIt_ParseModel(&d, &models[i]);
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

/** The render state RE1's models draw with (textured, z-buffered, the masks in front). */
static void NvIt_Setup(GraphicsContext* gfxCtx) {
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
}

/** For z_nv_weapon.c: draw a model with the current matrix. */
void NvModel_Draw(GraphicsContext* gfxCtx, void* model) {
    NvIt_Setup(gfxCtx);
    NvIt_DrawModel(gfxCtx, (NvItModel*)model);
}

#define NV_PSX_ANGLE(a) ((a) * (2.0f * M_PI / 4096.0f))

/**
 * The frame of a carrier that isn't a room object, in RE1's space (InitScaMatrix on the player's
 * or an enemy's scaMatrixData): 0xFE the player (Link, where he stands, turned as he faces), 0x80 +
 * n the enemy in slot n. False for anything else (or an empty slot).
 */
static s32 NvIt_ApplyCarrier(s32 parent) {
    extern PlayState* gPlayState;
    s32 NvEnemy_Place(s32 slot, s32* t, s16* angle);
    s32 t[3];
    s16 angle;

    if (parent == 0xFE) {
        Player* p = (gPlayState != NULL) ? GET_PLAYER(gPlayState) : NULL;

        if (p == NULL) {
            return false;
        }
        t[0] = (s32)(p->actor.world.pos.x / NV_RE1_SCALE);
        t[1] = (s32)(-p->actor.world.pos.y / NV_RE1_SCALE);
        t[2] = (s32)(-p->actor.world.pos.z / NV_RE1_SCALE);
        angle = (s16)(((0x4000 - p->actor.shape.rot.y) & 0xFFFF) >> 4); // (RE1's facing)
    } else if ((parent >= 0x80) && (parent < 0xFE)) {
        if (!NvEnemy_Place(parent & 0x7F, t, &angle)) {
            return false;
        }
    } else {
        return false;
    }
    Matrix_Translate(t[0], t[1], t[2], MTXMODE_APPLY);
    Matrix_RotateY(NV_PSX_ANGLE(angle), MTXMODE_APPLY);
    return true;
}

/**
 * Apply room object i's place in RE1's space to the current matrix, its parents' first
 * (InitScaMatrix: an object on another object moves with it, one on the player or an enemy with
 * them). False when it isn't placed.
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
    } else if (parent != 0xFF) {
        NvIt_ApplyCarrier(parent);
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
        } else if (((parent < 0x80) && NvIt_ApplyObject(parent, 0)) || ((parent == 0xFE) && NvIt_ApplyCarrier(parent))) {
            Matrix_Translate(pos[0], pos[1], pos[2], MTXMODE_APPLY); // (an offset in its carrier's frame)
        } else {
            Matrix_Pop();
            continue;
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
        case 0x42: // serum (Yawn's poison)
            return 0x10;
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

/**
 * fire_consume_ammo_stack (PlayerAnimations.cpp 0x0045a580): the trigger on an empty gun loads it
 * from the biggest stack of its ammo (item weapon + 9), up to the gun's capacity; a stack it
 * empties is gone. Returns the rounds loaded (0: no ammo for it).
 */
s32 NvItems_Reload(s32 gun) {
    // g_ItemMaxQty for the guns that load (2 the handgun .. 9 the bazooka's flame rounds)
    static const u8 sCap[10] = { 0, 0, 15, 7, 6, 6, 240, 6, 6, 6 };
    s32 weapon;
    s32 ammo;
    s32 best = -1;
    s32 i;
    s32 load;

    if ((gun < 0) || (gun >= NvItems_SlotCount())) {
        return 0;
    }
    weapon = sNvSlots[gun].id;
    if ((weapon < 2) || (weapon > 9)) {
        return 0;
    }
    ammo = weapon + 9;
    for (i = 0; i < NvItems_SlotCount(); i++) {
        if ((sNvSlots[i].id == ammo) && (sNvSlots[i].qty > 0) && ((best < 0) || (sNvSlots[i].qty > sNvSlots[best].qty))) {
            best = i;
        }
    }
    if (best < 0) {
        return 0;
    }
    load = MIN(sNvSlots[best].qty, sCap[weapon]);
    sNvSlots[gun].qty = load;
    sNvSlots[best].qty -= load;
    if (sNvSlots[best].qty == 0) {
        sNvSlots[best].qty = 1;
        NvItems_TakeOne(best);
    }
    return load;
}

/* ---- the item box (RE1's g_itemboxSlots: one box, linked across every box in the mansion) ---- */

/** Box slot i's item (0: empty) and count. */
/**
 * Maps were once taken as items (RE1 never puts one in a slot: pickup_key_event only marks it
 * owned): an older save's, in Jill's slots or the box, go. Returns their map indices as bits.
 */
s32 NvItems_RemoveMaps(void) {
    s32 found = 0;
    s32 i;

    for (i = 0; i < NV_BOX_SLOTS; i++) {
        if ((sNvBox[i].id >= 0x4E) && (sNvBox[i].id <= 0x53)) {
            found |= 1 << (sNvBox[i].id - 0x4E);
            sNvBox[i].id = sNvBox[i].qty = 0;
        }
    }
    for (i = 0x4E; i <= 0x53; i++) {
        while (NvItems_Find(i) >= 0) {
            found |= 1 << (i - 0x4E);
            NvItems_Remove(i);
        }
    }
    return found;
}

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
 * open, faster and faster, past -199, then eases to a stop; the box screen then opens (z_nv_box.c).
 * When it closes the lid snaps shut.
 */
void NvObj_SetRotZ(s32 i, s16 z);
static s8 sNvBoxState;   // g_itembox_state: 0 idle, 1 start, 2 opening, 3 settling, 4 menu open
static s16 sNvBoxTimer;  // g_short_itembox_open_timer
static s16 sNvBoxStep;   // g_counter_increase
static s16 sNvBoxAngle;  // the lid's +0x76
static s8 sNvBoxLid;     // the lid's object

/** open_itembox: start opening the box whose lid is room object lid. */
void NvBox_Open(s32 lid) {
    if (sNvBoxState == 0) {
        sNvBoxState = 1;
        sNvBoxLid = lid;
    }
}

/** Whether the box screen is up (z_nv_box.c): the lid has settled. */
s32 NvBox_MenuOpen(void) {
    return sNvBoxState == 4;
}

/** The box screen closed (check_itembox_state's state 4): the lid shuts. */
void NvBox_Close(void) {
    sNvBoxState = 0;
    sNvBoxAngle = 0;
    NvObj_SetRotZ(sNvBoxLid, 0);
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
                sNvBoxState = 4; // settled: the box screen (z_nv_box.c)
                printf("NVBOX lid open (%d), menu\n", sNvBoxAngle);
                fflush(stdout);
            }
            break;
    }
    NvObj_SetRotZ(sNvBoxLid, sNvBoxAngle);
}

/** A new room: no box open. */
void NvBox_Reset(void) {
    sNvBoxState = 0;
    sNvBoxAngle = 0;
}

/** A round fired from slot i's weapon (z_nv_weapon.c): its count goes down, the gun stays. */
void NvItems_SpendRound(s32 i) {
    if ((i >= 0) && (i < NvItems_SlotCount()) && (sNvSlots[i].qty > 0)) {
        sNvSlots[i].qty--;
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
 * Slot i's weapon equipped onto a C button (0 C-Left, 1 C-Down, 2 C-Right), as OoT's item screen
 * equips: the status screen, a C button pressed on it. True when the slot holds a weapon.
 */
s32 NvItems_EquipTo(s32 i, s32 btn) {
    if (!NvItems_Equippable(NvItems_Slot(i, NULL)) || (btn < 0) || (btn > 2)) {
        return false;
    }
    sNvEquipped = i + 1;
    sNvGunBtn = btn;
    return true;
}

/** The C button the equipped weapon is on (0 C-Left, 1 C-Down, 2 C-Right). */
s32 NvItems_GunButton(void) {
    return sNvGunBtn;
}

/* ---- poison (RE1's player status: the snakes', the wasps', the spiders' and the first Yawn's) ---- */

// RE1's poison bits (the player's healthStatusFlags): 0x02 a creature's venom (the blue herb
// cures it), 0x20 the first Yawn's (only the serum does), 0x40 a room's gas (the scripts raise and
// drop it: player_prop_set 8; fast, and it can kill)
u8 gNvRe1Poison;

#ifndef NV_RE1_JILL_HEALTH
#define NV_RE1_JILL_HEALTH 96.0f // Jill's maximum health in RE1
#endif

/** A venomous hit lands: poisoned when the roll says so (RE1 rolls it per creature). */
void NvPoison_Give(s32 rolledIn) {
    if (rolledIn) {
        gNvRe1Poison |= 0x02;
    }
}

/**
 * Each frame in the mansion (z_nv_script.c): Yawn's poison follows its story flag (RE1 raises it
 * at the bite, bank 1 bit 0x43); poisoned, Link loses 2 of RE1's 96 every 120 of RE1's ticks
 * (four seconds), as Jill does, and it never kills: it stops at the last bit of a heart.
 */
void NvPoison_Update(PlayState* play) {
    static s32 sTimer;
    static f32 sOwed;
    s32 NvFlags_Test(s32 bank, s32 bit);
    s32 whole;

    if (NvFlags_Test(1, 0x43)) {
        gNvRe1Poison |= 0x20;
    }
    if (!(gNvRe1Poison & 0x62) || (gSaveContext.health <= ((gNvRe1Poison & 0x40) ? 0 : 1))) {
        sTimer = 0;
        return;
    }
    if (++sTimer < ((gNvRe1Poison & 0x40) ? 5 : 80)) { // four seconds at OoT's 20 a second; the gas, 7 of RE1's ticks
        return;
    }
    sTimer = 0;
    sOwed += 2.0f * gSaveContext.healthCapacity / NV_RE1_JILL_HEALTH;
    whole = (s32)sOwed;
    if (whole > 0) {
        sOwed -= whole;
        whole = MIN(whole, gSaveContext.health - ((gNvRe1Poison & 0x40) ? 0 : 1));
        if (whole > 0) {
            Health_ChangeBy(play, -whole);
        }
    }
}

/**
 * Use slot i's item (menu_item_use_heal): a heal fills Link's hearts by RE1's thirds; the blue
 * herb's cure (0x20) ends a creature's poison, the serum's (0x10) Yawn's. As RE1, an item that
 * would do nothing isn't used up.
 * Returns 1 used, 0 nothing happened, -1 not usable here.
 */
/** The slot holding item id, or -1 (get_item_slot). */
s32 NvItems_Find(s32 id) {
    s32 i;

    for (i = 0; i < NvItems_SlotCount(); i++) {
        if ((sNvSlots[i].id == id) && (id != 0)) {
            return i;
        }
    }
    return -1;
}

/** cmd_item_remove: item id's slot emptied, the rest closing up. True when she had it. */
s32 NvItems_Remove(s32 id) {
    s32 i = NvItems_Find(id);

    if (i < 0) {
        return false;
    }
    sNvSlots[i].qty = 1;
    NvItems_TakeOne(i);
    return true;
}

/** cmd_player_prop_set 0: nothing equipped. */
void NvItems_Unequip(void) {
    sNvEquipped = 0;
}

/** The equipped slot's item id (0: nothing equipped), as cmd_equipped_item_test reads it. */
s32 NvItems_EquippedId(void) {
    return (sNvEquipped > 0) ? sNvSlots[sNvEquipped - 1].id : 0;
}

/**
 * cmd_item_count_test's sum: the quantities of an item group she carries (10 any, 0x0B the
 * Beretta's, 0x0C the shotgun's, 0x0D the Pythons', 0x0F item 6's, 0x10-0x12 the launcher's;
 * otherwise the item itself or the weapon 0x0B below it); -1 when she has none of the group.
 */
s32 NvItems_GroupCount(s32 group) {
    s32 total = 0;
    s32 count = 0;
    s32 i;

    for (i = 0; i < NvItems_Held(); i++) {
        s32 id = sNvSlots[i].id;
        s32 q = sNvSlots[i].qty;

        if ((group == id) || ((u8)(id - group) == (u8)-0x0B)) {
            total = q;
        }
        switch (group) {
            case 0x0A:
                total = q;
                count++;
                break;
            case 0x0B:
            case 0x0C:
                if (id == group - 9) {
                    total += q;
                    count++;
                }
                break;
            case 0x0D:
                if ((id == 4) || (id == 5)) {
                    total += q;
                    count++;
                }
                break;
            case 0x0F:
                if (id == 6) {
                    total += q;
                    count++;
                }
                break;
            case 0x10:
            case 0x11:
            case 0x12:
                if ((id >= 7) && (id <= 9)) {
                    total += q;
                    count++;
                }
                break;
        }
    }
    return (count == 0) ? -1 : total;
}

void NvScript_SetUsedItem(s32 id);

/**
 * Use from the status screen (menu_item_use_item): the heals by RE1's table; the special items and
 * keys (0x1B-0x3D) and the red book (0x3E) where the room's script allows them this frame (its
 * item-use flags, bank 9, bit id - 0x1B): one is spent, and the script sees it used
 * (cmd_used_item_test). 1 healed, 2 used on the room (the menu closes), 0 nothing to do now,
 * -1 not usable at all.
 */
s32 NvItems_Use(PlayState* play, s32 i) {
    s32 id = NvItems_Slot(i, NULL);
    u8 heal = NvItems_Heal(id);
    s32 cap = gSaveContext.healthCapacity;
    s32 amount;

    s32 used = false;

    if (((id >= 0x1B) && (id < 0x3D + 1)) || (id == 0x3E)) {
        if (!NvFlags_Test(9, id - 0x1B)) {
            return 0;
        }
        NvScript_SetUsedItem(id);
        if (sNvSlots[i].qty > 0) { // (a count of 0 isn't spent: RE1 only takes one off a count)
            NvItems_TakeOne(i);
        }
        printf("NVITEM used %02X on the room\n", id);
        fflush(stdout);
        return 2;
    }
    if (heal == 0) {
        return -1;
    }
    if (((heal & 0x0F) != 0) && (gSaveContext.health < cap)) {
        amount = ((heal & 0x0F) == 1) ? cap / 3 : ((heal & 0x0F) == 2) ? cap * 2 / 3 : cap;
        Health_ChangeBy(play, amount);
        used = true;
    }
    if ((heal & 0x20) && (gNvRe1Poison & 0x02)) { // the blue herb
        gNvRe1Poison &= ~0x02;
        used = true;
    }
    if ((heal & 0x10) && (gNvRe1Poison & 0x20)) { // the serum
        void NvFlags_Op(s32 bank, s32 bit, s32 mode);

        gNvRe1Poison &= ~0x20;
        NvFlags_Op(1, 0x43, 1);
        used = true;
    }
    if (!used) {
        return 0;
    }
    NvItems_TakeOne(i);
    return 1;
}

/** RE1's herb mixes, for a re1.o2r made without the executable: the item a + b make, or 0. */
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

/**
 * RE1's item records and combine tables ("re1/combine", re1/tools/soh_weapons.py combine_blob):
 * "RCMB", u16 n, n item records of 4 bytes (g_ItemMaxQty: the most a slot holds, the icon, the
 * combine table or 0x80+ for none, flags), u16 t, t u16 offsets into the data, u16 size, the data.
 * A table is a count, then { the other item, the first's new id, the other's new id, the effect }.
 */
static const u8* sNvCmbItems;
static u16 sNvCmbItemCount;
static const u8* sNvCmbData;
static u16 sNvCmbOffs[64];
static u16 sNvCmbTables;
static u16 sNvCmbSize;

static s32 NvItems_CombineReady(void) {
    static s32 sState; // 0 untried, 1 loaded, -1 missing
    const u8* d;
    s32 p;

    if (sState != 0) {
        return sState > 0;
    }
    sState = -1;
    if (!ResourceMgr_FileExists("re1/combine")) {
        return false;
    }
    d = (const u8*)ResourceGetDataByName("re1/combine");
    if ((d == NULL) || (memcmp(d, "RCMB", 4) != 0)) {
        return false;
    }
    memcpy(&sNvCmbItemCount, d + 4, 2);
    sNvCmbItems = d + 6;
    p = 6 + sNvCmbItemCount * 4;
    memcpy(&sNvCmbTables, d + p, 2);
    if (sNvCmbTables > ARRAY_COUNT(sNvCmbOffs)) {
        return false;
    }
    memcpy(sNvCmbOffs, d + p + 2, sNvCmbTables * 2);
    p += 2 + sNvCmbTables * 2;
    memcpy(&sNvCmbSize, d + p, 2);
    sNvCmbData = d + p + 2;
    sState = 1;
    return true;
}

/** The most item id's slot holds (g_ItemMaxQty's byte 0). */
static s32 NvItems_MaxQty(s32 id) {
    if (!NvItems_CombineReady() || (id < 0) || (id >= sNvCmbItemCount)) {
        return NV_STACK_MAX;
    }
    return sNvCmbItems[id * 4];
}

/** Item id's combine record with other (4 bytes), or NULL. */
static const u8* NvItems_CombineRecord(s32 id, s32 other) {
    s32 t;
    s32 n;
    s32 off;

    if (!NvItems_CombineReady() || (id <= 0) || (id >= sNvCmbItemCount)) {
        return NULL;
    }
    t = sNvCmbItems[id * 4 + 2];
    if ((t >= 0x80) || (t >= sNvCmbTables)) {
        return NULL;
    }
    off = sNvCmbOffs[t];
    for (n = sNvCmbData[off], off++; (n > 0) && (off + 4 <= sNvCmbSize); n--, off += 4) {
        if (sNvCmbData[off] == other) {
            return sNvCmbData + off;
        }
    }
    return NULL;
}

/** Whether the first item can be combined at all (it has a table; RE1 ignores the press if not). */
s32 NvItems_Combinable(s32 id) {
    if (!NvItems_CombineReady()) {
        return true; // (the herbs' list)
    }
    return (id > 0) && (id < sNvCmbItemCount) && (sNvCmbItems[id * 4 + 2] < 0x80);
}

/** rearrange_item_slots (SaveLoadScreen.cpp): emptied slots close up, the equipped one moving with them. */
static void NvItems_Compact(void) {
    s32 n = NvItems_SlotCount();
    s32 eq = sNvEquipped - 1;
    s32 r;
    s32 w = 0;

    for (r = 0; r < n; r++) {
        if (sNvSlots[r].id == 0) {
            if (eq == r) {
                eq = -1; // (its item is gone)
            }
            continue;
        }
        if (w != r) {
            sNvSlots[w] = sNvSlots[r];
            if (eq == r) {
                eq = w;
            }
        }
        w++;
    }
    for (; w < n; w++) {
        sNvSlots[w].id = sNvSlots[w].qty = 0;
    }
    sNvEquipped = eq + 1;
}

static u8 sNvCmbAfter; // the last combine's message after it (NvItems_CombineAfter)

/**
 * The combine's effect on slots a (the first chosen) and b (g_CombineEffectFunctions, MainMenu.cpp):
 * 1 / 2 load the gun from its ammo (the first / the other is the gun), 3 two stacks merge (to 250),
 * 4 the chemicals gas (bank 0 flag 0x16), 5 the mixing fails, 6 / 7 an empty one is filled from
 * the other (the grenade launcher's loads). Done after the new ids are in.
 */
static void NvItems_CombineEffect(s32 effect, NvItemSlot* x, NvItemSlot* y) {
    s32 cap;
    s32 sum;

    switch (effect & 7) {
        case 2: // (as 1, the other way round)
            NvItems_CombineEffect(1, y, x);
            break;
        case 1:
            cap = NvItems_MaxQty(x->id);
            sum = y->qty + (x->qty & 0x7F);
            if (sum > cap) {
                x->qty = cap;
                y->qty = sum - cap;
            } else {
                x->qty = sum;
                y->id = 0;
            }
            break;
        case 3:
            sum = x->qty + y->qty;
            if (sum > NV_STACK_MAX) {
                x->qty = NV_STACK_MAX;
                y->qty = sum - NV_STACK_MAX;
            } else {
                x->qty = sum;
                y->id = 0;
            }
            break;
        case 4: {
            void NvFlags_Op(s32 bank, s32 bit, s32 mode);

            NvFlags_Op(0, 0x16, 0); // (mode 0 raises it)
            sNvCmbAfter = 2;
            break;
        }
        case 5:
            sNvCmbAfter = 3;
            break;
        case 7:
            NvItems_CombineEffect(6, y, x);
            break;
        case 6:
            cap = NvItems_MaxQty(x->id);
            if (y->qty > cap) {
                x->qty = cap;
                y->qty -= cap;
            } else {
                x->qty = y->qty;
                y->id = 0;
            }
            break;
    }
}

/** The ids (and the effect) of a's record with b; the slots close up. */
static void NvItems_CombineDo(s32 a, s32 b, const u8* rec, s32 withEffect) {
    sNvSlots[a].id = rec[1];
    sNvSlots[b].id = rec[2];
    if (withEffect && (rec[3] != 0)) {
        NvItems_CombineEffect(rec[3], &sNvSlots[a], &sNvSlots[b]);
    }
    NvItems_Compact();
}

/**
 * menu_item_check_combine (MainMenu.cpp 0x00401960): slot a (the first chosen) with slot b.
 * 0: they don't combine; 1: chemicals, outside the drug storehouse (RE1's message 0xF2);
 * 2: herbs that mix, after the question (0xF5: NvItems_CombineHerbs on yes); 3: herbs that don't
 * (0xF6); 4: combined (NvItems_CombineAfter: a message after it).
 */
s32 NvItems_Combine(s32 a, s32 b) {
    const u8* rec;
    s32 ida;
    s32 idb;

    sNvCmbAfter = 0;
    if ((a == b) || (NvItems_Slot(a, NULL) == 0) || (NvItems_Slot(b, NULL) == 0)) {
        return 0;
    }
    ida = sNvSlots[a].id;
    idb = sNvSlots[b].id;
    if (!NvItems_CombineReady()) { // (no executable's tables: the herbs alone, no question)
        s32 mix = NvItems_Mix(ida, idb);

        if (mix == 0) {
            return 0;
        }
        sNvSlots[a].id = mix;
        sNvSlots[a].qty = 1;
        NvItems_TakeOne(b);
        return 4;
    }
    rec = NvItems_CombineRecord(ida, idb);
    if (rec == NULL) {
        return ((ida >= 0x43) && (ida <= 0x4B) && (idb >= 0x43) && (idb <= 0x4B)) ? 3 : 0;
    }
    if ((rec[3] == 6) && (sNvSlots[a].qty != 0)) { // (the one being filled must be empty)
        return 0;
    }
    if ((rec[3] == 7) && (sNvSlots[b].qty != 0)) {
        return 0;
    }
    if ((ida >= 0x13) && (ida <= 0x1A)) { // the chemicals mix only in the drug storehouse (stage 3, room 9: 409)
        s32 stage;
        s32 room;
        s32 camera;
        void NvScript_Where(s32* stage, s32* room, s32* camera);

        NvScript_Where(&stage, &room, &camera);
        if ((stage != 3) || (room != 9)) {
            return 1;
        }
    } else if ((ida >= 0x43) && (ida <= 0x4B)) {
        return 2;
    }
    NvItems_CombineDo(a, b, rec, true);
    printf("NVITEM combined %02X with %02X (effect %d)\n", ida, idb, rec[3]);
    NvItems_Log();
    return 4;
}

/** The herbs' question answered yes (menu_item_apply_combine): only the ids change. */
s32 NvItems_CombineHerbs(s32 a, s32 b) {
    const u8* rec;

    if ((a == b) || (NvItems_Slot(a, NULL) == 0) || (NvItems_Slot(b, NULL) == 0)) {
        return false;
    }
    rec = NvItems_CombineRecord(sNvSlots[a].id, sNvSlots[b].id);
    if (rec == NULL) {
        return false;
    }
    NvItems_CombineDo(a, b, rec, false);
    printf("NVITEM mixed the herbs\n");
    NvItems_Log();
    return true;
}

/** After the last combine: 0 nothing, 2 the gas (0xF3, then the menu closes), 3 it failed (0xF4). */
s32 NvItems_CombineAfter(void) {
    return sNvCmbAfter;
}
