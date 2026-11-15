"""The passage between the volcano's east end and the potion room (owner,
2026-11-13), made on the player's own machine from their installed Creatures 2
(bring-your-own-data: only this recipe is part of the project, never the
game's pixels).

The volcano's east ledge (room 167, walking line y 2189) and the potion room
across the world's east-west seam (room 105, floor y 2329) were parted by a
sealed rock pillar (room 213, x 8240-8352). tools/additions/volcano-passage.cos
opens a way through it; this draws what goes with it:

  zvps  the seam hidden, x 8272 to 80 across it, y 1700-2399: three bands
        that cross-fade (above the passage the volcano rock darkens into the
        corner, leaving the machine room's cone machines alone; beside it the
        two rocks meet along a ragged fissure kept east of x 8 in the potion
        room, whose picture's own border lies at x 0; the floor band joins the
        two floors); each side carried past the seam by quilting a clean patch
        of its own rock
  zvpc  the passage carved out of the rock, mostly by light: darker inside,
        darkest under its ceiling, a warm lit lip, lava light spilling in at
        the door end (the rock behind it blended softly, no fissure)
  zvpr  the ramp: the ground rock both rooms stand on, shaded darker than the
        rock behind, its walking surface the volcano ledge's crust with a few
        embers turning into the floor rock, its foot fading into the floor
  zvdr  the volcano's own hatch door (the southwest one, dor2), mirrored:
        5 frames, 0 shut (seen edge-on), 4 open

    python3 volcano_passage.py C2_DIR USER_DATA_DIR

writes USER_DATA_DIR/Images/zvps.png, zvpc.png, zvpr.png, zvdr.png and prints
where each goes (volcano-passage.cos places them).
"""
import os, sys
import numpy as np
from PIL import Image, ImageFilter, ImageOps, PngImagePlugin
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from background import region
import s16

# the ramp's walking line: from the ledge's lip to the potion room's floor
# (the room map follows the same line)
# (from where the lava slab stops on the ledge to just short of the potion
# room's bookshelf arch, nearly straight: about 33 degrees at its steepest;
# at 40 degrees creatures could not walk up it, owner 2026-11-13)
LIP_X, LIP_Y, FLOOR_Y = 8205, 2189, 2329
RUN = (8352 - LIP_X) + 80                     # to x 80 in the potion room


def walk_y(wx):
    """the walking line's height at world x (x past 8352 continues as 0..)"""
    wx = np.asarray(wx, np.float32)
    t = np.clip((wx - LIP_X) / RUN, 0, 1)
    return LIP_Y + (t * t * (3 - 2 * t) * 0.15 + t * 0.85) * (FLOOR_Y - LIP_Y)


def smooth_noise(n, cell, seed):
    r = np.random.default_rng(seed)
    pts = r.random(n // cell + 3)
    xs = np.arange(n) / cell
    i = xs.astype(int); f = xs - i; f = f * f * (3 - 2 * f)
    return pts[i] * (1 - f) + pts[i + 1] * f

def quilt(patch, w, h, seed, block=36, feather=8):
    r = np.random.default_rng(seed)
    acc = np.zeros((h, w, 3), np.float32); wsum = np.zeros((h, w, 1), np.float32)
    ph, pw = patch.shape[:2]
    step = block - feather
    yy, xx = np.mgrid[0:block, 0:block]
    edge = np.minimum.reduce([xx, yy, block - 1 - xx, block - 1 - yy]).astype(np.float32)
    wt = np.clip((edge + 1) / feather, 0.05, 1)[..., None]
    for y0 in range(-feather, h, step):
        for x0 in range(-feather, w, step):
            sy = r.integers(0, ph - block); sx = r.integers(0, pw - block)
            blk = patch[sy:sy + block, sx:sx + block]
            if r.random() < 0.5:
                blk = blk[:, ::-1]
            ys, xs = max(0, y0), max(0, x0)
            ye, xe = min(h, y0 + block), min(w, x0 + block)
            if ye <= ys or xe <= xs:
                continue
            acc[ys:ye, xs:xe] += blk[ys - y0:ye - y0, xs - x0:xe - x0] * wt[ys - y0:ye - y0, xs - x0:xe - x0]
            wsum[ys:ye, xs:xe] += wt[ys - y0:ye - y0, xs - x0:xe - x0]
    return acc / np.maximum(wsum, 1e-3)


def make_seam(C2, soft=False):
    HALF = 80; Y0, Y1 = 1700, 2399; W = 2 * HALF
    def patch(x0, y0, x1, y1):
        return np.asarray(region(C2, x0, y0, x1, y1).convert('RGB')).astype(np.float32)
    def band(y0, y1, half, pl, pr, seed, amp, shift=0, minb=-1e9):
        """one band: each side carried past the seam with its own rock, joined"""
        H = y1 - y0
        L = patch(8352 - HALF, y0, 8352, y1); R = patch(0, y0, HALF, y1)
        def q(pt, sd):
            blk = int(min(36, pt.shape[0] - 2, pt.shape[1] - 2))
            return quilt(pt, W, H, sd, block=blk, feather=max(3, blk // 4))
        # each picture fades into its own continuation over the last 24 px before
        # the seam (an abrupt switch there drew a hairline down the seam)
        xs = np.arange(W)[None, :, None].astype(np.float32)
        keepL = np.clip((HALF - xs) / 24.0, 0, 1)          # 1 well west of the seam, 0 at it
        keepR = np.clip((xs - HALF) / 24.0, 0, 1)
        Lfull = np.concatenate([L, np.zeros_like(L)], axis=1); Rfull = np.concatenate([np.zeros_like(R), R], axis=1)
        L2full = Lfull
        Lext = Lfull * keepL + q(pl, seed) * (1 - keepL)
        Rext = Rfull * keepR + q(pr, seed + 1) * (1 - keepR)
        b = HALF + shift + (smooth_noise(H, 90, seed + 2) - 0.5) * amp + (smooth_noise(H, 25, seed + 3) - 0.5) * amp * 0.38 + (smooth_noise(H, 6, seed + 4) - 0.5) * 6
        # (never west of x 8 in the potion room: its picture's own left border,
        # the edges of its stone block and wooden recess, lies at x 0)
        b = np.maximum(b, HALF + minb)
        x = np.arange(W)[None, :]
        d = x - b[:, None]
        w = 1 / (1 + np.exp(-d / (22.0 if soft else 5.0)))
        img = Lext * (1 - w[..., None]) + Rext * w[..., None]
        if not soft:
            wid = 4.0 + 6.0 * smooth_noise(H, 40, seed + 5)[:, None]
            img *= (1 - 0.55 * np.exp(-(d / wid) ** 2) - 0.15 * np.exp(-(d / 28.0) ** 2))[..., None]
        # only `half` px each side of the seam: fade out beyond
        a = np.clip((half - np.abs(x - HALF + 0.5)) / 12.0, 0, 1) * np.ones((H, 1))
        if half < 0:
            # the volcano side only (above the passage the machine room's machines
            # stand right at the seam): its rock darkens into the corner instead
            corner = np.clip((x - (HALF - 34)) / 34.0, 0, 1)
            img = L2full * (1 - 0.6 * corner ** 1.5)[..., None]
            a = np.clip((HALF - x) / 2.0, 0, 1) * np.clip((x - (HALF - 40)) / 8.0, 0, 1) * np.ones((H, 1))
        return np.clip(img, 0, 255), a
    canvas = np.zeros((Y1 - Y0, W, 3), np.float32); acc = np.zeros((Y1 - Y0, W), np.float32)
    bands = [
        # above the passage: the volcano's rock / the machine room's dark wall left of its cone machines
        (1700, 1860, -1, patch(8290, 1705, 8350, 1840), patch(1, 1705, 26, 1830), 41, 20, 0, -1e9),
        # beside the passage: the pillar's rock / the potion room's cracked stone
        (1830, 2345, 80, patch(8280, 2020, 8350, 2160), patch(2, 1850, 74, 1935), 11, 60, 24, 8),
        # the floor both rooms stand on
        (2315, 2399, 70, patch(8000, 2340, 8340, 2395), patch(80, 2340, 380, 2395), 61, 60, 0, -1e9),
    ]
    for y0, y1, half, pl, pr, seed, amp, shift, minb in bands:
        img, a = band(y0, y1, half, pl, pr, seed, amp, shift, minb)
        H = y1 - y0
        # cross-fade with the bands above and below over 30 px
        ramp = np.ones(H)
        if y0 > Y0: ramp *= np.clip(np.arange(H) / 30.0, 0, 1)
        if y1 < Y1: ramp *= np.clip((H - 1 - np.arange(H)) / 30.0, 0, 1)
        a = a * ramp[:, None]
        sl = slice(y0 - Y0, y1 - Y0)
        canvas[sl] = canvas[sl] * (1 - a[..., None]) + img * a[..., None]
        acc[sl] = acc[sl] + a * (1 - acc[sl])
    rgb = np.where(acc[..., None] > 0, canvas / np.maximum(acc[..., None], 1e-3) * np.minimum(acc[..., None], 1) / np.maximum(acc[..., None], 1e-3) * acc[..., None], 0)
    rgb = canvas / np.maximum(acc[..., None], 1e-3)
    return Image.fromarray(np.dstack([np.clip(rgb, 0, 255).astype(np.uint8), (np.clip(acc, 0, 1) * 255).astype(np.uint8)]), 'RGBA')


def make_carve(C2, seam_img):
    # (from just behind the door's post, x 8225: the hollow reaches the door;
    # it started 8 px past the post and rounded off there, owner 2026-11-13)
    X0, Y0 = 8215, 2010            # world; the area runs across the seam into the potion room
    W, H = (8352 - X0) + 90, 2335 - Y0
    # the rock as it looks with the seam transition (its picture: x 8272.., y 1845..)
    left = region(C2, X0, Y0, 8352, Y0 + H).convert('RGBA'); right = region(C2, 0, Y0, W - (8352 - X0), Y0 + H).convert('RGBA')
    base = Image.new('RGBA', (W, H)); base.paste(left, (0, 0)); base.paste(right, (8352 - X0, 0))
    # (the soft seam: inside the passage the walls flow into each other, no fissure)
    base.alpha_composite(seam_img, (8272 - X0, 1700 - Y0))
    rock = np.asarray(base.convert('RGB')).astype(np.float32)
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    # the ramp's walking line (as the ramp sprite: from the ledge's lip at x 8238,
    # y 2192, to the potion room's floor, y 2329, by x 60)
    t = np.clip((xx[0] - (LIP_X - X0)) / RUN, 0, 1)
    floor = walk_y(xx[0] + X0) - Y0
    # the hollow's ceiling: about 125 px above the walking line, arched at the
    # door end, a little rough
    noise = smooth_noise
    ceil = floor - 125 + (noise(W, 30, 5) - 0.5) * 10
    ceil = np.maximum(ceil, 6)
    depth_in = yy - ceil[None, :]                         # below the ceiling line: inside
    inside = 1 / (1 + np.exp(-depth_in / 2.0))
    # the hollow's ends fade: it opens from the door post and into the potion room's recess
    ends = np.clip((W - 1 - xx) / 30, 0, 1) * np.ones_like(xx)   # (open at the door end: the post covers it)
    # lighting: darker inside, darkest under the ceiling (its shadow); a warm lit
    # lip along the ceiling's edge, strongest by the volcano's glow
    shadow = 0.34 - 0.18 * np.exp(-np.clip(depth_in, 0, None) / 22) + 0.3 * np.clip(depth_in / (floor - ceil)[None, :], 0, 1) ** 1.5
    light = rock * (1 - inside[..., None] * (1 - shadow[..., None]) * ends[..., None])
    glow = np.exp(-((depth_in + 2.0) / 2.8) ** 2) * np.clip(1.1 - xx / W, 0.25, 1) * ends
    warm = np.array([255, 150, 70], np.float32)
    light = light * (1 + 0.8 * glow[..., None]) + warm * 0.28 * glow[..., None]
    # warm light from the lava spilling in at the door end, fading deeper in
    mid = (ceil + floor) / 2
    dist = np.sqrt((xx - 25) ** 2 + ((yy - mid[None, :]) * 1.3) ** 2)   # (from the door, x 8240)
    spill = np.exp(-dist / 75) * inside * ends
    light = light + (warm * 0.55 - light * 0.2) * (0.5 * spill)[..., None]
    # an outer shadow just above the lip (the rock face turning in)
    outer = np.exp(-((depth_in + 6) / 4) ** 2) * ends
    light *= (1 - 0.35 * outer)[..., None]
    img = np.clip(light, 0, 255)
    a = np.clip(np.maximum(inside * ends, glow + outer) * 255, 0, 255)
    a = np.where(yy > floor[None, :] + 4, 0, a)            # nothing below the walking line (the ramp is there)
    return Image.fromarray(np.dstack([img.astype(np.uint8), a.astype(np.uint8)]), 'RGBA')


def make_ramp(C2):
    X0, YTOP, YEND = LIP_X, LIP_Y, FLOOR_Y
    W, H = RUN + 26, YEND - YTOP + 14
    r = np.random.default_rng(31)
    # body: the ground rock both rooms stand on, quilted
    ground = np.asarray(region(C2, 8000, 2340, 8340, 2395).convert('RGB')).astype(np.float32)
    body = quilt(ground, W, H, 21, block=30, feather=8)
    crust = quilt(np.asarray(region(C2, 8100, 2194, 8230, 2212).convert('RGB')).astype(np.float32), W, 12, 22, block=10, feather=3)
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    t = np.clip(xx[0] / RUN, 0, 1)
    ease = t * t * (3 - 2 * t) * 0.15 + t * 0.85
    top = ease * (YEND - YTOP) + np.sin(xx[0] * 0.45) * 0.8 + np.sin(xx[0] * 0.13 + 1.7) * 1.2
    depth = yy - top[None, :]
    inside = depth >= 0
    # the face: shaded darker than the rock behind, darker low down and near the ledge
    # (toward its foot the face lightens back to the ground's own tone, as it
    # settles into the floor)
    toe = np.clip((yy - (H - 34)) / 30, 0, 1)
    img = body * ((0.62 - 0.22 * np.clip(depth / 60, 0, 1) - 0.12 * np.clip(1 - xx / 50, 0, 1)) * (1 - toe) + 1.0 * toe)[..., None]
    # the walking surface: a band of the ledge's black crust with a few embers
    # near the volcano, turning into the mottled floor rock toward the potion room
    band = np.clip(1 - depth / 9, 0, 1) * inside
    cr = np.zeros_like(img)
    for x in range(W):
        for k in range(12):
            y = int(top[x]) + k
            if 0 <= y < H:
                cr[y, x] = crust[k, x]
    mix = np.clip(1 - xx / (W * 0.85), 0.15, 1)[..., None]
    surf = (cr * 0.5 + body * 0.5) * mix * 1.25 + body * (1 - mix) * 1.35
    img = img * (1 - band[..., None]) + surf * band[..., None]
    emb = (r.random((H, W)) < 0.018 * np.clip(1 - xx / (W * 0.6), 0, 1)) & (depth >= 0) & (depth < 4)
    img[emb] = np.array([235, 90 + r.integers(0, 60), 30])[None, :] * 0 + np.array([235, 110, 35])
    # a lit rim along the top and a thin dark line above it to part it from the wall
    img *= (1 + 0.9 * np.exp(-(depth / 1.4) ** 2))[..., None]
    alpha = inside.astype(np.float32)
    line = np.exp(-((depth + 1.2) / 0.8) ** 2)
    img = img * (1 - 0.75 * line[..., None]) ; alpha = np.maximum(alpha, line * 0.9)
    alpha *= np.clip(xx / 6, 0, 1) ** 0.5 * np.clip((W - 1 - xx) / 18, 0, 1)
    # under the ledge's lip the face fades into the rock below it (no hard edge)
    alpha *= np.where(depth > 10, np.clip(xx / 26, 0.0, 1), 1)
    # its foot fades into the floor over its last stretch, along an uneven line
    footline = (H - 26) + (np.sin(xx[0] * 0.21) * 3 + np.sin(xx[0] * 0.057 + 2.0) * 5)
    alpha *= np.clip(1 - (yy - footline[None, :]) / 22, 0, 1)
    # and the low end of the slope melts into the floor the same way
    alpha *= np.where(depth > 2, np.clip((W - 1 - xx) / 40, 0, 1), 1)
    a = np.asarray(Image.fromarray((np.clip(alpha, 0, 1) * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(0.6)))
    return Image.fromarray(np.dstack([np.clip(img, 0, 255).astype(np.uint8), a]), 'RGBA')


def save(img, path, frames=1):
    info = PngImagePlugin.PngInfo()
    info.add_text('openc2e-frames', str(frames))
    img.save(path, pnginfo=info)


def main():
    c2, user = sys.argv[1], sys.argv[2]
    out = os.path.join(user, 'Images')
    os.makedirs(out, exist_ok=True)
    seam = make_seam(c2)
    save(seam, os.path.join(out, 'zvps.png'))
    print('zvps', seam.size, 'top-left 8272 1700')
    carve = make_carve(c2, make_seam(c2, soft=True))
    save(carve, os.path.join(out, 'zvpc.png'))
    print('zvpc', carve.size, 'top-left 8215 2010')
    ramp = make_ramp(c2)
    save(ramp, os.path.join(out, 'zvpr.png'))
    print('zvpr', ramp.size, 'top-left', LIP_X, LIP_Y - 1)
    frames = [ImageOps.mirror(f) for f in s16.read(os.path.join(c2, 'Images', 'dor2.s16'))]
    w, h = frames[0].size
    strip = Image.new('RGBA', (w, h * len(frames)))
    for i, f in enumerate(frames):
        strip.alpha_composite(f, (0, i * h))
    save(strip, os.path.join(out, 'zvdr.png'), len(frames))
    print('zvdr', (w, h), len(frames), 'frames, top-left 8055 2028')


if __name__ == '__main__':
    main()
