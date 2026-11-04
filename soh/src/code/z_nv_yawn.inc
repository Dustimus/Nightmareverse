/*
 * Nightmareverse: Yawn, the snake (enemy types 13, the first fight in 210, and 18, the library
 * rematch in 70C). Included at the end of z_nv_zombie.c with the other ported enemies.
 *
 * As RE1 runs it (the decomp's entities/Yawn.cpp is the reference): it comes in along a fixed path
 * (or starts in the room, already reared up), then each tick picks what to do from how far the
 * player is and whether it can see her: slither after her, bite when close and lined up (10 of
 * RE1's health, and the first Yawn's bite poisons until the serum), rear up after five bites or
 * when she's nearly dead, and, once hurt enough (health under 2800 of its 3050), flee along its
 * escape path out of the room. Hits stagger it; the rematch Yawn can die.
 *
 * Left out, each a note where it would be: the swallow (the grown Yawn's kill of a nearly dead
 * player), its own chained skeleton (RE1 drags each body segment after the head; here the
 * animation plays on the whole body as for the other enemies, and the body doesn't trail through
 * turns), the dust and blood, the death shrink.
 */

#define YW (&E->yw)

static const s16 sYwSca[8] = { (s16)0x8000, 0, (s16)0xF830, 0, 0x07D0, 0x0320, 0, 0 };
static const u8 sYwBiteWindow[2][2] = { { 13, 6 }, { 18, 5 } }; // first frame, frames: juvenile, grown
// 210's paths (RE1 units): the way in, the way out, and where it goes when stuck against a wall
static const s16 sYwEntry[6][2] = { { 4500, 24500 }, { 11000, 24500 }, { 10500, 14000 },
                                    { 8500, 11500 }, { 6800, 14200 },  { 3800, 8400 } };
static const s16 sYwFlee[10][2] = { { 7300, 10000 },  { 12000, 12000 }, { 11000, 15500 }, { 11000, 25000 },
                                    { 11000, 25000 }, { 6500, 25000 },  { 2500, 26000 },  { 2500, 31000 },
                                    { 15000, 31000 }, { 0, 0 } };
static const s16 sYwReposition[4][2] = { { 6500, 7500 }, { 11500, 19500 }, { 7500, 24500 }, { 11500, 24500 } };

static void yw_anim(u8 id, u8 blend) {
    E->animationId = id;
    E->animation_frame_id = 0;
    E->timing_control = 0;
    E->blend_counter = blend;
}

static void yw_act(u8 behaviour) {
    E->action_behavior = behaviour;
    E->action_state = 0;
}

static void yw_decide_again(void) {
    E->ignore_player_flag = 0;
    yw_act(1);
}

/** The swim: a sideways sway that builds and flips (and hisses as it does). */
static void yw_sway(void) {
    if ((u16)(YW->sway + 900) > 0x708) {
        YW->sway -= YW->turnAcc;
        YW->turnAcc = -(YW->turnAcc / 0x40);
        Snd_em(0);
    }
    if ((YW->turnAcc % 0x40) != 0) {
        YW->turnAcc *= 2;
    }
    YW->sway += YW->turnAcc;
}

/** After moving: the room's walls, counting the ticks it's stuck against them. */
static void yw_after_move(void) {
    if (check_room_collision()) {
        if (YW->stuck < 0xFF) {
            YW->stuck++;
        }
    } else if (YW->stuck > 0) {
        YW->stuck--;
    }
}

/** Toward a waypoint; true once within reach. */
static s32 yw_head_for(const s16* wp, s32 reach, u16 turn) {
    s32 dx = wp[0] - E->t[0];
    s32 dz = wp[1] - E->t[2];

    entity_rotate_toward_target(wp[0], wp[1], turn);
    return SquareRoot0(dx * dx + dz * dz) < reach;
}

/* ---- actions ---- */

static void yw_idle(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        yw_anim(1, 7);
        YW->sway = 0;
        E->action_ticks_counter = (u16)((CbRand() & 0xF) + 0xF);
    }
    Joint_move(0, 0x200);
    if ((s16)E->action_ticks_counter-- == 0) {
        yw_act(1);
    }
    yw_after_move();
}

static void yw_slither(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        yw_anim(YW->form + 1, 7);
        YW->sway = 0;
        E->move_speed_current = 0xA0;
        if (((YW->nearlyDead != 0) || (YW->recover & 0x80)) && (YW->form != 0)) {
            E->animationId = 5; // reared up
        }
    }
    if (YW->recover & 0x7F) {
        if (--YW->recover == 0) {
            E->action_state = 0;
        }
    }
    if ((E->animationId == 5) && ((E->animation_frame_id == 0x0F) || (E->animation_frame_id == 0x28))) {
        Snd_em(1);
    }
    Joint_move(0, 0x200);
    Add_speedXZ(YW->sway);
    yw_after_move();
}

static void yw_bite(void) {
    switch (E->action_state) {
        case 0:
            E->action_state = 1;
            yw_anim(YW->form * 10 + 3, 7);
            E->move_speed_current = 100;
            YW->bites++;
            YW->turnDir = 1;
            Snd_em(2);
            // fallthrough
        case 1: {
            const u8* win = sYwBiteWindow[YW->form & 1];

            if (((u8)(E->animation_frame_id - win[0]) < win[1]) && (P->isBeingAttackedFlag == 0) && !gNvRe1DogFlinch) {
                s32 x;
                s32 z;

                RotY(E->angle, 1000, 0, &x, &z); // the jaws: 1000 ahead of its head
                if ((CbAbs(P->t[0] - (E->t[0] + x)) <= 800) && (CbAbs(P->t[2] - (E->t[2] + z)) <= 800)) {
                    void NvFlags_Op(s32 bank, s32 bit, s32 mode);
                    s32 NvFlags_Test(s32 bank, s32 bit);

                    gNvRe1DogBite = 10;
                    E->angle += 0x100;
                    if ((E->id == 0x0D) && !NvFlags_Test(0, 0x47)) { // the first Yawn poisons
                        NvFlags_Op(1, 0x43, 0);
                    }
                    YW->turnDir = -1;
                    YW->recover = 0;
                    YW->stuck = 0;
                    YW->hiss = 0xF0;
                    Snd_em(4);
                }
            }
            E->action_state += (u8)Joint_move(0, 0x200);
            entity_rotate_toward_target(P->t[0], P->t[2], 0x38);
            if ((s32)YW->form * 5 - (s32)E->animation_frame_id == -8) {
                E->move_speed_current = 200; // the lunge
                Snd_em(3);
            }
            break;
        }
        case 2:
            E->action_ticks_counter = 0x1E;
            E->move_speed_current = 0x78;
            E->action_state = YW->form + 3;
            break;
        case 3: // the juvenile pulls back
            entity_rotate_toward_target(P->t[0], P->t[2], (u16)((s16)YW->turnDir << 4));
            if ((s16)E->action_ticks_counter-- == 0) {
                yw_decide_again();
                YW->turnDir = 0;
            }
            break;
        default: // the grown one rears back up
            if (E->action_state == 4) {
                E->action_state = 5;
                yw_anim(4, 7);
                E->move_speed_current = 0x78;
            }
            entity_rotate_toward_target(P->t[0], P->t[2], (u16)((s16)YW->turnDir << 4));
            if (Joint_move(0, 0x200)) {
                yw_decide_again();
                YW->turnDir = 0;
            }
            break;
    }
    Add_speedXZ(0);
    yw_after_move();
}

/** Rear up (and, juvenile, grow into the reared form for a while). */
static void yw_rear(void) {
    switch (E->action_state) {
        case 0:
            E->action_state = 1;
            yw_anim(4, 7);
            E->move_speed_current = 0x78;
            YW->bites = 0;
            if (YW->nearlyDead && YW->form) {
                E->action_state = 3;
                break;
            }
            // fallthrough
        case 1:
            entity_rotate_toward_target(P->t[0], P->t[2], 0x30);
            E->action_state += (u8)Joint_move(YW->form, 0x200);
            break;
        case 2:
            if (YW->form == 0) {
                YW->form = 1;
                E->action_state = 3;
            } else {
                YW->form = 0;
                yw_decide_again();
            }
            break;
        case 3:
            E->action_state = 4;
            E->timing_control = 0;
            E->animationId = 5;
            E->blend_counter = 7;
            E->move_speed_current = 0x46;
            E->action_ticks_counter = 0x3C;
            // fallthrough
        default:
            entity_rotate_toward_target(P->t[0], P->t[2], 0x30);
            Joint_move(YW->form, 0x200);
            if ((E->animation_frame_id == 0x0F) || (E->animation_frame_id == 0x28)) {
                Snd_em(1);
            }
            if ((s16)E->action_ticks_counter-- == 0) {
                yw_decide_again();
            }
            break;
    }
    yw_sway();
    Add_speedXZ(YW->sway);
    yw_after_move();
}

/** The way in along its path; then it rears up out of the ground. */
static void yw_entry(void) {
    if (E->action_state == 0) {
        void NvFlags_Op(s32 bank, s32 bit, s32 mode);

        E->action_state = 1;
        yw_anim(1, 7);
        YW->sway = 0;
        E->move_speed_current = 0xBE;
        if (E->behavior_flags == 4) {
            E->move_speed_current = 0x82;
            YW->waypoint = 3;
        }
        E->action_ticks_counter = 2;
        if (E->id == 0x0D) {
            NvFlags_Op(0, 0x10, 0); // SCENARIO_FLAG_YAWN_BITE
        }
    }
    if (yw_head_for(sYwEntry[MIN(YW->waypoint, 5)], 500, 0x40)) {
        if ((s16)E->action_ticks_counter-- == 0) {
            yw_act(6);
        }
        if (YW->waypoint < 5) {
            YW->waypoint++;
        }
        Snd_em(0);
    }
    Joint_move(0, 0x200);
    Add_speedXZ(YW->sway);
    yw_after_move();
}

static void yw_emerge(void) {
    switch (E->action_state) {
        case 0:
            E->action_state = 1;
            yw_anim(4, 7);
            E->move_speed_current = 0;
            YW->form = 1;
            YW->waypoint = 0;
            // fallthrough
        case 1:
            if (Joint_move(0, 0x200)) {
                E->action_state = (E->behavior_flags == 0) ? 6 : 2;
            }
            break;
        case 2:
            E->action_state = 3;
            E->timing_control = 0;
            E->blend_counter = 7;
            E->animationId = YW->form * 10 + 3;
            // fallthrough
        case 3:
            E->action_state += (u8)Joint_move(0, 0x200);
            if ((s32)YW->form * 5 - (s32)E->animation_frame_id == -8) {
                Snd_em(3);
                E->move_speed_current = 0xBE;
            }
            if (E->animation_frame_id > 0x17) {
                E->move_speed_current = 0x32;
            }
            Add_speedXZ(0);
            yw_after_move();
            break;
        case 4:
            E->action_state = 5;
            E->timing_control = 0;
            E->animationId = 4;
            E->blend_counter = 7;
            // fallthrough
        case 5:
            E->action_state += (u8)Joint_move(0, 0x200);
            entity_rotate_toward_target(P->t[0], P->t[2], 0x30);
            break;
        case 6:
            E->action_state = 7;
            E->timing_control = 0;
            E->animationId = 5;
            E->blend_counter = 7;
            E->move_speed_current = 0x46;
            E->action_ticks_counter = 0x5A;
            // fallthrough
        default:
            Joint_move(YW->form, 0x200);
            if ((E->animation_frame_id == 0x0F) || (E->animation_frame_id == 0x28)) {
                Snd_em(1);
            }
            if ((s16)E->action_ticks_counter-- == 0) {
                yw_decide_again();
                E->status_flags &= ~4; // it's in the room now
            }
            break;
    }
}

/** Hurt enough: out along its escape path, gone at the end (its dead flag marks it fled). */
static void yw_flee(void) {
    if (E->action_state == 0) {
        void NvFlags_Op(s32 bank, s32 bit, s32 mode);

        E->action_state = 1;
        yw_anim(1, 0x0F);
        E->move_speed_current = 0xB4;
        YW->stuck = 0;
        YW->waypoint = 4;
        if ((E->t[0] < 9000) && (E->t[2] < 23000)) {
            YW->waypoint = 3;
        }
        if (E->t[2] < 0x3C8C) {
            YW->waypoint = 2;
        }
        if (E->t[2] < 9000) {
            YW->waypoint = 0;
        }
        if (E->t[0] > 0x2580) {
            YW->waypoint = 1;
        }
        YW->form = 0;
        YW->turnDir = 1;
        YW->turnAcc = 1;
        YW->sway = 0;
        if (E->death_event_id != 0xFF) {
            NvFlags_Op(3, E->death_event_id, 0);
        }
    }
    if (YW->waypoint <= 8) {
        if (yw_head_for(sYwFlee[YW->waypoint], 800, 0x60)) {
            if (++YW->waypoint == 1) {
                YW->waypoint = 2;
            }
            YW->sway = 0;
            if (YW->waypoint >= 6) { // into the gap it leaves by: past the walls from here
                E->status_flags |= 4;
            }
            if (YW->waypoint == 9) { // through the far end: gone
                E->behavior_flags |= 0x80;
                E->status_flags |= 2;
            }
        }
    }
    if ((YW->stuck > 0x96) && (YW->waypoint < 6)) {
        E->ignore_player_flag = 1;
        yw_act(8);
    }
    Joint_move(0, 0x100);
    if (YW->waypoint < 4) {
        yw_sway();
    }
    Add_speedXZ(YW->sway);
    yw_after_move();
}

static void yw_reposition(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        yw_anim(1, 0x0F);
        E->move_speed_current = 0xB4;
        YW->stuck = 0;
        YW->waypoint = 3;
        if (E->t[0] > 9000) {
            YW->waypoint = 2;
        }
        if (E->t[2] < 19000) {
            YW->waypoint = 1;
        }
        if (E->t[2] < 15000) {
            YW->waypoint = 0;
        }
        YW->form = 0;
        YW->turnDir = 1;
        YW->turnAcc = 1;
        YW->sway = 0;
    }
    if (YW->stuck > 0x96) {
        E->action_state = 0;
    }
    if (yw_head_for(sYwReposition[YW->waypoint & 3], 800, 0x60)) {
        YW->sway = 0;
        E->state = 1;
        E->ignore_player_flag = 0;
        yw_act(0);
    }
    Joint_move(0, 0x100);
    yw_sway();
    Add_speedXZ(YW->sway);
    yw_after_move();
}

/* ---- deciding ---- */

static void yw_decide(void) {
    s32 d = CbAbs(P->t[0] - E->t[0]) + CbAbs(P->t[2] - E->t[2]);
    s32 scripted = (E->behavior_flags & 7) >= 4;
    s32 seen;
    s32 hurtPlayer = 0; // (RE1's player under 10 health: Link's health isn't RE1's, so never)

    entity_rotate_toward_target(P->t[0], P->t[2], 0x30);
    if ((P->isBeingAttackedFlag == 0) && !gNvRe1DogFlinch) {
        seen = !check_line_of_sight(P->t[0], P->t[2]);
        if (seen && (d < 2000)) {
            E->ignore_player_flag = 1;
            yw_act(2);
        }
        if (seen && (d < (s32)((YW->form * 5 + 0x19) * 200)) && (turn_toward_target(P->t[0], P->t[2], 0x200) == 0)) {
            E->ignore_player_flag = 1;
            yw_act(2);
        }
        if ((d < 7000) && (YW->bites > 4)) {
            E->ignore_player_flag = 1;
            yw_act(3);
        }
        if (hurtPlayer && (YW->form == 0)) {
            YW->nearlyDead = 1;
            E->ignore_player_flag = 1;
            yw_act(3);
        }
        if (!scripted) {
            if ((E->t[2] > 17000) && YW->form && (E->action_behavior == 2)) {
                E->ignore_player_flag = 1;
                yw_act(3);
                YW->nearlyDead = 0;
            }
            if (YW->stuck > 0x96) {
                E->ignore_player_flag = 1;
                yw_act(8);
            }
            if (E->health < 2800) {
                E->ignore_player_flag = 1;
                yw_act(7);
            }
        }
    }
    yw_sway();
}

static void yw_run_action(void) {
    switch (E->action_behavior) {
        case 0:
            yw_idle();
            break;
        case 1:
            yw_slither();
            break;
        case 2:
            yw_bite();
            break;
        case 3:
            yw_rear();
            break;
        case 5:
            yw_entry();
            break;
        case 6:
            yw_emerge();
            break;
        case 7:
            yw_flee();
            break;
        case 8:
            yw_reposition();
            break;
        default: // 4, the swallow: see the top
            yw_decide_again();
            break;
    }
}

static void yw_damaged(void) {
    if (E->ignore_player_flag == 0 || E->action_state == 0) {
        if (E->action_state == 0) {
            E->ignore_player_flag = 1;
            E->action_state = 1;
            yw_anim(YW->form + 9, 7);
            YW->sway = 0;
            E->move_speed_current = 0x46;
            Snd_em(2);
        }
    }
    switch (E->action_state) {
        case 1:
            E->action_state += (u8)Joint_move(0, 0x200);
            break;
        case 2:
            E->action_state = 3;
            yw_anim(5, 7);
            E->move_speed_current = 0x78;
            E->action_ticks_counter = 0x3C;
            // fallthrough
        default: {
            s32 dir = ((s16)E->action_ticks_counter > 0x1E) ? -1 : 1;

            Joint_move(0, 0x200);
            if ((E->animation_frame_id == 0x0F) || (E->animation_frame_id == 0x28)) {
                Snd_em(1);
            }
            entity_rotate_toward_target(P->t[0], P->t[2], (u16)(dir * 0x30));
            if ((s16)E->action_ticks_counter-- == 0) {
                E->state = 1;
                E->hit_state = 0;
                yw_decide_again();
                YW->form = 1;
                YW->recover = 0xDA;
                Snd_em(1);
            }
            yw_sway();
            break;
        }
    }
    Add_speedXZ(YW->sway);
    yw_after_move();
}

static void yw_die(void) {
    switch (E->action_state) {
        case 0:
            if (E->death_event_id != 0xFF) {
                void NvFlags_Op(s32 bank, s32 bit, s32 mode);

                NvFlags_Op(3, E->death_event_id, 0);
            }
            E->ignore_player_flag = 1;
            E->action_state = 1;
            yw_anim(6, 7);
            YW->sway = 0;
            E->move_speed_current = 0;
            Snd_em(2);
            // fallthrough
        case 1:
        case 3:
        case 5:
            if (E->action_state == 3) {
                entity_rotate_toward_target(P->t[0], P->t[2], 0x30);
            }
            E->action_state += (u8)Joint_move(0, 0x200);
            Add_speedXZ(YW->sway);
            yw_after_move();
            break;
        case 2:
            E->action_state = 3;
            E->timing_control = 0;
            E->animationId = 7;
            E->blend_counter = 7;
            E->move_speed_current = 0x46;
            Snd_em(1);
            break;
        case 4:
            E->action_state = 5;
            E->timing_control = 0;
            E->animationId = 0x0F;
            E->blend_counter = 7;
            E->move_speed_current = 100;
            Snd_em(2);
            break;
        default: // still (RE1 shrinks it away into the floor)
            E->move_speed_current = 0;
            E->status_flags |= 10;
            break;
    }
}

/* ---- entry points ---- */

void NvYawn_Init(NvRe1Entity* e) {
    u8 kind;

    E = e;
    E->state = 1;
    E->ignore_player_flag = 0;
    yw_act(1);
    memset(&E->yw, 0, sizeof(E->yw));
    E->health = 0x0BEA;
    if (E->id == 0x12) {
        E->health = 0x012C; // the rematch's: it can die
    }
    E->animationId = (u8)((E->behavior_flags & 2) * 5 + 1);
    if ((E->behavior_flags & 0xF) == 2) {
        E->animationId = 2;
    }
    E->animation_frame_id = 0;
    E->timing_control = 0;
    E->blend_counter = 0;
    E->sca = sYwSca;
    E->t[1] = 0;
    YW->turnAcc = 0x40;
    Joint_move(0, 0x40);
    kind = E->behavior_flags & 0xF;
    if ((kind == 0) || (kind == 4)) { // comes in along its path
        E->status_flags |= 4;
        E->ignore_player_flag = 1;
        yw_act(5);
    } else { // already here, reared up
        YW->form = 1;
        if (E->id == 0x0D) {
            void NvFlags_Op(s32 bank, s32 bit, s32 mode);

            NvFlags_Op(0, 0x10, 0);
        }
    }
    if (E->behavior_flags & 0x80) { // the script's: waits (RE1's state 4)
        E->state = 4;
    }
}

/**
 * The body trails the head (RE1's chain: yawn_chain_follow drags each segment after the one ahead
 * of it). Each body joint (3-14, back from the head) points where the head pointed a few ticks
 * earlier: its turn against the joint ahead is the difference of those two old headings.
 */
#define YW_TRAIL_TICKS 4
static void yw_trail(void) {
    s32 k;

    YW->heading[YW->headingAt++ & 63] = E->angle;
    for (k = 0; k < NV_RE1_JOINTS; k++) {
        E->yawExtra[k] = 0;
    }
    if (E->status_flags & 8) { // (dead: the body lies as it fell)
        return;
    }
    for (k = 3; k <= 14; k++) {
        s32 newer = (k - 3) * YW_TRAIL_TICKS + 1;
        s32 older = newer + YW_TRAIL_TICKS;
        s16 hNewer = YW->heading[(u8)(YW->headingAt - newer) & 63];
        s16 hOlder = YW->heading[(u8)(YW->headingAt - older) & 63];
        s16 d = (s16)(((hOlder - hNewer + 0x800) & 0xFFF) - 0x800); // the short way round

        E->yawExtra[k] = (s16)CLAMP(d, -0x200, 0x200);
    }
}

void NvYawn_Update(NvRe1Entity* e) {
    E = e;
    if (YW->headingAt == 0) { // first tick: it has always faced this way
        s32 k;

        for (k = 0; k < 64; k++) {
            YW->heading[k] = E->angle;
        }
        YW->headingAt = 1;
    }
    E->pos[0] = (s16)E->t[0];
    E->pos[1] = (s16)E->t[1];
    E->pos[2] = (s16)E->t[2];
    // a hit while biting or just after one doesn't stop it (RE1 puts the state back)
    if ((E->state == 2) && ((YW->turnDir != 0) || (YW->recover != 0))) {
        E->state = YW->saved[0];
        E->ignore_player_flag = YW->saved[1];
        E->action_behavior = YW->saved[2];
        E->action_state = YW->saved[3];
        E->hit_state = 0;
    } else if ((E->state == 2) && (E->ignore_player_flag == 0) && (E->hit_state != 0)) {
        E->action_state = 0; // NvZombie_Hit's: a fresh stagger
    }
    if ((E->state == 3) && (E->id == 0x0D)) { // the first Yawn doesn't die: it flees
        E->state = 1;
        E->health = 1;
        E->ignore_player_flag = 1;
        yw_act(7);
    }
    switch (E->state) {
        case 0:
            NvYawn_Init(e);
            break;
        case 1:
            if (E->behavior_flags & 0x80) {
                break;
            }
            E->status_flags = (E->status_flags & 0x1F) | 0x40;
            if (YW->form == 0) {
                E->status_flags &= 0x7F;
                entity_check_visual_range(4000);
            }
            if (E->ignore_player_flag == 0) {
                yw_decide();
            }
            yw_run_action();
            YW->saved[0] = E->state;
            YW->saved[1] = E->ignore_player_flag;
            YW->saved[2] = E->action_behavior;
            YW->saved[3] = E->action_state;
            if (YW->hiss != 0) {
                YW->hiss--;
            }
            break;
        case 2: // (hit_state stays set through the stagger: no new hits land until it ends)
            yw_damaged();
            break;
        case 3:
            yw_die();
            break;
        case 4: // held by the script until it clears the hold
            if (!(E->behavior_flags & 0x80)) {
                E->state = 1;
            }
            break;
        default:
            break;
    }
    yw_trail();
    SetScaWorld(E->sca, E->angle, E->scaWorld);
    SetScaWorld(P->sca, P->angle, P->scaWorld);
    if (!(E->status_flags & 2)) {
        ResolveScaPush(P->sca, P->scaWorld, P->t, E);
    }
}
