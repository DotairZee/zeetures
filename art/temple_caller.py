"""The temple top floor's lift caller (owner, 2026-11-27): the pale horned
sculpture on the column cap west of the shaft made the caller, by an amber
gem set in a gold cup in its brow, at the foot of its stone cone. Our own
drawing, no game pixels: it is laid over the background's sculpture.

- Frame 0, idle: a deep, dim amber, a faint glow within; a gold bezel with
  depth (a lit rim, a shadowed lip over the stone).
- Frame 1, called: lit from within, white-gold at its heart, a soft
  checkered glow round it (as the game's other glows are dithered), and
  the bezel and the stone close by catching its light.

Both have a crisp highlight upper left and a soft second reflection lower
right; the gold is worn as the temple's other metal is (art/temple_pulley.py).
Creatures can only stand east of x 4183 on that floor, and a press, like a
click, uses the picture's box, so the box reaches on east, transparent, to
x 4188, and down to y 392.

    python3 temple_caller.py OUT.png

writes the two pictures stacked top to bottom ('openc2e-frames' 2) and
prints the top-left world corner and size (additions/temple-top.cos uses
it: NEW: CALL ... ztcb 2 0 ...).
"""
import sys
import math
import numpy as np
from PIL import Image, PngImagePlugin

import temple_pulley as tp

OX, OY, R = 4112.0, 320.0, 4.6         # the gem: centre, radius (the cone's foot)
BEZEL_IN, BEZEL_OUT = 4.7, 6.6
BOX = (4103, 311, 4188, 392)           # the picture's box (x0, y0, x1, y1)
HALO = (6.9, 12.5)                     # the called glow: from, to (px)


def area():
    x0, y0, x1, y1 = BOX
    tp.X0, tp.Y0, tp.X1, tp.Y1 = x0, y0, x1, y1
    tp.W, tp.H = (x1 - x0) * tp.SS, (y1 - y0) * tp.SS
    cv = tp.Canvas()
    tp.WEAR = tp.Wear(cv)
    return cv


def gem(cv, on):
    dx, dy = (cv.wx - OX) / R, (cv.wy - OY) / R
    q = dx * dx + dy * dy
    m = q <= 1
    r = np.sqrt(np.clip(q, 0, 1))
    nz = np.sqrt(np.clip(1 - q, 0, 1))
    # light enters at the upper left and gathers low right, as in glass
    focus = np.clip(1 - np.hypot(dx - 0.32, dy - 0.38) / 1.1, 0, 1) ** 1.6
    if on:
        heart = np.array([255, 250, 214], float)
        mid = np.array([255, 196, 84], float)
        rim = np.array([214, 104, 28], float)
        glow_in = np.clip(1 - np.hypot(dx - 0.08, dy - 0.1) / 0.95, 0, 1) ** 1.2
        col = rim[None, None] * (1 - glow_in[..., None]) + mid[None, None] * glow_in[..., None]
        col = col * (1 - (glow_in ** 2.4)[..., None]) + heart[None, None] * (glow_in ** 2.4)[..., None]
    else:
        deep = np.array([74, 30, 8], float)
        amber = np.array([170, 84, 22], float)
        warm = np.array([226, 138, 44], float)
        col = deep[None, None] * (1 - focus[..., None]) + amber[None, None] * focus[..., None]
        inner = np.clip(focus * 1.4 - 0.55, 0, 1)                      # a faint glow within
        col = col * (1 - inner[..., None]) + warm[None, None] * inner[..., None]
        # the dark rim of a cut stone, and the upper left shaded (the light
        # passes through and comes out low right)
        col = col * (0.62 + 0.38 * nz)[..., None]
    # a crisp highlight upper left, a soft second reflection lower right
    spec = np.hypot((dx + 0.38) / 0.30, (dy + 0.44) / 0.22) <= 1
    spec_soft = np.clip(1 - np.hypot((dx + 0.38) / 0.52, (dy + 0.44) / 0.40), 0, 1) * 0.45
    refl = np.clip(1 - np.hypot((dx - 0.42) / 0.30, (dy - 0.50) / 0.16), 0, 1) * (0.32 if not on else 0.18)
    white = np.array([255, 252, 236], float)
    col = col * (1 - spec_soft[..., None]) + white * spec_soft[..., None]
    col = col * (1 - refl[..., None]) + np.array([255, 214, 160], float) * refl[..., None]
    col = np.where(spec[..., None], white, col)
    cv.paint(m, col)


def bezel(cv, on):
    # the cup's outer slope and lit rim, then the lip's shadow on the gem
    tp.ring(cv, OX, OY, BEZEL_IN, BEZEL_OUT, 'gold')
    dx, dy = cv.wx - OX, cv.wy - OY
    r = np.hypot(dx, dy)
    lip = (r >= BEZEL_IN - 0.55) & (r < BEZEL_IN)
    cv.c = np.where(lip[..., None], cv.c * 0.45, cv.c)
    # four little prongs over the gem's edge (upper left lit)
    for k in range(4):
        a = math.pi / 4 + k * math.pi / 2
        tp.ellipsoid(cv, OX + math.cos(a) * (R + 0.2), OY + math.sin(a) * (R + 0.2), 0.95, 0.95, 'gold')
    if on:
        # the gold catching the light from within, strongest nearest the gem
        warm = np.array([255, 214, 120], float)
        k = np.clip(1 - (r - BEZEL_IN) / (BEZEL_OUT - BEZEL_IN + 0.6), 0, 1) * 0.55 * (cv.a > 0) * (r >= BEZEL_IN - 0.6)
        cv.c = cv.c * (1 - k[..., None]) + warm * k[..., None]


def halo(out, on):
    """The called glow, checkered: amber pixels thinning out from the bezel,
    the inner ring warmer; a few pale-gold on the stone closest (its light on
    the brow)."""
    if not on:
        return out
    bayer = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0
    x0, y0 = BOX[0], BOX[1]
    h, w = out.shape[:2]
    r0, r1 = HALO
    for y in range(h):
        for x in range(w):
            if out[y, x, 3]:
                continue
            d = math.hypot(x0 + x + 0.5 - OX, y0 + y + 0.5 - OY)
            if d < r0 - 0.6 or d > r1:
                continue
            a = (1 - (d - r0) / (r1 - r0)) ** 1.8 * 0.62
            if bayer[(y0 + y) % 4, (x0 + x) % 4] < a:
                t = np.clip((d - r0) / (r1 - r0), 0, 1)
                c = np.array([255, 232, 150]) * (1 - t) + np.array([255, 168, 60]) * t
                out[y, x] = (int(c[0]), int(c[1]), int(c[2]), 255)
    return out


def frame(on):
    cv = area()
    bezel(cv, on)
    gem(cv, on)
    out = tp.finish(cv)
    return halo(out, on)


def make():
    frames = [frame(False), frame(True)]
    return frames, (BOX[0], BOX[1])


def main():
    frames, (x, y) = make()
    sheet = np.concatenate(frames, axis=0)
    info = PngImagePlugin.PngInfo()
    info.add_text('openc2e-frames', '2')
    Image.fromarray(sheet, 'RGBA').save(sys.argv[1], pnginfo=info)
    print('wrote', sys.argv[1], 'top-left', x, y, 'size', frames[0].shape[1], 'x', frames[0].shape[0], '(2 frames)')


if __name__ == '__main__':
    main()
