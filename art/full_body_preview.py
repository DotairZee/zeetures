"""Whole creatures drawn from their body parts, as the engine draws them
(openc2e SkeletalCreature: each part hung from its parent's joint by the
body data's ATT points, drawn in the engine's order for the direction it
faces), for choosing poses for the Breeds page's full-body preview (owner,
2026-11-21: "let's do the full-body breed preview--provide me a couple
options for poses").

Read-only: made from the player's own installed game and user data folder
(Overrides first, then the game, then the user data folder); a preview for
the owner, not part of what the project ships.

    python3 -I full_body_preview.py OUT.png

A pose is a C2 pose string: the direction (0 away, 1 toward the camera, 2
right, 3 left) and one angle (0-3, 4 toward the camera) for each of the 14
parts in the engine's order: head, body, left thigh, left shin, left foot,
right thigh, right shin, right foot, left upper arm, left forearm, right
upper arm, right forearm, tail root, tail tip.
"""
import os
import sys

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from head_seating_check import IMAGES, read_att  # noqa: E402
from head_seating_sheet import read_rgba  # noqa: E402

# the engine's tables (SkeletalCreature.cpp): part letter, parent part, which
# of the parent's points it hangs from
PARTS = [("b", -1, -1), ("a", 0, 0), ("c", 0, 1), ("f", 0, 2), ("i", 0, 3), ("k", 0, 4), ("m", 0, 5),
         ("d", 2, 1), ("g", 3, 1), ("j", 4, 1), ("l", 5, 1), ("e", 7, 1), ("h", 8, 1), ("n", 6, 1)]
ZORDER = [[6, 13, 2, 7, 11, 4, 9, 0, 1, 5, 3, 8, 10, 12],  # right (the engine's, without the C3 ears and hair)
          [6, 13, 3, 8, 12, 5, 10, 0, 1, 2, 7, 11, 4, 9],  # left
          [6, 13, 2, 4, 9, 5, 3, 7, 8, 10, 0, 11, 12, 1],  # toward the camera
          [2, 4, 9, 1, 5, 3, 7, 8, 10, 11, 12, 0, 6, 13]]  # away
LOOKUP = [1, 0, 2, 7, 11, 3, 8, 12, 4, 9, 5, 10, 6, 13]  # pose string position -> part
DIRECTION = {"0": 3, "1": 2, "2": 0, "3": 1}


def frames_for(pose):
    d = DIRECTION[pose[0]]
    out = [0] * 14
    for i, ch in enumerate(pose[1:15]):
        n = 8 if ch == "4" else int(ch)
        if n < 4:
            n = 6 + d if d > 1 else n + d * 4
        out[LOOKUP[i]] = n
    return d, out


def part_files(letter, sex, stage, slot, fallback_slot):
    """The engine falls back to younger stages, then the standard breed."""
    for sl in (slot, fallback_slot):
        for st in range(int(stage), -1, -1):
            name = f"{letter}{sex}{st}{sl}"
            if name + ".s16" in IMAGES:
                imgs, att = read_rgba(name), read_att(name)
                if imgs and att:
                    return imgs, att
    return None, None


def render(sex, stage, slot, pose, fallback_slot="h"):
    d, fr = frames_for(pose)
    imgs, atts = [], []
    for letter, _, _ in PARTS:
        im, at = part_files(letter, sex, stage, slot, fallback_slot)
        imgs.append(im)
        atts.append(at)
    if imgs[0] is None or imgs[1] is None:
        return None
    x = [0] * 14
    y = [0] * 14
    for i, (_, parent, order) in enumerate(PARTS):
        if parent < 0 or imgs[i] is None:
            continue
        ax, ay = atts[i][fr[i]][0], atts[i][fr[i]][1]
        if parent == 0:
            px, py = atts[0][fr[0]][order * 2], atts[0][fr[0]][order * 2 + 1]
        else:
            if imgs[parent] is None:
                continue
            px = x[parent] + atts[parent][fr[parent]][order * 2]
            py = y[parent] + atts[parent][fr[parent]][order * 2 + 1]
        x[i], y[i] = px - ax, py - ay
    present = [i for i in range(14) if imgs[i] is not None and fr[i] < len(imgs[i])]
    lo_x = min(x[i] for i in present)
    lo_y = min(y[i] for i in present)
    hi_x = max(x[i] + imgs[i][fr[i]].shape[1] for i in present)
    hi_y = max(y[i] + imgs[i][fr[i]].shape[0] for i in present)
    canvas = Image.new("RGBA", (hi_x - lo_x, hi_y - lo_y), (0, 0, 0, 0))
    for i in ZORDER[d]:
        if i in present:
            part = Image.fromarray(imgs[i][fr[i]])
            canvas.alpha_composite(part, (x[i] - lo_x, y[i] - lo_y))
    return canvas


OPTIONS = [
    ("A  standing, side view", "2" + "1" * 14),
    ("B  facing you", "1" + "4" * 14),
    ("C  mid-stride, side view", "2" + "1" + "1" + "2" + "1" + "1" + "0" + "1" + "1" + "0" + "1" + "2" + "1" + "1" + "1"),
]
BREEDS = [("h", 0, "Hebe (norn)"), ("a", 0, "Brown Mouse"), ("k", 0, "Golden Desert"), ("l", 0, "Frog (Alex)"),
          ("a", 1, "Grendel"), ("a", 2, "Ettin")]


def main():
    out = sys.argv[1]
    stage = "4"
    S = 2
    cell_w, cell_h, label_w, head_h = 150, 150, 150, 30
    W = label_w + len(OPTIONS) * cell_w * S
    H = head_h + len(BREEDS) * cell_h * S
    sheet = Image.new("RGB", (W, H), (26, 28, 34))
    dr = ImageDraw.Draw(sheet)
    for k, (lab, _) in enumerate(OPTIONS):
        dr.text((label_w + k * cell_w * S + 10, 8), lab, fill=(230, 220, 190))
    for r, (slot, genus, name) in enumerate(BREEDS):
        yy = head_h + r * cell_h * S
        if r % 2:
            dr.rectangle([0, yy, W, yy + cell_h * S - 1], fill=(34, 36, 44))
        dr.text((10, yy + 10), name, fill=(222, 224, 228))
        for k, (_, pose) in enumerate(OPTIONS):
            sex = genus + (4 if r % 2 else 0)  # alternate males and females
            im = render(sex, stage, slot, pose, fallback_slot="h" if genus == 0 else "a")
            if im is None:
                dr.text((label_w + k * cell_w * S + 10, yy + 20), "(no set)", fill=(120, 120, 120))
                continue
            im = im.resize((im.width * S, im.height * S), Image.NEAREST)
            px = label_w + k * cell_w * S + (cell_w * S - im.width) // 2
            py = yy + cell_h * S - im.height - 10
            sheet.paste(im, (px, py), im)
    sheet.save(out)
    print("wrote", out, sheet.size)


if __name__ == "__main__":
    main()
