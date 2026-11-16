// zths, ztbf and ztde: art/tube_short.py. The glass tube by the treehouses,
// shortened at its west end (owner, 2026-11-14: the treehouse cage lift's
// new top stop is on the upper hut's deck, under the tube's west end, and
// the lift should not come up through the tube).
//
// The tube is painted in the world's background and drawn again in front of
// creatures by a foreground piece (scenery 1 0 0 at 3531,1390: hall.s16
// frame 4, 504 x 172), so creatures walking along it are inside its glass.
// - zths: the foreground piece starting SHIFT px further east, its broken
//   west end (the cap columns) moved there with it; the rest as it was.
//   Drawn in place of the game's piece (tube-short.cos removes that one).
// - ztbf: behind creatures, the background's own tube end moved east with
//   the front piece, and where it was, each column filled with that
//   column's own background from just above the tube, mirrored at the
//   tube's top edge, so it carries on the trunk and leaves above it.
// - ztde: the upper hut's deck carried on under the tube's new mouth.

#include "MadeArt.h"

#include <algorithm>

namespace MadeArt {

namespace {

const int TX = 3531;              // the game's piece's top left (y 1390)
const int FRAME = 4;              // its picture in hall.s16
const int SHIFT = 130;            // how far east the tube's mouth moves (past the lift's cage, x 3650)
const int BLEND = 10;             // columns blended where the moved end meets the body
const int FILL_X0 = 3524, FILL_Y0 = 1398, FILL_Y1 = 1541; // the band filled: from the hut, above the deck
const int BAND_Y0 = 1388;         // the moved tube end's top (a little foliage above its broken edge)
const int SRC_X0 = 3527;          // the background's tube end, from just past the hut's wall

// zths and ztbf (tube_short.py make()); false if the game's piece is missing
bool makeTube(Rgba& zths, Rgba& ztbf) {
	std::vector<Rgba> hall = gameSprite("hall");
	if (int(hall.size()) <= FRAME)
		return false;
	const Rgba& piece = hall[FRAME];
	const int w = piece.w, h = piece.h;
	auto mask = [&](int x, int y) { return piece.at(x, y)[3] != 0; };
	if (w <= SHIFT + BLEND)
		return false;
	// each column's outline (the glass has dark holes the background shows
	// through, so the outline, not the opaque pixels, is the tube)
	std::vector<int> top(w, h);
	for (int c = 0; c < w; c++)
		for (int y = 0; y < h; y++)
			if (mask(c, y)) {
				top[c] = y;
				break;
			}
	// the broken end: the columns before the top edge reaches the body's
	std::vector<int> mid(top.begin() + w / 3, top.begin() + 2 * w / 3);
	std::sort(mid.begin(), mid.end());
	size_t m = mid.size();
	double median = m % 2 ? double(mid[m / 2]) : (double(mid[m / 2 - 1]) + double(mid[m / 2])) / 2.0;
	int bodyTop = int(median);
	int cap = 0;
	for (int c = 0; c < w; c++)
		if (top[c] <= bodyTop + 1) {
			cap = c;
			break;
		}
	cap += 6;
	// zths: the cap, then the body as it was
	const int nw = w - SHIFT;
	std::vector<float> out(size_t(h) * nw * 4, 0.0f);
	auto O = [&](int u, int y) { return &out[(size_t(y) * nw + u) * 4]; };
	for (int u = 0; u < nw; u++) {
		int src = u < cap ? u : u + SHIFT;
		for (int y = 0; y < h; y++) {
			const uint8_t* p = piece.at(src, y);
			float* o = O(u, y);
			o[0] = p[0];
			o[1] = p[1];
			o[2] = p[2];
			o[3] = mask(src, y) ? 255.0f : 0.0f;
		}
	}
	for (int k = 0; k < BLEND; k++) { // ease from the moved end into the body
		int u = cap - BLEND + k;
		double t = double(k + 1) / (BLEND + 1);
		float t1 = float(1 - t), t2 = float(t);
		for (int y = 0; y < h; y++)
			if (mask(u, y) && mask(u + SHIFT, y))
				for (int c = 0; c < 3; c++)
					O(u, y)[c] = float(piece.at(u, y)[c]) * t1 + float(piece.at(u + SHIFT, y)[c]) * t2;
	}
	zths = Rgba(nw, h);
	for (size_t i = 0; i < out.size(); i++)
		zths.px[i] = uint8_t(out[i]);

	// ztbf, behind creatures: the background's own tube end moved east with
	// the front piece (its inner, back glass is painted only there: owner,
	// 2026-11-14), and where it was (FILL_X0 to SRC_X0 + SHIFT), the trunk
	// and leaves from just above the band, mirrored at its top
	const int endX1 = TX + SHIFT + cap;
	const int span = endX1 - FILL_X0;
	const int bh = FILL_Y1 - BAND_Y0 + 1;
	std::vector<float> fill(size_t(bh) * span * 4, 0.0f);
	auto F = [&](int c, int y) { return &fill[(size_t(y) * span + c) * 4]; };
	const int fh = FILL_Y1 - FILL_Y0 + 1;
	Rgba above = backgroundRegion(FILL_X0, FILL_Y0 - fh - 2, endX1, FILL_Y0);
	Rgba old = backgroundRegion(SRC_X0, BAND_Y0, endX1 - SHIFT, FILL_Y1 + 1);
	if (!above.w || !old.w)
		return false;
	const int movedX0 = SRC_X0 + SHIFT;
	for (int c = 0; c < span; c++) {
		int x = FILL_X0 + c;
		if (x < movedX0) {
			for (int y = FILL_Y0; y <= FILL_Y1; y++) {
				const uint8_t* p = above.at(c, above.h - 1 - (y - FILL_Y0));
				float* f = F(c, y - BAND_Y0);
				f[0] = p[0];
				f[1] = p[1];
				f[2] = p[2];
				f[3] = 255.0f;
			}
		} else {
			int k = endX1 - x; // ease into the tube body it meets
			for (int y = 0; y < bh; y++) {
				const uint8_t* p = old.at(x - movedX0, y);
				float* f = F(c, y);
				float r = p[0], g = p[1], b = p[2];
				f[0] = r;
				f[1] = g;
				f[2] = b;
				// (not the tip of the hut's straw roof, which reaches into the
				// band at its west end: left out, the background shows there)
				bool straw = r > 120 && g > 85 && r - b > 55 && g > b + 20;
				f[3] = straw ? 0.0f : 255.0f;
				if (k < 12)
					f[3] *= float(k / 12.0);
			}
		}
	}
	// soft along the top and at the hut's side, so the edges do not show
	for (int k = 0; k < 8; k++)
		for (int c = 0; c < span; c++)
			F(c, k)[3] *= float((k + 1) / 9.0);
	for (int c = 0; c < 4; c++)
		for (int y = 0; y < bh; y++)
			F(c, y)[3] *= float((c + 1) / 5.0);
	ztbf = Rgba(span, bh);
	for (size_t i = 0; i < fill.size(); i++)
		ztbf.px[i] = uint8_t(fill[i]);
	return true;
}

// the upper hut's deck carried on under the tube's new mouth (owner,
// 2026-11-14): its tan planks (y 1541-1550), the orange log under them (to
// x 3617) and the thicker log below (to 3645, a ring and a blue lashing
// along it). A clean middle stretch (TILE) is laid on from DECK_X0, then
// the deck's own ragged end (END), moved out to DECK_X1. Only the wood is
// taken (orange and tan; the leaves and glass behind stay).
const int DECK_Y0 = 1540, DECK_Y1 = 1567;
const int TILE0 = 3466, TILE1 = 3586;
const int END0 = 3586, END1 = 3650;
const int DECK_X0 = 3586, DECK_X1 = 3796;

static Register zths("zths", [] {
	Rgba a, b;
	if (!makeTube(a, b))
		return std::vector<Rgba>();
	return std::vector<Rgba>{a};
});

static Register ztbf("ztbf", [] {
	Rgba a, b;
	if (!makeTube(a, b))
		return std::vector<Rgba>();
	return std::vector<Rgba>{b};
});

static Register ztde("ztde", [] {
	Rgba src = backgroundRegion(TILE0, DECK_Y0, END1, DECK_Y1);
	if (!src.w)
		return std::vector<Rgba>();
	const int tw = TILE1 - TILE0, ew = END1 - END0;
	const int w = DECK_X1 - DECK_X0, h = DECK_Y1 - DECK_Y0;
	const int endStart = w - ew;
	Rgba out(w, h);
	for (int c = 0; c < w; c++) {
		int sc = c >= endStart ? (TILE1 - TILE0) + (c - endStart) : c % tw;
		for (int y = 0; y < h; y++) {
			const uint8_t* p = src.at(sc, y);
			uint8_t* o = out.at(c, y);
			float r = p[0], g = p[1], b = p[2];
			bool wood = r > g + 5 && r - b > 35;
			o[0] = p[0];
			o[1] = p[1];
			o[2] = p[2];
			o[3] = wood ? 255 : 0;
		}
	}
	return std::vector<Rgba>{out};
});

} // namespace

} // namespace MadeArt
