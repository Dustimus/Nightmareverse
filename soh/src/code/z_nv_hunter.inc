/*
 * Nightmareverse: RE1's Hunter (enemy type 6, EM1006), the return mansion's enemy. Included at
 * the end of z_nv_zombie.c with the dog, sharing the RE1 entity helpers.
 *
 * As RE1 runs it (the decomp's entities/Hunter.cpp is the reference): each tick a decision layer
 * picks what to do by the Hunter's variant (the SCD record's behaviour byte, low nibble), then the
 * chosen behaviour runs: stand, run at the player, the slow approach once the path finder has
 * found him, the claw swipe up close, and the leaping swipe from further off. A hit staggers it
 * (two quick hits make it side-step away); death is its fall and last howl.
 *
 * Left out, each a note where it would be: the decapitation (RE1's leap and grab on a badly hurt
 * player: Link's health isn't RE1's, so its gate never opens), pairs hunting together (a lone
 * Hunter acts as one whose partner is gone, as RE1's does), the scripted intros and formation
 * jumps (behaviour flags 0x08 and 0x40: held, as the dogs are, until their script lets them go),
 * blood and joint tints.
 */

#define HN (&E->hn)

static const s16 sHnScaA[8] = { (s16)0x8000, 0, -180, 0, 180, 900, 0, 0 };
static const s16 sHnScaB[8] = { (s16)0x8000, 0, -180, 0, 180, 500, 0, 0 }; // flag 0x20
static const u8 sHnHealth[16] = { 95, 95, 95, 95, 79, 95, 79, 111, 95, 95, 79, 95, 95, 79, 95, 95 };

enum { HN_IDLE, HN_CHASE, HN_APPROACH, HN_NONE, HN_SWIPE, HN_POUNCE, HN_JUMP, HN_HOLD, HN_FORM, HN_HOWL,
       HN_BACKSTEP, HN_INTRO };

static s32 hn_dist(void) {
    return CbAbs(P->t[0] - E->t[0]) + CbAbs(P->t[2] - E->t[2]);
}

static void hn_do(u8 behaviour) {
    E->ignore_player_flag = 1;
    E->action_behavior = behaviour;
    E->action_state = 0;
}

static void hn_anim(u8 id, u8 blend) {
    E->animationId = id;
    E->animation_frame_id = 0;
    E->timing_control = 0;
    E->blend_counter = blend;
}

/** The claw's reach: RE1 tests a square around the hand joint; here a point ahead of the body. */
static u8 hn_reaches(s16 ahead, s16 reach) {
    s32 x;
    s32 z;

    RotY(E->angle, ahead, 0, &x, &z);
    return (CbAbs(P->t[0] - (E->t[0] + x)) <= reach) && (CbAbs(P->t[2] - (E->t[2] + z)) <= reach);
}

/** A claw lands: RE1's damage on its health (z_nv_enemy.c turns it into hearts and a knock back). */
static void hn_claw(s16 damage, u8 snd) {
    gNvRe1DogBite = damage;
    Snd_em(snd);
}

/* ---- the decision layer, by variant ---- */

static void hn_decide(void) {
    s32 d = hn_dist();
    u8 variant = E->behavior_flags & 0xF;

    if (E->behavior_flags & 0x10) { // a coward: keeps its distance until the player comes close
        if (((HN->pathFound != 0) && (d < 9000)) || (d < 4000)) {
            E->behavior_flags &= ~0x10;
        }
        return;
    }
    if ((variant >= 4) && (variant <= 6)) { // walks in, then acts as the plain kind
        E->status_flags &= 0x1F;
        if (d < 4000) {
            E->behavior_flags -= 4;
        }
        return;
    }
    // once the path finder has reached the player, close in slowly (6000 for the hard kinds)
    if ((HN->pathFound != 0) && (d < ((variant == 0) ? 4000 : 6000))) {
        if (E->action_behavior != HN_APPROACH) {
            E->action_state = 0;
            E->blend_counter = 7;
        }
        E->action_behavior = HN_APPROACH;
        E->ignore_player_flag = 0;
    }
    if ((P->isBeingAttackedFlag != 0) || gNvRe1DogFlinch) {
        return;
    }
    // in sight, close and lined up: the swipe
    if (!check_line_of_sight(P->t[0], P->t[2]) && (d < 3000) && (turn_toward_target(P->t[0], P->t[2], 0x80) == 0)) {
        hn_do(HN_SWIPE);
        return;
    }
    // further off and lined up, with the player turned away: the leaping swipe (RE1's variant 2
    // waits for her to be running away; variant 7 never leaps)
    if ((HN->pathFound != 0) && (variant != 7) && (d > 6400) && (d < 12000) &&
        (turn_toward_target(P->t[0], P->t[2], 0x20) == 0) && (P->speed > 60) && !is_facing_toward_entity()) {
        hn_do(HN_JUMP);
        E->status_flags &= 0x1F;
    }
}

/* ---- behaviours ---- */

static void hn_idle(void) {
    u8 variant = E->behavior_flags & 0xF;

    if ((variant == 1) || ((variant >= 4) && (variant <= 6))) { // walking in place, steered by the layer above
        if (E->action_state == 0) {
            E->action_state = 1;
            hn_anim(1, 7);
            E->move_speed_current = 0;
        }
        Joint_move(0, 0x200);
        return;
    }
    if (E->action_state == 0) {
        E->action_state = 1;
        hn_anim(0x15, 7);
        HN->ticks = 0x1E;
        E->move_speed_current = 0;
    }
    Joint_move(0, 0x200);
    if (HN->ticks-- == 0) {
        if (E->behavior_flags & 0x10) {
            HN->ticks = 200;
            return;
        }
        E->ignore_player_flag = 0;
        E->action_behavior = HN_CHASE;
        E->action_state = 0;
    }
}

static void hn_run(void) {
    E->animationId = 0x10;
    E->move_speed_current = 0x28;
    entity_update_wander_turn(E->move_dist, &HN->roomHit, &E->turn_counter, 0x18, 0x3C);
    if (E->action_state == 0) {
        E->action_state = 1;
        E->animation_frame_id = 0;
        E->timing_control = 0;
        E->blend_counter = 3;
    }
    if ((E->animation_frame_id == 1) || (E->animation_frame_id == 32)) {
        Snd_em(0); // footfall
    }
    if (hn_dist() < 2000) {
        entity_rotate_toward_target(P->t[0], P->t[2], 0x18);
    }
    Joint_move(0, 0x400);
    Add_speedXZ(0);
}

/** The slow approach: up to 90 ticks of stalking in, turning hard toward the player inside 4000. */
static void hn_approach(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        hn_anim(2, 7);
        E->move_speed_current = 0xF0;
        HN->ticks = 10;
        HN->approach = 0;
    }
    if (++HN->approach == 90) {
        E->action_behavior = HN_CHASE;
        E->action_state = 0;
    }
    Joint_move(0, 0x200);
    if ((E->animation_frame_id == 1) || (E->animation_frame_id == 15)) {
        Snd_em(1);
    }
    entity_update_wander_turn(E->move_dist, &HN->roomHit, &E->turn_counter, 0x60, 0x3C);
    HN->ticks--;
    if (hn_dist() < 4000) {
        entity_rotate_toward_target(P->t[0], P->t[2], 0x60);
    }
    Add_speedXZ(0);
}

/** The claw swipe: wind up, swing (frames 4-11 can catch the player: 10 of RE1's health). */
static void hn_swipe(void) {
    switch (E->action_state) {
        case 0:
            E->action_state = 1;
            hn_anim(5, 7);
            E->move_speed_current = 0;
            break;
        case 1:
            if (!Joint_move(0, 0x200)) {
                if (((u8)(E->animation_frame_id - 4) < 8) && (P->isBeingAttackedFlag == 0) && !gNvRe1DogFlinch &&
                    hn_reaches(700, 800)) {
                    hn_claw(10, 5);
                    E->action_state = 2;
                }
            } else {
                E->action_state = 4;
            }
            break;
        case 2: // the follow-through after a hit
            E->move_speed_current = 200;
            Add_speedXZ(0x52C);
            E->action_state = 3;
            HN->ticks = 4;
            break;
        case 3:
            E->move_speed_current = 0;
            if (Joint_move(0, 0x200)) {
                E->action_state = 4;
            }
            break;
        default: // back to the chase (RE1 sometimes chains a pounce here; see the top)
            E->ignore_player_flag = 0;
            E->action_behavior = HN_CHASE;
            E->action_state = 0;
            break;
    }
}

/**
 * The leaping swipe: a hop, then a long arc (rising 580, gravity -50) with both claws reaching
 * through it (frames 20-25 the long reach, 18-19 the near): 15 of RE1's health.
 */
static void hn_jump(void) {
    static const struct {
        u8 start;
        u8 window;
        s16 ahead;
        s16 reach;
    } sReach[2] = { { 18, 2, 1000, 600 }, { 20, 5, 0, 1500 } };

    switch (E->action_state) {
        case 0:
            E->action_state = 1;
            hn_anim(0x13, 7);
            E->death_timer = 0;
            E->move_speed_current = 0x5A;
            break;
        case 1:
            E->status_flags &= 0x1F;
            Joint_move(0, 0x200);
            if (E->animation_frame_id & 4) {
                E->action_state = 2;
                E->move_speed_current = 0x82;
                Snd_em(2);
            }
            Add_speedXZ(0);
            break;
        case 2:
        case 4: {
            s32 k;

            E->status_flags &= 0x1F;
            if (E->animation_frame_id > 8) {
                E->status_flags |= (E->action_state == 2) ? 0x80 : 0xC0;
            }
            if (E->action_state == 2) {
                for (k = 1; k >= 0; k--) {
                    if (((u8)(E->animation_frame_id - sReach[k].start) < sReach[k].window) &&
                        (P->isBeingAttackedFlag == 0) && !gNvRe1DogFlinch &&
                        hn_reaches(sReach[k].ahead + 700, sReach[k].reach)) {
                        hn_claw(15, 3);
                        E->action_state = 4;
                        break;
                    }
                }
            }
            Joint_move(0, 0x400);
            if (cb_ballistic(E->move_speed_current, 0x244, -50)) {
                E->status_flags &= 0x1F;
                if (E->action_state == 4) { // landed after a hit: straight on
                    E->ignore_player_flag = 0;
                    E->action_behavior = HN_APPROACH;
                    E->action_state = 0;
                } else {
                    E->action_state = 3;
                    E->move_speed_current = 0x5A;
                }
            }
            break;
        }
        default: // land, then the approach
            if (Joint_move(0, 0x400)) {
                E->ignore_player_flag = 0;
                E->action_behavior = HN_APPROACH;
                E->action_state = 0;
            }
            Add_speedXZ(0);
            break;
    }
}

/** Side-step away after two quick hits, then run again. */
static void hn_backstep(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        hn_anim(0x14, 3);
        E->move_speed_current = 0x50;
    }
    Add_speedXZ((E->turn_counter & 1) ? 0x400 : -0x400);
    if (Joint_move(0, 0x200)) {
        E->ignore_player_flag = 0;
        E->action_behavior = HN_CHASE;
        E->action_state = 0;
    }
}

static void hn_behave(void) {
    u8 path = (u8)entity_pathfind_update();

    if ((HN->pathFound == 0) && !(path & 0xFE) && (path & 1)) {
        HN->pathFound = 1;
        HN->repause = 0x3C;
    }
    zone_path_find(P->t[0], P->t[2], &E->player_pos_x, &E->player_pos_z);
    switch (E->action_behavior) {
        case HN_IDLE:
            hn_idle();
            break;
        case HN_CHASE:
            hn_run();
            break;
        case HN_APPROACH:
            hn_approach();
            break;
        case HN_SWIPE:
            hn_swipe();
            break;
        case HN_JUMP:
            hn_jump();
            break;
        case HN_BACKSTEP:
            hn_backstep();
            break;
        default: // the pounce and its hold, the formation jump, the howl, the intro: see the top
            E->ignore_player_flag = 0;
            E->action_behavior = HN_CHASE;
            E->action_state = 0;
            break;
    }
}

static void hn_state_run(void) {
    s32 dy;

    if (E->behavior_flags & 0x80) {
        return;
    }
    E->status_flags = (E->status_flags & 0x1F) | 0x40;
    entity_check_visual_range(4000);
    dy = P->t[1] - E->t[1];
    if (dy != 0) {
        E->status_flags = (E->status_flags & 0x1F) | ((dy < 0) ? 0x20 : 0x80);
    }
    if (HN->repause != 0) {
        HN->repause--;
    }
    if (E->ignore_player_flag == 0) {
        hn_decide();
    }
    hn_behave();
}

/* ---- hit and death ---- */

static void hn_hit(void) {
    if (E->ignore_player_flag == 0) {
        HN->hits++;
        if (HN->hitTimer == 0) {
            HN->hitTimer = 0x32;
        }
        E->state = 2;
        E->ignore_player_flag = 1;
        // a hit from the front or behind picks the stagger (RE1's +1 when it isn't facing him)
        E->action_behavior = ((turn_toward_target(P->t[0], P->t[2], 0x400) >> 10) & 1);
        E->action_state = 0;
    }
    E->animationId = (E->action_behavior == 0) ? 3 : 4;
    switch (E->action_state) {
        case 0:
            E->action_state = 1;
            E->move_speed_current = 0;
            E->animation_frame_id = 0;
            E->timing_control = 0;
            E->blend_counter = 7;
            Snd_em(6); // the cry
            break;
        case 1:
            if (Joint_move(0, 0x200)) {
                E->action_state = 2;
            }
            break;
        default:
            E->player_pos_x = (s16)P->t[0];
            E->player_pos_z = (s16)P->t[2];
            E->hit_state = 0;
            E->state = 1;
            E->ignore_player_flag = 0;
            E->action_behavior = HN_CHASE;
            E->action_state = 0;
            if ((HN->hitTimer != 0) && (HN->hits == 2)) { // two quick hits: side-step away
                HN->hitTimer = 0;
                HN->hits = 0;
                E->status_flags &= 0x1F;
                hn_do(HN_BACKSTEP);
            }
            break;
    }
}

static void hn_die(void) {
    static u8 sHowled;

    if (E->ignore_player_flag == 0) {
        E->state = 3;
        E->ignore_player_flag = 1;
        E->action_state = 0;
        if (E->death_event_id != 0xFF) {
            void NvFlags_Op(s32 bank, s32 bit, s32 mode);

            NvFlags_Op(3, E->death_event_id, 0);
        }
        sHowled = 0;
        hn_anim(0x11, 7);
        E->move_speed_current = 0;
    }
    if (E->action_state == 0) {
        if ((E->animation_frame_id == 0x24 || E->animation_frame_id == 0x32) && !sHowled) {
            Snd_em(7); // the last howl
            sHowled = 1;
        }
        if (E->animation_frame_id == 0x4B) {
            Snd_em(4); // it hits the floor
        }
        if (Joint_move(0, 0x200)) {
            E->action_state = 1;
            E->status_flags |= 10; // intangible, dead
            E->animation_frame_id = (u8)(E->anim->numSteps[E->animationId] - 1); // stays down
        }
    }
}

/* ---- entry points ---- */

void NvHunter_Init(NvRe1Entity* e) {
    E = e;
    E->state = 1;
    E->ignore_player_flag = 0;
    E->action_behavior = 0;
    E->action_state = 0;
    E->animationId = 0;
    E->hit_state = 0;
    memset(&E->hn, 0, sizeof(E->hn));
    E->health = sHnHealth[CbRand() & 0xF];
    E->sca = (E->behavior_flags & 0x20) ? sHnScaB : sHnScaA;
    if ((E->behavior_flags & 0xF) < 2) {
        E->behavior_flags = (E->behavior_flags & 0xF0) | 2; // no partner here: hunts alone
    }
    if ((E->behavior_flags & 0xF) == 0) {
        E->behavior_flags |= 2;
    }
    if (E->behavior_flags & 8) { // a scripted entrance: parked until the script releases it
        E->ignore_player_flag = 1;
        E->action_behavior = HN_INTRO;
    }
}

void NvHunter_Update(NvRe1Entity* e) {
    E = e;
    E->pos[0] = (s16)E->t[0];
    E->pos[1] = (s16)E->t[1];
    E->pos[2] = (s16)E->t[2];
    if (HN->hitTimer != 0) {
        if (--HN->hitTimer == 0) {
            HN->hits = 0;
        }
    }
    if (E->behavior_flags & 0x48) { // the script's (event entrances): held where it put it
        Joint_move(0, 0x200);
        return;
    }
    switch (E->state) {
        case 0:
            NvHunter_Init(e);
            break;
        case 1:
            hn_state_run();
            break;
        case 2:
            hn_hit();
            break;
        case 3:
            hn_die();
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
    HN->roomHit |= check_room_collision();
    E->move_dist = (u16)sTempVar;
}
