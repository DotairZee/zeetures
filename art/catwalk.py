"""Draw an original catwalk bridge sprite in the style of C2's eastern
industrial platforms: brass pipe railings over a dark riveted steel deck.

Everything here is drawn from scratch (no game pixels are read or copied),
so the output can ship with the project. Where the catwalk meets an
existing platform, a "join" describes that platform's end in numbers
(rail height, end-post line, deck slab shading, measured by eye from the
game) so our drawing can continue it seamlessly.

    python3 catwalk.py OUT_BASENAME [WIDTH] [RISE] [JOIN ...] [--back=bend|posts|none] [--no-railing]

writes OUT_BASENAME.png (for viewing) and OUT_BASENAME.s16 (for the game).
Each JOIN names an entry in JOINS: at most one left and one right end.
"""
import sys
import numpy as np
from PIL import Image, ImageDraw
import s16

SS = 4  # supersampling factor

BRASS_DARK = (40, 26, 6, 255)
BRASS = (124, 88, 30, 255)
BRASS_LIGHT = (208, 152, 56, 255)
STEEL_TOP = (46, 28, 4, 255)     # warm olive-gold deck plate, like the
STEEL_FACE = (12, 11, 8, 255)    # neighboring platforms, over a near-black face
STEEL_EDGE = (3, 4, 3, 255)
RIVET = (34, 26, 12, 255)
# C2 sprites have on/off transparency, so edges are anti-aliased by blending
# them into the typical color behind the sprite (the dark rock of the area)
MATTE = (10, 5, 1)

# Platform ends we join onto, in the sprite's own coordinates (x from the
# sprite's left edge, y from its top before the slope is applied).
JOINS = {
    # Upper east, the airlock gap. The sprite is 266 wide, placed at
    # x 7720, y 1240, rise 28.
    #
    # West platform (left end): its top rail stops 14 px into our sprite;
    # its end post leans right going up, 0.37 px per row, crossing x 2 at
    # y 60 (in left-end coordinates, which sit `rise` px lower); its deck
    # is a 13-row lit slab that brightens toward its end.
    'upper-east-west': {
        'side': 'left',
        'rail_from': 14,
        'rail_y': 7.5,              # its top rail's center, so ours lines up
        'post': (2.0, 32.0, 0.37),  # (x, y, lean), y in left-end coordinates
        'slab': [(32, 24, 0), (56, 40, 0), (56, 44, 0), (72, 52, 8), (96, 68, 8),
                 (104, 72, 8), (112, 76, 8), (128, 84, 8), (136, 84, 8), (128, 84, 8),
                 (128, 84, 8), (128, 84, 8), (128, 84, 8), (128, 80, 8), (24, 16, 0)],
        'slab_dim_per_px': 0.012,   # the slab darkens away from its lit end
        'slab_fade': (10, 44),      # then hands over to our deck between these x
        'seam': 1,                  # where our plate meets theirs
    },
    # East platform (right end; x measured from the sprite's right edge):
    # its corner post leans left going up, 0.26 px per row, 13 px in from
    # our edge at y 6; its three rails sit at y 7.5, 26 and 46; its deck
    # starts 11 px in from our edge, a dim brown top band over a near-black
    # face, with the lamp housing below it, which we must leave uncovered.
    # Its end railing panel recedes behind us from 12 to 72 px in: our
    # posts stop 76 px in, and our back railing bends into the panel's far
    # post (72 px in at y 12, leaning 0.38 px per row) at its rail heights.
    'upper-east-east': {
        'side': 'right',
        'rails_y': (7.5, 25.7, 46.0),
        'post': (13.0, 6.0, 0.26),  # (x from right edge, y, lean)
        'back_until': 76,
        'back_post': (72.0, 12.0, 0.38),  # far post of its end panel
        'slab': [(40, 20, 8), (56, 28, 8), (72, 32, 8), (64, 36, 16), (56, 28, 8),
                 (32, 20, 8), (8, 8, 0), (16, 8, 0), (16, 8, 0), (8, 8, 0),
                 (8, 8, 0), (8, 8, 0), (8, 8, 0), (8, 4, 0)],
        'slab_dim_per_px': 0.0,
        'slab_fade': (16, 40),
        'seam': 11,
        'clear_below': (36, 48),    # nothing of ours under its deck near the joint
    },
}


def _c(col, dim):
    return tuple(int(v * dim) for v in col[:3]) + (255,)


def pipe(d, p0, p1, w, dim=1.0):
    polypipe(d, [p0, p1], w, [dim])


def polypipe(d, pts, w, dims):
    """A pipe along a polyline, shaded in three passes (dark outline, body,
    specular streak toward the upper-left light) so the joints between
    segments do not show. dims gives each segment's brightness."""
    segs = list(zip(pts, pts[1:], dims))
    for col, width, off in ((BRASS_DARK, w + 1, 0.0), (BRASS, w, 0.0),
                            (BRASS_LIGHT, max(0.25, w * 0.35), -0.35)):
        for (x0, y0), (x1, y1), dim in segs:
            d.line([((x0 + off) * SS, (y0 + off) * SS), ((x1 + off) * SS, (y1 + off) * SS)],
                   fill=_c(col, dim), width=max(1, int(width * SS)))


def railing(d, w, top, deck, back=False, left=None, right=None, back_mode='bend'):
    """Three rails and posts. The back railing sits a little higher and to
    the right, drawn dimmer, for the same slight perspective as the game's
    platforms.

    At a joined end the front rails meet the platform's own end post (the
    top rail where the platform's rail stops), at the platform's rail
    heights, and our end post is left out. The back rails bend down over
    the last 24 px into the platform's railing (on the west, its end post;
    on the east, the far post of its receding end panel), brightening as
    they come forward, so neither railing just stops in mid-air."""
    yl = [top, top + (deck - top) * 0.36, top + (deck - top) * 0.70]
    yr = list(right['rails_y']) if right else yl
    BACK_DX, BACK_DY, BACK_DIM, BEND = 6, -4, 0.45, 24.0

    def rail_y(i, x):
        return yl[i] + (yr[i] - yl[i]) * x / w

    def end_x(join, i, y, side):
        px, py, lean = join['back_post' if back and side == 'right' else 'post']
        if side == 'left':
            return max(0.0, px + (py - y) * lean + 1.5)
        return min(float(w), w - (px - (y - py) * lean) - 1.5)

    ends = []
    for i in range(3):
        x0, x1 = 2.0, w - 3.0
        if left:
            x0 = left['rail_from'] if (i == 0 and not back) else end_x(left, i, rail_y(i, 0), 'left')
        if right:
            x1 = end_x(right, i, rail_y(i, w), 'right')
        ends.append((x0, x1))
        if not back:
            pipe(d, (x0, rail_y(i, x0)), (x1, rail_y(i, x1)), 1.4)
            continue

        if back_mode == 'posts':
            # a free-standing back railing with its own end posts, set in
            # from each joint instead of bending into the platform
            bx0 = 20.0 if left else 2.0
            bx1 = w - right['back_until'] if right else w - 3.0
            ends[-1] = (bx0, bx1)
            pipe(d, (bx0 + BACK_DX, rail_y(i, bx0) + BACK_DY), (bx1 + BACK_DX, rail_y(i, bx1) + BACK_DY), 1.0, BACK_DIM)
            continue

        def s(x):  # 1 where the back railing is fully set back, 0 at a joint
            k = 1.0
            if left:
                k = min(k, (x - x0) / BEND)
            if right:
                k = min(k, (x1 - x) / BEND)
            k = max(0.0, min(1.0, k))
            return k * k * (3 - 2 * k)
        xs = list(np.linspace(x0, x1, max(2, int((x1 - x0) / 2))))
        pts = [(x + BACK_DX * s(x), rail_y(i, x) + BACK_DY * s(x)) for x in xs]
        dims = [BACK_DIM + (1 - BACK_DIM) * 0.6 * (1 - s((xa + xb) / 2)) for xa, xb in zip(xs, xs[1:])]
        polypipe(d, pts, 1.0, dims)

    xs = 2 if left else 3
    xe = w - 13 if right else w - 4
    n = max(2, round((xe - xs) / 34))
    for i in range(n + 1):
        x = xs + (xe - xs) * i / n
        if (left and i == 0) or (right and i == n):
            continue
        if right and x > w - right['back_until']:
            continue  # the platform's end panel already has posts here
        if back:
            lo, hi = ends[0]
            if back_mode != 'posts':
                lo, hi = (lo + BEND if left else lo), (hi - BEND if right else hi)
            if x < lo or x > hi:
                continue
            pipe(d, (x + BACK_DX, rail_y(0, x) + BACK_DY - 1), (x + BACK_DX, deck + BACK_DY + 2), 1.0, BACK_DIM)
        else:
            pipe(d, (x, rail_y(0, x) - 1), (x, deck), 1.4)
    if back and back_mode == 'posts':
        for x in ends[0]:
            pipe(d, (x + BACK_DX, rail_y(0, x) + BACK_DY - 1), (x + BACK_DX, deck + BACK_DY + 2), 1.0, BACK_DIM)


def _noise(shape, seed, scale):
    """Smooth value noise in -1..1 at the given feature size (pixels)."""
    rng = np.random.default_rng(seed)
    h, w = shape
    gh, gw = h // scale + 2, w // scale + 2
    g = rng.uniform(-1, 1, (gh, gw)).astype(np.float32)
    im = Image.fromarray(((g + 1) * 127.5).astype(np.uint8)).resize((gw * scale, gh * scale), Image.BICUBIC)
    return np.asarray(im, np.float32)[:h, :w] / 127.5 - 1


def join_slab(im, deck, join):
    """Continue the platform's deck slab into ours, shading away from its
    end the way the platform's own slab does, then handing over to our deck
    through a ragged, mottled edge rather than a smooth smear. A seam marks
    where our deck plate butts against the platform's. With clear_below,
    whatever of ours lies under the platform's deck near the joint fades."""
    a = np.asarray(im).astype(np.float32)
    H, W = a.shape[:2]
    rows = np.array(join['slab'], np.float32)
    f0, f1 = join['slab_fade']
    c0, c1 = join.get('clear_below', (0, 0))
    seed = 7 if join['side'] == 'left' else 11
    # noise is made at sprite resolution and blown up to the 4x drawing, so
    # it reads as the pixel-scale grain of the game's prerendered metal
    w1, h1 = W // SS, H // SS
    mottle = 0.16 * _noise((h1, w1), seed, 5) + 0.07 * _noise((h1, w1), seed + 1, 2)
    ragged = 0.30 * _noise((h1, w1), seed + 2, 3)
    grain = np.random.default_rng(seed + 3).normal(0, 0.05, (h1, w1)).astype(np.float32)
    up = lambda m: np.kron(m, np.ones((SS, SS), np.float32))
    mottle, ragged, grain = up(mottle), up(ragged), up(grain)

    def ramp(xp, lo, hi):
        k = 1.0 if xp <= lo else max(0.0, 1.0 - (xp - lo) / (hi - lo))
        return k * k * (3 - 2 * k)  # smoothstep
    y0, y1 = deck * SS, (deck + len(rows)) * SS
    for x in range(W):
        xp = (x if join['side'] == 'left' else W - 1 - x) / SS
        if xp >= max(f1, c1) + 4:
            continue
        bright = max(0.35, 1.0 - join['slab_dim_per_px'] * xp)
        k = np.clip(ramp(xp, f0, f1) + ragged[y0:y1, x], 0, 1)[:, None]
        col = np.repeat(rows, SS, 0) * bright * (1 + mottle[y0:y1, x, None] + grain[y0:y1, x, None])
        a[y0:y1, x, :3] = a[y0:y1, x, :3] * (1 - k) + col * k
        a[y0:y1, x, 3] = np.maximum(a[y0:y1, x, 3], 255 * k[:, 0])
        if c1:
            a[y1:, x, 3] *= 1 - ramp(xp, c0, c1)
    # the seam: a dark gap with a lit lip on our plate's edge
    sx = (join['seam'] if join['side'] == 'left' else W // SS - join['seam']) * SS
    lip = sx + SS if join['side'] == 'left' else sx - SS
    a[y0:y1, sx:sx + SS, :3] *= 0.25
    lo, hi = min(lip, lip + SS), max(lip, lip + SS)
    a[y0 + SS:y1, lo:hi, :3] = np.minimum(255, a[y0 + SS:y1, lo:hi, :3] * 1.35)
    return Image.fromarray(a.clip(0, 255).astype(np.uint8), 'RGBA')


def draw(w=250, rise=0, left=None, right=None, back_mode='bend', front=True):
    top, deck, face = 6, 58, 13  # rail top, deck top surface, deck front-face height
    if left and 'rail_y' in left:
        top = left['rail_y']
    h = deck + 5 + face + 10
    im = Image.new('RGBA', (w * SS, h * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)

    if back_mode != 'none':
        railing(d, w, top, deck, back=True, left=left, right=right, back_mode=back_mode)

    # deck: lit top surface, then a dark front face, then braces underneath
    d.rectangle([0, deck * SS, w * SS, (deck + 5) * SS], fill=STEEL_TOP)
    d.line([(0, deck * SS), (w * SS, deck * SS)], fill=(96, 64, 18, 255), width=SS)
    fy0, fy1 = deck + 5, deck + 5 + face
    d.rectangle([0, fy0 * SS, w * SS, fy1 * SS], fill=STEEL_FACE)
    d.line([(0, fy0 * SS), (w * SS, fy0 * SS)], fill=(24, 18, 8, 255), width=SS)
    d.line([(0, fy1 * SS), (w * SS, fy1 * SS)], fill=STEEL_EDGE, width=2 * SS)
    for x in range(58 if left else 10, w - (44 if right else 4), 24):
        for ry in (fy0 + 3.5, fy1 - 3.5):
            r = 1.3
            d.ellipse([(x - r) * SS, (ry - r) * SS, (x + r) * SS, (ry + r) * SS], fill=RIVET)
            d.point([((x - 0.5) * SS, (ry - 0.5) * SS)], fill=(72, 52, 20, 255))
    # truss braces under the deck
    seg = 50
    for x in range(0, w - seg + 1, seg):
        for a, b in (((x + 4, fy1), (x + seg / 2, fy1 + 9)), ((x + seg / 2, fy1 + 9), (x + seg - 4, fy1))):
            d.line([(a[0] * SS, a[1] * SS), (b[0] * SS, b[1] * SS)], fill=STEEL_EDGE, width=3 * SS)
            d.line([(a[0] * SS, (a[1] - 0.6) * SS), (b[0] * SS, (b[1] - 0.6) * SS)], fill=(26, 20, 10, 255), width=SS)

    for j in (left, right):
        if j:
            im = join_slab(im, deck, j)
    d = ImageDraw.Draw(im)
    if front:
        railing(d, w, top, deck, back=False, left=left, right=right)

    if rise:
        # slope the whole catwalk (right end `rise` px higher; negative:
        # lower) by shifting columns of the 4x drawing, so the slope is
        # anti-aliased by the downscale below instead of stair-stepped
        src = np.asarray(im)
        W = src.shape[1]
        out = np.zeros((src.shape[0] + abs(rise) * SS, W, 4), np.uint8)
        for x in range(W):
            t = x / (W - 1)
            dy = round(abs(rise) * SS * ((1 - t) if rise > 0 else t))
            out[dy:dy + src.shape[0], x] = src[:, x]
        im = Image.fromarray(out, 'RGBA')
        h += abs(rise)
    im = im.resize((w, h), Image.BOX)  # area average: smooth edges from the 4x drawing
    a = np.asarray(im).astype(np.float32)
    alpha = a[..., 3:4] / 255.0
    # a very light grain so flat plates do not look like vector art
    rng = np.random.default_rng(2)
    grain = rng.normal(0, 1.5, a.shape[:2])[..., None]
    rgb = np.clip(a[..., :3] + grain, 0, 255)
    # matte the soft edges onto the background color, then make every pixel
    # with real coverage opaque: anti-aliased edges with on/off transparency
    a[..., :3] = rgb * alpha + np.array(MATTE, np.float32) * (1 - alpha)
    a[..., 3] = np.where(alpha[..., 0] < 0.16, 0, 255)
    return Image.fromarray(a.astype(np.uint8), 'RGBA')


if __name__ == '__main__':
    base = sys.argv[1]
    w = int(sys.argv[2]) if len(sys.argv) > 2 else 250
    rise = int(sys.argv[3]) if len(sys.argv) > 3 else 0
    back_mode = 'bend'
    front = True
    names = []
    for arg in sys.argv[4:]:
        if arg == '--no-railing':
            front, back_mode = False, 'none'
        elif arg.startswith('--back='):
            back_mode = arg.split('=', 1)[1]  # bend, posts or none
        else:
            names.append(arg)
    joins = [JOINS[n] for n in names]
    left = next((j for j in joins if j['side'] == 'left'), None)
    right = next((j for j in joins if j['side'] == 'right'), None)
    im = draw(w, rise, left, right, back_mode, front)
    im.save(base + '.png')
    s16.write(base + '.s16', [im])
    print(base, im.size)
