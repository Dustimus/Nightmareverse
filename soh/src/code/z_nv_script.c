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
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NV_EVT_SLOTS 8
#define NV_SCD_STACK 16
#define NV_SCD_GUARD 4096 // commands per run before giving up (a desynced stream)
#define NV_MSF_CAMERA_REDRAW 0x1A // bank 5's bit: the camera switched this frame (MSF_CAMERA_REDRAW)

void NvEnemy_ClearSlots(void);
void NvEnemy_ScdSet(PlayState* play, const u8* rec);
void NvEnemy_ScdProp(s32 slot, s32 subCmd, u16 param);
void NvEnemy_ScdPos(s32 slot, s16 angle, s16 x, s16 y, s16 z);
#include "z_nv_re1.h"
void NvMovie_Start(s32 id);
s32 NvMovie_Playing(void);
s32 NvDoorScene_Covering(void);

typedef struct {
    u8 active;
    u8 state; // 0 commands, 1 animation, 2 movement
    s8 depth; // -1: empty
    const u8* ip;
    const u8* ret[4];
    const u8* call[4];
    s16 counter[4];
} NvEvt;

static u32 sNvFlags[10][8];
static NvEvt sNvEvt[NV_EVT_SLOTS];
static const u8* sNvInit;
static const u8* sNvMain;
static const u8* sNvEvtTable;
static PlayState* sNvPlay;
static u8 sNvStage;
static u8 sNvRoom;
static u8 sNvCamera;
static s32 sNvLastCamera = -1;
static u8 sNvTickPhase;
static s32 sNvInitRun; // Enemy_set only places enemies while the init chain runs

// run_command_functions' state
static const u8* sNvIp;
static const u8* sNvStack[NV_SCD_STACK];
static s32 sNvSp;
static s32 sNvContinue;

/* ---- flags ---- */

s32 NvFlags_Test(s32 bank, s32 bit) {
    if ((bank < 0) || (bank >= 10)) {
        return 0;
    }
    return (sNvFlags[bank][(bit >> 5) & 7] & (0x80000000u >> (bit & 31))) != 0;
}

void NvFlags_Op(s32 bank, s32 bit, s32 mode) {
    u32* w;
    u32 mask = 0x80000000u >> (bit & 31);

    if ((bank < 0) || (bank >= 10)) {
        return;
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
    16, 4, 4, 4, 2, 2, 38, 14, 2, 2, 2, 2, 4, 2, 4, 2,  // 0x40
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

static u8 NvScd_StateByte(s32 field) {
    switch (field) {
        case 0:
            return sNvStage;
        case 1:
            return sNvRoom;
        case 2:
        case 3:
            return sNvCamera;
        default:
            return 0;
    }
}

/** One command at sNvIp (script_command_funcs_table): 1 to go on, 0 to stop. */
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
            NvEnemy_ScdProp(p[2], p[3], p[4] | (p[5] << 8));
            sNvIp = p + n;
            return 1;
        case 0x21: { // cmd_enemy_pos_set
            s16 v[5];

            memcpy(v, p + 4, sizeof(v)); // angle, (pitch), x, y, z
            NvEnemy_ScdPos((s8)p[1], v[0], v[2], v[3], v[4]);
            sNvIp = p + 14;
            return 1;
        }
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
        // tests not run yet: false
        case 0x07:
        case 0x10:
        case 0x11:
        case 0x1A:
        case 0x1D:
        case 0x22:
        case 0x2C:
        case 0x2D:
        case 0x36:
        case 0x38:
        case 0x3C:
        case 0x3F:
        case 0x50:
            sNvIp = p + NvScd_Len(p);
            return 0;
        default:
            n = NvScd_Len(p);
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
}

/** One slot until it yields (room_events_check's per-slot loop). */
static void NvEvt_Step(NvEvt* e) {
    s32 guard = 0;

    while (e->active && (++guard < 256)) {
        const u8* p = e->ip;
        s8 d = e->depth;

        switch (p[0]) {
            case 0xF6: // push, then wait as 0xF7
            case 0xF7:
                e->ip = p + 1;
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
                case 0x03:
                    e->state = 2;
                    e->ip = p + 1;
                    break;
                case 0x04: // the entity it drives (not driven yet)
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
            s32 n;

            switch (p[0]) {
                case 0x81:
                    n = (p[1] & 0x0F) ? 12 : 2;
                    break;
                case 0x83:
                    n = 9;
                    break;
                case 0x84:
                case 0x85:
                    n = 5;
                    break;
                case 0x87:
                    n = 4;
                    break;
                case 0x88:
                    n = 3;
                    break;
                case 0x89:
                case 0x8A:
                    n = 2;
                    break;
                case 0x00:
                case 0x80:
                case 0x82:
                case 0x86:
                case 0x8B:
                    n = 1;
                    break;
                default:
                    e->active = 0;
                    return;
            }
            if ((p[0] == 0x80) || (p[0] == 0x8B)) {
                e->state = 0;
            }
            e->ip = p + n;
        } else {
            static const u8 sLen[12] = { 1, 1, 1, 1, 1, 4, 4, 8, 3, 2, 4, 12 };

            if (p[0] >= 12) {
                e->active = 0;
                return;
            }
            e->ip = p + sLen[p[0]];
            if (p[0] == 0x01) {
                e->state = 0;
            } else if ((p[0] >= 2) && (p[0] <= 4)) {
                return; // a frame's movement
            }
        }
    }
}

/* ---- the room ---- */

/** Play_Init, once the room is in: RE1's room_set / room_init. */
void NvScript_RoomInit(PlayState* play) {
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
    NvRe1_RoomLoad(play); // RE1's collision and walk zones, for its enemies (z_nv_zombie.c)
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
    memcpy(lens, d + 4, sizeof(lens));
    sNvInit = d + 16;
    sNvMain = sNvInit + lens[0];
    sNvEvtTable = sNvMain + lens[1];
    if (getenv("NV_NO_ENEMIES") == NULL) {
        sNvInitRun = true;
        NvScd_Run(sNvInit);
        sNvInitRun = false;
    }
    // dev: NV_TEST_ENEMY="type,x,z" (OoT units) places one more, held still (behaviour 0x80), e.g. to
    // see it against a mask
    if (getenv("NV_TEST_ENEMY") != NULL) {
        int type;
        int x;
        int z;

        if (sscanf(getenv("NV_TEST_ENEMY"), "%d,%d,%d", &type, &x, &z) == 3) {
            u8 rec[22] = { 0 };
            s16 rx = (s16)(x / 0.0217f);
            s16 rz = (s16)(-z / 0.0217f);

            rec[0] = 0x1B;
            rec[1] = type;
            rec[2] = 0x80;
            rec[3] = 0xFF;
            memcpy(rec + 12, &rx, 2);
            memcpy(rec + 16, &rz, 2);
            rec[0x12] = 15;
            sNvInitRun = true;
            NvEnemy_ScdSet(play, rec);
            sNvInitRun = false;
        }
    }
    printf("NVSCRIPT %s init: stage %d room %d camera %d\n", file, sNvStage, sNvRoom, sNvCamera);
    fflush(stdout);
}

/** Play_Update: the main chain and the events, at RE1's tick rate. */
void NvScript_Update(PlayState* play) {
    s32 ticks;
    s32 t;
    s32 i;

    if ((sNvMain == NULL) || (sNvPlay != play) || NvDoorScene_Covering() || NvMovie_Playing() ||
        (play->transitionTrigger != TRANS_TRIGGER_OFF) || (getenv("NV_NO_ENEMIES") != NULL)) {
        return;
    }
    ticks = (sNvTickPhase ^= 1) ? 1 : 2;
    for (t = 0; t < ticks; t++) {
        s32 cam = GET_ACTIVE_CAM(play)->camDataIdx;

        sNvCamera = cam;
        if (cam != sNvLastCamera) { // a camera switch: game_loop's pending redraw, for one frame
            NvFlags_Op(5, NV_MSF_CAMERA_REDRAW, 0);
            sNvLastCamera = cam;
        }
        NvScd_Run(sNvMain);
        NvFlags_Op(5, NV_MSF_CAMERA_REDRAW, 1);
        for (i = 0; i < NV_EVT_SLOTS; i++) {
            if (NvMovie_Playing()) {
                return;
            }
            NvEvt_Step(&sNvEvt[i]);
        }
    }
}
