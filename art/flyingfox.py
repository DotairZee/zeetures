"""The flying fox: a woven basket riding a rope from the plank deck west of
the stone arch (floor 840) down past the small waterfall to the bamboo
scaffold by the bamboo lift (floor 1285), and back (owner, 2026-11-10: a
way between the two; "render it more fully with shading", not cartoonish).

Everything is drawn here from numbers: no game pixels. PNG sprites with
real transparency (openc2e loads NAME.png before NAME.s16 for C2; frames
stacked top to bottom, counted in a tEXt chunk "openc2e-frames").

The light comes from the upper left, as on our walkways (bamboo.py,
lowerdeck.py): every round thing (rope, poles, wheels, the basket's body)
is shaded as a cylinder or sphere under that light, then dappled and muted
toward the jungle's tones.

    python3 flyingfox.py OUT_DIR [--preview SCENE.png VIEW_X VIEW_Y OUT.png]

writes
  zffl.png  the line: the rope, the post it is tied to on the plank deck,
            the stub post and small landing at the scaffold (one frame,
            top-left at LINE_X0, LINE_Y0; plane behind creatures)
  zffb.png  the basket: frame 0 its back (inside, far rim, far ropes; behind
            a rider), frame 1 its front (near wall, rim, near ropes and the
            trolley on the rope; in front of a rider). Both frames share one
            origin: the trolley's point on the rope is at (BASKET_OX, BASKET_OY)
            in the frame, and the basket's floor is FLOOR px below it.
"""
import sys
import os
import math
import numpy as np
from PIL import Image, PngImagePlugin

SS = 4

# --- the line, in world coordinates ---------------------------------------
FLOOR = 80                         # basket floor below the trolley's rope point
TOP_STOP = (3840, 840 - FLOOR)     # trolley's rope point at the deck stop
BOT_STOP = (3395, 1285 - FLOOR)    # and at the scaffold stop
SLOPE = (BOT_STOP[1] - TOP_STOP[1]) / (BOT_STOP[0] - TOP_STOP[0])


def line_y(x):
    return TOP_STOP[1] + (x - TOP_STOP[0]) * SLOPE


TOP_POST_X, BOT_POST_X = 3900, 3368
LINE_X0, LINE_Y0, LINE_X1, LINE_Y1 = 3355, 690, 3945, 1338  # (wide enough for the brace's foot, 3926)
LANDING_X1 = 3512   # the landing's east end (flyingfox.cos: room 61 reaches here)

# --- colours (sRGB, before dapple and muting) -----------------------------
BAMBOO = np.array([196, 152, 58], np.float32)
BAMBOO_LIT = np.array([246, 210, 120], np.float32)
BAMBOO_DARK = np.array([104, 70, 18], np.float32)
ROPE = np.array([150, 112, 66], np.float32)
WICKER = np.array([182, 140, 84], np.float32)
WICKER_DARK = np.array([86, 58, 28], np.float32)
WOOD = np.array([112, 72, 38], np.float32)
IRON = np.array([58, 56, 52], np.float32)
PLANK_LIT = np.array([214, 132, 40], np.float32)
PLANK_DARK = np.array([112, 52, 12], np.float32)
LIGHT = np.array([-0.55, -0.62, 0.56], np.float32)
LIGHT /= np.linalg.norm(LIGHT)
MUTE = 0.74


def noise(shape, seed, scale):
    """Smooth value noise in -1..1 at the given feature size (pixels)."""
    rng = np.random.default_rng(seed)
    h, w = shape
    gh, gw = h // scale + 2, w // scale + 2
    g = rng.uniform(-1, 1, (gh, gw)).astype(np.float32)
    im = Image.fromarray(((g + 1) * 127.5).astype(np.uint8)).resize((gw * scale, gh * scale), Image.BICUBIC)
    return np.asarray(im, np.float32)[:h, :w] / 127.5 - 1


def cnoise(u, v, seed):
    """Smooth value noise in -1..1 at arbitrary coordinates (one unit is one
    feature), so a texture can be stretched along a pole or a strand."""
    rng = np.random.default_rng(seed)
    g = rng.uniform(-1, 1, (64, 64)).astype(np.float32)
    iu, iv = np.floor(u).astype(np.int64), np.floor(v).astype(np.int64)
    fu, fv = u - iu, v - iv
    fu, fv = fu * fu * (3 - 2 * fu), fv * fv * (3 - 2 * fv)
    G = lambda i, j: g[np.mod(j, 64), np.mod(i, 64)]
    return (G(iu, iv) * (1 - fu) * (1 - fv) + G(iu + 1, iv) * fu * (1 - fv)
            + G(iu, iv + 1) * (1 - fu) * fv + G(iu + 1, iv + 1) * fu * fv)


class Canvas:
    """An RGBA canvas at SS times the world's resolution, addressed in world
    (or local) pixels from an origin."""

    def __init__(self, x0, y0, w, h):
        self.x0, self.y0, self.w, self.h = x0, y0, w, h
        self.rgb = np.zeros((h * SS, w * SS, 3), np.float32)
        self.a = np.zeros((h * SS, w * SS), np.float32)

    def grid(self, bx0, by0, bx1, by1):
        """World coordinates of the sample points in a box (clipped)."""
        i0 = max(0, int((bx0 - self.x0) * SS)); i1 = min(self.w * SS, int(math.ceil((bx1 - self.x0) * SS)))
        j0 = max(0, int((by0 - self.y0) * SS)); j1 = min(self.h * SS, int(math.ceil((by1 - self.y0) * SS)))
        if i1 <= i0 or j1 <= j0:
            return None
        ys, xs = np.mgrid[j0:j1, i0:i1].astype(np.float32)
        return (slice(j0, j1), slice(i0, i1)), self.x0 + (xs + 0.5) / SS, self.y0 + (ys + 0.5) / SS

    def put(self, sl, col, cov):
        cov = np.clip(cov, 0, 1)
        self.rgb[sl] = self.rgb[sl] * (1 - cov[..., None]) + col * cov[..., None]
        self.a[sl] = self.a[sl] + cov * (1 - self.a[sl])

    def image(self, dapple_seed=None, dapple=0.22, mute=MUTE, dim=0.9):
        rgb = self.rgb.copy()
        if dapple_seed is not None:
            blobs = np.kron(noise((self.h, self.w), dapple_seed, 9) + 0.5 * noise((self.h, self.w), dapple_seed + 1, 4), np.ones((SS, SS)))
            rgb *= (1 + dapple * np.clip(blobs, -1, 1))[..., None]
        lum = (rgb * np.array([0.3, 0.59, 0.11], np.float32)).sum(-1, keepdims=True)
        rgb = (lum + (rgb - lum) * mute) * dim
        out = np.dstack([rgb * self.a[..., None], self.a])
        out = out.reshape(self.h, SS, self.w, SS, 4).mean(axis=(1, 3))
        a = out[..., 3:4]
        out[..., :3] = np.where(a > 1e-4, out[..., :3] / np.maximum(a, 1e-4), 0)
        out[..., 3] = out[..., 3] * 255
        return Image.fromarray(np.clip(out, 0, 255).astype(np.uint8), 'RGBA')


def lit(nx, ny, nz):
    return np.clip(nx * LIGHT[0] + ny * LIGHT[1] + nz * LIGHT[2], 0, 1)


def tube(cv, p, q, width, kind, seed=0):
    """A round thing from p to q: 'bamboo' (node rings, lit stripe), 'rope'
    (three twisted strands) or 'cord' (thin rope, plain twist)."""
    (ax, ay), (bx, by) = p, q
    r = width / 2
    g = cv.grid(min(ax, bx) - r - 1, min(ay, by) - r - 1, max(ax, bx) + r + 1, max(ay, by) + r + 1)
    if g is None:
        return
    sl, X, Y = g
    vx, vy = bx - ax, by - ay
    ln = max(math.hypot(vx, vy), 1e-6)
    ux, uy = vx / ln, vy / ln
    nx, ny = -uy, ux                                  # across
    s = (X - ax) * ux + (Y - ay) * uy                 # along, px
    t = ((X - ax) * nx + (Y - ay) * ny) / r           # -1..1 across
    inside = (s >= -0.2) & (s <= ln + 0.2) & (np.abs(t) <= 1)
    # round ends
    for ex, ey in ((ax, ay), (bx, by)):
        inside |= (X - ex) ** 2 + (Y - ey) ** 2 <= r * r
    cov = inside.astype(np.float32) * np.clip((1 - np.abs(t)) * r * 1.6 + 0.4, 0, 1)
    tz = np.sqrt(np.clip(1 - t * t, 0, 1))
    shade = 0.32 + 0.9 * lit(t * nx, t * ny, tz)
    if kind == 'green':
        # the green cane of the poles under the plank bridge, muted (owner,
        # 2026-11-10): olive,
        # near-black at the edges, a hard pale highlight down the lit side and
        # a faint cool bounce light down the shade side; long streaks and
        # blotches of weathering along the cane (yellowed, darkened), fine
        # fibres, and raised nodes far apart with a lit lip and a dark seat
        g_dark = np.array([12, 20, 10], np.float32)
        g_mid = np.array([100, 116, 80], np.float32)
        g_yel = np.array([150, 150, 100], np.float32)
        g_hl = np.array([206, 214, 192], np.float32)
        across = (t + 1) / 2
        streak = cnoise(s / 26.0, across * 5.0, 70 + seed) * 0.6 + cnoise(s / 9.0, across * 11.0, 80 + seed) * 0.4
        blotch = cnoise(s / 14.0, across * 2.2, 90 + seed)
        fibre = cnoise(s / 3.5, across * 26.0, 100 + seed)
        shade2 = np.clip(shade * 1.2 - 0.15, 0, 1) ** 1.45
        base = g_dark + (g_mid - g_dark) * shade2[..., None]
        yel = np.clip(blotch * 1.6 - 0.35, 0, 1) * shade2
        base = base + (g_yel - base) * (0.55 * yel)[..., None]
        base = base * (1 + 0.36 * streak + 0.16 * fibre)[..., None]
        speck = np.clip(cnoise(s / 1.6, across * 9.0, 110 + seed) * 2.2 - 1.3, 0, 1)
        base = base * (1 - 0.18 * speck)[..., None]
        base = base * (1 - 0.18 * np.clip(-blotch * 1.8 - 0.5, 0, 1))[..., None]
        hl = np.exp(-((t + 0.44 + 0.05 * streak) / 0.11) ** 2) * (0.75 + 0.25 * np.clip(streak + 0.6, 0, 1))
        base = base + (g_hl - base) * (0.6 * hl)[..., None]
        bounce = np.exp(-((t - 0.8) / 0.1) ** 2) * 0.18
        base = base + (np.array([120, 150, 130], np.float32) - base) * bounce[..., None]
        sn = (s + 15 + seed * 11) % 44
        lip = np.exp(-((sn - 21.6) / 0.7) ** 2)
        seat = np.exp(-((sn - 23.0) / 0.9) ** 2)
        base = base * (1 + 0.55 * lip * shade2)[..., None] * (1 - 0.6 * seat)[..., None]
        # rough, weathered surface (owner, 2026-11-10: "more rough texture"):
        # a bumpy relief lit from the upper left, split lengthwise cracks,
        # pale, dry patches where the skin has worn (a faint rim along their
        # lower edge; owner: "less dingy", so clean rather than grimy), and a nicked, uneven outline
        def bumps(ss, aa):
            return cnoise(ss / 2.4, aa * 13.0, 160 + seed) * 0.6 + cnoise(ss / 0.9, aa * 30.0, 170 + seed) * 0.4
        relief = (bumps(s - 0.5, across) - bumps(s + 0.5, across)) * 1.8 + (bumps(s, across - 0.03) - bumps(s, across + 0.03)) * 1.2
        base = base * np.clip(1 + 0.45 * relief, 0.6, 1.6)[..., None]
        crack = np.clip(1 - np.abs(cnoise(s / 20.0, across * 6.0, 180 + seed)) / 0.05, 0, 1) * np.clip(cnoise(s / 11.0, 0.3, 185 + seed) + 0.2, 0, 1)
        base = base * (1 - 0.35 * crack)[..., None]
        pn = cnoise(s / 7.0, across * 3.0, 190 + seed)
        patch = np.clip((pn - 0.42) / 0.08, 0, 1)
        dry = np.array([196, 204, 142], np.float32) * (0.4 + 0.75 * shade2)[..., None]
        base = base + (dry - base) * (0.5 * patch)[..., None]
        prim = np.clip(1 - np.abs(cnoise((s - 0.8) / 7.0, across * 3.0, 190 + seed) - 0.42) / 0.05, 0, 1) * (1 - patch)
        base = base * (1 - 0.2 * prim)[..., None]
        col = base
        nick = 0.16 * np.abs(cnoise(s / 1.8, (t > 0) * 7.0, 200 + seed))
        cov = cov * np.clip((1 - nick - np.abs(t)) * r * 1.6 + 0.4, 0, 1)
    elif kind == 'scaffold':
        # the bamboo scaffold by the desert lift (its rails and poles): a
        # saturated orange-red cane, dark red-brown at the edges, a warm
        # highlight, node rings
        o_dark = np.array([70, 18, 0], np.float32)
        o_mid = np.array([214, 96, 6], np.float32)
        o_lit = np.array([255, 178, 84], np.float32)
        sh = np.clip(shade, 0, 1.2)
        base = o_dark + (o_mid - o_dark) * np.clip(sh, 0, 1)[..., None]
        base = base + (o_lit - base) * (np.exp(-((t + 0.45) / 0.18) ** 2) * 0.7)[..., None]
        ring = np.abs(((s + 9 + seed * 7) % 30) - 15) > 14.1
        base = np.where(ring[..., None], base * 0.55, base)
        grain = cnoise(s / 6.0, (t + 1) * 4.0, 210 + seed)
        col = base * (1 + 0.1 * grain[..., None])
    elif kind == 'bamboo':
        base = BAMBOO_DARK + (BAMBOO - BAMBOO_DARK) * np.clip(shade, 0, 1)[..., None]
        base = base + (BAMBOO_LIT - BAMBOO) * (np.clip(shade - 0.9, 0, 1) * 3)[..., None]
        ring = np.abs(((s + 9 + seed * 7) % 27) - 13.5) > 12.6
        base = np.where(ring[..., None], base * 0.6, base)
        grain = noise(X.shape, 40 + seed, 3 * SS)
        col = base * (1 + 0.06 * grain[..., None])
    else:
        period = 3.6 if kind == 'rope' else 2.4  # rope, cord, greycord, bluecord
        phase = ((s + t * r * 1.1) / period) % 1.0
        strand = np.abs(phase - 0.5) * 2               # 1 at the grooves
        crown = np.clip(1 - strand / 0.45, 0, 1) ** 2   # the middle of each strand
        fibre = cnoise((s - t * r * 1.1) / 1.1, phase * 6.0, 120 + seed)
        hue = cnoise(s / 30.0, 0.5, 130 + seed)
        shade = shade * (1 - 0.72 * strand ** 2.2) * (1 + 0.32 * crown * np.clip(shade, 0, 1)) * (1 + 0.16 * fibre)
        shade = np.clip(shade, 0, 1.4) ** 1.15
        rope_col = (np.array([128, 136, 148], np.float32) if kind == 'greycord' else
                    np.array([142, 156, 196], np.float32) if kind == 'bluecord' else ROPE)
        base = rope_col * (1 + 0.1 * hue[..., None]) + np.array([-10, -6, 4], np.float32) * np.clip(-hue, 0, 1)[..., None]
        col = base * shade[..., None]
    cv.put(sl, col, cov)


def lashing(cv, x, y, w, h, angle=0.0, seed=0, kind='cord'):
    """Cord wound round a joint: a short tube of turns across a pole."""
    turns = max(3, int(h / 1.6))
    ca, sa = math.cos(angle), math.sin(angle)
    for k in range(turns):
        off = (k - (turns - 1) / 2) * 1.6
        cx, cy = x - sa * off, y + ca * off
        tube(cv, (cx - ca * w / 2, cy - sa * w / 2), (cx + ca * w / 2, cy + sa * w / 2), 1.7, kind, seed + k)


def cross_lashing(cv, x, y, post_w, rail_w, seed=0):
    """A lashing like the bamboo scaffold's at a post-rail crossing: blue-grey
    cord wound a few turns round the post just above and just below the rail,
    and two diagonal wraps crossing over the joint in an X."""
    for side in (-1, 1):
        cy = y + side * (rail_w / 2 + 2.2)
        lashing(cv, x, cy, post_w + 3.4, 4.8, 0.0, seed + (3 if side > 0 else 0), 'bluecord')
    h = rail_w / 2 + 4.5
    w = post_w / 2 + 3.5
    tube(cv, (x - w, y - h), (x + w, y + h), 2.6, 'bluecord', seed + 7)
    tube(cv, (x + w, y - h), (x - w, y + h), 2.6, 'bluecord', seed + 8)


def wrap_lashing(cv, x, y, w, angle, seed=0, turns=4):
    """Turns of blue-grey cord round a pole where another meets it."""
    lashing(cv, x, y, w + 1.5, turns * 1.8, angle, seed, 'bluecord')


def draw_line():
    cv = Canvas(LINE_X0, LINE_Y0, LINE_X1 - LINE_X0, LINE_Y1 - LINE_Y0)
    # the landing at the scaffold (owner, 2026-11-10: an extension of the
    # scaffold it is built off, in its reddish wood): the scaffold's deck
    # board carried on to LANDING_X1 over an orange-red cane beam, on two
    # orange-red cane posts with braces back toward the scaffold, the joints
    # lashed with grey cord as on the scaffold
    # (owner, 2026-11-10: no post at the far end; the braces joined
    # smoothly) one post, mid-landing; a brace from the scaffold's corner
    # post up to it, and one from low on it out to the beam's far end, each
    # end lashed where it meets a post or the beam
    MID = 3440
    tube(cv, (MID, 1290), (MID, 1336), 5.2, 'scaffold', 3)
    tube(cv, (3358, 1326), (MID - 2, 1298), 3.8, 'scaffold', 4)
    tube(cv, (MID + 2, 1324), (LANDING_X1 - 6, 1297), 3.8, 'scaffold', 7)
    tube(cv, (3362, 1294), (LANDING_X1 + 1, 1294), 6.0, 'scaffold', 9)
    # the board: tan wood, lit along its top edge, darker underneath, a
    # long grain and a couple of joints
    g = cv.grid(3364, 1284.5, LANDING_X1 + 1, 1291)
    sl, X, Y = g
    tt = np.clip((Y - 1284.5) / 6.5, 0, 1)
    tan_lit = np.array([236, 188, 112], np.float32)
    tan = np.array([204, 142, 64], np.float32)
    tan_dark = np.array([112, 62, 18], np.float32)
    face = tan + (tan_dark - tan) * np.clip((tt - 0.35) / 0.65, 0, 1)[..., None] ** 1.2
    face = face + (tan_lit - face) * np.clip(1 - tt / 0.22, 0, 1)[..., None]
    grain = cnoise(X / 9.0, Y / 1.2, 220) * 0.5 + cnoise(X / 2.5, Y / 0.8, 221) * 0.5
    face = face * (1 + 0.12 * grain[..., None])
    for jx in (3420, 3478):
        face = np.where((np.abs(X - jx) < 0.6)[..., None], face * 0.5, face)
    cv.put(sl, face, np.ones_like(X))
    # lashings like the scaffold's (owner, 2026-11-10): blue-grey cord, an X
    # over the post-beam crossing with turns above and below, and full turns
    # where each brace meets a post or the beam
    cross_lashing(cv, MID, 1294, 5.2, 6.0, 10)                        # post and beam
    wrap_lashing(cv, MID, 1302.5, 7.5, 0.0, 11, 3)                     # brace 1 to the post
    wrap_lashing(cv, MID, 1324, 7.5, 0.0, 12, 4)                       # brace 2 to the post
    wrap_lashing(cv, 3358, 1325, 7.0, 0.0, 13, 4)                      # brace 1 to the scaffold's post
    wrap_lashing(cv, LANDING_X1 - 6, 1295.5, 7.0, 1.5708, 14, 3)       # brace 2 under the beam's end
    # the rope: tied round the deck post, over the stops, to the stub post
    ta = (TOP_POST_X - 2, line_y(TOP_POST_X - 2))
    ba = (BOT_POST_X + 2, line_y(BOT_POST_X + 2))
    # posts (behind the rope where it is tied)
    # the deck post and its brace: green cane like the poles under the bridge
    # (their feet stand on the deck's planks, floor 840: owner, 2026-11-10,
    # they seemed to go below it; the round ends reach 841)
    tube(cv, (TOP_POST_X, 836), (TOP_POST_X, ta[1] - 14), 10.0, 'green', 1)
    tube(cv, (TOP_POST_X + 26, 837.5), (TOP_POST_X + 3, ta[1] + 40), 6.5, 'green', 2)
    tube(cv, (BOT_POST_X, 1290), (BOT_POST_X, ba[1] - 9), 6.0, 'scaffold', 5)
    tube(cv, ta, ba, 3.2, 'rope', 0)
    # the knots: turns of the rope round each post, and the brace's lashing
    lashing(cv, TOP_POST_X, ta[1] + 1, 9.5, 6, 0.0, 20)
    lashing(cv, TOP_POST_X + 3, ta[1] + 42, 8.5, 5, 0.5, 30)
    lashing(cv, BOT_POST_X, ba[1] + 1, 8.5, 6, 0.0, 40)
    # a short tail of rope hanging from each knot
    tube(cv, (TOP_POST_X + 4, ta[1] + 4), (TOP_POST_X + 7, ta[1] + 20), 2.2, 'cord', 50)
    tube(cv, (BOT_POST_X - 4, ba[1] + 4), (BOT_POST_X - 6, ba[1] + 17), 2.2, 'cord', 51)
    # less muted than the walkways, so the cane's light and dark read strongly
    return cv.image(dapple_seed=71, dapple=0.18, mute=0.9, dim=0.97)


# --- the basket ------------------------------------------------------------
BW, BH = 146, 124
BASKET_OX, BASKET_OY = 73, 14          # the trolley's rope point in the frame
RIM_Y, RIM_RX, RIM_RY = 42, 40.0, 7.5  # the rim ellipse (local, from the rope point)
BOTTOM = FLOOR + 6                     # outside bottom of the basket
WALL_BOTTOM_RX = 33.0


def rim_front_y(x):
    import basket3d
    ry = basket3d.R_TOP * basket3d.SIN_E
    return RIM_Y + ry * math.sqrt(max(0.0, 1 - (x / basket3d.R_TOP) ** 2)) - 1.2


def rim_back_y(x):
    import basket3d
    ry = basket3d.R_TOP * basket3d.SIN_E
    return RIM_Y - ry * math.sqrt(max(0.0, 1 - (x / basket3d.R_TOP) ** 2)) - 1.2


def rim_lashing(cv, x, y, seed):
    """Where a cord meets the rim: three turns wound round the rim's coil,
    tight against each other, and a knot on top where the cord comes off."""
    for k in range(3):
        cx = x + (k - 1) * 2.1
        tube(cv, (cx - 1.6, y - 4.6), (cx + 1.4, y + 4.4), 1.9, 'cord', seed + k)
    # the knot: a small round lump of cord
    g = cv.grid(x - 3.2, y - 8.2, x + 3.2, y - 2.0)
    sl, X, Y = g
    dx, dy = (X - x) / 2.6, (Y - (y - 5.2)) / 2.4
    rr = np.sqrt(dx * dx + dy * dy)
    nz = np.sqrt(np.clip(1 - rr ** 2, 0, 1))
    shade = 0.32 + 0.9 * lit(dx, dy, nz)
    twist = np.abs(((dx * 2.2 + dy * 1.4) % 1.0) - 0.5) * 2
    shade = shade * (1 - 0.3 * twist ** 2)
    cv.put(sl, ROPE * np.clip(shade, 0, 1.3)[..., None], np.clip((1 - rr) * 3, 0, 1))


def basket_frames():
    """The basket's two frames, from the 3D renderer (basket3d.py): the back
    (far cords, the inside, the far half of the rim) and the front (the near
    wall, the near half of the rim, the near cords, the trolley)."""
    import basket3d
    back = Canvas(-BASKET_OX, -BASKET_OY, BW, BH)
    front = Canvas(-BASKET_OX, -BASKET_OY, BW, BH)
    full = (slice(0, BH * SS), slice(0, BW * SS))
    ring = (0.0, 19.0)  # (the trolley's ring)
    rim_y = RIM_Y
    for sx in (-42, 42):
        tube(back, ring, (sx, rim_back_y(sx) - 4.5), 1.9, 'cord', 60 + sx)
    for which, cv in (('inside', back), ('rim_back', back), ('outside', front), ('rim_front', front)):
        rgb, cov = basket3d.render(which, -BASKET_OX, -BASKET_OY, BW * SS, BH * SS, SS, rim_y)
        cv.put(full, rgb, cov)
        if which == 'rim_back':
            for sx in (-42, 42):
                rim_lashing(back, sx, rim_back_y(sx), 70 + sx)
    for sx in (-55, 55):
        tube(front, ring, (sx, rim_front_y(sx) - 4.5), 1.9, 'cord', 60 + sx)
        rim_lashing(front, sx, rim_front_y(sx), 60 + sx)
    trolley(front)
    # dappled canopy light over it, dried colours a little faded
    return (back.image(dapple_seed=81, dapple=0.3, mute=0.9, dim=0.95),
            front.image(dapple_seed=81, dapple=0.3, mute=0.9, dim=0.95))


def weave(X, Y, inside, half=None):
    """Wicker: upright stakes every few px, round the body (so they crowd
    toward its sides), with weavers passing over and under them in rows;
    shaded as a cylinder under the light, with shadow under the rim and
    toward the bottom; the inside is in shade."""
    if half is None:
        half = np.full_like(X, RIM_RX)
    u = np.clip(X / np.maximum(half, 1e-3), -0.999, 0.999)
    ang = np.arcsin(u)                                  # around the body
    col_i = ang * 7.0                                   # stakes
    row = (Y - RIM_Y) / 4.2                             # weaver rows
    fc, fr = col_i % 1.0, row % 1.0
    over = ((np.floor(col_i) + np.floor(row)) % 2) == 0
    # each weaver is a long round band across the body, lit on its upper
    # edge, with a dark seam between rows; where it passes under a stake it
    # pinches in a little (a small dark notch), and alternate spans sit a
    # touch higher (lighter) than their neighbours
    bulge = np.sin(np.pi * fr) ** 0.7
    hl = np.clip(1 - np.abs(fr - 0.3) / 0.18, 0, 1) * 0.28
    seam = np.clip(1 - np.minimum(fr, 1 - fr) / 0.14, 0, 1)
    notch = np.clip(1 - np.abs(fc - 0.5) / 0.09, 0, 1) * (~over)
    tone = 0.5 + 0.42 * bulge + hl - 0.4 * seam - 0.22 * notch + np.where(over, 0.06, -0.04)
    nx = u
    nz = np.sqrt(1 - u * u)
    shade = 0.38 + 0.8 * lit(nx * (1 if not inside else -1), np.zeros_like(nx) - 0.15, nz)
    if inside:
        shade = shade * 0.42
    else:
        under_rim = np.clip(1 - (Y - RIM_Y - RIM_RY * nz) / 9.0, 0, 1) * 0.35
        toward_bottom = np.clip((Y - (BOTTOM - 14)) / 14.0, 0, 1) * 0.3
        shade = shade * (1 - under_rim - toward_bottom)
    grain = noise(X.shape, 91 if inside else 92, 2 * SS)
    col = WICKER_DARK + (WICKER - WICKER_DARK) * np.clip(tone * shade, 0, 1.4)[..., None]
    return col * (1 + 0.07 * grain[..., None])


def rim(cv, far):
    """The rolled, braided rim: a round band along the rim ellipse, its far
    half (upper arc) on the back frame, the near half on the front."""
    g = cv.grid(-RIM_RX - 5, RIM_Y - RIM_RY - 5, RIM_RX + 5, RIM_Y + RIM_RY + 5)
    sl, X, Y = g
    th = np.arctan2((Y - RIM_Y) / RIM_RY, X / RIM_RX)
    ex, ey = RIM_RX * np.cos(th), RIM_RY * np.sin(th)
    d = np.hypot(X - ex, Y - RIM_Y - ey)
    band = d <= 2.7
    sel = (ey < 0.5) if far else (ey >= -0.5)
    cov = (band & sel).astype(np.float32) * np.clip((2.7 - d) * 1.4, 0, 1)
    # across the band: shaded as a tube lit from above-left
    nyv = np.clip((Y - RIM_Y - ey) / 2.7, -1, 1)
    nxv = np.clip((X - ex) / 2.7, -1, 1)
    nz = np.sqrt(np.clip(1 - nxv ** 2 - nyv ** 2, 0, 1))
    shade = 0.35 + 0.85 * lit(nxv, nyv, nz)
    braid = np.abs(((th * 26 + nyv * 1.6) % 1.0) - 0.5) * 2
    shade = shade * (1 - 0.3 * braid ** 2)
    if far:
        shade = shade * 0.8
    col = (WICKER * 1.05) * np.clip(shade, 0, 1.3)[..., None]
    cv.put(sl, col, cov)


def trolley(cv):
    """A wooden pulley block riding the rope, tilted with it: two cheek
    plates (one each side of the rope; the near one drawn) holding two
    grooved wheels that sit on the rope, the wheels' tops showing above the
    plate; iron bolts at the axles; an iron strap down to a ring."""
    ang = math.atan2(SLOPE, 1.0)          # the rope's angle
    ux, uy = math.cos(ang), math.sin(ang)     # along the rope
    upx, upy = math.sin(ang), -math.cos(ang)  # perpendicular, above it
    wr = 5.4
    axles = [(-9.5 * ux + upx * (wr - 0.8), -9.5 * uy + upy * (wr - 0.8)),
             (9.5 * ux + upx * (wr - 0.8), 9.5 * uy + upy * (wr - 0.8))]
    # wheels: a turned wooden disc, dark groove near its rim, hub
    for (cx, cy) in axles:
        g = cv.grid(cx - wr - 1, cy - wr - 1, cx + wr + 1, cy + wr + 1)
        sl, X, Y = g
        dx, dy = (X - cx) / wr, (Y - cy) / wr
        rr = np.sqrt(dx * dx + dy * dy)
        cov = np.clip((1 - rr) * wr * 1.5, 0, 1)
        face = 0.5 + 0.6 * np.clip(-(dx * 0.6 + dy * 0.7), -1, 1)       # lit upper left
        groove = np.clip(1 - np.abs(rr - 0.8) / 0.12, 0, 1)
        rimhl = np.clip(1 - np.abs(rr - 0.93) / 0.06, 0, 1) * np.clip(-(dx + dy), 0, 1)
        rimdk = np.clip(1 - np.abs(rr - 0.95) / 0.08, 0, 1) * np.clip(dx + dy, 0, 1)
        hub = np.clip(1 - rr / 0.35, 0, 1)
        shade = face * (1 - 0.8 * groove) + 0.75 * rimhl - 0.45 * rimdk + 0.15 * hub
        ang_ = np.arctan2(dy, dx)
        rings = np.sin(rr * 26 + cnoise(ang_ * 2, rr * 3, 140 + int(cx)) * 3) * 0.1 * (rr < 0.75)
        crack = np.clip(1 - np.abs(((ang_ * 7 / np.pi + 0.3) % 2) - 1) / 0.08, 0, 1) * (rr > 0.38) * (rr < 0.74) * 0.35
        shade = shade + rings - crack
        col = WOOD * 1.3 * np.clip(shade, 0.08, 1.6)[..., None]
        cv.put(sl, col, cov)
    # the near cheek plate: a rounded slab along the rope, over the wheels'
    # lower halves, with a tongue down to the strap
    def seg_d(X, Y, ax, ay, bx, by):
        vx, vy = bx - ax, by - ay
        tt = np.clip(((X - ax) * vx + (Y - ay) * vy) / (vx * vx + vy * vy), 0, 1)
        return np.hypot(X - ax - tt * vx, Y - ay - tt * vy)
    a0 = (axles[0][0] - upx * 2.2, axles[0][1] - upy * 2.2)
    a1 = (axles[1][0] - upx * 2.2, axles[1][1] - upy * 2.2)
    g = cv.grid(-20, -16, 20, 15)
    sl, X, Y = g
    d = np.minimum(seg_d(X, Y, *a0, *a1) - 4.2, seg_d(X, Y, 0, 0, 0, 10.5) - 2.8)
    cov = np.clip(-d * 1.6 + 0.5, 0, 1)
    bevel = np.clip(1 + d / 1.6, 0, 1)
    # light across the slab and a bright bevel on its upper-left edges
    toward = -(X * LIGHT[0] + Y * LIGHT[1])
    shade = 0.74 + 0.035 * toward - 0.55 * bevel * (toward < 0) + 0.6 * bevel * (toward >= 0)
    # grain along the plate: long wavy lines, a darker knot, worn pale scuffs
    along_ = X * ux + Y * uy
    acr = X * upx + Y * upy
    wav = cnoise(along_ / 6.0, acr / 2.0, 96)
    lines = np.sin(acr * 3.4 + wav * 2.2) * 0.16
    knot = np.exp(-(((along_ - 3.5) / 1.6) ** 2 + ((acr - 1.2) / 1.0) ** 2))
    scuff = np.clip(cnoise(along_ / 2.0, acr / 2.0, 97) * 2 - 1.1, 0, 1) * 0.35
    shade = shade + lines - 0.45 * knot + scuff
    col = WOOD * np.clip(shade, 0.12, 1.6)[..., None]
    cv.put(sl, col, cov)
    # iron bolts at the axles
    for (cx, cy) in axles:
        g = cv.grid(cx - 2.2, cy - 2.2, cx + 2.2, cy + 2.2)
        sl, X, Y = g
        rr = np.hypot(X - cx, Y - cy) / 1.6
        nz = np.sqrt(np.clip(1 - rr ** 2, 0, 1))
        shade = 0.3 + 1.1 * lit((X - cx) / 1.6, (Y - cy) / 1.6, nz)
        rust = np.clip(cnoise((X - cx) * 1.5, (Y - cy) * 1.5, 150 + int(cx)) * 1.5, 0, 1)
        icol = IRON * 1.6 * np.clip(shade, 0, 1.6)[..., None]
        icol = icol + (np.array([120, 62, 28], np.float32) * np.clip(shade, 0.3, 1.2)[..., None] - icol) * (0.6 * rust)[..., None]
        cv.put(sl, icol, np.clip((1 - rr) * 3, 0, 1))
    # the strap and the ring
    g = cv.grid(-2, 10, 2, 17)
    sl, X, Y = g
    tt = np.clip(X / 1.2, -1, 1)
    shade = 0.35 + 0.9 * lit(tt, np.zeros_like(tt), np.sqrt(1 - tt * tt))
    cv.put(sl, IRON * 1.5 * shade[..., None], (np.abs(X) <= 1.2).astype(np.float32))
    g = cv.grid(-5, 14, 5, 24)
    sl, X, Y = g
    rr = np.hypot(X, (Y - 19.0))
    band = np.abs(rr - 3.0)
    cov = np.clip((1.0 - band) * 1.8, 0, 1)
    nx_, ny_ = X / np.maximum(rr, 1e-3), (Y - 19.0) / np.maximum(rr, 1e-3)
    shade = 0.4 + 0.9 * lit(nx_ * (rr - 3.0), ny_ * (rr - 3.0), np.sqrt(np.clip(1 - band ** 2, 0, 1)))
    cv.put(sl, IRON * 1.6 * np.clip(shade, 0, 1.4)[..., None], cov)


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
    save(os.path.join(out, 'zffl.png'), [draw_line()])
    b, f = basket_frames()
    save(os.path.join(out, 'zffb.png'), [b, f])
    print('wrote zffl.png and zffb.png to', out)
    print('line top-left', (LINE_X0, LINE_Y0), 'basket origin', (BASKET_OX, BASKET_OY), 'stops', TOP_STOP, BOT_STOP)
