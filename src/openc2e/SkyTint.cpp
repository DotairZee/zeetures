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

// The light through the day (owner, 2026-11-11: a filter over the whole
// view rather than a recoloured sky). Everything in the world view, the
// background, objects and creatures alike, is multiplied by a colour for
// C2's time of day: rosy at dawn, warm at dusk, dark blue at night, none
// in the morning and afternoon. Caves and the underground get half of it:
// how much of the outdoors each spot is comes from the painted sky, traced
// once per background (full down to each column's lowest sky, fading over
// 400 px below to half), kept as a map at 1/8 size and stretched smoothly
// over the background. The hand and speech bubbles stay unfiltered.
struct Sky {
	bool built = false, any = false;
	int W = 0, H = 0; // the background's size
	int mw = 0, mh = 0; // the map's size (cells of 8 px)
	std::vector<float> share; // per cell: how much of the light applies (0.5-1)
	creaturesImage filter; // the colour to multiply by, per cell
	float shown[3] = {-1, -1, -1}; // the colour the filter was made for
};
std::map<std::string, Sky> s_skies;
const int CELL = 8;

// the colour each of C2's times of day multiplies the outdoors by
std::array<float, 3> lightFor(int timeofday) {
	switch (timeofday) {
		case 0: return {1.00f, 0.90f, 0.84f}; // dawn: rosy gold
		case 3: return {1.00f, 0.80f, 0.70f}; // evening: warm, pinker
		case 4: return {0.32f, 0.38f, 0.64f}; // night: dark and blue
		default: return {1.0f, 1.0f, 1.0f}; // morning, afternoon: as painted
	}
}

void build(Sky& sky, creaturesImage& bkgd, bool wraps) {
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
	// there, widened (the most within 250 px either side, so the ground under
	// a tree's canopy counts as outdoors) and smoothed, so
	// the grade's edge has no steps. Columns with no sky near them: none.
	std::vector<float> bottom(W, -1.0f);
	for (int x = 0; x < W; x++)
		for (int y = H - 1; y >= 0; y--)
			if (mask[size_t(y) * W + x]) {
				bottom[x] = (float)y;
				break;
			}
	{
		// (in a world that wraps, across the seam too: stopping at the
		// picture's edges left a hard edge in the light there, by the
		// volcano)
		auto col = [&](int k) { return wraps ? ((k % W) + W) % W : std::clamp(k, 0, W - 1); };
		std::vector<float> wide(W, -1.0f), smooth(W, -1.0f);
		for (int x = 0; x < W; x++)
			for (int k = x - 250; k <= x + 250; k++)
				if (wraps || (k >= 0 && k < W))
					wide[x] = std::max(wide[x], bottom[col(k)]);
		for (int x = 0; x < W; x++) {
			float sum = 0;
			int n = 0;
			for (int k = x - 60; k <= x + 60; k++)
				if ((wraps || (k >= 0 && k < W)) && wide[col(k)] >= 0) {
					sum += wide[col(k)];
					n++;
				}
			smooth[x] = n ? sum / n : -1.0f;
		}
		bottom = smooth;
	}
	const float fade = 400.0f; // the outdoors fades to the caves' share over this far below the sky
	const float caves = 0.5f; // the share of the light in caves and underground
	sky.W = W;
	sky.H = H;
	sky.mw = (W + CELL - 1) / CELL;
	sky.mh = (H + CELL - 1) / CELL;
	sky.share.assign(size_t(sky.mw) * sky.mh, caves);
	for (int cy = 0; cy < sky.mh; cy++)
		for (int cx = 0; cx < sky.mw; cx++) {
			int x = std::min(W - 1, cx * CELL + CELL / 2), y = cy * CELL + CELL / 2;
			float out = 0.0f;
			if (bottom[x] >= 0)
				out = y <= bottom[x] ? 1.0f : std::max(0.0f, 1.0f - (y - bottom[x]) / fade);
			sky.share[size_t(cy) * sky.mw + cx] = caves + (1.0f - caves) * out;
		}
	// developer aid: OPENC2E_SKY_DUMP=<prefix> writes the sky trace as
	// <prefix>mask.pgm and the outdoors map as <prefix>share.pgm
	if (const char* dump = std::getenv("OPENC2E_SKY_DUMP")) {
		if (FILE* f = std::fopen(fmt::format("{}mask.pgm", dump).c_str(), "wb")) {
			std::fprintf(f, "P5 %d %d 255\n", W, H);
			for (uint8_t v : mask) {
				uint8_t c = v ? 255 : 0;
				std::fwrite(&c, 1, 1, f);
			}
			std::fclose(f);
		}
		if (FILE* f = std::fopen(fmt::format("{}share.pgm", dump).c_str(), "wb")) {
			std::fprintf(f, "P5 %d %d 255\n", sky.mw, sky.mh);
			for (float v : sky.share) {
				uint8_t c = (uint8_t)std::lround(v * 255);
				std::fwrite(&c, 1, 1, f);
			}
			std::fclose(f);
		}
	}
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
	fmt::print("* Light: {} px of sky in {}, outdoors map {}x{} ({} ms)\n", count, bkgd.getName(), sky.mw, sky.mh, ms);
}

// the filter for a colour: per cell, 1 - share x (1 - colour)
void makeFilter(Sky& sky, const std::array<float, 3>& c) {
	Image out;
	out.width = sky.mw;
	out.height = sky.mh;
	out.format = if_rgba32;
	out.data = shared_array<uint8_t>(size_t(sky.mw) * sky.mh * 4);
	uint8_t* o = out.data.data();
	for (size_t i = 0; i < sky.share.size(); i++) {
		float sh = sky.share[i];
		for (int ch = 0; ch < 3; ch++)
			o[i * 4 + ch] = (uint8_t)std::lround(std::clamp(1.0f - sh * (1.0f - c[ch]), 0.0f, 1.0f) * 255);
		o[i * 4 + 3] = 255;
	}
	sky.filter.images = {out};
	sky.filter.textures.clear();
	sky.filter.textures.resize(1);
	for (int ch = 0; ch < 3; ch++)
		sky.shown[ch] = c[ch];
}

} // namespace

float outdoors(MetaRoom* m, int x0, int y0, int x1, int y1) {
	if (engine.version != 2 || !m)
		return -1.0f;
	auto bkgd = m->getCurrentBackground();
	if (!bkgd)
		return -1.0f;
	auto it = s_skies.find(bkgd->getName());
	if (it == s_skies.end() || !it->second.built)
		return -1.0f;
	const Sky& sky = it->second;
	if (!sky.any)
		return 0.0f;
	// (the map's cells are in the background's terms, from the metaroom's corner)
	int cx0 = std::max(0, (x0 - (int)m->x()) / CELL), cy0 = std::max(0, (y0 - (int)m->y()) / CELL);
	int cx1 = std::min(sky.mw - 1, (x1 - (int)m->x()) / CELL), cy1 = std::min(sky.mh - 1, (y1 - (int)m->y()) / CELL);
	float sum = 0.0f;
	int n = 0;
	for (int cy = cy0; cy <= cy1; cy++)
		for (int cx = cx0; cx <= cx1; cx++) {
			sum += sky.share[size_t(cy) * sky.mw + cx];
			n++;
		}
	if (!n)
		return 0.0f;
	return std::clamp((sum / n - 0.5f) * 2.0f, 0.0f, 1.0f); // the share runs 0.5 (caves) to 1
}

void draw(RenderTarget* surface, MetaRoom* m, int destx, int desty) {
	if (engine.version != 2 || !m)
		return;
	auto bkgd = m->getCurrentBackground();
	if (!bkgd)
		return;
	Sky& sky = s_skies[bkgd->getName()];
	if (!sky.built)
		build(sky, *bkgd, m->wraparound());
	if (!sky.any)
		return;

	// fade from the last time of day's light to this one's
	static int s_from = -2, s_to = -2;
	static unsigned int s_changed = 0;
	int want = (int)world.timeofday;
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
	if (const char* l = std::getenv("OPENC2E_SKY_LAYER")) { // developer aid: =0 dawn, 1 dusk, 2 night, -1 none
		int k = std::atoi(l);
		s_from = s_to = k == 0 ? 0 : k == 1 ? 3 : k == 2 ? 4 : 1;
		f = 1.0f;
	}
	auto a = lightFor(s_from), b = lightFor(s_to);
	std::array<float, 3> c;
	for (int ch = 0; ch < 3; ch++)
		c[ch] = a[ch] + (b[ch] - a[ch]) * f;
	if (c[0] >= 0.999f && c[1] >= 0.999f && c[2] >= 0.999f)
		return; // daylight: nothing to do
	bool changed = false;
	for (int ch = 0; ch < 3; ch++)
		if (std::fabs(c[ch] - sky.shown[ch]) > 0.5f / 255)
			changed = true;
	if (changed)
		makeFilter(sky, c);

	RenderOptions opt;
	opt.multiply = true;
	opt.smooth = true;
	opt.override_drawsize = true;
	opt.overridden_drawwidth = sky.W;
	opt.overridden_drawheight = sky.H;
	for (int pass = 0; pass < (m->wraparound() ? 2 : 1); pass++)
		surface->renderCreaturesImage(sky.filter, 0, destx + pass * (int)m->width(), desty, opt);
}

} // namespace SkyTint
