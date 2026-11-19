"""Draw original railing pieces that close two walkway ends where creatures
walked off into long drops (owner, 2026-11-19: "railing + closed end"):

  treehouse  the treehouse platform's east end (room 51 ends at x 3917,
             walk line y 1747; 508 px down to the cave floor). Its top rail
             runs on past the last post (x ~3887) to about x 3940 with
             nothing under it: a new post under the rail's end, lashed to
             the rail and the deck beam, and a lower rail across to the
             last post.
  scaffold   the scaffold's east end in the cave (room 54 ends at x 3828,
             walk line y 1996; 259 px down to the flower beds). Its last
             leaning post already stands at the end: a lower rail across
             the last two posts finishes it.

Both walkways are rust-orange square-cut poles with grey cord lashings; the
colors here are numbers chosen by eye to match, and everything is drawn
from scratch (no game pixels are read or copied), so the output can ship
with the project. Placed by additions/rail-ends.cos, which also closes the
doorways (150: too small for a creature, open to thrown things).

    python3 rail_ends.py OUT_BASENAME [mockup BG6.png BG3.png OUT.png]

writes OUT_BASENAME.png (frame 0, treehouse) and OUT_BASENAME-1.png (frame
1, scaffold) and OUT_BASENAME.s16; prints where to place each.
"""
import sys
import numpy as np
from PIL import Image
import s16

SS = 4
POLE = np.array([176, 80, 22], np.float32)
POLE_LIT = np.array([232, 136, 52], np.float32)
POLE_DARK = np.array([78, 30, 6], np.float32)
CORD = np.array([150, 152, 146], np.float32)
MATTE = np.array([40, 40, 36], np.float32)

# each piece: its box in world pixels, and what to draw (world coordinates)
PIECES = {
    'treehouse': {
        # (measured from an in-game shot: the last post leans a little left,
        # x 3888 at the top rail and 3882 at the beam; the top rail, y ~1701,
        # runs on to x 3940; the deck beam, y ~1757, stops at x ~3910)
        'box': (3876, 1686, 3946, 1772),
        'poles': [((3906, 1757), (3937, 1757), 5.0),        # the beam carried on
                  ((3887, 1727), (3934, 1727), 4.4),        # lower rail
                  ((3936, 1690), (3931, 1767), 7.5)],       # the new post
        'lashings': [(3935, 1701), (3933, 1727), (3931, 1757), (3886, 1727)],
        'dim': 0.92,
    },
    'scaffold': {
        # the last two posts lean right toward the top: at y 1972 they are at
        # x ~3807 and ~3829; a lower rail across them, as on the treehouse
        'box': (3798, 1962, 3840, 1983),
        'poles': [((3804, 1972), (3833, 1972), 4.4)],
        'lashings': [(3807, 1972), (3829, 1972)],
        'dim': 0.78,  # darker in the cave
    },
}


def draw(name):
    p = PIECES[name]
    x0, y0, x1, y1 = p['box']
    w, h = x1 - x0, y1 - y0
    rgb = np.zeros((h * SS, w * SS, 3), np.float32)
    rgb[:] = MATTE
    alpha = np.zeros((h * SS, w * SS), np.float32)
    ys, xs = np.mgrid[0:h * SS, 0:w * SS].astype(np.float32)
    wx = x0 + (xs + 0.5) / SS
    wy = y0 + (ys + 0.5) / SS

    def pole(a, b, width):
        (ax, ay), (bx, by) = a, b
        vx, vy = bx - ax, by - ay
        ln = max(np.hypot(vx, vy), 1e-3)
        t = np.clip(((wx - ax) * vx + (wy - ay) * vy) / ln ** 2, 0, 1)
        dx, dy = wx - (ax + t * vx), wy - (ay + t * vy)
        m = (np.hypot(dx, dy) <= width / 2).astype(np.float32)
        # rounded, lit from the upper left: across runs -1 (lit side) to 1
        nx, ny = -vy / ln, vx / ln
        if nx + ny > 0:
            nx, ny = -nx, -ny
        across = np.clip((dx * nx + dy * ny) / (width / 2), -1, 1)[..., None]
        col = np.where(across > 0.2, POLE_LIT + (POLE - POLE_LIT) * np.clip(1 - (across - 0.2) / 0.8, 0, 1) * 0 + POLE_LIT * 0,
                       0) if False else None
        lit = np.clip((across + 1) / 2, 0, 1)  # 1 on the lit side
        col = np.where(lit > 0.55, POLE + (POLE_LIT - POLE) * ((lit - 0.55) / 0.45), POLE_DARK + (POLE - POLE_DARK) * (lit / 0.55))
        along = t * ln
        grain = 1 + 0.05 * np.sin(along * 1.7 + across[..., 0] * 3)
        col = col * grain[..., None]
        rgb[:] = rgb * (1 - m[..., None]) + col * m[..., None]
        alpha[:] = np.maximum(alpha, m)

    def lashing(x, y, r=4.2):
        # a wrapped band of grey cord: a few turns, wider than the pole,
        # lit from the upper left
        dx, dy = (wx - x) / (r * 1.25), (wy - y) / r
        m = ((np.abs(dx) <= 1) & (np.abs(dy) <= 1)).astype(np.float32)
        round_ = np.sqrt(np.clip(1 - dx ** 2, 0, 1))
        turns = np.abs((dy * 1.6 + dx * 0.25) % 1.0 - 0.5) * 2
        light = np.clip(0.55 - 0.45 * dx - 0.2 * dy, 0, 1)
        col = CORD[None, None, :] * ((0.35 + 0.8 * light) * (0.55 + 0.45 * round_) * (1 - 0.4 * turns ** 2))[..., None]
        rgb[:] = rgb * (1 - m[..., None]) + col * m[..., None]
        alpha[:] = np.maximum(alpha, m)

    for a, b, wd in p['poles']:
        pole(a, b, wd)
    for lx, ly in p['lashings']:
        lashing(lx, ly)
    rgb *= p['dim']
    a = np.dstack([rgb, alpha[..., None] * 255]).reshape(h, SS, w, SS, 4).mean(axis=(1, 3))
    out = a.copy()
    out[..., 3] = np.where(a[..., 3] >= 128, 255, 0)
    return Image.fromarray(out.clip(0, 255).astype(np.uint8), 'RGBA'), (x0, y0)


if __name__ == '__main__':
    base = sys.argv[1]
    t, tpos = draw('treehouse')
    s, spos = draw('scaffold')
    t.save(base + '.png')
    s.save(base + '-1.png')
    s16.write(base + '.s16', [t, s])
    print(f'treehouse end (frame 0) at {tpos[0]},{tpos[1]}; scaffold end (frame 1) at {spos[0]},{spos[1]}')
    if len(sys.argv) >= 6 and sys.argv[2] == 'mockup':
        # mock-up on two in-game shots taken at zoom 1 (camera 3600,1500 and
        # 3550,1850, toolbar cropped): before and after, side by side, 2x
        out = []
        for img, pos, bg, cam in ((t, tpos, sys.argv[3], (3600, 1500)), (s, spos, sys.argv[4], (3550, 1850))):
            b = Image.open(bg).convert('RGBA')
            after = b.copy()
            after.alpha_composite(img, (pos[0] - cam[0], pos[1] - cam[1]))
            cx, cy = pos[0] - cam[0] + img.width // 2, pos[1] - cam[1] + img.height // 2
            box = (max(0, cx - 110), max(0, cy - 80), max(0, cx - 110) + 220, max(0, cy - 80) + 160)
            pair = Image.new('RGB', (900, 330), (20, 20, 20))
            pair.paste(b.crop(box).convert('RGB').resize((440, 320), Image.LANCZOS), (5, 5))
            pair.paste(after.crop(box).convert('RGB').resize((440, 320), Image.LANCZOS), (455, 5))
            out.append(pair)
        sheet = Image.new('RGB', (900, 660), (20, 20, 20))
        sheet.paste(out[0], (0, 0))
        sheet.paste(out[1], (0, 330))
        sheet.save(sys.argv[5])
        print('mock-up', sys.argv[5])
