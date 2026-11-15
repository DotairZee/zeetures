"""Draw an original treat dispenser for the domed lab and its treats (owner,
2026-11-09: "maybe it should have some sort of special treat for any
creatures who make it there"; then: make it "more on theme" with the lift
pod, the call pillars and the shaft-top iris).

The dispenser is in the pod's materials: a tinted glass cylinder (the pod's
glass, small) full of glowing gel drops, between a worn brushed steel cap
and a steel base, each with a bronze band, cool white light lines, a dark
glass display and a chute. Frame 0 idle, 1 dispensing: the display, the
chute's light and the glow under the drops come up bright. The treat is a
glossy gel drop with a soft glow, in three colors (frames 0 pink, 1 teal,
2 gold).

PNG with real transparency (see lift.py). Drawn from scratch at 4x (no game
pixels are read or copied), so the output can ship with the project.

    python3 labtreat.py OUT_DIR

writes OUT_DIR/zltd.png (the dispenser) and OUT_DIR/zltt.png (the drops),
with a preview.
"""
import sys
import os
import numpy as np
from PIL import Image
from lift import (Layer, soft, blur, mix, cyl_light, write_png, brushed_h,
                  ST_D, ST, ST_L, BRONZE, LED, LED_DIM, TINT, SHEEN, GRAPH_D)

DW, DH = 34, 50             # the dispenser (its feet on the lab floor)
TW, TH = 10, 9              # a drop
CX = DW / 2

DROPS = [np.array(c, np.float32) for c in ((255, 84, 150), (40, 206, 200), (255, 168, 40), (130, 220, 70))]

GR = 11.5                   # the glass cylinder's radius
G0, G1 = 8.0, 27.0          # its top and bottom
BR = 13.5                   # the cap's and the base's radius
EYR = 1.1                   # how deep the ellipses look (as the pod, seen a little from above)


def ell(X, Y, cy, rx, ry):
    k = np.sqrt(((X - CX) / rx) ** 2 + ((Y - cy) / ry) ** 2)
    return (k - 1) * min(rx, ry * 3)


def drum(L, br, y0, y1, r, base=ST * 0.82, top=True):
    """A short steel cylinder from y0 to y1, its elliptical top showing."""
    X, Y = L.X, L.Y
    u = (X - CX) / r
    side = soft(np.abs(X - CX) - r, 0.6) * soft(y0 - Y, 0.6) * soft(Y - y1, 0.6)
    L.over(side, cyl_light(u, base, amb=0.42, spec=0.4, shine=20) * br)
    if top:
        tf = soft(ell(X, Y, y0, r, EYR), 0.6) * (Y < y0)
        L.over(tf, mix(ST_D, ST, 0.8 - 0.4 * u) * br)
    return side


def band(L, br, mask, y, half, u):
    L.over(mask * soft(np.abs(L.Y - y) - half, 0.4), cyl_light(u, BRONZE * 0.9, amb=0.45, spec=0.6, shine=20) * br)


def groove(L, mask, y, strength=0.7):
    L.over(mask * np.exp(-((L.Y - y) / 0.35) ** 2) * strength, ST_D * 0.55)


def dispenser(lit):
    L = Layer(DW, DH)
    X, Y = L.X, L.Y
    br = brushed_h(DW, DH, 91)
    led = LED if lit else LED_DIM
    # a soft contact shadow on the floor
    L.over(blur(soft(np.abs(X - CX) - BR - 1.5, 1.0) * soft(np.abs(Y - (DH - 0.6)) - 0.7, 0.8), 0.8) * 0.45, np.array([0, 0, 0], np.float32))

    # the glass cylinder, back first: its far wall seen through the near
    # one (a darkening of what's behind, deeper toward the edges, and a soft
    # band where light gathers inside)
    u = (X - CX) / GR
    inside = soft(np.abs(X - CX) - GR + 0.4, 0.8) * soft(G0 - Y, 0.6) * soft(Y - G1, 0.6)
    edge = np.clip(np.abs(u), 0, 1) ** 4
    L.over((0.3 + 0.35 * edge) * inside, TINT)
    L.over(blur(np.exp(-((u - 0.4) / 0.14) ** 2), 1.0) * inside * 0.14, SHEEN)

    # the gel drops heaped in its lower part, each glossy, lit from below
    # by the base's light (brighter when dispensing)
    rng = np.random.default_rng(3)
    pts = []
    for _ in range(600):
        px, py = rng.uniform(CX - GR + 1.6, CX + GR - 1.6), rng.uniform(15.5, G1 - 1.4)
        if any(np.hypot(px - qx, (py - qy) * 1.2) < 3.3 for qx, qy, _ in pts):
            continue
        pts.append((px, py, DROPS[len(pts) % len(DROPS)]))
    pts.sort(key=lambda p: p[1])
    k = 1.12 if lit else 0.95
    for px, py, col in pts:
        d = np.hypot((X - px) / 1.1, Y - py)
        m = soft(d - 1.75, 0.35) * inside
        sh = np.clip(1.0 - np.hypot(X - px + 0.6, Y - py + 0.7) / 2.4, 0, 1)
        L.over(m, mix(col * 0.5, np.minimum(col * 1.1 * k + 20, 255), sh))
        L.over(soft(np.hypot(X - px + 0.7, Y - py + 0.8) - 0.35, 0.3) * m * 0.8, np.array([255, 255, 255], np.float32))
    glow = blur(np.exp(-((Y - (G1 - 1)) / 2.4) ** 2) * (np.abs(X - CX) < GR - 1), 1.0) * inside
    L.over(glow * (0.35 if lit else 0.14), LED)

    # the near glass: darker toward the edges, a bright line down each edge,
    # and a broad soft reflection on the left with a sharp core
    L.over((0.04 + 0.4 * edge ** 2) * inside, mix(TINT, SHEEN, 0.35 + 0.4 * edge))
    dd = np.abs(X - CX) - GR
    L.over(np.exp(-((dd + 0.4) / 0.4) ** 2) * inside * (0.4 + 0.3 * (u < 0)), SHEEN)
    body = np.clip((Y - G0 - 1) / 5, 0, 1) * np.clip((G1 - 1 - Y) / 6, 0, 1)
    L.over(blur(np.exp(-((u + 0.5) / 0.12) ** 2), 0.8) * body * inside * 0.3, SHEEN)
    L.over(blur(np.exp(-((u + 0.52) / 0.035) ** 2), 0.4) * body * inside * 0.55, np.array([236, 248, 255], np.float32))
    L.over(blur(np.exp(-((u - 0.62) / 0.06) ** 2), 0.6) * body * inside * 0.16, SHEEN)

    # the cap: a steel drum with a bronze band between fine grooves, a
    # small steel boss on top, and a light line along its foot
    cap = drum(L, br, 1.6, G0 + 0.4, BR)
    cu = (X - CX) / BR
    band(L, br, cap, 4.6, 1.1, cu)
    groove(L, cap, 3.2)
    groove(L, cap, 6.0)
    L.over(cap * np.exp(-((Y - (G0 - 0.4)) / 0.4) ** 2) * (np.abs(cu) < 0.9) * (0.95 if lit else 0.6), led)
    drum(L, br, 0.5, 1.6, 4.0, ST * 0.9)

    # the base: a steel drum with a light line along its top, a bronze band,
    # a dark glass display (a drop icon, bright when dispensing) and the
    # chute, its mouth lit when a drop comes out; on a bronze foot ring
    b0, b1 = G1 - 0.4, DH - 3.2
    base = drum(L, br, b0, b1, BR, top=False)
    L.over(base * np.exp(-((Y - (b0 + 0.9)) / 0.4) ** 2) * (np.abs(cu) < 0.9) * (0.95 if lit else 0.6), led)
    if lit:
        L.over(blur(np.exp(-((Y - (b0 - 0.3)) / 0.7) ** 2) * (np.abs(X - CX) < GR), 1.0) * 0.3, LED)
    groove(L, base, b0 + 2.2)
    band(L, br, base, b0 + 3.7, 1.0, cu)
    groove(L, base, b0 + 5.2)
    # the display
    dy = b0 + 8.4
    bez = soft(np.maximum(np.abs(X - CX) - 7.2, np.abs(Y - dy) - 2.6), 0.4)
    L.over(bez * 0.9, ST_D * 0.8 * br)
    dsp = soft(np.maximum(np.abs(X - CX) - 6.4, np.abs(Y - dy) - 1.9), 0.4)
    L.over(dsp, np.array([10, 14, 18], np.float32))
    icon = soft(np.hypot((X - CX) / 1.0, (Y - dy - 0.3) / 1.0) - 1.1, 0.3) + \
        soft(np.abs(X - CX) - (Y - (dy - 1.8)) * 0.55, 0.3) * soft(Y - dy, 0.3) * soft(dy - 1.8 - Y, 0.3)
    L.over(np.clip(icon, 0, 1) * dsp * (1.0 if lit else 0.7), np.array([255, 150, 200], np.float32) if lit else np.array([70, 150, 175], np.float32))
    for s in (-1, 1):
        L.over(soft(np.hypot(X - (CX + s * 4.6), Y - dy) - 0.5, 0.3) * dsp * (0.9 if lit else 0.5), led)
    L.over(dsp * np.clip(1 - (Y - (dy - 1.9)) / 1.0, 0, 1) * 0.12, SHEEN)
    # the chute: a rounded dark mouth in a steel bezel, its lip lit
    cy = b0 + 14.0
    mo = np.maximum(np.abs(X - CX) - 5.6, np.abs(Y - cy) - 1.9)
    L.over(soft(mo - 0.9, 0.4), cyl_light(np.clip((X - CX) / 7, -1, 1), ST * 0.85, amb=0.45, spec=0.5) * br)
    mouth = soft(mo, 0.4)
    L.over(mouth, mix(np.array([6, 7, 9], np.float32), np.array([30, 32, 36], np.float32), np.clip((Y - (cy - 1.9)) / 3.8, 0, 1)))
    lip = mouth * np.exp(-((Y - (cy + 1.6)) / 0.4) ** 2)
    L.over(lip * (1.0 if lit else 0.5), np.array([255, 150, 200], np.float32) if lit else LED_DIM)
    if lit:
        L.over(blur(np.exp(-((Y - (cy + 0.8)) / 1.2) ** 2) * (np.abs(X - CX) < 6), 1.0) * mouth * 0.35, np.array([255, 150, 200], np.float32))
    L.over(base * np.exp(-((Y - (b1 - 0.6)) / 0.4) ** 2) * 0.45, ST_L)
    # the foot: a bronze ring, a little wider, its top showing
    ft = drum(L, br, b1, DH - 0.8, BR + 0.8, BRONZE * 0.75, top=False)
    L.over(ft * np.exp(-((Y - (b1 + 0.5)) / 0.35) ** 2) * 0.5, np.array([206, 160, 102], np.float32))
    L.over(ft * np.exp(-((Y - (DH - 1.2)) / 0.4) ** 2) * 0.6, GRAPH_D)
    return L.image()


def drop(col):
    L = Layer(TW, TH)
    X, Y = L.X, L.Y
    # a soft glossy gel bean, flattened a little where it sits, with a
    # faint glow round it and a bright highlight
    cx, cy = TW / 2, 5.2
    e = np.sqrt(((X - cx) / 3.9) ** 2 + ((Y - cy) / 3.0) ** 2)
    L.over(blur(soft(e - 1.15, 0.2), 0.8) * 0.3, col)
    body = soft((e - 1) * 3, 0.4)
    sh = np.clip(1.05 - np.hypot(X - cx + 1.2, Y - cy + 1.3) / 4.4, 0, 1)
    L.over(body, mix(col * 0.5, np.minimum(col * 1.1 + 30, 255), sh))
    L.over(body * np.exp(-((e - 0.85) / 0.12) ** 2) * 0.3, col * 0.3)
    L.over(soft(np.hypot((X - (cx - 1.4)) / 1.3, Y - (cy - 1.4)) - 0.7, 0.3), np.array([255, 255, 255], np.float32))
    L.over(soft(np.hypot(X - (cx + 1.6), Y - (cy + 1.4)) - 0.35, 0.3) * 0.5, np.array([255, 255, 255], np.float32))
    return L.image()


if __name__ == '__main__':
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    disp = [dispenser(False), dispenser(True)]
    drops = [drop(DROPS[0]), drop(DROPS[1]), drop(DROPS[2])]
    write_png(os.path.join(out, 'zltd.png'), disp)
    write_png(os.path.join(out, 'zltt.png'), drops)
    sheet = Image.new('RGBA', (DW * 2 + TW * 3 + 60, DH + 10), (180, 110, 50, 255))
    x = 5
    for im in disp + drops:
        sheet.alpha_composite(im, (x, 5 + DH - im.height))
        x += im.width + 10
    sheet.resize((sheet.width * 5, sheet.height * 5), Image.NEAREST).save(os.path.join(out, 'labtreat-sheet.png'))
    print('wrote', out)
