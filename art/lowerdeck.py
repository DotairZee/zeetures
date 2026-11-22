"""Draw an original extension of the bamboo deck under the lower hut in the
middle caves (owner, 2026-11-10: extend it right, under the bamboo lift, so
the lift can come down to it).

The deck there is a thin golden bamboo beam (its top the walking line,
y 2077) ending at about x 5127, with an X-braced railing box at x 5032-5098.
This continues the beam to x 5356 with a lower rail and lashed supports,
leaves the stretch under the lift open, and closes the new end with an
X-braced box like the old one.

Everything here is drawn from scratch (no game pixels are read or copied),
so the output can ship with the project. Colors are numbers chosen by eye
(the golden bamboo of our walkways, art/bamboo.py, a little
dimmer for the cave).

    python3 lowerdeck.py OUT_BASENAME

writes OUT_BASENAME.png, OUT_BASENAME-ropes.png and OUT_BASENAME.s16 (frame 0
the deck, placed at world x 5120, y 2024; frame 1 the lift's ropes and a
pulley, at x 5146, y 1858; see treelift.cos).
"""
import sys
import numpy as np
from PIL import Image, ImageDraw
import s16
from catwalk import _noise

SS = 4
X0, Y0 = 5120, 2024
W, H = 240, 68              # x 5120-5360, y 2024-2092
DECK = 2077                 # the walking line (top of the beam)
END = 5356                  # the new end of the deck
FRAME = (5290, END)         # the end box, as wide as the old one (66 px)
FRAME_TOP = 2028
SUPPORTS = [5160, 5250]     # lashed uprights under the beam

DIM = 0.95
BAMBOO = np.array([196, 152, 58], np.float32) * DIM
BAMBOO_LIT = np.array([244, 206, 112], np.float32) * DIM
BAMBOO_DARK = np.array([112, 74, 18], np.float32) * DIM
ROPE = np.array([110, 66, 30], np.float32) * DIM
LASH = np.array([206, 158, 92], np.float32) * DIM  # lashing cord, light tan (as the walkways' knots)
PLANK_LIT = np.array([214, 132, 40], np.float32) * DIM
PLANK_DARK = np.array([112, 52, 12], np.float32) * DIM
DAPPLE = 0.30  # patches of light and shade, as on the hut deck
MUTE = 0.68  # how much colour is kept (1 = all)
MATTE = np.array([52, 60, 52], np.float32)  # the dark green-gray behind


def draw():
    w4, h4 = W * SS, H * SS
    rgb = np.zeros((h4, w4, 3), np.float32)
    rgb[:] = MATTE
    alpha = np.zeros((h4, w4), np.float32)

    def px(x, y):
        return ((x - X0) * SS, (y - Y0) * SS)

    def pole(p, q, width, lengthwise=True):
        """A bamboo pole from p to q (world px), lit along its upper side,
        with node rings every so often."""
        layer = Image.new('L', (w4, h4), 0)
        d = ImageDraw.Draw(layer)
        d.line([px(*p), px(*q)], fill=255, width=int(width * SS))
        for e in (p, q):
            cx, cy = px(*e)
            r = width * SS / 2
            d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=255)
        m = np.asarray(layer, np.float32) / 255
        # shading across the pole: lit toward its top/left edge
        (ax, ay), (bx, by) = px(*p), px(*q)
        vx, vy = bx - ax, by - ay
        ln = max(np.hypot(vx, vy), 1)
        nx, ny = -vy / ln, vx / ln
        if ny > 0 or (ny == 0 and nx > 0):
            nx, ny = -nx, -ny  # the normal pointing up (or left)
        ys, xs = np.mgrid[0:h4, 0:w4].astype(np.float32)
        across = ((xs - ax) * nx + (ys - ay) * ny) / (width * SS / 2)  # -1 .. 1, +1 lit side
        along = ((xs - ax) * vx + (ys - ay) * vy) / ln / SS  # world px along
        t = np.clip(across, -1, 1)
        col = np.where(t[..., None] > 0.25,
                       BAMBOO + (BAMBOO_LIT - BAMBOO) * ((t[..., None] - 0.25) / 0.75),
                       BAMBOO_DARK + (BAMBOO - BAMBOO_DARK) * ((t[..., None] + 1) / 1.25))
        # node rings
        node = (np.abs(((along + 7) % 26) - 13) > 12.2)
        col = np.where(node[..., None], col * 0.62, col)
        grain = _noise((h4 // SS + 1, w4 // SS + 1), 41, 2)
        g = np.asarray(Image.fromarray(((grain + 1) * 127.5).astype(np.uint8)).resize((w4 + SS, h4 + SS), Image.NEAREST), np.float32)[:h4, :w4] / 127.5 - 1
        col = col * (1 + 0.08 * g[..., None])
        rgb[:] = rgb * (1 - m[..., None]) + col * m[..., None]
        alpha[:] = np.maximum(alpha, m)

    def lashing(x, y, r=2.6):
        """Cord lashed round a joint (owner, 2026-11-10: tie it together like
        the walkways' own): a rounded bundle of turns, crossed both ways over
        the joint, lit from the upper left, each turn of cord a raised strand
        with a dark groove between, a darker rim where it wraps under."""
        r = r * 1.6
        ys, xs = np.mgrid[0:h4, 0:w4].astype(np.float32)
        cx, cy = px(x, y)
        dx, dy = (xs - cx) / (r * SS), (ys - cy) / (r * SS * 0.85)
        rr = np.sqrt(dx * dx + dy * dy)
        m = np.clip((1 - rr) * r * SS * 0.5, 0, 1)
        nz = np.sqrt(np.clip(1 - rr ** 2, 0, 1))
        light = np.clip(-0.55 * dx - 0.6 * dy + 0.6 * nz, 0, 1.2)
        # turns crossing each way (an X lashing), the uppermost set on top
        a = np.abs(((dx + dy) * 3.2) % 1.0 - 0.5) * 2
        b = np.abs(((dx - dy) * 3.2) % 1.0 - 0.5) * 2
        strand = np.where((dx - dy) > 0, a, b)
        shade = (0.35 + 0.85 * light) * (1 - 0.55 * strand ** 2) * (1 - 0.35 * np.clip(rr - 0.7, 0, 1) / 0.3)
        col = LASH[None, None, :] * shade[..., None]
        rgb[:] = rgb * (1 - m[..., None]) + col * m[..., None]
        alpha[:] = np.maximum(alpha, m)

    def planks():
        """The deck: a row of thick planks like the hut deck's (dark orange
        faces under a bright gold top edge, darker gaps between, each plank
        a little lighter or darker than the next)."""
        rng = np.random.default_rng(52)
        x = X0 + 2.0
        ys, xs = np.mgrid[0:h4, 0:w4].astype(np.float32)
        wy = Y0 + (ys + 0.5) / SS
        wx = X0 + (xs + 0.5) / SS
        top, bot = DECK - 0.5, DECK + 4.0
        while x < END:
            ln = rng.uniform(13, 20)
            x1 = min(x + ln, END)
            m = ((wx >= x + 0.6) & (wx <= x1 - 0.6) & (wy >= top) & (wy <= bot)).astype(np.float32)
            gap = ((wx >= x) & (wx <= x1) & (wy >= top) & (wy <= bot)).astype(np.float32)
            t = np.clip((wy - top) / (bot - top), 0, 1)
            face = PLANK_LIT + (PLANK_DARK - PLANK_LIT) * np.clip(t[..., None] * 1.6, 0, 1)
            rim = np.clip(1 - (wy - top) / 1.6, 0, 1)[..., None]
            face = face * (1 - rim) + BAMBOO_LIT * rim
            face = face * rng.uniform(0.82, 1.1)
            col = np.where(m[..., None] > 0, face, PLANK_DARK * 0.45)  # the gaps
            rgb[:] = rgb * (1 - gap[..., None]) + col * gap[..., None]
            alpha[:] = np.maximum(alpha, gap)
            x = x1

    # supports first (behind the beams), down to a lower rail
    for sx in SUPPORTS:
        pole((sx, DECK + 2), (sx, DECK + 10), 2.6)
    pole((X0 + 4, DECK + 7), (END - 2, DECK + 7), 2.0)       # lower rail
    planks()                                                 # the deck
    for sx in SUPPORTS:
        lashing(sx, DECK + 3)
        lashing(sx, DECK + 7, 1.8)
    # the end box: two uprights, a top rail, an X, lashed at the joints
    fx0, fx1 = FRAME
    pole((fx0 + 2, FRAME_TOP + 2), (fx1 - 3, DECK - 2), 3.4)
    pole((fx1 - 3, FRAME_TOP + 2), (fx0 + 2, DECK - 2), 3.4)
    pole((fx0, FRAME_TOP), (fx0, DECK), 4.2)
    pole((fx1 - 1, FRAME_TOP), (fx1 - 1, DECK), 4.2)
    pole((fx0, FRAME_TOP), (fx1 - 1, FRAME_TOP), 4.2)
    mx, my = (fx0 + fx1) / 2, (FRAME_TOP + DECK) / 2
    for (lx, ly) in [(fx0, FRAME_TOP), (fx1 - 1, FRAME_TOP), (mx, my), (fx0, DECK), (fx1 - 1, DECK)]:
        lashing(lx, ly)

    dapple(rgb, (H, W), 61)
    # muted (owner: tone the colouring down a bit): toward gray, a touch darker
    lum = (rgb * np.array([0.3, 0.59, 0.11], np.float32)).sum(axis=2, keepdims=True)
    rgb[:] = (lum + (rgb - lum) * MUTE) * 0.92
    a = np.dstack([rgb, alpha[..., None] * 255]).reshape(H, SS, W, SS, 4).mean(axis=(1, 3))
    out = a.copy()
    out[..., 3] = np.where(a[..., 3] >= 128, 255, 0)
    return Image.fromarray(out.clip(0, 255).astype(np.uint8), 'RGBA')


def dapple(rgb, shape, seed):
    """Dappled light: soft patches of sun and shade over everything drawn
    (as in art/bamboo.py, stronger here to match the hut deck)."""
    h, w = shape
    blobs = np.kron(_noise((h, w), seed, 7) + 0.6 * _noise((h, w), seed + 1, 3), np.ones((SS, SS)))
    sun = np.clip((blobs - 0.1) * 1.6, 0, 1)
    shade = np.clip((-blobs - 0.05) * 1.6, 0, 1)
    rgb *= (1 + 0.5 * DAPPLE / 0.32 * sun - 0.55 * DAPPLE / 0.32 * shade)[..., None]


# The lift's ropes, carried down from the pulley on the walkway (x 5160 and
# 5181, ending at y ~1856) to a new pulley on the new deck, where the cage's
# lowest stop now is. Its own picture (frame 1), plane -21: behind the cage.
RX0, RY0 = 5146, 1858
RW, RH = 50, 222            # x 5146-5196, y 1858-2080
ROPES = (5160.5, 5181.5)
WHEEL = (5171, 2059, 10.5)  # centre and radius: the ropes meet it at its sides
ROPE_COL = np.array([178, 136, 82], np.float32)
STEEL = np.array([150, 160, 160], np.float32)
STEEL_DARK = np.array([52, 58, 60], np.float32)
IRON = np.array([40, 38, 34], np.float32)


def draw_ropes():
    w4, h4 = RW * SS, RH * SS
    rgb = np.zeros((h4, w4, 3), np.float32)
    rgb[:] = MATTE
    alpha = np.zeros((h4, w4), np.float32)
    ys, xs = np.mgrid[0:h4, 0:w4].astype(np.float32)
    wx = RX0 + (xs + 0.5) / SS
    wy = RY0 + (ys + 0.5) / SS
    cx, cy, r = WHEEL
    # the ropes: twisted (diagonal bands), lit on the left
    for rx in ROPES:
        m = ((np.abs(wx - rx) <= 1.5) & (wy <= cy)).astype(np.float32)
        twist = (np.sin((wy * 1.9 + wx * 1.0)) > 0.2)
        col = ROPE_COL * np.where(twist, 1.0, 0.72)[..., None] * (1.08 - 0.25 * np.clip((wx - rx + 1.5) / 3, 0, 1))[..., None]
        rgb[:] = rgb * (1 - m[..., None]) + col * m[..., None]
        alpha[:] = np.maximum(alpha, m)
    # a bracket under the wheel down to the deck (dark iron), drawn first
    br = ((np.abs(wx - cx) <= 3.5) & (wy >= cy) & (wy <= 2077)).astype(np.float32)
    foot = ((np.abs(wx - cx) <= 8) & (wy >= 2071) & (wy <= 2077)).astype(np.float32)
    m = np.maximum(br, foot)
    col = IRON * (1.2 - 0.4 * np.clip((wx - cx + 8) / 16, 0, 1))[..., None]
    rgb[:] = rgb * (1 - m[..., None]) + col * m[..., None]
    alpha[:] = np.maximum(alpha, m)
    # the wheel: a steel rim with a groove, spokes and a hub, lit up-left
    d = np.hypot(wx - cx, wy - cy)
    rim = ((d <= r) & (d >= r - 3.2)).astype(np.float32)
    groove = ((d <= r - 1.2) & (d >= r - 2.0)).astype(np.float32)
    ang = np.arctan2(wy - cy, wx - cx)
    spokes = ((d < r - 3) & (np.abs(np.sin(ang * 3)) < 0.22)).astype(np.float32)
    hub = (d <= 3.0).astype(np.float32)
    light = np.clip(0.5 - 0.5 * ((wx - cx) + (wy - cy)) / (r * 1.4), 0, 1)[..., None]
    steel = STEEL_DARK + (STEEL - STEEL_DARK) * light
    for m, col in ((rim, steel), (spokes, steel * 0.8), (hub, steel * 1.1)):
        rgb[:] = rgb * (1 - m[..., None]) + col * m[..., None]
        alpha[:] = np.maximum(alpha, m)
    rgb[:] = rgb * (1 - groove[..., None]) + (STEEL_DARK * 0.6) * groove[..., None]
    dapple(rgb, (RH, RW), 71)
    a = np.dstack([rgb, alpha[..., None] * 255]).reshape(RH, SS, RW, SS, 4).mean(axis=(1, 3))
    out = a.copy()
    out[..., 3] = np.where(a[..., 3] >= 128, 255, 0)
    return Image.fromarray(out.clip(0, 255).astype(np.uint8), 'RGBA')


# A support post under the deck's right end, down to the plank boardwalk
# over the pool (its top at y ~2294), with a brace up to the deck (owner:
# without one the deck seemed to float). Frame 2, plane -22.
PX0, PY0 = 5292, 2080
PW, PH = 62, 216            # x 5292-5354, y 2080-2296
POST_X, POST_FOOT = 5342.5, 2295
BRACE = ((5342.5, 2140), (5300, 2084))


def draw_post():
    w4, h4 = PW * SS, PH * SS
    rgb = np.zeros((h4, w4, 3), np.float32)
    rgb[:] = MATTE
    alpha = np.zeros((h4, w4), np.float32)
    ys, xs = np.mgrid[0:h4, 0:w4].astype(np.float32)
    wx = PX0 + (xs + 0.5) / SS
    wy = PY0 + (ys + 0.5) / SS

    def pole(p, q, width):
        (ax, ay), (bx, by) = p, q
        vx, vy = bx - ax, by - ay
        ln = max(np.hypot(vx, vy), 1e-3)
        t = np.clip(((wx - ax) * vx + (wy - ay) * vy) / ln ** 2, 0, 1)
        dx, dy = wx - (ax + t * vx), wy - (ay + t * vy)
        dist = np.hypot(dx, dy)
        m = (dist <= width / 2).astype(np.float32)
        nx, ny = -vy / ln, vx / ln
        if ny > 0 or (ny == 0 and nx > 0):
            nx, ny = -nx, -ny
        across = np.clip((dx * nx + dy * ny) / (width / 2), -1, 1)[..., None]
        col = np.where(across > 0.25, BAMBOO + (BAMBOO_LIT - BAMBOO) * ((across - 0.25) / 0.75),
                       BAMBOO_DARK + (BAMBOO - BAMBOO_DARK) * ((across + 1) / 1.25))
        along = t * ln
        node = (np.abs(((along + 9) % 30) - 15) > 14.2)[..., None]
        col = np.where(node, col * 0.62, col)
        rgb[:] = rgb * (1 - m[..., None]) + col * m[..., None]
        alpha[:] = np.maximum(alpha, m)

    def lashing(x, y, r=2.4):
        # as the deck's (draw): a rounded bundle of crossed turns of cord
        r = r * 1.6
        dx, dy = (wx - x) / r, (wy - y) / (r * 0.85)
        rr = np.sqrt(dx * dx + dy * dy)
        m = np.clip((1 - rr) * r * 2, 0, 1)
        nz = np.sqrt(np.clip(1 - rr ** 2, 0, 1))
        light = np.clip(-0.55 * dx - 0.6 * dy + 0.6 * nz, 0, 1.2)
        a = np.abs(((dx + dy) * 3.2) % 1.0 - 0.5) * 2
        b = np.abs(((dx - dy) * 3.2) % 1.0 - 0.5) * 2
        strand = np.where((dx - dy) > 0, a, b)
        shade = (0.35 + 0.85 * light) * (1 - 0.55 * strand ** 2) * (1 - 0.35 * np.clip(rr - 0.7, 0, 1) / 0.3)
        col = LASH[None, None, :] * shade[..., None]
        rgb[:] = rgb * (1 - m[..., None]) + col * m[..., None]
        alpha[:] = np.maximum(alpha, m)

    pole(BRACE[0], BRACE[1], 2.6)
    pole((POST_X, PY0 + 1), (POST_X, POST_FOOT), 4.2)
    lashing(POST_X, PY0 + 4)
    lashing(*BRACE[0])
    lashing(BRACE[1][0], BRACE[1][1] + 1, 2.0)
    dapple(rgb, (PH, PW), 81)
    lum = (rgb * np.array([0.3, 0.59, 0.11], np.float32)).sum(axis=2, keepdims=True)
    rgb[:] = (lum + (rgb - lum) * MUTE) * 0.92
    # deeper down it is in the pool's mist: dimmer and a little green
    fade = np.clip((wy - 2150) / 140, 0, 1)[..., None]
    rgb[:] = rgb * (1 - 0.35 * fade) + np.array([40, 60, 48], np.float32) * 0.35 * fade
    a = np.dstack([rgb, alpha[..., None] * 255]).reshape(PH, SS, PW, SS, 4).mean(axis=(1, 3))
    out = a.copy()
    out[..., 3] = np.where(a[..., 3] >= 128, 255, 0)
    return Image.fromarray(out.clip(0, 255).astype(np.uint8), 'RGBA')


if __name__ == '__main__':
    base = sys.argv[1]
    im = draw()
    ropes = draw_ropes()
    post = draw_post()
    im.save(base + '.png')
    ropes.save(base + '-ropes.png')
    post.save(base + '-post.png')
    s16.write(base + '.s16', [im, ropes, post])
    print(f'deck (frame 0) at {X0},{Y0} ({W}x{H}); ropes and pulley (frame 1) at {RX0},{RY0} ({RW}x{RH}); post (frame 2) at {PX0},{PY0} ({PW}x{PH})')
