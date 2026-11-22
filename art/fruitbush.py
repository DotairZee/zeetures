"""Draw an original seasonal fruit bush and its four seasonal fruits, in the
soft prerendered look of C2's plants.

Inspired by JayD's Cornucopia Bush (Plato Project, a Creatures 2 COB): a
bush whose fruit changes with the season. Drawn from scratch here (no game
or COB pixels are read or copied), so the output can ship with the project.

    python3 fruitbush.py OUT_DIR

writes, for each of the bush's looks (to suit where it grows: zfbs
mossy green, zfbm machine-cave teal and rust, zfbv volcanic ember-red,
zfbw warm-rock bronze, zfbd desert sage), OUT_DIR/<look>.s16 with 12 images (stage * 4 +
season: seedling, young, full; spring, summer, autumn, winter), and
OUT_DIR/zfrt.s16, 24 images (kind * 6 + stage: dewberry, sunplum,
emberpear, frostnut; bud, half-grown, ripe, overripe, then ripe and
overripe lying on the ground: on their side except the round sunplum); and, for bushes standing on
built platforms, each look in a copper planter (planter.py): OUT_DIR/zfps,
zfpm, zfpv, zfpw.s16, 156 x 128, in the same picture order, with a PNG preview;
and the two smaller forms for tight places (see FORMS): compact zfc* (108 x
80) and slender zfn* (56 x 118), potted zfq* and zfo*, one per look.
"""
import sys
import os
import numpy as np
from PIL import Image, ImageDraw
import s16
import planter

SS = 4
BW, BH = 156, 108           # the bush, full-grown (1.5 times the first size)
FW, FH = 24, 24             # a fruit (1.5 times the first size)

# Looks for different places (each: per-season leaf ramps from deep shadow
# to full light, how leafy, extras; bark colors; where the light comes
# from; how much to tone it down for the place's light).
VARIANTS = {
    # mossy green caves and open ground (the first look)
    'moss': {
        'file': 'zfbs', 'bark': [(40, 32, 26), (88, 74, 60), (150, 136, 116)], 'light': 'above', 'dim': (0.8, 0.9),
        'spring': {'leaf': [(16, 40, 30), (40, 92, 36), (104, 160, 46), (190, 222, 92)], 'sprays': 1.0, 'blossom': ((248, 214, 228), 50)},
        'summer': {'leaf': [(12, 34, 28), (30, 78, 30), (84, 142, 38), (170, 210, 70)], 'sprays': 1.15},
        'autumn': {'leaf': [(40, 22, 12), (120, 52, 18), (200, 110, 30), (246, 196, 84)], 'sprays': 0.9, 'mix': (70, 110, 34)},
        'winter': {'leaf': [(16, 30, 30), (40, 62, 56), (84, 112, 100), (170, 196, 196)], 'sprays': 0.45, 'flecks': ((226, 238, 246), 90)},
    },
    # dark machine caves: dull teal-olive, spotted with rust
    'machine': {
        'file': 'zfbm', 'bark': [(26, 22, 20), (64, 54, 46), (120, 100, 80)], 'light': 'above', 'dim': (0.78, 0.8),
        'spring': {'leaf': [(10, 22, 22), (30, 56, 46), (72, 104, 66), (140, 160, 96)], 'sprays': 1.0, 'mix': (150, 76, 30), 'blossom': ((220, 196, 150), 30)},
        'summer': {'leaf': [(8, 20, 20), (26, 52, 42), (64, 96, 60), (128, 150, 88)], 'sprays': 1.1, 'mix': (150, 76, 30)},
        'autumn': {'leaf': [(30, 16, 10), (96, 44, 20), (160, 86, 36), (210, 150, 80)], 'sprays': 0.9, 'mix': (60, 80, 56)},
        'winter': {'leaf': [(12, 20, 22), (34, 46, 48), (70, 84, 82), (140, 150, 150)], 'sprays': 0.45, 'flecks': ((190, 120, 70), 50)},
    },
    # in and by the volcano: charred stems, ember-red leaves lit from the lava
    # below, ash and glowing cinders; seasons only shift the burn
    'volcano': {
        'file': 'zfbv', 'bark': [(14, 8, 8), (40, 24, 20), (150, 60, 24)], 'light': 'below', 'dim': (0.95, 1.0),
        'spring': {'leaf': [(12, 2, 2), (60, 6, 6), (130, 16, 12), (214, 62, 30)], 'sprays': 1.0, 'mix': (18, 12, 12), 'blossom': ((255, 150, 50), 34), 'flecks': ((255, 110, 30), 50)},
        'summer': {'leaf': [(12, 2, 2), (70, 8, 8), (146, 20, 14), (226, 76, 34)], 'sprays': 1.1, 'mix': (18, 12, 12), 'flecks': ((255, 130, 40), 60)},
        'autumn': {'leaf': [(10, 4, 4), (52, 8, 8), (112, 16, 12), (190, 50, 26)], 'sprays': 0.9, 'mix': (52, 46, 46), 'flecks': ((255, 110, 30), 70)},
        'winter': {'leaf': [(10, 8, 8), (36, 26, 26), (74, 56, 54), (140, 110, 100)], 'sprays': 0.55, 'mix': (18, 12, 12), 'flecks': ((255, 100, 24), 120)},
    },
    # the open desert above the incubator: silvery sage on pale weathered
    # wood, in full sun; yellow blossom in spring, ochre in autumn
    'desert': {
        'file': 'zfbd', 'bark': [(64, 52, 40), (128, 110, 88), (204, 188, 156)], 'light': 'above', 'dim': (1.0, 0.9),
        'spring': {'leaf': [(40, 44, 34), (98, 108, 80), (156, 166, 124), (214, 220, 182)], 'sprays': 0.9, 'blossom': ((252, 214, 76), 46)},
        'summer': {'leaf': [(44, 42, 30), (104, 104, 72), (166, 162, 112), (224, 216, 164)], 'sprays': 1.0},
        'autumn': {'leaf': [(52, 38, 22), (130, 94, 50), (192, 152, 86), (240, 208, 142)], 'sprays': 0.85, 'mix': (110, 110, 80)},
        'winter': {'leaf': [(42, 40, 36), (94, 88, 78), (152, 144, 128), (212, 206, 192)], 'sprays': 0.45},
    },
    # warm brown rock under dim amber light: bronze and olive
    'rock': {
        'file': 'zfbw', 'bark': [(30, 20, 14), (78, 56, 38), (150, 112, 70)], 'light': 'above', 'dim': (0.82, 0.92),
        'spring': {'leaf': [(22, 20, 8), (64, 68, 24), (128, 128, 46), (208, 196, 98)], 'sprays': 1.0, 'blossom': ((250, 214, 150), 40)},
        'summer': {'leaf': [(20, 18, 8), (60, 62, 22), (122, 118, 42), (198, 184, 86)], 'sprays': 1.1},
        'autumn': {'leaf': [(34, 14, 6), (110, 50, 16), (184, 104, 34), (240, 176, 84)], 'sprays': 0.9, 'mix': (80, 80, 30)},
        'winter': {'leaf': [(20, 16, 12), (56, 48, 38), (104, 92, 76), (180, 166, 146)], 'sprays': 0.45, 'flecks': ((230, 214, 190), 70)},
    },
}

def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def ramp(cols, t):
    """Color along a dark-to-light ramp, t in 0..1."""
    t = float(np.clip(t, 0, 0.999)) * (len(cols) - 1)
    i = int(t)
    return lerp(cols[i], cols[i + 1], t - i)


def skeleton(rng, x, y, ang, length, width, depth, segs, tips, spread_k=1.0):
    """A branching skeleton: tapering segments that split two or three ways."""
    pts = [(x, y)]
    for _ in range(4):
        ang += rng.normal(0, 0.12)
        x += np.cos(ang) * length / 4
        y += np.sin(ang) * length / 4
        pts.append((x, y))
    segs.append((pts, width, depth))
    if depth == 0 or width < 0.7:
        tips.append((x, y, ang))
        return
    n = 2 if rng.random() < 0.6 else 3
    for k in range(n):
        spread = (k - (n - 1) / 2) * rng.uniform(0.45, 0.75) * spread_k
        skeleton(rng, x, y, ang + spread + rng.normal(0, 0.15) * spread_k, length * rng.uniform(0.62, 0.78),
                 width * 0.66, depth - 1, segs, tips, spread_k)
        if rng.random() < 0.35:   # a side twig part way along
            tips.append((x - np.cos(ang) * length * 0.3, y - np.sin(ang) * length * 0.3, ang + spread * 2))


def leaf(d, x, y, ang, L, Wd, lit_col, shade_col, rib_col):
    """A pointed leaf along `ang`: a lit half and a shaded half, a midrib."""
    ca, sa = np.cos(ang), np.sin(ang)
    ts = np.linspace(0, 1, 9)
    half = Wd * np.sin(np.pi * ts) ** 0.75
    left = [(x + ca * L * t - sa * h, y + sa * L * t + ca * h) for t, h in zip(ts, half)]
    right = [(x + ca * L * t + sa * h, y + sa * L * t - ca * h) for t, h in zip(ts, half)]
    spine = [(x + ca * L * t, y + sa * L * t) for t in ts]
    up_is_left = (sa - ca) > 0                    # the half whose side faces the upper-left light
    first, second = (lit_col, shade_col) if up_is_left else (shade_col, lit_col)
    d.polygon([(px * SS, py * SS) for px, py in left + spine[::-1]], fill=first + (255,))
    d.polygon([(px * SS, py * SS) for px, py in right + spine[::-1]], fill=second + (255,))
    d.line([(px * SS, py * SS) for px, py in spine[:-2]], fill=rib_col + (255,), width=max(1, SS // 3))


# the smaller forms: plain and potted file prefixes, picture size, stem
# scale, slender or not, pot size
FORMS = [('zfc', 'zfq', 108, 80, 0.68, False, 52, 24),
         ('zfn', 'zfo', 56, 118, 1.5, True, 40, 24)]

# growth stages: seedling, young, full (stems, stem length, branch depth)
STAGES = [(1, 0.5, 2), (3, 0.72, 3), (5, 1.0, 4)]


def bush(season, seed, stage=2, variant='moss', BW=BW, BH=BH, size=1.0, column=False):
    """size scales the stems (1 for the full bush; the compact bush, for
    low rooms and slopes, is drawn smaller with size < 1 and the same size
    leaves, so it reads as the same plant). column: the slender upright
    form, for narrow gaps between machines and lifts: stems close together
    and nearly upright, branching narrowly, the crown a tall oval."""
    V = VARIANTS[variant]
    cfg = V[season]
    BARK = V['bark']
    below = V['light'] == 'below'
    nstem, lscale, depth = STAGES[stage]
    rng = np.random.default_rng(seed)
    w, h = BW * SS, BH * SS
    im = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    cx, cy = BW / 2, BH * 0.42

    # the skeleton: two or three stems from the base, branching upward
    segs, tips = [], []
    angles = {1: (-1.57,), 3: (-2.1, -1.57, -1.05), 5: (-2.35, -1.9, -1.57, -1.25, -0.8)}[nstem]
    if column:
        angles = {1: (-1.57,), 3: (-1.72, -1.57, -1.42), 5: (-1.8, -1.68, -1.57, -1.46, -1.34)}[nstem]
    for k, a in enumerate(angles):
        skeleton(rng, BW / 2 + (k - (nstem - 1) / 2) * (1.2 if column else 2.5), BH - 1, a + rng.normal(0, 0.08 * (0.4 if column else 1)),
                 rng.uniform(21, 27) * lscale * size, 3.6 * (0.6 + 0.4 * lscale), depth, segs, tips, 0.42 if column else 1.0)
    # keep the crown inside a dome (smaller while young, standing on the ground)
    cy = BH - (BH - BH * 0.42) * lscale - 2
    rx, ry = BW * 0.47 * lscale, BH * 0.46 * lscale
    lim = 1.15
    if column:   # leaves spill past the tips, so the tips keep well inside
        cy = BH - (BH - BH * 0.5) * lscale - 2
        rx, ry = BW * 0.2 * lscale, BH * 0.28 * lscale
        lim = 1.4
    inside = lambda x, y, m: ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 < m
    all_tips = tips
    tips = [t for t in tips if inside(t[0], t[1], lim)]
    if len(tips) < 3:   # a small seedling: keep its highest tips leafy
        tips = sorted(all_tips, key=lambda t: t[1])[:3]
    if column:   # no bare twigs reaching out past the leaves
        segs = [sg for sg in segs if sg[2] >= depth - 1 or inside(sg[0][-1][0], sg[0][-1][1], lim * 1.3)]

    def light_at(x, y):
        # sun from the upper left, plus soft patches of sun through the crown
        if below:   # lava glow from beneath and the left
            base = 0.5 - (x - cx) / BW * 0.5 + (y - cy) / BH * 1.1
        else:
            base = 0.6 - (x - cx) / BW * 0.8 - (y - cy) / BH * 1.0
        patch = 0.3 * np.sin(x * 0.13 + 1.3) * np.cos(y * 0.17 + 0.4)
        return base + patch

    dark, mid, light, sun = cfg['leaf']

    def spray(layer):
        for (tx, ty, ta) in tips:
            n = int(rng.integers(7, 14) * cfg['sprays'] * (0.8 if layer == 0 else 1))
            for _ in range(n):
                a = ta + rng.normal(0, 1.1)
                r = rng.uniform(0, 14)
                x = tx + np.cos(a) * r * 0.6 + rng.normal(0, 1.4)
                y = ty + np.sin(a) * r * 0.6 + rng.normal(0, 1.4)
                L = rng.uniform(7.5, 11)
                Wd = L * rng.uniform(0.2, 0.27)
                t = light_at(x, y) + rng.normal(0, 0.08)
                if layer == 0:       # the back leaves sit in shade, a little blue
                    t = t * 0.45 - 0.05
                lit_col = ramp(cfg['leaf'], t + 0.2)
                shade_col = ramp(cfg['leaf'], t - 0.26)
                if cfg.get('mix') and rng.random() < 0.22:
                    lit_col, shade_col = lerp(cfg['mix'], lit_col, 0.35), lerp(cfg['mix'], shade_col, 0.35)
                rib = lerp(shade_col, dark, 0.5)
                leaf(d, x - np.cos(a) * L / 2, y - np.sin(a) * L / 2, a, L, Wd, lit_col, shade_col, rib)

    spray(0)
    # branches over the back leaves: shaded body, lit upper-left edge
    for pts, width, depth in segs:
        wpx = max(1, int(width * SS))
        P = [(px * SS, py * SS) for px, py in pts]
        d.line(P, fill=BARK[0] + (255,), width=wpx + SS // 2)
        d.line(P, fill=BARK[1] + (255,), width=wpx)
        d.line([(px - wpx * 0.25, py - wpx * 0.25) for px, py in P], fill=BARK[2] + (255,), width=max(1, wpx // 3))
    spray(1)

    if cfg.get('blossom'):
        col, n = cfg['blossom']
        for _ in range(int(n * lscale)):
            tx, ty, _ = tips[int(rng.integers(len(tips)))]
            x, y = tx + rng.normal(0, 4), ty + rng.normal(0, 4)
            for k in range(5):
                a = k * 2 * np.pi / 5 + rng.uniform(0, 1)
                px, py = x + 1.4 * np.cos(a), y + 1.4 * np.sin(a)
                d.ellipse([(px - 1.2) * SS, (py - 1.2) * SS, (px + 1.2) * SS, (py + 1.2) * SS], fill=col + (255,))
            d.ellipse([(x - 0.7) * SS, (y - 0.7) * SS, (x + 0.7) * SS, (y + 0.7) * SS], fill=(236, 190, 70, 255))
    if cfg.get('flecks'):   # frost, rust, or glowing cinders
        col, n = cfg['flecks']
        for _ in range(int(n * lscale)):
            tx, ty, _ = tips[int(rng.integers(len(tips)))]
            x, y = tx + rng.normal(0, 5), ty + rng.normal(0, 5)
            r = rng.uniform(0.5, 1.1)
            d.ellipse([(x - r) * SS, (y - r) * SS, (x + r) * SS, (y + r) * SS], fill=col + (255,))
    return finish(im, BW, BH)


def finish(im, W, H, matte=(40, 44, 36)):
    small = im.resize((W, H), Image.BOX)
    a = np.asarray(small).astype(np.float32)
    al = a[..., 3:4] / 255.0
    a[..., :3] = a[..., :3] * al + np.array(matte, np.float32) * (1 - al)
    a[..., 3] = np.where(al[..., 0] < 0.35, 0, 255)
    return Image.fromarray(a.clip(0, 255).astype(np.uint8), 'RGBA')


def ball(d, cx, cy, r, base, light, dark, hl=True, u=SS, rim=True):
    # a shaded sphere lit from the upper left, by concentric steps, with a
    # dark rim (u: drawing pixels per unit)
    if rim:
        rc = lerp(dark, (0, 0, 0), 0.7)
        d.ellipse([(cx - r - 0.7) * u, (cy - r - 0.7) * u, (cx + r + 0.7) * u, (cy + r + 0.7) * u], fill=rc + (255,))
    steps = 14
    for i in range(steps):
        t = i / (steps - 1)
        rr = r * (1 - 0.75 * t)
        ox, oy = -r * 0.32 * t, -r * 0.32 * t
        c = lerp(dark, base, min(1, t * 1.6)) if t < 0.6 else lerp(base, light, (t - 0.6) / 0.4)
        d.ellipse([(cx + ox - rr) * u, (cy + oy - rr) * u, (cx + ox + rr) * u, (cy + oy + rr) * u], fill=c + (255,))
    if hl:
        hr = max(0.6, r * 0.2)
        d.ellipse([(cx - r * 0.45 - hr) * u, (cy - r * 0.45 - hr) * u, (cx - r * 0.45 + hr) * u, (cy - r * 0.45 + hr) * u], fill=(255, 252, 240, 255))


# fruits lying on their side once they are on the ground (the round sunplum
# stays as it is)
LIES_DOWN = ('dewberry', 'emberpear', 'frostnut')


def fruit(kind, over, side=False):
    """A ripe (or overripe) fruit. Shapes are laid out on a 16-unit grid and
    drawn at FW / 16 pixels per unit. side: lying on its side, stem to the
    east, still lit from the upper left; either way it rests on the bottom."""
    u = SS * FW / 16
    im = Image.new('RGBA', (FW * SS, FH * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    # stems and leaves on their own layer: thin, so they keep their color
    # instead of becoming all outline
    top = Image.new('RGBA', (FW * SS, FH * SS), (0, 0, 0, 0))
    dt = ImageDraw.Draw(top)
    dim = 0.7 if over else 1.0
    side = side and kind in LIES_DOWN

    def T(x, y):
        # a quarter turn clockwise about the grid's center (top goes east)
        return (16 - y, x) if side else (x, y)

    def P(pts):
        return [(T(x, y)[0] * u, T(x, y)[1] * u) for x, y in pts]

    def oval(x0, y0, x1, y1, col):
        (ax, ay), (bx, by) = T(x0, y0), T(x1, y1)
        d.ellipse([min(ax, bx) * u, min(ay, by) * u, max(ax, bx) * u, max(ay, by) * u], fill=col + (255,))

    def bl(x, y, r, base, light, dark, hl=True, rim=True):
        # matte, like the game's fruit: a softer light and no glossy spot
        cx, cy = T(x, y)
        ball(d, cx, cy, r, base, lerp(base, light, 0.45), dark, hl=False, u=u, rim=rim)

    def line(p, q, col):
        dt.line(P([p, q]), fill=col + (255,), width=max(1, int(u * 1.3)))

    def cap(x0, y0, x1, y1, col):
        # the top half of an oval (a nut's cap)
        cx, cy, rx, ry = (x0 + x1) / 2, (y0 + y1) / 2, (x1 - x0) / 2, (y1 - y0) / 2
        pts = [(cx + rx * np.cos(t), cy + ry * np.sin(t)) for t in np.linspace(np.pi, 2 * np.pi, 24)]
        d.polygon(P(pts), fill=col + (255,))

    def c(col):
        if over:   # overripe: darker, browner
            col = lerp(col, (70, 46, 26), 0.35)
        return tuple(int(v * dim) for v in col)
    if kind == 'dewberry':      # spring: a raspberry-like berry of round beads, with a green cap
        oval(3.2, 3.6, 12.8, 14.4, c((70, 14, 34)))
        for (x, y) in ((6, 6.4), (8.4, 5.8), (10.4, 7), (5.2, 8.8), (7.6, 8.6), (10, 9.4), (6.2, 11.2), (8.6, 11.4), (7.6, 13)):
            bl(x, y, 1.6 * (0.88 if over else 1), c((206, 52, 92)), c((255, 170, 196)), c((110, 18, 50)))
        d.polygon(P([(5.6, 4.8), (8, 2.6), (10.4, 4.8), (8, 4)]), fill=c((76, 128, 44)) + (255,))
        line((8, 3), (8.4, 0.6), c((70, 110, 40)))
    elif kind == 'sunplum':     # summer: a golden-orange plum
        bl(8, 9, 5.6 * (0.92 if over else 1), c((236, 150, 34)), c((255, 226, 130)), c((150, 64, 10)))
        line((8.4, 3.8), (9.6, 1), c((86, 60, 30)))
        dt.polygon(P([(9.6, 1.6), (13, 0.6), (11.4, 3)]), fill=c((80, 140, 50)) + (255,))
    elif kind == 'emberpear':   # autumn: a russet-red pear
        # one pear-shaped body: the neck and the belly as one, outlined together
        bl(8, 6.2, 3.0 * (0.92 if over else 1), c((182, 72, 34)), c((246, 160, 100)), c((90, 26, 10)), rim=False)
        bl(8, 10.2, 4.8 * (0.92 if over else 1), c((176, 60, 30)), c((246, 150, 90)), c((86, 22, 8)), rim=False)
        line((8, 3.4), (7.2, 0.8), c((70, 46, 24)))
    else:                       # winter: a glossy pale-blue nut under a scaly cap
        bl(8, 10, 4.7 * (0.92 if over else 1), c((160, 186, 210)), c((240, 248, 255)), c((70, 92, 122)))
        cap(3, 2.4, 13, 9.8, c((60, 42, 26)))
        cap(3.6, 2.9, 12.4, 9.2, c((124, 92, 60)))
        for (x, y) in ((5.2, 5.4), (7, 4.4), (9, 4.4), (10.8, 5.4), (6.2, 6.2), (8, 5.6), (9.8, 6.2)):
            oval(x - 0.7, y - 0.5, x + 0.7, y + 0.5, c((160, 124, 84)))
        line((8, 3), (8.6, 0.6), c((70, 50, 30)))
    # rest on the bottom of the picture, so a fruit on the floor sits on it
    a = np.asarray(im)
    rows = np.nonzero(a[..., 3].max(1) > 0)[0]
    drop = (FH * SS - 1) - rows.max()
    if drop > 0:
        im = Image.fromarray(np.roll(a, drop, axis=0), 'RGBA')
        top = Image.fromarray(np.roll(np.asarray(top), drop, axis=0), 'RGBA')
    return im, top


def crisp(big, w, h, seed):
    """Shrink a large fruit drawing (body, stems) to w x h the way the game's
    fruit look: hard edges, a mottled matte surface and a solid dark
    outline round the body; the stems keep their color."""
    big, top = big
    a = np.asarray(big.resize((w, h), Image.BOX)).astype(np.float32)
    m = a[..., 3] >= 128
    rng = np.random.default_rng(seed)
    blot = np.asarray(Image.fromarray((rng.random((max(2, h // 3), max(2, w // 3))) * 255).astype(np.uint8)).resize((w, h), Image.BICUBIC)).astype(np.float32) / 255 - 0.5
    a[..., :3] *= (1 + 0.16 * blot + rng.normal(0, 0.06, (h, w)))[..., None]
    pad = np.pad(m, 1)
    inner = pad[:-2, 1:-1] & pad[2:, 1:-1] & pad[1:-1, :-2] & pad[1:-1, 2:]
    edge = m & ~inner
    a[..., :3][edge] = a[..., :3][edge] * 0.22 + np.array([6, 4, 3], np.float32)
    t = np.asarray(top.resize((w, h), Image.BOX)).astype(np.float32)
    tm = t[..., 3] >= 100
    a[..., :3][tm] = t[..., :3][tm] * (1 + rng.normal(0, 0.05, tm.sum()))[..., None]
    m = m | tm
    a[..., 3] = np.where(m, 255, 0)
    a[..., :3][~m] = 0
    return Image.fromarray(a.clip(0, 255).astype(np.uint8), 'RGBA')


def dim(im, k, sat=0.82, tint=(0.94, 0.98, 1.06)):
    # the hungry corners are caves and machine rooms: tone the plant down to
    # their light (darker, a little cooler, a little less saturated)
    a = np.asarray(im).astype(np.float32)
    rgb = a[..., :3]
    lum = rgb.mean(-1, keepdims=True)
    rgb = (rgb * sat + lum * (1 - sat)) * k * np.array(tint, np.float32)
    a[..., :3] = rgb
    return Image.fromarray(a.clip(0, 255).astype(np.uint8), 'RGBA')


def grown(big, size, green, seed):
    """A younger fruit: smaller and greener, where the ripe fruit's body
    will be (so a bud never pokes out above the leaves)."""
    def tint(im):
        a = np.asarray(im).astype(np.float32)
        a[..., :3] = a[..., :3] * (1 - green) + np.array([92, 140, 52], np.float32) * green * (a[..., :3].mean(-1, keepdims=True) / 120 + 0.3)
        return Image.fromarray(a.clip(0, 255).astype(np.uint8), 'RGBA')
    small = crisp((tint(big[0]), big[1]), max(4, int(FW * size)), max(4, int(FH * size)), seed)
    out = Image.new('RGBA', (FW, FH), (0, 0, 0, 0))
    out.alpha_composite(small, ((FW - small.width) // 2, 6))
    return out


if __name__ == '__main__':
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    # each look's file: stage * 4 + season (seedling, young, full; spring to winter)
    sheets = {}
    for vname, V in VARIANTS.items():
        k, sat = V['dim']
        imgs = [dim(bush(s, 7 + i, st, vname), k, sat=sat) for st in range(3) for i, s in enumerate(('spring', 'summer', 'autumn', 'winter'))]
        s16.write(os.path.join(out, V['file'] + '.s16'), imgs)
        sheets[vname] = imgs
        # the same bush in a copper planter, for bushes on built platforms
        # (zfps, zfpm, zfpv, zfpw: 156 x 128, same picture order)
        tub = dim(planter.matte(planter.pot('copper')), k, sat=sat)
        s16.write(os.path.join(out, 'zfp' + V['file'][3] + '.s16'), [planter.potted(im, tub) for im in imgs])
    # the smaller forms (owner, 2026-11-18: for places too small for the
    # full bush): compact (zfc*, 108 x 80, for slopes and low rooms) and
    # slender (zfn*, 56 x 118, for narrow gaps), each also in a pot for
    # built floors (zfq*, zfo*), in the same picture order. Only the forms
    # placed in the world (additions/food.cos), or by an earlier version
    # whose saves may still hold one (zfnm), are kept in art/sprites; the
    # others went on 2026-11-21 (3 MB in every download). To plant another,
    # add its name here and make it again.
    SMALL_FORMS_USED = {'zfcd', 'zfnm', 'zfns', 'zfom', 'zfov', 'zfqw'}
    for vname, V in VARIANTS.items():
        k, sat = V['dim']
        L = V['file'][3]
        for (plain, pot_name, w, h, sz, col, pw, ph) in FORMS:
            imgs = [dim(bush(s, 7 + i, st, vname, BW=w, BH=h, size=sz, column=col), k, sat=sat)
                    for st in range(3) for i, s in enumerate(('spring', 'summer', 'autumn', 'winter'))]
            if plain + L in SMALL_FORMS_USED:
                s16.write(os.path.join(out, plain + L + '.s16'), imgs)
            if pot_name + L in SMALL_FORMS_USED:
                with planter.sized(pw, ph):
                    tub = dim(planter.matte(planter.pot('copper')), k, sat=sat)
                    s16.write(os.path.join(out, pot_name + L + '.s16'),
                              [planter.potted(im, tub, soil_y=h - 8, drape=4) for im in imgs])
    bushes = sheets['moss']
    # zfrt: kind * 6 + stage (bud, half-grown, ripe, overripe, then ripe and
    # overripe lying on the ground)
    fruits = []
    for n, k in enumerate(('dewberry', 'sunplum', 'emberpear', 'frostnut')):
        def look(big, sd):
            return dim(crisp(big, FW, FH, sd), 0.85, sat=0.92)
        ripe = fruit(k, False)
        fruits += [dim(grown(ripe, 0.42, 0.75, 10 * n + 1), 0.85, sat=0.92), dim(grown(ripe, 0.7, 0.4, 10 * n + 2), 0.85, sat=0.92),
                   look(ripe, 10 * n + 3), look(fruit(k, True), 10 * n + 4),
                   look(fruit(k, False, side=True), 10 * n + 5), look(fruit(k, True, side=True), 10 * n + 6)]
    s16.write(os.path.join(out, 'zfrt.s16'), fruits)
    sheet = Image.new('RGBA', (max((BW + 10) * 4, (FW * 2 + 8) * 6 + 12), (BH + 6) * 3 + (FH * 2 + 6) * 4 + 20), (50, 52, 46, 255))
    for i, b in enumerate(bushes):
        sheet.alpha_composite(b, ((i % 4) * (BW + 10), (i // 4) * (BH + 6)))
    for i, f in enumerate(fruits):
        sheet.alpha_composite(f.resize((FW * 2, FH * 2), Image.NEAREST), ((i % 6) * (FW * 2 + 8) + 6, (BH + 6) * 3 + 10 + (i // 6) * (FH * 2 + 6)))
    sheet.save(os.path.join(out, 'fruitbush-sheet.png'))
    print('wrote', out)
