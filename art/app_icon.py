"""Zeetures's program icon: an original design, drawn from scratch (no game
art), at 1024 x 1024 with 4x supersampling.

    python3 app_icon.py OUT_DIR [KEY]      # a design's icon files (the chosen one)
    python3 app_icon.py --install          # the chosen design (CHOSEN),
                                           # into the repository

The icon files: zeetures.png (256), zeetures.ico (16-256), zeetures.icns
(16-512) for packaging/icons, and zeetures_128.png for the window icon.
--install writes the first three into packaging/icons and the 128 x 128
window icon into src/sdlbackend/openc2e_icon.h. The other designs tried
(sprouts, trees, a helix, a constellation and more; 2026-11-20) are in the
history before 2026-11-21.
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


def radial(size, center, radius, inner, outer):
    w = h = size
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    d = np.sqrt((x - center[0]) ** 2 + (y - center[1]) ** 2) / radius
    t = np.clip(d, 0, 1)[..., None]
    inner = np.array(inner, np.float32)
    outer = np.array(outer, np.float32)
    img = inner * (1 - t) + outer * t
    return Image.fromarray(img.astype(np.uint8), 'RGBA')


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


def mind_egg(bgkind, tilt=0.35, rim=(196, 140, 176), rimw=0.035, outline=(110, 40, 90), core=((255, 246, 228), (236, 170, 190)),
             vein=(255, 252, 240, 235), depth=6, halo=(255, 200, 220), simple=False, seed=7, symmetric=False, tree=None, mirror=False, shoot=0.75, wobble=0.14, thin=0.72, weep=0.0, bend=0.0, region=None, foot=True):
    """The current icon's tilted mauve egg (openc2e's: a plum outline, a pale
    center) crossed with D's light within and branching vein. bgkind: None
    (no background, as the current icon), 'night' (D's circle), 'plum'
    (a rounded square), 'dusk' (a plum-to-night circle)."""
    Z = S * SS
    img = Image.new('RGBA', (Z, Z), (0, 0, 0, 0))
    cx, cy = Z * 0.5, Z * 0.52
    big = 0.4
    a, b = Z * big * 0.78, Z * big
    outer = egg_points(cx, cy, a, b, tilt=tilt)
    om = mask_from(outer, Z)
    # the outline, then the mauve shell, then the glowing core
    ol = egg_points(cx, cy, a + Z * 0.012, b + Z * 0.012, tilt=tilt)
    ImageDraw.Draw(img).polygon(ol, fill=outline + (255,))
    shell = Image.new('RGBA', (Z, Z), (0, 0, 0, 0))
    # (symmetric and tilted, as F8: the light, the core and the highlight
    # turn with the egg; at tilt 0 these are the upright egg's own places)
    lx = cx + math.sin(tilt) * Z * 0.1 if symmetric else cx + Z * 0.05
    ly = cy - math.cos(tilt) * Z * 0.1 if symmetric else cy - Z * 0.1
    shell.paste(radial(Z, (lx, ly), Z * 0.5, rim + (255,), tuple(max(0, v - 40) for v in rim) + (255,)), (0, 0), om)
    img = Image.alpha_composite(img, shell)
    # the core: the shell less its rim, lower-left of center as the current icon's
    icx = cx - math.sin(tilt) * Z * 0.02 if symmetric else cx - Z * 0.012
    icy = cy + math.cos(tilt) * Z * 0.02 if symmetric else icy
    inner = egg_points(icx, icy, a * (1 - rimw * 4), b * (1 - rimw * 3), tilt=tilt)
    im_ = mask_from(inner, Z).filter(ImageFilter.GaussianBlur(Z * 0.006))
    glow = radial(Z, (cx - math.sin(tilt) * Z * 0.06 if symmetric else cx - Z * 0.02, cy + math.cos(tilt) * Z * 0.06), b * 1.05, core[0] + (255,), core[1] + (255,))
    egg = Image.new('RGBA', (Z, Z), (0, 0, 0, 0))
    egg.paste(glow, (0, 0), im_)
    d = ImageDraw.Draw(egg)
    rng = random.Random(seed)

    # every branch stays wholly inside the inner egg (owner, 2026-11-20): a
    # branch whose end, with its width and a margin, would fall outside a
    # copy of the inner egg drawn smaller is shortened, or not grown
    inner_a, inner_b = a * (1 - rimw * 4), b * (1 - rimw * 3)
    keep_in = mask_from(egg_points(icx, icy, inner_a * 0.9, inner_b * 0.92, tilt=tilt), Z)
    if region is not None:
        keep_in = ImageChops.multiply(keep_in, region(Z, icx, icy, inner_a, inner_b))

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
        tree(Z, icx, icy, a * (1 - rimw * 4), b * (1 - rimw * 3), seg, crown, inside, rng, draw=d, vein=vein)
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
        over(Z, icx, icy, a * (1 - rimw * 4), b * (1 - rimw * 3), oseg)
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
        hx, hy = cx + math.sin(tilt) * b * 0.62, cy - math.cos(tilt) * b * 0.62
    hm = Image.new('L', (Z, Z), 0)
    ImageDraw.Draw(hm).ellipse((hx - Z * 0.04, hy - Z * 0.06, hx + Z * 0.03, hy + Z * 0.04), fill=55)
    hl.putalpha(hm.filter(ImageFilter.GaussianBlur(Z * 0.015)))
    img = Image.alpha_composite(img, hl)
    return img.resize((S, S), Image.LANCZOS)



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


def pulse_level(tilt):
    """The heartbeat for an egg tipped by tilt (radians): the line stays
    level, and its beat sits at the middle of the core's width at the line's
    height (owner, 2026-11-21: "reorient the heartbeat inside of it so it is
    still level")."""
    def middle(x0, y0, a, b):
        yl = y0 + b * 0.12
        pts = egg_points(x0, y0, a, b, tilt=tilt, n=1440)
        xs = []
        for (xa, ya), (xb, yb) in zip(pts, pts[1:] + pts[:1]):
            if (ya - yl) * (yb - yl) <= 0 and ya != yb:
                xs.append(xa + (xb - xa) * (yl - ya) / (yb - ya))
        return (min(xs) + max(xs)) / 2 if xs else x0

    def beat(Z, x0, y0, a, b, seg, crown, inside, rng, draw=None, vein=None, **kw):
        return tree_pulse(Z, middle(x0, y0, a, b), y0, a, b, seg, crown, inside, rng, draw=draw, vein=vein, heart=False, ends=False)

    def ends(Z, x0, y0, a, b, seg):
        return pulse_ends(Z, middle(x0, y0, a, b), y0, a, b, seg)
    beat.over_shell = ends
    return beat


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
    'F7': ('Heartbeat, no heart', lambda: mind_egg(None, tilt=0.0, rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, tree=pulse_no_heart, foot=False)),
    # F7 tipped 16 degrees, top to the right, the line still level (owner,
    # 2026-11-21, from the logo mockups: the egg "tipped as if rolling")
    'F8': ('Heartbeat, tipped', lambda: mind_egg(None, tilt=math.radians(16), rim=(204, 150, 190), core=((255, 236, 236), (230, 140, 170)), symmetric=True, tree=pulse_level(math.radians(16)), foot=False)),
}


def files(out, key):
    icon = VARIATIONS[key][1]()
    icon.resize((256, 256), Image.LANCZOS).save(os.path.join(out, 'zeetures.png'))
    icon.resize((256, 256), Image.LANCZOS).save(os.path.join(out, 'zeetures.ico'),
                                                 sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    icon.resize((512, 512), Image.LANCZOS).save(os.path.join(out, 'zeetures.icns'))
    icon.resize((128, 128), Image.LANCZOS).save(os.path.join(out, 'zeetures_128.png'))
    print('wrote zeetures.png, .ico, .icns and zeetures_128.png in', out)


# the design in use (owner, 2026-11-21): F7 tipped, its heartbeat level;
# before that (2026-11-20) F7 upright: the heartbeat egg without its heart,
# the pulse line running under the shell's inner edge
CHOSEN = 'F8'


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
    files(out, sys.argv[2] if len(sys.argv) > 2 else CHOSEN)
