"""The glass tube by the treehouses, shortened at its west end (owner,
2026-11-14: "any way to modify the sprites so we can contract the glass tube
to the right so that the elevator doesn't come up through the tube?"; the
treehouse cage lift's new top stop is on the upper hut's deck, under the
tube's west end). Made on the player's own machine from their installed
Creatures 2 (bring-your-own-data: only this recipe is part of the project).

The tube is painted in the world's background and drawn again in front of
creatures by a foreground piece (scenery 1 0 0 at 3531,1390: hall.s16
frame 4, 504 x 172), so creatures walking along it are inside its glass.
This makes two pictures:
- zths: the foreground piece starting SHIFT px further east, its broken
  west end (the CAP columns) moved there with it; the rest as it was.
  Drawn in place of the game's piece (tube-short.cos removes that one).
- ztbf: behind creatures, over the background's tube where it no longer is
  (from its old west end to the new end's far side): each column filled
  with that column's own background from just above the tube, mirrored at
  the tube's top edge, so it carries on the trunk and leaves above it.

    python3 tube_short.py C2_DIR USER_DATA_DIR
"""
import os, struct, sys
import numpy as np
from PIL import Image, PngImagePlugin
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from background import region

TX, TY = 3531, 1390          # the game's piece's top left
FRAME = 4                    # its picture in hall.s16
SHIFT = 130                  # how far east the tube's mouth moves (past the lift's cage, x 3650)
BLEND = 10                   # columns blended where the moved end meets the body
FILL_X0, FILL_Y0, FILL_Y1 = 3524, 1398, 1541   # the band filled: from the hut, above the deck
BAND_Y0 = 1388               # the moved tube end's top (a little foliage above its broken edge)
SRC_X0 = 3527                # the background's tube end, from just past the hut's wall


def s16_frame(path, index):
    d = open(path, 'rb').read()
    flags, n = struct.unpack_from('<IH', d, 0)
    off, w, h = struct.unpack_from('<IHH', d, 6 + 8 * index)
    v = np.frombuffer(d, '<u2', w * h, off).reshape(h, w).astype(np.uint32)
    if flags & 1:
        r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
        rgb = np.dstack([r << 3, g << 2, b << 3])
    else:
        r, g, b = (v >> 10) & 31, (v >> 5) & 31, v & 31
        rgb = np.dstack([r << 3, g << 3, b << 3])
    return rgb.astype(np.float32), (v != 0)


def make(c2):
    rgb, mask = s16_frame(os.path.join(c2, 'Images', 'hall.s16'), FRAME)
    h, w = mask.shape
    # each column's outline (the glass has dark holes the background shows
    # through, so the outline, not the opaque pixels, is the tube)
    top = np.array([np.argmax(mask[:, c]) if mask[:, c].any() else h for c in range(w)])
    bot = np.array([h - 1 - np.argmax(mask[::-1, c]) if mask[:, c].any() else -1 for c in range(w)])
    # the broken end: the columns before the top edge reaches the body's
    body_top = int(np.median(top[w // 3: 2 * w // 3]))
    cap = int(np.argmax(top <= body_top + 1)) + 6
    # zths: the cap, then the body as it was
    nw = w - SHIFT
    out = np.zeros((h, nw, 4), np.float32)
    for u in range(nw):
        src = u if u < cap else u + SHIFT
        out[:, u, :3] = rgb[:, src]
        out[:, u, 3] = mask[:, src] * 255.0
    for k in range(BLEND):  # ease from the moved end into the body
        u = cap - BLEND + k
        t = (k + 1) / (BLEND + 1)
        a1, a2 = mask[:, u], mask[:, u + SHIFT]
        both = a1 & a2
        out[both, u, :3] = rgb[both, u] * (1 - t) + rgb[both, u + SHIFT] * t
    zths = Image.fromarray(out.astype(np.uint8), 'RGBA')
    # ztbf, behind creatures: the background's own tube end moved east with
    # the front piece (its inner, back glass is painted only there: owner,
    # 2026-11-14, "the back side of the glass tube is missing"), and where
    # it was (FILL_X0 to SRC_X0 + SHIFT), the trunk and leaves from just
    # above the band, mirrored at its top
    end_x1 = TX + SHIFT + cap
    span = end_x1 - FILL_X0
    bh = FILL_Y1 - BAND_Y0 + 1
    fill = np.zeros((bh, span, 4), np.float32)
    fh = FILL_Y1 - FILL_Y0 + 1
    above = np.asarray(region(c2, FILL_X0, FILL_Y0 - fh - 2, end_x1, FILL_Y0).convert('RGB')).astype(np.float32)
    old = np.asarray(region(c2, SRC_X0, BAND_Y0, end_x1 - SHIFT, FILL_Y1 + 1).convert('RGB')).astype(np.float32)
    moved_x0 = SRC_X0 + SHIFT
    for c in range(span):
        x = FILL_X0 + c
        if x < moved_x0:
            for y in range(FILL_Y0, FILL_Y1 + 1):
                fill[y - BAND_Y0, c, :3] = above[above.shape[0] - 1 - (y - FILL_Y0), c]
                fill[y - BAND_Y0, c, 3] = 255.0
        else:
            col = old[:, x - moved_x0]
            fill[:, c, :3] = col
            # (not the tip of the hut's straw roof, which reaches into the
            # band at its west end: left out, the background shows there)
            straw = (col[:, 0] > 120) & (col[:, 1] > 85) & (col[:, 0] - col[:, 2] > 55) & (col[:, 1] > col[:, 2] + 20)
            fill[:, c, 3] = np.where(straw, 0.0, 255.0)
            k = end_x1 - x  # ease into the tube body it meets
            if k < 12:
                fill[:, c, 3] *= k / 12.0
    # soft along the top and at the hut's side, so the edges do not show
    for k in range(8):
        fill[k, :, 3] *= (k + 1) / 9.0
    for c in range(4):
        fill[:, c, 3] *= (c + 1) / 5.0
    ztbf = Image.fromarray(fill.astype(np.uint8), 'RGBA')
    return zths, ztbf, cap


# the upper hut's deck carried on under the tube's new mouth (owner,
# 2026-11-14: "the platform below needs to be extended ... look closely at
# the platform you are extending"): its tan planks (y 1541-1550), the orange
# log under them (to x 3617) and the thicker log below (to 3645, a ring and
# a blue lashing along it). A clean middle stretch (TILE) is laid on from
# DECK_X0, then the deck's own ragged end (END), moved out to DECK_X1. Only
# the wood is taken (orange and tan; the leaves and glass behind stay).
DECK_Y0, DECK_Y1 = 1540, 1567
TILE = (3466, 3586)
END = (3586, 3650)
DECK_X0, DECK_X1 = 3586, 3796


def deck(c2):
    src = np.asarray(region(c2, TILE[0], DECK_Y0, END[1], DECK_Y1).convert('RGB')).astype(np.float32)
    r, g, b = src[..., 0], src[..., 1], src[..., 2]
    wood = (r > g + 5) & (r - b > 35)
    tw, ew = TILE[1] - TILE[0], END[1] - END[0]
    w = DECK_X1 - DECK_X0
    h = DECK_Y1 - DECK_Y0
    out = np.zeros((h, w, 4), np.float32)
    end_start = w - ew
    for c in range(w):
        sc = (TILE[1] - TILE[0]) + (c - end_start) if c >= end_start else c % tw
        out[:, c, :3] = src[:, sc]
        out[:, c, 3] = wood[:, sc] * 255.0
    return Image.fromarray(out.astype(np.uint8), 'RGBA')


def save(img, path):
    info = PngImagePlugin.PngInfo()
    info.add_text('openc2e-frames', '1')
    img.save(path, pnginfo=info)


if __name__ == '__main__':
    c2, user = sys.argv[1], sys.argv[2]
    out = os.path.join(user, 'Images')
    os.makedirs(out, exist_ok=True)
    zths, ztbf, cap = make(c2)
    save(zths, os.path.join(out, 'zths.png'))
    save(ztbf, os.path.join(out, 'ztbf.png'))
    save(deck(c2), os.path.join(out, 'ztde.png'))
    print('ztde at', (DECK_X0, DECK_Y0))
    print('zths at', (TX + SHIFT, TY), zths.size, '; ztbf at', (FILL_X0, BAND_Y0), ztbf.size, '; cap', cap)
