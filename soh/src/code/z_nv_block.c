/*
 * Nightmareverse: RE1's room objects made solid for Link (the crates, the statues, the cabinets).
 *
 * RE1 resolves the player out of each object's box every frame (RoomCollision.cpp ChkEntitySlide,
 * from update_room_objects); here each solid object is an OoT collision box instead, so Link
 * walks into it as into a wall, stands on it, and climbs onto it with A as onto any ledge (RE1's
 * climbable crates, flag 0x40: 1800 high, the climb check_climb_object starts). One actor per
 * object (z_nv_script.c NvObj_Update spawns it): its box is the object's (half extents x, y, z,
 * from its floor up twice the y one), it follows the object as it's pushed, and its collision is
 * off while the object is hidden or intangible.
 */
#include "global.h"
#include <math.h>
#include <string.h>

#define NV_RE1_SCALE 0.0217f // OoT units per RE1 unit (re1/tools/rdt2scene.py SCALE)
#define NV_BLOCK_SCALE 0.1f  // the actor's scale: its box is kept in tenths of OoT units

s32 NvObj_Box(s32 i, s16* pos, s16* ext, s32* solid, s32* climbable);

typedef struct {
    DynaPolyActor dyna;
    s32 obj;
    s32 off; // its collision switched off (the object hidden)
    CollisionHeader col;
    Vec3s verts[8];
    CollisionPoly polys[12];
    SurfaceType surface[1];
    CamData cam[1]; // CAM_SET_NONE: standing on it keeps the room's camera
} NvBlock;

const size_t gNvBlockSize = sizeof(NvBlock);

/** One face's two triangles (corners a, b, c, d in order around it), its normal outward. */
static void NvBlock_Face(NvBlock* this, s32 n, s32 a, s32 b, s32 c, s32 d, s32 axis, s32 sign) {
    s32 t;
    u16 tri[2][3] = { { a, b, c }, { a, c, d } };

    for (t = 0; t < 2; t++) {
        CollisionPoly* p = &this->polys[n + t];
        Vec3s* v = &this->verts[tri[t][0]];

        memset(p, 0, sizeof(*p));
        p->type = 0;
        p->flags_vIA = tri[t][0];
        p->flags_vIB = tri[t][1];
        p->vIC = tri[t][2];
        p->normal.x = (axis == 0) ? sign * 0x7FFF : 0;
        p->normal.y = (axis == 1) ? sign * 0x7FFF : 0;
        p->normal.z = (axis == 2) ? sign * 0x7FFF : 0;
        p->dist = -(s16)(sign * ((axis == 0) ? v->x : (axis == 1) ? v->y : v->z));
    }
}

/**
 * The box: x -ex..ex, y 0..h, z -ez..ez (the actor's units), its triangles wound outward. Only a
 * climbable one (RE1's flag 0x40, the crates) has a top: OoT climbs any ledge it can stand on,
 * and RE1's statues and cabinets can't be climbed.
 */
static void NvBlock_Build(NvBlock* this, f32 ex, f32 h, f32 ez, s32 climbable) {
    s32 i;

    for (i = 0; i < 8; i++) {
        this->verts[i].x = (s16)((i & 1) ? ex : -ex);
        this->verts[i].y = (s16)((i & 2) ? h : 0);
        this->verts[i].z = (s16)((i & 4) ? ez : -ez);
    }
    // (corners: bit 0 +x, bit 1 top, bit 2 +z)
    NvBlock_Face(this, 0, 1, 3, 7, 5, 0, 1);   // +x
    NvBlock_Face(this, 2, 0, 4, 6, 2, 0, -1);  // -x
    NvBlock_Face(this, 4, 4, 5, 7, 6, 2, 1);   // +z
    NvBlock_Face(this, 6, 0, 2, 3, 1, 2, -1);  // -z
    NvBlock_Face(this, 8, 0, 1, 5, 4, 1, -1);  // bottom
    if (climbable) {
        NvBlock_Face(this, 10, 2, 6, 7, 3, 1, 1); // top
    }
    memset(this->surface, 0, sizeof(this->surface));
    memset(&this->col, 0, sizeof(this->col));
    this->col.minBounds.x = (s16)-ex;
    this->col.minBounds.y = 0;
    this->col.minBounds.z = (s16)-ez;
    this->col.maxBounds.x = (s16)ex;
    this->col.maxBounds.y = (s16)h;
    this->col.maxBounds.z = (s16)ez;
    this->col.numVertices = 8;
    this->col.vtxList = this->verts;
    this->col.numPolygons = climbable ? 12 : 10;
    this->col.polyList = this->polys;
    this->col.surfaceTypeList = this->surface;
    memset(this->cam, 0, sizeof(this->cam));
    this->col.cameraDataList = this->cam;
    this->col.cameraDataListLen = 1;
}

static void NvBlock_Follow(NvBlock* this, PlayState* play) {
    s16 pos[3];
    s16 ext[3];
    s32 solid;
    s32 climbable;
    s32 there = NvObj_Box(this->obj, pos, ext, &solid, &climbable);

    if (!there) {
        Actor_Kill(&this->dyna.actor);
        return;
    }
    this->dyna.actor.world.pos.x = pos[0] * NV_RE1_SCALE;
    this->dyna.actor.world.pos.y = -pos[1] * NV_RE1_SCALE;
    this->dyna.actor.world.pos.z = -pos[2] * NV_RE1_SCALE;
    if (solid && this->off) {
        func_8003EC50(play, &play->colCtx.dyna, this->dyna.bgId);
        this->off = false;
    } else if (!solid && !this->off) {
        func_8003EBF8(play, &play->colCtx.dyna, this->dyna.bgId);
        this->off = true;
    }
}

void NvBlock_Init(Actor* thisx, PlayState* play) {
    NvBlock* this = (NvBlock*)thisx;
    s16 pos[3];
    s16 ext[3];
    s32 solid;
    s32 climbable;
    f32 k = NV_RE1_SCALE / NV_BLOCK_SCALE;

    this->obj = thisx->params;
    if (!NvObj_Box(this->obj, pos, ext, &solid, &climbable)) {
        Actor_Kill(thisx);
        return;
    }
    Actor_SetScale(thisx, NV_BLOCK_SCALE);
    DynaPolyActor_Init(&this->dyna, 0);
    NvBlock_Build(this, ext[0] * k, ext[1] * 2 * k, ext[2] * k, climbable);
    this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, thisx, &this->col);
    this->off = false;
    NvBlock_Follow(this, play);
}

void NvBlock_Destroy(Actor* thisx, PlayState* play) {
    NvBlock* this = (NvBlock*)thisx;

    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
}

void NvBlock_Update(Actor* thisx, PlayState* play) {
    NvBlock_Follow((NvBlock*)thisx, play);
}
