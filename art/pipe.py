"""Draw an original iron water pipe running over the ground from the old
standpipe by the waterwheel (x 3036-3046, its cap at y 2260) to the back of
the water pump's new place (x 3290; see walkways.cos), in the dark olive iron
of that standpipe: an elbow into the standpipe's top, a run along the
ground with flanged joints and small ground clamps, and its end hidden
behind the pump's body.

Drawn from scratch at 4x and box-downscaled (no game pixels are read or
copied), so the output can ship with the project.

    python3 pipe.py OUT_BASENAME

writes OUT_BASENAME.png/.s16 (the run, behind things) and
OUT_BASENAME-inlet.png/.s16 (the fitting, in front of the pump) and prints
where to place them.
"""
import sys
import numpy as np
from PIL import Image
import s16
from catwalk import _noise

SS = 4
X0, Y0 = 3030, 2244          # the picture's top left in the world
W, H = 300, 24               # to x 3330, y 2268
STUB_X = 3041                # the standpipe's middle
RUN_Y = 2254                 # the pipe's centre line along the ground
R = 3.6                      # its radius
END_X = 3298                 # into the inlet fitting on the pump's side
DARK = np.array([26, 26, 18], np.float32)
BODY = np.array([74, 72, 58], np.float32)
LIT = np.array([150, 146, 128], np.float32)
MATTE = np.array([60, 50, 40], np.float32)


def draw():
    w, h = W * SS, H * SS
    xs = (np.arange(w, dtype=np.float32) + 0.5) / SS + X0
    ys = (np.arange(h, dtype=np.float32) + 0.5) / SS + Y0
    X, Y = np.meshgrid(xs, ys)
    rng = np.random.default_rng(21)
    tex = np.kron(0.08 * _noise((H, W), 81, 3) + rng.normal(0, 0.03, (H, W)), np.ones((SS, SS))).astype(np.float32)
    rgb = np.zeros((h, w, 3), np.float32)
    cover = np.zeros((h, w), np.float32)

    def shade(t):
        # across a pipe: t from -1 (top or near side) to 1, lit from above
        k = np.clip(1 - np.abs(t + 0.35) * 1.1, 0, 1)
        col = DARK + (BODY - DARK) * np.clip((1 - np.abs(t)) * 1.6, 0, 1)[..., None]
        return col + (LIT - BODY) * (k ** 3)[..., None]

    # the run along the ground, from the elbow to under the pump
    ex = STUB_X + R                       # where the elbow's bend ends
    run = (X >= ex) & (X <= END_X) & (np.abs(Y - RUN_Y) <= R)
    col = shade((Y - RUN_Y) / R)
    rgb[run] = col[run]
    cover[run] = 1
    # the elbow: a quarter torus turning down into the standpipe's top
    cx, cy = ex, RUN_Y + R                # the bend's centre
    d = np.hypot(X - cx, Y - cy)
    ang_ok = (X <= cx) & (Y <= cy)
    elbow = ang_ok & (np.abs(d - R) <= R)
    col = shade((d - R) / R * -1)
    rgb[elbow] = col[elbow]
    cover[elbow] = 1
    # the short drop into the standpipe's cap, with a collar
    drop = (np.abs(X - STUB_X) <= R) & (Y >= cy) & (Y <= 2262)
    col = shade((X - STUB_X) / R)
    rgb[drop] = col[drop]
    cover[drop] = 1
    collar = (np.abs(X - STUB_X) <= R + 1.2) & (np.abs(Y - 2259.5) <= 1.3)
    rgb[collar] = (shade((X - STUB_X) / (R + 1.2)) * 1.1)[collar]
    cover[collar] = 1
    # flanged joints along the run, and small clamps pinning it to the ground
    for fx in np.arange(ex + 52, END_X - 20, 66):
        f = (np.abs(X - fx) <= 1.4) & (np.abs(Y - RUN_Y) <= R + 1.3)
        rgb[f] = (shade((Y - RUN_Y) / (R + 1.3)) * 1.15)[f]
        cover[f] = 1
        # bolt heads on the flange's face
        for by in (RUN_Y - R, RUN_Y + R):
            b = np.hypot(X - fx, Y - by) <= 0.75
            rgb[b] = LIT * 0.9
    for kx in np.arange(ex + 85, END_X - 30, 66):
        k = (np.abs(X - kx) <= 1.0) & (Y >= RUN_Y + R - 0.5) & (Y <= RUN_Y + R + 3.5)
        rgb[k] = DARK * 1.3
        cover[k] = 1
        band = (np.abs(X - kx) <= 1.6) & (np.abs(Y - RUN_Y) <= R + 0.4)
        rgb[band] = (shade((Y - RUN_Y) / (R + 0.4)) * 0.75)[band]
    rgb *= (1 + tex)[..., None]
    # rust and grime toward the bottom of the pipe
    grime = np.clip((Y - RUN_Y) / R, 0, 1) * np.clip(np.kron(_noise((H, W), 82, 4), np.ones((SS, SS))) * 2, 0, 1) * 0.4
    rgb = rgb * (1 - grime[..., None]) + np.array([96, 58, 30], np.float32) * grime[..., None]

    big = np.dstack([rgb, cover * 255]).clip(0, 255).astype(np.uint8)
    im = Image.fromarray(big, 'RGBA').resize((W, H), Image.BOX)
    a = np.asarray(im).astype(np.float32)
    al = a[..., 3:4] / 255.0
    a[..., :3] = a[..., :3] * al + MATTE * (1 - al)
    a[..., 3] = np.where(al[..., 0] < 0.35, 0, 255)
    return Image.fromarray(a.clip(0, 255).astype(np.uint8), 'RGBA')


IX0, IY0, IW, IH = 3290, 2242, 32, 22       # the inlet fitting
SLEEVE_Y, SLEEVE_R = 2253.0, 6.6            # over the pump's hose (y 2240-2259; owner: smaller)
SLEEVE_X0, SLEEVE_X1 = 3298, 3320           # (owner: 2 px further left)


def inlet():
    w, h = IW * SS, IH * SS
    xs = (np.arange(w, dtype=np.float32) + 0.5) / SS + IX0
    ys = (np.arange(h, dtype=np.float32) + 0.5) / SS + IY0
    X, Y = np.meshgrid(xs, ys)
    rng = np.random.default_rng(22)
    tex = np.kron(0.08 * _noise((IH, IW), 83, 3) + rng.normal(0, 0.03, (IH, IW)), np.ones((SS, SS))).astype(np.float32)

    def shade(t):
        k = np.clip(1 - np.abs(t + 0.35) * 1.1, 0, 1)
        col = DARK + (BODY - DARK) * np.clip((1 - np.abs(t)) * 1.6, 0, 1)[..., None]
        return col + (LIT - BODY) * (k ** 3)[..., None]
    # the reducer: the pipe's radius and centre line widening to the sleeve's
    u = np.clip((X - IX0) / (SLEEVE_X0 - IX0), 0, 1)
    u = u * u * (3 - 2 * u)
    cy = RUN_Y + (SLEEVE_Y - RUN_Y) * u
    r = R + (SLEEVE_R - R) * u
    body = (X >= IX0) & (X <= SLEEVE_X1) & (np.abs(Y - cy) <= r)
    col = shade((Y - cy) / r)
    # a flange where the sleeve meets the pump, and one at the reducer
    for fx, fr in ((SLEEVE_X1 - 1.5, SLEEVE_R + 1.0), (SLEEVE_X0 + 0.5, SLEEVE_R + 0.6)):
        f = (np.abs(X - fx) <= 1.5) & (np.abs(Y - SLEEVE_Y) <= fr)
        col = np.where(f[..., None], shade((Y - SLEEVE_Y) / fr) * 1.15, col)
        body = body | f
        for by in (SLEEVE_Y - fr + 1.2, SLEEVE_Y + fr - 1.2):
            b = np.hypot(X - fx, Y - by) <= 0.8
            col = np.where(b[..., None], LIT * 0.95, col)
    # a band round the sleeve's middle
    band = (np.abs(X - (SLEEVE_X0 + SLEEVE_X1) / 2) <= 1.0) & (np.abs(Y - SLEEVE_Y) <= SLEEVE_R)
    col = np.where(band[..., None], col * 0.75, col)
    col = col * (1 + tex)[..., None]
    rgb = np.zeros((h, w, 3), np.float32)
    rgb[body] = col[body]
    cover = body.astype(np.float32)
    big = np.dstack([rgb, cover * 255]).clip(0, 255).astype(np.uint8)
    im = Image.fromarray(big, 'RGBA').resize((IW, IH), Image.BOX)
    a = np.asarray(im).astype(np.float32)
    al = a[..., 3:4] / 255.0
    a[..., :3] = a[..., :3] * al + MATTE * (1 - al)
    a[..., 3] = np.where(al[..., 0] < 0.35, 0, 255)
    return Image.fromarray(a.clip(0, 255).astype(np.uint8), 'RGBA')


if __name__ == '__main__':
    base = sys.argv[1]
    im = draw()
    im.save(base + '.png')
    s16.write(base + '.s16', [im])
    print(base, im.size, 'place at', X0, Y0)
    fit = inlet()
    fit.save(base + '-inlet.png')
    s16.write(base + '-inlet.s16', [fit])
    print(base + '-inlet', fit.size, 'place at', IX0, IY0)
