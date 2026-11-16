"""Compare the engine's made art (OPENC2E_DUMP_MADE_ART=DIR, NAME.rgba) with
pictures made by the Python recipes (DIR2/NAME.png, frames stacked).

    python3 compare_made.py DUMP_DIR PNG_DIR [NAME ...]

For each picture: its size and frames, the largest difference in any
channel, and how many pixels differ by more than 2 in any channel.
"""
import os
import sys
import numpy as np
from PIL import Image


def load_dump(path):
    with open(path, 'rb') as f:
        w, h, n = map(int, f.readline().split())
        a = np.frombuffer(f.read(), np.uint8)
    return a.reshape(n * h, w, 4), (w, h, n)


def main():
    dump, pngs, names = sys.argv[1], sys.argv[2], sys.argv[3:]
    if not names:
        names = sorted(f[:-5] for f in os.listdir(dump) if f.endswith('.rgba'))
    bad = 0
    for n in names:
        a, (w, h, k) = load_dump(os.path.join(dump, n + '.rgba'))
        p = os.path.join(pngs, n + '.png')
        if not os.path.exists(p):
            print(f'{n}: no {p}')
            continue
        b = np.asarray(Image.open(p).convert('RGBA'))
        if a.shape != b.shape:
            print(f'{n}: size {a.shape} vs {b.shape}')
            bad += 1
            continue
        # colour only matters where something shows
        d = np.abs(a.astype(int) - b.astype(int))
        d[..., :3][(a[..., 3] == 0) & (b[..., 3] == 0)] = 0
        big = (d.max(-1) > 2).sum()
        print(f'{n}: {w}x{h}x{k} max diff {d.max()}, {big} px off by more than 2 ({100 * big / (a.shape[0] * a.shape[1]):.2f}%)')
        bad += big > 0
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
