"""Bamboo props under what is left of the cage lift's old top deck, by the
huts (owner, approved 2026-11-19: the deck is no longer walkable, and the
remains of it, the crane, its lamp and the double pulley's housing, need
holding up). A doubled beam under the remains, to just past the pulley's
housing, held by one V-shaped prop west of the lift's shaft (x 5106-5230),
as the platforms around here are held up: two upright posts, a V from their
tops to the middle of the lower tie, two ties across, a slim post down the
middle to the walkway, lashed where they meet. The east prop the owner had
taken away, and this one moved a little east, between the village's V props
and the braced tower tried first.

Drawn from scratch in the style of lowerdeck.py (its colors, poles with
nodes, lashings), with catwalk.py's dappled light; no game pixels are read
or copied, so the output ships with the project. tube_walk.py takes the old
deck's pieces out of the scene; this goes where they were. It stands behind
the village walkway's railings (separate scenery objects, in front by plane).

    python3 lift_deck_props.py [OUT_DIR]

writes OUT_DIR/ztdp.s16 (one frame) and OUT_DIR/ztdp.png (default OUT_DIR:
art/sprites) and prints the frame's world top left.
"""
import os
import sys
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import s16
from catwalk import _noise

SS = 4
DIM = 0.95
BAMBOO = np.array([196, 152, 58], np.float32) * DIM
BAMBOO_LIT = np.array([244, 206, 112], np.float32) * DIM
BAMBOO_DARK = np.array([112, 74, 18], np.float32) * DIM
LASH = np.array([206, 158, 92], np.float32) * DIM
SHEET = (4990, 1595, 5290, 1870)     # the drawing's world box
FOOT = 1858                          # where the posts stand, on the walkway


class Sheet:
    """A supersampled RGBA drawing over the world box x0..x1, y0..y1."""

    def __init__(self, x0, y0, x1, y1):
        self.x0, self.y0, self.w, self.h = x0, y0, x1 - x0, y1 - y0
        self.rgb = np.zeros((self.h * SS, self.w * SS, 3), np.float32)
        self.a = np.zeros((self.h * SS, self.w * SS), np.float32)
        ys, xs = np.mgrid[0:self.h * SS, 0:self.w * SS].astype(np.float32)
        self.wx = x0 + (xs + 0.5) / SS
        self.wy = y0 + (ys + 0.5) / SS

    def put(self, m, col):
        self.rgb[:] = self.rgb * (1 - m[..., None]) + col * m[..., None]
        self.a[:] = np.maximum(self.a, m)

    def pole(self, p, q, width, node_phase=9):
        """A bamboo pole from p to q: lit on its upper side, nodes every 30 px."""
        (ax, ay), (bx, by) = p, q
        wx, wy = self.wx, self.wy
        vx, vy = bx - ax, by - ay
        ln = max(np.hypot(vx, vy), 1e-3)
        t = np.clip(((wx - ax) * vx + (wy - ay) * vy) / ln ** 2, 0, 1)
        dx, dy = wx - (ax + t * vx), wy - (ay + t * vy)
        m = (np.hypot(dx, dy) <= width / 2).astype(np.float32)
        nx, ny = -vy / ln, vx / ln
        if ny > 0 or (ny == 0 and nx > 0):
            nx, ny = -nx, -ny
        across = np.clip((dx * nx + dy * ny) / (width / 2), -1, 1)[..., None]
        col = np.where(across > 0.25, BAMBOO + (BAMBOO_LIT - BAMBOO) * ((across - 0.25) / 0.75),
                       BAMBOO_DARK + (BAMBOO - BAMBOO_DARK) * ((across + 1) / 1.25))
        along = t * ln
        node = (np.abs(((along + node_phase) % 30) - 15) > 14.2)[..., None]
        col = np.where(node, col * 0.62, col)
        self.put(m, col)

    def lashing(self, x, y, r=2.4):
        """A rope lashing where poles meet: a lit, cross-wound knot."""
        wx, wy = self.wx, self.wy
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
        self.put(m, LASH[None, None, :] * shade[..., None])

    def result(self, dapple_seed=5):
        """At 1x: dappled light (as catwalk.py's), a little muted. Returns
        (rgb, opaque mask)."""
        h, w = self.h, self.w
        blobs = np.kron(_noise((h, w), dapple_seed, 7) + 0.6 * _noise((h, w), dapple_seed + 1, 3), np.ones((SS, SS)))
        sun = np.clip((blobs - 0.1) * 1.6, 0, 1)
        shade = np.clip((-blobs - 0.05) * 1.6, 0, 1)
        self.rgb *= (1 + 0.47 * sun - 0.52 * shade)[..., None]
        lum = (self.rgb * np.array([0.3, 0.59, 0.11], np.float32)).sum(axis=2, keepdims=True)
        rgb = (lum + (self.rgb - lum) * 0.85) * 0.97
        rgba = np.dstack([rgb, self.a[..., None] * 255]).reshape(h, SS, w, SS, 4).mean(axis=(1, 3))
        return rgba[..., :3], (rgba[..., 3] >= 128)


def draw():
    sh = Sheet(*SHEET)
    sh.pole((4998, 1613), (5224, 1613), 6.0)                   # the beam, doubled, to just past the pulley's housing
    sh.pole((5000, 1620), (5222, 1620), 5.0, 17)
    xl, xr = 5040, 5098                                        # the prop's two posts
    xm = (xl + xr) / 2
    y1, y2 = 1712, 1800                                        # its ties
    sh.pole((xl, 1623), (xl, FOOT), 4.6, 5)
    sh.pole((xr, 1623), (xr, FOOT), 4.6, 19)
    sh.pole((xl + 2, 1625), (xm - 1, y2), 4.0, 11)             # the V
    sh.pole((xr - 2, 1625), (xm + 1, y2), 4.0, 23)
    sh.pole((xm, y2), (xm, FOOT), 3.0, 7)                      # the middle post
    for y in (y1, y2):
        sh.pole((xl, y), (xr, y), 3.0)
        sh.lashing(xl, y, 2.2); sh.lashing(xr, y, 2.2)
    sh.lashing(xm, y2, 2.8)
    for x in (xl, xr):
        sh.lashing(x, 1627)
        sh.lashing(x, FOOT - 7, 3.0)
    return sh.result()


def sprite():
    """(RGBA image cropped to the drawing, its world top left)."""
    rgb, al = draw()
    ys, xs = np.nonzero(al)
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    rgba = np.dstack([rgb.clip(0, 255).astype(np.uint8), np.where(al, 255, 0).astype(np.uint8)])[y0:y1, x0:x1]
    return Image.fromarray(rgba, 'RGBA'), (SHEET[0] + int(x0), SHEET[1] + int(y0))


if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), 'sprites')
    im, at = sprite()
    im.save(os.path.join(out, 'ztdp.png'))
    s16.write(os.path.join(out, 'ztdp.s16'), [im])
    print(f'ztdp (frame 0) at {at[0]},{at[1]} ({im.width}x{im.height})')
