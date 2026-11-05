/*
 * Nightmareverse: RE1's Black Tiger (enemy type 4, EM1004), the giant spider of the caves.
 * Included at the end of z_nv_zombie.c with the other ported enemies.
 *
 * As RE1 runs it (the decomp's entities/BlackTiger.cpp is the reference; its scheme is the Web
 * Spinner's, z_nv_spider.inc): it picks a move when it's free and runs it to the end. Seeing her
 * within 5000 it goes for the bite (a crouch, a turn to face her, the strike: 20 of RE1's
 * health), lined up it spits venom (7), far off it walks at her, and after a while stuck it
 * strafes or rushes. Hit, it rears and comes on; it has 204 health and dies on its back.
 *
 * Left out, each a note where it would be: the walk's speed from its feet (as the Web
 * Spinner's), the venom as a flying effect, the bite's reach (RE1 measures two
 * of its leg joints to her, 1300; here within 3000 and in front at the strike frame), and the
 * web threads and small spiders it bursts into when it dies.
 */

#define BT (&E->bt)

static const s16 sBtScaIdle[8] = { (s16)0x8000, -500, -180, 0, 180, 2500, 0, 0 };
static const s16 sBtScaWalk[8] = { (s16)0x8000, 0, -180, 1000, 180, 2000, 0, 0 };
static const s16 sBtScaDead[8] = { (s16)0x8000, 0, 0, 0, 0, 100, 0, 0 };

static const u8 sBtStepSfx[16] = { 0, 2, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0 };

enum { BT_REST, BT_WALK, BT_STRAFE, BT_RUSH, BT_WALK_ON = 4, BT_CIRCLE = 10, BT_BITE = 12, BT_SPIT = 13 };

static void bt_take(u8 behaviour) {
    E->ignore_player_flag = 1;
    E->action_behavior = behaviour;
    E->action_state = 0;
}

static void bt_free(void) {
    E->ignore_player_flag = 0;
    E->action_behavior = BT_REST;
    E->action_state = 0;
    E->sca = sBtScaIdle;
}

static void bt_anim(u8 id) {
    E->animation_frame_id = 0;
    E->timing_control = 0;
    E->blend_counter = 3;
    E->animationId = id;
}

static void bt_pick(void) {
    BT->dist = CbAbs(P->t[0] - E->t[0]) + CbAbs(P->t[2] - E->t[2]);
    if (BT->cooldown != 0) { // just in: it walks at her if she's not right there
        if (BT->dist > 2000) {
            E->action_behavior = BT_WALK;
        }
        BT->cooldown--;
        return;
    }
    if ((BT->stuck < 0x15) && BT->sees) {
        if (BT->dist < 5000) {
            bt_take(BT_BITE);
        }
        if ((BT->delay == 0) && (turn_toward_target(P->t[0], P->t[2], 0x200) == 0)) {
            bt_take(BT_SPIT);
        }
        if (BT->dist > 9000) {
            E->ignore_player_flag = 0;
            E->action_behavior = BT_WALK;
        }
    } else { // lost her, or stuck: a strafe or a rush
        bt_take((u8)((CbRand() & 1) + BT_STRAFE));
        BT->stuck = 0;
    }
}

static void bt_rest(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        bt_anim(0);
    }
    Joint_move(0, 0x400);
}

static void bt_walk(void) {
    u8 sfx;

    E->animationId = 3;
    if (BT->sees) {
        E->angle = (E->angle + turn_toward_target(P->t[0], P->t[2], 0x10)) & 0xFFF;
    }
    if (E->action_state == 0) {
        E->action_state = 1;
        E->animation_frame_id = 0;
        E->timing_control = 0;
        E->blend_counter = 3;
        E->action_ticks_counter = (u16)((CbRand() & 0x1F) * E->action_behavior);
        if (E->action_behavior < 2) {
            E->action_ticks_counter += 0x50;
        }
        E->sca = sBtScaWalk;
    }
    sfx = sBtStepSfx[E->animation_frame_id & 15];
    if (sfx != 0) {
        Snd_em(sfx);
    }
    Joint_move(0, 0x400);
    E->move_speed_current = 0x50;
    Add_speedXZ(0);
    if (E->action_ticks_counter-- == 0) {
        bt_free();
    }
}

static void bt_strafe(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        bt_anim(3);
        E->action_ticks_counter = CbRand() & 0x3F;
        BT->turn = (E->action_ticks_counter & 1) ? -0x18 : 0x18;
        E->sca = sBtScaWalk;
    }
    E->angle = (E->angle + BT->turn) & 0xFFF;
    Joint_move(0, 0x400);
    if (BT->sees && (turn_toward_target(P->t[0], P->t[2], BT->turn) == 0) && (BT->delay == 0)) {
        bt_take(BT_SPIT);
        return;
    }
    if (E->action_ticks_counter-- == 0) {
        bt_free();
    }
}

/** A rush: it turns at her fast until it faces her or its time's up. */
static void bt_rush(void) {
    s32 turn;

    E->animationId = 2;
    if (E->action_state == 0) {
        E->animation_frame_id = 0;
        E->timing_control = 0;
        E->blend_counter = 3;
        E->hit_state = 0;
        E->action_state = 1;
        E->action_ticks_counter = (CbRand() & 0x1F) + 0x50;
        E->sca = sBtScaWalk;
    } else if (E->action_state == 2) {
        E->hit_state = 0;
        bt_free();
        E->player_pos_x = (s16)P->t[0];
        E->player_pos_z = (s16)P->t[2];
        return;
    }
    turn = turn_toward_target(P->t[0], P->t[2], 0x80);
    if ((E->action_ticks_counter-- == 0) || (turn == 0)) {
        E->action_state = 2;
    }
    Joint_move(0, 0x400);
    E->angle = (E->angle + turn) & 0xFFF;
}

/** Around her (after it's been stuck): a sweeping sidestep, its root motion carrying it. */
static void bt_circle(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        bt_anim(6);
        BT->turn = (s16)(((CbRand() & 1) ? -0x32C : 0x32C) + (BT->sees ? 0 : (CbRand() & 0xFFF)));
        E->move_speed_current = (s16)((CbRand() & 0x3F) + 0xFA);
        Add_speedXZ(BT->turn);
    } else if (E->action_state == 2) {
        E->hit_state = 0;
        E->ignore_player_flag = 0;
        E->action_behavior = BT->sees ? BT_WALK_ON : BT_REST;
        E->action_state = 0;
        BT->stuck = 0;
        return;
    }
    E->action_state += (u8)Joint_move(0, 0x400);
    E->angle = (E->angle + turn_toward_target(P->t[0], P->t[2], 0x40)) & 0xFFF;
    Add_speedXZ(BT->turn);
}

/** The bite: a crouch, a turn to face her, the strike. */
static void bt_bite(void) {
    switch (E->action_state) {
        case 0:
            E->action_state = 1;
            bt_anim(8);
            // fall through
        case 1:
            E->action_state += (u8)Joint_move(0, 0x400);
            return;
        case 2:
            E->action_state = 3;
            bt_anim(0xB);
            E->action_ticks_counter = (CbRand() & 0x1F) + 0x14;
            // fall through
        case 3: {
            s32 turn;

            Joint_move(0, 0x400);
            turn = turn_toward_target(P->t[0], P->t[2], 0x40);
            E->angle = (E->angle + turn) & 0xFFF;
            if (turn == 0) {
                E->action_state = 4;
            }
            if (E->action_ticks_counter-- == 0) {
                E->action_state = 4;
            }
            return;
        }
        case 4:
            E->action_state = 5;
            bt_anim(9);
            // fall through
        case 5:
            BT->dist = CbAbs(P->t[0] - E->t[0]) + CbAbs(P->t[2] - E->t[2]);
            if ((E->animation_frame_id == 0xC) && (P->isBeingAttackedFlag == 0) && !gNvRe1DogFlinch &&
                (BT->dist < 3000) && (turn_toward_target(P->t[0], P->t[2], 0x200) == 0)) {
                Snd_em(4);
                gNvRe1DogBite = 20;
            }
            E->action_state += (u8)Joint_move(0, 0x400);
            return;
        default:
            Snd_em(3);
            bt_free();
            if (P->health < 0) { // she's down: it keeps at her
                bt_take(BT_BITE);
            }
            return;
    }
}

/** The spit: it rears and spits; the venom lands if she's in front of it, near enough. */
static void bt_spit(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        bt_anim(0xC);
        Snd_em(7);
        BT->dist = CbAbs(P->t[0] - E->t[0]) + CbAbs(P->t[2] - E->t[2]);
        if ((BT->dist < 7000) && (turn_toward_target(P->t[0], P->t[2], 0x200) == 0) && BT->sees &&
            (P->isBeingAttackedFlag == 0) && !gNvRe1DogFlinch) {
            gNvRe1DogBite = 7;
            NvPoison_Give((CbRand() & 1) && (CbRand() & 1)); // the venom: RE1's splash, one in four
        }
    } else if (E->action_state == 2) {
        bt_take(BT_BITE); // and after it, the bite
        BT->delay = 0x14;
        return;
    }
    E->action_state += (u8)Joint_move(0, 0x400);
}

static void bt_act(void) {
    u32 path = entity_pathfind_update();

    if (!(path & 0xFE)) {
        BT->sees = (u8)(path & 1);
    }
    switch (E->action_behavior) {
        case BT_REST:
            bt_rest();
            break;
        case BT_WALK:
        case BT_WALK_ON:
            bt_walk();
            break;
        case BT_STRAFE:
            bt_strafe();
            break;
        case BT_RUSH:
            bt_rush();
            break;
        case BT_CIRCLE:
            bt_circle();
            break;
        case BT_BITE:
            bt_bite();
            break;
        case BT_SPIT:
            bt_spit();
            break;
        default:
            break;
    }
}

static void bt_run(void) {
    if (E->ignore_player_flag == 0) {
        bt_pick();
        bt_act();
    } else if (E->ignore_player_flag == 1) {
        bt_act();
    }
    entity_check_visual_range(4000);
    if (BT->delay != 0) {
        BT->delay--;
    }
    if (BT->coll == 0) {
        BT->stuck = 0;
    } else if (++BT->stuck > 0x3C) { // long against a wall: it goes round
        bt_take(BT_CIRCLE);
    }
}

/** A hit: it rears back, then comes on at her. */
static void bt_flinch(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        bt_anim(1);
        E->move_speed_current = 0;
        Snd_em(4);
    } else if (E->action_state == 2) {
        E->state = 1;
        E->hit_state = 0;
        bt_take(BT->sees ? BT_RUSH : BT_STRAFE);
        return;
    }
    E->action_state += (u8)Joint_move(0, 0x400);
}

/** Death: it rolls onto its back, its legs curl, and it lies still. */
static void bt_die(void) {
    switch (E->action_state) {
        case 0:
            if (E->death_event_id != 0xFF) {
                void NvFlags_Op(s32 bank, s32 bit, s32 mode);

                NvFlags_Op(3, E->death_event_id, 0);
            }
            E->action_state = 1;
            bt_anim(4);
            E->move_speed_current = 0;
            E->sca = sBtScaDead;
            Snd_em(5);
            // fall through
        case 1:
            E->action_state += (u8)Joint_move(0, 0x400);
            return;
        case 2:
            E->action_state = 3;
            bt_anim(5);
            E->action_ticks_counter = 0xB4;
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

void NvBlackTiger_Init(NvRe1Entity* e) {
    E = e;
    memset(&E->bt, 0, sizeof(E->bt));
    E->state = 1;
    E->ignore_player_flag = 0;
    E->action_behavior = BT_REST;
    E->action_state = 0;
    E->action_ticks_counter = 0;
    E->hit_state = 0;
    bt_anim(0);
    E->blend_counter = 0;
    Joint_move(0, 0x400);
    CbRand();
    E->health = 204;
    E->sca = sBtScaIdle;
    BT->cooldown = 0x2D;
}

void NvBlackTiger_Update(NvRe1Entity* e) {
    E = e;
    E->pos[0] = (s16)E->t[0];
    E->pos[1] = (s16)E->t[1];
    E->pos[2] = (s16)E->t[2];
    switch (E->state) {
        case 0:
            NvBlackTiger_Init(e);
            break;
        case 1:
            bt_run();
            break;
        case 2:
            bt_flinch();
            break;
        case 3:
            bt_die();
            break;
        default:
            break;
    }
    if ((E->state != 3) || (E->action_state < 4)) {
        SetScaWorld(E->sca, E->angle, E->scaWorld);
        SetScaWorld(P->sca, P->angle, P->scaWorld);
        if (!(E->status_flags & 2)) {
            ResolveScaPush(P->sca, P->scaWorld, P->t, E);
            HandleEnemyCollisions();
        }
        BT->coll = check_room_collision();
    }
}
