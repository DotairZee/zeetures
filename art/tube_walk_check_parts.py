"""Proof for src/openc2e/madeart/tube_walk_parts.{h,cpp}, the C++ port of
the tube's parts in tube_walk.py (paste_rot_masked, blob, smooth_fill,
deck_fill, load_front) and tube_walk_lib.py (dock_join, _fbm, _q8,
stalactites, render_rock_earth).

The recipe's own make() is run on the player's game with those functions
wrapped (the recipe files are not changed): each call's real inputs are
written to DIR/in and its results to DIR/ref. The engine, run with
OPENC2E_TW_CHECK_PARTS=DIR, calls its ports on DIR/in and writes DIR/out;
this compares DIR/out with DIR/ref: every array must show max difference 0,
floats bit for bit. _fbm and _q8 are also checked alone on fresh generators.

    python3 art/tube_walk_check_parts.py DIR [--c2 C2_DIR] [--engine PROGRAM] [--fresh]

--engine runs the engine's check itself (any openc2e program, e.g.
build-fast/caos_to_bytecode); without it, run that by hand and then this
again. --fresh records the recipe's run again (it is kept in DIR otherwise).
Written against NumPy 1.26.4 and Pillow 10.2.0.
"""
import argparse
import inspect
import os
import shutil
import subprocess
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import tube_walk as tw          # noqa: E402
import tube_walk_lib as lib     # noqa: E402

C2 = os.path.expanduser('~/.steam/debian-installation/steamapps/common/Creatures The Albian Years/Creatures 2')
DT = {np.dtype(np.float32): 'f4', np.dtype(np.float64): 'f8', np.dtype(np.uint8): 'u1', np.dtype(np.int64): 'i8'}
NP = {'f4': np.float32, 'f8': np.float64, 'u1': np.uint8, 'i8': np.int64}


class Store:
    """A folder of raw arrays: NAME.bin (C order) and index.txt lines
    'NAME DTYPE D0 [D1 [D2]]'."""

    def __init__(self, d):
        os.makedirs(d, exist_ok=True)
        self.d = d
        self.idx = open(os.path.join(d, 'index.txt'), 'w')

    def put(self, name, a):
        a = np.asarray(a)
        if a.dtype == bool:
            a = a.astype(np.uint8)
        a = np.ascontiguousarray(a)
        a.tofile(os.path.join(self.d, name + '.bin'))
        self.idx.write('%s %s %s\n' % (name, DT[a.dtype], ' '.join(str(s) for s in a.shape)))
        self.idx.flush()


def load(d):
    out = {}
    for line in open(os.path.join(d, 'index.txt')):
        p = line.split()
        if not p:
            continue
        name, dt, shape = p[0], p[1], tuple(int(s) for s in p[2:])
        out[name] = np.fromfile(os.path.join(d, name + '.bin'), NP[dt]).reshape(shape)
    return out


def f8(*v):
    return np.array([float(x) for x in v], np.float64)


def need(a, dt, what):
    if a.dtype != dt:
        raise SystemExit('%s: expected %s, the recipe gave %s' % (what, dt, a.dtype))


def record(d, c2):
    """Run the recipe's make() with the parts wrapped, writing each call."""
    for sub in ('in', 'ref'):
        shutil.rmtree(os.path.join(d, sub), ignore_errors=True)
    IN, REF = Store(os.path.join(d, 'in')), Store(os.path.join(d, 'ref'))
    cases = open(os.path.join(d, 'in', 'cases.txt'), 'w')
    count = {}

    def case(kind):
        i = count.get(kind, 0)
        count[kind] = i + 1
        cases.write('%s %d\n' % (kind, i))
        cases.flush()
        return '%s%d.' % (kind, i)

    def wrap(mod, name, body):
        orig = getattr(mod, name)
        sig = inspect.signature(orig)

        def w(*a, **k):
            b = sig.bind(*a, **k)
            b.apply_defaults()
            return body(orig, b.arguments)
        setattr(mod, name, w)
        return orig

    def paste(orig, A):
        p = case('paste')
        need(A['out'], np.float32, 'paste out'); need(A['pc'], np.float32, 'paste pc')
        IN.put(p + 'out', A['out']); IN.put(p + 'pc', A['pc']); IN.put(p + 'keep', A['keep_out'])
        IN.put(p + 'args', f8(A['angle_deg'], A['left_top'][0], A['left_top'][1]))
        r = orig(**A)
        REF.put(p + 'out', A['out'])
        return r

    def smooth(orig, A):
        p = case('smooth')
        need(A['F'], np.float32, 'smooth F')
        IN.put(p + 'F', A['F']); IN.put(p + 'M', A['M'])
        IN.put(p + 'args', f8(A['iters'], A['vk'], A['div']))
        r = orig(**A)
        REF.put(p + 'F', A['F'])
        return r

    def deck(orig, A):
        p = case('deck')
        need(A['img'], np.float32, 'deck img')
        IN.put(p + 'img', A['img']); IN.put(p + 'deck_rm', A['deck_rm'])
        has = A['inside'] is not None
        if has:
            IN.put(p + 'inside', A['inside'])
        IN.put(p + 'args', f8(*A['box'], A['seed'], has))
        r = orig(**A)
        REF.put(p + 'img', A['img'])
        return r

    def stal(orig, A):
        p = case('stal')
        need(A['o'], np.float32, 'stal o'); need(A['bg'], np.float32, 'stal bg')
        IN.put(p + 'o', A['o']); IN.put(p + 'bg', A['bg'])
        IN.put(p + 'items', np.array(A['items'], np.float64).reshape(-1, 5))
        IN.put(p + 'args', f8(A['OX'], A['OY'], A['seed'], A['ss']))
        r = orig(**A)
        REF.put(p + 'o', A['o'])
        return r

    def dock(orig, A):
        p = case('dock')
        for k in ('o', 'base', 'MOD'):
            need(A[k], np.float32, 'dock ' + k)
        IN.put(p + 'o', A['o']); IN.put(p + 'base', A['base']); IN.put(p + 'MOD', A['MOD']); IN.put(p + 'keep', A['keep'])
        IN.put(p + 'args', f8(A['X0'], A['start_top'], A['slope'], A['OX'], A['OY'], A['n'], A['blend'], A['kstart']))
        r = orig(**A)
        REF.put(p + 'o', A['o'])
        return r

    def blob(orig, A):
        p = case('blob')
        IN.put(p + 'points', np.array(A['points'], np.float64).reshape(-1, 2))
        IN.put(p + 'args', f8(A['jitter'], A['seed']))
        r = orig(**A)
        REF.put(p + 'pts', np.array(r, np.float64).reshape(-1, 2))
        return r

    def rock(orig, A):
        p = case('rock')
        need(A['sample'], np.float32, 'rock sample')
        IN.put(p + 'outline', np.array(A['outline'], np.float64).reshape(-1, 2))
        IN.put(p + 'sample', A['sample'])
        IN.put(p + 'args', f8(A['seed'], A['ss'], A['patch']))
        r = orig(**A)
        x0, y0, rgb, al = r
        need(rgb, np.float64, 'rock rgb'); need(al, np.float32, 'rock alpha')
        REF.put(p + 'xy', np.array([x0, y0], np.int64)); REF.put(p + 'rgb', rgb); REF.put(p + 'alpha', al)
        return r

    def front(orig, A):
        p = case('front')
        r = orig(**A)
        REF.put(p + 'front', r)
        return r

    origs = [(tw, 'paste_rot_masked', wrap(tw, 'paste_rot_masked', paste)),
             (tw, 'smooth_fill', wrap(tw, 'smooth_fill', smooth)),
             (tw, 'deck_fill', wrap(tw, 'deck_fill', deck)),
             (tw, 'blob', wrap(tw, 'blob', blob)),
             (tw, 'load_front', wrap(tw, 'load_front', front)),
             (lib, 'stalactites', wrap(lib, 'stalactites', stal)),
             (lib, 'dock_join', wrap(lib, 'dock_join', dock)),
             (lib, 'render_rock_earth', wrap(lib, 'render_rock_earth', rock))]
    try:
        tw.make(c2)
    finally:
        for mod, name, o in origs:
            setattr(mod, name, o)

    # _fbm and _q8 alone, on fresh generators (sizes and cells as the
    # stalactites and the rock use them, ss 3 and 2)
    for seed, H, W, cells, amps in ((3, 222, 117, [15], [1.0]),
                                    (5, 222, 117, [18, 2.5 * 3], [1.0, 0.8]),
                                    (7, 222, 117, [1.2 * 3], [1.0]),
                                    (9, 222, 30, [1.0 * 3, 0.5 * 3], [1.0, 0.6]),
                                    (11, 437, 233, [40, 18], [1.0, 0.5])):
        p = case('fbm')
        IN.put(p + 'args', f8(seed, H, W)); IN.put(p + 'cells', f8(*cells)); IN.put(p + 'amps', f8(*amps))
        out = lib._fbm(np.random.default_rng(seed), H, W, cells, amps)
        need(out, np.float32, 'fbm')
        REF.put(p + 'out', out); REF.put(p + 'q8', lib._q8(out))
    cases.close()
    print('recorded:', ', '.join('%s %d' % kv for kv in count.items()))


def compare(d):
    ref, out = load(os.path.join(d, 'ref')), load(os.path.join(d, 'out'))
    bad = 0
    print('%-16s %-8s %-18s %12s %10s' % ('array', 'dtype', 'shape', 'max diff', 'bits off'))
    for name, r in ref.items():
        o = out.get(name)
        if o is None or o.shape != r.shape or o.dtype != r.dtype:
            print('%-16s %-8s %-18s %s' % (name, DT[r.dtype], 'x'.join(map(str, r.shape)),
                                         'MISSING' if o is None else 'shape/dtype %s %s' % (o.dtype, o.shape)))
            bad += 1
            continue
        if r.dtype.kind == 'f':
            ui = np.uint32 if r.dtype == np.float32 else np.uint64
            off = int((r.view(ui) != o.view(ui)).sum())
            with np.errstate(invalid='ignore'):
                md = float(np.nanmax(np.abs(r.astype(np.float64) - o.astype(np.float64)))) if r.size else 0.0
        else:
            off = int((r != o).sum())
            md = float(np.abs(r.astype(np.int64) - o.astype(np.int64)).max()) if r.size else 0.0
        bad += off > 0
        print('%-16s %-8s %-18s %12.6g %10d' % (name, DT[r.dtype], 'x'.join(map(str, r.shape)), md, off))
    print('ALL MATCH' if not bad else '%d ARRAYS DIFFER' % bad)
    return bad


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('dir')
    ap.add_argument('--c2', default=C2)
    ap.add_argument('--engine')
    ap.add_argument('--fresh', action='store_true')
    a = ap.parse_args()
    if a.fresh or not os.path.exists(os.path.join(a.dir, 'ref', 'index.txt')):
        record(a.dir, a.c2)
    if a.engine:
        shutil.rmtree(os.path.join(a.dir, 'out'), ignore_errors=True)
        subprocess.run([a.engine], env=dict(os.environ, OPENC2E_TW_CHECK_PARTS=a.dir), check=True)
    if not os.path.exists(os.path.join(a.dir, 'out', 'index.txt')):
        print('now run any openc2e program with OPENC2E_TW_CHECK_PARTS=%s, then this again' % a.dir)
        sys.exit(0)
    sys.exit(1 if compare(a.dir) else 0)
