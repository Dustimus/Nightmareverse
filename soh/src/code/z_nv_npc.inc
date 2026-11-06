/*
 * Nightmareverse: RE1's characters (entity ids 32 and up: Chris, Barry, Rebecca, Wesker, Richard,
 * Enrico and their variants), as the room event scripts drive them. Included at the end of
 * z_nv_zombie.c with the ported enemies.
 *
 * As RE1 runs them (the decomp's entities/CharacterNpc.cpp is the reference): a character spawned
 * by cmd_enemy_set drops into state 1, where it holds its pose (idle 0 for the living ones), until
 * an event script writes state 8 and a behaviour into it (z_nv_script.c's event runner, the
 * state-1 opcodes 0x83 / 0x84 / 0x85). State 8 then runs that behaviour: 0 play, 1 play once and
 * raise the script's flag, 2 turn and walk to the target, 3 run to it and stop, 4 / 5 back away to
 * it, 6 turn to it, 7 play to the end, 8 fire (the animation; the flash and the shell are left
 * out), 9 play in reverse, 10 play and loop. Each one that finishes raises its flag in bank 4
 * (Flg_on(g_SysFlags, scd_anim_param)), which the script waits on.
 *
 * The same behaviours walk the player's stand-in (NvNpc_Player, z_nv_enemy.c moves Link with it).
 * The player has no RE1 model here, so an animation she would play just takes NV_NPC_PLAYER_ANIM
 * ticks.
 *
 * Left out, each a note where it would be: the head tracking (state-1 opcode 0x81's look-at), the
 * held weapon's joint, the footsteps, the shadow, the follow-the-player pathfinding (state 9).
 */

#define NV_NPC_PLAYER_ANIM 30 // ticks an animation takes on the player's stand-in (no model)

/** Joint_move, or for the model-less stand-in a count of ticks: 1 once the animation is done. */
static s32 npc_play(s32 reverse, s16 blendStep) {
    if (E->anim == NULL) {
        if (++E->frame_hold >= NV_NPC_PLAYER_ANIM) {
            E->frame_hold = 0;
            return 1;
        }
        return 0;
    }
    return Joint_move(reverse, blendStep);
}

static void npc_rewind(u8 blend) {
    E->animation_frame_id = 0;
    E->timing_control = 0;
    E->blend_counter = blend;
    E->frame_hold = 0;
}

static void npc_done(void) {
    void NvFlags_Op(s32 bank, s32 bit, s32 mode);

    NvFlags_Op(4, E->scd_anim_param, 0);
}

/** entity_apply_walk_speed: slower on the frames a foot is planted. */
static void npc_walk_speed(s16 speed) {
    u8 f = E->animation_frame_id;

    E->move_speed_current = speed;
    if ((u8)(f - 0x15) < 7) {
        E->move_speed_current -= 0xD;
    }
    if ((u8)(f - 7) < 7) {
        E->move_speed_current -= 0xD;
    }
    if ((u8)(f - 0x17) < 3) {
        E->move_speed_current -= 0xE;
    }
    if ((u8)(f - 9) < 3) {
        E->move_speed_current -= 0xE;
    }
}

static s32 npc_target_dist(void) {
    s32 dz = E->t[2] - E->unk_c8;
    s32 dx = E->t[0] - E->unk_c6;

    return SquareRoot0(dz * dz + dx * dx);
}

/* ---- state 8: the script's behaviours (g_npcScdBehaviors) ---- */

static void npc_scd_00(void) {
    if (E->action_state == 0) {
        npc_rewind(7);
        E->animationId = 0;
        E->action_state = 1;
    } else if (E->action_state != 1) {
        return;
    }
    npc_play(E->scd_entity_flags & 1, 0x200);
}

static void npc_scd_01(void) {
    u8 st = E->action_state;

    if (st == 0) {
        npc_rewind((E->scd_entity_flags & 0x20) ? 0 : 7);
        E->action_state = 1;
        E->move_speed_current = 0;
    } else if (st != 1) {
        if (st == 2) {
            npc_done();
            E->scd_timer = 0;
            if (E->scd_entity_flags & 0x10) {
                E->action_state = 0;
            }
        }
        return;
    }
    if (npc_play(E->scd_entity_flags & 1, 0x200)) {
        E->action_state = 2;
        E->angle += (s16)E->scd_timer;
    }
}

static void npc_scd_02(void) {
    switch (E->action_state) {
        case 0:
            npc_rewind(7);
            E->animationId = 7;
            E->action_state = 1;
            // fall through
        case 1:
            E->angle += (s16)turn_toward_target(E->unk_c6, E->unk_c8, (s16)E->scd_timer);
            npc_play(E->scd_entity_flags & 1, 0x200);
            if (turn_toward_target(E->unk_c6, E->unk_c8, 0x16A) == 0) {
                E->action_state = 2;
            }
            break;
        case 2:
            npc_rewind(7);
            E->animationId = 7;
            E->action_state = 3;
            // fall through
        case 3:
            npc_walk_speed(0x5D);
            entity_rotate_toward_target(E->unk_c6, E->unk_c8, E->scd_timer);
            npc_play(E->scd_entity_flags & 1, 0x200);
            Add_speedXZ(0);
            if (npc_target_dist() < 0x96) {
                npc_done();
                if (!(E->collisionFlags & 0x80)) {
                    E->action_behavior = 0;
                    E->action_state = 0;
                }
            }
            break;
    }
}

static void npc_scd_03(void) {
    switch (E->action_state) {
        case 0:
            npc_rewind(7);
            E->animationId = 8;
            E->action_state = 1;
            // fall through
        case 1:
            E->angle += (s16)turn_toward_target(E->unk_c6, E->unk_c8, (s16)E->scd_timer);
            npc_play(E->scd_entity_flags & 1, 0x200);
            if (turn_toward_target(E->unk_c6, E->unk_c8, 0x16A) == 0) {
                E->action_state = 2;
            }
            break;
        case 2:
            E->move_speed_current = 0xD2;
            npc_rewind(7);
            E->animationId = 8;
            E->action_state = 3;
            // fall through
        case 3:
            entity_rotate_toward_target(E->unk_c6, E->unk_c8, E->scd_timer);
            npc_play(E->scd_entity_flags & 1, 0x200);
            Add_speedXZ(0);
            if (npc_target_dist() < 0xFA) {
                E->action_state = 4;
                if (E->collisionFlags & 0x80) {
                    E->action_state = 3;
                    npc_done();
                }
            }
            break;
        case 4:
            npc_rewind(7);
            E->animationId = 0;
            E->action_state = 5;
            E->action_ticks_counter = 0;
            // fall through
        case 5:
            npc_play(E->scd_entity_flags & 1, 0x200);
            if ((s16)++E->action_ticks_counter > 3) {
                E->action_state = 6;
            }
            E->move_speed_current -= 0x1E;
            Add_speedXZ(0);
            break;
        case 6:
            E->action_behavior = 0;
            E->action_state = 0;
            npc_done();
            break;
    }
}

/** Behaviours 4 and 5's step: back toward the target (its back turned to it), 100 to arrive. */
static void npc_walk_backward_step(void) {
    E->angle = (E->angle + 0x800) & 0xFFF;
    entity_rotate_toward_target(E->unk_c6, E->unk_c8, E->scd_timer);
    E->angle -= 0x800;
    npc_play(E->scd_entity_flags & 1, 0x200);
    Add_speedXZ(0x800);
    if (npc_target_dist() < 100) {
        npc_done();
        if (!(E->collisionFlags & 0x80)) {
            E->action_behavior = 0;
            E->action_state = 0;
        }
    }
}

static void npc_scd_04(void) {
    if (E->action_state == 0) {
        npc_rewind(7);
        E->action_state = 1;
        E->animationId = 3;
    } else if (E->action_state != 1) {
        return;
    }
    E->move_speed_current = 0x40; // (RE1's: 0x3C, then a +4 whose test always holds)
    npc_walk_backward_step();
}

static void npc_scd_05(void) {
    if (E->action_state == 0) {
        npc_rewind(7);
        E->action_state = 1;
        E->animationId = 2;
        E->move_speed_current = 0x1D;
    } else if (E->action_state != 1) {
        return;
    }
    npc_walk_backward_step();
}

static void npc_scd_06(void) {
    u8 st = E->action_state;

    if (st == 0) {
        npc_rewind(7);
        E->animationId = 7;
        E->action_state = 1;
    } else if (st != 1) {
        if (st == 2) {
            E->action_behavior = 0;
            E->action_state = 0;
            npc_done();
        }
        return;
    }
    entity_rotate_toward_target(E->unk_c6, E->unk_c8, E->scd_timer);
    npc_play(E->scd_entity_flags & 1, 0x200);
    if (turn_toward_target(E->unk_c6, E->unk_c8, 0x28) == 0) {
        E->action_state = 2;
    }
}

static void npc_scd_07(void) {
    s8 st = (s8)E->action_state;

    if (st == 0) {
        npc_rewind(7);
        E->action_state = 1;
    }
    if ((s8)E->action_state == 1) {
        E->action_state += (u8)npc_play(E->scd_entity_flags & 1, 0x200);
    } else if ((s8)E->action_state == 2) {
        npc_done();
    }
    E->angle += (s16)E->scd_timer;
}

static void npc_scd_08(void) {
    u8 weapon = (u8)(E->behavior_flags - 2);

    switch (E->action_state) {
        case 0:
            npc_rewind(3);
            E->action_state = 1;
            if (weapon == 3) {
                E->action_state = 3;
                E->animationId = 0x17;
            }
            // fall through
        case 1:
        case 3: // (the muzzle flash, the shell and the second flash would spawn on their frames here)
            E->action_state += (u8)npc_play(0, 0x400);
            break;
        case 2:
            npc_done();
            break;
        case 4:
            E->action_state = 5;
            E->timing_control = 0;
            E->animationId = 0x14;
            E->blend_counter = 3;
            // fall through
        case 5: // the flamethrower's sweep (its fire and sound left out)
            npc_play(0, 0x400);
            E->angle += (s16)E->scd_timer;
            break;
    }
}

static void npc_scd_09(void) {
    s8 st = (s8)E->action_state;

    if (st == 0) {
        npc_rewind(3);
        E->action_state = 1;
    } else if (st != 1) {
        if (st == 2) {
            E->action_behavior = 0;
            E->action_state = 0;
            npc_done();
        }
        return;
    }
    E->action_state += (u8)npc_play(1, 0x400);
}

static void npc_scd_10(void) {
    s8 st = (s8)E->action_state;

    if (st == 0) {
        npc_rewind(7);
        E->action_state = 1;
        E->move_speed_current = 0;
    } else if (st != 1) {
        if (st == 2) {
            npc_done();
            if (E->scd_entity_flags & 0x10) {
                E->action_state = 0; // loop
            }
        }
        return;
    }
    E->action_state += (u8)npc_play(E->scd_entity_flags & 1, 0x200);
}

static void (*const sNpcScd[11])(void) = {
    npc_scd_00, npc_scd_01, npc_scd_02, npc_scd_03, npc_scd_04, npc_scd_05,
    npc_scd_06, npc_scd_07, npc_scd_08, npc_scd_09, npc_scd_10,
};

/** npc_state8_action_update: the behaviour, twice when the script asks for a double step. */
static void npc_state8(void) {
    if (E->action_behavior >= 11) {
        return;
    }
    sNpcScd[E->action_behavior]();
    if ((E->scd_entity_flags & 2) && (E->action_behavior < 11)) {
        sNpcScd[E->action_behavior]();
    }
}

/* ---- state 1: its own (the idle table) ---- */

/** Idle 1: a stagger forward, slowing, then the knock (the wall probe left out: it stops when spent). */
static void npc_idle_01(void) {
    switch (E->action_state) {
        case 0:
            E->action_state = 1;
            npc_rewind(0);
            E->animationId = 0x35;
            E->move_speed_current = 1000;
            // fall through
        case 1:
            E->move_speed_current -= E->animation_frame_id * 0xF;
            Joint_move(0, 0x400);
            if (E->move_speed_current <= 0) {
                E->move_speed_current = 0;
                E->action_state = 2;
                break;
            }
            Add_speedXZ(0x800);
            break;
        case 2:
            E->action_state = 3;
            npc_rewind(3);
            E->animationId = 0x36;
            // fall through
        case 3:
            E->action_state += (u8)Joint_move(0, 0x400);
            break;
    }
}

/** Idle 2: a death (animation 0x33; the blood spray and its pool left out). */
static void npc_idle_02(void) {
    switch (E->action_state) {
        case 0:
            E->action_state = 1;
            npc_rewind(3);
            E->animationId = 0x33;
            E->death_timer = 0xB4;
            // fall through
        case 1:
            E->action_state += (u8)Joint_move(0, 0x400);
            break;
    }
}

/** Idle 3: run through by the Tyrant (animation 0x30, facing it), bleeding out over 250 ticks. */
static void npc_idle_03(void) {
    NvRe1Entity* self = E;
    NvRe1Entity* ty;

    if (E->action_state == 0) {
        E->action_state = 1;
        npc_rewind(0);
        E->animationId = 0x30;
        E->action_ticks_counter = 0xFA;
        E->hit_state = 0x80;
        E->status_flags |= 6;
        ty = NvEnemy_SlotEntity(1);
        E = self;
        if (ty != NULL) {
            E->angle = ty->angle;
        }
    } else if (E->action_state == 2) {
        E->status_flags &= ~2;
        E->health = -1;
        if ((s16)--E->action_ticks_counter == 0) {
            E->action_state = 3;
        }
        return;
    } else if (E->action_state != 1) {
        return;
    }
    if (Joint_move(0, 0x400)) {
        E->action_state = 2;
    }
}

/* ---- the character ---- */

/** npc_state0_init: straight into state 1, facing as placed; the character's first pose. */
void NvNpc_Init(NvRe1Entity* e) {
    E = e;
    E->state = 1;
    E->ignore_player_flag = 0;
    E->action_behavior = 0;
    E->action_state = 0;
    E->health = -1;
    E->blend_counter = 0; // (the pose cmd_enemy_set gave it: its animation and frame)
    if (E->anim != NULL) {
        Joint_move(0, 0x400);
    }
}

/** character_npc_update: one tick. */
void NvNpc_Update(NvRe1Entity* e) {
    E = e;
    E->pos[0] = (s16)E->t[0];
    E->pos[1] = (s16)E->t[1];
    E->pos[2] = (s16)E->t[2];
    switch (E->state) {
        case 0:
            NvNpc_Init(e);
            break;
        case 1:
            // idle: the living characters hold their pose (idle 0 resolves to the no-op for
            // them); 1-3 the knock and the deaths; 9, 10 and 13 play animation 0
            if (E->action_behavior == 1) {
                npc_idle_01();
            } else if (E->action_behavior == 2) {
                npc_idle_02();
            } else if (E->action_behavior == 3) {
                npc_idle_03();
            } else if ((E->action_behavior == 9) || (E->action_behavior == 10) || (E->action_behavior == 13)) {
                if (E->action_state == 0) {
                    E->action_state = 1;
                    E->animationId = 0;
                    npc_rewind(0);
                }
                if (E->action_state == 1) {
                    Joint_move(0, 0x400);
                }
            }
            break;
        case 8:
            npc_state8();
            break;
    }
}

/** The player's stand-in, one tick of the behaviour the script gave her (state 8). */
void NvNpc_PlayerTick(NvRe1Entity* e) {
    E = e;
    if (E->state == 8) {
        npc_state8();
    }
}
