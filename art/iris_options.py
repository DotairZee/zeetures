"""Show the shaft-top iris's options side by side for the owner to choose:
finishes (iris.STYLE) at one tilt, and tilts (iris.TILT) in one finish, each
shut and open with the pod in it, over a local preview of the lab.

The background is a screenshot the owner's own game made (a local preview
only: the output is for looking at, never shipped).

    python3 iris_options.py BACKGROUND.bmp CAM_X CAM_Y POD_PNG OUT.png

CAM_X, CAM_Y: the world position of the screenshot's top-left (the world
view starts 93 px down the window).
"""
import sys
import numpy as np
from PIL import Image, ImageDraw
import iris

bg_path, camx, camy, pod_path, out = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4], sys.argv[5]
bg = Image.open(bg_path).convert('RGBA')
pod_sheet = Image.open(pod_path).convert('RGBA')
PH = pod_sheet.height // 10
pod = [pod_sheet.crop((0, i * PH, pod_sheet.width, (i + 1) * PH)) for i in range(10)]
VX0, VY0, VX1, VY1 = 4222, 1425, 4365, 1592          # the world area shown
SCALE = 3


def scene(style, tilt, with_pod):
    iris.STYLE, iris.TILT = style, tilt
    fr = iris.frames()
    sq, cy, ih, y0 = iris.geom()
    im = bg.crop((VX0 - camx, VY0 - camy + 93, VX1 - camx, VY1 - camy + 93)).copy()

    def put(sprite, wx, wy):
        im.alpha_composite(sprite, (wx - VX0, wy - VY0))
    put(fr[6 if with_pod else 0], iris.X0, y0)
    if with_pod:
        for i in (1, 0, 7, 8):                     # far glass, floor, open doors, shell
            put(pod[i], 4248, 1442)
    put(fr[7], iris.X0, y0)
    return im


cells = []
for style in ('steel', 'bronze', 'fluted', 'channel'):
    cells.append((f'{style}, tilt 10', style, 10.0))
for tilt in (4.0, 6.8, 10.0, 13.0):
    cells.append((f'steel, tilt {tilt:g}' + (' (now)' if tilt == 6.8 else ''), 'steel', tilt))
w, h = (VX1 - VX0) * SCALE, (VY1 - VY0) * SCALE
pad = 24
sheet = Image.new('RGB', (2 * (2 * w + 12) + 3 * pad, 4 * (h + pad) + pad), (24, 24, 28))
d = ImageDraw.Draw(sheet)
for n, (label, style, tilt) in enumerate(cells):
    col, row = n // 4, n % 4
    x = pad + col * (2 * w + 12 + pad)
    y = pad + row * (h + pad)
    for j, with_pod in enumerate((False, True)):
        sc = scene(style, tilt, with_pod).resize((w, h), Image.LANCZOS)
        sheet.paste(sc.convert('RGB'), (x + j * (w + 12), y))
    d.text((x, y - 16), f'{"ABCD"[row] if col == 0 else "1234"[row]}. {label}', fill=(235, 235, 235))
sheet.save(out)
print('wrote', out)
