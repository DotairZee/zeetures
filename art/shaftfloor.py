"""Draw an original landing for the lab lift's shaft (zlsf.png).

Owner, 2026-11-23: "there needs to be a floor graphic in this shaft where
the norn is walking". The right platform's floor (y 1739) runs on into the
copper shaft (room 255, x 4225-4349), where creatures wait for the pod; with
the pod away they stood in mid-air. This is a slim landing across the shaft
at that level, in the pod's own materials (lift.py: brushed steel, graphite,
a cool light line), drawn behind the pod and behind creatures, so the pod
passes in front of it.

One frame, 104 x 12: the landing's top face seen a little from above (rows
0-3, the floor line at row 3), its graphite front edge with a faint light
line (rows 3-7), small brackets into the shaft's side posts at each end, and
a soft shadow beneath. Placed at x 4241, y 1736 (shaftfloor.cos).

Drawn from scratch at 4x and downscaled (no game pixels are read or copied).

    python3 shaftfloor.py OUT_DIR
"""
import os
import sys

import numpy as np
from PIL import Image

from lift import (GRAPH, GRAPH_D, GRAPH_L, LED_DIM, ST, ST_D, ST_L, Layer, brushed_h, mix, soft, write_png)

W, H = 104, 12
TOP, LINE, EDGE = 0.6, 3.0, 7.0  # the top face from TOP to LINE, the front edge to EDGE
X0, X1 = 2.0, W - 2.0  # the landing's ends (into the side posts)


def landing():
    L = Layer(W, H)
    X, Y = L.X, L.Y
    across = soft(X0 - X, 0.6) * soft(X - X1, 0.6)
    # a soft shadow under it, on the shaft's back wall
    sh = np.exp(-((Y - EDGE - 1.2) / 1.6) ** 2) * soft(X0 + 3 - X, 3) * soft(X - X1 + 3, 3)
    L.over(sh * 0.45, np.array([20, 10, 6], np.float32))
    # the top face: brushed steel, lighter toward the back (lit from above,
    # the upper left), with fine slots of a grating
    top = across * soft(TOP - Y, 0.5) * soft(Y - LINE, 0.4)
    t = (LINE - Y) / (LINE - TOP)
    col = mix(ST, ST_L, 0.35 + 0.4 * t - 0.15 * (X - W / 2) / W) * brushed_h(W, H, 23)
    slots = (np.mod(X - X0, 4.0) < 0.9) & (Y > TOP + 0.6) & (Y < LINE - 0.4) & (X > X0 + 3) & (X < X1 - 3)
    col = np.where(slots[..., None], col * 0.55, col)
    L.over(top, col)
    # the front edge: graphite, a highlight along its top, a cool light line
    front = across * soft(LINE - Y, 0.4) * soft(Y - EDGE, 0.5)
    v = (Y - LINE) / (EDGE - LINE)
    L.over(front, mix(GRAPH_L, GRAPH_D, np.clip(v * 1.3, 0, 1)) * brushed_h(W, H, 41, 0.04))
    L.over(across * np.exp(-((Y - LINE - 0.4) / 0.35) ** 2) * 0.7, ST_L)
    glow = across * np.exp(-((Y - (LINE + EDGE) / 2 - 0.4) / 0.45) ** 2) * soft(X0 + 6 - X, 1) * soft(X - X1 + 6, 1)
    L.over(glow * 0.75, LED_DIM * 1.25)
    # brackets: short steel blocks where it meets the side posts
    for bx in (X0 + 1.5, X1 - 1.5):
        b = soft(np.abs(X - bx) - 2.2, 0.5) * soft(TOP + 0.4 - Y, 0.5) * soft(Y - EDGE - 2.5, 0.6)
        L.over(b, mix(ST_D, ST, np.clip(1 - (Y - TOP) / 9, 0, 1)) * brushed_h(W, H, 7, 0.03))
        L.over(b * np.exp(-((X - bx + 1.2) / 0.5) ** 2) * 0.5, ST_L)
    return L.image()


if __name__ == '__main__':
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    im = landing()
    write_png(os.path.join(out, 'zlsf.png'), [im])
    # preview at 4x over a copper stand-in for the shaft
    prev = Image.new('RGBA', (W, H + 8), (150, 74, 34, 255))
    prev.alpha_composite(im, (0, 4))
    prev.resize((W * 4, (H + 8) * 4), Image.NEAREST).save(os.path.join(out, 'zlsf-preview.png'))
    print('zlsf.png', im.size)
