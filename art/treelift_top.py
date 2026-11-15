"""The treehouse cage lift's new top: its pulley moved up onto a cane arm
(owner, 2026-11-14: the lift should go one floor higher, to the upper hut's
deck; "the pulley sprite ... should continue up ... and some kind of overhang
sprite should be generated ... keep in mind what kind of sticks are already
in that area"; then "you could hang the pulley off of extending this area":
the flying fox's landing beam over the hut). Made on the player's own machine
from their installed Creatures 2 (bring-your-own-data: only this recipe is
part of the project; the pulley wheel and the patch over its old place are
the player's own background pixels).

Two pictures:
- ztlc: the background around the old pulley and its hook (under the upper
  deck, x 3554-3598, y 1562-1612) taken from just beside it, so the wheel is
  gone from there; drawn just above the background.
- ztla: an arm of the landing's orange-red cane (flyingfox.py's 'scaffold'
  tube) lashed onto the end of the landing's beam and carried out over the
  lift's shaft, a cord stay from its tip up to the flying fox's stub post, an
  iron hook under the tip, the pulley wheel hung on it, and the lift's two
  ropes from the wheel down to where the painted ropes go on (y 1612).

    python3 treelift_top.py C2_DIR USER_DATA_DIR

writes USER_DATA_DIR/Images/ztlc.png and ztla.png; tools/additions/
treelift-top.cos places them at (CX0, CY0) and (X0, Y0).
"""
import os, sys
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from background import region
import flyingfox as ff

# the painted pulley: wheel centre, radius; its hook from the deck's underside
WHEEL_C, WHEEL_R = (3576.0, 1595.0), 15.0
ROPE_X = (3564.0, 3588.0)            # the two painted ropes below the wheel
ROPE_JOIN = 1612.0                   # where the painted ropes go on
CABLE_SRC = (1618, 1738)             # a clean stretch of the painted cables, repeated above
CABLE_LIFT = (1.15, 1.0)             # west, east: brightness of the extension
DECK_ROWS = (1540, 1567)             # the upper deck's planks and logs: the cables pass behind them
# the patch over the old wheel and hook
CX0, CY0, CX1, CY1 = 3554, 1562, 3598, 1612
PATCH_DX = -46                       # taken from this far beside it

# the new top
X0, Y0, X1, Y1 = 3440, 1120, 3620, 1616
ARM = (3456.0, 3606.0, 1294.0)       # from, to (x), at the landing beam's height
NEW_C = (3576.0, 1326.0)             # the wheel's new centre


def wheel_image(c2):
    """The painted wheel, cut out (bronze rim, spokes, hub) from the player's
    background."""
    x0, y0 = int(WHEEL_C[0] - WHEEL_R - 2), int(WHEEL_C[1] - WHEEL_R - 2)
    x1, y1 = int(WHEEL_C[0] + WHEEL_R + 3), int(WHEEL_C[1] + WHEEL_R + 3)
    bg = np.asarray(region(c2, x0, y0, x1, y1).convert('RGB')).astype(np.float32)
    h, w = bg.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    d = np.hypot(xx + x0 + 0.5 - WHEEL_C[0], yy + y0 + 0.5 - WHEEL_C[1])
    r, g, b = bg[..., 0], bg[..., 1], bg[..., 2]
    bronze = (r > g * 1.02) & (r - b > 18)
    a = np.where((d <= WHEEL_R - 0.5) & (bronze | (d < 4.5)), 255.0, 0.0)
    a = np.where((d > WHEEL_R - 0.5) & (d <= WHEEL_R + 0.8) & bronze, 160.0, a)
    return Image.fromarray(np.dstack([bg, a]).astype(np.uint8), 'RGBA'), (x0, y0)


def cover(c2):
    src = region(c2, CX0 + PATCH_DX, CY0, CX1 + PATCH_DX, CY1).convert('RGB')
    arr = np.asarray(src).astype(np.float32)
    h, w = arr.shape[:2]
    # soft left and right edges, so the patch blends into what is beside it
    xx = np.arange(w, dtype=np.float32)
    edge = np.clip(np.minimum(xx, w - 1 - xx) / 5.0, 0, 1)
    a = np.tile(edge * 255, (h, 1))
    return Image.fromarray(np.dstack([arr, a]).astype(np.uint8), 'RGBA')


def art(c2):
    cv = ff.Canvas(X0, Y0, X1 - X0, Y1 - Y0)
    ax0, ax1, ay = ARM
    # the stay: cord from the arm's tip up to the flying fox's stub post
    post_top = ff.line_y(ff.BOT_POST_X + 2) - 6
    ff.tube(cv, (ax1 - 4, ay - 2), (ff.BOT_POST_X + 3, post_top + 8), 2.0, 'bluecord', 61)
    # the arm: the landing's cane, lashed on where it overlaps the beam's end
    ff.tube(cv, (ax0, ay + 0.5), (ax1, ay - 1.5), 5.4, 'scaffold', 62)
    ff.wrap_lashing(cv, ax0 + 10, ay + 0.2, 7.0, 1.5708, 63, 4)
    ff.wrap_lashing(cv, ff.LANDING_X1 - 2, ay - 0.3, 7.0, 1.5708, 64, 4)
    ff.wrap_lashing(cv, ax1 - 5, ay - 1.4, 6.5, 1.5708, 65, 3)
    # the hook: an iron strap from a lashing round the arm to the wheel's hub
    hx, hy = NEW_C
    ff.tube(cv, (hx, ay + 1), (hx, hy), 2.6, 'greycord', 66)
    ff.wrap_lashing(cv, hx, ay - 1.0, 6.5, 1.5708, 67, 3)
    img = cv.image(dapple_seed=73, dapple=0.12, mute=0.9, dim=0.97)
    # the lift's two cables, from the wheel down to where the painted ones go
    # on: the painted cables' own pixels (owner, 2026-11-14: the drawn rope
    # "doesn't match the rope in the original section"), a clean stretch of
    # each repeated upward, only its gold (not the leaves behind it)
    hx, hy = NEW_C
    for cx, lift in zip(ROPE_X, CABLE_LIFT):
        x0 = int(cx) - 4
        strip = np.asarray(region(c2, x0, CABLE_SRC[0], x0 + 9, CABLE_SRC[1]).convert('RGB')).astype(np.float32)
        r, g, b = strip[..., 0], strip[..., 1], strip[..., 2]
        gold = np.clip((r - b - 45) / 30.0, 0, 1) * (r > g * 0.95)
        # a clean cable from the painted one's cross-section (owner,
        # 2026-11-14: repeated stretches "look scattered, rather than like a
        # solid line", leaves crossing the painted cable leaving gaps): each
        # column across it gets the painted cable's usual colour there (the
        # median of its gold pixels) and a solidity from how often that
        # column is gold; a faint shading along it keeps it from looking flat
        n = strip.shape[0]
        prof = np.zeros((9, 3), np.float32)
        solid = np.zeros(9, np.float32)
        for c in range(9):
            on = gold[:, c] > 0.5
            frac = on.mean()
            if on.any():
                prof[c] = np.median(strip[on, c], axis=0)
            solid[c] = np.clip((frac - 0.25) / 0.45, 0, 1)
        # (the west cable a little lighter: owner, 2026-11-14, its extension
        # read darker than the painted cable)
        prof = np.clip(prof * lift, 0, 255)
        y_top, y_bot = int(hy + 9), int(ROPE_JOIN + 4)
        L = y_bot - y_top
        cable = np.zeros((L, 9, 4), np.float32)
        along = 1.0 + 0.04 * np.sin(np.arange(L) / 7.3) + 0.03 * np.sin(np.arange(L) / 2.9 + 1.0)
        for k in range(L):
            y = y_top + k
            cable[k, :, :3] = np.clip(prof * along[k], 0, 255)
            a = solid * 255.0
            if DECK_ROWS[0] <= y <= DECK_ROWS[1]:
                a = a * 0.0          # behind the upper deck's planks and logs
            cable[k, :, 3] = a
        for k in range(4):  # ease into the painted cable below
            cable[-1 - k, :, 3] *= (k + 1) / 5.0
        cim = Image.fromarray(cable.astype(np.uint8), 'RGBA')
        layer = Image.new('RGBA', img.size, (0, 0, 0, 0))
        layer.paste(cim, (x0 - X0, y_top - Y0))
        img = Image.alpha_composite(img, layer)
    wheel, (wx0, wy0) = wheel_image(c2)
    dx, dy = NEW_C[0] - WHEEL_C[0], NEW_C[1] - WHEEL_C[1]
    img.alpha_composite(wheel, (int(round(wx0 + dx - X0)), int(round(wy0 + dy - Y0))))
    return img


def save(img, path):
    from PIL import PngImagePlugin
    info = PngImagePlugin.PngInfo()
    info.add_text('openc2e-frames', '1')
    img.save(path, pnginfo=info)


if __name__ == '__main__':
    c2, user = sys.argv[1], sys.argv[2]
    out = os.path.join(user, 'Images')
    os.makedirs(out, exist_ok=True)
    save(cover(c2), os.path.join(out, 'ztlc.png'))
    save(art(c2), os.path.join(out, 'ztla.png'))
    print('ztlc at', (CX0, CY0), 'ztla at', (X0, Y0))
