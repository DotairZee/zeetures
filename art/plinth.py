"""Draw an original column plinth (the foot of a column and the stone slab it
stands on) in the style of C2's castle stonework, as a foreground piece:
creatures walking past pass behind it.

Everything here is drawn from scratch (no game pixels are read or copied),
so the output can ship with the project. Shapes and tones are numbers
chosen by eye to continue the column above it.

    python3 plinth.py OUT_BASENAME

writes OUT_BASENAME.png and OUT_BASENAME.s16. Placed at world x 1448,
y 953 (see bridges.cos): the column east of the Gelsemium ramp.
"""
import sys
import numpy as np
from PIL import Image
import s16
from catwalk import _noise

SS = 4
X0, Y0 = 1448, 953
W, H = 92, 26               # x 1448-1540, y 953-979

FOOT = (1470, 1527, 953, 961)    # the column's foot: x0, x1, top, bottom
SLAB = (1462, 1536, 960, 977)    # the slab under it, centered on the foot
# cool teal-grays like the column above, lit from the east (right)
FOOT_DARK = np.array([6, 13, 14], np.float32)
FOOT_MID = np.array([80, 104, 98], np.float32)
FOOT_LIT = np.array([130, 144, 134], np.float32)
SLAB_TOP = np.array([72, 86, 84], np.float32)
SLAB_RGB = np.array([34, 44, 42], np.float32)
MATTE = np.array([20, 26, 24], np.float32)


def draw():
    w, h = W * SS, H * SS
    xs = (np.arange(w, dtype=np.float32) + 0.5) / SS + X0
    ys = (np.arange(h, dtype=np.float32) + 0.5) / SS + Y0
    X, Y = np.meshgrid(xs, ys)
    rng = np.random.default_rng(9)

    def up(m):
        return np.kron(m, np.ones((SS, SS), np.float32))
    # weathered stone like the column above: blotches, fine relief, pits
    pits = (rng.random((H, W)) < 0.06) * rng.uniform(-0.35, -0.15, (H, W))
    tex = up(0.18 * _noise((H, W), 41, 4) + 0.12 * _noise((H, W), 42, 1)
             + rng.normal(0, 0.08, (H, W)) + pits).astype(np.float32)

    rgb = np.zeros((h, w, 3), np.float32)
    cover = np.zeros((h, w), np.float32)

    # the slab: a dark block with a lit upper edge, rounded at its east end
    x0, x1, y0, y1 = SLAB
    r = 4.0
    cx = np.where(X > x1 - r, X - (x1 - r), 0)
    cy = np.where(Y > y1 - r, Y - (y1 - r), np.where(Y < y0 + r, (y0 + r) - Y, 0))
    slab = (X >= x0) & (X <= x1) & (Y >= y0) & (Y <= y1) & (np.hypot(cx, cy) <= r)
    edge = np.clip(1 - (Y - y0) / 3.5, 0, 1)
    across = np.clip((X - x0) / (x1 - x0), 0, 1)
    col = SLAB_RGB * (1 - edge[..., None]) + SLAB_TOP * edge[..., None]
    col = col * (0.55 + 0.6 * across ** 0.8)[..., None]   # lit from the east
    col = col * (1 - 0.35 * np.clip((Y - (y1 - 4)) / 4, 0, 1))[..., None]
    col = col * (1 + 0.7 * tex)[..., None]                # a little smoother than the foot
    # a thin dark seam where the foot sits on the slab
    col = col * (1 - 0.5 * np.clip(1 - np.abs(Y - (y0 + 0.6)) / 1.0, 0, 1) * (X > FOOT[0]) * (X < FOOT[1]))[..., None]
    rgb[slab] = col[slab]
    cover[slab] = 1

    # the column's foot above it, rounded like a torus: dark on its west
    # side, brightest a little east of center
    fx0, fx1, fy0, fy1 = FOOT
    # the foot flares a little toward the slab, like the bell above it
    t = np.clip((Y - fy0) / (fy1 - fy0), 0, 1)
    ex0, ex1 = fx0 - 3 * t, fx1 + 4 * t
    foot = (X >= ex0) & (X <= ex1) & (Y >= fy0) & (Y < fy1)
    u = np.clip((X - ex0) / (ex1 - ex0), 0, 1)
    # like the ring above it: shadowed on the far west, brightening east,
    # with only a thin dark rim at the east end
    # a rounded foot lit from the east: near black on the west, brightest
    # about seven tenths of the way across, a short falloff at the east rim
    k = np.clip((u - 0.04) / 0.5, 0, 1)
    lit = np.clip((u - 0.45) / 0.25, 0, 1)
    fcol = FOOT_DARK + (FOOT_MID - FOOT_DARK) * k[..., None]
    fcol = fcol * (1 - lit[..., None]) + (FOOT_LIT * k[..., None]) * lit[..., None]
    # the foot's rounded underside turns away from the light toward the slab
    fcol = fcol * (1 - 0.15 * t ** 2)[..., None]
    fcol = fcol * (1 + 0.8 * tex)[..., None]
    rgb[foot] = fcol[foot]
    cover[foot] = 1

    big = np.dstack([rgb, cover * 255]).clip(0, 255).astype(np.uint8)
    im = Image.fromarray(big, 'RGBA').resize((W, H), Image.BOX)
    a = np.asarray(im).astype(np.float32)
    al = a[..., 3:4] / 255.0
    a[..., :3] = a[..., :3] * al + MATTE * (1 - al)
    a[..., 3] = np.where(al[..., 0] < 0.3, 0, 255)
    return Image.fromarray(a.clip(0, 255).astype(np.uint8), 'RGBA')


if __name__ == '__main__':
    base = sys.argv[1]
    im = draw()
    im.save(base + '.png')
    s16.write(base + '.s16', [im])
    print(base, im.size)
