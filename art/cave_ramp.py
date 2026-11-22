"""Planks sloping from the cave lift's dock down into the stream (owner,
2026-11-14: "rather than a dock ... a ramp down into that water where
creatures can wade ... the wooden planks essentially go into the earth under
the water"; then "really look at the dock and model it after that"). Made
on the player's own machine from their installed Creatures 2
(bring-your-own-data: only this recipe is part of the project, never the
game's pixels).

The ramp is built the way the dock is: chunky deck boards with rounded ends
and dark gaps, on a weathered grey log (the stringer). Its boards and its
log are the dock's own, cut from the player's background (only the boards'
and the log's pixels: none of the rock, rope, cables or water around them),
laid end to end along the slope (about 17 degrees) starting where the dock's
last board ends (x 2329), so the deck and its log run on unbroken. Under the
water they take on the water's own colour, more with depth, and the water's
surface line crosses them; where the water meets the earth of the bed they
stop, so they run into it.

    python3 cave_ramp.py C2_DIR USER_DATA_DIR

writes USER_DATA_DIR/Images/zcrp.png; additions/water-pocket.cos
places it at world X0, Y0.
"""
import math, os, sys
import numpy as np
from PIL import Image, PngImagePlugin
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from background import region

X0, Y0, X1, Y1 = 2300, 2290, 2600, 2410
SLOPE = 0.30                     # px down per px east (about 17 degrees)
START = (2332, 2315)             # just past the dock's last board (it ends at 2329), a gap as between its boards
DECK_ROWS = (2315, 2326)         # the dock's boards, top to bottom
LOG_ROWS = (2326, 2335)          # its log (the stringer) under them
BOARDS = [(2114, 2137), (2189, 2218), (2297, 2329), (2085, 2113)]  # clean boards (no rope, cables)
LOG_SPAN = (2190, 2295)          # a clean stretch of the log
GAP = 3                          # between boards, as on the dock
LENGTH = 260                     # along the slope (the bed hides the rest)


def board_tiles(c2):
    """The dock's boards, each with its own alpha (the dark gaps and the
    rock above its rounded corners left out)."""
    tiles = []
    for x0, x1 in BOARDS:
        im = np.asarray(region(c2, x0, DECK_ROWS[0], x1, DECK_ROWS[1]).convert('RGB')).astype(np.float32)
        lum = im.mean(2)
        r, g, b = im[..., 0], im[..., 1], im[..., 2]
        wood = (r - b > 14) | (lum < 60)          # the board's wood, lit or in shade
        a = np.where(wood, 255.0, 0.0)
        a[lum < 12] = 0.0                         # the gaps' near-black
        # the top row: only the lit wood (above it is rock of the same hue)
        a[0] = np.where((r[0] - b[0] > 30) & (lum[0] > 50), a[0], 0)
        tiles.append(np.dstack([im, a]))
    return tiles


def log_tile(c2):
    im = np.asarray(region(c2, LOG_SPAN[0], LOG_ROWS[0], LOG_SPAN[1], LOG_ROWS[1]).convert('RGB')).astype(np.float32)
    a = np.full(im.shape[:2], 255.0)
    return np.dstack([im, a])


def strip(c2):
    """The ramp laid out flat: boards on top, the log under them."""
    boards = board_tiles(c2)
    log = log_tile(c2)
    bh = DECK_ROWS[1] - DECK_ROWS[0]
    lh = LOG_ROWS[1] - LOG_ROWS[0]
    H = bh + lh
    out = np.zeros((H, LENGTH, 4), np.float32)
    # the log, end to end (each stretch mirrored every other time, so the
    # grain does not visibly repeat)
    x, k = 0, 0
    while x < LENGTH:
        t = log if k % 2 == 0 else log[:, ::-1]
        n = min(t.shape[1], LENGTH - x)
        out[bh:bh + lh, x:x + n] = t[:, :n]
        x += n
        k += 1
    # the boards, with the dock's dark gaps between them
    x, k = 0, 0
    while x < LENGTH:
        t = boards[k % len(boards)]
        if (k // len(boards)) % 2:
            t = t[:, ::-1]
        n = min(t.shape[1], LENGTH - x)
        seg = t[:, :n]
        m = seg[..., 3:4] / 255.0
        out[:bh, x:x + n, :3] = out[:bh, x:x + n, :3] * (1 - m) + seg[..., :3] * m
        out[:bh, x:x + n, 3] = np.maximum(out[:bh, x:x + n, 3], seg[..., 3])
        x += n
        g = min(GAP, LENGTH - x)
        if g > 0:  # the gap: dark wood in shadow, as on the dock
            out[2:bh, x:x + g, :3] = 14.0
            out[2:bh, x:x + g, 3] = 255.0
        x += GAP
        k += 1
    return out


def make(c2):
    bg = np.asarray(region(c2, X0, Y0, X1, Y1).convert('RGB')).astype(np.float32)
    h, w = bg.shape[:2]
    flat = strip(c2)
    # tilt it: rotate about its top-left corner, down to the east
    ang = math.degrees(math.atan(SLOPE))
    im = Image.fromarray(np.clip(flat, 0, 255).astype(np.uint8), 'RGBA')
    big = im.resize((im.width * 4, im.height * 4), Image.LANCZOS)
    rot = big.rotate(-ang, resample=Image.BICUBIC, expand=True)
    rot = rot.resize((max(1, rot.width // 4), max(1, rot.height // 4)), Image.LANCZOS)
    # where the strip's top-left corner went in the rotated picture
    th = math.radians(ang)
    fh = flat.shape[0]
    ox = fh * math.sin(th)        # expand put the corner this far right
    canvas = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    canvas.alpha_composite(rot, (int(round(START[0] - X0 - ox)), int(round(START[1] - Y0))))
    c = np.asarray(canvas).astype(np.float32)
    rgb, alpha = c[..., :3], c[..., 3]
    # the stream: its water, its surface line, the bed below it
    r, g, b = bg[..., 0], bg[..., 1], bg[..., 2]
    yy = np.arange(h)[:, None] + Y0
    water = (g >= r) & (b >= r * 0.8) & (bg.sum(2) > 30) & (b < 140) & (yy < 2396)
    surface = np.full(w, Y1, np.float32)
    for x in range(w):
        rows = np.nonzero(water[:, x])[0]
        if len(rows):
            surface[x] = Y0 + rows[0]
    above = yy < surface[None, :]
    # only above the water (where it leaves the dock) or in it: into the bed it vanishes
    alpha = alpha * (above | water)
    # under the water: the water's own colour, more with depth
    depth = np.clip((yy - surface[None, :]) / 40.0, 0.0, 1.0)
    under = (~above).astype(np.float32)
    mix = (0.22 + 0.48 * depth) * under
    rgb = rgb * (1 - mix[..., None]) + bg * mix[..., None]
    # the surface line crosses the planks: the water's own top rows over them
    line = ((yy - surface[None, :]) >= 0) & ((yy - surface[None, :]) < 2)
    rgb = np.where(line[..., None], rgb * 0.35 + bg * 0.65, rgb)
    out = np.dstack([np.clip(rgb, 0, 255), np.clip(alpha, 0, 255)]).astype(np.uint8)
    return Image.fromarray(out, 'RGBA')


def save(img, path, frames=1):
    info = PngImagePlugin.PngInfo()
    info.add_text('openc2e-frames', str(frames))
    img.save(path, pnginfo=info)


def main():
    c2, user = sys.argv[1], sys.argv[2]
    out = os.path.join(user, 'Images')
    os.makedirs(out, exist_ok=True)
    img = make(c2)
    save(img, os.path.join(out, 'zcrp.png'))
    print('zcrp', img.size, 'top-left', X0, Y0)


if __name__ == '__main__':
    main()
