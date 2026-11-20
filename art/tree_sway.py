"""The big tree's sway picture (tre2, the game's object 2 3 8 over the west
sea: 11 frames of 820 x 500 at 840, 0) with a flaw in the game's own art
removed: frame 4's bottom row has 77 opaque near-white pixels at its left
end (x 0-76), drawn as a white line in the sky over the west sea whenever
the tree's sway rests on that frame (the owner's "white line in the sky",
seen many times since 2026-11-11; found 2026-11-20 with the game paused
on it). Made on the player's own machine from their installed Creatures 2
(only this recipe is part of the project, never the game's pixels): every
frame as the game's, frame 4's bottom row transparent where it is
near-white (all three channels over 200).

    python3 tree_sway.py C2_DIR USER_DATA_DIR

writes USER_DATA_DIR/Overrides/Images/tre2.s16; the engine makes the same
picture itself (madeart/tree_sway.cpp) in its place.
"""
import os
import sys
import numpy as np
from PIL import Image
import s16

FRAME, BRIGHT = 4, 200


def fixed(c2):
    frames = s16.read(os.path.join(c2, 'Images', 'tre2.s16'))
    out = []
    for i, im in enumerate(frames):
        a = np.asarray(im.convert('RGBA')).copy()
        if i == FRAME:
            row = a[-1]
            light = (row[:, 3] > 0) & (row[:, 0] > BRIGHT) & (row[:, 1] > BRIGHT) & (row[:, 2] > BRIGHT)
            row[light] = 0
        out.append(Image.fromarray(a, 'RGBA'))
    return out


def main():
    c2, data = sys.argv[1], sys.argv[2]
    os.makedirs(os.path.join(data, 'Overrides', 'Images'), exist_ok=True)
    s16.write(os.path.join(data, 'Overrides', 'Images', 'tre2.s16'), fixed(c2))
    print('wrote Overrides/Images/tre2.s16 in', data)


if __name__ == '__main__':
    main()
