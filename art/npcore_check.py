"""Proof for src/openc2e/madeart/npcore.{h,cpp}, the shared numeric core of
the C++ made-art ports: every primitive there, run on fixed inputs, against
numpy and Pillow doing the same here. Floats are compared bit for bit
(NaN only with NaN); every case must show max difference 0.

    OPENC2E_NPCORE_CHECK=DIR build-fast/caos_to_bytecode   (any openc2e program)
    python3 art/npcore_check.py DIR

Written against NumPy 1.26.4 and Pillow 10.2.0 on the reference machine
(x86-64 with AVX512_SKX, glibc, Python 3.12). Numpy's float transcendentals
(exp, log, sin, cos, power) are its own SIMD code or Intel SVML here, not
glibc, so a port must not assume std::exp equals np.exp; npcore's header
says which functions it matches. Also checked: tube_walk_lib.py's
deterministic pw / ex against npcore's np::pw / np::ex. --transcendentals prints the measured
share of inputs where numpy and glibc disagree.
"""
import math
import os
import sys
import warnings

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

M32 = 0xFFFFFFFF


# --- the fixed inputs, as npcore.cpp's selfCheck makes them ---------------

def hash32(i, seed):
    i = np.asarray(i, dtype=np.uint64) & np.uint64(M32)
    h = (i * np.uint64(2654435761) + np.uint64((seed * 40503) & M32) + np.uint64(12345)) & np.uint64(M32)
    h ^= h >> np.uint64(15)
    h = (h * np.uint64(2246822519)) & np.uint64(M32)
    h ^= h >> np.uint64(13)
    h = (h * np.uint64(3266489917)) & np.uint64(M32)
    h ^= h >> np.uint64(16)
    return h


def test_img(h, w, c, seed, kind):
    y, x, k = np.meshgrid(np.arange(h), np.arange(w), np.arange(c), indexing='ij')
    i = (y * w + x) * c + k
    hv = hash32(i, seed).astype(np.int64)
    if kind == 0:
        v = hv & 255
    elif kind == 1:
        v = ((x * 256) // w + (y * 128) // h + k * 60 + (hv & 15)) & 255
    else:
        dx, dy, r = x - w // 3, y - h // 2, min(w, h) // 3
        v = np.where((dx * dx + dy * dy < r * r) | (hv % 11 == 0), 255, 0)
    if c == 4:
        a = np.where(x < w // 5, 0, np.where(x > 4 * w // 5, 255, (x * 5 + y * 3) & 255))
        v = np.where(k == 3, a, v)
    v = v.astype(np.uint8)
    return v[..., 0] if c == 1 else v


def test_f32(n, seed, scale, offset):
    hv = hash32(np.arange(n), seed)
    return ((hv & np.uint64(0xFFFFFF)).astype(np.float64) / 16777216.0 * scale + offset).astype(np.float32)


def test_f64(n, seed, scale, offset):
    return hash32(np.arange(n), seed).astype(np.float64) / 4294967296.0 * scale + offset


def pil(a):
    return Image.fromarray(a)


def arr(im):
    return np.asarray(im)


# --- the expected results ---------------------------------------------------

def expected():
    E = {}
    BI, BL = Image.BICUBIC, Image.BILINEAR

    # random streams
    g = np.random.default_rng(8)
    s = []
    s += list(g.random(37))
    s += [g.integers(0, 3) for _ in range(5)]
    s += list(g.normal(0, 1, 50))
    s.append(g.uniform(0.3, 0.7))
    s.append(g.random())
    s += [g.integers(-6, 7) for _ in range(4)]
    s += list(g.uniform(-5, 5, 20))
    s += [g.normal(0, 0.6) for _ in range(10)]
    s += list(g.random((7, 9)).ravel())
    s += [g.integers(0, 1000) for _ in range(3)]
    s += [g.integers(0, 1 << 40) for _ in range(2)]
    s += list(g.normal(128, 30, (2, 12)).ravel())
    s += list(g.integers(0, 3, 6))
    s += list(g.random(4))
    s += [g.standard_normal() for _ in range(3)]
    s.append(g.integers(0, 7))
    s.append(g.uniform(-1, 1))
    E['rng_mixed_s8'] = ('random', np.array(s, np.float64))
    E['rng_normal_s41'] = ('random', np.random.default_rng(41).normal(0, 1, 300000))
    s = []
    for seed in (0, 1, 3, 5, 7, 11, 12, 21, 31, 41, 52, 61, 62, 4294967301, 123456789012345):
        g = np.random.default_rng(seed)
        s += list(g.random(3))
        s += [g.integers(0, 100), g.integers(0, 100)]
        s += list(g.normal(0, 1, 2))
    E['rng_seeds'] = ('random', np.array(s, np.float64))
    g = np.random.default_rng(99)
    s = [g.integers(0, 1 << 32) for _ in range(3)]
    s += [g.integers(-(1 << 62), 1 << 62) for _ in range(3)]
    s += [g.integers(5, 6), g.integers(0, 3)]
    s.append(np.int64(np.uint64(g.bit_generator.random_raw()).astype(np.int64)))
    s += [g.integers(0, 37) for _ in range(1000)]
    s.append(np.uint64(g.bit_generator.random_raw()).astype(np.int64))
    E['rng_ints_s99'] = ('random', np.array(s, np.int64))

    # resize
    R = {'bicubic': BI, 'bilinear': BL, 'lanczos': Image.LANCZOS, 'hamming': Image.HAMMING, 'box': Image.BOX}
    for name, h, w, c, kind, W, H, r in (
            ('rs_L_noise_bicubic', 5, 7, 1, 0, 64, 40, 'bicubic'),
            ('rs_L_canvas_bicubic', 153, 282, 1, 0, 364, 672, 'bicubic'),
            ('rs_L_streaks_bicubic', 5, 14, 1, 0, 100, 80, 'bicubic'),
            ('rs_RGB_tp_bicubic', 24, 24, 3, 1, 91, 75, 'bicubic'),
            ('rs_L_vonly_bilinear', 90, 26, 1, 1, 100, 90, 'bilinear'),
            ('rs_L_down_bilinear', 90, 100, 1, 1, 100, 9, 'bilinear'),
            ('rs_L_up_bicubic_from9', 9, 100, 1, 1, 100, 90, 'bicubic'),
            ('rs_L_down_bicubic', 30, 40, 1, 0, 17, 11, 'bicubic'),
            ('rs_RGB_bilinear', 21, 33, 3, 1, 70, 50, 'bilinear'),
            ('rs_RGBA_bicubic', 16, 20, 4, 1, 45, 37, 'bicubic'),
            ('rs_RGBA_lanczos', 16, 20, 4, 1, 45, 37, 'lanczos'),
            ('rs_L_slow_bicubic', 2, 12, 1, 0, 138, 20, 'bicubic'),
            ('rs_L_mo_bicubic', 6, 16, 1, 0, 132, 42, 'bicubic'),
            ('rs_L_lanczos_down', 50, 61, 1, 0, 23, 19, 'lanczos'),
            ('rs_L_hamming', 20, 30, 1, 1, 47, 13, 'hamming'),
            ('rs_L_box', 20, 30, 1, 1, 47, 13, 'box')):
        E[name] = ('resize ' + r, arr(pil(test_img(h, w, c, 7, kind)).resize((W, H), R[r])))

    # GaussianBlur, BoxBlur
    for rs in ('0.6', '1.2', '2.5', '4', '5', '6', '7.5', '8', '9'):
        rad = float(rs)
        E['gb_L_' + rs] = ('GaussianBlur', arr(pil(test_img(61, 97, 1, 11, 1)).filter(ImageFilter.GaussianBlur(rad))))
        E['gb_RGB_' + rs] = ('GaussianBlur', arr(pil(test_img(61, 97, 3, 12, 0)).filter(ImageFilter.GaussianBlur(rad))))
    E['gb_L_tiny_8'] = ('GaussianBlur', arr(pil(test_img(5, 7, 1, 13, 0)).filter(ImageFilter.GaussianBlur(8))))
    E['gb_L_mask_8'] = ('GaussianBlur', arr(pil(test_img(90, 120, 1, 14, 2)).filter(ImageFilter.GaussianBlur(8))))
    E['gb_RGBA_2.5'] = ('GaussianBlur', arr(pil(test_img(30, 40, 4, 15, 1)).filter(ImageFilter.GaussianBlur(2.5))))
    E['bb_L_3'] = ('BoxBlur', arr(pil(test_img(30, 40, 1, 16, 0)).filter(ImageFilter.BoxBlur(3))))

    # rank filters
    mask, noise = pil(test_img(60, 80, 1, 21, 2)), pil(test_img(40, 50, 1, 22, 0))
    E['rk_min3_mask'] = ('MinFilter', arr(mask.filter(ImageFilter.MinFilter(3))))
    E['rk_min5_mask'] = ('MinFilter', arr(mask.filter(ImageFilter.MinFilter(5))))
    E['rk_max3_mask'] = ('MaxFilter', arr(mask.filter(ImageFilter.MaxFilter(3))))
    E['rk_max5_mask'] = ('MaxFilter', arr(mask.filter(ImageFilter.MaxFilter(5))))
    E['rk_max9_mask'] = ('MaxFilter', arr(mask.filter(ImageFilter.MaxFilter(9))))
    E['rk_min5_noise'] = ('MinFilter', arr(noise.filter(ImageFilter.MinFilter(5))))
    E['rk_max9_noise'] = ('MaxFilter', arr(noise.filter(ImageFilter.MaxFilter(9))))
    E['rk_median7_noise'] = ('MedianFilter', arr(noise.filter(ImageFilter.MedianFilter(7))))
    E['rk_median7_mask'] = ('MedianFilter', arr(mask.filter(ImageFilter.MedianFilter(7))))
    E['rk_median7_RGB'] = ('MedianFilter', arr(pil(test_img(30, 33, 3, 23, 1)).filter(ImageFilter.MedianFilter(7))))
    cur = mask
    for _ in range(6):
        cur = cur.filter(ImageFilter.MinFilter(5))
    E['rk_min5x6_mask'] = ('MinFilter', arr(cur))

    # rotate (tube_walk.py's angle)
    def top_of(x):
        return 1438 + (x - 4835) * (1496 - 1438) / (5935 - 4835)
    start_top = top_of(4780)
    slope = (1798 - 134 - start_top) / ((6113 - 110) + 20 - 4780)
    ang = math.degrees(math.atan(slope))
    E['rot_angles'] = ('rotate', np.array([ang]))
    rgba = pil(test_img(40, 60, 4, 31, 1))
    for an, a in (('p10.434190', 10.434190), ('m10.434190', -10.434190), ('pang', ang), ('mang', -ang),
                  ('p33.3', 33.3), ('p90', 90), ('p180', 180), ('m90', -90)):
        r = arr(rgba.rotate(a, resample=BI, expand=True))
        E['rot_RGBA_' + an + '_size'] = ('rotate', np.array(r.shape[:2], np.int64))
        E['rot_RGBA_' + an] = ('rotate', r)
    st = test_img(170, 300, 4, 32, 1)
    st[..., 3] = np.where(st[..., 3] > 100, 255, 0)
    r = arr(pil(st).rotate(-ang, resample=BI, expand=True))
    E['rot_strip_mang_size'] = ('rotate', np.array(r.shape[:2], np.int64))
    E['rot_strip_mang'] = ('rotate', r)
    for nm, im in (('rot_L_m10.434190', pil(test_img(40, 60, 1, 33, 1))), ('rot_RGB_m10.434190', pil(test_img(40, 60, 3, 34, 0)))):
        r = arr(im.rotate(-10.434190, resample=BI, expand=True))
        E[nm + '_size'] = ('rotate', np.array(r.shape[:2], np.int64))
        E[nm] = ('rotate', r)
    r = arr(rgba.rotate(-10.434190, resample=BL, expand=True))
    E['rot_RGBA_bilinear_size'] = ('rotate', np.array(r.shape[:2], np.int64))
    E['rot_RGBA_bilinear'] = ('rotate', r)
    E['rot_RGBA_noexpand_25'] = ('rotate', arr(rgba.rotate(25.0, resample=BI, expand=False)))

    # ImageDraw
    BR = [[(190, 70), (215, 5), (232, 10), (226, 90), (252, 180), (225, 188), (205, 140)],
          [(308, 150), (345, 122), (378, 128), (362, 188), (318, 188)],
          [(388, 70), (405, 8), (422, 14), (452, 100), (457, 182), (442, 222), (408, 218), (414, 150)]]
    m = Image.new('L', (520, 260), 0)
    d = ImageDraw.Draw(m)
    for p in BR:
        d.polygon([(x + 4780 - 4730, y + 1380 - 1360) for x, y in p], fill=255)
    E['pg_branches'] = ('polygon', arr(m))
    b = Image.new('L', (340, 300), 0)
    pts = []
    for k in range(90):
        t = k * (2 * math.pi / 90)
        rr = 60 + 25 * math.sin(3 * t) + 10 * math.sin(7 * t + 1) + float(int(hash32(k, 41)) & 1023) / 1023.0 * 9 - 4.5
        pts.append(((170 + rr * math.cos(t) * 1.3 - 3.25) * 2 - 170, (150 + rr * math.sin(t)) * 2 - 150))
    ImageDraw.Draw(b).polygon(pts, fill=255)
    E['pg_blob'] = ('polygon', arr(b))
    s_ = Image.new('L', (90, 80), 0)
    d = ImageDraw.Draw(s_)
    d.polygon([(-10, 5), (30, 5), (50, 5), (45, 30), (88, 40), (95, 79), (60, 60), (40, 85), (20, 60), (20, 60), (5, 70), (0, 40)], fill=200)
    d.polygon([(10, 10), (20, 20), (10, 30), (0, 20)], fill=255)
    d.polygon([(60, 10), (70, 10), (80, 10), (80, 20), (70, 20), (60, 20)], fill=128)
    d.polygon([(30.7, 40.2), (44.9, 44.99), (36.5, 58.5), (29.999, 52.1)], fill=77)
    E['pg_shapes'] = ('polygon', arr(s_))
    for i, (Hh, Ww, x0, y0, x1, y1, r) in enumerate(((173, 35, 0, 0, 34, 172, 10), (169, 32, 0, 0, 31, 168, 9), (30, 50, 2, 3, 47, 26, 20),
                                                    (20, 20, 0, 0, 19, 19, 15), (40, 40, 3, 3, 30, 35, 0), (45, 60, 5, 4, 50, 40, 7), (30, 60, 0, 0, 59, 12, 9))):
        im = Image.new('L', (Ww, Hh), 0)
        ImageDraw.Draw(im).rounded_rectangle((x0, y0, x1, y1), radius=r, fill=255)
        E['rr_%d' % i] = ('rounded_rectangle', arr(im))
    e = Image.new('L', (70, 50), 0)
    d = ImageDraw.Draw(e)
    d.ellipse((3, 4, 60, 40), fill=255)
    d.ellipse((50, 30, 69, 49), fill=128)
    d.pieslice((10, 10, 40, 45), 30, 200, fill=60)
    d.pieslice((0, 0, 25, 25), -45, 45, fill=90)
    d.rectangle((60, 2, 66, 8), fill=33)
    E['dr_misc'] = ('ellipse/pieslice/rectangle', arr(e))

    # reductions
    ns = list(range(1, 301)) + [1000, 8191, 8192, 8193, 20000, 100000]
    s32, m32, s64, m64 = [], [], [], []
    for n in ns:
        a = test_f32(n, n, 255, 0)
        dd = test_f64(n, n + 7, 2, -1)
        s32.append(a.sum()); m32.append(a.mean()); s64.append(dd.sum()); m64.append(dd.mean())
    E['red_sum_f32'] = ('sum (flat)', np.array(s32, np.float32))
    E['red_mean_f32'] = ('mean (flat)', np.array(m32, np.float32))
    E['red_sum_f64'] = ('sum (flat)', np.array(s64, np.float64))
    E['red_mean_f64'] = ('mean (flat)', np.array(m64, np.float64))
    big = test_f32(3000 * 40 * 3, 5, 255, 0).reshape(3000, 40, 3)
    vs, vm, vsd = [], [], []
    for n in (100, 2730, 2731, 2800, 2999):
        v = big[:n, 7]
        vs.append(v.sum()); vm.append(v.mean()); vsd.append(v.std())
    E['red_view_sum'] = ('sum (strided view)', np.array(vs, np.float32))
    E['red_view_mean'] = ('mean (strided view)', np.array(vm, np.float32))
    E['red_view_std'] = ('std', np.array(vsd, np.float32))
    E['red_std_f32'] = ('std', np.array([test_f32(n, n + 3, 40, -20).std() for n in (5, 37, 1000, 9000)], np.float32))
    a3 = test_f32(70 * 90 * 3, 6, 200, -100).reshape(70, 90, 3)
    d3 = test_f64(70 * 90 * 3, 8, 200, -100).reshape(70, 90, 3)
    E['red_sumlast_f32'] = ('sum(-1)', a3.sum(-1).ravel())
    E['red_meanlast_f32'] = ('mean(-1)', a3.mean(-1).ravel())
    E['red_sumlast_f64'] = ('sum(-1)', d3.sum(-1).ravel())
    E['red_meanlast_f64'] = ('mean(-1)', d3.mean(-1).ravel())
    E['red_normlast_f32'] = ('linalg.norm(axis=-1)', np.linalg.norm(a3, axis=-1).ravel())
    E['red_normlast_f64'] = ('linalg.norm(axis=-1)', np.linalg.norm(d3, axis=-1).ravel())
    ax = []
    for n in (5, 100, 1000, 50000):
        r = test_f32(n * 3, n + 9, 255, 0).reshape(n, 3)
        ax += list(r.mean(axis=0)) + list(r.sum(axis=0))
    E['red_axis0_f32'] = ('mean/sum(axis=0)', np.array(ax, np.float32))
    for ss in (2, 3):
        p = test_f32(60 * ss * 70 * ss, ss + 10, 255, 0).reshape(60 * ss, 70 * ss)
        E['red_block_L_%d' % ss] = ('block mean(axis=(1,3))', p.reshape(60, ss, 70, ss).mean(axis=(1, 3)).ravel())
        q = test_f32(60 * ss * 70 * ss * 3, ss + 20, 255, 0).reshape(60 * ss, 70 * ss, 3)
        E['red_block_RGB_%d' % ss] = ('block mean(axis=(1,3))', q.reshape(60, ss, 70, ss, 3).mean(axis=(1, 3)).ravel())
        pd = test_f64(60 * ss * 70 * ss, ss + 30, 1, 0).reshape(60 * ss, 70 * ss)
        E['red_block_Ld_%d' % ss] = ('block mean(axis=(1,3))', pd.reshape(60, ss, 70, ss).mean(axis=(1, 3)).ravel())
        qd = test_f64(60 * ss * 70 * ss * 3, ss + 40, 255, 0).reshape(60 * ss, 70 * ss, 3)
        E['red_block_RGBd_%d' % ss] = ('block mean(axis=(1,3))', qd.reshape(60, ss, 70, ss, 3).mean(axis=(1, 3)).ravel())

    # functions
    m32_, m64_ = [], []
    for n in (1, 2, 7, 100, 101, 1000, 1001):
        a = test_f32(n, n + 40, 255, 0)
        m32_ += [np.median(a.reshape(-1, 1), axis=0)[0], np.median(a)]
        dd = test_f64(n, n + 41, 1, 0)
        m64_ += [np.median(dd.reshape(-1, 1), axis=0)[0], np.median(dd)]
    E['fn_median_f32'] = ('median', np.array(m32_, np.float32))
    E['fn_median_f64'] = ('median', np.array(m64_, np.float64))
    prof = test_f32(170 * 3, 50, 90, 3).reshape(170, 3)
    pc35 = np.percentile(prof[16:132], 35, axis=0)
    assert pc35.dtype == np.float64, pc35.dtype
    pc = list(pc35)
    l37 = test_f32(37, 51, 255, 0)
    pc += [np.float64(np.percentile(l37, q)) for q in (0.0, 10.0, 35.0, 50.0, 62.5, 99.9, 100.0)]
    d37 = test_f64(37, 52, 1, 0)
    pc += [np.percentile(d37, q) for q in (0.0, 10.0, 35.0, 50.0, 99.9, 100.0)]
    E['fn_percentile'] = ('percentile', np.array(pc, np.float64))
    src = test_f32(1000, 53, 255, 0)
    src = np.floor(src * 4) / 4
    ss_ = np.sort(src)
    E['fn_sort'] = ('sort', ss_)
    q = test_f32(2000, 54, 270, -5)
    q[:500] = ss_[(np.arange(500) * 7) % 1000]
    E['fn_searchsorted_left'] = ('searchsorted', np.searchsorted(ss_, q, side='left').astype(np.int64))
    E['fn_searchsorted_right'] = ('searchsorted', np.searchsorted(ss_, q, side='right').astype(np.int64))
    xp = np.linspace(0, 1, len(ss_))
    xs = test_f64(5000, 55, 1.2, -0.1)
    xs[:50] = xp[np.arange(50) * 13]
    E['fn_interp'] = ('interp', np.interp(xs, xp, ss_))
    ls = []
    for n in (1, 2, 3, 7, 24, 4440):
        ls += list(np.linspace(0, 1, n))
    ls += list(np.linspace(0.95, 0.7, 6)) + list(np.linspace(1, 0, 16)) + list(np.linspace(-3.5, 7.25, 13))
    E['fn_linspace'] = ('linspace', np.array(ls))
    k81 = np.array([math.exp(-0.5 * ((i / 16.0) * (i / 16.0))) for i in range(-40, 41)])
    k51 = np.array([math.exp(-0.5 * ((i / 10.0) * (i / 10.0))) for i in range(-25, 26)])
    k81 /= k81.sum()
    k51 /= k51.sum()
    col = test_f32(300, 56, 255, 0)
    cv = list(np.convolve(np.pad(col, 40, mode='edge'), k81, mode='valid'))
    cv += list(np.convolve(np.pad(col, 25, mode='edge'), k51, mode='valid'))
    sa = col[:30]
    for mode in ('same', 'full', 'valid'):
        cv += list(np.convolve(sa, k51, mode=mode))
        cv += list(np.convolve(sa, [0.25, 0.5, 0.25], mode=mode))
    E['fn_convolve'] = ('convolve', np.array(cv, np.float64))
    gpl = test_f32(23 * 37, 57, 100, -20).reshape(23, 37)
    gy, gx = np.gradient(gpl)
    E['fn_gradient_y'] = ('gradient', gy.ravel())
    E['fn_gradient_x'] = ('gradient', gx.ravel())
    tx = np.concatenate([test_f64(200000, 58, 16, -8),
                         [0.0, -0.0, 1e-300, -1e-300, 5e-324, 1e-9, 0.125, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 19.0, 19.5, 30.0, 1e10, -1e10, 1e300,
                          np.inf, -np.inf]])
    ty = np.tanh(tx)
    # the recipe calls np.tanh on a Python float: the same code path
    assert all(np.float64(np.tanh(float(v))).view(np.uint64) == t.view(np.uint64) for v, t in zip(tx[:3000], ty[:3000]))
    E['fn_tanh_f64'] = ('tanh', ty)
    tfx = np.concatenate([test_f32(200000, 59, 20, -10), np.array([0.0, -0.0, 1e-30, 1e-45, 0.5, 1.0, 3.0, 8.0, 9.0, 9.5, 20.0, 1e20, -1e20], np.float32)])
    E['fn_tanh_f32'] = ('tanh', np.tanh(tfx))
    inf, nan = np.float32(np.inf), np.float32(np.nan)
    ex = np.concatenate([test_f32(300000, 65, 240, -120),
                         np.array([0.0, -0.0, 88.7, 88.72283935546875, 89.0, -103.9, -103.97208404541015625, -110.0, 1e-40, -87.5, inf, -inf, nan], np.float32)])
    with np.errstate(all='ignore'):
        e_ = np.exp(ex)
    assert all(np.exp(v).view(np.uint32) == t.view(np.uint32) for v, t in zip(ex[:2000], e_[:2000]))   # scalar path the same
    E['fn_exp_f32'] = ('exp (float32)', e_)
    sx = np.concatenate([test_f32(300000, 66, 400, -200), test_f32(20000, 67, 300000, -150000),
                         np.array([0.0, -0.0, 1e-30, 71476.0625, 71476.07, 117435.992, 117436.0, 1e30, inf, nan], np.float32)])
    with np.errstate(all='ignore'):
        E['fn_sin_f32'] = ('sin/cos (float32)', np.sin(sx))
        E['fn_cos_f32'] = ('sin/cos (float32)', np.cos(sx))
    # pw and ex: tube_walk_lib.py's deterministic power and exp
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import tube_walk_lib as twl
    xr = test_f64(200000, 70, 1.2, 0)
    xw = np.ldexp(test_f64(50000, 71, 0.5, 0.5), (hash32(np.arange(50000), 72) % np.uint64(2100)).astype(np.int64) - 1074)
    xw = np.concatenate([xw, [0.0, 1.0, 0.5, 1.2, 5e-324, 1e-310, 2.0 ** -1022, 1e300, 0.7071067811865476, 0.7071067811865475]])
    po = []
    with np.errstate(all='ignore'):
        for e in (0.85, 1.6, 1.1, 0.4, 0.8, 1.5, 3.0, 12.0, 20.0, 24.0, 26.0, 30.0, -0.7, 2.5, 0.0, 1.0 / 3):
            po += [twl.pw(xr, e), twl.pw(xw, e)]
        xn = test_f64(20000, 73, 4, -2)
        po += [twl.pw(xn, 3.0), twl.pw(xn, 4.0)]
        E['fn_pw_f64'] = ('pw (float64)', np.concatenate(po))
        x32 = np.concatenate([test_f32(200000, 74, 1.2, 0), np.array([0.0, 1.0, 1e-45, 1e-38, 0.5, 1.2], np.float32)])
        E['fn_pw_f32'] = ('pw (float32)', np.concatenate([twl.pw(x32, e) for e in (0.85, 1.6, 1.1, 0.4, 0.8, 1.5, 3.0)]))
        xe = [test_f64(300000, 75, 80, -70)]
        xe += [-0.5 * (np.arange(-h, h + 1) / d) ** 2 for h, d in ((40, 16.0), (25, 10.0), (8, 1.6))]
        xe.append(np.array([0.0, -0.0, 1.0, -745.0, 709.0, 710.0, -1000.0]))
        E['fn_ex_f64'] = ('ex', twl.ex(np.concatenate(xe)))
        E['fn_ex_f32'] = ('ex', twl.ex(test_f32(100000, 76, 60, -50)))
    assert twl.pw(np.float32(0.5), 1.6).dtype == np.float32 and type(twl.pw(0.5, 1.6)) == np.float64
    ha, hb = test_f64(100000, 60, 4000, -2000), test_f64(100000, 61, 300, -150)
    hy = [math.hypot(a, b) for a, b in zip(ha, hb)]
    hy += [math.hypot(math.floor(a), math.floor(b)) for a, b in zip(ha[:1000], hb[:1000])]
    hy += [math.hypot(a * 1e-310, b * 1e-310) for a, b in zip(ha[:1000], hb[:1000])]
    hy += [math.hypot(0, 0), math.hypot(3, 4), math.hypot(1e308, 1e308)]
    E['fn_hypot'] = ('math.hypot', np.array(hy))
    nv = [np.linalg.norm(np.array(v)) for v in ([-0.55, -0.55, 0.63], [0.9, -0.1, 0.25], [-0.6, -0.45, 0.66])]
    nv += [np.linalg.norm(test_f64(n, n + 62, 2, -1)) for n in (3, 5, 7, 20, 101)]
    E['fn_norm'] = ('linalg.norm (1-D)', np.array(nv))
    uf = np.concatenate([test_f32(5000, 63, 700, -200), np.array([255.9, 256.0, 300.5, -1.0, -0.5, -200.7, 1e10, -0.0], np.float32)])
    with warnings.catch_warnings():
        warnings.simplefilter('ignore')
        E['fn_u8_f32'] = ('astype(uint8)', uf.astype(np.uint8))
        E['fn_u8clip_f32'] = ('astype(uint8)', np.clip(uf, 0, 255).astype(np.uint8))
        E['fn_u8_f64'] = ('astype(uint8)', test_f64(3000, 64, 700, -200).astype(np.uint8))
    E['fn_clip_f32'] = ('clip', np.clip(uf, -3.5, 100.25))
    return E


DT = {'u8': np.uint8, 'i8': np.int64, 'f4': np.float32, 'f8': np.float64}


def compare(exp, got):
    """(cases differing, max abs difference) between two arrays."""
    exp = np.ascontiguousarray(exp).ravel()
    if exp.dtype.kind == 'f':
        if got.dtype != exp.dtype:
            return len(exp), float('inf')
        u = np.uint32 if exp.dtype == np.float32 else np.uint64
        both_nan = np.isnan(exp) & np.isnan(got)
        bad = (exp.view(u) != got.view(u)) & ~both_nan
        diff = np.abs(exp.astype(np.float64) - got.astype(np.float64))
        diff[both_nan] = 0
        diff = np.where(bad & ~np.isfinite(diff), np.inf, diff)
        return int(bad.sum()), float(diff[bad].max()) if bad.any() else 0.0
    if got.shape != exp.shape:
        return len(exp), float('inf')
    d = np.abs(exp.astype(np.int64) - got.astype(np.int64))
    return int((d != 0).sum()), int(d.max()) if len(d) else 0


def transcendentals():
    """How often numpy's float functions differ from glibc's here."""
    import ctypes
    m = ctypes.CDLL('libm.so.6')
    rng = np.random.default_rng(0)
    rows = []
    for name, cname, dt, lo, hi in (('exp', 'expf', np.float32, -30, 1), ('exp', 'exp', np.float64, -40, 1),
                                    ('sin', 'sinf', np.float32, -20, 40), ('cos', 'cosf', np.float32, -20, 40),
                                    ('log', 'logf', np.float32, 0.01, 300), ('sqrt', 'sqrtf', np.float32, 0, 300),
                                    ('tanh', 'tanh', np.float64, -6, 6)):
        f = getattr(m, cname)
        ct = ctypes.c_float if dt == np.float32 else ctypes.c_double
        f.restype, f.argtypes = ct, [ct]
        x = rng.uniform(lo, hi, 20000).astype(dt)
        lib = np.array([f(float(v)) for v in x], dt)
        rows.append(('np.%s %s' % (name, np.dtype(dt).name), (getattr(np, name)(x) != lib).mean()))
    for dt, cname in ((np.float32, 'powf'), (np.float64, 'pow')):
        f = getattr(m, cname)
        ct = ctypes.c_float if dt == np.float32 else ctypes.c_double
        f.restype, f.argtypes = ct, [ct, ct]
        x = rng.uniform(0, 1.2, 20000).astype(dt)
        for e in (0.85, 1.6, 3.0):
            lib = np.array([f(float(v), e) for v in x], dt)
            rows.append(('x ** %g %s' % (e, np.dtype(dt).name), ((x ** dt(e) if dt == np.float32 else x ** e) != lib).mean()))
    print('\nnumpy against glibc on this machine (share of inputs that differ):')
    for nm, fr in rows:
        print('  %-22s %6.1f%%' % (nm, 100 * fr))


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(2)
    d = sys.argv[1]
    got = {}
    for line in open(os.path.join(d, 'index.txt')):
        name, dt, n = line.split()
        got[name] = np.fromfile(os.path.join(d, name + '.bin'), dtype=DT[dt])
        assert len(got[name]) == int(n), name
    E = expected()
    missing = sorted(set(E) - set(got)) + sorted(set(got) - set(E))
    table = {}
    worst = 0
    print('%-28s %-30s %10s %8s %10s' % ('case', 'op', 'values', 'differ', 'max diff'))
    for name in sorted(E):
        op, exp = E[name]
        if name not in got:
            continue
        nbad, md = compare(exp, got[name])
        n = np.asarray(exp).size
        print('%-28s %-30s %10d %8d %10s' % (name, op, n, nbad, md))
        t = table.setdefault(op, [0, 0, 0, 0.0])
        t[0] += 1; t[1] += n; t[2] += nbad; t[3] = max(t[3], md)
        worst = max(worst, md if md == md else float('inf'))
    print('\n%-30s %6s %10s %8s %10s' % ('op', 'cases', 'values', 'differ', 'max diff'))
    for op, (c, n, nb, md) in table.items():
        print('%-30s %6d %10d %8d %10s' % (op, c, n, nb, md))
    if missing:
        print('missing on one side:', missing)
    if '--transcendentals' in sys.argv:
        transcendentals()
    ok = worst == 0 and not missing
    print('\nALL EXACT' if ok else '\nDIFFERENCES FOUND')
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
