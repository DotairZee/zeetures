// tre2: art/tree_sway.py. The big tree's sway picture over the west sea
// (object 2 3 8) as the game's, but for frame 4's bottom row, whose 77
// near-white pixels at its left end drew a white line in the sky whenever
// the sway rested on that frame (owner, 2026-11-20). Takes the place of the
// game's own tre2.

#include "MadeArt.h"

namespace MadeArt {

static std::vector<Rgba> treeSway() {
	std::vector<Rgba> frames = gameSprite("tre2");
	const size_t FRAME = 4;
	const int BRIGHT = 200;
	if (frames.size() <= FRAME || frames[FRAME].h < 1)
		return {};
	Rgba& f = frames[FRAME];
	for (int x = 0; x < f.w; x++) {
		uint8_t* p = f.at(x, f.h - 1);
		if (p[3] > 0 && p[0] > BRIGHT && p[1] > BRIGHT && p[2] > BRIGHT)
			p[0] = p[1] = p[2] = p[3] = 0;
	}
	return frames;
}

static Register tre2("tre2", treeSway, true);

} // namespace MadeArt
