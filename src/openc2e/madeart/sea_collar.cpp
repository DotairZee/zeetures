// zscl: art/sea_collar.py. The collar at the east end of the sandy tunnel
// under the west sea's east shore, copied from the background (its band and
// two knobs only) so it is drawn in front of creatures; the game's front
// piece for the tunnel ends just short of it (sea-collar.cos places it at
// 1145, 995).

#include "MadeArt.h"

namespace MadeArt {

static Register zscl("zscl", [] {
	const int X0 = 1145, Y0 = 995, W = 47, H = 105;
	Rgba im = backgroundRegion(X0, Y0, X0 + W, Y0 + H);
	if (!im.w)
		return std::vector<Rgba>();
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++) {
			const int X = X0 + x, Y = Y0 + y;
			const bool band = X >= 1156 && X <= 1182 && Y >= 998 && Y <= 1097;
			const bool knobs = (X - 1151) * (X - 1151) + (Y - 1007) * (Y - 1007) <= 20 || (X - 1185) * (X - 1185) + (Y - 1007) * (Y - 1007) <= 20;
			uint8_t* p = im.at(x, y);
			if (band || knobs)
				p[3] = 255;
			else
				p[0] = p[1] = p[2] = p[3] = 0;
		}
	return std::vector<Rgba>{im};
});

} // namespace MadeArt
