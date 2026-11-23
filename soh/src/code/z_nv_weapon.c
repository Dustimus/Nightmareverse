/*
 * Nightmareverse: RE1's guns in Link's hands.
 *
 * In RE1's rooms the weapon equipped on RE1's status screen (z_nv_items.c) takes a C button (C-Left
 * unless a C button was pressed on it there; its RE1 icon on the button; the OoT item there comes back
 * outside the mansion). Holding the button squares
 * Link up as RE1's aim does (PlayerAnimations.cpp player_behavior_12_gun_aim): he stops, the stick
 * turns him on the spot and up/down aims high or low, and Jill's raise and hold for that weapon
 * and direction play on him (re1/tools/soh_weapons.py, mirrored: Link is left-handed; through
 * z_nv_linkanim.c). Releasing it fires (B fires too and keeps the aim); a shot spends a round
 * from the slot (the knife none) and plays Jill's fire.
 *
 * The aim: Z-targeting picks the target when the lock-on is one of RE1's enemies; otherwise, as
 * RE1's auto-aim (player_reticle_enemy), the nearest enemy ahead within the weapon's reach turns
 * Link toward it. The hit: RE1's gun test (weapon_hit_detect_gun, simplified to its near band: an
 * enemy within the weapon's range ahead and its radius + 200 to either side, the nearest), then
 * RE1's damage and hit state for that weapon (z_nv_enemy.c NvEnemy_GunHit).
 *
 * The gun: RE1's item-viewer model (not Jill's low-poly hand-and-gun mesh), in Link's left fist.
 * It's drawn only while the gun is up, so any other item (the ocarina) replaces it.
 */
#include "global.h"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>
#include "overlays/effects/ovl_Effect_Ss_HitMark/z_eff_ss_hitmark.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NV_RE1_SCALE 0.0217f

extern PlayState* gPlayState;

s32 NvStatus_InMansion(PlayState* play);
s32 NvItems_Equipped(void);
s32 NvItems_Slot(s32 i, s32* qty);
void NvItems_SpendRound(s32 i);
const u16* NvStatus_IconFor(s32 id);
s32 NvLinkAnim_PlayFrom(const char* path, s32 skip, s32 anim, s32 hold, s32 mirror);
void NvLinkAnim_Stop(void);
s32 NvLinkAnim_Done(void);
s32 NvEnemy_GunTarget(Actor* a, s32* x, s32* z, s32* radius);
void NvEnemy_GunHit(Actor* a, s32 item, u8 aim);
s32 NvEnemy_ShotRules(Actor* a, s32 item, u8 aim, s32* reachAdj);
s32 NvRe1_ShotBlocked(s32 x, s32 z, s32 tx, s32 tz);
u8 NvEnemy_AimHeights(Actor* a);
void* NvModel_Parse(const u8* d);
void NvModel_Bounds(void* model, s16 mn[3], s16 mx[3]);
void NvModel_Draw(GraphicsContext* gfxCtx, void* model);
s32 NvClimb_Active(void);
void NvSound_Menu(u16 sfx);
void NvSound_Shot(s32 item);
s32 NvDoorScene_Covering(void);
s32 NvActor_EnemyId(void);

typedef enum { NV_GUN_NONE, NV_GUN_RAISE, NV_GUN_AIM, NV_GUN_FIRE } NvGunState;
typedef enum { NV_AIM_LEVEL, NV_AIM_UP, NV_AIM_DOWN } NvAimDir;

// the aim as RE1's player flags have it (0x20 up, 0x40 level, 0x80 down): what an enemy's status
// flags are tested against, and what raises a hit's state
static const u8 sNvAimFlags[3] = { 0x40, 0x20, 0x80 };

// Jill's range with each weapon (WeaponDamage.cpp weapons_ranges, her half), by item id - 1; the
// rocket launcher's 1100 is its blast's: its rocket flies (effect_behavior_rocket) until it meets
// something, so here it reaches across a room
static const s16 sNvGunRange[10] = { 500, 1300, 2600, 800, 800, 400, 900, 900, 900, 16000 };

// Link's fist: the gun's length in his hand limb's units, by item id - 1 (set by eye: the Beretta
// about as long as his forearm looks)
static const s16 sNvGunLength[10] = { 1800, 1500, 4600, 1900, 1900, 3600, 3800, 3800, 3800, 4800 };

typedef struct {
    s32 item;     // RE1's item id loaded (0: none)
    char path[32];
    s32 skip;     // the "RPLA" part's offset in the blob
    void* model;  // the gun
    f32 scale;    // model units to Link's
    f32 center[3];
} NvGun;

static NvGun sNvGun;
static s32 sNvGunState;
static s32 sNvGunDir;
static s32 sNvGunFireHeld; // the fire came from B: stay up after it
static MtxF sNvGunHand;    // Link's left hand this frame (Player_PostLimbDrawGameplay)
static s32 sNvGunHandSet;

/** The equipped weapon (RE1's item id) if it's one Link can use here and its file is there. */
static s32 NvGun_Equipped(void) {
    s32 slot = NvItems_Equipped();
    s32 id = (slot >= 0) ? NvItems_Slot(slot, NULL) : 0;
    char path[32];

    if ((id <= 0) || (id > 10)) {
        return 0;
    }
    snprintf(path, sizeof(path), "re1/weapons/w%d", (int)id);
    return ResourceMgr_FileExists(path) ? id : 0;
}

/** Load weapon item's blob ("RWPN", soh_weapons.py): Jill's moves and the gun's model. */
static s32 NvGun_Load(s32 item) {
    const u8* d;
    const u8* p;
    u16 bones, nf, na;
    s32 i;

    if (sNvGun.item == item) {
        return sNvGun.model != NULL;
    }
    sNvGun.item = item;
    sNvGun.model = NULL;
    snprintf(sNvGun.path, sizeof(sNvGun.path), "re1/weapons/w%d", (int)item);
    d = (const u8*)ResourceGetDataByName(sNvGun.path);
    if ((d == NULL) || (memcmp(d, "RWPN", 4) != 0) || (memcmp(d + 6, "RPLA", 4) != 0)) {
        return false;
    }
    sNvGun.skip = 6;
    // past the "RPLA" part to the model: bones, frames, animations (as z_nv_linkanim.c reads it)
    p = d + 6 + 4;
    memcpy(&bones, p, 2);
    p += 2 + bones * 8;
    memcpy(&nf, p, 2);
    p += 2 + nf * (6 + 3 * bones) * 2;
    memcpy(&na, p, 2);
    p += 2;
    for (i = 0; i < na; i++) {
        u16 ns;

        memcpy(&ns, p, 2);
        p += 2 + ns * 4;
    }
    p += 4; // the model's length
    sNvGun.model = NvModel_Parse(p);
    {
        s16 mn[3], mx[3];
        f32 ext = 1.0f;
        s32 k;

        NvModel_Bounds(sNvGun.model, mn, mx);
        for (k = 0; k < 3; k++) {
            ext = MAX(ext, (f32)(mx[k] - mn[k]));
            sNvGun.center[k] = (mn[k] + mx[k]) * 0.5f;
        }
        sNvGun.scale = sNvGunLength[CLAMP(item - 1, 0, 9)] / ext;
    }
    return true;
}

/** The RE1 item id of the weapon in her hand (0: none), for the scenes' hand set (z_nv_enemy.c). */
s32 NvWeapon_EquippedItem(void) {
    return NvGun_Equipped();
}

s32 NvItems_GunButton(void);

/**
 * The item button RE1's gun has in the mansion (OoT's item button index: 1 C-Left, 2 C-Down,
 * 3 C-Right; the status screen sets it), or 0: none (no weapon equipped, or not in the mansion).
 */
s32 NvWeapon_GunButton(PlayState* play) {
    return (NvStatus_InMansion(play) && (NvGun_Equipped() != 0)) ? 1 + NvItems_GunButton() : 0;
}

static const u16 sNvGunBtnMask[] = { 0, BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT };

/** Whether the gun is up (drawn in Link's fist; his left hand a fist). */
s32 NvWeapon_Holding(void) {
    return sNvGunState != NV_GUN_NONE;
}

// Jill's animations for the weapon and aim direction (PlayerAnimations.cpp: the raise, the fire,
// the hold; the grenade launcher has the level aim only, the knife its raise and slash)
static void NvGun_Anims(s32 item, s32 dir, s32* raise, s32* fire, s32* hold) {
    if ((item == 1) || (item >= 6)) {
        dir = NV_AIM_LEVEL;
    }
    *raise = 5 + dir * 3;
    *fire = 6 + dir * 3;
    *hold = 7 + dir * 3;
    if (item == 1) { // the knife: the raise, then the standing slash
        *fire = 7;
        *hold = 5;
    }
}

static void NvGun_Play(s32 anim) {
    NvLinkAnim_PlayFrom(sNvGun.path, sNvGun.skip, anim, true, true);
}

static void NvGun_Lower(void) {
    if (sNvGunState != NV_GUN_NONE) {
        NvLinkAnim_Stop();
    }
    sNvGunState = NV_GUN_NONE;
}

/** Link in RE1's units: where, and his facing (OoT's yaw). */
static void NvGun_Re1Pos(Player* player, s32* x, s32* z) {
    *x = (s32)lroundf(player->actor.world.pos.x / NV_RE1_SCALE);
    *z = (s32)lroundf(-player->actor.world.pos.z / NV_RE1_SCALE);
}

/**
 * An enemy's place relative to Link: ahead along his facing, and to the side (RE1 units), and the
 * OoT yaw from him to it.
 */
static void NvGun_Relative(Player* player, s32 ex, s32 ez, f32* ahead, f32* side, s16* yaw) {
    s32 px, pz;
    f32 dx, dz;
    f32 a = player->actor.shape.rot.y * (M_PI / 32768.0f);

    NvGun_Re1Pos(player, &px, &pz);
    dx = (f32)(ex - px);
    dz = -(f32)(ez - pz); // (OoT's z)
    *ahead = dx * sinf(a) + dz * cosf(a);
    *side = dx * cosf(a) - dz * sinf(a);
    *yaw = (s16)(atan2f(dx, dz) * (32768.0f / M_PI));
}

/** The enemy the aim is on: the Z-target if it's RE1's, else the nearest ahead in reach. */
static Actor* NvGun_Target(PlayState* play, Player* player, s32 item, s32 reach, s32 autoAim) {
    s32 reachHere;
    Actor* best = NULL;
    f32 bestAhead = 1e9f;
    Actor* a;
    s32 ex, ez, r;

    if ((player->focusActor != NULL) && NvEnemy_GunTarget(player->focusActor, &ex, &ez, &r)) {
        s32 adj;

        if (autoAim || NvEnemy_ShotRules(player->focusActor, item, sNvAimFlags[sNvGunDir], &adj)) {
            return player->focusActor;
        }
    }
    for (a = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; a != NULL; a = a->next) {
        f32 ahead, side;
        s16 yaw;

        if (!NvEnemy_GunTarget(a, &ex, &ez, &r)) {
            continue;
        }
        if (!autoAim) { // apply_weapon_damage's own rules for the shot: the heights it's hit at, its reach
            s32 adj;

            if (!NvEnemy_ShotRules(a, item, sNvAimFlags[sNvGunDir], &adj)) {
                continue;
            }
            reachHere = reach + adj;
        } else {
            reachHere = reach;
        }
        NvGun_Relative(player, ex, ez, &ahead, &side, &yaw);
        if (autoAim) { // RE1's reticle: ahead within about 45 degrees and in reach
            if ((ahead <= 0.0f) || (ahead > reachHere + r) || (fabsf(side) > ahead + r)) {
                continue;
            }
        } else if (item == 1) {
            // weapon_hit_detect_knife (WeaponDamage.cpp 0x0043d690): from the blade in her hand (joint 14
            // at the thrust, here 600 ahead of her), the enemy's place within its radius + the reach
            f32 dh = ahead - 600.0f;

            if ((ahead <= 0.0f) || ((dh * dh + side * side) > (f32)(r + reachHere) * (f32)(r + reachHere))) {
                continue;
            }
        } else if ((item >= 2) && (item <= 5)) {
            // weapon_hit_detect_gun (WeaponDamage.cpp 0x0043d410) / checkEntityInRangeCone: the guns' range
            // is the half-width of a band straight ahead (the shotgun's the widest: its spread), from 650
            // in front of her on, however far; the nearest in it is hit
            if ((ahead < 650.0f) || (fabsf(side) > r + reachHere)) {
                continue;
            }
        } else if ((ahead <= 0.0f) || (ahead > reachHere + r) || (fabsf(side) > r + 200)) {
            continue; // the knife and the launchers: in front, within its reach
        }
        if (ahead < bestAhead) {
            bestAhead = ahead;
            best = a;
        }
    }
    return best;
}

/** Link's hand (the gun's), RE1's units: RE1's joint 14 for the fire effects. */
static void NvGun_HandRe1(Player* player, s32* at) {
    if (sNvGunHandSet) {
        at[0] = (s32)lroundf(sNvGunHand.xw / NV_RE1_SCALE);
        at[1] = (s32)lroundf(-sNvGunHand.yw / NV_RE1_SCALE);
        at[2] = (s32)lroundf(-sNvGunHand.zw / NV_RE1_SCALE);
    } else {
        at[0] = (s32)lroundf(player->actor.world.pos.x / NV_RE1_SCALE);
        at[1] = (s32)lroundf(-(player->actor.world.pos.y + 30.0f) / NV_RE1_SCALE);
        at[2] = (s32)lroundf(-player->actor.world.pos.z / NV_RE1_SCALE);
    }
}

/**
 * A flying round (the launcher's: z_nv_effects.c fx_bullet) at (x, z), RE1's units, its height
 * giving the aim flags: apply_weapon_damage with weapon_hit_detect_projectile, the nearest enemy
 * whose radius plus the weapon's range takes in the round, at a height it can be hit at. True if one
 * was hit.
 */
s32 NvGun_RoundHit(s32 item, s32 x, s32 z, u8 aim) {
    PlayState* play = gPlayState;
    Actor* best = NULL;
    f32 bestD = 1e9f;
    Actor* a;

    if ((play == NULL) || (item < 1) || (item > 10)) {
        return false;
    }
    for (a = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; a != NULL; a = a->next) {
        s32 ex, ez, r, adj;
        f32 d;

        if (!NvEnemy_GunTarget(a, &ex, &ez, &r) || !NvEnemy_ShotRules(a, item, aim, &adj)) {
            continue;
        }
        d = sqrtf((f32)(ex - x) * (f32)(ex - x) + (f32)(ez - z) * (f32)(ez - z));
        if ((d < (f32)(r + sNvGunRange[item - 1] + adj)) && (d < bestD)) {
            bestD = d;
            best = a;
        }
    }
    if (best == NULL) {
        return false;
    }
    {
        Vec3f at = best->focus.pos;

        NvEnemy_GunHit(best, item, aim);
        EffectSsHitMark_SpawnFixedScale(play, EFFECT_HITMARK_WHITE, &at);
    }
    return true;
}

/** A shot (apply_weapon_damage): spend a round, hit what's in the line. */
static void NvGun_Fire(PlayState* play, Player* player) {
    s32 slot = NvItems_Equipped();
    s32 qty = 0;
    s32 item = sNvGun.item;
    Actor* hit;
    s32 raise, fire, hold;

    NvItems_Slot(slot, &qty);
    if ((item != 1) && (qty <= 0)) { // empty: loaded from her ammo instead of a shot, else the click
        s32 NvItems_Reload(s32 gun);
        s32 loaded = NvItems_Reload(slot);

        if (loaded > 0) {
            Player_PlaySfx(&player->actor, NA_SE_IT_SWORD_PUTAWAY);
            printf("NVGUN reload: %d rounds\n", (int)loaded);
            fflush(stdout);
            return;
        }
        NvSound_Menu(NA_SE_SY_ERROR);
        printf("NVGUN click (empty)\n");
        fflush(stdout);
        return;
    }
    if (item != 1) {
        NvItems_SpendRound(slot);
    }
    NvGun_Anims(item, sNvGunDir, &raise, &fire, &hold);
    NvGun_Play(fire);
    sNvGunState = NV_GUN_FIRE;
    NvSound_Shot(item); // RE1's shot for the weapon (z_nv_sound.c)
    {
        void NvFx_FireGun(s32 item, const s32* hand, s32 angle, s32 pitch);
        s32 at[3];

        NvGun_HandRe1(player, at);
        NvFx_FireGun(item, at, ((0x4000 - player->actor.shape.rot.y) & 0xFFFF) >> 4,
                     (sNvGunDir == NV_AIM_UP) ? 0x200 : (sNvGunDir == NV_AIM_DOWN) ? -0x200 : 0);
    }
    if ((item >= 7) && (item <= 9)) { // the launcher: RE1's round flies from the gun (z_nv_effects.c)
        s32 NvFx_FireRound(s32 item, const s32* hand, s32 angle);
        s32 at[3];

        NvGun_HandRe1(player, at);
        if (NvFx_FireRound(item, at, ((0x4000 - player->actor.shape.rot.y) & 0xFFFF) >> 4)) {
            printf("NVGUN fire weapon %d: the round flies from %d,%d,%d\n", (int)item, (int)at[0], (int)at[1],
                   (int)at[2]);
            fflush(stdout);
            return;
        }
    }
    hit = NvGun_Target(play, player, item, sNvGunRange[CLAMP(item - 1, 0, 9)], false);
    printf("NVGUN fire weapon %d dir %d rounds %d -> %s\n", (int)item, (int)sNvGunDir, (int)(qty - 1),
           (hit != NULL) ? "hit" : "miss");
    if ((hit == NULL) && getenv("NV_LOG")) { // what it missed
        Actor* e;

        for (e = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; e != NULL; e = e->next) {
            s32 ex, ez, r;
            f32 ahead, side;
            s16 yaw;

            if (NvEnemy_GunTarget(e, &ex, &ez, &r)) {
                NvGun_Relative(player, ex, ez, &ahead, &side, &yaw);
                printf("NVGUN   missed one %.0f ahead, %.0f to the side (radius %d)\n", ahead, side, (int)r);
            } else if (e->id == NvActor_EnemyId()) {
                printf("NVGUN   one not a target now\n");
            }
        }
    }
    fflush(stdout);
    if ((hit != NULL) && (item <= 5)) { // check_weapon_line_of_sight: the knife and the guns, not through walls
        s32 px, pz, ex, ez, r;

        NvGun_Re1Pos(player, &px, &pz);
        if (NvEnemy_GunTarget(hit, &ex, &ez, &r) && NvRe1_ShotBlocked(px, pz, ex, ez)) {
            printf("NVGUN   blocked: a wall between\n");
            hit = NULL;
        }
    }
    if (hit != NULL) {
        Vec3f at = hit->focus.pos;

        NvEnemy_GunHit(hit, item, sNvAimFlags[sNvGunDir]);
        EffectSsHitMark_SpawnFixedScale(play, EFFECT_HITMARK_WHITE, &at);
    }
}

/**
 * Each frame, before Link acts on the input (z_player.c Player_Update, on its copy of it): RE1's
 * aim and fire on its C button; while the gun is up the stick turns him and he doesn't walk or swing.
 */
void NvWeapon_PreUpdate(Player* player, PlayState* play, Input* input) {
    s32 gunBtn = NvWeapon_GunButton(play);
    u16 mask = sNvGunBtnMask[gunBtn];
    s32 item = (gunBtn != 0) ? NvGun_Equipped() : 0;
    s32 held = (mask != 0) && CHECK_BTN_ALL(input->cur.button, mask);
    s32 raise, fire, hold;
    s32 dir;

    // only when Link could act: on the ground, free, nothing else going on
    if ((item == 0) || !NvGun_Load(item) || (player->stateFlags1 & (PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_IN_WATER |
                                                                      PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_DEAD)) ||
        (player->stateFlags2 & PLAYER_STATE2_GRABBED_BY_ENEMY) || !(player->actor.bgCheckFlags & 1) ||
        (play->msgCtx.msgMode != MSGMODE_NONE) || NvClimb_Active() || NvDoorScene_Covering() ||
        (play->transitionTrigger != TRANS_TRIGGER_OFF)) {
        if ((item != 0) && held && getenv("NV_LOG") && ((play->gameplayFrames % 20) == 0)) {
            printf("NVGUN held but can't aim: s1 %08X s2 %08X ground %d msg %d climb %d door %d trans %d\n",
                   (unsigned)player->stateFlags1, (unsigned)player->stateFlags2, player->actor.bgCheckFlags & 1,
                   play->msgCtx.msgMode, NvClimb_Active(), NvDoorScene_Covering(), play->transitionTrigger);
        }
        NvGun_Lower();
        return;
    }
    // another item button while the gun is up: that item instead (the gun goes away)
    if ((sNvGunState != NV_GUN_NONE) && CHECK_BTN_ANY(input->press.button, (BTN_CLEFT | BTN_CDOWN | BTN_CRIGHT) & ~mask)) {
        NvGun_Lower();
        return;
    }
    if ((sNvGunState == NV_GUN_NONE) && !held) {
        return;
    }
    // the aim's direction: up/down on the stick, as RE1's D-pad; with the stick level and the aim on an
    // enemy, its height (auto_aim_pitch_update: RE1's auto-aim points her arm at the target, and the
    // shot's height is where the arm points: up at a chimera on the ceiling, down at a snake)
    dir = (input->rel.stick_y > 45) ? NV_AIM_UP : (input->rel.stick_y < -45) ? NV_AIM_DOWN : NV_AIM_LEVEL;
    if (dir == NV_AIM_LEVEL) {
        Actor* aimed = NvGun_Target(play, player, item, sNvGunRange[CLAMP(item - 1, 0, 9)], true);
        u8 heights = (aimed != NULL) ? NvEnemy_AimHeights(aimed) : 0x40;

        if (!(heights & 0x40)) {
            dir = (heights & 0x80) ? NV_AIM_DOWN : (heights & 0x20) ? NV_AIM_UP : NV_AIM_LEVEL;
        }
    }
    NvGun_Anims(item, dir, &raise, &fire, &hold);

    switch (sNvGunState) {
        case NV_GUN_NONE: // its button: the gun comes up
            sNvGunState = NV_GUN_RAISE;
            sNvGunDir = dir;
            NvGun_Play(raise);
            break;
        case NV_GUN_RAISE:
        case NV_GUN_AIM:
            if (dir != sNvGunDir) { // a new direction: raise to it
                sNvGunDir = dir;
                sNvGunState = NV_GUN_RAISE;
                NvGun_Play(raise);
            } else if ((sNvGunState == NV_GUN_RAISE) && NvLinkAnim_Done()) {
                sNvGunState = NV_GUN_AIM;
                NvGun_Play(hold);
            }
            if (sNvGunState == NV_GUN_AIM) {
                if (!held) { // let go: the shot
                    sNvGunFireHeld = false;
                    NvGun_Fire(play, player);
                    if (sNvGunState != NV_GUN_FIRE) {
                        NvGun_Lower();
                    }
                } else if (CHECK_BTN_ALL(input->press.button, BTN_B)) { // B: a shot, still aiming
                    sNvGunFireHeld = true;
                    NvGun_Fire(play, player);
                }
            } else if (!held) {
                NvGun_Lower(); // let go before it was up
            }
            break;
        case NV_GUN_FIRE:
            if (NvLinkAnim_Done()) {
                if (held) {
                    sNvGunState = NV_GUN_AIM;
                    NvGun_Play(hold);
                } else {
                    NvGun_Lower();
                }
            }
            break;
    }
    if (sNvGunState == NV_GUN_NONE) {
        return;
    }
    // squared up: the stick turns him (RE1's 0x48 a step, at OoT's rate), or the aim does
    {
        Actor* target = NvGun_Target(play, player, item, sNvGunRange[CLAMP(item - 1, 0, 9)], true);
        s16 turn = (s16)(input->rel.stick_x * 0x6C0 / 60);

        if (ABS(input->rel.stick_x) > 20) {
            player->actor.shape.rot.y -= turn;
        } else if (target != NULL) {
            s32 ex, ez, r;
            f32 ahead, side;
            s16 yaw;

            NvEnemy_GunTarget(target, &ex, &ez, &r);
            NvGun_Relative(player, ex, ez, &ahead, &side, &yaw);
            Math_ScaledStepToS(&player->actor.shape.rot.y, yaw, 0x800);
        }
        player->actor.world.rot.y = player->yaw = player->actor.shape.rot.y;
    }
    player->actor.speedXZ = 0.0f;
    input->rel.stick_x = input->rel.stick_y = 0;
    input->cur.stick_x = input->cur.stick_y = 0;
    input->press.button &= ~(BTN_A | BTN_B | mask);
    input->cur.button &= ~(BTN_A | BTN_B | mask);
}

/** Link's left hand's matrix as his skeleton draws (z_player_lib.c Player_PostLimbDrawGameplay). */
void NvWeapon_NoteHand(void) {
    if (sNvGunState != NV_GUN_NONE) {
        Matrix_Get(&sNvGunHand);
        sNvGunHandSet = true;
    }
}

/**
 * After Link has drawn (z_player.c Player_Draw): the gun in his left fist, placed as Mod1 placed
 * its Beretta in his right (set by eye there, and it looked right in play: the model's own origin,
 * its y turned up, scale 0.28 for the Beretta, moved (276, 614, 50) and turned (0x4000, 0x4000, 0)
 * in the hand limb's space), mirrored across the hand for the left one. NV_GUN_FIT=
 * "x,y,z,rx,ry,rz,size%" and NV_GUN_MIRROR=x|y|z|none change it for checks.
 */
void NvWeapon_Draw(PlayState* play, Player* player) {
    static s32 sFit[7] = { 276, 614, 50, 0x4000, 0x4000, 0, 100 };
    static char sMirror = 'y';
    static s32 sFitRead;
    f32 sc;

    if (!sNvGunHandSet || (sNvGunState == NV_GUN_NONE) || (sNvGun.model == NULL)) {
        sNvGunHandSet = false;
        return;
    }
    sNvGunHandSet = false;
    if (!sFitRead) {
        sFitRead = true;
        if (getenv("NV_GUN_FIT") != NULL) {
            sscanf(getenv("NV_GUN_FIT"), "%d,%d,%d,%d,%d,%d,%d", &sFit[0], &sFit[1], &sFit[2], &sFit[3], &sFit[4],
                   &sFit[5], &sFit[6]);
        }
        if (getenv("NV_GUN_MIRROR") != NULL) {
            sMirror = getenv("NV_GUN_MIRROR")[0];
        }
    }
    // the Beretta's 0.28, the others by their length against its
    sc = 0.28f * sNvGunLength[CLAMP(sNvGun.item - 1, 0, 9)] / sNvGunLength[1] * sFit[6] / 100.0f;
    Matrix_Push();
    Matrix_Put(&sNvGunHand);
    Matrix_Scale((sMirror == 'x') ? -1.0f : 1.0f, (sMirror == 'y') ? -1.0f : 1.0f, (sMirror == 'z') ? -1.0f : 1.0f,
                 MTXMODE_APPLY); // the left hand: the right's place, mirrored
    Matrix_Translate(sFit[0], sFit[1], sFit[2], MTXMODE_APPLY);
    Matrix_RotateZYX(sFit[3], sFit[4], sFit[5], MTXMODE_APPLY);
    Matrix_Scale(sc, -sc, sc, MTXMODE_APPLY); // (Mod1's model had RE1's y turned up)
    NvModel_Draw(play->state.gfxCtx, sNvGun.model);
    Matrix_Pop();
}

/**
 * Its C button's icon while the RE1 weapon has it (z_parameter.c Interface_DrawItemIconTexture): RE1's
 * icon (40 x 30) in the button's square, at (x, y), w wide. True when drawn.
 */
s32 NvWeapon_DrawCIcon(PlayState* play, s16 x, s16 y, s16 w) {
    const u32* NvStatus_CIconFor(s32 id);
    const u32* cicon = NvStatus_CIconFor(NvGun_Equipped());
    const u16* icon = NvStatus_IconFor(NvGun_Equipped());
    s16 h = w * 3 / 4;

    if (cicon != NULL) { // (its own C-button icon: as OoT draws its item icons)
        OPEN_DISPS(play->state.gfxCtx);
        gDPLoadTextureBlock(OVERLAY_DISP++, cicon, G_IM_FMT_RGBA, G_IM_SIZ_32b, 32, 32, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                            G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gSPWideTextureRectangle(OVERLAY_DISP++, x << 2, y << 2, (x + w) << 2, (y + w) << 2, G_TX_RENDERTILE, 0, 0,
                                (32 << 10) / w, (32 << 10) / w);
        CLOSE_DISPS(play->state.gfxCtx);
        return true;
    }
    if (icon == NULL) {
        return false;
    }
    OPEN_DISPS(play->state.gfxCtx);
    gDPLoadTextureBlock(OVERLAY_DISP++, icon, G_IM_FMT_RGBA, G_IM_SIZ_16b, 40, 30, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gSPWideTextureRectangle(OVERLAY_DISP++, x << 2, (y + (w - h) / 2) << 2, (x + w) << 2, (y + (w - h) / 2 + h) << 2,
                            G_TX_RENDERTILE, 0, 0, (40 << 10) / w, (30 << 10) / h);
    CLOSE_DISPS(play->state.gfxCtx);
    return true;
}
