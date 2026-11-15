"""Rebuild the female ettin's head from the male ettin's, on the player's own
machine (owner, 2026-11-10: the female ettin's head is ugly; rework the male
head with the female's red eyes and slightly more feminine hair).

Bring-your-own-data: this reads the male ettin head pictures and body data
from the player's installed Creatures 2 (Images/a20a.s16, Body Data/a20a.att)
and writes the female head (a60a) into openc2e's user data folder,
Overrides/Images and Overrides/Body Data, which openc2e searches before the
game's own files. No game pixels are part of the project: only this recipe.

The recipe, frame by frame (120 frames: views, expressions, eyes shut):
- eyes: the dark maroon eye pixels become a glossy red ramp, keeping each
  eye's shading and its white highlight;
- hair: the pale, grayish locks are warmed toward gold and brightened;
- locks: wherever hair hangs at the head's outer edge (its lowest pixels in
  a column, with nothing below), it grows a few pixels longer, darkening
  toward the tips, with the outline moved down to the new ends. Frames get
  margin at the bottom only, so the body data's points (from each frame's
  top-left) still fit, and the male's body data is used as it is.

    python3 ettinhead.py C2_DIR OUT_DIR [--preview PNG]

writes OUT_DIR/Images/a60a.s16 and OUT_DIR/Body Data/a60a.att.
"""
import sys
import os
import colorsys
import numpy as np
from PIL import Image
import s16

LOCK = 3        # how much longer the hanging locks grow, in pixels
PAD = LOCK + 1  # margin added at the bottom of each frame

EYE_RAMP = np.array([[52, 0, 4], [128, 6, 12], [196, 22, 26], [236, 70, 62], [255, 150, 140]], np.float32)
HAIR_HUE, HAIR_SAT, HAIR_LIFT = 0.12, 0.40, 1.22


def hsv(a):
    rgb = a[..., :3] / 255.0
    mx, mn = rgb.max(-1), rgb.min(-1)
    v = mx
    s = np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-6), 0)
    h = np.zeros_like(v)
    d = np.maximum(mx - mn, 1e-6)
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    h = np.where(mx == r, ((g - b) / d) % 6, h)
    h = np.where(mx == g, (b - r) / d + 2, h)
    h = np.where(mx == b, (r - g) / d + 4, h)
    return (h / 6) % 1.0, s, v


def ramp(t):
    t = np.clip(t, 0, 1) * (len(EYE_RAMP) - 1)
    i = np.minimum(t.astype(int), len(EYE_RAMP) - 2)
    f = (t - i)[..., None]
    return EYE_RAMP[i] * (1 - f) + EYE_RAMP[i + 1] * f


def blobs_at_least(mask, n):
    """The mask without its connected pieces smaller than n pixels."""
    H, W = mask.shape
    seen = np.zeros_like(mask)
    keep = np.zeros_like(mask)
    for y in range(H):
        for x in range(W):
            if mask[y, x] and not seen[y, x]:
                stack, piece = [(y, x)], []
                seen[y, x] = True
                while stack:
                    cy, cx = stack.pop()
                    piece.append((cy, cx))
                    for ny, nx in ((cy + 1, cx), (cy - 1, cx), (cy, cx + 1), (cy, cx - 1)):
                        if 0 <= ny < H and 0 <= nx < W and mask[ny, nx] and not seen[ny, nx]:
                            seen[ny, nx] = True
                            stack.append((ny, nx))
                if len(piece) >= n:
                    for py, px in piece:
                        keep[py, px] = True
    return keep


def rework(im):
    a = np.asarray(im.convert('RGBA')).astype(np.float32)
    H, W = a.shape[:2]
    out = np.zeros((H + PAD, W, 4), np.float32)
    out[:H] = a
    a = out
    opaque = a[..., 3] > 0
    h, s, v = hsv(a)
    # the eyes: a pure deep red core (hue near 0, almost no green or blue)
    # in blobs of 4 pixels or more, and the near-red ring around them; the
    # skin's dark shading (under the chin) is orange-brown and stays skin
    core = opaque & ((h < 0.03) | (h > 0.95)) & (s > 0.6) & (v < 0.6) & (v > 0.08)
    core = blobs_at_least(core, 4)
    ring = np.zeros_like(core)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            ring |= np.roll(np.roll(core, dy, 0), dx, 1)
    eye = core | (ring & opaque & ((h < 0.045) | (h > 0.95)) & (s > 0.45) & (v < 0.5))
    # the eye's white highlight: bright and pale, beside eye pixels
    near = np.zeros_like(eye)
    for dy in (-2, -1, 0, 1, 2):
        for dx in (-2, -1, 0, 1, 2):
            near |= np.roll(np.roll(eye, dy, 0), dx, 1)
    shine = opaque & near & (v > 0.75) & (s < 0.3) & ~eye
    hair = opaque & ~eye & ~shine & (v > 0.30) & (s < 0.36) & (((h > 0.07) & (h < 0.40)) | (s < 0.12))

    rgb = a[..., :3].copy()
    # eyes: shading kept, as a red ramp
    if eye.any():
        rgb[eye] = ramp(v[eye] / 0.6 * 0.92)
    rgb[shine] = rgb[shine] * 0.85 + np.array([255, 200, 190], np.float32) * 0.15
    # hair: warmed toward gold and brightened, lightness kept
    hv = np.clip(v[hair] * HAIR_LIFT, 0, 1)
    hs = np.clip(HAIR_SAT * (0.6 + 0.6 * s[hair] / 0.36), 0, 0.55)
    gold = np.array([colorsys.hsv_to_rgb(HAIR_HUE, ss, vv) for ss, vv in zip(hs, hv)], np.float32) * 255 if hair.any() else np.zeros((0, 3))
    rgb[hair] = gold

    # locks: a column whose lowest opaque pixels are hair (with at most an
    # outline pixel under them) hangs free: grow it LOCK pixels longer
    alpha = a[..., 3].copy()
    new_rgb, new_alpha = rgb.copy(), alpha.copy()
    for x in range(W):
        col = np.nonzero(alpha[:, x] > 0)[0]
        if len(col) == 0:
            continue
        yb = col[-1]
        top_hair = yb
        outline = v[yb, x] < 0.30 and not hair[yb, x]
        if outline:
            top_hair = yb - 1
        if top_hair < 2 or not (hair[top_hair, x] and hair[top_hair - 1, x]):
            continue
        # taper at the ends of a run of hanging columns
        def hangs(xx):
            if xx < 0 or xx >= W:
                return False
            c = np.nonzero(alpha[:, xx] > 0)[0]
            if len(c) == 0:
                return False
            y2 = c[-1]
            if v[y2, xx] < 0.30 and not hair[y2, xx]:
                y2 -= 1
            return y2 >= 1 and hair[y2, xx]
        length = LOCK if (hangs(x - 1) and hangs(x + 1)) else LOCK - 1
        base = rgb[top_hair, x]
        for k in range(1, length + 1):
            new_rgb[top_hair + k, x] = base * (1 - 0.12 * k)
            new_alpha[top_hair + k, x] = 255
        if outline:
            new_rgb[top_hair + length + 1, x] = rgb[yb, x]
            new_alpha[top_hair + length + 1, x] = 255
    res = np.dstack([np.clip(new_rgb, 0, 255), new_alpha])
    return Image.fromarray(res.astype(np.uint8), 'RGBA')


def main():
    c2, out = sys.argv[1], sys.argv[2]
    preview = sys.argv[sys.argv.index('--preview') + 1] if '--preview' in sys.argv else None
    male = s16.read(os.path.join(c2, 'Images', 'a20a.s16'))
    female = [rework(im) for im in male]
    os.makedirs(os.path.join(out, 'Images'), exist_ok=True)
    os.makedirs(os.path.join(out, 'Body Data'), exist_ok=True)
    s16.write(os.path.join(out, 'Images', 'a60a.s16'), female)
    with open(os.path.join(c2, 'Body Data', 'a20a.att'), 'rb') as f:
        att = f.read()
    with open(os.path.join(out, 'Body Data', 'a60a.att'), 'wb') as f:
        f.write(att)
    print(f'wrote {len(female)} frames to {out}')
    if preview:
        old = s16.read(os.path.join(c2, 'Images', 'a60a.s16'))
        pick = list(range(0, 16)) + [40, 41, 44, 56, 60, 72, 76, 88, 92]
        W = max(im.width for im in old + female) + 4
        H = max(im.height for im in old + female) + 4
        sheet = Image.new('RGBA', (len(pick) * W, 3 * H), (40, 40, 40, 255))
        for row, ims in enumerate((male, female, old)):
            for c, k in enumerate(pick):
                sheet.paste(ims[k], (c * W + 2, row * H + 2), ims[k])
        sheet = sheet.resize((sheet.width * 3, sheet.height * 3), Image.NEAREST)
        sheet.save(preview)


if __name__ == '__main__':
    main()
