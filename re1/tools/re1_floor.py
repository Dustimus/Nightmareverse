"""A room's walkable floor and walls for SoH, from RE1's own rules.

RE1 has no floor geometry: the player keeps the height she came in at, the room's collision
records keep her out of things, and only scripted climbs change her height. So here:

- Collision (RDT+0x4C; RE1's PC code check_room_collision 0x0047d310, see
  ecruells/resident-evil-pc-decomp docs/RDT_FILE_FORMAT.md "Collision Boundary Data"): a
  24-byte header (split point, four quadrant record counts, unused), then 12-byte records
  u16 xMax, zMax, xMin, zMin, type, flags. Records straddling the split repeat in each
  quadrant. type & 0xFF is the shape: 1 a box, 3 an ellipse in the box, 5 a box skipped under
  collision flag 0x10 (stairwells: the player stands in them), 4 a soft zone (no push). 1 and 3
  push the player (the
  floor-step reading of records without flag 0x100 applies only to entities with collision
  flag 4, which the player never has).
- Floor: where the player can get to. A fine grid (CELL RE1 units) is flooded from the room's
  arrival points through cells clear of the pushing shapes, within the shapes' overall extent.
- Levels: the flooded area splits into the parts the shapes keep apart. Each takes the height
  of the doors that arrive in it, and climbs carry heights across: zones of type 5 (along x) and
  6 (along z) set by the script's Aot_set (0x0D) move the player 4350 along the axis, from the
  side she approached, and 2885 up, or down when (approached from the high side) xor (the
  zone's first data word) is 1 (player_door_open_sequence 0x00457390; rooms 5 and 0x0E: 2114
  and 1402).
- Walls: every face of every pushing shape that has floor beside it, and the floor's own outer
  edge (where the flood stopped at the extent), from each level's floor up WALL_HEIGHT, moved
  WALL_INSET into the shape (out past the edge) so Link stops where Jill does.
"""
import math
import struct

CELL = 100  # RE1 units (~2.2 OoT units)
WALL_HEIGHT = 30000  # RE1 needs only the player blocked; OoT throws things (bombs, arrows, hookshot)
CLIMB = (4350, 2885)
# Where the player stops: RE1 pushes Jill out to her radius (372, the SCA record's +10) plus 18
# past a shape's face (collision_push_rect); Link stops at his own wall radius (18 OoT units,
# adult) from an OoT wall. So the walls sit the difference inside RE1's faces, and Link's body
# stops where Jill's does.
JILL_STOP = 372 + 18
LINK_RADIUS = 18 / 0.0217
WALL_INSET = LINK_RADIUS - JILL_STOP
CLIMB_SHALLOW = (2114, 1402)  # rooms 0x05 and 0x0E of every stage


def collision_records(raw):
    """[(xmin, zmin, xmax, zmax, shape, flags)], each once."""
    o = struct.unpack_from("<I", raw, 0x4C)[0]
    counts = struct.unpack_from("<4i", raw, o + 4)
    out, seen = [], set()
    p = o + 24
    for _ in range(sum(counts)):
        xmax, zmax, xmin, zmin, t, fl = struct.unpack_from("<6H", raw, p)
        p += 12
        rec = (min(xmin, xmax), min(zmin, zmax), max(xmin, xmax), max(zmin, zmax), t & 0xFF, fl)
        if rec not in seen:
            seen.add(rec)
            out.append(rec)
    return out


def pushes(rec):
    """Whether a record keeps the player out. Shape 5 boxes don't: RE1 skips them under the
    entity's collision flag 0x10, and the player is placed inside them (207's stairwell, where
    the stairs from 10B arrive at (8600, 2885, 19000), lies in one)."""
    return rec[4] in (1, 3)


def inside(rec, x, z):
    x0, z0, x1, z1, shape, _ = rec
    if shape == 3:
        cx, cz, rx, rz = (x0 + x1) / 2, (z0 + z1) / 2, max((x1 - x0) / 2, 1), max((z1 - z0) / 2, 1)
        return ((x - cx) / rx) ** 2 + ((z - cz) / rz) ** 2 <= 1
    return x0 <= x <= x1 and z0 <= z <= z1


def climb_zones(aots):
    """aots: [(type, x, z, w, d, d0, sat)] from Aot_set; the climbs among them as (type, x, z, w, d, d0)."""
    return [tuple(a[:6]) for a in aots if a[0] in (5, 6)]


class Floor:
    def __init__(self, raw, room_number, arrivals, aots):
        """arrivals: [(x, z, y)] RE1 positions where the player enters (doors, start
        positions); room_number: the room's id within its stage (0x05, 0x0E are shallow)."""
        self.records = collision_records(raw)
        solid = [r for r in self.records if pushes(r)]
        xs = [r[0] for r in solid] + [r[2] for r in solid] + [a[0] for a in arrivals]
        zs = [r[1] for r in solid] + [r[3] for r in solid] + [a[1] for a in arrivals]
        self.gx0, self.gz0 = int(min(xs) // CELL), int(min(zs) // CELL)
        self.gx1, self.gz1 = int(max(xs) // CELL), int(max(zs) // CELL)

        def blocked(k):
            x, z = (k[0] + 0.5) * CELL, (k[1] + 0.5) * CELL
            return any(inside(r, x, z) for r in solid)

        # flood from every arrival, then split into the parts the shapes keep apart
        self.cells = set()
        comp = {}
        ncomp = 0
        starts = [self.cell(a[0], a[1]) for a in arrivals]
        dx_climb, _ = CLIMB_SHALLOW if room_number in (0x05, 0x0E) else CLIMB
        for zt, zx, zz, zw, zd, _d0 in climb_zones(aots):  # both ends of each climb
            cx, cz = zx + zw / 2, zz + zd / 2
            for s in (-1, 1):
                ex, ez = (cx + s * dx_climb / 2, cz) if zt == 5 else (cx, cz + s * dx_climb / 2)
                starts.append(self.cell(ex, ez))
        for st in starts:
            if st in comp or not self.in_extent(st) or blocked(st):
                continue
            stack = [st]
            comp[st] = ncomp
            while stack:
                k = stack.pop()
                self.cells.add(k)
                for n in ((k[0] + 1, k[1]), (k[0] - 1, k[1]), (k[0], k[1] + 1), (k[0], k[1] - 1)):
                    if n not in comp and self.in_extent(n) and not blocked(n):
                        comp[n] = ncomp
                        stack.append(n)
            ncomp += 1
        self.comp = comp

        # levels: arrivals first, then across climbs until nothing changes
        level = {}
        for x, z, y in arrivals:
            c = comp.get(self.cell(x, z))
            if c is not None and y and c not in level:
                level[c] = y
        for x, z, y in arrivals:
            c = comp.get(self.cell(x, z))
            if c is not None and c not in level:
                level[c] = y
        dx, dy = CLIMB_SHALLOW if room_number in (0x05, 0x0E) else CLIMB
        changed = True
        while changed:
            changed = False
            for zt, zx, zz, zw, zd, d0 in climb_zones(aots):
                cx, cz = zx + zw / 2, zz + zd / 2
                # from the low side the player moves +dx and climbs up unless d0 is odd
                lo = (cx - dx / 2, cz) if zt == 5 else (cx, cz - dx / 2)
                hi = (cx + dx / 2, cz) if zt == 5 else (cx, cz + dx / 2)
                clo, chi = comp.get(self.cell(*lo)), comp.get(self.cell(*hi))
                if clo is None or chi is None or clo == chi:
                    continue
                up_from_low = -dy if (d0 & 1) == 0 else dy  # RE1 y: negative is up
                if clo in level and chi not in level:
                    level[chi] = level[clo] + up_from_low
                    changed = True
                elif chi in level and clo not in level:
                    level[clo] = level[chi] - up_from_low
                    changed = True
        self.level = level

    def cell(self, x, z):
        return (int(x // CELL), int(z // CELL))

    def in_extent(self, k):
        return self.gx0 - 1 <= k[0] <= self.gx1 + 1 and self.gz0 - 1 <= k[1] <= self.gz1 + 1

    def height(self, x, z):
        """The floor's RE1 height at a point (0 where unknown)."""
        c = self.comp.get(self.cell(x, z))
        return self.level.get(c, 0)

    def mesh(self, cam_of, wall_surface, to_oot):
        """(verts, polys) for SoH: polys (surface, (a, b, c), unit normal, dist). cam_of(cell)
        -> the floor surface index for a cell."""
        verts, vindex, polys = [], {}, []

        def v(p):
            key = tuple(int(round(c)) for c in p)
            if key not in vindex:
                vindex[key] = len(verts)
                verts.append(key)
            return vindex[key]

        def tri(surface, a, b, c, want):
            ia, ib, ic = v(a), v(b), v(c)
            pa, pb, pc = verts[ia], verts[ib], verts[ic]
            u = [pb[i] - pa[i] for i in range(3)]
            w = [pc[i] - pa[i] for i in range(3)]
            n = [u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0]]
            ln = math.sqrt(sum(x * x for x in n))
            if ln == 0:
                return
            n = [x / ln for x in n]
            if n[0] * want[0] + n[1] * want[1] + n[2] * want[2] < 0:
                ib, ic = ic, ib
                n = [-x for x in n]
            polys.append((surface, (ia, ib, ic), n, int(round(-sum(n[i] * verts[ia][i] for i in range(3))))))

        def quad(surface, a, b, c, d, want):
            tri(surface, a, b, c, want)
            tri(surface, a, c, d, want)

        # floor: rows of cells merged into strips of one surface and level
        keyed = {k: (cam_of(k), self.level.get(self.comp[k], 0)) for k in self.cells}
        rows = {}
        for k in keyed:
            rows.setdefault(k[1], []).append(k[0])
        for gz, gxs in rows.items():
            gxs.sort()
            start = prev = gxs[0]
            for gx in gxs[1:] + [None]:
                if gx is not None and gx == prev + 1 and keyed[(gx, gz)] == keyed[(start, gz)]:
                    prev = gx
                    continue
                surf, y = keyed[(start, gz)]
                x0, x1, z0, z1 = start * CELL, (prev + 1) * CELL, gz * CELL, (gz + 1) * CELL
                quad(surf, to_oot((x0, y, z0)), to_oot((x1, y, z0)), to_oot((x1, y, z1)), to_oot((x0, y, z1)),
                     (0, 1, 0))
                if gx is not None:
                    start = prev = gx

        def wall(ax, az, bx, bz, y, want_x, want_z):
            quad(wall_surface, to_oot((ax, y, az)), to_oot((bx, y, bz)), to_oot((bx, y - WALL_HEIGHT, bz)),
                 to_oot((ax, y - WALL_HEIGHT, az)), (want_x, 0, -want_z))

        # the shapes' faces with floor beside them, at that floor's level
        for r in self.records:
            if not pushes(r):
                continue
            x0, z0, x1, z1, shape, _ = r
            cx, cz = (x0 + x1) / 2, (z0 + z1) / 2
            # the face moved in by the inset, down to the shape's middle line (a thin shape
            # still blocks)
            ix, iz = min(WALL_INSET, (x1 - x0) / 2), min(WALL_INSET, (z1 - z0) / 2)
            if shape == 3:
                rx, rz = (x1 - x0) / 2, (z1 - z0) / 2
                ring = [(math.cos(t * math.pi / 8), math.sin(t * math.pi / 8)) for t in range(16)]
                outer = [(cx + rx * c, cz + rz * sn) for c, sn in ring]
                inner = [(cx + (rx - ix) * c, cz + (rz - iz) * sn) for c, sn in ring]
            else:
                outer = [(x0, z0), (x1, z0), (x1, z1), (x0, z1)]
                inner = [(x0 + ix, z0 + iz), (x1 - ix, z0 + iz), (x1 - ix, z1 - iz), (x0 + ix, z1 - iz)]
            for i in range(len(outer)):
                (ax, az), (bx, bz) = outer[i], outer[(i + 1) % len(outer)]
                mx, mz = (ax + bx) / 2, (az + bz) / 2
                ox, oz = mx - cx, mz - cz
                ol = math.hypot(ox, oz) or 1.0
                beside = self.cell(mx + ox / ol * CELL, mz + oz / ol * CELL)
                if beside not in self.cells:
                    continue
                (ax, az), (bx, bz) = inner[i], inner[(i + 1) % len(inner)]
                wall(ax, az, bx, bz, self.level.get(self.comp[beside], 0), ox / ol, oz / ol)

        # the floor's outer edge
        for (gx, gz) in self.cells:
            y = self.level.get(self.comp[(gx, gz)], 0)
            for ddx, ddz in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                if (gx + ddx, gz + ddz) in self.cells:
                    continue
                if ddx:
                    ex = (gx + (ddx > 0)) * CELL + ddx * WALL_INSET
                    wall(ex, gz * CELL, ex, (gz + 1) * CELL, y, -ddx, 0)
                else:
                    ez = (gz + (ddz > 0)) * CELL + ddz * WALL_INSET
                    wall(gx * CELL, ez, (gx + 1) * CELL, ez, y, 0, -ddz)
        return verts, polys
