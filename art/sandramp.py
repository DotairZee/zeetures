"""Draw an original sand ramp that opens the Gelsemium pit down into the
glass observation tube below it, in the style of the tube's own cool,
blue-lit sand.

Everything here is drawn from scratch (no game pixels are read or copied),
so the output can ship with the project. Colors are numbers chosen by eye
to sit with the surrounding scene.

    python3 sandramp.py OUT_BASENAME

writes OUT_BASENAME.png (for viewing) and OUT_BASENAME.s16 (for the game).
The sprite is placed at world x 1205, y 950 (see bridges.cos).
"""
import sys
import numpy as np
from PIL import Image, ImageDraw
import s16
from catwalk import _noise

SS = 4
X0, Y0 = 1205, 950          # sprite origin in the world
W, H = 286, 169             # x 1205-1491, y 950-1119

# world geometry (must match the floor line set in bridges.cos)
# the walking line up the ramp (owner's sketch, 2026-11-08): it leaves the
# tube's dune gently and steepens toward the east walkway (bridges.cos sets
# the same floor points)
RAMP_PTS = [(1211, 1062), (1258, 1058), (1290, 1054), (1328, 1041), (1370, 1015), (1412, 987), (1440, 964)]
RAMP_FOOT = RAMP_PTS[2]     # where the slope leaves the front dune
RAMP_TOP = RAMP_PTS[-1]     # where it meets the east walkway
TUBE_FLOOR = 1100
# The tube's own dune, read by eye from the scene: its pale top line, and
# the brown earth hump it rests on. Creatures walk along the dune's front
# (y 1100), so the ramp's sand only shows where the ramp rises above the
# dune's top; below that the dune itself carries on.
# (the front dune: the lower, nearer one, whose top edge rises from y 1064
# at x 1180 to 1055 at x 1300; the pale dune behind it ends near x 1265,
# and the ramp grows out of the front one only)
DUNE_X = [1180, 1220, 1260, 1280, 1300, 1340, 1360, 1380, 1395, 1430]
DUNE_Y = [1064, 1061, 1058, 1057, 1055, 1056, 1058, 1060, 1064, 1064]
DUNE_RGB_X = [1240, 1280, 1310, 1340, 1370, 1385]   # its top edge, left to right
DUNE_RGB = [(96, 104, 90), (92, 106, 96), (104, 112, 96), (96, 104, 88), (100, 104, 84), (94, 96, 76)]
EARTH_X = [1240, 1260, 1270, 1290, 1300, 1310, 1320, 1340, 1360, 1380, 1398]
EARTH_Y = [1093, 1087, 1084, 1083, 1084, 1086, 1089, 1091, 1093, 1095, 1097]
# under the ramp's east part, the line where the tube's glass floor meets
# the brown earth: the sand fills down to it, covering the glass floor
UNDER_X = [1340, 1350, 1360, 1380, 1390, 1400, 1410, 1440, 1445, 1450, 1455, 1460, 1465, 1470, 1475, 1490]
UNDER_Y = [1096, 1097, 1098, 1099, 1105, 1114, 1116, 1116, 1112, 1108, 1104, 1100, 1095, 1089, 1083, 1080]
# blends into the dune: the ramp's toe fades in over TOE, and the skirt
# over the dune's front fades in over SKIRT (x ranges)
TOE = (1272, 1318)
SKIRT = (1290, 1385)
# the brass band standing in the tube under the ramp, buried to its foot
BAND_X, BAND_FOOT = (1399, 1427), 1111
# the tube's rounded glass east end out to the brown rock behind it: the
# sand fills it like a container, so its east side takes that curve
TUBE_END_X, TUBE_END_Y, TUBE_END_R = 1438, 1056, (50, 54)
# its upper right reaches further out than that ellipse: the brown edge
# traced by eye (y, x), so no blue glass shows between sand and rock
DOME_UPPER = [(1001, 1452), (1002, 1458), (1004, 1463), (1008, 1471), (1012, 1475), (1016, 1479),
              (1020, 1482), (1026, 1484), (1032, 1486), (1040, 1488), (1056, 1489)]
TUBE_ROOF = 1006            # underside of the walkway slab, inside the tube
ROCK_UNDERSIDE = 1003       # under the east walkway rock, past the ramp top
OPEN_LEFT = 1211            # the opening's west edge (the alcove's lip)


def walkway_y(x):
    """The old walkway floor line across the opening (rooms 202 and 201)."""
    return 955 + (x - 1211) * 5 / 123 if x < 1334 else 960 + (x - 1334) * 8 / 219


def ramp_y(x):
    return float(np.interp(x, [p[0] for p in RAMP_PTS], [p[1] for p in RAMP_PTS]))


SAND_LIT = np.array([128, 136, 112], np.float32)   # the dune's lit crest is about 130,139,118
SAND = np.array([92, 100, 84], np.float32)
CAVITY = np.array([40, 54, 54], np.float32)   # the tube's back wall, about 35,48,48
CAVITY_DEEP = np.array([18, 24, 24], np.float32)
MATTE = np.array([30, 38, 36], np.float32)


def draw():
    w, h = W * SS, H * SS
    xs = (np.arange(w, dtype=np.float32) + 0.5) / SS + X0
    ys = (np.arange(h, dtype=np.float32) + 0.5) / SS + Y0
    X, Y = np.meshgrid(xs, ys)
    rampY = np.vectorize(ramp_y)(xs)[None, :]
    walkY = np.vectorize(walkway_y)(xs)[None, :]
    rng = np.random.default_rng(3)

    def up(m):
        return np.kron(m, np.ones((SS, SS), np.float32))
    mottle = up(0.10 * _noise((H, W), 21, 7) + 0.06 * _noise((H, W), 22, 3))
    grain = up(rng.normal(0, 0.05, (H, W)).astype(np.float32))

    rgb = np.zeros((h, w, 3), np.float32)
    cover = np.zeros((h, w), np.float32)

    # the opening: dark rock seen down the hole, from the old walkway line to
    # the tube roof, above the sand; darkest just under the lip
    lipY = walkY + 1.0 + 1.6 * np.abs(up(_noise((H, W), 25, 3)))   # broken top edge
    bottomY = TUBE_ROOF + 4 + 5 * up(_noise((H, W), 26, 6))          # ragged, fades out
    open_mask = (X >= OPEN_LEFT) & (X <= RAMP_TOP[0] + 2) & (Y >= lipY) & (Y <= bottomY + 6) & (Y < rampY)
    depth = np.clip((Y - walkY) / (TUBE_ROOF + 8 - walkY), 0, 1)
    cav = CAVITY_DEEP + (CAVITY - CAVITY_DEEP) * depth[..., None] ** 1.3
    # rock relief: blotches and cracks, lit from above
    relief = up(_noise((H, W), 27, 4))
    fine = up(0.22 * _noise((H, W), 29, 1) + rng.normal(0, 0.16, (H, W)).astype(np.float32))
    flecks = up((rng.random((H, W)) < 0.05).astype(np.float32) * rng.uniform(0.4, 0.9, (H, W)).astype(np.float32))
    rock = 1 + 2.4 * mottle + 0.7 * (relief - np.roll(relief, SS, 0)) + fine + flecks
    cav = cav * rock[..., None]
    # a shadow band right under the lip
    cav *= (0.45 + 0.55 * np.clip((Y - lipY) / 5.0, 0, 1))[..., None]
    rgb[open_mask] = cav[open_mask]
    cover[open_mask] = np.clip((bottomY + 6 - Y) / 6.0, 0, 1)[open_mask]
    # the tube's glass now ends at the middle brass band: paint the glass
    # reflection just west of the ramp over with the tube's back wall,
    # feathered into the wall around it
    gl = np.sqrt(((X - 1250) / 58) ** 2 + ((Y - 1023) / 15) ** 2)
    gk = np.clip((1.0 - gl) / 0.35, 0, 1)
    gk = gk * gk * (3 - 2 * gk)
    wall = CAVITY * rock[..., None] * 0.92
    gsel = gk > cover
    rgb[gsel] = wall[gsel]
    cover[gsel] = gk[gsel]
    # the cut face of the east walkway's rock, above the drift
    face = (X >= RAMP_TOP[0] - 0.5) & (X < RAMP_TOP[0] + 4 + 1.5 * up(_noise((H, W), 28, 3))) & (Y > RAMP_TOP[1] + 1) & (Y < ROCK_UNDERSIDE + 2)
    rgb[face] = np.array([40, 32, 20], np.float32) * (1 + 2 * mottle[face][:, None])
    cover[face] = 1
    # the alcove's broken lip: a rough stone face on the west edge
    lip = open_mask & (X < OPEN_LEFT + 2.5 + 1.2 * up(_noise((H, W), 23, 4)))
    rgb[lip] = np.array([58, 64, 54], np.float32) * (1 + mottle[lip][:, None] * 2)
    # the cut end of the tube's glass roof, just inside the west edge
    glass = (X >= OPEN_LEFT) & (X < OPEN_LEFT + 4) & (Y > TUBE_ROOF - 3) & (Y < TUBE_ROOF + 1)
    rgb[glass] = np.array([92, 122, 128], np.float32)
    cover[glass] = 1

    # the sand: the tube's dune built up into a ramp. Its top is the ramp
    # line (and, past the ramp's top, the rock it is packed up against); its
    # bottom is the dune's top, except over the brass band, which it buries
    # to the foot, and in the dome, which it fills out to the brown rock
    duneY = np.interp(xs, DUNE_X, DUNE_Y)[None, :]
    earthY = np.interp(xs, EARTH_X, EARTH_Y)[None, :]
    dome_dx = np.clip(1 - ((X - TUBE_END_X) / TUBE_END_R[0]) ** 2, 0, 1)
    dome_top = TUBE_END_Y - TUBE_END_R[1] * np.sqrt(dome_dx)
    dome_bot = TUBE_END_Y + TUBE_END_R[1] * np.sqrt(dome_dx)
    in_dome = (X > TUBE_END_X - 12) & (dome_dx > 0)
    upper_x = np.interp(Y, [p[0] for p in DOME_UPPER], [p[1] for p in DOME_UPPER])
    upper = (X > TUBE_END_X) & (Y >= DOME_UPPER[0][0]) & (Y <= TUBE_END_Y) & (X <= upper_x + 1)
    top = np.where(X <= RAMP_TOP[0], rampY, np.where(upper, DOME_UPPER[0][0], np.maximum(ROCK_UNDERSIDE + 1, dome_top)))
    # overlaps the front dune's top edge, tapering to nothing at the toe
    bottom = duneY + 5 * np.clip((X - TOE[0]) / 24, 0, 1)
    # beside the band the sand heaps down over the dune's front to rest on
    # the earth, and curls just past it onto the band's foot
    # the dune's front, under the ramp's lower half, is skirted with the
    # same sand down to the earth, fading in from the west (see `soft`)
    underY = np.interp(xs, UNDER_X, UNDER_Y)[None, :]
    floorY = np.where(X < UNDER_X[0], earthY, underY)
    skirt = (X >= SKIRT[0]) & (X < BAND_X[0])
    bottom = np.where(skirt, floorY + 1, bottom)
    on_band = (X >= BAND_X[0]) & (X <= BAND_X[1])
    bottom = np.where(on_band, np.maximum(BAND_FOOT, underY), bottom)
    bottom = np.where(in_dome & (X > BAND_X[1]), underY, bottom)
    sand_mask = (X > TOE[0]) & (Y >= top) & (Y <= bottom) & ((X <= TUBE_END_X) | (dome_dx > 0) | upper)
    # a little spill over the walkway rock at the ramp's top
    spill = (X > RAMP_TOP[0]) & (X < RAMP_TOP[0] + 12)
    spill_top = RAMP_TOP[1] + (X - RAMP_TOP[0]) * 0.18 + (X - RAMP_TOP[0]) ** 2 * 0.02
    spill_zone = spill & (Y <= RAMP_TOP[1] + 4)
    sand_mask |= spill_zone & (Y >= spill_top)

    # shading: the same smooth, softly lit sand as the dune. Where it meets
    # the dune it takes the dune's own top color; up the slope it catches
    # the light along its surface; toward the walkway it falls into the
    # hole's shadow, and under the walkway rock deeper still
    dune_rgb = np.stack([np.interp(xs, DUNE_RGB_X, [c[i] for c in DUNE_RGB]) for i in range(3)], -1)[None]
    surf = np.where(spill_zone, spill_top, np.where(X <= RAMP_TOP[0], rampY, RAMP_TOP[1]))
    d = Y - surf
    span = np.clip(duneY - surf, 8, None)
    t = np.clip(d / span, 0, 1)
    # near the toe the new sand is the dune itself: its color all the way up,
    # so the slope rises out of the dune with no edge (no dithering here)
    t = np.maximum(t, 1 - np.clip((X - TOE[0]) / (TOE[1] - TOE[0]), 0, 1))[..., None]
    body = SAND * (1 - t) + dune_rgb * t
    lit = (np.clip(1 - d / 6.0, 0, 1) ** 1.5)[..., None]
    lit = np.where(((X > RAMP_TOP[0]) & ~spill_zone)[..., None], lit * 0.3, lit)
    lit = lit * np.clip((X - TOE[0]) / 40, 0, 1)[..., None]   # no bright point at the toe
    shade = body * (1 - lit) + SAND_LIT * lit
    climb = np.clip((X - 1260) / (RAMP_TOP[0] - 1260), 0, 1.4)
    shade = shade * (1.04 - 0.30 * climb * (1 - t[..., 0]))[..., None]
    under = np.clip((X - (RAMP_TOP[0] - 25)) / 30, 0, 1) * np.clip(1 - (Y - ROCK_UNDERSIDE) / 45, 0, 1)
    under = np.where(spill_zone, 0, under)
    shade = shade * (1 - 0.45 * under)[..., None]
    # below the dune line (over the band, in the dome) the sand is the
    # dune's lower body: a little darker toward the floor
    low = np.clip((Y - duneY) / 25, 0, 1)
    shade = shade * (1 - 0.28 * low)[..., None]
    # the dome's sand is rounded by its container: darker toward the glass
    rad = np.sqrt(((X - TUBE_END_X) / TUBE_END_R[0]) ** 2 + ((Y - TUBE_END_Y) / TUBE_END_R[1]) ** 2)
    shade = shade * (1 - 0.30 * np.clip((X - (TUBE_END_X - 45)) / 40, 0, 1) * (np.clip(rad - 0.45, 0, 1) / 0.55) ** 2)[..., None]
    # the underside: darker where the sand rests on the tube's floor
    rest = np.clip((X - SKIRT[0]) / (SKIRT[1] - SKIRT[0]), 0, 1) * np.clip(1 - (bottom - Y) / 14, 0, 1) ** 1.5
    shade = shade * (1 - 0.32 * rest)[..., None]
    # volume: the sand curves away toward the back and right of the rounded
    # end, so it darkens that way, and darkest where it presses against the
    # glass (a soft occlusion band just inside the rim)
    back_side = np.clip((X - 1380) / (TUBE_END_X + TUBE_END_R[0] - 1380), 0, 1) ** 1.4
    rim = np.clip((rad - 0.72) / 0.28, 0, 1) * np.clip((X - (TUBE_END_X - 40)) / 30, 0, 1)
    shade = shade * (1 - 0.38 * back_side - 0.25 * rim ** 1.5)[..., None]
    # and a soft rounded highlight where the slope's crest turns into the
    # dome, so the mound reads as a curved surface rather than a flat fill
    crest = np.exp(-(((X - 1415) / 26) ** 2 + ((Y - 1030) / 30) ** 2))
    shade = shade * (1 + 0.10 * crest)[..., None]
    # the dune's surface is smooth: only faint ripples and grain
    ripple = 0.04 * np.sin(d * 0.9 + (X - X0) * 0.25 + 5 * up(_noise((H, W), 24, 9))) * np.clip(1 - d / 20, 0, 1)
    fine_sand = up(rng.normal(0, 0.045, (H, W)).astype(np.float32) + 0.05 * _noise((H, W), 32, 2))
    shade = shade * (1 + ripple[..., None] + 0.4 * mottle[..., None] + 0.6 * grain[..., None] + fine_sand[..., None])
    rgb[sand_mask] = shade[sand_mask]
    cover[sand_mask] = 1
    # soft: how much of the sand to keep where it blends into the dune; the
    # partial values are dithered at the end (C2 sprites are on/off), which
    # reads as sand grains thinning out rather than an edge
    def smooth(v):
        v = np.clip(v, 0, 1)
        return v * v * (3 - 2 * v)
    soft = np.ones((h, w), np.float32)
    skirt_zone = sand_mask & skirt & (Y >= duneY + 5)
    soft[skirt_zone] = smooth((X[skirt_zone] - SKIRT[0]) / (SKIRT[1] - SKIRT[0]))

    rgb[face] = np.array([40, 32, 20], np.float32) * (1 + 2 * mottle[face][:, None])
    cover[face] = 1
    # Pillow premultiplies RGBA itself when resizing, so the color goes in
    # plain and comes out as the covered samples' average
    big = np.dstack([rgb, cover * 255]).clip(0, 255).astype(np.uint8)
    im = Image.fromarray(big, 'RGBA').resize((W, H), Image.BOX)
    a = np.asarray(im).astype(np.float32)
    al = a[..., 3:4] / 255.0
    # anti-aliasing with on/off transparency: each soft edge pixel is blended
    # toward what lies behind it there (tube wall above the slope, brown rock
    # and earth to the right and below, the dune at the slope's foot), then
    # kept if it is mostly covered
    sx = np.arange(W) + X0 + 0.5
    sy = (np.arange(H) + Y0 + 0.5)[:, None]
    dune_line = np.interp(sx, DUNE_X, DUNE_Y)[None, :]
    matte = np.empty((H, W, 3), np.float32)
    matte[:] = np.array([34, 46, 46], np.float32)              # tube wall
    matte[(sy > dune_line - 2) & (sx[None, :] < BAND_X[0])] = (70, 78, 67)   # the dune
    matte[(sy > 1092) | (sx[None, :] > RAMP_TOP[0] + 4)] = (42, 31, 19)     # rock and earth
    a[..., :3] = a[..., :3] * al + matte * (1 - al)
    sp = np.asarray(Image.fromarray((soft * 255).astype(np.uint8)).resize((W, H), Image.BOX), np.float32) / 255
    dither = np.where(sp > 0.995, 0.0, rng.random((H, W)))
    a[..., 3] = np.where((al[..., 0] < 0.3) | (sp <= dither), 0, 255)
    return Image.fromarray(a.clip(0, 255).astype(np.uint8), 'RGBA')


if __name__ == '__main__':
    base = sys.argv[1]
    im = draw()
    im.save(base + '.png')
    s16.write(base + '.s16', [im])
    print(base, im.size)
