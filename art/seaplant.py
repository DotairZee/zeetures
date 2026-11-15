"""Draw an original sea plant (a kelp clump) and its four seasonal sea fruits,
in the soft prerendered look of C2's plants, for the seasonal fruit bush's
sea look (owner, 2026-11-11: a bush that seeded on the sea floor should be a
sea plant with sea fruits).

Drawn from scratch here (no game or COB pixels are read or copied), so the
output can ship with the project. It reuses fruitbush.py's helpers so the
two match: the same supersampling, matte and fruit pipeline.

    python3 seaplant.py OUT_DIR [BACKGROUND.png]

writes OUT_DIR/zfbk.s16, 12 images (stage * 4 + season: seedling, young,
full; spring, summer, autumn, winter), 156 x 108 like the bushes, and the
kept copper-planter version OUT_DIR/zfpk.s16 (156 x 128); OUT_DIR/zfrk.s16,
24 sea fruits (kind * 6 + stage, as zfrt: sea grapes, kelp pod, sea plum,
frost pearl; bud, half-grown, ripe, overripe, then ripe and overripe lying
on the sea floor); and a PNG preview, over BACKGROUND if one is given.

The fronds stay inside the same dome as a bush's crown at each stage, so
food.cos places fruit on the plant the way it does on a bush.
"""
import sys
import os
import numpy as np
from PIL import Image, ImageDraw
import s16
import planter
from fruitbush import (SS, BW, BH, FW, FH, lerp, ramp, finish, ball, crisp, grown, dim, STAGES)

# Kelp tones per season, deep shadow to full light: olive and gold-brown
# blades like real kelp, fresher green in spring, russet in autumn, a dark
# blue-green in winter. Bladders are a lighter, warmer tone of the blade.
SEASONS = {
    'spring': {'blade': [(14, 34, 22), (40, 86, 40), (98, 146, 62), (176, 206, 112)], 'bladder': (150, 170, 70)},
    'summer': {'blade': [(26, 28, 10), (78, 74, 24), (142, 128, 44), (210, 190, 98)], 'bladder': (196, 162, 64)},
    'autumn': {'blade': [(32, 16, 8), (98, 48, 20), (162, 96, 40), (220, 160, 92)], 'bladder': (188, 120, 54)},
    'winter': {'blade': [(8, 22, 24), (22, 58, 58), (54, 104, 98), (122, 168, 158)], 'bladder': (100, 150, 140)},
}
HOLDFAST = [(22, 18, 14), (58, 46, 36), (104, 88, 70)]
# under water: darker, cooler, bluer (the sea rooms' light)
SEA_DIM = (0.82, 0.86, (0.86, 0.98, 1.12))

# fronds per growth stage (seedling, young, full)
FRONDS = [3, 6, 9]


def frond(d, rng, base, ang, length, width, cfg, light_at, cx, cy, rx, ry, layer, bladders):
    """One kelp blade: a ribbon rising from the holdfast in a gentle sway,
    tapering at the tip, ruffled along its edges, with a pale midrib and
    round gas bladders where it leaves the stipe."""
    n = 26
    x, y = base
    pts = []
    sway = rng.uniform(0.7, 1.1) * (1 if rng.random() < 0.5 else -1)
    phase = rng.uniform(0, 1.5)
    for i in range(n):
        t = i / (n - 1)
        # a slow S-curve, as a blade hangs and drifts in the current
        a = ang + sway * 0.55 * np.sin(t * 4.2 + phase)
        x += np.cos(a) * length / n
        y += np.sin(a) * length / n
        # keep inside the crown's dome
        ex, ey = (x - cx) / rx, (y - cy) / ry
        r = np.hypot(ex, ey)
        if r > 1.0 and y < cy:
            x, y = cx + ex / r * rx, cy + ey / r * ry
        pts.append((x, y))
    pts = np.array(pts)
    # normals along the blade
    tang = np.gradient(pts, axis=0)
    tang /= np.maximum(1e-6, np.linalg.norm(tang, axis=1, keepdims=True))
    nrm = np.stack([-tang[:, 1], tang[:, 0]], 1)
    ts = np.linspace(0, 1, n)
    wid = width * (0.3 + 0.7 * np.sin(np.pi * np.clip(ts * 0.85 + 0.12, 0, 1)) ** 0.6) * (1 - 0.35 * ts)
    ruff = 1 + 0.22 * np.sin(ts * 46 + rng.uniform(0, 6)) + 0.08 * np.sin(ts * 97)
    left = pts + nrm * (wid * ruff)[:, None]
    right = pts - nrm * (wid * (2 - ruff))[:, None]
    # stipe: the bare stem at the bottom of the blade
    stipe = int(n * 0.18)
    sc = ramp(cfg['blade'], 0.22 if layer else 0.08)
    d.line([(px * SS, py * SS) for px, py in pts[:stipe + 1]], fill=sc + (255,), width=max(1, int(1.6 * SS)))
    # the blade, in short quads, each shaded by where it is and which way it faces
    for i in range(stipe, n - 1):
        mx, my = pts[i]
        t = light_at(mx, my)
        facing = 0.18 * (nrm[i][0] * -0.7 + nrm[i][1] * -0.7)
        if layer == 0:
            t = t * 0.5 - 0.06          # back fronds sit in shadow
        lit = ramp(cfg['blade'], t + 0.18 + facing)
        shade = ramp(cfg['blade'], t - 0.22 + facing)
        q1 = [tuple(left[i]), tuple(left[i + 1]), tuple(pts[i + 1]), tuple(pts[i])]
        q2 = [tuple(pts[i]), tuple(pts[i + 1]), tuple(right[i + 1]), tuple(right[i])]
        d.polygon([(px * SS, py * SS) for px, py in q1], fill=lit + (255,))
        d.polygon([(px * SS, py * SS) for px, py in q2], fill=shade + (255,))
    # texture: fine veins running the blade's length, either side of the
    # midrib; a lit edge on the upper side, a dark one under; dark mottling
    # and pale flecks
    for off in (-0.62, -0.32, 0.32, 0.62):
        vein = [tuple(pts[i] + nrm[i] * wid[i] * off * ruff[i]) for i in range(stipe, n - 2)]
        t0 = light_at(*pts[n // 2]) * (0.5 if layer == 0 else 1)
        vc = ramp(cfg['blade'], t0 - 0.05 + (0.12 if off < 0 else -0.12))
        d.line([(px * SS, py * SS) for px, py in vein], fill=vc + (255,), width=max(1, SS // 4))
    edge_lit = ramp(cfg['blade'], 0.95 if layer else 0.55)
    edge_dark = ramp(cfg['blade'], 0.0)
    d.line([(px * SS, py * SS) for px, py in left[stipe:]], fill=edge_lit + (255,), width=max(1, SS // 3))
    d.line([(px * SS, py * SS) for px, py in right[stipe:]], fill=edge_dark + (255,), width=max(1, SS // 3))
    for _ in range(int(length * 0.7)):
        i = int(rng.integers(stipe, n - 2))
        o = rng.uniform(-0.8, 0.8)
        mx, my = pts[i] + nrm[i] * wid[i] * o
        r = rng.uniform(0.35, 0.95)
        light = rng.random() < 0.3
        col = ramp(cfg['blade'], (0.9 if light else 0.05) * (0.6 if layer == 0 else 1))
        d.ellipse([(mx - r) * SS, (my - r * 0.7) * SS, (mx + r) * SS, (my + r * 0.7) * SS], fill=col + (255,))
    rib = ramp(cfg['blade'], 0.85)
    d.line([(px * SS, py * SS) for px, py in pts[stipe:-3]], fill=rib + (255,), width=max(1, SS // 3))
    # gas bladders where the blade meets the stipe (and one more on longer blades)
    for k in ([stipe] if length < 40 else [stipe, int(n * 0.5)]):
        if not bladders:
            break
        bx, by = pts[k]
        r = 2.2 + 0.9 * (length / 70)
        b = cfg['bladder']
        ball(d, bx, by, r, b, lerp(b, (255, 250, 220), 0.55), lerp(b, (0, 0, 0), 0.55), hl=True, rim=True)


def plant(season, seed, stage=2):
    cfg = SEASONS[season]
    _, lscale, _ = STAGES[stage]
    rng = np.random.default_rng(seed)
    im = Image.new('RGBA', (BW * SS, BH * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    # the same dome as a bush's crown at this stage
    cx = BW / 2
    cy = BH - (BH - BH * 0.42) * lscale - 2
    rx, ry = BW * 0.47 * lscale, BH * 0.46 * lscale

    def light_at(x, y):
        # light from above, broken into shifting patches (sun through water)
        base = 0.62 - (x - cx) / BW * 0.5 - (y - cy) / BH * 1.1
        patch = 0.22 * np.sin(x * 0.11 + 0.7) * np.cos(y * 0.09 + 1.9)
        return base + patch

    nf = FRONDS[stage]
    base_y = BH - 3
    fronds = []
    for k in range(nf):
        # fan out from the holdfast, the outer ones arching sideways
        f = (k + 0.5) / nf - 0.5
        ang = -np.pi / 2 + f * 1.25 + rng.normal(0, 0.1)
        length = ry * 1.95 * rng.uniform(0.8, 1.05) * (1 - 0.25 * abs(f))
        width = rng.uniform(6.5, 9.0) * (0.6 + 0.4 * lscale)
        fronds.append((f, ang, length, width))
    # back fronds first, then the front ones
    order = sorted(range(nf), key=lambda i: rng.random())
    for layer, idx in ((0, order[: nf // 2]), (1, order[nf // 2:])):
        for i in idx:
            f, ang, length, width = fronds[i]
            frond(d, rng, (cx + f * 10 * lscale, base_y), ang, length, width, cfg, light_at, cx, cy, rx, ry, layer,
                  bladders=stage > 0)
    # the holdfast: a knot of root-like grips on the bottom, on a dark pad
    d.ellipse([(cx - 9 * (0.6 + 0.4 * lscale)) * SS, (BH - 6) * SS, (cx + 9 * (0.6 + 0.4 * lscale)) * SS, (BH - 1) * SS],
              fill=HOLDFAST[0] + (255,))
    for k in range(14 + 6 * stage):
        a = rng.uniform(np.pi * 0.05, np.pi * 0.95)
        L = rng.uniform(4, 9) * (0.7 + 0.3 * lscale)
        x0, y0 = cx + rng.normal(0, 2.5), BH - 4
        x1, y1 = x0 + np.cos(a) * L * (1 if rng.random() < 0.5 else -1), y0 + np.sin(a) * 2.5
        d.line([(x0 * SS, y0 * SS), (x1 * SS, y1 * SS)], fill=HOLDFAST[0] + (255,), width=int(2.4 * SS))
        d.line([(x0 * SS, y0 * SS), (x1 * SS, y1 * SS)], fill=HOLDFAST[1] + (255,), width=int(1.5 * SS))
        d.line([((x0 - 0.3) * SS, (y0 - 0.4) * SS), ((x1 - 0.3) * SS, (y1 - 0.4) * SS)], fill=HOLDFAST[2] + (255,), width=max(1, SS // 2))
    out = finish(im, BW, BH, matte=(30, 44, 46))
    # surface grain: a soft mottle and a little noise over the plant, as on
    # the game's prerendered plants
    a = np.asarray(out).astype(np.float32)
    m = a[..., 3] > 0
    blot = np.asarray(Image.fromarray((rng.random((BH // 4, BW // 4)) * 255).astype(np.uint8)).resize((BW, BH), Image.BICUBIC)).astype(np.float32) / 255 - 0.5
    a[..., :3] *= (1 + 0.22 * blot + rng.normal(0, 0.05, (BH, BW)))[..., None]
    a[..., :3][~m] = 0
    return Image.fromarray(a.clip(0, 255).astype(np.uint8), 'RGBA')


# sea fruits lying on their side on the sea floor (the round sea plum and
# frost pearl stay as they are)
LIES_DOWN = ('seagrape', 'kelppod')


def seafruit(kind, over, side=False):
    """A ripe (or overripe) sea fruit, laid out on fruitbush's 16-unit grid."""
    u = SS * FW / 16
    im = Image.new('RGBA', (FW * SS, FH * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    top = Image.new('RGBA', (FW * SS, FH * SS), (0, 0, 0, 0))
    dt = ImageDraw.Draw(top)
    dimk = 0.7 if over else 1.0
    side = side and kind in LIES_DOWN

    def T(x, y):
        return (16 - y, x) if side else (x, y)

    def P(pts):
        return [(T(x, y)[0] * u, T(x, y)[1] * u) for x, y in pts]

    def bl(x, y, r, base, light, dark, gloss=False, rim=True):
        cx, cy = T(x, y)
        ball(d, cx, cy, r, base, lerp(base, light, 0.55 if gloss else 0.45), dark, hl=gloss, u=u, rim=rim)

    def line(p, q, col, w=1.3):
        dt.line(P([p, q]), fill=col + (255,), width=max(1, int(u * w)))

    def c(col):
        if over:   # overripe: duller and browner, as if waterlogged
            col = lerp(col, (60, 52, 36), 0.4)
        return tuple(int(v * dimk) for v in col)

    if kind == 'seagrape':      # spring: a bunch of glossy green beads on a short stalk
        line((8, 3.2), (8.4, 0.6), c((58, 100, 54)))
        for (x, y, r) in ((8, 4.6, 1.7), (6.2, 6.4, 1.8), (9.8, 6.4, 1.8), (8, 7.8, 1.9), (5.4, 9.2, 1.8), (10.6, 9.2, 1.8),
                          (7, 10.6, 1.9), (9.2, 10.8, 1.9), (8, 13, 1.8)):
            bl(x, y, r * (0.88 if over else 1), c((92, 168, 92)), c((220, 255, 210)), c((24, 70, 40)), gloss=True)
    elif kind == 'kelppod':     # summer: an amber float pod, a little see-through, with a frond tip
        bl(8, 9.2, 5.0 * (0.92 if over else 1), c((206, 148, 52)), c((255, 230, 160)), c((112, 66, 18)), gloss=True)
        bl(8, 3.6, 1.6, c((150, 112, 44)), c((220, 190, 120)), c((80, 54, 20)), rim=False)
        dt.polygon(P([(8.4, 2.4), (12.6, 0.4), (14.2, 1.6), (10.2, 3.6)]), fill=c((120, 118, 46)) + (255,))
    elif kind == 'seaplum':     # autumn: a deep purple round fruit with pale speckles
        bl(8, 9, 5.6 * (0.92 if over else 1), c((110, 44, 112)), c((214, 150, 220)), c((46, 14, 52)))
        cx, cy = T(8, 9)
        for (ox, oy) in ((-2.4, -2), (0.4, -3.2), (2.6, -1), (-1.2, 0.8), (1.6, 2.2), (-2.8, 2.4), (0, 4)):
            px, py = T(8 + ox, 9 + oy)
            d.ellipse([(px - 0.35) * u, (py - 0.35) * u, (px + 0.35) * u, (py + 0.35) * u], fill=c((236, 206, 240)) + (255,))
        line((8, 3.6), (8.8, 1), c((70, 40, 60)))
    else:                       # winter: a frost pearl, pale and lustrous, in a little cup of frond
        bl(8, 8.6, 4.9 * (0.92 if over else 1), c((214, 224, 232)), c((255, 255, 255)), c((120, 132, 150)), gloss=True)
        cup = [(8 + 5.4 * np.cos(t), 10.4 + 3.6 * np.sin(t)) for t in np.linspace(0, np.pi, 18)]
        d.polygon(P(cup), fill=c((60, 104, 96)) + (255,))
        cup2 = [(8 + 4.4 * np.cos(t), 10.8 + 2.4 * np.sin(t)) for t in np.linspace(0, np.pi, 18)]
        d.polygon(P(cup2), fill=c((96, 150, 136)) + (255,))
    a = np.asarray(im)
    rows = np.nonzero(a[..., 3].max(1) > 0)[0]
    drop = (FH * SS - 1) - rows.max()
    if drop > 0:
        im = Image.fromarray(np.roll(a, drop, axis=0), 'RGBA')
        top = Image.fromarray(np.roll(np.asarray(top), drop, axis=0), 'RGBA')
    return im, top


KINDS = ('seagrape', 'kelppod', 'seaplum', 'frostpearl')


def sea(im):
    k, sat, tint = SEA_DIM
    return dim(im, k, sat=sat, tint=tint)


if __name__ == '__main__':
    out = sys.argv[1]
    bgpath = sys.argv[2] if len(sys.argv) > 2 else None
    os.makedirs(out, exist_ok=True)
    seasons = ('spring', 'summer', 'autumn', 'winter')
    imgs = [sea(plant(s, 31 + i, st)) for st in range(3) for i, s in enumerate(seasons)]
    s16.write(os.path.join(out, 'zfbk.s16'), imgs)
    tub = sea(planter.matte(planter.pot('copper')))
    s16.write(os.path.join(out, 'zfpk.s16'), [planter.potted(im, tub) for im in imgs])
    fruits = []
    for n, k in enumerate(KINDS):
        def look(big, sd):
            return sea(crisp(big, FW, FH, sd))
        ripe = seafruit(k, False)
        fruits += [sea(grown(ripe, 0.42, 0.75, 50 + 10 * n + 1)), sea(grown(ripe, 0.7, 0.4, 50 + 10 * n + 2)),
                   look(ripe, 50 + 10 * n + 3), look(seafruit(k, True), 50 + 10 * n + 4),
                   look(seafruit(k, False, side=True), 50 + 10 * n + 5), look(seafruit(k, True, side=True), 50 + 10 * n + 6)]
    s16.write(os.path.join(out, 'zfrk.s16'), fruits)

    # preview: the twelve plants, then the fruits (doubled), over the sea floor
    W, H = (BW + 10) * 4 + 10, (BH + 8) * 3 + (FH * 2 + 8) * 4 + 30
    if bgpath:
        bg = Image.open(bgpath).convert('RGBA')
        sheet = Image.new('RGBA', (W, H), (40, 60, 66, 255))
        for yy in range(0, H, bg.height):
            for xx in range(0, W, bg.width):
                sheet.paste(bg, (xx, yy))
    else:
        sheet = Image.new('RGBA', (W, H), (40, 60, 66, 255))
    for i, b in enumerate(imgs):
        sheet.alpha_composite(b, (10 + (i % 4) * (BW + 10), 6 + (i // 4) * (BH + 8)))
    for i, f in enumerate(fruits):
        sheet.alpha_composite(f.resize((FW * 2, FH * 2), Image.NEAREST),
                              (14 + (i % 6) * (FW * 2 + 10), (BH + 8) * 3 + 18 + (i // 6) * (FH * 2 + 8)))
    sheet.save(os.path.join(out, 'seaplant-preview.png'))
    print('wrote', out)
