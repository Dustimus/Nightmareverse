/*
 * Nightmareverse: RE1's crows (enemy type 5, EM1005), the gallery's (117) and the 2F corridors'.
 * Included at the end of z_nv_zombie.c with the other ported enemies.
 *
 * As RE1 flies them (the decomp's entities/Crow.cpp is the reference): a crow perches and
 * preens until the player raises a gun (or it starts in the air: behaviour flag 0x10), then flies a loop of climbs and sinks between two rolled altitudes,
 * turning toward the player, and when it comes round low and lined up it swoops at her and pecks
 * (6 of RE1's health), flapping off to climb again. A hit throws it down and it recovers, or
 * dies on the floor.
 *
 * Left out, each a note where it would be: the latch-on (RE1's crow that grabs a hurt player's
 * shoulder and keeps pecking until shaken off: here the peck and away), its banking roll (drawn
 * level), the feathers, and the shadow.
 */

#define CR (&E->cr)

static const s16 sCrSca[8] = { (s16)0x8000, 0, 0, 0, 180, 200, 0, 0 };

enum { CR_PERCH, CR_FLAP, CR_DIVE, CR_TURN, CR_STRIKE, CR_SINK, CR_CLIMB, CR_SWOOP, CR_CLOSE, CR_PECK,
       CR_LEVEL, CR_LATCH, CR_DOWN, CR_TAKEOFF };

static void cr_set(u8 state, u8 air, u8 behaviour) {
    E->state = state;
    E->ignore_player_flag = air;
    E->action_behavior = behaviour;
    E->action_state = 0;
}

static void cr_anim_start(void) {
    E->animation_frame_id = 0;
    E->timing_control = 0;
    E->death_timer = 0;
    E->blend_counter = 3;
    E->action_state = 1;
}

/** A looping animation. */
static void cr_loop(void) {
    if (E->action_state == 0) {
        cr_anim_start();
    }
    Joint_move(0, 0x400);
}

/** An animation played once, then back on the perch (ignore 0, behaviour 0). */
static void cr_once(void) {
    if (E->action_state == 0) {
        cr_anim_start();
    } else if (E->action_state == 2) {
        E->ignore_player_flag = 0;
        E->action_behavior = 0;
        E->action_state = 0;
        return;
    }
    E->action_state += (u8)Joint_move(0, 0x400);
}

/** The next move after a dive or a turn: line up and strike if it can, else turn again. */
static void cr_after_turn(s32 lined) {
    cr_set(1, 1, CR_TURN);
    if (lined) {
        E->action_behavior = CR_STRIKE;
    }
    if (P->health < 0) {
        E->action_behavior = CR_PERCH;
    }
}

static s16 cr_speed_to_player(void) {
    return (s16)((4 - (CbRand() & 1)) * 0x32 + CR->dist / 100);
}

/** Lined up low within 1200 at RE1's head height band: the peck. */
static void cr_check_peck(void) {
    if ((CR->dist < 0x4B0) && (turn_toward_target(P->t[0], P->t[2], 0x100) == 0) &&
        ((u32)(-2500 - E->t[1]) < 300)) {
        cr_set(1, 1, CR_PECK);
    }
}

static void cr_behave(void) {
    s32 lined;
    u8 path = (u8)entity_pathfind_update();

    if (!(path & 0xFE)) {
        CR->pathFound = path & 1;
    }
    lined = (turn_toward_target(P->t[0], P->t[2], 0x200) == 0);
    switch (E->action_behavior) {
        case CR_PERCH: // preen a while, then a random perched animation (one of them caws)
            switch (E->action_state) {
                case 0:
                    cr_anim_start();
                    E->action_ticks_counter = (u16)((CbRand() & 0x1F) * 2 + 0x28);
                    // fallthrough
                case 1:
                    if (((s16)E->action_ticks_counter-- == 0) || (P->health < 0)) {
                        E->action_state = 2;
                    }
                    break;
                case 2:
                    cr_anim_start();
                    E->action_state = 3;
                    E->animationId = 0xB;
                    if (CbRand() & 1) {
                        E->animationId = 0xC;
                        if (CbRand() & 1) {
                            E->animationId = 2;
                            Snd_em(1); // caw
                        }
                    }
                    // fallthrough
                case 3:
                    E->action_state += (u8)Joint_move(0, 0x400);
                    break;
                default:
                    cr_set(1, 0, CR_PERCH);
                    break;
            }
            break;
        case CR_FLAP: // the first wingbeats up off the perch
            E->animationId = 1;
            E->move_speed_current = (s16)((CbRand() & 1) * 0x10 + 0x32);
            cr_once();
            Add_speedXZ(0);
            E->t[1] -= 30;
            if (E->animation_frame_id > 8) {
                CR->vy = E->animation_frame_id + 0x34;
                E->t[1] += CR->vy;
            }
            if ((E->ignore_player_flag == 0) || (E->t[1] > -1500)) {
                cr_set(1, 1, (E->t[1] > -800) ? CR_SWOOP : CR_SINK);
            }
            break;
        case CR_DIVE:
            E->animationId = 4;
            cr_once();
            E->move_speed_current = (s16)(200 - E->animation_frame_id * 6);
            if (E->move_speed_current > 0) {
                Add_speedXZ(0);
            }
            E->t[1] -= CR->highLimit / 0x28;
            if ((E->ignore_player_flag == 0) || (E->t[1] > -450)) {
                E->move_speed_current = 0;
                E->t[1] = -450;
                cr_after_turn(lined);
            }
            break;
        case CR_TURN: // a banking turn toward the player
            E->animationId = 3;
            CR->turnRate = (s16)((CbRand() & 1) * 0x20 - (CbRand() & 1) * 0x40 + 0x80);
            if (E->action_state == 0) {
                cr_anim_start();
                E->action_ticks_counter = (u16)((CbRand() & 0x1F) * 2 + 0x50);
            } else if (E->action_state == 1) {
                s16 turn = (s16)turn_toward_target(P->t[0], P->t[2], CR->turnRate);

                if (((s16)E->action_ticks_counter-- == 0) || (turn == 0)) {
                    E->action_state = 2;
                }
                Joint_move(0, 0x400);
                E->angle += turn;
            } else {
                cr_set(1, 0, CR_PERCH);
                E->player_pos_x = (s16)P->t[0];
                E->player_pos_z = (s16)P->t[2];
            }
            if (E->ignore_player_flag == 0) {
                cr_after_turn(lined);
            }
            break;
        case CR_STRIKE: // launch at the player from the ground
            E->animationId = 7;
            cr_once();
            if ((E->animation_frame_id > 8) && (CR->dist != 0)) {
                CR->vy = (s16)(((CR->dist / 100) + 200) * 2600 / CR->dist);
                if (CR->vy > 500) {
                    CR->vy = 200;
                }
                E->move_speed_current = (s16)(CR->dist / 100 + 200);
                cr_set(1, 1, CR_CLIMB);
                Snd_em(4); // wings
            }
            break;
        case CR_SINK: // glide down, turning toward the player
        case CR_CLIMB: { // climb, the same
            s32 climbing = (E->action_behavior == CR_CLIMB);

            E->animationId = climbing ? 5 : 0;
            E->angle += (s16)(turn_toward_target(P->t[0], P->t[2], 0x10) * CR->pathFound * 2);
            if ((CR->dist < 2000) && ((E->animation_frame_id % 9) == 0)) {
                Snd_em(5); // flapping
            }
            cr_loop();
            Add_speedXZ(0);
            if (++CR->vy > 500) {
                CR->vy = 200;
            }
            if (climbing) {
                E->t[1] -= CR->vy;
                CR->lowLimit = (s16)((CbRand() & 1) * -1000 + CR->altBias - 0x1194);
                if (E->t[1] < CR->lowLimit) {
                    cr_set(1, 1, CR_LEVEL);
                }
            } else {
                E->t[1] += CR->vy;
                CR->highLimit = (s16)(((CbRand() & 1) * -500 + CR->altBias) * 2 - 1500);
                if ((CR->highLimit < E->t[1]) || (E->t[1] > -1500)) {
                    cr_set(1, 1, CR_SWOOP);
                    if (!lined && !(E->behavior_flags & 1)) {
                        cr_set(1, 1, CR_DIVE);
                    }
                }
            }
            cr_check_peck();
            break;
        }
        case CR_SWOOP: // the swoop at the player
            E->animationId = 7;
            cr_once();
            E->move_speed_current = cr_speed_to_player();
            Add_speedXZ(0);
            if (E->animation_frame_id > 0x12) {
                cr_set(1, 1, CR_CLIMB);
                if ((E->behavior_flags & 1) && (CR->dist != 0)) {
                    CR->vy = (s16)((((CR->dist / 100) * 0x145) + 65000) * 8 / CR->dist);
                    if (CR->vy > 500) {
                        CR->vy = 200;
                    }
                }
            }
            break;
        case CR_CLOSE:
            E->animationId = 5;
            entity_rotate_toward_target(P->t[0], P->t[2], 0x40);
            cr_loop();
            Add_speedXZ(0);
            if (E->animation_frame_id > 8) {
                cr_set(1, 1, CR_CLIMB);
            }
            break;
        case CR_PECK: // the peck: 6 of RE1's health if it's still lined up when it ends
            E->animationId = 8;
            entity_rotate_toward_target(P->t[0], P->t[2], 0x30);
            cr_once();
            if (E->action_state == 0) { // the animation ended (cr_once went back to the perch)
                cr_set(1, 1, CR_SWOOP);
                if ((CR->dist < 0x5DC) && (turn_toward_target(P->t[0], P->t[2], 0x100) == 0) && CR->pathFound &&
                    (P->isBeingAttackedFlag == 0) && !gNvRe1DogFlinch) {
                    gNvRe1DogBite = 6;
                    Snd_em(3);
                    // (RE1 latches on here; this one pecks and flies off)
                    E->move_speed_current = cr_speed_to_player();
                    E->angle += (s16)((CbRand() & 1) * 0x800);
                    E->t[1] -= 100;
                    cr_set(1, 1, CR_CLIMB);
                }
            }
            break;
        case CR_LEVEL: // level out at the top, then sink again
            E->animationId = 5;
            cr_once();
            E->t[1] -= 25;
            Add_speedXZ(0);
            if (E->ignore_player_flag == 0) {
                cr_set(1, 1, CR_SINK);
            }
            break;
        case CR_DOWN:
            E->animationId = 0;
            entity_rotate_toward_target(P->t[0], P->t[2], 0x80);
            cr_loop();
            E->move_speed_current = 200;
            Add_speedXZ(0);
            E->t[1] += ++CR->vy;
            if (E->t[1] > -3000) {
                cr_set(1, 1, CR_DIVE);
            }
            break;
        case CR_TAKEOFF: // startled: a moment, then up
            if (E->action_state == 0) {
                cr_anim_start();
                E->action_ticks_counter = (u16)((CbRand() & 1) * 3 + (CbRand() & 1) * 8);
                E->ignore_player_flag = 1;
                E->behavior_flags |= 0x10;
            } else if (E->action_state == 1) {
                if ((s16)E->action_ticks_counter-- == 0) {
                    E->action_state = 2;
                }
            } else {
                cr_set(1, 1, CR_FLAP);
                Snd_em(4);
            }
            break;
        default:
            break;
    }
}

static void cr_run(void) {
    if (E->behavior_flags & 0x80) {
        return;
    }
    // flying into the player at speed from behind knocks it back (a hit for both)
    if ((E->behavior_flags & 0x10) && CR->touch && (P->isBeingAttackedFlag == 0) && (E->action_behavior != CR_PECK)) {
        if (E->behavior_flags & 1) {
            if ((u32)(-2500 - E->t[1]) > 300) {
                E->t[1] = -2500;
            }
            cr_set(1, 1, CR_PECK);
            return;
        }
        if ((E->move_speed_current > 300) && !is_facing_toward_entity()) {
            cr_set(2, 0, 0);
            return;
        }
    }
    if ((E->behavior_flags & 0x10) && CR->coll) { // against a wall: turn off it
        if (++CR->stuck > 5) {
            E->angle += (s16)turn_toward_target(P->t[0], P->t[2], 0x40);
        }
        if (CR->stuck > 0x1E) {
            CR->stuck = 0;
            E->angle += 0x400;
        }
    }
    CR->dist = (u16)(CbAbs(P->t[2] - E->t[2]) + CbAbs(P->t[0] - E->t[0]));
    // perched: a gun raised at it (RE1's player in the gun aim, behaviour 0x14) startles it up
    if ((E->ignore_player_flag == 0) && !(E->behavior_flags & 0x10) && (P->flags & 0xE0)) {
        E->angle += (s16)((CbRand() & 1) * 0x40);
        cr_set(1, 1, CR_TAKEOFF);
    }
    if (E->ignore_player_flag <= 1) {
        cr_behave();
    }
    E->status_flags &= 0x1F;
    entity_check_alert_range(5000);
    if (E->t[1] > -4500) {
        E->status_flags = (E->status_flags & 0x1F) | 0x40;
        if (E->t[1] > -2000) {
            entity_check_visual_range(2000);
        }
    }
    if (E->t[1] > -1500) {
        E->status_flags &= 0x1F;
        entity_check_visual_range(5000);
    }
}

static void cr_recoil(void) {
    if (E->action_behavior == 0) { // knocked down
        E->animationId = 6;
        cr_loop();
        E->move_speed_current = 200;
        E->t[1] += 200;
        if (CR->touch) {
            Add_speedXZ(0x800);
        }
        if (E->animation_frame_id == 1) {
            Snd_em(2);
        }
        if (E->t[1] > -450) {
            E->action_behavior = 1;
            E->action_state = 0;
            E->t[1] = -450;
        }
    } else { // up again
        E->animationId = 9;
        cr_once();
        if (E->action_state == 0) {
            cr_set(1, 1, CR_TURN);
            E->hit_state = 0;
            E->status_flags &= 0x1F;
        }
    }
}

static void cr_die(void) {
    switch (E->action_behavior) {
        case 0: // falling
            if (E->action_state == 0) {
                if (E->death_event_id != 0xFF) {
                    void NvFlags_Op(s32 bank, s32 bit, s32 mode);

                    NvFlags_Op(3, E->death_event_id, 0);
                }
            }
            E->animationId = 6;
            if (!(E->hit_state & 1)) {
                E->move_speed_current = 200;
                Add_speedXZ((turn_toward_target(P->t[0], P->t[2], 0x400) == 0) ? 0x800 : 0);
            }
            cr_loop();
            E->t[1] += 200;
            if (E->animation_frame_id == 1) {
                Snd_em(0);
            }
            if (E->t[1] > -450) {
                E->action_behavior = 1;
                E->action_state = 0;
                E->t[1] = -450;
            }
            break;
        case 1: // on the floor
            E->animationId = 0xA;
            cr_once();
            if (E->action_state == 0) {
                E->action_behavior = 4;
            }
            break;
        default:
            E->status_flags |= 10; // intangible, dead
            break;
    }
}

void NvCrow_Init(NvRe1Entity* e) {
    E = e;
    cr_set(1, 0, CR_PERCH);
    memset(&E->cr, 0, sizeof(E->cr));
    E->action_ticks_counter = 0;
    E->animationId = 2;
    E->death_timer = 0;
    E->hit_state = 0;
    E->collisionFlags |= 4;
    if (E->behavior_flags & 0x10) { // already flying
        CR->vy = 200;
        E->move_speed_current = (s16)((CbRand() & 1) * 0x10 + 0x32);
        cr_set(1, 1, CR_SINK);
        E->animationId = 0;
    }
    if (E->behavior_flags & 2) { // the script's: waits for its flight
        cr_set(8, 0, 0);
        E->animationId = 0;
    }
    Joint_move(0, 0x400);
    CbRand();
    E->health = (s16)((CbRand() & 7) * 2 + (CbRand() & 7) * 2 + (CbRand() & 7) * 2 + 10);
    if (E->behavior_flags & 1) { // the low flyer: one hit
        E->health = 0;
        CR->altBias = -400;
    }
    E->sca = sCrSca;
}

void NvCrow_Update(NvRe1Entity* e) {
    u8 hit;

    E = e;
    E->pos[0] = (s16)E->t[0];
    E->pos[1] = (s16)E->t[1];
    E->pos[2] = (s16)E->t[2];
    switch (E->state) {
        case 0:
            NvCrow_Init(e);
            break;
        case 1:
            cr_run();
            break;
        case 2:
            cr_recoil();
            break;
        case 3:
            cr_die();
            break;
        default: // 4-8: the script flies it (a held crow waits where it is)
            if (!(E->behavior_flags & 0x80)) {
                Joint_move(0, 0x200);
            }
            break;
    }
    SetScaWorld(E->sca, E->angle, E->scaWorld);
    SetScaWorld(P->sca, P->angle, P->scaWorld);
    CR->touch = 0;
    if (!(E->status_flags & 2)) {
        CR->touch = (u8)ResolveScaPush(P->sca, P->scaWorld, P->t, E);
        HandleEnemyCollisions();
    }
    E->collisionFlags &= ~0x08;
    hit = check_room_collision();
    CR->coll = hit;
    // the flight envelope: never through the floor or the ceiling
    if (E->t[1] > 0) {
        CR->vy = 200;
        E->t[1] = -100;
        cr_set(1, 1, CR_SWOOP);
    }
    if (E->t[1] < -30000) {
        CR->vy = 200;
        E->t[1] = -29000;
        cr_set(1, 1, CR_LEVEL);
    }
}
