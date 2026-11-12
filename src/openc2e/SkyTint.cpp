#include "SkyTint.h"

#include "Engine.h"
#include "MetaRoom.h"
#include "World.h"
#include "common/backend/Backend.h"
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
		return std::abs(a[0] - b[0]) + std::abs(a[1] - b[1]) + std::abs(a[2] - b[2]) < tol && (b[0] + b[1] + b[2]) / 3 > 60;
	});
	// sky walled in by what stands in front of it (seen through the bell
	// tower's arch): sky-coloured pixels close to the sky's own colour in
	// that row, grown on the same way
	{
		std::vector<std::array<long, 4>> row(H, {0, 0, 0, 0});
		for (int y = 0; y < H; y++)
			for (int x = 0; x < W; x++)
				if (mask[size_t(y) * W + x]) {
					const uint8_t* p = at(x, y);
					row[y][0] += p[0];
					row[y][1] += p[1];
					row[y][2] += p[2];
					row[y][3]++;
				}
		// (compared with the sky within 300 px along the row: the painted
		// sky runs from blue in the west to pink in the east)
		std::vector<std::array<long, 4>> pre(W + 1);
		for (int y = 0; y < H; y++) {
			if (row[y][3] < 200)
				continue;
			pre[0] = {0, 0, 0, 0};
			for (int x = 0; x < W; x++) {
				pre[x + 1] = pre[x];
				if (mask[size_t(y) * W + x]) {
					const uint8_t* p = at(x, y);
					pre[x + 1][0] += p[0];
					pre[x + 1][1] += p[1];
					pre[x + 1][2] += p[2];
					pre[x + 1][3]++;
				}
			}
			for (int x = 0; x < W; x++) {
				if (mask[size_t(y) * W + x])
					continue;
				int a0 = std::max(0, x - 300), a1 = std::min(W, x + 300);
				long n = pre[a1][3] - pre[a0][3];
				if (n < 40)
					continue;
				int mr = (pre[a1][0] - pre[a0][0]) / n, mg = (pre[a1][1] - pre[a0][1]) / n, mb = (pre[a1][2] - pre[a0][2]) / n;
				const uint8_t* p = at(x, y);
				if (skyColoured(p) && std::abs(p[0] - mr) + std::abs(p[1] - mg) + std::abs(p[2] - mb) < 36) {
					mask[size_t(y) * W + x] = 1;
					q.push_back({x, y});
				}
			}
		}
		grow([&](const uint8_t* a, const uint8_t* b) {
			return std::abs(a[0] - b[0]) + std::abs(a[1] - b[1]) + std::abs(a[2] - b[2]) < tol && (b[0] + b[1] + b[2]) / 3 > 60;
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

	// the sky's extent
	int minx = W, miny = H, maxx = -1, maxy = -1;
	size_t count = 0;
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++)
			if (mask[size_t(y) * W + x]) {
				minx = std::min(minx, x);
				maxx = std::max(maxx, x);
				miny = std::min(miny, y);
				maxy = std::max(maxy, y);
				count++;
			}
	if (count < 1000)
		return;
	sky.any = true;
	sky.x0 = minx;
	sky.y0 = miny;
	sky.w = maxx - minx + 1;
	sky.h = maxy - miny + 1;

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
				int gx = sky.x0 + x, gy = sky.y0 + y;
				uint8_t* d = o + (size_t(y) * sky.w + x) * 4;
				int c = cover(gx, gy);
				if (!c) {
					d[0] = d[1] = d[2] = d[3] = 0;
					continue;
				}
				const uint8_t* p = at(gx, gy);
				float r = p[0], g = p[1], b = p[2], R, G, B;
				if (layer == 0) { // dawn: warm, a rosy gold
					R = r * 1.06f + 38;
					G = g * 0.86f + 12;
					B = b * 0.80f - 8;
				} else if (layer == 1) { // dusk: orange-red
					R = r * 1.12f + 34;
					G = g * 0.72f - 4;
					B = b * 0.62f - 18;
				} else { // night: a deep blue, with stars
					float lum = (r + g + b) / 3;
					R = lum * 0.16f + 6;
					G = lum * 0.20f + 10;
					B = lum * 0.42f + 32;
					if (c == 255 && !cloudy(p) && uni(rng) < 0.0012f) {
						float s = 150 + uni(rng) * 105;
						R = s;
						G = s;
						B = std::min(255.0f, s * 1.05f);
					}
				}
				d[0] = (uint8_t)std::clamp(R, 0.0f, 255.0f);
				d[1] = (uint8_t)std::clamp(G, 0.0f, 255.0f);
				d[2] = (uint8_t)std::clamp(B, 0.0f, 255.0f);
				d[3] = (uint8_t)c;
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
