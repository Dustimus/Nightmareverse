/*
 * Nightmareverse: an RE1 entity, as RE1's own code keeps and moves it (z_nv_zombie.c), for the
 * actors that draw RE1's enemies (z_nv_enemy.c). Field names follow the PC decomp's Entity.
 */
#ifndef Z_NV_RE1_H
#define Z_NV_RE1_H

// (included after global.h)

#define NV_RE1_JOINTS 24

typedef struct NvRe1Anim {
    const s16* frames; // per frame: root offset[3], root speed[3], angles[3 * joints]
    s32 frameShorts;
    s32 numFrames;
    s32 numAnims;
    const u16* steps[64]; // per step: u16 frame, u16 flags (low byte: ticks to hold it)
    s32 numSteps[64];
    s32 numJoints;
} NvRe1Anim;

typedef struct NvRe1Entity {
    u8 status_flags;
    u8 id;
    u8 behavior_flags;
    u8 state;
    u8 ignore_player_flag;
    u8 action_behavior;
    u8 action_state;
    u8 state_mirror[4];
    s32 t[3];      // localMatrix.t: where it is
    s16 pos[3];    // position: last frame's (collision rolls back to it)
    s16 angle;
    s16 angleBackup; // +0x7E
    s16 speed[3];
    s16 health;
    u8 hit_state;
    u8 hit_threshold;
    u8 stagger_timer;
    u8 action_speed; // hit counter; 0x80 = knocked down
    u8 animationId;
    u8 animation_frame_id;
    u8 timing_control;
    u8 blend_counter;
    u16 action_ticks_counter;
    s16 death_timer;
    u16 move_speed;
    s16 move_speed_current;
    s16 turn_speed;
    s16 next_turn_timer;
    s16 player_pos_x; // the waypoint (+0x166/+0x168)
    s16 player_pos_z;
    u8 is_moving;
    u8 move_max_steps;
    u8 pathfind_state; // +0x164
    u8 dir_control_flags;
    u8 turn_counter;   // +0x179
    u16 move_dist;     // +0x17a: how far the last collision step moved it
    u8 attacking_direction;
    u8 attack_timer;   // +0x177
    u8 behavior_step;
    u8 internal_timer;
    u8 collisionFlags;
    u8 action_counter;
    u8 death_event_id;
    u8 scd_anim_param;
    u8 scd_entity_flags;
    s16 unk_c6; // grab / SCD target point
    s16 unk_c8;
    s16 turn_delta; // +0x170
    const s16* sca; // Sca_info record: [0] terminator, [1..3] offset, [4] half height, [5] radius
    s16 scaWorld[3];
    // the skeleton's pose (Joint_move)
    const NvRe1Anim* anim;
    s16 rootT[3];
    s16 rot[NV_RE1_JOINTS][3];
} NvRe1Entity;

/** The player as RE1's enemy code sees her: Link, in RE1's units, each tick. */
typedef struct NvRe1Player {
    s32 t[3];
    s16 pos[3];
    s16 angle;
    u8 isBeingAttackedFlag;
    u8 flags;       // aim bits 0xE0 (0x20 level)
    u16 attackTimer;
    s16 health;
    const s16* sca;
    s16 scaWorld[3];
    u8 padHeld;     // RE1's mash test: 1 directions, 2 buttons
} NvRe1Player;

extern NvRe1Player gNvRe1Player;

void NvRe1_RoomLoad(void* play);
void NvRe1_TickBegin(void);
void NvZombie_Init(NvRe1Entity* e);
void NvZombie_Update(NvRe1Entity* e);
void NvZombie_Hit(NvRe1Entity* e, s16 damage, u8 hit, u8 weaponId);
s32 NvRe1_JointMove(NvRe1Entity* e, s32 reverse, s16 blendStep);

#endif
