#!/usr/bin/env python3
"""Writes Workbench icons (.info) for the CTP2 release.

    tools/ci/make-icon.py <out.info>            the launcher (a tool icon)
    tools/ci/make-icon.py --drawer <out.info>   the CTP2 drawer
    tools/ci/make-icon.py --stack N <out.info>  a tool icon asking for N bytes
                                                of stack (the AROS program)

An OS 2/3 icon: struct DiskObject, for a drawer its DrawerData (with the OS 2
extension), two planar 4-colour images (normal and selected), no tool types.
Workbench's default palette: 0 grey, 1 black, 2 white, 3 blue. The picture
is a globe with a ring round it, drawn here so the repository needs no
binary file.
"""

import math
import struct
import sys

W, H = 48, 38


def picture(selected):
    px = [[0] * W for _ in range(H)]
    cx, cy, r = W / 2 - 0.5, H / 2 - 0.5, 15.5
    for y in range(H):
        for x in range(W):
            dx, dy = x - cx, (y - cy) * 1.15
            d = math.hypot(dx, dy)
            if d <= r:
                c = 3                                        # sea
                # land: a few blobs, white
                for (lx, ly, lr) in ((-6, -5, 6), (5, 3, 7), (-3, 8, 4), (8, -8, 3)):
                    if math.hypot(dx - lx, dy - ly) < lr:
                        c = 2
                # meridian and equator, black
                if abs(dx) < 0.6 or abs(dy) < 0.6:
                    c = 1
                if d > r - 1.2:
                    c = 1                                    # outline
                px[y][x] = c
            # the ring: an ellipse across the globe, black with a white edge
            e = math.hypot(dx / 22.5, (y - cy) / 6.5)
            if 0.93 < e < 1.07 and not (d < r and (y - cy) < 0):
                px[y][x] = 1 if e < 1.0 else 2
    if selected:
        # Workbench's usual selected look: colours swapped
        swap = {0: 1, 1: 0, 2: 3, 3: 2}
        px = [[swap[c] if c else 0 for c in row] for row in px]
    return px


def planes(px, depth=2):
    words = (W + 15) // 16
    out = b""
    for p in range(depth):
        for y in range(H):
            row = 0
            for x in range(words * 16):
                bit = (px[y][x] >> p) & 1 if x < W else 0
                row = (row << 1) | bit
            out += row.to_bytes(words * 2, "big")
    return out


def image(px):
    # struct Image: LeftEdge, TopEdge, Width, Height, Depth, ImageData,
    # PlanePick, PlaneOnOff, NextImage
    return struct.pack(">hhhhhIBBI", 0, 0, W, H, 2, 1, 3, 0, 0) + planes(px)


def drawer_data():
    # struct NewWindow (48 bytes), dd_CurrentX/Y, then OS 2's dd_Flags and
    # dd_ViewModes (read when the gadget's UserData revision is 1).
    newwin = struct.pack(
        ">hhhhBBIIIIIIIhhHHH",
        60, 40, 420, 200,   # LeftEdge, TopEdge, Width, Height
        255, 255,           # DetailPen, BlockPen
        0, 0,               # IDCMPFlags, Flags
        0, 0, 0, 0, 0,      # FirstGadget, CheckMark, Title, Screen, BitMap
        90, 40, 65535, 65535,  # Min/Max size
        1,                  # Type: WBENCHSCREEN
    )
    assert len(newwin) == 48, len(newwin)
    return newwin + struct.pack(">iiIH", 0, 0, 0, 0)


def main():
    args = sys.argv[1:]
    drawer = "--drawer" in args
    args = [a for a in args if a != "--drawer"]
    stack = 65536
    if "--stack" in args:
        i = args.index("--stack")
        stack = int(args[i + 1])
        del args[i:i + 2]
    out = args[0]
    gadget = struct.pack(
        ">IhhhhHHHIIIIIHI",
        0,              # NextGadget
        0, 0, W, H,     # LeftEdge, TopEdge, Width, Height
        0x0004 | 0x0002,  # Flags: GFLG_GADGIMAGE | GFLG_GADGHIMAGE
        0x0003,         # Activation: RELVERIFY | GADGIMMEDIATE
        0x0001,         # GadgetType: BOOLGADGET
        1, 1,           # GadgetRender, SelectRender (present)
        0, 0, 0,        # GadgetText, MutualExclude, SpecialInfo
        0,              # GadgetID
        1,              # UserData: WB_DISKREVISION
    )
    diskobj = struct.pack(">HH", 0xE310, 1) + gadget + struct.pack(
        ">BBIIiiIII",
        2 if drawer else 3, 0,      # do_Type WBDRAWER / WBTOOL, pad
        0, 0,                       # DefaultTool, ToolTypes
        -0x80000000, -0x80000000,   # NO_ICON_POSITION
        1 if drawer else 0, 0,      # DrawerData, ToolWindow
        0 if drawer else stack,     # StackSize
    )
    assert len(diskobj) == 78, len(diskobj)
    with open(out, "wb") as f:
        f.write(diskobj + (drawer_data() if drawer else b"")
                + image(picture(False)) + image(picture(True)))


if __name__ == "__main__":
    main()
