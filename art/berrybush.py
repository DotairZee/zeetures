"""Draw an original berry shrub for the places where the game's red berries
come from (owner, 2026-11-09: they turned up all over the place with
nothing to show where), in the same soft prerendered look as the fruit
bushes (fruitbush.py, whose drawing it reuses at a smaller size).

The shrub carries no berries of its own: the game's berries (2 25 11, from
the player's own copy) are hung on it by food.cos and ripen, fall and rot
by their own script. Drawn from scratch (no game or COB pixels are read or
copied), so the output can ship with the project.

    python3 berrybush.py OUT_DIR

writes OUT_DIR/zbrd.s16 (the glass dome's garden: fresh green in soft
light) and OUT_DIR/zbrc.s16 (the bamboo walkway in the east caves: the
same leaves toned to its dim warm lamplight), 4 images each (spring,
summer, autumn, winter), 96 x 60; OUT_DIR/zbrp.s16, the walkway's shrub in
a small terracotta pot (planter.py's urn at 0.8 size, warm like the
bamboo; a bush on a built walkway stands in a pot, owner), 96 x 78; and a
PNG preview sheet.
"""
import sys
import os
import numpy as np
from PIL import Image
import s16
import fruitbush as fb
import planter

W, H = 96, 60
LOOKS = [
    # (file, fruit-bush look, brightness, saturation, tint)
    ('zbrd', 'moss', 0.95, 0.95, (0.98, 1.02, 0.98)),
    ('zbrc', 'moss', 0.72, 0.8, (1.06, 0.98, 0.86)),
]

if __name__ == '__main__':
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    # a shrub, not a small tree: finer leaves (70%) on a young bush's frame,
    # drawn on a roomier canvas and trimmed to the plant
    fb.BW, fb.BH = 120, 80
    leaf = fb.leaf
    fb.leaf = lambda d, x, y, ang, L, Wd, *rest: leaf(d, x, y, ang, L * 0.7, Wd * 0.8, *rest)

    def trimmed(im):
        a = np.asarray(im)[..., 3]
        ys, xs = np.nonzero(a)
        cx = (xs.min() + xs.max()) // 2
        box = (cx - W // 2, a.shape[0] - H, cx - W // 2 + W, a.shape[0])
        return im.crop(box)
    sheets = []
    for name, look, k, sat, tint in LOOKS:
        imgs = [trimmed(fb.dim(fb.bush(s, 31 + i, 1, look), k, sat=sat, tint=tint)) for i, s in enumerate(('spring', 'summer', 'autumn', 'winter'))]
        s16.write(os.path.join(out, name + '.s16'), imgs)
        sheets.append(imgs)
    # the walkway's shrub in a smaller terracotta pot, toned like the shrub
    planter.PW, planter.PH = 54, 22
    urn = planter.matte(planter.pot('terra').resize((54, 22), Image.BOX))
    urn = fb.dim(urn, 0.72, sat=0.8, tint=(1.06, 0.98, 0.86))
    potted = [planter.potted(im, urn, soil_y=56, drape=4) for im in sheets[1]]
    s16.write(os.path.join(out, 'zbrp.s16'), potted)
    sheets.append([p.crop((0, 0, W, H)) for p in potted])
    sheet = Image.new('RGBA', ((W + 8) * 4 + 8, (H + 8) * len(sheets) + 8), (60, 50, 40, 255))
    for r, imgs in enumerate(sheets):
        for i, im in enumerate(imgs):
            sheet.alpha_composite(im, (8 + i * (W + 8), 8 + r * (H + 8)))
    sheet.resize((sheet.width * 3, sheet.height * 3), Image.NEAREST).save(os.path.join(out, 'berrybush-sheet.png'))
    print('wrote', out)
