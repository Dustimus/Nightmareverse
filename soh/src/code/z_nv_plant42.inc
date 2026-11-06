/*
 * Nightmareverse: RE1's Plant 42 (enemy type 8, EM1008), the guardhouse boss in 40C. Included at
 * the end of z_nv_zombie.c with the other ported enemies.
 *
 * As RE1 runs it (the decomp's entities/Plant42.cpp is the reference): the room places the plant
 * (behaviour 0; 8 is the one the V-Jolt has weakened) and its vines (1 and 5: 5 reaches less far),
 * all hanging from the ceiling, swinging by their facing and their roll; where a vine's tip is
 * (its joint 14) decides what it does. Near her, lined up, a vine sweeps sideways (16 of RE1's
 * health, knocking her aside), strikes down at her (8), or reaches down and, its tip at her on
 * the floor, grabs her: the squeeze lets her go with 20, or, under 40, kills her. Shot, a vine
 * recoils; killed, it falls limp and the plant loses one. The plant itself can't be hurt while
 * more than one vine is left; when it dies its flag goes up and the vines left wither.
 *
 * Left out, each a note where it would be: the flower body and the root ball (RE1 draws them as
 * copies of the model with their own animations: here the plant's own model stands for the
 * body), the acid in the room, the scripted states (Chris in the vines, the fallen core: the
 * script holds those, behaviour bit 0x40 and kinds 4 and 6), and its own sub-frame animator.
 */

#define PL (&E->pl)

static const s16 sPlSca[8] = { (s16)0x8000, 0, -500, 0, 1000, 800, 0, 0 };

enum { PL_IDLE, PL_SWEEP, PL_STRIKE, PL_REACH, PL_HOLD, PL_RECOIL, PL_DIE, PL_WITHER, PL_KILL };

static NvRe1Entity* sPlMain; // the room's plant (the vines report to it)

static s32 pl_is_vine(void) {
    return (E->behavior_flags & 1) != 0;
}

static void pl_set(u8 behaviour) {
    E->ignore_player_flag = 1;
    E->action_behavior = behaviour;
    E->action_state = 0;
}

static void pl_free(void) {
    E->state = 1;
    E->ignore_player_flag = 0;
    E->action_behavior = PL_IDLE;
    E->action_state = 0;
}

static void pl_anim(u8 id) {
    E->animationId = id;
    E->animation_frame_id = 0;
    E->timing_control = 0;
    E->blend_counter = 0x3F;
}

/** Its tip (joint 14), and how far she is from it. */
static s32 pl_tip_y(void) {
    return (E->numJoints > 14) ? E->joint[14][1] : E->t[1];
}

static s32 pl_tip_dist(s32 j) {
    if (E->numJoints <= j) {
        return 0x7FFF;
    }
    return CbAbs(P->t[0] - E->joint[j][0]) + CbAbs(P->t[2] - E->joint[j][2]);
}

static s32 pl_free_to_hit(void) {
    return (P->isBeingAttackedFlag == 0) && !gNvRe1DogFlinch && !E->holding;
}

/* ---- the moves ---- */

static void pl_idle(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        E->animation_frame_id = 0;
        E->timing_control = 0;
        E->blend_counter = 0x3F;
        PL->ticks = (CbRand() & 0xF) + 0xF;
        PL->sway = (s8)((CbRand() & 0xF) * ((CbRand() & 2) ? -1 : 1));
        PL->swing = (s8)((CbRand() & 7) * ((CbRand() & 2) ? -1 : 1));
    }
    Joint_move(0, 0x40);
    if (!(E->behavior_flags & 0x40)) {
        E->angle = (E->angle + PL->sway) & 0xFFF;
        entity_rotate_toward_target(P->t[0], P->t[2], 8);
    }
    E->roll += PL->swing;
    if ((u16)((E->roll - 0x3E) & 0xFFF) < 0xE32) { // a vine keeps within its swing
        E->roll -= PL->swing;
    }
    if (pl_tip_y() > -600) { // its tip at the floor: lift it
        E->roll -= 0x10;
    }
    if (PL->ticks-- == 0) {
        E->action_state = 0;
    }
}

/** The sweep: a wind-up one way, the lash back across her. */
static void pl_sweep(void) {
    s32 dir = PL->dir ? -1 : 1;

    switch (E->action_state) {
        case 0:
            E->action_state = 1;
            pl_anim(0xC);
            PL->ticks = 0x32;
            PL->dir = (turn_toward_target(P->t[0], P->t[2], 1) < 0) ? 0 : 1;
            PL->step = 0;
            // fall through
        case 1: // the wind-up
            Joint_move(PL->dir ? 0 : 1, 0x40);
            E->angle = (E->angle + dir * 0x10) & 0xFFF;
            if (pl_tip_y() < -2500) {
                E->roll += 8;
            }
            if (pl_tip_y() > -1000) {
                E->roll -= 8;
            }
            if (PL->ticks-- == 0) {
                E->action_state = 2;
                PL->ticks = 0xF;
            }
            return;
        case 2: // a hold
            Joint_move(PL->dir ? 1 : 0, 0x40);
            if (PL->ticks-- == 0) {
                E->action_state = 3;
                pl_anim(0xC);
            }
            return;
        case 3: { // the lash, faster and faster
            s32 reach = PL->dist - ((E->behavior_flags == 5) ? 0xDAC : 2000);

            PL->step += 4;
            if (PL->step > 0x28) {
                PL->step += 0x1C;
            }
            Joint_move(PL->dir ? 0 : 1, 0x40);
            E->angle = (E->angle - dir * PL->step) & 0xFFF;
            if (((u32)reach <= 0x157B) && (turn_toward_target(P->t[0], P->t[2], PL->step) == 0) &&
                pl_free_to_hit() && (PL->step > 0x50)) {
                E->roll -= 200;
                gNvRe1DogBite = 16;
                E->action_state = 4;
                return;
            }
            if (PL->step > 0xC0) {
                E->action_state = 4;
                Snd_em(3);
            }
            return;
        }
        case 4: // the swing runs out
            E->roll -= 8;
            Joint_move(PL->dir ? 1 : 0, 0x40);
            if (PL->step < 1) {
                pl_free();
                return;
            }
            E->angle = (E->angle - dir * PL->step) & 0xFFF;
            PL->step -= 0x10;
            return;
        default:
            pl_free();
            return;
    }
}

/** The strike from above: it rears, then comes down at her. */
static void pl_strike(void) {
    switch (E->action_state) {
        case 0:
            E->action_state = 1;
            pl_anim(7);
            PL->step = 4;
            // fall through
        case 1:
            if (Joint_move(0, 0x40)) {
                E->action_state = 2;
                E->roll -= PL->step;
                PL->step -= 0x30;
                Snd_em(1);
                return;
            }
            if (E->animation_frame_id < 8) {
                E->roll += 0x40;
                return;
            }
            if (E->animation_frame_id > 0xC) {
                E->roll -= PL->step;
                PL->step += 0x18;
            }
            if ((E->animation_frame_id > 10) && pl_free_to_hit() && (pl_tip_dist(15) < 1000) &&
                (pl_tip_y() > -2500)) {
                gNvRe1DogBite = 8;
                PL->step -= 8;
                Snd_em(2);
            }
            return;
        case 2:
            E->roll -= PL->step;
            PL->step -= 8;
            if (PL->step < 0) {
                E->action_state = 3;
            }
            return;
        default: // back up
            E->animationId = 2;
            E->roll += 0x10;
            Joint_move(0, 0x40);
            if (E->roll > -400) {
                pl_free();
            }
            return;
    }
}

/** The reach: down toward her, and at her on the floor, the grab. */
static void pl_reach(void) {
    switch (E->action_state) {
        case 0:
            E->action_state = 1;
            pl_anim(8);
            PL->ticks = 0x3C;
            PL->step = 8;
            // fall through
        case 1: // down
            if (PL->ticks-- == 0) {
                pl_free();
                return;
            }
            E->angle = (E->angle + turn_toward_target(P->t[0], P->t[2], 0x10)) & 0xFFF;
            Joint_move(1, 0x40);
            if (pl_tip_y() < -2500) {
                E->roll += 0x10;
            } else if (pl_tip_y() < -999) {
                if (PL->ticks < 0x1E) {
                    E->action_state = 2;
                    PL->ticks = 0x3C;
                }
            } else {
                E->roll -= 0x10;
            }
            return;
        case 2: // feeling for her
            if ((PL->ticks-- == 0) || (P->isBeingAttackedFlag != 0)) {
                pl_free();
                return;
            }
            E->angle = (E->angle + turn_toward_target(P->t[0], P->t[2], 0x10)) & 0xFFF;
            if (((pl_tip_dist(14) >= 800) && (pl_tip_dist(12) >= 500)) || !pl_free_to_hit() || (P->t[1] != 0)) {
                Joint_move(1, 0x40);
                E->roll += PL->step;
                if ((pl_tip_y() > -1500) && (PL->step >= 0)) {
                    PL->step = -PL->step;
                }
                if ((pl_tip_y() < -2500) && (PL->step < 0)) {
                    PL->step = -PL->step;
                }
                return;
            }
            // caught
            pl_anim(0xE);
            E->blend_counter = 3;
            E->hit_state = 1;
            E->unk_c6 = (s16)E->joint[11][0];
            E->unk_c8 = (s16)E->joint[11][2];
            E->holding = 1;
            Snd_em(5);
            pl_set(PL_HOLD);
            return;
        default:
            pl_free();
            return;
    }
}

/** The squeeze: a while, then she's thrown down (20), or, too weak to take it, it kills her. */
static void pl_hold(void) {
    switch (E->action_state) {
        case 0:
            E->action_state = 1;
            PL->ticks = 0x3C;
            pl_anim(0xF);
            Snd_em(6);
            if (P->health < 0x28) {
                pl_set(PL_KILL);
                return;
            }
            // fall through
        case 1:
            Joint_move(0, 0x400);
            if (PL->ticks-- == 0) {
                E->action_state = 2;
            }
            return;
        default:
            E->holding = 0;
            gNvRe1DogBite = 20;
            E->hit_state = 0;
            pl_free();
            return;
    }
}

/** The kill: the squeeze doesn't stop. */
static void pl_kill(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        PL->ticks = 0x5A;
        pl_anim(0xF);
    }
    Joint_move(0, 0x400);
    if (PL->ticks-- == 0) {
        gNvRe1Kill = 1;
        E->holding = 0;
        pl_free();
    }
}

/** The plant is dead: the vine sags and lies still. */
static void pl_wither(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        pl_anim(0xD);
        PL->ticks = 0xD2;
        E->holding = 0;
        E->hit_state = 1;
    }
    Joint_move(0, 0x40);
    if (pl_tip_y() < -600) {
        E->roll -= 8;
    }
    if (PL->ticks-- == 0) {
        E->status_flags |= 10;
    }
}

static void pl_pick(void) {
    static const u8 sNear[8] = { PL_SWEEP, PL_STRIKE, PL_REACH, PL_SWEEP, PL_REACH, PL_SWEEP, PL_SWEEP, PL_STRIKE };
    static const u8 sNearShort[8] = { PL_SWEEP, PL_STRIKE, PL_SWEEP, PL_SWEEP, PL_STRIKE, PL_SWEEP, PL_SWEEP, PL_STRIKE };

    if ((P->isBeingAttackedFlag != 0) || !pl_is_vine() || ((E->behavior_flags & 7) == 4)) {
        return;
    }
    if (PL->dist < 11000) {
        E->angle = (E->angle + turn_toward_target(P->t[0], P->t[2], 4)) & 0xFFF;
    }
    if ((PL->dist < 10000) && (turn_toward_target(P->t[0], P->t[2], 0x80) == 0)) {
        pl_set((u8)(PL_SWEEP + (CbRand() & 1)));
    }
    if ((PL->dist < 9000) && (turn_toward_target(P->t[0], P->t[2], 0x100) == 0)) {
        pl_set(((E->behavior_flags & 7) == 5) ? sNearShort[CbRand() & 7] : sNear[CbRand() & 7]);
    }
}

static void pl_act(void) {
    switch (E->action_behavior) {
        case PL_IDLE:
            pl_idle();
            break;
        case PL_SWEEP:
            pl_sweep();
            break;
        case PL_STRIKE:
            pl_strike();
            break;
        case PL_REACH:
            pl_reach();
            break;
        case PL_HOLD:
            pl_hold();
            break;
        case PL_KILL:
            pl_kill();
            break;
        case PL_WITHER:
            pl_wither();
            break;
        default:
            pl_free();
            break;
    }
}

static void pl_run(void) {
    s32 dx = P->t[0] - E->t[0];
    s32 dz = P->t[2] - E->t[2];

    if (E->behavior_flags & 0x80) {
        return; // held by its script
    }
    PL->dist = SquareRoot0(dx * dx + dz * dz);
    if (pl_is_vine() && (sPlMain != NULL) && (sPlMain->pl.dead) && (E->action_behavior != PL_WITHER)) {
        pl_set(PL_WITHER);
    }
    if (E->ignore_player_flag == 0) {
        pl_pick();
    }
    pl_act();
    // the plant itself is out of reach while more than one vine is left
    if (!pl_is_vine() && (E->action_behavior == PL_IDLE)) {
        E->hit_state = (PL->vinesLeft > 1) ? 1 : 0;
    }
}

/** Shot: a vine recoils (and lets go of her); the plant shudders. */
static void pl_recoil(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        pl_anim((u8)(2 + (CbRand() & 3)));
        E->blend_counter = 3;
        E->holding = 0;
        PL->ticks = 0x14;
    }
    Joint_move(0, 0x400);
    E->roll -= 4;
    if (PL->ticks-- == 0) {
        E->hit_state = 0;
        pl_free();
    }
}

/** Killed: a vine falls limp and the plant has one fewer; the plant's death raises its flag. */
static void pl_die(void) {
    if (E->action_state == 0) {
        E->action_state = 1;
        pl_anim(0xD);
        E->holding = 0;
        E->status_flags |= 2;
        PL->ticks = 0x5A;
        if (pl_is_vine()) {
            if ((sPlMain != NULL) && (sPlMain->pl.vinesLeft > 0)) {
                sPlMain->pl.vinesLeft--;
            }
        } else {
            PL->dead = 1;
            if (E->death_event_id != 0xFF) {
                void NvFlags_Op(s32 bank, s32 bit, s32 mode);

                NvFlags_Op(3, E->death_event_id, 0);
            }
        }
    }
    Joint_move(0, 0x40);
    if (pl_tip_y() < -600) {
        E->roll -= 0x10;
    }
    if (PL->ticks-- == 0) {
        E->status_flags |= 10;
    }
}

/** The plant's actor goes (the room left): the vines forget it. */
void NvPlant42_Forget(NvRe1Entity* e) {
    if (sPlMain == e) {
        sPlMain = NULL;
    }
}

void NvPlant42_Init(NvRe1Entity* e) {
    E = e;
    memset(&E->pl, 0, sizeof(E->pl));
    E->state = 1;
    E->ignore_player_flag = 0;
    E->action_behavior = PL_IDLE;
    E->action_state = 0;
    E->hit_state = 0;
    E->health = pl_is_vine() ? 0x28 : 0x28 + 100;
    pl_anim(2);
    Joint_move(0, 0x40);
    E->sca = sPlSca;
    if (!pl_is_vine()) {
        sPlMain = E;
        E->hit_state = 1;
        PL->vinesLeft = ((E->behavior_flags & 0xF) == 8) ? 0 : 4; // the V-Jolt: no vines to wear down
    }
}

void NvPlant42_Update(NvRe1Entity* e) {
    E = e;
    E->pos[0] = (s16)E->t[0];
    E->pos[1] = (s16)E->t[1];
    E->pos[2] = (s16)E->t[2];
    switch (E->state) {
        case 0:
            NvPlant42_Init(e);
            break;
        case 1:
            pl_run();
            break;
        case 2:
            pl_recoil();
            break;
        case 3:
            pl_die();
            break;
        default:
            if (!(E->behavior_flags & 0x80)) {
                Joint_move(0, 0x40); // the script's
            }
            break;
    }
    if (!(E->status_flags & 2)) {
        SetScaWorld(E->sca, E->angle, E->scaWorld);
        SetScaWorld(P->sca, P->angle, P->scaWorld);
        ResolveScaPush(P->sca, P->scaWorld, P->t, E);
    }
}
