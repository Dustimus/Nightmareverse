"""MM's get-items, all of them at once: what each gives and how it's drawn over Link's head.

MM gives items through a table (z_player.c sGetItemTable: per get-item id its item, object, model
id, text and chest animation) and draws them with another (z_draw.c sDrawItemTable: per model id a
draw routine and its display lists). Both are read here from the decomp and written out:
  soh/src/code/nv_mm_getitems.inc   both tables as C, with MM's names as MM's numbers and the
                                    display lists as our resource paths (z_nv_mm_getitem.c)
  mm/tools/mm_getitem_objects.py    the get-item objects to convert (their ROM files and display
                                    lists, from the decomp's asset XMLs), for mm_objects.py
Both are committed: the decomp is only needed to make them again.

    mm_getitems.py <mm decomp> <mm rom>
"""

import os
import re
import struct
import sys
import xml.etree.ElementTree as ET

sys.path.insert(0, os.path.dirname(__file__))
REPO = os.path.join(os.path.dirname(__file__), "..", "..")


def enum_values(path, prefix):
    out = {}
    for m in re.finditer(r"/\*\s*(-?0x[0-9A-Fa-f]+|-?\d+)\s*\*/\s*(" + prefix + r"\w*)\s*[,=]", open(path).read()):
        out.setdefault(m.group(2), int(m.group(1), 0))
    return out


def object_files(decomp, rom):
    """MM's object id names (OBJECT_GI_RUPY) -> (object_gi_rupy, ROM file index), by MM's object
    table in the code file (located by object_nb's file, 1027 at id 4)."""
    import mm_objects
    names = {}
    for m in re.finditer(r"/\* 0x(\w+) \*/ DEFINE_OBJECT\w*\((\w+), (\w+)\)",
                         open(os.path.join(decomp, "include", "tables", "object_table.h")).read()):
        names[m.group(3)] = (int(m.group(1), 16), m.group(2))
    d, dma = rom.data, rom.release["dmadata"]
    by_vrom = {}
    for i in range(1600):
        vs, ve = struct.unpack_from(">2I", d, dma + 16 * i)
        if ve:
            by_vrom[vs] = i
    code = mm_objects._load(rom, 31)[0]
    base = code.find(struct.pack(">I", rom.entry_index(1027)[0])) - 8 * 4
    out = {}
    for name, (oid, sym) in names.items():
        vs = struct.unpack_from(">I", code, base + 8 * oid)[0]
        if vs in by_vrom:
            out[sym] = by_vrom[vs]  # object_gi_rupy -> its file
    return out


def symbol_objects(decomp):
    """Every symbol in MM's object XMLs -> (object, tag, offset)."""
    d = os.path.join(decomp, "assets", "xml", "N64_US", "objects")
    out = {}
    for fn in os.listdir(d):
        if fn.endswith(".xml"):
            for el in ET.parse(os.path.join(d, fn)).getroot().iter():
                if "Name" in el.attrib and "Offset" in el.attrib:
                    out.setdefault(el.attrib["Name"], (fn[:-4], el.tag, int(el.attrib["Offset"], 16)))
    return out


def xml_dlists(decomp, obj):
    path = os.path.join(decomp, "assets", "xml", "N64_US", "objects", obj + ".xml")
    out = {}
    for el in ET.parse(path).getroot().iter():
        if el.tag in ("DList", "Mtx", "TextureAnimation") and "Name" in el.attrib:
            out[el.attrib["Name"]] = (el.tag, int(el.attrib["Offset"], 16))
    return out


def main():
    decomp, rom_path = sys.argv[1:]
    import mm_rom
    rom = mm_rom.MMRom(rom_path)
    inc = os.path.join(decomp, "include")
    items = enum_values(os.path.join(inc, "z64item.h"), "ITEM_")
    gids = enum_values(os.path.join(inc, "z64item.h"), "GID_")
    gis = enum_values(os.path.join(inc, "z64item.h"), "GI_")
    files = object_files(decomp, rom)
    where = symbol_objects(decomp)

    # the draw table: per model id, a routine and its resources (and the object they're in)
    draw = open(os.path.join(decomp, "src", "code", "z_draw.c")).read()
    body = draw[draw.index("sDrawItemTable[] = {"):]
    body = body[:body.index("\n};")]
    entries = []
    for chunk in re.split(r"\n\s*// ", body)[1:]:  # one per model id, in order
        head, rest = chunk.split("\n", 1)
        m = re.search(r"\{\s*(GetItem_\w+),\s*\{([^}]*)\}", rest, re.S)
        entries.append((head.split(",")[0].strip(), None, m.group(1), m.group(2)))
    objects = {}  # object name -> (file, {dl symbol: offset})
    draw_rows = []
    for gid, _comment, func, res in entries:
        paths = []
        for r in [x.strip().lstrip("&") for x in res.split(",") if x.strip()]:
            if r == "NULL":
                paths.append("NULL")
                continue
            obj, kind, off = where[r]
            if kind != "DList":  # (a matrix: the seahorse's and the fairy's billboards; drawn without)
                paths.append("NULL")
                continue
            objects.setdefault(obj, (files[obj], {}))[1][r] = off
            paths.append(f'"__OTR__mm/objects/{obj}/{r}"')
        draw_rows.append(f"    /* {gid} */ {{ {func.replace('GetItem_', 'NvMmGetItem_')}, {{ {', '.join(paths)} }} }},")
    for sym in ("gGiFairyBottleTexAnim", "gGiMoonsTearTexAnim"):  # the animated materials two routines draw
        obj, _kind, off = where[sym]
        objects.setdefault(obj, (files[obj], {}))[1].setdefault("@matanim", {})[sym] = off

    # the get-item table
    player = open(os.path.join(decomp, "src", "overlays", "actors", "ovl_player_actor", "z_player.c")).read()
    tb = player[player.index("GetItemEntry sGetItemTable[GI_MAX - 1] = {"):]
    tb = tb[:tb.index("\n};")]
    gi_rows = []
    tb = tb.split("// #region 2S2H")[0]  # (the port's own additions after MM's table: not MM's)
    item00 = {}
    for root, _d, fs in os.walk(os.path.join(decomp, "src", "overlays", "actors", "ovl_En_Item00")):
        for fn in fs:
            if fn.endswith(".h"):
                item00.update(enum_values(os.path.join(root, fn), "ITEM00_"))
    flags = {"GIFIELD_20": 0x20, "GIFIELD_40": 0x40, "GIFIELD_NO_COLLECTIBLE": 0x80}

    def value(expr):  # a field as MM's compiler would see it (GIFIELD(a, b) is a | b)
        e = re.sub(r"GIFIELD\(", "(", expr).replace(",", " | ")
        e = re.sub(r"\b[A-Z][A-Z0-9_]+\b", lambda m: str(flags.get(m.group(0), item00.get(m.group(0), 0))), e)
        return eval(e, {})

    def args(text):  # top-level comma split
        out, depth, cur = [], 0, ""
        for ch in text:
            if ch == "," and depth == 0:
                out.append(cur.strip())
                cur = ""
                continue
            depth += (ch == "(") - (ch == ")")
            cur += ch
        return out + [cur.strip()]

    for gi, body in re.findall(r"// (GI_\w+)\s*GET_ITEM\((.*?)\),\s*(?=//|$)", tb + "\n//", re.S):
        a = args(re.sub(r"\s+", " ", body))
        item, gid, text, chest = a[0], a[2], a[3], a[5]
        gi_rows.append(f"    /* {gi} 0x{gis[gi]:02X} */ {{ 0x{items.get(item, 0xFF):02X}, {gids.get(gid, -1)}, "
                       f"0x{value(text):X}, {1 if chest == 'CHEST_ANIM_LONG' else 0} }}, // {item}")

    out = os.path.join(REPO, "soh", "src", "code", "nv_mm_getitems.inc")
    with open(out, "w") as f:
        f.write("/* Generated by mm/tools/mm_getitems.py from the MM decomp's z_player.c (sGetItemTable) and\n"
                " * z_draw.c (sDrawItemTable): MM's get-items and how they're drawn. Do not edit. */\n\n")
        f.write("// per MM get-item id - 1: MM's item, its model id (GID), its text, a long chest opening\n")
        f.write("static const NvMmGetItemRow sGetItemTable[] = {\n" + "\n".join(gi_rows) + "\n};\n\n")
        f.write("// per MM model id (GID): MM's draw routine and its display lists\n")
        f.write("static const NvMmDrawItemRow sDrawItemTable[] = {\n" + "\n".join(draw_rows) + "\n};\n")
    with open(os.path.join(os.path.dirname(__file__), "mm_getitem_objects.py"), "w") as f:
        f.write('"""Generated by mm_getitems.py: the get-item objects (MM\'s object_gi_*), for mm_objects.py. Do not edit."""\n\n')
        f.write("GETITEM_OBJECTS = {\n")
        for obj, (fileidx, dls) in sorted(objects.items()):
            mat = dls.pop("@matanim", {})
            f.write(f'    "{obj}": ({fileidx}, {{"deps": ["gameplay_keep"], "dl": {{' + ", ".join(f'"{s}": 0x{o:X}' for s, o in dls.items()) + "}"
                    + (', "matanim": {' + ", ".join(f'"{s}": 0x{o:X}' for s, o in mat.items()) + "}" if mat else "")
                    + "}),\n")
        f.write("}\n")
    print(out, len(gi_rows), "get-items,", len(draw_rows), "models,", len(objects), "objects")


if __name__ == "__main__":
    main()
