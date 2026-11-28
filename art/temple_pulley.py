"""The temple lift's pulley and its swan-neck gallows (owner, 2026-11-27: the
ornate variant, "but let's make it look like it's been in the desert for a
long time"). Our own drawing, no game pixels: shaded at 4x and brought
down, hard-edged like the game's sprites with a soft dark edge against the
sky, light from the upper left like the temple.

- The gallows: a twisted iron post on a bolted foot plate standing on the
  slab east of the lift (x 4291, on the slab at y 396; owner, 2026-11-27:
  mirrored to stand right of the lift, its leaf and counterweight tail
  taken off), a swan neck sweeping west over the shaft with a gold finial
  on its crest, collars where the neck springs and at its tip, and a hook
  down to the pulley's eye. Shaded for its own side, the light still from
  the upper left.
- The pulley: a cast iron web with six small windows, twelve gold rays from
  a gold boss out to the rim, a gold-lipped rim, a cheek strap with bronze
  collars and a ring eye, and the lift's cable lying in its groove over the
  top. Its centre (4241.5, 196), groove radius 20: the cable leaves it at
  x 4221.5 and 4261.5, where the game's own cables carry on down
  (madeart/temple_top.cpp draws them from y 196).
- Long in the desert: the iron sun-bleached and sand-scoured to a dusty
  brown-grey, rust blooming warm in patches, pitted, a few flakes showing
  paler metal; sand and dust settled on every upward face and caught in the
  post's spiral; the gold worn thin, rubbed through to dark metal on its
  exposed edges and bright only in recesses; verdigris in the bronze's
  hollows.

    python3 temple_pulley.py OUT.png

writes the sprite (one frame, RGBA, 'openc2e-frames' 1) and prints its
top-left world corner and size (additions/temple-top.cos places it there).
"""
import sys
import math
import numpy as np
from PIL import Image, PngImagePlugin

# the area drawn (trimmed to what is painted when written)
X0, Y0, X1, Y1 = 4190, 80, 4310, 400
SS = 4
W, H = (X1 - X0) * SS, (Y1 - Y0) * SS
FLOOR = 396
CX, CY, GROOVE, FLANGE = 4241.5, 196.0, 20.0, 23.0
PX = 4291.0                      # the post: east of the cage (x 4172-4278)
LIGHT = np.array([-0.55, -0.65, 0.53])
LIGHT /= np.linalg.norm(LIGHT)
HALF = LIGHT + np.array([0, 0, 1.0])
HALF /= np.linalg.norm(HALF)


class Canvas:
    def __init__(self):
        self.c = np.zeros((H, W, 3), float)
        self.a = np.zeros((H, W), float)
        ys, xs = np.mgrid[0:H, 0:W].astype(float)
        self.wx = X0 + (xs + 0.5) / SS
        self.wy = Y0 + (ys + 0.5) / SS

    def paint(self, mask, col):
        m = mask.astype(float) if mask.dtype == bool else mask
        self.c = self.c * (1 - m[..., None]) + col * m[..., None]
        self.a = np.maximum(self.a, m)


# ---- noise (world coordinates, so every part weathers as one piece) ----------

def _lattice(ix, iy, seed):
    h = (ix * 374761393 + iy * 668265263 + seed * 2246822519) & 0xffffffff
    h = ((h ^ (h >> 13)) * 1274126177) & 0xffffffff
    return ((h ^ (h >> 16)) & 0xffff) / 65535.0


def vnoise(x, y, scale, seed):
    gx, gy = x / scale, y / scale
    ix, iy = np.floor(gx).astype(np.int64), np.floor(gy).astype(np.int64)
    fx, fy = gx - ix, gy - iy
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    a, b = _lattice(ix, iy, seed), _lattice(ix + 1, iy, seed)
    c, d = _lattice(ix, iy + 1, seed), _lattice(ix + 1, iy + 1, seed)
    return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy


class Wear:
    """The weathering fields, made once."""
    def __init__(self, cv):
        x, y = cv.wx, cv.wy
        self.rust = 0.6 * vnoise(x, y, 6.0, 11) + 0.4 * vnoise(x, y, 2.2, 12)
        self.dust = 0.6 * vnoise(x, y, 3.0, 21) + 0.4 * vnoise(x, y, 1.1, 22)
        self.grit = vnoise(x, y, 0.55, 31)
        self.flake = vnoise(x, y, 3.5, 41)
        self.pit = vnoise(x, y, 0.4, 51)
        self.verd = 0.5 * vnoise(x, y, 2.5, 61) + 0.5 * vnoise(x, y, 0.9, 62)


WEAR = None

SAND = np.array([226, 200, 148], float)
DUST = np.array([176, 156, 122], float)
RUST = np.array([150, 82, 40], float)
RUST_DARK = np.array([92, 52, 30], float)
VERDIGRIS = np.array([96, 150, 120], float)
BARE = np.array([150, 140, 126], float)

# materials: (base, highlight, specular, power)
MATS = {
    'iron': (np.array([86, 76, 66], float), np.array([238, 214, 176], float), 0.55, 18),
    'iron_dull': (np.array([80, 72, 62], float), np.array([210, 186, 150], float), 0.30, 14),
    'bronze': (np.array([132, 96, 52], float), np.array([246, 214, 150], float), 0.60, 20),
    'gold': (np.array([176, 140, 62], float), np.array([255, 236, 170], float), 0.55, 18),
}


def lit(mat, nx, ny, nz, belly=0.28, groove=None):
    """Shaded, then weathered by material: what faces up gathers sand, the
    iron rusts in patches and is pitted, the gold is worn through on its
    exposed edges, and the bronze greens in its hollows."""
    base, hi, spec, power = MATS[mat]
    d = np.clip(nx * LIGHT[0] + ny * LIGHT[1] + nz * LIGHT[2], 0, 1)
    s = np.clip(nx * HALF[0] + ny * HALF[1] + nz * HALF[2], 0, 1) ** power
    up = np.clip(-ny * 1.6, 0, 1)                  # how much it faces the sky (its upper half)
    hollow = np.clip(1 - nz, 0, 1) ** 1.5          # edges and turned-away faces
    w = WEAR
    albedo = np.broadcast_to(base, (H, W, 3)).copy()
    if mat.startswith('iron'):
        # sun-bleached; rust blooms where the noise gathers, deeper in the
        # lower, sheltered faces; pits; a few flakes of paler bare metal
        r = np.clip((w.rust - 0.46) / 0.16, 0, 1) * (0.65 + 0.35 * np.clip(ny + 0.3, 0, 1)) * (0.75 + 0.5 * w.grit)
        rustcol = RUST[None, None] * (1 - 0.4 * r[..., None]) + RUST_DARK[None, None] * 0.4 * r[..., None]
        albedo = albedo * (1 - r[..., None]) + rustcol * r[..., None]
        fl = (w.flake > 0.78) & (w.grit > 0.45)
        albedo[fl] = albedo[fl] * 0.4 + BARE * 0.6
        pit = w.pit > 0.86
        albedo[pit] *= 0.55
        spec_k = spec * (1 - 0.6 * r)
    elif mat == 'gold':
        # worn thin: dull, and rubbed through to dark metal where it is most
        # exposed (lit, facing out); bright only in its recesses
        exposed = np.clip(d * 1.2 - 0.35, 0, 1) * np.clip((w.grit + w.dust) / 2 + 0.25, 0, 1)
        albedo = albedo * (1 - 0.75 * exposed[..., None]) + np.array([70, 56, 38], float) * 0.75 * exposed[..., None]
        albedo = albedo * (1 + 0.35 * hollow * (1 - exposed))[..., None]
        spec_k = spec * (0.45 + 0.75 * hollow) * (1 - exposed)
    else:  # bronze
        v = np.clip((hollow * 1.8 + 0.6 * np.clip(ny, 0, 1)) * (0.55 + w.verd) - 0.28, 0, 1)
        albedo = albedo * (1 - v[..., None]) + VERDIGRIS * v[..., None]
        spec_k = spec * (1 - v) * 0.8
    if groove is not None:
        # sand caught in a groove
        g = np.clip(groove * 1.6 - 0.3, 0, 1) * (0.5 + 0.5 * w.dust)
        albedo = albedo * (1 - g[..., None]) + SAND * g[..., None]
    rim = np.clip(-nx * 0.6 + ny * 0.5, 0, 1) ** 3 * 0.18
    k = (0.32 + 0.78 * d) * (1 - belly * np.clip(ny, 0, 1)) + rim
    col = albedo * k[..., None] + hi[None, None] * (spec_k * s)[..., None]
    # sand and dust settled on what faces up, in drifts, gritty
    sd = np.clip((up - 0.3) / 0.35, 0, 1) * np.clip(w.dust * 1.8 - 0.25, 0, 1)
    sand = (SAND * (0.85 + 0.25 * d)[..., None] * (0.92 + 0.16 * w.grit)[..., None])
    col = col * (1 - sd[..., None]) + sand * sd[..., None]
    # a fine dust over everything, more in the hollows
    dd = (0.10 + 0.12 * hollow) * w.dust
    col = col * (1 - dd[..., None]) + DUST * dd[..., None]
    return col


def tube(cv, x0, y0, x1, y1, w0, w1, mat, twist=None, ext=0.15):
    dx, dy = x1 - x0, y1 - y0
    L = math.hypot(dx, dy)
    if L < 1e-6:
        return
    ux, uy = dx / L, dy / L
    px, py = cv.wx - x0, cv.wy - y0
    t = px * ux + py * uy
    tt = np.clip(t / L, 0, 1)
    w = w0 + (w1 - w0) * tt
    u = (px * -uy + py * ux) / (w / 2)
    m = (t >= -ext) & (t <= L + ext) & (np.abs(u) <= 1)
    nz = np.sqrt(np.clip(1 - u * u, 0, 1))
    nx, ny = -uy * u, ux * u
    groove = None
    if twist is not None:
        ph = (t / twist + u * 0.35) % 1.0
        groove = np.clip(1 - np.abs(ph - 0.5) * 5, 0, 1)
    col = lit(mat, nx, ny, nz, groove=groove)
    if twist is not None:
        col = col * (1 - 0.45 * groove[..., None]) + (MATS[mat][1] * 0.18)[None, None] * np.clip(np.sin(ph * 2 * np.pi), 0, 1)[..., None] * nz[..., None]
    cv.paint(m, col)


def bezier(p0, p1, p2, p3, n):
    out = []
    for i in range(n + 1):
        t = i / n
        a, b, c, d = (1 - t) ** 3, 3 * (1 - t) ** 2 * t, 3 * (1 - t) * t * t, t ** 3
        out.append((a * p0[0] + b * p1[0] + c * p2[0] + d * p3[0], a * p0[1] + b * p1[1] + c * p2[1] + d * p3[1]))
    return out


def sweep(cv, pts, w0, w1, mat, twist=None):
    n = len(pts) - 1
    for i, ((xa, ya), (xb, yb)) in enumerate(zip(pts, pts[1:])):
        tube(cv, xa, ya, xb, yb, w0 + (w1 - w0) * i / n, w0 + (w1 - w0) * (i + 1) / n, mat, twist=twist, ext=0.25)


def ellipsoid(cv, cx, cy, rx, ry, mat, facets=0, belly=0.32):
    dx, dy = (cv.wx - cx) / rx, (cv.wy - cy) / ry
    q = dx * dx + dy * dy
    m = q <= 1
    nx, ny = dx, dy
    if facets:
        ang = np.arctan2(dy, dx)
        step = 2 * math.pi / facets
        snap = np.round(ang / step) * step
        r = np.sqrt(np.clip(q, 0, 1))
        nx, ny = np.cos(snap) * r, np.sin(snap) * r
    nz = np.sqrt(np.clip(1 - nx * nx - ny * ny, 0, 1))
    cv.paint(m, lit(mat, nx, ny, nz, belly=belly))


def band(cv, cx, cy, w, h, mat):
    dy = (cv.wy - cy) / (h / 2)
    m = (np.abs(cv.wx - cx) <= w / 2) & (np.abs(dy) <= 1)
    nz = np.sqrt(np.clip(1 - dy * dy, 0, 1))
    nx = (cv.wx - cx) / (w / 2) * 0.55
    cv.paint(m, lit(mat, nx * nz, dy, nz * np.sqrt(np.clip(1 - nx * nx, 0, 1))))


def ring(cv, cx, cy, r0, r1, mat, concave=False):
    dx, dy = cv.wx - cx, cv.wy - cy
    r = np.hypot(dx, dy) + 1e-6
    m = (r >= r0) & (r <= r1)
    t = (r - (r0 + r1) / 2) / ((r1 - r0) / 2)
    if concave:
        t = -t
    nz = np.sqrt(np.clip(1 - t * t, 0, 1))
    cv.paint(m, lit(mat, dx / r * t, dy / r * t, nz))


def plate(cv, x0, y0, x1, y1, mat):
    m = (cv.wx >= x0) & (cv.wx < x1) & (cv.wy >= y0) & (cv.wy < y1)
    ny = np.where(cv.wy < y0 + 1.2, -0.8, np.where(cv.wy > y1 - 1.0, 0.8, 0.0))
    nx = np.where(cv.wx < x0 + 1.0, -0.7, np.where(cv.wx > x1 - 1.0, 0.7, 0.0))
    nz = np.sqrt(np.clip(1 - nx * nx - ny * ny, 0.05, 1))
    cv.paint(m, lit(mat, nx, ny, nz, belly=0))


def occlude(cv, cx, cy, r, k=0.45):
    d = np.hypot(cv.wx - cx, cv.wy - cy)
    f = np.clip(1 - d / r, 0, 1) ** 1.5 * k
    cv.c = cv.c * (1 - f * (cv.a > 0))[..., None]


def occlude_line(cv, x0, y0, x1, y1, r, k=0.4):
    dx, dy = x1 - x0, y1 - y0
    L = math.hypot(dx, dy)
    ux, uy = dx / L, dy / L
    px, py = cv.wx - x0, cv.wy - y0
    t = np.clip(px * ux + py * uy, 0, L)
    d = np.hypot(px - t * ux, py - t * uy)
    f = np.clip(1 - d / r, 0, 1) ** 1.5 * k
    cv.c = cv.c * (1 - f * (cv.a > 0))[..., None]


# ---- the pulley ---------------------------------------------------------------

def cable_in_groove(cv):
    dx, dy = cv.wx - CX, cv.wy - CY
    r = np.hypot(dx, dy) + 1e-6
    over = (np.abs(r - (GROOVE + 0.4)) <= 1.15) & (dy <= 0.2)
    top = over & (r > GROOVE + 0.5) & (dx < 5)
    cv.paint(over, np.broadcast_to(np.array([24, 19, 12], float), (H, W, 3)))
    cv.paint(top, np.broadcast_to(np.array([104, 90, 68], float), (H, W, 3)))   # (dusty on top)


def web(cv, holes):
    dx, dy = cv.wx - CX, cv.wy - CY
    r = np.hypot(dx, dy) + 1e-6
    w = r <= GROOVE - 0.5
    hole = np.zeros_like(w)
    for hx, hy, hr in holes:
        hole |= np.hypot(cv.wx - hx, cv.wy - hy) <= hr
    t = np.clip(r / (GROOVE - 0.5), 0, 1)
    nx, ny = dx / r * 0.32 * t, dy / r * 0.32 * t
    nz = np.sqrt(np.clip(1 - nx * nx - ny * ny, 0, 1))
    cv.paint(w & ~hole, lit('iron_dull', nx, ny, nz) * (0.94 + 0.12 * WEAR.grit)[..., None])
    ring(cv, CX, CY, 7.6, 8.8, 'iron')
    for hx, hy, hr in holes:
        ring(cv, hx, hy, hr, hr + 1.2, 'iron', concave=True)


def sunburst(cv, n, r_long, r_short, w_long, w_short, boss):
    occlude(cv, CX, CY, r_long + 1.5, k=0.3)
    for k in range(n):
        a = k * 2 * math.pi / n
        long = k % 2 == 0
        r2 = r_long if long else r_short
        tube(cv, CX + math.cos(a) * 2.4, CY + math.sin(a) * 2.4, CX + math.cos(a) * r2, CY + math.sin(a) * r2,
             (w_long if long else w_short), 0.5, 'gold')
    ellipsoid(cv, CX, CY, boss, boss, 'gold', belly=0.35)


def pulley(cv):
    """The sunburst grown out to the rim: twelve gold rays over a plain cast
    web, small windows between the long ones; returns the eye's height."""
    eye_y = CY - 34
    cable_in_groove(cv)
    holes = [(CX + math.cos(k * math.pi / 3 + math.pi / 6) * 13.2, CY + math.sin(k * math.pi / 3 + math.pi / 6) * 13.2, 2.4) for k in range(6)]
    web(cv, holes)
    ring(cv, CX, CY, GROOVE - 0.8, FLANGE - 1.4, 'iron')
    ring(cv, CX, CY, FLANGE - 1.9, FLANGE, 'gold')
    ring(cv, CX, CY, GROOVE - 1.8, GROOVE - 0.6, 'iron')
    cable_in_groove(cv)
    # the cheek strap and its eye
    occlude_line(cv, CX, eye_y + 4, CX, CY - 3, 4.2, k=0.35)
    tube(cv, CX, eye_y + 4, CX, CY - 2, 3.6, 5.0, 'iron')
    band(cv, CX, CY - 13, 6.6, 2.4, 'bronze')
    band(cv, CX, eye_y + 7.2, 5.2, 2.0, 'bronze')
    ring(cv, CX, eye_y, 2.2, 4.6, 'gold')
    sunburst(cv, 12, GROOVE - 1.2, 11.5, 2.0, 1.4, 3.4)
    ring(cv, CX, CY, 4.2, 5.0, 'gold')
    return eye_y


# ---- the gallows ----------------------------------------------------------------

def gallows(cv):
    eye_y = pulley(cv)
    plate(cv, PX - 6, FLOOR - 4, PX + 6, FLOOR, 'iron_dull')
    for bx in (PX - 6 + 1.8, PX + 6 - 1.8):
        ellipsoid(cv, bx, FLOOR - 4 + 1.6, 1.0, 0.9, 'bronze')
    occlude(cv, PX, FLOOR - 4, 6, k=0.35)
    tube(cv, PX, FLOOR - 3.5, PX, 186, 4.6, 4.0, 'iron', twist=3.2)
    ellipsoid(cv, PX, FLOOR - 6.5, 3.8, 2.4, 'iron')
    # the swan neck, from the post west over the shaft to the wheel
    path = bezier((PX, 185.0), (PX, 98.0), (CX, 98.0), (CX, 150.0), 90)
    sweep(cv, path, 4.0, 2.6, 'iron')
    top = min(path, key=lambda p: p[1])
    tube(cv, top[0], top[1] - 1.5, top[0], top[1] - 6.0, 1.6, 0.9, 'gold')
    ellipsoid(cv, top[0], top[1] - 7.6, 1.9, 2.3, 'gold')
    occlude(cv, PX, 186, 5, k=0.4)
    band(cv, PX, 186, 7.2, 3.2, 'bronze')
    band(cv, PX, 189.6, 5.6, 1.6, 'gold')
    band(cv, CX, 150.5, 5.4, 2.4, 'bronze')
    occlude(cv, CX, eye_y - 4, 4, k=0.3)
    tube(cv, CX, 151.5, CX, eye_y - 4.0, 1.8, 1.6, 'iron')


def finish(cv):
    """Down to 1x: hard alpha, with a soft dark edge (1 px) against the sky."""
    a = cv.a
    grow = a.copy()
    for _ in range(SS):
        g = grow.copy()
        g[1:] = np.maximum(g[1:], grow[:-1]); g[:-1] = np.maximum(g[:-1], grow[1:])
        g[:, 1:] = np.maximum(g[:, 1:], grow[:, :-1]); g[:, :-1] = np.maximum(g[:, :-1], grow[:, 1:])
        grow = g
    edge = np.clip(grow - a, 0, 1) * 0.55
    col = cv.c * a[..., None] + np.array([40, 32, 24], float) * edge[..., None]
    al = np.clip(a + edge, 0, 1)
    h, w = H // SS, W // SS
    col = col.reshape(h, SS, w, SS, 3).mean(axis=(1, 3))
    al = al.reshape(h, SS, w, SS).mean(axis=(1, 3))
    out = np.zeros((h, w, 4), np.uint8)
    out[..., :3] = np.clip(col / np.maximum(al, 1e-6)[..., None], 0, 255)
    out[..., 3] = np.where(al > 0.42, 255, 0)
    return out


def make():
    """The sprite (RGBA array) and its top-left world corner."""
    global WEAR
    cv = Canvas()
    WEAR = Wear(cv)
    gallows(cv)
    out = finish(cv)
    ys, xs = np.nonzero(out[..., 3])
    x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
    return out[y0:y1, x0:x1], (X0 + int(x0), Y0 + int(y0))


def main():
    sprite, (x, y) = make()
    info = PngImagePlugin.PngInfo()
    info.add_text('openc2e-frames', '1')
    Image.fromarray(sprite, 'RGBA').save(sys.argv[1], pnginfo=info)
    print('wrote', sys.argv[1], 'top-left', x, y, 'size', sprite.shape[1], 'x', sprite.shape[0])


if __name__ == '__main__':
    main()
