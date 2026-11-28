"""The status bar's weather pictures (owner, 2026-11-27: "style A", a
painted strip like C2's season picture, between the time of day and the
temperature): our own art, drawn here, written to data/statusbar/weather.png
and built into the program (C2StatusBar.cpp).

    python3 statusbar_weather.py ../data/statusbar/weather.png

48 x 16 pictures stacked top to bottom, in the order of Weather::Spell
(clear, light showers, showers, thunderstorm, torrential rain, fog, snow,
heatwave, wind storm), then the thunderstorm with hail.
"""
import math
import random
import sys
import numpy as np
from PIL import Image, ImageDraw, PngImagePlugin

W, H, S = 48, 16, 4  # a picture's size; drawn 4 times as big, then shrunk

def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def vgrad(w, h, top, bottom):
    im = Image.new('RGB', (w, h))
    d = ImageDraw.Draw(im)
    for y in range(h):
        d.line([(0, y), (w, y)], fill=lerp(top, bottom, y / max(1, h - 1)))
    return im


def hgrad(w, h, left, right):
    im = Image.new('RGB', (w, h))
    d = ImageDraw.Draw(im)
    for x in range(w):
        d.line([(x, 0), (x, h)], fill=lerp(left, right, x / max(1, w - 1)))
    return im


def dither_mask(w, h, a, ox=0, oy=0):
    B = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0
    ys, xs = np.mgrid[0:h, 0:w]
    return B[(ys + oy) % 4, (xs + ox) % 4] < a


# ---- shared little shapes (drawn at S x) ----------------------------------

def cloud(d, cx, cy, r, fill, outline=None):
    for dx, dy, rr in ((-1.2, 0.25, 0.75), (0, -0.2, 1.0), (1.2, 0.25, 0.7), (0.6, 0.35, 0.8), (-0.5, 0.4, 0.8)):
        x, y, q = cx + dx * r, cy + dy * r, rr * r
        d.ellipse([x - q, y - q, x + q, y + q], fill=fill)


def sun(d, cx, cy, r, core=(255, 214, 60), rays=(255, 190, 40)):
    for i in range(12):
        t = i * math.pi / 6
        d.line([(cx + math.cos(t) * r * 1.25, cy + math.sin(t) * r * 1.25), (cx + math.cos(t) * r * 1.8, cy + math.sin(t) * r * 1.8)], fill=rays, width=S)
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=core)


def bolt(d, x, y, s, fill=(255, 236, 110)):
    pts = [(x, y), (x + 3 * s, y), (x + 1.5 * s, y + 4 * s), (x + 3.5 * s, y + 4 * s), (x - 0.5 * s, y + 10 * s), (x + 0.8 * s, y + 5.5 * s), (x - 1 * s, y + 5.5 * s)]
    d.polygon(pts, fill=fill)


def flake(d, cx, cy, r, fill=(255, 255, 255)):
    for i in range(3):
        t = i * math.pi / 3
        d.line([(cx - math.cos(t) * r, cy - math.sin(t) * r), (cx + math.cos(t) * r, cy + math.sin(t) * r)], fill=fill, width=S)


# ---- style A: a painted strip of sky and land, as C2's season block --------

def style_a(name):
    rng = random.Random(name)
    skies = {
        'Clear': ((70, 150, 235), (170, 215, 250)),
        'Light showers': ((120, 160, 205), (185, 205, 225)),
        'Showers': ((95, 120, 160), (150, 165, 185)),
        'Thunderstorm': ((45, 45, 75), (95, 90, 120)),
        'Thunderstorm with hail': ((50, 55, 85), (110, 110, 140)),
        'Torrential rain': ((60, 70, 95), (100, 110, 130)),
        'Fog': ((175, 180, 185), (205, 205, 205)),
        'Snow': ((150, 165, 195), (215, 222, 235)),
        'Heatwave': ((235, 150, 60), (255, 220, 120)),
        'Wind storm': ((185, 150, 95), (225, 195, 140)),
    }
    top, bot = skies[name]
    im = vgrad(W * S, H * S, top, bot)
    d = ImageDraw.Draw(im)
    ground = (60, 150, 60)
    if name == 'Snow':
        ground = (240, 244, 250)
    if name in ('Heatwave', 'Wind storm'):
        ground = (210, 175, 100)
    # clouds
    nc = {'Clear': 1, 'Light showers': 3, 'Showers': 4, 'Thunderstorm': 5, 'Thunderstorm with hail': 5, 'Torrential rain': 6, 'Fog': 0, 'Snow': 4, 'Heatwave': 0, 'Wind storm': 2}[name]
    cc = {'Thunderstorm': (70, 70, 95), 'Thunderstorm with hail': (85, 85, 110), 'Torrential rain': (90, 95, 115), 'Showers': (150, 155, 170), 'Snow': (225, 228, 238)}.get(name, (245, 245, 250))
    for i in range(nc):
        cloud(d, (6 + i * 9 + rng.uniform(-2, 2)) * S, (3.2 + rng.uniform(-0.6, 0.6)) * S, 2.3 * S, cc)
    if name == 'Clear':
        sun(d, 40 * S, 5 * S, 2.6 * S)
    if name == 'Heatwave':
        sun(d, 40 * S, 5 * S, 3.4 * S, core=(255, 245, 150), rays=(255, 120, 30))
    # falling things
    def streaks(n, length, slant, col, w=1):
        for _ in range(n):
            x, y = rng.uniform(0, W), rng.uniform(5, 13)
            d.line([(x * S, y * S), ((x - slant * length) * S, (y - length) * S)], fill=col, width=w * S // 2)
    if name == 'Light showers':
        streaks(10, 2.0, 0.2, (210, 225, 245))
    if name == 'Showers':
        streaks(20, 2.5, 0.25, (210, 222, 240))
    if name in ('Thunderstorm', 'Thunderstorm with hail'):
        streaks(22, 2.8, 0.3, (190, 200, 225))
        bolt(d, 29 * S, 3 * S, 0.9 * S)
    if name == 'Thunderstorm with hail':
        for _ in range(9):
            x, y = rng.uniform(1, 47), rng.uniform(6, 13)
            d.ellipse([(x - 0.7) * S, (y - 0.7) * S, (x + 0.7) * S, (y + 0.7) * S], fill=(240, 245, 255))
    if name == 'Torrential rain':
        streaks(45, 3.5, 0.35, (175, 190, 215))
    if name == 'Snow':
        for _ in range(16):
            x, y = rng.uniform(1, 47), rng.uniform(5, 13)
            d.ellipse([(x - 0.6) * S, (y - 0.6) * S, (x + 0.6) * S, (y + 0.6) * S], fill=(255, 255, 255))
    if name == 'Fog':
        for i, y in enumerate((4, 7.5, 11)):
            d.rounded_rectangle([(2 + i * 4) * S, (y - 0.9) * S, (40 + i * 3) * S, (y + 0.9) * S], radius=S, fill=(235, 235, 235))
    if name == 'Wind storm':
        for _ in range(14):
            x, y = rng.uniform(0, 44), rng.uniform(3, 12)
            d.line([(x * S, y * S), ((x + rng.uniform(4, 9)) * S, y * S)], fill=(240, 220, 170), width=S // 2 + 1)
        for x, y in ((12, 6), (30, 9), (38, 4)):
            d.ellipse([x * S, y * S, (x + 1.6) * S, (y + 0.9) * S], fill=(95, 140, 50))
    if name == 'Heatwave':
        for x in range(4, 30, 6):
            for k in range(8):
                y0 = 7 + k * 0.6
                d.point([((x + math.sin(k) * 0.8) * S, y0 * S)], fill=(255, 235, 190))
    # the ground with flowers or tufts, as C2's season picture
    d.rectangle([0, 13 * S, W * S, H * S], fill=ground)
    for _ in range(10):
        x = rng.uniform(0, W)
        col = (240, 240, 245) if name == 'Snow' else (rng.choice([(250, 120, 160), (255, 230, 90), (255, 255, 255)]) if name in ('Clear', 'Light showers') else (80, 120, 60))
        d.point([(x * S, 14 * S)], fill=col)
    return im.resize((W, H), Image.LANCZOS)



ORDER = ['Clear', 'Light showers', 'Showers', 'Thunderstorm', 'Torrential rain', 'Fog', 'Snow',
         'Heatwave', 'Wind storm', 'Thunderstorm with hail']


def main():
    frames = [style_a(n).convert('RGBA') for n in ORDER]
    sheet = Image.new('RGBA', (W, H * len(frames)))
    for i, f in enumerate(frames):
        sheet.paste(f, (0, i * H))
    info = PngImagePlugin.PngInfo()
    info.add_text('openc2e-frames', str(len(frames)))
    sheet.save(sys.argv[1], pnginfo=info)
    print('wrote', sys.argv[1], len(frames), 'pictures')


if __name__ == '__main__':
    main()
