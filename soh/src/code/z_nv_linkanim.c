/*
 * Nightmareverse: RE1's player animations played on Link.
 *
 * RE1 keeps the player's side of an enemy's grabs in that enemy's EMD (its first section: the
 * player's skeleton and animations, which RE1 plays through emdScratchPtr; PlayerAnimations.cpp).
 * re1/tools/soh_enemy.py writes them to "re1/enemies/em<type>_pl". This plays one on Link by
 * matching directions, not raw angles (the skeletons differ in rest pose and proportions): for each
 * frame it poses Jill's skeleton, then turns Link's waist so his body faces as hers does (hips and
 * up axis), and each of his thighs, shins, upper arms and forearms so the limb points along her
 * matching bone. Link's own pose this frame supplies everything else (twist, hands, feet, head),
 * so it overrides only what RE1 animates.
 *
 * Jill's skeleton (as the EMD has it): 0 chest (the root), 1 head, 2 pelvis; 3-5 and 6-8 the legs
 * (hip, knee, ankle); 9-11 and 12-14 the arms (shoulder, elbow, wrist).
 */
#include "global.h"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define NV_LA_MAX_BONES 24
#define NV_LA_MAX_ANIMS 32
#define NV_LA_PI 3.14159265f

typedef struct {
    f32 m[3][3]; // m[row][col], acting on column vectors
} NvMat3;

typedef struct {
    const u8* blob;
    s32 numBones;
    s16 relpos[NV_LA_MAX_BONES][3];
    s16 parent[NV_LA_MAX_BONES];
    const s16* frames;
    s32 frameShorts;
    s32 numFrames;
    s32 numAnims;
    const u16* steps[NV_LA_MAX_ANIMS];
    s32 numSteps[NV_LA_MAX_ANIMS];
} NvLaSource;

static NvLaSource sNvLa;
static s32 sNvLaType = -1;
static s32 sNvLaActive;
static s32 sNvLaAnim;
static s32 sNvLaStep;
static s32 sNvLaHold; // stop at the last step instead of looping
static u8 sNvLaPhase;

/* ---- small 3x3 math ---- */

static NvMat3 NvLa_Mul(const NvMat3* a, const NvMat3* b) {
    NvMat3 r;
    s32 i;
    s32 j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            r.m[i][j] = a->m[i][0] * b->m[0][j] + a->m[i][1] * b->m[1][j] + a->m[i][2] * b->m[2][j];
        }
    }
    return r;
}

static NvMat3 NvLa_T(const NvMat3* a) {
    NvMat3 r;
    s32 i;
    s32 j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            r.m[i][j] = a->m[j][i];
        }
    }
    return r;
}

static void NvLa_Apply(const NvMat3* a, const f32* v, f32* out) {
    s32 i;

    for (i = 0; i < 3; i++) {
        out[i] = a->m[i][0] * v[0] + a->m[i][1] * v[1] + a->m[i][2] * v[2];
    }
}

/** Rz * Ry * Rx, angles in radians: OoT's Matrix_RotateZYX and the GTE's RotMatrix alike. */
static NvMat3 NvLa_RotZYX(f32 x, f32 y, f32 z) {
    f32 sx = sinf(x), cx = cosf(x), sy = sinf(y), cy = cosf(y), sz = sinf(z), cz = cosf(z);
    NvMat3 r;

    r.m[0][0] = cz * cy;
    r.m[0][1] = cz * sy * sx - sz * cx;
    r.m[0][2] = cz * sy * cx + sz * sx;
    r.m[1][0] = sz * cy;
    r.m[1][1] = sz * sy * sx + cz * cx;
    r.m[1][2] = sz * sy * cx - cz * sx;
    r.m[2][0] = -sy;
    r.m[2][1] = cy * sx;
    r.m[2][2] = cy * cx;
    return r;
}

/** The inverse: binangs for a jointTable entry. */
static Vec3s NvLa_ToZYX(const NvMat3* r) {
    Vec3s out;
    f32 sy = -r->m[2][0];
    f32 y;
    f32 x;
    f32 z;

    sy = CLAMP(sy, -1.0f, 1.0f);
    y = asinf(sy);
    if (fabsf(sy) < 0.9999f) {
        x = atan2f(r->m[2][1], r->m[2][2]);
        z = atan2f(r->m[1][0], r->m[0][0]);
    } else { // gimbal lock: put it all in x
        x = atan2f(-r->m[1][2], r->m[1][1]);
        z = 0.0f;
    }
    out.x = (s16)(x * (32768.0f / NV_LA_PI));
    out.y = (s16)(y * (32768.0f / NV_LA_PI));
    out.z = (s16)(z * (32768.0f / NV_LA_PI));
    return out;
}

static f32 NvLa_Norm(f32* v) {
    f32 l = sqrtf(SQ(v[0]) + SQ(v[1]) + SQ(v[2]));

    if (l > 1e-6f) {
        v[0] /= l, v[1] /= l, v[2] /= l;
    }
    return l;
}

static void NvLa_Cross(const f32* a, const f32* b, f32* out) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

/** The smallest rotation taking direction a to direction b (Rodrigues). */
static NvMat3 NvLa_Between(const f32* a0, const f32* b0) {
    f32 a[3] = { a0[0], a0[1], a0[2] };
    f32 b[3] = { b0[0], b0[1], b0[2] };
    f32 k[3];
    f32 c;
    f32 s;
    NvMat3 r;
    s32 i;
    s32 j;

    NvLa_Norm(a);
    NvLa_Norm(b);
    NvLa_Cross(a, b, k);
    s = NvLa_Norm(k);
    c = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    if (s < 1e-5f) { // parallel (or opposite: rare for a limb, left as is)
        memset(&r, 0, sizeof(r));
        r.m[0][0] = r.m[1][1] = r.m[2][2] = 1.0f;
        return r;
    }
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            r.m[i][j] = (1.0f - c) * k[i] * k[j] + ((i == j) ? c : 0.0f);
        }
    }
    r.m[0][1] -= s * k[2], r.m[1][0] += s * k[2];
    r.m[0][2] += s * k[1], r.m[2][0] -= s * k[1];
    r.m[1][2] -= s * k[0], r.m[2][1] += s * k[0];
    return r;
}

/** The rotation taking frame (up, right) to frame (up2, right2): up exactly, right as near as it can. */
static NvMat3 NvLa_Frame(const f32* up, const f32* right, const f32* up2, const f32* right2) {
    f32 a[3][3];
    f32 b[3][3];
    NvMat3 ma;
    NvMat3 mb;
    s32 i;

    for (i = 0; i < 3; i++) {
        a[1][i] = up[i], b[1][i] = up2[i];
        a[0][i] = right[i], b[0][i] = right2[i];
    }
    NvLa_Norm(a[1]);
    NvLa_Norm(b[1]);
    NvLa_Cross(a[0], a[1], a[2]); // forward = right x up
    NvLa_Cross(b[0], b[1], b[2]);
    NvLa_Norm(a[2]);
    NvLa_Norm(b[2]);
    NvLa_Cross(a[1], a[2], a[0]); // right again, square to up
    NvLa_Cross(b[1], b[2], b[0]);
    for (i = 0; i < 3; i++) { // columns: right, up, forward
        ma.m[i][0] = a[0][i], ma.m[i][1] = a[1][i], ma.m[i][2] = a[2][i];
        mb.m[i][0] = b[0][i], mb.m[i][1] = b[1][i], mb.m[i][2] = b[2][i];
    }
    ma = NvLa_T(&ma);
    return NvLa_Mul(&mb, &ma);
}

/* ---- the source: an enemy's player animations ---- */

static s32 NvLa_Load(s32 type) {
    char path[64];
    const u8* d;
    u16 n;
    u16 nf;
    u16 na;
    s32 i;

    if (type == sNvLaType) {
        return sNvLa.blob != NULL;
    }
    sNvLaType = type;
    memset(&sNvLa, 0, sizeof(sNvLa));
    snprintf(path, sizeof(path), "re1/enemies/em%d_pl", (int)type);
    if (!ResourceMgr_FileExists(path)) {
        return false;
    }
    d = (const u8*)ResourceGetDataByName(path);
    if ((d == NULL) || (memcmp(d, "RPLA", 4) != 0)) {
        return false;
    }
    memcpy(&n, d + 4, 2);
    if (n > NV_LA_MAX_BONES) {
        return false;
    }
    sNvLa.numBones = n;
    d += 6;
    for (i = 0; i < n; i++) {
        memcpy(sNvLa.relpos[i], d, 6);
        memcpy(&sNvLa.parent[i], d + 6, 2);
        d += 8;
    }
    memcpy(&nf, d, 2);
    d += 2;
    sNvLa.frameShorts = 6 + 3 * n;
    sNvLa.numFrames = nf;
    sNvLa.frames = (const s16*)d;
    d += nf * sNvLa.frameShorts * 2;
    memcpy(&na, d, 2);
    d += 2;
    sNvLa.numAnims = MIN(na, NV_LA_MAX_ANIMS);
    for (i = 0; i < sNvLa.numAnims; i++) {
        u16 ns;

        memcpy(&ns, d, 2);
        d += 2;
        sNvLa.numSteps[i] = ns;
        sNvLa.steps[i] = (const u16*)d;
        d += ns * 4;
    }
    sNvLa.blob = d;
    return true;
}

/**
 * Jill's joint positions for a frame, in Link's model space: RE1's x (her forward) is Link's z, her
 * y (down) his -y, her z his x.
 */
static void NvLa_Pose(s32 frame, f32 pos[NV_LA_MAX_BONES][3]) {
    const s16* f = sNvLa.frames + frame * sNvLa.frameShorts;
    NvMat3 world[NV_LA_MAX_BONES];
    f32 re1[NV_LA_MAX_BONES][3];
    s32 done[NV_LA_MAX_BONES] = { 0 };
    s32 pass;
    s32 i;

    for (pass = 0; pass < sNvLa.numBones; pass++) { // parents first, whatever the order
        for (i = 0; i < sNvLa.numBones; i++) {
            const s16* ang = f + 6 + 3 * i;
            NvMat3 local;
            s32 p = sNvLa.parent[i];

            if (done[i] || ((p >= 0) && !done[p])) {
                continue;
            }
            local = NvLa_RotZYX(ang[0] * (NV_LA_PI / 2048.0f), ang[1] * (NV_LA_PI / 2048.0f),
                                ang[2] * (NV_LA_PI / 2048.0f));
            if (p < 0) {
                world[i] = local;
                re1[i][0] = f[0], re1[i][1] = f[1], re1[i][2] = f[2];
            } else {
                f32 rel[3] = { sNvLa.relpos[i][0], sNvLa.relpos[i][1], sNvLa.relpos[i][2] };
                f32 off[3];

                world[i] = NvLa_Mul(&world[p], &local);
                NvLa_Apply(&world[p], rel, off);
                re1[i][0] = re1[p][0] + off[0], re1[i][1] = re1[p][1] + off[1], re1[i][2] = re1[p][2] + off[2];
            }
            done[i] = true;
        }
    }
    for (i = 0; i < sNvLa.numBones; i++) {
        pos[i][0] = re1[i][2];
        pos[i][1] = -re1[i][1];
        pos[i][2] = re1[i][0];
    }
}

/* ---- playing ---- */

/** Start (or switch to) an enemy type's player animation anim; hold: stop at its end. */
s32 NvLinkAnim_Play(s32 type, s32 anim, s32 hold) {
    if (!NvLa_Load(type) || (anim < 0) || (anim >= sNvLa.numAnims)) {
        sNvLaActive = false;
        return false;
    }
    if (!sNvLaActive || (anim != sNvLaAnim)) {
        sNvLaStep = 0;
    }
    sNvLaActive = true;
    sNvLaAnim = anim;
    sNvLaHold = hold;
    return true;
}

void NvLinkAnim_Stop(void) {
    sNvLaActive = false;
}

s32 NvLinkAnim_Playing(void) {
    return sNvLaActive;
}

/** True once a held animation has reached its last step. */
s32 NvLinkAnim_Done(void) {
    return !sNvLaActive || (sNvLaHold && (sNvLaStep >= sNvLa.numSteps[sNvLaAnim] - 1));
}

// Link's limbs that follow a Jill bone: the limb's segment (to its child) along Jill's bone a -> b
typedef struct {
    u8 limb; // jointTable index (PLAYER_LIMB_*)
    u8 child;
    u8 a;
    u8 b;
} NvLaSegment;

static const NvLaSegment sNvLaSegments[] = {
    { PLAYER_LIMB_R_THIGH, PLAYER_LIMB_R_SHIN, 3, 4 },
    { PLAYER_LIMB_R_SHIN, PLAYER_LIMB_R_FOOT, 4, 5 },
    { PLAYER_LIMB_L_THIGH, PLAYER_LIMB_L_SHIN, 6, 7 },
    { PLAYER_LIMB_L_SHIN, PLAYER_LIMB_L_FOOT, 7, 8 },
    { PLAYER_LIMB_R_SHOULDER, PLAYER_LIMB_R_FOREARM, 9, 10 },
    { PLAYER_LIMB_R_FOREARM, PLAYER_LIMB_R_HAND, 10, 11 },
    { PLAYER_LIMB_L_SHOULDER, PLAYER_LIMB_L_FOREARM, 12, 13 },
    { PLAYER_LIMB_L_FOREARM, PLAYER_LIMB_L_HAND, 13, 14 },
};

/**
 * After Link's own animation has posed him this frame (z_player.c, the end of Player_Update):
 * advance RE1's animation (its 30 Hz steps, three every two frames) and turn the limbs it animates.
 */
void NvLinkAnim_Apply(Player* player, PlayState* play) {
    static u8 sPhase;
    Vec3s* jt = player->skelAnime.jointTable;
    void** limbs = player->skelAnime.skeleton;
    s32 n = player->skelAnime.limbCount;
    s32 parent[PLAYER_LIMB_MAX];
    NvMat3 ref[PLAYER_LIMB_MAX];
    NvMat3 world[PLAYER_LIMB_MAX];
    f32 refPos[PLAYER_LIMB_MAX][3];
    f32 jill[NV_LA_MAX_BONES][3];
    s32 frame;
    s32 i;
    s32 k;

    if (!sNvLaActive || (jt == NULL) || (limbs == NULL) || (n + 1 > PLAYER_LIMB_MAX) || (sNvLa.numBones < 15)) {
        return;
    }
    // RE1's step for this frame
    {
        s32 ticks = (sPhase ^= 1) ? 1 : 2;
        s32 last = sNvLa.numSteps[sNvLaAnim] - 1;

        sNvLaStep += ticks;
        if (sNvLaStep > last) {
            sNvLaStep = sNvLaHold ? last : (sNvLaStep % (last + 1));
        }
        frame = sNvLa.steps[sNvLaAnim][sNvLaStep * 2];
        if (frame >= sNvLa.numFrames) {
            return;
        }
    }
    NvLa_Pose(frame, jill);

    // Link's skeleton: parents (limb i is jointTable[i + 1]; the skeleton lists parents first)
    for (i = 0; i < PLAYER_LIMB_MAX; i++) {
        parent[i] = -1;
    }
    for (i = 0; i < n; i++) {
        LodLimb* l = (LodLimb*)limbs[i];
        s32 c = l->child;

        while (c != 0xFF) {
            parent[c + 1] = i + 1;
            c = ((LodLimb*)limbs[c])->sibling;
        }
    }
    // his pose as his own animation left it: world rotations and joint positions (model space)
    for (i = 1; i <= n; i++) {
        LodLimb* l = (LodLimb*)limbs[i - 1];
        NvMat3 local = NvLa_RotZYX(jt[i].x * (NV_LA_PI / 32768.0f), jt[i].y * (NV_LA_PI / 32768.0f),
                                   jt[i].z * (NV_LA_PI / 32768.0f));
        s32 p = parent[i];

        if (p < 0) {
            ref[i] = local;
            refPos[i][0] = refPos[i][1] = refPos[i][2] = 0.0f;
        } else {
            f32 jp[3] = { l->jointPos.x, l->jointPos.y, l->jointPos.z };
            f32 off[3];

            ref[i] = NvLa_Mul(&ref[p], &local);
            NvLa_Apply(&ref[p], jp, off);
            for (k = 0; k < 3; k++) {
                refPos[i][k] = refPos[p][k] + off[k];
            }
        }
    }

    // the hips' height: hers above the floor, scaled by leg length, against his standing height
    {
        LodLimb* shin = (LodLimb*)limbs[PLAYER_LIMB_R_SHIN - 1];
        LodLimb* foot = (LodLimb*)limbs[PLAYER_LIMB_R_FOOT - 1];
        f32 legL = sqrtf(SQ(shin->jointPos.x) + SQ(shin->jointPos.y) + SQ(shin->jointPos.z)) +
                   sqrtf(SQ(foot->jointPos.x) + SQ(foot->jointPos.y) + SQ(foot->jointPos.z));
        f32 legJ = sqrtf(SQ(sNvLa.relpos[4][0]) + SQ(sNvLa.relpos[4][1]) + SQ(sNvLa.relpos[4][2])) +
                   sqrtf(SQ(sNvLa.relpos[5][0]) + SQ(sNvLa.relpos[5][1]) + SQ(sNvLa.relpos[5][2]));

        if (legJ > 1.0f) {
            f32 drop = (jill[2][1] - legJ) * (legL / legJ); // below standing: negative
            jt[0].y = (s16)CLAMP(jt[0].y + MIN(drop, 0.0f), -32768.0f, 32767.0f);
        }
    }

    // the new pose, parents first: the waist faces as her body does, the limbs point as hers
    for (i = 1; i <= n; i++) {
        s32 p = parent[i];
        NvMat3 local = NvLa_RotZYX(jt[i].x * (NV_LA_PI / 32768.0f), jt[i].y * (NV_LA_PI / 32768.0f),
                                   jt[i].z * (NV_LA_PI / 32768.0f));
        NvMat3 w = (p < 0) ? local : NvLa_Mul(&world[p], &local);

        if (i == PLAYER_LIMB_WAIST) {
            // up: from between the hips to the head; right: from one hip to the other
            f32 upL[3];
            f32 rightL[3];
            f32 upJ[3];
            f32 rightJ[3];
            NvMat3 q;
            NvMat3 wRef = (p < 0) ? local : NvLa_Mul(&ref[p], &local);
            NvMat3 fix;

            for (k = 0; k < 3; k++) {
                upL[k] = refPos[PLAYER_LIMB_HEAD][k] - (refPos[PLAYER_LIMB_R_THIGH][k] + refPos[PLAYER_LIMB_L_THIGH][k]) / 2;
                rightL[k] = refPos[PLAYER_LIMB_R_THIGH][k] - refPos[PLAYER_LIMB_L_THIGH][k];
                upJ[k] = jill[1][k] - (jill[3][k] + jill[6][k]) / 2;
                rightJ[k] = jill[3][k] - jill[6][k];
            }
            q = NvLa_Frame(upL, rightL, upJ, rightJ);
            // as his reference waist, carried by q; then from the current parent instead
            fix = NvLa_Mul(&q, &wRef);
            w = fix;
        } else {
            for (k = 0; k < (s32)ARRAY_COUNT(sNvLaSegments); k++) {
                const NvLaSegment* s = &sNvLaSegments[k];

                if (s->limb == i) {
                    LodLimb* c = (LodLimb*)limbs[s->child - 1];
                    f32 jp[3] = { c->jointPos.x, c->jointPos.y, c->jointPos.z };
                    f32 v[3];
                    f32 d[3] = { jill[s->b][0] - jill[s->a][0], jill[s->b][1] - jill[s->a][1],
                                 jill[s->b][2] - jill[s->a][2] };
                    NvMat3 r;

                    NvLa_Apply(&w, jp, v);
                    r = NvLa_Between(v, d);
                    w = NvLa_Mul(&r, &w);
                    break;
                }
            }
        }
        world[i] = w;
        if (p < 0) {
            jt[i] = NvLa_ToZYX(&w);
        } else {
            NvMat3 pt = NvLa_T(&world[p]);
            NvMat3 l = NvLa_Mul(&pt, &w);

            jt[i] = NvLa_ToZYX(&l);
        }
    }
}
