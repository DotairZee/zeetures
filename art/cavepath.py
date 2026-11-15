"""Scenery for the hidden path behind the pond's waterfall, made on the
player's own machine from their installed Creatures 2 (bring-your-own-data:
only this recipe is part of the project, never the game's pixels).

The path's two ends (owner, 2026-11-10):
- the exit, the old stone arch west of the bamboo-cage lift: "the boundary
  of the arch itself" is what a creature walks out through. The arch's right
  jamb and the wall of log blocks beside it (to the lift's shaft) are drawn
  again in front of creatures, cut along the opening's inner edge, so a
  creature that comes out of the tunnel stands hidden behind them and walks
  out past the jamb's edge into the arch.
- the entrance, in the cave behind the waterfall: the waterfall object's six
  frames carry the cave wall painted in around the water; the copy made
  here keeps only what moves (the water and its splash), so a creature
  walking behind the falls is seen around the water, against the dark
  opening (cavemouth.py) behind it, until the rock's edge hides it.

    python3 cavepath.py C2_DIR USER_DATA_DIR

writes USER_DATA_DIR/Images/zajb.png (the jamb, top-left at JAMB_X0,
JAMB_Y0) and USER_DATA_DIR/Overrides/Images/wfal.s16 (the water-only falls).
"""
import sys
import os
import math
import numpy as np
from PIL import Image, PngImagePlugin
from background import region
import s16

# the arch: its opening's inner edge on the right, as seen on the scenery: a
# straight jamb at x 4120 below y 752, the arch's inner curve above it
JAMB_X0, JAMB_Y0, JAMB_X1, JAMB_Y1 = 4078, 700, 4186, 843
ARC_CX, ARC_CY, ARC_R, JAMB_X = 4083.0, 752.0, 37.0, 4120.0


def inner_edge(y):
    if y >= ARC_CY:
        return JAMB_X
    dy = ARC_CY - y
    return ARC_CX + math.sqrt(max(ARC_R * ARC_R - dy * dy, 0.0))


def jamb(c2):
    im = np.asarray(region(c2, JAMB_X0, JAMB_Y0, JAMB_X1, JAMB_Y1)).astype(np.float32)
    h, w = im.shape[:2]
    a = np.zeros((h, w), np.float32)
    for j in range(h):
        y = JAMB_Y0 + j + 0.5
        ex = inner_edge(y)
        for i in range(w):
            x = JAMB_X0 + i + 0.5
            a[j, i] = np.clip((x - ex) + 0.5, 0, 1)   # a one-pixel soft edge
    # above the arch's crown nothing is needed (the arch is in front anyway)
    im[..., 3] = a * 255
    return Image.fromarray(im.astype(np.uint8), 'RGBA')


STILL = 12  # a waterfall pixel that never varies more than this is wall
GROW = 1


def largest_parts(mask, min_size):
    """The mask without its connected pieces smaller than min_size."""
    h, w = mask.shape
    seen = np.zeros_like(mask)
    out = np.zeros_like(mask)
    for y in range(h):
        for x in range(w):
            if mask[y, x] and not seen[y, x]:
                stack, piece = [(y, x)], []
                seen[y, x] = True
                while stack:
                    cy, cx = stack.pop()
                    piece.append((cy, cx))
                    for ny, nx in ((cy + 1, cx), (cy - 1, cx), (cy, cx + 1), (cy, cx - 1)):
                        if 0 <= ny < h and 0 <= nx < w and mask[ny, nx] and not seen[ny, nx]:
                            seen[ny, nx] = True
                            stack.append((ny, nx))
                if len(piece) >= min_size:
                    for py, px in piece:
                        out[py, px] = True
    return out


def water_only(c2):
    frames = s16.read(os.path.join(c2, 'Images', 'wfal.s16'))
    if len({im.size for im in frames}) != 1:
        raise SystemExit('wfal.s16: frames differ in size; not the expected waterfall')
    a = np.stack([np.asarray(im.convert('RGBA')).astype(np.int16) for im in frames])
    keep = (a[..., :3].max(0) - a[..., :3].min(0)).max(-1) > STILL
    # only the water itself: drop small moving patches apart from the main
    # body (a flicker in the painted wall at the falls' left edge)
    keep = largest_parts(keep, 200)
    for _ in range(GROW):
        g = keep.copy()
        g[1:] |= keep[:-1]; g[:-1] |= keep[1:]; g[:, 1:] |= keep[:, :-1]; g[:, :-1] |= keep[:, 1:]
        keep = g
    out = []
    for f in a:
        f = f.copy()
        f[..., 3] = np.where(keep, f[..., 3], 0)
        out.append(Image.fromarray(f.astype(np.uint8), 'RGBA'))
    return out


def main():
    c2, data = sys.argv[1], sys.argv[2]
    os.makedirs(os.path.join(data, 'Images'), exist_ok=True)
    os.makedirs(os.path.join(data, 'Overrides', 'Images'), exist_ok=True)
    info = PngImagePlugin.PngInfo()
    info.add_text('openc2e-frames', '1')
    jamb(c2).save(os.path.join(data, 'Images', 'zajb.png'), pnginfo=info)
    s16.write(os.path.join(data, 'Overrides', 'Images', 'wfal.s16'), water_only(c2))
    print('wrote Images/zajb.png and Overrides/Images/wfal.s16 in', data)


if __name__ == '__main__':
    main()
