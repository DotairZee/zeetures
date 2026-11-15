"""The hidden way from the pond's cave to the bamboo-cage lift: its entrance,
a hint of a cave mouth behind the pond's waterfall (owner, 2026-11-10: "just
the hint of a cave entrance behind the waterfall, so it feels hidden"), and
its exit, the old stone arch west of the lift, its bricked-up back opened
into a dark passage (owner: "let's have the exit for the cave be this arch").

Original art, drawn here from nothing but numbers: no game pixels. Both are
PNG sprites with real transparency (openc2e loads NAME.png before NAME.s16
for C2; frames stacked top to bottom, counted in a tEXt chunk
"openc2e-frames"), so the shadows fade softly into the painted scenery.
waterfall-path.cos places them behind creatures.

    python3 cavemouth.py OUT_DIR

writes OUT_DIR/zcvm.png and OUT_DIR/zcva.png.

zcvm.png, the entrance, 84 x 144, top-left at (4338, 748): a tall narrow
hollow in the space the owner outlined, between the dark earth and the
falling water, with depth (a lit inner wall receding into dark, a floor
running in, a lit lip). Frame 0, the whole hollow, behind creatures (a
creature is seen in it); frame 1, its part over the dark earth, in front of
the world's rock picture (where a creature is hidden by the rock anyway).

zcva.png, the arch's passage, 80 x 130, top-left at (4040, 712): the arch's
inner opening (x 4043-4117, crown ~716, floor 840) as a passage receding
into darkness, lighter toward the sill, thinner over the ivy hanging down
its left side so the leaves still show. Two versions:
  0  dark passage
  1  dark passage with a faint warm glimmer far inside (the cave's light)
"""
import sys
import os
import math
import random
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, PngImagePlugin

S = 4  # supersampling for smooth edges


def blend_over(out, color, la):
    """Lay a colour with alpha la over the RGBA float array out."""
    oa = out[..., 3] / 255.0
    na = la + oa * (1 - la)
    out[..., :3] = (np.asarray(color, np.float32) * la[..., None] + out[..., :3] * (oa * (1 - la))[..., None]) / np.maximum(na, 1e-6)[..., None]
    out[..., 3] = na * 255
    return out


# ---- the entrance: a dark opening behind the creature ----

MX, MY = 4338, 748   # the sprite's top-left in the world
MW, MH = 84, 144

# the right edge of the world's foreground rock picture there (the cut-out
# in front of creatures), measured from its transparency: left of it a
# creature is hidden by the rock; this is the opening's left boundary
CUT = [(730, 4375), (742, 4373), (754, 4368), (766, 4366), (778, 4364),
       (790, 4361), (802, 4359), (814, 4357), (826, 4356), (832, 4355), (864, 4355),
       (867, 4356), (870, 4357), (873, 4359), (876, 4361), (879, 4367), (882, 4374),
       (885, 4381), (888, 4382)]


def cut_x(y):
    for (y0, x0), (y1, x1) in zip(CUT, CUT[1:]):
        if y0 <= y <= y1:
            return x0 + (x1 - x0) * (y - y0) / (y1 - y0)
    return 4375 if y < 730 else 4382


# The opening's outline as the owner drew it (2026-11-10, second drawing),
# world px: a tall narrow hollow, a point at the top, its left side down
# over the dark earth, its right side down where the falling water begins,
# its foot below the shelf
OPEN_LEFT = [(4363.7, 770.5), (4361.0, 778), (4358.0, 790), (4355.8, 802), (4354.5, 813),
             (4353.8, 830), (4353.4, 850), (4352.8, 865), (4355.5, 872), (4359.3, 878), (4364.0, 883), (4369.0, 886.5)]
OPEN_RIGHT = [(4367.5, 770.5), (4369.4, 780), (4370.2, 790), (4370.6, 808.6), (4370.6, 830),
              (4370.2, 850), (4369.6, 873), (4369.2, 880), (4369.0, 886.5)]
OPEN_TOP, OPEN_BOTTOM = 770.5, 886.5


def along(pts, y):
    if y <= pts[0][1]:
        return pts[0][0]
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        if y0 <= y <= y1:
            return x0 + (x1 - x0) * (y - y0) / (y1 - y0)
    return pts[-1][0]


def smooth_left(sigma=6.0, margin=1.6):
    """The opening's left edge as (y, x) samples: the rock picture's edge
    above y 828, the owner's outline below, blended across the join, kept
    margin px left of the rock picture's edge (whose edge turns into a ruler-
    straight vertical cut at x 4355 below y 828; the hollow's soft curve,
    drawn in front of the rock there, hides it) and smoothed with a Gaussian
    of sigma px along y."""
    ys = np.arange(OPEN_TOP - 20, OPEN_BOTTOM + 20, 0.25)
    a = np.array([cut_x(y) for y in ys])
    b = np.array([along(OPEN_LEFT, y) for y in ys])
    w = np.clip((ys - 818) / 20.0, 0, 1)
    raw = np.minimum(a * (1 - w) + b * w, a) - margin
    k = np.arange(-int(4 * sigma / 0.25), int(4 * sigma / 0.25) + 1) * 0.25
    k = np.exp(-0.5 * (k / sigma) ** 2); k /= k.sum()
    pad = len(k) // 2
    sm = np.convolve(np.pad(raw, pad, mode='edge'), k, mode='valid')
    # the foot keeps the owner's curve where it closes on the right side
    foot = np.clip((ys - 868) / 10.0, 0, 1)
    sm = sm * (1 - foot) + b * foot
    return ys, sm


def mouth():
    """The entrance (owner, 2026-11-10: the left side of the cave entrance
    the boundary, "the black spot behind the creature"; then the space the
    owner outlined, "not just a black hole, but some depth to it"): a tall
    narrow hollow between the dark earth and the falling water. Inside, the
    right-hand inner wall catches the light from the upper left and recedes
    into darkness, a floor runs in from the shelf, the rock's lip along the
    right edge is lit, and the back is deep dark. Returns two frames: the
    whole hollow (behind creatures, so a creature is seen in it) and the part
    over the dark earth (in front of the world's rock picture, which hides a
    creature there anyway)."""
    ys, xs = np.mgrid[0:MH * S, 0:MW * S].astype(np.float32)
    X = MX + (xs + 0.5) / S
    Y = MY + (ys + 0.5) / S
    # the upper left follows the dark rock's own contour (the rock picture's
    # edge, owner: "more aligned with the contour of the darker rock wall"),
    # the lower part the owner's outline
    # (owner, 2026-11-10: "weird on the left side ... some smoothing out"):
    # the two traced pieces are joined and the edge smoothed along its length
    left = np.interp(Y, *smooth_left())
    right = np.vectorize(lambda y: along(OPEN_RIGHT, y))(Y)
    width = np.maximum(right - left, 0.01)
    inside = (Y >= OPEN_TOP) & (Y <= OPEN_BOTTOM) & (X >= left - 1) & (X <= right + 1)
    d = np.minimum(X - left, right - X)                       # px from the nearer side
    a = np.clip(d / 1.0 + 0.5, 0, 1) * inside
    u = np.clip((X - left) / width, 0, 1)                     # 0 left .. 1 right
    v = np.clip((Y - OPEN_TOP) / (OPEN_BOTTOM - OPEN_TOP), 0, 1)
    # rock texture: soft lumps and fine grain
    rnd = np.random.default_rng(17)
    g1 = rnd.uniform(-1, 1, (40, 40)); g2 = rnd.uniform(-1, 1, (80, 80))
    def vn(g, sc):
        fx, fy = (X - MX) / sc, (Y - MY) / sc
        ix, iy = np.floor(fx).astype(int), np.floor(fy).astype(int)
        tx, ty = fx - ix, fy - iy
        tx, ty = tx * tx * (3 - 2 * tx), ty * ty * (3 - 2 * ty)
        n = g.shape[0]
        G = lambda i, j: g[np.mod(j, n), np.mod(i, n)]
        return G(ix, iy) * (1 - tx) * (1 - ty) + G(ix + 1, iy) * tx * (1 - ty) + G(ix, iy + 1) * (1 - tx) * ty + G(ix + 1, iy + 1) * tx * ty
    tex = 0.6 * vn(g1, 5.0) + 0.4 * vn(g2, 1.8)
    deep = np.array([6, 5, 4], np.float32)
    rock = np.array([118, 82, 46], np.float32)
    sand = np.array([150, 112, 66], np.float32)
    lip = np.array([196, 150, 92], np.float32)
    # the right inner wall, lit, receding into dark toward the left (deeper)
    wall = np.clip((u - 0.5) / 0.5, 0, 1) ** 1.3 * (0.6 + 0.4 * np.clip(1 - v * 0.6, 0, 1))
    # the floor running in, lit near the front (right/bottom), dark further in
    floor = np.clip((v - 0.74) / 0.26, 0, 1) ** 1.1 * (0.3 + 0.7 * u)
    # the top of the hollow in shadow
    roof = np.clip(1 - v / 0.25, 0, 1)
    # contour in the dark (owner: "hint at more contour in the dark area"):
    # a height field of rounded rock forms and the dark rock's own layering,
    # lit faintly from the upper left, so folds and ledges just show
    g3 = rnd.uniform(-1, 1, (40, 40))
    H = 1.0 * vn(g3, 9.0) + 0.45 * vn(g1, 4.0) + 0.25 * np.sin((Y + 5 * vn(g2, 6.0)) / 4.2)
    dHy, dHx = np.gradient(H, 1.0 / S)
    relief = np.clip(-(dHx * 0.7 + dHy * 0.75) * 3.2, -1, 1)
    dim = np.array([92, 66, 40], np.float32)
    glow = np.clip(1 - v * 0.35, 0, 1) * (0.35 + 0.65 * np.clip(u * 1.6, 0, 1))
    deepc = deep + (dim - deep) * (np.clip(relief, 0, 1) * 0.65 * glow)[..., None]
    deepc = deepc * (1 - 0.5 * np.clip(-relief, 0, 1))[..., None]
    col = deepc + (rock - deepc) * (wall * 1.0 * (1 - 0.5 * roof))[..., None]
    col = col + (sand - col) * (floor * 0.75)[..., None]
    col = col * (1 + 0.22 * tex)[..., None]
    # the lit lip of rock along the right edge (and a little round the top)
    lipband = np.clip(1 - np.abs((right - X) - 0.5) / 0.7, 0, 1) * (0.35 + 0.65 * (1 - roof))
    col = col + (lip - col) * (lipband * 0.7)[..., None]
    out = np.dstack([np.clip(col, 0, 255), a * 255])
    full = Image.fromarray(out.astype(np.uint8), 'RGBA').resize((MW, MH), Image.LANCZOS)
    # the part over the dark earth: left of the rock picture's edge
    cut = np.vectorize(cut_x)(Y)
    # (to the far side of the rock picture's last, half-clear edge pixels,
    # so no light seam shows between the rock and the hollow)
    over = np.clip((cut + 1.3 - X) * 1.0 + 0.5, 0, 1)
    # at the foot the rock's own edge, curving out over the hollow's bottom,
    # stays in front (owner, 2026-11-10: "have the line for the rock in the
    # foreground be in front of the cave"): below y ~862 the hollow is only
    # behind the rock, which closes it along its edge
    over = over * np.clip((866 - Y) / 8.0, 0, 1)
    out2 = out.copy()
    out2[..., 3] = out2[..., 3] * over
    front = Image.fromarray(out2.astype(np.uint8), 'RGBA').resize((MW, MH), Image.LANCZOS)
    return full, front


# ---- the arch's passage ----

AW, AH = 80, 130


def arch(glimmer):
    # the inner opening, inset a pixel from the stones: a round head over
    # straight jambs down to the sill
    cx, cy, r = 39.5, 40.0, 35.0
    mask = Image.new('L', (AW * S, AH * S), 0)
    d = ImageDraw.Draw(mask)
    d.ellipse([(cx - r) * S, (cy - r) * S, (cx + r) * S, (cy + r) * S], fill=255)
    d.rectangle([(cx - r) * S, cy * S, (cx + r) * S, AH * S], fill=255)
    hard = mask.resize((AW, AH), Image.LANCZOS)
    a = np.asarray(hard.filter(ImageFilter.GaussianBlur(1.2)), np.float32) / 255.0
    yy, xx = np.mgrid[0:AH, 0:AW].astype(np.float32)
    # deepest under the head, lighter toward the sill (light falls in
    # across the floor); the last few pixels fade to leave the sill
    depth = np.clip(0.93 - np.clip((yy - 60) / 70.0, 0, 1) * 0.28, 0, 1)
    sill = np.clip((AH - 2 - yy) / 6.0, 0, 1)
    # thinner over the ivy that hangs down the left side, from the head to
    # about two-thirds down
    ivy = 1 - 0.42 * np.clip((40 - xx) / 14.0, 0, 1) * np.clip((96 - yy) / 20.0, 0, 1)
    alpha = a * depth * sill * ivy
    # colour: cool near-black stone-shadow, a touch warmer low down
    t = np.clip((yy - 50) / 80.0, 0, 1)
    top = np.array([12, 13, 12], np.float32)
    low = np.array([34, 30, 24], np.float32)
    rgb = top * (1 - t[..., None]) + low * t[..., None]
    out = np.dstack([rgb, alpha * 255])
    if glimmer:
        # far inside, a faint warm glow: the cave's light at the tunnel's end
        g = np.exp(-(((xx - 44) / 9.0) ** 2 + ((yy - 84) / 14.0) ** 2)) * 0.22 * a
        out[..., :3] = out[..., :3] * (1 - g[..., None]) + np.array([210, 160, 90], np.float32) * g[..., None]
    return Image.fromarray(np.clip(out, 0, 255).astype(np.uint8), 'RGBA')


def save(path, frames):
    w, h = frames[0].size
    sheet = Image.new('RGBA', (w, h * len(frames)))
    for i, f in enumerate(frames):
        sheet.paste(f, (0, i * h))
    info = PngImagePlugin.PngInfo()
    info.add_text('openc2e-frames', str(len(frames)))
    sheet.save(path, pnginfo=info)


if __name__ == '__main__':
    out = sys.argv[1]
    save(os.path.join(out, 'zcvm.png'), list(mouth()))
    save(os.path.join(out, 'zcva.png'), [arch(g) for g in (False, True)])
    print('wrote zcvm.png and zcva.png to', out)
