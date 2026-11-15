"""The west sea's seabed airlock (owner, 2026-11-12): its two doors, the
invisible car that carries a creature through the rock, and the rock drawn
again in front of the creature while it rides.

The doors are original art, drawn here from numbers (no game pixels): each
is modelled as heights (a bead rim with bolts, a domed face with a lip,
rivets, a handwheel), textured with noise (rust, grime, pitting, rust weeping
from rivets, weed and silt low on the sea door) and lit from the top left
with cast shadows, ambient occlusion and a little rough-metal shine (owner,
2026-11-12: "more rusty and dingy, more dark, and more rendered with volume
and light"). The
rock is cut from the player's own installed background on their machine
(bring-your-own-data: only this recipe is part of the project).
westlock.cos places them.

    python3 westlock.py C2_DIR USER_DATA_DIR

writes into USER_DATA_DIR/Images:
  zwsd.png  the sea door, 56 x 56, top-left (600, 1302), in the rock at
            the foot of the seabed's slope. Frames: 0 shut (lamp green, ready),
            1 shut (lamp red, busy), 2-4 swinging open, 5 open.
  zwld.png  the land door, 56 x 56, top-left (538, 1484), in the rock face
            at the east end of the floor by the cage lift. Frames: 0 shut,
            1-3 swinging open, 4 open.
  zwcr.png  the car, 48 x 110, all clear: its box is where it picks up.
  zwrk.png  the rock, top-left (ROCK_X0, ROCK_Y0), to x 660 (east of that
            the game's own rock is drawn over everything): the rock below the
            seabed's floor line round the hidden way down and west, clear
            above that line (creatures on the seabed show) and over the two
            doors (the whole door shows; a creature is seen in a doorway).
"""
import sys
import os
import math
import numpy as np
from PIL import Image, PngImagePlugin

SS = 4  # supersampling

SEA_DOOR = (628, 1330)   # centre, world px (left of the game's rock, drawn over all, from x ~650)
LAND_DOOR = (566, 1512)
DOOR_D = 56              # the sprites' size; the opening's radius:
OPEN_R = 0.78 * DOOR_D / 2   # leaves a heavy rim at actual size

ROCK_X0, ROCK_Y0, ROCK_X1, ROCK_Y1 = 573, 1245, 660, 1546


def floor_y(x):
    """The seabed's floor line (room 121) between x 549 and 669."""
    return 1317 + (x - 549) * 55.0 / 120.0



# ---- the doors: lit height maps ----
N = DOOR_D * SS
C = N / 2
R = N / 2 - 1
R_OPEN = OPEN_R * SS
FR = R_OPEN + 1.5 * SS          # the face's radius (overlaps the rim a little)

LIGHT = np.array([-0.60, -0.72, 0.45])
LIGHT /= np.linalg.norm(LIGHT)
HALF = LIGHT + np.array([0, 0, 1.0])
HALF /= np.linalg.norm(HALF)

ys, xs = np.mgrid[0:N, 0:N].astype(np.float64)
DX, DY = xs + 0.5 - C, ys + 0.5 - C
RR = np.hypot(DX, DY)
ANG = np.arctan2(DY, DX)


# ---- small numpy helpers (no scipy here) ----
def blur(a, s):
    if s <= 0:
        return a
    r = int(3 * s)
    k = np.exp(-0.5 * (np.arange(-r, r + 1) / s) ** 2)
    k /= k.sum()
    p = np.pad(a, r, mode='edge')
    p = np.apply_along_axis(lambda v: np.convolve(v, k, 'valid'), 0, p)
    p = np.apply_along_axis(lambda v: np.convolve(v, k, 'valid'), 1, p)
    return p


def noise(scale, seed, octaves=5):
    """fBm value noise in 0..1, features about `scale` px."""
    rng = np.random.default_rng(seed)
    out = np.zeros((N, N))
    amp, tot = 1.0, 0.0
    s = scale
    for _ in range(octaves):
        g = max(2, int(N / s) + 2)
        grid = rng.random((g, g)).astype(np.float32)
        up = np.asarray(Image.fromarray(grid, 'F').resize((N, N), Image.BICUBIC), np.float64)
        out += amp * up
        tot += amp
        amp *= 0.5
        s /= 2
    out /= tot
    lo, hi = np.percentile(out, 1), np.percentile(out, 99)
    return np.clip((out - lo) / (hi - lo), 0, 1)


def smooth(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)


def bump(cx, cy, r, h):
    d = np.hypot(xs + 0.5 - cx, ys + 0.5 - cy)
    return h * np.sqrt(np.clip(1 - (d / r) ** 2, 0, 1)), d < r


def smear_down(mask, length, decay=0.93):
    """Streaks running down from a mask (rust weeping from rivets)."""
    out = mask.copy()
    cur = mask.copy()
    for _ in range(length):
        cur = np.roll(cur, 1, axis=0) * decay
        cur[0] = 0
        out = np.maximum(out, cur)
    return out


def normals(h):
    gy, gx = np.gradient(h)
    n = np.dstack([-gx, -gy, np.ones_like(h)])
    return n / np.linalg.norm(n, axis=2, keepdims=True)


def shadow(h, steps=28, step=1.5):
    """1 lit, 0 in shadow: march toward the light over the height map."""
    lxy = LIGHT[:2] / np.linalg.norm(LIGHT[:2])
    rise = LIGHT[2] / np.linalg.norm(LIGHT[:2])
    occ = np.zeros_like(h)
    for k in range(1, steps + 1):
        t = k * step
        ox, oy = int(round(lxy[0] * t)), int(round(lxy[1] * t))
        # sample the height t px toward the light
        sh = np.roll(np.roll(h, -oy, axis=0), -ox, axis=1)
        occ = np.maximum(occ, np.clip((sh - (h + t * rise)) / 3.0, 0, 1))
    return 1 - 0.85 * blur(occ, 1.5)


def shade(h, albedo, rough, ao_strength=1.0, amb=0.18, sun=1.25, spec=0.75, tint=(1.0, 0.96, 0.88)):
    n = normals(h)
    diff = np.clip(n @ LIGHT, 0, 1)
    nh = np.clip(n @ HALF, 0, 1)
    shin = 6 + 30 * (1 - rough)
    sp = (nh ** shin) * spec * (1 - rough) ** 1.5
    sh = shadow(h)
    ao = np.clip(blur(h, 6) - h, 0, None)
    ao = 1 - ao_strength * np.clip(ao / 6.0, 0, 0.8)
    # light from a little below too (dim teal bounce from the sea floor)
    bounce = np.clip(n @ np.array([0.2, 0.75, 0.6]) / 1.0, 0, 1)[..., None] * np.array([0.10, 0.16, 0.18])
    lit = albedo * (amb * ao[..., None] + sun * (diff * sh)[..., None] * np.array(tint) + bounce * ao[..., None])
    lit += (sp * sh)[..., None] * np.array([1.0, 0.95, 0.85]) * 255
    return lit


# ---- materials ----
def mix(a, b, t):
    t = t[..., None] if np.ndim(t) == 2 else t
    return a * (1 - t) + b * t


def col(c):
    return np.broadcast_to(np.array(c, np.float64), (N, N, 3))


def build(kind):
    if kind == 'sea':
        face_c, rim_c, wheel_c = (58, 66, 68), (112, 88, 50), (118, 40, 30)
        seed = 11
    else:
        face_c, rim_c, wheel_c = (112, 86, 46), (66, 68, 72), (46, 46, 50)
        seed = 23
    n1, n2, n3, n4 = noise(60, seed), noise(16, seed + 1), noise(5, seed + 2, 3), noise(110, seed + 3, 3)
    pit = noise(2.2, seed + 4, 2)
    low = smooth(-0.2 * N, 0.5 * N, DY)             # 0 at the top, 1 low down

    # ---------- rim ----------
    mid, half = (R + R_OPEN) / 2, (R - R_OPEN) / 2 + 2
    t = (RR - mid) / half
    rim_h = 14 * np.sqrt(np.clip(1 - t * t, 0, 1)) ** 0.8
    rim_in = (RR <= R) & (RR >= R_OPEN - 2)
    bolts = np.zeros((N, N))
    bolt_m = np.zeros((N, N), bool)
    for k in range(12):
        a = k * math.pi / 6 + math.pi / 12
        bh, bm = bump(C + mid * math.cos(a), C + mid * math.sin(a), 1.8 * SS, 5)
        bolts = np.maximum(bolts, bh)
        bolt_m |= bm
    rim_h = rim_h + bolts + 0.6 * (pit - 0.5) + 1.2 * (n2 - 0.5)
    rust_r = smooth(0.45, 0.75, 0.55 * n1 + 0.30 * n2 + 0.35 * low + 0.15 * smear_down(bolt_m.astype(float), 9 * SS))
    if kind == 'sea':
        verd = smooth(0.55, 0.8, n4 * 0.7 + n2 * 0.4) * (1 - rust_r)
        rim_alb = mix(col(rim_c), col((52, 82, 70)), 0.75 * verd)   # verdigris
    else:
        rim_alb = col(rim_c)
    rim_alb = mix(rim_alb, col((96, 46, 22)), 0.85 * rust_r)
    rim_alb = mix(rim_alb, col((50, 28, 16)), 0.6 * rust_r * n3)
    rim_alb = rim_alb * (0.75 + 0.35 * n3[..., None])
    rim_rough = np.clip(0.55 + 0.4 * rust_r, 0, 1)

    # ---------- the opening behind the door ----------
    inner = RR < R_OPEN
    depth = smooth(R_OPEN - 3 * SS, R_OPEN, RR)
    # the far inner wall catches light at the bottom right; the rest is dark
    wall = np.clip((DX * 0.5 + DY * 0.7) / R_OPEN, 0, 1) * depth
    hole_c = (10, 14, 16) if kind == 'sea' else (12, 11, 10)
    hole = col(hole_c) * (1 + 0.6 * n2[..., None] * 0.3) + wall[..., None] * np.array([40, 34, 28]) * 0.8
    if kind == 'sea':  # a sheen of standing water low in the chamber
        hole += (smooth(0.35 * R_OPEN, 0.8 * R_OPEN, DY) * (1 - depth))[..., None] * np.array([6, 16, 18])

    base_h = np.where(rim_in, rim_h, 0.0)
    base_alb = np.where(rim_in[..., None], rim_alb, 0)
    base_rough = np.where(rim_in, rim_rough, 0.7)
    base = shade(base_h, base_alb, base_rough, sun=0.85)
    base = np.where(inner[..., None], hole, base)
    # the rim shadows the opening's top-left edge
    base *= np.where(inner, 1 - 0.6 * smooth(R_OPEN - 4 * SS, R_OPEN, RR) * np.clip(-(DX + DY) / (1.4 * R_OPEN), 0, 1), 1)[..., None]
    base_a = (RR <= R).astype(np.float64)

    # ---------- the face ----------
    fr = FR
    rf = RR / fr
    face_m = RR <= fr
    face_h = 4 + 12 * np.clip(1 - rf * rf, 0, 1) ** 0.8               # domed
    face_h += 4 * smooth(0.82, 0.87, rf) * (1 - smooth(0.94, 0.99, rf))  # the lip
    riv = np.zeros((N, N))
    riv_m = np.zeros((N, N), bool)
    for k in range(10):
        a = k * math.pi / 5 + math.pi / 10
        bh, bm = bump(C + fr * 0.76 * math.cos(a), C + fr * 0.76 * math.sin(a), 1.9 * SS, 3.5)
        riv = np.maximum(riv, bh)
        riv_m |= bm
    face_h += riv
    # handwheel: a ring, three spokes, a hub
    wr = fr * 0.38
    wt = 1.6 * SS
    ring = 6 * np.sqrt(np.clip(1 - ((RR - wr) / wt) ** 2, 0, 1))
    spokes = np.zeros((N, N))
    for k in range(3):
        a = k * math.pi / 3 + math.pi / 6
        d = np.abs(-math.sin(a) * DX + math.cos(a) * DY)
        spokes = np.maximum(spokes, 5 * np.sqrt(np.clip(1 - (d / (1.1 * SS)) ** 2, 0, 1)) * (RR < wr))
    hub, hub_m = bump(C, C, 2.8 * SS, 7)
    wheel = np.maximum(np.maximum(ring, spokes), hub)
    wheel_m = wheel > 0.05
    face_h = np.where(wheel_m, face_h + 4 + wheel, face_h)
    face_h += 0.5 * (pit - 0.5) + 1.0 * (n2 - 0.5) * 0.8

    weep = smear_down(riv_m.astype(float), 14 * SS) * (0.5 + 0.5 * n2)
    weep = np.maximum(weep, smear_down(hub_m.astype(float) * 0.8, 18 * SS) * (0.4 + 0.6 * n3))
    rust_f = smooth(0.48, 0.70, 0.60 * n1 + 0.25 * n2 + 0.30 * low + 0.6 * weep + 0.25 * smooth(0.85, 1.0, rf))
    alb = col(face_c)
    if kind == 'land':  # tarnished brass, darker in patches
        alb = mix(alb, col((70, 60, 38)), 0.6 * n4)
    alb = mix(alb, col((98, 48, 24)), 0.9 * rust_f)
    alb = mix(alb, col((48, 26, 14)), 0.55 * rust_f * n3)
    grime = smooth(0.4, 0.9, low * 0.8 + n1 * 0.4)
    if kind == 'sea':  # weed and silt on the lower half
        alb = mix(alb, col((34, 48, 28)), 0.55 * grime * smooth(0.3, 0.7, n2))
    else:
        alb = mix(alb, col((40, 34, 26)), 0.45 * grime)
    # the wheel's own paint, chipped to rust
    chip = smooth(0.45, 0.6, n3 * 0.6 + n2 * 0.5)
    wcol = mix(col(wheel_c), col((86, 42, 20)), chip)
    alb = np.where(wheel_m[..., None], wcol, alb)
    alb = alb * (0.72 + 0.4 * n3[..., None])
    rough = np.clip(0.45 + 0.5 * rust_f + 0.2 * grime, 0, 1)
    rough = np.where(wheel_m, 0.5 + 0.4 * chip, rough)
    face = shade(np.where(face_m, face_h, 0), alb, rough, sun=0.95)
    # the rim's lip shades the face's outer edge
    face *= (1 - 0.35 * smooth(0.9, 1.0, rf) * np.clip(-(DX + DY) / (1.4 * fr) + 0.3, 0, 1))[..., None]
    face_a = face_m.astype(np.float64)
    return dict(base=base, base_a=base_a, face=face, face_a=face_a, face_c=face_c)


def compose(parts, open_frac, lamp=None, exposure=0.78):
    base = parts['base'].copy()
    a = parts['base_a'].copy()
    sx = 1.0 - open_frac
    if sx > 0.04:
        fw = max(1, int(round(N * sx)))
        hinge = C - FR                         # the face's left edge stays put
        face = parts['face'] * (0.45 + 0.55 * sx)   # turning away from the light
        fimg = np.dstack([face, parts['face_a'] * 255])
        fimg = np.asarray(Image.fromarray(np.clip(fimg, 0, 255).astype(np.uint8), 'RGBA').resize((fw, N), Image.LANCZOS), np.float64)
        # place so that the face's left edge (x = C - FR) stays at the hinge
        left_src = C - FR
        x0 = int(round(left_src - left_src * sx))
        layer = np.zeros((N, N, 4))
        layer[:, x0:x0 + fw] = fimg[:, :max(0, min(fw, N - x0))]
        # the door's thickness shows on its free edge as it swings
        if open_frac > 0:
            th = int(round(3 * SS * math.sin(open_frac * math.pi / 2)))
            fa = layer[..., 3] > 128
            edge = np.zeros_like(fa)
            for k in range(1, th + 1):
                edge |= np.roll(fa, k, axis=1) & ~fa
            ec = np.array(parts['face_c']) * 0.45
            layer[edge] = np.concatenate([ec, [255]])
        # a soft shadow cast by the door onto the opening (light from the left)
        sh = blur((layer[..., 3] / 255.0), 3 * SS)
        sh = np.roll(sh, int(4 * SS * (1 - sx)), axis=1)
        base *= (1 - 0.5 * sh * (1 - sx))[..., None]
        la = layer[..., 3:4] / 255.0
        base = base * (1 - la) + layer[..., :3] * la
        a = np.maximum(a, la[..., 0])
    img = np.dstack([np.clip(base * exposure, 0, 255), a * 255])
    im = Image.fromarray(img.astype(np.uint8), 'RGBA')
    if lamp:  # a small caged lamp on the rim's top, glowing
        lx, ly = C, C - (R + R_OPEN) / 2
        d = np.hypot(xs + 0.5 - lx, ys + 0.5 - ly)
        lr = 2.9 * SS
        arr = np.asarray(im, np.float64).copy()
        lampc = np.array(lamp, np.float64)
        glow = np.exp(-(d / (7 * SS)) ** 2) * 0.7
        arr[..., :3] += glow[..., None] * lampc * (arr[..., 3:4] / 255)
        body = d < lr + SS
        arr[body, :3] = (22, 20, 18)
        core = d < lr
        shade_c = 0.55 + 0.6 * np.clip(1 - np.hypot(xs + 0.5 - lx + 0.6 * SS, ys + 0.5 - ly + 0.7 * SS) / lr, 0, 1)
        arr[core, :3] = (lampc * shade_c[..., None])[core]
        hl = np.hypot(xs + 0.5 - lx + 0.8 * SS, ys + 0.5 - ly + 0.9 * SS) < 0.7 * SS
        arr[hl, :3] = (255, 250, 235)
        # two cage bars
        cage = (np.abs(xs + 0.5 - lx) < 0.35 * SS) & (d < lr + SS)
        arr[cage, :3] = (30, 26, 22)
        arr[..., :3] = np.clip(arr[..., :3], 0, 255)
        arr[..., 3] = np.maximum(arr[..., 3], 0)
        im = Image.fromarray(arr.astype(np.uint8), 'RGBA')
    return im.resize((DOOR_D, DOOR_D), Image.LANCZOS)


GREEN, RED = (110, 235, 120), (245, 70, 50)


def sea_door():
    p = build('sea')
    frames = [compose(p, 0.0, GREEN), compose(p, 0.0, RED)]
    frames += [compose(p, f, RED) for f in (0.3, 0.6, 0.85)]
    frames.append(compose(p, 1.0, RED))
    return frames


def land_door():
    p = build('land')
    return [compose(p, f) for f in (0.0, 0.3, 0.6, 0.85, 1.0)]



def rock(c2):
    from background import region
    im = np.asarray(region(c2, ROCK_X0, ROCK_Y0, ROCK_X1, ROCK_Y1).convert('RGBA')).copy()
    h, w = im.shape[:2]
    ys, xs = np.mgrid[0:h, 0:w]
    wx, wy = xs + ROCK_X0 + 0.5, ys + ROCK_Y0 + 0.5
    keep = wy > np.vectorize(floor_y)(np.minimum(wx, 669)) + 1   # below the seabed's line
    for (dx, dy) in (SEA_DOOR, LAND_DOOR):
        keep &= (wx - dx) ** 2 + (wy - dy) ** 2 > (DOOR_D / 2 + 0.5) ** 2   # the whole door shows
    im[..., 3] = np.where(keep, 255, 0)
    return Image.fromarray(im, 'RGBA')


def save(path, frames):
    w, h = frames[0].size
    sheet = Image.new('RGBA', (w, h * len(frames)))
    for i, f in enumerate(frames):
        sheet.paste(f, (0, i * h))
    info = PngImagePlugin.PngInfo()
    info.add_text('openc2e-frames', str(len(frames)))
    sheet.save(path, pnginfo=info)


def main():
    c2, user = sys.argv[1], sys.argv[2]
    out = os.path.join(user, 'Images')
    os.makedirs(out, exist_ok=True)
    save(os.path.join(out, 'zwsd.png'), sea_door())
    save(os.path.join(out, 'zwld.png'), land_door())
    save(os.path.join(out, 'zwcr.png'), [Image.new('RGBA', (48, 110), (0, 0, 0, 0))])
    save(os.path.join(out, 'zwrk.png'), [rock(c2)])
    print('wrote zwsd, zwld, zwcr and zwrk to', out)


if __name__ == '__main__':
    main()
