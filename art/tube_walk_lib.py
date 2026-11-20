"""Helpers for tube_walk.py (the glass tube from the domed lab's dock down to
the gene lab): a small supersampled renderer for the lab's new west wall,
the patch fill that paints out the old tube, the stalactites and the rock
face. Our own drawing; colors and textures are sampled at run time from the
player's installed game (bring-your-own-data), never stored."""
import math
import numpy as np
from numpy.lib.stride_tricks import sliding_window_view as swv
from PIL import Image, ImageDraw, ImageFilter

SS = 2                                    # the wall's supersampling
L = np.array([-0.55, -0.55, 0.63]); L = L / np.linalg.norm(L)     # key light, upper left
RIM = np.array([0.9, -0.1, 0.25]); RIM = RIM / np.linalg.norm(RIM)  # warm rim, from the lab
V = np.array([0.0, 0.0, 1.0])
WARM = np.array([255, 150, 80], np.float32)


def blur(a, s):
    return np.asarray(Image.fromarray(a.clip(0, 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(s))).astype(np.float32)


# --- a power that is the same on every machine ------------------------
# numpy's x ** e (any e but 2, 0.5, 1, -1, 0) goes through Intel SVML or
# libm depending on the CPU, so its last bits vary between machines. pw is
# built only from IEEE-exact steps (frexp, ldexp, rint, +, -, *, /), so it
# gives the same bits everywhere, and the engine's C++ port
# (madeart/npcore.cpp, np::pw) gives them too. Float64 inside, about 1e-15
# relative error; returns the input's dtype as ** would (float32 in, float32
# out; a Python float gives a float64).
_LN2 = float.fromhex('0x1.62e42fefa39efp-1')
_INV_LN2 = float.fromhex('0x1.71547652b82fep+0')
_SQRT_HALF = float.fromhex('0x1.6a09e667f3bcdp-1')
_LN_C = [1.0 / (2 * j + 1) for j in range(12)]               # atanh series: ln m = 2 s (1 + s^2/3 + s^4/5 + ...)
_EXP_C = [1.0 / math.factorial(k) for k in range(15)]       # e^z, |z| <= ln2 / 2


def pw(x, e):
    """x ** e, deterministic (x >= 0; a negative x only with an integer e)."""
    a = np.asarray(x)
    dt = a.dtype if a.dtype in (np.float32, np.float64) else np.dtype(np.float64)
    xd = a.astype(np.float64)
    ax = np.abs(xd)
    m, k = np.frexp(ax)                                     # ax = m 2^k, m in [0.5, 1)
    lo = m < _SQRT_HALF
    m = np.where(lo, m * 2, m)                              # m in [0.707, 1.414)
    k = np.where(lo, k - 1, k).astype(np.float64)
    s = (m - 1) / (m + 1)
    s2 = s * s
    p = np.full_like(s, _LN_C[-1])
    for c in _LN_C[-2::-1]:
        p = p * s2 + c
    t = e * (k + (2 * s * p) * _INV_LN2)                    # e log2 |x|
    n = np.rint(t)
    z = (t - n) * _LN2
    q = np.full_like(z, _EXP_C[-1])
    for c in _EXP_C[-2::-1]:
        q = q * z + c
    r = np.ldexp(q, np.clip(n, -2000, 2000).astype(np.int64))
    r = np.where(ax == 0, 0.0 if e > 0 else (1.0 if e == 0 else np.inf), r)
    if e == int(e):
        r = np.where((xd < 0) & (int(e) % 2 == 1), -r, r)
    else:
        r = np.where(xd < 0, np.nan, r)
    r = r.astype(dt)
    return r if r.ndim else r[()]


_LN2_HI = float.fromhex('0x1.62e42fee00000p-1')             # ln 2, high part (n * _LN2_HI exact for |n| < 2^11)
_LN2_LO = float.fromhex('0x1.a39ef35793c76p-33')            # and the rest


def ex(x):
    """e ** x, deterministic like pw (np.exp of float64 is SVML or libm,
    by CPU); the recipe uses it on its fixed Gaussian kernels."""
    a = np.asarray(x)
    dt = a.dtype if a.dtype in (np.float32, np.float64) else np.dtype(np.float64)
    xd = a.astype(np.float64)
    n = np.rint(xd * _INV_LN2)
    z = (xd - n * _LN2_HI) - n * _LN2_LO                    # Cody-Waite: |z| <= ln2 / 2
    q = np.full_like(z, _EXP_C[-1])
    for c in _EXP_C[-2::-1]:
        q = q * z + c
    r = np.ldexp(q, np.clip(n, -2000, 2000).astype(np.int64)).astype(dt)
    return r if r.ndim else r[()]


# --- the lab's new west wall ------------------------------------------

class Canvas:
    """World-placed, supersampled RGB + coverage, lit like the lab (key
    light from the upper left, a warm rim from the right)."""

    def __init__(self, x0, y0, w, h, seed=1):
        self.x0, self.y0, self.w, self.h = x0, y0, w, h
        self.W, self.H = w * SS, h * SS
        self.rgb = np.zeros((self.H, self.W, 3), np.float32)
        self.a = np.zeros((self.H, self.W), np.float32)
        self.rng = np.random.default_rng(seed)
        ys, xs = np.mgrid[0:self.H, 0:self.W].astype(np.float32)
        self.X = x0 + (xs + 0.5) / SS
        self.Y = y0 + (ys + 0.5) / SS

    def noise(self, cells, amp=1.0):
        small = self.rng.random((max(2, int(self.h // cells) + 2), max(2, int(self.w // cells) + 2))).astype(np.float32)
        im = Image.fromarray((small * 255).astype(np.uint8)).resize((self.W, self.H), Image.BICUBIC)
        return (np.asarray(im).astype(np.float32) / 255 - 0.5) * 2 * amp

    def shade(self, n, albedo, spec=0.35, shin=24, rimk=0.35):
        """Light a surface: n is (H, W, 3) normals, albedo (H, W, 3)."""
        diff = np.clip((n * L).sum(-1), 0, 1)
        r = 2 * (n * L).sum(-1, keepdims=True) * n - L
        sp = pw(np.clip((r * V).sum(-1), 0, 1), shin)
        rim = pw(np.clip((n * RIM).sum(-1), 0, 1), 3)
        return albedo * (0.22 + 0.85 * diff)[..., None] + spec * 255 * sp[..., None] + rimk * WARM * rim[..., None] * (albedo.mean(-1, keepdims=True) / 128)

    def put(self, mask, col):
        m = mask.astype(np.float32)
        self.rgb = self.rgb * (1 - m[..., None]) + col * m[..., None]
        self.a = np.maximum(self.a, m)

    def sphere(self, cx, cy, r, color, spec=0.5):
        dx, dy = (self.X - cx) / r, (self.Y - cy) / r
        d2 = dx * dx + dy * dy
        n = np.dstack([dx, dy, np.sqrt(np.clip(1 - d2, 0, 1))])
        self.put(d2 <= 1, self.shade(n, np.broadcast_to(np.array(color, np.float32), self.X.shape + (3,)), spec=spec, shin=30))

    def cyl_h(self, x0, x1, cy, r, color, spec=0.45, tarnish=0.0):
        X, Y = self.X, self.Y
        dy = (Y - cy) / r
        m = (np.abs(dy) <= 1) & (X >= x0) & (X < x1)
        n = np.dstack([np.zeros_like(dy), dy, np.sqrt(np.clip(1 - dy * dy, 0, 1))])
        alb = np.broadcast_to(np.array(color, np.float32), X.shape + (3,)).copy()
        alb *= (1 + self.noise(9, 0.10))[..., None]
        if tarnish:
            t = np.clip(self.noise(14, 1.0), 0, 1) * tarnish
            alb = alb * (1 - t[..., None]) + np.array([70, 90, 60], np.float32) * t[..., None]
        self.put(m, self.shade(n, alb, spec=spec, shin=26))

    def flange_h(self, cx, cy, r, w, color, bolts=3):
        """A collar round a horizontal pipe, with bolts."""
        X, Y = self.X, self.Y
        dy = (Y - cy) / r
        m = (np.abs(dy) <= 1) & (np.abs(X - cx) <= w / 2)
        n = np.dstack([np.zeros_like(dy), dy, np.sqrt(np.clip(1 - dy * dy, 0, 1))])
        self.put(m, self.shade(n, np.broadcast_to(np.array(color, np.float32), X.shape + (3,)), spec=0.3, shin=20))
        for k in range(bolts):
            by = cy + (k - (bolts - 1) / 2) * (r * 1.4 / max(1, bolts - 1))
            self.sphere(cx, by, 1.6, np.array(color) * 1.3)

    def ibeam_h(self, x0, x1, y0, y1, color):
        X, Y = self.X, self.Y
        m = (X >= x0) & (X < x1) & (Y >= y0) & (Y < y1)
        h = y1 - y0
        v = (Y - y0) / h
        n = np.zeros(X.shape + (3,), np.float32); n[..., 2] = 1
        n[(v < 0.2)] = [0, -0.6, 0.8]
        n[(v > 0.8)] = [0, 0.6, 0.8]
        alb = np.broadcast_to(np.array(color, np.float32), X.shape + (3,)).copy() * (1 + self.noise(8, 0.12))[..., None]
        col = self.shade(n, alb, spec=0.2, shin=12)
        col[(v >= 0.2) & (v <= 0.8)] *= 0.72
        self.put(m, col)
        for rx in np.arange(x0 + 8, x1, 22):
            self.sphere(rx, y0 + h * 0.5, 1.6, np.array(color) * 1.4)

    def grime(self, amount=0.25):
        g = np.clip(self.noise(24, 1.0) + 0.3 * self.noise(6, 1.0), -1, 1)
        self.rgb *= (1 - amount * np.clip(g, 0, 1))[..., None]
        # darker toward the floor and in the corners
        v = (self.Y - self.y0) / self.h
        self.rgb *= (1 - 0.35 * v ** 2)[..., None]

    def result(self):
        rgb = self.rgb.reshape(self.h, SS, self.w, SS, 3).mean(axis=(1, 3))
        a = self.a.reshape(self.h, SS, self.w, SS).mean(axis=(1, 3))
        return rgb.clip(0, 255), a


def light_pools(c, pools, ambient=0.5):
    """Uneven light over the strip: warm pools on a darker ambient."""
    lum = np.full(c.X.shape, ambient, np.float32)
    tint = np.zeros(c.X.shape + (3,), np.float32)
    for (px, py, rad, k, col) in pools:
        g = np.exp(-(((c.X - px) ** 2 + (c.Y - py) ** 2) / (2 * rad * rad)))
        lum += k * g
        tint += g[..., None] * np.array(col, np.float32)
    # (light only brightens and darkens, tinted a little warm: no added haze)
    warm = 1 + 0.25 * tint / 255
    c.rgb = c.rgb * lum[..., None] * warm


def rust_and_grime(c, m, rust=0.5, streaks=0.5, chips=0.3):
    """Rust blotches, dirt streaks running down, chipped paint, on mask m."""
    mf = m.astype(np.float32)
    blot = np.clip(c.noise(10, 1.0) * 1.6 + c.noise(3, 0.6) - 0.35, 0, 1) * rust
    rc = np.array([120, 58, 22], np.float32) * (0.8 + 0.4 * np.clip(c.noise(2, 1.0), -1, 1))[..., None]
    k = (blot * mf)[..., None]
    c.rgb = c.rgb * (1 - k * 0.85) + rc * k * 0.85
    # streaks: wide, soft stains (a few columns of noise, blurred), not thin stripes
    small = c.rng.random((5, max(3, c.w // 14))).astype(np.float32)
    st = np.asarray(Image.fromarray((small * 255).astype(np.uint8)).resize((c.W, c.H), Image.BICUBIC).filter(ImageFilter.GaussianBlur(4 * SS))).astype(np.float32) / 255
    st = np.clip((st - 0.5) * 2, 0, 1) * streaks
    c.rgb *= (1 - 0.5 * st * mf)[..., None]
    # chips: fewer, clustered spots of bare metal; rare dark ones, not pinholes everywhere
    sp = c.noise(1.5, 1.0)
    sp2 = c.noise(4, 1.0)
    bare = (sp > 0.9) & (sp2 > 0.2) & m
    dark = (sp < -0.95) & (sp2 < -0.3) & m
    c.rgb[bare] = c.rgb[bare] * (1 - chips * 0.6) + np.array([140, 136, 126], np.float32) * chips * 0.6
    c.rgb[dark] = c.rgb[dark] * 0.55


def trim_wall(c, x0, y0, x1, y1, plate, trim, seed=3, light=((6080, 1580, 60, 0.5),)):
    """Olive metal plating crossed by thin raised trim strips (as the gene
    lab's own wall): the area is split into irregular insets; each inset's
    edges are raised strips of rust-red trim, lit along the top and left and
    with a dark line under them; a few strips stop short (L-shapes, paired
    lines); the plate is warmed by soft light and mottled."""
    rng = np.random.default_rng(seed)
    X, Y = c.X, c.Y
    m = (X >= x0) & (X < x1) & (Y >= y0) & (Y < y1)
    alb = np.broadcast_to(np.array(plate, np.float32), X.shape + (3,)).copy()
    alb *= (1 + c.noise(14, 0.18) + c.noise(4, 0.08) + c.noise(1.5, 0.05))[..., None]
    lum = np.full(X.shape, 0.75, np.float32)
    for (lx, ly, rad, k) in light:
        lum += k * np.exp(-(((X - lx) ** 2 + ((Y - ly) * 0.8) ** 2) / (2 * rad * rad)))
    c.put(m, alb * lum[..., None])
    # the insets: recursive splits into irregular rectangles
    rects = [(x0, y0, x1, y1)]
    out = []
    while rects:
        rx0, ry0, rx1, ry1 = rects.pop()
        w, h = rx1 - rx0, ry1 - ry0
        if (w < 70 and h < 60) or (w < 40 or h < 34) or rng.random() < 0.25 and w < 110 and h < 110:
            out.append((rx0, ry0, rx1, ry1)); continue
        if w > h * 1.2 or (w >= 70 and rng.random() < 0.4):
            cut = rx0 + int(w * rng.uniform(0.3, 0.7))
            rects += [(rx0, ry0, cut, ry1), (cut, ry0, rx1, ry1)]
        else:
            cut = ry0 + int(h * rng.uniform(0.3, 0.7))
            rects += [(rx0, ry0, rx1, cut), (rx0, cut, rx1, ry1)]
    tcol = np.array(trim, np.float32)
    c.crease = np.zeros(X.shape, bool)

    def strip_h(xa, xb, y, t=2.2):
        mm = m & (X >= xa) & (X < xb) & (Y >= y) & (Y < y + t)
        lit = mm & (Y < y + 0.9)
        c.rgb[mm] = tcol * 1.0 * lum[mm][:, None]
        c.rgb[lit] = np.minimum(255, tcol * 1.35 * lum[lit][:, None] + 6)
        sh = m & (X >= xa) & (X < xb) & (Y >= y + t) & (Y < y + t + 1.5)
        c.rgb[sh] *= 0.45
        c.crease |= mm | sh

    def strip_v(x, ya, yb, t=2.2):
        mm = m & (Y >= ya) & (Y < yb) & (X >= x) & (X < x + t)
        lit = mm & (X < x + 0.9)
        c.rgb[mm] = tcol * 1.0 * lum[mm][:, None]
        c.rgb[lit] = np.minimum(255, tcol * 1.35 * lum[lit][:, None] + 6)
        sh = m & (Y >= ya) & (Y < yb) & (X >= x + t) & (X < x + t + 1.5)
        c.rgb[sh] *= 0.45
        c.crease |= mm | sh
    for (rx0, ry0, rx1, ry1) in out:
        inset = 3 + rng.integers(0, 3)
        ax0, ay0, ax1, ay1 = rx0 + inset, ry0 + inset, rx1 - inset, ry1 - inset
        if ax1 - ax0 < 8 or ay1 - ay0 < 8:
            continue
        # the inset sits back a little (darker), its trim round it
        ins = m & (X >= ax0) & (X < ax1) & (Y >= ay0) & (Y < ay1)
        c.rgb[ins] *= 0.93
        sides = rng.random(4) > 0.35                     # some sides left open (L-shapes)
        if sides[0]: strip_h(ax0, ax1, ay0)
        if sides[1]: strip_h(ax0, ax1, ay1 - 2)
        if sides[2]: strip_v(ax0, ay0, ay1)
        if sides[3]: strip_v(ax1 - 2, ay0, ay1)
        if rng.random() < 0.15:                          # a paired line
            strip_h(ax0 + 4, ax1 - 4, ay0 + 5)


WALL_X0, WALL_X1, WALL_Y0, WALL_Y1 = 5990, 6172, 1478, 1814   # (past the join: faded or cut per row)
WALL_FLOOR = 1798


def lab_wall():
    """The gene lab's new west wall strip, wall only (owner, 2026-11-19: the
    teleporter stands here instead of machines): the room's own kind of
    wall, olive plating with raised rust-red trim, its dark steel beam, and
    its big rust pipe carried on west into the rock. Returns (rgb, alpha)
    at 1x over WALL_X0..X1, WALL_Y0..Y1."""
    X0, X1, Y0 = WALL_X0, WALL_X1, WALL_Y0
    c = Canvas(X0, Y0, X1 - X0, WALL_Y1 - Y0, seed=8)
    trim_wall(c, X0, Y0, X1, WALL_FLOOR, [78, 66, 40], [70, 46, 32], seed=5, light=((6085, 1575, 55, 0.3), (6070, 1730, 60, 0.2)))
    # texture: fine grain, pitting and blotchy stains over the plating
    c.rgb *= (1 + c.noise(1.2, 0.10) + c.noise(3, 0.08))[..., None]
    c.rgb = c.rgb * np.asarray(np.clip(1 - 0.35 * np.clip(c.noise(12, 1.0) - 0.2, 0, 1), 0, 1))[..., None]
    c.ibeam_h(X0, X1, 1499, 1517, [64, 70, 66])
    # the wall's wear and light first, so the pipe keeps its own
    rust_and_grime(c, (c.a > 0), rust=0.3, streaks=0.15, chips=0.12)
    # a dirty lime-green wash over the plating (owner, 2026-11-19), blotchy
    # and uneven, like the green grime on the room's own metal
    lime = np.array([112, 150, 34], np.float32)
    amt = np.clip(0.5 + 0.35 * c.noise(16, 1.0) + 0.15 * c.noise(5, 1.0), 0.1, 0.85)
    lum = c.rgb.mean(axis=2, keepdims=True)
    c.rgb = c.rgb * (1 - amt[..., None]) + (lime / lime.mean()) * lum * amt[..., None]
    # the creases (trim strips and their shadow lines) rusty brown again, over the wash
    cr = c.crease
    rust = np.array([150, 74, 34], np.float32)
    cl = c.rgb[cr].mean(axis=1, keepdims=True)
    c.rgb[cr] = c.rgb[cr] * 0.35 + (rust / rust.mean()) * cl * 0.65 * (1 + 0.2 * c.noise(3, 1.0)[cr])[:, None]
    c.grime(0.3)
    light_pools(c, [(6060, 1730, 40, 0.2, (255, 150, 80)), (6110, 1760, 40, 0.2, (255, 90, 40)), (6080, 1560, 40, 0.15, (255, 140, 80))], ambient=0.55)
    # the room's big rust pipe (about 42 px across, centered at y 1649),
    # carried on west into the rock, a collar like its own, and its shadow
    # on the wall below
    sh = (np.abs(c.Y - 1670) < 6) & (c.X < X1 + 1)
    c.rgb[sh] *= (0.45 + 0.55 * np.clip(np.abs(c.Y[sh] - 1670) / 6, 0, 1))[:, None]
    c.cyl_h(X0, X1 + 1, 1646, 19, [110, 62, 40], spec=0.3, tarnish=0.1)
    # a double ring, as the room's flange
    c.flange_h(6063, 1646, 21, 5, [70, 38, 24], bolts=3)
    c.flange_h(6073, 1646, 21, 5, [70, 38, 24], bolts=3)
    pm = (np.abs(c.Y - 1646) <= 22) & (c.X < X1 + 1)
    rust_and_grime(c, pm, rust=0.12, streaks=0.0, chips=0.08)
    return c.result()


# --- painting out the old tube ----------------------------------------

def quilt_fill(img, OX, OY, top_of, xs0, xs1, bgt, OYT, seed=3):
    """Fill the old tube's band [top_of(x)-8, top_of(x)+160) for x in
    [xs0, xs1) with patches of the real cave rock and bark from just above
    it (image quilting in the tube's own straightened frame), in two rows of
    shorter patches (96 rows, 16 of overlap), so the source stays within the
    bark just above the band (130 rows) and never reaches the ceiling's
    crust higher up. bgt: the background from OYT down, a taller copy."""
    rng = np.random.default_rng(seed)
    A, BAND, CTX = 300, 168, 8           # rows above kept; band (from 16 px above the old top, to cover its rings); context
    SX0, SX1 = xs0 - 150, xs1
    Wd = SX1 - SX0
    rows = A + BAND + 40
    S = np.zeros((rows, Wd, 3), np.float32)
    for i, x in enumerate(range(SX0, SX1)):
        t = int(round(top_of(x))) - 16
        y0 = t - A
        S[:, i] = bgt[y0 - OYT:y0 - OYT + rows, x - OX]
    out = S.copy()
    src = S[A - 130:A]
    F = 4                                # the search runs at a quarter size

    def pool(a):
        h, w = a.shape[0] // F * F, a.shape[1] // F * F
        return a[:h, :w].reshape(h // F, F, w // F, F, 3).mean(axis=(1, 3))
    srcd = pool(src)
    BW, OV, BH, VOV = 64, 24, 96, 16
    for band_row, (b0, b1) in enumerate(((0, BH), (BH - VOV, 2 * BH - VOV))):
        prev = None
        x = xs0 - SX0
        # rows taken: the block plus context (above for the first row, below for the second)
        top_ctx = CTX if band_row == 0 else VOV
        while x < Wd:
            bw = min(BW, Wd - x)
            bw -= bw % F
            if bw < OV + F * 2:
                break
            bwd = bw // F
            Hb = top_ctx + (b1 - b0) + (CTX if band_row == 1 else 0)
            Hd = Hb // F
            # what the block must match, from the output so far
            above_d = pool(out[A + b0 - top_ctx:A + b0, x:x + bw])
            below_d = pool(S[A + BAND:A + BAND + CTX, x:x + bw]) if band_row == 1 else None
            prevd = pool(prev) if prev is not None else None
            best = None
            for r0d in range(0, srcd.shape[0] - Hd + 1):
                blk = srcd[r0d:r0d + Hd]
                ad = above_d.shape[0]
                top = swv(blk[:ad], (ad, bwd, 3))[0, :, 0]
                e = ((top - above_d) ** 2).sum(axis=(1, 2, 3))
                if below_d is not None:
                    bd = below_d.shape[0]
                    bot = swv(blk[-bd:], (bd, bwd, 3))[0, :, 0]
                    e = e + 0.5 * ((bot - below_d) ** 2).sum(axis=(1, 2, 3))
                if prevd is not None:
                    OVd = OV // F
                    left = swv(blk[:, :OVd], (Hd, OVd, 3))[0, :, 0]
                    e = e + ((left - prevd[:Hd, :OVd]) ** 2).sum(axis=(1, 2, 3))
                sx = np.arange(len(e)) * F
                # prefer rock from nearby columns, so the bark's grain carries
                # on; not a copy of what lies straight above
                e = e + 1e5 * (np.abs(sx - x) < F * 2) + 40.0 * np.abs(sx - x)
                k = np.argsort(e)[:3]
                jj = k[rng.integers(0, len(k))]
                if best is None or e[jj] < best[0]:
                    best = (e[jj], r0d * F, jj * F)
            _, r0, j = best
            blk = src[r0:r0 + Hb, j:j + bw].copy()
            dst = out[A + b0 - top_ctx:A + b0 - top_ctx + Hb, x:x + bw]
            wcol = np.ones(bw, np.float32)
            if prev is not None:
                wcol[:OV] = np.linspace(0, 1, OV)
            wrow = np.ones(Hb, np.float32)
            if band_row == 0:
                wrow[:top_ctx + 10] = np.linspace(0, 1, top_ctx + 10)   # into the rock above, done before the old edge
            else:
                wrow[:top_ctx] = np.linspace(0, 1, top_ctx)
                wrow[-CTX * 2:] = np.linspace(1, 0, CTX * 2) * 0.6
            wgt = wrow[:, None] * wcol[None, :]
            dst[:] = dst * (1 - wgt[..., None]) + blk * wgt[..., None]
            prev = out[A + b0 - top_ctx:A + b0 - top_ctx + Hb, x + bw - OV:x + bw].copy()
            x += bw - OV
    res = img.copy()
    for i, xx in enumerate(range(SX0, SX1)):
        if xx < xs0:
            continue
        t = int(round(top_of(xx))) - 16
        y0 = t - A
        res[y0 + A - CTX - OY:y0 + A + BAND + CTX - OY, xx - OX] = out[A - CTX:A + BAND + CTX, i]
    return res


def tone_correct(res, bg, band, sigma=9):
    """Keep the patches' texture but give the band the broad light and shade
    of a smooth gradient from the tones just above it to those just below it
    (smoothed sideways), so no rectangular blocks of tone show."""
    soft_bg = blur(bg, 5)
    low_res = blur(res, sigma)
    Wd = band.shape[1]
    tops = np.zeros((Wd, 3), np.float32); bots = np.zeros((Wd, 3), np.float32)
    tb = {}
    for x in range(Wd):
        r = np.nonzero(band[:, x])[0]
        if not len(r):
            continue
        t, b = r.min(), r.max()
        tb[x] = (t, b)
        tops[x] = soft_bg[max(t - 6, 0), x]
        # (a vine or bamboo just below: the tone above carried on instead)
        pb = bg[min(b + 6, bg.shape[0] - 1), x]
        bots[x] = tops[x] * 0.8 if (pb[0] - pb[2] > 25 and pb.mean() > 50) else soft_bg[min(b + 6, bg.shape[0] - 1), x]
    k = ex(-0.5 * (np.arange(-40, 41) / 16.0) ** 2); k /= k.sum()
    for arr in (tops, bots):
        for c in range(3):
            arr[:, c] = np.convolve(np.pad(arr[:, c], 40, mode='edge'), k, mode='valid')
    out = res.copy()
    for x, (t, b) in tb.items():
        n = b - t + 1
        a = (np.arange(n) + 0.5) / n
        w = (a * a * (3 - 2 * a))[:, None]
        target = tops[x] * (1 - w) + bots[x] * w
        out[t:b + 1, x] = res[t:b + 1, x] - low_res[t:b + 1, x] + target
    return out


def match_texture(res, bg, band, gain_max=2.6):
    """Raise the fill's fine texture to the strength of the bark just above
    it, column by column (smoothed sideways): the patches' overlaps and the
    tone correction had softened it."""
    hp_res = res - blur(res, 2.5)
    hp_bg = bg - blur(bg, 2.5)
    Wd = band.shape[1]
    gain = np.ones(Wd, np.float32)
    spans = {}
    for x in range(Wd):
        r = np.nonzero(band[:, x])[0]
        if not len(r):
            continue
        t, b = r.min(), r.max()
        spans[x] = (t, b)
        above = hp_bg[max(t - 60, 0):t - 4, x]
        inside = hp_res[t + 10:b - 10, x]
        if len(above) > 10 and len(inside) > 10:
            gain[x] = np.clip(above.std() / max(inside.std(), 1e-3), 1.0, gain_max)
    k = ex(-0.5 * (np.arange(-25, 26) / 10.0) ** 2); k /= k.sum()
    gain = np.convolve(np.pad(gain, 25, mode='edge'), k, mode='valid')
    out = res.copy()
    for x, (t, b) in spans.items():
        n = b - t + 1
        ramp = np.clip(np.minimum(np.arange(n), n - 1 - np.arange(n)) / 12.0, 0, 1)[:, None]
        out[t:b + 1, x] = res[t:b + 1, x] + hp_res[t:b + 1, x] * (gain[x] - 1) * ramp
    return out


def restore_branches(img, scene, strip_mod, top_of, OX, OY, polys, SH=170):
    """Put back, inside the owner's circles, what the background painted over
    the old tube there (the branches): the scene's pixels where they differ
    from clean glass."""
    res = img.copy()
    Hh, Ww = img.shape[:2]
    m = Image.new('L', (Ww, Hh), 0)
    d = ImageDraw.Draw(m)
    for p in polys:
        d.polygon([(x - OX, y - OY) for x, y in p], fill=255)
    inside = np.asarray(m) > 0
    keep = np.zeros((Hh, Ww), bool)
    for x in range(min(p[0] for poly in polys for p in poly), max(p[0] for poly in polys for p in poly) + 1):
        t0 = int(round(top_of(x))) - 12
        ref = strip_mod[:, (x - 5630) % 300]
        col = scene[t0 - OY:t0 - OY + SH, x - OX]
        diff = np.abs(col - ref[:len(col), :3]).sum(axis=1)
        keep[t0 - OY:t0 - OY + len(col), x - OX] = diff > 70
    keep &= inside
    # above and below the old tube, inside the circles, the branch is the background itself
    res[keep] = scene[keep]
    return res


# --- the tube's join at the dock collar -------------------------------

def dock_join(o, base, MOD, X0, start_top, slope, keep, OX, OY, n=56, blend=8, kstart=-1):
    """The tube's first n px drawn sheared, not rotated: each column of plain
    glass moved down by the slope, so at the collar the tube is upright and
    meets the collar's ring square (the rotated tube's slanted end and its
    first ring right beside the collar are covered); then a short blend into
    the rotated tube."""
    SH = MOD.shape[0]
    # the module's first ring (columns whose outline rises above the glass)
    ringc = np.nonzero((MOD[0:8, :60, 3] > 0).any(axis=0))[0]
    R = int(ringc.max()) if len(ringc) else 0
    n = max(n, R + 26)
    # the rotated tube's slanted end and first ring, by the collar: back to the background
    y0 = int(start_top - 12 - OY)
    o[y0 - 16:y0 + SH + 30, X0 - 26 - OX:X0 + n - OX] = base[y0 - 16:y0 + SH + 30, X0 - 26 - OX:X0 + n - OX]
    for k in range(kstart, n + blend):
        x = X0 + k
        # the column the rotated tube shows here (pc = 20 start columns, then
        # the module), mirrored back from just past the ring nearer the collar
        s = k - 20
        if s <= R + 3:
            s = 2 * (R + 4) - s
        s = int(np.clip(s, 0, MOD.shape[1] - 1))
        col = MOD[:, s]
        top = start_top - 12 + slope * k
        ti = int(math.floor(top)); fr = top - ti
        # sub-pixel shift: linear between rows
        c = np.zeros((SH + 1, 4), np.float32)
        c[:SH] += col * (1 - fr)
        c[1:] += col * fr
        a = c[:, 3:] / 255
        w = 1.0 if k < n else 1 - (k - n + 1) / (blend + 1)
        yy0 = ti - OY
        reg = o[yy0:yy0 + SH + 1, x - OX]
        kp = ~keep[yy0:yy0 + SH + 1, x - OX]
        aa = (a[:, 0] * w * kp)[:, None]
        reg[:] = reg * (1 - aa) + c[:, :3] * aa


# --- stalactites and the rock face ------------------------------------

def _fbm(rng, H, W, cells_list, amps):
    out = np.zeros((H, W), np.float32)
    for cells, a in zip(cells_list, amps):
        small = rng.random((max(2, int(H / cells) + 2), max(2, int(W / cells) + 2))).astype(np.float32)
        out += (np.asarray(Image.fromarray((small * 255).astype(np.uint8)).resize((W, H), Image.BICUBIC)).astype(np.float32) / 255 - 0.5) * a
    return out


def _q8(a):
    """a in -0.5..0.5 rounded through 8 bits, as the mock-up's image round trip did."""
    return np.asarray(Image.fromarray(((a + 0.5) * 255).clip(0, 255).astype(np.uint8))).astype(np.float32) / 255 - 0.5


def stalactites(o, bg, items, OX, OY, seed=3, ss=3):
    """Hanging cones of the cave's own rock: each (x, y, width, length, lean)
    hangs from (x, y), its color taken from the rock just above it, lit from
    the upper left (a lit rim on the left, a dark shadow side, a dark
    outline), with drip rings, a rounded tip and a soft shadow cast down and
    to the right on the wall behind."""
    rng = np.random.default_rng(seed)
    Lt = np.array([-0.6, -0.45, 0.66]); Lt /= np.linalg.norm(Lt)
    for (x, y, wd, ln, lean) in items:
        # its color and grain: the rock right where it hangs from
        patch = bg[int(y) - 10 - OY:int(y) - 2 - OY, int(x - wd / 2) - OX:int(x + wd / 2) - OX].reshape(-1, 3)
        colr = np.median(patch, axis=0) * 1.3
        pad = 8
        bw, bh = int(wd + 2 * pad + abs(lean) * ln + 6), int(ln + pad + 4)
        bx0, by0 = int(x - bw / 2 + min(0, lean) * ln * 0.5), int(y - 3)
        W, H = bw * ss, bh * ss
        yy, xx = np.mgrid[0:H, 0:W].astype(np.float32) / ss
        t = np.clip((yy - 3) / ln, 0, 1.2)                         # 0 at the root, 1 at the tip
        cx = (x - bx0) + lean * ln * pw(t, 1.6) + 1.2 * np.sin(t * 7 + x)   # a gentle wander
        # the radius: wide at the root, tapering, drip rings, a rounded tip
        r = wd / 2 * pw(np.clip(1 - t, 0, 1), 0.85) + 0.6
        r = r + wd * 0.45 * np.exp(-t * ln / 5.0)                   # the root flares into the ceiling
        r = r * (1 + 0.12 * _fbm(rng, H, W, [5 * ss], [1.0]))        # a lumpy outline
        r = r * (1 + 0.07 * np.sin(t * ln / 3.2 + x * 0.7))
        r = np.where(t >= 1, 0, r)
        u = (xx - cx) / np.maximum(r, 1e-3)                        # -1..1 across
        inside = (np.abs(u) <= 1) & (yy >= 3) & (t < 1)
        # a round section: the normal across it, tilted down the cone
        nz = np.sqrt(np.clip(1 - u * u, 0, 1))
        n = np.dstack([u, np.full_like(u, 0.12), nz]); n /= np.linalg.norm(n, axis=-1, keepdims=True)
        diff = np.clip((n * Lt).sum(-1), 0, 1)
        grain = _fbm(rng, H, W, [6 * ss, 2.5 * ss], [1.0, 0.8])
        streak = _q8(_fbm(rng, H, W, [1.2 * ss], [1.0]))
        shade = (0.42 + 1.0 * pw(diff, 1.1)) * (1 + 0.35 * grain + 0.15 * streak)
        shade *= 1 - 0.25 * t                                      # darker toward the tip (less light reaches)
        # the rock's own grain, from just above where it hangs, stretched down its length
        tp = bg[int(y) - 26 - OY:int(y) - 2 - OY, int(x) - 12 - OX:int(x) + 12 - OX]
        tpi = np.asarray(Image.fromarray(tp.clip(0, 255).astype(np.uint8)).resize((W, H), Image.BICUBIC)).astype(np.float32)
        tex = tpi.mean(-1) / max(1.0, tpi.mean())
        # flowstone: fine streaks running down its length
        fl = _fbm(rng, H, W // 4 + 1, [1.0 * ss, 0.5 * ss], [1.0, 0.6])
        fl = np.asarray(Image.fromarray(((fl + 0.5) * 255).clip(0, 255).astype(np.uint8)).resize((W, H), Image.BILINEAR)).astype(np.float32) / 255 - 0.5
        fl = np.asarray(Image.fromarray(((fl + 0.5) * 255).clip(0, 255).astype(np.uint8)).resize((W, max(2, H // 10)), Image.BILINEAR).resize((W, H), Image.BICUBIC)).astype(np.float32) / 255 - 0.5
        colr = colr * np.array([1.06, 1.0, 0.9])
        col = colr[None, None, :] * (shade * (0.3 + 0.7 * tex) * (1 + 0.5 * fl))[..., None]
        col += np.array([30, 28, 24], np.float32) * (np.clip(-u - 0.55, 0, 1) * 2)[..., None] * (1 - t)[..., None]  # lit rim
        edge = np.clip((1 - np.abs(u)) * r / 1.4, 0, 1)              # dark outline
        col *= (0.35 + 0.65 * edge)[..., None]
        a = inside.astype(np.float32)
        # its shadow on the wall: the same shape moved down-right, soft
        sh = np.roll(np.roll(a, int(5 * ss), 0), int(4 * ss), 1)
        sh = np.asarray(Image.fromarray((sh * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(2.5 * ss))).astype(np.float32) / 255
        rgb = col.reshape(bh, ss, bw, ss, 3).mean(axis=(1, 3))
        al = a.reshape(bh, ss, bw, ss).mean(axis=(1, 3))
        sh = sh.reshape(bh, ss, bw, ss).mean(axis=(1, 3))
        sd = shade.reshape(bh, ss, bw, ss).mean(axis=(1, 3))
        reg = o[by0 - OY:by0 + bh - OY, bx0 - OX:bx0 + bw - OX]
        # the root grows out of the rock behind it (owner, 2026-11-19): over
        # its top 60% it is the wall's own pixels, given the cone's light and
        # shade, and its edge there fades out; no shadow cast at the root
        ty = (np.arange(bh)[:, None] - 3) / ln
        rb = pw(np.clip(1 - ty / 0.6, 0, 1), 1.1) * np.ones((1, bw))
        own = reg * np.clip(sd / max(1e-3, np.median(sd[al > 0.5])), 0.55, 1.5)[..., None]
        rgb = rgb * (1 - rb[..., None]) + own * rb[..., None]
        al = al * (1 - 0.8 * rb * pw(np.clip(1 - al, 0, 1), 0.4)) * np.clip(1 - rb * 0.45, 0, 1)
        sh = sh * (1 - rb)
        reg *= (1 - 0.45 * sh * (1 - al))[..., None]
        reg[:] = reg * (1 - al[..., None]) + rgb * al[..., None]


def render_rock_earth(outline, sample, seed=11, ss=2, patch=44):
    """The rock face made of the earth below the gene lab (owner, 2026-11-19:
    texture it like the earth there): its surface is that earth's own
    pixels, laid in overlapping feathered patches (no seams, no repeats in a
    grid), shaded gently as a rounded mass lit from the upper left, darker
    toward its outline. Returns (x0, y0, rgb, alpha) at 1x."""
    rng = np.random.default_rng(seed)
    xs = [p[0] for p in outline]; ys = [p[1] for p in outline]
    x0, y0 = int(min(xs)) - 6, int(min(ys)) - 6
    w, h = int(max(xs)) + 7 - x0, int(max(ys)) + 7 - y0
    W, H = w * ss, h * ss
    m = Image.new('L', (W, H), 0)
    ImageDraw.Draw(m).polygon([((x - x0) * ss, (y - y0) * ss) for x, y in outline], fill=255)
    mm = np.asarray(m).astype(np.float32) / 255
    # distance-like depth from the edge
    depth = np.zeros_like(mm); cur = m
    for k in range(24):
        cur = cur.filter(ImageFilter.MinFilter(5))
        depth += (np.asarray(cur) > 128)
    depth = np.asarray(Image.fromarray((depth / 24 * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(4 * ss))).astype(np.float32) / 255
    depth1 = depth.reshape(h, ss, w, ss).mean(axis=(1, 3))
    mm1 = mm.reshape(h, ss, w, ss).mean(axis=(1, 3))

    # the earth's surface: feathered patches from the sample, at 1x
    sh_, sw_ = sample.shape[:2]
    acc = np.zeros((h, w, 3), np.float32); wt = np.zeros((h, w), np.float32)
    yy, xx = np.mgrid[0:patch, 0:patch].astype(np.float32)
    feather = np.minimum(np.minimum(xx + 1, patch - xx), np.minimum(yy + 1, patch - yy)) / (patch / 2)
    feather = pw(np.clip(feather, 0, 1), 1.5)
    step = patch // 2
    for py in range(-step, h, step):
        for px in range(-step, w, step):
            jy, jx = py + rng.integers(-6, 7), px + rng.integers(-6, 7)
            sy, sx = rng.integers(0, sh_ - patch), rng.integers(0, sw_ - patch)
            tile = sample[sy:sy + patch, sx:sx + patch]
            a0, b0 = max(0, jy), max(0, jx)
            a1, b1 = min(h, jy + patch), min(w, jx + patch)
            if a1 <= a0 or b1 <= b0:
                continue
            t = tile[a0 - jy:a1 - jy, b0 - jx:b1 - jx]; f = feather[a0 - jy:a1 - jy, b0 - jx:b1 - jx]
            acc[a0:a1, b0:b1] += t * f[..., None]; wt[a0:a1, b0:b1] += f
    alb = acc / np.maximum(wt, 1e-3)[..., None]
    # (feathered averaging softens the grain a little: give it back)
    bl = blur(alb, 1.2)
    alb = bl + (alb - bl) * 1.5

    # a rounded mass: large soft lumps over the depth, lit from the upper left, gently
    lump = _fbm(rng, h, w, [40, 18], [1.0, 0.5])
    height = 40 * np.sqrt(np.clip(depth1, 0, 1)) + 12 * lump
    gy, gx = np.gradient(height)
    n = np.dstack([-gx, -gy, np.ones_like(gx) * 1.4]); n /= np.linalg.norm(n, axis=-1, keepdims=True)
    diff = np.clip((n * L).sum(-1), 0, 1)
    dm = float(diff[mm1 > 0.5].mean())
    shade = 1.0 + 0.55 * (diff - dm)
    edge = np.clip(depth1 * 6, 0, 1)
    rgb = alb * shade[..., None] * (0.55 + 0.45 * edge)[..., None]
    # a touch of the lab's warm light on the east side, low down
    east = np.clip((np.arange(w)[None, :] / w - 0.55) / 0.45, 0, 1) * np.clip(np.arange(h)[:, None] / h * 1.4 - 0.2, 0, 1)
    rgb = rgb * (1 + 0.25 * east * np.clip(n[..., 0], 0, 1) * 3)[..., None]
    # grayer toward the top (owner, 2026-11-19): from halfway up, the color
    # graded toward a neutral gray (a little lighter, so it reads as gray, not black)
    rows = np.nonzero(mm1.max(axis=1) > 0.5)[0]
    top, bot = rows.min(), rows.max()
    mid = (top + bot) / 2
    g = np.clip((mid - np.arange(h)) / max(1, mid - top), 0, 1)[:, None]
    g = (g * g * (3 - 2 * g)) * 0.85
    lum = rgb.mean(-1, keepdims=True)
    gray = lum * 1.3 * np.array([0.98, 1.0, 1.04])
    rgb = rgb * (1 - g[..., None]) + gray * g[..., None]
    return x0, y0, rgb.clip(0, 255), mm1
