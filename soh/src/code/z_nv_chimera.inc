/*
 * Nightmareverse: RE1's Chimeras (enemy type 9, EM1009), the lab's. Included at the end of
 * z_nv_zombie.c with the other ported enemies.
 *
 * As RE1 runs them (the decomp's entities/Chimera.cpp is the reference), by the variant:
 * - on the floor (0, crouched; 1, upright): it stalks her when it sees her, turns at her when it
 *   doesn't, and close and in front swipes (20 of RE1's health) or claws up at her; from the
 *   claw, catching her facing it, it grabs her and bites (10). Crouched, it leaps up to the
 *   ceiling when she raises a gun far off; upright, it turns about between the two stances.
 * - on the ceiling (2, upside down): it crawls away from her when it can't see her, round at
 *   her when it can, and when she's under it drops onto her (grabbing her if she faces it) or
 *   swoops down and back up.
 * - 3: the hard ones (a floor variant that switches stance mid-fight).
 * Hit, it staggers or is knocked down off the ceiling; killed, it dissolves.
 *
 * Left out, each a note where it would be: the acid it bleeds when hurt badly, its claws' reach
 * (RE1 measures its hand joints to her; here a 2000 cone in front), the tint where the claws
 * land, and the hard ones' extra touch attack.
 */

#define CM (&E->cm)

static const s16 sCmSca[8] = { (s16)0x8000, 0, -180, 0, 180, 500, 0, 0 };

// the ceiling dive's arc: its height below the ceiling by the dive animation's frame
static const s16 sCmArc[33] = { 1540, 1225, 1040, 1290, 1625, 2100, 2500, 2900, 3200, 3500, 3800,
                                4100, 4300, 4500, 4600, 4700, 4700, 4700, 4700, 4700, 4700, 4700,
                                4700, 4700, 4700, 4800, 4900, 5000, 5068, 5068, 5068, 5068, 5068 };

#define CM_CEILING_Y (-6008)

enum { CM_IDLE, CM_WALK, CM_TURN, CM_SWIPE, CM_GRAB, CM_STANCE, CM_DROP, CM_SWOOP, CM_SPIT, CM_CRAWL_AWAY,
       CM_TURN_BACK, CM_STANCE_QUICK, CM_CLAW, CM_FALL };

static void cm_take(u8 behaviour) {
    E->ignore_player_flag = 1;
    E->action_behavior = behaviour;
    E->action_state = 0;
}

static void cm_free(void) {
    E->state = 1;
    E->ignore_player_flag = 0;
    E->action_behavior = CM_IDLE;
    E->action_state = 0;
}

static void cm_anim(u8 id, u8 blend) {
    E->animation_frame_id = 0;
    E->timing_control = 0;
    E->blend_counter = blend;
    E->animationId = id;
}

/** She's in its reach: within a cone of `cone` and `range`, nothing in between. */
static s32 cm_reaches(s16 cone, s32 range) {
    s32 dx = P->t[0] - E->t[0];
    s32 dz = P->t[2] - E->t[2];

    return (turn_toward_target(P->t[0], P->t[2], cone) == 0) && (dx * dx + dz * dz < range * range) &&
           !check_line_of_sight(P->t[0], P->t[2]);
}

static s32 cm_free_to_hit(void) {
    return (P->isBeingAttackedFlag == 0) && !gNvRe1DogFlinch;
}

/* ---- the brains, by the variant ---- */

/** Crouched on the floor. */
static void cm_brain_crouched(void) {
    if (CM->sees) {
        E->ignore_player_flag = 0;
        if (E->action_behavior != CM_WALK) {
            E->action_behavior = CM_WALK;
            E->action_state = 0;
            E->animationId = 9;
            E->move_speed_current = 0xB4;
        }
    }
    if (cm_reaches(0x200, 1500) && cm_free_to_hit()) {
        cm_take(is_facing_toward_entity() ? CM_CLAW : CM_SWOOP);
        if (CM->hard) {
            E->action_behavior = CM_CLAW;
            if (CM->repause != 0) {
                E->action_behavior = CM_STANCE;
            }
        }
        return;
    }
    if ((!CM->sees && (CM->dist < 6000)) || (CM->wallTicks > 0x14)) {
        cm_take(CM_STANCE_QUICK);
        CM->wallTicks = 0;
    }
    // she raises a gun far off, turned away: up to the ceiling
    if ((P->flags & 0x20) && (CM->dist > 2000) && !is_facing_toward_entity()) {
        cm_take(CM->hard ? CM_STANCE_QUICK : ((CbRand() & 1) ? CM_STANCE_QUICK : CM_SWOOP));
    }
    if (P->isBeingAttackedFlag & 0x80) {
        cm_take(CM_FALL);
    }
}

/** Upright on the floor. */
static void cm_brain_upright(void) {
    if (CM->sees) {
        E->ignore_player_flag = 0;
        if (E->action_behavior != CM_WALK) {
            E->action_behavior = CM_WALK;
            E->action_state = 0;
            if (CM->dist > 5000) {
                CM->far = 1;
            }
            E->animationId = CM->far ? 2 : 3;
            E->move_speed_current = CM->far ? 200 : 100;
        }
    }
    if (cm_reaches(0x200, 1500) && cm_free_to_hit() && (CbRand() & 1)) {
        cm_take(is_facing_toward_entity() ? CM_CLAW : CM_SPIT);
        if (CM->hard && (CM->repause != 0)) {
            E->action_behavior = CM_STANCE;
        }
        CM->far = 0;
        return;
    }
    if ((!CM->sees && (CM->dist < 8000)) || (CM->wallTicks > 0x14)) {
        cm_take(CM_TURN);
        E->animationId = 3;
        CM->turnStep = 0x20;
        if ((CM->dist < 4000) || CM->far) {
            E->animationId = 2;
            CM->turnStep = 0x40;
        }
        CM->wallTicks = 0;
        CM->far = 0;
    }
    if (CM->sees && !is_facing_toward_entity() && (CM->dist > 6000) &&
        (turn_toward_target(P->t[0], P->t[2], 0x40) == 0) && (P->speed > 150)) {
        cm_take(CM_STANCE_QUICK); // she runs off: it drops to a crouch
        CM->far = 0;
    }
    if (P->isBeingAttackedFlag & 0x80) {
        cm_take(CM_FALL);
    }
}

/** On the ceiling (it faces backward along its crawl: its facing is turned half round here). */
static void cm_brain_ceiling(void) {
    E->angle = (E->angle + 0x800) & 0xFFF;
    if (!CM->sees) {
        E->ignore_player_flag = 0;
        if (E->action_behavior != CM_CRAWL_AWAY) {
            E->action_behavior = CM_CRAWL_AWAY;
            E->action_state = 0;
            if (CM->dist > 3000) {
                CM->far = 1;
            }
        }
    }
    if (CM->sees || (CM->wallTicks > 0x1E)) {
        cm_take(CM_TURN_BACK);
        E->animationId = 3;
        CM->turnStep = 0x20;
        if ((CM->dist < 4000) || CM->far) {
            E->animationId = 2;
            CM->turnStep = 0x40;
        }
        CM->wallTicks = 0;
        CM->far = 0;
    }
    if ((CM->repause == 0) && (CM->dist < 2000) && cm_free_to_hit()) {
        cm_take(is_facing_toward_entity() ? CM_DROP : CM_SWIPE);
        CM->far = 0;
    }
    if ((CM->dist > 15000) && (turn_toward_target(P->t[0], P->t[2], 0x200) == 0)) {
        cm_take(CM_DROP);
        CM->far = 0;
    }
    if (P->isBeingAttackedFlag & 0x80) {
        cm_take(CM_FALL);
    }
    E->angle = (E->angle - 0x800) & 0xFFF;
}

/* ---- the moves ---- */

static void cm_idle(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        cm_anim(E->behavior_flags ? 1 : 0, 3);
        if (E->behavior_flags == 2) {
            E->action_ticks_counter = (CbRand() & 0x3F) + (CM->sees ? 0 : 0x50);
        }
    }
    Joint_move(0, 0x400);
    if ((E->behavior_flags == 2) && (E->action_ticks_counter-- == 0)) {
        E->ignore_player_flag = 0;
        E->action_behavior = CM_IDLE;
        E->action_state = 0;
    }
}

/** A walk for a while (the stalk, or the crawl away on the ceiling). */
static void cm_walk_for_a_while(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        E->animation_frame_id = 0;
        E->timing_control = 0;
        E->blend_counter = 3;
        E->action_ticks_counter = (u16)((CbRand() & 0x1F) * E->action_behavior);
        if (E->action_behavior < 2) {
            E->action_ticks_counter += 0x50;
        }
    }
    Joint_move(0, 0x400);
    if (E->action_ticks_counter-- == 0) {
        E->ignore_player_flag = 0;
        E->action_behavior = CM_IDLE;
        E->action_state = 0;
    }
}

static void cm_walk(void) {
    if (CM->sees) {
        zone_path_find(P->t[0], P->t[2], &E->player_pos_x, &E->player_pos_z);
    }
    cm_walk_for_a_while();
    Add_speedXZ(0);
    if ((((E->animation_frame_id % 6) == 0) && (E->animationId == 2)) ||
        (((E->animation_frame_id % 10) == 0) && (E->animationId == 3)) ||
        (((E->animation_frame_id % 15) == 0) && (E->animationId == 9))) {
        Snd_em(0);
    }
}

static void cm_crawl_away(void) {
    E->animationId = CM->far ? 2 : 3;
    E->move_speed_current = CM->far ? 200 : 100;
    cm_walk_for_a_while();
    Add_speedXZ(0x800);
    if ((E->animation_frame_id == 0) && (E->animationId == 2)) {
        Snd_em(2);
    }
}

/** A turn at her (backward = the ceiling's, whose facing is half round). */
static void cm_turn(s32 backward) {
    s32 turn;

    if (E->action_state == 0) {
        E->animation_frame_id = 0;
        E->timing_control = 0;
        E->blend_counter = 3;
        E->action_state = 1;
        E->action_ticks_counter = backward ? 0x50 : (CbRand() & 0x1F) + 0x50;
    } else if (E->action_state == 2) {
        E->ignore_player_flag = 0;
        E->action_behavior = CM_IDLE;
        E->action_state = 0;
        if (!backward && CM->sees) {
            CM->far = 1;
        }
        E->player_pos_x = (s16)P->t[0];
        E->player_pos_z = (s16)P->t[2];
        return;
    }
    if (backward) {
        E->angle = (E->angle + 0x800) & 0xFFF;
    }
    turn = turn_toward_target(P->t[0], P->t[2], CM->turnStep);
    if ((E->action_ticks_counter-- == 0) || (turn == 0)) {
        E->action_state = 2;
    }
    Joint_move(0, 0x400);
    E->angle = (E->angle + turn) & 0xFFF;
    if (backward) {
        E->angle = (E->angle - 0x800) & 0xFFF;
    }
}

/** The swipe: on frames 7-12 its claws land if she's in reach. */
static void cm_swipe(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        cm_anim(8, 3);
        Snd_em(3);
    } else if (E->action_state == 2) {
        E->ignore_player_flag = 0;
        E->action_behavior = CM_IDLE;
        E->action_state = 0;
        CM->repause = 0x3C;
        return;
    }
    if (((u8)(E->animation_frame_id - 7) < 6) && cm_free_to_hit() && cm_reaches(0x300, 2000)) {
        gNvRe1DogBite = 20;
        Snd_em(5);
    }
    if (Joint_move(0, 0x400)) {
        E->action_state = 2;
    }
}

/** The grab: it seizes her and bites at frame 34 (10); her mashing doesn't free her. */
static void cm_grab(void) {
    if (E->action_state == 0) {
        E->animation_frame_id = 10;
        E->timing_control = 0;
        E->action_state = 1;
        E->blend_counter = 0;
        E->angle = P->angle;
        E->t[1] = 0;
        E->pitch = 0;
        E->status_flags |= 2;
        E->animationId = 5;
        E->unk_c6 = (s16)P->t[0];
        E->unk_c8 = (s16)P->t[2];
        E->hit_state = 1;
        E->holding = 1;
    } else if (E->action_state == 2) {
        E->holding = 0;
        E->hit_state = 0;
        E->behavior_flags = 1;
        E->status_flags &= ~2;
        cm_take(CM_SWOOP);
        if (CM->hard) {
            E->action_behavior = CM_STANCE;
            CM->repause = 0x5A;
        }
        return;
    } else if (E->action_state != 1) {
        return;
    }
    if (Joint_move(0, 0x400)) {
        E->action_state = 2;
    }
    if (E->animation_frame_id == 0x22) {
        Snd_em(6);
        gNvRe1Drain = 10;
    }
}

/** A change of stance: crouched <-> upright (the hard ones' fight move). */
static void cm_stance(s32 quick) {
    switch (E->action_state) {
        case 0:
            E->action_state = 1;
            if (!quick && (E->behavior_flags != 0)) {
                E->action_state = 3;
                return;
            }
            cm_anim(4, 3);
            Snd_em(0);
            // fall through
        case 1:
            if (Joint_move((s32)E->behavior_flags, 0x400)) {
                E->action_state = 2;
            }
            return;
        case 2:
            Snd_em(0);
            E->behavior_flags = (E->behavior_flags + 1) & 1;
            if (quick) {
                cm_free();
                return;
            }
            E->action_state = 3;
            // fall through
        case 3:
            E->action_state = 4;
            cm_anim(7, 3);
            CM->far = 0;
            // fall through
        case 4:
            E->action_state += (u8)Joint_move(0, 0x400);
            return;
        default:
            E->action_state = 0;
            if (CM->repause == 0) {
                cm_free();
            }
            return;
    }
}

/** Off the ceiling onto her: it falls along its arc, grabbing her at frame 25 if she faces it. */
static void cm_drop(void) {
    switch (E->action_state) {
        case 0:
            E->action_state = 1;
            cm_anim(0x10, 0);
            E->action_ticks_counter = 0;
            E->hit_state = 1;
            E->status_flags |= 2;
            // fall through
        case 1:
            E->t[1] = -sCmArc[32 - E->animation_frame_id];
            Joint_move(1, 0x400);
            E->action_state = 2;
            return;
        case 2:
            E->pitch = 0; // right way up
            E->action_state = 3;
            E->blend_counter = 0;
            E->behavior_flags = 1;
            Snd_em(3);
            // fall through
        case 3:
            E->angle = (E->angle + turn_toward_target(P->t[0], P->t[2], 0x20)) & 0xFFF;
            if ((E->animation_frame_id == 0x19) && cm_reaches(0x400, 2000) && cm_free_to_hit() &&
                is_facing_toward_entity()) {
                E->action_behavior = CM_GRAB;
                E->action_state = 0;
                E->angle = getAngleTowardsTarget(P->t[0], P->t[2]);
                E->hit_state = 0;
                return;
            }
            E->t[1] = -sCmArc[32 - MIN(E->animation_frame_id, 32)];
            E->move_speed_current = ((E->animation_frame_id >= 6) && (E->animation_frame_id < 16)) ? 150 : 0;
            Add_speedXZ(0x800);
            E->action_state += (u8)Joint_move(1, 0x400);
            if (E->animation_frame_id == 0x1F) {
                Snd_em(4);
            }
            return;
        case 4:
            E->hit_state = 0;
            cm_anim(3, 0);
            Joint_move(0, 0x40);
            E->status_flags &= ~2;
            E->t[1] = 0;
            E->ignore_player_flag = 0;
            E->action_behavior = CM_IDLE;
            E->action_state = 0;
            return;
        default:
            return;
    }
}

/** The swoop: down along its arc past her and back up onto the ceiling. */
static void cm_swoop(void) {
    switch (E->action_state) {
        case 0:
            E->action_state = 1;
            cm_anim(0x10, 0);
            E->action_ticks_counter = 0;
            E->hit_state = 1;
            Snd_em(1);
            // fall through
        case 1:
            E->t[1] = -sCmArc[MIN(E->animation_frame_id, 32)];
            E->move_speed_current = ((E->animation_frame_id >= 18) && (E->animation_frame_id < 28)) ? 150 : 0;
            Add_speedXZ(0);
            Joint_move(0, 0x400);
            if (E->animation_frame_id == 0x20) {
                E->action_state = 2;
            }
            return;
        case 2:
            E->t[1] = -sCmArc[32];
            E->pitch = 0x800; // upside down again
            E->action_state = 3;
            E->blend_counter = 0;
            E->behavior_flags = 2;
            Snd_em(2);
            // fall through
        case 3:
            E->action_state += (u8)Joint_move(0, 0x400);
            return;
        case 4:
            E->hit_state = 0;
            CM->repause = 0x5A;
            E->status_flags &= ~2;
            cm_anim(1, 0);
            Joint_move(0, 0x40);
            E->t[1] = CM_CEILING_Y;
            E->ignore_player_flag = 0;
            E->action_behavior = CM_IDLE;
            E->action_state = 0;
            return;
        default:
            return;
    }
}

/** The upright one's strike from above (frames 10-12: 15). */
static void cm_spit(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        cm_anim(0xE, 3);
        Snd_em(3);
    } else if (E->action_state == 2) {
        if (CM->hard) {
            CM->repause = 0x28;
        }
        E->behavior_flags = 0;
        cm_free();
        return;
    }
    if (((u8)(E->animation_frame_id - 10) < 3) && cm_reaches(0x200, 2000) && cm_free_to_hit()) {
        gNvRe1DogBite = 15;
        Snd_em(6);
    }
    if (Joint_move(0, 0x400)) {
        E->action_state = 2;
    }
}

/** The claw up at her: at frame 10, if she faces it in reach, it grabs her. */
static void cm_claw(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        cm_anim(10, 3);
        Snd_em(1);
    } else if (E->action_state == 2) {
        E->behavior_flags = 1;
        cm_free();
        return;
    }
    if (CM->sees) {
        E->angle = (E->angle + turn_toward_target(P->t[0], P->t[2], 0x20)) & 0xFFF;
    }
    if ((E->animation_frame_id == 10) && cm_reaches(0x200, 2000) && cm_free_to_hit() && is_facing_toward_entity()) {
        E->action_behavior = CM_GRAB;
        E->action_state = 0;
        E->angle = getAngleTowardsTarget(P->t[0], P->t[2]);
        E->hit_state = 1;
        return;
    }
    if (Joint_move(0, 0x400)) {
        E->action_state = 2;
    }
    Add_speedXZ(0);
    if (E->animation_frame_id == 0x17) {
        Snd_em(4);
    }
}

/** She's been knocked down: it crouches over her. */
static void cm_fall(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        cm_anim((E->behavior_flags == 2) ? 0 : 6, 3);
        Snd_em(8);
    }
    Joint_move(0, 0x400);
}

static void cm_act(void) {
    u32 path = entity_pathfind_update();

    if (!(path & 0xFE)) {
        CM->sees = (u8)(path & 1);
    }
    switch (E->action_behavior) {
        case CM_IDLE:
            cm_idle();
            break;
        case CM_WALK:
            cm_walk();
            break;
        case CM_TURN:
            cm_turn(false);
            break;
        case CM_SWIPE:
            cm_swipe();
            break;
        case CM_GRAB:
            cm_grab();
            break;
        case CM_STANCE:
            cm_stance(false);
            break;
        case CM_DROP:
            cm_drop();
            break;
        case CM_SWOOP:
            cm_swoop();
            break;
        case CM_SPIT:
            cm_spit();
            break;
        case CM_CRAWL_AWAY:
            cm_crawl_away();
            break;
        case CM_TURN_BACK:
            cm_turn(true);
            break;
        case CM_STANCE_QUICK:
            cm_stance(true);
            break;
        case CM_CLAW:
            cm_claw();
            break;
        default:
            cm_fall();
            break;
    }
}

static void cm_run(void) {
    if (E->behavior_flags & 0x80) {
        return; // held by its script
    }
    if (E->ignore_player_flag == 0) {
        CM->dist = CbAbs(P->t[2] - E->t[2]) + CbAbs(P->t[0] - E->t[0]);
        switch (E->behavior_flags) {
            case 0:
                cm_brain_crouched();
                break;
            case 1:
                cm_brain_upright();
                break;
            default:
                cm_brain_ceiling();
                break;
        }
        cm_act();
    } else if (E->ignore_player_flag == 1) {
        cm_act();
    }
    entity_check_visual_range(4000);
    if (E->behavior_flags & 2) {
        entity_check_alert_range(5000);
    }
    if (CM->repause != 0) {
        CM->repause--;
    }
    CM->wallTicks = CM->coll ? CM->wallTicks + 1 : 0;
}

/** A hit: staggered back (the floor ones), or knocked off the ceiling. */
static void cm_hurt(void) {
    if (E->ignore_player_flag == 0) {
        static const u8 sKind[8] = { 0, 2, 0, 2, 0, 4, 4, 0 }; // by the hit's strength
        u8 kind = sKind[E->hit_state & 7];

        E->holding = 0;
        if (turn_toward_target(P->t[0], P->t[2], 0x400) != 0) {
            kind++; // from behind
        }
        if (E->behavior_flags == 2) {
            kind = 4; // on the ceiling: down it comes
        }
        cm_take(kind);
    }
    if (E->action_behavior < 4) { // the stagger
        if (E->action_state == 0) {
            E->action_state = 1;
            cm_anim((E->action_behavior == 0) ? 0xD : (E->action_behavior == 1) ? 0xC : (E->behavior_flags ? 6 : 0xF), 3);
            E->move_speed_current = 0xA0;
            Snd_em(7);
        } else if (E->action_state == 2) {
            E->hit_state = 0;
            cm_free();
            if (CbRand() & 1) {
                cm_take(CM->hard ? CM_CLAW : CM_SWOOP);
            }
            return;
        }
        if (Joint_move(0, 0x400)) {
            E->action_state = 2;
        }
        if (E->action_behavior < 2) {
            if (E->animation_frame_id > 9) {
                E->move_speed_current = 0x3C;
            }
            Add_speedXZ((1 - E->action_behavior) * 0x800);
        }
        return;
    }
    // knocked down
    switch (E->action_state) {
        case 0:
            E->action_state = 1;
            cm_anim(0xB, 0);
            if (E->behavior_flags == 2) {
                cm_anim(0x10, 0);
            }
            E->action_ticks_counter = 0;
            // fall through
        case 1:
            if (E->behavior_flags == 2) {
                E->t[1] = -sCmArc[32 - MIN(E->animation_frame_id, 32)];
            }
            Joint_move(1, 0x400);
            E->action_state = 2;
            return;
        case 2:
            if (E->t[1] < 0) {
                E->t[1] += 1000;
            }
            E->pitch = 0;
            E->action_state = 3;
            E->blend_counter = 0;
            E->behavior_flags = 1;
            E->action_ticks_counter = (u16)(((0x82 - E->health) >> 2) + 0x14);
            // fall through
        case 3:
            E->t[1] += (E->animation_frame_id * 5 + 10) * 10;
            if (E->t[1] > 0) {
                E->t[1] = 0;
                if (E->hit_state > 1) {
                    Snd_em(4);
                    E->hit_state = 1;
                }
            }
            if (Joint_move(0, 0x400)) {
                E->action_state = 4;
            }
            return;
        case 4: // down a while
            if (E->action_ticks_counter-- == 0) {
                E->action_state = 5;
            }
            return;
        case 5:
            E->action_state = 6;
            cm_anim(0x11, 0);
            // fall through
        case 6: // getting up
            if (E->animation_frame_id == 0x16) {
                E->angle = (E->angle + 0x800) & 0xFFF;
            }
            E->action_state += (u8)Joint_move(0, 0x400);
            return;
        default:
            E->hit_state = 0;
            cm_free();
            if (CbRand() & 1) {
                cm_take(CM_SWOOP);
            }
            return;
    }
}

/** Death: it staggers, falls (off the ceiling first, if it's there) and dissolves. */
static void cm_die(void) {
    if (E->ignore_player_flag == 0) {
        E->holding = 0;
        cm_take((u8)(turn_toward_target(P->t[0], P->t[2], 0x400) != 0));
        if (E->behavior_flags & 2) {
            E->action_behavior = 4;
        }
    }
    switch (E->action_state) {
        case 0:
            E->action_state = 1;
            cm_anim((E->action_behavior == 1) ? 0xC : 0xB, 3);
            E->move_speed_current = 0xA0;
            E->action_ticks_counter = 0x3C;
            Snd_em(8);
            // fall through
        case 1:
            if (E->action_behavior == 4) { // off the ceiling
                E->t[1] += (E->animation_frame_id * 5 + 10) * 10;
                E->pitch = 0;
                if (E->t[1] >= 0) {
                    E->t[1] = 0;
                    E->action_state = 2;
                }
                Joint_move(0, 0x400);
                return;
            }
            if (Joint_move(0, 0x400)) {
                E->action_state = 2;
            }
            if (E->animation_frame_id > 9) {
                E->move_speed_current = 0x3C;
            }
            Add_speedXZ((1 - E->action_behavior) * 0x800);
            if ((E->animation_frame_id == 0xF) && (E->action_behavior != 0)) {
                E->action_state = 2;
            }
            return;
        case 2:
            E->action_state = 3;
            E->status_flags |= 10; // intangible, dead
            // fall through
        case 3:
            if (E->action_ticks_counter-- == 0) {
                E->action_state = 4;
                if (E->death_event_id != 0xFF) {
                    void NvFlags_Op(s32 bank, s32 bit, s32 mode);

                    NvFlags_Op(3, E->death_event_id, 0);
                }
            }
            return;
        default:
            return;
    }
}

void NvChimera_Init(NvRe1Entity* e) {
    E = e;
    memset(&E->cm, 0, sizeof(E->cm));
    E->state = 1;
    E->ignore_player_flag = 0;
    E->action_behavior = CM_IDLE;
    E->action_state = 0;
    E->action_ticks_counter = 0;
    E->death_timer = 0;
    E->hit_state = 0;
    if (E->behavior_flags == 3) { // a hard one
        CM->hard = 1;
        E->behavior_flags = CbRand() & 1;
    }
    E->pitch = 0;
    if (E->behavior_flags < 2) {
        E->t[1] = 0;
    } else { // on the ceiling, upside down
        E->pitch = 0x800;
        E->t[1] = CM_CEILING_Y;
        E->behavior_flags = 2;
    }
    CbRand();
    E->health = (s16)((CbRand() & 7) * 2 + (CbRand() & 7) * 2 + (CbRand() & 7) * 2 + 0x50);
    cm_anim(E->behavior_flags, 0);
    E->sca = sCmSca;
    Joint_move(0, 0x40);
}

void NvChimera_Update(NvRe1Entity* e) {
    E = e;
    E->pos[0] = (s16)E->t[0];
    E->pos[1] = (s16)E->t[1];
    E->pos[2] = (s16)E->t[2];
    switch (E->state) {
        case 0:
            NvChimera_Init(e);
            break;
        case 1:
            cm_run();
            break;
        case 2:
            cm_hurt();
            break;
        case 3:
            cm_die();
            break;
        default:
            break;
    }
    if (!(E->status_flags & 2) && (E->pitch == 0)) {
        SetScaWorld(E->sca, E->angle, E->scaWorld);
        SetScaWorld(P->sca, P->angle, P->scaWorld);
        CM->touch = (u8)ResolveScaPush(P->sca, P->scaWorld, P->t, E);
        HandleEnemyCollisions();
    }
    CM->coll = check_room_collision();
}
