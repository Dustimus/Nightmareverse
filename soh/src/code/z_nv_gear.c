/*
 * Nightmareverse: the gear page, a second page in the pause menu's Equipment column (under OoT's
 * Equipment; NV_GAMES.md "The swords, decided", the user 2026-11-24). A plain 24-slot page (6 by 4,
 * as MM's item pages), not an equipment-shaped one: room to work with while the games' swords and
 * shields come in, to be organized later. Its frame is MM's mask page's with the title left off
 * (mm/tools/mm_pause.py: gNvPauseBlankTop10Tex), so it needs MM's archive for now.
 *
 * Its rows:
 *   swords:  the Kokiri Sword (MM's Razor and Gilded: its upgrades, later), the Master Sword, the
 *            Giant's Knife, Biggoron's Sword, the Great Fairy's Sword (on B: later), the Double Helix
 *   shields: the Deku, MM's Hero's (later), the Hylian, MM's Mirror (later), OoT's Mirror
 * A on a sword puts it on B; on a shield, wears it. What's worn is outlined, as on OoT's page.
 *
 * The Giant's Knife and Biggoron's Sword are two items here (OoT keeps them in one slot, the Knife
 * overwritten when Biggoron's comes): each owned on its own (our "gear" save section,
 * NvGearSave.cpp); OoT's one slot holds whichever is in hand (bgsFlag: Biggoron's), so OoT's own
 * code (breaking, the B icon, Medigoron) goes on working on that. Medigoron sells a new Knife
 * whenever you've no whole one, Biggoron's or not (the user: the old stop goes).
 */
#include "global.h"
#include "z_nv_pause.h"
#include "textures/icon_item_static/icon_item_static.h"
#include "textures/item_name_static/item_name_static.h"
#include "textures/parameter_static/parameter_static.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEX(file, name) "__OTR__mm/textures/" file "/" name

Gfx* KaleidoScope_QuadTextureIA8(Gfx* gfx, void* texture, s16 width, s16 height, u16 point);
s32 NvMmItem_Has(s32 item);
s32 NvMmForm_Get(void); // 0 the Fierce Deity, 1 Goron, 2 Zora, 3 Deku, 4 human
const char* NvMmPause_Icon(s32 mmItem);

/* ---- the Knife and Biggoron's, apart ---- */

typedef struct {
    s32 knife;       // 0 none, 1 whole, 2 broken
    s32 bgs;         // Biggoron's Sword owned
    f32 knifeHealth; // the Knife's (OoT's swordHealth while it's the one in the slot)
    s32 gfs;         // the Great Fairy's Sword is the one on B (in OoT's two-handed slot, as Biggoron's)
    s32 mmShield;    // MM's shield worn (1 the Hero's, in OoT's Hylian slot; 2 MM's Mirror, in OoT's Mirror slot)
} NvGearSave;

static NvGearSave sGear;

static void NvGear_GfsOff(void);

NvGearSave* NvGear_SaveVars(void) {
    return &sGear;
}

void NvGear_NewGame(void) {
    memset(&sGear, 0, sizeof(sGear));
}

#define OWNS_SLOT() CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BIGGORON)
#define SLOT_BROKEN() CHECK_OWNED_EQUIP_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE)

/** Each frame (z_play.c): what OoT's one slot holds now, into ours (it changes by OoT's own code: the
 * Knife breaking, Biggoron handing his over, an old save). */
void NvGear_Update(PlayState* play) {
    if ((sGear.mmShield != 0) &&
        (LINK_IS_ADULT || (CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) !=
                           ((sGear.mmShield == 1) ? EQUIP_VALUE_SHIELD_HYLIAN : EQUIP_VALUE_SHIELD_MIRROR)))) {
        sGear.mmShield = 0; // (another shield on, eaten by a Like Like, grown up)
    }
    if (sGear.gfs) {
        if ((CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_BIGGORON) &&
            (gSaveContext.equips.buttonItems[0] == ITEM_SWORD_BGS)) {
            return; // (OoT's slot is standing in for it)
        }
        NvGear_GfsOff(); // (another sword taken by OoT's own page, or the age changed)
    }
    if (!OWNS_SLOT() && !SLOT_BROKEN()) {
        return;
    }
    if (gSaveContext.bgsFlag) {
        sGear.bgs = true;
    } else {
        sGear.knife = SLOT_BROKEN() ? 2 : 1;
        sGear.knifeHealth = gSaveContext.swordHealth;
    }
}

/** z_player.c's get-item hook: a Knife bought (Medigoron) is the Knife's, whatever's in the slot. */
void NvGear_GotItem(s32 getItemId) {
    if (getItemId == GI_SWORD_KNIFE) {
        sGear.knife = 1;
        sGear.knifeHealth = 8.0f;
        if (gSaveContext.bgsFlag) { // (Biggoron's in hand: it stays there, OoT's slot as it was)
            gSaveContext.inventory.equipment &= ~OWNED_EQUIP_FLAG_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE);
        }
    } else if (getItemId == GI_SWORD_BGS) {
        sGear.bgs = true;
    }
}

/** Medigoron's hook (z_en_gm.c): his line, OoT's numbering (1 sells, 2 sells to replace a broken one,
 * 3 has nothing for you); he sells whenever there's no whole Knife. */
s32 NvGear_MedigoronCase(void) {
    return (sGear.knife == 0) ? 1 : (sGear.knife == 2) ? 2 : 3;
}

/** OoT's slot as it rests with nothing of ours in it: Biggoron's if owned, else the Knife, else empty
 * (the Great Fairy's Sword put away). */
static void NvGear_RestSlot(void) {
    gSaveContext.inventory.equipment &= ~OWNED_EQUIP_FLAG_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE);
    if (sGear.bgs || sGear.knife) {
        gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BIGGORON);
    } else {
        gSaveContext.inventory.equipment &= ~OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BIGGORON);
    }
    gSaveContext.bgsFlag = sGear.bgs;
    if (!sGear.bgs && (sGear.knife == 2)) {
        gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE);
    }
    gSaveContext.swordHealth = sGear.bgs ? 8.0f : (sGear.knife == 2) ? 0.0f : sGear.knifeHealth;
}

/** The Great Fairy's Sword is the one on B (z_nv_mm_form.c draws it, z_parameter.c shows its icon). */
s32 NvGear_GfsOn(void) {
    return sGear.gfs;
}

/** MM's shield young Link has on (1 the Hero's, 2 MM's Mirror; 0 none, or not him just now): OoT's
 * Hylian or Mirror slot stands in, z_nv_mm_form.c draws MM's (and holds the Hero's as MM does). */
s32 NvGear_MmShield(void) {
    return ((NvMmForm_Get() == 4) && !LINK_IS_ADULT) ? sGear.mmShield : 0;
}

/** Off it, OoT's slot back as it rests. */
static void NvGear_GfsOff(void) {
    if (sGear.gfs) {
        sGear.gfs = false;
        NvGear_RestSlot();
    }
}

/** z_nv_mm_form.c: a change of form. The Great Fairy's Sword is human Link's (MM's): the Fierce Deity
 * takes his Double Helix in its place. */
void NvGear_FormChanged(PlayState* play, s32 form) {
    if (sGear.gfs && (form == 0)) {
        NvGear_GfsOff();
        Inventory_ChangeEquipment(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_KOKIRI);
        gSaveContext.equips.buttonItems[0] = ITEM_SWORD_KOKIRI;
    }
}

/** The B button's icon (z_parameter.c's hook): the Great Fairy's Sword's, the Double Helix's (MM's),
 * where OoT's slot stands in for them; else OoT's. */
void* NvGear_BIcon(void* oot) {
    if (sGear.gfs && (gSaveContext.equips.buttonItems[0] == ITEM_SWORD_BGS)) {
        return (void*)NvMmPause_Icon(0x10);
    }
    if ((NvMmForm_Get() == 0) && (gSaveContext.equips.buttonItems[0] == ITEM_SWORD_KOKIRI)) {
        return (void*)NvMmPause_Icon(0x50);
    }
    return oot;
}

/** Puts the Knife (bgs false) or Biggoron's in OoT's slot and on B. */
static void NvGear_HoldBig(s32 bgs) {
    sGear.gfs = false;
    gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BIGGORON);
    gSaveContext.inventory.equipment &= ~OWNED_EQUIP_FLAG_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE);
    if (bgs) {
        gSaveContext.bgsFlag = true;
        gSaveContext.swordHealth = 8.0f;
        gSaveContext.equips.buttonItems[0] = ITEM_SWORD_BGS;
    } else {
        gSaveContext.bgsFlag = false;
        if (sGear.knife == 2) {
            gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE);
            gSaveContext.swordHealth = 0.0f;
            gSaveContext.equips.buttonItems[0] = ITEM_SWORD_KNIFE;
        } else {
            gSaveContext.swordHealth = sGear.knifeHealth;
            gSaveContext.equips.buttonItems[0] = ITEM_SWORD_BGS;
        }
    }
    Inventory_ChangeEquipment(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_BIGGORON);
}

/* ---- the page ---- */

typedef enum {
    GEAR_NONE,
    GEAR_KOKIRI,
    GEAR_MASTER,
    GEAR_KNIFE,
    GEAR_BGS,
    GEAR_GREAT_FAIRY,
    GEAR_HELIX,
    GEAR_SHIELD_DEKU,
    GEAR_SHIELD_HERO,
    GEAR_SHIELD_HYLIAN,
    GEAR_SHIELD_MIRROR_MM,
    GEAR_SHIELD_MIRROR,
} NvGearKind;

static const u8 sSlots[24] = {
    GEAR_KOKIRI, GEAR_MASTER, GEAR_KNIFE, GEAR_BGS, GEAR_GREAT_FAIRY, GEAR_HELIX,
    GEAR_SHIELD_DEKU, GEAR_SHIELD_HERO, GEAR_SHIELD_HYLIAN, GEAR_SHIELD_MIRROR_MM, GEAR_SHIELD_MIRROR, GEAR_NONE,
    GEAR_NONE, GEAR_NONE, GEAR_NONE, GEAR_NONE, GEAR_NONE, GEAR_NONE,
    GEAR_NONE, GEAR_NONE, GEAR_NONE, GEAR_NONE, GEAR_NONE, GEAR_NONE,
};

static s16 sCursor;

static s32 NvGear_Owned(s32 kind) {
    switch (kind) {
        case GEAR_KOKIRI: return CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_KOKIRI);
        case GEAR_MASTER: return CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_MASTER);
        case GEAR_KNIFE: return sGear.knife != 0;
        case GEAR_BGS: return sGear.bgs;
        case GEAR_GREAT_FAIRY: return NvMmItem_Has(0x10);
        case GEAR_HELIX: return NvMmItem_Has(0x35); // (the Fierce Deity's Mask: his sword comes with him)
        case GEAR_SHIELD_DEKU: return CHECK_OWNED_EQUIP(EQUIP_TYPE_SHIELD, EQUIP_INV_SHIELD_DEKU);
        case GEAR_SHIELD_HERO: return NvMmItem_Has(0x51);
        case GEAR_SHIELD_HYLIAN: return CHECK_OWNED_EQUIP(EQUIP_TYPE_SHIELD, EQUIP_INV_SHIELD_HYLIAN);
        case GEAR_SHIELD_MIRROR_MM: return NvMmItem_Has(0x52);
        case GEAR_SHIELD_MIRROR: return CHECK_OWNED_EQUIP(EQUIP_TYPE_SHIELD, EQUIP_INV_SHIELD_MIRROR);
        default: return false;
    }
}

/** Whether Link as he is now can take it: OoT's ages; the Fierce Deity takes adult Link's swords and
 * his own; the other forms take nothing (they've no sword or shield). */
static s32 NvGear_CanUse(s32 kind) {
    s32 form = NvMmForm_Get();
    s32 fd = (form == 0);
    s32 adult = LINK_IS_ADULT;

    if ((form != 4) && !fd) {
        return false;
    }
    switch (kind) {
        case GEAR_KOKIRI: return !adult && !fd;
        case GEAR_MASTER:
        case GEAR_KNIFE:
        case GEAR_BGS: return adult || fd;
        case GEAR_HELIX: return fd;
        case GEAR_GREAT_FAIRY: return form == 4; // (human Link's, either age: the user, 2026-11-24)
        case GEAR_SHIELD_HERO:
        case GEAR_SHIELD_MIRROR_MM: return (form == 4) && !adult; // (MM's young Link's)
        case GEAR_SHIELD_DEKU: return !adult && !fd;
        case GEAR_SHIELD_HYLIAN: return true;
        case GEAR_SHIELD_MIRROR: return adult || fd; // (adult Link's shields: his too)
        default: return false;
    }
}

static s32 NvGear_Worn(s32 kind) {
    s32 sword = CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
    s32 fd = (NvMmForm_Get() == 0);

    if (gSaveContext.infTable[29] && (kind <= GEAR_HELIX)) {
        return false; // (swordless: B empty)
    }
    switch (kind) {
        case GEAR_KOKIRI: return (sword == EQUIP_VALUE_SWORD_KOKIRI) && !fd;
        case GEAR_HELIX: return (sword == EQUIP_VALUE_SWORD_KOKIRI) && fd;
        case GEAR_MASTER: return sword == EQUIP_VALUE_SWORD_MASTER;
        case GEAR_KNIFE: return (sword == EQUIP_VALUE_SWORD_BIGGORON) && !gSaveContext.bgsFlag && !sGear.gfs;
        case GEAR_BGS: return (sword == EQUIP_VALUE_SWORD_BIGGORON) && gSaveContext.bgsFlag && !sGear.gfs;
        case GEAR_GREAT_FAIRY: return sGear.gfs;
        case GEAR_SHIELD_DEKU: return CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) == EQUIP_VALUE_SHIELD_DEKU;
        case GEAR_SHIELD_HYLIAN: return (CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) == EQUIP_VALUE_SHIELD_HYLIAN) && (sGear.mmShield != 1);
        case GEAR_SHIELD_MIRROR: return (CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) == EQUIP_VALUE_SHIELD_MIRROR) && (sGear.mmShield != 2);
        case GEAR_SHIELD_HERO: return (CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) == EQUIP_VALUE_SHIELD_HYLIAN) && (sGear.mmShield == 1);
        case GEAR_SHIELD_MIRROR_MM: return (CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) == EQUIP_VALUE_SHIELD_MIRROR) && (sGear.mmShield == 2);
        default: return false;
    }
}

static const char* NvGear_Icon(s32 kind) {
    switch (kind) {
        case GEAR_KOKIRI: return gItemIconSwordKokiriTex;
        case GEAR_MASTER: return gItemIconSwordMasterTex;
        case GEAR_KNIFE: return (sGear.knife == 2) ? gItemIconBrokenGiantsKnifeTex : gItemIconSwordBiggoronTex; // (OoT's Knife has Biggoron's icon)
        case GEAR_BGS: return gItemIconSwordBiggoronTex;
        case GEAR_GREAT_FAIRY: return NvMmPause_Icon(0x10);
        case GEAR_HELIX: return NvMmPause_Icon(0x50);
        case GEAR_SHIELD_DEKU: return gItemIconShieldDekuTex;
        case GEAR_SHIELD_HERO: return NvMmPause_Icon(0x51);
        case GEAR_SHIELD_HYLIAN: return gItemIconShieldHylianTex;
        case GEAR_SHIELD_MIRROR_MM: return NvMmPause_Icon(0x52);
        case GEAR_SHIELD_MIRROR: return gItemIconShieldMirrorTex;
        default: return NULL;
    }
}

const char* NvMmPause_NameTex(s32 mmItem); // z_nv_mm_pause.c

static const char* NvGear_NameOf(s32 kind) {
    switch (kind) {
        case GEAR_KOKIRI: return gKokiriSwordItemNameENGTex;
        case GEAR_MASTER: return gMasterSwordItemNameENGTex;
        case GEAR_KNIFE: return (sGear.knife == 2) ? gBrokenGiantsKnifeItemNameENGTex : gGiantsKnifeItemNameENGTex;
        case GEAR_BGS: return gBiggoronsSwordItemNameENGTex;
        case GEAR_GREAT_FAIRY: return NvMmPause_NameTex(0x10);
        case GEAR_HELIX: return NvMmPause_NameTex(0x50);
        case GEAR_SHIELD_DEKU: return gDekuShieldItemNameENGTex;
        case GEAR_SHIELD_HERO: return NvMmPause_NameTex(0x51);
        case GEAR_SHIELD_HYLIAN: return gHylianShieldItemNameENGTex;
        case GEAR_SHIELD_MIRROR_MM: return NvMmPause_NameTex(0x52);
        case GEAR_SHIELD_MIRROR: return gMirrorShieldItemNameENGTex;
        default: return NULL;
    }
}

static void NvGear_Sfx(u16 sfx) {
    Audio_PlaySfxGeneral(sfx, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                         &gSfxDefaultReverb);
}

/** A on a slot: the sword onto B, the shield on. */
static void NvGear_Take(PlayState* play, s32 kind) {
    if (!NvGear_Owned(kind) || !NvGear_CanUse(kind)) {
        NvGear_Sfx(NA_SE_SY_ERROR);
        return;
    }
    if ((kind <= GEAR_HELIX) && (kind != GEAR_GREAT_FAIRY)) {
        NvGear_GfsOff();
    }
    switch (kind) {
        case GEAR_GREAT_FAIRY: // (OoT's two-handed slot stands in: Biggoron's swings, never breaking)
            sGear.gfs = true;
            gSaveContext.bgsFlag = true;
            gSaveContext.swordHealth = 8.0f;
            gSaveContext.inventory.equipment &= ~OWNED_EQUIP_FLAG_ALT(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BROKENGIANTKNIFE);
            Inventory_ChangeEquipment(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_BIGGORON);
            gSaveContext.equips.buttonItems[0] = ITEM_SWORD_BGS;
            break;
        case GEAR_KOKIRI:
        case GEAR_HELIX: // (the Double Helix is the Fierce Deity's Kokiri Sword slot, z_nv_mm_form.c)
            Inventory_ChangeEquipment(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_KOKIRI);
            gSaveContext.equips.buttonItems[0] = ITEM_SWORD_KOKIRI;
            break;
        case GEAR_MASTER:
            Inventory_ChangeEquipment(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_MASTER);
            gSaveContext.equips.buttonItems[0] = ITEM_SWORD_MASTER;
            break;
        case GEAR_KNIFE:
        case GEAR_BGS:
            NvGear_HoldBig(kind == GEAR_BGS);
            break;
        case GEAR_SHIELD_DEKU:
            sGear.mmShield = 0;
            Inventory_ChangeEquipment(EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_DEKU);
            break;
        case GEAR_SHIELD_HYLIAN:
            sGear.mmShield = 0;
            Inventory_ChangeEquipment(EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_HYLIAN);
            break;
        case GEAR_SHIELD_MIRROR:
            sGear.mmShield = 0;
            Inventory_ChangeEquipment(EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_MIRROR);
            break;
        case GEAR_SHIELD_HERO: // (OoT's Hylian slot stands in: metal, held in hand as MM holds it)
            sGear.mmShield = 1;
            Inventory_ChangeEquipment(EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_HYLIAN);
            break;
        case GEAR_SHIELD_MIRROR_MM: // (OoT's Mirror slot stands in: it reflects light as OoT's)
            sGear.mmShield = 2;
            Inventory_ChangeEquipment(EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_MIRROR);
            break;
    }
    if (kind <= GEAR_HELIX) {
        gSaveContext.infTable[29] = 0; // (a sword on B again)
        Interface_LoadItemIcon1(play, 0);
    }
    NvGear_Sfx(NA_SE_SY_DECIDE);
    if (getenv("NV_LOG") != NULL) {
        printf("NVGEAR took %d: equips %04X owned %08X B %d bgsFlag %d health %d; knife %d bgs %d gfs %d mmShield %d\n", (int)kind,
               (unsigned)gSaveContext.equips.equipment, (unsigned)gSaveContext.inventory.equipment,
               (int)gSaveContext.equips.buttonItems[0], (int)gSaveContext.bgsFlag, (int)gSaveContext.swordHealth,
               (int)sGear.knife, (int)sGear.bgs, (int)sGear.gfs, (int)sGear.mmShield);
        fflush(stdout);
    }
}

/** The cursor, as MM's pages (z_nv_mm_pause.c): one step a push, past the left or right column the
 * box turns, past the top or bottom row the column slides. */
static void NvGear_Input(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    Input* input = &play->state.input[0];
    s16 x = sCursor % 6;
    s16 y = sCursor / 6;
    s16 old = sCursor;
    s32 dx, dy;

    NvPause_Steps(play, &dx, &dy);
    if (pauseCtx->cursorSpecialPos != 0) {
        if ((pauseCtx->cursorSpecialPos == PAUSE_CURSOR_PAGE_LEFT) && (pauseCtx->stickRelX > 30)) {
            pauseCtx->cursorSpecialPos = 0;
            sCursor = y * 6;
        } else if ((pauseCtx->cursorSpecialPos == PAUSE_CURSOR_PAGE_RIGHT) && (pauseCtx->stickRelX < -30)) {
            pauseCtx->cursorSpecialPos = 0;
            sCursor = y * 6 + 5;
        }
        return;
    }
    if (dx < 0) {
        if (x == 0) {
            KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_LEFT);
        } else {
            sCursor--;
        }
    } else if (dx > 0) {
        if (x == 5) {
            KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_RIGHT);
        } else {
            sCursor++;
        }
    } else if (dy < 0) {
        if (y == 0) {
            NvPause_Edge(play, PAUSE_EQUIP, -1);
            return;
        }
        sCursor -= 6;
    } else if (dy > 0) {
        if (y == 3) {
            NvPause_Edge(play, PAUSE_EQUIP, 1);
            return;
        }
        sCursor += 6;
    } else if (CHECK_BTN_ALL(input->press.button, BTN_A) && (sSlots[sCursor] != GEAR_NONE) &&
               NvGear_Owned(sSlots[sCursor])) {
        NvGear_Take(play, sSlots[sCursor]);
    }
    if (sCursor != old) {
        NvGear_Sfx(NA_SE_SY_CURSOR);
    }
}

static void NvGear_Draw(PlayState* play, s32 front) {
    PauseContext* pauseCtx = &play->pauseCtx;
    s32 i;

    if (front) {
        s32 kind = sSlots[sCursor];
        s32 shown = (kind != GEAR_NONE) && NvGear_Owned(kind);
        static s32 sDevTook;

        // dev: NV_GEAR_TAKE="3,4,6" takes those (NvGearKind) in turn, 20 frames apart, once the page is in front
        if ((getenv("NV_GEAR_TAKE") != NULL) && ((play->state.frames % 20) == 0)) {
            const char* l = getenv("NV_GEAR_TAKE");
            s32 n = 0;

            while (*l != '\0') {
                s32 k = (s32)strtol(l, (char**)&l, 10);

                if (n++ == sDevTook) {
                    sDevTook++;
                    NvGear_Take(play, k);
                    break;
                }
                while (*l == ',') {
                    l++;
                }
            }
        }
        NvGear_Input(play);
        kind = sSlots[sCursor];
        shown = (kind != GEAR_NONE) && NvGear_Owned(kind);
        pauseCtx->cursorSlot[PAUSE_EQUIP] = sCursor;
        // (cursorItem: what the name bar keys on; ours kept apart from OoT's and MM's ids)
        pauseCtx->cursorItem[PAUSE_EQUIP] = shown ? (0x300 + kind + ((kind == GEAR_KNIFE) ? sGear.knife * 16 : 0))
                                                  : PAUSE_ITEM_NONE;
        pauseCtx->cursorColorSet = shown ? 8 : 0;
    }
    OPEN_DISPS(play->state.gfxCtx);
    // what's worn: OoT's outline, under the icon
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, ZREG(39), ZREG(40), ZREG(41), pauseCtx->alpha);
    gDPSetEnvColor(POLY_OPA_DISP++, ZREG(43), ZREG(44), ZREG(45), 0);
    for (i = 0; i < 24; i++) {
        if ((sSlots[i] != GEAR_NONE) && NvGear_Owned(sSlots[i]) && NvGear_Worn(sSlots[i])) {
            gDPPipeSync(POLY_OPA_DISP++);
            gSPVertex(POLY_OPA_DISP++, &pauseCtx->itemVtx[i * 4], 4, 0);
            POLY_OPA_DISP = KaleidoScope_QuadTextureIA8(POLY_OPA_DISP, gEquippedItemOutlineTex, 32, 32, 0);
        }
    }
    Gfx_SetupDL_42Opa(play->state.gfxCtx);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    for (i = 0; i < 24; i++) {
        s32 kind = sSlots[i];
        const char* icon;

        if ((kind == GEAR_NONE) || !NvGear_Owned(kind) || ((icon = NvGear_Icon(kind)) == NULL)) {
            continue;
        }
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
        if (!NvGear_CanUse(kind)) { // (greyed, as OoT's page greys the wrong age's)
            gDPSetGrayscaleColor(POLY_OPA_DISP++, 109, 109, 109, 255);
            gSPGrayscale(POLY_OPA_DISP++, true);
        }
        gSPVertex(POLY_OPA_DISP++, &pauseCtx->itemVtx[i * 4], 4, 0);
        KaleidoScope_DrawQuadTextureRGBA32(play->state.gfxCtx, (void*)icon, 32, 32, 0);
        gSPGrayscale(POLY_OPA_DISP++, false);
    }
    CLOSE_DISPS(play->state.gfxCtx);
    if (front && (pauseCtx->cursorSpecialPos == 0)) {
        KaleidoScope_SetCursorVtx(pauseCtx, sCursor * 4, pauseCtx->itemVtx);
        KaleidoScope_DrawCursor(play, PAUSE_EQUIP);
    }
}

static const char* NvGear_Name(PlayState* play) {
    s32 kind = sSlots[sCursor];

    return ((kind != GEAR_NONE) && NvGear_Owned(kind)) ? NvGear_NameOf(kind) : NULL;
}

static void NvGear_Enter(PlayState* play, s32 dir) {
    if (dir > 0) {
        sCursor = sCursor % 6;
    } else if (dir < 0) {
        sCursor = 18 + sCursor % 6;
    }
}

static s32 NvGear_Available(PlayState* play) {
    return ResourceMgr_FileExists("mm/textures/nv_pause/gNvPauseBlankTop10Tex");
}

// MM's mask page's frame, the title left off; MM's flat colour
static const char* const sTiles[] = {
    TEX("icon_item_static", "gPauseMasks00Tex"),  TEX("icon_item_static", "gPauseMasks01Tex"),
    TEX("icon_item_static", "gPauseMasks02Tex"),  TEX("icon_item_static", "gPauseMasks03Tex"),
    TEX("icon_item_static", "gPauseMasks04Tex"),  TEX("nv_pause", "gNvPauseBlankTop10Tex"),
    TEX("icon_item_static", "gPauseMasks11Tex"),  TEX("icon_item_static", "gPauseMasks12Tex"),
    TEX("icon_item_static", "gPauseMasks13Tex"),  TEX("icon_item_static", "gPauseMasks14Tex"),
    TEX("icon_item_static", "gPauseMasks20Tex"),  TEX("icon_item_static", "gPauseMasks21Tex"),
    TEX("icon_item_static", "gPauseMasks22Tex"),  TEX("icon_item_static", "gPauseMasks23Tex"),
    TEX("icon_item_static", "gPauseMasks24Tex"),
};

static const NvPausePage sGearPage = {
    "the gear page", PAUSE_EQUIP, NvGear_Available, NULL, sTiles, { 180, 180, 120 },
    NvGear_Draw, NvGear_Name, NvGear_Enter,
};

void NvPauseGear_Register(void) {
    NvPause_Register(&sGearPage);
}
