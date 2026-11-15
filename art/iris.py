"""Draw an original iris hatch for the top of the lab lift's shaft (owner,
2026-11-09: the stone at the top of the shaft should become "something metal
that would be opening and closing around a cylindrical shape"; then: tilt it
back a few degrees, more like brushed steel, and not the black underside).

A machined collar sits where the stone lintel is (x 4249-4338, y 1543-1571,
the lab floor at 1544). Its top, seen a little from above, is a steel ring
round eight curved blades that meet in the middle. When the pod comes
through, the blades turn and draw back into the ring, opening a round hole
just wider than the pod. In the map the lab floor over the shaft is solid
(creatures walk over it), which matches the closed iris.

Two parts, as separate objects in lift.cos:
  0-6  the back: the ring's far half, the blades and (as they open) the
       dark hole, closed (0) to open (6); drawn behind the pod
  7    the front: the ring's near half and the collar's face, drawn in front
       of the pod, so the pod looks to pass through the ring
PNG with real transparency (see lift.py). Drawn from scratch at 4x (no game
pixels are read or copied), so the output can ship with the project.

The view angle (TILT, degrees above level) and the face's finish (STYLE)
are settings; iris_options.py draws the choices side by side.

    python3 iris.py OUT_DIR

writes OUT_DIR/zlir.png and a preview, and prints where to place it.
"""
import sys
import os
import math
import numpy as np
from PIL import Image
from catwalk import _noise
from lift import (Layer, soft, blur, mix, cyl_light, write_png, SS,
                  GRAPH_D, LED_DIM, ST_D, ST, ST_L, BRONZE_D, BRONZE, BRONZE_L,
                  brushed_h, brushed_ring)
import lift

N = 8                        # blades
STEPS = [0.0, 0.1, 0.28, 0.5, 0.72, 0.9, 1.0]   # open fraction per frame (eased)
IW = 96
X0 = 4245
CX = 48.0                    # the hole's centre: world x 4293 (the pod's)
FLOOR_Y = 1544               # ... at the lab floor
FACE_H = 28.5                # the face, from the floor line down to y 1572.5
RIN = 44.6                   # the hole, fully open (the pod's widest part is 43.5)
ROUT = 47.6                  # the ring's outer edge

TILT = 4.0                   # degrees above level (owner chose the flatter view)
STYLE = 'bronze'             # owner's choice of the options (iris_options.py)


def geom():
    """Sizes that follow from the tilt."""
    sq = math.sin(math.radians(TILT))
    cy = math.ceil(ROUT * sq) + 2.0
    ih = int(math.ceil(cy + FACE_H + 1.5))
    return sq, cy, ih, FLOOR_Y - int(cy)


def polar(X, Y, sq, cy):
    ux = X - CX
    vy = (Y - cy) / sq
    return np.hypot(ux, vy), np.arctan2(vy, ux), ux, vy


def back(f):
    sq, cy, ih, _ = geom()
    L = Layer(IW, ih)
    X, Y = L.X, L.Y
    r, th, ux, vy = polar(X, Y, sq, cy)
    ra = f * RIN                                   # the hole's radius now
    # the hole: looking down into the shaft, its far inner wall faintly lit
    hole = soft(r - ra, 0.25 / sq) * (r < RIN + 0.5)
    wall = np.clip(-vy / RIN, 0, 1)
    L.over(hole, mix(np.array([10, 11, 14], np.float32), np.array([70, 60, 52], np.float32), wall ** 2 * 0.8))
    # the blades: between the hole and the ring, curved leaves of brushed
    # steel; seams follow spirals that turn as the iris opens
    blades = soft(ra - r, 0.25) * (r <= RIN + 0.6)
    twist, rot = 2.1, f * 1.1
    phase = ((th - twist * (r / RIN) - rot) % (2 * np.pi)) / (2 * np.pi / N)
    k = np.floor(phase)
    t = phase - k                                  # 0 at a blade's seam, 1 at the next
    tone = 0.58 + 0.2 * (k % 2) + 0.3 * (t - 0.5)
    tone = tone * (0.85 + 0.15 * np.clip(-vy / RIN, -1, 1))     # the far side a little lighter
    L.over(blades, mix(ST_D, ST_L, tone) * brushed_ring(r, 81))
    seam = blades * np.exp(-(np.minimum(t, 1 - t) / 0.07) ** 2) * np.clip(r / RIN + 0.3, 0, 1)
    L.over(seam * 0.85, GRAPH_D)
    L.over(blades * np.exp(-((t - 0.06) / 0.03) ** 2) * np.clip(r / RIN, 0, 1) * 0.5, ST_L)
    L.over(blades * np.clip((r - (RIN - 2.5)) / 2.5, 0, 1) * 0.3, GRAPH_D)
    # the ring, far half: flat brushed steel, a thin cool light line inside
    ring = soft(r - ROUT, 0.25 / sq) * soft(RIN - r, 0.25 / sq) * (vy <= 0.5)
    L.over(ring, mix(ST, ST_L, 0.35 + 0.4 * np.clip(-vy / ROUT, 0, 1)) * brushed_ring(r, 82))
    L.over(ring * np.exp(-((r - (RIN + 0.5)) / 0.6) ** 2), LED_DIM * 1.5)
    L.over(soft(np.abs(r - ROUT) - 0.4, 0.3) * (vy <= 0.5) * 0.5, ST_D)
    return L.image()


def front():
    sq, cy, ih, _ = geom()
    L = Layer(IW, ih)
    X, Y = L.X, L.Y
    r, th, ux, vy = polar(X, Y, sq, cy)
    br = brushed_h(IW, ih, 72)
    u = (X - CX) / ROUT
    bottom = cy + FACE_H
    edge_y = cy + ROUT * sq * np.sqrt(np.clip(1 - u * u, 0, 1))  # the ring's near outline
    face = soft(np.abs(X - CX) - ROUT, 0.6) * soft(edge_y - Y, 0.6) * soft(Y - bottom, 0.6)
    d = Y - edge_y                                 # depth down the face
    H = bottom - edge_y

    def steel(mask, tone=1.0, spec=0.7, base=ST):
        L.over(mask, cyl_light(u * 0.97, base * tone, amb=0.48, spec=spec, shine=20) * br)

    def groove(at, strength=0.75, width=0.45):
        L.over(face * np.exp(-((d - at) / width) ** 2) * strength, ST_D * 0.6)

    def bevel_bottom(base=ST):
        # a rounded bottom edge: a bright line where it turns toward the
        # light, then darker as it turns under (steel, not black)
        b = face * soft(-(d - (H - 3.2)), 0.5)
        k = np.clip((d - (H - 3.2)) / 3.2, 0, 1)
        L.over(b, cyl_light(u * 0.97, base * (1.15 - 0.55 * k)[..., None], amb=0.5, spec=0.8, shine=22) * br)
        L.over(face * np.exp(-((d - (H - 2.9)) / 0.45) ** 2) * 0.6, ST_L)

    def bolts(at, base=ST):
        for bx in np.arange(-36, 37, 12):
            by = cy + ROUT * sq * math.sqrt(max(0.0, 1 - (bx / ROUT) ** 2)) + at
            dd = np.hypot(X - (CX + bx), Y - by)
            L.over(soft(dd - 1.2, 0.4) * face, cyl_light(np.clip((X - (CX + bx)) / 1.3, -1, 1) * 0.9, base * 0.75, amb=0.4, spec=0.9, shine=24))

    light = face * np.exp(-((d - 2.2) / 0.4) ** 2) * (np.abs(u) < 0.85) * 0.8

    if STYLE == 'steel':
        # brushed steel all the way down, in three courses split by fine
        # grooves, a rounded bottom edge
        steel(face, 1.0)
        steel(face * soft(np.abs(d - 9.5) - 4.0, 0.5), 1.12, spec=0.9)
        groove(4.6)
        groove(14.4)
        bolts(20.5)
        bevel_bottom()
        L.over(light, LED_DIM * 1.6)
    elif STYLE == 'bronze':
        # brushed steel with a warm bronze band and a bronze bottom lip,
        # picking up the copper of the shaft and the lab's warm light
        steel(face, 1.0)
        L.over(face * soft(np.abs(d - 9.5) - 4.0, 0.5), cyl_light(u * 0.97, BRONZE, amb=0.48, spec=0.8, shine=20) * br)
        groove(4.6)
        groove(14.4)
        bolts(20.5)
        bevel_bottom(BRONZE)
        L.over(light, LED_DIM * 1.6)
    elif STYLE == 'fluted':
        # a plain brushed steel band on top, then a skirt of vertical flutes,
        # each catching the light on its left side
        steel(face, 1.05)
        skirt = face * soft(-(d - 9.0), 0.5)
        ang = np.arcsin(np.clip(u, -0.999, 0.999))
        fl = ((ang / (np.pi / 2)) * 13) % 1.0       # 26 flutes across the front
        fn = np.sin(fl * 2 * np.pi)
        L.over(skirt, cyl_light(np.clip(u * 0.6 + fn * 0.35, -0.99, 0.99), ST, amb=0.45, spec=0.75, shine=18) * br)
        groove(8.6, 0.85)
        bevel_bottom()
        L.over(light, LED_DIM * 1.6)
    elif STYLE == 'channel':
        # brushed steel with a recessed channel round the middle holding the
        # light line, and a rounded bottom edge
        steel(face, 1.05)
        ch = face * soft(np.abs(d - 11) - 2.2, 0.4)
        L.over(ch, cyl_light(u * 0.97, ST_D * 1.1, amb=0.5, spec=0.3) * br)
        L.over(face * np.exp(-((d - 8.8) / 0.4) ** 2) * 0.7, ST_D * 0.5)
        L.over(face * np.exp(-((d - 13.3) / 0.4) ** 2) * 0.8, ST_L)
        L.over(face * np.exp(-((d - 11) / 0.45) ** 2) * (np.abs(u) < 0.9), LED_DIM * 1.8)
        bolts(19.5)
        bevel_bottom()
    # the ring, near half: on top of the face, in front of the pod
    ring = soft(r - ROUT, 0.25 / sq) * soft(RIN - r, 0.25 / sq) * (vy > 0.5)
    L.over(ring, mix(ST_D, ST, 0.85 - 0.3 * np.clip(vy / ROUT, 0, 1)) * brushed_ring(r, 82))
    L.over(soft(np.abs(r - RIN) - 0.35, 0.3) * (vy > 0.5) * 0.8, ST_L)
    L.over(ring * np.exp(-((r - (RIN + 0.5)) / 0.6) ** 2) * 0.8, LED_DIM * 1.5)
    if STYLE == 'bronze':
        L.over(soft(np.abs(r - ROUT) - 0.5, 0.3) * (vy > 0.5) * 0.8, BRONZE)
    return L.image()


def frames():
    return [back(f) for f in STEPS] + [front()]


if __name__ == '__main__':
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    fr = frames()
    write_png(os.path.join(out, 'zlir.png'), fr)
    sq, cy, ih, y0 = geom()
    sheet = Image.new('RGBA', (IW * 4 + 50, ih * 2 + 15), (180, 120, 70, 255))
    for i, k in enumerate((0, 2, 4, 6)):
        im = fr[k].copy()
        im.alpha_composite(fr[7])
        sheet.alpha_composite(im, (5 + i * (IW + 10), 5))
        sheet.alpha_composite(fr[k], (5 + i * (IW + 10), ih + 10))
    sheet.resize((sheet.width * 4, sheet.height * 4), Image.NEAREST).save(os.path.join(out, 'iris-sheet.png'))
    print('wrote', out, 'place at', X0, y0)
