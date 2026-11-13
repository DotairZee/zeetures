"""Generates the symbols for creatures' thought bubbles (owner, 2026-11-13:
the "pictures" style): our own symbols for needs, directions and the like.
The bubble itself is C2's own thought bubble, drawn from the player's game
(syst.s16, as for the hand's typing). All drawn from scratch for openc2e (no game data;
pictures of things and creatures come from the player's own game as it
runs). The engine embeds the PNGs this writes (CMake,
scripts/embed_files.py); rerun this after changing it:

    python3 data/thoughts/make_art.py data/thoughts

Writes icons.png (one 64 x 64 cell per symbol, in the order of ICONS,
which src/openc2e/creatures/Thoughts.cpp repeats; shown at about 20 px).
Needs Pillow."""
import math, os, sys
from PIL import Image, ImageDraw, ImageFont

ICONS = ["hunger", "nose", "question", "heart", "feet", "sun", "snow", "zzz", "home", "people", "cross", "bang",
         "fear", "arrow_w", "arrow_e", "arrow_up", "arrow_down", "pain", "injury", "breath", "tired"]
BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"


def font(sz):
    try:
        return ImageFont.truetype(BOLD, sz)
    except OSError:
        return ImageFont.load_default()


def icon(kind, size=64):
    K = 4
    s = size * K
    im = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    def arrow(angle, col=(70, 150, 80)):
        a = Image.new("RGBA", (s, s), (0, 0, 0, 0))
        ImageDraw.Draw(a).polygon([(s*0.08, s*0.5), (s*0.45, s*0.16), (s*0.45, s*0.38), (s*0.92, s*0.38), (s*0.92, s*0.62),
                                   (s*0.45, s*0.62), (s*0.45, s*0.84)], fill=col)
        return a.rotate(angle, resample=Image.BICUBIC)
    if kind == "hunger":  # an apple
        d.ellipse([s*0.14, s*0.26, s*0.86, s*0.92], fill=(214, 60, 50))
        d.ellipse([s*0.26, s*0.36, s*0.42, s*0.52], fill=(240, 120, 110))
        d.rectangle([s*0.47, s*0.1, s*0.53, s*0.32], fill=(110, 70, 40))
        d.ellipse([s*0.52, s*0.08, s*0.84, s*0.26], fill=(80, 160, 70))
    elif kind == "nose":  # a sniffing nose and wafts
        d.ellipse([s*0.44, s*0.4, s*0.9, s*0.84], fill=(230, 160, 140))
        d.ellipse([s*0.56, s*0.62, s*0.66, s*0.72], fill=(150, 90, 80))
        for k in range(3):
            d.arc([s*(0.02+k*0.1), s*(0.06+k*0.07), s*(0.44+k*0.1), s*(0.5+k*0.07)], 190, 300, fill=(110, 165, 100), width=int(s*0.06))
    elif kind == "question":
        d.text((s*0.26, -s*0.04), "?", font=font(int(s*0.95)), fill=(90, 90, 120))
    elif kind == "bang":
        d.text((s*0.36, -s*0.04), "!", font=font(int(s*0.95)), fill=(220, 130, 40))
    elif kind == "heart":
        r = s * 0.24
        d.ellipse([s*0.14, s*0.18, s*0.14+2*r, s*0.18+2*r], fill=(226, 70, 120))
        d.ellipse([s*0.86-2*r, s*0.18, s*0.86, s*0.18+2*r], fill=(226, 70, 120))
        d.polygon([(s*0.16, s*0.46), (s*0.84, s*0.46), (s*0.5, s*0.9)], fill=(226, 70, 120))
    elif kind == "feet":
        for (cx, cy, a) in [(0.34, 0.6, -15), (0.66, 0.38, 15)]:
            e = Image.new("RGBA", (s, s), (0, 0, 0, 0))
            ed = ImageDraw.Draw(e)
            ed.ellipse([s*0.41, s*0.3, s*0.59, s*0.66], fill=(120, 90, 60))
            for t in range(3):
                ed.ellipse([s*(0.4+t*0.07), s*0.18, s*(0.46+t*0.07), s*0.26], fill=(120, 90, 60))
            e = e.rotate(a, center=(s*0.5, s*0.45))
            im.alpha_composite(e, (int((cx-0.5)*s), int((cy-0.45)*s)))
    elif kind == "sun":
        d.ellipse([s*0.28, s*0.28, s*0.72, s*0.72], fill=(245, 175, 40))
        for k in range(8):
            a = k * math.pi / 4
            d.line([s*0.5 + math.cos(a)*s*0.33, s*0.5 + math.sin(a)*s*0.33, s*0.5 + math.cos(a)*s*0.47, s*0.5 + math.sin(a)*s*0.47],
                   fill=(245, 175, 40), width=int(s*0.07))
    elif kind == "snow":
        for k in range(3):
            a = k * math.pi / 3
            d.line([s*0.5 - math.cos(a)*s*0.42, s*0.5 - math.sin(a)*s*0.42, s*0.5 + math.cos(a)*s*0.42, s*0.5 + math.sin(a)*s*0.42],
                   fill=(80, 150, 220), width=int(s*0.08))
            for sgn in (-1, 1):
                bx, by = s*0.5 + sgn*math.cos(a)*s*0.28, s*0.5 + sgn*math.sin(a)*s*0.28
                for da in (0.7, -0.7):
                    d.line([bx, by, bx + sgn*math.cos(a+da)*s*0.12, by + sgn*math.sin(a+da)*s*0.12], fill=(80, 150, 220), width=int(s*0.05))
    elif kind == "zzz":
        d.text((s*0.04, s*0.38), "z", font=font(int(s*0.36)), fill=(80, 110, 200))
        d.text((s*0.34, s*0.16), "z", font=font(int(s*0.46)), fill=(80, 110, 200))
        d.text((s*0.64, -s*0.08), "z", font=font(int(s*0.56)), fill=(80, 110, 200))
    elif kind == "home":
        d.polygon([(s*0.1, s*0.5), (s*0.5, s*0.12), (s*0.9, s*0.5)], fill=(190, 90, 60))
        d.rectangle([s*0.2, s*0.5, s*0.8, s*0.9], fill=(235, 205, 150))
        d.rectangle([s*0.42, s*0.62, s*0.58, s*0.9], fill=(140, 90, 50))
    elif kind == "people":
        for (cx, col) in [(0.32, (200, 130, 90)), (0.68, (150, 110, 190))]:
            d.ellipse([s*(cx-0.13), s*0.14, s*(cx+0.13), s*0.4], fill=col)
            d.pieslice([s*(cx-0.21), s*0.44, s*(cx+0.21), s*1.02], 180, 360, fill=col)
    elif kind == "cross":
        d.line([s*0.2, s*0.2, s*0.8, s*0.8], fill=(210, 60, 50), width=int(s*0.14))
        d.line([s*0.8, s*0.2, s*0.2, s*0.8], fill=(210, 60, 50), width=int(s*0.14))
    elif kind == "fear":  # a frightened face
        d.ellipse([s*0.1, s*0.1, s*0.9, s*0.9], fill=(150, 120, 200))
        d.ellipse([s*0.28, s*0.3, s*0.42, s*0.48], fill=(255, 255, 255))
        d.ellipse([s*0.58, s*0.3, s*0.72, s*0.48], fill=(255, 255, 255))
        d.ellipse([s*0.32, s*0.36, s*0.38, s*0.44], fill=(40, 30, 60))
        d.ellipse([s*0.62, s*0.36, s*0.68, s*0.44], fill=(40, 30, 60))
        d.ellipse([s*0.4, s*0.58, s*0.6, s*0.8], fill=(60, 40, 80))
    elif kind == "arrow_w":
        im.alpha_composite(arrow(0))
    elif kind == "arrow_e":
        im.alpha_composite(arrow(180))
    elif kind == "arrow_up":
        im.alpha_composite(arrow(-90))
    elif kind == "arrow_down":
        im.alpha_composite(arrow(90))
    elif kind == "pain":  # a jagged burst
        pts = []
        for k in range(16):
            a = k * math.pi / 8
            r = s * (0.46 if k % 2 == 0 else 0.24)
            pts.append((s*0.5 + math.cos(a)*r, s*0.5 + math.sin(a)*r))
        d.polygon(pts, fill=(230, 80, 50))
        d.ellipse([s*0.38, s*0.38, s*0.62, s*0.62], fill=(255, 210, 90))
    elif kind == "injury":  # a sticking plaster
        p = Image.new("RGBA", (s, s), (0, 0, 0, 0))
        pd = ImageDraw.Draw(p)
        pd.rounded_rectangle([s*0.08, s*0.36, s*0.92, s*0.64], int(s*0.12), fill=(235, 190, 150))
        pd.rectangle([s*0.38, s*0.36, s*0.62, s*0.64], fill=(250, 235, 215))
        for i in range(2):
            for j in range(2):
                pd.ellipse([s*(0.43+i*0.1), s*(0.42+j*0.1), s*(0.47+i*0.1), s*(0.46+j*0.1)], fill=(210, 170, 140))
        im.alpha_composite(p.rotate(35, resample=Image.BICUBIC))
    elif kind == "breath":  # bubbles rising
        for (cx, cy, r) in [(0.36, 0.72, 0.17), (0.62, 0.44, 0.13), (0.42, 0.2, 0.09)]:
            d.ellipse([s*(cx-r), s*(cy-r), s*(cx+r), s*(cy+r)], outline=(70, 140, 210), width=int(s*0.05))
            d.ellipse([s*(cx-r*0.5), s*(cy-r*0.6), s*(cx-r*0.1), s*(cy-r*0.2)], fill=(200, 230, 250))
    elif kind == "tired":  # a battery running low
        d.rounded_rectangle([s*0.1, s*0.3, s*0.82, s*0.7], int(s*0.06), outline=(80, 80, 100), width=int(s*0.06))
        d.rectangle([s*0.82, s*0.42, s*0.9, s*0.58], fill=(80, 80, 100))
        d.rectangle([s*0.17, s*0.37, s*0.3, s*0.63], fill=(220, 70, 50))
    return im.resize((size, size), Image.LANCZOS)


def main(out):
    strip = Image.new("RGBA", (64 * len(ICONS), 64), (0, 0, 0, 0))
    for i, k in enumerate(ICONS):
        strip.alpha_composite(icon(k), (64 * i, 0))
    strip.save(os.path.join(out, "icons.png"))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.abspath(__file__)))
