"""Minimal reader and writer for Creatures 2 .s16 sprite files (RGB565).

Used to save our own original sprites; reading is only for viewing the
player's installed game locally, never for copying pixels into our art.
"""
import struct
import numpy as np
from PIL import Image


def write(path, images):
    """Write RGBA PIL images as a 565 .s16; alpha below 128 becomes transparent (0)."""
    n = len(images)
    off = 6 + 8 * n
    header = struct.pack('<IH', 1, n)
    data = b''
    for im in images:
        a = np.asarray(im.convert('RGBA')).astype(np.uint16)
        v = ((a[..., 0] >> 3) << 11) | ((a[..., 1] >> 2) << 5) | (a[..., 2] >> 3)
        v[v == 0] = 1 << 5  # keep opaque black from turning transparent
        v[a[..., 3] < 128] = 0
        header += struct.pack('<IHH', off + len(data), im.width, im.height)
        data += v.astype('<u2').tobytes()
    open(path, 'wb').write(header + data)


def read(path):
    d = open(path, 'rb').read()
    flags, n = struct.unpack_from('<IH', d, 0)
    out = []
    for i in range(n):
        off, w, h = struct.unpack_from('<IHH', d, 6 + 8 * i)
        v = np.frombuffer(d, '<u2', w * h, off).reshape(h, w)
        if flags & 1:
            rgb = [((v >> 11) & 31) << 3, ((v >> 5) & 63) << 2, (v & 31) << 3]
        else:
            rgb = [((v >> 10) & 31) << 3, ((v >> 5) & 31) << 3, (v & 31) << 3]
        alpha = np.where(v == 0, 0, 255)
        out.append(Image.fromarray(np.stack(rgb + [alpha], -1).astype(np.uint8), 'RGBA'))
    return out
