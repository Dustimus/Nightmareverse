/*
 * Nightmareverse: RE1's doors.
 *
 * RE1 (Bio.exe 0x456880) opens a door when the action button is pressed and the point 600
 * units in front of the player (13 OoT units) lies in the door's rectangle; a door flagged
 * 0x40 tests the player's own position instead. Nothing happens by walking into a door.
 * Here A does the same, and the door takes Link to the target room's start position for it
 * (re1/tools/soh_scene.py writes the doors, with RE1's arrival points as start positions).
 *
 * Data: "scenes/shared/<name>_scene/<name>_doors" (soh_scene.door_blob): "RDOR", u32 count;
 * per door f32 x0, z0, x1, z1, u8 flags, u8 anim, u16 spawn, u8 Door_set bytes 10, 11, 13, 15
 * (the door scene's inputs), char target[24], u8 lock, u8 key, 2 pad ("RDOR": without the last 4).
 *
 * The locks (PlayerAnimations.cpp door_try_enter): a door whose lock byte has 0x80 is locked until
 * its bit (the low 6) is up in the locks bank (2: the scripts and the lab's computer raise them
 * too). Its key byte says what opens it: an item (the key turns: "You have used the ..."; the
 * door opens on the next press; a key with no more doors to open asks to be thrown away), 0xFE
 * only from its other side (from there: "You unlocked it."), 0xFF nothing ("locked from inside");
 * the sword key's doors take Jill's lock pick once she has it (scenario flag 0x7C). Without the
 * key, RE1's "It's locked" line for it. For testing they can all be left open: the Cheats menu's
 * "RE1: Unlock All Doors", or NV_NO_LOCKS.
 *
 * A door with a door scene (z_nv_doorscene.c) cuts straight to it, as RE1 does; any other
 * fades to black.
 *
 * Called from Play_Update (z_play.c) before the actors update, so the A press it takes is not
 * also Link's (no roll into the door).
 */
#include "global.h"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>
#include <stdio.h>
#include <string.h>

typedef struct NvDoor {
    f32 x0;
    f32 z0;
    f32 x1;
    f32 z1;
    u8 flags;
    u8 anim;
    u16 spawn;
    u8 b10; // Door_set bytes 10, 11, 13 and 15: what RE1's door scene reads (z_nv_doorscene.c)
    u8 b11;
    u8 b13;
    u8 b15;
    char target[24];
    u8 lock; // the record's +0xC: 0x80 locked, the low 6 bits its flag in the locks bank
    u8 key;  // +0x16: the item that opens it (0xFE from the other side, 0xFF never)
    u8 pad[2];
} NvDoor;

static u32 sNvDoorStride = sizeof(NvDoor); // the room's records' size ("RDOR": no lock bytes)

static s32 NvDoor_Unlocked(PlayState* play, NvDoor* door);
static void NvDoor_DiscardUpdate(PlayState* play);

s32 NvDoorScene_Exists(s32 anim);
void NvDoorScene_Queue(s32 anim, s32 b10, s32 b11, s32 b13, s32 b15);
s32 NvClimb_Active(void);
s32 NvClimb_TryStart(PlayState* play, Player* player, f32 nearD, f32 farD, s32 start);

#define NV_DOOR_ACTION 0x80   // opens with the action button
#define NV_DOOR_AT_PLAYER 0x40 // tested at the player's position, not in front of him
// RE1's staircases between rooms are doors with a stairs scene (kai01..kai03: the foyer to 203,
// 101 to 201, 10B to 207). Walking into one takes it, no A needed (the user's choice: RE1 wants
// the action button); every other door opens on A only
#define NV_DOOR_IS_STAIRS(anim) (((anim) >= 22) && ((anim) <= 24))
#define NV_DOOR_STAIRS_STICK 30 // stick push that counts as walking into the stairs (pressed against
                                // their solid box Link stands still, so it's the stick, not his speed)
// How far in front of Link the door point is. RE1's 600 units in front of Jill reach about 150
// past her body (13 OoT units against a radius taken as ~450 units, ~10 OoT: RE2's player
// radius, Jill's own still to be read from Bio.exe). Measured from Link's body instead (his wall
// radius: 18 adult, 14 child), as RE1's door rectangles start at the wall's surface and Link
// stops his radius short of it.
#define NV_DOOR_REACH_PAST_BODY 4.0f
// RE1's own point: 600 units in front of the player. A door whose rectangle lies on open floor
// (a doorway, a corner) can be smaller than Link's reach; standing in it, RE1's point is inside
// while the far one is past it. So the door opens if any point from RE1's to Link's reach is in.
#define NV_DOOR_REACH_RE1 13.0f

// The entrance for a scene file's start position (soh_tables.py: four per start position, the
// first is setup 0), or -1
s16 NvDoor_Entrance(const char* sceneFile, s32 spawn) {
    s32 scene;
    s32 i;

    for (scene = 0; scene < SCENE_ID_MAX; scene++) {
        if ((gSceneTable[scene].sceneFile.fileName != NULL) &&
            (strcmp(gSceneTable[scene].sceneFile.fileName, sceneFile) == 0)) {
            break;
        }
    }
    if (scene == SCENE_ID_MAX) {
        return -1;
    }
    for (i = 0; i < ENTR_MAX; i++) {
        if ((gEntranceTable[i].scene == scene) && (gEntranceTable[i].spawn == spawn)) {
            return i;
        }
    }
    return -1;
}

static u8* NvDoor_Data(PlayState* play, u32* count) {
    char path[256];
    const char* file;
    size_t len;
    u8* data;

    if ((play->loadedScene == NULL) || (play->loadedScene->sceneFile.fileName == NULL)) {
        return NULL;
    }
    file = play->loadedScene->sceneFile.fileName;
    len = strlen(file);
    if ((len <= 6) || (strcmp(file + len - 6, "_scene") != 0)) {
        return NULL;
    }
    snprintf(path, sizeof(path), "scenes/shared/%s/%.*s_doors", file, (int)(len - 6), file);
    if (!ResourceMgr_FileExists(path)) {
        return NULL;
    }
    data = (u8*)ResourceGetDataByName(path);
    if ((data == NULL) || ((memcmp(data, "RDR2", 4) != 0) && (memcmp(data, "RDOR", 4) != 0))) {
        return NULL;
    }
    sNvDoorStride = (memcmp(data, "RDR2", 4) == 0) ? sizeof(NvDoor) : sizeof(NvDoor) - 4;
    memcpy(count, data + 4, sizeof(*count));
    return data + 8;
}

// Whether Link may use a door or stairs now (standing, in control, nothing else going on; not
// while an enemy holds him: RE1 checks doors only in the player's normal state)
static s32 NvDoor_CanAct(PlayState* play, Player* player) {
    return (player != NULL) && (play->pauseCtx.state == 0) && (play->transitionTrigger == TRANS_TRIGGER_OFF) &&
           (play->transitionMode == TRANS_MODE_OFF) && !Player_InBlockingCsMode(play, player) &&
           (play->msgCtx.msgMode == MSGMODE_NONE) && (player->actor.bgCheckFlags & 1) &&
           (player->actor.parent == NULL) && !(player->stateFlags2 & PLAYER_STATE2_GRABBED_BY_ENEMY) &&
           !NvClimb_Active();
}

static f32 NvDoor_Reach(Player* player) {
    return player->ageProperties->wallCheckRadius + NV_DOOR_REACH_PAST_BODY;
}

// The door A would open now (RE1's test), or -1
static s32 NvDoor_Find(PlayState* play, Player* player, NvDoor* out) {
    u8* data;
    u32 count;
    u32 i;
    f32 sx = Math_SinS(player->actor.shape.rot.y);
    f32 sz = Math_CosS(player->actor.shape.rot.y);
    f32 reach = NvDoor_Reach(player);

    data = NvDoor_Data(play, &count);
    if (data == NULL) {
        return -1;
    }
    for (i = 0; i < count; i++) {
        NvDoor door;
        f32 px;
        f32 pz;

        memset(&door, 0, sizeof(door));
        memcpy(&door, data + i * sNvDoorStride, sNvDoorStride);
        if (!(door.flags & NV_DOOR_ACTION)) {
            continue;
        }
        if (door.flags & NV_DOOR_AT_PLAYER) {
            px = player->actor.world.pos.x;
            pz = player->actor.world.pos.z;
            if ((px < door.x0) || (px > door.x1) || (pz < door.z0) || (pz > door.z1)) {
                continue;
            }
        } else {
            f32 d;
            s32 in = false;

            for (d = NV_DOOR_REACH_RE1; d <= reach + 0.01f; d += 1.0f) {
                px = player->actor.world.pos.x + sx * d;
                pz = player->actor.world.pos.z + sz * d;
                if ((px >= door.x0) && (px <= door.x1) && (pz >= door.z0) && (pz <= door.z1)) {
                    in = true;
                    break;
                }
            }
            if (!in) {
                continue;
            }
        }
        *out = door;
        return i;
    }
    return -1;
}

/**
 * The A button's label for RE1's doors and stairs (z_player.c, Player_UpdateInterface):
 * DO_ACTION_OPEN or DO_ACTION_CLIMB when A would use one, else -1.
 */
s32 NvAot_DoAction(PlayState* play, Player* player);
s32 NvObj_TryClimb(PlayState* play, Player* player, s32 start);
s32 NvAot_Act(PlayState* play, Player* player);

s32 NvDoor_DoAction(PlayState* play, Player* player) {
    NvDoor door;

    if (!NvDoor_CanAct(play, player)) {
        return -1;
    }
    if (NvDoor_Find(play, player, &door) >= 0) {
        return NV_DOOR_IS_STAIRS(door.anim) ? DO_ACTION_CLIMB : DO_ACTION_OPEN;
    }
    if (NvClimb_TryStart(play, player, NV_DOOR_REACH_RE1, NvDoor_Reach(player), false) ||
        NvObj_TryClimb(play, player, false)) {
        return DO_ACTION_CLIMB;
    }
    return NvAot_DoAction(play, player); // what else RE1 would act on: an examine spot (z_nv_script.c)
}

static void NvDoor_Take(PlayState* play, Input* input, NvDoor* doorp, s32 i);

static char sNvFromScene[32]; // the room the last door was taken from

// RE1's ride rooms: the elevator car (700) has no doors to walk to, only 1x1 ones at (0, 0) that
// its script takes: from 713 the car goes to 71C, from 71C to 713 (ROOM700's init: the room it
// was entered from picks the ride's event, which ends in the other door). So in a room where
// every door is such a point, the car rides for a while and then takes the door that doesn't
// lead back where Link came from.
#define NV_RIDE_FRAMES 100
static s32 NvDoor_Ride(PlayState* play, Input* input) {
    static s32 sScene = -1;
    static s32 sTimer;
    u8* data;
    u32 count;
    u32 k;
    s32 pick = -1;
    NvDoor door;

    data = NvDoor_Data(play, &count);
    if ((data == NULL) || (count == 0)) {
        return false;
    }
    for (k = 0; k < count; k++) {
        NvDoor d;

        memset(&d, 0, sizeof(d));
        memcpy(&d, data + k * sNvDoorStride, sNvDoorStride);
        if (((d.x1 - d.x0) > 1.0f) || ((d.z1 - d.z0) > 1.0f)) {
            return false; // a room with a door to walk to
        }
        d.target[sizeof(d.target) - 1] = '\0';
        // the first door not back where Link came from (else the first)
        if ((pick < 0) || ((strcmp(door.target, sNvFromScene) == 0) && (strcmp(d.target, sNvFromScene) != 0))) {
            door = d;
            pick = k;
        }
    }
    if (sScene != play->sceneNum) {
        sScene = play->sceneNum;
        sTimer = 0;
    }
    if ((play->transitionTrigger != TRANS_TRIGGER_OFF) || (play->transitionMode != TRANS_MODE_OFF)) {
        return true;
    }
    if (++sTimer >= NV_RIDE_FRAMES) {
        sTimer = 0;
        sScene = -1;
        NvDoor_Take(play, input, &door, pick);
    }
    return true;
}

void NvDoor_Update(PlayState* play) {
    Player* player = GET_PLAYER(play);
    Input* input = &play->state.input[0];
    NvDoor door;
    s32 i;

    s32 pressA = CHECK_BTN_ALL(input->press.button, BTN_A);

    NvDoor_DiscardUpdate(play);
    if ((player != NULL) && NvDoor_Ride(play, input)) {
        return;
    }
    if (!NvDoor_CanAct(play, player)) {
        return;
    }
    i = NvDoor_Find(play, player, &door);
    if (!pressA) { // only stairs between rooms, by walking into them
        if ((i < 0) || !NV_DOOR_IS_STAIRS(door.anim) ||
            (SQ(input->rel.stick_x) + SQ(input->rel.stick_y) < SQ(NV_DOOR_STAIRS_STICK))) {
            return;
        }
    } else if (i < 0) {
        // no door: the room's stairs (z_nv_climb.c), with the same points
        // then, as RE1's player_input_to_behavior, the room's examine spots (z_nv_script.c)
        if (NvClimb_TryStart(play, player, NV_DOOR_REACH_RE1, NvDoor_Reach(player), true) ||
            NvObj_TryClimb(play, player, true) || NvAot_Act(play, player)) {
            input->press.button &= ~BTN_A;
            input->cur.button &= ~BTN_A;
        }
        return;
    }
    if (!NvDoor_Unlocked(play, &door)) {
        input->press.button &= ~BTN_A;
        input->cur.button &= ~BTN_A;
        return;
    }
    NvDoor_Take(play, input, &door, i);
}

s32 NvFlags_Test(s32 bank, s32 bit);
void NvFlags_Op(s32 bank, s32 bit, s32 mode);
void NvSnd_At(s32 slot, s32 x, s32 z, s32 lx, s32 lz);
void NvScript_SayItem(PlayState* play, s32 id, s32 item);
void NvScript_Ask(PlayState* play, s32 id);
s32 NvItems_Find(s32 id);
s32 NvItems_Slot(s32 i, s32* qty);
void NvItems_SpendRound(s32 i);
s32 NvItems_Remove(s32 id);

#define NV_ITEM_LOCK_PICK 0x31
#define NV_ITEM_SWORD_KEY 0x33
#define NV_SCN_HAS_LOCKPICK 0x7C

static s32 sNvDoorDiscard; // a key just used up: "This key is useless now. Discard?" once the line's read
static s32 sNvDoorAsking;

/** The locks are off for testing: the Cheats menu's "RE1: Unlock All Doors", or NV_NO_LOCKS. */
static s32 NvDoor_LocksOff(void) {
    return CVarGetInteger(CVAR_CHEAT("NvDoorsUnlocked"), 0) || (getenv("NV_NO_LOCKS") != NULL);
}

/**
 * door_try_enter: true if the door may be taken now; otherwise its lock has said why (or the key
 * has just turned: the next press goes through, as in RE1).
 */
static s32 NvDoor_Unlocked(PlayState* play, NvDoor* door) {
    s32 key = door->key;
    s32 item;
    s32 slot;
    s32 qty = 0;

    if (!(door->lock & 0x80) || NvFlags_Test(2, door->lock & 0x3F) || NvDoor_LocksOff()) {
        return true;
    }
    if (key == NV_ITEM_SWORD_KEY) { // Jill: the lock pick, once she has it
        if (!NvFlags_Test(0, NV_SCN_HAS_LOCKPICK)) {
            NvSnd_At(0x14, 0, 0, 0, 0);
            NvScript_SayItem(play, 0xC0 | 0x15, 0); // "It's locked. Use the lockpick to open the door."
            return false;
        }
        item = NV_ITEM_LOCK_PICK;
    } else if (key == 0xFE) { // opens from this side only
        NvScript_SayItem(play, 0xC0 | 0x14, 0); // "You unlocked it."
        NvSnd_At(0x21, 0, 0, 0, 0);
        NvFlags_Op(2, door->lock & 0x3F, 0);
        printf("NVLOCK bit %d unlocked from this side\n", door->lock & 0x3F);
        fflush(stdout);
        return false;
    } else if (key == 0xFF) {
        NvSnd_At(0x14, 0, 0, 0, 0);
        NvScript_SayItem(play, 0xC0 | 0x13, 0); // "It's locked from inside."
        return false;
    } else {
        slot = NvItems_Find(key);
        if (slot < 0) { // RE1's line for the key it wants (200 + key - the sword key, the last for any other)
            s32 m = key - NV_ITEM_SWORD_KEY;

            if ((m < 0) || (m > 9)) {
                m = 10;
            }
            NvSnd_At(0x14, 0, 0, 0, 0);
            NvScript_SayItem(play, 0xC0 | (8 + m), 0);
            printf("NVLOCK bit %d locked: wants item 0x%02X\n", door->lock & 0x3F, key);
            fflush(stdout);
            return false;
        }
        item = key;
        NvItems_Slot(slot, &qty);
        if (qty <= 1) {
            sNvDoorDiscard = item; // (its last door: asked whether to throw it away)
        } else {
            NvItems_SpendRound(slot);
        }
    }
    // the key turns: the door opens on the next press
    NvScript_SayItem(play, 0xC0 | 0x03, item); // "You have used the ..."
    NvSnd_At(0x22, 0, 0, 0, 0);
    NvFlags_Op(2, door->lock & 0x3F, 0);
    printf("NVLOCK bit %d opened with item 0x%02X\n", door->lock & 0x3F, item);
    fflush(stdout);
    return false;
}

/** The used-up key's question, once the "used" line has been read (handle_message_post_action). */
static void NvDoor_DiscardUpdate(PlayState* play) {
    if ((sNvDoorDiscard == 0) || (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE && sNvDoorAsking == 2)) {
        sNvDoorAsking = 0;
        sNvDoorDiscard = 0;
        return;
    }
    if (sNvDoorAsking == 0) {
        if (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE) {
            NvScript_Ask(play, 0xC0 | 0x07); // "This key is useless now. Discard?"
            sNvDoorAsking = 1;
        }
        return;
    }
    if ((sNvDoorAsking == 1) && (Message_GetState(&play->msgCtx) == TEXT_STATE_CHOICE) && Message_ShouldAdvance(play)) {
        if (play->msgCtx.choiceIndex == 0) {
            NvItems_Remove(sNvDoorDiscard);
            printf("NVLOCK key 0x%02X thrown away\n", sNvDoorDiscard);
            fflush(stdout);
        }
        Message_CloseTextbox(play);
        sNvDoorAsking = 2;
    }
}

/**
 * Take a door: to its room's start position, through its door scene or a fade. A room this build
 * doesn't have yet says so instead.
 */
static void NvDoor_Take(PlayState* play, Input* input, NvDoor* doorp, s32 i) {
    NvDoor door = *doorp;
    s16 entrance;

    sNvFromScene[0] = '\0';
    if ((play->loadedScene != NULL) && (play->loadedScene->sceneFile.fileName != NULL)) {
        snprintf(sNvFromScene, sizeof(sNvFromScene), "%s", play->loadedScene->sceneFile.fileName);
    }
    door.target[sizeof(door.target) - 1] = '\0';
    entrance = NvDoor_Entrance(door.target, door.spawn);
    printf("NVDOOR %d -> %s spawn %d entrance 0x%X\n", (int)i, door.target, (int)door.spawn, (int)entrance);
    fflush(stdout);
    if (entrance < 0) {
        // a room this build doesn't have yet: say so, so it doesn't look like a broken door
        void NvMsg_Show(PlayState * play, const char* text);
        char line[96];

        input->press.button &= ~BTN_A;
        input->cur.button &= ~BTN_A;
        snprintf(line, sizeof(line), "This door leads to room %.3s, which isn't in the build yet.",
                 (strncmp(door.target, "re1_", 4) == 0) ? door.target + 4 : door.target);
        NvMsg_Show(play, line);
        return;
    }
    // the press is the door's, not Link's
    input->press.button &= ~BTN_A;
    input->cur.button &= ~BTN_A;
    play->nextEntranceIndex = entrance;
    play->transitionTrigger = TRANS_TRIGGER_START;
    if (NvDoorScene_Exists(door.anim)) {
        NvDoorScene_Queue(door.anim, door.b10, door.b11, door.b13, door.b15);
        play->transitionType = TRANS_TYPE_INSTANT;
        gSaveContext.nextTransitionType = TRANS_TYPE_INSTANT;
    } else {
        play->transitionType = TRANS_TYPE_FADE_BLACK;
        gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
    }
    return;
}
