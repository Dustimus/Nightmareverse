/*
 * Nightmareverse: RE1's room scripts, run as RE1 runs them.
 *
 * Each room keeps its three scripts as they are in the RDT ("scenes/shared/<name>_scene/<name>_scd",
 * re1/tools/soh_scene.py): the init chain (run once as the room loads), the main chain (game_loop
 * runs it every frame) and the event scripts (room_events_check 0x0041d6a0: eight slots, each a
 * coroutine that runs until it yields). The PC decomp's docs/SCD_SCRIPT_SYSTEM.md and
 * src/game/CmdFunctions.cpp / RoomEvents.cpp are the reference: run_command_functions' branch stack
 * (cmd_if pushes where its block ends; a failed test returns 0 and resumes there), the event
 * control bytes 0xF6-0xFF in any state, state 0's commands.
 *
 * What runs so far: if/else/end if, the flag banks (test, set/clear/toggle), the state byte test
 * (stage, room, camera), Enemy_set (z_nv_enemy.c), enemy_prop_set's behaviour, enemy_pos_set, events (create,
 * kill), movies (cmd_fmv_set: z_nv_movie.c). Every other command is stepped over at its RE1 length;
 * the other tests are false. Events' animation (state 1) and movement (state 2) opcodes are
 * stepped over too: nothing is animated or moved by them yet.
 *
 * Ticks are RE1's 30 Hz: three every two of OoT's frames.
 *
 * The flags are RE1's ten banks with RE1's bit numbering (bit n: dword n / 32, mask
 * 0x80000000 >> n % 32). They last for the session; saving them comes with the save system.
 */
#include "global.h"
#include "soh/OTRGlobals.h"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NV_EVT_SLOTS 8
#define NV_SCD_STACK 16
#define NV_SCD_GUARD 4096 // commands per run before giving up (a desynced stream)
#define NV_MSF_CHAR_VARIANT 0x08 // bank 5's bit: Jill's game (MSF_CHAR_VARIANT, raised at RE1's character select)
#define NV_MSF_VOICE_PLAYING 0x0E // a voice line playing (MSF_VOICE_PLAYING: the scripts wait on it)
#define NV_MSF_OBJECT_PUSH 0x19   // she's pushing an object (MSF_OBJECT_PUSH)
#define NV_MSF_ON_OBJECT 0x18     // she's climbed up onto an object (MSF_DOOR_TRANSITION: check_climb_object)
#define NV_MSF_CAMERA_REDRAW 0x1A // bank 5's bit: the camera switched this frame (MSF_CAMERA_REDRAW)

void NvEnemy_ClearSlots(void);
void NvEnemy_ScdSet(PlayState* play, const u8* rec);
void NvEnemy_ScdProp(s32 slot, s32 subCmd, u16 param, u8 hit);
void NvEnemy_ScdPos(s32 slot, s16 angle, s16 x, s16 y, s16 z);
#include "z_nv_re1.h"

#define NV_RE1_SCALE 0.0217f // OoT units per RE1 unit (re1/tools/rdt2scene.py SCALE)
s32 NvItems_InRoom(s32 n);
void NvItems_SetTaken(s32 n);
s32 NvItems_Give(s32 id, s32 qty);
const char* NvItems_Name(s32 id);
void NvItems_Log(void);
void NvItems_RoomLoad(PlayState* play);
void NvMovie_Start(s32 id);
void NvMsg_EnsureHook(void);
void NvBgm_ScdPlay(s32 ch);
void NvBgm_ScdStop(s32 ch);
void NvBgm_ScdStopAll(void);
void NvBgm_ScdFade(s32 n);
void NvBgm_ScdPanVol(s32 ch, s32 pan, s32 vol);
void NvBgm_ScdRamp(s32 ch, s32 dir, s32 n);
void NvBgm_Tick(void);
void NvBgm_ScdRestore(void);
void NvBgm_ScdState(s32 stage, s32 room, u8 state);
s32 NvMovie_Playing(void);
s32 NvDoorScene_Covering(void);
void NvCast_RoomLoad(void);
s32 NvClimb_Active(void);
s32 NvSound_Line(const char* name);
void NvSound_LineStop(void);
s32 NvSound_LinePlaying(void);
void NvCast_Update(PlayState* play, s32 eventsLive);

typedef struct {
    u8 active;
    u8 state; // 0 commands, 1 animation, 2 movement
    s8 depth; // -1: empty
    const u8* ip;
    const u8* ret[4];
    const u8* call[4];
    s16 counter[4];
    u8 entKind; // the character it drives (state 0's 0x04): 0 the player, 1 an enemy slot
    u8 entIdx;
} NvEvt;

static u32 sNvFlags[10][8];
static NvEvt sNvEvt[NV_EVT_SLOTS];
static NvEvt* sNvEvtCur; // the event running now (its character goes to the events it makes)
static const u8* sNvInit;
static const u8* sNvMain;
static const u8* sNvEvtTable;
static PlayState* sNvPlay;
s32 gNvDevUseItem = -1; // dev: an item to Use this tick, as the status screen (NV_WALK "use:XX")
static u8 sNvStage;
static u8 sNvRoom;
static u8 sNvCamera;
static s32 sNvLastCamera = -1;
static u8 sNvTickPhase;
static s32 sNvInitRun; // Enemy_set only places enemies while the init chain runs

// the room action table (g_RoomActionTable: 12-byte entries the scripts build with door_set,
// room_action_set and change with room_action_reset / _arm), as RE1 keeps it
#define NV_AOT_SLOTS 32
typedef struct {
    u8 handler; // room_check_actions index: 1 door, 2 message, ...
    u8 flags;   // 0x01 probed, 0x80 on the action press, 0x40 at the player (not in front)
    u16 param[3];
    u16 zone[4]; // x, z, width, depth (RE1 units, unsigned compares)
    u8 item;     // item_set's record: the item, its count, its room item flag
    u8 qty;
    u8 itemFlag;
    u8 isItem;   // set by item_set: a model to draw while the entry is live
    u8 model;    // the room's item model (RDT item_models)
    u8 parent;   // 0xFF: pos is in the room; 0xFE the player; else a room object (not drawn yet)
    s16 pos[3];
    u8 sparkle;  // its glint's effect slot (z_nv_effects.c), 0xFF none
} NvAot;
static NvAot sNvAot[NV_AOT_SLOTS];
static void NvAot_FlagSet(const NvAot* e);
static s32 sNvAotCount;

// the room's objects (g_omodel_table: furniture, statues, the item box's lid...), as the scripts'
// omodel_set places them (CmdFunctions.cpp cmd_omodel_set) and obj_flag_set shows or hides them
#define NV_OBJ_SLOTS 64
typedef struct {
    u8 set;    // placed by omodel_set
    u8 flags;  // its byte 2 (bit 0: drawn, render_room_objects)
    u8 parent; // 0xFF the room; 0xFE the player; 0x80+ an enemy; else another object
    s16 pos[3];
    s16 rot[3]; // RE1's SVECTOR (+0x72): x 0, y the record's, z 0 (the item box's lid turns it)
    s16 ext[3];   // its box's half extents (Sca_info +2/+4/+6: the record's +0x18, +0x16, +0x1A)
    s16 push;     // update_room_objects' push counter (+0x86): nine ticks leaned on, it moves
    Actor* block; // its collision box for Link (z_nv_block.c)
    u8 top;       // its box has a top: she's climbed onto it (a crate: RE1's climbable, 0x40)
    u8 pushTick;  // where in her push's step (behavior_10_push: 16 ticks of moving, then a pause)
    s16 speed[3]; // an event's movement (state 2: 0x05 sets it, 0x02 / 0x04 add it a tick)
    s16 turn[3];  // and its turn (0x06 sets it, 0x03 / 0x04 add it: x, y, z)
} NvObj;
static NvObj sNvObj[NV_OBJ_SLOTS];
static u8 sNvItemHidden[NV_AOT_SLOTS]; // obj_flag_set on an item model: bit 0 clear hides it
static const u8* sNvMsg; // the room's messages ("<name>_msg", re1/tools/soh_scene.py)

// run_command_functions' state
static const u8* sNvIp;
static const u8* sNvStack[NV_SCD_STACK];
static s32 sNvSp;
static s32 sNvContinue;

/* ---- flags ---- */

/**
 * RE1's flags as a new game of Jill's starts them (GameStart.cpp, game_init): all clear (bank 0
 * bit 0, the return flag, too), but bank 7, the room items' flags (set: the item still lies
 * there), all set but items 1 and 172.
 */
static u16 sNvStateWord[128];
void NvFlags_Op(s32 bank, s32 bit, s32 mode);
static s32 sNvRunReady; // the run's flags are set up (a new game's, or a typewriter save's)

static void NvFlags_NewGame(void) {
    if (sNvRunReady) {
        return;
    }
    sNvRunReady = true;
    memset(sNvFlags, 0, sizeof(sNvFlags));
    memset(sNvStateWord, 0, sizeof(sNvStateWord));
    memset(sNvFlags[7], 0xFF, sizeof(sNvFlags[7]));
    sNvFlags[7][1 >> 5] &= ~(0x80000000u >> (1 & 31));
    sNvFlags[7][172 >> 5] &= ~(0x80000000u >> (172 & 31));
    NvFlags_Op(5, NV_MSF_CHAR_VARIANT, 0); // (her game: the rooms' scripts branch on it)
    if (getenv("NV_WORD") != NULL) { // dev: NV_WORD="i:v" a state word from the start (4: the countdown)
        int wi;
        int wv;

        if (sscanf(getenv("NV_WORD"), "%d:%d", &wi, &wv) == 2) {
            sNvStateWord[wi & 0x7F] = (u16)wv;
        }
    }
    if (getenv("NV_FLAGS") != NULL) { // dev: NV_FLAGS="bank:bit,bank:bit" (hex bits) set from the start
        const char* f = getenv("NV_FLAGS");
        int b;
        int n;

        while (sscanf(f, "%d:%x", &b, &n) == 2) {
            NvFlags_Op(b, n, 0);
            f = strchr(f, ',');
            if (f == NULL) {
                break;
            }
            f++;
        }
    }
    // bank 0 bit 0 stays clear: it's the return flag (set once back from the guardhouse:
    // SCENARIO_FLAG_STAGE_VARIANT, which also sends doors to stages 6 and 7), not the character.
    // The first visit's rooms test it clear (210 only brings Yawn in while it is); the character
    // is state byte 9 (NvScd_StateByte)
}

s32 NvFlags_Test(s32 bank, s32 bit) {
    if ((bank < 0) || (bank >= 10)) {
        return 0;
    }
    if ((bank == 5) && ((bit & 0xFF) >= 0x40) && ((bit & 0xFF) < 0x80)) {
        // (bank 5 is two words: RE1 reads past them into the system flags, bank 4 - 205's gas
        // trap tests 5:40, which is 4:00)
        bank = 4;
        bit -= 0x40;
    }
    return (sNvFlags[bank][(bit >> 5) & 7] & (0x80000000u >> (bit & 31))) != 0;
}

void NvFlags_Op(s32 bank, s32 bit, s32 mode) {
    u32* w;
    u32 mask = 0x80000000u >> (bit & 31);

    if ((bank < 0) || (bank >= 10)) {
        return;
    }
    if ((bank == 5) && ((bit & 0xFF) >= 0x40) && ((bit & 0xFF) < 0x80)) {
        bank = 4; // (as NvFlags_Test)
        bit -= 0x40;
        mask = 0x80000000u >> (bit & 31);
    }
    w = &sNvFlags[bank][(bit >> 5) & 7];
    if (mode == 0) {
        *w |= mask;
    } else if (mode == 1) {
        *w &= ~mask;
    } else if (mode == 2) {
        *w ^= mask;
    }
}

/* ---- the command interpreter ---- */

// fixed command lengths (the converter's rdt.SCD_LEN, from reevengi's notes, checked against
// CmdFunctions.cpp for the ones run here)
static const u8 sNvScdLen[0x51] = {
    2, 2, 2, 2, 4, 4, 4, 6, 4, 2, 2, 4, 26, 18, 2, 8,  // 0x00
    2, 2, 10, 4, 4, 2, 2, 0, 26, 4, 2, 22, 6, 2, 4, 28, // 0x10
    14, 14, 4, 2, 4, 4, 1, 2, 0, 2, 12, 4, 2, 4, 1, 4,  // 0x20
    12, 4, 4, 0, 8, 4, 4, 4, 4, 2, 4, 6, 6, 12, 2, 6,   // 0x30
    16, 4, 4, 4, 2, 2, 44, 14, 2, 2, 2, 2, 4, 2, 4, 2,  // 0x40 (0x46: 44, cmd_room_lights_set)
    2,
};

static s32 NvScd_Len(const u8* p) {
    u8 op = p[0];

    if (op == 0x17) {
        return (p[4] <= 3) ? 10 : 6;
    }
    if (op == 0x28) {
        return (p[3] == 1) ? 8 : ((p[3] == 6) || (p[3] == 8)) ? 4 : 6;
    }
    if (op == 0x33) {
        switch (p[1]) {
            case 0:
            case 4:
            case 6:
            case 7:
                return 2;
            default:
                return 4;
        }
    }
    return (op <= 0x50) ? sNvScdLen[op] : 0;
}

static void NvEvt_Create(s32 slot, s32 script);
static void NvScd_Message(s32 id, u32 pause);
static void NvVoice_Play(s32 id);
static void NvAot_Run(s32 i, s32 handler);
static const char* NvMsg_Text(s32 id, s32 item);
static s32 sNvMsgYn; // NvMsg_Text's last message ends in RE1's yes / no
static s32 NvMsg_Start(PlayState* play, s32 id, s32 item, u32 pause);
static s32 NvDesk_Key(void);
const char* NvText_Get(s32 kind, s32 i);
s32 NvItems_Find(s32 id);
s32 NvItems_Remove(s32 id);
s32 NvItems_EquippedId(void);
s32 NvItems_GroupCount(s32 group);
s32 NvItems_Slot(s32 i, s32* qty);

// the rest of RE1's state block as the scripts write it (cmd_state_byte_set 0x08, cmd_state_word_set
// 0x31): what a room's script sets for later ones to test (the stage, room and camera are live)
static u8 sNvStateSet[256];
// update_player_position's probe results (BioCard 0x210/0x211, state bytes 16/17): the index + 1 of
// the action entry hit by the point ahead of her, and by her (or a room object's) own position;
// cleared each frame with the item-use ids (room_state_reset)
static u8 sNvFwdPosAction;
static u8 sNvEntPosAction;
static u8 sNvStateVal[256];
static u8 sNvFxLastBullet; // the last 0x3D's type (0x3E clears it)
void NvFx_ScdClearType(u8 type);
void NvFx_Kill(u8 slot);
u8 NvFx_Spawn(u8 type, u8 group, const s32* pos);

// the room's special light (cmd_room_light_fade_set, main_loop's g_FadingRect): a colour over the
// whole screen, its strength stepping by the delta each tick until it runs past either end
static s16 sNvLightState = -1; // g_SpecialRoomLightState (negative: off)
static s16 sNvLightDelta;
static u8 sNvLightMode; // its blend: 0 half, 1 added, 2 taken away, 3 a quarter added
static u8 sNvLightR;
static u8 sNvLightG;
static u8 sNvLightB;

/** A new RE1 run (z_nv_save.c): the flags and the state block as a new game has them. */
void NvScript_NewRun(void) {
    memset(sNvStateSet, 0, sizeof(sNvStateSet));
    memset(sNvStateVal, 0, sizeof(sNvStateVal));
    sNvRunReady = false;
    NvFlags_NewGame();
}

/** The run's script state for the typewriter's save (z_nv_save.c): its size, written to p if not NULL. */
u32 NvScript_SaveState(u8* p) {
    u32 n = sizeof(sNvFlags) + sizeof(sNvStateWord) + sizeof(sNvStateSet) + sizeof(sNvStateVal);

    if (p != NULL) {
        memcpy(p, sNvFlags, sizeof(sNvFlags));
        p += sizeof(sNvFlags);
        memcpy(p, sNvStateWord, sizeof(sNvStateWord));
        p += sizeof(sNvStateWord);
        memcpy(p, sNvStateSet, sizeof(sNvStateSet));
        p += sizeof(sNvStateSet);
        memcpy(p, sNvStateVal, sizeof(sNvStateVal));
    }
    return n;
}

void NvScript_LoadState(const u8* p) {
    memcpy(sNvFlags, p, sizeof(sNvFlags));
    p += sizeof(sNvFlags);
    memcpy(sNvStateWord, p, sizeof(sNvStateWord));
    p += sizeof(sNvStateWord);
    memcpy(sNvStateSet, p, sizeof(sNvStateSet));
    p += sizeof(sNvStateSet);
    memcpy(sNvStateVal, p, sizeof(sNvStateVal));
    memset(sNvFlags[9], 0, sizeof(sNvFlags[9])); // (the item-use flags are per frame)
    NvFlags_Op(5, NV_MSF_CHAR_VARIANT, 0); // (a save from before the flag was kept)
    sNvRunReady = true;
}


/** The state block's word i (4: the self-destruct countdown, z_nv_ending.c). */
u16* NvScript_StateWord(s32 i) {
    return &sNvStateWord[i & 0x7F];
}
static u32 sNvSprOff;    // the room sprites switched off, a bit per group id (all on again at each cut)
static u8 sNvUsedItem;   // g_usedItemId: the item last used on the room (the status screen's Use)
static u8 sNvPickedItem; // g_pickedItemId: the item last picked up

/** RoomSpr_SetActive / _SetInactive: the camera's sprite group id shown or not (z_nv_mask.c draws it). */
void NvSpr_Set(s32 id, s32 on) {
    if ((id >= 0) && (id < 32)) {
        if (on) {
            sNvSprOff &= ~(1u << id);
        } else {
            sNvSprOff |= 1u << id;
        }
    }
}

s32 NvSpr_On(s32 id) {
    return (id < 0) || (id >= 32) || !(sNvSprOff & (1u << id));
}

/** The status screen used an item on the room (z_nv_items.c NvItems_Use). */
void NvScript_SetUsedItem(s32 id) {
    sNvUsedItem = (u8)id;
    if ((id == 0x1D) || (id == 0x1E)) {
        // the crank (MainMenu.cpp's exit: animation 8, then SysFlags 0x20 at its end): she turns
        // it, and the room's script waits on that flag
        NvRe1Entity* e = NvCast_Player();

        e->state = 8;
        e->ignore_player_flag = 0;
        e->action_behavior = 1;
        e->action_state = 0;
        e->scd_anim_param = 0x20;
        e->scd_entity_flags = 0;
        e->scd_timer = 0;
    }
}

/* ---- g_message_flags (bank 6's word): what RE1 lets run ---- */
#define NV_MF_PLAYER 0x001 // the player's state machine
#define NV_MF_NPC 0x002    // the characters think
#define NV_MF_ENEMY 0x004  // the enemies think
#define NV_MF_EVENTS 0x080 // the room's events step
#define NV_MF_INPUT 0x100  // the pad reaches the player
#define NV_MF_FADE_TICKS 7 // game_loop's fade into a room: nothing but the main script runs
#define NV_MF_LOCK_MAX (20 * 120) // (a scene that never gives the pad back: let go, and say so)

static u32 sNvMfBackup; // g_messageFlagsBackup: the flags a message put aside
static s32 sNvMfHeld;   // a message is holding them
static s32 sNvMfFade;   // ticks of the room's fade left
static s32 sNvMfFirst;  // the room's events haven't had room_set's first step
static s32 sNvMfLocked; // frames the pad has been kept from the player
s32 gNvInputLock;       // the pad is kept from the player now (z_player.c, z_nv_door.c)

/** g_message_flags now: all on outside RE1's rooms. */
u32 NvScript_MsgFlags(void) {
    if ((sNvMain == NULL) || (sNvPlay == NULL) || (sNvPlay->sceneNum < SCENE_RE1_100)) {
        return 0xFFFF;
    }
    return sNvFlags[6][0] & 0xFFFF;
}

/** set_message_display: the flags in mask are put down while the message is up. */
static void NvMf_Hold(u32 mask) {
    if (!sNvMfHeld) {
        sNvMfHeld = true;
        sNvMfBackup = sNvFlags[6][0];
        sNvFlags[6][0] &= ~mask;
    }
}

/**
 * Each frame: a message that came up some other way (a door's, an item's: RE1's own show theirs
 * with everything held, 0xFF) holds the flags too; they come back as it closes. Then the pad:
 * kept from the player while the scripts have the input or her state machine off.
 */
static void NvMf_Update(PlayState* play) {
    s32 open = (Message_GetState(&play->msgCtx) != TEXT_STATE_NONE);
    u32 mf;

    if (open && !sNvMfHeld && (sNvMain != NULL)) {
        NvMf_Hold(0xFF);
    } else if (!open && sNvMfHeld) {
        sNvMfHeld = false;
        sNvFlags[6][0] = sNvMfBackup;
    }
    mf = NvScript_MsgFlags();
    gNvInputLock = ((mf & (NV_MF_INPUT | NV_MF_PLAYER)) != (NV_MF_INPUT | NV_MF_PLAYER)) && !open;
    if (!gNvInputLock) {
        sNvMfLocked = 0;
    } else if (++sNvMfLocked > NV_MF_LOCK_MAX) {
        printf("NVMF the room kept the pad too long (flags %04X): given back\n", (unsigned)mf);
        fflush(stdout);
        sNvFlags[6][0] |= NV_MF_INPUT | NV_MF_PLAYER;
        gNvInputLock = false;
        sNvMfLocked = 0;
    }
}

static u8 sNvLastEnemyFlags; // g_scdLastEnemyFlags
static s32 sNvDeskAsk = -1; // check_desk: the desk whose "use the key?" is open
static s32 sNvDeskCam;      // the desk's camera is up while its item is offered
static u8 sNvChoice; // the last yes / no a room's message asked: 0 yes, 1 no (g_menu_choice_id)
static s32 sNvAsk;   // such a question is open

static u8 NvScd_StateByte(s32 field) {
    if ((field > 3) && (field != 5) && (field != 9) && (field != 10) && sNvStateSet[field & 0xFF]) {
        return sNvStateVal[field & 0xFF];
    }
    switch (field) {
        case 0:
            return sNvStage;
        case 1:
            return sNvRoom;
        case 2:
        case 3:
            return sNvCamera;
        case 5: // menu_choice_id: 0x80 while a message is up, then the answer to its yes / no (0 yes, 1 no)
            return (((sNvPlay != NULL) && (Message_GetState(&sNvPlay->msgCtx) != TEXT_STATE_NONE)) ? 0x80 : 0) |
                   sNvChoice;
        case 9: // characterModelId: Jill (0 Chris, 1 Jill; +8 her other outfit)
            return 1;
        case 10: // scdLastEnemyFlags (cmd_enemy_flags_get)
            return sNvLastEnemyFlags;
        case 16:
            return sNvFwdPosAction;
        case 17:
            return sNvEntPosAction;
        case 18: // usedItemId, pickedItemId (what cmd_used_item_test / cmd_picked_item_test read)
            return sNvUsedItem;
        case 19:
            return sNvPickedItem;
        default:
            return 0;
    }
}

/** One command at sNvIp (script_command_funcs_table): 1 to go on, 0 to stop. */
s32 NvEnemy_Place(s32 slot, s32* t, s16* angle);
s32 NvObj_Pose(s32 i, s16* pos, s16* rot);

void NvCast_Placed(void);

static s32 NvScd_Cmd(void) {
    const u8* p = sNvIp;
    s32 n;

    switch (p[0]) {
        case 0x00: // cmd_block_end
            sNvContinue = 0;
            return 0;
        case 0x01: // cmd_if: where the block ends
            if (sNvSp < NV_SCD_STACK) {
                sNvStack[sNvSp++] = p + 2 + p[1];
            }
            sNvContinue++;
            sNvIp = p + 2;
            return 1;
        case 0x02: // cmd_else
            if (sNvSp > 0) {
                sNvSp--;
            }
            sNvContinue--;
            sNvIp = p + p[1];
            return 1;
        case 0x03: // cmd_end_if
            if (sNvSp > 0) {
                sNvSp--;
            }
            sNvContinue--;
            sNvIp = p + 2;
            return 1;
        case 0x04: // cmd_bit_test: true when the bit differs from the byte given
            sNvIp = p + 4;
            return NvFlags_Test(p[1], p[2]) ^ (p[3] != 0);
        case 0x05: // cmd_bit_op
            NvFlags_Op(p[1], p[2], p[3]);
            sNvIp = p + 4;
            return 1;
        case 0x06: { // cmd_state_byte_test
            u8 v = NvScd_StateByte(p[1]);
            u8 c = p[3];

            sNvIp = p + 4;
            switch (p[2]) {
                case 0:
                    return c == v;
                case 1:
                    return c < v;
                case 2:
                    return c <= v;
                case 3:
                    return v < c;
                case 4:
                    return v <= c;
                case 5:
                    return c != v;
                default:
                    return 0;
            }
        }
        case 0x0C: // cmd_door_set: handler 1 (doors themselves are z_nv_door.c's)
        case 0x0D: // cmd_room_action_set
            if (p[1] < NV_AOT_SLOTS) {
                NvAot* a = &sNvAot[p[1]];

                memcpy(a->zone, p + 2, sizeof(a->zone));
                a->isItem = false;
                if (p[0] == 0x0C) {
                    a->handler = 1;
                    a->flags = p[0x19];
                } else {
                    a->handler = p[10];
                    a->flags = p[11];
                    memcpy(a->param, p + 12, sizeof(a->param));
                }
                sNvAotCount = MAX(sNvAotCount, p[1] + 1);
            }
            sNvIp = p + NvScd_Len(p);
            return 1;
        case 0x12: // cmd_room_action_reset: all but the zone
            if (p[1] < NV_AOT_SLOTS) {
                sNvAot[p[1]].handler = p[2];
                sNvAot[p[1]].flags = p[3];
                memcpy(sNvAot[p[1]].param, p + 4, sizeof(sNvAot[p[1]].param));
            }
            sNvIp = p + 10;
            return 1;
        case 0x13: // cmd_room_action_arm: handler and flags
            if (p[1] < NV_AOT_SLOTS) {
                sNvAot[p[1]].handler = p[2];
                sNvAot[p[1]].flags = p[3];
            }
            sNvIp = p + 4;
            return 1;
        case 0x15: // cmd_bgm_play: a music channel starts (z_nv_sound.c)
            NvBgm_ScdPlay(p[1]);
            sNvIp = p + 2;
            return 1;
        case 0x16: // cmd_bgm_stop
            NvBgm_ScdStop(p[1]);
            sNvIp = p + 2;
            return 1;
        case 0x33: // cmd_player_prop_set: 0 the weapon put away, 5 her facing, 8 her status bits (the gas)
            if (p[1] == 0) {
                void NvItems_Unequip(void);

                NvItems_Unequip();
            } else if ((p[1] == 5) && (sNvPlay != NULL)) {
                Player* player = GET_PLAYER(sNvPlay);
                s16 dir = (s16)(p[2] | (p[3] << 8));

                player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw = (s16)(0x4000 - dir * 16);
            } else if (p[1] == 8) {
                extern u8 gNvRe1Poison;

                if (p[3] == 0) {
                    gNvRe1Poison = p[2];
                } else if (p[3] == 1) {
                    gNvRe1Poison |= p[2];
                } else if (p[3] == 2) {
                    gNvRe1Poison ^= p[2];
                }
                if (getenv("NV_LOG")) {
                    printf("NVSCD her status bits now %02X\n", gNvRe1Poison);
                    fflush(stdout);
                }
            }
            sNvIp = p + NvScd_Len(p);
            return 1;
        case 0x1C: // cmd_room_light_fade_set: the screen's coloured flash or fade (NvLight_Draw)
            sNvLightMode = p[1];
            sNvLightDelta = (s16)(p[2] | (p[3] << 8));
            sNvLightB = (p[4] & 1) ? 0xFF : 0;
            sNvLightG = (p[4] & 2) ? 0xFF : 0;
            sNvLightR = (p[4] & 4) ? 0xFF : 0;
            if (sNvLightDelta != 0) {
                sNvLightState = (sNvLightDelta < 1) ? 0x7FFF : 0;
            }
            if (getenv("NV_LOG")) {
                printf("NVLIGHT mode %d delta %d colour %d%d%d\n", sNvLightMode, sNvLightDelta, sNvLightR != 0,
                       sNvLightG != 0, sNvLightB != 0);
                fflush(stdout);
            }
            sNvIp = p + 6;
            return 1;
        case 0x27: // cmd_snd_fade_set: the music fades out (z_nv_sound.c)
            NvBgm_ScdFade(p[1]);
            sNvIp = p + 2;
            return 1;
        case 0x2F: // cmd_snd_pan_vol_set: a music channel's volume
            NvBgm_ScdPanVol(p[1], p[2], p[3]);
            sNvIp = p + 4;
            return 1;
        case 0x43: // cmd_bgm_volume_ramp: a playing channel slides away
            NvBgm_ScdRamp(p[1], (s8)p[2], p[3]);
            sNvIp = p + 4;
            return 1;
        case 0x4A: // cmd_bgm_restore: the music put aside by 0x4B starts again
            NvBgm_ScdRestore();
            sNvIp = p + 2;
            return 1;
        case 0x4B: // cmd_bgm_stop_all
            NvBgm_ScdStopAll();
            sNvIp = p + 2;
            return 1;
        case 0x37: // cmd_room_bgm_state_set: a room's music state (stage p[1] & 7, room p[2])
            NvBgm_ScdState(p[1] & 7, p[2], p[3]);
            sNvIp = p + 4;
            return 1;
        case 0x1F: // cmd_omodel_set: a room object (CmdFunctions.cpp 0x00461ac0)
            {
                NvObj* o = &sNvObj[p[1] & 0x3F];

                o->set = true;
                o->flags = p[2];
                o->parent = p[3];
                o->pos[0] = (s16)(p[4] | (p[5] << 8));
                o->pos[1] = (s16)(p[6] | (p[7] << 8));
                o->pos[2] = (s16)(p[8] | (p[9] << 8));
                o->rot[0] = 0;
                o->rot[1] = (s16)(p[10] | (p[11] << 8));
                o->rot[2] = 0;
                o->ext[0] = (s16)(p[0x18] | (p[0x19] << 8));
                o->ext[1] = (s16)(p[0x16] | (p[0x17] << 8));
                o->ext[2] = (s16)(p[0x1A] | (p[0x1B] << 8));
                o->push = 0;
                printf("NVOBJ %d flags %02X parent %02X pos %d,%d,%d ry %d\n", p[1] & 0x3F, p[2], p[3], o->pos[0],
                       o->pos[1], o->pos[2], o->rot[1]);
            }
            sNvIp = p + 28;
            return 1;
        case 0x35: // cmd_obj_flag_set: an object's (or item model's) flags byte
            // (op1 = the opcode and its kind byte; op2 = the index and the new value)
            if (p[1] == 0) {
                sNvObj[p[2] & 0x3F].flags = p[3];
            } else if (p[1] == 1) {
                sNvItemHidden[p[2] % NV_AOT_SLOTS] = !(p[3] & 1);
            }
            sNvIp = p + 4;
            return 1;
        case 0x18: // cmd_item_model_set: an item lying in the room (CmdFunctions.cpp 0x00461220)
            if ((p[1] & 0x7F) < NV_AOT_SLOTS) {
                NvAot* a = &sNvAot[p[1] & 0x7F];
                u8 item = p[10];

                memcpy(a->zone, p + 2, sizeof(a->zone));
                a->item = item;
                a->qty = p[11];
                a->itemFlag = p[0x16];
                // the handler by kind (4 an item, 0x0F a map, 0x0D a document), 0 once taken. Ink
                // ribbons lie in the rooms for Jill too: the console (and PC second-run) rule, the
                // user's choice over the PC's first-run Jill, who saves without them
                if (!NvItems_InRoom(a->itemFlag)) {
                    a->handler = 0;
                } else {
                    a->handler = (item < 0x4E) ? 4 : (item <= 0x53) ? 0x0F : 0x0D;
                }
                a->flags = p[0x17];
                a->isItem = true;
                a->model = p[12];
                a->parent = p[13];
                memcpy(a->pos, p + 0xE, sizeof(a->pos));
                a->param[0] = (p[0x18] | (p[0x19] << 8)) & 1;
                a->param[1] = p[12];
                a->param[2] = p[0x16];
                a->sparkle = 0xFF;
                {
                    // its glint (type 0x0B) while it lies there, if the record asks for one: its
                    // group by the record's kind (flags 0xF00), above the item by (flags & 0xF0) * 2
                    u16 f = p[0x18] | (p[0x19] << 8);
                    u16 kind = f & 0xF00;
                    u8 grp;
                    s32 at[3];

                    if (kind < 0x101) {
                        grp = (kind == 0x100) ? 0x0B : ((f & 0xF00) == 0) ? 0x03 : 0x14;
                    } else if (kind < 0x301) {
                        grp = (kind == 0x300) ? 0x1B : (kind == 0x200) ? 0x13 : 0x14;
                    } else if (kind < 0x501) {
                        grp = (kind == 0x500) ? 0x0C : (kind == 0x400) ? 0x04 : 0x14;
                    } else {
                        grp = (kind == 0x600) ? 0x14 : (kind == 0x700) ? 0x1C : 0x14;
                    }
                    if ((f & 0x8000) && NvItems_InRoom(a->itemFlag) && (p[13] == 0xFF)) {
                        at[0] = a->pos[0];
                        at[1] = a->pos[1] - (f & 0xF0) * 2;
                        at[2] = a->pos[2];
                        a->sparkle = NvFx_Spawn(0x0B, grp, at);
                    }
                }
                sNvAotCount = MAX(sNvAotCount, (p[1] & 0x7F) + 1);
            }
            sNvIp = p + 0x1A;
            return 1;
        case 0x14: // cmd_scd_event_create
            NvEvt_Create(p[2], p[3]);
            sNvIp = p + 4;
            return 1;
        case 0x1B: // cmd_enemy_set
            if (sNvInitRun) {
                NvEnemy_ScdSet(sNvPlay, p);
            }
            sNvIp = p + 22;
            return 1;
        case 0x28: // cmd_enemy_prop_set
            n = NvScd_Len(p);
            NvEnemy_ScdProp(p[2], p[3], p[4] | (p[5] << 8), p[6]);
            sNvIp = p + n;
            return 1;
        case 0x30: { // cmd_boundary_set: a collision record rewritten (a gate, a shelf: z_nv_zombie.c)
            void NvRe1_BoundSet(s32 list, s32 idx, u8 flagMode, u16 z, u16 w, u16 x, u16 y);
            u16 v[4];

            memcpy(v, p + 4, sizeof(v)); // z, w, x, y
            NvRe1_BoundSet(p[1], p[2], p[3], v[0], v[1], v[2], v[3]);
            sNvIp = p + 12;
            return 1;
        }
        case 0x08: // cmd_state_byte_set: one byte of the state block
            sNvStateSet[p[1]] = 1;
            sNvStateVal[p[1]] = p[2];
            if (p[1] == 2) { // (g_roomCameraId: a scene's camera, usually with the cut lock on)
                void NvCamera_Set(s32 n);

                NvCamera_Set(p[2]);
                if (getenv("NV_LOG")) {
                    printf("NVCAM the script cuts to camera %d\n", p[2]);
                    fflush(stdout);
                }
            }
            sNvIp = p + 4;
            return 1;
        case 0x31: { // cmd_state_word_set: one word of it (the lab countdown, 411's emergency light)
            u16 v;

            memcpy(&v, p + 2, 2);
            sNvStateWord[p[1] & 0x7F] = v;
            if (p[1] == 1) { // (specialRoomLightState and its delta: the light over the room, NvLight_Draw)
                sNvLightState = (s16)v;
            } else if (p[1] == 2) {
                sNvLightDelta = (s16)v;
            }
            sNvIp = p + 4;
            return 1;
        }
        case 0x09: { // cmd_cut_lock_set: cut to a camera and hold it there (z_nv_camera.c)
            void NvCamera_CutLock(s32 n);

            NvCamera_CutLock(p[1]);
            sNvIp = p + 2;
            return 1;
        }
        case 0x0A: { // cmd_current_cut_set: back to the camera before the cut
            void NvCamera_CutRestore(void);

            NvCamera_CutRestore();
            sNvIp = p + 2;
            return 1;
        }
        case 0x23: { // cmd_cut_lock_write
            void NvCamera_Lock(s32 on);

            NvCamera_Lock(p[1]);
            sNvIp = p + 2;
            return 1;
        }
        case 0x3A: { // cmd_cut_zone_set: a camera switch zone's cameras rewritten
            void NvCamera_ZoneSet(s32 i, s32 from, s32 to);

            NvCamera_ZoneSet(p[1], p[2], p[3]);
            sNvIp = p + 4;
            return 1;
        }
        case 0x1E: // cmd_voice_play: 1 a cutscene's voice line by number (the stage's), 2 it ends
            if (p[1] == 1) {
                NvVoice_Play(p[2] | (p[3] << 8));
            } else if (p[1] == 2) {
                NvSound_LineStop();
            }
            sNvIp = p + 4;
            return 1;
        case 0x25: // cmd_room_sprite_set: a sprite group on (high byte 0) or off
            NvSpr_Set(p[2], p[3] == 0);
            sNvIp = p + 4;
            return 1;
        case 0x49: // cmd_room_sprite_hide: a sprite group off (0xFF: none)
            if (p[1] != 0xFF) {
                NvSpr_Set(p[1] & 0x1F, false);
            }
            sNvIp = p + 2;
            return 1;
        case 0x3B: // cmd_obj_rotation_set: an object's (or item model's) tilt, x and z
            if ((p[1] & 0x80) && sNvObj[p[1] & 0x3F].set) {
                memcpy(&sNvObj[p[1] & 0x3F].rot[0], p + 2, 2);
                memcpy(&sNvObj[p[1] & 0x3F].rot[2], p + 4, 2);
            } // (an item model's: not drawn turned yet)
            sNvIp = p + 6;
            return 1;
        case 0x47: { // cmd_obj_transform_set: an object's whole place and turn
            NvObj* o = &sNvObj[p[1] & 0x3F];

            memcpy(o->rot, p + 2, 6);
            memcpy(o->pos, p + 8, 6);
            o->set = true;
            sNvIp = p + 14;
            return 1;
        }
        case 0x0B: // cmd_message_set: a room message, shown now, with p[2..3] of the message flags held
            NvScd_Message(p[1], p[2] | (p[3] << 8));
            sNvIp = p + 4;
            return 1;
        case 0x20: { // cmd_player_pos_set: the player put at (x, y, z) facing an angle
            s16 v[6];
            Player* player = (sNvPlay != NULL) ? GET_PLAYER(sNvPlay) : NULL;

            memcpy(v, p + 2, sizeof(v)); // (pad), angle, (speed), x, y, z
            if (player != NULL) {
                player->actor.world.pos.x = player->actor.home.pos.x = v[3] * NV_RE1_SCALE;
                player->actor.world.pos.y = player->actor.home.pos.y = -v[4] * NV_RE1_SCALE;
                player->actor.world.pos.z = player->actor.home.pos.z = -v[5] * NV_RE1_SCALE;
                player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw = (s16)(0x4000 - v[1] * 16);
                player->linearVelocity = 0.0f;
            }
            NvCast_Player()->t[0] = v[3]; // (a scene holding her goes on from there)
            NvCast_Player()->t[1] = v[4];
            NvCast_Player()->t[2] = v[5];
            NvCast_Player()->angle = v[1] & 0xFFF;
            NvCast_Placed();
            sNvIp = p + 14;
            return 1;
        }
        case 0x41: { // cmd_entity_posy_set: her height (entity 0; the rooms use only her: 300-305's starts)
            Player* player = (sNvPlay != NULL) ? GET_PLAYER(sNvPlay) : NULL;
            s16 y;

            memcpy(&y, p + 2, 2);
            if ((p[1] == 0) && (player != NULL)) {
                player->actor.world.pos.y = player->actor.home.pos.y = -y * NV_RE1_SCALE;
                player->actor.prevPos.y = player->actor.world.pos.y;
                NvCast_Player()->t[1] = y;
            }
            sNvIp = p + 4;
            return 1;
        }
        case 0x45: { // cmd_player_posy_add: her height moved by a signed byte (310's ladder: her climb)
            Player* player = (sNvPlay != NULL) ? GET_PLAYER(sNvPlay) : NULL;

            if (player != NULL) {
                player->actor.world.pos.y -= (s8)p[1] * NV_RE1_SCALE;
                player->actor.velocity.y = -player->actor.gravity; // (held there: the climb's room has no floor)
            }
            NvCast_Player()->t[1] += (s8)p[1];
            sNvIp = p + 2;
            return 1;
        }
        case 0x21: { // cmd_enemy_pos_set
            s16 v[5];

            memcpy(v, p + 4, sizeof(v)); // angle, (pitch), x, y, z
            NvEnemy_ScdPos((s8)p[1], v[0], v[2], v[3], v[4]);
            sNvIp = p + 14;
            return 1;
        }
        case 0x17: { // cmd_sfx_3d_play: bank p[1], sound p[2], from (x, z) (position type 0) or the player
            void NvSnd_At(s32 slot, s32 x, s32 z, s32 lx, s32 lz);
            s16 x;
            s16 z;

            memcpy(&x, p + 6, 2);
            memcpy(&z, p + 8, 2);
            if (p[4] != 0) {
                x = (s16)gNvRe1Player.t[0];
                z = (s16)gNvRe1Player.t[2];
            }
            if (p[1] == 2) { // the room's bank (doors, glass, its enemies): Play3DSnd's type 2
                NvSnd_At(p[2], (u16)x, (u16)z, gNvRe1Player.t[0], gNvRe1Player.t[2]);
            } else if (p[1] == 0) { // the room's door set (load_room_sfx: a gate, a lift)
                void NvSnd_RoomSet(s32 id);

                NvSnd_RoomSet(p[2]);
            }
            sNvIp = p + NvScd_Len(p);
            return 1;
        }
        case 0x3E: // cmd_bullet_effect_clear: the last 0x3D's type, freed (z_nv_effects.c)
            NvFx_ScdClearType(sNvFxLastBullet);
            sNvIp = p + 2;
            return 1;
        case 0x42: // cmd_effect_clear_typed: a type / group pair's effects freed
            NvFx_ScdClearTyped(p[1], p[2]);
            sNvIp = p + 4;
            return 1;
        case 0x48: // cmd_effect_pool_clear: all of them
            NvFx_ScdClearAll();
            sNvIp = p + 2;
            return 1;
        case 0x19: // cmd_model_flag_set: an item model's flags byte (as cmd_obj_flag_set's kind 1)
            sNvItemHidden[p[1] % NV_AOT_SLOTS] = !(p[2] & 1);
            sNvIp = p + 4;
            return 1;
        case 0x3D: // cmd_bullet_effect_spawn, 0x2A cmd_effect_spawn: RE1's 2D effects (z_nv_effects.c)
            sNvFxLastBullet = p[1];
            // fall through
        case 0x2A:
            NvFx_ScdSpawn(p);
            sNvIp = p + NvScd_Len(p);
            return 1;
        case 0x29: // cmd_fmv_set
            NvMovie_Start(p[1]);
            sNvIp = p + 2;
            return 1;
        case 0x44: // cmd_scd_event_kill
            if (p[1] < NV_EVT_SLOTS) {
                sNvEvt[p[1]].active = 0;
            }
            sNvIp = p + 2;
            return 1;
        case 0x07: { // cmd_state_word_test: a word of the state block (3 the dice, 4 the lab's countdown)
            u16 v = sNvStateWord[p[2] & 0x7F];
            u16 c = p[4] | (p[5] << 8);

            if (p[2] == 1) { // (the light over the room, as it stands)
                v = (u16)sNvLightState;
            } else if (p[2] == 2) {
                v = (u16)sNvLightDelta;
            } else if (p[2] == 5) { // (playerHealthCopy: the health her game started with)
                v = 96;
            }

            sNvIp = p + 6;
            switch (p[3]) {
                case 0:
                    return c == v;
                case 1:
                    return v > c;
                case 2:
                    return v >= c;
                case 3:
                    return c > v;
                case 4:
                    return c >= v;
                case 5:
                    return c != v;
                default:
                    return 0;
            }
        }
        case 0x10: // cmd_used_item_test
            sNvIp = p + 2;
            if ((sNvUsedItem != 0) && getenv("NV_LOG")) {
                printf("NVSCD used item %02X tested against %02X (4:23 %d)\n", sNvUsedItem, p[1], (int)NvFlags_Test(4, 0x23));
                fflush(stdout);
            }
            return p[1] == sNvUsedItem;
        case 0x11: // cmd_picked_item_test
            sNvIp = p + 2;
            return p[1] == sNvPickedItem;
        case 0x1A: // cmd_item_search: she carries it
            sNvIp = p + 2;
            return NvItems_Find(p[1]) >= 0;
        case 0x1D: // cmd_equipped_item_test (the scripts test 0: nothing equipped)
            sNvIp = p + 2;
            return NvItems_EquippedId() == p[1];
        case 0x22: { // cmd_item_count_test: how many of an item group she carries
            s32 total = NvItems_GroupCount(p[1]);
            s32 c = p[3];

            sNvIp = p + 4;
            if (total < 0) {
                return 0;
            }
            switch (p[2]) {
                case 0:
                    return total == c;
                case 1:
                    return c < total;
                case 2:
                    return c <= total;
                case 3:
                    return total < c;
                case 4:
                    return total <= c;
                case 5:
                    return total != c;
                default:
                    return 0;
            }
        }
        case 0x2C: // cmd_item_remove
            sNvIp = p + 2;
            return NvItems_Remove(p[1]);
        case 0x24: // cmd_room_action: a room action entry's handler run by the script
            NvAot_Run(p[1], p[2]);
            sNvIp = p + 4;
            return 1;
        case 0x2D: // cmd_got_item: the entry's handler (an item given), with the "got it" prompt
            NvAot_Run(p[1], p[2]);
            sNvIp = p + 4;
            return 0;
        case 0x4C: { // cmd_item_record_transfer: an item entry's quantity to / from a state byte
            u8 f = p[3];
            NvAot* a = (p[2] < NV_AOT_SLOTS) ? &sNvAot[p[2]] : NULL;

            if (a != NULL) {
                if (p[1] == 0) {
                    sNvStateSet[f] = 1;
                    sNvStateVal[f] = a->qty;
                } else if (p[1] == 1) {
                    a->qty = NvScd_StateByte(f);
                } else if ((p[1] == 2) && (NvItems_Find(a->item) >= 0)) {
                    s32 q;

                    NvItems_Slot(NvItems_Find(a->item), &q);
                    sNvStateSet[f] = 1;
                    sNvStateVal[f] = (u8)q;
                    a->qty = (u8)q;
                }
            }
            sNvIp = p + 4;
            return 1;
        }
        case 0x3C: // cmd_player_dist_test: within u16 +4 of enemy slot n (kind 0) or object n (kind 1)
        case 0x3F: { // cmd_player_dir_test: her facing within [u16 +2, u16 +4] (wrapping, 4096 a turn)
            Player* pl = (sNvPlay != NULL) ? GET_PLAYER(sNvPlay) : NULL;
            s32 r = 0;

            if (pl != NULL) {
                if (p[0] == 0x3F) {
                    u16 lo = p[2] | (p[3] << 8);
                    u16 hi = p[4] | (p[5] << 8);
                    u16 dir = (u16)((0x4000 - pl->actor.shape.rot.y) / 16) & 0xFFF;

                    r = (u16)(dir - lo) <= (u32)(hi - lo);
                } else {
                    s32 t[3];
                    s16 ang;
                    s16 op[3];
                    s16 orot[3];
                    s32 ok = false;
                    u32 maxd = p[4] | (p[5] << 8);

                    if (p[2] == 0) {
                        ok = NvEnemy_Place(p[3], t, &ang);
                    } else if ((p[2] == 1) && NvObj_Pose(p[3], op, orot)) {
                        t[0] = op[0];
                        t[2] = op[2];
                        ok = true;
                    }
                    if (ok) {
                        f32 dx = pl->actor.world.pos.x / NV_RE1_SCALE - t[0];
                        f32 dz = -pl->actor.world.pos.z / NV_RE1_SCALE - t[2];

                        r = sqrtf(SQ(dx) + SQ(dz)) <= maxd;
                    }
                }
            }
            sNvIp = p + NvScd_Len(p);
            return r;
        }
        case 0x0E: // cmd_skip_2bytes_opcode, cmd_skip_4bytes: nothing in RE1 either
        case 0x32:
            sNvIp = p + NvScd_Len(p);
            return 1;
        case 0x50: // cmd_costume_variant_test: the costume bit 0x4F sets, which no Jill script sets: false
            sNvIp = p + NvScd_Len(p);
            return 0;
        case 0x36: { // cmd_obj_field_test: object p[1]'s push counter (+0x86) against p[3], compare p[2]
            s32 v = ((p[1] < NV_OBJ_SLOTS) && sNvObj[p[1]].set) ? sNvObj[p[1]].push : 0;
            s32 c = p[3];
            s32 r;

            switch (p[2]) {
                case 0: r = (v == c); break;
                case 1: r = (v > c); break;
                case 2: r = (v >= c); break;
                case 3: r = (c > v); break;
                case 4: r = (c >= v); break;
                case 5: r = (c != v); break;
                default: r = 0; break;
            }
            sNvIp = p + NvScd_Len(p);
            return r;
        }
        case 0x38: // cmd_dpad_test: the pad's held buttons against a mask - none of them, here
            sNvIp = p + 4;
            return p[1] != 0; // (p[1]: true while none of the mask is held)
        case 0x39: { // cmd_enemy_flags_get: slot p[1]'s behaviour flags into state byte 10
            s32 NvEnemy_SlotFlags(s32 slot);

            sNvLastEnemyFlags = NvEnemy_SlotFlags(p[1]);
            sNvIp = p + 2;
            return 1;
        }
        default:
            n = NvScd_Len(p);
            if (getenv("NV_LOG")) { // (the room sweep counts these: commands not run yet)
                static u8 sSeen[256];

                if (!sSeen[p[0]]) {
                    sSeen[p[0]] = 1;
                    printf("NVSCD %s op 0x%02X (room %d.%02X)\n", (n <= 0) ? "unknown, the script stops at" : "skipped",
                           p[0], sNvStage + 1, sNvRoom);
                    fflush(stdout);
                }
            }
            if (n <= 0) {
                return 0;
            }
            sNvIp = p + n;
            return 1;
    }
}

/** run_command_functions: a block chain, each block to its end. */
static void NvScd_Run(const u8* chain) {
    s32 guard = 0;

    sNvSp = 0;
    sNvContinue = 0;
    while ((chain[0] | (chain[1] << 8)) != 0) {
        u16 size = chain[0] | (chain[1] << 8);

        sNvIp = chain + 2;
        while (true) {
            while (NvScd_Cmd()) {
                if (++guard > NV_SCD_GUARD) {
                    return;
                }
            }
            if ((sNvContinue <= 0) || (sNvSp <= 0)) {
                break;
            }
            sNvIp = sNvStack[--sNvSp];
            sNvContinue--;
            if (++guard > NV_SCD_GUARD) {
                return;
            }
        }
        chain += size;
    }
}

/* ---- events ---- */

static void NvEvt_Init(NvEvt* e, s32 script) {
    u32 off;

    if (sNvEvtTable == NULL) {
        e->active = 0;
        return;
    }
    memcpy(&off, sNvEvtTable + script * 4, 4);
    e->active = 1;
    e->state = 0;
    e->depth = -1;
    e->ip = sNvEvtTable + off;
    // ScdEventEntry_Init hands it the current entity: the creating event's character
    e->entKind = (sNvEvtCur != NULL) ? sNvEvtCur->entKind : 0xFF;
    e->entIdx = (sNvEvtCur != NULL) ? sNvEvtCur->entIdx : 0;
}

/** The event's character (scd_event_cmd_set_entity), or NULL (a room object, or none). */
static NvRe1Entity* NvEvt_Ent(const NvEvt* e) {
    if (e->entKind == 0) {
        return NvCast_Player();
    }
    if (e->entKind == 1) {
        return NvEnemy_SlotEntity(e->entIdx);
    }
    return NULL;
}

/** scd_event_state1_anim: one of the character opcodes (state 1); its length. */
static s32 NvEvt_Anim(NvEvt* e, const u8* p) {
    NvRe1Entity* t = NvEvt_Ent(e);
    NvRe1Entity dummy;
    u16 w;

    if (t == NULL) {
        memset(&dummy, 0, sizeof(dummy));
        t = &dummy; // (an object's: read past)
    }
    switch (p[0]) {
        case 0x00:
            return 1;
        case 0x80: // done with it
            t->ignore_player_flag = 0;
            // fall through
        case 0x8B:
            e->state = 0;
            return 1;
        case 0x81: // where it looks (its head: z_nv_npc.inc)
            t->lookFlags = p[1];
            if (!(p[1] & 0x0F)) {
                return 2;
            }
            if (p[1] == 0x93) { // a character, followed
                t->lookKind = p[2];
                t->lookIdx = p[4];
            } else {
                s16 v[3];

                memcpy(v, p + 2, sizeof(v));
                t->lookX = ((p[1] & 0x20) && (v[0] < 0)) ? v[0] + 0x1000 : v[0];
                t->lookZ = v[2];
            }
            t->lookStep = p[8] ? p[8] : 0xC0;
            return 10;
        case 0x82: // it stops following (the head stays where it is)
            t->lookFlags &= ~0x10;
            return 1;
        case 0x83: // a behaviour with a target point: walk there, turn to it
            t->state = 8;
            t->ignore_player_flag = 0;
            if (!(p[1] & 0x10)) {
                t->action_behavior = p[1];
                t->action_state = 0;
                t->collisionFlags &= 0x7F;
            } else if (!(t->collisionFlags & 0x80)) {
                t->action_behavior = p[1] & 0x0F;
                t->action_state = 0;
                t->collisionFlags |= 0x80;
            } else {
                t->action_behavior = p[1] & 0x0F;
            }
            t->unk_c6 = (s16)(p[2] | (p[3] << 8));
            t->unk_c8 = (s16)(p[4] | (p[5] << 8));
            t->scd_anim_param = p[6];
            t->scd_timer = 0x28;
            t->scd_entity_flags = 0;
            return 7;
        case 0x84: // play an animation (behaviour 1), raising a flag at its end
            t->state = 8;
            t->ignore_player_flag = 0;
            t->action_behavior = 1;
            t->action_state = 0;
            t->animationId = p[1];
            w = p[2] | (p[3] << 8);
            t->scd_anim_param = (u8)w;
            t->scd_entity_flags = (w >> 6) & 0x3FC;
            t->scd_timer = 0;
            return 4;
        case 0x85: // a behaviour and its animation
            t->state = 8;
            t->ignore_player_flag = 0;
            t->action_behavior = p[1];
            t->action_state = 0;
            t->animationId = p[2];
            t->scd_anim_param = p[3];
            t->scd_timer = 0;
            t->scd_entity_flags = 0;
            return 4;
        case 0x86: // back to its own control
            t->state = 1;
            t->ignore_player_flag = 0;
            t->action_behavior = 0;
            t->action_state = 0;
            t->hit_state = 0;
            return 1;
        case 0x87:
            w = p[2] | (p[3] << 8);
            if (p[1] == 0) {
                t->scd_entity_flags |= w;
            } else if (p[1] == 1) {
                t->scd_entity_flags = w;
            } else if (p[1] == 2) {
                t->scd_entity_flags ^= w;
            }
            return 4;
        case 0x88:
            t->scd_timer = p[2] | (p[3] << 8);
            return 4;
        case 0x89: // a frame of an animation (the player's remap table isn't ported: as 0x8A)
        case 0x8A:
            t->animation_frame_id = p[1];
            t->timing_control = 0;
            t->blend_counter = (t->scd_entity_flags & 0x20) ? 0 : 7;
            t->move_speed_current = 0;
            if ((p[0] == 0x89) && (t->id >= 0x20)) {
                t->action_state = 1;
            }
            return 2;
    }
    return 0;
}

/** scd_event_state2_movement: one of the movement opcodes (state 2) on the character; its length. */
static s32 NvEvt_Move(NvEvt* e, const u8* p) {
    NvRe1Entity* t = NvEvt_Ent(e);
    s16 v[3];

    if ((e->entKind == 2) || (e->entKind == 3)) { // a room object or an item model: placed, turned
        s16* pos = NULL;
        s16* rot = NULL;
        s32 k;

        if ((e->entKind == 2) && (e->entIdx < NV_OBJ_SLOTS)) {
            pos = sNvObj[e->entIdx].pos;
            rot = sNvObj[e->entIdx].rot;
            sNvObj[e->entIdx].set = true;
        } else {
            for (k = 0; k < sNvAotCount; k++) { // (the room item drawn with that model)
                if (sNvAot[k].isItem && (sNvAot[k].model == e->entIdx)) {
                    pos = sNvAot[k].pos;
                }
            }
        }
        if ((p[0] == 0x07) && (pos != NULL)) {
            memcpy(pos, p + 2, 6);
        } else if ((p[0] == 0x0A) && (pos != NULL) && (p[1] <= 2)) {
            memcpy(&pos[p[1]], p + 2, 2);
        } else if ((p[0] == 0x0B) && (rot != NULL)) {
            memcpy(rot, p + 2, 6);
        }
        if ((e->entKind == 2) && (e->entIdx < NV_OBJ_SLOTS)) { // an object's motion (state 2)
            NvObj* o = &sNvObj[e->entIdx];

            switch (p[0]) {
                case 0x02:
                case 0x04:
                    o->pos[0] += o->speed[0];
                    o->pos[1] += o->speed[1];
                    o->pos[2] += o->speed[2];
                    if (p[0] == 0x02) {
                        break;
                    }
                    // fall through
                case 0x03:
                    o->rot[0] += o->turn[0];
                    o->rot[1] += o->turn[1];
                    o->rot[2] += o->turn[2];
                    break;
                case 0x05:
                    o->speed[0] = (s8)p[1];
                    o->speed[1] = (s8)p[2];
                    o->speed[2] = (s8)p[3];
                    break;
                case 0x06:
                    o->turn[0] = (s8)p[1];
                    o->turn[1] = (s8)p[2];
                    o->turn[2] = (s8)p[3];
                    break;
            }
        }
        t = NULL;
    }

    if ((p[0] >= 2) && (p[0] <= 0x0B) && (t != NULL)) {
        switch (p[0]) {
            case 0x02:
            case 0x04:
                t->t[0] += t->speed[0];
                t->t[1] += t->speed[1];
                t->t[2] += t->speed[2];
                if (p[0] == 0x02) {
                    break;
                }
                // fall through
            case 0x03:
                t->angle += t->turn_delta;
                break;
            case 0x05:
                t->speed[0] = (s8)p[1];
                t->speed[1] = (s8)p[2];
                t->speed[2] = (s8)p[3];
                break;
            case 0x06:
                t->turn_delta = (s8)p[2];
                break;
            case 0x07:
                memcpy(v, p + 2, sizeof(v));
                t->t[0] = v[0];
                t->t[1] = v[1];
                t->t[2] = v[2];
                break;
            case 0x08:
                if (p[1] < 4) {
                    ((u8*)&t->state)[p[1]] = p[2]; // the state block from +0x84: state, ignore, behaviour, action
                }
                break;
            case 0x09:
                t->hit_state = p[1];
                break;
            case 0x0A:
                memcpy(v, p + 2, 2);
                if (p[1] <= 2) {
                    t->t[p[1]] = v[0];
                } else if (p[1] == 3) {
                    t->health = v[0];
                } else if (p[1] == 4) {
                    t->unk_c6 = v[0];
                } else if (p[1] == 5) {
                    t->unk_c8 = v[0];
                }
                break;
            case 0x0B:
                memcpy(v, p + 2, sizeof(v));
                t->angle = v[1] & 0xFFF;
                break;
        }
    }
    {
        static const u8 sLen[12] = { 1, 1, 1, 1, 1, 4, 4, 8, 3, 2, 4, 8 };

        return (p[0] < 12) ? sLen[p[0]] : 0;
    }
}

/** ScdEventEntry_Create: a slot past 7 takes the first free one. */
static void NvEvt_Create(s32 slot, s32 script) {
    s32 i;

    if (slot >= NV_EVT_SLOTS) {
        for (i = 0; i < NV_EVT_SLOTS; i++) {
            if (!sNvEvt[i].active) {
                break;
            }
        }
        if (i == NV_EVT_SLOTS) {
            return;
        }
        slot = i;
    }
    NvEvt_Init(&sNvEvt[slot], script);
    if (getenv("NV_LOG")) {
        printf("NVEVT event %d starts in slot %d\n", (int)script, (int)slot);
        fflush(stdout);
    }
}

/** One slot until it yields (room_events_check's per-slot loop). */
static void NvEvt_Step(NvEvt* e) {
    s32 guard = 0;

    sNvEvtCur = e;
    while (e->active && (++guard < 256)) {
        const u8* p = e->ip;
        s8 d = e->depth;

        if (getenv("NV_EVT_TRACE") != NULL) { // dev: each op an event runs, once per place in it
            static const u8* sLast[NV_EVT_SLOTS];
            s32 slot = (s32)(e - sNvEvt);

            if (sLast[slot] != p) {
                sLast[slot] = p;
                printf("NVEVT slot %d +%d: state %d op %02x %02x %02x %02x\n", (int)slot, (int)(p - sNvEvtTable),
                       e->state, p[0], p[1], p[2], p[3]);
                fflush(stdout);
            }
        }

        switch (p[0]) {
            case 0xF6: // push, then wait as 0xF7
                if (d < 3) {
                    e->depth = ++d;
                }
                e->ip = p = p + 1;
                // fall through
            case 0xF7: // wait for the voice line (MSF_VOICE_PLAYING) and the message to end
                if (!NvSound_LinePlaying() &&
                    ((sNvPlay == NULL) || (Message_GetState(&sNvPlay->msgCtx) == TEXT_STATE_NONE))) {
                    e->ip = p + 1;
                    if (e->depth >= 0) {
                        e->depth--;
                    }
                }
                return;
            case 0xF8: // wait n frames
                if (d < 3) {
                    e->depth = ++d;
                }
                e->counter[d] = (s16)(p[2] | (p[3] << 8));
                e->ip = p = p + 1;
                // fall through
            case 0xF9:
                if (--e->counter[d] <= 0) {
                    e->ip = p + 3;
                    e->depth--;
                }
                return;
            case 0xFA: // loop head
                if (d < 3) {
                    e->depth = ++d;
                }
                e->counter[d] = (s16)(p[2] | (p[3] << 8));
                e->ip = p + 4;
                e->ret[d] = e->ip;
                continue;
            case 0xFB: // loop tail
                if ((d < 0) || (--e->counter[d] <= 0)) {
                    e->ip = p + 1;
                    e->depth--;
                } else {
                    e->ip = e->ret[d];
                }
                continue;
            case 0xFC: // the test for 0xFD
                if (d < 3) {
                    e->depth = ++d;
                }
                e->call[d] = p + 2;
                e->ip = p + p[1];
                e->ret[d] = e->ip;
                continue;
            case 0xFD: // while the test holds, back to the loop
                if (d < 0) {
                    e->ip = p + 1;
                    continue;
                }
                sNvIp = e->call[d];
                if (getenv("NV_EVT_TRACE") && (sNvIp[0] == 0x04)) {
                    static u32 sLastW;
                    static s32 sLastF;

                    if ((sLastW != sNvFlags[4][1]) || (sLastF != (s32)sNvPlay->gameplayFrames - 2)) {
                        printf("NVEVT test %02x %02x %02x %02x: bank4[1] %08X f=%d\n", sNvIp[0], sNvIp[1], sNvIp[2],
                               sNvIp[3], sNvFlags[4][1], (int)sNvPlay->gameplayFrames);
                    }
                    sLastW = sNvFlags[4][1];
                    sLastF = (s32)sNvPlay->gameplayFrames;
                }
                if (NvScd_Cmd() == 0) {
                    e->ip = p + 1;
                    e->depth--;
                } else {
                    e->ip = e->ret[d];
                }
                continue;
            case 0xFE:
                e->ip = p + 1;
                return;
            case 0xFF:
                e->active = 0;
                e->ip = p + 1;
                return;
        }
        if (e->state == 0) {
            switch (p[0]) {
                case 0x00:
                    e->ip = p + 1;
                    break;
                case 0x01:
                    e->state = 1;
                    e->ip = p + 1;
                    break;
                case 0x02:
                case 0x03: {
                    NvRe1Entity* t = NvEvt_Ent(e);

                    e->state = 2;
                    e->ip = p + 1;
                    if ((p[0] == 0x02) && (t != NULL)) {
                        t->ignore_player_flag = 2;
                        t->action_behavior = 0;
                        t->action_state = 0;
                    }
                    break;
                }
                case 0x04: // the character it drives
                    e->entKind = p[1];
                    e->entIdx = p[2];
                    e->ip = p + 3;
                    break;
                case 0x05:
                    e->ip = p + 3;
                    NvEvt_Create(p[1], p[2]);
                    break;
                case 0x06: // an inline block
                    e->ip = p + p[1];
                    NvScd_Run(p + 2);
                    break;
                case 0x07: // one command
                    e->ip = p + p[1];
                    sNvIp = p + 2;
                    NvScd_Cmd();
                    break;
                case 0x08:
                    NvEvt_Init(e, p[1]);
                    return;
                case 0x09:
                    if (p[1] < NV_EVT_SLOTS) {
                        sNvEvt[p[1]].active = 0;
                    }
                    e->ip = p + 2;
                    break;
                default:
                    e->active = 0;
                    return;
            }
            if (NvMovie_Playing()) {
                return; // the movie plays before anything else runs
            }
        } else if (e->state == 1) {
            s32 n = NvEvt_Anim(e, p);

            if (n == 0) {
                e->active = 0;
                return;
            }
            e->ip = p + n;
        } else {
            s32 n = NvEvt_Move(e, p);

            if (n == 0) {
                e->active = 0;
                return;
            }
            e->ip = p + n;
            if (p[0] == 0x01) {
                e->state = 0;
            }
        }
    }
}

/* ---- the room ---- */

/** Play_Init, once the room is in: RE1's room_set / room_init. */
/* ---- examine spots: the room action table's message entries ---- */

#define NV_TEXT_ID 0xA1E0    // the text box RE1's messages are shown in (NvMessages.cpp)

static const char* sNvMsgText;

/** The room's message n ("" for none). */
static const char* NvMsg_Get(s32 n) {
    u32 count;
    u32 off;

    if (sNvMsg == NULL) {
        return "";
    }
    memcpy(&count, sNvMsg + 4, sizeof(count));
    if ((n < 0) || ((u32)n >= count)) {
        return "";
    }
    memcpy(&off, sNvMsg + 8 + 4 * n, sizeof(off));
    return (const char*)sNvMsg + 8 + 4 * count + off;
}

/** The text NvMessages.cpp shows for NV_TEXT_ID. */
/** Our own line in OoT's text box (Navi's blue), e.g. a door to a room not built yet (z_nv_door.c). */
void NvMsg_Show(PlayState* play, const char* text) {
    static char sLine[160];

    snprintf(sLine, sizeof(sLine), "%s", text);
    sNvMsgText = sLine;
    NvMsg_EnsureHook();
    Message_StartTextbox(play, NV_TEXT_ID, NULL);
}

const char* NvMsg_PendingText(void) {
    return sNvMsgText;
}


/**
 * check_action_object (0x0041c150): the action press probes the point 600 units in front of the
 * player (the player's own position for entries flagged 0x40) against every entry flagged 0x01
 * and 0x80, in table order; the first one that holds it is the one acted on. -1: none.
 */
static s32 NvAot_Probe(Player* player) {
    s32 px = (s32)lroundf(player->actor.world.pos.x / NV_RE1_SCALE);
    s32 pz = (s32)lroundf(-player->actor.world.pos.z / NV_RE1_SCALE);
    s32 angle = (0x400 - player->actor.shape.rot.y / 16) & 0xFFF; // (OoT yaw = 0x4000 - 16 angle)
    f32 a = angle * (3.14159265f / 2048.0f);
    // MovePlayerXZ(angle, (600, 0, 0)): RE1's forward is +x turned by RotMatrixY
    s32 fx = px + (s32)lroundf(600.0f * cosf(a));
    s32 fz = pz - (s32)lroundf(600.0f * sinf(a));
    s32 i;

    if (sNvMain == NULL) {
        return -1;
    }
    for (i = 0; i < sNvAotCount; i++) {
        const NvAot* e = &sNvAot[i];
        s32 x = (e->flags & 0x40) ? px : fx;
        s32 z = (e->flags & 0x40) ? pz : fz;

        if ((e->handler == 0) || !(e->flags & 0x01) || !(e->flags & 0x80)) {
            continue;
        }
        // is_point_in_action_zone: unsigned compares, so a point left of or above the zone fails
        if (((u32)(x - e->zone[0]) <= e->zone[2]) && ((u32)(z - e->zone[1]) <= e->zone[3])) {
            return i;
        }
    }
    return -1;
}

/** Item i of the room action table, if it's an item still lying in the room: its model, its
 * parent (0xFF: none) and position (z_nv_items.c draws it). */
s32 NvAot_Item(s32 i, s32* model, s32* parent, s16* pos) {
    const NvAot* a;

    if ((i < 0) || (i >= sNvAotCount)) {
        return false;
    }
    a = &sNvAot[i];
    if (!a->isItem || (a->handler == 0) || sNvItemHidden[i]) {
        return false;
    }
    *model = a->model;
    *parent = a->parent;
    memcpy(pos, a->pos, sizeof(a->pos));
    return true;
}

/** Room object i's parent, position and rotation, if the scripts placed it. */
s32 NvObj_Placed(s32 i, s32* parent, s16* pos, s16* rot) {
    NvObj* o;

    if ((i < 0) || (i >= NV_OBJ_SLOTS) || !sNvObj[i].set) {
        return false;
    }
    o = &sNvObj[i];
    *parent = o->parent;
    memcpy(pos, o->pos, sizeof(o->pos));
    memcpy(rot, o->rot, sizeof(o->rot));
    return true;
}

/** As NvObj_Placed, and only if it's drawn (render_room_objects: bit 0 of its flags). */
s32 NvObj_Get(s32 i, s32* parent, s16* pos, s16* rot) {
    return NvObj_Placed(i, parent, pos, rot) && (sNvObj[i].flags & 1);
}

/** The item box's lid: object i's z turn (check_itembox_state's +0x76). */
void NvObj_SetRotZ(s32 i, s16 z) {
    if ((i >= 0) && (i < NV_OBJ_SLOTS)) {
        sNvObj[i].rot[2] = z;
    }
}

/** The A button's label for what A would do here, or -1 (z_nv_door.c, after doors and stairs). */
s32 NvAot_DoAction(PlayState* play, Player* player) {
    s32 i = NvAot_Probe(player);

    if (i < 0) {
        return -1;
    }
    switch (sNvAot[i].handler) {
        case 2:    // a message
        case 4:    // an item
        case 0x0F: // a map
        case 0x0D: // a document
        case 9:    // a room event (something to look at that sets one off)
            return DO_ACTION_CHECK;
        case 8: // the item box
            return DO_ACTION_OPEN;
        default:
            return -1;
    }
}

static char sNvPrompt[160];
static s32 sNvPickup = -1; // the item spot whose prompt is open

/** The pickup's question (RE1's shared message 0, "Will you take the ...?", else our own), then yes / no. */
static void NvPickup_Prompt(s32 item) {
    if (NvText_Get(0, 0) != NULL) {
        snprintf(sNvPrompt, sizeof(sNvPrompt), "%s\x1B%%gYes&No%%w", NvMsg_Text(0x40, item));
    } else {
        snprintf(sNvPrompt, sizeof(sNvPrompt), "Take the %%r%s%%w?\x1B%%gYes&No%%w", NvItems_Name(item));
    }
}

/**
 * The pickup prompt's answer (RE1: message 0xC0's yes/no, then handle_message_post_action's
 * room_event_take_item). Yes: the item goes to the slots and leaves the room, or, with no room
 * for it, stays where it lies with RE1's refusal. Each frame, from NvScript_Update.
 */
static void NvPickup_Update(PlayState* play) {
    NvAot* a;

    if (sNvPickup < 0) {
        return;
    }
    if (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE) {
        sNvPickup = -1;
        return;
    }
    if ((Message_GetState(&play->msgCtx) != TEXT_STATE_CHOICE) || !Message_ShouldAdvance(play)) {
        return;
    }
    a = &sNvAot[sNvPickup];
    sNvPickup = -1;
    if (play->msgCtx.choiceIndex != 0) {
        Message_CloseTextbox(play);
        return;
    }
    if (!NvItems_Give(a->item, a->qty)) {
        snprintf(sNvPrompt, sizeof(sNvPrompt), "%s", (NvText_Get(0, 2) != NULL) ? NvText_Get(0, 2) : "You can't carry any more.");
        sNvMsgText = sNvPrompt;
        Message_ContinueTextbox(play, NV_TEXT_ID);
        return;
    }
    NvItems_SetTaken(a->itemFlag);
    NvFx_Kill(a->sparkle); // (its glint goes with it)
    a->sparkle = 0xFF;
    a->handler = 0;
    sNvPickedItem = a->item; // (cmd_picked_item_test)
    printf("NVITEM took %02X x%d (flag %d)\n", a->item, a->qty, a->itemFlag);
    NvItems_Log();
    Message_CloseTextbox(play);
}

/**
 * The action press, when no door or stairs took it (z_nv_door.c): the entry's handler, as
 * room_check_actions runs it. So far display_msg_room_action (2): the room's message, in OoT's
 * text box. True when the press was taken.
 */
s32 NvFiles_Take(s32 id);
void NvBox_Open(s32 lid);
void NvBox_Update(PlayState* play);
void NvBox_Reset(void);

/**
 * cmd_room_action / cmd_got_item: room action entry i's record run through handler (the script's
 * choice, not necessarily the entry's own): 2 its message, 4 / 0x0F an item offered (got_item's
 * "take it?"), 0x0D a document taken and read, 7 its flag, 8 the item box, 9 its event.
 */
static void NvAot_Run(s32 i, s32 handler) {
    NvAot* a;

    if ((i < 0) || (i >= NV_AOT_SLOTS) || (sNvPlay == NULL)) {
        return;
    }
    a = &sNvAot[i];
    printf("NVAOT script runs entry %d as handler %d\n", (int)i, (int)handler);
    fflush(stdout);
    switch (handler) {
        case 2:
            NvScd_Message(a->param[0] & 0x7F, a->param[1]);
            break;
        case 4:
        case 0x0F:
            if ((Message_GetState(&sNvPlay->msgCtx) == TEXT_STATE_NONE) && (a->item != 0)) {
                NvPickup_Prompt(a->item);
                sNvMsgText = sNvPrompt;
                sNvPickup = i;
                NvMsg_EnsureHook();
                Message_StartTextbox(sNvPlay, NV_TEXT_ID, NULL);
            }
            break;
        case 0x0D:
            if (a->item != 0) {
                sNvPickedItem = a->item;
                NvItems_SetTaken(a->itemFlag);
    NvFx_Kill(a->sparkle); // (its glint goes with it)
    a->sparkle = 0xFF;
                a->handler = 0;
                NvFiles_Take(a->item);
            }
            break;
        case 7:
            NvAot_FlagSet(a);
            break;
        case 8:
            NvBox_Open(a->param[1]);
            break;
        case 9:
            NvEvt_Create(a->param[0] & 0xFF, a->param[1] & 0xFF);
            break;
    }
}

// RE1 plays her interaction animation (action_behavior 0xC) after an action-button room action,
// with no input until it ends; by then the event it set off has changed the entry (210's Moon
// Crest: its event, pressed twice, gave two). Link has no such animation: no action press for
// as long (about a second)
#define NV_ACT_LOCK_FRAMES 20
static u32 sNvActFrame;

s32 NvAot_Act(PlayState* play, Player* player) {
    s32 i;
    u16 id;

    if ((play->gameplayFrames - sNvActFrame) < NV_ACT_LOCK_FRAMES) {
        return false;
    }
    i = NvAot_Probe(player);
    if (i < 0) {
        return false;
    }
    sNvActFrame = play->gameplayFrames;
    if ((sNvAot[i].handler == 4) || (sNvAot[i].handler == 0x0F)) { // an item or a map: take it?
        NvPickup_Prompt(sNvAot[i].item);
        sNvMsgText = sNvPrompt;
        sNvPickup = i;
        printf("NVITEM spot %d: %02X x%d\n", (int)i, sNvAot[i].item, sNvAot[i].qty);
        fflush(stdout);
        NvMsg_EnsureHook();
        Message_StartTextbox(play, NV_TEXT_ID, NULL);
        return true;
    }
    if (sNvAot[i].handler == 9) { // create_room_event: the entry's event (70C's rematch, 210's hole)
        printf("NVEVT spot %d: event %d in slot %d\n", (int)i, sNvAot[i].param[1] & 0xFF, sNvAot[i].param[0] & 0xFF);
        fflush(stdout);
        NvEvt_Create(sNvAot[i].param[0] & 0xFF, sNvAot[i].param[1] & 0xFF);
        return true;
    }
    if (sNvAot[i].handler == 0x0E) { // check_desk: param 0 its lock (bank 2), 1 its item's entry, 2 its camera
        NvAot* d = &sNvAot[i];
        s32 k = d->param[1];
        static char sAsk[300];

        if ((k >= NV_AOT_SLOTS) || !sNvAot[k].isItem || (sNvAot[k].handler == 0) ||
            !NvFlags_Test(7, sNvAot[k].itemFlag)) {
            return false; // (the desk's emptied: nothing happens)
        }
        if (!NvFlags_Test(2, d->param[0])) { // locked
            if (NvDesk_Key() == 0) {
                return NvMsg_Start(play, 0xD8, 0, 0xFF); // "it's locked"
            }
            snprintf(sAsk, sizeof(sAsk), "%s\x1B%%gYes&No%%w", NvMsg_Text(0xD9, NvDesk_Key()));
            sNvMsgText = sAsk;
            sNvChoice = 0;
            sNvAsk = true;
            sNvDeskAsk = i;
            printf("NVDESK spot %d: locked (lock %d), asks to open it\n", (int)i, d->param[0]);
            fflush(stdout);
            NvMsg_EnsureHook();
            Message_StartTextbox(play, NV_TEXT_ID, NULL);
            return true;
        }
        { // unlocked: the lid, the desk's camera, then its item offered as any other
            void NvSnd_At(s32 slot, s32 x, s32 z, s32 lx, s32 lz);
            void NvCamera_CutLock(s32 n);

            NvSnd_At(0x24, 0, 0, 0, 0);
            NvCamera_CutLock(d->param[2]);
            sNvDeskCam = true;
            NvPickup_Prompt(sNvAot[k].item);
            sNvMsgText = sNvPrompt;
            sNvPickup = k;
            printf("NVDESK spot %d: open, offers %02X (camera %d)\n", (int)i, sNvAot[k].item, d->param[2]);
            fflush(stdout);
            NvMsg_EnsureHook();
            Message_StartTextbox(play, NV_TEXT_ID, NULL);
            return true;
        }
    }
    if (sNvAot[i].handler == 0x10) { // check_typewriter: the run's save (z_nv_save.c)
        void NvSave_Typewriter(PlayState * play);

        NvSave_Typewriter(play);
        return true;
    }
    if (sNvAot[i].handler == 7) { // flag_bank_set on the action press
        NvAot_FlagSet(&sNvAot[i]);
        return true;
    }
    if (sNvAot[i].handler == 8) { // the item box: open_itembox (its lid: the entry's +4, param 1)
        printf("NVBOX spot %d: lid object %d\n", (int)i, sNvAot[i].param[1]);
        fflush(stdout);
        {
            void NvSnd_At(s32 slot, s32 x, s32 z, s32 lx, s32 lz);

            NvSnd_At(0x20, 0, 0, 0, 0); // RE1's lid (open_itembox: play_sfx(2, 0x20), the room bank)
        }
        NvBox_Open(sNvAot[i].param[1]);
        return true;
    }
    if (sNvAot[i].handler == 0x0D) { // a document: RE1 takes it and reads it at once
        NvAot* a = &sNvAot[i];

        printf("NVFILE spot %d: %02X\n", (int)i, a->item);
        fflush(stdout);
        sNvPickedItem = a->item;
        NvItems_SetTaken(a->itemFlag);
    NvFx_Kill(a->sparkle); // (its glint goes with it)
    a->sparkle = 0xFF;
        a->handler = 0;
        NvFiles_Take(a->item);
        return true;
    }
    if (sNvAot[i].handler != 2) {
        return false;
    }
    id = sNvAot[i].param[0];
    printf("NVMSG spot %d: message %d\n", (int)i, id & 0x3F);
    fflush(stdout);
    return NvMsg_Start(play, id, 0, sNvAot[i].param[1]); // (its own pause word; bit 0x40: the executable's shared messages)
}

/** cmd_message_set: room message id in OoT's box (as an examine spot's). */
/* ---- voice lines ---- */

/** voice_load_and_play: the stage's line `id` (re1/voice_names: the game's own table, re1/tools/soh_sound.py). */
static void NvVoice_Play(s32 id) {
    static const u8* sNames;
    static s32 sLoaded;
    const u8* d;
    s32 row = (sNvStage >= 5) ? sNvStage - 5 : sNvStage; // (stages 6, 7 are the mansion's and the guardhouse's again)
    s32 r;
    char name[10];

    if (!sLoaded) {
        sLoaded = true;
        if (ResourceMgr_FileExists("re1/voice_names")) {
            sNames = (const u8*)ResourceGetDataByName("re1/voice_names");
            if ((sNames != NULL) && (memcmp(sNames, "RVNM", 4) != 0)) {
                sNames = NULL;
            }
        }
    }
    if (sNames == NULL) {
        static s32 sTold;

        if (!sTold) {
            sTold = true;
            printf("NVVOICE no voice names (the converter needs the game's .exe: --exe)\n");
            fflush(stdout);
        }
        return;
    }
    if (row >= *(const u16*)(sNames + 4)) {
        return;
    }
    d = sNames + 6;
    for (r = 0; r < row; r++) {
        d += 2 + 9 * *(const u16*)d;
    }
    if (id >= *(const u16*)d) {
        return;
    }
    memcpy(name, d + 2 + 9 * id, 9);
    name[9] = '\0';
    if (name[0] == '\0') {
        return;
    }
    if (!NvSound_Line(name) || getenv("NV_LOG")) {
        printf("NVVOICE stage %d line %d: %s\n", (int)sNvStage + 1, (int)id, name);
        fflush(stdout);
    }
}

const char* NvText_Get(s32 kind, s32 i);

/**
 * Message id's text: bit 0x40 the executable's shared messages (re1/exe_text: the prompts, the
 * locked doors...), else the room's own; RE1's "the item in question" ({item}) filled in with
 * item's name. "" for none.
 */
static const char* NvMsg_Text(s32 id, s32 item) {
    static char sBuf[400];
    const char* src = (id & 0x40) ? NvText_Get(0, id & 0x3F) : NvMsg_Get(id & 0x3F);
    const char* at;
    const char* name = (item > 0) ? NvItems_Name(item) : "it";
    char* yn;

    sNvMsgYn = false;
    if (src == NULL) {
        return "";
    }
    at = strstr(src, "{item}");
    if (at == NULL) {
        if (strstr(src, "{yn}") == NULL) {
            return src;
        }
        snprintf(sBuf, sizeof(sBuf), "%s", src);
    } else {
        snprintf(sBuf, sizeof(sBuf), "%.*s%s%s", (int)(at - src), src, name, at + 6);
    }
    yn = strstr(sBuf, "{yn}"); // (RE1's yes / no after it: re1/tools/soh_scene.py decode_message)
    if (yn != NULL) {
        *yn = '\0';
        sNvMsgYn = true;
    }
    return sBuf;
}

/**
 * A room's message in the text box, as cmd_message_set and the examine spots show it: with RE1's
 * yes / no when the message ends in one, the answer kept for the room's scripts (state byte 5).
 */
static s32 NvMsg_Start(PlayState* play, s32 id, s32 item, u32 pause) {
    static char sAsk[440];
    const char* t = NvMsg_Text(id, item);

    if (t[0] == '\0') {
        return false;
    }
    sNvChoice = 0;
    sNvAsk = sNvMsgYn;
    if (sNvMsgYn) {
        snprintf(sAsk, sizeof(sAsk), "%s\x1B%%gYes&No%%w", t);
        t = sAsk;
    }
    sNvMsgText = t;
    NvMsg_EnsureHook();
    Message_StartTextbox(play, NV_TEXT_ID, NULL);
    NvMf_Hold(pause);
    return true;
}

/** What she opens a desk with: the lockpick (Jill's), else the small key; 0 for neither. */
static s32 NvDesk_Key(void) {
    if (NvFlags_Test(0, 0x7C) || (NvItems_Find(0x31) >= 0)) { // (SCENARIO_FLAG_HAS_LOCKPICK: Barry's, in 106)
        return 0x31;
    }
    return (NvItems_Find(0x3D) >= 0) ? 0x3D : 0;
}

/** The question's answer, each frame (Rendering.cpp's message state 4: the cursor's side, then closed). */
static void NvAsk_Update(PlayState* play) {
    if (!sNvAsk) {
        return;
    }
    if (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE) {
        sNvAsk = false;
        return;
    }
    if ((Message_GetState(&play->msgCtx) == TEXT_STATE_CHOICE) && Message_ShouldAdvance(play)) {
        sNvChoice = (play->msgCtx.choiceIndex != 0) ? 1 : 0;
        sNvAsk = false;
        if (getenv("NV_LOG")) {
            printf("NVMSG answered %s\n", sNvChoice ? "no" : "yes");
            fflush(stdout);
        }
        if ((sNvDeskAsk >= 0) && (sNvChoice == 0)) { // check_desk_state 3: unlocked, "key turned"
            void NvSnd_At(s32 slot, s32 x, s32 z, s32 lx, s32 lz);
            NvAot* d = &sNvAot[sNvDeskAsk];

            sNvDeskAsk = -1;
            NvFlags_Op(2, d->param[0], 0);
            NvSnd_At(0x26, 0, 0, 0, 0);
            printf("NVDESK unlocked (lock %d)\n", d->param[0]);
            fflush(stdout);
            sNvMsgText = NvMsg_Text(0xC3, NvDesk_Key());
            Message_ContinueTextbox(play, NV_TEXT_ID);
            return;
        }
        sNvDeskAsk = -1;
        Message_CloseTextbox(play);
    }
}

/** For the interactive screens (z_nv_panel.c): message id in the text box, with RE1's yes / no after it. */
void NvScript_Ask(PlayState* play, s32 id) {
    static char sAsk[440];

    snprintf(sAsk, sizeof(sAsk), "%s\x1B%%gYes&No%%w", NvMsg_Text(id, 0));
    sNvMsgText = sAsk;
    NvMsg_EnsureHook();
    Message_StartTextbox(play, NV_TEXT_ID, NULL);
}

/** Message id (a room's, or with 0x40 the executable's shared ones) with an item's name for {item}. */
void NvScript_SayItem(PlayState* play, s32 id, s32 item) {
    NvMsg_Show(play, NvMsg_Text(id, item));
}

/** The room's message id in the message box (a screen's caption: z_nv_panel.c); false if it's empty. */
s32 NvScript_Say(PlayState* play, s32 id) {
    const char* t = NvMsg_Text(id, 0);

    if (t[0] == '\0') {
        return false;
    }
    NvMsg_Show(play, t);
    return true;
}

static void NvScd_Message(s32 id, u32 pause) {
    if ((sNvPlay == NULL) || (Message_GetState(&sNvPlay->msgCtx) != TEXT_STATE_NONE)) {
        return;
    }
    NvMsg_Start(sNvPlay, id, sNvPickedItem, pause);
}

void NvBgm_RoomEnter(s32 stage, s32 room);
void NvBgm_Leave(void);
void NvBgm_ScdPlay(s32 ch);
void NvBgm_ScdStop(s32 ch);
void NvBgm_ScdState(s32 stage, s32 room, u8 state);

/*
 * The way in: the mansion's own warp song (its notes from the basement theme's shape), played anywhere in free
 * play, warps into the foyer. It rides OoT's warp-song sequence under Nocturne of Shadow's slot
 * (the warp light, the fade), but only this song goes to the mansion: Nocturne itself warps to
 * the graveyard as ever. In place of a melody it plays a stretch of RE1's opening movie's sound
 * (the cast roll), from the player's own files. The first arrival plays the whole opening movie,
 * as before; "seen it" is a save flag (an event flag OoT and the randomizer leave unused).
 */
#define NV_WARP_SONG OCARINA_SONG_NOCTURNE // the slot it rides
#define NV_WARP_SEEN_INDEX (0xD5 >> 4)
#define NV_WARP_SEEN_MASK (1 << (0xD5 & 15))

// the notes: the opening phrase of the user's MIDI of the Dual Shock basement theme, its shape
// (a chromatic crawl, a dip, the crawl back, the leap) on the ocarina's five notes:
// C-Down, C-Right, A, C-Right, C-Left, A, C-Left, C-Up (none of OoT's songs is in it)
static const u8 sNvSongNotes[] = { OCARINA_BTN_C_DOWN, OCARINA_BTN_C_RIGHT, OCARINA_BTN_A,    OCARINA_BTN_C_RIGHT,
                                   OCARINA_BTN_C_LEFT, OCARINA_BTN_A,       OCARINA_BTN_C_LEFT, OCARINA_BTN_C_UP };

// the sound it plays: movie NV_SONG_CLIP_MOVIE (0, the opening) from NV_SONG_CLIP_START_MS for
// NV_SONG_CLIP_LENGTH_MS. Placeholder times: set them to the cast roll once it's been timed.
#define NV_SONG_CLIP_MOVIE 0
#define NV_SONG_CLIP_START_MS 0
#define NV_SONG_CLIP_LENGTH_MS 8000

s16 NvDoor_Entrance(const char* sceneFile, s32 spawn);
static s32 sNvWarpIn;     // a warp song into the mansion is on its way
static s32 sNvSongPlayed; // the mansion's song was just played (the warp sequence is running)
static u8 sNvSongHeard[8]; // the last notes played in free play, newest last
static void NvSong_Restore(void);

/** A note played in free play (z_message_PAL.c): true when it completes the mansion's song. */
s32 NvSong_Note(PlayState* play, u8 button) {
    s32 n = ARRAY_COUNT(sNvSongNotes);
    s32 k;

    sNvSongPlayed = false; // (a song played where OoT won't warp leaves nothing pending)
    NvSong_Restore();
    memmove(sNvSongHeard, sNvSongHeard + 1, sizeof(sNvSongHeard) - 1);
    sNvSongHeard[sizeof(sNvSongHeard) - 1] = button;
    if (getenv("NV_LOG")) {
        printf("NVSONG note %d (action %d)\n", (int)button, (int)play->msgCtx.ocarinaAction);
        fflush(stdout);
    }
    if (play->msgCtx.ocarinaAction != OCARINA_ACTION_FREE_PLAY) {
        return false;
    }
    for (k = 0; k < n; k++) {
        if (sNvSongHeard[sizeof(sNvSongHeard) - n + k] != sNvSongNotes[k]) {
            return false;
        }
    }
    if (NvDoor_Entrance("re1_106_scene", 0) < 0) { // no mansion in the mod files
        return false;
    }
    memset(sNvSongHeard, 0xFF, sizeof(sNvSongHeard));
    sNvSongPlayed = true;
    NvMsg_EnsureHook(); // its warp question (NvMessages.cpp), wherever it's played
    printf("NVSONG the mansion's song\n");
    fflush(stdout);
    return true;
}

/** The warp sequence is the mansion's song's (z_message_PAL.c plays its clip, not a melody). */
s32 NvSong_Pending(void) {
    return sNvSongPlayed;
}

/*
 * Its playback: OoT plays a correct song back from its note table (sOcarinaSongNotes), so while
 * the warp sequence runs ours stands in for Nocturne's there (the same feel as Nocturne's quick
 * run: 18 frames a note, the last held), and Nocturne's own notes go back once the warp starts.
 */
extern OcarinaNote sOcarinaSongNotes[OCARINA_SONG_MAX][20];
static OcarinaNote sNvSongSaved[20];
static s32 sNvSongSwapped;

static void NvSong_Restore(void) {
    if (sNvSongSwapped) {
        memcpy(sOcarinaSongNotes[NV_WARP_SONG], sNvSongSaved, sizeof(sNvSongSaved));
        sNvSongSwapped = false;
    }
}

static u8 NvSong_Pitch(u8 button) {
    switch (button) {
        case OCARINA_BTN_A:
            return OCARINA_PITCH_D4;
        case OCARINA_BTN_C_DOWN:
            return OCARINA_PITCH_F4;
        case OCARINA_BTN_C_RIGHT:
            return OCARINA_PITCH_A4;
        case OCARINA_BTN_C_LEFT:
            return OCARINA_PITCH_B4;
        default:
            return OCARINA_PITCH_D5;
    }
}

/** OoT's playback of the song, ours in Nocturne's slot (z_message_PAL.c, in place of its call). */
void NvSong_Playback(void) {
    OcarinaNote* n = sOcarinaSongNotes[NV_WARP_SONG];
    s32 count = ARRAY_COUNT(sNvSongNotes);
    s32 k;

    if (!sNvSongSwapped) {
        memcpy(sNvSongSaved, n, sizeof(sNvSongSaved));
        sNvSongSwapped = true;
    }
    for (k = 0; k < count; k++) {
        n[k].pitch = NvSong_Pitch(sNvSongNotes[k]);
        n[k].length = (k == count - 1) ? 90 : 18;
        n[k].volume = 86;
        n[k].vibrato = 0;
        n[k].bend = 0;
        n[k].bFlat4Flag = 0;
    }
    n[count].pitch = OCARINA_PITCH_NONE; // the end
    n[count].length = 0;
    n[count].volume = 86;
    n[count].vibrato = 0;
    n[count].bend = 0;
    n[count].bFlat4Flag = 0;
    AudioOcarina_SetPlaybackSong(NV_WARP_SONG + 1, 1);
}

/*
 * Its fanfare: the player's own arrangement (their MIDI, made into an OoT sequence by
 * re1/tools/midi2seq.py and packed in the mod as custom/music/.../clowns_farting_in_basement),
 * in place of Nocturne's, from the second time the song is played (the first, the opening movie
 * plays on arrival). Nothing when the mod has no arrangement.
 */
extern char** sequenceMap;
extern size_t sequenceMapSize;

static s32 NvSong_FanfareId(void) {
    size_t i;

    for (i = 0; i < sequenceMapSize + 0xF; i++) {
        if ((sequenceMap[i] != NULL) && (strstr(sequenceMap[i], "clowns_farting_in_basement") != NULL)) {
            return (s32)i;
        }
    }
    return -1;
}

void NvSong_Fanfare(void) {
    s32 id;

    if (!(gSaveContext.eventChkInf[NV_WARP_SEEN_INDEX] & NV_WARP_SEEN_MASK) && !getenv("NV_SONG_SEEN")) {
        return; // the first time: the opening movie on arrival instead (dev: NV_SONG_SEEN as if seen)
    }
    id = NvSong_FanfareId();
    if (id >= 0) {
        Audio_PlayFanfare((u16)id);
        printf("NVSONG fanfare %d\n", (int)id);
        fflush(stdout);
    }
}

/** Its sound in the warp, after the playback (the opening movie's stretch, until the arrangement). */
void NvSong_PlayClip(void) {
    void NvMovie_SoundClip(s32 id, u32 startMs, u32 lengthMs);

    NvMovie_SoundClip(NV_SONG_CLIP_MOVIE, NV_SONG_CLIP_START_MS, NV_SONG_CLIP_LENGTH_MS);
}

s16 NvSave_Enter(void);

s16 NvWarp_SongEntrance(s32 song, s16 entrance) {
    s16 foyer;

    if (song == -1) { // the dev boot's (NV_BOOT_ENTRANCE=warp): as if the song were played
        sNvSongPlayed = true;
        song = NV_WARP_SONG;
    }
    if ((song != NV_WARP_SONG) || !sNvSongPlayed) {
        return entrance;
    }
    sNvSongPlayed = false;
    NvSong_Restore(); // the playback's done: Nocturne's notes back
    foyer = NvDoor_Entrance("re1_106_scene", 0);
    if (foyer < 0) {
        return entrance;
    }
    sNvWarpIn = true;
    // the run's save, if there's one: back where it was written (z_nv_save.c); else a new run
    {
        s16 saved = NvSave_Enter();

        if (saved >= 0) {
            return saved;
        }
    }
    return foyer;
}

static void NvWarp_Arrive(PlayState* play) {
    if (!sNvWarpIn) {
        return;
    }
    sNvWarpIn = false;
    if ((play->sceneNum != SCENE_RE1_106) || (gSaveContext.eventChkInf[NV_WARP_SEEN_INDEX] & NV_WARP_SEEN_MASK)) {
        return;
    }
    gSaveContext.eventChkInf[NV_WARP_SEEN_INDEX] |= NV_WARP_SEEN_MASK;
    NvMovie_Start(0); // RE1's opening
}

/** The RE1 room loaded now (its number in its stage: RE1's g_roomId), for the enemies. */
s32 NvScript_RoomNum(void) {
    return sNvRoom;
}

void NvScript_RoomInit(PlayState* play) {
    NvFlags_NewGame();
    sNvLightState = -1; // (room_set: the special light off)
    if ((play->loadedScene != NULL) && (play->loadedScene->sceneFile.fileName != NULL)) {
        NvFx_RoomLoad(play->loadedScene->sceneFile.fileName); // (InitRoomEffSprite)
    }
    sNvLightDelta = 0;
    NvWarp_Arrive(play);
    if (play->sceneNum < SCENE_RE1_100) {
        NvBgm_Leave(); // out of the mansion: RE1's sounds stop
    }
    char path[256];
    const char* file;
    size_t len;
    u8* d;
    u32 lens[3];
    s32 i;

    sNvInit = sNvMain = sNvEvtTable = NULL;
    sNvPlay = play;
    for (i = 0; i < NV_EVT_SLOTS; i++) {
        sNvEvt[i].active = 0;
    }
    NvEnemy_ClearSlots();
    NvCast_RoomLoad();
    sNvUsedItem = sNvPickedItem = 0; // (room_set clears them)
    sNvSprOff = 0;
    if (getenv("NV_TEXT_TEST") != NULL) { // dev: the executable's text as the game shows it
        NvPickup_Prompt(0x1F);
        printf("NVTEXT prompt: %s\nNVTEXT carry: %s\nNVTEXT msg 8: %s\nNVTEXT name 0x41: %s\nNVTEXT desc 2: %s\n", sNvPrompt,
               NvMsg_Text(0x42, 0), NvMsg_Text(0x48, 0), NvItems_Name(0x41),
               (NvText_Get(2, 1) != NULL) ? NvText_Get(2, 1) : "-");
        fflush(stdout);
    }
    NvSound_LineStop(); // (a line cut off by the door)
    {
        void NvCamera_RoomLoad(void);

        NvCamera_RoomLoad();
    }
    memset(sNvAot, 0, sizeof(sNvAot));
    sNvAotCount = 0;
    // room_set: the system flags (bank 4, both words) start again with the room; a room's puzzle
    // in progress (117's switches, 4:1E) doesn't carry into the next
    sNvFlags[4][0] = sNvFlags[4][1] = 0;
    // (and the low nibble of both main state words: the mirror, the camera's deferral)
    sNvFlags[5][0] &= ~0xFu;
    sNvFlags[5][1] &= ~0xFu;
    // game_loop: nothing but the main script runs through the fade in; then all of it
    sNvFlags[6][0] = 0;
    sNvMfHeld = false;
    sNvMfFade = NV_MF_FADE_TICKS;
    sNvMfFirst = true;
    sNvDeskAsk = -1;
    sNvDeskCam = false;
    memset(sNvObj, 0, sizeof(sNvObj));
    NvBox_Reset();
    memset(sNvItemHidden, 0, sizeof(sNvItemHidden));
    sNvMsg = NULL;
    NvRe1_RoomLoad(play); // RE1's collision and walk zones, for its enemies (z_nv_zombie.c)
    NvItems_RoomLoad(play); // the room's item models (z_nv_items.c)
    gNvRe1Player.isBeingAttackedFlag = 0;
    sNvLastCamera = -1;
    if ((play->loadedScene == NULL) || (play->loadedScene->sceneFile.fileName == NULL)) {
        return;
    }
    file = play->loadedScene->sceneFile.fileName;
    len = strlen(file);
    if ((len <= 6) || (strcmp(file + len - 6, "_scene") != 0)) {
        return;
    }
    snprintf(path, sizeof(path), "scenes/shared/%s/%.*s_scd", file, (int)(len - 6), file);
    if (!ResourceMgr_FileExists(path)) {
        return;
    }
    d = (u8*)ResourceGetDataByName(path);
    if ((d == NULL) || (memcmp(d, "RSCD", 4) != 0)) {
        return;
    }
    // "re1_104_scene": stage 1 (RE1's g_stageId counts from 0), room 0x04
    {
        char hex[3] = { 0, 0, 0 };

        if (len >= 7) {
            hex[0] = file[5];
            hex[1] = file[6];
        }
        sNvStage = (len >= 7) ? (file[4] - '1') : 0;
        sNvRoom = (u8)strtol(hex, NULL, 16);
    }
    sNvCamera = GET_ACTIVE_CAM(play)->camDataIdx;
    snprintf(path, sizeof(path), "scenes/shared/%s/%.*s_msg", file, (int)(len - 6), file);
    if (ResourceMgr_FileExists(path)) {
        sNvMsg = (const u8*)ResourceGetDataByName(path);
        if ((sNvMsg != NULL) && (memcmp(sNvMsg, "RMSG", 4) != 0)) {
            sNvMsg = NULL;
        }
    }
    memcpy(lens, d + 4, sizeof(lens));
    sNvInit = d + 16;
    sNvMain = sNvInit + lens[0];
    sNvEvtTable = sNvMain + lens[1];
    if (getenv("NV_NO_ENEMIES") == NULL) {
        sNvInitRun = true;
        NvScd_Run(sNvInit);
        sNvInitRun = false;
    }
    // dev: NV_TEST_ENEMY="type,x,z[,behaviour]" (OoT units) places one more, held still (behaviour
    // 0x80) unless a behaviour is given (e.g. 2: a crawler), e.g. to see it against a mask
    if (getenv("NV_TEST_ENEMY") != NULL) {
        int type;
        int x;
        int z;
        int beh = 0x80;
        int slot = 15; // (15: no collider, to look at it; another slot to fight it)

        if (sscanf(getenv("NV_TEST_ENEMY"), "%d,%d,%d,%d,%d", &type, &x, &z, &beh, &slot) >= 3) {
            u8 rec[22] = { 0 };
            s16 rx = (s16)(x / 0.0217f);
            s16 rz = (s16)(-z / 0.0217f);

            rec[0] = 0x1B;
            rec[1] = type;
            rec[2] = beh;
            rec[3] = 0xFF;
            memcpy(rec + 12, &rx, 2);
            memcpy(rec + 16, &rz, 2);
            rec[0x12] = (u8)(slot & 0xF);
            {
                void NvEnemy_DevFree(s32 slot);

                NvEnemy_DevFree(slot & 0xF); // (it takes the slot from the room's own)
            }
            sNvInitRun = true;
            NvEnemy_ScdSet(play, rec);
            sNvInitRun = false;
        }
    }
    {
        void NvSnd_SetFootstepZones(const u8* d);

        snprintf(path, sizeof(path), "scenes/shared/%s/%.*s_ftzones", file, (int)(len - 6), file);
        NvSnd_SetFootstepZones(ResourceMgr_FileExists(path) ? (const u8*)ResourceGetDataByName(path) : NULL);
    }
    NvBgm_RoomEnter(sNvStage, sNvRoom); // RE1's room music (z_nv_sound.c), after the init chain set its state
    printf("NVSCRIPT %s init: stage %d room %d camera %d\n", file, sNvStage, sNvRoom, sNvCamera);
    fflush(stdout);
}

/** Play_Update: the main chain and the events, at RE1's tick rate. */
/**
 * The room actions the player sets off by being there (check_room_actions, each frame): every
 * entry flagged 0x01 but not 0x80 (the action press's), tested at the player's position (0x40)
 * or 600 in front of him. Handler 9 (create_room_event) starts event param[1] in slot param[0]:
 * 108's windows, the dogs coming in. (The other walk-in handlers aren't run yet.)
 */
/**
 * flag_bank_set (room action 7): the entry's flag set or cleared; its +2 the bank (0 the
 * scenario's, 1 its second, 2 the locks, 3 the enemies', 4 the system's, 7 the room items, 8 the
 * room's, anything else bank 9: the item-use flags, which game_loop clears every frame).
 */
static void NvAot_FlagSet(const NvAot* e) {
    s32 bank = e->param[0];

    if ((bank == 5) || (bank == 6)) {
        return; // (the main state and message flags: RE1's engine's own, not kept here)
    }
    if (bank > 8) {
        bank = 9;
    }
    NvFlags_Op(bank, e->param[1] & 0xFF, (e->param[2] != 0) ? 0 : 1);
    if ((bank == 9) && getenv("NV_LOG")) { // (once per item: where it can be used)
        static u8 sTold[64];
        s32 b = e->param[1] & 0x3F;

        if (!sTold[b]) {
            sTold[b] = 1;
            printf("NVAOT item %02X usable here (entry %d)\n", b + 0x1B, (int)(e - sNvAot));
            fflush(stdout);
        }
    }
}

static void NvAot_WalkIn(PlayState* play) {
    Player* player = GET_PLAYER(play);
    s32 px;
    s32 pz;
    s32 fx;
    s32 fz;
    f32 a;
    s32 i;

    if (player == NULL) {
        return;
    }
    px = (s32)lroundf(player->actor.world.pos.x / NV_RE1_SCALE);
    pz = (s32)lroundf(-player->actor.world.pos.z / NV_RE1_SCALE);
    a = ((0x400 - player->actor.shape.rot.y / 16) & 0xFFF) * (3.14159265f / 2048.0f);
    fx = px + (s32)lroundf(600.0f * cosf(a));
    fz = pz - (s32)lroundf(600.0f * sinf(a));
    for (i = 0; i < sNvAotCount; i++) {
        NvAot* e = &sNvAot[i];
        s32 x = (e->flags & 0x40) ? px : fx;
        s32 z = (e->flags & 0x40) ? pz : fz;

        if ((e->handler == 0) || !(e->flags & 0x01) || (e->flags & 0x80)) {
            continue;
        }
        if (((u32)(x - e->zone[0]) > e->zone[2]) || ((u32)(z - e->zone[1]) > e->zone[3])) {
            continue;
        }
        if (e->flags & 0x40) {
            sNvEntPosAction = (u8)(i + 1);
        } else {
            sNvFwdPosAction = (u8)(i + 1);
        }
        if (e->handler == 9) {
            printf("NVEVT walk-in %d: event %d in slot %d\n", (int)i, e->param[1] & 0xFF, e->param[0] & 0xFF);
            fflush(stdout);
            NvEvt_Create(e->param[0] & 0xFF, e->param[1] & 0xFF);
        } else if (e->handler == 7) {
            NvAot_FlagSet(e); // (bank 9: an item can be used here)
        }
    }
}

/** dev (z_nv_dev.c "aot:N"): room action entry i's zone (OoT units: x0, z0, x1, z1), its handler
 * and item; false if the entry isn't live. */
s32 NvAot_DevRect(s32 i, f32* r, s32* handler, s32* item) {
    const NvAot* a;

    if ((i < 0) || (i >= NV_AOT_SLOTS) || (sNvAot[i].handler == 0)) {
        return false;
    }
    a = &sNvAot[i];
    r[0] = a->zone[0] * NV_RE1_SCALE;
    r[2] = (a->zone[0] + a->zone[2]) * NV_RE1_SCALE;
    r[1] = -(a->zone[1] + a->zone[3]) * NV_RE1_SCALE;
    r[3] = -a->zone[1] * NV_RE1_SCALE;
    *handler = a->handler;
    *item = a->isItem ? a->item : -1;
    return true;
}

/** dev: the room's live action entries and placed objects, logged (to pick "aot:N" steps). */
void NvAot_DevList(void) {
    s32 i;

    for (i = 0; i < NV_OBJ_SLOTS; i++) {
        if (sNvObj[i].set) {
            printf("NVOBJLIST %d flags %02X at %d,%d,%d box %d,%d\n", (int)i, sNvObj[i].flags, sNvObj[i].pos[0],
                   sNvObj[i].pos[1], sNvObj[i].pos[2], sNvObj[i].ext[0], sNvObj[i].ext[2]);
        }
    }

    for (i = 0; i < NV_AOT_SLOTS; i++) {
        const NvAot* a = &sNvAot[i];

        if (a->handler != 0) {
            printf("NVAOTLIST %d handler %d flags %02X zone %u,%u %ux%u item %02X%s\n", (int)i, a->handler, a->flags,
                   a->zone[0], a->zone[1], a->zone[2], a->zone[3], a->item, a->isItem ? " (pickup)" : "");
        }
    }
    fflush(stdout);
}

/* ---- the room's objects: solid, pushed (RoomCollision.cpp update_room_objects) ---- */

/** For z_nv_block.c: object i's place and box, and whether it's solid now. False: it's gone. */
/** A room object's place and turn (an effect attached to it: z_nv_effects.c); false if it isn't placed. */
s32 NvObj_Pose(s32 i, s16* pos, s16* rot) {
    if ((i < 0) || (i >= NV_OBJ_SLOTS) || !sNvObj[i].set) {
        return false;
    }
    memcpy(pos, sNvObj[i].pos, sizeof(sNvObj[i].pos));
    memcpy(rot, sNvObj[i].rot, sizeof(sNvObj[i].rot));
    return true;
}

s32 NvObj_Box(s32 i, s16* pos, s16* ext, s32* solid, s32* climbable) {
    NvObj* o;

    if ((i < 0) || (i >= NV_OBJ_SLOTS) || !sNvObj[i].set || (sNvObj[i].ext[0] <= 0) || (sNvObj[i].ext[2] <= 0) ||
        (sNvObj[i].parent != 0xFF)) {
        return false; // (one carried by another object, an enemy or her: its place is its parent's, not boxed yet)
    }
    o = &sNvObj[i];
    memcpy(pos, o->pos, sizeof(o->pos));
    memcpy(ext, o->ext, sizeof(o->ext));
    *solid = (o->flags & 0x01) && !(o->flags & 0x0A);
    *climbable = o->top; // (the top only while she's on it: walking into a crate pushes it, as RE1's)
    return true;
}

#define NV_JILL_RADIUS 372 // her Sca_info: radius, half height
#define NV_JILL_HALF_H 1350

/** Whether an entity at (x, y, z) with radius r and half height h is inside object o's box. */
static s32 NvObj_Touches(const NvObj* o, s32 x, s32 y, s32 z, s32 r, s32 h, s32* dx, s32* dz) {
    s32 ex = o->ext[0] + r;
    s32 ey = o->ext[1] + h;
    s32 ez = o->ext[2] + r;
    s32 dy = o->pos[1] - y;

    *dx = o->pos[0] - x;
    *dz = o->pos[2] - z;
    return ((u32)(*dx + ex) <= (u32)(ex * 2)) && ((u32)(dy + ey) <= (u32)(ey * 2)) &&
           ((u32)(*dz + ez) <= (u32)(ez * 2));
}

/** ChkEntitySlide's response: the entity out of the box along the shallower way. */
static s32 NvObj_Slide(const NvObj* o, s32* x, s32 y, s32* z, s32 r, s32 h) {
    s32 dx;
    s32 dz;

    if (!NvObj_Touches(o, *x, y, *z, r, h, &dx, &dz)) {
        return false;
    }
    if (ABS((o->ext[0] + r) * dz) < ABS((o->ext[2] + r) * dx)) {
        *x = o->pos[0] + ((dx >= 0) ? -(o->ext[0] + r) : (o->ext[0] + r));
    } else {
        *z = o->pos[2] + ((dz >= 0) ? -(o->ext[2] + r) : (o->ext[2] + r));
    }
    return true;
}

/** Link in RE1's units, and his facing as RE1's angle (4096 a turn). */
static void NvObj_LinkRe1(Player* player, s32* x, s32* y, s32* z, s32* dir) {
    *x = (s32)lroundf(player->actor.world.pos.x / NV_RE1_SCALE);
    *y = (s32)lroundf(-player->actor.world.pos.y / NV_RE1_SCALE);
    *z = (s32)lroundf(-player->actor.world.pos.z / NV_RE1_SCALE);
    *dir = ((0x4000 - (s32)(u16)player->actor.shape.rot.y) / 16) & 0xFFF;
}

/**
 * check_climb_object: the action button on a climbable object (flag 0x40) in reach ahead, she
 * facing it squarely from its climbing side (within 299 of its facing turned round). She climbs
 * on: 1800 up and 1850 on (player_door_open_sequence's climb), the crate's top there for her.
 * With start false, whether she would (the A button's label).
 */
s32 NvObj_TryClimb(PlayState* play, Player* player, s32 start) {
    void NvClimb_Carry(Player* player, Vec3f* to);
    s32 px;
    s32 py;
    s32 pz;
    s32 dir;
    s32 i;

    NvObj_LinkRe1(player, &px, &py, &pz, &dir);
    for (i = NV_OBJ_SLOTS - 1; i >= 0; i--) {
        NvObj* o = &sNvObj[i];
        f32 a = player->actor.shape.rot.y * (M_PI / 32768.0f);
        s32 ax = px + (s32)(1100.0f * sinf(a));
        s32 az = pz - (s32)(1100.0f * cosf(a));
        s32 diff;

        if (!o->set || !(o->flags & 0x01) || !(o->flags & 0x40) || o->top || (o->ext[0] <= 0)) {
            continue;
        }
        if (((u32)(ax - o->pos[0] + o->ext[0]) > (u32)(o->ext[0] * 2)) ||
            ((u32)(az - o->pos[2] + o->ext[2]) > (u32)(o->ext[2] * 2))) {
            continue;
        }
        diff = (((dir + 0x800) & 0xFFF) - (o->rot[1] & 0xFFF)) & 0xFFF;
        if ((diff > 299) && (diff < 0xED5)) {
            continue;
        }
        if (start) {
            Vec3f to;
            s32 tx;
            s32 tz;

            // square to the crate's side, then up and on
            dir = (dir + 0x200) & 0xC00;
            tx = px + ((dir == 0) ? 1850 : (dir == 0x800) ? -1850 : 0);
            tz = pz + ((dir == 0x400) ? 1850 : (dir == 0xC00) ? -1850 : 0);
            to.x = tx * NV_RE1_SCALE;
            to.y = -(o->pos[1] - o->ext[1] * 2) * NV_RE1_SCALE;
            to.z = -tz * NV_RE1_SCALE;
            o->top = true;
            if (o->block != NULL) { // (its box rebuilt with its top)
                Actor_Kill(o->block);
                o->block = NULL;
            }
            player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw = (s16)(0x4000 - dir * 16);
            NvClimb_Carry(player, &to);
            printf("NVOBJ %d climbed onto (top %d)\n", (int)i, (int)(o->ext[1] * 2));
            fflush(stdout);
        }
        return true;
    }
    return false;
}

/** Room action entries probed by an object (mask 4: a statue on its spot), run for object o. */
static void NvObj_WalkIn(const NvObj* o) {
    s32 i;

    for (i = 0; i < sNvAotCount; i++) {
        NvAot* e = &sNvAot[i];

        if ((e->handler == 0) || !(e->flags & 0x04) || (e->flags & 0x80)) {
            continue;
        }
        if (((u32)(o->pos[0] - e->zone[0]) > e->zone[2]) || ((u32)(o->pos[2] - e->zone[1]) > e->zone[3])) {
            continue;
        }
        // (update_player_position run for the object, mask 4: its place counts as the entity's,
        // whatever the handler, 10 included: 202's balcony statue at the railing's gap)
        if (e->flags & 0x40) {
            sNvEntPosAction = (u8)(i + 1);
        } else {
            sNvFwdPosAction = (u8)(i + 1);
        }
        if (e->handler == 7) {
            NvAot_FlagSet(e);
        } else if (e->handler == 9) {
            NvEvt_Create(e->param[0] & 0xFF, e->param[1] & 0xFF);
        }
    }
}

/**
 * One tick of the room's objects: the enemies and Link pushed out of each solid one; Link leaning
 * on one (walking into it, within reach ahead, nine ticks on end) pushes it along that side, at
 * her push's pace, unless the room's walls or another object stop it; then the zones it reaches.
 * Flags (the record's byte 2): 0x01 there, 0x02 intangible, 0x04 no floor probe, 0x08 no collision,
 * 0x20 not pushable.
 */
static void NvObj_Update(PlayState* play) {
    s32 NvRe1_ObjBlocked(s32 x, s32 z, s32 ex, s32 ez);
    Player* player = GET_PLAYER(play);
    s32 px;
    s32 py;
    s32 pz;
    s32 moved = false;
    s32 pressing;
    s32 i;
    s32 k;

    NvFlags_Op(5, NV_MSF_OBJECT_PUSH, 1); // (raised again below while she's moving one)
    {
        s32 up = NvClimb_Active(); // (and while she's on a crate: 107's shelf is in reach from it)

        for (i = 0; i < NV_OBJ_SLOTS; i++) {
            up |= sNvObj[i].set && sNvObj[i].top;
        }
        NvFlags_Op(5, NV_MSF_ON_OBJECT, up ? 0 : 1);
    }
    if ((player == NULL) || (play->sceneNum < SCENE_RE1_100)) {
        return;
    }
    px = (s32)lroundf(player->actor.world.pos.x / NV_RE1_SCALE);
    py = (s32)lroundf(-player->actor.world.pos.y / NV_RE1_SCALE);
    pz = (s32)lroundf(-player->actor.world.pos.z / NV_RE1_SCALE);
    // walking into it: the stick held (RE1: forward on the pad) and Link not held by a scene
    pressing = (sqrtf(SQ((f32)play->state.input[0].rel.stick_x) + SQ((f32)play->state.input[0].rel.stick_y)) > 30.0f) &&
               (NvCast_Player()->state != 8);
    for (i = 0; i < NV_OBJ_SLOTS; i++) {
        NvObj* o = &sNvObj[i];
        s32 dx;
        s32 dz;
        s32 reach;

        if (!o->set || !(o->flags & 0x01) || (o->flags & 0x0A) || (o->ext[0] <= 0) || (o->ext[2] <= 0) ||
            (o->parent != 0xFF)) {
            continue;
        }
        for (k = 0; k < 16; k++) { // the enemies out of it
            NvRe1Entity* en = NvEnemy_SlotEntity(k);

            if ((en != NULL) && (en->health >= 0)) {
                s32 r = ((en->sca != NULL) && (en->sca[5] > 0)) ? en->sca[5] : 400;

                NvObj_Slide(o, &en->t[0], en->t[1], &en->t[2], r, 1500);
            }
        }
        // the push probe: touching it, leaning on it, the point 470 ahead of her inside its box
        {
            f32 a = player->actor.shape.rot.y * (M_PI / 32768.0f);
            // (RE1's 470 ahead, plus where OoT's walls stop Link: further out than her radius, about
            // 1060 from the box's face; and a step or two of the box moving off before he follows,
            // or a push from his own standing spot stalls after its first step: 205's armors)
            s32 ax = px + (s32)(1250.0f * sinf(a));
            s32 az = pz - (s32)(1250.0f * cosf(a));

            reach = ((u32)(ax - o->pos[0] + o->ext[0]) <= (u32)(o->ext[0] * 2)) &&
                    ((u32)(az - o->pos[2] + o->ext[2]) <= (u32)(o->ext[2] * 2));
        }
        if (getenv("NV_PUSHLOG") && ((play->gameplayFrames % 4) == 0) && (SQ(px - o->pos[0]) + SQ(pz - o->pos[2]) < SQ(2500))) {
            printf("NVPUSH obj %d at %d,%d her %d,%d (link %.1f,%.1f) touch %d press %d reach %d push %d\n", (int)i,
                   o->pos[0], o->pos[2], px, pz, player->actor.world.pos.x, player->actor.world.pos.z,
                   NvObj_Touches(o, px, py, pz, NV_JILL_RADIUS + 950, NV_JILL_HALF_H, &dx, &dz), pressing, reach, o->push);
            fflush(stdout);
        }
        if (NvObj_Touches(o, px, py, pz, NV_JILL_RADIUS + 950, NV_JILL_HALF_H, &dx, &dz) && pressing && reach &&
            !o->top) {
            o->push++;
        } else {
            o->push = 0;
            o->pushTick = 0;
        }
        if (!(o->flags & 0x20) && (o->push >= 9)) {
            s32 nx = o->pos[0];
            s32 nz = o->pos[2];
            s32 step = (o->pushTick < 16) ? 50 : 0; // (her push: 16 ticks of moving, then she sets again)
            s32 veto = false;

            o->pushTick = (o->pushTick + 1) % 30;
            if (ABS((o->ext[0] + NV_JILL_RADIUS) * dz) < ABS((o->ext[2] + NV_JILL_RADIUS) * dx)) { // (the side she's on)
                nx += (dx >= 0) ? step : -step;
            } else {
                nz += (dz >= 0) ? step : -step;
            }
            // (the object's own floor probe; flag 0x04: it never needs one, update_room_objects)
            if (!(o->flags & 0x04) && NvRe1_ObjBlocked(nx, nz, o->ext[0], o->ext[2])) {
                veto = true;
            }
            for (k = 0; (k < NV_OBJ_SLOTS) && !veto; k++) { // another object in the way
                const NvObj* q = &sNvObj[k];

                if ((k != i) && q->set && (q->flags & 0x01) && !(q->flags & 0x08) &&
                    (ABS(q->pos[0] - nx) < q->ext[0] + o->ext[0]) && (ABS(q->pos[2] - nz) < q->ext[2] + o->ext[2])) {
                    veto = true;
                }
            }
            if (veto) {
                o->push = 10; // (parked until she lets go)
            } else {
                void NvPlayer_StartPush(PlayState* play, Actor* block);

                if (step != 0) {
                    o->pos[0] = nx;
                    o->pos[2] = nz;
                }
                NvFlags_Op(5, NV_MSF_OBJECT_PUSH, 0); // (MSF_OBJECT_PUSH: 205's armour waits on it)
                NvPlayer_StartPush(play, o->block); // (Link's own pushing, z_player.c)
            }
            if (getenv("NV_LOG") && (o->pushTick == 1)) {
                printf("NVOBJ %d pushed to %d,%d%s\n", (int)i, o->pos[0], o->pos[2], veto ? " (blocked)" : "");
                fflush(stdout);
            }
        }
        // off the crate's top once she's left it (it's a box she pushes again)
        if (o->top && !NvClimb_Active() &&
            (((u32)(px - o->pos[0] + o->ext[0] + 200) > (u32)(o->ext[0] * 2 + 400)) ||
             ((u32)(pz - o->pos[2] + o->ext[2] + 200) > (u32)(o->ext[2] * 2 + 400)) ||
             (py > o->pos[1] - o->ext[1] * 2 + 300))) {
            o->top = false;
            if (o->block != NULL) {
                Actor_Kill(o->block);
                o->block = NULL;
            }
        }
        // she can't walk through it: its collision box (z_nv_block.c), spawned once per object
        if ((o->block == NULL) && (o->flags & 0x01)) {
            s32 NvActor_BlockId(void);

            o->block = Actor_Spawn(&play->actorCtx, play, NvActor_BlockId(), 0.0f, 0.0f, 0.0f, 0, 0, 0, i);
        }
        NvObj_WalkIn(o);
    }
    if (moved) {
        player->actor.world.pos.x = px * NV_RE1_SCALE;
        player->actor.world.pos.z = -pz * NV_RE1_SCALE;
    }
}

void NvPanel_Update(PlayState* play);
void NvFx_ScdSpawn(const u8* p);
void NvFx_ScdClearTyped(u8 type, u8 group);
void NvFx_ScdClearAll(void);
void NvFx_Tick(PlayState* play);
void NvFx_RoomLoad(const char* sceneFile);
void NvSave_Update(PlayState* play);
void NvCountdown_Tick(PlayState* play);

void NvScript_Update(PlayState* play) {
    extern s32 gNvSunSongSeen;
    s32 ticks;
    s32 t;
    s32 i;

    // the Sun's Song reached the room's enemies last frame (z_nv_enemy.c): it's spent, as
    // En_Rd ends it once its ReDeads have it
    if (gNvSunSongSeen && (gSaveContext.sunsSongState == SUNSSONG_SPECIAL)) {
        gSaveContext.sunsSongState = SUNSSONG_INACTIVE;
    }
    gNvSunSongSeen = false;
    NvPickup_Update(play); // an item prompt's answer
    NvAsk_Update(play);    // a room message's yes / no
    NvMf_Update(play);     // what a message holds still, and the pad
    if (sNvDeskCam && (sNvPickup < 0) && (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE)) {
        void NvCamera_CutRestore(void);

        sNvDeskCam = false; // check_desk_state 4: back to the room's camera
        NvCamera_CutRestore();
    }
    NvSave_Update(play);   // the typewriter's answer, a loaded save's spot (z_nv_save.c)
    NvPanel_Update(play);  // a screen the room's script put up (z_nv_panel.c)
    if ((play->sceneNum >= SCENE_RE1_100) && !NvDoorScene_Covering()) { // off the boxes its scripts moved
        s32 NvRe1_PushPlayer(s32* x, s32* z);
        Player* player = GET_PLAYER(play);
        s32 x = (s32)lroundf(player->actor.world.pos.x / NV_RE1_SCALE);
        s32 z = (s32)lroundf(-player->actor.world.pos.z / NV_RE1_SCALE);

        if (NvRe1_PushPlayer(&x, &z)) {
            player->actor.world.pos.x = x * NV_RE1_SCALE;
            player->actor.world.pos.z = -z * NV_RE1_SCALE;
        }
    }
    if (!NvDoorScene_Covering() && !NvMovie_Playing() && (play->pauseCtx.state == 0) && (play->sceneNum >= SCENE_RE1_100)) {
        void NvPoison_Update(PlayState* play);

        NvPoison_Update(play); // RE1's poison wearing her down (z_nv_items.c)
    }
    NvBox_Update(play);    // the item box's lid
    if ((sNvMain == NULL) || (sNvPlay != play) || NvDoorScene_Covering() || NvMovie_Playing() ||
        (play->transitionTrigger != TRANS_TRIGGER_OFF) || (getenv("NV_NO_ENEMIES") != NULL)) {
        return;
    }
    ticks = (sNvTickPhase ^= 1) ? 1 : 2;
    for (t = 0; t < ticks; t++) {
        s32 cam = GET_ACTIVE_CAM(play)->camDataIdx;

        // game_loop's per-frame resets: the item-use flags (bank 9: the room's script raises an
        // item's bit while it can be used where she stands) and the dice the scripts roll (state
        // word 3, g_RandSeed)
        {
            // (before the item-use flags are cleared: the status screen's Use sees last tick's, as RE1's)
            static s32 sUseDone;
            int ui;
            int uf;

            // dev: NV_USE="item,frame" uses that item from the slots once (the status screen's Use)
            if (!sUseDone && (getenv("NV_USE") != NULL) &&
                (sscanf(getenv("NV_USE"), "%i,%d", &ui, &uf) == 2) && (play->gameplayFrames >= (u32)uf)) {
                s32 NvItems_Use(PlayState* play, s32 i);

                sUseDone = true;
                printf("NVUSE dev: item %02X in slot %d -> %d\n", ui, (int)NvItems_Find(ui),
                       (NvItems_Find(ui) >= 0) ? (int)NvItems_Use(play, NvItems_Find(ui)) : -9);
                fflush(stdout);
            }
            if (gNvDevUseItem >= 0) { // (the same, from NV_WALK's "use:XX" step: z_nv_dev.c)
                s32 NvItems_Use(PlayState* play, s32 i);

                ui = gNvDevUseItem;
                gNvDevUseItem = -1;
                printf("NVUSE dev: item %02X in slot %d -> %d\n", ui, (int)NvItems_Find(ui),
                       (NvItems_Find(ui) >= 0) ? (int)NvItems_Use(play, NvItems_Find(ui)) : -9);
                fflush(stdout);
            }
        }
        memset(sNvFlags[9], 0, sizeof(sNvFlags[9]));
        sNvStateWord[3] = (u16)(Rand_Next() >> 16);
        NvFlags_Op(5, NV_MSF_VOICE_PLAYING, NvSound_LinePlaying() ? 0 : 1);
        NvCountdown_Tick(play); // (and game_loop's countdown: z_nv_ending.c)
        NvBgm_Tick(); // (the music's fades and ramps)
        NvFx_Tick(play); // (RE1's 2D effects: z_nv_effects.c)
        if (sNvLightState >= 0) { // main_loop's special light, a step a tick
            sNvLightState += sNvLightDelta;
        }
        sNvCamera = cam;
        if (cam != sNvLastCamera) { // a camera switch: game_loop's pending redraw, for one frame
            NvFlags_Op(5, NV_MSF_CAMERA_REDRAW, 0);
            sNvSprOff = 0; // (Room_LoadCameraSprites: the new camera's sprites all on)
            sNvLastCamera = cam;
        }
        NvAot_WalkIn(play);
        {
            // dev: NV_EVT_AT="slot,script,frame" starts that event once (as its room action would)
            static s32 sEvtDone;
            int es;
            int en;
            int ef;

            if (!sEvtDone && (getenv("NV_EVT_AT") != NULL) &&
                (sscanf(getenv("NV_EVT_AT"), "%d,%d,%d", &es, &en, &ef) == 3) && (play->gameplayFrames >= (u32)ef)) {
                sEvtDone = true;
                printf("NVEVT dev: event %d in slot %d\n", en, es);
                fflush(stdout);
                NvEvt_Create(es, en);
            }
        }
        // (a pickup's "Take it?" is RE1's item view, a menu mode: the game loop, scripts and
        // events with it, waits for it; 210's Moon Crest event tests the item's flag the frame
        // after offering it)
        if (sNvPickup < 0) {
            s32 first = sNvMfFirst;

            if (first) { // room_set: the events the init script made take their first step
                sNvMfFirst = false;
            } else {
                NvScd_Run(sNvMain);
                NvFlags_Op(5, NV_MSF_CAMERA_REDRAW, 1);
                if ((sNvMfFade > 0) && (--sNvMfFade == 0)) { // (the fade's done: g_short_message_flags)
                    sNvFlags[6][0] = 0xFFFF;
                    sNvMfHeld = false;
                }
            }
            // room_events_check: only while the message flags let the events run
            for (i = 0; (i < NV_EVT_SLOTS) && (first || (sNvFlags[6][0] & NV_MF_EVENTS)); i++) {
                if (NvMovie_Playing()) {
                    return;
                }
                NvEvt_Step(&sNvEvt[i]);
                sNvEvtCur = NULL;
            }
        }
        // room_state_reset, after the scripts and the events each frame: the item used and the
        // one picked up are seen once, and the system flags 0x20-0x3F (what the room's walk-in
        // zones raise for them) start again
        sNvUsedItem = sNvPickedItem = 0;
        sNvFwdPosAction = sNvEntPosAction = 0; // (the same dword: 0x210-0x213)
        sNvFlags[4][1] = 0;
        NvObj_Update(play); // the room's objects: solid, pushed, on their spots
        {
            s32 live = false;

            for (i = 0; i < NV_EVT_SLOTS; i++) {
                live |= sNvEvt[i].active;
            }
            NvCast_Update(play, live); // the player, while a scene walks her
        }
    }
}

/**
 * main_loop's special light over the room (z_play.c): its colour masked by the strength (state >> 7),
 * blended as RE1's draw_rect would (added, taken away, half) - here as see-through colour. Not in
 * the rooms RE1 leaves it out of (the courtyard's garden, water gate and falls, the fountain's
 * first camera, the boulder passage's second, the lab's lift and its entry).
 */
void NvLight_Draw(PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    s32 m;
    s32 r;
    s32 g;
    s32 b;
    s32 a;
    s32 st = sNvStage;
    s32 rm = sNvRoom;

    if ((sNvLightState < 0) || (play->sceneNum < SCENE_RE1_100) || (sNvMain == NULL)) {
        return;
    }
    if ((st == 2) && ((rm <= 2) || ((rm == 5) && (sNvCamera == 0)) || ((rm == 0x0F) && (sNvCamera == 1)) ||
                      ((rm == 0x10) && (sNvCamera == 0)))) {
        return;
    }
    if ((st == 4) && (rm == 0)) {
        return;
    }
    m = (sNvLightState >> 7) & 0xFF;
    r = sNvLightR & m;
    g = sNvLightG & m;
    b = sNvLightB & m;
    if ((st == 2) && (rm == 5) && (sNvCamera == 3)) { // (the fountain's third camera: doubled)
        r = MIN(r * 2, 255);
        g = MIN(g * 2, 255);
        b = MIN(b * 2, 255);
    }
    a = MAX(r, MAX(g, b));
    if (a == 0) {
        return;
    }
    switch (sNvLightMode & 3) {
        case 2: // taken away: the screen darkened by it
            r = g = b = 0;
            a = a * 3 / 4;
            break;
        case 0: // half and half
            a = a / 2;
            break;
        case 3: // a quarter added
            a = a / 4;
            break;
        default: // added: its colour, as strong as it is
            a = a * 3 / 4;
            r = r ? 255 : 0;
            g = g ? 255 : 0;
            b = b ? 255 : 0;
            break;
    }
    OPEN_DISPS(gfxCtx);
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCycleType(OVERLAY_DISP++, G_CYC_1CYCLE);
    gDPSetRenderMode(OVERLAY_DISP++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, r, g, b, a);
    gDPFillWideRectangle(OVERLAY_DISP++, OTRGetRectDimensionFromLeftEdge(0), 0,
                         OTRGetRectDimensionFromRightEdge(SCREEN_WIDTH), SCREEN_HEIGHT);
    gDPPipeSync(OVERLAY_DISP++);
    CLOSE_DISPS(gfxCtx);
}
