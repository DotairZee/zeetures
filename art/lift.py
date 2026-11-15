"""Draw an original modern glass lift pod and its call pillars for the shaft
in the domed lab.

Owner, 2026-11-09: a pod with glass; callers in keeping with the lab,
standing behind the platform railings; then "more modern", with glass like
the game's observation tube under the Gelsemium pit (by our sand ramp):
nearly clear, a little darker than what's behind it, soft edges and soft,
blurry reflections rather than hard white lines.

These are PNG sprites with real transparency (openc2e loads NAME.png before
NAME.s16 for C2; frames are stacked top to bottom and counted in a tEXt
chunk "openc2e-frames"). So the glass can be truly see-through, with soft
reflections, and every edge is smooth.

The pod (zlft.png, 90 x 116), ten frames for four parts of one vehicle
(lift.cos):
  0    the floor, seen a little from above, behind riders
  1    the far side of the glass with two rear frame posts, behind the floor
       and riders
  2-7  the near glass, over riders: two curved sliding doors with
       aluminium edges, closed (2) to open (7); the doors slide round to
       the front frame posts, their reflections going with them
  8    the top housing, front frame posts and base, over riders, lights dim
  9    the same, lights bright (moving)
The call pillar (zlbt.png, 25 x 75; drawn as a 20 x 60 design at 1.25x,
owner 2026-11-09: they read small beside similar devices): a slim pillar with
edges, a small dark display with up/down arrows and a round touch button
ringed with light (frame 0 idle, 1 called).
The railing pieces (zlrl.png, one frame per platform caller): the platforms'
copper railings are part of the background, so for each platform caller we
draw, from scratch, just the bits of railing that pass in front of it (the
top rail and any post, on lines measured from the background in a local
preview), only where the caller is, so nothing doubles up.

Drawn from scratch at 4x and downscaled (no game pixels are read or
copied), so the output can ship with the project.

    python3 lift.py OUT_DIR

writes OUT_DIR/zlft.png, zlbt.png and zlrl.png, a preview sheet, and prints
where the callers go.
"""
import sys
import os
import numpy as np
from PIL import Image, ImageDraw, PngImagePlugin
from catwalk import _noise

SS = 4

def grid(w, h):
    xs = (np.arange(w * SS, dtype=np.float32) + 0.5) / SS
    ys = (np.arange(h * SS, dtype=np.float32) + 0.5) / SS
    return np.meshgrid(xs, ys)


# ---- soft layers with real transparency ----------------------------------
class Layer:
    """A picture built at 4x from soft-edged pieces, each with its own
    opacity, laid one over another; downscaled with its transparency kept."""
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.X, self.Y = grid(w, h)
        self.rgb = np.zeros(self.X.shape + (3,), np.float32)
        self.a = np.zeros(self.X.shape, np.float32)

    def over(self, alpha, col):
        """Lay color col over the layer with opacity alpha (0-1 per
        sub-pixel)."""
        alpha = np.clip(np.asarray(alpha, np.float32), 0, 1)
        col = np.asarray(col, np.float32)
        if col.ndim == 1:
            col = np.broadcast_to(col, self.rgb.shape)
        out_a = alpha + self.a * (1 - alpha)
        num = col * alpha[..., None] + self.rgb * (self.a * (1 - alpha))[..., None]
        self.rgb = np.where(out_a[..., None] > 1e-6, num / np.maximum(out_a, 1e-6)[..., None], 0)
        self.a = out_a

    def image(self):
        # downscale premultiplied, then unpremultiply
        pre = self.rgb * self.a[..., None]
        h, w = self.h, self.w
        pre = pre.reshape(h, SS, w, SS, 3).mean((1, 3))
        a = self.a.reshape(h, SS, w, SS).mean((1, 3))
        rgb = np.where(a[..., None] > 1e-4, pre / np.maximum(a, 1e-4)[..., None], 0)
        out = np.dstack([rgb, a * 255]).clip(0, 255).astype(np.uint8)
        out[out[..., 3] < 3] = 0
        return Image.fromarray(out, 'RGBA')


def soft(d, width):
    """Coverage of an edge: 1 inside (d < 0), fading out over `width` px."""
    return np.clip(0.5 - d / max(width, 1e-3), 0, 1)


def blur(a, px):
    """Gaussian blur of a 4x array by px (final) pixels."""
    s = px * SS
    r = int(s * 3) + 1
    k = np.exp(-0.5 * (np.arange(-r, r + 1) / s) ** 2)
    k /= k.sum()
    a = np.pad(a, ((0, 0), (r, r)), mode='edge')
    a = np.apply_along_axis(lambda v: np.convolve(v, k, 'valid'), 1, a)
    a = np.pad(a, ((r, r), (0, 0)), mode='edge')
    return np.apply_along_axis(lambda v: np.convolve(v, k, 'valid'), 0, a)


def brushed(w, h, seed, along='x', amount=0.06):
    """Brushed-metal streaks at 4x (fine noise stretched along one axis),
    plus a soft mottle, as a brightness factor around 1."""
    rng = np.random.default_rng(seed)
    if along == 'x':
        n = np.repeat(rng.normal(0, 1, (h * SS, 1)), w * SS, 1) * 0.6
    else:
        n = np.repeat(rng.normal(0, 1, (1, w * SS)), h * SS, 0) * 0.6
    n = n + rng.normal(0, 1, (h * SS, w * SS)) * 0.25
    m = np.kron(_noise((h, w), seed + 7, 4), np.ones((SS, SS)))
    return (1 + amount * n + amount * 0.6 * m).astype(np.float32)


def mix(a, b, t):
    t = np.clip(np.asarray(t, np.float32), 0, 1)[..., None]
    return np.asarray(a, np.float32) * (1 - t) + np.asarray(b, np.float32) * t


# modern materials: graphite, brushed aluminium, cool white light
GRAPH_D = np.array([18, 20, 24], np.float32)
GRAPH = np.array([44, 48, 55], np.float32)
GRAPH_L = np.array([96, 104, 114], np.float32)
ALU_D = np.array([92, 98, 106], np.float32)
ALU = np.array([164, 170, 178], np.float32)
ALU_L = np.array([226, 230, 234], np.float32)
LED = np.array([214, 244, 255], np.float32)
LED_DIM = np.array([96, 140, 160], np.float32)
TINT = np.array([18, 40, 56], np.float32)          # the glass's own dark blue-green
SHEEN = np.array([190, 222, 238], np.float32)      # reflections


def cyl_light(u, base, amb=0.35, spec=0.6, shine=18):
    """Light from the upper left on a vertical cylinder; u from -1 (left
    edge) to 1 (right edge)."""
    u = np.clip(u, -0.999, 0.999)
    nz = np.sqrt(1 - u * u)
    d = np.clip(u * -0.55 + nz * 0.83, 0, 1)
    s = np.clip(u * -0.29 + nz * 0.96, 0, 1) ** shine * spec
    rim = np.clip(u - 0.75, 0, 1) * 0.6           # a little back light on the right edge
    base = np.asarray(base, np.float32)
    return base * (amb + (1 - amb) * d + rim)[..., None] + 255 * s[..., None]


# ---- worn brushed steel and bronze ---------------------------------------
# shared by the pod and the shaft-top iris (owner, 2026-11-09: the iris's
# steel and bronze, "slightly darker and a bit more scuffed up/brushed";
# then the pod "similar texturally and with the bronze piece")
ST_D = np.array([50, 53, 58], np.float32)
ST = np.array([118, 123, 128], np.float32)
ST_L = np.array([188, 192, 196], np.float32)
BRONZE_D = np.array([66, 40, 20], np.float32)
BRONZE = np.array([140, 92, 48], np.float32)
BRONZE_L = np.array([206, 160, 102], np.float32)
BRUSH = 0.13                 # how strong the brushing streaks are
SCUFF = 0.3                  # how strong the scuffs and scratches are


def scuffs(w, h, seed, count):
    """Wear at 4x, as a signed field: short scratches (mostly along the
    brushing, some across it; bright where they catch the light, dark where
    grime sits in them) and soft grimy patches."""
    rng = np.random.default_rng(seed)
    im = Image.new('F', (w * SS, h * SS), 0.0)
    d = ImageDraw.Draw(im)
    for _ in range(count):
        x, y = rng.uniform(0, w * SS), rng.uniform(0, h * SS)
        ang = rng.normal(0, 0.25) if rng.random() < 0.75 else rng.uniform(0, np.pi)
        ln = rng.uniform(4, 22) * SS / 2
        val = rng.choice([1.0, -1.0], p=[0.6, 0.4]) * rng.uniform(0.4, 1.0)
        d.line([(x, y), (x + np.cos(ang) * ln, y + np.sin(ang) * ln)], fill=float(val), width=1)
    sc = np.asarray(im, np.float32)
    grime = np.kron(_noise((h, w), seed + 11, 6), np.ones((SS, SS)))
    return sc - 0.6 * np.clip(grime, 0, None)


def brushed_h(w, h, seed, amount=None):
    """Horizontal brushing: long fine streaks, a little mottle, and wear."""
    amount = BRUSH if amount is None else amount
    rng = np.random.default_rng(seed)
    rows = rng.normal(0, 1, (h * SS, 1))
    n = np.repeat(rows, w * SS, 1) * 0.55 + rng.normal(0, 1, (h * SS, w * SS)) * 0.3
    n += np.repeat(rng.normal(0, 1, (h * SS // 2, 1)), 2, 0).repeat(w * SS, 1) * 0.3
    m = np.kron(_noise((h, w), seed + 7, 4), np.ones((SS, SS)))
    wear = scuffs(w, h, seed + 3, w * h // 22)
    return (1 + amount * n + amount * 0.5 * m + SCUFF * wear).astype(np.float32)[..., None]


def brushed_ring(r, seed, amount=None):
    """Circular brushing on a flat ring (streaks that follow the radius),
    with the same wear as the face."""
    amount = BRUSH if amount is None else amount
    rng = np.random.default_rng(seed)
    table = rng.normal(0, 1, 4096)
    idx = np.clip((r * 40).astype(int), 0, 4095)
    h, w = r.shape[0] // SS, r.shape[1] // SS
    wear = scuffs(w, h, seed + 5, w * h // 30)
    return (1 + amount * table[idx] + SCUFF * wear).astype(np.float32)[..., None]


# ---- the pod -------------------------------------------------------------
LW, LH = 90, 116
FLOOR = 102                 # the floor line (cabin bottom), from its top
CX = LW / 2
R = 42.0                    # the glass cylinder's radius
GTOP = 15.0                 # glass starts under the top housing
EY = 3.0                    # how deep the ellipses look: seen about 4 degrees from above, as the shaft-top iris (owner chose the flatter view)
POST_U = 0.9                # the side frames' inner edge, as a fraction of the radius
DOOR_STEPS = [0.0, 0.14, 0.38, 0.64, 0.86, 1.0]   # door frames, closed to open (eased)
LEAD_OPEN = 0.86            # where a door's leading edge ends up when open


def ell(X, Y, cy, rx, ry):
    """Roughly the distance (px) outside an ellipse centred at (CX, cy)."""
    k = np.sqrt(((X - CX) / rx) ** 2 + ((Y - cy) / ry) ** 2)
    return (k - 1) * min(rx, ry * 3)


def metal_bar(L, x, half, y0, y1, base, lit_side=-1, alpha=1.0):
    """A vertical metal bar (a rounded-square section, lit from the left)."""
    X, Y = L.X, L.Y
    m = soft(np.abs(X - x) - half, 0.6) * soft(y0 - Y, 0.6) * soft(Y - y1, 0.6)
    L.over(m * alpha, cyl_light((X - x) / max(half, 0.5) * 0.85, base, amb=0.45, spec=0.7, shine=20))


def pod_floor():
    L = Layer(LW, LH)
    X, Y = L.X, L.Y
    # the floor's top: a light grey disc (its far half shows above the line)
    d = ell(X, Y, FLOOR, R - 1, EY)
    top = soft(d, 1.0) * (Y <= FLOOR + 0.2)
    u = (X - CX) / R
    col = mix(ST, ST_L, 0.45 - 0.35 * u + 0.25 * (FLOOR - Y) / EY)
    col = col * brushed_h(LW, LH, 61)
    L.over(top, col)
    # a soft cool light ring set into it
    ring = np.sqrt(((X - CX) / (R - 6)) ** 2 + ((Y - FLOOR) / (EY - 1.2)) ** 2)
    L.over(np.exp(-((ring - 1) / 0.06) ** 2) * (Y <= FLOOR) * 0.8, LED_DIM * 1.3)
    return L.image()


def pod_glass_back():
    # the far wall of the cylinder seen through the near one, in the tube's
    # manner: a slight darkening of what's behind, deeper toward the edges, a
    # soft bright band where light gathers on the inside, a soft blue glow
    L = Layer(LW, LH)
    X, Y = L.X, L.Y
    u = (X - CX) / R
    inside = soft(np.abs(X - CX) - R + 0.5, 1.2) * soft(GTOP - Y, 1.0) * soft(Y - FLOOR, 1.0)
    edge = np.clip(np.abs(u), 0, 1) ** 4
    L.over((0.2 + 0.35 * edge) * inside, TINT)
    band = blur(np.exp(-((u - 0.38) / 0.12) ** 2) * ((Y > GTOP + 6) & (Y < FLOOR - 4)), 2.0)
    L.over(band * 0.16 * inside, SHEEN)
    spot = blur(np.exp(-(((X - CX - 14) / 3.5) ** 2 + ((Y - 70) / 10) ** 2)), 1.5)
    L.over(spot * 0.18 * inside, np.array([110, 170, 230], np.float32))
    return L.image()


def pod_glass_front(f):
    """The near glass: two curved sliding doors meeting in the middle, with
    aluminium leading edges, open by fraction f (0 closed, 1 open). Each
    door slides round toward its frame post, its reflections going with it."""
    L = Layer(LW, LH)
    X, Y = L.X, L.Y
    u = (X - CX) / R
    rows = soft(GTOP - 1 - Y, 1.0) * soft(Y - FLOOR - 1, 1.0)
    inside = soft(np.abs(X - CX) - R, 0.8) * rows
    lead = f * LEAD_OPEN                        # each leading edge's |u|
    au = np.abs(u)
    # fixed glass outside the posts (the cylinder's turning sides)
    side = (au > POST_U).astype(np.float32)
    # the doors: between the leading edge and the post on each side
    door = ((au >= lead) & (au <= POST_U + 0.02)).astype(np.float32)
    pane = inside * np.maximum(side, door)
    # door coordinate: 0 at the leading edge, 1 at the post
    sdoor = np.clip((au - lead) / max(POST_U - lead, 1e-3), 0, 1)
    edge = np.clip(au, 0, 1)
    L.over((0.05 + 0.4 * edge ** 6) * pane, mix(TINT, SHEEN, 0.35 + 0.4 * edge ** 3))
    dd = np.abs(X - CX) - R
    L.over(np.exp(-((dd + 1.6) / 0.7) ** 2) * inside * 0.35, np.array([8, 20, 28], np.float32))
    L.over(np.exp(-((dd + 0.4) / 0.45) ** 2) * inside * (0.35 + 0.25 * (u < 0)), SHEEN)
    # reflections ride on the doors: placed by door coordinate
    body = ((Y > GTOP + 4) & (Y < FLOOR - 3)).astype(np.float32)
    fade = np.clip((Y - GTOP - 4) / 18, 0, 1) * np.clip((FLOOR - 3 - Y) / 22, 0, 1)
    on_door = door * body
    left = (u < 0).astype(np.float32)
    broad = blur(np.exp(-((sdoor - 0.7) / 0.13) ** 2) * on_door * left, 1.6) * fade
    core = blur(np.exp(-((sdoor - 0.74) / 0.03) ** 2) * on_door * left, 0.6) * fade
    right = blur(np.exp(-((sdoor - 0.65) / 0.06) ** 2) * on_door * (1 - left), 1.2) * fade
    L.over(broad * 0.22 * inside, SHEEN)
    L.over(core * 0.5 * inside, np.array([236, 248, 255], np.float32))
    L.over(right * 0.16 * inside, SHEEN)
    # the doors' leading edges: slim worn steel stiles with a bright edge, a
    # dark gap between them when closed
    br = brushed_h(LW, LH, 64)
    for s in (-1, 1):
        x = CX + s * (lead * R + 1.0)
        m = soft(np.abs(X - x) - 1.0, 0.6) * soft(GTOP - 0.5 - Y, 0.6) * soft(Y - (FLOOR + 0.5), 0.6)
        L.over(m, cyl_light((X - x) * 0.85, ST, amb=0.48, spec=0.7, shine=20) * br)
        L.over(soft(np.abs(X - (x - s * 0.7)) - 0.25, 0.4) * soft(GTOP - Y, 0.6) * soft(Y - FLOOR, 0.6) * 0.7, ST_L)
    if f == 0:
        L.over(soft(np.abs(X - CX) - 0.35, 0.4) * rows, ST_D * 0.45)
    return L.image()


def pod_shell(lights_on):
    """The top housing, the side frames and the base, in the shaft-top
    iris's worn brushed steel with bronze bands, and thin light lines."""
    L = Layer(LW, LH)
    X, Y = L.X, L.Y
    u = (X - CX) / (R + 1)
    led = LED if lights_on else LED_DIM
    br = brushed_h(LW, LH, 62)

    def groove(mask, y, strength=0.75):
        L.over(mask * np.exp(-((Y - y) / 0.4) ** 2) * strength, ST_D * 0.55)

    # the top housing: a steel cylinder (y 2.5-15) with an elliptical top, a
    # bronze band between fine grooves, and a light line along its foot
    hx = R + 1.5
    side = soft(np.abs(X - CX) - hx, 0.8) * soft(2.5 - Y, 0.8) * soft(Y - 15.0, 0.8)
    L.over(side, cyl_light(u, ST, amb=0.48, spec=0.7, shine=20) * br)
    band = side * soft(np.abs(Y - 7.2) - 2.6, 0.5)
    L.over(band, cyl_light(u, BRONZE, amb=0.48, spec=0.8, shine=20) * br)
    groove(side, 4.4)
    groove(side, 10.1)
    groove(side, 12.6, 0.5)
    tf = soft(ell(X, Y, 2.5, hx, 1.5), 0.8) * (Y < 2.5)
    L.over(tf, mix(ST_D, ST, 0.75 - 0.4 * u) * br)
    L.over(side * np.exp(-((Y - 14.2) / 0.45) ** 2) * (0.95 if lights_on else 0.6), led)
    if lights_on:
        L.over(blur(np.exp(-((Y - 15.2) / 0.8) ** 2) * (np.abs(X - CX) < R), 1.2) * 0.35, LED)

    # the side frames, where the doors slide to: steel uprights along the
    # pod's sides, each with a bronze strip toward the middle and a light line
    for s in (-1, 1):
        x0, x1 = CX + s * POST_U * R, CX + s * (R + 1.5)
        xm, hw = (x0 + x1) / 2, abs(x1 - x0) / 2
        m = soft(np.abs(X - xm) - hw, 0.6) * soft(GTOP - 1 - Y, 0.6) * soft(Y - (FLOOR + 2), 0.6)
        L.over(m, cyl_light((X - xm) / hw * 0.85, ST, amb=0.48, spec=0.7, shine=20) * br)
        strip = soft(np.abs(X - (x0 + s * 0.9)) - 0.9, 0.5) * soft(GTOP - 1 - Y, 0.6) * soft(Y - (FLOOR + 2), 0.6)
        L.over(strip, cyl_light(np.full_like(X, -0.3 * s), BRONZE, amb=0.5, spec=0.7) * br)
        L.over(soft(np.abs(X - (xm + s * 0.6)) - 0.3, 0.4) * soft(GTOP + 4 - Y, 0.6) * soft(Y - (FLOOR - 4), 0.6) * (0.9 if lights_on else 0.45), led)

    # the base: a steel lip in front of the floor (the near half of the
    # floor's ellipse as a band) with a light line, then a steel underbody
    # with a bronze ring, tapering to a rounded bottom edge
    lip_out = ell(X, Y, FLOOR, R + 1.8, EY + 1.6)
    lip = soft(lip_out, 0.8) * (Y > FLOOR) * soft(Y - (FLOOR + EY + 2.2), 0.6)
    lip_top = FLOOR + (EY + 0.2) * np.sqrt(np.clip(1 - ((X - CX) / (R + 1.8)) ** 2, 0, 1))
    lv = np.clip((Y - lip_top + 2.6) / 5.0, 0, 1)
    L.over(lip, cyl_light(u, ST * (1.15 - 0.3 * lv)[..., None], amb=0.5, spec=0.8, shine=20) * br)
    L.over(lip * np.exp(-((Y - (lip_top - 0.6)) / 0.45) ** 2), led)
    by0 = FLOOR + EY + 1.6
    bw = R + 1.5 - np.clip(Y - by0, 0, None) * 2.2
    body = soft(np.abs(X - CX) - bw, 0.8) * soft(by0 - Y, 0.6) * soft(Y - (LH - 0.8), 0.8)
    bu = (X - CX) / np.maximum(bw, 1)
    shade = 1 - 0.4 * np.clip((Y - by0) / (LH - by0), 0, 1)
    L.over(body, cyl_light(bu, ST * shade[..., None], amb=0.45, spec=0.6, shine=18) * br)
    ring = body * soft(np.abs(Y - (by0 + 2.4)) - 1.3, 0.5)
    L.over(ring, cyl_light(bu, BRONZE, amb=0.48, spec=0.8, shine=20) * br)
    groove(body, by0 + 0.6, 0.6)
    groove(body, by0 + 4.2, 0.6)
    L.over(body * np.exp(-((Y - (by0 + 6.3)) / 0.4) ** 2) * (np.abs(X - CX) < 9) * 0.9, led)
    if lights_on:
        L.over(blur(np.exp(-((Y - (by0 + 6.3)) / 0.6) ** 2) * (np.abs(X - CX) < 9), 1.4) * 0.33, LED)
    L.over(body * np.exp(-((Y - (LH - 1.6)) / 0.45) ** 2) * 0.5, ST_L)
    # the guide shoes that run in the shaft's walls, either side
    for s in (-1, 1):
        gx = CX + s * (R + 2.0)
        for gy in (8.5, FLOOR + 3):
            shoe = soft(np.abs(X - gx) - 2.0, 0.6) * soft(np.abs(Y - gy) - 3.2, 0.6)
            L.over(shoe, mix(ST_D, ST, 0.7 - 0.25 * s - (Y - gy) / 10) * br)
    return L.image()


# ---- the call pillar -----------------------------------------------------
CK = 1.25                                    # callers drawn 25% bigger (owner, 2026-11-09)
CW0, CH0 = 20, 60                            # the design's size; the picture is CK times it
CW, CH = round(CW0 * CK), round(CH0 * CK)


def caller(called):
    """A call pillar in the pod's worn brushed steel, with bronze edge trims,
    a bronze band and foot, a dark glass display and a touch button."""
    L = Layer(round(CW0 * CK), round(CH0 * CK))
    X, Y = L.X / CK, L.Y / CK                 # drawn in the design's units
    CW, CH = CW0, CH0                         # (the design's size, here)
    cx = CW / 2
    pw = 6.5                                  # half width of the pillar
    u = (X - cx) / pw
    br = brushed_h(L.w, L.h, 63)
    # a soft contact shadow on the deck
    L.over(blur(soft(np.abs(X - cx) - pw - 1, 1.0) * soft(np.abs(Y - (CH - 0.5)) - 0.6, 0.8), 0.8) * 0.45, np.array([0, 0, 0], np.float32))
    # the pillar: steel, a flat face with rounded sides and rounded top
    # corners, on a bronze foot
    r = 2.6
    qx = np.abs(X - cx) - (pw - r)
    qy = np.maximum(r + 1 - Y, 0)
    dtop = np.where((qx > 0) & (Y < r + 1), np.hypot(np.maximum(qx, 0), qy) - r, np.maximum(np.abs(X - cx) - pw, 1 - Y))
    body = soft(dtop, 0.8) * soft(Y - (CH - 2.0), 0.6)
    face = np.clip(np.abs(u) - 0.55, 0, 1) / 0.45 * np.sign(u)
    L.over(body, cyl_light(face * 0.95, ST * 0.78, amb=0.42, spec=0.35, shine=20) * br)
    # bronze edge trims down both sides
    for s in (-1, 1):
        trim = body * soft(np.abs(X - (cx + s * (pw - 0.9))) - 0.55, 0.4)
        L.over(trim * 0.95, cyl_light(np.full_like(X, s * 0.6), BRONZE * 0.9, amb=0.45, spec=0.5) * br)
    # a bronze band low on the pillar between fine grooves
    band = body * soft(np.abs(Y - (CH - 9.0)) - 2.4, 0.5)
    L.over(band, cyl_light(face * 0.95, BRONZE * 0.82, amb=0.42, spec=0.4, shine=20) * br)
    for gy in (CH - 12.0, CH - 6.0, 4.6):
        L.over(body * np.exp(-((Y - gy) / 0.4) ** 2) * 0.7, ST_D * 0.55)
    foot = soft(np.abs(X - cx) - pw - 1.2, 0.6) * soft(np.abs(Y - (CH - 1.3)) - 0.9, 0.5)
    L.over(foot, cyl_light((X - cx) / (pw + 1.2), BRONZE * 0.75, amb=0.45, spec=0.5) * br)
    # the display: dark glass with up and down arrows in cool white (bright
    # when called) and a faint reflection across its top
    dsp = soft(np.maximum(np.abs(X - cx) - 4.6, np.abs(Y - 7.2) - 3.2), 0.5)
    bez = soft(np.maximum(np.abs(X - cx) - 5.4, np.abs(Y - 7.2) - 4.0), 0.5)
    L.over(bez * 0.9, ST_D * 0.8 * br)
    L.over(dsp, np.array([10, 14, 18], np.float32))
    ac = LED if called else np.array([90, 120, 135], np.float32)
    for ay, up in ((5.6, True), (8.9, False)):
        k = (Y - (ay - 1.1)) if up else ((ay + 1.1) - Y)
        tri = (np.abs(X - cx) <= k * 1.1) & (k >= 0) & (np.abs(Y - ay) <= 1.1)
        L.over(soft(np.where(tri, -1.0, 1.0), 0.5) * dsp * 0.95, ac)
    L.over(dsp * np.clip(1 - (Y - 4.2) / 1.6, 0, 1) * 0.12, SHEEN)
    # the touch button: a round dark-glass disc with a ring of light (soft
    # cyan when idle, bright white with a glow when called)
    by = 17.0
    bd = np.hypot(X - cx, Y - by)
    L.over(soft(np.abs(bd - 4.0) - 0.45, 0.4), cyl_light(np.clip((X - cx) / 4.5, -1, 1), BRONZE * 0.85, amb=0.45, spec=0.5) * br)
    L.over(soft(bd - 3.6, 0.5), np.array([14, 18, 22], np.float32))
    ring_c = LED if called else np.array([70, 170, 200], np.float32)
    L.over(np.exp(-((bd - 2.9) / 0.45) ** 2), ring_c)
    L.over(soft(bd - 1.2, 0.5) * (0.9 if called else 0.25), ring_c)
    if called:
        L.over(blur(np.exp(-((bd - 2.9) / 0.6) ** 2), 1.2) * 0.4, LED)
    # a slim light line low on the face
    L.over(body * np.exp(-((Y - 26) / 0.4) ** 2) * (np.abs(X - cx) < 3.2) * (0.9 if called else 0.4), ring_c)
    return L.image()


# the platform callers: (name, caller left x, deck y, top rail y, the rail's
# colors (dark, mid, light), posts as (x at top rail, x at deck)), measured
# from a local preview of the background
SITES = [
    ('right', 4445, 1739, 1699, ((60, 38, 14), (140, 92, 40), (226, 170, 78)),
     [(4449.6, 4449.2), (4464.8, 4464.8)]),
    ('left', 4145, 1858, 1819, ((70, 44, 16), (150, 102, 44), (226, 168, 80)),
     [(4141.1, 4153.5), (4162.0, 4170.5)]),
]


def railing_piece(caller_img, site):
    """The bits of the site's railing that pass in front of the caller,
    drawn smooth, only over the caller (its own transparency)."""
    name, x0, deck, rail_y, cols, posts = site
    dark, mid, light = (np.array(c, np.float32) for c in cols)
    L = Layer(CW, CH)
    X, Y = L.X, L.Y
    wx, wy = x0 + X, deck - CH + Y
    # the top rail: a round copper bar about 2 px thick, bright on top
    t = (wy - (rail_y - 0.3)) / 1.0
    L.over(soft(np.abs(t) - 1.0, 0.5), mix(light, dark, np.clip((t + 1) / 2, 0, 1)))
    # the posts, lit on the left
    for xa, xb in posts:
        xl = xa + (xb - xa) * (wy - rail_y) / (deck - rail_y)
        dx = wx - xl
        post = soft(np.abs(dx) - 0.9, 0.5) * (wy > rail_y) * (wy < deck)
        L.over(post, mix(light * 0.85, dark, np.clip((dx + 1) / 2, 0, 1)))
    im = np.asarray(L.image()).astype(np.float32)
    ca = np.asarray(caller_img).astype(np.float32)[..., 3] / 255.0
    im[..., 3] = im[..., 3] * ca
    im[im[..., 3] < 3] = 0
    return Image.fromarray(im.clip(0, 255).astype(np.uint8), 'RGBA')


def write_png(path, frames):
    """Frames stacked top to bottom, counted in a tEXt chunk."""
    w, h = frames[0].size
    sheet = Image.new('RGBA', (w, h * len(frames)), (0, 0, 0, 0))
    for i, f in enumerate(frames):
        assert f.size == (w, h)
        sheet.paste(f, (0, i * h))
    info = PngImagePlugin.PngInfo()
    info.add_text('openc2e-frames', str(len(frames)))
    sheet.save(path, pnginfo=info)


if __name__ == '__main__':
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    pod = [pod_floor(), pod_glass_back()] + [pod_glass_front(f) for f in DOOR_STEPS] + [pod_shell(False), pod_shell(True)]
    calls = [caller(False), caller(True)]
    rails = [railing_piece(calls[0], s) for s in SITES]
    write_png(os.path.join(out, 'zlft.png'), pod)
    write_png(os.path.join(out, 'zlbt.png'), calls)
    write_png(os.path.join(out, 'zlrl.png'), rails)
    for s in SITES:
        print(f"{s[0]} caller: mvto {s[1]} {s[2] - CH}")
    # preview at 3x: the pod's parts in drawing order over a copper stand-in
    # for the shaft (lights off, then on), then the callers and their
    # railing pieces
    sheet = Image.new('RGBA', (LW * 4 + CW * 4 + 90, LH + 10), (150, 74, 34, 255))
    x = 5
    for door, lights in ((2, 9), (4, 8), (7, 8), (5, 9)):
        for i in (1, 0, door, lights):
            sheet.alpha_composite(pod[i], (x, 5))
        x += LW + 10
    for im in calls:
        sheet.alpha_composite(im, (x, 5))
        x += CW + 10
    for r in rails:
        both = calls[0].copy()
        both.alpha_composite(r)
        sheet.alpha_composite(both, (x, 5))
        x += CW + 10
    sheet.resize((sheet.width * 3, sheet.height * 3), Image.NEAREST).save(os.path.join(out, 'lift-sheet.png'))
    print('wrote', out)
