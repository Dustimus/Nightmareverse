/*
 * Nightmareverse: RE1's snakes (the adders, enemy type 10, EM100A), the courtyard's and the
 * guardhouse's. Included at the end of z_nv_zombie.c with the other ported enemies.
 *
 * As RE1 runs them (the decomp's entities/Adder.cpp is the reference): by the spawn kind a snake
 * is on the floor (0), on the ceiling dropping when she comes within 3000 (1), on the ceiling
 * dropping right onto her after a while or when she raises a gun (2), or hidden in the scenery
 * coming out when she's within 4500 (3). On the floor it slithers at her, wobbling, and strikes
 * when she's close and in front (6 of RE1's health), recoiling after; one that can't see her or
 * gets stuck turns away. Shot it flinches; killed it writhes and lies still, and one killed by
 * a strong hit pulls back up into the ceiling and drops again later.
 *
 * Left out, each a note where it would be: the head's
 * reach (RE1 measures its head joint to her; here the strike frame lands within 1000 in front),
 * and the shadow.
 */

#define AD (&E->ad)

static const s16 sAdSca[8] = { (s16)0x8000, 0, 0, 0, 0, 200, 0, 0 };

enum { AD_CHASE, AD_DROP, AD_TURN, AD_BITE, AD_GLOAT, AD_WAIT, AD_EMERGE };

static void ad_pick(u8 behaviour) {
    E->ignore_player_flag = 1;
    E->action_behavior = behaviour;
    E->action_state = 0;
}

static void ad_start(u8 anim) {
    E->action_state = 1;
    E->animation_frame_id = 0;
    E->timing_control = 0;
    E->death_timer = 0;
    E->blend_counter = 3;
    E->animationId = anim;
}

/* ---- what it does next, by the spawn kind ---- */

static void ad_decide(void) {
    s32 lined = turn_toward_target(P->t[0], P->t[2], 0x100) == 0;

    AD->dist = (P->t[1] == 0) ? (u16)(CbAbs(P->t[2] - E->t[2]) + CbAbs(P->t[0] - E->t[0])) : 4999;
    switch (E->behavior_flags) {
        case 0: // on the floor
            if ((AD->dist < 10000) && !AD->sees) {
                ad_pick(AD_TURN);
            }
            if (((AD->dist < 2000) && lined) || AD->touch) {
                ad_pick(AD_BITE);
                AD->touch = 0;
            } else {
                if (AD->stuck > 0x3C) {
                    ad_pick(AD_TURN);
                    AD->stuck = 0;
                }
                if (P->health < 0) {
                    ad_pick(AD_GLOAT);
                }
            }
            break;
        case 1: // the ceiling: drops when she's near
            E->action_behavior = AD_WAIT;
            if (AD->dist < 3000) {
                ad_pick(AD_DROP);
            }
            break;
        case 2: { // the ceiling: drops onto her
            s32 drop;

            E->action_behavior = AD_WAIT;
            if ((P->flags & 0x20) && (AD->dist < 5000)) {
                drop = true;
            } else {
                drop = (E->action_ticks_counter-- == 0);
            }
            if (drop) {
                E->t[0] = P->t[0] + (CbRand() & 1) * 100;
                E->t[2] = P->t[2] + (CbRand() & 1) * 100;
                ad_pick(AD_DROP);
            }
            break;
        }
        default: // hidden: comes out when she's near
            E->action_behavior = AD_WAIT;
            if (AD->dist < 4500) {
                ad_pick(AD_EMERGE);
            }
            break;
    }
}

/* ---- the moves ---- */

static void ad_chase(void) {
    if (E->action_state == 0) {
        ad_start(0);
        E->move_speed_current = (s16)(0x32 - (CbRand() & 0xF));
        E->action_ticks_counter = (CbRand() & 0xF) + 0x10; // its turn rate this time
    } else if (E->action_state != 1) {
        return;
    }
    if (AD->dist < 8000) {
        if (AD->sees) {
            E->angle = (E->angle + turn_toward_target(P->t[0], P->t[2], (s16)E->action_ticks_counter)) & 0xFFF;
        }
    } else {
        entity_rotate_toward_target(P->t[0], P->t[2], 0x40);
    }
    E->angle = (E->angle + (CbRand() & 0xF) + (CbRand() & 0xF)) & 0xFFF; // the wobble
    Joint_move(0, 0x400);
    Add_speedXZ(0);
}

/** Off the ceiling: it falls, rights itself, lands with a hiss and slithers on. */
static void ad_drop(void) {
    if (E->action_state == 0) {
        ad_start(0);
        E->action_ticks_counter = 0;
        if (E->t[1] < -8120) {
            E->t[1] = -8120;
        }
        E->behavior_flags = 0; // a floor snake from now on
    } else if (E->action_state == 2) {
        Snd_em(2);
        E->ignore_player_flag = 0;
        E->action_behavior = AD_CHASE;
        E->action_state = 0;
        return;
    } else if (E->action_state != 1) {
        return;
    }
    E->t[1] += ((s16)E->action_ticks_counter * (s16)E->action_ticks_counter) << 3;
    E->action_ticks_counter++;
    E->pitch -= 0x55;
    if (E->pitch < 0) {
        E->pitch = 0;
    }
    if (E->t[1] >= 0) {
        E->t[1] = 0;
        E->pitch = 0;
        E->action_state = 2;
    }
    Joint_move(0, 0x400);
}

/** A turn away (one of two animations), then round the other way. */
static void ad_turn(void) {
    if (E->action_state == 0) {
        ad_start((u8)((CbRand() & 1) + 1));
        E->action_ticks_counter = CbRand() & 0x3F;
    } else if (E->action_state != 1) {
        return;
    }
    if (Joint_move(0, 0x400)) {
        E->angle = (E->angle + 0x800) & 0xFFF;
        E->ignore_player_flag = 0;
        E->action_behavior = AD_CHASE;
        E->action_state = 0;
    }
}

/** The strike: it lunges, and on the strike frame it lands if she's right in front of it. */
static void ad_bite(void) {
    if (E->action_state == 0) {
        ad_start(3);
        E->move_speed_current = 0x32;
        Snd_em(0);
    } else if (E->action_state == 2) {
        ad_pick(AD_TURN); // recoil out of it
        return;
    } else if (E->action_state != 1) {
        return;
    }
    E->action_state += (u8)Joint_move(0, 0x400);
    if ((E->animation_frame_id == 0xB) && (turn_toward_target(P->t[0], P->t[2], 0x200) == 0) &&
        NvRe1_JointReaches(1, 900) && (P->isBeingAttackedFlag == 0) && !gNvRe1DogFlinch && AD->sees) { // its head
        Snd_em(1);
        gNvRe1DogBite = 6;
        NvPoison_Give((CbRand() & 1) && (CbRand() & 1)); // RE1: one in four
    }
    Add_speedXZ(0);
}

/** She's dead: it strikes at her, over and over. */
static void ad_gloat(void) {
    if (E->action_state == 0) {
        ad_start(3);
        E->move_speed_current = 0x32;
        Snd_em(0);
    } else if (E->action_state != 1) {
        return;
    }
    Joint_move(0, 0x400);
    Add_speedXZ(0);
}

/** Out of the scenery: a slow crawl out, then it hunts. */
static void ad_emerge(void) {
    if (E->action_state == 0) {
        ad_start(0);
        E->move_speed_current = 0x1E;
        E->action_ticks_counter = 0x3C;
        E->behavior_flags = 0;
    } else if (E->action_state != 1) {
        return;
    }
    Joint_move(0, 0x400);
    Add_speedXZ(0);
    if (E->action_ticks_counter-- == 0) {
        E->hit_state = 0;
        AD->active = 1;
        E->ignore_player_flag = 0;
        E->action_behavior = AD_CHASE;
        E->action_state = 0;
    }
}

static void ad_act(void) {
    u32 path = entity_pathfind_update();

    if (!(path & 0xFE)) {
        AD->sees = (u8)(path & 1);
    }
    switch (E->action_behavior) {
        case AD_CHASE:
            ad_chase();
            break;
        case AD_DROP:
            ad_drop();
            break;
        case AD_TURN:
            ad_turn();
            break;
        case AD_BITE:
            ad_bite();
            break;
        case AD_GLOAT:
            ad_gloat();
            break;
        case AD_EMERGE:
            ad_emerge();
            break;
        default: // waiting
            break;
    }
}

static void ad_run(void) {
    if (E->ignore_player_flag == 0) {
        ad_decide();
        ad_act();
    } else if (E->ignore_player_flag == 1) {
        ad_act();
    }
    if ((E->t[1] == 0) && (P->t[1] == 0)) {
        entity_check_visual_range(5000);
    }
    AD->stuck = AD->coll ? AD->stuck + 1 : 0;
}

/** Shot: it rears, then holds a moment before it comes on again. */
static void ad_flinch(void) {
    if (E->action_state == 0) {
        ad_start(3);
        E->action_ticks_counter = (CbRand() & 0x1F) + 0x10;
    } else if (E->action_state == 2) {
        if (E->action_ticks_counter-- != 0) {
            return;
        }
        E->state = 1;
        E->ignore_player_flag = 0;
        E->action_behavior = AD_CHASE;
        E->action_state = 0;
        E->hit_state = 0;
        return;
    } else if (E->action_state != 1) {
        return;
    }
    E->action_state += (u8)Joint_move(0, 0x400);
}

static void ad_dead_flag(void) {
    if (E->death_event_id != 0xFF) {
        void NvFlags_Op(s32 bank, s32 bit, s32 mode);

        NvFlags_Op(3, E->death_event_id, 0);
    }
}

/** Killed: it writhes and lies still; a strong hit sends it back up to the ceiling instead. */
static void ad_writhe(void) {
    if (E->action_state == 0) {
        ad_start(4);
        E->animation_frame_id = 0x1E;
        E->status_flags |= 10; // intangible, dead
        E->move_speed_current = 0x28;
        E->action_ticks_counter = (CbRand() & 0xF) + 0x10;
        if ((E->hit_state & 0xF8) > 8) { // not the handgun: it goes back up
            E->action_behavior = 3;
            E->action_state = 0;
            return;
        }
    } else if (E->action_state == 2) {
        if (E->action_ticks_counter-- != 0) {
            return;
        }
        E->action_behavior = 4;
        ad_dead_flag();
        return;
    } else if (E->action_state != 1) {
        return;
    }
    if (Joint_move(0, 0x400)) {
        E->action_state = 2;
    }
    if (E->timing_control < 0x23) {
        Add_speedXZ(0);
    }
}

/** Back into the ceiling, out of sight a while, and down again (the water gate's near-drop kind). */
static void ad_retreat(void) {
    switch (E->action_state) {
        case 0:
            ad_start(4);
            E->action_ticks_counter = (CbRand() & 0x1F) + 0x10;
            // fall through
        case 1:
            E->action_state += (u8)Joint_move(0, 0x400);
            return;
        case 2:
            E->action_state = 3;
            E->action_ticks_counter = (CbRand() & 0x1F) + 0xD2;
            return;
        case 3:
            E->t[1] = -20000; // out of sight, high up
            E->action_state = 4;
            // fall through
        case 4:
            if (E->action_ticks_counter-- == 0) {
                E->behavior_flags = (NvScript_RoomNum() == 0x01) ? 1 : 2; // (the water gate's waits for her)
                E->status_flags &= ~10;
                E->state = 0;
                E->ignore_player_flag = 0;
                E->action_behavior = 0;
                E->action_state = 0;
            }
            return;
        default:
            return;
    }
}

static void ad_die(void) {
    if (E->action_behavior == 0) {
        ad_writhe();
    } else if (E->action_behavior == 3) {
        ad_retreat();
    }
}

void NvAdder_Init(NvRe1Entity* e) {
    E = e;
    memset(&E->ad, 0, sizeof(E->ad));
    E->state = 1;
    E->ignore_player_flag = 0;
    E->action_behavior = 0;
    E->action_state = 0;
    E->action_ticks_counter = 0;
    E->hit_state = 0;
    E->status_flags = 1;
    CbRand();
    E->health = (s16)((CbRand() & 7) * 2 + (CbRand() & 7) * 2 + (CbRand() & 7) * 2 + 10);
    E->animation_frame_id = 0;
    E->timing_control = 0;
    E->animationId = 0;
    Joint_move(0, 0x40);
    E->sca = sAdSca;
    E->pitch = 0;
    AD->active = 1;
    if (E->behavior_flags != 0) { // on the ceiling, upside down
        E->action_ticks_counter = (u16)((0x18 - (CbRand() & 6)) * 10);
        E->pitch = 0x800;
        if (E->t[1] == 0) {
            E->t[1] = -8120;
        }
    }
    if (E->behavior_flags == 3) { // hidden in the scenery
        E->pitch = 0;
        E->t[1] = 0;
        AD->active = 0;
        E->hit_state = 1;
    }
}

void NvAdder_Update(NvRe1Entity* e) {
    E = e;
    E->pos[0] = (s16)E->t[0];
    E->pos[1] = (s16)E->t[1];
    E->pos[2] = (s16)E->t[2];
    switch (E->state) {
        case 0:
            NvAdder_Init(e);
            break;
        case 1:
            ad_run();
            break;
        case 2:
            if (E->action_behavior == 0) {
                ad_flinch();
            }
            break;
        case 3:
            ad_die();
            break;
        default:
            break;
    }
    if (AD->active && !(E->status_flags & 2) && (E->t[1] > -4000)) {
        SetScaWorld(E->sca, E->angle, E->scaWorld);
        SetScaWorld(P->sca, P->angle, P->scaWorld);
        AD->touch = (u8)ResolveScaPush(P->sca, P->scaWorld, P->t, E);
        HandleEnemyCollisions();
        AD->coll = check_room_collision();
    }
}
