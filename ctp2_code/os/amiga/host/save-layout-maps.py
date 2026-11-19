#!/usr/bin/env python3
"""Field maps from the old PowerPC save layout to the 68k one (SaveLayout.h).

  save-layout-maps.py OLD.o NEW.o > ctp2_code/ctp/ctp2_utils/SaveLayoutMaps.h

OLD.o and NEW.o: save-layout-types.cpp compiled by the WarpOS compiler with
-g -fno-eliminate-unused-debug-types, OLD.o also with
-DCTP2_SAVE_LAYOUT_NATURAL (the layout of the WarpOS/MorphOS builds before
SaveLayout.h), NEW.o without (the 68k layout all Amiga builds use now).

Every saved range (a LoadChunk's members, or a whole struct) is split into
its leaves: scalars, pointers, arrays of scalars, unions. A leaf is found in
both layouts by its path (members, base classes, array indexes); each map is
a list of copies (to, from, length) from the old bytes into the new object.
Needs pyelftools.
"""
import struct
import sys

from elftools.elf import relocation as _rel
from elftools.elf.elffile import ELFFile


def _apply_ppc(self, stream, reloc, symtab):
    """R_PPC_ADDR32, all that .debug_* use: S + A, big-endian."""
    sym = symtab.get_symbol(reloc["r_info_sym"])
    stream.seek(reloc["r_offset"])
    stream.write(struct.pack(">I", (sym["st_value"] + reloc["r_addend"]) & 0xFFFFFFFF))


_rel.RelocationHandler._do_apply_relocation = _apply_ppc

# (map name, C++ type, first member, last member): a LoadChunk from first to
# the end of last; None, None: the whole object (Load of sizeof). Not here:
# "Name@member": the class of that name that has the member (ai::Agreement,
# not the game's Agreement handle). EndGameRecord (not built) and ThroneInfo (ThroneDB is not in saves).
RANGES = [
    ("Cell",                "Cell",                "m_env",       "m_cellOwner"),
    ("Happy",               "Happy",               "m_happiness", "m_crime"),
    ("Player",              "Player",              "m_owner",     "m_broken_alliances_and_cease_fires"),
    ("Threat",              "Threat",              None, None),
    ("RegardEvent",         "RegardEvent",         None, None),
    ("NegotiationEvent",    "NegotiationEvent",    None, None),
    ("OldNegotiationEvent", "OldNegotiationEvent", None, None),
    ("Difficulty",          "Difficulty",          None, None),
    ("PlayerHappiness",     "PlayerHappiness",     None, None),
    ("BuildNode",           "BuildNode",           None, None),
    ("Agreement",           "Agreement@proposal",  None, None),
    ("GameEventArgumentData", "GameEventArgument", "m_data",      "m_data"),
    ("SlicSymbolValue",     "SlicSymbolData",      "m_val",       "m_val"),
]


def name(d):
    a = d.attributes.get("DW_AT_name")
    return a.value.decode() if a else None


def target(d):
    return d.get_DIE_from_attribute("DW_AT_type") if "DW_AT_type" in d.attributes else None


DEFS = {}   # the file being read: qualified name -> complete definition


def qualified(d):
    q, p = name(d), d.get_parent()
    while p is not None and p.tag in ("DW_TAG_namespace", "DW_TAG_structure_type", "DW_TAG_class_type"):
        q = (name(p) or "") + "::" + q
        p = p.get_parent()
    return q


def strip(d):
    while d is not None and d.tag in ("DW_TAG_typedef", "DW_TAG_const_type", "DW_TAG_volatile_type"):
        d = target(d)
    if d is not None and is_aggregate(d) and "DW_AT_declaration" in d.attributes:
        d = DEFS[qualified(d)][0]   # a member's type given as a declaration
    return d


def member_offset(m):
    v = m.attributes["DW_AT_data_member_location"].value
    if not isinstance(v, list):
        return v
    assert v[0] == 0x23, v          # DW_OP_plus_uconst
    r = shift = 0
    for b in v[1:]:
        r |= (b & 0x7F) << shift
        shift += 7
        if not b & 0x80:
            break
    return r


def size_of(d):
    d = strip(d)
    if "DW_AT_byte_size" in d.attributes:
        return d.attributes["DW_AT_byte_size"].value
    if d.tag == "DW_TAG_array_type":
        n = 1
        for sub in d.iter_children():
            if sub.tag == "DW_TAG_subrange_type":
                if "DW_AT_count" in sub.attributes:
                    n *= sub.attributes["DW_AT_count"].value
                else:
                    n *= sub.attributes["DW_AT_upper_bound"].value + 1
        return n * size_of(target(d))
    raise ValueError(f"no size for {d.tag} {name(d)}")


def is_aggregate(d):
    return d.tag in ("DW_TAG_structure_type", "DW_TAG_class_type")


def leaves(d, base, path, out, only=None):
    """(path, offset, size) of every leaf of type d at offset base (of the
    members named in only, when given)."""
    d = strip(d)
    if is_aggregate(d):
        for c in d.iter_children():
            if only is not None and (c.tag != "DW_TAG_member" or name(c) not in only):
                continue
            if c.tag == "DW_TAG_inheritance":
                b = strip(target(c))
                leaves(b, base + member_offset(c), path + ("<%s>" % (name(b) or member_offset(c)),), out)
            elif c.tag == "DW_TAG_member" and "DW_AT_data_member_location" in c.attributes:
                assert "DW_AT_bit_size" not in c.attributes, f"bit field {path} {name(c)}"
                leaves(target(c), base + member_offset(c), path + (name(c) or "?",), out)
    elif d.tag == "DW_TAG_array_type" and is_aggregate(strip(target(d))):
        elem = size_of(target(d))
        for i in range(size_of(d) // elem):
            leaves(target(d), base + i * elem, path + (f"[{i}]",), out)
    else:   # scalar, pointer, enum, array of scalars, union
        out.append((path, base, size_of(d)))


def lookup(found, cls):
    """The definition of cls ("Name" or "Name@member": the one with that member)."""
    want, _, member = cls.partition("@")
    for d in found[want]:
        if not member or any(c.tag == "DW_TAG_member" and name(c) == member for c in d.iter_children()):
            return d
    raise KeyError(cls)


def find_types(path):
    elf = ELFFile(open(path, "rb"))
    found = {}
    for cu in elf.get_dwarf_info().iter_CUs():
        for d in cu.iter_DIEs():
            if is_aggregate(d) and "DW_AT_declaration" not in d.attributes and name(d):
                found.setdefault(qualified(d), []).append(d)
    return found


def top_members(d):
    return [(name(c), member_offset(c), size_of(target(c))) for c in strip(d).iter_children()
            if c.tag == "DW_TAG_member" and "DW_AT_data_member_location" in c.attributes]


def range_of(d, first, last):
    """(start, end, member names) of a saved range in type d."""
    if first is None:
        return 0, size_of(d), None
    ms = top_members(d)
    names = [m[0] for m in ms]
    i, j = names.index(first), names.index(last)
    return ms[i][1], ms[j][1] + ms[j][2], set(names[i:j + 1])


def side(defs, cls, first, last):
    DEFS.clear()
    DEFS.update(defs)
    d = lookup(defs, cls)
    start, end, names = range_of(d, first, last)
    out = []
    leaves(d, 0, (), out, names)
    return start, end, names, out


def build_map(olddefs, newdefs, cls, first, last):
    o0, o1, onames, ol = side(olddefs, cls, first, last)
    n0, n1, _, nl = side(newdefs, cls, first, last)
    if onames is not None:
        ol = [x for x in ol if x[0][0] in onames]
        nl = [x for x in nl if x[0][0] in onames]
    oldby = {p: (off, size) for p, off, size in ol}
    assert len(oldby) == len(ol) and len(nl) == len(ol), "leaf lists differ"
    ops = []
    for p, noff, nsize in nl:
        ooff, osize = oldby[p]
        # A union is the one leaf whose size may differ (the old one has
        # tail padding); its members start at 0, the new size is copied.
        assert nsize <= osize, (p, nsize, osize)
        ops.append([noff - n0, ooff - o0, nsize])
    ops.sort()
    merged = []
    for op in ops:
        if merged and merged[-1][0] + merged[-1][2] == op[0] and merged[-1][1] + merged[-1][2] == op[1]:
            merged[-1][2] += op[2]
        else:
            merged.append(op)
    return n1 - n0, o1 - o0, merged


def main():
    old, new = find_types(sys.argv[1]), find_types(sys.argv[2])
    print("// Generated by ctp2_code/os/amiga/host/save-layout-maps.py: do not edit.")
    print("// Copies (to, from, length) from the old PowerPC save layout of each")
    print("// saved range into the 68k layout (SaveLayout.h, SaveLayout.cpp).")
    print()
    for mapname, cls, first, last in RANGES:
        newsize, oldsize, ops = build_map(old, new, cls, first, last)
        print(f"static const SaveLayoutCopy s_copies{mapname}[] =\n{{")
        for to, frm, n in ops:
            print(f"\t{{ {to}, {frm}, {n} }},")
        print("};")
        print(f"const SaveLayoutMap g_saveLayout{mapname} =\n"
              f"\t{{ {newsize}, {oldsize}, {len(ops)}, s_copies{mapname} }};\n")


if __name__ == "__main__":
    main()
