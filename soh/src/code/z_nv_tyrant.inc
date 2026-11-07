/*
 * Nightmareverse: RE1's Tyrant (enemy types 12, the lab's, and 16, the heliport's: one code path).
 * Included at the end of z_nv_zombie.c with the other ported enemies.
 *
 * As RE1 runs it (the decomp's entities/Tyrant.cpp is the reference): it walks at her; within
 * 2700 and roughly facing her it slashes (12 of RE1's health; the heliport one's knocks her
 * back), within 2000 and lined up it swipes (10), or, she being under 16 and turned away, it
 * impales her (the kill). The heliport one also charges from beyond 12000 (20), thrusts at her
 * when she's off to its side within 7000 (16), and rushes after a slash that missed. The claw is
 * its joint 8: a strike lands when the claw comes within its reach of her (800 the swipe and
 * the backhand, 1500 the slash and the thrust, 1800 the charge's).
 *
 * The lab one starts held by 513's script (it breaks out of its pod; state 8, the script's), the
 * heliport one with its eruption through the floor. Shot, it staggers; at 0 it falls (RE1 leaves
 * its deaths to the rooms' scripts and the rocket launcher: here it falls and its flag goes up).
 *
 * Left out, each a note where it would be: its heart and claw trails (the drawing), the 24
 * script-driven behaviours (the pod, the rocket death, the cutscene strikes: the scripts drive
 * them; until those scenes are built a held Tyrant just stands), the power-up, the second run's
 * damage.
 */

#define TY (&E->ty)

static const s16 sTySca[8] = { (s16)0x8000, 0, -1800, 0, 1800, 700, 0, 0 };

enum { TY_PAUSE, TY_WALK, TY_SWIPE, TY_SLASH, TY_THRUST, TY_BACKHAND, TY_IMPALE, TY_CHARGE, TY_ERUPT, TY_RUSH };

static s32 ty_roof(void) {
    return E->id == 0x10;
}

static void ty_set(u8 b) {
    E->ignore_player_flag = 1;
    E->action_behavior = b;
    E->action_state = 0;
}

static void ty_free(void) {
    E->state = 1;
    E->ignore_player_flag = 0;
    E->action_behavior = TY_WALK;
    E->action_state = 0;
}

static void ty_anim(u8 id, u8 blend) {
    E->animationId = id;
    E->animation_frame_id = 0;
    E->timing_control = 0;
    E->blend_counter = blend;
}

/** Its claw (joint 8) within `reach` of her. */
static s32 ty_claw_reaches(s32 reach) {
    if (E->numJoints <= 8) {
        return TY->dist < reach;
    }
    return (CbAbs(P->t[0] - E->joint[8][0]) + CbAbs(P->t[2] - E->joint[8][2])) < reach;
}

static s32 ty_free_to_hit(void) {
    return (P->isBeingAttackedFlag == 0) && !gNvRe1DogFlinch && (P->health >= 0);
}

/** A strike's window: on `from`..`to` its claw at her lands `damage`, once. */
static void ty_strike(u8 from, u8 to, s32 reach, s16 damage) {
    if ((E->animation_frame_id >= from) && (E->animation_frame_id <= to) && !TY->landed && ty_free_to_hit() &&
        ty_claw_reaches(reach)) {
        TY->landed = 1;
        gNvRe1DogBite = damage;
        Snd_em(2);
    }
}

static void ty_think(void) {
    if (TY->cooldown != 0) {
        TY->cooldown--;
        return;
    }
    if (ty_roof() && (TY->dist > 12000) && (turn_toward_target(P->t[0], P->t[2], 0x40) == 0)) {
        ty_set(TY_CHARGE);
    }
    if ((TY->dist < 2700) && (turn_toward_target(P->t[0], P->t[2], 0x340) == 0)) {
        ty_set(TY_SLASH);
    }
    if ((TY->dist < 2000) && (turn_toward_target(P->t[0], P->t[2], 0x200) == 0)) {
        ty_set(((P->health < 0x10) && !is_facing_toward_entity()) ? TY_IMPALE : TY_SWIPE);
    }
    if (ty_roof() && (TY->dist < 7000) && (turn_toward_target(P->t[0], P->t[2], 0x4C8) != 0)) {
        ty_set(TY_THRUST);
    }
}

/* ---- the moves ---- */

static void ty_pause(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        ty_anim(0, 0x1F);
        TY->ticks = 0x3C;
    }
    Joint_move(0, 0x80);
    if (TY->ticks-- == 0) {
        ty_free();
    }
}

static void ty_walk(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        ty_anim(1, 7);
    }
    entity_update_wander_turn(E->move_dist, &TY->wallHit, &E->turn_counter, 0x28, 0x28);
    Joint_move(0, 0x200);
    if ((E->animation_frame_id == 6) || (E->animation_frame_id == 0x1F)) {
        Snd_em(0); // its steps
    }
    E->move_speed_current = 60;
    Add_speedXZ(0);
}

/** One of its strikes: an animation, a speed that dies away, the claw's window. */
static void ty_attack(u8 anim, s16 speed, u8 from, u8 to, s32 reach, s16 damage, u8 end, s16 turn) {
    if (E->action_state == 0) {
        E->action_state = 1;
        ty_anim(anim, 7);
        E->move_speed_current = speed;
        TY->landed = 0;
        Snd_em(1);
        TY->cooldown = 10;
    } else if (E->action_state != 1) {
        ty_free();
        return;
    }
    E->action_state += (u8)Joint_move(0, 0x200);
    if (turn) {
        entity_rotate_toward_target(P->t[0], P->t[2], turn);
    }
    ty_strike(from, to, reach, damage);
    if (end && (E->animation_frame_id >= end)) {
        E->action_state = 2;
    }
    E->move_speed_current = MAX(0, E->move_speed_current - 5);
    Add_speedXZ(0);
}

/** The impale: through her, and up (the kill). */
static void ty_impale(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        ty_anim(5, 7);
        E->move_speed_current = 200;
        TY->landed = 0;
        Snd_em(1);
    } else if (E->action_state != 1) {
        E->holding = 0;
        ty_free();
        return;
    }
    E->action_state += (u8)Joint_move(0, 0x200);
    if (!TY->landed && (E->animation_frame_id >= 5) && (E->animation_frame_id <= 8) && ty_claw_reaches(800) &&
        (P->health >= 0)) {
        TY->landed = 1;
        E->unk_c6 = (s16)E->joint[8][0];
        E->unk_c8 = (s16)E->joint[8][2];
        E->holding = 1;
        Snd_em(4);
    }
    if (TY->landed && (E->animation_frame_id == 0x5C)) {
        gNvRe1Kill = 1;
    }
    E->move_speed_current = MAX(0, E->move_speed_current - 5);
    Add_speedXZ(0);
}

/** The heliport one's charge: a run at her, then the swing as it arrives (20). */
static void ty_charge(void) {
    switch (E->action_state) {
        case 0:
            E->action_state = 1;
            ty_anim(2, 7);
            E->move_speed_current = 400;
            TY->landed = 0;
            // fall through
        case 1:
            Joint_move(0, 0x200);
            entity_rotate_toward_target(P->t[0], P->t[2], 0x10);
            if ((E->animation_frame_id == 3) || (E->animation_frame_id == 0xF)) {
                Snd_em(0);
            }
            if (TY->dist < 3000) {
                E->action_state = 2;
                ty_anim(9, 7);
                E->move_speed_current = 300;
            }
            Add_speedXZ(0);
            return;
        case 2:
            E->action_state += (u8)Joint_move(0, 0x200);
            ty_strike(8, 14, 1800, 20);
            E->move_speed_current = MAX(0, E->move_speed_current - 10);
            Add_speedXZ(0);
            return;
        default:
            ty_free();
            return;
    }
}

/** The heliport one's rush: a run at her a while (after a slash or a thrust that missed). */
static void ty_rush(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        ty_anim(2, 7);
        E->move_speed_current = 400;
        TY->ticks = 0x3C;
    }
    Joint_move(0, 0x200);
    entity_rotate_toward_target(P->t[0], P->t[2], 0x20);
    Add_speedXZ(0);
    if ((TY->ticks-- == 0) || (TY->dist < 2700)) {
        ty_free();
    }
}

/** The heliport one's entrance: up through the floor. */
static void ty_erupt(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        ty_anim(6, 0);
        E->hit_state = 1;
    }
    if (Joint_move(0, 0x200)) {
        E->hit_state = 0;
        ty_free();
    }
}

static void ty_act(void) {
    switch (E->action_behavior) {
        case TY_PAUSE:
            ty_pause();
            break;
        case TY_WALK:
            ty_walk();
            break;
        case TY_SWIPE:
            ty_attack(5, 200, 5, 8, 800, 10, 0x32, 0x10);
            break;
        case TY_SLASH: {
            u8 was = TY->landed;

            ty_attack(4, 0, 7, 11, 1500, 12, 0, 8);
            if (!was && TY->landed && ty_roof()) {
                gNvRe1DogBite = 12; // (the knock back comes with it)
            }
            if ((E->action_state == 2) && ty_roof() && !TY->landed && (TY->dist > 5000) && (CbRand() & 1)) {
                ty_set(TY_RUSH);
            }
            break;
        }
        case TY_THRUST:
            ty_attack(7, 0, 10, 16, 1500, 16, 0, 0x10);
            if ((E->action_state == 2) && !TY->landed && (TY->dist > 5000) && (CbRand() & 1)) {
                ty_set(TY_RUSH);
            }
            break;
        case TY_BACKHAND:
            ty_attack(3, 300, 4, 10, 800, 12, 0, 0);
            break;
        case TY_IMPALE:
            ty_impale();
            break;
        case TY_CHARGE:
            ty_charge();
            break;
        case TY_ERUPT:
            ty_erupt();
            break;
        default:
            ty_rush();
            break;
    }
}

static void ty_run(void) {
    if (E->behavior_flags & 0x80) {
        return; // held by its script
    }
    TY->dist = CbAbs(P->t[0] - E->t[0]) + CbAbs(P->t[2] - E->t[2]);
    entity_check_visual_range(4000);
    entity_check_alert_range(4000);
    if (P->health < 0) {
        if (E->action_behavior != TY_PAUSE) {
            ty_set(TY_PAUSE);
        }
    } else if (E->ignore_player_flag == 0) {
        ty_think();
    }
    entity_pathfind_update();
    zone_path_find(P->t[0], P->t[2], &E->player_pos_x, &E->player_pos_z);
    ty_act();
}

/** Shot: a stagger (its hits break off), then on at her. */
static void ty_stagger(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        ty_anim((u8)((turn_toward_target(P->t[0], P->t[2], 0x400) == 0) ? 0xA : 0xB), 7);
        E->holding = 0;
        E->move_speed_current = 0;
    }
    if (Joint_move(0, 0x200)) {
        E->hit_state = 0;
        ty_free();
    }
}

/** Down: it falls and its flag goes up (the rooms' scripts finish it in RE1). */
static void ty_die(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        ty_anim(0xC, 7);
        E->holding = 0;
        E->status_flags |= 2;
        TY->ticks = 0x5A;
        if (E->death_event_id != 0xFF) {
            void NvFlags_Op(s32 bank, s32 bit, s32 mode);

            NvFlags_Op(3, E->death_event_id, 0);
        }
    }
    Joint_move(0, 0x200);
    if (TY->ticks-- == 0) {
        E->status_flags |= 10;
    }
}

/* ---- state 8: the script's behaviours (s_emScdBehaviors) ---- */

static void ty_scd_flag(u8 bit) {
    void NvFlags_Op(s32 bank, s32 bit, s32 mode);

    NvFlags_Op(4, bit, 0);
}

/** 0: hold the idle pose. */
static void ty_scd_idle(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        ty_anim(0, 0x1F);
    }
    Joint_move(0, 0x80);
}

/** 2: walk to the script's point (read unsigned), its flag within 1500. */
static void ty_scd_walk_to(void) {
    s32 x = (u16)E->unk_c6;
    s32 z = (u16)E->unk_c8;

    if (E->action_state == 0) {
        E->action_state = 1;
        ty_anim(1, 0x1F);
        TY->ticks = (E->behavior_flags & 1) ? 1 : 2;
    } else if (E->action_state == 2) {
        ty_scd_flag(E->scd_anim_param);
        if (!(E->collisionFlags & 0x80)) {
            E->action_behavior = 0;
            E->action_state = 0;
        } else {
            E->action_state = 1; // (the script loops the walk)
        }
        return;
    }
    entity_rotate_toward_target(x, z, 0x28);
    if (--TY->ticks <= 0) {
        Joint_move(0, 0x80);
        TY->ticks = (E->behavior_flags & 1) ? 1 : 2;
    }
    {
        s32 dz = E->t[2] - z;
        s32 dx = E->t[0] - x;

        if (SquareRoot0(dz * dz + dx * dx) < 0x5DC) {
            E->action_state = 2;
        }
    }
    if ((E->animation_frame_id == 3) || (E->animation_frame_id == 0x24)) {
        Snd_em(0);
    }
    E->move_speed_current = 60; // (RE1's: the walk's root motion; the free walk's pace here)
    Add_speedXZ(0);
}

/** 10: it pounds the capsule's glass and breaks out (the shards left out). */
static void ty_scd_break_glass(void) {
    void NvSnd_At(s32 slot, s32 x, s32 z, s32 lx, s32 lz);

    if (E->action_state == 0) {
        E->t[1] = 0;
        E->action_state = 1;
        ty_anim(6, 0x1F);
    } else if (E->action_state == 2) {
        ty_scd_flag(E->scd_anim_param);
        E->action_behavior = 0;
        E->action_state = 0;
        return;
    } else if (E->action_state != 1) {
        return;
    }
    E->action_state += (u8)Joint_move(0, 0x80);
    if (E->animation_frame_id == 0x1E) {
        NvSnd_At(0x19, 0x2A30, 5000, P->t[0], P->t[2]); // the first blow on the glass
    }
    if (E->animation_frame_id == 0x59) { // the glass bursts out
        E->fx = 3;
        E->fxAt[0] = 0x2A30;
        E->fxAt[1] = -0x1004;
        E->fxAt[2] = 5000;
    }
    if (E->animation_frame_id == 0x55) { // it shatters
        NvSnd_At(0x1A, 0x2A30, 5000, P->t[0], P->t[2]);
        NvSnd_At(0x1B, 0x2A30, 5000, P->t[0], P->t[2]);
    }
    if ((E->animation_frame_id == 0x8F) || (E->animation_frame_id == 0x97)) {
        Snd_em(0); // the two steps out
    }
}

/** 11: it floats in the stasis pod, bobbing, until the pod opens (4:0x1F). */
static void ty_scd_pod(void) {
    void NvFlags_Op(s32 bank, s32 bit, s32 mode);
    s32 NvFlags_Test(s32 bank, s32 bit);

    switch (E->action_state) {
        case 0:
            E->t[1] = -200;
            E->action_state = 1;
            ty_anim(6, 0);
            TY->ticks = 5;
            Joint_move(0, 0x80);
            // fall through
        case 1:
            if (--TY->ticks == 0) {
                TY->ticks = 5;
                E->t[1] -= 5;
                if (E->t[1] == -0x15E) {
                    E->action_state = 2;
                }
            }
            break;
        case 2:
            if (--TY->ticks == 0) {
                TY->ticks = 5;
                E->t[1] += 5;
                if (E->t[1] == -200) {
                    E->action_state = 1;
                }
            }
            break;
        case 3:
            E->t[1] += 4;
            if (E->t[1] > 0) {
                ty_scd_flag(E->scd_anim_param);
                E->action_state = 4;
                E->t[1] = 0;
            }
            break;
    }
    if (NvFlags_Test(4, 0x1F) && (E->action_state < 3)) {
        E->action_state = 3;
    }
}

/** 12: it runs the victim (slot 0: Wesker) through, raising 4:0x1E at the end. */
static void ty_scd_impale(void) {
    NvRe1Entity* self = E;
    NvRe1Entity* victim = NvEnemy_SlotEntity(0);

    if (E->action_state == 0) {
        E->action_state = 1;
        ty_anim(5, 0x1F);
        E->move_speed_current = 100;
        E->status_flags |= 2;
        if (victim != NULL) {
            entity_rotate_toward_target(victim->t[0], victim->t[2], 0x400);
            victim->status_flags |= 6;
            victim->state = 1; // its death: idle 3 (z_nv_npc.inc)
            victim->ignore_player_flag = 0;
            victim->action_behavior = 3;
            victim->action_state = 0;
        }
        Snd_em(1);
        return;
    }
    if (E->action_state == 2) {
        ty_scd_flag(0x1E);
        E->action_behavior = 0;
        E->action_state = 0;
        E->status_flags &= ~2;
        return;
    }
    if (E->action_state != 1) {
        return;
    }
    if (E->animation_frame_id == 6) {
        Snd_em(4);
    }
    if (E->animation_frame_id == 0x5F) {
        Snd_em(7);
    }
    E->action_state += (u8)Joint_move(0, 0x80);
    if ((E->move_speed_current != 0) && (victim != NULL)) {
        entity_rotate_toward_target(victim->t[0], victim->t[2], 0x10);
    }
    E->move_speed_current = MAX(0, E->move_speed_current - 5);
    Add_speedXZ(0);
    if (victim != NULL) { // (RE1 carries the victim with the claw's push)
        victim->t[0] += E->speed[0];
        victim->t[2] += E->speed[2];
    }
    E = self;
}

/** 14: animation 10 to its end, then the flag. */
static void ty_scd_anim(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        ty_anim(10, 0x1F);
        E->move_speed_current = 0;
    } else if (E->action_state != 1) {
        return;
    }
    if (Joint_move(0, 0x80)) {
        ty_scd_flag(E->scd_anim_param);
        E->action_behavior = 0;
        E->action_state = 0;
    }
}

/** 15: it turns to her (giving up after a while), then the flag. */
static void ty_scd_face_player(void) {
    s32 turn;

    if (E->action_state == 0) {
        ty_anim(E->animationId, 0x1F);
        E->hit_state = 0;
        if (turn_toward_target(P->t[0], P->t[2], 0x400) != 0) {
            E->animationId = 1;
        }
        E->action_state = 1;
        TY->ticks = (s16)((CbRand() & 0x3F) + 0x28);
    }
    turn = turn_toward_target(P->t[0], P->t[2], (E->behavior_flags & 1) ? 0x14 : 0x28);
    if ((TY->ticks-- == 0) || (turn == 0)) {
        ty_scd_flag(E->scd_anim_param);
        E->action_behavior = 0;
        E->action_state = 0;
        E->player_pos_x = (s16)P->t[0];
        E->player_pos_z = (s16)P->t[2];
    } else {
        Joint_move(0, 0x80);
        E->angle += (s16)turn;
    }
    if ((E->animation_frame_id == 3) || (E->animation_frame_id == 0x24)) {
        Snd_em(0);
    }
}

/** 16: the scripted swing at the victim (slot 0), 4:0x1E at its end. */
static void ty_scd_strike(void) {
    NvRe1Entity* victim = NvEnemy_SlotEntity(0);

    if (E->action_state == 0) {
        E->action_state = 1;
        ty_anim(4, 7);
        Snd_em(1);
    } else if (E->action_state == 2) {
        ty_scd_flag(0x1E);
        E->action_behavior = 0;
        E->action_state = 0;
        return;
    } else if (E->action_state != 1) {
        return;
    }
    E->action_state += (u8)Joint_move(0, 0x200);
    if ((E->animation_frame_id == 0x0B) && (victim != NULL)) {
        victim->state = 1; // knocked into its own reaction (idle 1)
        victim->ignore_player_flag = 0;
        victim->action_behavior = 1;
        victim->action_state = 0;
        Snd_em(2);
    }
}

/** 13: the rocket launcher's kill (the heliport): blown apart (here: it falls in the blasts), then still. */
/* the rocket death's five limbs: joint, launch speed (x, y, z), tumble a tick (x, y, z) */
static const s16 sTyLimbs[5][7] = {
    { 2, 0, -500, 0, 0x40, 0, 0x60 },
    { 4, 10, -400, 0x19, 0x40, 0x60, 0 },
    { 7, -0x1E, -0x1C2, 0x0F, 0, 0, 0x40 },
    { 10, -0x14, -0x15E, -10, 0, 0x60, 0 },
    { 12, -0x14, -300, 0x0F, 0, 0x40, 0x40 },
};

/** tyrant_limb_update: each limb flies, falls (15 a tick), bounces at the floor 3 times, tumbling. */
static void ty_limbs(void) {
    s32 k;

    for (k = 0; k < 5; k++) {
        if (TY->limbBounces[k] == 0) {
            continue;
        }
        TY->limbOff[k][0] += TY->limbV[k][0];
        TY->limbVy[k] += 0x0F;
        TY->limbOff[k][2] += TY->limbV[k][1];
        TY->limbOff[k][1] += TY->limbVy[k];
        if (TY->limbBaseY[k] + TY->limbOff[k][1] > -200) { // the floor
            TY->limbOff[k][1] -= TY->limbVy[k];
            TY->limbBounces[k]--;
            TY->limbVy[k] = -(TY->limbVy[k] / 2);
        }
        TY->limbSpin[k][0] += sTyLimbs[k][4];
        TY->limbSpin[k][1] += sTyLimbs[k][5];
        TY->limbSpin[k][2] += sTyLimbs[k][6];
    }
}

static void ty_scd_rocket_death(void) {
    void NvFlags_Op(s32 bank, s32 bit, s32 mode);
    void NvSnd_At(s32 slot, s32 x, s32 z, s32 lx, s32 lz);
    s32 k;

    switch (E->action_state) {
        case 0: // the body goes (ty_flags bit 3), five limbs blown off
            E->action_state = 1;
            ty_anim(0, 7);
            E->move_speed_current = 0;
            E->status_flags |= 2;
            TY->ticks = 0;
            TY->boom = 1;
            TY->blownUp = 1;
            for (k = 0; k < 5; k++) {
                s32 j = sTyLimbs[k][0];

                TY->limbOff[k][0] = TY->limbOff[k][1] = TY->limbOff[k][2] = 0;
                TY->limbV[k][0] = sTyLimbs[k][1];
                TY->limbVy[k] = sTyLimbs[k][2];
                TY->limbV[k][1] = sTyLimbs[k][3];
                TY->limbSpin[k][0] = TY->limbSpin[k][1] = TY->limbSpin[k][2] = 0;
                TY->limbBounces[k] = 3;
                TY->limbBaseY[k] = (E->numJoints > j) ? E->joint[j][1] : E->t[1] - 1000;
            }
            NvSnd_At(0x19, E->t[0], E->t[2], P->t[0], P->t[2]);
            break;
        case 1:
            if ((TY->ticks > 0x10) && (((TY->ticks + 1) & 3) == 0) && (TY->ticks < 0x40)) {
                TY->boom = 1; // the blasts off its body
            }
            if (++TY->ticks > 0x50) {
                E->action_state = 2;
                TY->ticks = 0;
            }
            break;
        case 2: // it smoulders, then the room moves on
            if (++TY->ticks > 200) {
                E->health = -1;
                if (E->death_event_id != 0xFF) {
                    NvFlags_Op(3, E->death_event_id, 0);
                }
                E->action_state = 3;
            }
            break;
    }
}

/** tyrant_state_scd: the lab one (behaviour bit 0x40) runs the script's; else it's let go. */
static void ty_scd(void) {
    if (!(E->behavior_flags & 0x40)) {
        E->state = 1;
        E->ignore_player_flag = 0;
        E->action_behavior = TY_WALK;
        E->action_state = 0;
        return;
    }
    switch (E->action_behavior) {
        case 0:
            ty_scd_idle();
            break;
        case 2:
            ty_scd_walk_to();
            break;
        case 10:
            ty_scd_break_glass();
            break;
        case 11:
            ty_scd_pod();
            break;
        case 12:
            ty_scd_impale();
            break;
        case 14:
            ty_scd_anim();
            break;
        case 15:
            ty_scd_face_player();
            break;
        case 16:
            ty_scd_strike();
            break;
        case 13:
            ty_scd_rocket_death();
            if (TY->blownUp) {
                ty_limbs();
            }
            break;
        default:
            Joint_move(0, 0x40);
            break;
    }
}

void NvTyrant_Init(NvRe1Entity* e) {
    E = e;
    memset(&E->ty, 0, sizeof(E->ty));
    E->t[1] = -210;
    E->hit_state = 0;
    E->health = ty_roof() ? 600 : 220;
    E->sca = sTySca;
    TY->cooldown = 0x5A;
    ty_anim(6, 0);
    if (ty_roof()) {
        E->state = 1;
        ty_set(TY_ERUPT);
    } else {
        E->state = 8; // the lab's: its script lets it out
        E->ignore_player_flag = 0;
        E->action_behavior = 0x0B;
        E->action_state = 0;
    }
    Joint_move(0, 0x40);
}

void NvTyrant_Update(NvRe1Entity* e) {
    E = e;
    E->pos[0] = (s16)E->t[0];
    E->pos[1] = (s16)E->t[1];
    E->pos[2] = (s16)E->t[2];
    switch (E->state) {
        case 0:
            NvTyrant_Init(e);
            break;
        case 1:
            if (ty_roof() && ((E->action_behavior == 13) || (E->action_behavior == 14))) {
                // the heliport's script gives it its end (event 26): under the script from here
                E->state = 8;
                E->behavior_flags |= 0x40;
                ty_scd();
                break;
            }
            ty_run();
            break;
        case 2:
            ty_stagger();
            break;
        case 3:
            ty_die();
            break;
        case 8: // the script's (the pod, the glass, the walk out, Wesker)
            ty_scd();
            break;
        default:
            if (!(E->behavior_flags & 0x80)) {
                Joint_move(0, 0x40);
            }
            break;
    }
    if (!(E->status_flags & 2)) {
        SetScaWorld(E->sca, E->angle, E->scaWorld);
        SetScaWorld(P->sca, P->angle, P->scaWorld);
        ResolveScaPush(P->sca, P->scaWorld, P->t, E);
        HandleEnemyCollisions();
    }
    TY->wallHit |= check_room_collision();
    // the rocket launcher's moment (tyrant_update's tail): the rooftop one at 200 raises 4:0x1F (the
    // room drops the launcher) and its health is pinned there; the launcher finishes it
    if (ty_roof() && (E->health < 0xC9) && (E->state != 8)) {
        void NvFlags_Op(s32 bank, s32 bit, s32 mode);

        NvFlags_Op(4, 0x1F, 0);
        E->health = 200;
    }
}
