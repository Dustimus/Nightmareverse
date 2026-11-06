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
    u16 scd_entity_flags;
    u16 scd_timer;  // +0xDE: the event script's turn step / timer
    u8 fx;          // an effect from its hand this tick (a character's: 1 a muzzle flash, 2 flame)
    s16 frame_hold; // +0xC4: a held frame's count (npc_scd_01); the stand-in's animation ticks
    s16 unk_c6; // grab / SCD target point
    s16 unk_c8;
    s16 turn_delta; // +0x170
    u8 headless;    // the head burst (a crawler's grab ends with the player's stomp)
    const s16* sca; // Sca_info record: [0] terminator, [1..3] offset, [4] half height, [5] radius
    s16 scaWorld[3];
    // the skeleton's pose (Joint_move)
    const NvRe1Anim* anim;
    s16 rootT[3];
    s16 rot[NV_RE1_JOINTS][3];
    s16 yawExtra[NV_RE1_JOINTS]; // drawn on top of the animation's turn (Yawn's body trailing its head)
    u8 holding;   // it holds the player (z_nv_enemy.c grabs Link with OoT's grab, at unk_c6/c8)
    u8 snd_group; // its sound group (Snd_em's id + group * 10): the SCD record's byte 0x15 >> 4 & 7
    s16 pitch;    // its turn about x, drawn under the facing (a spider on the ceiling: 0x800)
    s16 roll;     // its turn about z (RE1's rotation.z: Plant 42's vines swinging sideways)
    s32 joint[NV_RE1_JOINTS][3]; // its joints in RE1's world, last frame (z_nv_enemy.c NvEnemy_JointsWorld)
    u8 numJoints;
    s16 reaction_timer; // +0x176 (the dog: gravity in a leap, the turn it holds while swerving)
    // the dog's own (Cerberus.cpp's offsets 0x16C-0x18A)
    struct {
        s32 dist;      // |dx| + |dz| to the player
        s16 launchVy;  // a leap's starting rise
        s16 turnDelta;
        s16 turnStep;
        s16 probe;     // wall probes: bit 0 this tick, 1-3 the ticks before, 4 didn't move
        u8 path;       // entity_pathfind_update's answer
        s16 aggro;
        s16 phase;
        s16 swerve;    // also the head's turn
        s16 blood;
        u8 alert;
        s16 behFlags;  // bit 0 runs, bit 1 hunting, bit 2 gave up
        s16 aiFlags;
        s16 repause;
    } cb;
    // the Hunter's own
    struct {
        s16 ticks;
        s16 pathFound;  // the path finder has reached the player once (arms the slow approach)
        u8 roomHit;     // walls met while moving (the wander steering's)
        u8 hits;        // hits taken in the last stretch (two quick ones: a side-step)
        u8 hitTimer;
        u8 approach;    // ticks into the slow approach
        u8 repause;
        u8 pounce;      // the decapitation leap is rolled (a hurt player within reach)
        u8 jointSel;    // which side it leaps from (6 or 9: the crouch animation 0x12 - it)
        s16 targetX;    // the point past the player it leaps at
        s16 targetZ;
    } hn;
    // the crow's own
    struct {
        s16 vy;        // climb or sink speed
        s16 lowLimit;  // the altitudes it turns at (RE1's y: negative is up)
        s16 highLimit;
        s16 turnRate;
        s16 altBias;   // -400 for the low-flying kind (behaviour bit 0)
        u16 dist;      // |dx| + |dz| to the player
        u16 stuck;     // ticks against a wall
        u8 touch;      // it bumped into the player this tick
        u8 coll;       // it hit a wall this tick
        u8 pathFound;
    } cr;
    // the greenhouse plant's own
    struct {
        s32 dist;      // |dx| + |dz| to the player
        s16 holdoff;   // ticks before it can strike again
        s16 timer;     // the drain's beat / the miss count
        s16 fidget;    // ticks to the next sway
        s16 alerted;   // 1 until it first struck
        s16 angleBack; // its own facing, put back after a strike
        u16 hits;
        u8 animEnd;
        u8 holding;    // it holds the player (z_nv_enemy.c grabs Link with OoT's grab)
        u8 saved[4];   // state, behaviour, step, sub-step before a hit
    } mp;
    // Yawn's own
    struct {
        s8 turnAcc;   // the swim's sway: doubles until 0x40, then flips
        u8 form;      // 0 the juvenile (first fight), 1 grown (reared up, the rematch)
        u8 bites;     // bites in a row (five force a rear-up)
        s16 sway;     // the sideways part of the swim
        s8 turnDir;   // 1 while biting; -1 right after a bite landed
        u8 recover;   // after a bite (bit 7: interrupted)
        u8 waypoint;  // into the entry / flee / reposition paths
        u8 nearlyDead; // the player is nearly dead: rear up
        u8 stuck;     // ticks against walls (150 send it elsewhere)
        u8 hiss;
        u8 swallowCool; // ticks before it may try the swallow again
        u8 saved[4];  // state, flag, behaviour, step before a hit
        s16 heading[64]; // its facing, tick by tick (the body follows where the head went)
        u8 headingAt;
    } yw;
    // the Web Spinner's own
    struct {
        s32 dist;      // |dx| + |dz| to the player
        s16 turn;      // a circle's or a sidle's turn
        s16 pitch;     // upside down on the ceiling (0x800) until it drops
        u8 sees;       // the path finder sees her
        u8 cooldown;   // ticks before it may lunge or spit again
        u8 stuck;      // ticks against walls
        u8 touch;      // it bumped into her this tick
        u8 coll;       // it hit a wall this tick
    } ws;
    // the wasp's own
    struct {
        u16 dist;      // |dx| + |dz| to the player
        s16 bob;       // the climb or sink step, 0x23 to 0x96
        s16 drift;     // ticks cruising since the last turn toward her
        s16 timer;
        u8 climb;      // 1 climbing, 0 sinking
        u8 sees;       // the path finder sees her
        u8 touch;      // it touched her this tick
        u8 coll;       // it hit a wall this tick
        u8 animDone;   // the animation came round this tick
        u8 big;        // the big wasp (behaviour bit 1): twice the health and sting, no pin
        u8 respawns;   // a nest wasp's returns (the tenth is the last)
        u8 cried;      // its death cry played
    } wa;
    // the snake's own
    struct {
        u16 dist;      // |dx| + |dz| to the player (4999 while she's off the floor)
        u16 stuck;     // ticks against walls
        u8 sees;       // the path finder sees her
        u8 touch;      // it touched her this tick
        u8 coll;       // it hit a wall this tick
        u8 active;     // out (a hidden one isn't until it emerges)
    } ad;
    // the shark's own
    struct {
        s32 dist;      // |dx| + |dz| to the player
        s16 struggle;  // the bite's hold left (her mashing wears it down)
        s8 cue;        // the swim sound's beat
        u8 wallAhead;  // walls met 800 ahead (the steering's)
    } ne;
    // the Black Tiger's own
    struct {
        s32 dist;      // |dx| + |dz| to the player
        s16 turn;      // a strafe's or a circle's turn
        u16 stuck;     // ticks against walls, or without seeing her
        u8 sees;       // the path finder sees her
        u8 coll;       // it hit a wall this tick
        u8 delay;      // ticks before it may spit again
        u8 cooldown;   // ticks after it comes in before it picks a move
    } bt;
    // the Chimera's own
    struct {
        s32 dist;      // |dx| + |dz| to the player
        s16 turnStep;  // a turn's rate
        s16 repause;   // ticks before the next strike
        u16 wallTicks; // ticks against walls
        u8 sees;       // the path finder sees her
        u8 far;        // it started far off (the fast walk)
        u8 hard;       // a hard one (spawn kind 3)
        u8 touch;      // it touched her this tick
        u8 coll;       // it hit a wall this tick
    } cm;
    // Plant 42's own (the plant and its vines)
    struct {
        s32 dist;      // straight-line distance to the player
        s16 ticks;
        s16 step;      // a sweep's or a strike's swing speed
        s8 sway;       // the idle's drift in facing and roll
        s8 swing;
        u8 dir;        // which way a sweep goes
        u8 vinesLeft;  // the plant: vines still up (it can't be hurt while more than one is)
        u8 dead;       // the plant: dead (the vines wither)
    } pl;
    // the Tyrant's own
    struct {
        s32 dist;      // |dx| + |dz| to the player
        s16 ticks;
        u8 cooldown;   // ticks before it picks another strike
        u8 landed;     // this strike has hit her
        u8 wallHit;    // walls met (the wander steering's)
        u8 boom;       // an explosion on it this tick (the rocket's: z_nv_enemy.c draws it)
        u8 blownUp;    // the rocket blew it apart: its body isn't drawn (ty_flags bit 3)
    } ty;
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
    s16 speed;      // how fast she's going (RE1's units a tick: walking ~70, running ~230)
} NvRe1Player;

extern NvRe1Player gNvRe1Player;

void NvRe1_RoomLoad(void* play);
void NvRe1_TickBegin(void);
void NvZombie_Init(NvRe1Entity* e);
void NvNpc_Init(NvRe1Entity* e);
void NvNpc_Update(NvRe1Entity* e);
void NvNpc_PlayerTick(NvRe1Entity* e);
NvRe1Entity* NvEnemy_SlotEntity(s32 slot);
NvRe1Entity* NvCast_Player(void);
void NvZombie_Update(NvRe1Entity* e);
void NvZombie_Hit(NvRe1Entity* e, s16 damage, u8 hit, u8 weaponId);
s32 NvRe1_JointMove(NvRe1Entity* e, s32 reverse, s16 blendStep);
void NvCerberus_Init(NvRe1Entity* e);
void NvCerberus_Update(NvRe1Entity* e);
void NvHunter_Init(NvRe1Entity* e);
void NvHunter_Update(NvRe1Entity* e);
void NvCrow_Init(NvRe1Entity* e);
void NvCrow_Update(NvRe1Entity* e);
void NvPlant_Init(NvRe1Entity* e);
void NvPlant_Update(NvRe1Entity* e);
void NvYawn_Init(NvRe1Entity* e);
void NvYawn_Update(NvRe1Entity* e);
void NvSpider_Init(NvRe1Entity* e);
void NvSpider_Update(NvRe1Entity* e);
s32 NvScript_RoomNum(void); // z_nv_script.c
extern u8 gNvRe1Poison;          // z_nv_items.c: RE1's poison bits (0x02 venom, 0x20 Yawn's)
void NvPoison_Give(s32 rolledIn); // a venomous hit landed: poisoned when rolledIn
void NvWasp_Init(NvRe1Entity* e);
void NvWasp_Update(NvRe1Entity* e);
void NvAdder_Init(NvRe1Entity* e);
void NvAdder_Update(NvRe1Entity* e);
void NvNeptune_Init(NvRe1Entity* e);
void NvNeptune_Update(NvRe1Entity* e);
void NvBlackTiger_Init(NvRe1Entity* e);
void NvBlackTiger_Update(NvRe1Entity* e);
void NvChimera_Init(NvRe1Entity* e);
void NvChimera_Update(NvRe1Entity* e);
void NvPlant42_Init(NvRe1Entity* e);
void NvPlant42_Update(NvRe1Entity* e);
void NvPlant42_Forget(NvRe1Entity* e);
void NvTyrant_Init(NvRe1Entity* e);
void NvTyrant_Update(NvRe1Entity* e);
extern s16 gNvRe1Drain;
extern s32 gNvRe1Kill; // an enemy's kill move landed (the maul, the decapitation, the swallow): Link dies // a plant's drain this tick: RE1's health, no knock back (z_nv_enemy.c)
extern s16 gNvRe1DogBite;
extern s32 gNvRe1DogFlinch; // Link is still reeling from a bite (RE1's isBeingAttackedFlag for the dog) // a dog's bite this tick: RE1's damage (z_nv_enemy.c hurts Link)

#endif
