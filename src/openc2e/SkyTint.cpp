#include "SkyTint.h"

#include "Engine.h"
#include "MetaRoom.h"
#include "World.h"
#include "common/backend/Backend.h"
#include <cstdio>
#include <cstdlib>
#include "common/creaturesImage.h"
#include "fileformats/ImageUtils.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <deque>
#include <fmt/core.h>
#include <map>
#include <random>
#include <vector>

namespace SkyTint {

namespace {

// layers: 0 dawn, 1 dusk, 2 night (morning and afternoon: none)
struct Sky {
	bool built = false, any = false;
	int x0 = 0, y0 = 0, w = 0, h = 0; // the sky's extent in the background
	std::array<creaturesImage, 3> layers; // drawn as game images (one frame each)
};
std::map<std::string, Sky> s_skies;

// which layer each of C2's times of day uses (-1: the sky as painted)
int layerFor(unsigned int timeofday) {
	switch (timeofday) {
		case 0: return 0; // dawn
		case 3: return 1; // evening
		case 4: return 2; // night
		default: return -1; // morning, afternoon
	}
}

void build(Sky& sky, creaturesImage& bkgd) {
	sky.built = true;
	if (bkgd.images.empty() || !bkgd.images[0])
		return;
	auto t0 = std::chrono::steady_clock::now();
	Image rgb = ImageUtils::ToRGB24(bkgd.images[0]);
	const int W = rgb.width, H = rgb.height;
	const uint8_t* px = rgb.data.data();
	auto at = [&](int x, int y) { return px + (size_t(y) * W + x) * 3; };
	auto skyColoured = [&](const uint8_t* p) {
		int r = p[0], g = p[1], b = p[2];
		int bright = (r + g + b) / 3;
		// blue skies, and the pink and violet painted dusk: bright, and
		// blue at least as strong as green
		return bright > 90 && b + 20 >= g && b > 70;
	};
	auto cloudy = [&](const uint8_t* p) {
		// white to blue-grey (the clouds' shading), not warm stone
		int r = p[0], g = p[1], b = p[2];
		int mx = std::max({r, g, b}), mn = std::min({r, g, b});
		return mn > 105 && mx - mn < 70 && b + 6 >= r;
	};
	// the painted sea by the boat is green-blue: greener than it is red or
	// blue, which no sky is (the sunset's oranges are redder than green)
	auto notSea = [&](const uint8_t* p) { return !(p[1] > p[0] + 8 && p[1] > p[2] + 6); };
	// the sky: grown from sky-coloured pixels on the top edge through
	// smoothly changing colour; then the clouds touching it
	std::vector<uint8_t> mask(size_t(W) * H, 0);
	std::deque<std::pair<int, int>> q;
	for (int x = 0; x < W; x++)
		if (skyColoured(at(x, 0))) {
			mask[x] = 1;
			q.push_back({x, 0});
		}
	const int tol = 22;
	auto grow = [&](auto accept) {
		while (!q.empty()) {
			auto [x, y] = q.front();
			q.pop_front();
			const uint8_t* a = at(x, y);
			const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
			for (int k = 0; k < 4; k++) {
				int nx = x + dx[k], ny = y + dy[k];
				if (nx < 0 || ny < 0 || nx >= W || ny >= H || mask[size_t(ny) * W + nx])
					continue;
				const uint8_t* b = at(nx, ny);
				if (accept(a, b)) {
					mask[size_t(ny) * W + nx] = 1;
					q.push_back({nx, ny});
				}
			}
		}
	};
	grow([&](const uint8_t* a, const uint8_t* b) {
		return std::abs(a[0] - b[0]) + std::abs(a[1] - b[1]) + std::abs(a[2] - b[2]) < tol && (b[0] + b[1] + b[2]) / 3 > 60 && notSea(b);
	});
	// sky walled in by what stands in front of it (seen through the bell
	// tower's arches): sky-coloured pixels close to the colour of the sky
	// around them, grown on the same way
	{
		// (compared with the sky around it: the mean colour of the sky
		// within about 320 px across and 160 px up and down, on a grid of
		// 32 px blocks. Comparing along the row alone missed the sky seen
		// through the bell tower's arches, where the tower hides the rest
		// of the row; the painted sky also runs from blue in the west to
		// pink in the east)
		const int B = 32, nbx = (W + B - 1) / B, nby = (H + B - 1) / B;
		std::vector<double> bs(size_t(nbx + 1) * (nby + 1) * 4, 0.0);
		auto BI = [&](int bx, int by, int ch) -> double& { return bs[(size_t(by) * (nbx + 1) + bx) * 4 + ch]; };
		for (int y = 0; y < H; y++)
			for (int x = 0; x < W; x++)
				if (mask[size_t(y) * W + x]) {
					const uint8_t* p = at(x, y);
					int bx = x / B + 1, by = y / B + 1;
					BI(bx, by, 0) += p[0];
					BI(bx, by, 1) += p[1];
					BI(bx, by, 2) += p[2];
					BI(bx, by, 3) += 1;
				}
		for (int by = 1; by <= nby; by++)
			for (int bx = 1; bx <= nbx; bx++)
				for (int ch = 0; ch < 4; ch++)
					BI(bx, by, ch) += BI(bx - 1, by, ch) + BI(bx, by - 1, ch) - BI(bx - 1, by - 1, ch);
		for (int y = 0; y < H; y++) {
			int by = y / B;
			int y0 = std::max(0, by - 5), y1 = std::min(nby, by + 6);
			for (int x = 0; x < W; x++) {
				if (mask[size_t(y) * W + x])
					continue;
				const uint8_t* p = at(x, y);
				if (!skyColoured(p))
					continue;
				int bx = x / B;
				int x0 = std::max(0, bx - 10), x1 = std::min(nbx, bx + 11);
				double sum[4];
				for (int ch = 0; ch < 4; ch++)
					sum[ch] = BI(x1, y1, ch) - BI(x0, y1, ch) - BI(x1, y0, ch) + BI(x0, y0, ch);
				if (sum[3] < 200)
					continue;
				int mr = (int)(sum[0] / sum[3]), mg = (int)(sum[1] / sum[3]), mb = (int)(sum[2] / sum[3]);
				if (std::abs(p[0] - mr) + std::abs(p[1] - mg) + std::abs(p[2] - mb) < 48) {
					mask[size_t(y) * W + x] = 1;
					q.push_back({x, y});
				}
			}
		}
		grow([&](const uint8_t* a, const uint8_t* b) {
			return std::abs(a[0] - b[0]) + std::abs(a[1] - b[1]) + std::abs(a[2] - b[2]) < tol && (b[0] + b[1] + b[2]) / 3 > 60 && notSea(b);
		});
	}
	// clouds: whitish pixels reached from the sky
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++)
			if (mask[size_t(y) * W + x])
				for (int k = 0; k < 4; k++) {
					int nx = x + (k == 0) - (k == 1), ny = y + (k == 2) - (k == 3);
					if (nx >= 0 && ny >= 0 && nx < W && ny < H && !mask[size_t(ny) * W + nx] && cloudy(at(nx, ny))) {
						mask[size_t(ny) * W + nx] = 1;
						q.push_back({nx, ny});
					}
				}
	grow([&](const uint8_t*, const uint8_t* b) { return cloudy(b); });

	// specks: small islands of the trace apart from the rest (sky-coloured
	// flecks in the dunes, which turned orange at dusk) are dropped; the sky
	// seen through the bell tower's arches comes in pieces of thousands
	{
		std::vector<uint8_t> seen(size_t(W) * H, 0);
		std::vector<size_t> comp;
		size_t dropped = 0;
		for (size_t i = 0; i < seen.size(); i++) {
			if (!mask[i] || seen[i])
				continue;
			comp.clear();
			comp.push_back(i);
			seen[i] = 1;
			for (size_t k = 0; k < comp.size(); k++) {
				int x = int(comp[k] % W), y = int(comp[k] / W);
				const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
				for (int d = 0; d < 4; d++) {
					int nx = x + dx[d], ny = y + dy[d];
					if (nx < 0 || ny < 0 || nx >= W || ny >= H)
						continue;
					size_t j = size_t(ny) * W + nx;
					if (mask[j] && !seen[j]) {
						seen[j] = 1;
						comp.push_back(j);
					}
				}
			}
			if (comp.size() < 300) {
				for (size_t j : comp)
					mask[j] = 0;
				dropped += comp.size();
			}
		}
		fmt::print("* Sky: {} px of specks dropped\n", dropped);
	}

	// the sea's horizon: a long, straight, sharp edge where the traced sky
	// turns to darker water below (within a few rows: the painted horizon
	// is not quite level). Whatever of the trace lies below it there is sea,
	// not sky: the sky's haze carried the trace round the rocks and down
	// into the sea, an arc of sky colour in the water at dusk and night.
	// Cleared, with everything below the horizon within 400 px either side.
	{
		auto edgeAt = [&](int x, int y) { // a sharp drop from sky to darker water between rows y and y + 1
			if (y < 0 || y + 1 >= H || !mask[size_t(y) * W + x] || !mask[size_t(y + 1) * W + x])
				return false;
			const uint8_t *a = at(x, y), *b = at(x, y + 1);
			int d = std::abs(a[0] - b[0]) + std::abs(a[1] - b[1]) + std::abs(a[2] - b[2]);
			return d > 60 && (a[0] + a[1] + a[2]) - (b[0] + b[1] + b[2]) > 60;
		};
		std::vector<int> edgeRow(W);
		for (int y = 0; y + 1 < H; y++) {
			int run = 0;
			for (int x = 0; x <= W; x++) {
				int er = -1;
				if (x < W)
					for (int k = y; k <= y + 4 && er < 0; k++)
						if (edgeAt(x, k))
							er = k;
				if (er >= 0) {
					edgeRow[x] = er;
					run++;
					continue;
				}
				if (run >= 200) {
					int x0 = x - run, x1 = x - 1;
					int lo = std::max(0, x0 - 400), hi = std::min(W - 1, x1 + 400);
					size_t cleared = 0;
					for (int k = lo; k <= hi; k++) {
						int from = (k >= x0 && k <= x1) ? edgeRow[k] + 1 : y + 5;
						for (int yy = from; yy < H; yy++)
							if (mask[size_t(yy) * W + k]) {
								mask[size_t(yy) * W + k] = 0;
								cleared++;
							}
					}
					fmt::print("* Sky: sea horizon at y {}-{} from x {} to {}; {} px below it are sea\n", y, y + 4, x0, x1, cleared);
				}
				run = 0;
			}
		}
	}

	// the sky's extent
	size_t count = 0;
	for (uint8_t v : mask)
		count += v;
	if (count < 1000)
		return;
	sky.any = true;

	// How far down the outdoors reaches in each column: the lowest sky pixel
	// there, widened (the most within 120 px either side) and smoothed, so
	// the grade's edge has no steps. Columns with no sky near them: none.
	std::vector<float> bottom(W, -1.0f);
	for (int x = 0; x < W; x++)
		for (int y = H - 1; y >= 0; y--)
			if (mask[size_t(y) * W + x]) {
				bottom[x] = (float)y;
				break;
			}
	{
		std::vector<float> wide(W, -1.0f), smooth(W, -1.0f);
		for (int x = 0; x < W; x++)
			for (int k = std::max(0, x - 120); k <= std::min(W - 1, x + 120); k++)
				wide[x] = std::max(wide[x], bottom[k]);
		for (int x = 0; x < W; x++) {
			float sum = 0;
			int n = 0;
			for (int k = std::max(0, x - 60); k <= std::min(W - 1, x + 60); k++)
				if (wide[k] >= 0) {
					sum += wide[k];
					n++;
				}
			smooth[x] = n ? sum / n : -1.0f;
		}
		bottom = smooth;
	}
	const float fade = 400.0f; // the grade fades out this far below the sky
	float lowest = 0;
	for (float b : bottom)
		lowest = std::max(lowest, b);
	sky.x0 = 0;
	sky.y0 = 0;
	sky.w = W;
	sky.h = std::min(H, (int)(lowest + fade) + 1);

	// halo: pale, sky-coloured fringe pixels touching the sky join it
	for (int y = 1; y + 1 < H; y++)
		for (int x = 1; x + 1 < W; x++) {
			if (mask[size_t(y) * W + x])
				continue;
			// (== 1: pixels joined in this pass don't count, or the halo
			// would run on through anything sky-coloured, the sea included)
			bool touches = mask[size_t(y) * W + x - 1] == 1 || mask[size_t(y) * W + x + 1] == 1 || mask[size_t(y - 1) * W + x] == 1 || mask[size_t(y + 1) * W + x] == 1;
			if (touches && skyColoured(at(x, y)))
				mask[size_t(y) * W + x] = 2; // (2: joined this pass; counted as sky below)
		}
	for (auto& v : mask)
		if (v == 2)
			v = 1;

	// how textured the paint is around each pixel (mean change between
	// neighbours over a 9 x 9 square): the painted sky is smooth (about 1-3),
	// foliage and rock are not (10 and more). The sky's own colours fade out
	// where the paint is textured, so hazy foliage the trace took for sky
	// gets the scene's gentler light like the trees around it. Only the
	// changes between two traced sky pixels count, so neither the sky's own
	// edge nor the tree beside it is texture (counting them left a pale rim
	// round everything standing against the night sky).
	// (pixels on the sky's very edge are left out too: the pale fringe
	// painted round things is a change of its own)
	auto inner = [&](int x, int y) {
		return x > 0 && y > 0 && x + 1 < W && y + 1 < H && mask[size_t(y) * W + x] && mask[size_t(y) * W + x - 1] && mask[size_t(y) * W + x + 1] && mask[size_t(y - 1) * W + x] && mask[size_t(y + 1) * W + x];
	};
	std::vector<uint32_t> integ(size_t(W + 1) * (sky.h + 1), 0), pairs(size_t(W + 1) * (sky.h + 1), 0);
	for (int y = 0; y < sky.h; y++) {
		uint32_t rowsum = 0, rowpairs = 0;
		for (int x = 0; x < W; x++) {
			const uint8_t* a = at(x, y);
			if (inner(x, y)) {
				if (inner(x + 1, y)) {
					const uint8_t* b = at(x + 1, y);
					rowsum += (uint32_t)std::min(std::abs(a[0] - b[0]) + std::abs(a[1] - b[1]) + std::abs(a[2] - b[2]), 127);
					rowpairs++;
				}
				if (inner(x, y + 1)) {
					const uint8_t* b = at(x, y + 1);
					rowsum += (uint32_t)std::min(std::abs(a[0] - b[0]) + std::abs(a[1] - b[1]) + std::abs(a[2] - b[2]), 127);
					rowpairs++;
				}
			}
			integ[size_t(y + 1) * (W + 1) + x + 1] = integ[size_t(y) * (W + 1) + x + 1] + rowsum;
			pairs[size_t(y + 1) * (W + 1) + x + 1] = pairs[size_t(y) * (W + 1) + x + 1] + rowpairs;
		}
	}
	auto texture = [&](int x, int y) { // (the mean of two neighbour changes, as a pixel has)
		int x0 = std::max(0, x - 4), x1 = std::min(W, x + 5), y0 = std::max(0, y - 4), y1 = std::min(sky.h, y + 5);
		auto box = [&](const std::vector<uint32_t>& I) {
			return I[size_t(y1) * (W + 1) + x1] - I[size_t(y0) * (W + 1) + x1] - I[size_t(y1) * (W + 1) + x0] + I[size_t(y0) * (W + 1) + x0];
		};
		uint32_t n = box(pairs);
		return n ? 2.0f * box(integ) / n : 0.0f;
	};

	// the sky's brightness around each pixel (sky pixels within 7 px): the
	// night sky takes its colour from this rather than from the pixel
	// itself, so the pale halo painted along the edges of things does not
	// come out as a light rim round the bell tower and the pyramid
	std::vector<uint32_t> lumI(size_t(W + 1) * (sky.h + 1), 0), cntI(size_t(W + 1) * (sky.h + 1), 0);
	for (int y = 0; y < sky.h; y++) {
		uint32_t rl = 0, rc = 0;
		for (int x = 0; x < W; x++) {
			if (mask[size_t(y) * W + x]) {
				const uint8_t* p = at(x, y);
				rl += (p[0] + p[1] + p[2]) / 3;
				rc++;
			}
			lumI[size_t(y + 1) * (W + 1) + x + 1] = lumI[size_t(y) * (W + 1) + x + 1] + rl;
			cntI[size_t(y + 1) * (W + 1) + x + 1] = cntI[size_t(y) * (W + 1) + x + 1] + rc;
		}
	}
	auto skyLum = [&](int x, int y, float own) {
		int x0 = std::max(0, x - 7), x1 = std::min(W, x + 8), y0 = std::max(0, y - 7), y1 = std::min(sky.h, y + 8);
		auto box = [&](const std::vector<uint32_t>& I) {
			return I[size_t(y1) * (W + 1) + x1] - I[size_t(y0) * (W + 1) + x1] - I[size_t(y1) * (W + 1) + x0] + I[size_t(y0) * (W + 1) + x0];
		};
		uint32_t n = box(cntI);
		return n ? (float)box(lumI) / n : own;
	};
	// a soft edge: each sky pixel's coverage is the share of sky among it
	// and its eight neighbours, so the border blends into what it meets
	auto cover = [&](int x, int y) {
		int n = 0, s = 0;
		for (int j = -1; j <= 1; j++)
			for (int i = -1; i <= 1; i++) {
				int xx = x + i, yy = y + j;
				if (xx < 0 || yy < 0 || xx >= W || yy >= H)
					continue;
				n++;
				s += mask[size_t(yy) * W + xx];
			}
		return n ? s * 255 / n : 0;
	};
	// the lowest blue (or cloud) sky in each column, the most within 20 px
	// either side: traced pixels below it that are neither are the haze
	// over the dunes and the land at the skyline, which the trace runs into
	// through the paint's soft gradient. They take the scene's light, not
	// the sky's (in the sky's colours they came out as orange flecks at
	// dusk and blue patches at night). Warm paint above blue sky, like the
	// sunset's glow over the west palms, stays sky.
	std::vector<int> lowBlue(W, -1);
	{
		std::vector<int> raw(W, -1);
		for (int x = 0; x < W; x++)
			for (int y = H - 1; y >= 0; y--)
				if (mask[size_t(y) * W + x] && (skyColoured(at(x, y)) || cloudy(at(x, y)))) {
					raw[x] = y;
					break;
				}
		for (int x = 0; x < W; x++)
			for (int k = std::max(0, x - 20); k <= std::min(W - 1, x + 20); k++)
				lowBlue[x] = std::max(lowBlue[x], raw[k]);
	}
	std::mt19937 rng(7);
	std::uniform_real_distribution<float> uni(0.0f, 1.0f);
	for (int layer = 0; layer < 3; layer++) {
		Image out;
		out.width = sky.w;
		out.height = sky.h;
		out.format = if_rgba32;
		out.data = shared_array<uint8_t>(size_t(sky.w) * sky.h * 4);
		uint8_t* o = out.data.data();
		for (int y = 0; y < sky.h; y++)
			for (int x = 0; x < sky.w; x++) {
				uint8_t* d = o + (size_t(y) * sky.w + x) * 4;
				// how strongly the time of day lights this pixel: fully down
				// to the column's lowest sky, then fading out
				float strength = 0.0f;
				if (bottom[x] >= 0)
					strength = y <= bottom[x] ? 1.0f : std::max(0.0f, 1.0f - (y - bottom[x]) / fade);
				int c = cover(x, y);
				if (strength <= 0.0f && !c) {
					d[0] = d[1] = d[2] = d[3] = 0;
					continue;
				}
				const uint8_t* p = at(x, y);
				float r = p[0], g = p[1], b = p[2];
				// the scene in that light: a gentle grade, so leaves stay leaves
				float sr, sg, sb;
				// the sky itself: its own colours
				float kr, kg, kb;
				if (layer == 0) { // dawn: rosy gold
					sr = r * 1.04f + 8; sg = g * 0.90f + 2; sb = b * 0.84f - 4;
					kr = r * 1.06f + 38; kg = g * 0.86f + 12; kb = b * 0.80f - 8;
				} else if (layer == 1) { // dusk: warm, a little darker
					sr = r * 1.00f + 6; sg = g * 0.78f; sb = b * 0.66f - 4;
					kr = r * 1.12f + 34; kg = g * 0.72f - 4; kb = b * 0.62f - 18;
				} else { // night: dark and blue; the sky deep blue with stars
					sr = r * 0.30f + 4; sg = g * 0.36f + 6; sb = b * 0.58f + 18;
					float lum = skyLum(x, y, (r + g + b) / 3);
					kr = lum * 0.16f + 6; kg = lum * 0.20f + 10; kb = lum * 0.42f + 32;
					if (c == 255 && !cloudy(p) && (y <= lowBlue[x] || skyColoured(p)) && uni(rng) < 0.0012f) {
						float st = 150 + uni(rng) * 105;
						kr = st; kg = st; kb = std::min(255.0f, st * 1.05f);
					}
				}
				float w = mask[size_t(y) * W + x] && (y <= lowBlue[x] || skyColoured(p) || cloudy(p)) ? 1.0f : 0.0f; // (a sky pixel is wholly sky: blending its edge with the scene's grade left a pale rim)
				if (w > 0.0f) {
					float t = texture(x, y);
					w *= std::clamp((12.0f - t) / 7.0f, 0.0f, 1.0f); // full below 5, none above 12
				}
				float R = sr + (kr - sr) * w, G = sg + (kg - sg) * w, B = sb + (kb - sb) * w;
				float a = std::max(strength, w);
				d[0] = (uint8_t)std::clamp(R, 0.0f, 255.0f);
				d[1] = (uint8_t)std::clamp(G, 0.0f, 255.0f);
				d[2] = (uint8_t)std::clamp(B, 0.0f, 255.0f);
				d[3] = (uint8_t)std::lround(std::clamp(a, 0.0f, 1.0f) * 255);
			}
		// developer aid: OPENC2E_SKY_DUMP=<prefix> writes each layer laid over
		// the background as <prefix><layer>.ppm, and the sky mask as
		// <prefix>mask.pgm
		if (const char* dump = std::getenv("OPENC2E_SKY_DUMP")) {
			if (FILE* f = std::fopen(fmt::format("{}{}.ppm", dump, layer).c_str(), "wb")) {
				std::fprintf(f, "P6 %d %d 255\n", sky.w, sky.h);
				for (int y = 0; y < sky.h; y++)
					for (int x = 0; x < sky.w; x++) {
						const uint8_t* d = o + (size_t(y) * sky.w + x) * 4;
						const uint8_t* p = at(x, y);
						float al = d[3] / 255.0f;
						uint8_t c[3];
						for (int ch = 0; ch < 3; ch++)
							c[ch] = (uint8_t)std::lround(p[ch] + (d[ch] - p[ch]) * al);
						std::fwrite(c, 1, 3, f);
					}
				std::fclose(f);
			}
			if (layer == 0)
				if (FILE* f = std::fopen(fmt::format("{}mask.pgm", dump).c_str(), "wb")) {
					std::fprintf(f, "P5 %d %d 255\n", W, H);
					for (uint8_t v : mask) {
						uint8_t c = v ? 255 : 0;
						std::fwrite(&c, 1, 1, f);
					}
					std::fclose(f);
				}
			if (layer == 0)
				if (FILE* f = std::fopen(fmt::format("{}orig.ppm", dump).c_str(), "wb")) {
					std::fprintf(f, "P6 %d %d 255\n", W, H);
					std::fwrite(px, 1, size_t(W) * H * 3, f);
					std::fclose(f);
				}
		}
		sky.layers[layer].images = {out};
		sky.layers[layer].textures.resize(1);
	}
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
	fmt::print("* Sky: {} px of sky in {}, {}x{} at {},{} ({} ms)\n", count, bkgd.getName(), sky.w, sky.h, sky.x0, sky.y0, ms);
}

} // namespace

void draw(RenderTarget* surface, MetaRoom* m, int destx, int desty) {
	if (engine.version != 2 || !m)
		return;
	auto bkgd = m->getCurrentBackground();
	if (!bkgd)
		return;
	Sky& sky = s_skies[bkgd->getName()];
	if (!sky.built)
		build(sky, *bkgd);
	if (!sky.any)
		return;

	// fade between the last time of day's layer and this one's
	static int s_from = -2, s_to = -2;
	static unsigned int s_changed = 0;
	int want = layerFor(world.timeofday);
	unsigned int now = world.worldtickcount;
	if (s_to == -2) { // first frame: no fade
		s_from = s_to = want;
		s_changed = now;
	} else if (want != s_to) {
		s_from = s_to;
		s_to = want;
		s_changed = now;
	}
	const float fadeTicks = 300.0f; // about 30 seconds at C2's speed
	float f = std::clamp((now - s_changed) / fadeTicks, 0.0f, 1.0f);
	if (std::getenv("OPENC2E_SKY_LAYER")) { // developer aid: =0 dawn, 1 dusk, 2 night
		s_from = s_to = std::atoi(std::getenv("OPENC2E_SKY_LAYER"));
		f = 1.0f;
	}

	auto drawLayer = [&](int layer, float alpha) {
		if (layer < 0 || alpha <= 0.0f || sky.layers[layer].images.empty())
			return;
		RenderOptions opt;
		opt.alpha = (uint8_t)std::lround(alpha * 255);
		for (int pass = 0; pass < (m->wraparound() ? 2 : 1); pass++)
			surface->renderCreaturesImage(sky.layers[layer], 0, destx + sky.x0 + pass * (int)m->width(), desty + sky.y0, opt);
	};
	if (s_from == s_to) {
		drawLayer(s_to, 1.0f);
	} else {
		drawLayer(s_from, 1.0f - f);
		drawLayer(s_to, f);
	}
}

} // namespace SkyTint
