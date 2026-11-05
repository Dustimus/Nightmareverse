/*
 * Nightmareverse: RE1's sharks (Neptune, enemy type 11, EM100B), the guardhouse basement's flooded
 * corridors and its aquarium. Included at the end of z_nv_zombie.c with the other ported enemies.
 *
 * As RE1 swims them (the decomp's entities/Neptune.cpp is the reference): a shark cruises the
 * water, steering off the walls ahead of it; lined up within 3000 it bites and holds her,
 * chewing (7 of RE1's health a chew) until she mashes free or the struggle runs out; if she's
 * low (under 26) and within 3500 it goes for the kill instead, seizing and dragging her under.
 * Shot, a shark facing her thrashes and turns away, one that isn't sinks off; the aquarium's
 * own shark (behaviour bit 0) charges twice when it dies and beaches itself. The small ones
 * (bit 1) only swim.
 *
 * Left out, each a note where it would be: the bubbles and the blood in the water, the reach
 * (RE1 measures its jaw joint to her; here within 1500 and in front), her wading speed
 * switching its pace (RE1 speeds it up while she wades far off: here by distance alone), and
 * the boss's tint and shrink as it dies.
 */

#define NE (&E->ne)

static const s16 sNeScaBig[8] = { (s16)0x8000, 0, -800, 0, 800, 600, 0, 0 };
static const s16 sNeScaSmall[8] = { (s16)0x8000, 0, -400, 0, 400, 300, 0, 0 };

enum { NE_SWIM, NE_LUNGE, NE_TURN_AWAY, NE_CIRCLE, NE_DEVOUR, NE_BITE };

static void ne_anim(u8 id, u8 blend) {
    E->animationId = id;
    E->animation_frame_id = 0;
    E->timing_control = 0;
    E->blend_counter = blend;
}

/** Back to the cruise. */
static void ne_cruise(void) {
    E->state = 1;
    E->ignore_player_flag = 0;
    E->action_behavior = NE_SWIM;
    E->action_state = 0;
}

static s32 ne_reaches(void) {
    return (NE->dist < 1500) && (turn_toward_target(P->t[0], P->t[2], 0x200) == 0);
}

static void ne_choose(void) {
    entity_update_wander_turn(E->move_dist, &NE->wallAhead, &E->turn_counter, 0x20, 60);
    if ((P->isBeingAttackedFlag != 0) || gNvRe1DogFlinch || (E->behavior_flags & 2)) {
        return;
    }
    if (turn_toward_target(P->t[0], P->t[2], 768) != 0) {
        return;
    }
    if (P->health < 26) {
        if (NE->dist < 3500) {
            E->ignore_player_flag = 1;
            E->action_behavior = NE_DEVOUR;
            E->action_state = 0;
        }
    } else if (NE->dist < 3000) {
        E->ignore_player_flag = 1;
        E->action_behavior = NE_BITE;
        E->action_state = 0;
    }
}

static void ne_swim(void) {
    switch (E->action_state) {
        case 0:
            ne_anim(0, 0x3F);
            E->action_state = 1;
            E->move_speed_current = 0x78;
            NE->cue = 3;
            break;
        case 1: // far off: a little faster
            if (NE->dist > 10000) {
                E->action_state = 2;
                E->move_speed_current = 0x82;
            }
            break;
        default:
            if (NE->dist < 6000) {
                E->action_state = 1;
                E->move_speed_current = 100;
            }
            break;
    }
    NE->cue += (s8)Joint_move(0, 0x40);
    if (NE->cue == 3) {
        Snd_em(0);
        NE->cue = 0;
    }
    Add_speedXZ(0);
}

/** A fast pass (one of two lunges), veering. */
static void ne_lunge(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        E->move_speed_current = 0x32;
        ne_anim((u8)((CbRand() & 1) + 2), 0x3F);
        Snd_em(4);
    } else if (E->action_state != 1) {
        return;
    }
    if ((E->animationId == 2) && (E->animation_frame_id == 0x19)) {
        Snd_em(5);
    }
    if (Joint_move(0, 0x40)) {
        E->action_state = 0;
    }
    Add_speedXZ(CbRand() & 0xFFF);
    E->angle = (E->angle - 0x400) & 0xFFF;
    entity_rotate_toward_target(P->t[0], P->t[2], 4);
    E->angle = (E->angle + 0x400) & 0xFFF;
}

static void ne_turn_away(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        E->move_speed_current = 0x96;
        ne_anim(6, 0x3F);
    }
    Joint_move(0, 0x40);
    if (E->animation_frame_id == 0x17) {
        ne_cruise();
    }
    Add_speedXZ(0);
}

/** Circling: one way a while, then the other. */
static void ne_circle(void) {
    switch (E->action_state) {
        case 0:
            ne_anim(0, 0x3F);
            E->action_state = 1;
            E->move_speed_current = 100;
            E->action_ticks_counter = (CbRand() & 0x1F) + 0x5A;
            break;
        case 1:
            entity_update_wander_turn(E->move_dist, &NE->wallAhead, &E->turn_counter, 0x20, 0x3C);
            if (--E->action_ticks_counter == 0) {
                E->action_state = 2;
                E->action_ticks_counter = (CbRand() & 0x1F) + 0x1E;
            }
            break;
        default:
            entity_update_wander_turn(E->move_dist, &NE->wallAhead, &E->turn_counter, (u16)-0x20, 0x3C);
            if (--E->action_ticks_counter == 0) {
                E->action_state = 1;
                E->action_ticks_counter = (CbRand() & 0x1F) + 0x5A;
            }
            break;
    }
    Joint_move(0, 0x40);
    Add_speedXZ(0);
}

/** The kill: it seizes her, and if she's still in its jaws at the bite, drags her under. */
static void ne_devour(void) {
    switch (E->action_state) {
        case 0:
            entity_rotate_toward_target(P->t[0], P->t[2], 0x100);
            ne_anim(6, 0x3F);
            E->action_state = 1;
            E->status_flags |= 2;
            E->move_speed_current = 0x96;
            Snd_em(2);
            break;
        case 1: // the carry: at frame 15 she's in its jaws, or it missed
            Joint_move(0, 0x40);
            if (E->animation_frame_id == 0xF) {
                if (!ne_reaches()) {
                    E->action_state = 5;
                    break;
                }
                E->unk_c6 = (s16)E->t[0];
                E->unk_c8 = (s16)E->t[2];
                E->holding = 1;
                E->action_state = 2;
            }
            Add_speedXZ(0);
            break;
        case 2:
            Joint_move(0, 0x40);
            if (E->animation_frame_id == 0x13) {
                E->action_state = 3;
            }
            break;
        case 3: // under
            E->action_state += (u8)Joint_move(0, 0x40);
            gNvRe1Kill = 1;
            break;
        case 4:
            E->holding = 0;
            E->status_flags &= ~2;
            ne_cruise();
            break;
        case 5: // the miss: it turns off
            Joint_move(0, 0x40);
            if (E->animation_frame_id == 0x17) {
                E->action_state = 6;
                E->action_ticks_counter = 0x1E;
                E->status_flags &= ~2;
                ne_anim(0, 0x1F);
            }
            break;
        default:
            Joint_move(0, 0x80);
            if (--E->action_ticks_counter == 0) {
                ne_cruise();
                E->action_state = 1;
                E->move_speed_current = 0x78;
            }
            Add_speedXZ(0);
            break;
    }
}

/** The bite: it closes, clamps on, and chews until she shakes it off. */
static void ne_bite(void) {
    switch (E->action_state) {
        case 0:
            E->action_state = 1;
            E->move_speed_current = 100;
            ne_anim(7, 0x1F);
            E->status_flags |= 2;
            break;
        case 1: // closing
            if (Joint_move(0, 0x80)) { // missed
                E->action_state = 5;
                E->status_flags &= ~2;
                E->action_ticks_counter = 0x1E;
                ne_anim(0, 0x1F);
                break;
            }
            entity_rotate_toward_target(P->t[0], P->t[2], 0x20);
            if (ne_reaches()) {
                E->action_state = 2;
                ne_anim(8, 0x1F);
                E->unk_c6 = (s16)E->t[0];
                E->unk_c8 = (s16)E->t[2];
                E->holding = 1;
            }
            Add_speedXZ(0);
            break;
        case 2: // clamped on
            if (Joint_move(0, 0x80)) {
                E->action_state = 3;
                Snd_em(1);
                ne_anim(9, 0x1F);
                NE->struggle = 0x5A;
            }
            break;
        case 3: // chewing: each round 7, her mashing wears it down faster
            NE->struggle -= P->padHeld ? 8 : 1;
            if (Joint_move(0, 0x80)) {
                gNvRe1Drain = 7;
            }
            if (NE->struggle < 0) {
                E->action_state = 4;
                E->move_speed_current = 100;
                ne_anim(10, 0x1F);
                Snd_em(1);
            }
            break;
        case 4: // letting go
            entity_rotate_toward_target(P->t[0], P->t[2], (u16)-0x10);
            if (Joint_move(0, 0x80)) {
                E->holding = 0;
                E->action_state = 5;
                E->action_ticks_counter = 0x1E;
                E->status_flags &= ~2;
                ne_anim(0, 0x1F);
            }
            Add_speedXZ(0);
            break;
        default: // swimming off
            entity_rotate_toward_target(P->t[0], P->t[2], (u16)-0x20);
            Joint_move(0, 0x80);
            if (--E->action_ticks_counter == 0) {
                ne_cruise();
                E->action_state = 1;
                E->move_speed_current = 0x78;
            }
            Add_speedXZ(0);
            break;
    }
}

static void ne_run(void) {
    zone_path_find(P->t[0], P->t[2], &E->player_pos_x, &E->player_pos_z);
    NE->dist = CbAbs(P->t[0] - E->t[0]) + CbAbs(P->t[2] - E->t[2]);
    entity_check_visual_range(4000);
    if (E->ignore_player_flag == 0) {
        ne_choose();
    }
    switch (E->action_behavior) {
        case NE_SWIM:
            ne_swim();
            break;
        case NE_LUNGE:
            ne_lunge();
            break;
        case NE_TURN_AWAY:
            ne_turn_away();
            break;
        case NE_CIRCLE:
            ne_circle();
            break;
        case NE_DEVOUR:
            ne_devour();
            break;
        default:
            ne_bite();
            break;
    }
}

/** Shot: facing her it thrashes and turns away; not, it sinks off and comes round again. */
static void ne_hurt(void) {
    if (E->ignore_player_flag == 0) {
        E->ignore_player_flag = 1;
        E->action_behavior = 0;
        E->action_state = 0;
        E->holding = 0;
    }
    switch (E->action_behavior) {
        case 0:
            E->move_speed_current = 0x50;
            E->action_ticks_counter = 0x1E;
            if (turn_toward_target(P->t[0], P->t[2], 0x400) != 0) {
                E->action_behavior = 2;
                E->move_speed_current = 0x78;
                break;
            }
            E->action_behavior = 1;
            ne_anim(1, 7);
            Snd_em(2);
            // fall through
        case 1:
            if (Joint_move(0, 0x200)) {
                E->action_behavior = 2;
                E->move_speed_current = 0x78;
                ne_anim(0, 0x3F);
                E->angle = (E->angle + 0x800) & 0xFFF;
            }
            Add_speedXZ(0x800);
            break;
        default:
            entity_rotate_toward_target(P->t[0], P->t[2], (u16)-0x20);
            Joint_move(0, 0x40);
            Add_speedXZ(0);
            if (--E->action_ticks_counter == 0) {
                ne_cruise();
                E->blend_counter = 0x3F;
                E->hit_state = 0;
            }
            break;
    }
}

/** The aquarium's shark dying: two charges, then it beaches itself. */
static void ne_boss_die(void) {
    s32 off;

    switch (E->action_behavior) {
        case 0:
            if (E->death_event_id != 0xFF) {
                void NvFlags_Op(s32 bank, s32 bit, s32 mode);

                NvFlags_Op(3, E->death_event_id, 0);
            }
            E->action_behavior = 1;
            E->move_speed_current = 0x96;
            E->action_ticks_counter = 0x2D;
            ne_anim(2, 0x3F);
            Snd_em(4);
            // fall through
        case 1:
        case 2:
            entity_rotate_toward_target(P->t[0], P->t[2], (u16)-8);
            if (Joint_move(0, 0x40)) {
                ne_anim((u8)(E->action_behavior + 2), 0x3F);
                E->move_speed_current = (E->action_behavior == 1) ? 100 : 0;
                E->action_ticks_counter = 0x2D;
                E->action_behavior++;
                break;
            }
            if ((E->action_behavior == 2) || ((E->animation_frame_id > 0xC) && (E->animation_frame_id < 0x19))) {
                E->angle = (E->angle + 0x400) & 0xFFF;
                off = turn_toward_target(P->t[0], P->t[2], 0x400);
                E->angle = (E->angle - 0x400) & 0xFFF;
                Add_speedXZ(off ? 0x400 : 0xC00);
            }
            break;
        case 3: // beached
            if (E->action_state == 0) {
                if (E->animation_frame_id == 0) {
                    Snd_em(4);
                }
                E->action_ticks_counter--;
                if (((s16)E->action_ticks_counter < 0) && (E->animation_frame_id == 8)) {
                    E->action_state = 1;
                    ne_anim(5, 0x3F);
                }
            } else {
                E->action_behavior = 4;
                E->action_state = 0;
                E->status_flags |= 2;
                E->action_ticks_counter = 0x5A;
            }
            Joint_move(0, 0x40);
            break;
        default:
            E->health = -1;
            if ((E->action_state == 0) && (--E->action_ticks_counter == 0)) {
                E->action_state = 1;
                E->status_flags |= 10; // gone
            }
            Joint_move(0, 0x40);
            break;
    }
}

void NvNeptune_Init(NvRe1Entity* e) {
    E = e;
    memset(&E->ne, 0, sizeof(E->ne));
    E->state = 1;
    E->ignore_player_flag = 0;
    E->action_behavior = 0;
    E->action_state = 0;
    E->action_ticks_counter = 15;
    E->hit_state = 0;
    E->sca = (E->behavior_flags & 2) ? sNeScaSmall : sNeScaBig;
    CbRand();
    E->health = (s16)((CbRand() & 7) + (CbRand() & 7) + (CbRand() & 7) + 0x28);
    ne_anim(0, 0x3F);
    if (E->behavior_flags & 1) { // the aquarium's: lunging from the start
        ne_anim(2, 0);
        E->ignore_player_flag = 1;
        E->action_behavior = NE_LUNGE;
    }
    Joint_move(0, 0x40);
    E->turn_counter = 0;
    if (E->behavior_flags == 2) {
        E->status_flags |= 2;
    }
}

void NvNeptune_Update(NvRe1Entity* e) {
    s32 px;
    s32 pz;

    E = e;
    if ((E->state != 0) && (E->behavior_flags & 0x80)) {
        return; // held by its script
    }
    E->pos[0] = (s16)E->t[0];
    E->pos[1] = (s16)E->t[1];
    E->pos[2] = (s16)E->t[2];
    switch (E->state) {
        case 0:
            NvNeptune_Init(e);
            break;
        case 1:
            ne_run();
            break;
        default: // hit or dying
            if (E->behavior_flags & 1) {
                if (E->health < 0) {
                    ne_boss_die();
                } else {
                    ne_hurt();
                }
            } else {
                ne_hurt();
                if (E->health < 0) {
                    E->status_flags |= 10; // a corridor shark: dead in the water
                }
            }
            break;
    }
    SetScaWorld(E->sca, E->angle, E->scaWorld);
    SetScaWorld(P->sca, P->angle, P->scaWorld);
    if (!(E->status_flags & 2)) {
        ResolveScaPush(P->sca, P->scaWorld, P->t, E);
        HandleEnemyCollisions();
    }
    // the wall 800 ahead of it, for its steering
    RotY(E->angle, 800, 0, &px, &pz);
    E->t[0] += px;
    E->t[2] += pz;
    NE->wallAhead |= check_room_collision(); // (the steering clears it)
    E->t[0] -= px;
    E->t[2] -= pz;
}
