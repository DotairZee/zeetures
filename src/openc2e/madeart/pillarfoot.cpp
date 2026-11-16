// zpft: art/pillarfoot.py. The ledge under the twisted pillar beside the
// lift shaft west of the arches, copied from the background so it is drawn
// in front of creatures walking behind the pillar (pillar.cos places it at
// 4270, 1172).

#include "MadeArt.h"

namespace MadeArt {

static Register zpft("zpft", [] {
	const int X0 = 4270, Y0 = 1172, X1 = 4318, Y1 = 1180; // the base's width, its foot to the rubble
	Rgba im = backgroundRegion(X0, Y0, X1, Y1);
	if (!im.w)
		return std::vector<Rgba>();
	return std::vector<Rgba>{im};
});

} // namespace MadeArt
