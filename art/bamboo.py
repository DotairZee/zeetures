"""Draw an original plank-and-bamboo walkway in the style of C2's treehouse
scaffolds: rust-orange planks laid end to end on a golden bamboo beam,
bamboo posts at the ends lashed with rope, and a sagging rope hand line.

Inspired by JayD's Plato Project walkways (Creatures 2 COB) in where they
go; drawn from scratch here (no game or COB pixels are read or copied), so
the output can ship with the project.

    python3 bamboo.py OUT_BASENAME SITE

writes OUT_BASENAME.png and OUT_BASENAME.s16, and prints where to place it.
"""
import sys
import numpy as np
from PIL import Image, ImageDraw
import s16
from catwalk import _noise

SS = 4

# walkway sites: the deck line's two ends in world pixels (the creatures'
# floor line), and the color behind the edges for anti-aliasing
SITES = {
    # across the waterfall, from the scaffold's top deck (by the telescope)
    # to the treehouse's upper deck
    # the rope hand line ties to the platforms' own bamboo: the lashing on the
    # scaffold's tall post and the treehouse railing's lashed post
    'falls-upper': {'p0': (2589, 1322), 'p1': (2910, 1285), 'matte': (150, 150, 140), 'dapple': 0.14,
                    'rope': ((2594, 1245), (2920, 1245)), 'end_posts': False},
    # across the waterfall's foot, from the scaffold's lower deck down to
    # the treehouse's lower deck
    # (starts at the scaffold's own corner pole, x 2575, so no water shows
    # between deck and bridge)
    'falls-lower': {'p0': (2575, 1453), 'p1': (3012, 1544), 'matte': (150, 150, 140), 'dapple': 0.14,
                    'rope': ((2576, 1425), (3025, 1492)), 'end_posts': False},
    # under the waterfall, from the planted rock ledge (y 2150) across the
    # mist to the ground by the great tree (y 2261), in two spans that meet
    # where rooms 192 and 58 do (x 3060, y 2180)
    # (in the shadow of the great tree and the falls: shaded, cooled, and a
    # little veiled by the mist)
    'mist-a': {'p0': (2908, 2150), 'p1': (3062, 2180), 'matte': (170, 170, 165), 'shade': 0.72},
    # (a stilt under the bend where the two spans meet, owner 2026-11-09,
    # like the two further along; ends where its underside meets the
    # ground, y 2259; the floor line
    # carries on to x 3310 and the ground, so creatures step off onto it)
    'mist-b': {'p0': (3058, 2180), 'p1': (3278, 2249), 'matte': (170, 170, 165), 'shade': 0.72,
               'supports': [(3061, 2258), (3135, 2258), (3215, 2258)], 'ground': 2259},
}

PLANK = np.array([192, 94, 28], np.float32)
PLANK_LIT = np.array([232, 136, 62], np.float32)
PLANK_DARK = np.array([96, 40, 10], np.float32)
BAMBOO = np.array([196, 152, 58], np.float32)
BAMBOO_LIT = np.array([244, 206, 112], np.float32)
BAMBOO_DARK = np.array([112, 74, 18], np.float32)
ROPE = np.array([118, 70, 34], np.float32)


def draw(site):
    (ax, ay), (bx, by) = site['p0'], site['p1']
    pad_bot = max([14] + [int(gy - max(ay, by)) + 3 for gx, gy in site.get('supports', [])])
    rope_pts = site.get('rope', ())
    xs_all = [ax, bx] + [p[0] for p in rope_pts]
    top_y = min([ay - 34, by - 34] + [p[1] - 6 for p in rope_pts])
    x0, y0 = int(min(xs_all)) - 5, int(top_y)
    W, H = int(max(xs_all) - min(xs_all)) + 11, int(max(ay, by) + pad_bot - top_y)
    w, h = W * SS, H * SS
    rgb = np.zeros((h, w, 3), np.float32)
    cover = np.zeros((h, w), np.float32)
    rng = np.random.default_rng(13)
    L = np.hypot(bx - ax, by - ay)
    ux, uy = (bx - ax) / L, (by - ay) / L       # along the walkway
    nx, ny = -uy, ux                              # across (down when ny > 0)
    if ny < 0:
        nx, ny = -nx, -ny

    def to_px(px, py):
        return ((px - x0) * SS, (py - y0) * SS)

    xs = (np.arange(w, dtype=np.float32) + 0.5) / SS + x0
    ys = (np.arange(h, dtype=np.float32) + 0.5) / SS + y0
    X, Y = np.meshgrid(xs, ys)
    t = (X - ax) * ux + (Y - ay) * uy          # distance along
    d = (X - ax) * nx + (Y - ay) * ny          # distance below the deck line
    grain = np.kron(0.08 * _noise((H, W), 51, 2) + rng.normal(0, 0.04, (H, W)), np.ones((SS, SS))).astype(np.float32)

    def paint(mask, col):
        rgb[mask] = col[mask] if col.ndim == 3 else col
        cover[mask] = 1

    # the bamboo beam under the deck: a cylinder lit from above, with nodes
    beam = (t >= -1) & (t <= L + 1) & (d >= 6) & (d <= 10)
    k = np.clip((d - 6) / 4, 0, 1)
    shade = 1 - np.abs(k - 0.3) * 1.4
    col = BAMBOO_DARK + (BAMBOO_LIT - BAMBOO_DARK) * np.clip(shade, 0, 1)[..., None]
    node = np.abs(((t + 7) % 24) - 12) > 10.8
    col = np.where(node[..., None], col * 0.62, col)
    paint(beam, col * (1 + grain[..., None]))

    # the planks: short boards end to end, each a little offset, with a lit
    # upper edge, a dark lower edge and small gaps between them
    n = max(1, int(round(L / 27)))
    edges = np.linspace(0, L, n + 1)
    for i in range(n):
        a, b = edges[i] + 0.6, edges[i + 1] - 0.6
        lift = rng.uniform(-0.8, 0.8)
        m = (t >= a) & (t <= b) & (d >= -1 + lift) & (d <= 6 + lift)
        kk = np.clip((d - (-1 + lift)) / 7, 0, 1)
        c = PLANK * (1 - np.abs(kk - 0.4))[..., None] * 1.25
        c = np.where((kk < 0.2)[..., None], PLANK_LIT, c)
        c = np.where((kk > 0.85)[..., None], PLANK_DARK, c)
        c = c * (1 + 1.5 * grain[..., None] + rng.uniform(-0.06, 0.06))
        paint(m, c)

    im = Image.fromarray(np.zeros((h, w, 4), np.uint8), 'RGBA')
    dr = ImageDraw.Draw(im)

    def pole(p, q, width):
        # a bamboo pole between two world points: dark rim, body, light streak
        for col, wd, off in ((BAMBOO_DARK, width + 1, 0), (BAMBOO, width, 0), (BAMBOO_LIT, width * 0.35, -0.5)):
            dr.line([to_px(p[0] + off, p[1]), to_px(q[0] + off, q[1])], fill=tuple(int(v) for v in col) + (255,), width=max(1, int(wd * SS)))

    def lashing(cx, cy):
        r = 3.2
        for j in range(4):
            yy = cy - r + j * 1.7
            dr.line([to_px(cx - r, yy), to_px(cx + r, yy + 0.8)], fill=(70, 38, 16, 255), width=int(1.4 * SS))
            dr.line([to_px(cx - r, yy - 0.5), to_px(cx + r, yy + 0.3)], fill=tuple(int(v) for v in ROPE) + (255,), width=int(0.7 * SS))

    # support posts from under the beam down to the ground
    for gx, gy in site.get('supports', []):
        deck_y = ay + (gx - ax) * (by - ay) / (bx - ax)
        pole((gx, deck_y + 9), (gx, gy), 3.0)
    # posts at both ends, from below the beam up to the hand line
    posts = [(ax + ux * 2, ay + uy * 2), (bx - ux * 2, by - uy * 2)]
    if site.get('end_posts', True):
        for px, py in posts:
            pole((px, py - 28), (px, min(py + 12, site.get('ground', 1e9) - 1)), 3.0)
    # the rope hand line, sagging between its two ties: the end posts' tops,
    # or points on the platforms' own bamboo
    if rope_pts:
        (p0x, p0y), (p1x, p1y) = rope_pts
    else:
        (p0x, p0y), (p1x, p1y) = (posts[0][0], posts[0][1] - 26), (posts[1][0], posts[1][1] - 26)
    sag = 9 + 0.015 * abs(p1x - p0x)
    pts = []
    for s in np.linspace(0, 1, 60):
        px = p0x + (p1x - p0x) * s
        py = p0y + (p1y - p0y) * s + sag * 4 * s * (1 - s)
        pts.append(to_px(px, py))
    dr.line(pts, fill=(70, 38, 16, 255), width=int(1.8 * SS))
    dr.line(pts, fill=tuple(int(v) for v in ROPE) + (255,), width=int(1.0 * SS))
    if site.get('end_posts', True):
        for px, py in posts:
            lashing(px, py + 7)
            lashing(px, py - 25)
    else:
        for px, py in rope_pts:
            lashing(px, py + 1)

    over = np.asarray(im).astype(np.float32)
    sel = over[..., 3] > 0
    rgb[sel] = over[..., :3][sel]
    cover[sel] = np.maximum(cover[sel], over[..., 3][sel] / 255)

    if site.get('dapple'):
        # dappled light through the leaves: soft patches of sun and shade
        k = site['dapple']
        blobs = np.kron(_noise((H, W), 61, 9) + 0.5 * _noise((H, W), 62, 4), np.ones((SS, SS)))
        sun = np.clip((blobs - 0.15) * 1.6, 0, 1)        # broad, soft-edged patches
        shade = np.clip((-blobs - 0.1) * 1.6, 0, 1)
        rgb *= (1 + 0.45 * k / 0.32 * sun - 0.55 * k / 0.32 * shade)[..., None]
        rgb += (np.array([18, 22, 4], np.float32) * sun[..., None] * k / 0.32)   # warm green-gold sun
    if site.get('shade'):
        # in shadow: darker and cooler, with a thin veil of the misty light,
        # deepening toward the great tree on the east
        k = site['shade']
        east = np.clip((X - 2960) / 360, 0, 1)
        dark = k - 0.12 * east
        lum = rgb.mean(-1, keepdims=True)
        rgb = (rgb * 0.82 + lum * 0.18) * dark[..., None]               # a little less saturated
        rgb = rgb * np.array([0.94, 0.99, 1.06], np.float32)            # cooler
        rgb = rgb * 0.9 + np.array([150, 156, 158], np.float32) * 0.1   # mist veil
    big = np.dstack([rgb, cover * 255]).clip(0, 255).astype(np.uint8)
    small = Image.fromarray(big, 'RGBA').resize((W, H), Image.BOX)
    a = np.asarray(small).astype(np.float32)
    al = a[..., 3:4] / 255.0
    a[..., :3] = a[..., :3] * al + np.array(site['matte'], np.float32) * (1 - al)
    a[..., 3] = np.where(al[..., 0] < 0.3, 0, 255)
    return Image.fromarray(a.clip(0, 255).astype(np.uint8), 'RGBA'), (x0, y0)


if __name__ == '__main__':
    base, name = sys.argv[1], sys.argv[2]
    im, (x0, y0) = draw(SITES[name])
    im.save(base + '.png')
    s16.write(base + '.s16', [im])
    print(base, im.size, 'place at', x0, y0)
