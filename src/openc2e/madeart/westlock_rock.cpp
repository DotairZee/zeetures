// zwrk: art/westlock.py, its rock() only (the doors zwsd, zwld and the car
// zwcr are drawn from numbers and ship as files). The west sea's seabed
// airlock: the rock drawn again in front of a creature riding the invisible
// car through it. westlock.cos places it at (ROCK_X0, ROCK_Y0); east of x 660
// the game's own rock is drawn over everything. The rock below the seabed's
// floor line round the hidden way down and west, clear above that line
// (creatures on the seabed show) and over the two doors (the whole door
// shows; a creature is seen in a doorway).

#include "MadeArt.h"

#include <algorithm>

namespace MadeArt {

namespace {

const double SEA_DOOR[2] = {628, 1330}; // centre, world px
const double LAND_DOOR[2] = {566, 1512};
const int DOOR_D = 56; // the doors' size
const int ROCK_X0 = 573, ROCK_Y0 = 1245, ROCK_X1 = 660, ROCK_Y1 = 1546;

// the seabed's floor line (room 121) between x 549 and 669
double floor_y(double x) {
	return 1317 + (x - 549) * 55.0 / 120.0;
}

} // namespace

static Register zwrk("zwrk", [] {
	Rgba im = backgroundRegion(ROCK_X0, ROCK_Y0, ROCK_X1, ROCK_Y1);
	if (!im.w)
		return std::vector<Rgba>();
	const double r = DOOR_D / 2.0 + 0.5;
	for (int y = 0; y < im.h; y++)
		for (int x = 0; x < im.w; x++) {
			double wx = x + ROCK_X0 + 0.5, wy = y + ROCK_Y0 + 0.5;
			bool keep = wy > floor_y(std::min(wx, 669.0)) + 1; // below the seabed's line
			for (const double* d : {SEA_DOOR, LAND_DOOR}) // the whole door shows
				keep = keep && (wx - d[0]) * (wx - d[0]) + (wy - d[1]) * (wy - d[1]) > r * r;
			im.at(x, y)[3] = keep ? 255 : 0;
		}
	return std::vector<Rgba>{im};
});

} // namespace MadeArt
