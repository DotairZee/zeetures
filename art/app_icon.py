"""Zeetures's program icon: original designs, drawn from scratch (no game
art). Each concept is drawn at 1024 x 1024 with 4x supersampling and shown
at the sizes the packages use.

    python3 app_icon.py OUT_DIR            # the concept sheet
    python3 app_icon.py OUT_DIR variations # the variations sheet
    python3 app_icon.py OUT_DIR KEY        # that design's icon files
    python3 app_icon.py --install          # the chosen design (CHOSEN),
                                           # into the repository

The icon files: zeetures.png (256), zeetures.ico (16-256), zeetures.icns
(16-512) for packaging/icons, and zeetures_128.png for the window icon.
--install writes the first three into packaging/icons and the 128 x 128
window icon into src/sdlbackend/openc2e_icon.h.
"""
import math
import os
import random
import sys

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

S = 1024  # final size
SS = 4    # supersampling


def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(len(a)))


def gradient(size, top, bottom, angle=90):
    """A linear gradient, RGBA."""
    w = h = size
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    a = math.radians(angle)
    t = (x * math.cos(a) + y * math.sin(a))
    t = (t - t.min()) / (t.max() - t.min())
    top = np.array(top, np.float32)
    bottom = np.array(bottom, np.float32)
    img = top[None, None, :] * (1 - t[..., None]) + bottom[None, None, :] * t[..., None]
    return Image.fromarray(img.astype(np.uint8), 'RGBA' if len(top) == 4 else 'RGB').convert('RGBA')


def radial(size, center, radius, inner, outer):
    w = h = size
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    d = np.sqrt((x - center[0]) ** 2 + (y - center[1]) ** 2) / radius
    t = np.clip(d, 0, 1)[..., None]
    inner = np.array(inner, np.float32)
    outer = np.array(outer, np.float32)
    img = inner * (1 - t) + outer * t
    return Image.fromarray(img.astype(np.uint8), 'RGBA')


def rounded_mask(size, radius):
    m = Image.new('L', (size, size), 0)
    ImageDraw.Draw(m).rounded_rectangle((0, 0, size - 1, size - 1), radius=radius, fill=255)
    return m


def egg_points(cx, cy, a, b, k=0.16, n=360, tilt=0.0):
    """An egg: wider below. a half width, b half height."""
    pts = []
    for i in range(n):
        t = 2 * math.pi * i / n
        x = a * math.cos(t) * (1 + k * math.sin(t))
        y = b * math.sin(t)
        xr = x * math.cos(tilt) - y * math.sin(tilt)
        yr = x * math.sin(tilt) + y * math.cos(tilt)
        pts.append((cx + xr, cy + yr))
    return pts


def mask_from(pts, size):
    m = Image.new('L', (size, size), 0)
    ImageDraw.Draw(m).polygon(pts, fill=255)
    return m


def shaded_egg(size, pts, base, light, dark, light_at, rad, speckle=None, seed=1):
    """An egg filled with a soft radial shade and speckles, RGBA."""
    m = mask_from(pts, size)
    shade = radial(size, light_at, rad, light + (255,), dark + (255,))
    egg = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    egg.paste(shade, (0, 0), m)
    if speckle:
        rng = random.Random(seed)
        d = ImageDraw.Draw(egg)
        xs = [p[0] for p in pts]
        ys = [p[1] for p in pts]
        for _ in range(speckle[0]):
            x = rng.uniform(min(xs), max(xs))
            y = rng.uniform(min(ys), max(ys))
            if m.getpixel((int(x), int(y))) < 255:
                continue
            r = rng.uniform(*speckle[1]) * size / 1024
            d.ellipse((x - r, y - r * 0.8, x + r, y + r * 0.8), fill=speckle[2])
        egg.putalpha(Image.composite(egg.getchannel('A'), Image.new('L', (size, size), 0), m))
    return egg, m


def drop_shadow(mask, size, offset, blur, alpha):
    sh = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    a = mask.point(lambda v: int(v * alpha))
    sh.putalpha(a)
    sh = sh.transform(sh.size, Image.AFFINE, (1, 0, -offset[0], 0, 1, -offset[1]))
    return sh.filter(ImageFilter.GaussianBlur(blur))


def leaf(d, base, tip, width, fill, vein):
    """A leaf from base to tip, as a polygon, with a center vein."""
    bx, by = base
    tx, ty = tip
    dx, dy = tx - bx, ty - by
    L = math.hypot(dx, dy)
    nx, ny = -dy / L, dx / L
    pts = []
    for i in range(41):
        t = i / 40
        w = width * math.sin(math.pi * t) ** 0.9
        pts.append((bx + dx * t + nx * w, by + dy * t + ny * w))
    for i in range(40, -1, -1):
        t = i / 40
        w = width * math.sin(math.pi * t) ** 0.9
        pts.append((bx + dx * t - nx * w * 0.85, by + dy * t - ny * w * 0.85))
    d.polygon(pts, fill=fill)
    d.line([(bx, by), (bx + dx * 0.9, by + dy * 0.9)], fill=vein, width=max(2, int(width * 0.12)))


# ---------------------------------------------------------------- concepts

def concept_sprout():
    """A: a speckled egg, cracked open at the top, a green sprout rising, on
    a deep teal rounded square."""
    Z = S * SS
    bg = gradient(Z, (28, 96, 104, 255), (10, 42, 58, 255), 90)
    img = Image.new('RGBA', (Z, Z), (0, 0, 0, 0))
    img.paste(bg, (0, 0), rounded_mask(Z, int(Z * 0.22)))
    glow = radial(Z, (Z * 0.5, Z * 0.62), Z * 0.45, (90, 200, 190, 120), (0, 0, 0, 0))
    img = Image.alpha_composite(img, glow)
    cx, cy = Z * 0.5, Z * 0.6
    pts = egg_points(cx, cy, Z * 0.25, Z * 0.31)
    egg, m = shaded_egg(Z, pts, None, (255, 248, 232), (214, 188, 150), (cx - Z * 0.09, cy - Z * 0.12), Z * 0.36,
                        speckle=(60, (4, 9), (150, 112, 78, 255)))
    img = Image.alpha_composite(img, drop_shadow(m, Z, (0, Z * 0.02), Z * 0.02, 0.45))
    img = Image.alpha_composite(img, egg)
    d = ImageDraw.Draw(img)
    # the crack across the top: a zigzag, the shell above it gone
    top = cy - Z * 0.31
    crack_y = top + Z * 0.13
    zig = []
    xs = np.linspace(cx - Z * 0.205, cx + Z * 0.205, 9)
    for i, x in enumerate(xs):
        zig.append((x, crack_y + (Z * 0.035 if i % 2 else -Z * 0.025)))
    cut = Image.new('L', (Z, Z), 0)
    ImageDraw.Draw(cut).polygon([(cx - Z * 0.3, top - Z * 0.05)] + zig + [(cx + Z * 0.3, top - Z * 0.05)], fill=255)
    a = img.getchannel('A')
    # (only the egg is cut: the background stays)
    egg_cut = Image.composite(Image.new('L', (Z, Z), 0), Image.new('L', (Z, Z), 255), cut)
    keep = Image.composite(egg_cut, Image.new('L', (Z, Z), 255), m)
    bgonly = Image.new('RGBA', (Z, Z), (0, 0, 0, 0))
    bgonly.paste(bg, (0, 0), rounded_mask(Z, int(Z * 0.22)))
    bgonly = Image.alpha_composite(bgonly, glow)
    img = Image.composite(img, bgonly, keep)
    d = ImageDraw.Draw(img)
    # the shell's inner rim along the crack
    d.line(zig, fill=(176, 140, 100, 255), width=int(Z * 0.012))
    # the sprout: a stem and two leaves
    stem_base = (cx, crack_y + Z * 0.04)
    stem_top = (cx + Z * 0.01, cy - Z * 0.47)
    d.line([stem_base, (cx - Z * 0.012, cy - Z * 0.36), stem_top], fill=(70, 150, 60, 255), width=int(Z * 0.026), joint='curve')
    leaf(d, (cx - Z * 0.005, cy - Z * 0.42), (cx - Z * 0.2, cy - Z * 0.53), Z * 0.07, (96, 188, 72, 255), (56, 120, 44, 255))
    leaf(d, (cx + Z * 0.008, cy - Z * 0.45), (cx + Z * 0.19, cy - Z * 0.6), Z * 0.065, (130, 210, 90, 255), (70, 140, 50, 255))
    return img.resize((S, S), Image.LANCZOS)


def concept_peek():
    """B: an egg with a curious eye peering out of a crack, on a warm
    sunset square."""
    Z = S * SS
    bg = gradient(Z, (250, 168, 96, 255), (150, 70, 120, 255), 90)
    img = Image.new('RGBA', (Z, Z), (0, 0, 0, 0))
    img.paste(bg, (0, 0), rounded_mask(Z, int(Z * 0.22)))
    cx, cy = Z * 0.5, Z * 0.54
    pts = egg_points(cx, cy, Z * 0.29, Z * 0.36, tilt=-0.12)
    egg, m = shaded_egg(Z, pts, None, (255, 252, 240), (222, 196, 160), (cx - Z * 0.1, cy - Z * 0.14), Z * 0.42,
                        speckle=(50, (4, 9), (176, 120, 90, 255)), seed=3)
    img = Image.alpha_composite(img, drop_shadow(m, Z, (0, Z * 0.025), Z * 0.025, 0.4))
    img = Image.alpha_composite(img, egg)
    d = ImageDraw.Draw(img)
    # a dark opening in the shell, jagged, and an eye in it
    hx, hy = cx + Z * 0.02, cy - Z * 0.05
    hole = []
    for i in range(18):
        t = 2 * math.pi * i / 18
        r = (Z * 0.13 if i % 2 == 0 else Z * 0.1)
        hole.append((hx + r * math.cos(t) * 1.15, hy + r * math.sin(t) * 0.8))
    d.polygon(hole, fill=(40, 26, 36, 255))
    # the eye: white, iris, pupil, a highlight
    ex, ey, er = hx + Z * 0.005, hy + Z * 0.005, Z * 0.072
    d.ellipse((ex - er * 1.2, ey - er, ex + er * 1.2, ey + er), fill=(250, 250, 245, 255))
    ir = er * 0.72
    d.ellipse((ex + er * 0.15 - ir, ey - ir, ex + er * 0.15 + ir, ey + ir), fill=(60, 140, 200, 255))
    pr = ir * 0.55
    d.ellipse((ex + er * 0.2 - pr, ey - pr, ex + er * 0.2 + pr, ey + pr), fill=(16, 16, 24, 255))
    hr = pr * 0.45
    d.ellipse((ex - hr * 0.6, ey - pr * 0.9, ex + hr * 1.4, ey - pr * 0.9 + hr * 2), fill=(255, 255, 255, 255))
    # crack lines running from the hole
    for ang, ln in ((-0.9, 0.12), (2.6, 0.1), (0.6, 0.08)):
        x0, y0 = hx + math.cos(ang) * Z * 0.14, hy + math.sin(ang) * Z * 0.1
        pts2 = [(x0, y0)]
        for j in range(3):
            x0 += math.cos(ang + (0.5 if j % 2 else -0.5)) * Z * ln / 3
            y0 += math.sin(ang + (0.5 if j % 2 else -0.5)) * Z * ln / 3
            pts2.append((x0, y0))
        d.line(pts2, fill=(150, 110, 80, 255), width=int(Z * 0.008))
    return img.resize((S, S), Image.LANCZOS)


def concept_zvine():
    """C: a bold Z drawn as a growing vine with leaves, on a dark plum
    square, a small egg at its foot."""
    Z = S * SS
    bg = gradient(Z, (64, 40, 88, 255), (24, 16, 40, 255), 90)
    img = Image.new('RGBA', (Z, Z), (0, 0, 0, 0))
    img.paste(bg, (0, 0), rounded_mask(Z, int(Z * 0.22)))
    d = ImageDraw.Draw(img)
    w = int(Z * 0.095)
    gold, gold2 = (240, 196, 92, 255), (200, 150, 60, 255)
    a, b, c, e = (Z * 0.24, Z * 0.27), (Z * 0.74, Z * 0.27), (Z * 0.28, Z * 0.73), (Z * 0.78, Z * 0.73)
    shadow = Image.new('RGBA', (Z, Z), (0, 0, 0, 0))
    ImageDraw.Draw(shadow).line([a, b, c, e], fill=(0, 0, 0, 140), width=w, joint='curve')
    img = Image.alpha_composite(img, shadow.transform(shadow.size, Image.AFFINE, (1, 0, 0, 0, 1, -Z * 0.015)).filter(ImageFilter.GaussianBlur(Z * 0.012)))
    d = ImageDraw.Draw(img)
    d.line([a, b, c, e], fill=gold, width=w, joint='curve')
    d.line([(a[0], a[1] - w * 0.25), (b[0], b[1] - w * 0.25)], fill=(255, 226, 140, 255), width=int(w * 0.25))
    for (p, q) in ((a, b), (c, e)):
        d.ellipse((p[0] - w / 2, p[1] - w / 2, p[0] + w / 2, p[1] + w / 2), fill=gold)
        d.ellipse((q[0] - w / 2, q[1] - w / 2, q[0] + w / 2, q[1] + w / 2), fill=gold)
    for (base, tip, col) in (((Z * 0.42, Z * 0.27), (Z * 0.36, Z * 0.13), (110, 200, 90, 255)),
                             ((Z * 0.6, Z * 0.27), (Z * 0.7, Z * 0.14), (90, 180, 80, 255)),
                             ((Z * 0.5, Z * 0.5), (Z * 0.66, Z * 0.44), (120, 210, 96, 255)),
                             ((Z * 0.45, Z * 0.73), (Z * 0.4, Z * 0.86), (96, 190, 80, 255))):
        leaf(d, base, tip, Z * 0.045, col, (60, 120, 50, 255))
    pts = egg_points(Z * 0.8, Z * 0.53, Z * 0.07, Z * 0.088)
    egg, m = shaded_egg(Z, pts, None, (255, 250, 236), (220, 194, 156), (Z * 0.78, Z * 0.5), Z * 0.1)
    img = Image.alpha_composite(img, egg)
    return img.resize((S, S), Image.LANCZOS)


def concept_mind():
    """D: an egg glowing from within, a branching vein of light inside like a
    leaf's or a nerve's, on a night-blue circle."""
    Z = S * SS
    img = Image.new('RGBA', (Z, Z), (0, 0, 0, 0))
    bg = radial(Z, (Z * 0.5, Z * 0.4), Z * 0.7, (40, 70, 130, 255), (10, 16, 40, 255))
    cm = Image.new('L', (Z, Z), 0)
    ImageDraw.Draw(cm).ellipse((Z * 0.02, Z * 0.02, Z * 0.98, Z * 0.98), fill=255)
    img.paste(bg, (0, 0), cm)
    cx, cy = Z * 0.5, Z * 0.53
    pts = egg_points(cx, cy, Z * 0.27, Z * 0.34)
    m = mask_from(pts, Z)
    glow = radial(Z, (cx, cy + Z * 0.05), Z * 0.45, (255, 236, 170, 255), (210, 140, 70, 255))
    halo = drop_shadow(m, Z, (0, 0), Z * 0.05, 0.9)
    halo = Image.merge('RGBA', (Image.new('L', (Z, Z), 255), Image.new('L', (Z, Z), 200), Image.new('L', (Z, Z), 110), halo.getchannel('A')))
    img = Image.alpha_composite(img, halo)
    egg = Image.new('RGBA', (Z, Z), (0, 0, 0, 0))
    egg.paste(glow, (0, 0), m)
    d = ImageDraw.Draw(egg)
    rng = random.Random(7)

    def branch(x, y, ang, ln, wd, depth):
        if depth == 0 or ln < Z * 0.02:
            return
        x2, y2 = x + math.cos(ang) * ln, y + math.sin(ang) * ln
        d.line([(x, y), (x2, y2)], fill=(255, 252, 230, 230), width=max(SS, int(wd)))
        for da in (-0.5, 0.45):
            branch(x2, y2, ang + da + rng.uniform(-0.15, 0.15), ln * 0.68, wd * 0.66, depth - 1)
    branch(cx, cy + Z * 0.27, -math.pi / 2, Z * 0.17, Z * 0.022, 6)
    egg.putalpha(Image.composite(egg.getchannel('A'), Image.new('L', (Z, Z), 0), m))
    img = Image.alpha_composite(img, egg)
    hl = Image.new('RGBA', (Z, Z), (0, 0, 0, 0))
    ImageDraw.Draw(hl).ellipse((cx - Z * 0.17, cy - Z * 0.27, cx - Z * 0.05, cy - Z * 0.1), fill=(255, 255, 255, 90))
    img = Image.alpha_composite(img, hl.filter(ImageFilter.GaussianBlur(Z * 0.02)))
    return img.resize((S, S), Image.LANCZOS)



def mind_egg(bgkind, tilt=0.35, rim=(196, 140, 176), rimw=0.035, outline=(110, 40, 90), core=((255, 246, 228), (236, 170, 190)),
             vein=(255, 252, 240, 235), depth=6, halo=(255, 200, 220), simple=False, seed=7, symmetric=False, tree=None, mirror=False, shoot=0.75, wobble=0.14, thin=0.72, weep=0.0, bend=0.0, region=None, foot=True):
    """The current icon's tilted mauve egg (openc2e's: a plum outline, a pale
    center) crossed with D's light within and branching vein. bgkind: None
    (no background, as the current icon), 'night' (D's circle), 'plum'
    (a rounded square), 'dusk' (a plum-to-night circle)."""
    Z = S * SS
    img = Image.new('RGBA', (Z, Z), (0, 0, 0, 0))
    if bgkind == 'night':
        bg = radial(Z, (Z * 0.5, Z * 0.4), Z * 0.7, (40, 70, 130, 255), (10, 16, 40, 255))
        cm = Image.new('L', (Z, Z), 0)
        ImageDraw.Draw(cm).ellipse((Z * 0.02, Z * 0.02, Z * 0.98, Z * 0.98), fill=255)
        img.paste(bg, (0, 0), cm)
    elif bgkind == 'plum':
        bg = gradient(Z, (92, 48, 96, 255), (36, 18, 48, 255), 90)
        img.paste(bg, (0, 0), rounded_mask(Z, int(Z * 0.22)))
    elif bgkind == 'dusk':
        bg = gradient(Z, (120, 60, 110, 255), (16, 24, 56, 255), 90)
        cm = Image.new('L', (Z, Z), 0)
        ImageDraw.Draw(cm).ellipse((Z * 0.02, Z * 0.02, Z * 0.98, Z * 0.98), fill=255)
        img.paste(bg, (0, 0), cm)
    cx, cy = Z * 0.5, Z * 0.52
    big = 0.33 if bgkind else 0.4
    a, b = Z * big * 0.78, Z * big
    outer = egg_points(cx, cy, a, b, tilt=tilt)
    om = mask_from(outer, Z)
    if bgkind:
        h = drop_shadow(om, Z, (0, 0), Z * 0.05, 0.85)
        hc = Image.merge('RGBA', (Image.new('L', (Z, Z), halo[0]), Image.new('L', (Z, Z), halo[1]), Image.new('L', (Z, Z), halo[2]), h.getchannel('A')))
        img = Image.alpha_composite(img, hc)
    # the outline, then the mauve shell, then the glowing core
    ol = egg_points(cx, cy, a + Z * 0.012, b + Z * 0.012, tilt=tilt)
    ImageDraw.Draw(img).polygon(ol, fill=outline + (255,))
    shell = Image.new('RGBA', (Z, Z), (0, 0, 0, 0))
    lx = cx if symmetric else cx + Z * 0.05
    shell.paste(radial(Z, (lx, cy - Z * 0.1), Z * 0.5, rim + (255,), tuple(max(0, v - 40) for v in rim) + (255,)), (0, 0), om)
    img = Image.alpha_composite(img, shell)
    # the core: the shell less its rim, lower-left of center as the current icon's
    icx = cx if symmetric else cx - Z * 0.012
    inner = egg_points(icx, cy + Z * 0.02, a * (1 - rimw * 4), b * (1 - rimw * 3), tilt=tilt)
    im_ = mask_from(inner, Z).filter(ImageFilter.GaussianBlur(Z * 0.006))
    glow = radial(Z, (cx if symmetric else cx - Z * 0.02, cy + Z * 0.06), b * 1.05, core[0] + (255,), core[1] + (255,))
    egg = Image.new('RGBA', (Z, Z), (0, 0, 0, 0))
    egg.paste(glow, (0, 0), im_)
    d = ImageDraw.Draw(egg)
    rng = random.Random(seed)

    # every branch stays wholly inside the inner egg (owner, 2026-11-20): a
    # branch whose end, with its width and a margin, would fall outside a
    # copy of the inner egg drawn smaller is shortened, or not grown
    inner_a, inner_b = a * (1 - rimw * 4), b * (1 - rimw * 3)
    keep_in = mask_from(egg_points(icx, cy + Z * 0.02, inner_a * 0.9, inner_b * 0.92, tilt=tilt), Z)
    if region is not None:
        keep_in = ImageChops.multiply(keep_in, region(Z, icx, cy + Z * 0.02, inner_a, inner_b))

    def inside(x, y, r):
        for ox, oy in ((0, 0), (r, 0), (-r, 0), (0, r), (0, -r)):
            px, py = int(x + ox), int(y + oy)
            if not (0 <= px < Z and 0 <= py < Z) or keep_in.getpixel((px, py)) < 128:
                return False
        return True

    def branch(x, y, ang, ln, wd, dep):
        if dep == 0 or ln < Z * 0.02:
            return
        w = max(SS, int(wd))
        def ok(ln_):
            x2_, y2_ = x + math.cos(ang) * ln_, y + math.sin(ang) * ln_
            if not inside(x2_, y2_, w / 2 + Z * 0.004):
                return False
            # (symmetric: each half keeps to its own side of the centre line,
            # so the two halves' inner branches do not cross)
            if symmetric and abs(x - cx) > 1:
                gap = w / 2 + Z * 0.012
                if (x < cx and x2_ > cx - gap) or (x > cx and x2_ < cx + gap):
                    return False
            return True
        while ln >= Z * 0.02 and not ok(ln):
            ln *= 0.8
        if ln < Z * 0.02:
            return
        x2, y2 = x + math.cos(ang) * ln, y + math.sin(ang) * ln
        d.line([(x, y), (x2, y2)], fill=vein, width=w)
        # rounded ends, so joints and tips are soft
        for (px, py) in ((x, y), (x2, y2)):
            d.ellipse((px - w / 2, py - w / 2, px + w / 2, py + w / 2), fill=vein)
        spread = (-0.44, 0.0, 0.44) if symmetric else (-0.55, 0.5) if not simple else (-0.6, 0.6)
        for da in spread:
            if da == 0.0 and dep > (depth if symmetric else 0):
                continue  # (no straight shoot from the trunk's first fork: the trunk is that)
            branch(x2, y2, ang + da + (0 if symmetric else rng.uniform(-0.12, 0.12)), ln * (0.74 if symmetric else 0.68 if not simple else 0.62), wd * 0.66, dep - 1)
    up = -math.pi / 2 + tilt
    bx, by = cx - math.sin(tilt) * b * 0.8, cy + math.cos(tilt) * b * 0.8
    if tree is None:
        branch(bx, by, up, b * (0.5 if symmetric else 0.42), Z * (0.024 if not simple else 0.04), (depth + 1 if symmetric else depth) if not simple else 3)
    else:
        # a custom tree: its own trunk, then natural crowns (three-way forks,
        # a little chance in every angle and length), all kept inside
        def seg(x, y, x2, y2, w):
            d.line([(x, y), (x2, y2)], fill=vein, width=max(SS, int(w)))
            for (px, py) in ((x, y), (x2, y2)):
                d.ellipse((px - w / 2, py - w / 2, px + w / 2, py + w / 2), fill=vein)

        def crown(x, y, ang, ln, wd, dep):
            if dep == 0 or ln < Z * 0.016:
                return
            w = max(SS, int(wd))
            while ln >= Z * 0.016 and not inside(x + math.cos(ang) * ln, y + math.sin(ang) * ln, w / 2 + Z * 0.004):
                ln *= 0.8
            if ln < Z * 0.016:
                return
            x2, y2 = x + math.cos(ang) * ln, y + math.sin(ang) * ln
            seg(x, y, x2, y2, w)
            kids = [-0.5, 0.5] + ([rng.uniform(-0.15, 0.15)] if rng.random() < shoot else [])
            for da in kids:
                # no twig turns downward in the middle of the crown; at its
                # outer edges (beyond weep of the inner egg's half width) they
                # may, so the crown wraps down at its sides as a weeping
                # tree's (owner, 2026-11-20: liked T1's drooping edges)
                outer = weep > 0 and abs(x2 - icx) > (a * (1 - rimw * 4)) * weep
                if math.sin(ang + da) > (0.1 if not outer else 0.98):
                    continue
                # (out there a twig also bends down at each fork, by bend,
                # the way a willow's do; toward its own side)
                droop = 0.0
                if outer and bend:
                    droop = bend if x2 > icx else -bend
                    droop = droop if math.cos(ang + da) * (1 if x2 > icx else -1) > -0.2 else 0.0
                crown(x2, y2, ang + da + droop + rng.uniform(-wobble, wobble), ln * rng.uniform(0.76, 0.88) * (1.08 if outer and bend else 1.0), wd * thin, dep - 1)
        if mirror:
            # (the tree on a layer of its own: its left half is reflected onto
            # its right, so it is natural but balanced; owner, 2026-11-20)
            tree_layer = Image.new('RGBA', (Z, Z), vein[:3] + (0,))
            d = ImageDraw.Draw(tree_layer)
        tree(Z, icx, cy + Z * 0.02, a * (1 - rimw * 4), b * (1 - rimw * 3), seg, crown, inside, rng, draw=d, vein=vein)
        if mirror:
            mid = int(round(icx))
            left = tree_layer.crop((0, 0, mid, Z))
            tree_layer.paste(Image.new('RGBA', (Z - mid, Z), vein[:3] + (0,)), (mid, 0))
            tree_layer.paste(left.transpose(Image.FLIP_LEFT_RIGHT), (mid, 0))
            egg.alpha_composite(tree_layer)
            d = ImageDraw.Draw(egg)
    # (the colour solid, only the alpha blurred: blurring colour over
    # transparent black left a grey ring)
    fr = b * 0.22 if foot else 0
    fm = Image.new('L', (Z, Z), 0)
    ImageDraw.Draw(fm).ellipse((bx - fr, by - fr * 0.9, bx + fr, by + fr * 0.9), fill=200)
    foot = Image.new('RGBA', (Z, Z), core[0] + (0,))
    foot.putalpha(fm.filter(ImageFilter.GaussianBlur(Z * 0.02)))
    egg = Image.alpha_composite(egg, foot)
    egg.putalpha(Image.composite(egg.getchannel('A'), Image.new('L', (Z, Z), 0), im_))
    img = Image.alpha_composite(img, egg)
    # what runs on to the core's very edge (a tree's
    # over_shell(Z, x0, y0, a, b, seg)): drawn, then cut sharply where the
    # core meets the mauve shell, not faded by the core's soft edge
    # (owner, 2026-11-20: the pulse line stops at the shell's inner line)
    over = getattr(tree, 'over_shell', None) if tree is not None else None
    if over:
        ol_layer = Image.new('RGBA', (Z, Z), vein[:3] + (0,))
        od = ImageDraw.Draw(ol_layer)

        def oseg(x, y, x2, y2, w):
            od.line([(x, y), (x2, y2)], fill=vein, width=max(SS, int(w)))
        over(Z, icx, cy + Z * 0.02, a * (1 - rimw * 4), b * (1 - rimw * 3), oseg)
        # (the line runs a little way under the shell; then the shell's dark
        # inner edge, taken from the picture before the line, is laid back
        # on top, so the edge crosses the line, not the line the edge;
        # owner, 2026-11-20)
        imask = mask_from(inner, Z)

        def grown(m, r):
            return m.filter(ImageFilter.GaussianBlur(r)).point(lambda v: 255 if v > 8 else 0)

        def shrunk(m, r):
            return m.filter(ImageFilter.GaussianBlur(r)).point(lambda v: 255 if v > 247 else 0)
        ol_layer.putalpha(Image.composite(ol_layer.getchannel('A'), Image.new('L', (Z, Z), 0), grown(imask, Z * 0.004)))
        ring = ImageChops.subtract(grown(imask, Z * 0.008), shrunk(imask, Z * 0.003)).filter(ImageFilter.GaussianBlur(Z * 0.001))
        before = img
        img = Image.alpha_composite(img, ol_layer)
        img = Image.composite(before, img, ring)
    hl = Image.new('RGBA', (Z, Z), (255, 246, 250, 0))
    hx, hy = cx + math.sin(tilt) * b * 0.35 - a * 0.45, cy - math.cos(tilt) * b * 0.42
    if symmetric:
        hx, hy = cx, cy - b * 0.62
    hm = Image.new('L', (Z, Z), 0)
    ImageDraw.Draw(hm).ellipse((hx - Z * 0.04, hy - Z * 0.06, hx + Z * 0.03, hy + Z * 0.04), fill=55)
    hl.putalpha(hm.filter(ImageFilter.GaussianBlur(Z * 0.015)))
    img = Image.alpha_composite(img, hl)
    return img.resize((S, S), Image.LANCZOS)



def tree_balanced(Z, x0, y0, a, b, seg, crown, inside, rng, **kw):
    """T1's tree with its limbs at mirrored angles."""
    base = (x0, y0 + b * 0.8)
    top = (x0, y0 + b * 0.08)
    seg(base[0], base[1], top[0], top[1], Z * 0.032)
    for da in (-1.05, -0.55, -0.18, 0.18, 0.55, 1.05):
        crown(top[0], top[1], -math.pi / 2 + da, b * 0.23, Z * 0.02, 7)


def tree_light(Z, x0, y0, a, b, seg, crown, inside, rng, **kw):
    """T7's tree, less cluttered (owner, 2026-11-20): four main limbs and
    one fewer level of twigs."""
    base = (x0, y0 + b * 0.8)
    top = (x0 + Z * 0.004, y0 + b * 0.08)
    seg(base[0], base[1], top[0], top[1], Z * 0.032)
    for ang in (-math.pi / 2 - 0.95, -math.pi / 2 - 0.3, -math.pi / 2 + 0.3, -math.pi / 2 + 0.95):
        crown(top[0], top[1], ang, b * 0.25, Z * 0.021, 6)


def tree_limbs(angles, trunk=0.032, limb=0.021, length=0.25):
    """T9's tree with given limb angles (from straight up), trunk and limb
    widths (of the full size) and first limb length (of the inner egg's
    half height)."""
    def t(Z, x0, y0, a, b, seg, crown, inside, rng, **kw):
        base = (x0, y0 + b * 0.8)
        top = (x0 + Z * 0.004, y0 + b * 0.08)
        seg(base[0], base[1], top[0], top[1], Z * trunk)
        for da in angles:
            crown(top[0], top[1], -math.pi / 2 + da, b * length, Z * limb, 6)
    return t


def tree_plain(Z, x0, y0, a, b, seg, crown, inside, rng, **kw):
    """A full, natural tree: a trunk from the base, crowns from its top."""
    base = (x0, y0 + b * 0.8)
    top = (x0 + Z * 0.004, y0 + b * 0.08)
    seg(base[0], base[1], top[0], top[1], Z * 0.032)
    for ang in (-math.pi / 2 - 1.05, -math.pi / 2 - 0.55, -math.pi / 2 - 0.15, -math.pi / 2 + 0.2, -math.pi / 2 + 0.6, -math.pi / 2 + 1.05):
        crown(top[0], top[1], ang, b * 0.23, Z * 0.02, 7)


def tree_z(bold):
    """The trunk drawn as a Z: a root along the base (its bottom bar), the
    trunk rising on the slant, a main limb across (its top bar); the crown
    grows from the limb and its ends."""
    def t(Z, x0, y0, a, b, seg, crown, inside, rng, **kw):
        w = Z * (0.034 if bold else 0.024)
        half = a * (0.36 if bold else 0.32)
        yb, yt = y0 + b * 0.66, y0 + b * 0.06
        bl, br = (x0 - half, yb), (x0 + half, yb)
        tl, tr = (x0 - half, yt), (x0 + half, yt)
        seg(bl[0], bl[1], br[0], br[1], w)       # the root (bottom bar)
        seg(tr[0], tr[1], bl[0], bl[1], w)       # the trunk (the slant: top right to bottom left, as a Z)
        seg(tl[0], tl[1], tr[0], tr[1], w * 0.9)  # the limb (top bar)
        # little roots under the bottom bar, so it reads as a tree's foot
        for (rx, da) in ((bl[0], 0.5), (br[0], -0.5), (x0, 0.0)):
            crown(rx, yb, math.pi / 2 + da, b * 0.09, w * 0.5, 2)
        # the crown, from the limb's ends and along it
        # the canopy fills the inner egg's top half (owner, 2026-11-20):
        # outer limbs reach out toward the sides, inner ones straight up
        crown(tl[0], tl[1], -math.pi / 2 - 0.95, b * 0.2, w * 0.62, 7)
        crown(tr[0], tr[1], -math.pi / 2 + 0.95, b * 0.2, w * 0.62, 7)
        crown(tl[0], tl[1], -math.pi / 2 - 0.4, b * 0.22, w * 0.6, 7)
        crown(tr[0], tr[1], -math.pi / 2 + 0.4, b * 0.22, w * 0.6, 7)
        for f in (0.3, 0.5, 0.7):
            crown(tl[0] + (tr[0] - tl[0]) * f, yt, -math.pi / 2 + (f - 0.5) * 0.6, b * 0.24, w * 0.55, 7)
    return t


def brain_region(Z, x0, y0, a, b):
    """The mind tree's canopy: two hemispheres side by side, a fissure
    between them, over the inner egg's top half."""
    m = Image.new('L', (Z, Z), 0)
    d = ImageDraw.Draw(m)
    for side in (-1, 1):
        cx = x0 + side * a * 0.36
        d.ellipse((cx - a * 0.4, y0 - b * 0.86, cx + a * 0.4, y0 - b * 0.02), fill=255)
    d.rectangle((x0 - a * 0.025, 0, x0 + a * 0.025, Z), fill=0)
    # the trunk's way in, through the fissure's foot
    d.rectangle((x0 - a * 0.06, y0 - b * 0.2, x0 + a * 0.06, Z), fill=255)
    return m


def tree_mind(Z, x0, y0, a, b, seg, crown, inside, rng, **kw):
    """V3's tree grown into a brain's shape."""
    base = (x0, y0 + b * 0.8)
    top = (x0, y0 - b * 0.08)
    seg(base[0], base[1], top[0], top[1], Z * 0.032)
    for da in (-1.25, -0.85, -0.45, -0.15, 0.15, 0.45, 0.85, 1.25):
        crown(top[0], top[1], -math.pi / 2 + da, b * 0.14, Z * 0.018, 9)


def tree_neuron(Z, x0, y0, a, b, seg, crown, inside, rng, draw=None, vein=None, **kw):
    """A nerve cell: a glowing body, dendrites branching above, an axon down
    to its terminals at the base, beaded where its sheath would part."""
    sx, sy, sr = x0, y0 - b * 0.18, a * 0.15
    draw.ellipse((sx - sr, sy - sr * 0.92, sx + sr, sy + sr * 0.92), fill=vein)
    for da in (-1.25, -0.7, -0.25, 0.25, 0.7, 1.25):
        ang = -math.pi / 2 + da
        crown(sx + math.cos(ang) * sr * 0.8, sy + math.sin(ang) * sr * 0.8, ang, b * 0.16, Z * 0.018, 6)
    # the axon, beaded
    yb = y0 + b * 0.7
    seg(sx, sy + sr * 0.8, sx, yb, Z * 0.018)
    for k in range(1, 5):
        yy = sy + sr + (yb - sy - sr) * k / 5
        draw.ellipse((sx - Z * 0.014, yy - Z * 0.02, sx + Z * 0.014, yy + Z * 0.02), fill=vein)
    # the terminals: short twigs spreading at its foot, each with a knob
    for da in (-1.0, -0.5, 0.0, 0.5, 1.0):
        ang = math.pi / 2 + da * 0.9
        x2, y2 = sx + math.cos(ang) * b * 0.12, yb + math.sin(ang) * b * 0.1
        seg(sx, yb, x2, y2, Z * 0.01)
        draw.ellipse((x2 - Z * 0.012, y2 - Z * 0.012, x2 + Z * 0.012, y2 + Z * 0.012), fill=vein)


def tree_creature(Z, x0, y0, a, b, seg, crown, inside, rng, draw=None, vein=None, **kw):
    """An original little creature, curled asleep: its body a fat crescent
    curled round, a round head tucked in at the top with two small pointed
    ears, its tail wrapped round the front, a closed eye. Drawn on a layer
    of its own (so the crescent can be cut) and laid on the egg."""
    img = getattr(draw, '_image', None)
    layer = Image.new('RGBA', (Z, Z), vein[:3] + (0,))
    ld = ImageDraw.Draw(layer)
    col = vein
    cx, cy = x0, y0 + b * 0.1
    # the body: a big oval less a smaller one set up and to the right
    R = a * 0.56
    ld.ellipse((cx - R, cy - R * 0.86, cx + R, cy + R * 0.86), fill=col)
    ld.ellipse((cx - R * 0.42 + a * 0.12, cy - R * 0.62 - a * 0.02, cx + R * 0.42 + a * 0.12, cy + R * 0.22 - a * 0.02), fill=vein[:3] + (0,))
    # the head, tucked in where the curl closes
    hx, hy, hr = cx + a * 0.18, cy - R * 0.62, a * 0.25
    ld.ellipse((hx - hr, hy - hr * 0.9, hx + hr, hy + hr * 0.9), fill=col)
    # two small pointed ears
    for ex, lean in ((hx - hr * 0.5, -0.25), (hx + hr * 0.25, 0.15)):
        tip = (ex + lean * hr, hy - hr * 1.55)
        ld.polygon([(ex - hr * 0.32, hy - hr * 0.55), tip, (ex + hr * 0.34, hy - hr * 0.7)], fill=col)
    # the tail round the front, from the body's foot up to under the chin
    Rt = a * 0.66
    ld.arc((cx - Rt, cy - Rt * 0.8, cx + Rt, cy + Rt * 0.8), start=95, end=200, fill=col, width=int(a * 0.12))
    # face: a closed eye, a little nose, a cheek, in the egg's rose
    rose = (214, 120, 160, 255)
    ey = hy + hr * 0.02
    ld.arc((hx - hr * 0.45, ey - hr * 0.22, hx + hr * 0.05, ey + hr * 0.22), start=20, end=160, fill=rose, width=int(Z * 0.009))
    ld.ellipse((hx + hr * 0.55, hy + hr * 0.05, hx + hr * 0.75, hy + hr * 0.22), fill=rose)
    ld.ellipse((hx - hr * 0.1, ey + hr * 0.3, hx + hr * 0.3, ey + hr * 0.52), fill=(240, 170, 196, 255))
    if img is not None:
        img.alpha_composite(layer)


def tree_world(Z, x0, y0, a, b, seg, crown, inside, rng, draw=None, vein=None, **kw):
    """A world in the egg: rolling hills, a small tree on one, a moon among
    stars above."""
    # stars and a crescent moon in the top half
    for _ in range(26):
        sx = x0 + rng.uniform(-a * 0.8, a * 0.8)
        sy = y0 + rng.uniform(-b * 0.85, -b * 0.15)
        r = rng.uniform(0.004, 0.009) * Z
        draw.ellipse((sx - r, sy - r, sx + r, sy + r), fill=(255, 250, 240, 220))
    mx, my, mr = x0 + a * 0.36, y0 - b * 0.5, a * 0.17
    draw.ellipse((mx - mr, my - mr, mx + mr, my + mr), fill=(255, 250, 236, 255))
    draw.ellipse((mx - mr * 0.55, my - mr * 1.05, mx + mr * 1.35, my + mr * 0.85), fill=(245, 196, 214, 255))
    # three hills, far to near, lighter to brighter
    for (hy, amp, ph, colr) in ((y0 + b * 0.18, b * 0.12, 0.3, (236, 170, 196, 255)),
                                (y0 + b * 0.36, b * 0.14, 2.1, (246, 206, 222, 255)),
                                (y0 + b * 0.56, b * 0.12, 4.0, (255, 240, 236, 255))):
        pts = [(x0 - a * 1.2, y0 + b * 1.2)]
        for k in range(61):
            xx = x0 - a * 1.2 + a * 2.4 * k / 60
            pts.append((xx, hy - amp * math.sin(k / 60 * math.pi * 1.6 + ph)))
        pts.append((x0 + a * 1.2, y0 + b * 1.2))
        draw.polygon(pts, fill=colr)
    # a small tree on the middle hill
    tx = x0 - a * 0.3
    ty = y0 + b * 0.36 - b * 0.14 * math.sin(((tx - (x0 - a * 1.2)) / (a * 2.4)) * math.pi * 1.6 + 2.1)
    seg(tx, ty, tx, ty - b * 0.2, Z * 0.016)
    for da in (-0.8, -0.25, 0.25, 0.8):
        crown(tx, ty - b * 0.2, -math.pi / 2 + da, b * 0.08, Z * 0.011, 5)


def tree_helix(Z, x0, y0, a, b, seg, crown, inside, rng, draw=None, vein=None, **kw):
    """A double helix rising up the egg, rungs between its strands."""
    top, bot = y0 - b * 0.78, y0 + b * 0.72
    n = 120
    A = a * 0.38
    pts1, pts2 = [], []
    for k in range(n + 1):
        t = k / n
        y = bot + (top - bot) * t
        ph = t * math.pi * 3.2
        pts1.append((x0 + A * math.sin(ph), y, math.cos(ph)))
        pts2.append((x0 - A * math.sin(ph), y, -math.cos(ph)))
    for k in range(0, n + 1, 8):
        x1, yy, _ = pts1[k]
        x2, _, _ = pts2[k]
        seg(x1, yy, x2, yy, Z * 0.01)
    for pts in (pts1, pts2):
        for k in range(n):
            (xa, ya, da), (xb, yb, db) = pts[k], pts[k + 1]
            w = Z * (0.018 + 0.01 * (da + 1) / 2)
            seg(xa, ya, xb, yb, w)


def tree_sprout(Z, x0, y0, a, b, seg, crown, inside, rng, draw=None, vein=None, **kw):
    """A seedling: a seed at the base, a curved stem, two leaves."""
    sx, sy = x0, y0 + b * 0.62
    draw.ellipse((sx - a * 0.16, sy - a * 0.1, sx + a * 0.16, sy + a * 0.12), fill=vein)
    pts = []
    for k in range(31):
        t = k / 30
        pts.append((sx + a * 0.08 * math.sin(t * math.pi * 1.2), sy - b * 0.9 * t))
    for p1, p2 in zip(pts, pts[1:]):
        seg(p1[0], p1[1], p2[0], p2[1], Z * 0.022)
    tx, ty = pts[-1]
    d = draw
    leaf(d, (tx, ty + b * 0.02), (tx - a * 0.55, ty - b * 0.1), a * 0.2, vein, (240, 180, 200, 255))
    leaf(d, (tx, ty + b * 0.02), (tx + a * 0.5, ty - b * 0.3), a * 0.18, vein, (240, 180, 200, 255))
    # roots
    for da in (-0.6, 0.0, 0.6):
        ang = math.pi / 2 + da
        seg(sx, sy, sx + math.cos(ang) * a * 0.18, sy + math.sin(ang) * a * 0.18, Z * 0.008)


def tree_fern(Z, x0, y0, a, b, seg, crown, inside, rng, draw=None, vein=None, **kw):
    """A fern frond unfurling: a stem rising into a spiral, with leaflets."""
    pts = []
    # the stem, from the base up
    for k in range(40):
        t = k / 39
        pts.append((x0 - a * 0.1 * math.sin(t * math.pi), y0 + b * 0.75 - b * 0.9 * t))
    # the spiral: radius shrinking, turning in
    cx, cy = pts[-1][0] + a * 0.26, pts[-1][1]
    r0 = a * 0.26
    for k in range(1, 90):
        t = k / 89
        ang = math.pi + t * math.pi * 2.6
        r = r0 * (1 - 0.82 * t)
        pts.append((cx + r * math.cos(ang), cy + r * math.sin(ang)))
    for i, (p1, p2) in enumerate(zip(pts, pts[1:])):
        w = Z * max(0.006, 0.024 * (1 - i / len(pts)) ** 0.7)
        seg(p1[0], p1[1], p2[0], p2[1], w)
    # leaflets along the stem, both sides, shrinking upward
    for k in range(4, 36, 4):
        x, y = pts[k]
        L = a * 0.2 * (1 - k / 44)
        for side in (-1, 1):
            leaf(draw, (x, y), (x + side * L, y - L * 0.5), L * 0.28, vein, (240, 180, 200, 255))


def tree_crack(Z, x0, y0, a, b, seg, crown, inside, rng, draw=None, vein=None, **kw):
    """Hatching: a zigzag crack across the egg, light pouring out in rays."""
    img = getattr(draw, '_image', None)
    yc = y0 - b * 0.1
    xs = [x0 - a * 0.95 + a * 1.9 * k / 8 for k in range(9)]
    zig = [(x, yc + (b * 0.07 if k % 2 else -b * 0.05)) for k, x in enumerate(xs)]
    if img is not None:
        rays = Image.new('RGBA', (Z, Z), vein[:3] + (0,))
        rm = Image.new('L', (Z, Z), 0)
        rd = ImageDraw.Draw(rm)
        cx, cy = x0, yc
        for k in range(14):
            ang = -math.pi + math.pi * (k + 0.5) / 14
            w = 0.07
            rd.polygon([(cx, cy), (cx + math.cos(ang - w) * Z, cy + math.sin(ang - w) * Z), (cx + math.cos(ang + w) * Z, cy + math.sin(ang + w) * Z)], fill=150)
        rays.putalpha(rm.filter(ImageFilter.GaussianBlur(Z * 0.01)))
        img.alpha_composite(rays)
    for p1, p2 in zip(zig, zig[1:]):
        seg(p1[0], p1[1], p2[0], p2[1], Z * 0.03)


def tree_starz(Z, x0, y0, a, b, seg, crown, inside, rng, draw=None, vein=None, **kw):
    """A constellation: stars joined by faint lines into a Z, small stars
    scattered round it."""
    for _ in range(40):
        sx = x0 + rng.uniform(-a * 0.85, a * 0.85)
        sy = y0 + rng.uniform(-b * 0.85, b * 0.8)
        r = rng.uniform(0.003, 0.007) * Z
        draw.ellipse((sx - r, sy - r, sx + r, sy + r), fill=(255, 250, 240, 200))
    zs = [(x0 - a * 0.42, y0 - b * 0.42), (x0 - a * 0.02, y0 - b * 0.46), (x0 + a * 0.42, y0 - b * 0.4),
          (x0 + a * 0.08, y0 - b * 0.02), (x0 - a * 0.4, y0 + b * 0.38), (x0 + a * 0.02, y0 + b * 0.42), (x0 + a * 0.44, y0 + b * 0.36)]
    faint = vein[:3] + (150,)
    for p1, p2 in zip(zs, zs[1:]):
        draw.line([p1, p2], fill=(255, 232, 236, 255), width=int(Z * 0.008))
    for (x, y) in zs:
        # (a soft halo in the egg's light pink, then the star: drawn solid,
        # as a halo with partial alpha replaced the glow under it)
        for r, colr in ((Z * 0.03, (255, 226, 232, 255)), (Z * 0.016, vein[:3] + (255,))):
            draw.ellipse((x - r, y - r, x + r, y + r), fill=colr)


def pulse_ends(Z, x0, y0, a, b, seg):
    """The pulse line's flat ends, out to the shell's inner edge."""
    yl = y0 + b * 0.12
    seg(x0 - a * 2, yl, x0 - a * 0.45, yl, Z * 0.024)
    seg(x0 + a * 0.34, yl, x0 + a * 2, yl, Z * 0.024)


def pulse_no_heart(*args, **kw):
    # (just the beat: the flat ends are pulse_ends', drawn under the shell's
    # inner edge, as the core's soft edge let its own ends show over it)
    return tree_pulse(*args, heart=False, ends=False, **kw)


pulse_no_heart.over_shell = pulse_ends


def tree_pulse(Z, x0, y0, a, b, seg, crown, inside, rng, draw=None, vein=None, heart=True, ends=True, **kw):
    """A heartbeat: a pulse line across the egg, a small heart above it."""
    yl = y0 + b * 0.12
    pts = [(x0 - a, yl), (x0 - a * 0.45, yl), (x0 - a * 0.3, yl - b * 0.08), (x0 - a * 0.18, yl + b * 0.06),
           (x0 - a * 0.05, yl - b * 0.42), (x0 + a * 0.1, yl + b * 0.3), (x0 + a * 0.24, yl - b * 0.06), (x0 + a * 0.34, yl), (x0 + a, yl)]
    if not ends:
        pts = pts[1:-1]
    for p1, p2 in zip(pts, pts[1:]):
        seg(p1[0], p1[1], p2[0], p2[1], Z * 0.024)
    if not heart:
        return
    hx, hy, hs = x0, y0 - b * 0.5, a * 0.2
    pts = []
    for k in range(80):
        t = 2 * math.pi * k / 80
        x = 16 * math.sin(t) ** 3
        y = -(13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t))
        pts.append((hx + x * hs / 16, hy + y * hs / 16))
    draw.polygon(pts, fill=vein)

VARIATIONS = {
    'D1': ('No background', lambda: mind_egg(None)),
    'D2': ('On night blue', lambda: mind_egg('night')),
    'D3': ('On plum', lambda: mind_egg('plum', core=((255, 244, 214), (240, 176, 120)), halo=(255, 210, 150))),
    'D4': ('Golden, dusk', lambda: mind_egg('dusk', rim=(214, 150, 186), core=((255, 238, 180), (226, 150, 80)), halo=(255, 200, 140))),
    'D5': ('Bold, simple', lambda: mind_egg('night', rimw=0.05, simple=True, core=((255, 240, 200), (238, 160, 150)))),
    'D6': ('Upright, rose', lambda: mind_egg('night', tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), halo=(255, 170, 200))),
    'D7': ('Symmetrical, no background', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), halo=(255, 170, 200), symmetric=True)),
    'Z1': ('Z trunk, bold', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, tree=tree_z(True), seed=11)),
    'Z2': ('Z trunk, subtle', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, tree=tree_z(False), seed=5)),
    'T1': ('Full tree, no Z', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, tree=tree_plain, seed=3)),
    'T2': ('Full tree, mirrored', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, tree=tree_plain, seed=3, mirror=True)),
    'T3': ('Balanced, not mirrored', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, tree=tree_balanced, seed=3, wobble=0.08)),
    'T4': ('Mirrored, airy', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, tree=tree_plain, seed=3, mirror=True, shoot=0.35, thin=0.64)),
    'T5': ('Balanced, weeping edges', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, tree=tree_balanced, seed=3, wobble=0.08, weep=0.5)),
    'T6': ('Mirrored, weeping edges', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, tree=tree_plain, seed=3, mirror=True, shoot=0.5, thin=0.68, weep=0.5)),
    'T7': ('More weeping', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, tree=tree_plain, seed=3, mirror=True, shoot=0.5, thin=0.68, weep=0.35, bend=0.3)),
    'T8': ('Willow', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, tree=tree_plain, seed=3, mirror=True, shoot=0.5, thin=0.68, weep=0.25, bend=0.45)),
    'T9': ('More weeping, lighter', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, tree=tree_light, seed=3, mirror=True, shoot=0.25, thin=0.68, weep=0.35, bend=0.3)),
    'V1': ('Seed 11', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, mirror=True, tree=tree_limbs((-0.95, -0.3, 0.3, 0.95)), seed=11, shoot=0.25, thin=0.68, weep=0.35, bend=0.3)),
    'V2': ('Seed 23', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, mirror=True, tree=tree_limbs((-0.95, -0.3, 0.3, 0.95)), seed=23, shoot=0.25, thin=0.68, weep=0.35, bend=0.3)),
    'V3': ('Seed 42', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, mirror=True, tree=tree_limbs((-0.95, -0.3, 0.3, 0.95)), seed=42, shoot=0.25, thin=0.68, weep=0.35, bend=0.3)),
    'V4': ('Three limbs', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, mirror=True, tree=tree_limbs((-0.85, 0.0, 0.85)), seed=3, shoot=0.25, thin=0.68, weep=0.35, bend=0.3)),
    'V5': ('Longer weeping', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, mirror=True, tree=tree_limbs((-0.95, -0.3, 0.3, 0.95)), seed=3, shoot=0.25, thin=0.68, weep=0.3, bend=0.42)),
    'V6': ('Bolder limbs', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, mirror=True, tree=tree_limbs((-0.95, -0.3, 0.3, 0.95), trunk=0.044, limb=0.03), seed=3, shoot=0.25, thin=0.66, weep=0.35, bend=0.3)),
    'E1': ('Mind tree', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, mirror=True, tree=tree_mind, region=brain_region, seed=42, shoot=0.9, thin=0.74)),
    'E2': ('Neuron', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, mirror=True, tree=tree_neuron, seed=42, shoot=0.3, thin=0.68, foot=False)),
    'E3': ('Curled creature', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, tree=tree_creature, seed=1, foot=False)),
    'E4': ('A world in an egg', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, tree=tree_world, seed=9, shoot=0.3, thin=0.7, foot=False)),
    'F1': ('DNA helix', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, tree=tree_helix, foot=False)),
    'F2': ('Sprout', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, tree=tree_sprout, foot=False)),
    'F3': ('Fiddlehead', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, tree=tree_fern, foot=False)),
    'F4': ('Cracking light', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, tree=tree_crack, foot=False)),
    'F5': ('Constellation Z', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, tree=tree_starz, seed=4, foot=False)),
    'F6': ('Heartbeat', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, tree=tree_pulse, foot=False)),
    'G0': ('Empty', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, tree=lambda *a, **k: None, foot=False)),
    'F7': ('Heartbeat, no heart', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, tree=pulse_no_heart, foot=False)),
}


def variation_sheet(out):
    f = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 19)
    small = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 14)
    n = len(VARIATIONS)
    per_row = 3
    rows = (n + per_row - 1) // per_row
    W, Hrow = per_row * 300 + 40, 470
    im = Image.new('RGB', (W, rows * Hrow), (236, 236, 240))
    d = ImageDraw.Draw(im)
    for i, (key, (name, fn)) in enumerate(VARIATIONS.items()):
        icon = fn()
        icon.save(os.path.join(out, f'icon-{key}.png'))
        x, y = 30 + (i % per_row) * 300, (i // per_row) * Hrow
        d.text((x, y + 10), f'{key}  {name}', font=f, fill=(30, 30, 40))
        big = icon.resize((256, 256), Image.LANCZOS)
        im.paste(big, (x, y + 40), big)
        d.rectangle((x - 10, y + 304, x + 270, y + 440), fill=(30, 32, 38))
        for j, sz in enumerate((64, 32, 16)):
            sm = icon.resize((sz, sz), Image.LANCZOS)
            im.paste(sm, (x + j * 80, y + 306 - 0), sm) if False else None
            im.paste(sm, (x + j * 80, y + 312), sm)
            im.paste(sm, (x + j * 80, y + 380), sm)
    im.save(os.path.join(out, 'app-icon-variations.png'))
    print('wrote', os.path.join(out, 'app-icon-variations.png'))

CONCEPTS = {'A': ('Sprouting egg', concept_sprout), 'B': ('Peeking egg', concept_peek),
            'C': ('Z vine', concept_zvine), 'D': ('Glowing mind egg', concept_mind)}


def sheet(out):
    MONO = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
    f = ImageFont.truetype(MONO, 22)
    small = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 15)
    cols = len(CONCEPTS)
    W, H = cols * 300 + 40, 610
    im = Image.new('RGB', (W, H), (236, 236, 240))
    d = ImageDraw.Draw(im)
    d.rectangle((0, 400, W, H), fill=(30, 32, 38))
    for i, (key, (name, fn)) in enumerate(CONCEPTS.items()):
        icon = fn()
        icon.save(os.path.join(out, f'icon-{key}.png'))
        x = 30 + i * 300
        d.text((x, 12), f'{key}  {name}', font=f, fill=(30, 30, 40))
        im.paste(icon.resize((256, 256), Image.LANCZOS), (x, 48), icon.resize((256, 256), Image.LANCZOS))
        for j, sz in enumerate((64, 32, 16)):
            s = icon.resize((sz, sz), Image.LANCZOS)
            im.paste(s, (x + j * 80, 320), s)
            im.paste(s, (x + j * 80, 420), s)
        d.text((x, 500), '64, 32 and 16 px, on light and dark', font=small, fill=(200, 200, 210))
    im.save(os.path.join(out, 'app-icon-concepts.png'))
    print('wrote', os.path.join(out, 'app-icon-concepts.png'))


def files(out, key):
    icon = (CONCEPTS.get(key) or VARIATIONS[key])[1]()
    icon.resize((256, 256), Image.LANCZOS).save(os.path.join(out, 'zeetures.png'))
    icon.resize((256, 256), Image.LANCZOS).save(os.path.join(out, 'zeetures.ico'),
                                                 sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    icon.resize((512, 512), Image.LANCZOS).save(os.path.join(out, 'zeetures.icns'))
    icon.resize((128, 128), Image.LANCZOS).save(os.path.join(out, 'zeetures_128.png'))
    print('wrote zeetures.png, .ico, .icns and zeetures_128.png in', out)


# the design in use (owner, 2026-11-20): the heartbeat egg without its heart,
# the pulse line running under the shell's inner edge
CHOSEN = 'F7'


def window_icon_header(icon, path):
    """The 128 x 128 window icon as the C array SDLBackend.cpp loads."""
    im = icon.resize((128, 128), Image.LANCZOS).convert('RGBA')
    data = im.tobytes()
    rows = []
    for i in range(0, len(data), 32):
        rows.append('\t' + ', '.join(str(v) for v in data[i:i + 32]) + ',')
    with open(path, 'w') as f:
        f.write('#pragma once\n\n'
                '// The window icon: Zeetures\'s egg with a heartbeat, drawn by\n'
                '// art/app_icon.py (python3 art/app_icon.py --install, which also\n'
                '// writes packaging/icons). 128 x 128, RGBA, 8 bits per channel,\n'
                '// rows top to bottom.\n\n'
                'static const int openc2e_icon_size = 128;\n'
                'static const unsigned char openc2e_icon_rgba[128 * 128 * 4] = {\n')
        f.write('\n'.join(rows) + '\n};\n')


def install(root):
    icon = VARIATIONS[CHOSEN][1]()
    icons = os.path.join(root, 'packaging', 'icons')
    icon.resize((256, 256), Image.LANCZOS).save(os.path.join(icons, 'zeetures.png'))
    icon.resize((256, 256), Image.LANCZOS).save(os.path.join(icons, 'zeetures.ico'),
                                                 sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    icon.resize((512, 512), Image.LANCZOS).save(os.path.join(icons, 'zeetures.icns'))
    window_icon_header(icon, os.path.join(root, 'src', 'sdlbackend', 'openc2e_icon.h'))
    print('installed', CHOSEN, 'into packaging/icons and src/sdlbackend/openc2e_icon.h')


if __name__ == '__main__':
    if sys.argv[1] == '--install':
        install(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        sys.exit(0)
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    if len(sys.argv) > 2 and sys.argv[2] == 'variations':
        variation_sheet(out)
    elif len(sys.argv) > 2:
        files(out, sys.argv[2])
    else:
        sheet(out)
