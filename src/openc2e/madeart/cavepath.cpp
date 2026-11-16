// zajb and wfal: art/cavepath.py. Scenery for the hidden path behind the
// pond's waterfall (owner, 2026-11-10):
// - zajb, the exit: the old stone arch west of the bamboo-cage lift. Its
//   right jamb and the log blocks beside it are drawn again in front of
//   creatures, cut along the opening's inner edge, so a creature coming out
//   of the tunnel walks out past the jamb's edge into the arch (top-left at
//   JAMB_X0, JAMB_Y0).
// - wfal, the entrance: the game's waterfall with only what moves (the
//   water and its splash) kept, so a creature walking behind the falls is
//   seen around the water until the rock's edge hides it. It takes the
//   place of the game's own wfal.

#include "MadeArt.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace MadeArt {

namespace {

// the arch: its opening's inner edge on the right, as seen on the scenery: a
// straight jamb at x 4120 below y 752, the arch's inner curve above it
const int JAMB_X0 = 4078, JAMB_Y0 = 700, JAMB_X1 = 4186, JAMB_Y1 = 843;
const double ARC_CX = 4083.0, ARC_CY = 752.0, ARC_R = 37.0, JAMB_X = 4120.0;

double innerEdge(double y) {
	if (y >= ARC_CY)
		return JAMB_X;
	double dy = ARC_CY - y;
	return ARC_CX + std::sqrt(std::max(ARC_R * ARC_R - dy * dy, 0.0));
}

std::vector<Rgba> jamb() {
	Rgba im = backgroundRegion(JAMB_X0, JAMB_Y0, JAMB_X1, JAMB_Y1);
	if (!im.w)
		return {};
	for (int j = 0; j < im.h; j++) {
		double y = JAMB_Y0 + j + 0.5;
		double ex = innerEdge(y);
		for (int i = 0; i < im.w; i++) {
			double x = JAMB_X0 + i + 0.5;
			// a one-pixel soft edge (float32, as the recipe's array)
			float a = float(std::clamp((x - ex) + 0.5, 0.0, 1.0));
			im.at(i, j)[3] = uint8_t(a * 255.0f);
		}
	}
	// above the arch's crown nothing is needed (the arch is in front anyway)
	return {im};
}

const int STILL = 12; // a waterfall pixel that never varies more than this is wall
const int GROW = 1;

// the mask without its (4-connected) pieces smaller than minSize
std::vector<char> largestParts(const std::vector<char>& mask, int w, int h, size_t minSize) {
	std::vector<char> seen(mask.size(), 0), out(mask.size(), 0);
	std::vector<std::pair<int, int>> stack, piece;
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			if (!mask[size_t(y) * w + x] || seen[size_t(y) * w + x])
				continue;
			stack.assign(1, {y, x});
			piece.clear();
			seen[size_t(y) * w + x] = 1;
			while (!stack.empty()) {
				auto [cy, cx] = stack.back();
				stack.pop_back();
				piece.push_back({cy, cx});
				const int ns[4][2] = {{cy + 1, cx}, {cy - 1, cx}, {cy, cx + 1}, {cy, cx - 1}};
				for (auto& n : ns) {
					int ny = n[0], nx = n[1];
					if (ny >= 0 && ny < h && nx >= 0 && nx < w && mask[size_t(ny) * w + nx] && !seen[size_t(ny) * w + nx]) {
						seen[size_t(ny) * w + nx] = 1;
						stack.push_back({ny, nx});
					}
				}
			}
			if (piece.size() >= minSize)
				for (auto& [py, px] : piece)
					out[size_t(py) * w + px] = 1;
		}
	return out;
}

std::vector<Rgba> waterOnly() {
	std::vector<Rgba> frames = gameSprite("wfal");
	if (frames.empty())
		return {};
	const int w = frames[0].w, h = frames[0].h;
	for (auto& f : frames)
		if (f.w != w || f.h != h)
			throw std::runtime_error("wfal.s16: frames differ in size; not the expected waterfall");
	// kept: any channel's range over the frames more than STILL
	std::vector<char> keep(size_t(w) * h, 0);
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			int most = 0;
			for (int c = 0; c < 3; c++) {
				int lo = 255, hi = 0;
				for (auto& f : frames) {
					int v = f.at(x, y)[c];
					lo = std::min(lo, v);
					hi = std::max(hi, v);
				}
				most = std::max(most, hi - lo);
			}
			keep[size_t(y) * w + x] = most > STILL;
		}
	// only the water itself: drop small moving patches apart from the main
	// body (a flicker in the painted wall at the falls' left edge)
	keep = largestParts(keep, w, h, 200);
	for (int g = 0; g < GROW; g++) {
		std::vector<char> grown = keep;
		for (int y = 0; y < h; y++)
			for (int x = 0; x < w; x++) {
				size_t i = size_t(y) * w + x;
				if ((y > 0 && keep[i - w]) || (y + 1 < h && keep[i + w]) || (x > 0 && keep[i - 1]) || (x + 1 < w && keep[i + 1]))
					grown[i] = 1;
			}
		keep = std::move(grown);
	}
	for (auto& f : frames)
		for (int y = 0; y < h; y++)
			for (int x = 0; x < w; x++)
				if (!keep[size_t(y) * w + x])
					f.at(x, y)[3] = 0;
	return frames;
}

} // namespace

static Register zajb("zajb", jamb);
static Register wfal("wfal", waterOnly, true);

} // namespace MadeArt
