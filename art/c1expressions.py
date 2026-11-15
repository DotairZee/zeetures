"""Draw the facial expressions the C1 norn breeds lack in Creatures 2, on the
player's own machine (owner, 2026-11-11).

The six C1 breeds converted for C2 (norn slots a, b, c, e, f, g) have head
pictures in C2's layout (120 frames: 6 expressions x 20 frames, each 10 head
angles with eyes open then the same 10 shut; expressions normal, happy, sad,
angry, surprised, sleepy), but for most angles their sad, angry, surprised
and sleepy frames are copies of the normal face, and slot a's angry front
frames sit on a black box. This makes them from each breed's own pixels:

- sleepy: the eyes half shut, the upper part of each eye taken from the
  same angle's eyes-shut frame;
- angry: eyelids slanting down toward the nose (from the eyes-shut frame)
  and the breed's own frown where it has one;
- surprised: eyes open, a small round open mouth;
- a frame on a black box: the box made transparent.

Bring-your-own-data: reads Images/a?Xs.s16 from the player's Creatures 2 and
writes openc2e's Overrides; no game pixels are part of the project.

    python3 c1expressions.py C2_DIR --preview PNG [--slots ag]
"""
import sys
import os
import numpy as np
from PIL import Image, ImageDraw
import s16

EXPR = ['normal', 'happy', 'sad', 'angry', 'surprised', 'sleepy']
FRONT, VIEWS = 8, 10


def find(c2, name):
    d = os.path.join(c2, 'Images')
    for f in os.listdir(d):
        if f.lower() == name:
            return os.path.join(d, f)
    return None


def arr(im):
    return np.array(im.convert('RGBA')).astype(np.int16)


def same(a, b):
    """The same picture, give or take compression noise: under 1.5% of the
    head's pixels differ noticeably."""
    if a.shape != b.shape:
        return False
    vis = (a[:, :, 3] > 0) | (b[:, :, 3] > 0)
    diff = (np.abs(a - b)[:, :, :3].sum(axis=2) > 45) & vis
    return diff.sum() <= 0.015 * max(1, vis.sum())


def unbox(a):
    """A frame drawn on an opaque dark box (it may have a transparent rim):
    the box, flood-filled from the edges through the rim and colours close
    to the box's, becomes transparent."""
    h, w, _ = a.shape
    # the outermost opaque pixel of each row and column: mostly dark means a box
    dark = (a[:, :, 3] > 0) & (a[:, :, :3].max(axis=2) < 60)
    band = np.zeros((h, w), bool)
    for y in range(h):
        xs = np.nonzero(a[y, :, 3] > 0)[0]
        if len(xs):
            band[y, xs[0]] = band[y, xs[-1]] = True
    for x in range(w):
        ys = np.nonzero(a[:, x, 3] > 0)[0]
        if len(ys):
            band[ys[0], x] = band[ys[-1], x] = True
    if band.sum() == 0 or (band & dark).sum() < 0.8 * band.sum():
        return a
    ref = np.median(a[band & dark][:, :3], axis=0)
    near = dark & (np.abs(a[:, :, :3] - ref).sum(axis=2) < 45)
    passable = near | (a[:, :, 3] == 0)
    out = a.copy()
    seen = np.zeros((h, w), bool)
    stack = [(y, x) for y in (0, h - 1) for x in range(w)] + [(y, x) for x in (0, w - 1) for y in range(h)]
    while stack:
        y, x = stack.pop()
        if y < 0 or x < 0 or y >= h or x >= w or seen[y, x] or not passable[y, x]:
            continue
        seen[y, x] = True
        stack += [(y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)]
    out[seen & near] = 0
    return out


def blobs(mask):
    """Connected parts of a mask (4-neighbour), each a list of (y, x)."""
    h, w = mask.shape
    seen = np.zeros_like(mask)
    out = []
    for y0 in range(h):
        for x0 in range(w):
            if not mask[y0, x0] or seen[y0, x0]:
                continue
            pts, stack = [], [(y0, x0)]
            while stack:
                y, x = stack.pop()
                if y < 0 or x < 0 or y >= h or x >= w or seen[y, x] or not mask[y, x]:
                    continue
                seen[y, x] = True
                pts.append((y, x))
                stack += [(y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)]
            if len(pts) >= 6:
                out.append(pts)
    return out


def eye_mask(open_, shut):
    if open_.shape != shut.shape:
        return np.zeros(open_.shape[:2], bool)
    return (np.abs(open_ - shut)[:, :, :3].sum(axis=2) > 60) & (open_[:, :, 3] > 0)


def lidded(open_, shut, share, slant=0.0, toward=None, arch_amount=0.12):
    """Eyes partly shut, drawn: within each eye (where the open and shut
    frames differ), the part above a line becomes eyelid, in the colour the
    shut frame gives that eyelid, shaded toward a darker lash edge. share:
    how far down the lid comes (0-1); slant: how much lower it comes at the
    inner corner (toward x = `toward`; negative: at the outer corner)."""
    out = open_.copy()
    for pts in blobs(eye_mask(open_, shut)):
        if len(pts) < 10:
            continue
        ys = np.array([p[0] for p in pts])
        xs = np.array([p[1] for p in pts])
        top, bottom, left, right = ys.min(), ys.max(), xs.min(), xs.max()
        cx = (left + right) / 2
        # the lid in the skin's colour: the open face just above the eye
        above = []
        for x in range(left, right + 1):
            for y in range(max(0, top - 3), top):
                if open_[y, x, 3] > 0:
                    above.append(open_[y, x, :3])
        lidcol = np.median(np.array(above), axis=0) if len(above) >= 3 else np.median(shut[ys, xs][:, :3], axis=0)
        span = max(1, bottom - top)
        for y, x in pts:
            lid = share
            if slant and toward is not None:
                inner = 1.0 if (toward > cx) == (x > cx) else -1.0
                lid += slant * inner * abs(x - cx) / max(1, (right - left) / 2)
            # a gently arched lid: lowest in the middle of the eye
            arch = 1.0 - ((x - cx) / max(1.0, (right - left) / 2)) ** 2
            edge = top + lid * span + arch_amount * span * max(0.0, arch)
            if y <= edge:
                # lid skin, a little darker near its edge; the edge itself a lash line
                t = 1.0 - min(1.0, (edge - y) / max(1.0, span * 0.35))
                col = lidcol * (1.0 - 0.25 * t)
                if edge - y < 1.0:
                    col = lidcol * 0.45
                out[y, x, :3] = np.clip(col, 0, 255)
                out[y, x, 3] = 255
    return out


def mouth_spot(normal, happy, sad=None):
    """Where the mouth is: where the happy face (and the sad one, if it
    differs) changes the lower part of the head, as a centre and size; None
    if neither changes it."""
    def changed(other):
        if other is None or other.shape != normal.shape:
            return None
        d = (np.abs(normal - other)[:, :, :3].sum(axis=2) > 60) & (normal[:, :, 3] > 0)
        d[: int(d.shape[0] * 0.45)] = False
        return d if d.sum() >= 4 else None
    h, s_ = changed(happy), changed(sad)
    d = h & s_ if h is not None and s_ is not None and (h & s_).sum() >= 4 else (h if h is not None else s_)
    if d is None:
        return None
    ys, xs = np.nonzero(d)
    # the lowest band of change: a smile and a frown both live at the mouth line
    cut = np.percentile(ys, 40)
    sel = ys >= cut
    ys, xs = ys[sel], xs[sel]
    return (xs.mean(), ys.mean(), xs.max() - xs.min() + 1, ys.max() - ys.min() + 1)


def surprised(normal, happy, sad=None):
    out = normal.copy()
    m = mouth_spot(normal, happy, sad)
    if not m:
        return out
    cx, cy, w, h = m
    # a small mouth: a fraction of the head's width, not of all the happy face changes
    head = normal.shape[1]
    rw = rh = max(1.5, min(w * 0.18, head * 0.05))
    img = Image.fromarray(out.astype(np.uint8), 'RGBA')
    d = ImageDraw.Draw(img)
    # a small dark round mouth with a lighter lip around it
    lip = tuple(int(v) for v in np.median(normal[int(cy)][max(0, int(cx - w / 2)):int(cx + w / 2)], axis=0)[:3])
    d.ellipse([cx - rw - 1, cy - rh - 1, cx + rw + 1, cy + rh + 1], fill=lip + (255,))
    d.ellipse([cx - rw, cy - rh, cx + rw, cy + rh], fill=(52, 18, 22, 255))
    return np.array(img).astype(np.int16)


def dark_mouth(img, cx, cy, rw, rh, lip):
    """A small open mouth: a dark oval with a lip-coloured rim."""
    im = Image.fromarray(np.clip(img, 0, 255).astype(np.uint8), 'RGBA')
    d = ImageDraw.Draw(im)
    d.ellipse([cx - rw - 1, cy - rh - 1, cx + rw + 1, cy + rh + 1], fill=tuple(int(v) for v in lip) + (255,))
    d.ellipse([cx - rw, cy - rh, cx + rw, cy + rh], fill=(52, 18, 22, 255))
    return np.array(im).astype(np.int16)


def frown_corner(img, mx, my, toward_right):
    """Angry, from the side: the mouth's corner turned down, a short dark
    stroke from the mouth point back and down."""
    im = Image.fromarray(np.clip(img, 0, 255).astype(np.uint8), 'RGBA')
    d = ImageDraw.Draw(im)
    back = -1 if toward_right else 1
    d.line([(mx, my), (mx + back * 4, my + 2)], fill=(60, 22, 20, 255), width=1)
    return np.array(im).astype(np.int16)


def surprised_front(normal, sad, shut):
    """Surprised, from the front (owner): the sad face's mouth area as the
    base, an open mouth whose top is just above where the frown starts,
    and the neutral face's eyes."""
    out = sad.copy()
    if normal.shape != sad.shape:
        return out
    d = (np.abs(normal - sad)[:, :, :3].sum(axis=2) > 60) & (normal[:, :, 3] > 0)
    d[: int(d.shape[0] * 0.5)] = False
    ys, xs = np.nonzero(d)
    if len(xs) >= 4:
        top = ys.min()
        cx = xs.mean()
        w = xs.max() - xs.min() + 1
        rw = max(2.0, min(w * 0.22, sad.shape[1] * 0.05))
        rh = rw * 1.1
        lip = np.median(sad[min(top + 1, sad.shape[0] - 1)][max(0, int(cx - w / 2)):int(cx + w / 2) + 1], axis=0)[:3]
        out = dark_mouth(out, cx, top - 1 + rh, rw, rh, lip)
    # the neutral face's eyes
    em = eye_mask(normal, shut)
    out[em] = normal[em]
    return out


def build(ims, att=None):
    """The 120 frames with the missing expressions drawn. att: the head's
    body data rows (its second point is the mouth, for the side views)."""
    A = [unbox(arr(im)) for im in ims]
    out = list(A)
    for v in range(VIEWS):
        n_open, n_shut = A[v], A[10 + v]
        happy, sad = A[20 + v], A[40 + v]
        side = v < 8
        snout_right = v <= 3
        m = mouth_spot(n_open, happy, sad)
        toward = m[0] if m else n_open.shape[1] / 2
        if side:
            toward = n_open.shape[1] if snout_right else 0
        mouth = None
        if side and att and v < len(att) and len(att[v]) >= 4:
            mouth = (att[v][2], att[v][3])
        lip = np.median(n_open[n_open[:, :, 3] > 0][:, :3], axis=0) * 0.85
        sad_drawn = same(sad, n_open)
        if side:
            # the owner's marks (2026-11-11), from the side:
            # sad: the lid drooping toward the back of the head
            if sad_drawn:
                out[40 + v] = lidded(n_open, n_shut, 0.3, slant=-0.35, toward=toward, arch_amount=0.0)
            # angry: the lid slanting down toward the snout; the mouth's corner turned down
            if same(A[60 + v], n_open):
                a = lidded(n_open, n_shut, 0.3, slant=0.45, toward=toward, arch_amount=0.0)
                if mouth:
                    a = frown_corner(a, mouth[0], mouth[1], snout_right)
                out[60 + v] = a
            # surprised: a small open mouth at the mouth point
            sp = n_open.copy()
            if mouth:
                sp = dark_mouth(sp, mouth[0], mouth[1], 2.2, 2.0, lip)
            out[80 + v] = sp
            # sleepy: a flat lid across the middle of the eye
            out[100 + v] = lidded(n_open, n_shut, 0.5, arch_amount=0.0)
        else:
            if sad_drawn:
                out[40 + v] = lidded(n_open, n_shut, 0.22, slant=-0.3, toward=toward)
            out[100 + v] = lidded(n_open, n_shut, 0.5)
            if v == FRONT and not sad_drawn:
                out[80 + v] = surprised_front(n_open, sad, n_shut)
            else:
                out[80 + v] = surprised(n_open, happy, sad)
            if same(A[60 + v], n_open):
                base = sad if not sad_drawn else n_open
                out[60 + v] = lidded(base, n_shut, 0.12, slant=0.4, toward=toward)
    return [Image.fromarray(np.clip(a, 0, 255).astype(np.uint8), 'RGBA') for a in out]


def read_att(c2, name):
    d = os.path.join(c2, 'Body Data')
    for f in os.listdir(d):
        if f.lower() == name:
            return [list(map(int, l.split())) for l in open(os.path.join(d, f)) if l.strip()]
    return None


def preview(c2, slots, path):
    cell = 76
    rows = []
    for sl in slots:
        p = find(c2, f'a03{sl}.s16')
        if not p:
            continue
        before = s16.read(p)
        after = build(before, read_att(c2, f'a03{sl}.att'))
        rows.append((sl, before, after))
    W = 110 + cell * 2 * 4
    sheet = Image.new('RGBA', (W, 24 + len(rows) * 2 * cell), (40, 40, 48, 255))
    d = ImageDraw.Draw(sheet)
    for j, e in enumerate(['sad', 'angry', 'surprised', 'sleepy']):
        d.text((110 + j * 2 * cell + 4, 6), e, fill=(230, 230, 230, 255))
    for i, (sl, before, after) in enumerate(rows):
        for r, (label, ims) in enumerate([('now', before), ('drawn', after)]):
            y = 24 + (i * 2 + r) * cell
            d.text((6, y + 30), f'slot {sl} {label}', fill=(230, 230, 230, 255))
            for j, e in enumerate([2, 3, 4, 5]):
                for k, v in enumerate([FRONT, 0]):
                    im = ims[e * 20 + v]
                    sheet.paste(im, (110 + (j * 2 + k) * cell, y), im)
    sheet = sheet.resize((sheet.size[0] * 2, sheet.size[1] * 2), Image.NEAREST)
    sheet.save(path)


def preview_all(c2, slots, path):
    """Every angle of the drawn expressions (sad, angry, surprised, sleepy),
    eyes open, for each breed."""
    cell = 70
    sheets = []
    for sl in slots:
        p = find(c2, f'a03{sl}.s16')
        if not p:
            continue
        after = build(s16.read(p), read_att(c2, f'a03{sl}.att'))
        sh = Image.new('RGBA', (90 + cell * VIEWS, 20 + cell * 4), (40, 40, 48, 255))
        d = ImageDraw.Draw(sh)
        d.text((6, 4), f'slot {sl}', fill=(230, 230, 230, 255))
        for r, e in enumerate([2, 3, 4, 5]):
            d.text((6, 20 + r * cell + 28), EXPR[e], fill=(230, 230, 230, 255))
            for v in range(VIEWS):
                im = after[e * 20 + v]
                sh.paste(im, (90 + v * cell, 20 + r * cell), im)
        sheets.append(sh)
    W = max(s.size[0] for s in sheets)
    H = sum(s.size[1] for s in sheets)
    out = Image.new('RGBA', (W, H), (40, 40, 48, 255))
    y = 0
    for sh in sheets:
        out.paste(sh, (0, y))
        y += sh.size[1]
    out.save(path)


if __name__ == '__main__':
    c2 = sys.argv[1]
    slots = 'ag'
    if '--slots' in sys.argv:
        slots = sys.argv[sys.argv.index('--slots') + 1]
    if '--preview' in sys.argv:
        preview(c2, slots, sys.argv[sys.argv.index('--preview') + 1])
    if '--preview-all' in sys.argv:
        preview_all(c2, slots, sys.argv[sys.argv.index('--preview-all') + 1])
