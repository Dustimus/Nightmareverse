/*
 * Nightmareverse: RE1's wasps (enemy type 7, EM1007), the guardhouse's nest room and its
 * corridors. Included at the end of z_nv_zombie.c with the other ported enemies.
 *
 * As RE1 flies them (the decomp's entities/Wasp.cpp is the reference): a loose wasp sits until
 * the player comes within 5000, a nest wasp (behaviour 0x10) waits out a countdown or her coming
 * within 4000 and rises out of the nest. Up, it spirals off, then cruises: bobbing between two
 * rolled heights, drifting faster and turning toward her when she's off its nose. Close, level
 * with her head and lined up, it stings (4 of RE1's health, 8 for the big one); from behind,
 * the small one pins her and stings (8), dying of it. Shot, it drops to the floor, lies there a
 * moment and can be stepped on; dead, a nest wasp comes back out of the nest (the tenth time
 * as the last one).
 *
 * Left out, each a note where it would be: the wing
 * blur (RE1 flags the wings for one frame in seven), the shadow, and the big wasp's blood.
 * The wasp has no wall collision in RE1 either (its radius is 0): it flies through walls.
 */

#define WA (&E->wa)

static const s16 sWaSca[8] = { (s16)0x8000, 0, 0, 0, 0, 0, 0, 0 };

enum { WA_NEST, WA_TAKEOFF, WA_HOVER, WA_STING, WA_CIRCLE, WA_PIN, WA_EMERGE };

/** Back to the cruise. */
static void wa_cruise(void) {
    E->ignore_player_flag = 0;
    E->action_behavior = WA_HOVER;
    E->action_state = 0;
    WA->bob = 0x23;
    WA->climb = 1;
}

static s16 wa_cruise_speed(void) {
    return (s16)(WA->dist / 100 + 200);
}

/** The current animation, looping; animDone is set the tick it wraps. */
static void wa_animate(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        E->animation_frame_id = 0;
        E->timing_control = 0;
        E->death_timer = 0;
        E->blend_counter = 3;
    } else if (E->action_state != 1) {
        return;
    }
    WA->animDone = (u8)Joint_move(0, 0x400);
}

/** The bob: a climb or a sink, speeding up each tick, turning at two rolled heights. */
static void wa_bob(s32 resetSpeed) {
    if (++WA->bob > 0x96) {
        WA->bob = 0x23;
    }
    if (E->t[1] < (-7 - (CbRand() & 1)) * 500) {
        WA->climb = 0;
        if (resetSpeed) {
            WA->bob = 0x23;
        }
    }
    if (E->t[1] > (-3 - (CbRand() & 1)) * 500) {
        WA->climb = 1;
        if (resetSpeed) {
            WA->bob = 0x23;
        }
    }
    E->t[1] += WA->climb ? -WA->bob : WA->bob;
}

static void wa_nest(void) {
    u8 kind = E->behavior_flags;

    E->hit_state = 1; // out of reach while it sits
    E->animationId = 0;
    wa_animate();
    if (kind & 0xF0) {
        if (kind >= 0x20) { // roused
            E->hit_state = 0;
            wa_cruise();
            E->move_speed_current = wa_cruise_speed();
            return;
        }
        if (kind & 1) { // cleared to come out
            E->action_behavior = WA_EMERGE;
            E->action_state = 0;
            E->move_speed_current = 0x1E;
            E->action_ticks_counter = 0;
            E->status_flags |= 2;
            return;
        }
        if ((E->action_ticks_counter-- == 0) || (WA->dist < 4000)) {
            E->behavior_flags = 0x11;
        }
        return;
    }
    if (kind == 0) {
        if (WA->dist >= 5000) {
            return;
        }
    } else if (WA->timer-- == 0) {
        return;
    }
    E->hit_state = 0;
    E->action_behavior = WA_TAKEOFF;
    E->action_state = 0;
    WA->timer = 0x5A;
    E->behavior_flags |= 0x20;
    E->move_speed_current = 0x32;
}

/** Off the floor in a spiral. */
static void wa_takeoff(void) {
    E->animationId = 1;
    E->move_speed_current++;
    wa_animate();
    E->angle = (E->angle + 0x55) & 0xFFF;
    Add_speedXZ((s16)(0x800 - E->animation_frame_id * 0x55));
    if (E->animation_frame_id == 0x18) {
        wa_cruise();
    }
}

static void wa_hover(void) {
    E->animationId = 2;
    wa_animate();
    if (((E->animation_frame_id % 7) == 0) && (WA->dist < 1000)) {
        Snd_em(0); // the buzz, close by
    }
    Add_speedXZ(0);
    wa_bob(false);
    // touching her, near, lined up, at her head's height: it strikes
    if (!WA->touch || (WA->dist >= 1000) || (turn_toward_target(P->t[0], P->t[2], 0x100) != 0) ||
        ((u32)(-2300 - E->t[1]) >= 600)) {
        return;
    }
    WA->drift = 0;
    wa_cruise();
    E->move_speed_current = wa_cruise_speed();
    if ((P->isBeingAttackedFlag != 0) || gNvRe1DogFlinch || !WA->sees) {
        return;
    }
    E->ignore_player_flag = 1;
    E->action_behavior = WA_STING;
    E->action_state = 0;
    // from behind her, the small one pins her
    if (!is_facing_toward_entity() && (turn_toward_target(P->t[0], P->t[2], 0xC0) == 0) && !WA->big) {
        E->action_behavior = WA_PIN;
        E->angle = getAngleTowardsTarget(P->t[0], P->t[2]);
        E->hit_state = 1;
    }
}

static void wa_sting(void) {
    E->animationId = 3;
    E->angle = (E->angle + turn_toward_target(P->t[0], P->t[2], 0x30)) & 0xFFF;
    wa_animate();
    if (!WA->animDone) {
        return;
    }
    if ((P->isBeingAttackedFlag == 0) && !gNvRe1DogFlinch && (WA->dist < 1500) &&
        (turn_toward_target(P->t[0], P->t[2], 0x100) == 0) && WA->sees) {
        gNvRe1DogBite = WA->big ? 8 : 4;
    }
    E->move_speed_current = 100;
    E->ignore_player_flag = 1;
    E->action_behavior = WA_TAKEOFF; // back up in the spiral
    E->action_state = 0;
}

/** She's dead: it circles over her. */
static void wa_circle(void) {
    E->animationId = 2;
    if (WA->sees) {
        E->angle = (E->angle + turn_toward_target(P->t[0], P->t[2], 0x10) * 2) & 0xFFF;
    }
    wa_animate();
    wa_bob(true);
    Add_speedXZ((WA->dist >= 2000) ? 0 : 0x400);
}

/** The pin: down on her, the sting at her neck, and the wasp is spent (it dies of it). */
static void wa_pin(void) {
    switch (E->action_state) {
        case 0:
            E->animation_frame_id = 0;
            E->timing_control = 0;
            E->action_state = 1;
            E->blend_counter = 3;
            E->angle = P->angle;
            E->t[1] = 0;
            E->status_flags |= 2;
            E->animationId = 5; // (4 is Chris's)
            E->unk_c6 = (s16)P->t[0];
            E->unk_c8 = (s16)P->t[2];
            E->holding = 1;
            // fall through
        case 1:
            E->action_state += (u8)Joint_move(0, 0x400);
            E->death_timer++;
            if (E->animation_frame_id == 4) {
                Snd_em(3);
            }
            if (E->animation_frame_id == 0x24) {
                E->action_state = 2;
            }
            return;
        default:
            E->holding = 0;
            gNvRe1DogBite = 8;
            NvPoison_Give(CbRand() & 1); // RE1: one in two
            E->health = -44;
            E->state = 3;
            E->ignore_player_flag = 0;
            E->action_behavior = 0;
            E->action_state = 0;
            return;
    }
}

/** Out of the nest: a climb, a hold, and away. */
static void wa_emerge(void) {
    E->animationId = 2;
    E->move_speed_current++;
    wa_animate();
    E->t[1] += 0x14;
    if (E->t[1] > -2800) {
        E->t[1] += 0x14;
        Add_speedXZ(0);
        if (++E->action_ticks_counter > 0x14) {
            E->t[1] -= 0x3C;
        }
    }
    if (E->action_ticks_counter > 0x19) {
        E->status_flags &= ~2;
        E->hit_state = 0;
        wa_cruise();
    }
}

static void wa_act(void) {
    u32 path = entity_pathfind_update();

    if (!(path & 0xFE)) {
        WA->sees = (u8)(path & 1);
    }
    switch (E->action_behavior) {
        case WA_NEST:
            wa_nest();
            break;
        case WA_TAKEOFF:
            wa_takeoff();
            break;
        case WA_HOVER:
            wa_hover();
            break;
        case WA_STING:
            wa_sting();
            break;
        case WA_CIRCLE:
            wa_circle();
            break;
        case WA_PIN:
            wa_pin();
            break;
        case WA_EMERGE:
            wa_emerge();
            break;
        default:
            break;
    }
}

static void wa_run(void) {
    if ((P->health < 0) && (E->action_behavior != WA_CIRCLE)) {
        WA->bob = 0x23;
        WA->climb = 1;
        E->move_speed_current = wa_cruise_speed();
        E->ignore_player_flag = 0;
        E->action_behavior = WA_CIRCLE;
        E->action_state = 0;
    }
    if (WA->coll) {
        E->angle = (E->angle + turn_toward_target(P->t[0], P->t[2], 0x80)) & 0xFFF;
    }
    WA->dist = (u16)(CbAbs(P->t[2] - E->t[2]) + CbAbs(P->t[0] - E->t[0]));
    if (E->ignore_player_flag == 0) {
        // the drift: faster each tick; once it's been a while and she's off its nose, a turn
        WA->drift++;
        E->move_speed_current += 2;
        if ((WA->drift > 0x1C) && (turn_toward_target(P->t[0], P->t[2], 0x100) != 0)) {
            E->angle = (E->angle + turn_toward_target(P->t[0], P->t[2], 0x80)) & 0xFFF;
            E->move_speed_current -= 8;
        }
        if (E->move_speed_current < 0x1E) {
            E->move_speed_current = wa_cruise_speed();
            E->angle = (E->angle - turn_toward_target(P->t[0], P->t[2], 0x200)) & 0xFFF;
        }
        if (E->move_speed_current > 400) {
            E->move_speed_current = 400;
        }
        wa_act();
    } else if (E->ignore_player_flag == 1) {
        wa_act();
    }
    if (E->t[1] < -4000) {
        entity_check_alert_range(4000);
    }
    if (E->t[1] > -1000) {
        entity_check_visual_range(4000);
    }
}

/** Shot down: it falls, lies on the floor a moment, and she can step on it. */
static void wa_downed(void) {
    switch (E->action_state) {
        case 0:
            E->move_speed_current = 0;
            E->action_state = 1;
            E->animation_frame_id = 0;
            E->timing_control = 0;
            E->action_ticks_counter = 0x1E;
            E->blend_counter = 3;
            E->hit_state = 1;
            E->status_flags |= 2;
            // fall through
        case 1:
            E->t[1] += 200;
            if (E->t[1] > -100) {
                E->t[1] = 0;
                E->action_state = 2;
                WA->timer = 0x1E;
            }
            return;
        case 2:
            if (WA->timer-- == 0) {
                E->action_state = 3;
                E->status_flags &= ~2;
                E->hit_state = 0;
            }
            return;
        case 3:
            entity_check_visual_range(4000);
            WA->dist = (u16)(CbAbs(P->t[2] - E->t[2]) + CbAbs(P->t[0] - E->t[0]));
            if ((WA->dist < 400) && (P->speed != 0)) { // stepped on
                E->hit_state = 1;
                E->health = -4;
                if (!WA->cried) {
                    Snd_em(2);
                    WA->cried = 1;
                }
                E->state = 3;
                E->ignore_player_flag = 0;
                E->action_behavior = 0;
                E->action_state = 0;
            }
            return;
        default:
            return;
    }
}

/** Dead: its flag, and a nest wasp comes round again after a while. */
static void wa_die(void) {
    switch (E->action_state) {
        case 0:
            E->move_speed_current = 0;
            E->animation_frame_id = 0;
            E->timing_control = 0;
            E->blend_counter = 3;
            E->status_flags |= 10; // intangible, dead
            if (!WA->cried) {
                Snd_em(1);
            }
            if ((E->death_event_id != 0xFF) && (E->behavior_flags < 0x10)) {
                void NvFlags_Op(s32 bank, s32 bit, s32 mode);

                NvFlags_Op(3, E->death_event_id, 0);
            }
            if ((E->behavior_flags & 0xF0) && !WA->big) { // a nest wasp: back out of the nest later
                E->action_state = 2;
                WA->timer = 0x14;
            } else {
                E->action_state = 4;
            }
            return;
        case 2:
            if (WA->timer-- != 0) {
                return;
            }
            E->behavior_flags = 0x10;
            if (WA->respawns > 10) {
                WA->respawns = 0;
            }
            if (WA->respawns == 10) {
                E->behavior_flags = 0x12; // the last one
            }
            WA->respawns++;
            E->angle = 0x400;
            // the nest (the guardhouse's wasp room)
            E->t[0] = (0xB4 - (CbRand() & 1)) * 100;
            E->t[1] = (-0x28 - (CbRand() & 1)) * 100;
            E->t[2] = (0xE5 - (CbRand() & 1)) * 100;
            E->status_flags &= ~10;
            E->state = 0;
            E->ignore_player_flag = 0;
            E->action_behavior = 0;
            E->action_state = 0;
            return;
        default:
            return;
    }
}

void NvWasp_Init(NvRe1Entity* e) {
    u8 respawns;

    E = e;
    respawns = WA->respawns;
    memset(&E->wa, 0, sizeof(E->wa));
    WA->respawns = respawns;
    E->state = 1;
    E->ignore_player_flag = 1;
    E->action_behavior = WA_NEST;
    E->action_state = 0;
    E->action_ticks_counter = 0;
    E->animation_frame_id = 0;
    E->death_timer = 0;
    E->hit_state = 0;
    E->status_flags = 1;
    WA->timer = 0x5A;
    E->sca = sWaSca;
    if (E->t[1] == 0) {
        E->t[1] = -4000;
    }
    if (E->behavior_flags & 2) { // the big one
        E->behavior_flags -= 2;
        WA->big = 1;
    }
    if (E->behavior_flags == 0x10) {
        E->action_ticks_counter = (u16)(800 - (CbRand() & 9) * 30);
    }
    Joint_move(0, 0x400);
    CbRand();
    E->health = (s16)((CbRand() & 7) * 2 + (CbRand() & 7) * 2 + (CbRand() & 7) * 2 + (WA->big + 1) * 20);
}

void NvWasp_Update(NvRe1Entity* e) {
    E = e;
    E->pos[0] = (s16)E->t[0];
    E->pos[1] = (s16)E->t[1];
    E->pos[2] = (s16)E->t[2];
    switch (E->state) {
        case 0:
            NvWasp_Init(e);
            break;
        case 1:
            wa_run();
            break;
        case 2:
            wa_downed();
            break;
        case 3:
            wa_die();
            break;
        default:
            break;
    }
    if ((E->t[1] != 0) && (E->action_behavior != WA_NEST) && (E->action_behavior != WA_EMERGE) &&
        !(E->status_flags & 2)) {
        SetScaWorld(E->sca, E->angle, E->scaWorld);
        SetScaWorld(P->sca, P->angle, P->scaWorld);
        WA->touch = (u8)ResolveScaPush(P->sca, P->scaWorld, P->t, E);
        HandleEnemyCollisions();
        WA->coll = check_room_collision();
    }
}
