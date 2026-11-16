"""Compare the engine's made art (OPENC2E_DUMP_MADE_ART=DIR, NAME.rgba) with
pictures made by the Python recipes (DIR2/NAME.png, frames stacked, or
DIR2/NAME.s16, as art/s16.py reads it; frames may differ in size).

    python3 compare_made.py DUMP_DIR PNG_DIR [NAME ...]

For each picture: its size and frames, the largest difference in any
channel, and how many pixels differ by more than 2 in any channel.
"""
import os
import sys
import numpy as np
from PIL import Image


def load_dump(path):
    """The frames, each an h x w x 4 array, and frame 0's (w, h, n)."""
    with open(path, 'rb') as f:
        w, h, n = map(int, f.readline().split())
        sizes = [tuple(map(int, s.split(b'x'))) for s in f.readline().split()]
        a = np.frombuffer(f.read(), np.uint8)
    frames, at = [], 0
    for fw, fh in sizes:
        frames.append(a[at:at + fw * fh * 4].reshape(fh, fw, 4))
        at += fw * fh * 4
    return frames, (w, h, n)


def load_ref(pngs, n):
    """The recipe's frames, from NAME.png (stacked, openc2e-frames) or NAME.s16."""
    p = os.path.join(pngs, n + '.png')
    if os.path.exists(p):
        im = Image.open(p)
        k = int(im.info.get('openc2e-frames', 1))
        a = np.asarray(im.convert('RGBA'))
        fh = a.shape[0] // k
        return [a[i * fh:(i + 1) * fh] for i in range(k)]
    p = os.path.join(pngs, n + '.s16')
    if os.path.exists(p):
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        import s16
        return [np.asarray(f) for f in s16.read(p)]
    return None


def main():
    dump, pngs, names = sys.argv[1], sys.argv[2], sys.argv[3:]
    if not names:
        names = sorted(f[:-5] for f in os.listdir(dump) if f.endswith('.rgba'))
    bad = 0
    for n in names:
        frames, (w, h, k) = load_dump(os.path.join(dump, n + '.rgba'))
        ref = load_ref(pngs, n)
        if ref is None:
            print(f'{n}: no {n}.png or {n}.s16 in {pngs}')
            continue
        if [f.shape for f in frames] != [r.shape for r in ref]:
            print(f'{n}: frames {[f.shape[:2] for f in frames][:4]}... vs {[r.shape[:2] for r in ref][:4]}... ({len(frames)} vs {len(ref)})')
            bad += 1
            continue
        a, b = np.concatenate([f.reshape(-1, 4) for f in frames]), np.concatenate([r.reshape(-1, 4) for r in ref])
        # colour only matters where something shows
        d = np.abs(a.astype(int) - b.astype(int))
        d[..., :3][(a[..., 3] == 0) & (b[..., 3] == 0)] = 0
        big = (d.max(-1) > 2).sum()
        print(f'{n}: {w}x{h}x{k} max diff {d.max()}, {big} px off by more than 2 ({100 * big / len(a):.2f}%)')
        bad += big > 0
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
