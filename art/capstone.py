"""Draw an original dressed-stone capstone in the style of C2's weathered
green-gray castle stonework, to seal a gap in a stone floor.

Everything here is drawn from scratch (no game pixels are read or copied),
so the output can ship with the project. The colors and lighting are
numbers chosen by eye to sit between the stones on either side.

    python3 capstone.py OUT_BASENAME [SITE]

writes OUT_BASENAME.png (for viewing) and OUT_BASENAME.s16 (for the game).
"""
import sys
import numpy as np
from PIL import Image
import s16
from catwalk import _noise

SS = 4

SITES = {
    # The Gelsemium pit, x 1236-1292, y 956-1004: a recess of small dark
    # bricks between two big blocks. The blocks to the west are brighter
    # (lit, about 80,84,66) and the one to the east dimmer (48,63,52), so
    # the light falls off left to right. Two courses, like the west side.
    'gelsemium': {
        'size': (56, 48),
        'courses': [  # (x0, y0, x1, y1) in sprite pixels, each one block
            (1.0, 1.0, 55.0, 23.0),
            (2.0, 24.5, 54.0, 47.0),
        ],
        'stone': (60, 66, 52),
        'light': (1.08, 0.80),       # brightness at the left and right edges
        'matte': (16, 26, 22),       # the dark recess behind
        'moss': (0.0, 0.35),         # moss tint on the top course, left to right
        'seed': 5,
    },
}


def draw(site):
    W, H = site['size']
    w, h = W * SS, H * SS
    rgb = np.zeros((h, w, 3), np.float32)
    alpha = np.zeros((h, w), np.float32)
    rng = np.random.default_rng(site['seed'])

    # texture at sprite resolution, blown up to the 4x drawing
    def up(m):
        return np.kron(m, np.ones((SS, SS), np.float32))
    seed = site['seed']
    # rough-hewn faces: broad mottling, then strong fine relief and grain,
    # then scattered pits, like the neighboring weathered blocks
    mottle = up(0.16 * _noise((H, W), seed, 7) + 0.11 * _noise((H, W), seed + 1, 3)
                + 0.07 * _noise((H, W), seed + 2, 1))
    grain = rng.normal(0, 0.07, (H, W)).astype(np.float32)
    pits = rng.random((H, W)) < 0.04
    grain[pits] -= rng.uniform(0.2, 0.4, pits.sum())
    # relief: lit where the surface rises toward the light above
    relief = _noise((H, W), seed + 3, 3)
    grain += 0.3 * (relief - np.roll(relief, 1, 0))
    grain = up(grain)

    ys, xs = np.mgrid[0:h, 0:w].astype(np.float32) / SS
    light = site['light'][0] + (site['light'][1] - site['light'][0]) * xs / W
    stone = np.array(site['stone'], np.float32)

    for ci, (x0, y0, x1, y1) in enumerate(site['courses']):
        # slightly irregular block outline: each edge wanders a little
        jx0 = x0 + 0.9 * _noise((1, H), seed + 10 + ci, 6)[0]
        jx1 = x1 + 0.9 * _noise((1, H), seed + 20 + ci, 6)[0]
        jy0 = y0 + 0.8 * _noise((1, W), seed + 30 + ci, 7)[0]
        jy1 = y1 + 0.8 * _noise((1, W), seed + 40 + ci, 7)[0]
        yi = np.clip(ys.astype(int), 0, H - 1)
        xi = np.clip(xs.astype(int), 0, W - 1)
        dl, dr = xs - jx0[yi], jx1[yi] - xs
        dt, db = ys - jy0[xi], jy1[xi] - ys
        # rounded, chipped corners
        corner = np.minimum(np.minimum(dl, dr), np.minimum(dt, db))
        r = 3.0
        cx = np.where(dl < r, r - dl, np.where(dr < r, r - dr, 0))
        cy = np.where(dt < r, r - dt, np.where(db < r, r - db, 0))
        inside = (corner > 0) & (np.hypot(cx, cy) < r)
        a = np.clip(np.minimum(corner, r - np.hypot(cx, cy)) * 2, 0, 1)

        shade = light * (1 + mottle + grain)
        # bevel: lit top and left edges, shadowed bottom and right
        shade = shade + 0.35 * np.clip(1 - dt / 2.0, 0, 1) + 0.18 * np.clip(1 - dl / 1.6, 0, 1)
        shade = shade - 0.40 * np.clip(1 - db / 2.4, 0, 1) - 0.25 * np.clip(1 - dr / 1.8, 0, 1)
        col = stone * shade[..., None]
        if ci == 0 and site.get('moss'):
            m0, m1 = site['moss']
            moss = (m0 + (m1 - m0) * xs / W) * np.clip(1 - dt / 3.0, 0, 1) * np.clip(0.5 + mottle * 3, 0, 1)
            col = col * (1 - moss[..., None]) + np.array([52, 70, 30], np.float32) * moss[..., None]
        sel = inside & (a > alpha)
        rgb[sel] = col[sel]
        alpha[sel] = a[sel]

    # cracks: short dark random walks from the block edges
    for _ in range(3):
        x, y = rng.uniform(6, W - 6), rng.choice([1.5, 24.5])
        ang = rng.uniform(0.9, 2.2)
        for _ in range(int(rng.uniform(6, 14) * SS)):
            ang += rng.normal(0, 0.25)
            x += np.cos(ang) / SS
            y += np.sin(ang) / SS
            px, py = int(x * SS), int(y * SS)
            if 0 <= px < w and 0 <= py < h and alpha[py, px] > 0.5:
                rgb[py:py + 2, px:px + 2] *= 0.45

    # down to sprite size; soft edges matted onto the recess color, then
    # on/off transparency as C2 needs
    # Pillow premultiplies RGBA itself when resizing, so the color goes in
    # plain and comes out as the covered samples' average
    big = np.dstack([rgb, alpha * 255]).clip(0, 255).astype(np.uint8)
    im = Image.fromarray(big, 'RGBA').resize((W, H), Image.BOX)
    a = np.asarray(im).astype(np.float32)
    al = a[..., 3:4] / 255.0
    a[..., :3] = a[..., :3] * al + np.array(site['matte'], np.float32) * (1 - al)
    a[..., 3] = np.where(al[..., 0] < 0.16, 0, 255)
    return Image.fromarray(a.clip(0, 255).astype(np.uint8), 'RGBA')


if __name__ == '__main__':
    base = sys.argv[1]
    site = SITES[sys.argv[2] if len(sys.argv) > 2 else 'gelsemium']
    im = draw(site)
    im.save(base + '.png')
    s16.write(base + '.s16', [im])
    print(base, im.size)
