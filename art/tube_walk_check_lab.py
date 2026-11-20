"""Proof for src/openc2e/madeart/tube_walk_lab.{h,cpp}, the C++ port of the
gene lab's new west end (tube_walk.py lab_west_end; tube_walk_lib.py
Canvas, light_pools, rust_and_grime, trim_wall, lab_wall, blur): the port,
run on the real recipe's inputs, against the recipe itself. Floats are
compared bit for bit; every array must show max difference 0.

    python3 art/tube_walk_check_lab.py DIR --inputs C2_DIR   (the real base and bg, from the recipe)
    OPENC2E_TW_CHECK_LAB=DIR build-fast/caos_to_bytecode       (any openc2e program)
    python3 art/tube_walk_check_lab.py DIR                     (compare)

--inputs runs make()'s steps up to lab_west_end (the scene, the old tube
painted out: slow, a minute or two) and writes base (what lab_west_end is
given, float32 840 x 2340 x 3) and bg (float32) to DIR. The comparison
calls the recipe's own functions (lib.lab_wall, tube_walk.lab_west_end) on
those inputs; the intermediates come from a step-by-step copy of the two
functions here, itself checked to give the recipe's own results to the bit.
"""
import os
import sys

import numpy as np
from PIL import Image, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tube_walk as tw          # noqa: E402
import tube_walk_lib as lib     # noqa: E402
from tube_walk_lib import Canvas, SS, light_pools, rust_and_grime, trim_wall  # noqa: E402,F401

OX, OY, W, H = tw.OX, tw.OY, tw.W, tw.H
DT = {'u8': np.uint8, 'i8': np.int64, 'f4': np.float32, 'f8': np.float64}
DTN = {np.dtype(np.uint8): 'u8', np.dtype(np.int64): 'i8', np.dtype(np.float32): 'f4', np.dtype(np.float64): 'f8'}


def write_inputs(d, c2):
    from background import region
    import s16
    os.makedirs(d, exist_ok=True)
    bg = np.asarray(region(c2, OX, OY, OX + W, OY + H)).astype(np.float32)[..., :3]
    bgt = np.asarray(region(c2, OX, OY - 300, OX + W, OY + H)).astype(np.float32)[..., :3]
    swmp_im = s16.read(os.path.join(c2, 'Images', 'swmp.s16'))
    swmp = [np.asarray(f).astype(np.float32) for f in swmp_im]
    tubecomp = bg.copy()
    tw.put_scenery(tubecomp, swmp, tw.SCENERY[:1])
    scene = tubecomp.copy()
    tw.put_scenery(scene, swmp, tw.SCENERY[1:])
    mask, strip = tw.old_tube(tubecomp, np.asarray(swmp_im[0])[..., 3] > 0)
    base = tw.paint_out(scene, bg, bgt, mask, strip)
    assert base.dtype == np.float32 and base.shape == (H, W, 3), (base.dtype, base.shape)
    np.ascontiguousarray(base).tofile(os.path.join(d, 'in_base.bin'))
    np.ascontiguousarray(bg).tofile(os.path.join(d, 'in_bg.bin'))
    print('inputs written to', d)


def read_inputs(d):
    base = np.fromfile(os.path.join(d, 'in_base.bin'), np.float32).reshape(H, W, 3)
    bg = np.fromfile(os.path.join(d, 'in_bg.bin'), np.float32).reshape(H, W, 3)
    return base, bg


# --- the recipe, step by step (copies of lab_wall and lab_west_end with taps) ---

def lab_wall_steps(tap):
    X0, X1, Y0 = lib.WALL_X0, lib.WALL_X1, lib.WALL_Y0
    c = Canvas(X0, Y0, X1 - X0, lib.WALL_Y1 - Y0, seed=8)
    trim_wall(c, X0, Y0, X1, lib.WALL_FLOOR, [78, 66, 40], [70, 46, 32], seed=5, light=((6085, 1575, 55, 0.3), (6070, 1730, 60, 0.2)))
    tap('w_trim_rgb', c.rgb); tap('w_trim_a', c.a); tap('w_trim_crease', c.crease)
    c.rgb *= (1 + c.noise(1.2, 0.10) + c.noise(3, 0.08))[..., None]
    c.rgb = c.rgb * np.asarray(np.clip(1 - 0.35 * np.clip(c.noise(12, 1.0) - 0.2, 0, 1), 0, 1))[..., None]
    tap('w_tex_rgb', c.rgb)
    c.ibeam_h(X0, X1, 1499, 1517, [64, 70, 66])
    tap('w_beam_rgb', c.rgb); tap('w_beam_a', c.a)
    rust_and_grime(c, (c.a > 0), rust=0.3, streaks=0.15, chips=0.12)
    tap('w_rust1_rgb', c.rgb)
    lime = np.array([112, 150, 34], np.float32)
    amt = np.clip(0.5 + 0.35 * c.noise(16, 1.0) + 0.15 * c.noise(5, 1.0), 0.1, 0.85)
    lum = c.rgb.mean(axis=2, keepdims=True)
    c.rgb = c.rgb * (1 - amt[..., None]) + (lime / lime.mean()) * lum * amt[..., None]
    tap('w_lime_rgb', c.rgb)
    cr = c.crease
    rust = np.array([150, 74, 34], np.float32)
    cl = c.rgb[cr].mean(axis=1, keepdims=True)
    c.rgb[cr] = c.rgb[cr] * 0.35 + (rust / rust.mean()) * cl * 0.65 * (1 + 0.2 * c.noise(3, 1.0)[cr])[:, None]
    tap('w_crease_rgb', c.rgb)
    c.grime(0.3)
    tap('w_grime_rgb', c.rgb)
    light_pools(c, [(6060, 1730, 40, 0.2, (255, 150, 80)), (6110, 1760, 40, 0.2, (255, 90, 40)), (6080, 1560, 40, 0.15, (255, 140, 80))], ambient=0.55)
    tap('w_pools_rgb', c.rgb)
    sh = (np.abs(c.Y - 1670) < 6) & (c.X < X1 + 1)
    c.rgb[sh] *= (0.45 + 0.55 * np.clip(np.abs(c.Y[sh] - 1670) / 6, 0, 1))[:, None]
    c.cyl_h(X0, X1 + 1, 1646, 19, [110, 62, 40], spec=0.3, tarnish=0.1)
    tap('w_pipe_rgb', c.rgb)
    c.flange_h(6063, 1646, 21, 5, [70, 38, 24], bolts=3)
    c.flange_h(6073, 1646, 21, 5, [70, 38, 24], bolts=3)
    tap('w_flange_rgb', c.rgb)
    pm = (np.abs(c.Y - 1646) <= 22) & (c.X < X1 + 1)
    rust_and_grime(c, pm, rust=0.12, streaks=0.0, chips=0.08)
    tap('w_rust2_rgb', c.rgb); tap('w_a', c.a)
    return c.result()


def lab_west_end_steps(base, bg, tap):
    lw, la = lib.lab_wall()
    tap('lab_wall_rgb', lw); tap('lab_wall_a', la)
    WX = lib.WALL_X0
    lab = bg[1478 - OY:1814 - OY, 6127 - OX:6157 - OX]
    rows_lab = lab.mean(axis=1)
    rows_new = lw[:, -30:].mean(axis=1)
    ratio = np.clip((rows_lab + 8) / (rows_new + 8), 0.6, 1.8)
    ratio[1626 - 1478:1673 - 1478] = 1.0
    ratio[1660 - 1478:] = ratio[1527 - 1478:1627 - 1478].mean(axis=0)
    ratio = np.asarray(np.clip(ratio * 100, 0, 255).astype(np.uint8)).astype(np.float32) / 100
    tap('e_ratio', ratio)
    wcols = lw.shape[1]
    for i in range(wcols):
        g = np.clip((i - (wcols - 60)) / 60, 0, 1)
        lw[:, i] = lw[:, i] * (1 + (ratio - 1) * g)
    tap('e_join_lw', lw)
    room = bg[1520 - OY:1640 - OY, 6113 - OX:6150 - OX].mean(axis=2).ravel()
    pre = lw.copy()
    lum = lw.mean(axis=2)
    src = np.sort(lum[la > 0.5])
    dst = np.sort(room)
    q = np.searchsorted(src, lum, side='left') / max(1, len(src) - 1)
    new = np.interp(np.clip(q, 0, 1), np.linspace(0, 1, len(dst)), dst)
    tap('e_hist_new', new)
    lw = lw * ((new + 1) / (lum + 1))[..., None]
    g = lw.mean(axis=2, keepdims=True)
    lw = np.clip(g + (lw - g) * 0.55, 0, 255)
    lw *= 0.8
    lw[1624 - 1478:1672 - 1478] = pre[1624 - 1478:1672 - 1478] * 0.92
    tap('e_hist_lw', lw)
    for y in range(1519, 1624):
        r = y - 1478
        tg = bg[y - OY, 6127 - OX:6150 - OX].mean(axis=0)
        ow = lw[r, 6080 - WX:6100 - WX].mean(axis=0)
        rt = np.clip((tg + 6) / (ow + 6), 0.6, 2.2)
        for x in range(6045, WX + lw.shape[1]):
            u = np.clip((x - 6060) / 76.0, 0, 1)
            lw[r, x - WX] *= 1 + (rt - 1) * u
    tap('e_upper_lw', lw)
    xs = np.arange(WX, WX + lw.shape[1])
    t = np.clip((xs - 6099) / 28.0, 0, 1)
    fade = np.repeat((1 - t * t * (3 - 2 * t))[None, :], lw.shape[0], axis=0)
    t2 = np.clip((xs - 6092) / 50.0, 0, 1)
    fade[1519 - 1478:1624 - 1478] = (1 - t2 * t2 * (3 - 2 * t2))[None, :]
    for y in range(1660, 1814):
        e = 6170 if y < 1703 else 6146
        for x in range(6105, 6170):
            if bg[y - OY, x - OX, 1] > 20:
                e = x
                break
        fade[y - 1478] = np.clip((e + 1 - xs) / 2.0, 0, 1)
    tap('e_fade', fade)
    la = la * fade
    bgs = bg[1478 - OY:1814 - OY, WX - OX:WX + lw.shape[1] - OX]
    blue = (bgs[..., 2] > bgs[..., 0] + 20) & (bgs[..., 2] > 60)
    blue[:, :6100 - WX] = False
    blue = np.asarray(Image.fromarray((blue * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(3))) > 0
    tap('e_blue', blue)
    la = la * (~blue)
    tap('e_la', la)
    reg = base[1478 - OY:1814 - OY, WX - OX:WX + lw.shape[1] - OX]
    reg[:] = reg * (1 - la[..., None]) + lw * la[..., None]
    tap('e_wall_base', base[227:564, 1590:1772])
    rr = bg[1664 - OY:1700 - OY, 6110 - OX:6165 - OX].reshape(-1, 3)
    rr = rr[(rr[:, 0] > 2 * rr[:, 1] + 8) & (rr[:, 2] < 30)]
    rust = rr.mean(axis=0)
    tap('e_rust', rust)
    ys0, ys1 = 1519, 1800
    blk = base[ys0 - OY:ys1 - OY, 6005 - OX:WX + lw.shape[1] - OX]
    al = la[ys0 - 1478:ys1 - 1478, 6005 - WX:].copy()
    al[1624 - ys0:1668 - ys0] = 0
    lm = blk.mean(axis=2, keepdims=True)
    rec = rust * np.clip(0.55 + 0.45 * lm / (lm[al > 0.5].mean() + 1), 0.4, 1.6)
    u = np.clip((np.arange(6005, WX + lw.shape[1]) - 6012) / 116.0, 0, 1)
    u = u[None, :, None] * al[..., None]
    blk[:] = blk * (1 - u) + rec * u
    tap('e_rustramp_base', base[227:564, 1590:1772])
    src = bg[1626 - OY:1668 - OY, 6112 - OX:6122 - OX]
    rng = np.random.default_rng(12)
    walk = 0.0
    rings = base[1620 - OY:1674 - OY, 6058 - OX:6079 - OX].copy()
    for x in range(5990, 6122):
        walk = 0.85 * walk + rng.normal(0, 0.6)
        k = int(abs(x * 3 + int(walk * 4))) % src.shape[1]
        base[1626 - OY:1668 - OY, x - OX] = src[:, k] * (1 + 0.06 * np.tanh(walk))
    tap('e_pipecols_base', base[227:564, 1590:1772])
    seg = base[1626 - OY:1668 - OY, 5990 - OX:6122 - OX]
    pad = np.concatenate([seg[:, :1].repeat(8, 1), seg, seg[:, -1:].repeat(8, 1)], axis=1)
    kern = lib.ex(-0.5 * (np.arange(-8, 9) / 1.6) ** 2); kern /= kern.sum()
    sm = np.zeros_like(seg)
    for i, kv in enumerate(kern):
        sm += kv * pad[:, i:i + seg.shape[1]]
    seg[:] = sm
    tap('e_pipesmooth_base', base[227:564, 1590:1772])
    PW = 6122 - 5990
    gr = rng.normal(0, 1, (1668 - 1626, PW)).astype(np.float32)
    gr = np.asarray(Image.fromarray(np.clip(gr * 40 + 128, 0, 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(0.6))).astype(np.float32) / 128 - 1
    mo = rng.random((6, max(4, PW // 8))).astype(np.float32)
    mo = np.asarray(Image.fromarray((mo * 255).astype(np.uint8)).resize((PW, 1668 - 1626), Image.BICUBIC)).astype(np.float32) / 255 - 0.5
    sp = rng.random((1668 - 1626, PW)) > 0.985
    tap('e_gr', gr); tap('e_mo', mo); tap('e_sp', sp)
    pr = base[1626 - OY:1668 - OY, 5990 - OX:6122 - OX]
    pr *= (1 + 0.28 * gr + 0.35 * mo)[..., None]
    pr[:] = np.where(sp[..., None], pr * 0.55, pr)
    tap('e_pipegrain_base', base[227:564, 1590:1772])
    for rc in (6063, 6073):
        a, b = rc - 2 - 6058, rc + 3 - 6058
        ring = rings[1627 - 1620:1660 - 1620, a:b]
        yb = 1666 if rc == 6073 else 1660
        if yb == 1666:
            ring = np.concatenate([ring, ring[-7:-1] * np.linspace(0.95, 0.7, 6)[:, None, None]], axis=0)
        d = base[1627 - OY:yb - OY, rc - 2 - OX:rc + 3 - OX]
        d[:] = np.clip(ring * 0.6 + np.array([22, 9, 3], np.float32), 0, 255) * (1 + 0.12 * rng.normal(0, 1, ring.shape[:2]))[..., None]
        d[:, 0] = d[:, 0] * 1.35 + 10
        base[1628 - OY:yb - OY, rc - 3 - OX] *= 0.5
        base[1628 - OY:yb - OY, rc + 3 - OX] *= 0.55
    tap('e_rings_base', base[227:564, 1590:1772])
    for x in range(6101, 6122):
        w = (x - 6101) / 20.0
        w = w * w * (3 - 2 * w)
        base[1626 - OY:1668 - OY, x - OX] = base[1626 - OY:1668 - OY, x - OX] * (1 - w) + bg[1626 - OY:1668 - OY, x - OX] * w
    tap('e_pipe_base', base[227:564, 1590:1772])
    bm = base[1477 - OY:1521 - OY, 5990 - OX:6128 - OX]
    bmpre = bm.copy()
    med = np.stack([np.asarray(Image.fromarray(bm[..., k].clip(0, 255).astype(np.uint8)).filter(ImageFilter.MedianFilter(7))).astype(np.float32) for k in range(3)], -1)
    hot = (bm.mean(-1) - med.mean(-1)) > 6
    hot = np.asarray(Image.fromarray((hot * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(3))) > 0
    tap('e_hot', hot)
    bm[hot] = med[hot]
    tgt = bg[1477 - OY:1521 - OY, 6130 - OX:6185 - OX].mean(axis=1)
    cur = bm[:, 6030 - 5990:6090 - 5990].mean(axis=1)
    bm *= np.clip((tgt + 4) / (cur + 4), 0.4, 1.6)[:, None, :]
    bm *= (1 + 0.08 * np.random.default_rng(21).normal(0, 1, bm.shape[:2]))[..., None]
    tap('e_beamtone_base', base[227:564, 1590:1772])
    bm[1517 - 1477:] = bmpre[1517 - 1477:]
    src = bg[1497 - OY:1517 - OY, 6122 - OX:6196 - OX]
    rng2 = np.random.default_rng(31)
    rng2.normal(0, 0.8, 6128 - 5990)
    prof = np.median(src, axis=1)
    tap('e_prof', prof)
    band = np.repeat(prof[:, None, :], 6128 - 5990, axis=1)
    slow = np.asarray(Image.fromarray(np.clip(rng2.normal(128, 30, (2, 12)), 0, 255).astype(np.uint8)).resize((6128 - 5990, 20), Image.BICUBIC)).astype(np.float32) / 128 - 1
    band *= (1 + 0.10 * rng2.normal(0, 1, band.shape[:2]) + 0.12 * slow)[..., None]
    bm[1497 - 1477:1517 - 1477] = band
    for x in range(6100, 6128):
        w = np.clip((x - 6116) / 12.0, 0, 1)
        w = w * w * (3 - 2 * w)
        wu = (x - 6100) / 28.0; wu = wu * wu * (3 - 2 * wu)
        bm[:1497 - 1477, x - 5990] = bm[:1497 - 1477, x - 5990] * (1 - wu) + bg[1477 - OY:1497 - OY, x - OX] * wu
        bm[1497 - 1477:1517 - 1477, x - 5990] = bm[1497 - 1477:1517 - 1477, x - 5990] * (1 - w) + bg[1497 - OY:1517 - OY, x - OX] * w
    tap('e_beam_base', base[227:564, 1590:1772])
    slab = bg[1784 - OY:1797 - OY, 6160 - OX:6280 - OX]
    for x in range(5990, 6136):
        w = np.clip((6136 - x) / 10.0, 0, 1)
        base[1784 - OY:1797 - OY, x - OX] = base[1784 - OY:1797 - OY, x - OX] * (1 - w) + slab[:, (x - 5990) % slab.shape[1]] * w


def blur_cases(bg):
    """lib.blur on a few real arrays (float32 RGB and plane, float64)."""
    E = {}
    a = bg[400:520, 1500:1700]
    E['blur_f32_rgb_5'] = lib.blur(a, 5)
    E['blur_f32_rgb_2.5'] = lib.blur(a * 1.3 - 20, 2.5)
    E['blur_f32_L_6'] = lib.blur(a.mean(-1), 6)
    E['blur_f64_rgb_9'] = lib.blur(a.astype(np.float64) * 1.1, 9)
    return E


def expected(d):
    base, bg = read_inputs(d)
    E = {}

    def tap(name, a):
        E[name] = np.array(a)
    # the canvas's own streams and shapes
    c = Canvas(lib.WALL_X0, lib.WALL_Y0, lib.WALL_X1 - lib.WALL_X0, lib.WALL_Y1 - lib.WALL_Y0, seed=8)
    E['cv_X'] = c.X; E['cv_Y'] = c.Y
    E['cv_noise_9'] = c.noise(9, 0.10); E['cv_noise_1.2'] = c.noise(1.2, 0.1); E['cv_noise_1.5'] = c.noise(1.5, 1.0)
    # lab_wall, real and step by step
    rgb, a = lib.lab_wall()
    S = {}
    rgb2, a2 = lab_wall_steps(lambda n, v: S.__setitem__(n, np.array(v)))
    assert rgb.dtype == rgb2.dtype and (rgb.view(np.uint64) == rgb2.view(np.uint64)).all(), 'lab_wall copy differs from the recipe'
    assert (a.view(np.uint32) == a2.view(np.uint32)).all()
    E.update(S)
    E['wall_rgb'] = rgb; E['wall_a'] = a
    # lab_west_end, real and step by step
    real = base.copy()
    tw.lab_west_end(real, bg)
    steps = base.copy()
    lab_west_end_steps(steps, bg, tap)
    assert (real.view(np.uint32) == steps.view(np.uint32)).all(), 'lab_west_end copy differs from the recipe'
    E['out_base'] = real
    E.update(blur_cases(bg))
    return E


def compare(exp, got):
    exp = np.ascontiguousarray(exp).ravel()
    if exp.dtype == bool:
        exp = exp.astype(np.uint8)
    if got.dtype != exp.dtype or got.size != exp.size:
        return exp.size, float('inf')
    if exp.dtype.kind == 'f':
        u = np.uint32 if exp.dtype == np.float32 else np.uint64
        both_nan = np.isnan(exp) & np.isnan(got)
        bad = (exp.view(u) != got.view(u)) & ~both_nan
        diff = np.abs(exp.astype(np.float64) - got.astype(np.float64))
        diff[both_nan] = 0
        diff = np.where(bad & ~np.isfinite(diff), np.inf, diff)
        return int(bad.sum()), float(diff[bad].max()) if bad.any() else 0.0
    dd = np.abs(exp.astype(np.int64) - got.astype(np.int64))
    return int((dd != 0).sum()), int(dd.max()) if len(dd) else 0


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(2)
    d = sys.argv[1]
    if '--inputs' in sys.argv:
        write_inputs(d, sys.argv[sys.argv.index('--inputs') + 1])
        return
    got, order = {}, []
    for line in open(os.path.join(d, 'index.txt')):
        name, dt, n = line.split()
        got[name] = np.fromfile(os.path.join(d, name + '.bin'), dtype=DT[dt])
        assert len(got[name]) == int(n), name
        order.append(name)
    E = expected(d)
    worst = 0
    print('%-22s %-6s %-16s %9s %8s %9s' % ('array', 'dtype', 'shape', 'values', 'differ', 'max diff'))
    for name in order + sorted(set(E) - set(got)):
        if name not in E:
            print('%-22s (not in the recipe)' % name)
            worst = float('inf')
            continue
        e = np.asarray(E[name])
        if name not in got:
            print('%-22s (missing from the port)' % name)
            worst = float('inf')
            continue
        nb, md = compare(e, got[name])
        print('%-22s %-6s %-16s %9d %8d %9s' % (name, DTN.get(e.dtype, 'bool'), 'x'.join(map(str, e.shape)), e.size, nb, md))
        worst = max(worst, md if md == md else float('inf'))
    ok = worst == 0
    print('\nALL EXACT' if ok else '\nDIFFERENCES FOUND')
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
