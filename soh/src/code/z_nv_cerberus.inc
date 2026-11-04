/*
 * Nightmareverse: RE1's dog (Cerberus, enemy type 2, EM1002), included at the end of
 * z_nv_zombie.c so it shares the RE1 entity helpers ported there (collision, turning, sight,
 * path finding, Joint_move).
 *
 * Behaviour as RE1's (the PC decomp's entities/Cerberus.cpp is the reference): a state (init,
 * behaviour, hit, death) and, while it behaves, one of ten behaviours picked from the SCD record's
 * behaviour byte: patrol, chase, leap, strafe, alert, maul, close bite, held by the script, stalk.
 * The dog steers by probing: a step 500 units ahead, tested against the room and taken back.
 *
 * Left out, each noted where it would be: blood billboards, the shadow and its
 * fade, the player's flinch animations, and the poison and second-playthrough variants.
 */

#define CB_AI_REPROBE 0x0001
#define CB_AI_LOCKED_ON 0x0002
#define CB_AI_SKID 0x0004
#define CB_AI_SKID2 0x0008
#define CB_AI_WALL 0x0010
#define CB_AI_HURT 0x0020
#define CB_AI_ACTIVE 0x0080

#define CB (&E->cb)

// collision: probe distance 500, radius 400
static const s16 sCbSca[8] = { (s16)0x8000, 500, (s16)-800, 0, 800, 400, 0, 0 };
static const u8 sCbHealth[16] = { 119, 99, 119, 99, 119, 99, 119, 99, 99, 99, 59, 99, 59, 99, 59, 99 };
static const s16 sCbRecoil[8] = { 128, -192, -128, 192, -256, 256, 256, -256 };
static const u8 sCbWalkAnim[2] = { 1, 14 }; // walk, run
static const s16 sCbWalkSpeed[2] = { 40, 130 };
static s32 sCbBarked; // shared by every dog in the room: one approach bark

s16 gNvRe1DogBite;
s32 gNvRe1DogFlinch;

static s32 CbRand(void) {
    return (s32)((Rand_Next() >> 9) & 0x7FFF);
}

static s32 CbAbs(s32 v) {
    return (v < 0) ? -v : v;
}

/* ---- probes ---- */

/** Would a step 500 ahead (along the facing) put the dog in a wall? */
static u8 cb_probe_ahead(void) {
    s32 x;
    s32 z;
    s32 t0 = E->t[0];
    s32 t2 = E->t[2];
    s16 p0 = E->pos[0];
    s16 p2 = E->pos[2];
    u8 flags = E->collisionFlags;
    s32 tmp = sTempVar;
    u8 hit;

    RotY(E->angle, sCbSca[1], 0, &x, &z);
    E->t[0] += x;
    E->t[2] += z;
    E->pos[0] = (s16)E->t[0];
    E->pos[2] = (s16)E->t[2];
    hit = check_room_collision();
    E->t[0] = t0;
    E->t[2] = t2;
    E->pos[0] = p0;
    E->pos[2] = p2;
    E->collisionFlags = flags;
    sTempVar = tmp;
    return (s8)hit > 0;
}

/** The same after turning by delta at mul times the speed (taken back after). */
static u8 cb_probe_turn(s16 delta, s16 mul) {
    s32 t0 = E->t[0];
    s32 t2 = E->t[2];
    s16 p0 = E->pos[0];
    s16 p2 = E->pos[2];
    s16 speed = E->move_speed_current;
    u8 blocked;

    E->move_speed_current = (s16)(speed * mul);
    Add_speedXZ(delta);
    blocked = cb_probe_ahead();
    E->t[0] = t0;
    E->t[2] = t2;
    E->pos[0] = p0;
    E->pos[2] = p2;
    E->move_speed_current = speed;
    return blocked;
}

/** +1: only the left is blocked, -1: only the right, 0: neither; bothBlocked when both are. */
static s8 cb_probe_sides(s16 mul, s8 bothBlocked) {
    s8 left = (s8)cb_probe_turn(0x400, mul);
    s8 right = -(s8)cb_probe_turn((s16)0xFC00, mul);

    if ((left != 0) && (right != 0)) {
        return bothBlocked;
    }
    return left + right;
}

static u8 cb_probe_to_player(s16 mul) {
    return cb_probe_turn((s16)(getAngleTowardsTarget(P->t[0], P->t[2]) - E->angle), mul);
}

static void cb_faster(s16 step, s16 max) {
    if (E->move_speed_current < max) {
        E->move_speed_current = MIN((s16)(E->move_speed_current + step), max);
    }
}

static void cb_slower(s16 step, s16 min) {
    if (E->move_speed_current > min) {
        E->move_speed_current = MAX((s16)(E->move_speed_current - step), min);
    }
}

/** Lean into the turn: 15 left, 2 straight, 16 right. */
static void cb_turn_anim(s16 turn) {
    u8 id = (turn == 0) ? 2 : (turn < 0) ? 15 : 16;

    if (E->animationId != id) {
        E->animationId = id;
        E->timing_control = 0;
        if (E->blend_counter < 4) {
            E->blend_counter = 7;
        }
    }
}

/** Turn the neck toward the player (joints 1, 3, 5, more toward the head); 1 at the limit. */
static u8 cb_head_track(void) {
    s16 saved = E->angle;
    s16 sw;

    E->rot[1][1] += CB->swerve;
    E->rot[3][1] += CB->swerve + (CB->swerve >> 2);
    E->rot[5][1] += CB->swerve + (CB->swerve >> 1);
    E->angle += CB->swerve + (CB->swerve >> 1);
    sw = CB->swerve + (s16)turn_toward_target(P->t[0], P->t[2], 8);
    E->angle = saved;
    CB->swerve = CLAMP(sw, -0x100, 0x100);
    return sw != CB->swerve;
}

/** One tick of a jump: fwd along the facing, rising by vy0 + ticks in the air * gravity (y down). */
static u32 cb_ballistic(s16 fwd, s16 vy0, s16 gravity) {
    s32 x;
    s32 z;
    s16 vy;

    RotY(E->angle, fwd, 0, &x, &z);
    E->t[0] += x;
    E->t[2] += z;
    vy = (s16)((u16)E->death_timer * (u16)gravity + vy0);
    E->t[1] -= vy;
    if (E->t[1] > 0) {
        E->t[1] = 0;
        return (u16)vy;
    }
    E->death_timer++;
    return 0;
}

/**
 * Does the jaw reach the player? RE1 tests a square of reach around its head joint; here the
 * head is taken 700 units ahead of the body.
 */
static u8 cb_jaw_reaches(s16 reach) {
    s32 x;
    s32 z;

    RotY(E->angle, 700, 0, &x, &z);
    return (CbAbs(P->t[0] - (E->t[0] + x)) <= reach) && (CbAbs(P->t[2] - (E->t[2] + z)) <= reach);
}

/* ---- behaviours ---- */

static void cb_select_stalk(void);

static void cb_select_patrol(void) {
    s32 run = E->behavior_flags >> 1;

    CB->behFlags = 4;
    if (E->behavior_flags == 3) {
        cb_select_stalk();
        return;
    }
    E->ignore_player_flag = 1;
    E->animationId = sCbWalkAnim[run & 1];
    E->animation_frame_id = (u8)(CbRand() & 0x1F);
    E->move_speed_current = sCbWalkSpeed[run & 1];
    CB->turnDelta = (s16)((run + 1) * 0x10);
    CB->behFlags = E->behavior_flags & 1;
    CB->swerve = (s16)((CbRand() - 2) & 0xF);
    E->blend_counter = 0xF;
}

static void cb_select_chase(void) {
    if (E->blend_counter > 7) {
        E->blend_counter = 7;
    }
    CB->turnDelta = (s16)((CbRand() & 7) + 0x3D);
    CB->aggro = CB->phase = CB->swerve = 0;
    E->reaction_timer = 0;
    if (CB->aiFlags & CB_AI_REPROBE) {
        CB->swerve = (s16)((CbRand() & 0xF) + 0x14);
    }
    E->ignore_player_flag = 2;
    E->player_pos_x = (s16)P->t[0];
    E->player_pos_z = (s16)P->t[2];
    CB->aiFlags = (CB->aiFlags & ~(CB_AI_SKID | CB_AI_SKID2)) | CB_AI_ACTIVE;
}

/** 5: a running leap; 6: in through a window; 7: down from above. */
static void cb_select_entrance(void) {
    E->death_timer = 0;
    if (E->behavior_flags == 5) {
        if (E->animationId == 16) {
            E->angle -= 0x20;
        } else if (E->animationId == 15) {
            E->angle += 0x20;
        }
        E->animationId = 6;
        E->move_speed_current = 0x41;
        CB->launchVy = 300;
        E->reaction_timer = -40;
    } else if (E->behavior_flags == 6) {
        E->status_flags |= 6;
        E->move_speed_current = 0x41;
        CB->launchVy = 480;
        E->reaction_timer = -40;
        E->animationId = 6;
        CB->behFlags = 5;
        E->hit_state = 1;
    } else {
        E->status_flags |= 4;
        E->animationId = 7;
        E->death_timer = 10;
        E->move_speed_current = 0x118;
        CB->launchVy = 400;
        E->reaction_timer = -40;
        CB->behFlags = 5;
        E->hit_state = 1;
    }
    E->ignore_player_flag = 3;
    E->blend_counter = 3;
    E->action_ticks_counter = 8;
    CB->aggro = 0;
    CB->blood = 0;
}

static void cb_select_bite(void) {
    E->ignore_player_flag = 7;
    E->animationId = 4;
    E->blend_counter = 0;
    E->hit_state = 1;
}

static void cb_select_strafe(void) {
    E->ignore_player_flag = 4;
    if (CB->turnStep == 0) {
        CB->turnStep = (CbRand() & 1) ? 0x80 : -0x80;
    }
    E->action_ticks_counter = 8;
    CB->aggro = 0;
    CB->aiFlags |= CB_AI_ACTIVE;
}

static void cb_select_alert(void) {
    E->ignore_player_flag = 5;
    E->animationId = 0;
    E->blend_counter = 0xF;
    E->animation_frame_id = 0;
    E->action_ticks_counter = 0x14;
    CB->turnStep = 0;
    CB->aggro = 0;
    E->hit_state = 1;
}

static void cb_select_stalk(void) {
    E->ignore_player_flag = 9;
    E->animationId = 1;
    E->animation_frame_id = (u8)(CbRand() & 0x1F);
    E->blend_counter = 0;
    CB->aggro = 0;
    E->move_speed_current = 0;
    if (!(CB->behFlags & 4)) {
        CB->behFlags |= 2;
    }
    E->hit_state = 0;
}

/** The behaviour byte (0-12, or 0x80 for the script's) to a behaviour. */
static void cb_select(void) {
    CB->alert = 0;
    E->timing_control = 0;
    if (CB->aiFlags & 0x80) {
        E->animation_frame_id = 0;
    }
    switch (E->behavior_flags) {
        case 0x80:
            E->ignore_player_flag = 8;
            E->animationId = 0;
            E->blend_counter = 0xF;
            break;
        case 0:
        case 1:
        case 2:
        case 3:
            cb_select_patrol();
            break;
        case 4:
            cb_select_chase();
            break;
        case 5:
        case 6:
        case 7:
            cb_select_entrance();
            break;
        case 8:
            cb_select_bite();
            break;
        case 9:
            cb_select_strafe();
            break;
        case 10:
        case 11:
            cb_select_alert();
            break;
        default:
            cb_select_stalk();
            break;
    }
}

static void cb_chase_again(u8 behaviour) {
    E->ignore_player_flag = 0;
    E->behavior_flags = behaviour;
}

/* patrol: wander, turning off walls, until the player comes within 4500 */
static void cb_patrol(void) {
    if (CB->dist < 4500) {
        cb_chase_again(4);
        E->blend_counter = 7;
    }
    Add_speedXZ(8);
    switch (E->action_behavior) {
        case 0:
            E->action_behavior++;
            E->t[1] = 0;
            E->timing_control = 0;
            E->blend_counter = 0xF;
            CB->aggro = CB->phase = CB->swerve = 0;
            break;
        case 1:
            if (!(CB->probe & 1) && ((CB->behFlags != 0) || (E->action_ticks_counter != 0))) {
                E->action_ticks_counter--;
                break;
            }
            E->action_behavior++;
            E->action_ticks_counter = 0x400;
            {
                s8 dir = cb_probe_sides(3, 0);

                if (dir == 0) {
                    CB->turnStep = ((CB->behFlags == 0) && !(CbRand() & 1)) ? -CB->turnDelta : CB->turnDelta;
                } else {
                    CB->turnStep = (s16)(CB->turnDelta * dir);
                    CB->phase = 0;
                }
                // a walking dog turning away from the player cuts the turn short
                if ((CB->behFlags == 0) && ((s32)(s16)turn_toward_target(P->t[0], P->t[2], 0x200) * dir < 0)) {
                    E->action_ticks_counter = (u16)((s16)E->action_ticks_counter >> 2);
                }
            }
            break;
        default:
            E->angle += CB->turnStep;
            E->action_ticks_counter = (u16)((s16)E->action_ticks_counter - CbAbs(CB->turnStep));
            if ((s16)E->action_ticks_counter < 0) {
                E->action_behavior = 0;
                if (CB->behFlags == 0) {
                    E->action_ticks_counter = (u16)((CbRand() & 0x1F) + 0x4B);
                }
            }
            break;
    }
    Joint_move(0, 0x100);
    if ((E->animation_frame_id == 0) || (E->animation_frame_id == 0x1B)) {
        Snd_em(9); // footfall
    }
}

/** Inside 5000 and lined up: leap, or, on a badly hurt player facing it, strafe for the kill lunge. */
static void cb_consider_attack(void) {
    if ((CB->dist >= 5000) || (P->isBeingAttackedFlag != 0) || gNvRe1DogFlinch) {
        return;
    }
    if (turn_toward_target(P->t[0], P->t[2], 0x100) == 0) {
        if ((P->health < 13) && is_facing_toward_entity() && ((CbRand() & 0x7F) < 126)) {
            cb_chase_again(9);
            Joint_move(0, 0x100);
            return;
        }
        cb_chase_again(5);
        CB->aiFlags |= CB_AI_ACTIVE;
        CB->swerve = 0;
        return;
    }
    if (CB->swerve < 10) {
        CB->swerve++;
        if (CB->swerve > 9) {
            CB->swerve += (s16)((CbRand() & 7) + 0x1E);
            E->reaction_timer = 0;
        }
    }
    if (turn_toward_target(P->t[0], P->t[2], 0x400) == 0) {
        CB->aiFlags |= CB_AI_LOCKED_ON;
        return;
    }
    if (((CB->aiFlags & 3) == CB_AI_LOCKED_ON) && ((CbRand() & 0x40) > 0x38)) {
        CB->aiFlags = (CB->aiFlags & ~CB_AI_LOCKED_ON) | CB_AI_REPROBE | CB_AI_SKID;
    }
}

static s16 cb_wall_follow(s16 phaseLimit, s16 step);

static void cb_chase_run(void) {
    s32 turn;

    cb_faster(0x14, 0xF0);
    if ((CB->repause == 0) && (CB->swerve < 10) && (E->move_speed_current > 0xEF)) {
        turn = (s16)turn_toward_target(E->player_pos_x, E->player_pos_z, CB->turnDelta);
        entity_rotate_toward_target(E->player_pos_x, E->player_pos_z, (u16)CB->turnDelta);
    } else {
        if (CB->probe & 0x11) {
            s16 step = CB->turnDelta + ((CB->aiFlags & CB_AI_REPROBE) ? 0x20 : 0);

            E->reaction_timer = (s16)turn_toward_target(E->player_pos_x, E->player_pos_z, step);
            CB->aiFlags &= ~CB_AI_REPROBE;
        }
        turn = E->reaction_timer;
        E->angle += E->reaction_timer;
        CB->swerve--;
        if (CB->swerve < 0xB) {
            CB->swerve = 5;
            CB->aiFlags &= ~CB_AI_REPROBE;
        }
    }
    // the path finder: 1 a fresh waypoint, 0 a dead one (six and it gives up), 2 still looking
    if (!(CB->path & 2)) {
        if (!(CB->path & 1)) {
            if (++CB->phase == 6) {
                cb_chase_again(12);
                CB->behFlags |= 4;
                return;
            }
        } else {
            CB->phase = 0;
            E->move_speed_current = 0xF0;
            E->player_pos_x = (s16)P->t[0];
            E->player_pos_z = (s16)P->t[2];
        }
    }
    cb_turn_anim((s16)turn);
    if (!(CB->behFlags & 2)) {
        if (CB->probe & 0x11) { // wedged: strafe out of it
            CB->aggro += 2;
            if (CB->aggro > 0xF) {
                cb_chase_again(9);
                CB->turnStep = (s16)(turn * 2);
                CB->aiFlags &= ~CB_AI_ACTIVE;
            }
        } else if (CB->aggro > 0) {
            CB->aggro--;
        }
    } else {
        if (cb_probe_to_player(4) && (CB->repause == 0)) {
            E->action_behavior = 1;
            E->action_state = 0;
        }
        if (CB->repause > 0) {
            CB->repause--;
        }
    }
    if (CB->aiFlags & (CB_AI_SKID | CB_AI_SKID2)) {
        E->action_behavior = 2;
        E->action_state = 0;
    }
}

static void cb_chase_wall(void) {
    s16 dir;

    cb_faster(0x14, 0xF0);
    dir = cb_wall_follow(0x5A, 0x40);
    if (dir != 3) {
        cb_turn_anim(dir - 1);
    }
    if (CB->aiFlags & CB_AI_WALL) {
        E->ignore_player_flag = 0;
        E->action_behavior = 4;
        CB->turnDelta = (s16)((CbRand() & 7) + 0x3D);
        CB->aiFlags &= ~CB_AI_ACTIVE;
        CB->repause = (s16)((CbRand() & 3) * 2 + 8);
    }
}

/** Overshot: brake, swing round toward the waypoint, and run again past half a turn. */
static void cb_chase_skid(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        E->action_ticks_counter = 0;
    }
    CB->turnStep = (s16)turn_toward_target(E->player_pos_x, E->player_pos_z, 0x60);
    Add_speedXZ(-(s16)E->action_ticks_counter);
    if (E->action_state == 1) {
        cb_slower(0x0E, 10);
        if (E->move_speed_current < 0xB) {
            E->action_state++;
        }
    } else {
        cb_faster(0x0E, 0xF0);
    }
    E->angle += CB->turnStep;
    E->action_ticks_counter = (u16)((s16)E->action_ticks_counter + CB->turnStep);
    cb_turn_anim(CB->turnStep);
    if ((CbAbs((s16)E->action_ticks_counter) > 0x7FF) || (CB->turnStep == 0)) {
        E->action_behavior = 0;
        CB->aiFlags &= ~(CB_AI_SKID | CB_AI_SKID2);
    }
}

static void cb_chase(void) {
    if (E->action_behavior < 2) {
        Add_speedXZ(0);
    }
    switch (E->action_behavior) {
        case 0:
            cb_chase_run();
            break;
        case 1:
            cb_chase_wall();
            break;
        default:
            cb_chase_skid();
            break;
    }
    cb_consider_attack();
    if (Joint_move(0, 0x200)) {
        Snd_em(0); // running
    }
}

/**
 * The bite lands: 12 of RE1's health, and the dog tumbles away. On a player under 13 facing away,
 * the maul instead: it brings her down (aggro 2 sends the leap to the maul at its top).
 */
static void cb_bite_player(void) {
    E->hit_state = 0;
    if ((P->health < 13) && !is_facing_toward_entity()) {
        E->status_flags |= 2;
        E->move_speed_current = 0;
        CB->aggro = 2;
        E->hit_state = 1;
        P->isBeingAttackedFlag = 1;
        E->status_flags &= ~4;
        return;
    }
    E->unk_c6 = (s16)P->t[0];
    E->unk_c8 = (s16)P->t[2];
    if (E->behavior_flags != 6) {
        E->action_behavior = 4;
        E->animationId = 0x11;
        E->animation_frame_id = 1;
        E->timing_control = 0;
        E->blend_counter = 0xF;
        E->status_flags &= ~4;
        E->move_speed_current >>= 2;
    }
    gNvRe1DogBite = 12;
    CB->turnDelta = 0x200;
    CB->swerve = 0;
    CB->blood = 3;
    CB->phase = 0;
    Snd_em(3); // bite
}

static void cb_leap(void) {
    switch (E->action_behavior) {
        case 0: // crouch, then spring
            Add_speedXZ(0);
            E->action_ticks_counter--;
            if ((E->action_ticks_counter == 0) || (E->death_timer != 0)) {
                E->action_behavior++;
                E->move_speed_current = 0x118;
                CB->alert = 1;
                if (E->behavior_flags != 7) {
                    Snd_em(6);
                }
            }
            Joint_move(0, 0x400);
            break;
        case 1: { // in the air: the jaw can catch the player on the way up
            u8 air = (u8)E->death_timer;
            s16 gravity = E->reaction_timer;
            s16 vy0 = CB->launchVy;

            E->status_flags |= 0x40;
            cb_ballistic(E->move_speed_current, CB->launchVy, E->reaction_timer);
            if ((CB->aggro == 0) && cb_jaw_reaches((P->health > 12) ? 1000 : 800)) {
                CB->aggro = 1;
                if ((P->isBeingAttackedFlag == 0) && !gNvRe1DogFlinch && (E->t[1] > -0x708)) {
                    cb_bite_player();
                    if (CB->aggro != 2) {
                        if (E->animation_frame_id != 0) {
                            Joint_move(0, 0x100);
                        }
                        return;
                    }
                }
            }
            if ((air + 1) * gravity + vy0 < 1) { // past the top
                if (CB->aggro == 2) { // the maul
                    E->ignore_player_flag = 6;
                    E->action_behavior = 0;
                    E->status_flags |= 2;
                    break;
                }
                E->action_behavior++;
                if (E->animationId == 6) {
                    E->animationId = 7;
                    E->timing_control = 0;
                    E->blend_counter = 0xF;
                }
            }
            if (E->animation_frame_id != 0) {
                Joint_move(0, 0x100);
            }
            break;
        }
        case 2: // coming down
            E->status_flags |= 0x40;
            cb_ballistic(E->move_speed_current, CB->launchVy, E->reaction_timer);
            if (E->t[1] >= 0) {
                E->action_behavior++;
                E->t[1] = 0;
                E->status_flags = (E->status_flags & 0xF9) | 0x20;
                Snd_em(5); // landing
            }
            if (E->animation_frame_id != 9) {
                Joint_move(0, 0x100);
            }
            break;
        case 3: // back on its feet: chase, or strafe off a wall it's against
            Add_speedXZ(0);
            cb_slower(0x40, 0xF0);
            if (!Joint_move(0, 0x100)) {
                break;
            }
            E->status_flags &= ~4;
            E->ignore_player_flag = 0;
            E->hit_state = 0;
            CB->aiFlags &= ~CB_AI_REPROBE;
            if ((CB->probe & 0x11) && (E->behavior_flags != 6) && (E->behavior_flags != 7) && !(CB->behFlags & 2)) {
                E->behavior_flags = 9;
                CB->turnStep = (s16)(turn_toward_target(E->player_pos_x, E->player_pos_z, CB->turnDelta) * 2);
                if (CB->turnStep == 0) {
                    CB->turnStep = (s16)(cb_probe_sides(1, -1) * CB->turnDelta * 2);
                }
                break;
            }
            E->behavior_flags = 4;
            break;
        default: // the tumble after a bite: a half turn as it slides
            if (E->animation_frame_id < 0x10) {
                Add_speedXZ(0x800);
            }
            if (CB->swerve < 0x800) {
                E->angle += 0x80;
                CB->swerve += 0x80;
            }
            cb_ballistic(0, CB->launchVy, E->reaction_timer);
            if ((E->t[1] >= 0) && (CB->phase == 0)) {
                Snd_em(5);
                CB->phase = 1;
            }
            if (Joint_move(0, 0x100)) {
                E->behavior_flags = 4;
                E->ignore_player_flag = 0;
                E->move_speed_current = 0;
                CB->aiFlags |= CB_AI_REPROBE;
            }
            break;
    }
}

static void cb_back_to_chase(void) {
    cb_chase_again(4);
    CB->aiFlags &= ~CB_AI_ACTIVE;
    if (!(CB->behFlags & 1)) {
        CB->swerve = 0x28;
    }
}

static void cb_strafe(void) {
    E->angle += CB->turnStep;
    Add_speedXZ(0);
    switch (E->action_behavior) {
        case 0: // side-step for a while; turn the other way if it keeps hitting walls
            if ((s16)E->action_ticks_counter < 1) {
                if (!(CB->probe & 0x11)) {
                    cb_back_to_chase();
                } else if ((CB->aggro++ > 7) && (CB->phase == 0)) {
                    E->action_behavior++;
                    CB->turnStep = -CB->turnStep;
                    CB->aggro = 0;
                    break;
                }
                cb_consider_attack();
            } else {
                E->action_ticks_counter--;
            }
            cb_turn_anim(CB->turnStep);
            break;
        case 1: // still blocked: back up to spring
            if (CB->phase != 0) {
                cb_faster(0x32, 0x82);
            }
            if (!(CB->probe & 0x11)) {
                cb_back_to_chase();
            } else if (CB->aggro++ > 7) {
                E->action_behavior++;
                E->move_speed_current = -130;
                E->animationId = 10;
                E->animation_frame_id = 0;
                E->timing_control = 0;
                CB->aggro = 0;
                break;
            }
            cb_turn_anim(CB->turnStep);
            break;
        default: // the spring
            if (E->animation_frame_id < 0xF) {
                if (E->animation_frame_id > 7) {
                    cb_slower(0x20, 0x28);
                }
                entity_rotate_toward_target(P->t[0], P->t[2], 0x40);
            } else {
                E->move_speed_current = 0;
                CB->turnStep = 0;
            }
            if (E->animation_frame_id > 0x12) {
                cb_chase_again(4);
            }
            break;
    }
    if (Joint_move(0, 0x200)) {
        Snd_em(0);
    }
}

/** Ears up, holding still until the player comes within 6500, then watching, then stalking. */
static void cb_alert(void) {
    u8 frame = E->animation_frame_id;

    Joint_move(0, 0x80);
    E->animation_frame_id = frame;
    if (E->action_behavior == 0) {
        if ((CB->dist < 6500) || (E->behavior_flags == 11)) {
            E->action_behavior++;
            CB->swerve = 0;
            E->animationId = 1;
            E->animation_frame_id = 0;
            E->timing_control = 0;
            E->blend_counter = 0x1F;
            E->action_ticks_counter = (u16)((CbRand() & 0xF) + 0x20);
        }
    } else {
        cb_head_track();
        if (--E->action_ticks_counter == 0) {
            cb_chase_again(12);
        }
    }
}

/** The maul: it holds her down; frame 42 is the kill. */
static void cb_maul(void) {
    if (E->action_behavior > 1) {
        return;
    }
    if (E->action_behavior == 0) {
        E->action_behavior = 1;
        E->animationId = 8;
        E->animation_frame_id = 0;
        E->timing_control = 0;
        E->blend_counter = 0;
        CB->blood = 0;
        CB->swerve = 0;
        snap_player_to_grab_position();
        E->holding = 1;
        P->isBeingAttackedFlag = 1;
        E->move_speed_current = 0;
        return;
    }
    E->t[1] = 0;
    if (E->animation_frame_id < 0x14) {
        snap_player_to_grab_position();
    }
    entity_apply_anim_vertex();
    if (Joint_move(0, 0x100)) {
        E->action_behavior = 2;
    }
    if (E->animation_frame_id == 0x2A) {
        gNvRe1Kill = 1;
    }
    if (E->animation_frame_id == 0x28) {
        Snd_em(3);
    }
    if (E->animation_frame_id == 0x5E) {
        Snd_em(7);
    }
}

/** A close snap (behaviour 8): wait for the player within 4501, snap, then chase. */
static void cb_bite(void) {
    Joint_move(0, 0x100);
    if (E->action_behavior == 0) {
        if (CB->dist < 4501) {
            E->action_behavior++;
            E->animationId++;
            E->animation_frame_id = 0;
            E->timing_control = 0;
            E->blend_counter = 0xF;
            CB->swerve = 0;
        }
        return;
    }
    cb_head_track();
    if (E->animation_frame_id == 33) {
        cb_chase_again(4);
        E->hit_state = 0;
    }
}

/* the wall follow: pick a side, slide along until clear, commit, check again */
static s16 cb_wall_wait(void) {
    s16 r = 0;

    if (CB->probe & 1) {
        E->action_state++;
        r = CB->turnDelta;
    }
    if (CB->phase > 8) {
        CB->phase = 8;
    }
    return r;
}

static s16 cb_wall_follow(s16 phaseLimit, s16 step) {
    s16 sub = 0;

    switch (E->action_state) {
        case 0:
            E->action_state++;
            CB->phase = 0;
            CB->turnDelta = (s16)turn_toward_target(P->t[0], P->t[2], step);
            if (CB->turnDelta == 0) {
                CB->turnDelta = step;
            }
            CB->aiFlags &= ~CB_AI_WALL;
            sub = cb_wall_wait();
            break;
        case 1:
            sub = cb_wall_wait();
            break;
        case 2:
            if ((CB->probe & 0xF) == 0) {
                E->action_state++;
                E->action_ticks_counter = 0;
                CB->turnStep = cb_probe_sides(2, 1);
                if (CB->turnStep == 0) {
                    CB->turnStep = -1;
                }
            }
            E->angle += CB->turnDelta;
            if (CB->phase > 8) {
                CB->phase = 8;
            }
            sub = CB->turnDelta;
            break;
        case 3:
            if ((cb_probe_turn((s16)(CB->turnStep << 10), 1) || (E->action_ticks_counter != 0)) && !(CB->probe & 1)) {
                if ((s16)E->action_ticks_counter > 0) {
                    E->action_ticks_counter--;
                }
            } else {
                E->action_state = 4;
                CB->turnDelta = CB->turnStep;
                E->action_ticks_counter = 0x400;
            }
            sub = 0;
            break;
        default:
            E->angle += CB->turnDelta * step;
            E->action_ticks_counter = (u16)((s16)E->action_ticks_counter - step);
            if ((s16)E->action_ticks_counter < 1) {
                E->action_state = 3;
                E->action_ticks_counter = 4;
            }
            sub = CB->turnDelta;
            break;
    }
    // a clear line to the player after a while: this wall leads nowhere
    if (!cb_probe_to_player(4) && (CB->phase > 0xC)) {
        CB->turnStep = 0;
        CB->aiFlags |= CB_AI_WALL;
    }
    CB->phase++;
    if (phaseLimit < CB->phase) {
        E->action_state = 0;
    }
    return (sub == 0) ? 1 : (sub < 1) ? 0 : 2;
}

/** Circling, working up to it: patience builds near the player (fast if he runs), then a chase. */
static void cb_stalk(void) {
    cb_faster(2, 0x28);
    Add_speedXZ(0);
    switch (E->action_behavior) {
        case 0:
            E->action_behavior++;
            CB->turnDelta = (s16)((CbRand() & 7) + 0x10);
            CB->phase = 0;
            if (CB->swerve < 0) {
                CB->turnDelta = -CB->turnDelta;
            }
            E->action_ticks_counter = (u16)((CbRand() & 0x1F) + 0x2D);
            break;
        case 1:
            E->angle += CB->turnDelta;
            if (--E->action_ticks_counter == 0) {
                s32 a = CbRand() & 7;
                s32 b = CbRand() & 0xF;

                E->action_behavior++;
                E->action_ticks_counter = (u16)(a - b + 0x1E);
            }
            break;
        case 2:
            if ((--E->action_ticks_counter != 0) && !(CB->probe & 1)) {
                break;
            }
            CB->turnDelta = (s16)turn_toward_target(P->t[0], P->t[2], (s16)((CbRand() & 7) + 0x10));
            E->action_behavior = 1;
            E->action_ticks_counter = (u16)((CbRand() & 0x1F) + 0x2D);
            if ((CbRand() & 0x3F) > 0x30) {
                Snd_em(2); // growl
            }
            break;
        default:
            cb_wall_follow(600, 0x10);
            if (CB->aiFlags & CB_AI_WALL) {
                E->action_behavior = 0;
            }
            break;
    }
    Joint_move(0, 0x100);
    if ((E->animation_frame_id == 0) || (E->animation_frame_id == 0x1B)) {
        Snd_em(9);
    }
    cb_head_track();
    if (cb_probe_to_player(8) && (E->action_behavior != 3)) {
        E->action_behavior = 3;
        E->action_state = 0;
    }
    if (CB->dist < 2500) {
        CB->aggro++;
        if (CB->behFlags & 4) {
            CB->aggro = 0x41;
        }
        if (P->speed > 200) {
            CB->aggro += 0x14;
        }
        if ((turn_toward_target(P->t[0], P->t[2], 128) == 0) && (CB->dist > 800)) {
            cb_chase_again(5); // lined up: leap
            sCbBarked = 1;
        }
    } else {
        if (CB->aggro > 0) {
            CB->aggro--;
        }
        if ((turn_toward_target(P->t[0], P->t[2], 0x80) == 0) && (P->speed > 200)) {
            CB->aggro += 6;
        }
    }
    if ((CB->aggro > 0x40) || (sCbBarked && !(CB->behFlags & 4))) {
        cb_chase_again(4);
        CB->swerve = 0;
        if (!sCbBarked) {
            Snd_em(6); // bark
            sCbBarked = 1;
        }
    }
}

static void cb_behave(void) {
    s32 dx = P->t[0] - E->t[0];
    s32 dz = P->t[2] - E->t[2];

    if (E->behavior_flags == 0x80) {
        E->ignore_player_flag = 0;
    }
    CB->dist = CbAbs(dx) + CbAbs(dz);
    CB->path = (u8)entity_pathfind_update();
    E->status_flags = (E->status_flags & 0x1F) | 0x40;
    if (CB->alert == 0) {
        entity_check_visual_range(5000);
    }
    if (E->ignore_player_flag == 0) {
        cb_select();
        E->action_behavior = 0;
        E->action_state = 0;
    }
    switch (E->ignore_player_flag) {
        case 1:
            cb_patrol();
            break;
        case 2:
            cb_chase();
            break;
        case 3:
            cb_leap();
            break;
        case 4:
            cb_strafe();
            break;
        case 5:
            cb_alert();
            break;
        case 6:
            cb_maul();
            break;
        case 7:
            cb_bite();
            break;
        case 8: // the script holds it
            E->ignore_player_flag = 0;
            Joint_move(0, 0x100);
            break;
        case 9:
            cb_stalk();
            break;
        default:
            break;
    }
}

/* ---- hit and death ---- */

static void cb_land_from_knock(void) {
    Add_speedXZ(CB->turnDelta);
    cb_ballistic(0, CB->launchVy, E->reaction_timer);
    if (E->t[1] >= 0) {
        E->ignore_player_flag++;
        E->animationId++;
        E->animation_frame_id = 0;
        E->timing_control = 0;
        E->blend_counter = 0;
        E->t[1] = 0;
        Snd_em(1);
    }
    if (E->animation_frame_id != 0) {
        Joint_move(0, 0x200);
    }
}

static void cb_damaged(void) {
    E->status_flags &= 0x1F;
    if (!(CB->aiFlags & CB_AI_HURT)) {
        E->status_flags |= 0x40;
    }
    entity_check_visual_range(5000);
    switch (E->ignore_player_flag) {
        case 0: {
            u8 beh = E->behavior_flags;

            E->animation_frame_id = 0;
            E->timing_control = 0;
            E->blend_counter = 3;
            CB->aiFlags |= CB_AI_HURT;
            // in the air or side-stepping a hit only staggers it; otherwise it's knocked about
            if (!(((beh >= 4) && (beh <= 7)) || (beh == 9)) && (E->hit_state & 1)) {
                E->ignore_player_flag = 1;
                E->animationId = 9;
            } else {
                CB->turnDelta = (s16)((P->angle & 0xFFF) - (E->angle & 0xFFF)) + sCbRecoil[CbRand() & 7];
                if ((beh == 4) || ((CB->alert & 0x7F) == 0)) {
                    E->ignore_player_flag = 3;
                    E->animationId = 0x0C;
                    E->blend_counter = 0;
                    E->move_speed_current = 0xF0;
                    Snd_em(1);
                } else {
                    E->ignore_player_flag = 2;
                    E->animationId = 0x0B;
                    E->move_speed_current >>= 1;
                    CB->alert |= 0x80;
                }
            }
            E->action_behavior = 0;
            Joint_move(0, 0x400);
            Snd_em(8); // yelp
            break;
        }
        case 1: { // stagger, then get up facing the player
            s32 done = Joint_move(0, 0x400);

            if (E->action_behavior == 0) {
                if (done) {
                    E->action_behavior++;
                    E->animationId = 10;
                    E->timing_control = 0;
                    E->move_speed_current = 0x82;
                }
            } else {
                if (E->animation_frame_id < 0xF) {
                    if (E->animation_frame_id > 7) {
                        cb_slower(0x20, 0x28);
                    }
                    Add_speedXZ(0x800);
                    entity_rotate_toward_target(P->t[0], P->t[2], 0x40);
                }
                if (done) {
                    E->state = 1;
                    cb_chase_again(4);
                    E->hit_state = 0;
                }
            }
            break;
        }
        case 2:
            cb_land_from_knock();
            break;
        case 3: // skid along the floor, roll over, up
            if (E->animationId == 0x0C) {
                Add_speedXZ(CB->turnDelta);
                cb_slower(10, 0);
            }
            if (Joint_move(0, 0x400)) {
                E->animationId++;
                E->hit_state = 0;
                if (E->animationId == 0x0E) {
                    CB->aiFlags &= ~CB_AI_HURT;
                    if (turn_toward_target(P->t[0], P->t[2], 0x200) == 0) {
                        E->ignore_player_flag++;
                        E->animationId = 3;
                    } else {
                        E->ignore_player_flag = 1;
                        E->action_behavior = 1;
                        E->animationId = 10;
                        E->move_speed_current = 0x82;
                    }
                }
                E->timing_control = 0;
                E->blend_counter = 3;
            }
            break;
        case 4:
            if ((E->animation_frame_id == 10) && (E->timing_control == 1)) {
                Snd_em(2);
            }
            if (Joint_move(0, 0x400)) {
                E->state = 1;
                cb_chase_again(4);
                E->hit_state = 0;
                CB->alert &= 0x7F;
                Snd_em(6);
            }
            break;
        default:
            break;
    }
}

static void cb_die(void) {
    switch (E->ignore_player_flag) {
        case 0:
            if (E->death_event_id != 0xFF) {
                void NvFlags_Op(s32 bank, s32 bit, s32 mode);

                NvFlags_Op(3, E->death_event_id, 0);
            }
            E->animation_frame_id = 0;
            E->timing_control = 0;
            E->blend_counter = 7;
            E->action_behavior = 0;
            CB->turnDelta = (s16)((P->angle & 0xFFF) - (E->angle & 0xFFF));
            if ((E->behavior_flags == 4) || ((CB->alert & 0x7F) == 0)) {
                E->ignore_player_flag = 2;
                E->animationId = 0x0C;
                E->blend_counter = 0;
                E->move_speed_current = 0xF0;
            } else {
                E->ignore_player_flag = 1;
                E->animationId = 0x0B;
                E->move_speed_current >>= 1;
            }
            Joint_move(0, 0x200);
            Snd_em(4); // death
            break;
        case 1:
            cb_land_from_knock();
            break;
        case 2:
            if (E->animationId == 0x0C) {
                Add_speedXZ(CB->turnDelta);
                cb_slower(10, 0);
            }
            if (Joint_move(0, 0x200)) {
                E->animationId++;
                E->timing_control = 0;
            }
            if (E->animationId == 0x0D) {
                E->ignore_player_flag++;
                E->status_flags |= 10; // intangible, dead
                E->move_speed_current = 0;
                E->action_ticks_counter = 0x46;
            }
            break;
        case 3:
            if ((s16)E->action_ticks_counter == 0) {
                E->ignore_player_flag++;
            } else {
                E->action_ticks_counter--;
            }
            break;
        default:
            break;
    }
}

/* ---- entry points ---- */

void NvCerberus_Init(NvRe1Entity* e) {
    s32 x;
    s32 z;

    E = e;
    E->state = 1;
    E->ignore_player_flag = 0;
    E->action_behavior = 0;
    E->action_state = 0;
    CbRand();
    E->health = (s16)(sCbHealth[CbRand() & 0xF] + (CbRand() & 3));
    E->hit_state = 0;
    memset(&E->cb, 0, sizeof(E->cb));
    CB->behFlags = 1;
    CB->aiFlags = CB_AI_ACTIVE;
    E->sca = sCbSca;
    E->status_flags &= 0x1F;
    E->animationId = 0;
    E->animation_frame_id = 0;
    E->timing_control = 0;
    E->action_ticks_counter = 0;
    E->move_speed_current = 0;
    sCbBarked = 0;
    RotY(E->angle, sCbSca[1], 0, &x, &z); // (RE1 moves its 16-bit position 500 ahead)
    E->pos[0] = (s16)(E->pos[0] + x);
    E->pos[2] = (s16)(E->pos[2] + z);
    // the entrances through a window or from above start in the air, where the script put it
    if ((E->behavior_flags >= 6) && ((E->behavior_flags <= 7) || (E->behavior_flags == 0x80))) {
        E->status_flags |= 4;
        return;
    }
    E->t[1] = 0;
}

void NvCerberus_Update(NvRe1Entity* e) {
    s32 x0;
    s32 z0;

    E = e;
    x0 = E->t[0];
    z0 = E->t[2];
    E->pos[0] = (s16)E->t[0];
    E->pos[1] = (s16)E->t[1];
    E->pos[2] = (s16)E->t[2];
    switch (E->state) {
        case 0:
            NvCerberus_Init(e);
            break;
        case 1:
            cb_behave();
            break;
        case 2:
            cb_damaged();
            break;
        case 3:
            cb_die();
            break;
        default:
            break;
    }
    SetScaWorld(E->sca, E->angle, E->scaWorld);
    SetScaWorld(P->sca, P->angle, P->scaWorld);
    if (!(E->status_flags & 2)) {
        ResolveScaPush(P->sca, P->scaWorld, P->t, E);
        HandleEnemyCollisions();
    }
    E->collisionFlags &= ~0x08;
    check_room_collision();
    // the probe history: this tick's into bit 0, the last three shifted up
    CB->probe = (s16)((CB->probe & 7) * 2);
    if (!(E->status_flags & 4)) {
        CB->probe |= cb_probe_ahead();
    }
    if (!(CB->probe & 1) && (E->t[0] == x0) && (E->t[2] == z0)) {
        CB->probe |= 0x10;
    }
}
