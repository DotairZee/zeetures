"""The rock trunk at the top of the volcano's west side, drawn in front of
creatures (owner, 2026-11-13: creatures walking over the volcano's top showed
through it; "they should be obscured by that"). Made on the player's own
machine from their installed Creatures 2 (bring-your-own-data: only this
recipe is part of the project, never the game's pixels).

The walk over the volcano's top (rooms 142, 233 and 143, floor y 693) passes
behind the grey rock trunk that rises diagonally from the volcano's shoulder
toward the palm (x about 7320-7510, y 500-705: above y 500 no creature reaches). The background is one flat
picture, so creatures were drawn over the trunk. This cuts the trunk out of the player's Back.S16 along a hand-
traced outline (soft at the edges), to be placed in front of creatures.

    python3 volcano_trunk.py C2_DIR USER_DATA_DIR

writes USER_DATA_DIR/Images/zvtk.png; tools/additions/volcano-trunk.cos
places it at world X0, Y0.
"""
import os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, PngImagePlugin
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from background import region

X0, Y0, X1, Y1 = 7300, 500, 7530, 712

# the trunk's outline in world coordinates: down its west edge (below the
# bananas), along the shoulder at the floor, up its east edge (the shadowed
# gap before the rock behind it)
OUTLINE = [
    (7448, 500), (7446, 530), (7440, 548), (7428, 564), (7414, 590), (7394, 620), (7370, 655), (7350, 684),
    (7322, 706), (7496, 706),
    (7495, 650), (7499, 600), (7504, 550), (7509, 500),
]


def make(c2):
    rgb = region(c2, X0, Y0, X1, Y1).convert('RGB')
    w, h = rgb.size
    ss = 4
    m = Image.new('L', (w * ss, h * ss), 0)
    ImageDraw.Draw(m).polygon([((x - X0) * ss, (y - Y0) * ss) for x, y in OUTLINE], fill=255)
    m = m.resize((w, h), Image.LANCZOS).filter(ImageFilter.GaussianBlur(0.7))
    a = np.asarray(m).astype(np.float32)
    px = np.asarray(rgb).astype(np.float32)
    # sky showing between leaves at the west edge stays out
    r, g, b = px[..., 0], px[..., 1], px[..., 2]
    sky = (b > r + 8) & (b > g) & (b > 140)
    a[sky] = 0
    return Image.fromarray(np.dstack([px.astype(np.uint8), np.clip(a, 0, 255).astype(np.uint8)]), 'RGBA')


def save(img, path, frames=1):
    info = PngImagePlugin.PngInfo()
    info.add_text('openc2e-frames', str(frames))
    img.save(path, pnginfo=info)


def main():
    c2, user = sys.argv[1], sys.argv[2]
    out = os.path.join(user, 'Images')
    os.makedirs(out, exist_ok=True)
    img = make(c2)
    save(img, os.path.join(out, 'zvtk.png'))
    print('zvtk', img.size, 'top-left', X0, Y0)


if __name__ == '__main__':
    main()
