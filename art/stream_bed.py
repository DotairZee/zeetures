"""The stream bed's earth, drawn in front of creatures (owner, 2026-11-14:
creatures carried along the stream under the waterwheel poked out below the
ground; "I need the sprite for the ground to go all the way down to the
bottom of the screen"). Made on the player's own machine from their installed
Creatures 2 (bring-your-own-data: only this recipe is part of the project).

From the cave lift's dock to past the waterwheel (x X0-X1) the world's
picture ends a little below the stream's bed, and below it the view is
black. This takes the player's own earth from just under the stream's water
down to the picture's foot, and carries it on further down (ground drawn from
bands of that earth's own bottom rows, darker with depth), so creatures drawn a
little below the bed are covered by ground, not shown against black. Only
earth is taken: the water, the dock, the wheel and anything above the bed
stay as they are.

    python3 stream_bed.py C2_DIR USER_DATA_DIR

writes USER_DATA_DIR/Images/zsbd.png; tools/additions/water-pocket.cos
places it at world X0, Y0.
"""
import os, sys
import numpy as np
from PIL import Image, PngImagePlugin
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from background import region

X0, Y0, X1 = 2200, 2340, 3080   # from the dock to past the wheel; top of the search
FOOT = 2400                     # the world's picture ends here
EXTRA = 70                      # earth carried on below it


def make(c2):
    bg = np.asarray(region(c2, X0, Y0, X1, FOOT).convert('RGB')).astype(np.float32)
    h, w = bg.shape[:2]
    r, g, b = bg[..., 0], bg[..., 1], bg[..., 2]
    water = (g >= r) & (b >= r * 0.8) & (bg.sum(2) > 30) & (b < 140)
    out = np.zeros((h + EXTRA, w, 4), np.float32)
    tops = np.full(w, h, int)
    for x in range(w):
        rows = np.nonzero(water[:, x])[0]
        # the earth starts under the lowest water in this column (or, with no
        # water here, only in the bottom few rows: the pit behind the wheel)
        tops[x] = rows[-1] + 2 if len(rows) else h - 6
    # below the picture's foot: the real earth's bottom rows again, in bands
    # each shifted sideways (and every other one upside down, so they meet
    # without a seam), darker with depth
    BAND = 12
    strip = bg[h - BAND:h]                       # the picture's last rows: earth all along
    rng = np.random.default_rng(7)
    ground = np.zeros((EXTRA + 8, w, 3), np.float32)
    k = 0
    while k * BAND < EXTRA + 8:
        band = np.roll(strip, int(rng.integers(0, w)), axis=1)
        if k % 2 == 0:
            band = band[::-1]
        rows = min(BAND, EXTRA + 8 - k * BAND)
        ground[k * BAND:k * BAND + rows] = band[:rows]
        k += 1
    fade = (1.0 - 0.45 * np.linspace(0, 1, EXTRA + 8))[:, None, None]
    ground = ground * fade
    for x in range(w):
        top = tops[x]
        if top < h:
            out[top:h, x, :3] = bg[top:, x]
            out[top:h, x, 3] = 255
        out[h:, x, :3] = ground[8:, x]
        out[h:, x, 3] = 255
    # blend the picture's last 8 rows of earth into the drawn ground
    for k in range(8):
        wgt = (k + 1) / 9.0
        row = h - 8 + k
        has = out[row, :, 3] > 0
        out[row, has, :3] = out[row, has, :3] * (1 - wgt) + ground[k, has] * wgt
    # a soft top edge (one row), so the ground's line does not look cut
    a = out[..., 3]
    soft = np.zeros_like(a)
    soft[1:] = a[:-1]
    edge = (a > 0) & (soft == 0)
    a[edge] = 140
    out[..., 3] = a
    return Image.fromarray(np.clip(out, 0, 255).astype(np.uint8), 'RGBA')


def save(img, path, frames=1):
    info = PngImagePlugin.PngInfo()
    info.add_text('openc2e-frames', str(frames))
    img.save(path, pnginfo=info)


def main():
    c2, user = sys.argv[1], sys.argv[2]
    out = os.path.join(user, 'Images')
    os.makedirs(out, exist_ok=True)
    img = make(c2)
    save(img, os.path.join(out, 'zsbd.png'))
    print('zsbd', img.size, 'top-left', X0, Y0)


if __name__ == '__main__':
    main()
