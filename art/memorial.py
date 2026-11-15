"""Draw the memorial stone and its tablet (owner, 2026-11-15: an Easter egg
for "the four who grew old together", four norns that hatched within
seconds of each other and died of old age at the same moment).

The stone stands in the dark brick niche of the stone arcade west of the
incubator (x about 5010, floor 1172), so it is in that arcade's materials:
the gray, slightly warm stone of its twisted pillars and arches, lit warm
from the wall lamp to its left. It is a small round-topped headstone on a
plinth, drawn as a lit height field for volume (rounded, chipped edges,
grain, pits, a hairline crack, dirt streaks and pale lichen, shade where it
meets the plinth, the plinth's top face catching the light), with four small round marks carved side by side in a gentle arc
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
from lift import write_png, blur

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


def fbm(w, h, seed, scales):
    """Several octaves of value noise, -1 to 1."""
    out = np.zeros((h * SS, w * SS), np.float32)
    total = 0
    for k, (sc, amp) in enumerate(scales):
        out += (noise(w, h, seed * 31 + k, sc) * 2 - 1) * amp
        total += amp
    return out / total


def shade(hgt, L=(-0.62, -0.55, 0.56), relief=1.0, shine=18):
    """Lambert light on a height field (px units): light from the upper
    left and in front (the arcade's wall lamp)."""
    gy, gx = np.gradient(hgt * relief, 1 / SS)
    n = np.dstack([-gx, -gy, np.ones_like(hgt)])
    n /= np.linalg.norm(n, axis=2, keepdims=True)
    L = np.asarray(L, np.float32)
    L = L / np.linalg.norm(L)
    diff = np.clip((n * L).sum(2), 0, 1)
    # a little shine where the surface turns to the light
    h = L + np.array((0, 0, 1), np.float32)
    h = h / np.linalg.norm(h)
    spec = np.clip((n * h).sum(2), 0, 1) ** shine
    return diff, spec, n


# looks tried for the stone (owner, 2026-11-15: mock-ups of a few versions)
LOOKS = {
    'gray': dict(base=(146, 140, 128), shape='round', chips=9, moss=1.0, lichen=1.0, crack=True, plinth=True),
    'old': dict(base=(112, 112, 102), shape='round', chips=16, moss=3.0, lichen=2.6, crack=True, plinth=True),
    'sand': dict(base=(176, 152, 118), shape='gable', chips=12, moss=0.7, lichen=0.8, crack=False, plinth=True),
    'standing': dict(base=(132, 128, 118), shape='rough', chips=6, moss=1.6, lichen=1.6, crack=True, plinth=False),
    # chosen (owner, 2026-11-15): "number 2, but make it darker and give it
    # more of a 3D-rendered feel"
    'final': dict(base=(88, 90, 84), shape='round', chips=16, moss=3.0, lichen=2.0, crack=True, plinth=True, rendered=True),
}


def stone(look='final'):
    o = LOOKS[look]
    R = o.get('rendered', False)
    W, H = (52 if R else 46), 56  # (rendered: room on the right for its shadow on the wall)
    x, y = grid(W, H)
    rgb = np.zeros(x.shape + (3,), np.float32)
    a = np.zeros(x.shape, np.float32)
    warm = np.array((1.0, 0.86, 0.66), np.float32)   # the lamp's light
    cool = np.array((0.62, 0.66, 0.74), np.float32)  # the niche's shade
    base = np.array(o['base'], np.float32)            # the arcade's pillar gray, warm
    grain = fbm(W, H, 3, ((1.3, 0.5), (2.6, 0.8), (5, 0.6), (10, 0.4)))
    big = fbm(W, H, 4, ((6, 0.6), (16, 1.0)))

    # a soft shadow on the floor and the plinth's foot, so it sits down
    sh = np.hypot((x - 24) / 21, (y - 54.5) / 2.6) - 1
    rgb, a = over(rgb, a, (12, 10, 8), np.clip(-sh * 1.3, 0, 1) * 0.6)

    # -- the plinth: a worn block whose top face shows (seen a little from above)
    px0, px1, ptop, pfront, pbot = 4.5, 41.5, 43.5, 47.5, 54.0
    edge_rag = fbm(W, H, 5, ((1.2, 0.6), (3, 0.4))) * 0.7
    pbox = sdf_box(x, y, (px0 + px1) / 2, (ptop + pbot) / 2, (px1 - px0) / 2, (pbot - ptop) / 2, 1.6) + edge_rag
    pc = cover(pbox) * (1.0 if o['plinth'] else 0.0)
    topface = np.clip((pfront - y) / 1.2, 0, 1)            # 1 on the top face
    ph = np.clip(-pbox / 1.8, 0, 1) ** 0.7 * 2.2 + grain * 0.35 + big * 0.25
    if R:
        ph = blur(np.clip(-pbox / 2.4, 0, 1) ** 0.7 * 2.8 + grain * 0.12 + big * 0.35, 0.3)
    diff, spec, _ = shade(ph, relief=1.4 if R else 1.0, shine=30 if R else 18)
    lit = (0.12 + 1.0 * diff) if R else (0.30 + 0.85 * diff)
    lit = lit * (1 + 0.42 * topface)                        # the top face catches more light
    lit = lit * (1 - 0.22 * np.clip((y - pfront) / (pbot - pfront), 0, 1))  # the front darker lower down
    alb = base * (0.9 + 0.18 * grain[..., None] + 0.1 * big[..., None])
    col = alb * (lit[..., None] * warm * 0.85 + (1 - lit[..., None]) * cool * 0.35) + spec[..., None] * (50 if R else 30)
    # the top face's front edge: a thin worn highlight, then the front in shade
    lip = np.clip(1 - np.abs(y - pfront) / 0.7, 0, 1) * pc
    col = col * (1 + 0.25 * lip[..., None])
    if o['plinth']:
        rgb, a = over(rgb, a, col, pc)

    # -- the headstone: a slab with a round top, chipped and weathered
    cx, top, bot, hw = 23, 6.5, 46.5, 13.5
    if not o['plinth']:
        top, bot = 3.5, 52.5  # straight on the floor, taller
    body = np.maximum(np.abs(x - cx) - hw, y - bot)
    if o['shape'] == 'round':
        roundtop = np.hypot(x - cx, y - (top + hw)) - hw
        sd = np.where(y < top + hw, roundtop, body)
    elif o['shape'] == 'gable':  # square shoulders and a low pointed top
        shoulder = top + 9
        gable = (y - top) - np.abs(x - cx) * (shoulder - top) / hw
        sd = np.maximum(np.maximum(np.abs(x - cx) - hw, y - bot), -gable * 0.8)
    else:  # 'rough': a standing stone, narrower at the top, its outline uneven
        t = np.clip((y - top) / (bot - top), 0, 1)
        lean = (1 - t) * 3.0                     # its top leans a little east
        taper = hw * (0.55 + 0.5 * t)            # narrow at the top, broad at the foot
        bump = fbm(W, H, 9, ((2.5, 0.6), (6, 1.0))) * 2.6
        side = np.abs(x - cx - lean) - taper + bump
        cap = np.hypot((x - cx - lean - 1.5) / (hw * 0.62), (y - top - 8) / 8.5) * 8.5 - 8.5 + bump * 0.6
        sd = np.where(y < top + 8, np.maximum(side, cap), np.maximum(side, y - bot))
    rag = fbm(W, H, 6, ((0.9, 0.5), (2.2, 0.7), (5, 0.5)))
    # chips: a few bites out of the rim, deeper toward the top
    rng = np.random.default_rng(17)
    chips = np.zeros_like(sd)
    for _ in range(o['chips']):
        ang = rng.uniform(-np.pi * 0.95, -np.pi * 0.05) if rng.random() < 0.7 else rng.uniform(-0.2, 0.2) + rng.choice([0, np.pi])
        r = hw - 0.2
        if ang > -0.25 or ang < -np.pi + 0.25:  # on the straight sides
            ccx = cx + np.sign(np.cos(ang)) * hw
            ccy = rng.uniform(top + hw, bot - 4)
        else:
            ccx = cx + r * np.cos(ang)
            ccy = top + hw + r * np.sin(ang)
        rad = rng.uniform(0.8, 1.9)
        chips = np.maximum(chips, np.clip(rad - np.hypot(x - ccx, y - ccy), 0, None))
    sd = sd + rag * 0.55 + chips
    c = cover(sd)
    # height: rounded edges (a bevel about 3 px deep), the face gently
    # domed, grain and pitting, the carving cut in
    bevel = np.clip(-sd / (4.6 if R else 3.2), 0, 1)
    hgt = (1 - (1 - bevel) ** 2) * (4.2 if R else 3.0)
    hgt += np.clip(1 - ((x - cx) / (hw + 1)) ** 2, 0, 1) * (3.0 if R else 2.2)
    hgt += (grain * 0.16 + big * 0.5) if R else (grain * 0.3 + big * 0.35)
    pits = np.zeros_like(hgt)
    for _ in range(46):
        px_, py_ = rng.uniform(cx - hw, cx + hw), rng.uniform(top + 2, bot - 1)
        rr = rng.uniform(0.3, 0.75)
        pits = np.maximum(pits, np.clip(1 - np.hypot(x - px_, y - py_) / rr, 0, 1))
    hgt -= pits * 0.9
    mark = four_mark(x, y, cx, 19.5, 7.0)
    cut = np.clip(-mark / 0.9, 0, 1)
    hgt -= cut * 2.2
    # a hairline crack running down from the rim on the shadow side
    t = (y - 12) / 22
    crackx = cx + 7.5 + np.sin(t * 5.1) * 1.4 + t * 2.5
    crack = np.clip(1 - np.abs(x - crackx) / 0.35, 0, 1) * ((t > 0) & (t < 1))
    if o['crack']:
        hgt -= crack * 0.8
    else:
        crack = crack * 0
    if R:
        hgt = blur(hgt, 0.35)  # a smooth, modeled surface rather than noise
    diff, spec, n = shade(hgt, relief=1.6 if R else 1.3, shine=30 if R else 18)
    if R:
        # a cool fill light from the right, and the crevices' own shade
        fill, _, _ = shade(hgt, L=(0.75, -0.15, 0.5), relief=1.6)
        cavity = np.clip((blur(hgt, 1.2) - hgt) * 1.4, 0, 1)
    # ambient occlusion: darker in the carving, the pits, the crack and at
    # the foot where the stone meets the plinth
    ao = 1 - 0.6 * cut - 0.35 * pits - 0.5 * crack
    ao *= 1 - 0.35 * np.clip(1 - (bot - y) / 5, 0, 1)
    # albedo: the gray, lighter and darker patches, streaks of dirt run down
    # from the top, pale lichen
    alb = base * ((0.94 + 0.08 * grain[..., None] + 0.12 * big[..., None]) if R else (0.88 + 0.2 * grain[..., None] + 0.12 * big[..., None]))
    streak = noise(W, H, 41, 3)  # stretched down the stone
    streak = np.asarray(Image.fromarray((streak * 255).astype(np.uint8)).resize((W * SS // 3, H * SS)).resize((W * SS, H * SS)), np.float32) / 255
    alb = alb * (1 - 0.16 * np.clip((streak - 0.5) * 3, 0, 1)[..., None] * np.clip((y - top) / 30, 0.3, 1)[..., None])
    lichen = np.clip((noise(W, H, 43, 1.6) - 0.8 + 0.08 * o['lichen']) * 6, 0, 1) * np.clip((y - top - 4) / 10, 0, 1) * np.clip((x - cx + 4) / 10, 0, 1)
    alb = alb * (1 - lichen[..., None] * 0.35) + lichen[..., None] * np.array((168, 170, 120), np.float32) * 0.35
    if R:
        ao = ao * (1 - 0.7 * cavity) + 0.18 * cut  # (the carving dark, not black)
        lit = (0.08 + 1.15 * diff ** 1.2) * ao
        col = alb * (lit[..., None] * warm * 1.0 + 0.22 * fill[..., None] * cool * ao[..., None] + 0.08 * cool) + spec[..., None] * 62 * ao[..., None] * warm
    else:
        lit = (0.2 + 0.95 * diff) * ao
        col = alb * (lit[..., None] * warm * 0.88 + (1 - lit[..., None]) * cool * 0.3) + spec[..., None] * 34 * ao[..., None]
    # the rim on the lamp's side catches warm light
    rim = np.clip(1 + sd / 1.4, 0, 1) * np.clip((cx - x) / hw, 0, 1)
    col = col + rim[..., None] * np.array((40, 28, 12), np.float32)
    rgb, a = over(rgb, a, col, c)

    # -- moss: clumps on the plinth's top and up the stone's foot, bumpy
    mn = noise(W, H, 12, 1.4) * 0.6 + noise(W, H, 13, 3.5) * 0.4
    band = (np.clip(1 - np.abs(y - 45.8) / 2.6, 0, 1) + 0.6 * np.clip(1 - np.abs(y - 42) / 3, 0, 1) * np.clip((x - cx) / -6, 0, 1)) * o['moss'] ** 0.5
    band = band + (o['moss'] > 1.5) * 0.55 * np.clip(1 - np.abs(y - 36) / 8, 0, 1) * np.clip((cx + 2 - x) / 8, 0, 1)
    if o['moss'] > 2:
        creep = np.clip(1 - (bot - y) / 30, 0, 1) * 0.9 + np.clip((x - cx) / hw, 0, 1) * 0.4 + np.clip((top + 7 - y) / 6, 0, 1) * 0.7
        band = np.maximum(band, creep * (0.7 + 0.5 * noise(W, H, 14, 2.5)))
    mc = np.clip((mn * band - 0.32) * 4.5, 0, 1) * np.clip(np.maximum(c, pc), 0, 1)
    if R:
        # cushions: height from the coverage itself, so each clump bulges
        mh = blur(mc, 0.5) * 2.2 + mn * 0.8
        mdiff, mspec, _ = shade(mh, relief=1.5, shine=12)
        under = np.clip(np.roll(np.roll(blur(mc, 0.6), int(0.8 * SS), 0), int(0.6 * SS), 1) - mc, 0, 1)
        rgb = rgb * (1 - 0.45 * under[..., None])
        mcol = np.array((92, 126, 50), np.float32) * (0.42 + 0.95 * mdiff[..., None]) * warm + mspec[..., None] * 22
        rgb, a = over(rgb, a, mcol, np.clip(blur(mc, 0.25) * 1.35, 0, 1))
    else:
        mh = mn * 2.5
        mdiff, _, _ = shade(mh, relief=1.0)
        mcol = np.array((88, 116, 54), np.float32) * (0.45 + 0.75 * mdiff[..., None]) + np.array((30, 30, 0), np.float32) * (mn[..., None] > 0.6)
        rgb, a = over(rgb, a, mcol, mc * 0.95)

    if R:
        # the lamp throws the stone's shadow onto the wall behind it, to the
        # right and a little up (drawn under the stone and plinth)
        sil = np.clip(c + pc, 0, 1)
        cast = np.roll(np.roll(sil, int(-1.5 * SS), 0), int(4.5 * SS), 1)
        cast[:, :int(4.5 * SS)] = 0
        cast = blur(cast, 1.6) * 0.5
        A = a + cast * (1 - a)
        rgb = (rgb * a[..., None] + np.array((8, 6, 5), np.float32) * (cast * (1 - a))[..., None]) / np.maximum(A, 1e-4)[..., None]
        a = A

    # -- four small pale flowers at the foot, two each side
    fl = ((6.5, 46.5, 6), (10, 48, 4.5), (36.5, 48, 4.5), (40, 46.5, 6)) if o['plinth'] else ((5, 52, 6), (8.5, 53, 4.5), (38, 53, 4.5), (41.5, 52, 6))
    for fx, fy, h in fl:
        stem = np.abs(x - fx) - 0.45
        stem = np.maximum(stem, np.maximum(y - (50 if o['plinth'] else 54.5), (fy - h) - y))
        rgb, a = over(rgb, a, (64, 98, 46), cover(stem))
        for k, (lx, ly) in enumerate(((-1.6, -1.2), (1.6, -2.2))):
            leaf = np.hypot((x - fx - lx) / 1.4, (y - (fy - h * 0.35) - ly * 0.4) / 0.7) - 1
            rgb, a = over(rgb, a, (80, 120, 56), cover(leaf * 1.2))
        hy = fy - h
        for p in range(5):
            ang = p * 2 * np.pi / 5 - np.pi / 2
            pd2 = np.hypot(x - fx - 1.2 * np.cos(ang), y - hy - 1.2 * np.sin(ang)) - 0.95
            lightp = 1.0 if np.cos(ang) < 0.3 else 0.82  # petals on the lamp's side brighter
            rgb, a = over(rgb, a, np.array((236, 230, 246), np.float32) * lightp, cover(pd2))
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
