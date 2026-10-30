/*
 * Nightmareverse: RE1's items as RE1 keeps them, for the mansion's pickups.
 *
 * One pool (re1/README.md, "Items"): the player's slots (Jill 8, Chris 6), RE1's linked item box,
 * and the room items' taken flags (g_roomItemsFlags: one bit per item record across the game).
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
static u32 sNvTaken[8]; // the room items taken (RE1 keeps the opposite: a bit set while it lies there)
static u8 sNvMapsOwned; // bit n: map item 0x4E + n

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

/** RE1's room item flags: whether the item record n still lies in its room. */
s32 NvItems_InRoom(s32 n) {
    return !(sNvTaken[(n >> 5) & 7] & (1u << (n & 31)));
}

void NvItems_SetTaken(s32 n) {
    sNvTaken[(n >> 5) & 7] |= 1u << (n & 31);
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
