"""Generates the Hatchery nursery's art: a draped hessian wall, a rough-cut
shelf board and woven twig nests. All procedural, drawn from scratch for
openc2e (no game data). The engine embeds the PNGs this writes (CMake,
scripts/embed_files.py); rerun this after changing it:

    python3 data/hatchery/make_art.py data/hatchery

Needs numpy and Pillow. The output is deterministic (fixed seeds)."""
import math, os, random, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

def vnoise(w, h, cell, seed):
    """smooth value noise in 0..1, features about `cell` px across"""
    rng = np.random.default_rng(seed)
    gw, gh = max(2, int(w / cell) + 2), max(2, int(h / cell) + 2)
    g = (rng.random((gh, gw)) * 255).astype(np.uint8)
    im = Image.fromarray(g).resize((int(gw * cell), int(gh * cell)), Image.BICUBIC)
    return np.asarray(im).astype(np.float32)[:h, :w] / 255.0


def fbm(w, h, cell, seed, octaves=4):
    out = np.zeros((h, w), np.float32)
    amp, tot = 1.0, 0.0
    for o in range(octaves):
        out += amp * vnoise(w, h, max(1.5, cell / (2 ** o)), seed + o * 17)
        tot += amp
        amp *= 0.5
    return out / tot


def stretched(w, h, cx, cy, seed):
    """noise stretched cx times along x, cy along y (fibres, bark)"""
    sw, sh = max(4, int(w / cx)), max(4, int(h / cy))
    n = fbm(sw, sh, 3, seed, 3)
    return np.asarray(Image.fromarray((n * 255).astype(np.uint8)).resize((w, h), Image.BICUBIC)).astype(np.float32) / 255


def _light(h, k, L=(-0.5, -0.65, 0.58)):
    """Lambert light from a height map (light from the upper left)"""
    gx = np.zeros_like(h); gy = np.zeros_like(h)
    gx[:, 1:-1] = (h[:, 2:] - h[:, :-2]) * k
    gy[1:-1, :] = (h[2:, :] - h[:-2, :]) * k
    nz = 1.0 / np.sqrt(gx * gx + gy * gy + 1)
    L = np.array(L, np.float32); L = L / np.linalg.norm(L)
    return np.clip(-gx * nz * L[0] - gy * nz * L[1] + nz * L[2], 0, 1)


def hessian_wall(W, H, seed=0):
    """draped hessian: a coarse, thickly threaded open weave with slubs,
    dark gaps between the threads and loose fibres, hanging in soft folds"""
    rnd = random.Random(seed)
    ys, xs = np.mgrid[0:H, 0:W].astype(np.float32)
    # folds: broad vertical swells, each its own width, bending a little
    xw = xs + (fbm(W, H, 240, seed, 2) - 0.5) * 40
    folds = np.zeros((H, W), np.float32)
    x = -40.0
    while x < W + 40:
        fw = rnd.uniform(55, 130)
        amp = rnd.uniform(0.6, 1.2)
        folds += amp * np.exp(-((xw - x - fw / 2) / (fw * 0.42)) ** 2)
        x += fw * rnd.uniform(0.7, 1.0)
    # thick threads, spaced a little unevenly
    tx = 6.4
    jx = xw + (fbm(W, H, 22, seed + 11, 3) - 0.5) * 7.0 + (stretched(W, H, 1, 40, seed + 13) - 0.5) * 4.0
    jy = ys + (fbm(W, H, 22, seed + 12, 3) - 0.5) * 7.0 + (stretched(W, H, 40, 1, seed + 14) - 0.5) * 4.0
    fx = (jx / tx) % 1.0
    fy = (jy / tx) % 1.0
    # each thread a rounded ridge, its thickness varying along it (slubs)
    slub_w = stretched(W, H, 1, 14, seed + 3)
    slub_f = stretched(W, H, 14, 1, seed + 4)
    th_w = 0.20 + 0.36 * slub_w ** 1.5  # half-width of a warp thread, in cells
    th_f = 0.20 + 0.36 * slub_f ** 1.5
    warp = np.clip(1 - np.abs(fx - 0.5) / th_w, 0, 1) ** 0.6
    weft = np.clip(1 - np.abs(fy - 0.5) / th_f, 0, 1) ** 0.6
    over = (np.floor(jx / tx) + np.floor(jy / tx)) % 2  # which thread lies on top
    # where a thread passes under, it dips; the gaps are open and dark
    top = np.where(over > 0, warp * 1.0 + weft * 0.55, weft * 1.0 + warp * 0.55)
    thread = np.maximum(warp, weft)
    gap = np.clip(1 - thread * 2.2, 0, 1)
    # the fibres twisted round each thread: fine diagonal striation
    twist = 0.5 + 0.5 * np.sin((jx + jy * np.where(over > 0, 1.0, -1.0)) * 2 * math.pi / 2.1)
    height = folds * 11.0 + top * 1.6 + twist * thread * 0.25 + (fbm(W, H, 20, seed + 5, 2) - 0.5) * 0.5
    lam = _light(height, 1.1)
    tone = fbm(W, H, 120, seed + 6, 3)
    thread_tone = stretched(W, H, 3, 3, seed + 7)  # threads differ in colour
    col = np.array([158, 128, 86], np.float32)[None, None, :] * (0.78 + 0.28 * tone + 0.16 * (thread_tone - 0.5))[..., None]
    rgb = col * (0.22 + 1.05 * lam)[..., None] * (0.6 + 0.5 * thread)[..., None]
    rgb *= (1 - 0.9 * gap)[..., None]  # dark holes in the open weave
    img = Image.fromarray(np.dstack([np.clip(rgb, 0, 255), np.full((H, W), 255, np.float32)]).astype(np.uint8), 'RGBA')
    # loose hairy fibres catching the light
    hair = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    hd = ImageDraw.Draw(hair)
    for _ in range(int(W * H / 260)):
        x0, y0 = rnd.uniform(0, W), rnd.uniform(0, H)
        a = rnd.uniform(0, 2 * math.pi)
        L = rnd.uniform(3, 9)
        pts = [(x0, y0), (x0 + math.cos(a) * L * 0.5, y0 + math.sin(a) * L * 0.5 + rnd.uniform(-1, 1)), (x0 + math.cos(a) * L, y0 + math.sin(a) * L)]
        hd.line(pts, fill=(214, 186, 136, rnd.randint(70, 140)), width=1)
    img.alpha_composite(hair)
    a = np.asarray(img).astype(np.float32)
    vign = 1 - 0.5 * np.clip(((xs / W - 0.45) ** 2 * 1.4 + (ys / H - 0.38) ** 2 * 1.3), 0, 1)
    a[..., :3] *= (vign * (0.95 - 0.25 * ys / H))[..., None] * 0.8
    # muted (owner, 2026-11-13): less saturated, a little less contrast
    rgb = a[..., :3]
    lum = (rgb * np.array([0.299, 0.587, 0.114], np.float32)).sum(axis=2, keepdims=True)
    rgb = lum + (rgb - lum) * 0.68
    m = rgb.mean(axis=(0, 1), keepdims=True)
    a[..., :3] = m + (rgb - m) * 0.8
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8), 'RGBA')


def board3(width, depth=26, thick=22, seed=0):
    """a rough-cut board: sawmill arcs across the grain, torn fuzzy fibres,
    a square edge that wanders a little, and end checks; no polish"""
    rnd = random.Random(seed)

    def face(w, h, sd, period, saw_r):
        ys, xs = np.mgrid[0:h, 0:w].astype(np.float32)
        warp = (fbm(w, h, 260, sd, 3) - 0.5) * 26 + (fbm(w, h, 40, sd + 5, 2) - 0.5) * 3
        spacing = 0.7 + 0.6 * fbm(w, h, 120, sd + 8, 2)
        rings = 0.5 + 0.5 * np.sin((ys + warp) * spacing * 2 * math.pi / period)
        late = np.clip((rings - 0.62) * 3.0, 0, 1)
        fib = stretched(w, h, 50, 0.7, sd + 9)
        # sawmill marks: shallow arcs across the grain, from a big blade whose
        # centre creeps along as the board is fed, each pass a little different
        cx = xs + (fbm(w, h, 400, sd + 21, 2) - 0.5) * 30
        dist = np.sqrt((cx % 220 - 110 + saw_r) ** 2 + (ys + saw_r * 0.15) ** 2)
        depth_var = 0.5 + 0.5 * fbm(w, h, 90, sd + 22, 2)
        phase = dist / 6.0 + (fbm(w, h, 60, sd + 25, 2) - 0.5) * 2.5
        saw = ((0.5 + 0.5 * np.sin(phase * 2 * math.pi)) ** 4) * depth_var  # thin ridges, unevenly spaced
        # torn fibres: short, bright, raised flecks along the grain
        fuzz = np.clip((stretched(w, h, 4, 0.6, sd + 23) - 0.62) * 4, 0, 1)
        fuzz *= np.random.default_rng(sd + 24).random((h, w)) > 0.35
        height = 0.55 * (1 - late) + 0.30 * fib + 0.22 * saw + 0.30 * fuzz
        tone = fbm(w, h, 140, sd + 3, 3)
        return height, late, tone, fuzz
    H = depth + thick
    out = np.zeros((H, width, 4), np.float32)
    light = np.array([96, 70, 48], np.float32)
    dark = np.array([50, 34, 24], np.float32)
    # the top face
    ht, lt, tt, ft = face(width, depth, seed, 6.0, 260)
    mix = 0.14 * lt + 0.22 * (1 - tt) + 0.18
    col = light * (1 - mix)[..., None] + dark * mix[..., None]
    top = col * (0.72 + 0.44 * _light(ht, 1.6))[..., None]
    top += (ft * 14)[..., None]  # the fuzz catches the lamp
    top *= np.linspace(0.62, 1.10, depth)[:, None, None]  # crease at the back
    top[:3] *= np.array([0.55, 0.7, 0.85])[:, None, None]
    out[:depth, :, :3] = top
    # the front face: square-cut, sawn the same way, in shade
    hf, lf, tf, ff = face(width, thick, seed + 50, 4.5, 180)
    m2 = 0.16 * lf + 0.24
    colf = light * (1 - m2)[..., None] + dark * m2[..., None]
    shade = np.linspace(0.60, 0.40, thick)[:, None] * (0.80 + 0.36 * _light(hf, 1.5))
    out[depth:, :, :3] = colf * shade[..., None] + (ff * 8)[..., None]
    out[depth - 1, :, :3] *= 1.25  # the arris, unplaned but catching a little light
    out[depth, :, :3] *= 0.75
    out[-1, :, :3] *= 0.6
    out[..., :3] *= np.linspace(1.06, 0.90, width)[None, :, None]
    out[..., 3] = 255
    # the edges are not quite straight: nibble the top-front arris and the underside
    nib_t = (fbm(width, 1, 9, seed + 30, 2)[0] - 0.5) * 3.0
    nib_b = (fbm(width, 1, 7, seed + 31, 2)[0] - 0.5) * 3.0 + 0.6
    for x in range(width):
        k = int(round(max(0, nib_t[x])))
        if k:  # a torn notch at the front lip: shows the darker face below
            out[depth - k:depth, x, :3] = out[depth:depth + k, x, :3] * 0.8
        kb = int(round(max(0, nib_b[x])))
        if kb:
            out[H - kb:, x, 3] = 0
    # end checks: splits opening from each end along a grain line, tapering
    for xe, dirn in ((0, 1), (width - 1, -1)):
        for _ in range(rnd.randint(1, 2)):
            y = rnd.uniform(5, depth - 5); L = rnd.uniform(25, 60)
            for j in range(int(L)):
                x = xe + dirn * j
                if not 0 <= x < width:
                    break
                w_ = (1 - j / L) * 1.3
                yy = int(round(y + (fbm(1, 1, 1, seed + j, 1)[0, 0] - 0.5) * 0.0 + math.sin(j / 9) * 0.7))
                out[yy, x, :3] *= 0.25 + 0.5 * (1 - w_ / 1.3)
                if w_ > 0.8 and yy + 1 < depth:
                    out[yy + 1, x, :3] *= 0.6
                if yy - 1 >= 0:
                    out[yy - 1, x, :3] = np.minimum(out[yy - 1, x, :3] * 1.12, 255)
    return Image.fromarray(np.clip(out, 0, 255).astype(np.uint8), 'RGBA')


SS = 3  # supersampling
TWIGS = [(96, 66, 36), (120, 86, 48), (146, 108, 62), (78, 54, 30), (166, 130, 80), (108, 82, 54), (134, 116, 86), (88, 72, 50)]


def _bowl_mask(W, H, cx, cy, rx, ry, depth):
    """the nest's silhouette: the rim ellipse joined to a rounded underside"""
    m = Image.new('L', (W, H), 0)
    d = ImageDraw.Draw(m)
    d.ellipse((cx - rx, cy - ry, cx + rx, cy + ry), fill=255)
    d.ellipse((cx - rx, cy - depth, cx + rx, cy + depth), fill=255)  # lower half shows below the rim
    d.rectangle((0, 0, W, cy), fill=0)
    d.ellipse((cx - rx, cy - ry, cx + rx, cy + ry), fill=255)
    return m


def _twig(d, x, y, ang, L, w, col, bend, rnd):
    pts = []
    n = 4
    for i in range(n + 1):
        t = (i / n - 0.5) * L
        b = bend * ((i / n - 0.5) ** 2 - 0.25)
        pts.append((x + math.cos(ang) * t - math.sin(ang) * b, y + math.sin(ang) * t + math.cos(ang) * b))
    d.line(pts, fill=col, width=w, joint='curve')
    if w >= 2 * SS:  # a lit top edge
        d.line([(px, py - w * 0.35) for px, py in pts], fill=tuple(min(255, int(c * 1.35)) for c in col[:3]) + (255,), width=max(1, w // 3))


def _shade_layer(layer, cx, cy, rx, depth, ry):
    """light from the upper left: brighter at the rim and on the left,
    falling off down the bowl's underside"""
    a = np.asarray(layer).astype(np.float32)
    H, W = a.shape[:2]
    ys, xs = np.mgrid[0:H, 0:W]
    down = np.clip((ys - (cy - ry)) / (depth + ry), 0, 1)  # 0 at the rim's top, 1 at the bottom
    side = np.clip((xs - (cx - rx)) / (2 * rx), 0, 1)  # 0 left, 1 right
    k = 1.18 - 0.62 * down ** 1.3 - 0.18 * side
    a[..., :3] *= k[..., None]
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8), 'RGBA')


def nest(width, seed=0):
    """A woven twig nest, width px wide: (back, front, rim_y). The egg goes
    between the two layers, its base a little under rim_y."""
    rnd = random.Random(seed)
    W = width * SS
    H = int(width * 0.75) * SS
    cx = W / 2
    rx = W * 0.45
    ry = rx * 0.30  # the rim seen from a little above
    cy = H * 0.36
    depth = rx * 0.62  # how far the rounded underside reaches below the rim
    mask = _bowl_mask(W, H, cx, cy, rx, ry, depth)
    back = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    front = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    db, df = ImageDraw.Draw(back), ImageDraw.Draw(front)
    # the far inside wall and the hollow, in shadow
    db.ellipse((cx - rx * 0.96, cy - ry * 0.96, cx + rx * 0.96, cy + ry * 0.96), fill=(52, 36, 20, 255))
    hollow = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(hollow).ellipse((cx - rx * 0.74, cy - ry * 0.62, cx + rx * 0.74, cy + ry * 0.70), fill=(22, 14, 8, 255))
    back.alpha_composite(hollow.filter(ImageFilter.GaussianBlur(SS * 3)))
    # the far inner wall's twigs (seen inside the bowl, behind the egg)
    for _ in range(90):
        a = rnd.uniform(math.pi * 1.02, math.pi * 1.98)
        r = rnd.uniform(0.72, 0.97)
        x, y = cx + math.cos(a) * rx * r, cy + math.sin(a) * ry * r
        ang = a + math.pi / 2 + rnd.uniform(-0.6, 0.6)
        col = rnd.choice(TWIGS)
        _twig(db, x, y, ang, rnd.uniform(6, 14) * SS, rnd.choice([1, 1, 2]) * SS, tuple(int(c * 0.72) for c in col) + (255,), rnd.uniform(-3, 3) * SS, rnd)
    # soft grass and down lining in the hollow
    for _ in range(40):
        a = rnd.uniform(0, 2 * math.pi)
        r = rnd.uniform(0.1, 0.6)
        x, y = cx + math.cos(a) * rx * r, cy + math.sin(a) * ry * r * 0.8
        col = rnd.choice([(150, 132, 96), (178, 162, 124), (120, 102, 72)])
        _twig(db, x, y, rnd.uniform(-0.4, 0.4), rnd.uniform(6, 12) * SS, SS, col + (170,), rnd.uniform(-2, 2) * SS, rnd)
    # the outside of the bowl: dark packing, then woven twigs at crossing
    # angles, mostly running round the bowl
    # (the outside only: not over the rim's opening, where the egg shows)
    opening = Image.new('L', (W, H), 0)
    ImageDraw.Draw(opening).ellipse((cx - rx * 0.86, cy - ry * 0.80, cx + rx * 0.86, cy + ry * 0.80), fill=255)
    mask = Image.fromarray(np.where(np.asarray(opening) > 0, 0, np.asarray(mask)).astype(np.uint8))
    # and only below the front of the rim (the rim's own twigs make the lip)
    yy, xx = np.mgrid[0:H, 0:W]
    uu = np.clip((xx - cx) / rx, -1, 1)
    lip = cy + 0.55 * ry * np.sqrt(1 - uu * uu)
    mask = Image.fromarray(np.where(yy < lip, 0, np.asarray(mask)).astype(np.uint8))
    pack = Image.new('RGBA', (W, H), (40, 27, 15, 255))
    outer = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    outer.paste(pack, (0, 0), mask)
    do = ImageDraw.Draw(outer)
    for _ in range(520):
        # a point on the visible outside (below the rim's front half)
        x = rnd.uniform(cx - rx, cx + rx)
        u = (x - cx) / rx
        top = cy + ry * math.sqrt(max(0, 1 - u * u)) * rnd.uniform(-0.2, 1.0)
        bot = cy + depth * math.sqrt(max(0, 1 - u * u))
        if bot <= top:
            continue
        y = rnd.uniform(top, bot)
        if ((x - cx) / (rx * 0.86)) ** 2 + ((y - cy) / (ry * 0.8)) ** 2 < 1:
            continue
        # running round the bowl, following its curve, with crossing twigs
        slope = math.atan2(-u * depth, rx * math.sqrt(max(0.05, 1 - u * u))) * ((y - cy) / max(1, depth))
        ang = slope + rnd.choice([0, 0, 0, 0.5, -0.5]) + rnd.uniform(-0.35, 0.35)
        col = rnd.choice(TWIGS)
        _twig(do, x, y, ang, rnd.uniform(8, 20) * SS, rnd.choice([1, 2, 2, 3]) * SS, col + (255,), rnd.uniform(-4, 4) * SS, rnd)
    # keep the weave within the bowl's shape
    keep = Image.new('L', (W, H), 0)
    keep.paste(mask.filter(ImageFilter.MaxFilter(3 * SS + 1 if (3 * SS) % 2 == 0 else 3 * SS)), (0, 0))
    outer.putalpha(Image.fromarray(np.minimum(np.asarray(outer.split()[3]), np.asarray(keep))))
    front.alpha_composite(outer)
    # the rim: thick twigs round the lip, the near ones over the egg
    for _ in range(70):
        a = rnd.uniform(0, 2 * math.pi)
        x, y = cx + math.cos(a) * rx * rnd.uniform(0.9, 1.04), cy + math.sin(a) * ry * rnd.uniform(0.85, 1.1)
        ang = a + math.pi / 2 + rnd.uniform(-0.45, 0.45)
        col = rnd.choice(TWIGS[1:6])
        near = math.sin(a) > -0.15
        _twig(df if near else db, x, y, ang, rnd.uniform(10, 22) * SS, rnd.choice([2, 2, 3]) * SS, col + (255,), rnd.uniform(-4, 4) * SS, rnd)
    # stray twigs sticking out of the weave
    for _ in range(12):
        a = rnd.uniform(0, 2 * math.pi)
        x0, y0 = cx + math.cos(a) * rx * 0.98, cy + math.sin(a) * ry * 1.05 + (depth * 0.3 if math.sin(a) > 0 else 0)
        t = a + rnd.uniform(-0.7, 0.7)
        L = rnd.uniform(8, 20) * SS
        col = rnd.choice(TWIGS) + (255,)
        (df if math.sin(a) > -0.1 else db).line((x0, y0, x0 + math.cos(t) * L, y0 + math.sin(t) * L * 0.5), fill=col, width=SS + 1)
    # a wisp of down caught on the far rim
    for _ in range(rnd.choice([1, 2])):
        a = rnd.uniform(math.pi * 1.15, math.pi * 1.85)
        x, y = cx + math.cos(a) * rx * 0.95, cy + math.sin(a) * ry * 0.95
        f = Image.new('RGBA', (26 * SS, 14 * SS), (0, 0, 0, 0))
        fd = ImageDraw.Draw(f)
        for k in range(16):  # barbs
            bx = 4 * SS + k * 1.1 * SS
            fd.line((bx, 7 * SS, bx + rnd.uniform(-2, 2) * SS, rnd.choice([1, 13]) * SS), fill=(236, 230, 218, 150), width=SS)
        fd.line((3 * SS, 7 * SS, 22 * SS, 7 * SS), fill=(214, 204, 188, 255), width=SS)
        f = f.rotate(rnd.uniform(-35, 35), expand=True).filter(ImageFilter.GaussianBlur(SS * 0.5))
        back.alpha_composite(f, (int(x - f.width / 2), int(y - f.height / 2 - 2 * SS)))
    back = _shade_layer(back, cx, cy, rx, depth, ry)
    front = _shade_layer(front, cx, cy, rx, depth, ry)
    w1, h1 = width, H // SS
    # (the rim's centre line, and the bottom of the bowl where it rests)
    return back.resize((w1, h1), Image.LANCZOS), front.resize((w1, h1), Image.LANCZOS), int(cy / SS), int((cy + depth) / SS)

NEST_W = 70
NEST_VARIANTS = 8


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.abspath(__file__))
    hessian_wall(700, 900, 5).convert('RGB').save(os.path.join(outdir, 'wall.png'), optimize=True)
    board3(2048, 26, 22, 3).save(os.path.join(outdir, 'board.png'), optimize=True)
    # nests: every variant's back and front layers on one canvas size,
    # stacked as frames (back 0, front 0, back 1, ...); nests.txt gives each
    # variant's rim and bottom (rows from the canvas top)
    made = [nest(NEST_W, i * 7 + 11) for i in range(NEST_VARIANTS)]
    cw = max(max(b.width, f.width) for b, f, _, _ in made)
    ch = max(max(b.height, f.height) for b, f, _, _ in made)
    sheet = Image.new('RGBA', (cw, ch * 2 * NEST_VARIANTS), (0, 0, 0, 0))
    lines = []
    for i, (back, front, rim, bottom) in enumerate(made):
        sheet.alpha_composite(back, ((cw - back.width) // 2, ch * 2 * i))
        sheet.alpha_composite(front, ((cw - front.width) // 2, ch * (2 * i + 1)))
        lines.append(f"{i} {int(rim)} {int(bottom)}")
    from PIL import PngImagePlugin
    info = PngImagePlugin.PngInfo()
    info.add_text('openc2e-frames', str(2 * NEST_VARIANTS))
    sheet.save(os.path.join(outdir, 'nests.png'), pnginfo=info, optimize=True)
    with open(os.path.join(outdir, 'nests.txt'), 'w') as f:
        f.write('# variant rim bottom (pixel rows in each nest frame)\n' + '\n'.join(lines) + '\n')


if __name__ == '__main__':
    main()
