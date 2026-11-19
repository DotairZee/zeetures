"""Draw an original large planter for the fruit bushes that stand on built
platforms (metal decks, rock ledges with railings), so they grow from soil
in a pot instead of out of the hardware. Three styles to choose from:

    copper   a riveted copper tub with a rolled brass rim and verdigris
    iron     a dark iron planter on short feet, flanged rim, rust streaks
    terra    a terracotta urn with a lipped rim and a raised band

Drawn from scratch at 4x and box-downscaled (no game pixels are read or
copied), so the output can ship with the project.

    python3 planter.py OUT_DIR

writes OUT_DIR/planters.png (the three pots side by side). potted() sets a
bush picture from fruitbush.py into a pot.
"""
import sys
import os
import numpy as np
from PIL import Image
from catwalk import _noise

SS = 4
PW, PH = 68, 28            # the pot (about a norn's knee height)
RIM = 5                    # rim band height

STYLES = ('copper', 'iron', 'terra')


def _profile(style, t):
    """Half-width of the pot (as a share of PW/2) at height t (0 rim .. 1 foot)."""
    if style == 'copper':      # a tub tapering a little toward the foot
        return 0.98 - 0.16 * t
    if style == 'iron':        # straighter, a slight taper, short feet below
        return 0.94 - 0.08 * t
    # terra: an urn, swelling below the rim and drawing in to the foot
    return 0.86 + 0.12 * np.sin(np.clip(t, 0, 1) * np.pi * 0.9) - 0.22 * t ** 2


def pot(style, seed=5):
    w, h = PW * SS, PH * SS
    xs = (np.arange(w, dtype=np.float32) + 0.5) / SS
    ys = (np.arange(h, dtype=np.float32) + 0.5) / SS
    X, Y = np.meshgrid(xs, ys)
    cx = PW / 2
    rng = np.random.default_rng(seed)
    foot = PH - (3 if style == 'iron' else 1)
    t = np.clip((Y - RIM) / (foot - RIM), 0, 1)
    half = _profile(style, t) * PW / 2
    u = (X - cx) / np.maximum(half, 1)               # -1 .. 1 across the body
    body = (np.abs(u) <= 1) & (Y >= RIM - 1) & (Y <= foot)
    # round shading: lit from the upper left
    nx = np.clip(u, -1, 1)
    lam = np.clip(0.55 - 0.75 * nx + 0.25 * np.sqrt(np.clip(1 - nx ** 2, 0, 1)), 0, 1.25)
    lam = lam * (1 - 0.25 * t)                       # darker toward the floor
    tex = np.kron(0.10 * _noise((PH, PW), 71, 3) + rng.normal(0, 0.035, (PH, PW)), np.ones((SS, SS))).astype(np.float32)

    if style == 'copper':
        base = np.array([150, 82, 42], np.float32)
        # (owner: a bit more shadow) deeper toward the foot and the far side
        lam = lam * (1 - 0.25 * t) * (1 - 0.18 * np.clip(u, 0, 1))
        col = base[None, None, :] * (0.32 + 0.8 * lam)[..., None]
        # verdigris blooms, mostly low and in the creases
        green = np.clip(np.kron(_noise((PH, PW), 72, 3), np.ones((SS, SS))) * 1.6 - 0.75 + 0.5 * t, 0, 1) * 0.3
        col = col * (1 - green[..., None]) + np.array([70, 130, 110], np.float32) * (0.5 + 0.5 * lam)[..., None] * green[..., None]
        bands = [(11, 1.6), (20, 1.6)]
        band_col = np.array([120, 66, 32], np.float32)
        rim_col = np.array([206, 164, 82], np.float32)
        rivets = True
    elif style == 'iron':
        base = np.array([66, 70, 76], np.float32)
        col = base[None, None, :] * (0.45 + 0.7 * lam)[..., None]
        streak = np.clip(np.kron(_noise((PH, PW // 4 + 1), 73, 2), np.ones((SS, SS * 4)))[:, :w] * 2 - 0.4, 0, 1) * np.clip((Y - RIM) / 20, 0, 1) * 0.5
        col = col * (1 - streak[..., None]) + np.array([120, 62, 30], np.float32) * (0.5 + 0.5 * lam)[..., None] * streak[..., None]
        bands = [(PH - 7, 2.0)]
        band_col = np.array([44, 46, 50], np.float32)
        rim_col = np.array([92, 96, 102], np.float32)
        rivets = True
    else:
        base = np.array([180, 98, 58], np.float32)
        col = base[None, None, :] * (0.5 + 0.65 * lam)[..., None]
        bands = [(10, 2.2)]
        band_col = np.array([200, 120, 74], np.float32)
        rim_col = np.array([196, 112, 68], np.float32)
        rivets = False
    col = col * (1 + tex)[..., None]
    for by, bh in bands:
        m = np.abs(Y - by) < bh / 2
        bl = band_col * (0.55 + 0.7 * lam)[..., None]
        col = np.where(m[..., None], bl, col)
        # a lit upper edge and a shadowed lower edge on each band
        col = np.where((np.abs(Y - (by - bh / 2)) < 0.5)[..., None], col * 1.25, col)
        col = np.where((np.abs(Y - (by + bh / 2)) < 0.5)[..., None], col * 0.6, col)
    if rivets:
        for by, _ in bands:
            for k in np.linspace(-0.82, 0.82, 7):
                rx = cx + k * _profile(style, (by - RIM) / (foot - RIM)) * PW / 2
                r = np.hypot(X - rx, Y - by)
                m = r < 0.85
                shade = np.clip(1.3 - 0.6 * ((X - rx) + (Y - by)), 0.4, 1.5)
                col = np.where(m[..., None], (band_col * 1.2 * shade[..., None]), col)

    def across(uu, yy):
        # the x of a point a share uu across the body at height yy
        return cx + uu * _profile(style, np.clip((yy - RIM) / (foot - RIM), 0, 1)) * PW / 2

    def relief(mask, dx, dy, k):
        # raised (k > 0) or sunk (k < 0) detail: lit on its upper left
        return np.where(mask, 1 + k * np.clip(-(dx + dy), -1.5, 1.5), 1.0)

    if style == 'copper':
        # hammered all over: shallow dimples, each lit on its lower right
        for _ in range(70):
            px, py = rng.uniform(4, PW - 4), rng.uniform(RIM + 1, foot - 1)
            r = rng.uniform(1.3, 2.0)
            d = np.hypot(X - px, Y - py)
            m = d < r
            col = col * relief(m, (X - px) / r, (Y - py) / r, -0.16)[..., None]
        # an embossed vine between the bands: a wavy stem with leaves
        vy = 15.5
        stem = np.abs(Y - (vy + 1.1 * np.sin(u * np.pi * 3))) < 0.6
        col = np.where((stem & (np.abs(u) < 0.95))[..., None], np.array([214, 150, 78], np.float32) * (0.6 + 0.6 * lam)[..., None], col)
        for k, uu in enumerate(np.linspace(-0.8, 0.8, 7)):
            ly = vy + 1.1 * np.sin(uu * np.pi * 3) + (-1.6 if k % 2 else 1.6)
            lx = across(uu, ly)
            ang = 0.6 if k % 2 else -0.6
            ex, ey = (X - lx) * np.cos(ang) + (Y - ly) * np.sin(ang), -(X - lx) * np.sin(ang) + (Y - ly) * np.cos(ang)
            e = (ex / 2.6) ** 2 + (ey / 1.25) ** 2
            m = e < 1
            leaf = np.array([222, 150, 78], np.float32) * (0.55 + 0.7 * lam)[..., None]
            col = np.where(((e >= 1) & (e < 1.45))[..., None], col * 0.6, col)   # a dark edge round each leaf
            col = np.where(m[..., None], leaf * relief(m, ex / 2.6, ey / 1.25, 0.3)[..., None], col)
            # a touch of verdigris in the leaf's hollow edge
            col = np.where(((e > 0.7) & m & (ex + ey > 0))[..., None], col * 0.7 + np.array([60, 120, 100], np.float32) * 0.3, col)
    elif style == 'iron':
        # three recessed panels, each with a raised round boss
        top, bot = RIM + 3, foot - 9
        for u0, u1 in ((-0.86, -0.33), (-0.26, 0.26), (0.33, 0.86)):
            inside = (u >= u0) & (u <= u1) & (Y >= top) & (Y <= bot)
            col = np.where(inside[..., None], col * 0.82, col)
            edge_tl = inside & ((u - u0 < 0.05) | (Y - top < 0.8))
            edge_br = inside & ((u1 - u < 0.05) | (bot - Y < 0.8))
            col = np.where(edge_tl[..., None], col * 0.55, col)
            col = np.where(edge_br[..., None], col * 1.45, col)
            bx, by = across((u0 + u1) / 2, (top + bot) / 2), (top + bot) / 2
            d = np.hypot(X - bx, Y - by)
            m = d < 2.0
            boss = np.array([150, 150, 156], np.float32)[None, None, :] * (0.5 + 0.6 * lam)[..., None] * relief(m, (X - bx) / 2.0, (Y - by) / 2.0, 0.4)[..., None]
            col = np.where(m[..., None], boss, col)
        # pitting
        pits = np.kron((rng.random((PH, PW)) < 0.05) * rng.uniform(0.6, 0.85, (PH, PW)), np.ones((SS, SS)))
        col = np.where((pits > 0)[..., None], col * pits[..., None], col)
    else:
        # a row of stamped dots under the rim
        for uu in np.linspace(-0.88, 0.88, 13):
            dx_, dy_ = X - across(uu, RIM + 2.2), Y - (RIM + 2.2)
            m = np.hypot(dx_, dy_) < 0.9
            col = np.where(m[..., None], col * 0.55, col)
        # an incised zigzag band under the raised band
        z0, z1 = 13.0, 19.0
        phase = np.abs(((u + 1) * 5) % 2 - 1)            # 0..1 triangle wave across
        zy = z0 + phase * (z1 - z0)
        groove = (np.abs(Y - zy) < 0.7) & (np.abs(u) < 0.93)
        lip = (np.abs(Y - (zy + 1.0)) < 0.35) & (np.abs(u) < 0.93)
        col = np.where(groove[..., None], col * 0.48, col)
        col = np.where(lip[..., None], col * 1.25, col)
        for zl in (z0 - 1.2, z1 + 1.2):
            line = (np.abs(Y - zl) < 0.5) & (np.abs(u) < 0.95)
            col = np.where(line[..., None], col * 0.55, col)
        # clay grain: pale and dark specks
        g = np.kron(rng.random((PH, PW)), np.ones((SS, SS)))
        col = np.where((g > 0.94)[..., None], col * 1.15, np.where((g < 0.05)[..., None], col * 0.85, col))

    if style == 'copper':
        # the rolled rim overhangs the body: a soft shadow just under it
        under = np.clip(1 - (Y - RIM) / 3.0, 0, 1) * (Y >= RIM - 0.5)
        col = col * (1 - 0.45 * under)[..., None]

    rgb = np.zeros((h, w, 3), np.float32)
    cover = np.zeros((h, w), np.float32)
    rgb[body] = col[body]
    cover[body] = 1

    # the rim: a rolled band, wider than the body's top, with soil behind it
    rim_half = PW / 2 * (1.0 if style != 'terra' else 0.95)
    ru = (X - cx) / rim_half
    rim = (np.abs(ru) <= 1) & (Y >= 0.5) & (Y < RIM + 0.5)
    rlam = np.clip(0.6 - 0.7 * ru, 0, 1.2) * (1.1 - 0.5 * np.abs((Y - RIM / 2) / (RIM / 2)))
    rcol = rim_col[None, None, :] * (0.5 + 0.7 * rlam)[..., None] * (1 + tex)[..., None]
    # the soil, seen just over the back of the rim
    soil = (np.abs(ru) <= 0.92) & (Y >= 0.5) & (Y < 2.0)
    scol = np.array([46, 32, 22], np.float32) * (1 + 1.5 * tex)[..., None]
    rgb[rim] = rcol[rim]
    cover[rim] = 1
    rgb[soil] = scol[soil]
    if style == 'iron':
        # two short feet
        for fx in (cx - PW * 0.32, cx + PW * 0.32):
            m = (np.abs(X - fx) < 3.5) & (Y > foot) & (Y < PH)
            rgb[m] = (base * 0.55)
            cover[m] = 1
    big = np.dstack([rgb, cover * 255]).clip(0, 255).astype(np.uint8)
    return Image.fromarray(big, 'RGBA').resize((PW, PH), Image.BOX)


class sized:
    """Draw pots of another size for a while (the smaller bush forms get
    narrower pots): with sized(w, h): pot(...), potted(...)."""
    def __init__(self, w, h):
        self.w, self.h = w, h

    def __enter__(self):
        global PW, PH
        self.old = (PW, PH)
        PW, PH = self.w, self.h

    def __exit__(self, *exc):
        global PW, PH
        PW, PH = self.old


def potted(bush, potimg, soil_y=100, drape=6):
    """Set a bush picture (156 x 108, standing on its bottom row) into a
    pot: the bush is lifted so its base sits in the pot's soil (bush row
    soil_y at the rim), leaves may hang a little over the rim, and nothing
    of it shows below that beside the pot."""
    BW, BH = bush.size
    H = soil_y + PH
    out = Image.new('RGBA', (BW, H), (0, 0, 0, 0))
    b = np.asarray(bush).copy()
    px0 = (BW - PW) // 2
    outside = np.ones(BW, bool)
    outside[max(0, px0 - 2):min(BW, px0 + PW + 2)] = False
    # low bare twigs reaching out past the pot would hang in the air: below
    # the crown, drop bark-coloured (unsaturated) pixels outside the pot
    rgb = b[..., :3].astype(np.float32)
    sat = (rgb.max(-1) - rgb.min(-1)) / np.maximum(rgb.max(-1), 1)
    for y in range(soil_y - 18, BH):
        if y >= soil_y + drape:
            b[y, outside] = 0
        else:
            b[y, outside & (sat[y] < 0.3)] = 0
    out.alpha_composite(Image.fromarray(b, 'RGBA'), (0, 0))
    out.alpha_composite(potimg, (px0, soil_y))
    return out


def matte(im):
    """Hard on/off transparency like C2's."""
    a = np.asarray(im).astype(np.float32)
    a[..., 3] = np.where(a[..., 3] < 90, 0, 255)
    return Image.fromarray(a.clip(0, 255).astype(np.uint8), 'RGBA')


if __name__ == '__main__':
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    sheet = Image.new('RGBA', ((PW + 12) * 3, PH + 12), (60, 58, 50, 255))
    for i, st in enumerate(STYLES):
        sheet.alpha_composite(matte(pot(st)), (6 + i * (PW + 12), 6))
    sheet.save(os.path.join(out, 'planters.png'))
    print('wrote', out)
