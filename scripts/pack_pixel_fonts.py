#!/usr/bin/env python3
"""Packs the pixel fonts Zeetures draws in-world text with (speech bubbles,
the teaching computers' screens, notices) into its own small format, .zpf,
which the program carries built in (data/fonts; read by PixelText.cpp).

    pack_pixel_fonts.py SOURCE_DIR OUT_DIR

SOURCE_DIR holds the fonts as released (not kept in the repository):
  fusion-pixel-12px-proportional-ja.pcf, -10px-, -8px-   Fusion Pixel Font
      2026.11.10, TakWolf, OFL-1.1 (github.com/TakWolf/fusion-pixel-font,
      the 8, 10 and 12px "proportional pcf" release zips, Japanese build)
  unifont-18.0.01.hex.gz   GNU Unifont 18.0.01 (unifoundry.com), taken
      under the OFL-1.1 side of its dual licence
  6x13.bdf 6x10.bdf 5x8.bdf   misc-fixed, Markus Kuhn's ucs-fonts
      (cl.cam.ac.uk/~mgk25/ucs-fonts.html), public domain

The .zpf files are OFL "Modified Versions" of those fonts (Unifont's and
Fusion Pixel's parts): see data/fonts/LICENSES.

.zpf: "ZPFZ", the raw size (u32, little-endian), then the raw data
zlib-compressed. Raw: u8 ascent, u8 descent (the line's, in pixels), u16
0, u32 glyph count; then per glyph, in code point order, u32 code point,
i8 advance, i8 x offset, i8 top (of the bitmap, from the baseline: up is
negative), u8 width, u8 height, u8 flags (1: a combining mark, drawn over
the letter before it), u32 offset of its bitmap; then the bitmaps, a row
at a time, each row ceil(width / 8) bytes, the leftmost pixel in the top
bit.
"""
import gzip
import os
import struct
import sys
import unicodedata
import zlib


class PcfFont:
    def __init__(self, path):
        d = open(path, "rb").read()
        assert d[:4] == b"\x01fcp", path
        (n,) = struct.unpack_from("<i", d, 4)
        self.tables = {}
        for i in range(n):
            t, fmt, size, off = struct.unpack_from("<iiii", d, 8 + 16 * i)
            self.tables[t] = off
        self.d = d
        self._metrics()
        self._bitmaps()
        self._encodings()
        self._accel()

    def _hdr(self, t):
        off = self.tables[t]
        (fmt,) = struct.unpack_from("<i", self.d, off)
        return fmt, (">" if fmt & 4 else "<"), off + 4

    def _metrics(self):
        fmt, be, p = self._hdr(4)
        self.metrics = []
        if fmt & 0x100:
            (n,) = struct.unpack_from(be + "H", self.d, p)
            p += 2
            for _ in range(n):
                self.metrics.append(tuple(x - 0x80 for x in self.d[p:p + 5]))
                p += 5
        else:
            (n,) = struct.unpack_from(be + "i", self.d, p)
            p += 4
            for _ in range(n):
                self.metrics.append(struct.unpack_from(be + "hhhhh", self.d, p))
                p += 12

    def _bitmaps(self):
        fmt, be, p = self._hdr(8)
        (n,) = struct.unpack_from(be + "i", self.d, p)
        p += 4
        self.offsets = struct.unpack_from(be + "%di" % n, self.d, p)
        self.bm = p + 4 * n + 16
        self.pad = 1 << (fmt & 3)
        self.msbit = bool(fmt & 8)

    def _encodings(self):
        fmt, be, p = self._hdr(32)
        min2, max2, min1, max1, _ = struct.unpack_from(be + "hhhhh", self.d, p)
        p += 10
        cols, rows = max2 - min2 + 1, max1 - min1 + 1
        idx = struct.unpack_from(be + "%dH" % (cols * rows), self.d, p)
        self.map = {}
        for r in range(rows):
            for c in range(cols):
                if idx[r * cols + c] != 0xFFFF:
                    self.map[((r + min1) << 8) | (c + min2)] = idx[r * cols + c]

    def _accel(self):
        fmt, be, p = self._hdr(2)
        self.ascent, self.descent = struct.unpack_from(be + "ii", self.d, p + 8)

    def glyph(self, cp):
        g = self.map.get(cp)
        if g is None:
            return None
        lb, rb, w, a, de = self.metrics[g]
        gw, gh = rb - lb, a + de
        stride = ((gw + 7) // 8 + self.pad - 1) // self.pad * self.pad
        base = self.bm + self.offsets[g]
        rows = []
        for y in range(gh):
            row = []
            for x in range(gw):
                byte = self.d[base + y * stride + x // 8]
                row.append(bool((byte >> (7 - x % 8)) & 1 if self.msbit else (byte >> (x % 8)) & 1))
            rows.append(row)
        return [w, lb, -a, rows]


class BdfFont:
    def __init__(self, path):
        self.g = {}
        cur = None
        with open(path, encoding="latin-1") as f:
            for line in f:
                p = line.split()
                if not p:
                    continue
                if p[0] == "FONT_ASCENT":
                    self.ascent = int(p[1])
                elif p[0] == "FONT_DESCENT":
                    self.descent = int(p[1])
                elif p[0] == "STARTCHAR":
                    cur = {"rows": []}
                elif p[0] == "ENCODING":
                    cur["cp"] = int(p[1])
                elif p[0] == "DWIDTH":
                    cur["adv"] = int(p[1])
                elif p[0] == "BBX":
                    cur["bbx"] = tuple(int(x) for x in p[1:5])
                elif p[0] == "BITMAP":
                    cur["bm"] = True
                elif p[0] == "ENDCHAR":
                    w, h, xo, yo = cur["bbx"]
                    rows = []
                    for r in cur["rows"]:
                        v, nb = int(r, 16), len(r) * 4
                        rows.append([bool((v >> (nb - 1 - x)) & 1) for x in range(w)])
                    self.g[cur["cp"]] = [cur["adv"], xo, -(yo + h), rows]
                    cur = None
                elif cur and cur.get("bm"):
                    cur["rows"].append(p[0])

    def glyph(self, cp):
        g = self.g.get(cp)
        return [g[0], g[1], g[2], g[3]] if g else None


class HexFont:
    """Unifont: 16 rows, 8 or 16 wide, the baseline under row 14."""
    ascent, descent = 14, 2

    def __init__(self, path, keep):
        self.g = {}
        with gzip.open(path, "rt") as f:
            for line in f:
                cp, bits = line.strip().split(":")
                cp = int(cp, 16)
                if keep(cp):
                    self.g[cp] = bits

    def glyph(self, cp):
        bits = self.g.get(cp)
        if bits is None:
            return None
        w = len(bits) // 16 * 4
        rows = []
        for y in range(16):
            v = int(bits[y * w // 4:(y + 1) * w // 4], 16)
            rows.append([bool((v >> (w - 1 - x)) & 1) for x in range(w)])
        return [w, 0, -14, rows]


def trim(g, cp, proportional):
    """Crops a glyph to its ink; a monospace font's glyphs are given a
    proportional advance (ink + 1) to sit with Fusion Pixel's."""
    adv, xo, top, rows = g
    mark = unicodedata.category(chr(cp)) in ("Mn", "Me")
    h = len(rows)
    w = len(rows[0]) if rows else 0
    ys = [y for y in range(h) if any(rows[y])]
    xs = [x for x in range(w) if any(r[x] for r in rows)]
    if not ys:
        return [adv if proportional else max(3, adv // 2), 0, 0, [], 0]
    y0, y1, x0, x1 = ys[0], ys[-1] + 1, xs[0], xs[-1] + 1
    rows = [r[x0:x1] for r in rows[y0:y1]]
    if mark:
        return [0, 0, top + y0, rows, 1]
    if proportional:
        return [adv, xo + x0, top + y0, rows, 0]
    return [x1 - x0 + 1, 0, top + y0, rows, 0]


def write(path, ascent, descent, glyphs, space=None):
    entries, blob = [], bytearray()
    for cp in sorted(glyphs):
        adv, xo, top, rows, flags = glyphs[cp]
        if cp == 0x20 and space is not None:
            adv = space
        w, h = (len(rows[0]) if rows else 0), len(rows)
        off = len(blob)
        for r in rows:
            for b in range(0, w, 8):
                v = 0
                for i, on in enumerate(r[b:b + 8]):
                    if on:
                        v |= 0x80 >> i
                blob.append(v)
        entries.append(struct.pack("<IbbbBBBI", cp, adv, xo, top, w, h, flags, off))
    raw = struct.pack("<BBHI", ascent, descent, 0, len(entries)) + b"".join(entries) + bytes(blob)
    data = b"ZPFZ" + struct.pack("<I", len(raw)) + zlib.compress(raw, 9)
    open(path, "wb").write(data)
    print(f"{os.path.basename(path)}: {len(entries)} glyphs, {len(data) // 1024} KB")


def pack(font, keep, proportional):
    out = {}
    for cp in sorted(font.map if hasattr(font, "map") else font.g):
        if cp < 0x20 or not keep(cp):
            continue
        g = font.glyph(cp)
        if g:
            out[cp] = trim(g, cp, proportional)
    return out


def main():
    src, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    allcp = lambda cp: True  # noqa: E731
    # Fusion Pixel, whole (word spaces a pixel or two narrower, nearer the game's own)
    for size, space in ((12, 5), (10, 4), (8, 3)):
        f = PcfFont(f"{src}/fusion-pixel-{size}px-proportional-ja.pcf")
        write(f"{out}/fusion-{size}.zpf", f.ascent, f.descent, pack(f, allcp, True), space)
    # what Fusion Pixel lacks, smaller than Unifont where a font has it
    hebrew = lambda cp: 0x0590 <= cp <= 0x05FF or 0xFB1D <= cp <= 0xFB4F  # noqa: E731
    thai = lambda cp: 0x0E00 <= cp <= 0x0E7F  # noqa: E731
    for name, keep in (("6x10", hebrew), ("5x8", hebrew), ("6x13", thai)):
        f = BdfFont(f"{src}/{name}.bdf")
        write(f"{out}/fixed-{name}.zpf", f.ascent, f.descent, pack(f, keep, False))

    # GNU Unifont, the last resort: the alphabets and marks above, and Arabic
    def unikeep(cp):
        return (0x0100 <= cp <= 0x06FF or 0x0E00 <= cp <= 0x0E7F or 0x1E00 <= cp <= 0x1FFF
                or 0xFB1D <= cp <= 0xFDFF or 0xFE70 <= cp <= 0xFEFF)
    u = HexFont(f"{src}/unifont-18.0.01.hex.gz", unikeep)
    write(f"{out}/unifont-part.zpf", u.ascent, u.descent, pack(u, unikeep, False))


if __name__ == "__main__":
    main()
