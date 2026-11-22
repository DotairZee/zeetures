"""The rotten stage of our bushes' fruit (owner, 2026-11-21: "given that
rotten fruit spreads disease, should we consider whether the fruits from the
plants we made should have a rotten state--and sprite set?"). Made from the
fruit fruitbush.py and seaplant.py draw (our own art; no game pixels):
overripe, lying down, then browned, blotched with mold and slumped.

    python3 rottenfruit.py OUT_DIR

writes OUT_DIR/zfrr.s16, 16 images (kind * 2 + stage: dewberry, sunplum,
emberpear, frostnut, sea grape, kelp pod, sea plum, frost pearl; rotten,
then collapsed) and a preview.
"""
import os
import sys

import numpy as np
from PIL import Image

import s16
from fruitbush import FH, FW, SS, crisp, dim, fruit
from seaplant import KINDS as SEA_KINDS
from seaplant import sea, seafruit

LAND = ('dewberry', 'sunplum', 'emberpear', 'frostnut')
# mold: dusty blue-green on land; a pale slime in the sea
MOLD = {'land': ((84, 100, 80), (146, 158, 134)), 'sea': ((100, 112, 92), (158, 166, 146))}


def slump(layer, tall, wide):
    """The fruit sunk under its own weight, still resting on the bottom."""
    W, H = layer.size
    w, h = int(W * wide), int(H * tall)
    small = layer.resize((w, h), Image.BICUBIC)
    out = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    out.paste(small, ((W - w) // 2, H - h))
    return out


def spoil(body, where, stage, seed):
    """Browned, with soft dark blotches and patches of mold."""
    rng = np.random.default_rng(seed)
    a = np.asarray(body).astype(np.float32).copy()
    h, w = a.shape[:2]
    m = a[..., 3] > 0
    lum = a[..., :3].mean(-1, keepdims=True)
    rot = np.array([86, 62, 38] if where == 'land' else [84, 76, 48], np.float32) * (0.35 + lum / 150.0)
    k = 0.5 if stage == 0 else 0.72
    a[..., :3] = a[..., :3] * (1 - k) + rot * k

    def noise(n):
        g = (rng.random((n, n)) * 255).astype(np.uint8)
        return np.asarray(Image.fromarray(g).resize((w, h), Image.BICUBIC)).astype(np.float32) / 255
    dark = noise(5) > (0.62 if stage == 0 else 0.5)
    a[..., :3][dark & m] *= 0.62
    moldmap = noise(6)
    lo, hi = MOLD[where]
    patch = moldmap > (0.7 if stage == 0 else 0.6)
    fuzz = np.clip((moldmap - 0.6) / 0.35, 0, 1)[..., None]
    mold = np.array(lo, np.float32) * (1 - fuzz) + np.array(hi, np.float32) * fuzz
    sel = patch & m
    a[..., :3][sel] = a[..., :3][sel] * 0.35 + mold[sel] * 0.65
    return Image.fromarray(a.clip(0, 255).astype(np.uint8), 'RGBA')


def rotten(big, where, stage, seed):
    body, top = big
    tall, wide = ((0.8, 1.04), (0.54, 1.14))[stage]
    body = slump(spoil(body, where, stage, seed), tall, min(1.0, wide * 0.94))
    t = np.asarray(top).astype(np.float32).copy()
    t[..., :3] = t[..., :3] * 0.4 + np.array([52, 40, 26], np.float32) * 0.6  # a withered stem
    top = slump(Image.fromarray(t.clip(0, 255).astype(np.uint8), 'RGBA'), tall, min(1.0, wide * 0.94))
    return crisp((body, top), FW, FH, seed)


def make():
    out = []
    for n, k in enumerate(LAND):
        for st in range(2):
            out.append(dim(rotten(fruit(k, True, side=True), 'land', st, 300 + 10 * n + st), 0.85, sat=0.92))
    for n, k in enumerate(SEA_KINDS):
        for st in range(2):
            out.append(sea(rotten(seafruit(k, True, side=True), 'sea', st, 400 + 10 * n + st)))
    return out


if __name__ == '__main__':
    outdir = sys.argv[1]
    os.makedirs(outdir, exist_ok=True)
    imgs = make()
    s16.write(os.path.join(outdir, 'zfrr.s16'), imgs)
    Z = 5
    sheet = Image.new('RGBA', (len(imgs) // 2 * (FW * Z + 10) + 10, 3 * (FH * Z + 10) + 10), (58, 56, 50, 255))
    ripe = [dim(crisp(fruit(k, True, side=True), FW, FH, 1), 0.85, sat=0.92) for k in LAND] + [sea(crisp(seafruit(k, True, side=True), FW, FH, 1)) for k in SEA_KINDS]
    for i in range(len(imgs) // 2):
        for row, im in enumerate((ripe[i], imgs[i * 2], imgs[i * 2 + 1])):
            sheet.alpha_composite(im.resize((FW * Z, FH * Z), Image.NEAREST), (10 + i * (FW * Z + 10), 10 + row * (FH * Z + 10)))
    sheet.save(os.path.join(outdir, 'rottenfruit-preview.png'))
    print('wrote', outdir, len(imgs))
