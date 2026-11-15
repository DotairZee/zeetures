"""Read a rectangle of Creatures 2's world background from the player's own
installed game (bring-your-own-data: only this reader is part of the
project, never the pixels).

C2 keeps the background in Images/Back.S16: 928 tiles of 144 x 150 pixels,
58 across and 16 down, stored column by column (tile = column * 16 + row),
as openc2e's ImageUtils::StitchBackground assembles them.

    from background import region
    im = region(C2_DIR, x0, y0, x1, y1)   # RGBA, opaque
"""
import os
import struct
import numpy as np
from PIL import Image

TW, TH, ACROSS, DOWN = 144, 150, 58, 16


def _tile(data, flags, index):
    off, w, h = struct.unpack_from('<IHH', data, 6 + 8 * index)
    v = np.frombuffer(data, '<u2', w * h, off).reshape(h, w).astype(np.uint32)
    if flags & 1:  # 565
        r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
        rgb = np.dstack([r << 3, g << 2, b << 3])
    else:          # 555
        r, g, b = (v >> 10) & 31, (v >> 5) & 31, v & 31
        rgb = np.dstack([r << 3, g << 3, b << 3])
    return rgb.astype(np.uint8)


def region(c2dir, x0, y0, x1, y1):
    path = os.path.join(c2dir, 'Images', 'Back.S16')
    if not os.path.exists(path):
        path = os.path.join(c2dir, 'Images', 'back.s16')
    data = open(path, 'rb').read()
    flags, n = struct.unpack_from('<IH', data, 0)
    if n != ACROSS * DOWN:
        raise ValueError(f'{path}: {n} tiles, expected {ACROSS * DOWN}')
    out = np.zeros((y1 - y0, x1 - x0, 3), np.uint8)
    for col in range(x0 // TW, (x1 - 1) // TW + 1):
        for row in range(y0 // TH, (y1 - 1) // TH + 1):
            t = _tile(data, flags, col * DOWN + row)
            tx, ty = col * TW, row * TH
            ax0, ay0 = max(x0, tx), max(y0, ty)
            ax1, ay1 = min(x1, tx + TW), min(y1, ty + TH)
            out[ay0 - y0:ay1 - y0, ax0 - x0:ax1 - x0] = t[ay0 - ty:ay1 - ty, ax0 - tx:ax1 - tx]
    return Image.fromarray(np.dstack([out, np.full(out.shape[:2], 255, np.uint8)]), 'RGBA')
