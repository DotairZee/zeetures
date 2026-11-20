"""The glass tube from the domed lab's dock to the gene splice lab, redone
(owner, mock-up approved 2026-11-19): it slopes down at about 10.4 degrees
from the dock's collar (x ~4780) to the gene lab's floor (y 1798), so
creatures can walk it; an overhanging rock face hides its east end, which
creatures walk out past into the gene lab, widened west with a newly drawn
wall, pipe and beam; stalactites hang under the cave arch; through the
glass shows the cave behind it, dithered and muted; the dock's collar ring
is turned to the tube's angle; the cage lift's old top deck by the huts is
taken down to its crane, lamp and pulley housing (owner, 2026-11-19: no
longer walkable), held up by our own bamboo props (lift_deck_props.py,
ztdp). Made on the player's own machine from their
installed Creatures 2 (bring-your-own-data: only this recipe and our own
annotation mask in tube_walk_data/ are part of the project).

The scene is the game's background (x 4400-6740, y 1250-2090) with its
scenery sprites over it (swmp.s16 frames 0-3, the old tube's front piece and
the scaffold's), reworked. Three pictures:
- ztwb, behind creatures: the reworked scene where it differs from the
  background (the old tube painted out, the new one, the lab's west end,
  the stalactites, the rock), transparent elsewhere.
- ztwf, in front of creatures: the tube's glass on a checkerboard (a
  creature in it shows through, as if behind glass), its rings, the turned
  dock collar, and what the owner marked as standing in front of the tube.
- ztwr, in front of creatures: the rock face over the tube's east end.
- ztwg, in front of creatures: the gene lab's top beam (genc.s16 frame 0,
  at 5995,1478) without its west end (the old rock edge and arched bracket,
  which would cover the lab's new west wall): transparent west of x 6125.
The game's own scenery pieces here (swmp.s16 frames 0-3) are drawn into
these pictures (less the old deck's parts), so they are not needed as
objects any more.
Each is printed with its size and world top left. The whole scene goes to
USER_DATA_DIR/tube_walk_final.png, for checking.

    python3 tube_walk.py C2_DIR USER_DATA_DIR
"""
import math
import os
import sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, PngImagePlugin
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from background import region
import s16
import tube_walk_lib as lib

OX, OY, W, H = 4400, 1250, 2340, 840        # the scene, in world coordinates
SCENERY = ((0, (4735, 1410)), (1, (4933, 1556)), (2, (5042, 1467)), (3, (5212, 1555)))   # swmp.s16 frames
DATA = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'tube_walk_data')
# what stands in front of the tube (the owner's marking, 2026-11-19): the
# bamboo village's huts, posts and ropes painted below and over the old
# tube, and the scaffold's scenery pieces (the game's own objects there, the
# lift's cabin, lamps, plants and the walkway's railings, are left out: they
# draw themselves); 1-bit, its top left here
FRONT_AT = (4780, 1460)

# the old tube
TX, TY = 4735, 1410                         # its front piece (swmp frame 0)
X0, X1 = 4780, 6030                         # its span (dock side to cap)
SH = 170                                    # rows of the straightened tube
LAB_FLOOR = 1798


def top_of(x):
    """The old tube's top edge."""
    return 1438 + (x - 4835) * (1496 - 1438) / (5935 - 4835)


# the new tube
WD = 110                                    # the lab widened this much into the rock
EXIT_X = 6113 - WD                          # the lab's new west edge
END_X = 5965                                # where the tube's end lies, inside the rock
FLOOR_OFF = 134                             # the walking line: 2 px above the glass bottom (top + 136); owner, 2026-11-19
ENTRY = 5928                                # where the tube meets the rock (east of the scaffold tower, x 5751-5920)
start_top = top_of(X0)
slope = (LAB_FLOOR - FLOOR_OFF - start_top) / (EXIT_X + 20 - X0)
ang = math.degrees(math.atan(slope))


def tube_y(x):
    return start_top + slope * (x - X0)


# the owner's circles (2026-11-19, on the game's picture, top left 4780,1380):
# the branches that crossed the old tube, kept as background behind the new one
_C = (4780, 1380)
BRANCHES = [[(x + _C[0], y + _C[1]) for x, y in p] for p in (
    [(190, 70), (215, 5), (232, 10), (226, 90), (252, 180), (225, 188), (205, 140)],
    [(308, 150), (345, 122), (378, 128), (362, 188), (318, 188)],
    [(388, 70), (405, 8), (422, 14), (452, 100), (457, 182), (442, 222), (408, 218), (414, 150)])]
# owner, 2026-11-19 (zoomed-out view): stalactites under the cave arch:
# (x, y, width, length, lean), from the ceiling band's lower edge, and inside the arch
STALACTITES = [
    (5392, 1326, 16, 58, 0.05), (5455, 1336, 10, 34, -0.03), (5512, 1302, 18, 70, 0.04),
    (5650, 1318, 12, 40, 0.0), (5790, 1326, 17, 62, 0.03), (5838, 1350, 9, 30, 0.0),
    (5655, 1394, 7, 22, 0.0), (5672, 1384, 10, 40, 0.0), (5694, 1378, 17, 78, 0.03),
    (5716, 1380, 11, 48, -0.02), (5740, 1390, 8, 28, 0.0)]
# the cage lift's old top deck (owner, 2026-11-19: no longer walkable, the
# pulley down one level, the deck's remains held up on props): what is taken
# out of the scene; the crane, its lamp and the double pulley's housing stay
DECK_RM = [
    (5214, 1600, 5320, 1627),       # the old beam past the pulley's housing
    (5262, 1508, 5294, 1612),       # the top stop's caller
    (4895, 1555, 4997, 1626),       # box A and the deck's west end
    (5207, 1550, 5300, 1612),       # box D
    (5275, 1600, 5398, 1626)]       # the deck's east end


def put_scenery(out, swmp, frames, cut=None):
    """Composite swmp frames at their places (less cut, world-sized bool)."""
    for frame, (px, py) in frames:
        f = swmp[frame]
        h, w = f.shape[:2]
        a = f[..., 3:] / 255
        if cut is not None:
            a = a * (~cut[py - OY:py - OY + h, px - OX:px - OX + w])[..., None]
        reg = out[py - OY:py - OY + h, px - OX:px - OX + w]
        reg[:] = reg * (1 - a) + f[..., :3] * a


def paste_rot_masked(out, pc, angle_deg, left_top, keep_out):
    """Turn an RGBA piece by angle (down to the east positive) about its top
    left corner, which lands at left_top; pasted where its alpha is over half
    and keep_out is not set."""
    im = Image.fromarray(pc.clip(0, 255).astype(np.uint8), 'RGBA')
    r = im.rotate(-angle_deg, resample=Image.BICUBIC, expand=True)
    th = math.radians(angle_deg)
    w0, h0 = im.size
    dx, dy = -w0 / 2, -h0 / 2               # (PIL turns about the center)
    kx = dx * math.cos(th) - dy * math.sin(th) + r.width / 2
    ky = dx * math.sin(th) + dy * math.cos(th) + r.height / 2
    ox, oy = int(round(left_top[0] - kx - OX)), int(round(left_top[1] - ky - OY))
    a = np.asarray(r).astype(np.float32)
    al = (a[..., 3] > 128).astype(np.float32)
    y0, x0 = max(0, oy), max(0, ox)
    y1, x1 = min(out.shape[0], oy + a.shape[0]), min(out.shape[1], ox + a.shape[1])
    sub = a[y0 - oy:y1 - oy, x0 - ox:x1 - ox]
    sal = al[y0 - oy:y1 - oy, x0 - ox:x1 - ox] * (~keep_out[y0:y1, x0:x1])
    reg = out[y0:y1, x0:x1]
    reg[:] = reg * (1 - sal[..., None]) + sub[..., :3] * sal[..., None]


def blob(points, jitter, seed):
    """A rock outline: the polygon's edges broken up with seeded jitter."""
    r = np.random.default_rng(seed)
    pts = []
    for i in range(len(points)):
        (ax, ay), (bx, by) = points[i], points[(i + 1) % len(points)]
        n = max(2, int(math.hypot(bx - ax, by - ay) / 14))
        for k in range(n):
            t = k / n
            pts.append((ax + (bx - ax) * t + r.uniform(-jitter, jitter), ay + (by - ay) * t + r.uniform(-jitter, jitter)))
    return pts


def smooth_fill(F, M, iters=1200, vk=0.35, div=2.7):
    """Fill M in F smoothly from around it (relaxation, mostly sideways:
    vertical neighbors weighted vk; div = 2 + 2 vk)."""
    F[M] = F[~M].mean(axis=0)
    for _ in range(iters):
        avg = (np.roll(F, 1, 1) + np.roll(F, -1, 1) + vk * (np.roll(F, 1, 0) + np.roll(F, -1, 0))) / div
        F[M] = avg[M]


def deck_fill(img, deck_rm, box, seed, inside=None):
    """The old deck's place in img, a little wider (MaxFilter 5), less
    inside, filled smoothly from around it with a faint grain."""
    y0, y1, x0, x1 = box
    F = img[y0:y1, x0:x1].copy()
    M = np.asarray(Image.fromarray((deck_rm[y0:y1, x0:x1] * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(5))) > 0
    if inside is not None:
        M &= ~inside[y0:y1, x0:x1]
    if not M.any():
        return
    smooth_fill(F, M, 2500, 0.5, 3.0)
    F[M] *= (1 + 0.06 * np.random.default_rng(seed).normal(0, 1, (M.sum(), 1)))
    img[y0:y1, x0:x1] = F


def old_tube(tubecomp, ta):
    """The old tube's outline (front piece, filled top to bottom, bottom kept
    within the tube's own depth so scaffold bits stay), and the tube as it
    looks, straightened (each column moved up to a level top)."""
    tw = ta.shape[1]
    mask = np.zeros((H, W), bool)
    for x in range(X0, X1 + 1):
        c = x - TX
        if not (0 <= c < tw):
            continue
        rows = np.nonzero(ta[:, c])[0]
        if not len(rows):
            continue
        t = TY + rows.min()
        b = min(TY + rows.max(), int(top_of(x)) + 146)
        mask[max(0, t - 3 - OY):b + 3 - OY, x - OX] = True
    strip = np.zeros((SH, X1 - X0 + 1, 4), np.float32)
    for x in range(X0, X1 + 1):
        t0 = int(round(top_of(x))) - 12
        strip[:, x - X0, :3] = tubecomp[t0 - OY:t0 - OY + SH, x - OX]
        strip[:, x - X0, 3] = mask[t0 - OY:t0 - OY + SH, x - OX] * 255
    return mask, strip


def new_tube(strip):
    """The tube built from one clean ring-to-ring section (x 5630-5930)
    repeated: west of x 5330 the background paints the bamboo platform, its
    mast, a pulley and branches over the tube, which came along when the
    whole tube was moved (owner, 2026-11-19)."""
    MOD = strip[:, 5630 - X0:5930 - X0].copy()
    # the module's lower half shows the vines behind the old tube through its
    # glass (yellow specks along its rim, repeated with every module): take
    # the yellow out there, keeping the light and shade
    lo = MOD[80:, :, :3]
    warm = (lo[..., 0] - lo[..., 2] > 18) & (MOD[80:, :, 3] > 0)
    ref = lo[~warm & (MOD[80:, :, 3] > 0)].mean(axis=0)
    lum = lo.mean(axis=2, keepdims=True)
    lo[warm] = (lum * ref / ref.mean())[warm]
    MOD[80:, :, :3] = lo
    length = math.hypot(END_X - X0, start_top + slope * (END_X - X0) - start_top)
    parts = [strip[:, 0:20]]
    n = 20
    while n < length:
        parts.append(MOD)
        n += MOD.shape[1]
    return MOD, np.concatenate(parts, axis=1)[:, :int(length)]


def paint_out(scene, bg, bgt, mask, strip):
    """The old tube painted out: its band quilted from the rock and bark
    above, toned and textured to match, the owner's branches put back."""
    base = lib.quilt_fill(scene, OX, OY, top_of, X0 - 4, X1 + 6, bgt, OY - 300)
    band = mask.copy()
    for x in range(X0 - 4, X1 + 6):
        t = int(round(top_of(x)))
        band[t - 16 - OY:t + 152 - OY, x - OX] = True
    base = lib.tone_correct(base, bg, band)
    base = lib.match_texture(base, bg, band)
    return lib.restore_branches(base, scene, strip[:, 5630 - X0:5930 - X0], top_of, OX, OY, BRANCHES)


def lab_west_end(base, bg):
    """The gene lab's new west wall strip (x 5990-6172), joined to the room."""
    lw, la = lib.lab_wall()
    WX = lib.WALL_X0
    # a fluid join with the room (owner, 2026-11-19: no divider): the strip's
    # color graded toward the room's own first columns over its last 60 px
    lab = bg[1478 - OY:1814 - OY, 6127 - OX:6157 - OX]
    rows_lab = lab.mean(axis=1)                                   # per row, the room's tone at the join
    rows_new = lw[:, -30:].mean(axis=1)
    ratio = np.clip((rows_lab + 8) / (rows_new + 8), 0.6, 1.8)
    ratio[1626 - 1478:1673 - 1478] = 1.0      # the pipe's rows keep their own light (it should read clearly)
    # below the pipe the room beside the join is dark rock and the copper elbow: take the upper wall's tone there
    ratio[1660 - 1478:] = ratio[1527 - 1478:1627 - 1478].mean(axis=0)
    ratio = np.asarray(np.clip(ratio * 100, 0, 255).astype(np.uint8)).astype(np.float32) / 100   # (8-bit, as the mock-up's)
    wcols = lw.shape[1]
    for i in range(wcols):
        g = np.clip((i - (wcols - 60)) / 60, 0, 1)
        lw[:, i] = lw[:, i] * (1 + (ratio - 1) * g)
    # bold like the room (owner, 2026-11-19: it looked faded and ghostly):
    # the strip's brightness remapped to the room's wall (its olive panels
    # beside the join, not its bright machines), each pixel keeping its color
    room = bg[1520 - OY:1640 - OY, 6113 - OX:6150 - OX].mean(axis=2).ravel()
    pre = lw.copy()
    lum = lw.mean(axis=2)
    src = np.sort(lum[la > 0.5])
    dst = np.sort(room)
    q = np.searchsorted(src, lum, side='left') / max(1, len(src) - 1)
    new = np.interp(np.clip(q, 0, 1), np.linspace(0, 1, len(dst)), dst)
    lw = lw * ((new + 1) / (lum + 1))[..., None]
    g = lw.mean(axis=2, keepdims=True)
    lw = np.clip(g + (lw - g) * 0.55, 0, 255)
    lw *= 0.8                                          # muted: a little darker than the room's lit wall
    # the pipe keeps its own round shading (owner: it read as a flat block)
    lw[1624 - 1478:1672 - 1478] = pre[1624 - 1478:1672 - 1478] * 0.92
    # the upper wall graded into the room's warm lit wall beside it (owner,
    # 2026-11-19): color and light per row moved toward the room's wall
    # (x 6127-6150) in a long, even ramp from the rock's edge (x 6060) to the
    # room wall (x 6136); owner: fade over a greater distance
    for y in range(1519, 1624):
        r = y - 1478
        tg = bg[y - OY, 6127 - OX:6150 - OX].mean(axis=0)
        ow = lw[r, 6080 - WX:6100 - WX].mean(axis=0)
        rt = np.clip((tg + 6) / (ow + 6), 0.6, 2.2)
        for x in range(6045, WX + lw.shape[1]):
            u = np.clip((x - 6060) / 76.0, 0, 1)
            lw[r, x - WX] *= 1 + (rt - 1) * u
    # fade out across the room's first columns (x 6099-6127), so the room's
    # art takes over gradually: no edge at the join; the upper wall hands
    # over to the room's own from x 6092 (a longer fade)
    xs = np.arange(WX, WX + lw.shape[1])
    t = np.clip((xs - 6099) / 28.0, 0, 1)
    fade = np.repeat((1 - t * t * (3 - 2 * t))[None, :], lw.shape[0], axis=0)
    t2 = np.clip((xs - 6092) / 50.0, 0, 1)
    fade[1519 - 1478:1624 - 1478] = (1 - t2 * t2 * (3 - 2 * t2))[None, :]
    # below the pipe (y 1660 on): the panel wall runs right up to the copper
    # elbow's left edge (or to x 6146 above it), cut cleanly: the pipe stands
    # in front of the wall (owner, 2026-11-19: like the upper wall, not a
    # muddy fade); up to the room's next object, the copper pipe from its
    # shadow side on, or the drum (the dark red rock between has almost no
    # green in it)
    for y in range(1660, 1814):
        e = 6170 if y < 1703 else 6146
        for x in range(6105, 6170):
            if bg[y - OY, x - OX, 1] > 20:
                e = x
                break
        fade[y - 1478] = np.clip((e + 1 - xs) / 2.0, 0, 1)
    la = la * fade
    # the room's blue cable, which hangs down through here, stays in front
    bgs = bg[1478 - OY:1814 - OY, WX - OX:WX + lw.shape[1] - OX]
    blue = (bgs[..., 2] > bgs[..., 0] + 20) & (bgs[..., 2] > 60)
    blue[:, :6100 - WX] = False
    blue = np.asarray(Image.fromarray((blue * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(3))) > 0
    la = la * (~blue)
    reg = base[1478 - OY:1814 - OY, WX - OX:WX + lw.shape[1] - OX]
    reg[:] = reg * (1 - la[..., None]) + lw * la[..., None]
    # the wall fades into the room's rusty brown (owner, 2026-11-19, the dark
    # red behind the cable), the upper wall too: its light and shade kept,
    # its color turned to that rust in a long, even ramp over x 6012-6128
    # (owner: fade more gradually from one side to the other); not the pipe
    rr = bg[1664 - OY:1700 - OY, 6110 - OX:6165 - OX].reshape(-1, 3)
    rr = rr[(rr[:, 0] > 2 * rr[:, 1] + 8) & (rr[:, 2] < 30)]
    rust = rr.mean(axis=0)
    ys0, ys1 = 1519, 1800
    blk = base[ys0 - OY:ys1 - OY, 6005 - OX:WX + lw.shape[1] - OX]
    al = la[ys0 - 1478:ys1 - 1478, 6005 - WX:].copy()
    al[1624 - ys0:1668 - ys0] = 0
    lm = blk.mean(axis=2, keepdims=True)
    rec = rust * np.clip(0.55 + 0.45 * lm / (lm[al > 0.5].mean() + 1), 0.4, 1.6)
    u = np.clip((np.arange(6005, WX + lw.shape[1]) - 6012) / 116.0, 0, 1)
    u = u[None, :, None] * al[..., None]
    blk[:] = blk * (1 - u) + rec * u
    # the pipe's surface taken from the room's own pipe (owner, 2026-11-19:
    # match the adjacent piping): columns of it between the rock and its
    # flange (x 6112-6121, y 1626-1667), chosen and varied column by column
    # so they do not repeat, laid along the new stretch; the flange keeps its shape
    src = bg[1626 - OY:1668 - OY, 6112 - OX:6122 - OX]
    rng = np.random.default_rng(12)
    walk = 0.0
    rings = base[1620 - OY:1674 - OY, 6058 - OX:6079 - OX].copy()      # (our two rings, kept)
    for x in range(5990, 6122):
        walk = 0.85 * walk + rng.normal(0, 0.6)
        k = int(abs(x * 3 + int(walk * 4))) % src.shape[1]
        base[1626 - OY:1668 - OY, x - OX] = src[:, k] * (1 + 0.06 * np.tanh(walk))
    # smoothed along the pipe's length (a pipe varies round it, not along
    # it): no vertical ribbing from the column copies
    seg = base[1626 - OY:1668 - OY, 5990 - OX:6122 - OX]
    pad = np.concatenate([seg[:, :1].repeat(8, 1), seg, seg[:, -1:].repeat(8, 1)], axis=1)
    kern = lib.ex(-0.5 * (np.arange(-8, 9) / 1.6) ** 2); kern /= kern.sum()
    sm = np.zeros_like(seg)
    for i, kv in enumerate(kern):
        sm += kv * pad[:, i:i + seg.shape[1]]
    seg[:] = sm
    # the room pipe's fine grain over the smoothed surface, rust mottling in
    # patches, and a few darker spots
    PW = 6122 - 5990
    gr = rng.normal(0, 1, (1668 - 1626, PW)).astype(np.float32)
    gr = np.asarray(Image.fromarray(np.clip(gr * 40 + 128, 0, 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(0.6))).astype(np.float32) / 128 - 1
    mo = rng.random((6, max(4, PW // 8))).astype(np.float32)
    mo = np.asarray(Image.fromarray((mo * 255).astype(np.uint8)).resize((PW, 1668 - 1626), Image.BICUBIC)).astype(np.float32) / 255 - 0.5
    sp = rng.random((1668 - 1626, PW)) > 0.985
    pr = base[1626 - OY:1668 - OY, 5990 - OX:6122 - OX]
    pr *= (1 + 0.28 * gr + 0.35 * mo)[..., None]
    pr[:] = np.where(sp[..., None], pr * 0.55, pr)
    # the two rings back in front of the textured pipe: raised metal, dark
    # gaps each side, lit rims, the same grain (they stand 1 px proud of the
    # pipe, y 1626-1668, no further); owner, 2026-11-19: the collar reaches
    # the pipe's bottom edge, y 1666, its lowest rows carried down, a shade
    # darker, no wider; the one by the rock stays short (its foot would show
    # past the rock)
    for rc in (6063, 6073):
        a, b = rc - 2 - 6058, rc + 3 - 6058
        ring = rings[1627 - 1620:1660 - 1620, a:b]
        yb = 1666 if rc == 6073 else 1660
        if yb == 1666:
            ring = np.concatenate([ring, ring[-7:-1] * np.linspace(0.95, 0.7, 6)[:, None, None]], axis=0)
        d = base[1627 - OY:yb - OY, rc - 2 - OX:rc + 3 - OX]
        d[:] = np.clip(ring * 0.6 + np.array([22, 9, 3], np.float32), 0, 255) * (1 + 0.12 * rng.normal(0, 1, ring.shape[:2]))[..., None]
        d[:, 0] = d[:, 0] * 1.35 + 10                                # the lit rim
        base[1628 - OY:yb - OY, rc - 3 - OX] *= 0.5                  # dark gap before it
        base[1628 - OY:yb - OY, rc + 3 - OX] *= 0.55                 # and after it
    # into the room's pipe gradually over the last 20 px before its flange
    for x in range(6101, 6122):
        w = (x - 6101) / 20.0
        w = w * w * (3 - 2 * w)
        base[1626 - OY:1668 - OY, x - OX] = base[1626 - OY:1668 - OY, x - OX] * (1 - w) + bg[1626 - OY:1668 - OY, x - OX] * w
    # the beam across the top of the new wall (owner, 2026-11-19): no
    # rivets, the darker green of the room's beam beside it (row by row),
    # and a smooth hand-over into it over x 6090-6128
    bm = base[1477 - OY:1521 - OY, 5990 - OX:6128 - OX]
    bmpre = bm.copy()
    # rivets out: each row's small bright dots replaced by a horizontal median
    med = np.stack([np.asarray(Image.fromarray(bm[..., k].clip(0, 255).astype(np.uint8)).filter(ImageFilter.MedianFilter(7))).astype(np.float32) for k in range(3)], -1)
    hot = (bm.mean(-1) - med.mean(-1)) > 6
    hot = np.asarray(Image.fromarray((hot * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(3))) > 0
    bm[hot] = med[hot]
    tgt = bg[1477 - OY:1521 - OY, 6130 - OX:6185 - OX].mean(axis=1)            # the room's beam, per row
    cur = bm[:, 6030 - 5990:6090 - 5990].mean(axis=1)
    bm *= np.clip((tgt + 4) / (cur + 4), 0.4, 1.6)[:, None, :]
    bm *= (1 + 0.08 * np.random.default_rng(21).normal(0, 1, bm.shape[:2]))[..., None]   # its grain back
    # (owner, 2026-11-19: no seam where it meets the room's beam, and the
    # green ends at y 1517, the wall below): the beam itself, rows 1497-1517,
    # is the room's own beam (a clean stretch, x 6122-6196): a beam is the
    # same all along, so each row is that beam's median color, with a fine
    # grain and a slow variation, no copied features; rows below 1517 keep
    # the wall as it was
    bm[1517 - 1477:] = bmpre[1517 - 1477:]
    src = bg[1497 - OY:1517 - OY, 6122 - OX:6196 - OX]
    rng2 = np.random.default_rng(31)
    rng2.normal(0, 0.8, 6128 - 5990)          # (draws the mock-up spent on a column pick it no longer uses)
    prof = np.median(src, axis=1)
    band = np.repeat(prof[:, None, :], 6128 - 5990, axis=1)
    slow = np.asarray(Image.fromarray(np.clip(rng2.normal(128, 30, (2, 12)), 0, 255).astype(np.uint8)).resize((6128 - 5990, 20), Image.BICUBIC)).astype(np.float32) / 128 - 1
    band *= (1 + 0.10 * rng2.normal(0, 1, band.shape[:2]) + 0.12 * slow)[..., None]
    bm[1497 - 1477:1517 - 1477] = band
    # the room's beam takes over over x 6116-6128 (past its old bracket); the
    # rows above the beam over x 6100-6128
    for x in range(6100, 6128):
        w = np.clip((x - 6116) / 12.0, 0, 1)
        w = w * w * (3 - 2 * w)
        wu = (x - 6100) / 28.0; wu = wu * wu * (3 - 2 * wu)
        bm[:1497 - 1477, x - 5990] = bm[:1497 - 1477, x - 5990] * (1 - wu) + bg[1477 - OY:1497 - OY, x - OX] * wu
        bm[1497 - 1477:1517 - 1477, x - 5990] = bm[1497 - 1477:1517 - 1477, x - 5990] * (1 - w) + bg[1497 - OY:1517 - OY, x - OX] * w
    # the room's stone floor slab (y 1784-1797) carried west to the rock face
    # (owner, 2026-11-19), from a clean stretch of the room's own floor
    slab = bg[1784 - OY:1797 - OY, 6160 - OX:6280 - OX]
    for x in range(5990, 6136):
        w = np.clip((6136 - x) / 10.0, 0, 1)
        base[1784 - OY:1797 - OY, x - OX] = base[1784 - OY:1797 - OY, x - OX] * (1 - w) + slab[:, (x - 5990) % slab.shape[1]] * w


def load_front():
    m = np.asarray(Image.open(os.path.join(DATA, 'front.png')).convert('1'))
    front = np.zeros((H, W), bool)
    front[FRONT_AT[1] - OY:FRONT_AT[1] - OY + m.shape[0], FRONT_AT[0] - OX:FRONT_AT[0] - OX + m.shape[1]] = m
    return front


def make(c2):
    """Returns (final scene, bg, layer masks: tube glass, rings, collar,
    front, rock alpha), all over the scene's W x H."""
    bg = np.asarray(region(c2, OX, OY, OX + W, OY + H)).astype(np.float32)[..., :3]
    bgt = np.asarray(region(c2, OX, OY - 300, OX + W, OY + H)).astype(np.float32)[..., :3]
    swmp_im = s16.read(os.path.join(c2, 'Images', 'swmp.s16'))
    swmp = [np.asarray(f).astype(np.float32) for f in swmp_im]
    # the tube as the background paints it, with its own front piece over
    # it; and the scene, the scaffold's front pieces over that too
    tubecomp = bg.copy()
    put_scenery(tubecomp, swmp, SCENERY[:1])
    scene = tubecomp.copy()
    put_scenery(scene, swmp, SCENERY[1:])
    mask, strip = old_tube(tubecomp, np.asarray(swmp_im[0])[..., 3] > 0)
    MOD, pc = new_tube(strip)

    base = paint_out(scene, bg, bgt, mask, strip)
    lab_west_end(base, bg)

    deck_rm = np.zeros((H, W), bool)
    for x0, y0, x1, y1 in DECK_RM:
        deck_rm[y0 - OY:y1 - OY, x0 - OX:x1 - OX] = True
    ys, xs = np.nonzero(deck_rm.any(1))[0], np.nonzero(deck_rm.any(0))[0]
    deck_box = (ys.min() - 6, ys.max() + 7, xs.min() - 6, xs.max() + 7)
    front = load_front() & ~deck_rm
    o = base.copy()
    lib.stalactites(o, bg, STALACTITES, OX, OY)
    before = o.copy()                         # what the tube's glass is laid over
    keep = front | (np.arange(W)[None, :] + OX >= ENTRY + 30)
    paste_rot_masked(o, pc, ang, (X0, start_top - 12), keep)
    # owner, 2026-11-19: the tube meets the dock collar square (sheared
    # start, no ring beside the collar); glass from under the collar, which
    # swings at its foot
    lib.dock_join(o, base, MOD, X0, start_top, slope, front, OX, OY, kstart=-34)
    # where the tube's rings (bracers) land: the same paste, with only the
    # ring columns white (their outline rises above the glass and drops below it)
    pcr = pc.copy()
    al = pc[..., 3] > 0
    rc = al[0:4].any(axis=0) & al[152:].any(axis=0)
    rc = np.asarray(Image.fromarray((rc[None, :] * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(3)))[0] > 0
    pcr[..., :3] = 0
    pcr[:, rc, :3] = 255
    ringimg = np.zeros_like(o)
    paste_rot_masked(ringimg, pcr, ang, (X0, start_top - 12), keep)
    rings_mask = ringimg.mean(-1) > 128
    rings_mask[:, :X0 + 60 - OX] = False      # (the first ring was taken out at the collar)

    # the dock's upright collar is drawn again turned (below); seen through
    # the glass, its old place is a smooth fill
    oc = Image.new('L', (4781 - 4746, 1584 - 1411), 0)
    ImageDraw.Draw(oc).rounded_rectangle((0, 0, 4781 - 4746 - 1, 1584 - 1411 - 1), radius=10, fill=255)
    oc = np.asarray(oc) > 0
    ys0, ys1, xs0, xs1 = 1405 - OY, 1590 - OY, 4740 - OX, 4788 - OX
    F = before[ys0:ys1, xs0:xs1].copy()
    M = np.zeros(F.shape[:2], bool)
    M[1411 - 1405:1584 - 1405, 4746 - 4740:4781 - 4740] = oc
    smooth_fill(F, M)
    before[ys0:ys1, xs0:xs1] = F
    # and the old deck's place, seen through the glass
    deck_fill(before, deck_rm, deck_box, 61)

    # the tube's inside (owner, 2026-11-19: the old glass showed "a weird
    # swooshy vibe", the old cave seen through it repeated with every
    # module): over its rows 16-132, the cave behind it, its ridges and
    # hollows brought out, muted (less color, less contrast, a little
    # darker), dithered with the glass's own light and shade
    tp = np.abs(o - before).sum(-1) > 0.5
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    th = math.radians(ang); ty0 = start_top - 12
    v = -(xx + OX - X0) * math.sin(th) + (yy + OY - ty0) * math.cos(th)
    inn = tp & ~rings_mask & (v >= 16) & (v <= 132) & ~front
    # (ease in over the inside's top and bottom few rows, so the rims join)
    edge = np.clip(np.minimum(v - 16, 132 - v) / 5.0, 0, 1)
    alr = MOD[..., 3] > 0
    ringc = alr[0:4].any(0) & alr[152:].any(0)
    prof = np.median(MOD[:, ~ringc, :3], axis=1)                      # the glass's own light and shade, row by row
    P = prof[np.clip(v, 0, MOD.shape[0] - 1).astype(int)]
    grain = np.random.default_rng(41).normal(0, 1, (H, W)).astype(np.float32)
    relief = before + (before - lib.blur(before, 6)) * 0.9
    cave = relief * 0.62 * np.array([0.86, 1.0, 0.95])
    cg = cave.mean(-1, keepdims=True)
    cave = cg + (cave - cg) * 0.5
    cm = float(cave[inn].mean()) if inn.any() else 30.0
    cave = (cm + (cave - cm) * 0.7) * 0.88
    hl = np.clip(P - np.percentile(prof[16:132], 35, axis=0), 0, None)
    glass = P * 0.85 + hl * 0.6
    # ordered dither: each pixel either the cave or the glass, by a 4x4
    # Bayer pattern (the cave's share stronger where the glass is darkest)
    bay = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], np.float32) / 16 + 1 / 32
    B = np.tile(bay, (H // 4 + 1, W // 4 + 1))[:H, :W]
    share = 0.55 + 0.2 * (1 - np.clip(P.mean(-1) / 60.0, 0, 1))
    new = np.where((B < share)[..., None], cave + hl * 0.35, glass * 0.9 + cave * 0.15)
    new *= (1 + 0.04 * grain)[..., None]
    w = (inn * edge)[..., None]
    o[:] = o * (1 - w) + new * w

    # the dock's collar ring at the tube's angle (owner, 2026-11-19: "the
    # same angle as the other collars"): cut out of the picture by its
    # rounded outline, the gap filled, and drawn again turned
    CX0, CY0, CX1, CY1 = 4748, 1413, 4780, 1582
    cmk = Image.new('L', (CX1 - CX0, CY1 - CY0), 0)
    ImageDraw.Draw(cmk).rounded_rectangle((0, 0, CX1 - CX0 - 1, CY1 - CY0 - 1), radius=9, fill=255)
    cma = np.asarray(cmk).astype(np.float32) / 255
    col = base[CY0 - OY:CY1 - OY, CX0 - OX:CX1 - OX].copy()          # (the glass now runs under it)
    # its old place, where no glass covers it (its cap above the glass, its
    # foot below): filled smoothly from around it (the glass is sheared, not
    # turned, at the collar)
    vs = (yy + OY) - (start_top - 12 + slope * np.minimum(xx + OX - X0, 0))
    tp2 = (vs >= 9) & (vs <= 147)                                    # inside the glass's height there
    ys0, ys1, xs0, xs1 = 1405 - OY, 1595 - OY, 4738 - OX, 4790 - OX
    F = o[ys0:ys1, xs0:xs1].copy()
    M = np.zeros(F.shape[:2], bool)
    M[1411 - 1405:1584 - 1405, 4746 - 4738:4781 - 4738] = oc
    M = (np.asarray(Image.fromarray((M * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(5))) > 0) & ~tp2[ys0:ys1, xs0:xs1]
    smooth_fill(F, M)
    F[M] *= (1 + 0.05 * np.random.default_rng(52).normal(0, 1, (M.sum(), 1)))
    o[ys0:ys1, xs0:xs1] = F
    # the collar turned about its top (owner, 2026-11-19: a turn about its
    # middle uncovered the dock box's corner): its cap stays where the box
    # meets it, its foot swings west
    rim = Image.fromarray(np.dstack([col, cma * 255]).clip(0, 255).astype(np.uint8), 'RGBA').rotate(-ang, resample=Image.BICUBIC, expand=True)
    ra = np.asarray(rim).astype(np.float32)
    w0, h0 = CX1 - CX0, CY1 - CY0
    pvx, pvy = w0 / 2, 6.0                                  # the pivot, in the collar's own picture
    dx, dy = pvx - w0 / 2, pvy - h0 / 2
    rx = dx * math.cos(th) - dy * math.sin(th) + ra.shape[1] / 2
    ry = dx * math.sin(th) + dy * math.cos(th) + ra.shape[0] / 2
    px, py = int(round(CX0 + pvx - rx)), int(round(CY0 + pvy - ry))
    d = o[py - OY:py - OY + ra.shape[0], px - OX:px - OX + ra.shape[1]]
    aa = lib.pw(ra[..., 3:] / 255, 0.8)
    d[:] = d * (1 - aa) + ra[..., :3] * aa
    collar_mask = np.zeros((H, W), bool)
    collar_mask[py - OY:py - OY + ra.shape[0], px - OX:px - OX + ra.shape[1]] = ra[..., 3] > 128

    tube_px = np.abs(o - before).sum(-1) > 0.5           # where the tube's glass was drawn
    # the old deck's place outside the glass (by the glass's geometry: the
    # glass itself was filled above), filled from around it
    deck_fill(o, deck_rm, deck_box, 62, inside=(v >= 9) & (v <= 147) & (np.arange(W)[None, :] + OX >= X0 - 34))
    # what was painted or placed over the old tube, back in front
    o[front] = scene[front]
    put_scenery(o, swmp, SCENERY[1:], cut=deck_rm)

    # the rock: its edge wraps the tube (a lip over it, a ledge under it, a
    # jagged edge across the glass tilted with the tube), an overhang over
    # the exit into the lab
    tt = tube_y(ENTRY)                     # the tube's top edge at the entry
    tb = tt + 136                          # its bottom edge
    sl = math.tan(math.radians(ang))

    def across(f):                         # a point across the tube's mouth, tilted with it
        return (ENTRY + 10 + 8 * math.sin(f * 7) - (f - 0.5) * 136 * sl, tt + f * 136)
    outline = [(ENTRY - 30, 1470), (5960, 1400), (6030, 1392), (EXIT_X + 62, 1478),
               (EXIT_X + 66, LAB_FLOOR - 142), (EXIT_X + 42, LAB_FLOOR - 128), (EXIT_X + 22, LAB_FLOOR - 104),
               (EXIT_X + 10, LAB_FLOOR - 64), (EXIT_X + 5, LAB_FLOOR + 20),
               (EXIT_X - 20, LAB_FLOOR + 60), (ENTRY - 34, LAB_FLOOR + 60),
               (ENTRY - 30, tb + 46), (ENTRY - 56, tb + 18), (ENTRY - 50, tb + 4), (ENTRY - 14, tb - 4)]
    outline += [across(f) for f in (0.92, 0.75, 0.55, 0.35, 0.18, 0.06)]
    outline += [(ENTRY - 18, tt + 2), (ENTRY - 58, tt - 6), (ENTRY - 52, tt - 24), (ENTRY - 30, tt - 44)]
    # the rock's shadow on the glass just before it goes in
    for x in range(ENTRY - 70, ENTRY + 20):
        f = np.clip((x - (ENTRY - 70)) / 80, 0, 1)
        y0, y1 = int(tube_y(x) - 6), int(tube_y(x) + 140)
        o[y0 - OY:y1 - OY, x - OX] *= (1 - 0.55 * f * f)
    # textured like the earth below the lab (owner, 2026-11-19)
    earth_sample = bg[1802 - OY:1990 - OY, 6436 - OX:6660 - OX].copy()
    rx, ry, rr, ra = lib.render_rock_earth(blob(outline, 5, 61), earth_sample, seed=11)
    reg = o[ry - OY:ry - OY + rr.shape[0], rx - OX:rx - OX + rr.shape[1]]
    # its top meets the cave's pebbly earth and the lab's corner (owner,
    # 2026-11-19: it felt separate): above y 1525 the edge is feathered and
    # the earth's own speckled texture is carried down into the rock, fading
    # out (owner: fade into the north more)
    rh, rw = ra.shape
    cur = Image.fromarray((ra * 255).astype(np.uint8)); dist = np.zeros_like(ra)
    for _ in range(30):
        cur = cur.filter(ImageFilter.MinFilter(3)); dist += np.asarray(cur) > 128
    zone = np.clip((1565 - (np.arange(rh)[:, None] + ry)) / 75.0, 0, 1) * np.ones((1, rw))
    zone = np.asarray(Image.fromarray((zone * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(4))).astype(np.float32) / 255
    # (not against the pale arch rock on the left: a feather there reads as fog)
    pale = np.asarray(Image.fromarray((np.clip((reg.mean(-1) - 60) / 30, 0, 1) * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(9)).filter(ImageFilter.GaussianBlur(5))).astype(np.float32) / 255
    zone = zone * (1 - pale)
    ra2 = ra * (1 - zone * (1 - np.clip(dist / 20.0, 0, 1)))
    mix = (zone * 0.85 * (1 - np.clip(dist / 55.0, 0, 1)))[..., None]
    earth = reg.copy()
    carry = earth * np.clip((rr.mean(-1, keepdims=True) + 10) / (earth.mean(-1, keepdims=True) + 10), 0.6, 1.6) ** 0.5   # the earth's texture, in the rock's light
    rr2 = rr * (1 - mix) + carry * mix
    reg[:] = reg * (1 - ra2[..., None]) + rr2 * ra2[..., None]
    rock = np.zeros((H, W), np.float32)
    rock[ry - OY:ry - OY + rr.shape[0], rx - OX:rx - OX + rr.shape[1]] = ra2
    # the overhang's shadow on the lab's new wall, just inside the exit
    for x in range(EXIT_X + 6, EXIT_X + 50):
        f = 1 - (x - EXIT_X - 6) / 44
        o[int(LAB_FLOOR - 124) - OY:LAB_FLOOR - OY, x - OX] *= 1 - 0.45 * f
    return o, bg, dict(tube=tube_px, rings=rings_mask, collar=collar_mask, front=front, rock=rock)


def layer(img8, m):
    """img8 where m, transparent elsewhere, cropped to m: (RGBA image, world top left)."""
    ys, xs = np.nonzero(m)
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    rgba = np.dstack([img8[y0:y1, x0:x1], np.where(m[y0:y1, x0:x1], 255, 0).astype(np.uint8)])
    return Image.fromarray(rgba, 'RGBA'), (int(x0) + OX, int(y0) + OY)


def save(img, path):
    info = PngImagePlugin.PngInfo()
    info.add_text('openc2e-frames', '1')
    img.save(path, pnginfo=info)


if __name__ == '__main__':
    c2, user = sys.argv[1], sys.argv[2]
    out = os.path.join(user, 'Images')
    os.makedirs(out, exist_ok=True)
    o, bg, m = make(c2)
    o8 = o.clip(0, 255).astype(np.uint8)
    Image.fromarray(o8).save(os.path.join(user, 'tube_walk_final.png'))
    # behind creatures: where the scene differs from the background, a
    # couple of px wider
    changed = np.abs(o8.astype(int) - bg.astype(np.uint8)).max(-1) > 0
    changed = np.asarray(Image.fromarray((changed * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(5))) > 0
    # in front of creatures: the glass on a checkerboard (world x + y even)
    yy, xx = np.mgrid[0:H, 0:W]
    checker = ((xx + OX + yy + OY) % 2) == 0
    tube_front = (m['tube'] & checker) | m['rings'] | m['collar'] | m['front']
    for name, mm in (('ztwb', changed), ('ztwf', tube_front), ('ztwr', m['rock'] > 0.5)):
        im, at = layer(o8, mm)
        save(im, os.path.join(out, name + '.png'))
        print(name, 'at', at, im.size)
    # the gene lab's top beam, its west end taken off (the new wall's beam
    # hands over to the room's own at x 6116-6128)
    g = s16.read(os.path.join(c2, 'Images', 'genc.s16'))[0]
    ga = np.asarray(g).copy()
    ga[:, :6125 - 5995, 3] = 0
    save(Image.fromarray(ga, 'RGBA'), os.path.join(out, 'ztwg.png'))
    print('ztwg at', (5995, 1478), g.size)
