/*
 * Nightmareverse: RE1's spider web (enemy type 19, EM1013), the one across 30C's door to 30D.
 * Included at the end of z_nv_zombie.c with the other ported enemies.
 *
 * As RE1 has it (the decomp's entities/SpiderWeb.cpp): not a creature, a solid post in the
 * doorway with 55 health that only cutting and burning wear down - the knife (10 a hit), the
 * flamethrower and the launchers; bullets do nothing. Each hit it takes, a strand more of its six
 * is gone, from the top; destroyed, the last goes, it no longer blocks, and its flag (the room's
 * script waits on it to open the door) goes up.
 */

static const s16 sWebSca[8] = { (s16)0x8000, -500, 0, 0, 0, 500, 0, 0 }; // (Jill's: Chris's is -700 out)

void NvWeb_Init(NvRe1Entity* e) {
    E = e;
    E->state = 1;
    E->ignore_player_flag = 0;
    E->action_behavior = 0;
    E->action_state = 0;
    E->health = 0x37;
    E->hit_state = 0;
    E->webGone = 0;
    E->sca = sWebSca;
    E->animationId = 0;
    E->animation_frame_id = 0;
    E->timing_control = 0;
    E->blend_counter = 0;
    Joint_move(0, 0x400);
}

void NvWeb_Update(NvRe1Entity* e) {
    E = e;
    E->pos[0] = (s16)E->t[0];
    E->pos[1] = (s16)E->t[1];
    E->pos[2] = (s16)E->t[2];
    switch (E->state) {
        case 0:
            NvWeb_Init(e);
            break;
        case 1: // (idle: nothing of its own)
            E->status_flags &= 0x1F;
            break;
        case 2: { // a hit it survived: back to idle, and strands go if it was a blade or fire
            u32 weapon = E->hit_state & 0xF8;

            E->state = 1;
            if ((weapon == 0x08) || (weapon > 0x28)) {
                s32 burnt = (E->health < 50) + (E->health < 40) + (E->health < 30) + (E->health < 20) +
                            (E->health < 10);
                s32 k;

                for (k = 0; k < burnt; k++) {
                    E->webGone |= (u8)(1 << (5 - k)); // from joint 5 down
                }
            }
            E->hit_state = 0;
            break;
        }
        case 3: { // destroyed
            void NvFlags_Op(s32 bank, s32 bit, s32 mode);

            E->webGone = 0x3F;
            E->status_flags |= 2; // (no longer solid, nor a target)
            if (E->death_event_id != 0xFF) {
                NvFlags_Op(3, E->death_event_id, 0);
            }
            E->state = 4;
            break;
        }
        default:
            break;
    }
    if (!(E->status_flags & 2)) {
        SetScaWorld(E->sca, E->angle, E->scaWorld);
        SetScaWorld(P->sca, P->angle, P->scaWorld);
        ResolveScaPush(P->sca, P->scaWorld, P->t, E);
    }
}
