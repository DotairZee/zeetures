"""The flying fox's basket, rendered in 3D (owner, 2026-11-10: "really
viscerally" woven, "with volume and light", not a texture drawn on a flat
shape). Original art, from numbers only.

The basket is a bellied bowl, a surface of revolution about a vertical
axis, seen a little from above. Its wall is built from strands: upright
stakes (round rods, one every few degrees, so they draw together toward the
base) and horizontal weavers (round strands one row above the next, each
bowing outward over one stake and inward under the next, alternating row to
row). The surface is displaced by those strands, so the silhouette is
lumpy, and lit per pixel from the displaced surface's own normals: a soft
key light from the upper left and front (as on our walkways), a little gloss
on strand tops, occlusion in the crevices between strands, the shadow of the
rim on the wall below it, the inside falling into shade. Every strand gets
its own straw tone, with fibres running along it. The rim is a braided coil,
a torus with twisting ridges.

The renderer splats a dense grid of surface samples into a depth buffer at
the canvas's resolution (which is itself supersampled), nearest sample
winning; render() returns colour and coverage for one pass:
  'inside'    the far inner wall (seen over the near rim)
  'rim_back'  the far half of the rim
  'outside'   the near outer wall and its base
  'rim_front' the near half of the rim
"""
import math
import numpy as np

R_TOP, R_BOT, DEPTH, BELLY = 58.0, 46.0, 44.0, 3.2   # wide enough for two riders
TILT = 0.2                      # the view's tilt (radians), looking down a little
SIN_E, COS_E = math.sin(TILT), math.cos(TILT)
N_STAKES = 62
ROW = 4.0                       # weaver row height (px)
LIGHT = np.array([-0.55, -0.62, 0.56], np.float32)
LIGHT /= np.linalg.norm(LIGHT)
HALF = (LIGHT + np.array([0, 0, 1], np.float32))
HALF /= np.linalg.norm(HALF)

STRAW = np.array([132, 72, 42], np.float32)     # dried, weathered: a dark red-brown
STAKE = np.array([100, 52, 30], np.float32)
BOUNCE = np.array([0.92, 0.96, 0.84], np.float32)   # greenish light from the jungle below


def profile(h):
    """The wall's radius at depth h below the rim."""
    t = np.clip(h / DEPTH, 0, 1)
    r = R_TOP + (R_BOT - R_TOP) * t ** 1.4 + BELLY * np.sin(np.pi * t)
    base = np.clip((h - (DEPTH - 7)) / 7.5, 0, 1)        # the rounded base
    return r * np.sqrt(np.clip(1 - base ** 2, 0.02, 1))


def cell_hash(a, b, seed):
    """A repeatable pseudo-random number in 0..1 for each (a, b) cell."""
    v = np.sin(a * 12.9898 + b * 78.233 + seed * 37.719) * 43758.5453
    return v - np.floor(v)


def weave_height(theta, h, inside=False):
    """Radial displacement of the woven wall (px), which strand shows there
    (True: a weaver, False: a stake), the strand's indices, and where a
    broken weaver leaves a gap."""
    ph = theta * N_STAKES / (2 * np.pi)          # in stakes
    fs = ph - np.floor(ph)
    row = h / ROW
    ir = np.floor(row)
    fr = row - ir
    wave = np.cos(np.pi * (ph + ir))              # +1: the weaver is outward (over a stake)
    w_prof = np.sqrt(np.clip(1 - ((fr - 0.5) / 0.5) ** 2, 0, 1))
    hw = 1.25 * w_prof + 0.75 * wave - 0.35
    # beaten up: here and there a weaver span is broken out, leaving short
    # frayed stubs at each end (the strand snapped and splayed)
    span = np.floor(ph)
    # breaks run over one span or two (a pair of spans shares a hash)
    pair = np.floor(span / 2)
    broken = ((cell_hash(span, ir, 5) < 0.09) | (cell_hash(pair, ir, 6) < 0.035)) & (ir >= 1) & (ir <= DEPTH / ROW - 2)
    stub = np.clip(1 - np.minimum(fs, 1 - fs) / 0.24, 0, 1)           # near the span's ends
    # snapped ends splay outward; a few stick right out past the wall
    splay = 0.9 + 2.6 * (cell_hash(span, ir, 7) < 0.3)
    fray = splay * np.abs(np.sin(h * 11.0 + theta * 160.0)) ** 0.7 * stub
    hw = np.where(broken, np.where(stub > 0.05, hw * 0.55 + fray, -2.0), hw)
    s_prof = np.sqrt(np.clip(1 - ((fs - 0.5) / 0.2) ** 2, 0, 1))
    hs = 1.15 * s_prof - 0.25 - 0.35 * (wave > 0)
    weaver = hw >= hs
    hgt = np.maximum(hw, hs)
    hole = broken & (stub <= 0.05) & ~(hs > -1.0)
    hole = hole | (broken & (hgt < -1.2))
    return hgt, weaver, ir, span, hole


def to_view(x, y, z):
    """World (x right, y down, z toward the viewer) to view coordinates."""
    return x, y * COS_E + z * SIN_E, z * COS_E - y * SIN_E


def normals(P):
    """Normals of a sampled surface P[..., 3] (grid over theta, h)."""
    du = np.gradient(P, axis=1)
    dv = np.gradient(P, axis=0)
    n = np.cross(du, dv)
    ln = np.linalg.norm(n, axis=-1, keepdims=True)
    return n / np.maximum(ln, 1e-6)


def shade(n, albedo, ao, gloss=0.06, shininess=12.0, ambient=0.3):
    nd = np.clip((n @ LIGHT + 0.25) / 1.25, 0, 1)            # soft wrap
    sp = np.clip(n @ HALF, 0, 1) ** shininess * gloss
    up = np.clip(n[..., 1], 0, 1)                             # facing down: jungle bounce
    amb = ambient * (1 + 0.35 * up)[..., None] * BOUNCE
    col = albedo * (amb * ao[..., None] + 0.95 * nd[..., None] * np.sqrt(ao)[..., None]) + 255 * sp[..., None] * ao[..., None]
    return col


def fibre(a, b, seed):
    """Fine streaks running along a strand: noise that varies quickly across
    (b) and slowly along (a)."""
    rng = np.random.default_rng(seed)
    k = rng.uniform(0, 6.28, 6)
    return (np.sin(b * 7.1 + k[0] + np.sin(a * 0.9 + k[1]) * 1.3) * 0.5 +
            np.sin(b * 13.7 + k[2] + np.sin(a * 1.7 + k[3])) * 0.3 +
            np.sin(b * 23.3 + k[4] + a * 0.5) * 0.2)


def value_noise(a, b, seed, scale):
    """Smooth noise (-1..1) over two surface coordinates, at a feature size."""
    rng = np.random.default_rng(seed)
    g = rng.uniform(-1, 1, (64, 64)).astype(np.float32)
    fa, fb = a / scale, b / scale
    ia, ib = np.floor(fa).astype(int), np.floor(fb).astype(int)
    ta, tb = fa - ia, fb - ib
    ta, tb = ta * ta * (3 - 2 * ta), tb * tb * (3 - 2 * tb)
    def G(i, j):
        return g[np.mod(j, 64), np.mod(i, 64)]
    return (G(ia, ib) * (1 - ta) * (1 - tb) + G(ia + 1, ib) * ta * (1 - tb) +
            G(ia, ib + 1) * (1 - ta) * tb + G(ia + 1, ib + 1) * ta * tb)


def mottle(a, b, seed):
    """Weathering: broad faded and darkened blotches, a finer grain over them."""
    m = 0.62 * value_noise(a, b, seed, 14.0) + 0.38 * value_noise(a, b, seed + 1, 5.0)
    return 1 + 0.24 * m


def tone_table(n, seed, spread):
    rng = np.random.default_rng(seed)
    return 1 + rng.uniform(-spread, spread, n).astype(np.float32)


def dents(theta, h):
    """Knocks and sagging: the wall pushed in and out, more near the rim."""
    a = theta * R_TOP
    return (3.6 * value_noise(a, h, 31, 24.0) + 1.6 * value_noise(a, h, 32, 9.0)) * (1.0 - 0.45 * np.clip(h / DEPTH, 0, 1))


def wall(theta, h, inside):
    hgt, weaver, ir, ist, hole = weave_height(theta, h, inside)
    r = profile(h) + dents(theta, h)
    rr = r + (-1.0 if inside else 1.0) * 0.85 * hgt - (2.2 if inside else 0)
    x = rr * np.sin(theta)
    z = rr * np.cos(theta)
    P = np.stack(to_view(x, h, z), -1)
    n = normals(P)
    # orient: outward for the outside, inward for the inside
    rad = np.stack(to_view(np.sin(theta), np.zeros_like(h), np.cos(theta)), -1)
    flip = np.sign((n * rad).sum(-1, keepdims=True))
    n = n * flip * (-1 if inside else 1)
    rows = tone_table(64, 3, 0.16)
    stakes = tone_table(N_STAKES + 2, 4, 0.14)
    t_row = rows[np.clip(ir.astype(int), 0, 63)]
    t_stk = stakes[np.mod(ist.astype(int), N_STAKES)]
    fib_w = fibre(theta * R_TOP, h, 1)
    fib_s = fibre(h, theta * R_TOP, 2)
    alb = np.where(weaver[..., None], STRAW * (t_row * (1 + 0.06 * fib_w))[..., None],
                   STAKE * (t_stk * (1 + 0.06 * fib_s))[..., None])
    a = theta * R_TOP
    # dried and weathered: blotches across the weave, sun-faded and darkened
    alb = alb * mottle(a, h, 11 if inside else 12)[..., None]
    # worn: the most exposed strand tops rubbed pale
    alb = alb * (1 + 0.22 * np.clip((hgt - 0.85) / 0.6, 0, 1))[..., None]
    # stains soaked in, dark and blotchy
    stain = np.clip(value_noise(a, h, 41, 11.0) * 1.7 - 0.45, 0, 1)
    alb = alb * (1 - 0.42 * stain)[..., None]
    # where a weaver is broken out, the gap shows the dark inside
    alb = np.where(hole[..., None], alb * 0.28, alb)
    # occlusion: crevices (low displacement) darker
    ao = 0.42 + 0.58 * np.clip((hgt + 0.6) / 1.9, 0, 1)
    if inside:
        # the near wall shades the inside; deeper is darker
        ao = ao * np.clip(1.0 - h / 18.0, 0.18, 1.0) * 0.7
    else:
        ao = ao * (1 - 0.5 * np.exp(-h / 3.2))          # the rim's shadow
        ao = ao * (1 - 0.25 * np.clip((h - (DEPTH - 9)) / 9, 0, 1))
    col = shade(n, alb, ao)
    return P, col


def rim_coil(theta, phi):
    """The braided rim: a torus round the top, its tube swelling in twisting
    ridges (three strands)."""
    ridge = np.abs(np.sin(theta * 34 + phi * 1.5))
    rt = 3.3 + 0.6 * ridge ** 0.6
    # beaten up: a section rubbed thin, the whole rim sagging and wobbling
    rt = rt * (1 - 0.32 * np.exp(-((np.mod(theta - 0.75 + np.pi, 2 * np.pi) - np.pi) / 0.16) ** 2))
    a = theta * R_TOP
    rho = R_TOP + 0.8 + rt * np.cos(phi) + 2.6 * value_noise(a, np.zeros_like(a), 31, 24.0) + 0.8 * value_noise(a, phi, 23, 6.0)
    hh = -1.2 - rt * np.sin(phi) + 1.6 * value_noise(a, np.full_like(a, 3.0), 22, 30.0)
    x = rho * np.sin(theta)
    z = rho * np.cos(theta)
    P = np.stack(to_view(x, hh, z), -1)
    n = normals(P)
    out = np.stack(to_view(np.cos(phi) * np.sin(theta), -np.sin(phi), np.cos(phi) * np.cos(theta)), -1)
    n = n * np.sign((n * out).sum(-1, keepdims=True))
    fib = fibre(theta * 40, phi * 3, 7)
    alb = STRAW * 1.04 * (1 + 0.06 * fib)[..., None]
    ao = 0.55 + 0.45 * ridge ** 0.5
    alb = alb * mottle(theta * 40, phi * 4 + theta * 20, 13)[..., None]
    return P, shade(n, alb, ao, gloss=0.08)


def splat(P, col, x0, y0, w, h, ss, cy):
    """Splat samples into a depth-buffered image; returns rgb, coverage."""
    X = (P[..., 0] - x0) * ss
    Y = (P[..., 1] + cy - y0) * ss
    D = P[..., 2]
    px = np.round(X).astype(int).ravel()
    py = np.round(Y).astype(int).ravel()
    d = D.ravel()
    c = col.reshape(-1, 3)
    ok = (px >= 0) & (px < w) & (py >= 0) & (py < h)
    px, py, d, c = px[ok], py[ok], d[ok], c[ok]
    order = np.argsort(d)                   # far first: nearer samples written last
    img = np.zeros((h, w, 3), np.float32)
    cov = np.zeros((h, w), np.float32)
    img[py[order], px[order]] = c[order]
    cov[py[order], px[order]] = 1
    return img, cov


def render(which, x0, y0, w, h, ss, rim_y):
    """One pass at the canvas's (supersampled) resolution: (rgb, coverage)."""
    if which in ('inside', 'outside'):
        inside = which == 'inside'
        n_t, n_h = 2500, 560
        if inside:
            th = np.linspace(np.pi / 2 - 0.1, 3 * np.pi / 2 + 0.1, n_t)
        else:
            th = np.linspace(-np.pi / 2 - 0.1, np.pi / 2 + 0.1, n_t)
        hh = np.linspace(0, DEPTH, n_h)
        T, H = np.meshgrid(th, hh)
        P, col = wall(T, H, inside)
        if not inside:
            # the base, seen only at the bottom edge: close the bowl
            pass
        return splat(P, col, x0, y0, w, h, ss, rim_y)
    th = np.linspace(-np.pi / 2 - 0.15, np.pi / 2 + 0.15, 3200) if which == 'rim_front' else np.linspace(np.pi / 2 - 0.15, 3 * np.pi / 2 + 0.15, 3200)
    ph = np.linspace(0, 2 * np.pi, 120)
    T, F = np.meshgrid(th, ph)
    P, col = rim_coil(T, F)
    return splat(P, col, x0, y0, w, h, ss, rim_y)
