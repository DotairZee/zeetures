"""Seat the Golden Desert norn's head (C2 norn slot k) the way Hebe's sits (slot h).

Owner, 2026-11-13: Golden Desert heads rested higher than Hebe's, most
visibly from the front, where a gap of neck showed under the chin.

Recipe, run on the player's own machine against their own installed game
(nothing from the game is shipped; this script writes corrected copies of
the player's body data into their openc2e Overrides folder, which the
engine reads in place of the game's files of the same name):

1. Body: Golden Desert's bodies have the same frame sizes and outlines as
   Hebe's apart from mane tufts, so each body's neck joint (the first point
   of b??k.att) is set to Hebe's for that frame. Hebe's neck joint is
   consistent across frames; Golden Desert's had been nudged by hand on a
   few frames, which moved the head up or down from pose to pose.
2. Head (redone 2026-11-13 evening; owner: "look how far off the chin is
   from the body on the golden norn"): the first pass matched the lowest
   point of the face side, which on the males is mane hanging below the
   chin, so their heads rode high and forward. Now each head picture (10
   per sex and age: right-facing 0-3 looking down to up, left-facing 4-7,
   front 8, back 9) is placed by its face: the skin-coloured area of the
   Golden Desert face is matched against Hebe's in the same picture (the
   offset with the most overlap), and the neck point is set where Hebe's
   is relative to that face. The back view has no face; it moves up or
   down with the front view.

Life stages: baby, child, youth and old for both sexes (0, 1, 3, 5; female
sets 4x). Adolescent, adult and senile fall back to these in the engine.

Usage: python3 -I golden_desert_heads.py [--dry-run]
"""
import os
import struct
import sys

import numpy as np

C2 = os.path.expanduser("~/.steam/debian-installation/steamapps/common/Creatures The Albian Years/Creatures 2")
OUT = os.path.expanduser("~/.openc2e/Creatures 2/Overrides/Body Data")
GROUP = {j: ([0, 1, 2, 3] if j < 4 else [4, 5, 6, 7] if j < 8 else [j]) for j in range(10)}


def find(folder, name):
    for f in os.listdir(os.path.join(C2, folder)):
        if f.lower() == name.lower():
            return os.path.join(C2, folder, f)
    raise FileNotFoundError(name)


def read_att(name):
    return [list(map(int, l.split())) for l in open(find("Body Data", name + ".att")) if l.strip()]


def write_att(path, rows, like):
    # keep the game's layout: a trailing space after each number if it had one, CRLF
    trailing = open(find("Body Data", like + ".att"), "rb").read().split(b"\r\n")[0].endswith(b" ")
    with open(path, "wb") as f:
        for r in rows:
            f.write((" ".join(map(str, r)) + (" " if trailing else "") + "\r\n").encode())


def read_masks(name):
    """Opaque masks of a 16-bit sprite's frames, without single stray pixels
    (some of the game's sprites carry lone marker dots at their joints)."""
    d = open(find("Images", name + ".s16"), "rb").read()
    flags, n = struct.unpack_from("<IH", d, 0)
    out = []
    for i in range(n):
        off, w, h = struct.unpack_from("<IHH", d, 6 + 8 * i)
        a = np.frombuffer(d, "<u2", w * h, off).reshape(h, w) != 0
        p = np.pad(a, 1)
        nb = p[:-2, 1:-1].astype(int) + p[2:, 1:-1] + p[1:-1, :-2] + p[1:-1, 2:]
        out.append(a & (nb >= 2))
    return out


def chin(mask, nx, j):
    cols = mask[:, nx:] if j < 4 else mask[:, :nx + 1] if j < 8 else mask[:, max(0, nx - 6):nx + 7]
    return np.nonzero(cols.any(1))[0].max()


def body_top(mask, jx):
    return np.nonzero(mask[:, max(0, jx - 10):jx + 11].any(1))[0].min()


def overlap(head, ha, body, ba, j):
    return ba[1] - ha[1] + chin(head, ha[0], j) - body_top(body, ba[0])


def read_rgb(name):
    """A 16-bit sprite's frames as RGB arrays."""
    d = open(find("Images", name + ".s16"), "rb").read()
    flags, n = struct.unpack_from("<IH", d, 0)
    out = []
    for i in range(n):
        off, w, h = struct.unpack_from("<IHH", d, 6 + 8 * i)
        a = np.frombuffer(d, "<u2", w * h, off).reshape(h, w).astype(int)
        if flags & 1:
            r, g, b = (a >> 11) & 31, (a >> 5) & 63, a & 31
            out.append(np.dstack([r << 3, g << 2, b << 3]))
        else:
            r, g, b = (a >> 10) & 31, (a >> 5) & 31, a & 31
            out.append(np.dstack([r << 3, g << 3, b << 3]))
    return out


def skin(im):
    r, g, b = im[..., 0], im[..., 1], im[..., 2]
    return (r > 170) & (g > 110) & (b > 90) & (r >= g) & (g >= b - 10)


def face_offset(mk, mh):
    """(dx, dy) putting Golden Desert's face mask mk best onto Hebe's mh."""
    H, W = mh.shape
    h, w = mk.shape
    best = (0, 0, -1.0)
    for dy in range(-15, 16):
        for dx in range(-15, 16):
            y0, x0, y1, x1 = max(0, dy), max(0, dx), min(H, h + dy), min(W, w + dx)
            if y1 <= y0 or x1 <= x0:
                continue
            a = mh[y0:y1, x0:x1]
            b = mk[y0 - dy:y1 - dy, x0 - dx:x1 - dx]
            inter = (a & b).sum()
            s = inter / (mh.sum() + mk.sum() - inter)
            if s > best[2]:
                best = (dx, dy, s)
    return best[0], best[1]


def fit(sex, stage):
    k, h = f"{sex}{stage}k", f"{sex}{stage}h"
    bk, bh, ak, ah = read_att("b" + k), read_att("b" + h), read_att("a" + k), read_att("a" + h)
    for i in range(10):
        bk[i][0], bk[i][1] = bh[i][0], bh[i][1]
    heads_k, heads_h = read_rgb("a" + k), read_rgb("a" + h)
    moves = []
    for j in range(9):
        dx, dy = face_offset(skin(heads_k[j]), skin(heads_h[j]))
        nx, ny = ah[j][0] - dx, ah[j][1] - dy
        moves.append((nx - ak[j][0], ny - ak[j][1]))
        ak[j][0], ak[j][1] = nx, ny
    ak[9][1] += moves[8][1]  # the back view with the front
    moves.append((0, moves[8][1]))
    return bk, ak, moves


def main():
    dry = "--dry-run" in sys.argv
    os.makedirs(OUT, exist_ok=True)
    for sex in "04":
        for stage in "0135":
            bk, ak, moves = fit(sex, stage)
            print(f"{'female' if sex == '4' else 'male'} stage {stage}: neck point moved by {moves} (x, y; frames 0-9)")
            if not dry:
                write_att(os.path.join(OUT, f"b{sex}{stage}k.att"), bk, f"b{sex}{stage}k")
                write_att(os.path.join(OUT, f"a{sex}{stage}k.att"), ak, f"a{sex}{stage}k")


if __name__ == "__main__":
    main()
