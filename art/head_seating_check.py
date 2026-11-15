"""Check how every installed breed's heads sit on their bodies (owner's
TODO, "other breeds' head seating": the check that found Golden Desert's
heads riding high, run on every breed; look before fixing).

Read-only: it reads the player's own installed game and user data folder
and prints a report; it changes nothing.

For each breed (slot letter), sex and life stage with both a head and a
body picture set, and each head picture (0-3 right-facing looking down to
up, 4-7 left-facing, 8 front, 9 back), it measures how far the chin (the
lowest point of the head on the side it faces; the middle for the front)
comes below the body's top edge at the neck, averaged over the body frames
that head is drawn with. Then it compares that with the species' standard
breed in the same picture (norns: Hebe, slot h; grendels and ettins:
slot a; the fourth picture set: against Hebe). A positive difference: the
head sits lower (sunk into the body); negative: higher (a gap of neck).
The back view is left out (no chin to measure).

Files are found as the engine finds them: the user data folder's Overrides
first, then the game, then the user data folder.

    python3 -I head_seating_check.py [--all]

prints every breed's largest differences; flagged lines (3 px or more)
first.
"""
import os
import re
import struct
import sys

import numpy as np

C2 = os.path.expanduser("~/.steam/debian-installation/steamapps/common/Creatures The Albian Years/Creatures 2")
USER = os.path.expanduser("~/.openc2e/Creatures 2")
ORDER = [os.path.join(USER, "Overrides"), C2, USER]
GROUP = {j: ([0, 1, 2, 3] if j < 4 else [4, 5, 6, 7] if j < 8 else [j]) for j in range(9)}
STAGES = "012345"


def listing(folder):
    out = {}
    for base in reversed(ORDER):  # later ones overwritten by earlier
        d = os.path.join(base, folder)
        if os.path.isdir(d):
            for f in os.listdir(d):
                out[f.lower()] = os.path.join(d, f)
    return out


IMAGES = listing("Images")
BODY = listing("Body Data")


def read_att(name):
    p = BODY.get(name + ".att")
    if not p:
        return None
    return [list(map(int, l.split())) for l in open(p, errors="replace") if l.strip()]


def read_masks(name):
    p = IMAGES.get(name + ".s16")
    if not p:
        return None
    d = open(p, "rb").read()
    flags, n = struct.unpack_from("<IH", d, 0)
    out = []
    for i in range(n):
        off, w, h = struct.unpack_from("<IHH", d, 6 + 8 * i)
        a = np.frombuffer(d, "<u2", w * h, off).reshape(h, w) != 0
        p2 = np.pad(a, 1)
        nb = p2[:-2, 1:-1].astype(int) + p2[2:, 1:-1] + p2[1:-1, :-2] + p2[1:-1, 2:]
        out.append(a & (nb >= 2))  # without lone marker dots
    return out


def chin(mask, nx, j):
    nx = max(0, min(mask.shape[1] - 1, nx))
    cols = mask[:, nx:] if j < 4 else mask[:, :nx + 1] if j < 8 else mask[:, max(0, nx - 6):nx + 7]
    rows = np.nonzero(cols.any(1))[0]
    return rows.max() if len(rows) else None


def body_top(mask, jx):
    cols = mask[:, max(0, jx - 10):jx + 11]
    rows = np.nonzero(cols.any(1))[0]
    return rows.min() if len(rows) else None


def seating(code):
    """Per head picture 0-8: mean chin overlap (px) over its body frames,
    or None; code like '0' + stage + slot."""
    heads, hatt = read_masks("a" + code), read_att("a" + code)
    bodies, batt = read_masks("b" + code), read_att("b" + code)
    if not (heads and hatt and bodies and batt):
        return None
    out = []
    for j in range(9):
        vals = []
        for i in GROUP[j]:
            if j >= len(heads) or j >= len(hatt) or i >= len(bodies) or i >= len(batt):
                continue
            c = chin(heads[j], hatt[j][0], j)
            t = body_top(bodies[i], batt[i][0])
            if c is None or t is None:
                continue
            vals.append(batt[i][1] - hatt[j][1] + c - t)
        out.append(float(np.mean(vals)) if vals else None)
    return out


def reference(sp):
    # sp: the first digit of the file name (species and sex)
    kind = sp % 4
    return ("h", sp) if kind in (0, 3) and sp in (0, 4) else ("h", sp - 3) if kind == 3 else ("a", sp)


def main():
    show_all = "--all" in sys.argv
    codes = set()
    for f in IMAGES:
        m = re.match(r"^a([0-7])([0-5])([a-z]+)\.s16$", f)
        if m:
            codes.add((int(m.group(1)), m.group(2), m.group(3)))
    names = {0: "norn m", 4: "norn f", 1: "grendel m", 5: "grendel f", 2: "ettin m", 6: "ettin f", 3: "set 4 m", 7: "set 4 f"}
    lines = []
    for sp, st, slot in sorted(codes):
        rslot, rsp = reference(sp)
        if (rslot, rsp) == (slot, sp):
            continue
        mine = seating(f"{sp}{st}{slot}")
        ref = seating(f"{rsp}{st}{rslot}")
        if mine is None:
            lines.append((0, f"{slot:>3} {names[sp]:9} stage {st}: no body set of its own (uses another stage's or breed's)"))
            continue
        if ref is None:
            lines.append((0, f"{slot:>3} {names[sp]:9} stage {st}: no {rslot} stage {st} to compare with"))
            continue
        diffs = [None if a is None or b is None else a - b for a, b in zip(mine, ref)]
        known = [d for d in diffs if d is not None]
        if not known:
            continue
        worst = max(known, key=abs)
        txt = " ".join("  ." if d is None else f"{d:+3.0f}" for d in diffs)
        lines.append((abs(worst), f"{slot:>3} {names[sp]:9} stage {st}: worst {worst:+4.1f}  by picture 0-8: {txt}"))
    lines.sort(key=lambda t: -t[0])
    for w, l in lines:
        if show_all or w >= 3 or w == 0:
            print(("! " if w >= 3 else "  ") + l)
    print(f"({len(lines)} breed/sex/stage sets checked; '!' = 3 px or more from the standard breed)")


if __name__ == "__main__":
    main()
