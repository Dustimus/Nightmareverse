/*
 * Nightmareverse: RE1's guns in Link's hands.
 *
 * In RE1's rooms the weapon equipped on RE1's status screen (z_nv_items.c) takes C-Left (its RE1
 * icon on the button; the OoT item there comes back outside the mansion). Holding C-Left squares
 * Link up as RE1's aim does (PlayerAnimations.cpp player_behavior_12_gun_aim): he stops, the stick
 * turns him on the spot and up/down aims high or low, and Jill's raise and hold for that weapon
 * and direction play on him (re1/tools/soh_weapons.py, mirrored: Link is left-handed; through
 * z_nv_linkanim.c). Releasing C-Left fires (B fires too and keeps the aim); a shot spends a round
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

s32 NvStatus_InMansion(PlayState* play);
s32 NvItems_Equipped(void);
s32 NvItems_Slot(s32 i, s32* qty);
void NvItems_SpendRound(s32 i);
const u16* NvStatus_IconFor(s32 id);
s32 NvLinkAnim_PlayFrom(const char* path, s32 skip, s32 anim, s32 hold, s32 mirror);
void NvLinkAnim_Stop(void);
s32 NvLinkAnim_Done(void);
s32 NvEnemy_GunTarget(Actor* a, s32* x, s32* z, s32* radius);
void NvEnemy_GunHit(Actor* a, s32 item);
void* NvModel_Parse(const u8* d);
void NvModel_Bounds(void* model, s16 mn[3], s16 mx[3]);
void NvModel_Draw(GraphicsContext* gfxCtx, void* model);
s32 NvClimb_Active(void);
s32 NvDoorScene_Covering(void);

typedef enum { NV_GUN_NONE, NV_GUN_RAISE, NV_GUN_AIM, NV_GUN_FIRE } NvGunState;
typedef enum { NV_AIM_LEVEL, NV_AIM_UP, NV_AIM_DOWN } NvAimDir;

// Jill's range with each weapon (WeaponDamage.cpp weapons_ranges, her half), by item id - 1
static const s16 sNvGunRange[10] = { 500, 1300, 2600, 800, 800, 400, 900, 900, 900, 1100 };

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

/** Whether the equipped RE1 weapon has C-Left here (z_player.c, z_parameter.c). */
s32 NvWeapon_OwnsCLeft(PlayState* play) {
    return NvStatus_InMansion(play) && (NvGun_Equipped() != 0);
}

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
static Actor* NvGun_Target(PlayState* play, Player* player, s32 reach, s32 autoAim) {
    Actor* best = NULL;
    f32 bestAhead = 1e9f;
    Actor* a;
    s32 ex, ez, r;

    if ((player->focusActor != NULL) && NvEnemy_GunTarget(player->focusActor, &ex, &ez, &r)) {
        return player->focusActor;
    }
    for (a = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; a != NULL; a = a->next) {
        f32 ahead, side;
        s16 yaw;

        if (!NvEnemy_GunTarget(a, &ex, &ez, &r)) {
            continue;
        }
        NvGun_Relative(player, ex, ez, &ahead, &side, &yaw);
        if (autoAim) { // RE1's reticle: ahead within about 45 degrees and in reach
            if ((ahead <= 0.0f) || (ahead > reach + r) || (fabsf(side) > ahead + r)) {
                continue;
            }
        } else if ((ahead <= 0.0f) || (ahead > reach + r) || (fabsf(side) > r + 200)) {
            continue; // the shot: in front, within its radius + 200 to either side
        }
        if (ahead < bestAhead) {
            bestAhead = ahead;
            best = a;
        }
    }
    return best;
}

/** A shot (apply_weapon_damage): spend a round, hit what's in the line. */
static void NvGun_Fire(PlayState* play, Player* player) {
    s32 slot = NvItems_Equipped();
    s32 qty = 0;
    s32 item = sNvGun.item;
    Actor* hit;
    s32 raise, fire, hold;

    NvItems_Slot(slot, &qty);
    if ((item != 1) && (qty <= 0)) { // empty: the click, no shot
        Audio_PlaySfxGeneral(NA_SE_SY_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
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
    Audio_PlayActorSound2(&player->actor, (item == 1) ? NA_SE_IT_SWORD_SWING : NA_SE_IT_SLING_SHOT);
    hit = NvGun_Target(play, player, sNvGunRange[CLAMP(item - 1, 0, 9)], false);
    printf("NVGUN fire weapon %d dir %d rounds %d -> %s\n", (int)item, (int)sNvGunDir, (int)(qty - 1),
           (hit != NULL) ? "hit" : "miss");
    fflush(stdout);
    if (hit != NULL) {
        Vec3f at = hit->focus.pos;

        NvEnemy_GunHit(hit, item);
        EffectSsHitMark_SpawnFixedScale(play, EFFECT_HITMARK_WHITE, &at);
    }
}

/**
 * Each frame, before Link acts on the input (z_player.c Player_Update, on its copy of it): RE1's
 * aim and fire on C-Left; while the gun is up the stick turns him and he doesn't walk or swing.
 */
void NvWeapon_PreUpdate(Player* player, PlayState* play, Input* input) {
    s32 item = NvWeapon_OwnsCLeft(play) ? NvGun_Equipped() : 0;
    s32 held = CHECK_BTN_ALL(input->cur.button, BTN_CLEFT);
    s32 raise, fire, hold;
    s32 dir;

    // only when Link could act: on the ground, free, nothing else going on
    if ((item == 0) || !NvGun_Load(item) || (player->stateFlags1 & (PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_IN_WATER |
                                                                      PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_DEAD)) ||
        (player->stateFlags2 & PLAYER_STATE2_GRABBED_BY_ENEMY) || !(player->actor.bgCheckFlags & 1) ||
        (play->msgCtx.msgMode != MSGMODE_NONE) || NvClimb_Active() || NvDoorScene_Covering() ||
        (play->transitionTrigger != TRANS_TRIGGER_OFF)) {
        NvGun_Lower();
        return;
    }
    // another item button while the gun is up: that item instead (the gun goes away)
    if ((sNvGunState != NV_GUN_NONE) && CHECK_BTN_ANY(input->press.button, BTN_CDOWN | BTN_CRIGHT)) {
        NvGun_Lower();
        return;
    }
    if ((sNvGunState == NV_GUN_NONE) && !held) {
        return;
    }
    // the aim's direction: up/down on the stick, as RE1's D-pad
    dir = (input->rel.stick_y > 45) ? NV_AIM_UP : (input->rel.stick_y < -45) ? NV_AIM_DOWN : NV_AIM_LEVEL;
    NvGun_Anims(item, dir, &raise, &fire, &hold);

    switch (sNvGunState) {
        case NV_GUN_NONE: // C-Left: the gun comes up
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
        Actor* target = NvGun_Target(play, player, sNvGunRange[CLAMP(item - 1, 0, 9)], true);
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
    input->press.button &= ~(BTN_A | BTN_B | BTN_CLEFT);
    input->cur.button &= ~(BTN_A | BTN_B | BTN_CLEFT);
}

/** Link's left hand's matrix as his skeleton draws (z_player_lib.c Player_PostLimbDrawGameplay). */
void NvWeapon_NoteHand(void) {
    if (sNvGunState != NV_GUN_NONE) {
        Matrix_Get(&sNvGunHand);
        sNvGunHandSet = true;
    }
}

/**
 * After Link has drawn (z_player.c Player_Draw): the gun in his left fist. Its place in the fist
 * is set by eye; NV_GUN_FIT="x,y,z,rx,ry,rz,size%" (hand units, binary angles) moves it for checks.
 */
void NvWeapon_Draw(PlayState* play, Player* player) {
    static s32 sFit[7] = { 600, 300, 0, 0, 0, 0, 100 }; // ..., size in percent
    static s32 sFitRead;

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
    }
    if (getenv("NV_LOG_GUN") != NULL) {
        printf("NVGUN draw hand %.1f %.1f %.1f scale %.4f\n", sNvGunHand.xw, sNvGunHand.yw, sNvGunHand.zw,
               sNvGun.scale);
        fflush(stdout);
    }
    Matrix_Push();
    Matrix_Put(&sNvGunHand);
    Matrix_Translate(sFit[0], sFit[1], sFit[2], MTXMODE_APPLY);
    Matrix_RotateZYX(sFit[3], sFit[4], sFit[5], MTXMODE_APPLY);
    Matrix_Scale(sNvGun.scale * sFit[6] / 100.0f, sNvGun.scale * sFit[6] / 100.0f, sNvGun.scale * sFit[6] / 100.0f,
                 MTXMODE_APPLY);
    Matrix_Translate(-sNvGun.center[0], -sNvGun.center[1], -sNvGun.center[2], MTXMODE_APPLY);
    NvModel_Draw(play->state.gfxCtx, sNvGun.model);
    Matrix_Pop();
}

/**
 * C-Left's icon while the RE1 weapon has it (z_parameter.c Interface_DrawItemIconTexture): RE1's
 * icon (40 x 30) in the button's square, at (x, y), w wide. True when drawn.
 */
s32 NvWeapon_DrawCIcon(PlayState* play, s16 x, s16 y, s16 w) {
    const u16* icon = NvStatus_IconFor(NvGun_Equipped());
    s16 h = w * 3 / 4;

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
