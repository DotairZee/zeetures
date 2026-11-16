// zsbd: art/stream_bed.py. The stream bed's earth, drawn in front of
// creatures (owner, 2026-11-14: creatures carried along the stream under the
// waterwheel poked out below the ground). From the cave lift's dock to past
// the waterwheel (x X0-X1) the world's picture ends a little below the
// stream's bed, and below it the view is black. This takes the player's own
// earth from just under the stream's water down to the picture's foot, and
// carries it on further down (ground drawn from bands of that earth's own
// bottom rows, darker with depth). Only earth is taken: the water, the dock,
// the wheel and anything above the bed stay as they are.
// tools/additions/water-pocket.cos places it at world X0, Y0.
//
// The recipe works in numpy float32 (float64 where numpy promotes to it) and
// draws its band shifts from numpy's default_rng(7); both are matched here
// (numpy's SeedSequence, PCG64 and Generator.integers) so the picture is the
// same to the bit.

#include "MadeArt.h"

#include <algorithm>

namespace MadeArt {

namespace {

// ---- numpy's default_rng(seed).integers(0, n), as numpy 1.26 makes them ----

struct U128 {
	uint64_t hi, lo;
};

U128 mul64(uint64_t a, uint64_t b) {
	uint64_t a0 = a & 0xffffffffu, a1 = a >> 32, b0 = b & 0xffffffffu, b1 = b >> 32;
	uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
	uint64_t mid = (p00 >> 32) + (p01 & 0xffffffffu) + (p10 & 0xffffffffu);
	return {p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32), (mid << 32) | (p00 & 0xffffffffu)};
}

U128 mul(U128 a, U128 b) {
	U128 r = mul64(a.lo, b.lo);
	r.hi += a.hi * b.lo + a.lo * b.hi;
	return r;
}

U128 add(U128 a, U128 b) {
	uint64_t lo = a.lo + b.lo;
	return {a.hi + b.hi + (lo < a.lo), lo};
}

class NumpyRng {
  public:
	explicit NumpyRng(uint32_t seed) {
		// SeedSequence(seed): entropy [seed] hashed into a pool of 4 words
		const uint32_t INIT_A = 0x43b0d7e5, MULT_A = 0x931e8875, INIT_B = 0x8b51f9dd, MULT_B = 0x58f38ded;
		uint32_t hashConst = INIT_A;
		auto hashmix = [&](uint32_t value) {
			value ^= hashConst;
			hashConst *= MULT_A;
			value *= hashConst;
			value ^= value >> 16;
			return value;
		};
		auto mix = [](uint32_t x, uint32_t y) {
			uint32_t result = 0xca01f9dd * x - 0x4973f715 * y;
			result ^= result >> 16;
			return result;
		};
		uint32_t pool[4];
		for (int i = 0; i < 4; i++)
			pool[i] = hashmix(i == 0 ? seed : 0);
		for (int src = 0; src < 4; src++)
			for (int dst = 0; dst < 4; dst++)
				if (src != dst)
					pool[dst] = mix(pool[dst], hashmix(pool[src]));
		// generate_state(4, uint64): 8 words, little-endian pairs
		uint32_t hc = INIT_B;
		uint64_t words[4];
		for (int i = 0; i < 8; i++) {
			uint32_t v = pool[i % 4];
			v ^= hc;
			hc *= MULT_B;
			v *= hc;
			v ^= v >> 16;
			if (i % 2 == 0)
				words[i / 2] = v;
			else
				words[i / 2] |= uint64_t(v) << 32;
		}
		// PCG64: pcg_setseq_128_srandom_r(state words 0-1, increment words 2-3)
		U128 initstate{words[0], words[1]}, initseq{words[2], words[3]};
		state = {0, 0};
		inc = {(initseq.hi << 1) | (initseq.lo >> 63), (initseq.lo << 1) | 1};
		step();
		state = add(state, initstate);
		step();
	}

	// Generator.integers(0, n): Lemire's bounded draw on 32-bit outputs
	int64_t integers(uint32_t n) {
		const uint32_t rng = n - 1, rngExcl = n;
		uint64_t m = uint64_t(next32()) * rngExcl;
		uint32_t leftover = uint32_t(m);
		if (leftover < rngExcl) {
			const uint32_t threshold = (UINT32_MAX - rng) % rngExcl;
			while (leftover < threshold) {
				m = uint64_t(next32()) * rngExcl;
				leftover = uint32_t(m);
			}
		}
		return int64_t(m >> 32);
	}

  private:
	U128 state{0, 0}, inc{0, 0};
	bool hasUint32 = false;
	uint32_t uinteger = 0;

	void step() {
		const U128 MULT{2549297995355413924ULL, 4865540595714422341ULL};
		state = add(mul(state, MULT), inc);
	}
	uint64_t next64() {
		step();
		uint64_t x = state.hi ^ state.lo;
		unsigned rot = unsigned(state.hi >> 58);
		return (x >> rot) | (x << ((64 - rot) & 63));
	}
	uint32_t next32() {
		if (hasUint32) {
			hasUint32 = false;
			return uinteger;
		}
		uint64_t next = next64();
		hasUint32 = true;
		uinteger = uint32_t(next >> 32);
		return uint32_t(next);
	}
};

const int X0 = 2200, Y0 = 2340, X1 = 3080; // from the dock to past the wheel; top of the search
const int FOOT = 2400; // the world's picture ends here
const int EXTRA = 70; // earth carried on below it

} // namespace

static Register zsbd("zsbd", [] {
	Rgba bgIm = backgroundRegion(X0, Y0, X1, FOOT);
	if (!bgIm.w)
		return std::vector<Rgba>();
	const int w = bgIm.w, h = bgIm.h;
	auto bg = [&](int x, int y, int c) { return float(bgIm.at(x, y)[c]); };

	std::vector<int> tops(w, h);
	for (int x = 0; x < w; x++) {
		int last = -1;
		for (int y = 0; y < h; y++) {
			float r = bg(x, y, 0), g = bg(x, y, 1), b = bg(x, y, 2);
			bool water = g >= r && b >= r * 0.8f && r + g + b > 30 && b < 140;
			if (water)
				last = y;
		}
		// the earth starts under the lowest water in this column (or, with no
		// water here, only in the bottom few rows: the pit behind the wheel)
		tops[x] = last >= 0 ? last + 2 : h - 6;
	}

	// below the picture's foot: the real earth's bottom rows again, in bands
	// each shifted sideways (and every other one upside down, so they meet
	// without a seam), darker with depth
	const int BAND = 12, GH = EXTRA + 8;
	NumpyRng rng(7);
	std::vector<float> ground32(size_t(GH) * w * 3, 0.f);
	for (int k = 0; k * BAND < GH; k++) {
		int shift = int(rng.integers(uint32_t(w)));
		int rows = std::min(BAND, GH - k * BAND);
		for (int j = 0; j < rows; j++) {
			// the strip is the picture's last BAND rows: earth all along
			int sy = h - BAND + (k % 2 == 0 ? BAND - 1 - j : j);
			for (int x = 0; x < w; x++) {
				int sx = ((x - shift) % w + w) % w; // np.roll
				for (int c = 0; c < 3; c++)
					ground32[(size_t(k * BAND + j) * w + x) * 3 + c] = bg(sx, sy, c);
			}
		}
	}
	// fade = 1 - 0.45 * linspace(0, 1, GH), in float64, as numpy promotes it
	std::vector<double> ground(ground32.size());
	for (int i = 0; i < GH; i++) {
		double lin = i == GH - 1 ? 1.0 : i * (1.0 / (GH - 1));
		double fade = 1.0 - 0.45 * lin;
		for (size_t j = 0; j < size_t(w) * 3; j++)
			ground[size_t(i) * w * 3 + j] = double(ground32[size_t(i) * w * 3 + j]) * fade;
	}
	auto gr = [&](int row, int x, int c) { return ground[(size_t(row) * w + x) * 3 + c]; };

	const int H = h + EXTRA;
	std::vector<float> out(size_t(H) * w * 4, 0.f);
	auto o = [&](int x, int y) { return &out[(size_t(y) * w + x) * 4]; };
	for (int x = 0; x < w; x++) {
		int top = tops[x];
		if (top < h)
			for (int y = top; y < h; y++) {
				for (int c = 0; c < 3; c++)
					o(x, y)[c] = bg(x, y, c);
				o(x, y)[3] = 255;
			}
		for (int y = h; y < H; y++) {
			for (int c = 0; c < 3; c++)
				o(x, y)[c] = float(gr(8 + y - h, x, c));
			o(x, y)[3] = 255;
		}
	}
	// blend the picture's last 8 rows of earth into the drawn ground
	// (float32 times a scalar stays float32; plus the float64 ground)
	for (int k = 0; k < 8; k++) {
		double wgt = (k + 1) / 9.0;
		float keepW = float(1 - wgt);
		int row = h - 8 + k;
		for (int x = 0; x < w; x++) {
			float* p = o(x, row);
			if (p[3] > 0)
				for (int c = 0; c < 3; c++)
					p[c] = float(double(p[c] * keepW) + gr(k, x, c) * wgt);
		}
	}
	// a soft top edge (one row), so the ground's line does not look cut
	for (int x = 0; x < w; x++) {
		float above = 0;
		for (int y = 0; y < H; y++) {
			float a = o(x, y)[3];
			if (a > 0 && above == 0)
				o(x, y)[3] = 140;
			above = a;
		}
	}

	Rgba im(w, H);
	for (size_t i = 0; i < out.size(); i++)
		im.px[i] = uint8_t(std::clamp(out[i], 0.f, 255.f));
	return std::vector<Rgba>{im};
});

} // namespace MadeArt
