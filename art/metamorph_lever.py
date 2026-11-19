"""Draw the Metamorph Device's version switch (owner, 2026-11-19: one device
with a switch for its two versions): a small riveted plate on the front of
the device's left end block, with two embossed, painted icons, a slot
between them and a lever that points at the version in use.

  top icon     a lightning bolt: the creature inside is ended at once
  bottom icon  a skull: the creature inside is given poisons, let out to die

  frame 0  lever up (at once)
  frame 1  lever down (poisons)

The owner chose the icons and finish from mock-ups (2026-11-19): bolt "L4"
(orange, dark brown outline), skull "S1b" (bone white, the same outline),
embossed and painted ("E4"), the paint rubbed off the raised edges and
a little faded ("W2"). Light from the upper left, as on the device. The
plate's olive is a color chosen by eye to sit on the device's housings;
everything is drawn from scratch (no game pixels are read or copied), so the
output can ship with the project. Placed by additions/metamorph.cos.

    python3 metamorph_lever.py OUT_BASENAME [mockup DEVICE.s16 BG.png OUT.png]

writes OUT_BASENAME.png (frame 0), OUT_BASENAME-1.png (frame 1) and
OUT_BASENAME.s16.
"""
import sys
import numpy as np
from PIL import Image, ImageDraw
import s16

W, H = 22, 46
K = 8           # icons are drawn at K times and reduced
N = 14          # icon size
PLATE = np.array([104, 99, 66], np.float32)
PLATE_LIT = np.array([150, 144, 100], np.float32)
PLATE_DARK = np.array([52, 49, 32], np.float32)
LIGHT = np.array([196, 188, 132], np.float32)
SHADE = np.array([34, 32, 20], np.float32)
BARE = np.array([158, 152, 112], np.float32)   # metal where the paint is rubbed off
SLOT = (24, 22, 16)
STEEL = (176, 178, 172)
KNOB = (40, 38, 34)

ORANGE = (255, 190, 40, 255)
OUTLINE = (70, 40, 10, 255)
BONE = (232, 228, 214, 255)
DARK = (25, 22, 18, 255)


def P(v):
    return v * K


def bolt():
    pts = [(8.5, 0.5), (2.5, 8), (6.5, 8), (5, 13.5), (11.5, 5.5), (7.5, 5.5), (9.5, 0.5)]
    im = Image.new('RGBA', (N * K, N * K), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    for s, col in ((1.05, OUTLINE), (0.82, ORANGE)):
        d.polygon([(P(7 + (x - 7) * s), P(7 + (y - 7) * s)) for x, y in pts], fill=col)
    return im


def skull():
    im = Image.new('RGBA', (N * K, N * K), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)

    def one(s, col, holes):
        def b(x0, y0, x1, y1):
            return [P(7 + (x0 - 7) * s), P(7 + (y0 - 7) * s), P(7 + (x1 - 7) * s), P(7 + (y1 - 7) * s)]
        d.ellipse(b(2, 0.8, 12, 10), fill=col)
        d.rectangle(b(4, 8, 10, 12.6), fill=col)
        d.ellipse(b(3.8, 4.2, 6.6, 7.2), fill=holes)
        d.ellipse(b(7.4, 4.2, 10.2, 7.2), fill=holes)
        d.polygon([(P(7), P(7 + 0.6 * s)), (P(7 - 0.8 * s), P(7 + 2 * s)), (P(7 + 0.8 * s), P(7 + 2 * s))], fill=holes)
        for x in (5.6, 7, 8.4):
            d.line([(P(7 + (x - 7) * s), P(7 + 3.6 * s)), (P(7 + (x - 7) * s), P(7 + 5.6 * s))], fill=holes, width=max(1, int(0.7 * s * K)))
    one(1.0, OUTLINE, OUTLINE)
    one(0.80, BONE, DARK)
    return im


def shift(a, dx, dy):
    out = np.zeros_like(a)
    h, w = a.shape
    out[max(dy, 0):h + min(dy, 0), max(dx, 0):w + min(dx, 0)] = a[max(-dy, 0):h + min(-dy, 0), max(-dx, 0):w + min(-dx, 0)]
    return out


def erode(m, r):
    out = m.copy()
    for dx in range(-r, r + 1, max(1, r // 2)):
        for dy in range(-r, r + 1, max(1, r // 2)):
            out = np.minimum(out, shift(m, dx, dy))
    return out


def embossed(icon):
    """The icon raised from the plate, painted, faded a little, the paint
    rubbed off its raised edges; a 14 x 14 RGB tile of plate."""
    a = np.asarray(icon).astype(np.float32)
    m = (a[..., 3] > 127).astype(np.float32)
    fill = a[..., :3]
    lum = fill @ np.array([0.3, 0.59, 0.11], np.float32)
    faded = np.repeat(lum[..., None], 3, axis=2) * 0.55 + PLATE * 0.45
    fill = fill * 0.8 + faded * 0.2
    rgb = np.empty(m.shape + (3,), np.float32)
    rgb[:] = PLATE
    rgb = rgb * (1 - m[..., None]) + fill * m[..., None]
    rubbed = m - erode(m, K)
    rgb = rgb * (1 - rubbed[..., None]) + BARE * rubbed[..., None]
    tl = np.clip(m - shift(m, K, K), 0, 1)
    br = np.clip(m - shift(m, -K, -K), 0, 1)
    cast = np.clip(shift(m, K, K) - m, 0, 1)
    rgb = rgb * (1 - 0.8 * tl[..., None]) + LIGHT * 0.8 * tl[..., None]
    rgb = rgb * (1 - 0.8 * br[..., None]) + SHADE * 0.8 * br[..., None]
    rgb = rgb * (1 - 0.5 * cast[..., None]) + SHADE * 0.5 * cast[..., None]
    return Image.fromarray(rgb.clip(0, 255).astype(np.uint8), 'RGB').resize((N, N), Image.LANCZOS)


def plate():
    """The riveted plate, bevelled light at the upper left, as RGBA."""
    ss = 4
    ys, xs = np.mgrid[0:H * ss, 0:W * ss].astype(np.float32)
    x = (xs + 0.5) / ss
    y = (ys + 0.5) / ss
    x0, y0, x1, y1, r = 0.5, 0.5, W - 0.5, H - 0.5, 3.0
    cx = np.clip(x, x0 + r, x1 - r)
    cy = np.clip(y, y0 + r, y1 - r)
    inside = (np.hypot(x - cx, y - cy) <= r).astype(np.float32)
    edge = np.minimum.reduce([x - x0, x1 - x, y - y0, y1 - y])
    lit = ((x - x0) + (y - y0)) < ((x1 - x) + (y1 - y))
    col = np.empty(x.shape + (3,), np.float32)
    col[:] = PLATE
    col *= (1 + 0.04 * np.sin(x * 2.3 + y * 0.7))[..., None]
    col = np.where((edge < 1.2)[..., None], np.where(lit[..., None], PLATE_LIT, PLATE_DARK), col)
    for rx, ry in ((2.8, 2.8), (W - 2.8, 2.8), (2.8, H - 2.8), (W - 2.8, H - 2.8)):
        d = np.hypot(x - rx, y - ry)
        col = np.where((d <= 1.1)[..., None], np.where(((x - rx) + (y - ry) < 0)[..., None], PLATE_LIT * 1.1, PLATE_DARK), col)
    a = np.dstack([col, inside[..., None] * 255]).reshape(H, ss, W, ss, 4).mean(axis=(1, 3))
    a[..., 3] = np.where(a[..., 3] >= 128, 255, 0)
    return Image.fromarray(a.clip(0, 255).astype(np.uint8), 'RGBA')


def draw(state):
    im = plate()
    im.paste(embossed(bolt()), (4, 2))
    im.paste(embossed(skull()), (4, 30))
    d = ImageDraw.Draw(im)
    # the slot between the icons, and the lever pointing at the one in use
    d.rectangle([10, 18, 12, 27], fill=SLOT)
    px, py = 11, 22
    kx, ky = (15, 18) if state == 0 else (15, 26)
    d.line([(px, py), (kx, ky)], fill=STEEL, width=1)
    d.point((px, py), fill=(90, 90, 86))
    d.ellipse([kx - 2, ky - 2, kx + 2, ky + 2], fill=KNOB)
    d.point((kx - 1, ky - 1), fill=(140, 136, 128))
    return im


# where it goes on the device (relative to its top left): the left end
# block's front face
ON_DEVICE = (3, 139)

if __name__ == '__main__':
    base = sys.argv[1]
    f0, f1 = draw(0), draw(1)
    f0.save(base + '.png')
    f1.save(base + '-1.png')
    s16.write(base + '.s16', [f0, f1])
    if len(sys.argv) >= 6 and sys.argv[2] == 'mockup':
        # on an in-game shot (camera so that world = screen + (5850, 1256)),
        # the device at 6244,1601 drawn from the player's own sprite file
        # (for this picture only), at rest and working, each switch state
        fr = s16.read(sys.argv[3])
        bg = Image.open(sys.argv[4]).convert('RGBA')
        dx, dy = 6244 - 5850, 1601 - 1256
        tiles = []
        for frame, f in ((1, f0), (1, f1), (7, f0), (7, f1)):
            im = bg.copy()
            dv = Image.new('RGBA', (216, 188), (0, 0, 0, 0))
            dv.alpha_composite(fr[8], (70, 45))
            dv.alpha_composite(fr[frame])
            dv.alpha_composite(f, ON_DEVICE)
            im.alpha_composite(dv, (dx, dy))
            tiles.append(im.crop((dx - 40, dy - 30, dx + 256, dy + 200)))
        sheet = Image.new('RGB', (4 * 306 + 160, 230), (20, 20, 20))
        for i, t in enumerate(tiles):
            sheet.paste(t.convert('RGB'), (i * 306, 0))
        z = Image.new('RGBA', (2 * W * 3 + 10, H * 3), (20, 20, 20, 255))
        z.alpha_composite(f0.resize((W * 3, H * 3), Image.NEAREST), (0, 0))
        z.alpha_composite(f1.resize((W * 3, H * 3), Image.NEAREST), (W * 3 + 10, 0))
        sheet.paste(z.convert('RGB'), (4 * 306, 40))
        sheet.save(sys.argv[5])
        print('mock-up', sys.argv[5])
