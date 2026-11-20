#pragma once

// The walkable glass tube's made art, part: the tube's parts (art/tube_walk.py: paste_rot_masked, blob, smooth_fill, deck_fill, load_front; art/tube_walk_lib.py: dock_join, stalactites, render_rock_earth).
// A pixel-exact port of the Python recipe (2026-11-19); see npcore.h for the
// numpy/Pillow operations it is built on.
//
// Each function mirrors the Python one of the same name: the same
// arguments in the same order (defaults included), the same dtypes
// (float32 pictures are np::ImgF, float64 np::ImgD), and the same results
// to the bit. Pictures are (h, w, c) as numpy's, indexed (y, x, k). A bool
// mask is an np::Img8 with c 1 holding 0 or 1. "In place" means the Python
// one changes its first argument and so does this.
//
// Proof: art/tube_walk_check_parts.py runs the recipe's make() with these
// wrapped and records every call; OPENC2E_TW_CHECK_PARTS=DIR makes any
// openc2e program run these on the recorded inputs, write their results to
// DIR/out and exit; the script then compares (max difference 0, floats bit
// for bit).

#include "npcore.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace MadeArt {
namespace tubewalk {

// tube_walk.py paste_rot_masked(out, pc, angle_deg, left_top, keep_out), in
// place on out (the scene, H x W x 3 float32, its top left at world ox, oy:
// the recipe's OX, OY): the RGBA piece pc (float32) turned by angleDeg
// (down to the east positive; Pillow's bicubic rotate, expanded) about its
// top left corner, which lands at world leftTop; pasted where its alpha is
// over 128 and keepOut (H x W mask) is not set.
void paste_rot_masked(np::ImgF& out, const np::ImgF& pc, double angleDeg, std::pair<double, double> leftTop,
	const np::Img8& keepOut, int ox = 4400, int oy = 1250);

// tube_walk.py blob(points, jitter, seed): a rock outline, the polygon's
// edges broken up with numpy.random.default_rng(seed) jitter; points are
// (x, y), world coordinates
std::vector<std::pair<double, double>> blob(const std::vector<std::pair<double, double>>& points, double jitter, uint64_t seed);

// tube_walk.py smooth_fill(F, M, iters, vk, div), in place on F (float32,
// h x w x 3): M (h x w mask) filled smoothly from around it by np.roll
// relaxation (wrapping at the edges, as np.roll does), float32 throughout
void smooth_fill(np::ImgF& F, const np::Img8& M, int iters = 1200, double vk = 0.35, double div = 2.7);

// tube_walk.py deck_fill(img, deck_rm, box, seed, inside), in place on img
// (float32, H x W x 3): box is (y0, y1, x0, x1) as the recipe's deck_box;
// inside may be null (None)
struct DeckBox {
	int y0, y1, x0, x1;
};
void deck_fill(np::ImgF& img, const np::Img8& deckRm, DeckBox box, uint64_t seed, const np::Img8* inside = nullptr);

// tube_walk.py load_front(): the owner's front mask (our own annotation,
// art/tube_walk_data/front.png, embedded in the engine as
// tube_walk_front_data.inc) placed over the scene: H x W (840 x 2340), 0/1
np::Img8 load_front();

// tube_walk_lib.py dock_join(o, base, MOD, X0, start_top, slope, keep, OX,
// OY, n, blend, kstart), in place on o (float32, H x W x 3): the tube's
// first n px drawn sheared at the dock collar. MOD is the tube module
// (float32, 170 x 300 x 4), keep an H x W mask. (The recipe calls it with
// kstart=-34.)
void dock_join(np::ImgF& o, const np::ImgF& base, const np::ImgF& MOD, int X0, double startTop, double slope,
	const np::Img8& keep, int OX, int OY, int n = 56, int blend = 8, int kstart = -1);

// tube_walk_lib.py _fbm(rng, H, W, cells_list, amps): bicubic value noise,
// float32 H x W, drawn from rng (the generator moves on as numpy's does)
np::ImgF fbm(np::Generator& rng, int H, int W, const std::vector<double>& cells, const std::vector<double>& amps);

// tube_walk_lib.py _q8(a): a (float32, about -0.5..0.5) rounded through 8 bits
np::ImgF q8(const np::ImgF& a);

// tube_walk_lib.py stalactites(o, bg, items, OX, OY, seed, ss), in place on
// o (float32, the scene): each item (x, y, width, length, lean) hangs a cone
// of the cave's rock from (x, y); colors from bg (float32, the background
// over the same scene)
struct Stalactite {
	double x, y, wd, ln, lean;
};
void stalactites(np::ImgF& o, const np::ImgF& bg, const std::vector<Stalactite>& items, int OX, int OY,
	uint64_t seed = 3, int ss = 3);

// tube_walk_lib.py render_rock_earth(outline, sample, seed, ss, patch): the
// rock face, textured from sample (float32 h x w x 3, the earth below the
// lab). Returns its world top left (x0, y0), rgb (float64, clipped to
// 0..255) and alpha (float32), both at 1x
struct RockEarth {
	int x0 = 0, y0 = 0;
	np::ImgD rgb;
	np::ImgF alpha;
};
RockEarth render_rock_earth(const std::vector<std::pair<double, double>>& outline, const np::ImgF& sample, uint64_t seed = 11,
	int ss = 2, int patch = 44);

// developer aid: with OPENC2E_TW_CHECK_PARTS=DIR set, any openc2e program
// runs every function above on the inputs art/tube_walk_check_parts.py
// recorded in DIR/in, writes the results to DIR/out at startup and exits
int checkParts(const std::string& dir);

} // namespace tubewalk
} // namespace MadeArt
