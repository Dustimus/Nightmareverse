/*
 * Nightmareverse: MM's get-items (mm/README.md "MM's get-items").
 *
 * MM's people and chests give items by MM's get-item ids (GI_*). Here every one of them becomes a
 * SoH GetItemEntry that rides OoT's own get-item (Link holding it up, the text, the fanfare), with:
 * - MM's text for it (ours 0xB000 + MM's id),
 * - MM's model over Link's head: MM's z_draw.c get-item draw routines, ported as they are (2S2H's,
 *   already written for SoH's renderer), with MM's display lists by path (all of MM's get-item
 *   objects, converted by mm_objects.py from mm_getitem_objects.py);
 * - a mod index of its own (NV_MM_MOD_INDEX), so the give comes here (Randomizer_Item_Give hands
 *   it over: randomizer.cpp's one-line hook).
 * Both tables are MM's, generated whole by mm/tools/mm_getitems.py (nv_mm_getitems.inc).
 *
 * The inventory is one: what OoT has too (rupees, hearts, heart pieces, magic, ammo and its
 * upgrades, the bow, hookshots, lens, beans, bottles and the contents OoT knows) goes into OoT's
 * own; what only MM has (masks, MM's songs, the trade items, the Pictograph Box, the Powder Keg,
 * MM's bottle contents, swords and shields MM's own...) is kept here, by MM's item ids, saved in
 * the "termina" save section (NvMmSave.cpp). The Song of Time takes nothing of Link's but a short
 * list of event items (decided with the user: mm/README.md "What the Song of Time takes"); OoT's
 * side (rupees, ammo, bottles) it never touches.
 */
#include "global.h"
#include <stdio.h>
#include <stdlib.h>

#define NV_MM_MOD_INDEX 0x4D4D // ("MM": GetItemEntry.modIndex for MM's get-items)
#define NV_MM_TEXT(id) (0xB000 + (id))

typedef struct {
    u8 item;      // MM's item (ITEM_*)
    s8 gid;       // MM's model id (GID_*), -1 none
    u16 textId;   // MM's text
    u8 chestLong; // MM's long chest opening
} NvMmGetItemRow;

typedef struct {
    void (*draw)(PlayState* play, s16 drawId);
    const char* res[8];
} NvMmDrawItemRow;

static void NvMmGetItem_DrawBombchu(PlayState* play, s16 drawId);
static void NvMmGetItem_DrawPoes(PlayState* play, s16 drawId);
static void NvMmGetItem_DrawFairyBottle(PlayState* play, s16 drawId);
static void NvMmGetItem_DrawSkullToken(PlayState* play, s16 drawId);
static void NvMmGetItem_DrawCompass(PlayState* play, s16 drawId);
static void NvMmGetItem_DrawPotion(PlayState* play, s16 drawId);
static void NvMmGetItem_DrawGoronSword(PlayState* play, s16 drawId);
static void NvMmGetItem_DrawDekuNuts(PlayState* play, s16 drawId);
static void NvMmGetItem_DrawRecoveryHeart(PlayState* play, s16 drawId);
static void NvMmGetItem_DrawFish(PlayState* play, s16 drawId);
static void NvMmGetItem_DrawOpa0(PlayState* play, s16 drawId);
static void NvMmGetItem_DrawOpa0Xlu1(PlayState* play, s16 drawId);
static void NvMmGetItem_DrawOpa01(PlayState* play, s16 drawId);
static void NvMmGetItem_DrawXlu01(PlayState* play, s16 drawId);
static void NvMmGetItem_DrawSeahorse(PlayState* play, s16 drawId);
static void NvMmGetItem_DrawFairyContainer(PlayState* play, s16 drawId);
static void NvMmGetItem_DrawMoonsTear(PlayState* play, s16 drawId);
static void NvMmGetItem_DrawMagicArrow(PlayState* play, s16 drawId);
static void NvMmGetItem_DrawUpgrades(PlayState* play, s16 drawId);
static void NvMmGetItem_DrawRupee(PlayState* play, s16 drawId);
static void NvMmGetItem_DrawSmallRupee(PlayState* play, s16 drawId);
static void NvMmGetItem_DrawWallet(PlayState* play, s16 drawId);
static void NvMmGetItem_DrawRemains(PlayState* play, s16 drawId);

#include "nv_mm_getitems.inc"

void NvMmMatAnim_Draw(PlayState* play, const char* path);

#define RES(i) ((Gfx*)sDrawItemTable[drawId].res[i])
#define DL_OPA(i)                                                                                                 \
    do {                                                                                                          \
        if (RES(i) != NULL) {                                                                                     \
            gSPDisplayList(POLY_OPA_DISP++, RES(i));                                                              \
        }                                                                                                         \
    } while (0)
#define DL_XLU(i)                                                                                                 \
    do {                                                                                                          \
        if (RES(i) != NULL) {                                                                                     \
            gSPDisplayList(POLY_XLU_DISP++, RES(i));                                                              \
        }                                                                                                         \
    } while (0)
#define LOAD_OPA() gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW)
#define LOAD_XLU() gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW)
#define FRAMES ((s32)play->state.frames)

/* ---- MM's z_draw.c get-item routines (MM's GetItem_Draw*) ---- */

static void NvMmGetItem_DrawBombchu(PlayState* play, s16 drawId) {
    OPEN_DISPS(play->state.gfxCtx);
    POLY_OPA_DISP = Gfx_SetupDL(POLY_OPA_DISP, 23); // (MM's Gfx_SetupDL23_Opa)
    LOAD_OPA();
    DL_OPA(0);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void NvMmGetItem_DrawPoes(PlayState* play, s16 drawId) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    LOAD_OPA();
    DL_OPA(0);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    LOAD_XLU();
    DL_XLU(1);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, G_TX_RENDERTILE, 0, 0, 16, 32, 1, FRAMES, -(FRAMES * 6), 16, 32,
                                  0, 0, 1, -6));
    Matrix_Push();
    Matrix_ReplaceRotation(&play->billboardMtxF);
    LOAD_XLU();
    DL_XLU(3);
    DL_XLU(2);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void NvMmGetItem_DrawFairyBottle(PlayState* play, s16 drawId) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    LOAD_OPA();
    DL_OPA(0);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    LOAD_XLU();
    DL_XLU(1);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, G_TX_RENDERTILE, 0, 0, 32, 32, 1, FRAMES, -(FRAMES * 6), 32, 320,
                                  0, 0, 1, -6));
    Matrix_Push();
    Matrix_ReplaceRotation(&play->billboardMtxF);
    LOAD_XLU();
    DL_XLU(2);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void NvMmGetItem_DrawSkullToken(PlayState* play, s16 drawId) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    LOAD_OPA();
    DL_OPA(0);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, G_TX_RENDERTILE, 0, -(FRAMES * 5), 32, 32, 1, 0, 0, 32, 64, 0, -5,
                                  0, 0));
    LOAD_XLU();
    DL_XLU(1);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void NvMmGetItem_DrawCompass(PlayState* play, s16 drawId) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    LOAD_OPA();
    DL_OPA(0);
    POLY_XLU_DISP = Gfx_SetupDL(POLY_XLU_DISP, 5);
    LOAD_XLU();
    DL_XLU(1);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void NvMmGetItem_DrawPotion(PlayState* play, s16 drawId) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, G_TX_RENDERTILE, -FRAMES, FRAMES, 32, 32, 1, -FRAMES, FRAMES, 32,
                                  32, -1, 1, -1, 1));
    LOAD_OPA();
    DL_OPA(1);
    DL_OPA(0);
    DL_OPA(2);
    DL_OPA(3);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    LOAD_XLU();
    DL_XLU(4);
    DL_XLU(5);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void NvMmGetItem_DrawGoronSword(PlayState* play, s16 drawId) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, G_TX_RENDERTILE, FRAMES, 0, 32, 32, 1, 0, 0, 32, 32, 1, 0, 0, 0));
    LOAD_OPA();
    DL_OPA(0);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void NvMmGetItem_DrawDekuNuts(PlayState* play, s16 drawId) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, G_TX_RENDERTILE, FRAMES * 6, FRAMES * 6, 32, 32, 1, FRAMES * 6,
                                  FRAMES * 6, 32, 32, 6, 6, 6, 6));
    LOAD_OPA();
    DL_OPA(0);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void NvMmGetItem_DrawRecoveryHeart(PlayState* play, s16 drawId) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, G_TX_RENDERTILE, 0, -(FRAMES * 3), 32, 32, 1, 0, -(FRAMES * 2), 32,
                                  32, 0, -3, 0, -2));
    LOAD_XLU();
    DL_XLU(0);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void NvMmGetItem_DrawFish(PlayState* play, s16 drawId) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScrollEx(play->state.gfxCtx, G_TX_RENDERTILE, 0, FRAMES, 32, 32, 1, 0, FRAMES, 32, 32, 0, 1, 0,
                                  1));
    LOAD_XLU();
    DL_XLU(0);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void NvMmGetItem_DrawOpa0(PlayState* play, s16 drawId) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    LOAD_OPA();
    DL_OPA(0);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void NvMmGetItem_DrawOpa0Xlu1(PlayState* play, s16 drawId) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    LOAD_OPA();
    DL_OPA(0);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    LOAD_XLU();
    DL_XLU(1);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void NvMmGetItem_DrawOpa01(PlayState* play, s16 drawId) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    LOAD_OPA();
    DL_OPA(0);
    DL_OPA(1);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void NvMmGetItem_DrawXlu01(PlayState* play, s16 drawId) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    LOAD_XLU();
    DL_XLU(0);
    DL_XLU(1);
    CLOSE_DISPS(play->state.gfxCtx);
}

/** MM's GetItem_DrawSeahorse (its glow's billboard matrix isn't carried: drawn facing the camera). */
static void NvMmGetItem_DrawSeahorse(PlayState* play, s16 drawId) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    LOAD_OPA();
    DL_OPA(0);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    LOAD_XLU();
    DL_XLU(1);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    LOAD_XLU();
    DL_XLU(2);
    CLOSE_DISPS(play->state.gfxCtx);
}

/** MM's GetItem_DrawFairyContainer (the same for its billboard). */
static void NvMmGetItem_DrawFairyContainer(PlayState* play, s16 drawId) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    NvMmMatAnim_Draw(play, "mm/objects/object_gi_bottle_04/gGiFairyBottleTexAnim");
    LOAD_OPA();
    DL_OPA(0);
    LOAD_XLU();
    DL_XLU(1);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    LOAD_XLU();
    DL_XLU(2);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void NvMmGetItem_DrawMoonsTear(PlayState* play, s16 drawId) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    NvMmMatAnim_Draw(play, "mm/objects/object_gi_reserve00/gGiMoonsTearTexAnim");
    LOAD_OPA();
    DL_OPA(0);
    Matrix_ReplaceRotation(&play->billboardMtxF);
    LOAD_XLU();
    DL_XLU(1);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void NvMmGetItem_DrawMagicArrow(PlayState* play, s16 drawId) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    LOAD_OPA();
    DL_OPA(0);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    LOAD_XLU();
    DL_XLU(1);
    DL_XLU(2);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void NvMmGetItem_DrawUpgrades(PlayState* play, s16 drawId) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    LOAD_OPA();
    DL_OPA(1);
    DL_OPA(0);
    DL_OPA(2);
    DL_OPA(3);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void NvMmGetItem_DrawRupee(PlayState* play, s16 drawId) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    LOAD_OPA();
    DL_OPA(1);
    DL_OPA(0);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    LOAD_XLU();
    DL_XLU(3);
    DL_XLU(2);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void NvMmGetItem_DrawSmallRupee(PlayState* play, s16 drawId) {
    Matrix_Scale(0.7f, 0.7f, 0.7f, MTXMODE_APPLY);
    NvMmGetItem_DrawRupee(play, drawId);
}

static void NvMmGetItem_DrawWallet(PlayState* play, s16 drawId) {
    s32 i;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    LOAD_OPA();
    DL_OPA(1);
    DL_OPA(0);
    for (i = 2; i < 8; i++) {
        DL_OPA(i);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

/** MM's GetItem_DrawRemains (its object by path, no slot). */
static void NvMmGetItem_DrawRemains(PlayState* play, s16 drawId) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
    LOAD_OPA();
    DL_OPA(0);
    CLOSE_DISPS(play->state.gfxCtx);
}

/** SoH's GetItemEntry.drawFunc: MM's model for the entry (its gid). */
static void NvMmGetItem_DrawEntry(PlayState* play, GetItemEntry* entry) {
    s16 gid = (s16)entry->gid;

    if ((gid >= 0) && (gid < ARRAY_COUNT(sDrawItemTable))) {
        sDrawItemTable[gid].draw(play, gid);
    }
}

/* ---- the entries ---- */

static const NvMmGetItemRow* NvMmGetItem_Row(s32 mmGi) {
    return ((mmGi >= 1) && (mmGi <= ARRAY_COUNT(sGetItemTable))) ? &sGetItemTable[mmGi - 1] : NULL;
}

/** MM's get-item `mmGi` as a SoH GetItemEntry (Link holds it up, MM's text, MM's model); a GI_NONE
 * entry if MM has none by that id. */
GetItemEntry NvMmGetItem_Entry(s32 mmGi) {
    const NvMmGetItemRow* row = NvMmGetItem_Row(mmGi);
    GetItemEntry e = (GetItemEntry)GET_ITEM_NONE;

    if ((row == NULL) || (row->item == 0xFF)) {
        return e;
    }
    // itemId: one OoT's code always counts as not had (its songs), so the get-item always plays;
    // nothing reads it as an OoT item (the mod index sends the give here)
    e.itemId = ITEM_SONG_MINUET;
    e.drawItemId = ITEM_SONG_MINUET;
    e.field = 0;
    e.gi = (row->chestLong ? 1 : -1) * (row->gid + 1);
    e.textId = NV_MM_TEXT(row->textId);
    e.objectId = OBJECT_GI_RUPY; // (OoT's get-item object slot loads something; MM's model is drawn by path)
    e.modIndex = NV_MM_MOD_INDEX;
    e.tableId = NV_MM_MOD_INDEX;
    e.getItemId = mmGi;
    e.gid = row->gid;
    e.collectable = true;
    e.getItemFrom = ITEM_FROM_NPC;
    e.getItemCategory = ITEM_CATEGORY_LESSER;
    e.drawModIndex = NV_MM_MOD_INDEX;
    e.drawFunc = NvMmGetItem_DrawEntry;
    return e;
}

s32 NvMmGetItem_IsMm(GetItemEntry* e) {
    return e->modIndex == NV_MM_MOD_INDEX;
}

/* ---- the inventory: MM's own items ---- */

static u8 sMmItems[256]; // MM's item id -> how many (1 for most: owned)

// What the Song of Time takes: only the event items the user decided on (mm/README.md "What the
// Song of Time takes", 2026-11-20), not MM's Sram_SaveEndOfCycle. Nothing else of Link's goes:
// rupees, ammo, bottles and their contents, masks, songs, everything else carries over.
static s32 NvMmItem_EndsWithCycle(s32 item) {
    switch (item) {
        case 0x21: // ITEM_ZORA_EGG (in a bottle)
        case 0x17: // ITEM_DEKU_PRINCESS (in a bottle)
        case 0x78: // ITEM_KEY_SMALL (the dungeon's doors relock)
        case 0x28: // ITEM_MOONS_TEAR      } the trade items: the deeds' chain (the Moon's Tear at its
        case 0x29: // ITEM_DEED_LAND       } start), the Room Key, the letters; the Pendant of Memories
        case 0x2A: // ITEM_DEED_SWAMP      } goes with Anju and Kafei's story the cycle undoes
        case 0x2B: // ITEM_DEED_MOUNTAIN   }
        case 0x2C: // ITEM_DEED_OCEAN      }
        case 0x2D: // ITEM_ROOM_KEY        }
        case 0x2E: // ITEM_LETTER_MAMA     }
        case 0x2F: // ITEM_LETTER_TO_KAFEI }
        case 0x30: // ITEM_PENDANT_OF_MEMORIES
            return true;
        default: // (the Powder Keg: a balance question for later; it stays for now)
            return false;
    }
}

s32 NvMmItem_Has(s32 item) {
    return (item >= 0) && (item < 256) && (sMmItems[item] != 0);
}

s32 NvMmItem_Count(s32 item) {
    return ((item >= 0) && (item < 256)) ? sMmItems[item] : 0;
}

void NvMmItem_Remove(s32 item) {
    if ((item >= 0) && (item < 256)) {
        sMmItems[item] = 0;
    }
}

u8* NvMmItem_Array(void) { // (NvMmSave.cpp)
    return sMmItems;
}

/** MM's CHECK_QUEST_ITEM for what MM keeps as quest items: the remains, the songs, the notebook. */
s32 NvMmItem_HasQuest(s32 quest) {
    if ((quest >= 0) && (quest <= 3)) {
        return NvMmItem_Has(0x5D + quest); // ITEM_REMAINS_*
    }
    if ((quest >= 6) && (quest <= 0x11)) {
        return NvMmItem_Has(0x61 + quest - 6); // ITEM_SONG_*
    }
    if (quest == 0x12) {
        return NvMmItem_Has(0x6D); // ITEM_BOMBERS_NOTEBOOK
    }
    return false;
}

static s32 NvMmItem_OoT(s32 item);

/** MM's INV_CONTENT(item) == item: OoT's inventory for what it shares, else MM's own. */
s32 NvMmItem_Check(s32 item) {
    s32 oot = NvMmItem_OoT(item);

    if (oot != ITEM_NONE) {
        return (oot < ITEM_SWORD_KOKIRI) && (INV_CONTENT(oot) == oot);
    }
    return NvMmItem_Has(item);
}

/** MM's Inventory_DeleteItem (a script's DELETE_ITEM: the letters, the deeds, handed over). */
void NvMmItem_Delete(s32 item) {
    s32 oot = NvMmItem_OoT(item);

    if (oot == ITEM_NONE) {
        NvMmItem_Remove(item);
    } else if ((oot < ITEM_SWORD_KOKIRI) && (INV_CONTENT(oot) == oot)) {
        Inventory_DeleteItem(oot, SLOT(oot));
    }
}

/** The Song of Time (z_nv_mm_scene.c NvMmScene_SongOfTime): the event items it takes. */
void NvMmItem_NewCycle(void) {
    s32 i;

    for (i = 0; i < 256; i++) {
        if (NvMmItem_EndsWithCycle(i)) {
            sMmItems[i] = 0;
        }
    }
}

void NvMmItem_NewGame(void) {
    memset(sMmItems, 0, sizeof(sMmItems));
}

/** MM's item as OoT's, where OoT has the same thing; ITEM_NONE where it's MM's own. */
static s32 NvMmItem_OoT(s32 item) {
    switch (item) {
        case 0x01: return ITEM_BOW;
        case 0x02: return ITEM_ARROW_FIRE;
        case 0x03: return ITEM_ARROW_ICE;
        case 0x04: return ITEM_ARROW_LIGHT;
        case 0x06: return ITEM_BOMB;
        case 0x07: return ITEM_BOMBCHU;
        case 0x08: return ITEM_STICK;
        case 0x09: return ITEM_NUT;
        case 0x0A: return ITEM_BEAN;
        case 0x0E: return ITEM_LENS;
        case 0x0F: return ITEM_HOOKSHOT;
        case 0x11: return ITEM_LONGSHOT;
        case 0x12: return ITEM_BOTTLE;
        case 0x13: return ITEM_POTION_RED;
        case 0x14: return ITEM_POTION_GREEN;
        case 0x15: return ITEM_POTION_BLUE;
        case 0x16: return ITEM_FAIRY;
        case 0x18: return ITEM_MILK_BOTTLE;
        case 0x19: return ITEM_MILK_HALF;
        case 0x1A: return ITEM_FISH;
        case 0x1B: return ITEM_BUG;
        case 0x1C: return ITEM_BLUE_FIRE;
        case 0x1D: return ITEM_POE;
        case 0x1E: return ITEM_BIG_POE;
        case 0x4D: return ITEM_SWORD_KOKIRI;
        case 0x53: return ITEM_QUIVER_30;
        case 0x54: return ITEM_QUIVER_40;
        case 0x55: return ITEM_QUIVER_50;
        case 0x56: return ITEM_BOMB_BAG_20;
        case 0x57: return ITEM_BOMB_BAG_30;
        case 0x58: return ITEM_BOMB_BAG_40;
        case 0x5A: return ITEM_WALLET_ADULT;
        case 0x5B: return ITEM_WALLET_GIANT;
        case 0x6F: return ITEM_HEART_CONTAINER;
        case 0x70: return ITEM_HEART_PIECE_2;
        case 0x7B: return ITEM_HEART_PIECE_2;
        case 0x79: return ITEM_MAGIC_SMALL;
        case 0x7A: return ITEM_MAGIC_LARGE;
        case 0x83: return ITEM_HEART;
        case 0x84: return ITEM_RUPEE_GREEN;
        case 0x85: return ITEM_RUPEE_BLUE;
        case 0x87: return ITEM_RUPEE_RED;
        case 0x88: return ITEM_RUPEE_PURPLE;
        case 0x8A: return ITEM_RUPEE_GOLD;
        case 0x8B: return ITEM_STICKS_5;
        case 0x8C: return ITEM_STICKS_10;
        case 0x8D: return ITEM_NUTS_5;
        case 0x8E: return ITEM_NUTS_10;
        case 0x8F: return ITEM_BOMBS_5;
        case 0x90: return ITEM_BOMBS_10;
        case 0x91: return ITEM_BOMBS_20;
        case 0x92: return ITEM_BOMBS_30;
        case 0x97: return ITEM_BOMBCHUS_20;
        case 0x9A: return ITEM_BOMBCHUS_5;
        case 0x9B: return ITEM_STICK_UPGRADE_20;
        case 0x9C: return ITEM_STICK_UPGRADE_30;
        case 0x9D: return ITEM_NUT_UPGRADE_30;
        case 0x9E: return ITEM_NUT_UPGRADE_40;
        case 0xA0: return ITEM_MILK_BOTTLE;
        default: return ITEM_NONE;
    }
}

/** MM's Item_Give for an MM item: into OoT's inventory where OoT has it, else kept here. */
void NvMmItem_Give(PlayState* play, s32 item) {
    s32 oot = NvMmItem_OoT(item);

    switch (item) { // MM's amounts OoT's items don't come in
        case 0x86: // ITEM_RUPEE_10
            Rupees_ChangeBy(10);
            return;
        case 0x89: // ITEM_RUPEE_SILVER
            Rupees_ChangeBy(100);
            return;
        case 0x93: // ITEM_ARROWS_10
        case 0x94: // ITEM_ARROWS_30
        case 0x95: // ITEM_ARROWS_40
        case 0x96: // ITEM_ARROWS_50
            Inventory_ChangeAmmo(ITEM_BOW, (item == 0x93) ? 10 : (item == 0x94) ? 30 : (item == 0x95) ? 40 : 50);
            return;
        case 0x98: // ITEM_BOMBCHUS_10
        case 0x99: // ITEM_BOMBCHUS_1
            Inventory_ChangeAmmo(ITEM_BOMBCHU, (item == 0x98) ? 10 : 1);
            return;
        default:
            break;
    }
    if (oot != ITEM_NONE) {
        Item_Give(play, oot);
        return;
    }
    if (item == 0x0C) { // the Powder Keg: one at a time, as MM
        sMmItems[item] = 1;
    } else if (sMmItems[item] < 0xFF) {
        sMmItems[item] = (sMmItems[item] == 0) ? 1 : sMmItems[item] + ((item >= 0x13) && (item <= 0x27));
    }
    if (getenv("NV_LOG") != NULL) {
        printf("NVMM item 0x%02X kept (MM's own)\n", item);
        fflush(stdout);
    }
}

/** Randomizer_Item_Give's hook (randomizer.cpp): an MM get-item handed over. */
void NvMmGetItem_Give(PlayState* play, GetItemEntry* e) {
    const NvMmGetItemRow* row = NvMmGetItem_Row(e->getItemId < 0 ? -e->getItemId : e->getItemId);

    if (row != NULL) {
        NvMmItem_Give(play, row->item);
    }
}

/** The get-item fanfare (z_player.c's hook): MM's, by what it is; true if played. */
s32 NvMmGetItem_Fanfare(GetItemEntry* e) {
    const NvMmGetItemRow* row;
    s32 item;

    if (!NvMmGetItem_IsMm(e) || ((row = NvMmGetItem_Row(e->getItemId < 0 ? -e->getItemId : e->getItemId)) == NULL)) {
        return false;
    }
    item = row->item;
    if (((item >= 0x84) && (item <= 0x9A)) || (item == 0x83)) { // rupees, ammo, a recovery heart
        Audio_PlaySfxGeneral(NA_SE_SY_GET_BOXITEM, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    } else if (item == 0x6F) {
        Audio_PlayFanfare(NA_BGM_HEART_GET | 0x900);
    } else if ((item == 0x70) || (item == 0x7B)) {
        Audio_PlayFanfare(NA_BGM_SMALL_ITEM_GET);
    } else {
        Audio_PlayFanfare(NA_BGM_ITEM_GET | 0x900);
    }
    return true;
}

/** The chests in Termina (z_en_box.c's hook): an MM chest's item (params' get-item field is MM's
 * get-item there) as MM's entry; false outside Termina. */
s32 NvMmGetItem_ChestEntry(PlayState* play, Actor* chest, GetItemEntry* out) {


    if (!NvMm_IsScene(play->sceneNum)) {
        return false;
    }
    *out = NvMmGetItem_Entry((chest->params >> 5) & 0x7F);
    out->getItemId = -out->getItemId; // (a chest's: negative, as OoT's)
    return out->modIndex == NV_MM_MOD_INDEX;
}
