"""The ledge under the twisted pillar beside the lift shaft west of the
arches (world x ~4290, floor y 1175), made on the player's own machine from
their installed Creatures 2 (bring-your-own-data: only this recipe is part
of the project, never the game's pixels).

The game draws the pillar again in front of creatures (overlay 1 0 0, sprite
dung frame 12, top-left 4271, 1053, plane 8999) down to the foot of its base
at y 1172. Creatures there walk on y 1175, so one passing behind the pillar
showed its feet in the dark ledge under the base (owner's screenshot,
2026-11-12). This copies that ledge, and the top of the rubble below it,
so it is drawn in front of creatures too; it is the same picture, so
nothing changes to the eye until a creature walks behind it.

    python3 pillarfoot.py C2_DIR USER_DATA_DIR

writes USER_DATA_DIR/Images/zpft.png, 48 x 8, top-left at (X0, Y0);
additions/pillar.cos places it.
"""
import sys
import os
import numpy as np
from PIL import Image, PngImagePlugin
from background import region

X0, Y0, X1, Y1 = 4270, 1172, 4318, 1180  # the base's width, its foot to the rubble


def main():
    c2, user = sys.argv[1], sys.argv[2]
    im = region(c2, X0, Y0, X1, Y1).convert('RGBA')
    a = np.asarray(im).copy()
    a[..., 3] = 255
    out = os.path.join(user, 'Images')
    os.makedirs(out, exist_ok=True)
    info = PngImagePlugin.PngInfo()
    info.add_text('openc2e-frames', '1')
    Image.fromarray(a, 'RGBA').save(os.path.join(out, 'zpft.png'), pnginfo=info)
    print('wrote', os.path.join(out, 'zpft.png'), a.shape[1], 'x', a.shape[0])


if __name__ == '__main__':
    main()
