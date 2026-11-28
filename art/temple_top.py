"""The temple's top floor (owner, 2026-11-27, with a marked screenshot): made
on the player's own machine from their installed Creatures 2
(bring-your-own-data: only this recipe is part of the project, never the
game's pixels). additions/temple-top.cos places the pictures.

The stone temple east of the long rope bridge has a long slab across its
front (its top at y 396, x 4183-4894), above the ground floor where the
cage lift (3 2 2, cage x 4172-4275) stops. The lift now goes up one more
floor, onto the slab, and:

- ztcl: the twisted column at the slab's west end (x 4078-4144, y 407-606),
  copied so it is drawn in front of creatures walking past it below.
- ztpc: behind everything, the lift's painted mechanism (a cog, a pale
  wheel, its belts and a coiled green cable, on the wall under the slab at
  x 4212-4280, y 476-560) painted out with the stone around it, and the
  lift's two cables (x 4221 and 4263) carried on up to where the mechanism
  hangs now.
- ztpu: behind creatures, the same mechanism moved up 330 px, hung under a
  stone arm on a slim twisted post standing on the slab east of the shaft:
  the post from the column itself (its helix scaled down and repeated, its
  capital and base), the arm from the slab's own edge.
- ztbc and ztbl: the great bell (x 4577-4747, y 113-278), which swings. ztbc
  paints the bell out of the background (the sky either side carried
  across, the stones either side of its skirt carried inward); ztbl is the
  bell turned about the top of its dome (4662, 118): frame 0 still, 1-3 swung east
  by 0.9, 1.8 and 2.6 degrees, 4-6 west by the same.

    python3 temple_top.py C2_DIR USER_DATA_DIR [MOCKUP.png]

writes USER_DATA_DIR/Images/ztcl.png, ztpc.png, ztpu.png, ztbc.png and
ztbl.png (top-left corners in TOPLEFT below), and with a third argument a
mock-up of the temple with every piece in place (the bell at frame 3).
"""
import sys
import os
import math
import numpy as np
from PIL import Image, PngImagePlugin
from background import region

FLOOR = 396                      # the slab's top: the new floor
# the column
CL_X0, CL_Y0, CL_X1, CL_Y1 = 4074, 407, 4148, 607
# the painted mechanism under the slab
MX0, MY0, MX1, MY1 = 4210, 474, 4282, 562
WHEEL = (4257.5, 505.5, 9.5)     # the pale wheel: centre and radius
CABLES = (4221, 4263)            # the lift's two cables (left edge, 2 px each)
GRAIN_X0 = 4306                  # plain stones east of the mechanism, for grain
# the post and arm it hangs from, and the block of wall carrying it
POST_X0, POST_W = 4282, 30       # east of the cage (4275), west of the caller (4324)
ARM_X0, ARM_X1, ARM_Y0, ARM_H = 4204, 4318, 110, 15
LIFT = MY0 - (ARM_Y0 + ARM_H)    # how far up the block moves (its top under the arm)
PU_X0, PU_Y0, PU_X1, PU_Y1 = 4200, 106, 4320, FLOOR + 2
PC_Y0 = MY1 - LIFT               # the cables from the block's foot down
# the bell
BX0, BY0, BX1, BY1 = 4548, 106, 4776, 288
PIVOT = (4662.0, 118.0)          # where the hanger rods meet the dome
BELL_TOP, BELL_BOTTOM = 113, 278   # the whole bell, its crown loop and dome too
COVER_TOP = 113
ANGLES = [0.0, 0.9, 1.8, 2.6, -0.9, -1.8, -2.6]   # gentle: a great bell, and its rods stay joined

TOPLEFT = {
    'ztcl': (CL_X0, CL_Y0),
    'ztpc': (MX0, PC_Y0),
    'ztpu': (PU_X0, PU_Y0),
    'ztbc': (BX0, BY0),
    'ztbl': (BX0, BY0),
}


def rgb(c2, x0, y0, x1, y1):
    im = region(c2, x0, y0, x1, y1)
    a = np.asarray(im if hasattr(im, 'size') and not isinstance(im, np.ndarray) else im)
    return np.asarray(a)[..., :3].astype(np.float32)


def rgba_of(rgbf, alpha):
    out = np.zeros(rgbf.shape[:2] + (4,), np.uint8)
    out[..., :3] = np.clip(rgbf + 0.5, 0, 255).astype(np.uint8)
    out[..., 3] = np.clip(alpha * 255 + 0.5, 0, 255).astype(np.uint8)
    return out


def dilate(m, n=1):
    for _ in range(n):
        d = m.copy()
        d[1:] |= m[:-1]
        d[:-1] |= m[1:]
        d[:, 1:] |= m[:, :-1]
        d[:, :-1] |= m[:, 1:]
        m = d
    return m


def inpaint(img, hole, rounds=400):
    """Fill the hole from its edges inward (each pass, every hole pixel with
    a known neighbour takes their mean), then a few smoothing passes over the
    filled part."""
    img = img.copy()
    known = ~hole
    h, w = hole.shape
    for _ in range(rounds):
        if known.all():
            break
        acc = np.zeros_like(img)
        cnt = np.zeros((h, w), np.float32)
        for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1), (-1, -1), (-1, 1), (1, -1), (1, 1)):
            src = np.roll(np.roll(img, dy, 0), dx, 1)
            k = np.roll(np.roll(known, dy, 0), dx, 1)
            if dy == -1: k[-1, :] = False
            if dy == 1: k[0, :] = False
            if dx == -1: k[:, -1] = False
            if dx == 1: k[:, 0] = False
            acc += src * k[..., None]
            cnt += k
        grow = (~known) & (cnt > 0)
        img[grow] = acc[grow] / cnt[grow][:, None]
        known = known | grow
    return img


# ---- the column ---------------------------------------------------------

def column(c2):
    a = rgb(c2, CL_X0, CL_Y0, CL_X1, CL_Y1)
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    warm = (r > b + 10)              # stone, not the blue mountain and sky
    h, w = warm.shape
    ys, xs = np.mgrid[0:h, 0:w]
    wx, wy = xs + CL_X0, ys + CL_Y0
    # the column's own width (its capital and base a little wider), so the
    # warm stones and dirt beside it are left out
    inside = (wx >= 4078) & (wx <= 4143)
    inside &= ~((wy > 432) & (wy < 576) & ((wx < 4082) | (wx > 4140)))
    m = warm & inside
    # keep the one big piece (the column), drop specks
    m = dilate(~dilate(~m, 1), 1)
    alpha = m.astype(np.float32)
    return rgba_of(a, alpha)


# ---- the mechanism and its old place -------------------------------------

def components(m):
    """Connected pieces of a mask (4-neighbour), as (size, mask) pairs."""
    h, w = m.shape
    seen = np.zeros_like(m)
    out = []
    for y0 in range(h):
        for x0 in range(w):
            if not m[y0, x0] or seen[y0, x0]:
                continue
            stack = [(y0, x0)]
            seen[y0, x0] = True
            piece = []
            while stack:
                y, x = stack.pop()
                piece.append((y, x))
                for yy, xx in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1)):
                    if 0 <= yy < h and 0 <= xx < w and m[yy, xx] and not seen[yy, xx]:
                        seen[yy, xx] = True
                        stack.append((yy, xx))
            pm = np.zeros_like(m)
            ys, xs = zip(*piece)
            pm[list(ys), list(xs)] = True
            out.append((len(piece), pm))
    return out


def mechanism_mask(a):
    """The mechanism's pixels: iron black and the cable's olive and yellow
    are short of blue (blue under half the green), the sandstone and its
    shadowed joints are not; plus the pale wheel. Only pieces of some size
    that touch the mechanism's middle are kept (no specks)."""
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    h, w = r.shape
    ys, xs = np.mgrid[0:h, 0:w]
    wx, wy = xs + MX0, ys + MY0
    lum = 0.3 * r + 0.59 * g + 0.11 * b
    metal = (b < 0.5 * g) | (lum < 26)
    wheel = (wx - WHEEL[0]) ** 2 + (wy - WHEEL[1]) ** 2 <= WHEEL[2] ** 2
    m = metal | wheel
    keep = np.zeros_like(m)
    for size, pm in components(m):
        if size >= 25:
            keep |= pm
    # the cables below the wheel are the lift's, not the mechanism's
    cable = np.zeros_like(keep)
    for cx in CABLES:
        cable |= (wx >= cx - 1) & (wx <= cx + 2) & (wy > 516)
    return keep, cable


def cable_layer(h, w, x0, y0, y_from):
    """The two cables, 2 px dark with a soft edge, from y_from down."""
    col = np.zeros((h, w, 3), np.float32)
    al = np.zeros((h, w), np.float32)
    for cx in CABLES:
        for dx, k in ((-1, 0.35), (0, 1.0), (1, 1.0), (2, 0.35)):
            x = cx + dx - x0
            if 0 <= x < w:
                y = max(0, y_from - y0)
                al[y:, x] = np.maximum(al[y:, x], k)
                col[y:, x] = (22, 20, 18)
    return col, al


def old_place(c2):
    """ztpc: the old place painted out, and the cables from under the arm."""
    a = rgb(c2, MX0, MY0, MX1, MY1)
    m, cable = mechanism_mask(a)
    hole = dilate(m | cable, 2)
    filled = inpaint(a, hole)
    # stone grain over the filled part, from plain stones of the same
    # courses further east (x 4306-4378)
    shifted = rgb(c2, GRAIN_X0, MY0, GRAIN_X0 + (MX1 - MX0), MY1)
    grain = shifted - (np.roll(shifted, 1, 0) + np.roll(shifted, -1, 0) + np.roll(shifted, 1, 1) + np.roll(shifted, -1, 1)) / 4
    filled[hole] += 0.6 * grain[hole]
    h = MY1 - PC_Y0
    w = MX1 - MX0
    col = np.zeros((h, w, 3), np.float32)
    al = np.zeros((h, w), np.float32)
    off = MY0 - PC_Y0
    col[off:] = filled
    al[off:] = hole.astype(np.float32)
    cc, ca = cable_layer(h, w, MX0, PC_Y0, PC_Y0)
    col = col * (1 - ca[..., None]) + cc * ca[..., None]
    al = np.maximum(al, ca)
    return rgba_of(col, al)


def helix_period(shaft):
    """The column's twist repeats down its length: the period (px) from the
    luminance's autocorrelation."""
    lum = shaft.mean(axis=(1, 2))
    lum = lum - lum.mean()
    best, bp = -1e9, 20
    for p in range(12, 40):
        c = float((lum[:-p] * lum[p:]).mean())
        if c > best:
            best, bp = c, p
    return bp


def scaled(img, alpha, w, h):
    im = Image.fromarray(rgba_of(img, alpha), 'RGBA').resize((w, h), Image.LANCZOS)
    a = np.asarray(im).astype(np.float32)
    return a[..., :3], a[..., 3] / 255.0


def post_and_arm(c2):
    """ztpu: the post, the arm, and the mechanism hung under the arm."""
    W, H = PU_X1 - PU_X0, PU_Y1 - PU_Y0
    col = np.zeros((H, W, 3), np.float32)
    al = np.zeros((H, W), np.float32)

    def put(img, a, x, y):
        h, w = a.shape
        x -= PU_X0
        y -= PU_Y0
        sx0, sy0 = max(0, -x), max(0, -y)
        x0, y0 = max(0, x), max(0, y)
        x1, y1 = min(W, x + w), min(H, y + h)
        if x1 <= x0 or y1 <= y0:
            return
        s = (slice(sy0, sy0 + y1 - y0), slice(sx0, sx0 + x1 - x0))
        k = a[s][..., None]
        col[y0:y1, x0:x1] = col[y0:y1, x0:x1] * (1 - k) + img[s] * k
        al[y0:y1, x0:x1] = np.maximum(al[y0:y1, x0:x1], a[s])

    # the column, cut (its alpha from column()), scaled to the post's width
    c = column(c2).astype(np.float32)
    crgb, ca = c[..., :3], c[..., 3] / 255.0
    scale = POST_W / 64.0
    cap = slice(0, 26)                         # capital, y 407-433
    shaft = slice(30, 168)                     # the twist, y 437-575
    base = slice(168, 200)                     # base, y 575-607
    per = helix_period(crgb[shaft])
    one = slice(shaft.start, shaft.start + per)
    def sc(sl):
        im, a = crgb[sl], ca[sl]
        im, a = scaled(im, a, POST_W, max(1, int(round((sl.stop - sl.start) * scale))))
        a = (a > 0.6)
        # one pixel off each side: the cut column's edge pixels are its
        # outline against the sky, a dotted line when scaled down
        a[:, 1:] &= a[:, :-1]
        a[:, :-1] &= a[:, 1:]
        return im, a.astype(np.float32)   # hard edged, as the game's sprites
    capi, capa = sc(cap)
    basi, basa = sc(base)
    twi, twa = sc(one)
    top = ARM_Y0 + ARM_H - 2
    y = top + capa.shape[0]
    bottom = FLOOR + 1 - basa.shape[0]
    while y < bottom:
        put(twi, twa, POST_X0, y)
        y += twa.shape[0]
    put(basi, basa, POST_X0, bottom)
    put(capi, capa, POST_X0, top)

    # the arm: a length of the slab's own edge (x 4330-4436, y 392-409),
    # its east end resting on the post's capital, its west end cut square
    s = rgb(c2, 4330, 392, 4330 + (ARM_X1 - ARM_X0), 409)
    s = np.asarray(Image.fromarray(np.clip(s, 0, 255).astype(np.uint8)).resize((ARM_X1 - ARM_X0, ARM_H), Image.LANCZOS)).astype(np.float32)
    sa = np.ones(s.shape[:2], np.float32)
    # a little shade under its west end (it is thinner there, carved)
    for x in range(12):
        sa[ARM_H - 1 - x // 4:, x] = 0
    put(s, sa, ARM_X0, ARM_Y0)

    # the block of wall carrying the mechanism, moved up under the arm: the
    # same picture, stones and all, its cut sides finished with a shadowed
    # edge and a lit top edge, and a shadow under the arm
    a = rgb(c2, MX0, MY0, MX1, MY1)
    blk = a.copy()
    bh, bw = blk.shape[:2]
    for i, k in enumerate((0.45, 0.7)):
        blk[:, i] *= k
        blk[:, bw - 1 - i] *= k
        blk[bh - 1 - i, :] *= k
    blk[0:3] *= np.array([0.55, 0.7, 0.85])[:, None, None]
    put(blk, np.ones((bh, bw), np.float32), MX0, MY0 - LIFT)
    return rgba_of(col, al)


# ---- the bell -------------------------------------------------------------

def bell_mask(a):
    """The bell within (BX0, BY0)-(BX1, BY1): bronze against the sky row by
    row down to its flange, then its skirt's known edges."""
    h, w = a.shape[:2]
    r, b = a[..., 0], a[..., 2]
    sky = b > r + 12
    m = np.zeros((h, w), bool)
    cx = int(PIVOT[0]) - BX0
    for y in range(BELL_TOP - BY0, BELL_BOTTOM - BY0 + 1):
        wy = y + BY0
        if wy < 214:
            x0 = cx
            while x0 > 0 and not sky[y, x0 - 1]:
                x0 -= 1
            x1 = cx
            while x1 < w - 1 and not sky[y, x1 + 1]:
                x1 += 1
            if wy < 133:
                x0, x1 = max(x0, 4606 - BX0), min(x1, 4720 - BX0)   # the dome's top, not the rods
            else:
                x0, x1 = max(x0, 4575 - BX0), min(x1, 4749 - BX0)
        elif wy < 224:
            x0, x1 = 4576 - BX0, 4748 - BX0   # the flange
        elif wy < 270:
            x0, x1 = 4579 - BX0, 4746 - BX0   # the skirt
        else:
            x0, x1 = 4576 - BX0, 4748 - BX0   # the lip
        m[y, x0:x1 + 1] = True
    return m


def bell_cover(c2):
    a = rgb(c2, BX0, BY0, BX1, BY1)
    m = bell_mask(a)
    hole = dilate(m, 1)
    hole[:COVER_TOP - BY0] = False      # the crown stays; the turned bell covers its join
    out = a.copy()
    h, w = hole.shape
    for y in range(h):
        wy = y + BY0
        xs = np.where(hole[y])[0]
        if not len(xs):
            continue
        x0, x1 = xs[0], xs[-1]
        if wy < 214:
            # the sky carried across: left and right samples just outside
            L = a[y, max(0, x0 - 6):x0].mean(axis=0)
            R = a[y, x1 + 1:min(w, x1 + 7)].mean(axis=0)
            t = (np.arange(x0, x1 + 1) - x0) / max(1, x1 - x0)
            out[y, x0:x1 + 1] = L[None] * (1 - t[:, None]) + R[None] * t[:, None]
        else:
            # the stones either side carried inward (only the bell's edges
            # are ever uncovered as it swings), mirrored so the courses meet
            mid = (x0 + x1) // 2
            for x in range(x0, x1 + 1):
                if x <= mid:
                    sx = x0 - 1 - ((x - x0) % 18)
                else:
                    sx = x1 + 1 + ((x1 - x) % 18)
                out[y, x] = a[y, int(np.clip(sx, 0, w - 1))]
    al = hole.astype(np.float32)
    return rgba_of(out, al)


def bell_frames(c2):
    a = rgb(c2, BX0, BY0, BX1, BY1)
    m = bell_mask(a).astype(np.float32)
    # soften the edge by half a pixel
    soft = (m + np.roll(m, 1, 0) + np.roll(m, -1, 0) + np.roll(m, 1, 1) + np.roll(m, -1, 1)) / 5
    m = np.where(m > 0, np.maximum(m * 0.85, soft), soft * 0.5)
    h, w = m.shape
    px, py = PIVOT[0] - BX0, PIVOT[1] - BY0
    frames = []
    ys, xs = np.mgrid[0:h, 0:w].astype(np.float32)
    for deg in ANGLES:
        t = math.radians(deg)
        # inverse map: a point of the frame comes from the bell turned back
        dx, dy = xs - px, ys - py
        sx = px + dx * math.cos(t) + dy * math.sin(t)
        sy = py - dx * math.sin(t) + dy * math.cos(t)
        x0 = np.floor(sx).astype(int)
        y0 = np.floor(sy).astype(int)
        fx, fy = sx - x0, sy - y0
        def at(arr, yy, xx):
            ok = (xx >= 0) & (xx < w) & (yy >= 0) & (yy < h)
            yy2, xx2 = np.clip(yy, 0, h - 1), np.clip(xx, 0, w - 1)
            v = arr[yy2, xx2]
            return np.where(ok[..., None] if v.ndim == 3 else ok, v, 0)
        prem = a * m[..., None]
        c = (at(prem, y0, x0) * ((1 - fx) * (1 - fy))[..., None] + at(prem, y0, x0 + 1) * (fx * (1 - fy))[..., None] +
             at(prem, y0 + 1, x0) * ((1 - fx) * fy)[..., None] + at(prem, y0 + 1, x0 + 1) * (fx * fy)[..., None])
        al = (at(m, y0, x0) * (1 - fx) * (1 - fy) + at(m, y0, x0 + 1) * fx * (1 - fy) +
              at(m, y0 + 1, x0) * (1 - fx) * fy + at(m, y0 + 1, x0 + 1) * fx * fy)
        col = np.where(al[..., None] > 0, c / np.maximum(al, 1e-6)[..., None], 0)
        # binary-ish edge: C2's other sprites have hard edges
        al = np.clip((al - 0.25) / 0.5, 0, 1)
        frames.append(rgba_of(col, al))
    return frames


def save(path, frames):
    frames = frames if isinstance(frames, list) else [frames]
    info = PngImagePlugin.PngInfo()
    info.add_text('openc2e-frames', str(len(frames)))
    h = frames[0].shape[0]
    strip = np.concatenate(frames, axis=1) if len(frames) > 1 else frames[0]
    Image.fromarray(strip, 'RGBA').save(path, pnginfo=info)


def main():
    c2, user = sys.argv[1], sys.argv[2]
    out = os.path.join(user, 'Images')
    os.makedirs(out, exist_ok=True)
    made = {
        'ztcl': column(c2),
        'ztpc': old_place(c2),
        'ztpu': post_and_arm(c2),
        'ztbc': bell_cover(c2),
        'ztbl': bell_frames(c2),
    }
    for name, fr in made.items():
        save(os.path.join(out, name + '.png'), fr)
        f0 = fr[0] if isinstance(fr, list) else fr
        print('wrote', name, f0.shape[1], 'x', f0.shape[0], len(fr) if isinstance(fr, list) else 1, 'frame(s)')
    if len(sys.argv) > 3:
        X0, Y0, X1, Y1 = 3980, 60, 4950, 700
        bg = Image.fromarray(np.asarray(region(c2, X0, Y0, X1, Y1))[..., :3].astype(np.uint8)).convert('RGBA')
        for name, frame in (('ztpc', made['ztpc']), ('ztbc', made['ztbc']), ('ztpu', made['ztpu']),
                            ('ztbl', made['ztbl'][3]), ('ztcl', made['ztcl'])):
            x, y = TOPLEFT[name]
            layer = Image.new('RGBA', bg.size)
            layer.paste(Image.fromarray(frame, 'RGBA'), (x - X0, y - Y0))
            bg = Image.alpha_composite(bg, layer)
        bg.save(sys.argv[3])
        print('mock-up', sys.argv[3])


if __name__ == '__main__':
    main()
