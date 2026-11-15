"""Draw an original extension of the red rock ledge in the upper east, under
the potted bush right of the deck's lift (owner, 2026-11-10: the bush hung
off the ledge's thin tip, and moved left it covered the lift's cables).

The ledge is a tongue of red-brown rock whose flat top is the deck's walking
line (y 906) and whose underside slopes up to a point at about x 7677. This
continues it about 75 px east to a blunter tip, so the planter stands wholly
on rock; ledge.cos (tools/additions) moves the walking floor out to match.

Everything here is drawn from scratch (no game pixels are read or copied),
so the output can ship with the project. Colors are numbers chosen by eye to
sit with the rock around it; the left end is dithered into the rock behind.

    python3 ledge.py OUT_BASENAME

writes OUT_BASENAME.png (for viewing) and OUT_BASENAME.s16 (for the game).
Placed at world x 7636, y 904 (see ledge.cos).
"""
import sys
import numpy as np
from PIL import Image
import s16
from catwalk import _noise

SS = 4
X0, Y0 = 7636, 904
W, H = 120, 52              # x 7636-7756, y 904-956
TOP = 906                   # the walking line
TIP = 7750                  # where the new tip ends
UNDER_LEFT = (7636, 948)    # the underside where we take over (below the old one)

RIM = np.array([152, 66, 38], np.float32)      # the lit top edge
BODY = np.array([98, 34, 16], np.float32)     # the rock's face
DEEP = np.array([58, 17, 6], np.float32)       # toward the underside
EDGE = np.array([18, 4, 0], np.float32)        # the underside's dark line
MATTE = np.array([10, 3, 0], np.float32)       # the dark cave behind


_ROUGH = _noise((1, 40), 35, 4)[0]


def underside(x):
    """World y of the underside at world x (curving up into a blunt tip,
    with a rough, broken edge like the rock's own)."""
    t = np.clip((x - UNDER_LEFT[0]) / (TIP - UNDER_LEFT[0]), 0, 1)
    i = np.clip(((x - X0) / 3).astype(int), 0, len(_ROUGH) - 1)
    return TOP + (UNDER_LEFT[1] - TOP - 8) * (1 - t) ** 1.0 + 8 * (1 - t ** 5) + 3.0 * _ROUGH[i] * (1 - t ** 4)


def draw():
    w4, h4 = W * SS, H * SS
    ys, xs = np.mgrid[0:h4, 0:w4].astype(np.float32)
    wx = X0 + (xs + 0.5) / SS
    wy = Y0 + (ys + 0.5) / SS
    bot = underside(wx)
    # the tip is rounded: the top edge bends down over its last few pixels
    tipround = np.clip((wx - (TIP - 6)) / 6, 0, 1) ** 2 * 4
    inside = (wy >= TOP + tipround) & (wy <= bot) & (wx <= TIP)
    depth = np.clip((wy - TOP) / np.maximum(bot - TOP, 1), 0, 1)  # 0 top, 1 underside

    # grain at the game's pixel scale, plus streaks running down to the
    # left (sheared noise), as in the rock face west of the cables
    g = _noise((H, W), 31, 2) * 0.5 + _noise((H, W), 32, 1) * 0.5
    yy, xx = np.mgrid[0:H, 0:W]
    big = _noise((H + 8, W + H + 8), 33, 3)
    streak = big[yy, np.clip(xx + yy, 0, W + H + 7)]
    streak = np.sign(streak) * np.abs(streak) ** 0.6
    g1 = np.clip(0.45 * g + 0.75 * streak, -1, 1)
    grain = np.asarray(Image.fromarray(((g1 + 1) * 127.5).clip(0, 255).astype(np.uint8)).resize((w4, h4), Image.NEAREST), np.float32) / 127.5 - 1

    col = BODY[None, None, :] * (1 - depth[..., None]) + DEEP[None, None, :] * depth[..., None]
    col = col * (1 + 0.5 * grain[..., None])
    # the lit rim along the top, two pixels deep
    rim = np.clip(1 - (wy - TOP - tipround) / 1.6, 0, 1) * 0.8
    col = col * (1 - rim[..., None]) + RIM[None, None, :] * (1 + 0.3 * grain[..., None]) * rim[..., None]
    # the dark line along the underside, one pixel
    edge = np.clip(1 - (bot - wy) / 1.4, 0, 1)
    col = col * (1 - edge[..., None]) + EDGE[None, None, :] * edge[..., None]

    rgba = np.zeros((h4, w4, 4), np.float32)
    rgba[..., :3] = np.where(inside[..., None], col, MATTE)
    rgba[..., 3] = inside * 255.0
    big = Image.fromarray(rgba.clip(0, 255).astype(np.uint8), 'RGBA')
    # box-downscale (soft edges matted onto the dark cave color)
    a = np.asarray(big, np.float32).reshape(H, SS, W, SS, 4).mean(axis=(1, 3))
    al = a[..., 3:4] / 255.0
    rgb = a[..., :3]  # already premultiplied against MATTE where outside
    out = np.zeros((H, W, 4), np.float32)
    out[..., :3] = rgb
    out[..., 3] = np.where(al[..., 0] >= 0.5, 255, 0)
    # dither the left end into the rock behind: no straight seam
    rng = np.random.default_rng(34)
    xs1 = np.arange(W)[None, :].repeat(H, 0)
    keep = rng.uniform(0, 1, (H, W)) < np.clip((xs1 - 1) / 16.0, 0, 1) ** 0.8
    out[..., 3] = np.where(keep, out[..., 3], 0)
    return Image.fromarray(out.clip(0, 255).astype(np.uint8), 'RGBA')


if __name__ == '__main__':
    base = sys.argv[1]
    im = draw()
    im.save(base + '.png')
    s16.write(base + '.s16', [im])
