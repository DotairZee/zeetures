"""Draw the memorial stone and its tablet (owner, 2026-11-15: an Easter egg
for "the four who grew old together", four norns that hatched within
seconds of each other and died of old age at the same moment).

The stone stands in the dark brick niche of the stone arcade west of the
incubator (x about 5010, floor 1172), so it is in that arcade's materials:
the grey, slightly warm stone of its twisted pillars and arches, lit warm
from the wall lamp to its left. It is a small round-topped headstone on a
plinth, with four small round marks carved side by side in a gentle arc
(the four, together), moss on the plinth, and four small pale flowers growing
at its foot. Pressing it opens the tablet: a weathered sandstone panel
with a carved border and the same four marks, on which
memorial.cos's window writes the words (announcements.txt, 40 and 41).

Drawn from scratch at 4x (no game pixels are read or copied), so the
output can ship with the project.

    python3 memorial.py OUT_DIR

writes OUT_DIR/zmem.png (the stone) and OUT_DIR/zmtb.png (the tablet), and
OUT_DIR/memorial-preview.png.
"""
import os
import sys
import numpy as np
from PIL import Image, ImageFilter
from lift import write_png

SS = 4  # supersampling


def grid(w, h):
    y, x = np.mgrid[0:h * SS, 0:w * SS].astype(np.float32)
    return (x + 0.5) / SS, (y + 0.5) / SS


def noise(w, h, seed, scale):
    """Smooth value noise in 0-1 at about `scale` px."""
    rng = np.random.default_rng(seed)
    small = rng.random((max(2, int(h / scale) + 2), max(2, int(w / scale) + 2))).astype(np.float32)
    im = Image.fromarray((small * 255).astype(np.uint8)).resize((w * SS, h * SS), Image.BICUBIC)
    return np.asarray(im, np.float32) / 255


def finish(rgb, alpha, w, h):
    im = Image.fromarray(np.dstack([np.clip(rgb, 0, 255), np.clip(alpha, 0, 1) * 255]).astype(np.uint8), 'RGBA')
    return im.resize((w, h), Image.LANCZOS)


def over(rgb, a, col, ca):
    """Paint col (HxWx3 or 3) with coverage ca over (rgb, a)."""
    col = np.broadcast_to(np.asarray(col, np.float32), rgb.shape)
    ca = ca[..., None]
    rgb = rgb * (1 - ca) + col * ca
    return rgb, np.maximum(a, ca[..., 0])


def sdf_box(x, y, cx, cy, hw, hh, r):
    qx = np.abs(x - cx) - (hw - r)
    qy = np.abs(y - cy) - (hh - r)
    return np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - r


def cover(d, soft=0.6):
    return np.clip(0.5 - d / soft, 0, 1)


def four_mark(x, y, cx, cy, size):
    """Distance field of four small round marks side by side in a gentle
    arc (the four, together)."""
    d = np.full(x.shape, 1e9, np.float32)
    for k in range(4):
        t = (k - 1.5) / 1.5
        px = cx + t * size
        py = cy + (t * t) * size * 0.28
        d = np.minimum(d, np.hypot(x - px, y - py) - size * 0.27)
    return d


def carve(rgb, d, depth_dark=0.55, lit=1.25, x=None, y=None):
    """A chiseled groove where d < 0: dark inside, its lower-right lip lit
    (light from the upper left)."""
    inside = cover(d, 0.8)
    dy = np.gradient(d, axis=0)
    dx = np.gradient(d, axis=1)
    # the groove's wall facing the light (normal pointing up-left) is dark,
    # the one facing away catches light on its lip
    edge = np.clip(1 - np.abs(d) / 0.9, 0, 1)
    facing = np.clip(-(dx + dy) * SS * 0.7, -1, 1)
    out = rgb * (1 - inside[..., None] * (1 - depth_dark))
    out = out * (1 + edge[..., None] * np.clip(facing, 0, 1)[..., None] * (lit - 1))
    return out


def stone():
    W, H = 46, 56
    x, y = grid(W, H)
    rgb = np.zeros(x.shape + (3,), np.float32)
    a = np.zeros(x.shape, np.float32)
    base = np.array((132, 127, 118), np.float32)       # the arcade's pillar grey, warm, in the niche's shade
    tex = noise(W, H, 7, 2.5) * 0.6 + noise(W, H, 8, 7) * 0.4
    warm = np.array((255, 214, 160), np.float32) / 255  # the lamp's light, from the left
    # the plinth
    pd = sdf_box(x, y, 23, 50.5, 19, 4.5, 1.2)
    # the headstone: a slab with a round top
    cx, top, bot, hw = 23, 8, 47, 13
    body = np.maximum(np.abs(x - cx) - hw, y - bot)
    roundtop = np.hypot(x - cx, y - (top + hw)) - hw
    sd = np.where(y < top + hw, roundtop, body)
    for d, shade in ((pd, 0.9), (sd, 1.0)):
        c = cover(d)
        light = np.clip(0.75 + (cx - x) / 40 + (30 - y) / 160, 0.45, 1.15)
        col = base[None, None, :] * shade * (0.82 + 0.3 * tex[..., None]) * light[..., None]
        col = col * (0.7 + 0.3 * warm)
        # a soft rounded edge: darker toward the rim on the shadow side
        rim = np.clip(1 + d / 2.2, 0, 1)
        col = col * (1 - 0.25 * rim[..., None] * np.clip((x - cx) / hw, 0, 1)[..., None])
        rgb, a = over(rgb, a, col, c)
    # the carving: four small round marks side by side
    rgb = carve(rgb, four_mark(x, y, cx, 20, 7.0))
    # weathering: a few darker stains running down from the top
    stain = noise(W, H, 11, 4)
    rgb = rgb * (1 - 0.12 * np.clip((stain - 0.55) * 3, 0, 1)[..., None] * cover(sd)[..., None])
    # moss on the plinth and at the stone's foot
    moss = noise(W, H, 12, 2) * np.clip(1 - np.abs(y - 46.5) / 3.5, 0, 1)
    mc = np.clip((moss - 0.25) * 3, 0, 1) * np.clip(cover(np.minimum(pd, sd)) * 1.0, 0, 1)
    rgb, a = over(rgb, a, np.array((92, 112, 58), np.float32) * (0.8 + 0.4 * tex[..., None]), mc * 0.85)
    # four small pale flowers at the foot, two each side
    for fx, fy, h in ((5.5, 46.5, 6), (9.5, 48, 4.5), (37.5, 48, 4.5), (41, 46.5, 6)):
        stem = np.abs(x - fx) - 0.45
        stem = np.maximum(stem, np.maximum(y - 50, (fy - h) - y))
        rgb, a = over(rgb, a, (70, 104, 50), cover(stem))
        for k, (lx, ly) in enumerate(((-1.6, -1.2), (1.6, -2.2))):
            leaf = np.hypot((x - fx - lx) / 1.4, (y - (fy - h * 0.35) - ly * 0.4) / 0.7) - 1
            rgb, a = over(rgb, a, (84, 122, 58), cover(leaf * 1.2))
        hy = fy - h
        for p in range(5):
            ang = p * 2 * np.pi / 5 - np.pi / 2
            pd2 = np.hypot(x - fx - 1.2 * np.cos(ang), y - hy - 1.2 * np.sin(ang)) - 0.95
            rgb, a = over(rgb, a, (236, 230, 246), cover(pd2))
        rgb, a = over(rgb, a, (238, 196, 92), cover(np.hypot(x - fx, y - hy) - 0.6))
    return finish(rgb, a, W, H)


def tablet():
    W, H = 340, 236
    x, y = grid(W, H)
    rgb = np.zeros(x.shape + (3,), np.float32)
    a = np.zeros(x.shape, np.float32)
    sand = np.array((206, 192, 162), np.float32)
    tex = noise(W, H, 21, 3) * 0.5 + noise(W, H, 22, 14) * 0.3 + noise(W, H, 23, 40) * 0.2
    outer = sdf_box(x, y, W / 2, H / 2, W / 2 - 3, H / 2 - 3, 14)
    # a dark drop shadow, so it reads over any part of the world
    shadow = sdf_box(x, y, W / 2 + 3, H / 2 + 3, W / 2 - 3, H / 2 - 3, 14)
    rgb, a = over(rgb, a, (20, 16, 12), cover(shadow, 5) * 0.55)
    light = np.clip(1.06 - (x / W) * 0.12 - (y / H) * 0.1, 0.8, 1.1)
    col = sand[None, None, :] * (0.84 + 0.28 * tex[..., None]) * light[..., None]
    # bevel: the rim lit on the upper left, shaded on the lower right
    bev = np.clip(1 + outer / 6, 0, 1)
    dx = np.gradient(outer, axis=1)
    dy = np.gradient(outer, axis=0)
    facing = np.clip(-(dx + dy) * SS, -1, 1)
    col = col * (1 + 0.22 * bev[..., None] * facing[..., None])
    rgb, a = over(rgb, a, col, cover(outer))
    # a carved border line
    border = np.abs(sdf_box(x, y, W / 2, H / 2, W / 2 - 13, H / 2 - 13, 9)) - 0.9
    rgb = carve(rgb, border, 0.62)
    # the four marks, small, at the top middle
    rgb = carve(rgb, four_mark(x, y, W / 2, 24, 9.0), 0.6)
    # little leaves either side of it
    for s in (-1, 1):
        stem = np.abs(y - 26) - 0.7  # (the leaves' line, beside the marks)
        stem = np.maximum(stem, np.maximum(np.abs(x - W / 2) - 44, 14 - np.abs(x - W / 2)))
        rgb = carve(rgb, stem, 0.7)
        for k in range(3):
            lx = W / 2 + s * (20 + k * 9)
            leaf = np.hypot((x - lx) / 3.2, (y - 26 + (2.6 if k % 2 else -2.6)) / 1.5) - 1
            rgb = carve(rgb, leaf, 0.7)
    # weathering: soft stains and chips at the edge
    stain = noise(W, H, 24, 20)
    rgb = rgb * (1 - 0.07 * np.clip((stain - 0.5) * 3, 0, 1)[..., None])
    return finish(rgb, a, W, H)


if __name__ == '__main__':
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    st, tb = stone(), tablet()
    write_png(os.path.join(out, 'zmem.png'), [st])
    write_png(os.path.join(out, 'zmtb.png'), [tb])
    pv = Image.new('RGBA', (tb.width + st.width * 4 + 30, max(tb.height, st.height * 4) + 20), (40, 36, 32, 255))
    pv.alpha_composite(tb, (10, 10))
    pv.alpha_composite(st.resize((st.width * 4, st.height * 4), Image.NEAREST), (tb.width + 20, 10))
    pv.save(os.path.join(out, 'memorial-preview.png'))
    print('wrote', out)
