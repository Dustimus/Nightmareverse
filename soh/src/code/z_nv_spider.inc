/*
 * Nightmareverse: RE1's Web Spinner (enemy type 3, EM1003), the giant spiders of 608 and later
 * the caves'. Included at the end of z_nv_zombie.c with the other ported enemies.
 *
 * As RE1 runs it (the decomp's entities/WebSpinner.cpp is the reference): a spider on the floor
 * wanders and creeps toward the player, turning to face her; lined up it lunges (a bite: 10 of
 * RE1's health) or rears and spits venom (10). One that starts on the ceiling
 * (behaviour 2) hangs there until she comes within 3000 or raises a gun, then drops, lands and
 * joins in. A hit makes it flinch and come back at her; at 0 it rolls over and dies.
 *
 * Left out, each a note where it would be: the walk's speed from its feet (RE1 measures how far
 * the planted leg pair moved; here the walk's extra speed alone), the venom's glob as a flying
 * effect (here the spit lands when she's lined up within its range at the spit's
 * frame, poisoning her one time in four), the webs, and the small spiders a dead one bursts
 * into (RE1's clones, chasing and biting 2: noted for when they're wanted).
 */

#define WS (&E->ws)

static const s16 sWsScaBig[8] = { (s16)0x8000, 0, -180, 0, 180, 1000, 0, 0 };
static const s16 sWsScaSmall[8] = { (s16)0x8000, 0, 0, 0, 0, 500, 0, 0 };

// RE1's health roll, by rand & 15
static const u8 sWsHealth[16] = { 99, 99, 99, 99, 119, 99, 99, 119, 99, 99, 99, 119, 99, 89, 99, 99 };
// the next move after a rest, by rand & 7, the second row while it sees her
static const u8 sWsNext[16] = { 2, 1, 2, 2, 2, 1, 2, 5, 11, 11, 5, 11, 3, 5, 4, 4 };
// the scuttle's sounds, by the walk's frame
static const u8 sWsStepSfx[16] = { 0, 2, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0 };

enum { WS_REST, WS_WALK, WS_CIRCLE, WS_RUSH, WS_STALK, WS_CREEP, WS_LUNGE, WS_SPIT, WS_DROP, WS_NONE, WS_SIDLE,
       WS_SHUFFLE };

static void ws_pick(u8 behaviour) {
    E->ignore_player_flag = 1; // the move runs to its end before the next is picked
    E->action_behavior = behaviour;
    E->action_state = 0;
}

static void ws_free(u8 behaviour) {
    E->ignore_player_flag = 0;
    E->action_behavior = behaviour;
    E->action_state = 0;
}

static void ws_anim(u8 id) {
    E->animation_frame_id = 0;
    E->timing_control = 0;
    E->blend_counter = 3;
    E->animationId = id;
}

static s32 ws_facing(s16 step) {
    return turn_toward_target(P->t[0], P->t[2], step) == 0;
}

static void ws_turn(s16 step) {
    E->angle = (E->angle + turn_toward_target(P->t[0], P->t[2], step)) & 0xFFF;
}

static s32 ws_aiming(void) {
    return (P->flags & 0x20) != 0;
}

/** Its bite or venom lands: RE1's first-playthrough damage (a second run's harder hits: when we have one). */
static void ws_bite(s16 damage) {
    gNvRe1DogBite = damage;
}

/* ---- what it does next (by the kind: 0 hunter, 1 lurker, 2/3 on the ceiling) ---- */

static void ws_decide_hunter(void) {
    if ((WS->dist < 6000) && !WS->sees) {
        ws_pick(WS_RUSH);
    }
    if (WS->cooldown == 0) {
        if (((CbRand() & 0x1FF) == 0) && WS->sees) {
            ws_pick(WS_LUNGE);
        }
        if ((WS->dist < 7000) && ws_facing(0x80)) {
            ws_pick(WS_LUNGE + (CbRand() & 1));
        }
    }
    if (ws_aiming() && !(CbRand() & 1)) {
        ws_pick(WS_SIDLE);
    }
    if (WS->stuck > 30) {
        ws_pick(WS_SIDLE);
    }
    if (P->isBeingAttackedFlag & 0x80) {
        ws_pick(WS_SHUFFLE);
    }
}

static void ws_decide_lurker(void) {
    s32 lined = ws_facing(0x80);
    s32 square = ws_facing(0x100);

    if (ws_aiming() && !lined) {
        ws_pick(WS_RUSH);
    }
    if ((WS->dist < 8000) && !lined) {
        ws_pick(WS_RUSH);
    }
    if (WS->cooldown == 0) {
        if ((WS->dist < 7500) && WS->sees) {
            ws_pick(WS_LUNGE + (CbRand() & 1));
        }
        if ((WS->dist < 6000) && square) {
            ws_pick(WS_SPIT);
        }
    }
    if (ws_aiming() && !(CbRand() & 0xF)) {
        ws_pick(WS_SIDLE);
    }
    if (WS->stuck > 30) {
        ws_pick(WS_SIDLE);
    }
}

static void ws_decide_ceiling(void) {
    if ((WS->dist < 3000) || ws_aiming()) {
        ws_pick(WS_DROP);
        return;
    }
    if (WS->dist > 5000) {
        ws_pick(WS_CIRCLE);
        if (ws_facing(0x80)) {
            ws_pick(WS_SHUFFLE);
        }
    }
}

/* ---- the moves ---- */

static void ws_rest(void) {
    if (E->action_state == 0) {
        ws_anim(0);
        E->action_state = 1;
        E->action_ticks_counter = (u16)((WS->sees ? 0 : 0x50) + (CbRand() & 0x3F));
    }
    Joint_move(0, 0x400);
    if (E->behavior_flags != 0) {
        return; // the lurkers and the ceiling ones rest until their own rule moves them
    }
    if (E->action_ticks_counter-- == 0) {
        ws_free(sWsNext[(CbRand() & 7) + (WS->sees ? 8 : 0)]);
    }
}

/** A walk toward her for a while (its length: rand times the behaviour, longer for the slow ones). */
static void ws_walk(u8 anim, s16 turn, s16 speed) {
    u8 sfx;

    E->animationId = anim;
    if (E->action_state == 0) {
        E->animation_frame_id = 0;
        E->timing_control = 0;
        E->blend_counter = 3;
        E->action_state = 1;
        E->action_ticks_counter = (u16)((CbRand() & 0x1F) * E->action_behavior);
        if (E->action_behavior < 2) {
            E->action_ticks_counter += 0x50;
        }
    }
    ws_turn(turn);
    sfx = sWsStepSfx[E->animation_frame_id & 15];
    if (sfx != 0) {
        Snd_em(sfx);
    }
    Joint_move(0, 0x400);
    E->move_speed_current = speed;
    Add_speedXZ(0);
    if (E->action_ticks_counter-- == 0) {
        ws_free(WS_REST);
    }
}

/** Circling: sidesteps one way a while, turning to walk at her once she's in front. */
static void ws_circle(void) {
    if (E->action_state == 0) {
        ws_anim(3);
        E->action_state = 1;
        E->action_ticks_counter = CbRand() & 0x3F;
        WS->turn = (E->action_ticks_counter & 1) ? -0x18 : 0x18;
    }
    E->angle = (E->angle + WS->turn) & 0xFFF;
    Joint_move(0, 0x400);
    if (WS->sees && (turn_toward_target(P->t[0], P->t[2], WS->turn) == 0)) {
        E->action_behavior = WS_STALK;
        E->action_state = 0;
        return;
    }
    if (E->action_ticks_counter-- == 0) {
        ws_free(WS->sees ? WS_STALK : WS_REST);
    }
}

/** Closing in: turns at its rate until it faces her or its time's up, then on to a shuffle. */
static void ws_approach(u8 anim, s16 turn) {
    s32 lined;

    E->animationId = anim;
    if (E->action_state == 0) {
        E->animation_frame_id = 0;
        E->timing_control = 0;
        E->blend_counter = 3;
        E->hit_state = 0;
        E->action_state = 1;
        E->action_ticks_counter = (CbRand() & 0x1F) + 0x50;
    } else if (E->action_state == 2) {
        ws_free(WS->sees ? WS_SHUFFLE : WS_REST);
        E->player_pos_x = (s16)P->t[0];
        E->player_pos_z = (s16)P->t[2];
        return;
    }
    E->angle = (E->angle + turn_toward_target(P->t[0], P->t[2], turn)) & 0xFFF;
    Joint_move(0, 0x400);
    lined = ws_facing(turn);
    if ((E->action_ticks_counter-- == 0) || lined) {
        E->action_state = 2;
    }
}

/** The lunge: a crouch, a turn to face her, a rush that bites if it touches her, the recovery. */
static void ws_lunge(void) {
    switch (E->action_state) {
        case 0:
            ws_anim(8);
            E->action_state = 1;
            // fall through
        case 1: // the crouch
            E->action_state += (u8)Joint_move(0, 0x400);
            E->move_speed_current = 100;
            Add_speedXZ(0);
            return;
        case 2:
            ws_anim(0xB);
            E->action_state = 3;
            E->action_ticks_counter = (CbRand() & 0x1F) + 0x14;
            // fall through
        case 3: // turning to face her
            Joint_move(0, 0x400);
            if (ws_facing(0x40)) {
                E->action_state = 4;
            } else {
                ws_turn(0x40);
            }
            if (E->action_ticks_counter-- == 0) {
                E->action_state = 6;
            }
            return;
        case 4:
            ws_anim(0xB);
            E->action_state = 5;
            E->action_ticks_counter = (CbRand() & 0x1F) + 10;
            // fall through
        case 5: // the rush
            Joint_move(0, 0x400);
            ws_turn(0x10);
            E->move_speed_current = 300;
            Add_speedXZ(0);
            if ((E->action_ticks_counter-- == 0) || !ws_facing(0x200)) {
                E->action_state = 6;
            }
            if (WS->touch && (P->isBeingAttackedFlag == 0) && !gNvRe1DogFlinch) {
                E->action_state = 6;
                Snd_em(4);
                ws_bite(10);
            }
            return;
        case 6:
            ws_anim(9);
            E->action_state = 7;
            E->action_ticks_counter = 7;
            // fall through
        case 7: // the recovery
            E->action_state += (u8)Joint_move(0, 0x400);
            E->move_speed_current = 100;
            Add_speedXZ(0);
            return;
        default:
            ws_free(WS_REST);
            WS->cooldown = 15;
            return;
    }
}

/** The spit: it rears and spits; the venom lands if she's in front of it, near enough. */
static void ws_spit(void) {
    if (E->action_state == 0) {
        ws_anim(0xC);
        E->action_state = 1;
        Snd_em(7);
        // where the glob would land: RE1's flies from the mouth and splashes within 600 of her
        if ((WS->dist < 6000) && ws_facing(0x100) && WS->sees && (P->isBeingAttackedFlag == 0) &&
            !gNvRe1DogFlinch) {
            ws_bite(10);
            NvPoison_Give((CbRand() & 1) && (CbRand() & 1)); // the venom: RE1's splash, one in four
        }
    }
    if (E->action_state == 2) {
        E->state = 1;
        ws_free(WS_SHUFFLE);
        WS->cooldown = 15;
        return;
    }
    E->action_state += (u8)Joint_move(0, 0x400);
}

/** The drop from the ceiling: it falls, rights itself, lands with a thud and joins in. */
static void ws_drop(void) {
    switch (E->action_state) {
        case 0:
            ws_anim(0);
            E->action_state = 1;
            E->action_ticks_counter = 0;
            // fall through
        case 1:
            E->t[1] += (s32)(s16)E->action_ticks_counter * (s16)E->action_ticks_counter * 8;
            E->action_ticks_counter++;
            WS->pitch -= 0x100;
            if (WS->pitch < 0) {
                WS->pitch = 0;
            }
            if (E->t[1] >= 0) {
                E->t[1] = 0;
                WS->pitch = 0;
                E->action_state = 2;
            }
            Joint_move(0, 0x400);
            return;
        case 2:
            ws_anim(7);
            E->action_state = 3;
            Snd_em(3);
            E->behavior_flags &= ~2; // a floor spider from now on
            // fall through
        case 3:
            E->action_state += (u8)Joint_move(0, 0x400);
            return;
        default:
            ws_free(WS_REST);
            return;
    }
}

/** A short sideways turn (behaviour 10): around her, then rest. */
static void ws_sidle(void) {
    if (E->action_state == 0) {
        ws_anim(6);
        E->action_state = 1;
        WS->turn = (s16)(((CbRand() & 1) ? -0x32C : 0x32C) + (WS->sees ? 0 : (CbRand() & 0xFFF)));
        E->move_speed_current = (s16)((CbRand() & 0x3F) + 200);
    } else if (E->action_state == 2) {
        E->hit_state = 0;
        ws_free(WS->sees ? WS_STALK : WS_REST);
        WS->stuck = 0;
        return;
    }
    E->action_state += (u8)Joint_move(0, 0x400);
    ws_turn(0x40);
    Add_speedXZ(WS->turn);
}

static void ws_act(void) {
    u32 path = entity_pathfind_update();

    if (!(path & 0xFE)) {
        WS->sees = (u8)(path & 1);
    }
    switch (E->action_behavior) {
        case WS_REST:
            ws_rest();
            break;
        case WS_WALK:
            ws_walk(3, WS->sees ? 0x10 : 0, 0x3C);
            break;
        case WS_CIRCLE:
            ws_circle();
            break;
        case WS_RUSH:
            ws_approach(2, 0x80);
            break;
        case WS_STALK:
            ws_walk(2, 0x30, 100);
            break;
        case WS_CREEP:
            ws_approach(3, 0x20);
            break;
        case WS_LUNGE:
            ws_lunge();
            break;
        case WS_SPIT:
            ws_spit();
            break;
        case WS_DROP:
            ws_drop();
            break;
        case WS_SIDLE:
            ws_sidle();
            break;
        case WS_SHUFFLE:
            ws_walk(3, 0x10, 0x14);
            break;
        default:
            break;
    }
}

static void ws_dist(void) {
    WS->dist = CbAbs(P->t[0] - E->t[0]) + CbAbs(P->t[2] - E->t[2]);
}

static void ws_run(void) {
    if (E->behavior_flags & 0x80) {
        return; // held by its script
    }
    ws_dist();
    if (E->ignore_player_flag == 0) {
        switch (E->behavior_flags & 3) {
            case 0:
                ws_decide_hunter();
                break;
            case 1:
                ws_decide_lurker();
                break;
            default:
                ws_decide_ceiling();
                break;
        }
    }
    ws_act();
    entity_check_visual_range(4000);
    if ((E->behavior_flags & 2) && (E->action_behavior != WS_DROP) && ((WS->dist < 3000) || ws_aiming())) {
        ws_pick(WS_DROP);
    }
    if (WS->cooldown != 0) {
        WS->cooldown--;
    }
    if (WS->coll == 0) {
        WS->stuck = 0;
    } else {
        WS->stuck++;
    }
}

/** A hit: it flinches, then comes at her (the lurker walking, the others rushing). */
static void ws_flinch(void) {
    if (E->action_state == 0) {
        ws_anim(1);
        E->action_state = 1;
        E->move_speed_current = 0;
        Snd_em(4);
    } else if (E->action_state == 2) {
        E->state = 1;
        E->hit_state = 0;
        E->player_pos_x = (s16)P->t[0];
        E->player_pos_z = (s16)P->t[2];
        ws_pick(WS_STALK - (E->behavior_flags & 1));
        if (!WS->sees) {
            ws_pick(WS_SIDLE);
        }
        return;
    }
    E->action_state += (u8)Joint_move(0, 0x400);
}

/** Death: it rolls over, twitches, and lies still (the burst into small spiders left out). */
static void ws_die(void) {
    switch (E->action_state) {
        case 0:
            if (E->death_event_id != 0xFF) {
                void NvFlags_Op(s32 bank, s32 bit, s32 mode);

                NvFlags_Op(3, E->death_event_id, 0);
            }
            ws_anim(4);
            E->action_state = 1;
            E->move_speed_current = 0;
            E->sca = sWsScaSmall;
            Snd_em(5);
            if (WS->pitch != 0) { // shot off the ceiling: it falls first
                WS->pitch = 0;
                E->t[1] = 0;
            }
            // fall through
        case 1:
            E->action_state += (u8)Joint_move(0, 0x400);
            return;
        case 2:
            ws_anim(5);
            E->action_state = 3;
            E->action_ticks_counter = 0x78;
            // fall through
        case 3:
            Joint_move(0, 0x400);
            if (E->action_ticks_counter-- == 0) {
                E->action_state = 4;
            }
            return;
        default:
            E->status_flags |= 10; // intangible, dead
            return;
    }
}

void NvSpider_Init(NvRe1Entity* e) {
    E = e;
    memset(&E->ws, 0, sizeof(E->ws));
    E->state = 1;
    E->ignore_player_flag = 0;
    E->action_behavior = WS_REST;
    E->action_state = 0;
    E->action_ticks_counter = 0;
    E->death_timer = 0;
    E->hit_state = 0;
    E->animationId = 0;
    if (E->behavior_flags < 2) {
        E->t[1] = 0;
    } else {
        // on the ceiling, upside down (stage 3's caves hang theirs higher, at -7200)
        E->t[1] = -6136;
        WS->pitch = 0x800;
        if (E->behavior_flags > 3) {
            E->behavior_flags = 2;
        }
    }
    Joint_move(0, 0x400);
    CbRand();
    E->health = sWsHealth[CbRand() & 15];
    E->sca = sWsScaBig;
}

void NvSpider_Update(NvRe1Entity* e) {
    E = e;
    E->pos[0] = (s16)E->t[0];
    E->pos[1] = (s16)E->t[1];
    E->pos[2] = (s16)E->t[2];
    switch (E->state) {
        case 0:
            NvSpider_Init(e);
            break;
        case 1:
            ws_run();
            break;
        case 2:
            ws_flinch();
            break;
        case 3:
            ws_die();
            break;
        default:
            break;
    }
    E->pitch = WS->pitch;
    if (E->state != 3 || E->action_state < 4) {
        SetScaWorld(E->sca, E->angle, E->scaWorld);
        SetScaWorld(P->sca, P->angle, P->scaWorld);
        WS->touch = 0;
        if (!(E->status_flags & 2) && (WS->pitch == 0)) {
            WS->touch = (u8)ResolveScaPush(P->sca, P->scaWorld, P->t, E);
            HandleEnemyCollisions();
        }
        WS->coll = check_room_collision();
    }
}
