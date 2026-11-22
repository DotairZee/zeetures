// zvps, zvpc, zvpr, zvdr: art/volcano_passage.py. The passage between the
// volcano's east end and the potion room (owner, 2026-11-13).
//
// The volcano's east ledge (room 167, walking line y 2189) and the potion
// room across the world's east-west seam (room 105, floor y 2329) were
// parted by a sealed rock pillar (room 213, x 8240-8352).
// additions/volcano-passage.cos opens a way through it and places
// these:
//
//   zvps  the seam hidden, x 8272 to 80 across it, y 1700-2399: three bands
//         that cross-fade (above the passage the volcano rock darkens into
//         the corner; beside it the two rocks meet along a ragged fissure
//         kept east of x 8 in the potion room; the floor band joins the two
//         floors); each side carried past the seam by quilting a clean patch
//         of its own rock. Top-left 8272, 1700.
//   zvpc  the passage carved out of the rock, mostly by light: darker inside,
//         darkest under its ceiling, a warm lit lip, lava light spilling in
//         at the door end. Top-left 8215, 2010.
//   zvpr  the ramp: the ground rock both rooms stand on, shaded darker, its
//         walking surface the ledge's crust with a few embers, its foot
//         fading into the floor. Top-left 8205, 2188.
//   zvdr  the volcano's own hatch door (dor2), mirrored: 5 frames, 0 shut,
//         4 open. Top-left 8055, 2028.
//
// The recipe works in numpy floats; this keeps its float32 / float64 steps
// as numpy promotes them, its random numbers (numpy's default_rng: PCG64
// seeded through SeedSequence) and PIL's GaussianBlur and alpha_composite.

#include "MadeArt.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace MadeArt {

namespace {

// --- numpy's default_rng(seed) (numpy 1.26): SeedSequence, PCG64 XSL-RR ---

struct U128 {
	uint64_t hi = 0, lo = 0;
};

U128 add128(U128 a, U128 b) {
	U128 r;
	r.lo = a.lo + b.lo;
	r.hi = a.hi + b.hi + (r.lo < a.lo ? 1 : 0);
	return r;
}

// 64 x 64 -> 128
U128 mul64(uint64_t a, uint64_t b) {
	uint64_t a0 = a & 0xffffffffu, a1 = a >> 32, b0 = b & 0xffffffffu, b1 = b >> 32;
	uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
	uint64_t mid = (p00 >> 32) + (p01 & 0xffffffffu) + (p10 & 0xffffffffu);
	U128 r;
	r.lo = (mid << 32) | (p00 & 0xffffffffu);
	r.hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
	return r;
}

U128 mul128(U128 a, U128 b) {
	U128 r = mul64(a.lo, b.lo);
	r.hi += a.hi * b.lo + a.lo * b.hi;
	return r;
}

class NumpyRng {
  public:
	explicit NumpyRng(uint64_t seed) {
		// SeedSequence(seed).generate_state(4, uint64)
		const uint32_t INIT_A = 0x43b0d7e5, MULT_A = 0x931e8875, INIT_B = 0x8b51f9dd, MULT_B = 0x58f38ded;
		const uint32_t MIX_MULT_L = 0xca01f9dd, MIX_MULT_R = 0x4973f715;
		std::vector<uint32_t> entropy;
		if (seed == 0)
			entropy.push_back(0);
		while (seed) {
			entropy.push_back(uint32_t(seed));
			seed >>= 32;
		}
		uint32_t hc = INIT_A;
		auto hashmix = [&](uint32_t v) {
			v ^= hc;
			hc *= MULT_A;
			v *= hc;
			v ^= v >> 16;
			return v;
		};
		auto mix = [&](uint32_t x, uint32_t y) {
			uint32_t r = MIX_MULT_L * x - MIX_MULT_R * y;
			r ^= r >> 16;
			return r;
		};
		std::array<uint32_t, 4> pool;
		for (size_t i = 0; i < 4; i++)
			pool[i] = hashmix(i < entropy.size() ? entropy[i] : 0);
		for (size_t s = 0; s < 4; s++)
			for (size_t d = 0; d < 4; d++)
				if (s != d)
					pool[d] = mix(pool[d], hashmix(pool[s]));
		for (size_t s = 4; s < entropy.size(); s++)
			for (size_t d = 0; d < 4; d++)
				pool[d] = mix(pool[d], hashmix(entropy[s]));
		uint32_t hb = INIT_B;
		uint32_t st[8];
		for (int i = 0; i < 8; i++) {
			uint32_t v = pool[i % 4];
			v ^= hb;
			hb *= MULT_B;
			v *= hb;
			v ^= v >> 16;
			st[i] = v;
		}
		uint64_t w[4];
		for (int k = 0; k < 4; k++)
			w[k] = uint64_t(st[2 * k]) | (uint64_t(st[2 * k + 1]) << 32);
		// pcg64_set_seed: state w0:w1, sequence w2:w3
		U128 initstate{w[0], w[1]}, initseq{w[2], w[3]};
		state = U128{};
		inc = U128{(initseq.hi << 1) | (initseq.lo >> 63), (initseq.lo << 1) | 1};
		step();
		state = add128(state, initstate);
		step();
	}

	uint64_t next64() {
		step();
		uint64_t x = state.hi ^ state.lo;
		unsigned rot = unsigned(state.hi >> 58);
		return (x >> rot) | (x << ((64 - rot) & 63));
	}

	// numpy keeps the other half of a 64-bit draw for the next 32-bit one
	uint32_t next32() {
		if (hasU32) {
			hasU32 = false;
			return u32;
		}
		uint64_t n = next64();
		hasU32 = true;
		u32 = uint32_t(n >> 32);
		return uint32_t(n);
	}

	// Generator.random()
	double random() {
		return double(next64() >> 11) * (1.0 / 9007199254740992.0);
	}

	// Generator.integers(low, high): Lemire's method on 32-bit draws
	int64_t integers(int64_t low, int64_t high) {
		uint64_t rng = uint64_t(high - 1 - low);
		if (rng == 0)
			return low;
		if (rng > 0xffffffffu)
			throw std::runtime_error("integers: range too large");
		if (rng == 0xffffffffu)
			return low + next32();
		uint32_t excl = uint32_t(rng) + 1;
		uint64_t m = uint64_t(next32()) * excl;
		uint32_t leftover = uint32_t(m);
		if (leftover < excl) {
			uint32_t threshold = (0xffffffffu - uint32_t(rng)) % excl;
			while (leftover < threshold) {
				m = uint64_t(next32()) * excl;
				leftover = uint32_t(m);
			}
		}
		return low + int64_t(m >> 32);
	}

  private:
	U128 state, inc;
	bool hasU32 = false;
	uint32_t u32 = 0;

	void step() {
		const U128 MULT{0x2360ed051fc65da4ull, 0x4385df649fccf645ull};
		state = add128(mul128(state, MULT), inc);
	}
};

// --- float pictures ---

// RGB floats, as a numpy (h, w, 3) float32 array
struct Rgb32 {
	int w = 0, h = 0;
	std::vector<float> v;
	Rgb32() = default;
	Rgb32(int w_, int h_)
		: w(w_), h(h_), v(size_t(w_) * h_ * 3, 0.0f) {}
	float* at(int x, int y) { return &v[(size_t(y) * w + x) * 3]; }
	const float* at(int x, int y) const { return &v[(size_t(y) * w + x) * 3]; }
};

Rgb32 toFloat(const Rgba& im) {
	Rgb32 out(im.w, im.h);
	for (int y = 0; y < im.h; y++)
		for (int x = 0; x < im.w; x++)
			for (int c = 0; c < 3; c++)
				out.at(x, y)[c] = float(im.at(x, y)[c]);
	return out;
}

// region(...).convert('RGB') as float32
Rgb32 patch(int x0, int y0, int x1, int y1) {
	Rgba im = backgroundRegion(x0, y0, x1, y1);
	if (!im.w)
		throw std::runtime_error("no background");
	return toFloat(im);
}

template <typename T>
T clip(T v, T lo, T hi) {
	// np.clip: maximum then minimum
	return std::min(std::max(v, lo), hi);
}

// numpy's array.astype(uint8) of a value already clipped to 0..255
uint8_t u8(double v) {
	return uint8_t(int(v));
}

float sq(float v) {
	return v * v;
}
double sq(double v) {
	return v * v;
}

// smooth 1-D value noise (float64)
std::vector<double> smoothNoise(int n, int cell, uint64_t seed) {
	NumpyRng r(seed);
	std::vector<double> pts(static_cast<size_t>(n / cell + 3));
	for (auto& p : pts)
		p = r.random();
	std::vector<double> out(static_cast<size_t>(n));
	for (int k = 0; k < n; k++) {
		double xs = double(k) / cell;
		int i = int(xs);
		double f = xs - i;
		f = f * f * (3 - 2 * f);
		out[size_t(k)] = pts[size_t(i)] * (1 - f) + pts[size_t(i) + 1] * f;
	}
	return out;
}

// a w x h picture quilted from blocks of the patch (float32)
Rgb32 quilt(const Rgb32& patch, int w, int h, uint64_t seed, int block = 36, int feather = 8) {
	NumpyRng r(seed);
	Rgb32 acc(w, h);
	std::vector<float> wsum(size_t(w) * h, 0.0f);
	int ph = patch.h, pw = patch.w;
	int step = block - feather;
	std::vector<float> wt(size_t(block) * block);
	for (int yy = 0; yy < block; yy++)
		for (int xx = 0; xx < block; xx++) {
			int edge = std::min({xx, yy, block - 1 - xx, block - 1 - yy});
			wt[size_t(yy) * block + xx] = clip((float(edge) + 1.0f) / float(feather), 0.05f, 1.0f);
		}
	for (int y0 = -feather; y0 < h; y0 += step)
		for (int x0 = -feather; x0 < w; x0 += step) {
			int sy = int(r.integers(0, ph - block));
			int sx = int(r.integers(0, pw - block));
			bool flip = r.random() < 0.5;
			int ys = std::max(0, y0), xs = std::max(0, x0);
			int ye = std::min(h, y0 + block), xe = std::min(w, x0 + block);
			if (ye <= ys || xe <= xs)
				continue;
			for (int y = ys; y < ye; y++)
				for (int x = xs; x < xe; x++) {
					int by = y - y0, bx = x - x0;
					float k = wt[size_t(by) * block + bx];
					const float* s = patch.at(flip ? sx + block - 1 - bx : sx + bx, sy + by);
					float* a = acc.at(x, y);
					for (int c = 0; c < 3; c++)
						a[c] += s[c] * k;
					wsum[size_t(y) * w + x] += k;
				}
		}
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			float d = std::max(wsum[size_t(y) * w + x], 1e-3f);
			float* a = acc.at(x, y);
			for (int c = 0; c < 3; c++)
				a[c] = a[c] / d;
		}
	return acc;
}

// PIL's Image.alpha_composite of src over dst at (dx, dy), in place
void alphaComposite(Rgba& dst, const Rgba& src, int dx, int dy) {
	const int PRECISION_BITS = 7;
	auto div255 = [](uint32_t a) { return ((a >> 8) + a) >> 8; };
	for (int y = 0; y < src.h; y++)
		for (int x = 0; x < src.w; x++) {
			int tx = x + dx, ty = y + dy;
			if (tx < 0 || ty < 0 || tx >= dst.w || ty >= dst.h)
				continue;
			const uint8_t* s = src.at(x, y);
			uint8_t* d = dst.at(tx, ty);
			if (s[3] == 0)
				continue;
			uint32_t blend = uint32_t(d[3]) * (255 - s[3]);
			uint32_t outa255 = uint32_t(s[3]) * 255 + blend;
			uint32_t coef1 = uint32_t(s[3]) * 255 * 255 * (1 << PRECISION_BITS) / outa255;
			uint32_t coef2 = 255 * (1 << PRECISION_BITS) - coef1;
			for (int c = 0; c < 3; c++) {
				uint32_t t = s[c] * coef1 + d[c] * coef2;
				d[c] = uint8_t(div255(t + (0x80 << PRECISION_BITS)) >> PRECISION_BITS);
			}
			d[3] = uint8_t(div255(outa255 + 0x80));
		}
}

// PIL's ImageFilter.GaussianBlur(radius) on an 8-bit picture: three box
// blurs across, then three down (Pillow's BoxBlur.c)
float gaussianBlurRadius(float radius, int passes) {
	float sigma2, L, l, a;
	sigma2 = radius * radius / passes;
	L = float(std::sqrt(12.0 * sigma2 + 1.0));
	l = float(std::floor((L - 1.0) / 2.0));
	a = (2 * l + 1) * (l * (l + 1) - 3 * sigma2);
	a /= 6 * (sigma2 - (l + 1) * (l + 1));
	return l + a;
}

void lineBoxBlur8(uint8_t* out, const uint8_t* in, int lastx, int radius, int edgeA, int edgeB, uint32_t ww, uint32_t fw) {
	uint32_t acc, bulk;
	auto move = [&](int sub, int add) { acc += in[add] - in[sub]; };
	auto far = [&](int left, int right) { bulk = (acc * ww) + (in[left] + in[right]) * fw; };
	auto save = [&](int x) { out[x] = uint8_t((bulk + (1 << 23)) >> 24); };
	acc = in[0] * (radius + 1);
	for (int x = 0; x < edgeA - 1; x++)
		acc += in[x];
	acc += in[lastx] * (radius - edgeA + 1);
	if (edgeA <= edgeB) {
		for (int x = 0; x < edgeA; x++) {
			move(0, x + radius);
			far(0, x + radius + 1);
			save(x);
		}
		for (int x = edgeA; x < edgeB; x++) {
			move(x - radius - 1, x + radius);
			far(x - radius - 1, x + radius + 1);
			save(x);
		}
		for (int x = edgeB; x <= lastx; x++) {
			move(x - radius - 1, lastx);
			far(x - radius - 1, lastx);
			save(x);
		}
	} else {
		for (int x = 0; x < edgeB; x++) {
			move(0, x + radius);
			far(0, lastx);
			save(x);
		}
		for (int x = edgeB; x < edgeA; x++) {
			move(0, lastx);
			far(0, lastx);
			save(x);
		}
		for (int x = edgeA; x <= lastx; x++) {
			move(x - radius - 1, lastx);
			far(x - radius - 1, lastx);
			save(x);
		}
	}
}

// rows of w bytes, blurred along each row
void horizontalBoxBlur(std::vector<uint8_t>& im, int w, int h, float floatRadius) {
	int radius = int(floatRadius);
	uint32_t ww = uint32_t(float(1 << 24) / (floatRadius * 2 + 1));
	uint32_t fw = ((1 << 24) - (radius * 2 + 1) * ww) / 2;
	int edgeA = std::min(radius + 1, w);
	int edgeB = std::max(w - radius - 1, 0);
	std::vector<uint8_t> line(static_cast<size_t>(w));
	for (int y = 0; y < h; y++) {
		lineBoxBlur8(line.data(), &im[size_t(y) * w], w - 1, radius, edgeA, edgeB, ww, fw);
		std::copy(line.begin(), line.end(), im.begin() + size_t(y) * w);
	}
}

std::vector<uint8_t> transpose(const std::vector<uint8_t>& im, int w, int h) {
	std::vector<uint8_t> t(im.size());
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++)
			t[size_t(x) * h + y] = im[size_t(y) * w + x];
	return t;
}

void gaussianBlur(std::vector<uint8_t>& im, int w, int h, float radius) {
	const int passes = 3;
	float r = gaussianBlurRadius(radius, passes);
	if (r == 0)
		return;
	for (int i = 0; i < passes; i++)
		horizontalBoxBlur(im, w, h, r);
	std::vector<uint8_t> t = transpose(im, w, h);
	for (int i = 0; i < passes; i++)
		horizontalBoxBlur(t, h, w, r);
	im = transpose(t, h, w);
}

// --- the recipe ---

// the ramp's walking line: from the ledge's lip to the potion room's floor
// (from where the lava slab stops on the ledge to just short of the potion
// room's bookshelf arch, nearly straight: about 33 degrees at its steepest;
// at 40 degrees creatures could not walk up it, owner 2026-11-13)
const int LIP_X = 8205, LIP_Y = 2189, FLOOR_Y = 2329;
const int RUN = (8352 - LIP_X) + 80; // to x 80 in the potion room

// the walking line's height at world x (float32)
float walkY(float wx) {
	float t = clip((wx - float(LIP_X)) / float(RUN), 0.0f, 1.0f);
	return float(LIP_Y) + (t * t * (3.0f - 2.0f * t) * 0.15f + t * 0.85f) * float(FLOOR_Y - LIP_Y);
}

// one band of the seam: the picture (float64 RGB) and its alpha
struct Band {
	int w = 0, h = 0;
	std::vector<double> img, a;
};

Band seamBand(bool soft, int y0, int y1, int half, const Rgb32& pl, const Rgb32& pr, int seed, int amp, int shift, double minb) {
	const int HALF = 80, W = 2 * HALF;
	int H = y1 - y0;
	Band out;
	out.w = W;
	out.h = H;
	out.img.assign(size_t(W) * H * 3, 0.0);
	out.a.assign(size_t(W) * H, 0.0);
	Rgb32 L = patch(8352 - HALF, y0, 8352, y1), R = patch(0, y0, HALF, y1);
	if (half < 0) {
		// the volcano side only (above the passage the machine room's machines
		// stand right at the seam): its rock darkens into the corner instead
		for (int y = 0; y < H; y++)
			for (int x = 0; x < W; x++) {
				double corner = clip((x - (HALF - 34)) / 34.0, 0.0, 1.0);
				double k = 1 - 0.6 * std::pow(corner, 1.5);
				for (int c = 0; c < 3; c++) {
					double v = (x < HALF ? double(L.at(x, y)[c]) : 0.0) * k;
					out.img[(size_t(y) * W + x) * 3 + c] = clip(v, 0.0, 255.0);
				}
				out.a[size_t(y) * W + x] = clip((HALF - x) / 2.0, 0.0, 1.0) * clip((x - (HALF - 40)) / 8.0, 0.0, 1.0) * 1.0;
			}
		return out;
	}
	auto q = [&](const Rgb32& pt, int sd) {
		int blk = std::min({36, pt.h - 2, pt.w - 2});
		return quilt(pt, W, H, uint64_t(sd), blk, std::max(3, blk / 4));
	};
	// each picture fades into its own continuation over the last 24 px before
	// the seam (an abrupt switch there drew a hairline down the seam)
	Rgb32 qL = q(pl, seed), qR = q(pr, seed + 1);
	std::vector<double> n90 = smoothNoise(H, 90, uint64_t(seed + 2)), n25 = smoothNoise(H, 25, uint64_t(seed + 3)),
						n6 = smoothNoise(H, 6, uint64_t(seed + 4)), n40;
	if (!soft)
		n40 = smoothNoise(H, 40, uint64_t(seed + 5));
	for (int y = 0; y < H; y++) {
		double b = HALF + shift + (n90[size_t(y)] - 0.5) * amp + (n25[size_t(y)] - 0.5) * amp * 0.38 + (n6[size_t(y)] - 0.5) * 6;
		// (never west of x 8 in the potion room: its picture's own left border
		// lies at x 0)
		b = std::max(b, HALF + minb);
		for (int x = 0; x < W; x++) {
			float xs = float(x);
			float keepL = clip((float(HALF) - xs) / 24.0f, 0.0f, 1.0f); // 1 well west of the seam, 0 at it
			float keepR = clip((xs - float(HALF)) / 24.0f, 0.0f, 1.0f);
			double d = x - b;
			double w = 1 / (1 + std::exp(-d / (soft ? 22.0 : 5.0)));
			double k = 1;
			if (!soft) {
				double wid = 4.0 + 6.0 * n40[size_t(y)];
				k = 1 - 0.55 * std::exp(-sq(d / wid)) - 0.15 * std::exp(-sq(d / 28.0));
			}
			for (int c = 0; c < 3; c++) {
				float lf = x < HALF ? L.at(x, y)[c] : 0.0f;
				float rf = x >= HALF ? R.at(x - HALF, y)[c] : 0.0f;
				float lext = lf * keepL + qL.at(x, y)[c] * (1.0f - keepL);
				float rext = rf * keepR + qR.at(x, y)[c] * (1.0f - keepR);
				double v = double(lext) * (1 - w) + double(rext) * w;
				if (!soft)
					v *= k;
				out.img[(size_t(y) * W + x) * 3 + c] = clip(v, 0.0, 255.0);
			}
			// only `half` px each side of the seam: fade out beyond
			out.a[size_t(y) * W + x] = clip((half - std::abs(x - HALF + 0.5)) / 12.0, 0.0, 1.0) * 1.0;
		}
	}
	return out;
}

Rgba makeSeam(bool soft) {
	const int HALF = 80, Y0 = 1700, Y1 = 2399, W = 2 * HALF, HT = Y1 - Y0;
	std::vector<float> canvas(size_t(W) * HT * 3, 0.0f), acc(size_t(W) * HT, 0.0f);
	struct Spec {
		int y0, y1, half;
		std::array<int, 4> pl, pr;
		int seed, amp, shift;
		double minb;
	};
	const Spec bands[] = {
		// above the passage: the volcano's rock / the machine room's dark wall left of its cone machines
		{1700, 1860, -1, {8290, 1705, 8350, 1840}, {1, 1705, 26, 1830}, 41, 20, 0, -1e9},
		// beside the passage: the pillar's rock / the potion room's cracked stone
		{1830, 2345, 80, {8280, 2020, 8350, 2160}, {2, 1850, 74, 1935}, 11, 60, 24, 8},
		// the floor both rooms stand on
		{2315, 2399, 70, {8000, 2340, 8340, 2395}, {80, 2340, 380, 2395}, 61, 60, 0, -1e9},
	};
	for (auto& s : bands) {
		Rgb32 pl, pr;
		if (s.half >= 0) { // (the corner band uses neither)
			pl = patch(s.pl[0], s.pl[1], s.pl[2], s.pl[3]);
			pr = patch(s.pr[0], s.pr[1], s.pr[2], s.pr[3]);
		}
		Band b = seamBand(soft, s.y0, s.y1, s.half, pl, pr, s.seed, s.amp, s.shift, s.minb);
		int H = s.y1 - s.y0;
		for (int y = 0; y < H; y++) {
			// cross-fade with the bands above and below over 30 px
			double ramp = 1;
			if (s.y0 > Y0)
				ramp *= clip(y / 30.0, 0.0, 1.0);
			if (s.y1 < Y1)
				ramp *= clip((H - 1 - y) / 30.0, 0.0, 1.0);
			int cy = s.y0 - Y0 + y;
			for (int x = 0; x < W; x++) {
				double a = b.a[size_t(y) * W + x] * ramp;
				float* cv = &canvas[(size_t(cy) * W + x) * 3];
				for (int c = 0; c < 3; c++)
					cv[c] = float(double(cv[c]) * (1 - a) + b.img[(size_t(y) * W + x) * 3 + c] * a);
				float& ac = acc[size_t(cy) * W + x];
				ac = float(double(ac) + a * double(1.0f - ac));
			}
		}
	}
	Rgba out(W, HT);
	for (int y = 0; y < HT; y++)
		for (int x = 0; x < W; x++) {
			float ac = acc[size_t(y) * W + x];
			float d = std::max(ac, 1e-3f);
			uint8_t* p = out.at(x, y);
			for (int c = 0; c < 3; c++)
				p[c] = u8(clip(canvas[(size_t(y) * W + x) * 3 + c] / d, 0.0f, 255.0f));
			p[3] = u8(clip(ac, 0.0f, 1.0f) * 255.0f);
		}
	return out;
}

Rgba makeCarve(const Rgba& seamImg) {
	// (from just behind the door's post, x 8225: the hollow reaches the door;
	// it started 8 px past the post and rounded off there, owner 2026-11-13)
	const int X0 = 8215, Y0 = 2010; // world; the area runs across the seam into the potion room
	const int W = (8352 - X0) + 90, H = 2335 - Y0;
	// the rock as it looks with the seam transition (its picture: x 8272.., y 1845..)
	Rgba left = backgroundRegion(X0, Y0, 8352, Y0 + H), right = backgroundRegion(0, Y0, W - (8352 - X0), Y0 + H);
	if (!left.w || !right.w)
		throw std::runtime_error("no background");
	Rgba base(W, H);
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++) {
			const uint8_t* s = x < 8352 - X0 ? left.at(x, y) : right.at(x - (8352 - X0), y);
			std::copy(s, s + 4, base.at(x, y));
		}
	// (the soft seam: inside the passage the walls flow into each other, no fissure)
	alphaComposite(base, seamImg, 8272 - X0, 1700 - Y0);
	Rgb32 rock = toFloat(base);

	// the ramp's walking line (as the ramp sprite)
	std::vector<float> floor(static_cast<size_t>(W));
	for (int x = 0; x < W; x++)
		floor[size_t(x)] = walkY(float(x) + float(X0)) - float(Y0);
	// the hollow's ceiling: about 125 px above the walking line, arched at the
	// door end, a little rough
	std::vector<double> noise = smoothNoise(W, 30, 5), ceil(static_cast<size_t>(W));
	for (int x = 0; x < W; x++)
		ceil[size_t(x)] = std::max(double(floor[size_t(x)] - 125.0f) + (noise[size_t(x)] - 0.5) * 10, 6.0);
	const float warm[3] = {255, 150, 70};
	Rgba out(W, H);
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++) {
			double fl = floor[size_t(x)], ce = ceil[size_t(x)];
			double depthIn = double(float(y)) - ce; // below the ceiling line: inside
			double inside = 1 / (1 + std::exp(-depthIn / 2.0));
			// the hollow's ends fade: it opens from the door post and into the potion room's recess
			double ends = clip((float(W - 1) - float(x)) / 30.0f, 0.0f, 1.0f);
			// lighting: darker inside, darkest under the ceiling (its shadow); a warm lit
			// lip along the ceiling's edge, strongest by the volcano's glow
			double shadow = 0.34 - 0.18 * std::exp(-std::max(depthIn, 0.0) / 22) + 0.3 * std::pow(clip(depthIn / (fl - ce), 0.0, 1.0), 1.5);
			double glow = std::exp(-sq((depthIn + 2.0) / 2.8)) * double(clip(1.1f - float(x) / float(W), 0.25f, 1.0f)) * ends;
			// warm light from the lava spilling in at the door end, fading deeper in
			double mid = (ce + fl) / 2;
			double dist = std::sqrt(double(sq(float(x) - 25.0f)) + sq((double(float(y)) - mid) * 1.3)); // (from the door, x 8240)
			double spill = std::exp(-dist / 75) * inside * ends;
			// an outer shadow just above the lip (the rock face turning in)
			double outer = std::exp(-sq((depthIn + 6) / 4)) * ends;
			uint8_t* p = out.at(x, y);
			for (int c = 0; c < 3; c++) {
				double light = double(rock.at(x, y)[c]) * (1 - inside * (1 - shadow) * ends);
				light = light * (1 + 0.8 * glow) + double(warm[c] * 0.28f) * glow;
				light = light + (double(warm[c] * 0.55f) - light * 0.2) * (0.5 * spill);
				light *= (1 - 0.35 * outer);
				p[c] = u8(clip(light, 0.0, 255.0));
			}
			double a = clip(std::max(inside * ends, glow + outer) * 255, 0.0, 255.0);
			if (float(y) > floor[size_t(x)] + 4.0f) // nothing below the walking line (the ramp is there)
				a = 0;
			p[3] = u8(a);
		}
	return out;
}

Rgba makeRamp() {
	const int YTOP = LIP_Y, YEND = FLOOR_Y;
	const int W = RUN + 26, H = YEND - YTOP + 14;
	NumpyRng r(31);
	// body: the ground rock both rooms stand on, quilted
	Rgb32 body = quilt(patch(8000, 2340, 8340, 2395), W, H, 21, 30, 8);
	Rgb32 crust = quilt(patch(8100, 2194, 8230, 2212), W, 12, 22, 10, 3);
	std::vector<float> top(static_cast<size_t>(W));
	for (int x = 0; x < W; x++) {
		float xx = float(x);
		float t = clip(xx / float(RUN), 0.0f, 1.0f);
		float ease = t * t * (3.0f - 2.0f * t) * 0.15f + t * 0.85f;
		top[size_t(x)] = ease * float(YEND - YTOP) + std::sin(xx * 0.45f) * 0.8f + std::sin(xx * 0.13f + 1.7f) * 1.2f;
	}
	auto depthAt = [&](int x, int y) { return float(y) - top[size_t(x)]; };
	// the face: shaded darker than the rock behind, darker low down and near the ledge
	// (toward its foot the face lightens back to the ground's own tone)
	Rgb32 img(W, H);
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++) {
			float depth = depthAt(x, y), xx = float(x);
			float toe = clip((float(y) - float(H - 34)) / 30.0f, 0.0f, 1.0f);
			float k = (0.62f - 0.22f * clip(depth / 60.0f, 0.0f, 1.0f) - 0.12f * clip(1.0f - xx / 50.0f, 0.0f, 1.0f)) * (1.0f - toe) + 1.0f * toe;
			for (int c = 0; c < 3; c++)
				img.at(x, y)[c] = body.at(x, y)[c] * k;
		}
	// the walking surface: a band of the ledge's black crust with a few embers
	// near the volcano, turning into the mottled floor rock toward the potion room
	Rgb32 cr(W, H);
	for (int x = 0; x < W; x++)
		for (int k = 0; k < 12; k++) {
			int y = int(top[size_t(x)]) + k;
			if (0 <= y && y < H)
				std::copy(crust.at(x, k), crust.at(x, k) + 3, cr.at(x, y));
		}
	const float mixDiv = float(W * 0.85), embDiv = float(W * 0.6);
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++) {
			float depth = depthAt(x, y), xx = float(x);
			float band = clip(1.0f - depth / 9.0f, 0.0f, 1.0f) * (depth >= 0 ? 1.0f : 0.0f);
			float mix = clip(1.0f - xx / mixDiv, 0.15f, 1.0f);
			for (int c = 0; c < 3; c++) {
				float bd = body.at(x, y)[c];
				float surf = (cr.at(x, y)[c] * 0.5f + bd * 0.5f) * mix * 1.25f + bd * (1.0f - mix) * 1.35f;
				float& v = img.at(x, y)[c];
				v = v * (1.0f - band) + surf * band;
			}
		}
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++) {
			double rnd = r.random();
			float depth = depthAt(x, y);
			float lim = 0.018f * clip(1.0f - float(x) / embDiv, 0.0f, 1.0f);
			if (rnd < double(lim) && depth >= 0 && depth < 4) {
				float* v = img.at(x, y);
				v[0] = 235;
				v[1] = 110;
				v[2] = 35;
			}
		}
	// a lit rim along the top and a thin dark line above it to part it from the wall;
	// under the ledge's lip the face fades into the rock below it (no hard edge);
	// its foot fades into the floor over its last stretch, along an uneven line;
	// and the low end of the slope melts into the floor the same way
	std::vector<uint8_t> a8(static_cast<size_t>(W) * H);
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++) {
			float depth = depthAt(x, y), xx = float(x);
			float rim = 1.0f + 0.9f * std::exp(-sq(depth / 1.4f));
			float line = std::exp(-sq((depth + 1.2f) / 0.8f));
			float* v = img.at(x, y);
			for (int c = 0; c < 3; c++) {
				v[c] *= rim;
				v[c] = v[c] * (1.0f - 0.75f * line);
			}
			float alpha = depth >= 0 ? 1.0f : 0.0f;
			alpha = std::max(alpha, line * 0.9f);
			alpha *= std::sqrt(clip(xx / 6.0f, 0.0f, 1.0f)) * clip((float(W - 1) - xx) / 18.0f, 0.0f, 1.0f);
			alpha *= depth > 10 ? clip(xx / 26.0f, 0.0f, 1.0f) : 1.0f;
			float footline = float(H - 26) + (std::sin(xx * 0.21f) * 3.0f + std::sin(xx * 0.057f + 2.0f) * 5.0f);
			alpha *= clip(1.0f - (float(y) - footline) / 22.0f, 0.0f, 1.0f);
			alpha *= depth > 2 ? clip((float(W - 1) - xx) / 40.0f, 0.0f, 1.0f) : 1.0f;
			a8[size_t(y) * W + x] = u8(clip(alpha, 0.0f, 1.0f) * 255.0f);
		}
	gaussianBlur(a8, W, H, 0.6f);
	Rgba out(W, H);
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++) {
			uint8_t* p = out.at(x, y);
			for (int c = 0; c < 3; c++)
				p[c] = u8(clip(img.at(x, y)[c], 0.0f, 255.0f));
			p[3] = a8[size_t(y) * W + x];
		}
	return out;
}

static Register zvps("zvps", [] {
	return std::vector<Rgba>{makeSeam(false)};
});

static Register zvpc("zvpc", [] {
	return std::vector<Rgba>{makeCarve(makeSeam(true))};
});

static Register zvpr("zvpr", [] {
	return std::vector<Rgba>{makeRamp()};
});

// the volcano's southwest hatch door, mirrored: 5 frames, 0 shut (seen
// edge-on), 4 open (the recipe stacks them in one strip of frames the size
// of the first, each composited onto a clear frame)
static Register zvdr("zvdr", [] {
	std::vector<Rgba> frames = gameSprite("dor2");
	std::vector<Rgba> out;
	if (frames.empty())
		return out;
	int w = frames[0].w, h = frames[0].h;
	for (auto& f : frames) {
		Rgba m(f.w, f.h);
		for (int y = 0; y < m.h; y++)
			for (int x = 0; x < m.w; x++)
				std::copy(f.at(m.w - 1 - x, y), f.at(m.w - 1 - x, y) + 4, m.at(x, y));
		Rgba frame(w, h);
		alphaComposite(frame, m, 0, 0);
		out.push_back(std::move(frame));
	}
	return out;
});

} // namespace

} // namespace MadeArt
