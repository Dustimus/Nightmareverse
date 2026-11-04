/*
 * Nightmareverse: RE1's greenhouse plants (enemy type 15, EM100F), the vines of 10C and 60C.
 * Included at the end of z_nv_zombie.c with the other ported enemies.
 *
 * As RE1 runs them (the decomp's entities/MonsterPlant.cpp is the reference): a plant sways in
 * place and can't be killed by weapons (hits only count; four of them raise a scenario flag).
 * Its behaviour byte picks its kind:
 *   0  a grabber: once the player is within 2700 it watches (3), and inside 1800 it seizes her,
 *      draining 2 of RE1's health every 12 ticks for up to 60 ticks (mashing shortens it), then
 *      lets go and rests;
 *   1  a lasher: inside 3000 and lined up it whips (4), catching a lined-up player for 2;
 *   6, 7  dead (after the greenhouse is dealt with): limp, a last twitch.
 * The room script kills live ones with behaviour flag 0x40.
 *
 * Left out, each a note where it would be: the hidden vines that strike out of the ground
 * (kinds 2 and 5, which reveal the vine segment by segment: no mansion room uses them), the
 * player's held animations, the poisoned kind's tint, and the shadow.
 */

#define MP (&E->mp)

static const s16 sMpSca[8] = { (s16)0x8000, 0, 0, 0, 0, 400, 0, 0 };

s16 gNvRe1Drain;
s32 gNvRe1Kill;

static void mp_anim(u8 id, u8 blend) {
    E->animationId = id;
    E->animation_frame_id = 0;
    E->timing_control = 0;
    E->blend_counter = blend;
}

/** The idle sway: now and then another of its swaying animations. */
static void mp_fidget(void) {
    if (MP->fidget != 0) {
        MP->fidget--;
        return;
    }
    E->animationId = (CbRand() & 1) ? 8 : 3;
    E->timing_control = 0;
    E->blend_counter = 0x1F;
    MP->fidget = (s16)(CbRand() & 0x1F);
    if ((E->animationId == 3) && (E->animation_frame_id > 0x13)) {
        E->animation_frame_id -= 10;
    }
}

static void mp_become(u8 kind) {
    E->ignore_player_flag = 0;
    E->behavior_flags = kind;
}

/** A lasher's check: close enough (2500 once it has struck) and lined up. */
static void mp_lasher_watch(void) {
    if (MP->holdoff != 0) {
        MP->holdoff--;
        return;
    }
    if ((MP->dist <= ((MP->alerted == 0) ? 2500 : 3000)) && (P->isBeingAttackedFlag == 0) &&
        (turn_toward_target(P->t[0], P->t[2], 0x400) == 0)) {
        mp_become(4);
    }
}

/** Kind 0 or 1 idling (behaviour 1): sway until its check sends it to watching or lashing. */
static void mp_idle(void) {
    if (MP->fidget == 0) {
        E->animationId = (CbRand() & 1) ? 7 : 0;
        E->timing_control = 0;
        E->blend_counter = 0x1F;
        MP->fidget = (s16)(CbRand() & 0x1F);
        return;
    }
    MP->fidget--;
    if ((E->behavior_flags & 0xF) == 0) {
        if (MP->dist < 0xA8D) {
            mp_become(3);
        }
    } else if ((E->behavior_flags & 0xF) == 1) {
        mp_lasher_watch();
    }
    if (Joint_move(0, 0x80)) {
        E->ignore_player_flag = 0;
    }
    if (P->flags & 0xE0) { // the player aiming: it stirs faster
        if (Joint_move(0, 0x80)) {
            E->ignore_player_flag = 0;
        }
    }
}

/** The grabber, watching then seizing (behaviour 2's steps 0-5). */
static void mp_grabber(void) {
    MP->animEnd = (u8)Joint_move(0, 0x80);
    switch (E->action_behavior) {
        case 0: // watching
            if (MP->animEnd && (E->animationId == 1)) {
                E->animationId = 8;
                E->timing_control = 0;
                E->blend_counter = 0x0F;
            }
            if (E->animationId != 1) {
                mp_fidget();
            }
            if (MP->dist > 0x2134) {
                mp_become(0);
            }
            if (MP->holdoff != 0) {
                MP->holdoff--;
                break;
            }
            if ((MP->dist < 0x708) && (P->isBeingAttackedFlag == 0) && !gNvRe1DogFlinch) {
                E->action_behavior = 1;
                mp_anim(0x0A, 0); // the lunge
                if (turn_toward_target(P->t[0], P->t[2], 0x200) != 0) {
                    E->action_behavior = 5; // not facing her: a miss
                    MP->timer = 2;
                }
            }
            break;
        case 1: // got her
            E->action_behavior = 2;
            E->status_flags |= 2;
            MP->holding = 1;
            E->holding = 1;
            P->isBeingAttackedFlag = 1;
            snap_player_to_grab_position();
            Snd_em(2);
            break;
        case 2: // pull her in
            MP->angleBack = E->angle;
            entity_rotate_toward_target(P->t[0], P->t[2], 0x80);
            if (MP->animEnd) {
                E->action_behavior = 3;
                E->animationId++;
                E->timing_control = 0;
                E->blend_counter = 0;
                E->action_ticks_counter = 0x3C;
                MP->timer = 0x0C;
                Snd_em(3);
            }
            break;
        case 3: // the drain, until it's held long enough (mashing shortens it)
            if (--MP->timer == 0) {
                gNvRe1Drain += 2;
                MP->timer = 0x0C;
            }
            if (((s16)E->action_ticks_counter > 0) && (P->health >= 0)) {
                E->action_ticks_counter = (u16)((s16)E->action_ticks_counter - 1 - ((P->padHeld != 0) ? 3 : 0));
                break;
            }
            if (MP->animEnd) {
                E->action_behavior = 4;
                E->animationId++;
                E->timing_control = 0;
            }
            break;
        case 4: // let go
            MP->holding = 0;
            E->holding = 0;
            if (MP->animEnd) {
                P->isBeingAttackedFlag = 0;
                mp_become(0);
                E->status_flags &= ~2;
                MP->holdoff = 0x1E;
                E->angle = MP->angleBack;
            }
            break;
        default: // the miss: a couple of grabs at the air, then watch again
            if (!MP->animEnd) {
                break;
            }
            if (--MP->timer == 0) {
                if (MP->dist < 0xA8D) {
                    E->action_behavior = 0;
                    mp_anim(8, 0x0F);
                } else {
                    mp_become(3);
                }
                MP->holdoff = 0x1E;
                break;
            }
            E->animationId += 2;
            E->animation_frame_id = 0;
            E->timing_control = 0;
            E->blend_counter = 0x0F;
            Snd_em(0);
            break;
    }
}

/** The lasher's whip (behaviour 3: steps 6 and 7). */
static void mp_lasher(void) {
    if (E->action_behavior == 0) {
        MP->animEnd = (u8)Joint_move(0, 0x200);
        if (MP->animEnd) {
            E->action_behavior = 1;
            MP->fidget = 0;
            MP->alerted = 0;
        }
        if ((E->animation_frame_id == 6) && (P->isBeingAttackedFlag == 0) && !gNvRe1DogFlinch &&
            (turn_toward_target(P->t[0], P->t[2], 0x400) == 0)) {
            gNvRe1DogBite = 2; // the whip: a sting and a knock back
            Snd_em(1);
            MP->holdoff = 1;
        }
        return;
    }
    mp_fidget();
    mp_lasher_watch();
    if (MP->dist > 0x2133) {
        mp_become(1);
    }
    Joint_move(0, 0x80);
    if (P->flags & 0xE0) {
        Joint_move(0, 0x80);
    }
}

/** The kind byte to a behaviour (when the last one ended). */
static void mp_select(void) {
    u8 kind = E->behavior_flags & 0xF;

    if (E->behavior_flags & 0x80) { // the script's: idle
        kind = 0;
        E->status_flags |= 8;
    }
    switch (kind) {
        case 3:
            E->ignore_player_flag = 2;
            mp_anim(1, 0x1F);
            break;
        case 4:
            E->ignore_player_flag = 3;
            mp_anim((E->angle <= 0x7FF) ? 9 : 2, 7);
            E->status_flags |= 2;
            Snd_em(0);
            break;
        case 6:
        case 7:
            E->ignore_player_flag = 5; // dead: limp, a last twitch
            E->health = -1;
            mp_anim((CbRand() & 1) ? 7 : 0, 0);
            E->animation_frame_id = (u8)(CbRand() & 0xF);
            E->action_ticks_counter = 1;
            break;
        default: // 0, 1 (and the hidden vines, 2, 5, 8: idle for now)
            E->ignore_player_flag = 1;
            E->status_flags &= ~8;
            MP->alerted = 1;
            break;
    }
    E->action_behavior = 0;
    E->action_state = 0;
}

static void mp_run(void) {
    if (E->behavior_flags & 0x40) { // the script kills it
        E->state = 3;
        E->ignore_player_flag = 0;
        return;
    }
    E->status_flags &= 0x1F;
    if (((E->behavior_flags & 0xF) <= 1) || ((E->behavior_flags & 0xF) == 3) || ((E->behavior_flags & 0xF) == 4)) {
        entity_check_visual_range(5000);
    }
    MP->dist = CbAbs(P->t[0] - E->t[0]) + CbAbs(P->t[2] - E->t[2]);
    if (E->ignore_player_flag == 0) {
        mp_select();
    }
    switch (E->ignore_player_flag) {
        case 1:
            mp_idle();
            break;
        case 2:
            mp_grabber();
            break;
        case 3:
            mp_lasher();
            break;
        case 5:
            E->health = -1;
            if (E->action_ticks_counter != 0) {
                Joint_move(0, 0x100);
                E->action_ticks_counter--;
            }
            break;
        default:
            break;
    }
    MP->saved[0] = E->state;
    MP->saved[1] = E->ignore_player_flag;
    MP->saved[2] = E->action_behavior;
    MP->saved[3] = E->action_state;
}

/** Killed by the script: thrash a while, collapse, twitch, still. */
static void mp_die(void) {
    switch (E->ignore_player_flag) {
        case 0:
            E->blend_counter = 0x1F;
            E->ignore_player_flag = 1;
            MP->fidget = 0;
            E->action_ticks_counter = (u16)((CbRand() & 0x1F) + (CbRand() & 0x1F) + 1);
            E->health = -1;
            // fallthrough
        case 1:
            if (E->action_ticks_counter != 0) {
                if (--E->action_ticks_counter != 0) {
                    if (Joint_move(0, 0x80) && (CbRand() & 0x80)) {
                        E->animationId = (CbRand() & 1) ? 0x11 : 0x13;
                        E->blend_counter = 0x1F;
                    }
                    break;
                }
                mp_anim(0x0E, 0x1F); // the collapse
            }
            if (Joint_move(0, 0x80)) {
                E->ignore_player_flag = 2;
                mp_anim((CbRand() & 1) ? 7 : 0, 0x1F);
                E->animation_frame_id = (u8)(CbRand() & 0xF);
                E->action_ticks_counter = (u16)((CbRand() & 0xF) + 0x32);
            }
            break;
        case 2:
            if ((s32)(CbRand() & 0x3F) < (s32)E->action_ticks_counter) {
                Joint_move(0, 0x80);
                Joint_move(0, 0x80);
            }
            if (--E->action_ticks_counter == 0) {
                E->ignore_player_flag = 3;
                if (E->death_event_id != 0xFF) {
                    void NvFlags_Op(s32 bank, s32 bit, s32 mode);

                    NvFlags_Op(3, E->death_event_id, 0);
                }
            }
            break;
        default:
            break;
    }
}

void NvPlant_Init(NvRe1Entity* e) {
    E = e;
    E->state = 1;
    E->ignore_player_flag = 0;
    E->action_behavior = 0;
    E->action_state = 0;
    E->health = 1;
    E->sca = sMpSca;
    E->status_flags = (E->status_flags & 0x1F) | 10; // weapons don't kill it
    mp_anim(0, 0);
    memset(&E->mp, 0, sizeof(E->mp));
    MP->alerted = 1;
    Joint_move(0, 0x80);
    if (E->behavior_flags & 2) {
        E->health = -1;
    }
}

void NvPlant_Update(NvRe1Entity* e) {
    E = e;
    // a hit only counts (four raise RE1's scenario flag): it goes on with what it was doing
    if ((E->state == 2) || ((E->state == 3) && !(E->behavior_flags & 0x40) && (E->health > -100))) {
        if (++MP->hits > 3) {
            void NvFlags_Op(s32 bank, s32 bit, s32 mode);

            NvFlags_Op(0, 0x5B, 0); // SCENARIO_FLAG_MONSTER_PLANT_PROG
        }
        E->state = MP->saved[0] ? MP->saved[0] : 1;
        E->ignore_player_flag = MP->saved[1];
        E->action_behavior = MP->saved[2];
        E->action_state = MP->saved[3];
        E->hit_state = 0;
        if ((E->behavior_flags & 0xF) < 6) {
            E->health = 1;
        }
    }
    switch (E->state) {
        case 0:
            NvPlant_Init(e);
            break;
        case 1:
            mp_run();
            break;
        case 3:
            mp_die();
            break;
        default:
            break;
    }
    SetScaWorld(E->sca, E->angle, E->scaWorld);
}
