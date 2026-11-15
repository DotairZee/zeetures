"""A contact sheet of every installed norn breed's head on its body, joined
at the neck as the engine joins them (head picture's neck point on the
body picture's neck joint), for looking at head seating breed by breed
(owner's TODO, "other breeds' head seating": look before fixing).

Read-only: made on the player's own machine from their installed game and
user data folder (Overrides first, then the game, then the user data
folder, as the engine looks); the sheet is a preview for the owner, not
part of what the project ships.

    python3 -I head_seating_sheet.py OUT.png [STAGE]

STAGE: 0 baby, 1 child, 3 youth (default; adolescents and adults fall back
to it), 5 old. Per breed: male right, front, left, then female the same.
Arms and legs are left out (they do not touch the neck).
"""
import os
import struct
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from head_seating_check import IMAGES, read_att  # noqa: E402

VIEWS = [(1, 1, "right"), (8, 8, "front"), (5, 5, "left")]  # (body frame, head frame, label)
SCALE = 2
CELL_W, CELL_H = 110, 130
NX, NY = 55, 62  # where the neck joint is drawn in a cell


def read_rgba(name):
    p = IMAGES.get(name + ".s16")
    if not p:
        return None
    d = open(p, "rb").read()
    flags, n = struct.unpack_from("<IH", d, 0)
    out = []
    for i in range(n):
        off, w, h = struct.unpack_from("<IHH", d, 6 + 8 * i)
        a = np.frombuffer(d, "<u2", w * h, off).reshape(h, w).astype(int)
        if flags & 1:
            r, g, b = (a >> 11) & 31, (a >> 5) & 63, a & 31
            rgb = np.dstack([r << 3, g << 2, b << 3])
        else:
            r, g, b = (a >> 10) & 31, (a >> 5) & 31, a & 31
            rgb = np.dstack([r << 3, g << 3, b << 3])
        out.append(np.dstack([rgb, np.where(a != 0, 255, 0)]).astype(np.uint8))
    return out


def joined(code, bf, hf):
    heads, bodies = read_rgba("a" + code), read_rgba("b" + code)
    ha, ba = read_att("a" + code), read_att("b" + code)
    if not (heads and bodies and ha and ba) or hf >= len(heads) or bf >= len(bodies):
        return None
    cell = Image.new("RGBA", (CELL_W, CELL_H), (0, 0, 0, 0))
    body = Image.fromarray(bodies[bf])
    head = Image.fromarray(heads[hf])
    # body, then head over it, each placed so its neck point is at (NX, NY)
    for pic, pt in ((body, ba[bf]), (head, ha[hf])):
        layer = Image.new("RGBA", (CELL_W, CELL_H), (0, 0, 0, 0))
        layer.paste(pic, (NX - pt[0], NY - pt[1]), pic)
        cell.alpha_composite(layer)
    return cell


def main():
    out, stage = sys.argv[1], (sys.argv[2] if len(sys.argv) > 2 else "3")
    names = {"h": "Hebe (standard)", "a": "Brown Mouse", "b": "White Haired Pixie", "c": "Horse", "e": "Purple Mountain",
             "f": "Forest", "g": "Ron", "i": "Emerald", "j": "Pixie", "k": "Golden Desert (seated 11-13)",
             "l": "Frog (Alex)", "s": "Scorpio", "w": "Midsummer (M. Hilsenbeck)", "ka": "Kai (Wafuru, L. Morris)",
             "ga": "Goat (A. Allan)"}
    order = ["h", "a", "b", "c", "e", "f", "g", "i", "j", "k", "l", "s", "w", "ka", "ga"]
    rows = []
    for slot in order:
        cells = []
        for sex in "04":
            for bf, hf, _ in VIEWS:
                cells.append(joined(f"{sex}{stage}{slot}", bf, hf))
        if any(cells):
            rows.append((slot, cells))
    label_w, head_h = 190, 26
    W = label_w + 6 * CELL_W * SCALE + 20
    H = head_h + len(rows) * CELL_H * SCALE
    sheet = Image.new("RGB", (W, H), (236, 233, 224))
    dr = ImageDraw.Draw(sheet)
    for k, (_, _, lab) in enumerate(VIEWS * 2):
        x = label_w + k * CELL_W * SCALE + (10 if k >= 3 else 0)
        dr.text((x + 6, 6), ("male " if k < 3 else "female ") + lab, fill=(60, 60, 60))
    for r, (slot, cells) in enumerate(rows):
        y = head_h + r * CELL_H * SCALE
        if r % 2:
            dr.rectangle([0, y, W, y + CELL_H * SCALE - 1], fill=(226, 222, 212))
        dr.text((8, y + 8), f"{slot}: {names.get(slot, slot)}", fill=(30, 30, 30))
        for k, c in enumerate(cells):
            x = label_w + k * CELL_W * SCALE + (10 if k >= 3 else 0)
            if c is None:
                dr.text((x + 8, y + 20), "(no set)", fill=(120, 120, 120))
                continue
            big = c.resize((CELL_W * SCALE, CELL_H * SCALE), Image.NEAREST)
            sheet.paste(big, (x, y), big)
            # the neck joint's height, for comparing across rows
            dr.line([x + 4, y + NY * SCALE, x + 24, y + NY * SCALE], fill=(200, 60, 60))
    sheet.save(out)
    print("wrote", out, sheet.size, "rows", [s for s, _ in rows])


if __name__ == "__main__":
    main()
