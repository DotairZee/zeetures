"""Proof for src/openc2e/madeart/tube_walk_paint.{h,cpp}, the C++ port of
tube_walk.py's scene and old-tube paint-out (put_scenery, old_tube,
new_tube, paint_out; tube_walk_lib.py's quilt_fill, tone_correct,
match_texture, restore_branches): the real recipe run here on the player's
game, its every intermediate compared with the engine's (max difference 0,
floats bit for bit).

    OPENC2E_TW_CHECK_PAINT=DIR build-fast/caos_to_bytecode   (any openc2e program)
    python3 art/tube_walk_check_paint.py DIR [C2_DIR]

The engine reads the game from OPENC2E_TW_CHECK_PAINT_C2 (default: the Steam
copy below); give this script the same folder. The recipe files are only
imported, never changed: its sub-steps are recorded by wrapping the
library's functions while tube_walk.paint_out runs, and the patch search's
energies by wrapping np.argsort while quilt_fill runs. --ties reports
whether any search had tied energies among its three best (where numpy's
argsort tie order would matter).
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import s16                       # noqa: E402
import tube_walk as tw           # noqa: E402
import tube_walk_lib as lib      # noqa: E402
from background import region    # noqa: E402

C2 = os.path.expanduser('~/.steam/debian-installation/steamapps/common/Creatures The Albian Years/Creatures 2')
DT = {'u8': np.uint8, 'i8': np.int64, 'f4': np.float32, 'f8': np.float64}


def expected(c2):
    E = {}
    OX, OY, W, H = tw.OX, tw.OY, tw.W, tw.H
    # make()'s first lines, as written there
    bg = np.asarray(region(c2, OX, OY, OX + W, OY + H)).astype(np.float32)[..., :3]
    bgt = np.asarray(region(c2, OX, OY - 300, OX + W, OY + H)).astype(np.float32)[..., :3]
    swmp_im = s16.read(os.path.join(c2, 'Images', 'swmp.s16'))
    swmp = [np.asarray(f).astype(np.float32) for f in swmp_im]
    tubecomp = bg.copy()
    tw.put_scenery(tubecomp, swmp, tw.SCENERY[:1])
    scene = tubecomp.copy()
    tw.put_scenery(scene, swmp, tw.SCENERY[1:])
    ta = np.asarray(swmp_im[0])[..., 3] > 0
    mask, strip = tw.old_tube(tubecomp, ta)
    strip0 = strip.copy()
    MOD, pc = tw.new_tube(strip)
    assert np.array_equal(strip, strip0)

    rec = {}
    energies = []
    orig = {n: getattr(lib, n) for n in ('quilt_fill', 'tone_correct', 'match_texture', 'restore_branches')}

    def quilt_fill(*a, **k):
        real = np.argsort

        def argsort(e, *aa, **kk):
            energies.append(np.array(e, copy=True))
            return real(e, *aa, **kk)
        np.argsort = argsort
        try:
            r = orig['quilt_fill'](*a, **k)
        finally:
            np.argsort = real
        rec['quilt'] = r.copy()
        return r

    def tone_correct(res, bg_, band, *a, **k):
        rec['band'] = band.copy()
        r = orig['tone_correct'](res, bg_, band, *a, **k)
        rec['toned'] = r.copy()
        return r

    def match_texture(*a, **k):
        r = orig['match_texture'](*a, **k)
        rec['textured'] = r.copy()
        return r

    def restore_branches(*a, **k):
        r = orig['restore_branches'](*a, **k)
        rec['restored'] = r.copy()
        return r
    lib.quilt_fill, lib.tone_correct, lib.match_texture, lib.restore_branches = quilt_fill, tone_correct, match_texture, restore_branches
    try:
        base = tw.paint_out(scene, bg, bgt, mask, strip)
    finally:
        for n, f in orig.items():
            setattr(lib, n, f)

    # put_scenery's cut= form, as make() uses it (the old deck's place left out)
    deck_rm = np.zeros((H, W), bool)
    for x0, y0, x1, y1 in tw.DECK_RM:
        deck_rm[y0 - OY:y1 - OY, x0 - OX:x1 - OX] = True
    cut = base.copy()
    tw.put_scenery(cut, swmp, tw.SCENERY[1:], cut=deck_rm)

    length = __import__('math').hypot(tw.END_X - tw.X0, tw.start_top + tw.slope * (tw.END_X - tw.X0) - tw.start_top)
    E['consts'] = np.array([tw.start_top, tw.slope, tw.ang, length,
                            tw.top_of(4776), tw.top_of(6035), tw.tube_y(tw.ENTRY), tw.tube_y(5000.25)], np.float64)
    E['bg'] = bg
    E['bgt'] = bgt
    E['ta'] = ta.astype(np.uint8)
    E['tubecomp'] = tubecomp
    E['scene'] = scene
    E['mask'] = mask.astype(np.uint8)
    E['strip'] = strip
    E['MOD'] = MOD
    E['pc'] = pc
    E['quilt_e'] = np.concatenate(energies).astype(np.float64)
    E['quilt_e_n'] = np.array([len(e) for e in energies], np.int64)
    E['quilt'] = rec['quilt']
    E['band'] = rec['band'].astype(np.uint8)
    E['toned'] = rec['toned']
    E['textured'] = rec['textured']
    E['restored'] = rec['restored']
    E['base'] = base
    E['scenery_cut'] = cut
    return E, energies


def compare(exp, got):
    """(values differing, max abs difference); floats bit for bit."""
    exp = np.ascontiguousarray(exp).ravel()
    if got.dtype != exp.dtype or got.shape != exp.shape:
        return len(exp), float('inf')
    if exp.dtype.kind == 'f':
        u = np.uint32 if exp.dtype == np.float32 else np.uint64
        both_nan = np.isnan(exp) & np.isnan(got)
        bad = (exp.view(u) != got.view(u)) & ~both_nan
        diff = np.abs(exp.astype(np.float64) - got.astype(np.float64))
        return int(bad.sum()), (float(diff[bad].max()) if bad.any() else 0.0)
    d = np.abs(exp.astype(np.int64) - got.astype(np.int64))
    return int((d != 0).sum()), (int(d.max()) if len(d) else 0)


def ties(energies):
    """Searches whose three best energies are not all distinct from the rest."""
    n = 0
    for e in energies:
        s = np.sort(e)
        if len(s) > 3 and (s[2] == s[3] or s[0] == s[1] or s[1] == s[2]):
            n += 1
    return n


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(2)
    d = sys.argv[1]
    c2 = sys.argv[2] if len(sys.argv) > 2 and not sys.argv[2].startswith('--') else C2
    got, shapes = {}, {}
    for line in open(os.path.join(d, 'index.txt')):
        name, dt, n = line.split()[:3]
        got[name] = np.fromfile(os.path.join(d, name + '.bin'), dtype=DT[dt])
        assert len(got[name]) == int(n), name
    E, energies = expected(c2)
    worst = 0
    print('%-12s %-8s %-18s %10s %8s %10s' % ('array', 'dtype', 'shape', 'values', 'differ', 'max diff'))
    for name, exp in E.items():
        if name not in got:
            print('%-12s missing from the engine\'s output' % name)
            worst = float('inf')
            continue
        nbad, md = compare(exp, got[name])
        print('%-12s %-8s %-18s %10d %8d %10s' % (name, exp.dtype, 'x'.join(map(str, exp.shape)), exp.size, nbad, md))
        worst = max(worst, md if md == md else float('inf'))
    extra = sorted(set(got) - set(E))
    if extra:
        print('only in the engine\'s output:', extra)
        worst = float('inf')
    if '--ties' in sys.argv:
        print('\npatch searches: %d, with tied energies among the best three: %d' % (len(energies), ties(energies)))
    print('\nALL EXACT' if worst == 0 else '\nDIFFERENCES FOUND')
    sys.exit(0 if worst == 0 else 1)


if __name__ == '__main__':
    main()
