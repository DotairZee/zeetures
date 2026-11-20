"""The collar at the east end of the sandy tunnel under the west sea's east
shore (world x 1156-1182, y 998-1097), made on the player's own machine from
their installed Creatures 2 (bring-your-own-data: only this recipe is part
of the project, never the game's pixels).

The collar (a bronze band with four rivets and a white knob each side) is
painted in the background where the tunnel meets the rock. The game draws
the tunnel's front in front of creatures (overlay 1 0 0, sprite fore, 400 x
224 at 759, 952, plane 8999), but that piece ends at x 1158, so a creature
at the tunnel's end stood in front of the collar (owner's screenshot,
2026-11-19). This copies the collar, its band and its two knobs only, so it
is drawn in front of creatures too; it is the same picture, so nothing
changes to the eye until a creature walks behind it.

    python3 sea_collar.py C2_DIR USER_DATA_DIR

writes USER_DATA_DIR/Images/zscl.png, 47 x 105, top-left at (X0, Y0);
additions/sea-collar.cos places it.
"""
import sys
import os
import numpy as np
from PIL import Image, PngImagePlugin
from background import region

X0, Y0, W, H = 1145, 995, 47, 105


def mask():
    """The band (x 1156-1182, y 998-1097) and the two knobs (centres 1151 and
    1185, y 1007, radius squared 20); integer tests, as madeart/sea_collar.cpp."""
    yy, xx = np.mgrid[Y0:Y0 + H, X0:X0 + W]
    band = (xx >= 1156) & (xx <= 1182) & (yy >= 998) & (yy <= 1097)
    knobs = ((xx - 1151) ** 2 + (yy - 1007) ** 2 <= 20) | ((xx - 1185) ** 2 + (yy - 1007) ** 2 <= 20)
    return band | knobs


def main():
    c2, user = sys.argv[1], sys.argv[2]
    a = np.asarray(region(c2, X0, Y0, X0 + W, Y0 + H).convert('RGBA')).copy()
    a[..., 3] = np.where(mask(), 255, 0)
    a[a[..., 3] == 0] = 0
    out = os.path.join(user, 'Images')
    os.makedirs(out, exist_ok=True)
    info = PngImagePlugin.PngInfo()
    info.add_text('openc2e-frames', '1')
    Image.fromarray(a, 'RGBA').save(os.path.join(out, 'zscl.png'), pnginfo=info)
    print('wrote', os.path.join(out, 'zscl.png'), a.shape[1], 'x', a.shape[0])


if __name__ == '__main__':
    main()
