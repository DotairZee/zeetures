// ztlc and ztla: art/treelift_top.py. The treehouse cage lift's new top, its
// pulley moved up onto a cane arm over the shaft (owner, 2026-11-14: the lift
// goes one floor higher, to the upper hut's deck; the pulley hung off the
// flying fox's landing beam over the hut). additions/treelift-top.cos
// places them.
// - ztlc: the background around the old pulley and its hook, taken from just
//   beside it so the wheel is gone from there (drawn just above the
//   background).
// - ztla: an arm of the landing's orange-red cane lashed onto the end of the
//   landing's beam and carried out over the shaft, a cord stay up to the
//   flying fox's stub post, an iron hook, the painted pulley wheel hung on
//   it, and the lift's two cables down to where the painted ones go on.
//
// ztla is drawn with the parts of art/flyingfox.py it uses (Canvas, tube,
// lashing, noise, cnoise), ported here with numpy's float32/float64 steps
// kept as numpy 1.26 does them, numpy's default_rng (SeedSequence and
// PCG64) for the noise, and Pillow's bicubic resize and alpha_composite.

#include "MadeArt.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace MadeArt {

namespace {

// --- numpy's default_rng(seed).uniform(-1, 1, n), as float32 ------------

// numpy.random.SeedSequence(seed).generate_state(4, uint64)
void seedSequence(uint32_t seed, uint64_t out[4]) {
	const uint32_t INIT_A = 0x43b0d7e5, MULT_A = 0x931e8875, INIT_B = 0x8b51f9dd, MULT_B = 0x58f38ded;
	const uint32_t MIX_MULT_L = 0xca01f9dd, MIX_MULT_R = 0x4973f715;
	uint32_t hashConst = INIT_A;
	auto hashmix = [&](uint32_t value) {
		value ^= hashConst;
		hashConst *= MULT_A;
		value *= hashConst;
		value ^= value >> 16;
		return value;
	};
	auto mix = [](uint32_t x, uint32_t y) {
		uint32_t r = MIX_MULT_L * x - MIX_MULT_R * y;
		r ^= r >> 16;
		return r;
	};
	uint32_t pool[4];
	for (int i = 0; i < 4; i++)
		pool[i] = hashmix(i == 0 ? seed : 0);
	for (int src = 0; src < 4; src++)
		for (int dst = 0; dst < 4; dst++)
			if (src != dst)
				pool[dst] = mix(pool[dst], hashmix(pool[src]));
	uint32_t words[8];
	hashConst = INIT_B;
	for (int i = 0; i < 8; i++) {
		uint32_t v = pool[i % 4];
		v ^= hashConst;
		hashConst *= MULT_B;
		v *= hashConst;
		v ^= v >> 16;
		words[i] = v;
	}
	for (int i = 0; i < 4; i++)
		out[i] = uint64_t(words[2 * i]) | (uint64_t(words[2 * i + 1]) << 32);
}

// 128-bit arithmetic without __int128 (MSVC and 32-bit builds lack it)
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

// numpy's PCG64 (XSL-RR), seeded as numpy seeds it
struct Pcg64 {
	U128 state{0, 0}, inc{0, 0};
	explicit Pcg64(uint32_t seed) {
		uint64_t s[4];
		seedSequence(seed, s);
		U128 initstate{s[0], s[1]}, initseq{s[2], s[3]};
		inc = {(initseq.hi << 1) | (initseq.lo >> 63), (initseq.lo << 1) | 1};
		step();
		state = add(state, initstate);
		step();
	}
	void step() {
		const U128 MULT{0x2360ed051fc65da4ULL, 0x4385df649fccf645ULL};
		state = add(mul(state, MULT), inc);
	}
	uint64_t next64() {
		step();
		uint64_t x = state.hi ^ state.lo;
		unsigned rot = unsigned(state.hi >> 58);
		return (x >> rot) | (x << ((64 - rot) & 63));
	}
	double nextDouble() { return double(next64() >> 11) * (1.0 / 9007199254740992.0); }
};

std::vector<float> uniformF32(uint32_t seed, size_t n) {
	Pcg64 rng(seed);
	std::vector<float> v(n);
	for (auto& x : v)
		x = float(-1.0 + 2.0 * rng.nextDouble());
	return v;
}

// --- Pillow's resize(BICUBIC) of an 8-bit ("L") picture ------------------

double bicubic(double x) {
	const double a = -0.5;
	if (x < 0.0)
		x = -x;
	if (x < 1.0)
		return ((a + 2.0) * x - (a + 3.0)) * x * x + 1;
	if (x < 2.0)
		return (((x - 5) * x + 8) * x - 4) * a;
	return 0.0;
}

const int PRECISION_BITS = 32 - 8 - 2;

// precompute_coeffs and normalize_coeffs_8bpc (Resample.c)
void coeffs(int inSize, int outSize, int& ksize, std::vector<int>& bounds, std::vector<int>& kk) {
	double scale = double(inSize) / outSize, filterscale = std::max(scale, 1.0);
	double support = 2.0 * filterscale;
	ksize = int(std::ceil(support)) * 2 + 1;
	bounds.assign(outSize * 2, 0);
	kk.assign(size_t(outSize) * ksize, 0);
	std::vector<double> k(ksize);
	for (int xx = 0; xx < outSize; xx++) {
		double center = (xx + 0.5) * scale, ww = 0.0, ss = 1.0 / filterscale;
		int xmin = int(center - support + 0.5);
		if (xmin < 0)
			xmin = 0;
		int xmax = int(center + support + 0.5);
		if (xmax > inSize)
			xmax = inSize;
		xmax -= xmin;
		for (int x = 0; x < xmax; x++) {
			double w = bicubic((x + xmin - center + 0.5) * ss);
			k[x] = w;
			ww += w;
		}
		for (int x = 0; x < ksize; x++) {
			double v = x < xmax ? (ww != 0.0 ? k[x] / ww : k[x]) : 0.0;
			kk[size_t(xx) * ksize + x] = v < 0 ? int(-0.5 + v * (1 << PRECISION_BITS)) : int(0.5 + v * (1 << PRECISION_BITS));
		}
		bounds[xx * 2] = xmin;
		bounds[xx * 2 + 1] = xmax;
	}
}

uint8_t clip8(int in) {
	in >>= PRECISION_BITS;
	return uint8_t(in < 0 ? 0 : in > 255 ? 255 : in);
}

// horizontal pass first, then vertical, as Pillow does
std::vector<uint8_t> resizeBicubicL(const std::vector<uint8_t>& in, int w, int h, int W, int H) {
	int kh, kv;
	std::vector<int> bh, kkh, bv, kkv;
	coeffs(w, W, kh, bh, kkh);
	coeffs(h, H, kv, bv, kkv);
	std::vector<uint8_t> tmp(size_t(W) * h), out(size_t(W) * H);
	for (int y = 0; y < h; y++)
		for (int xx = 0; xx < W; xx++) {
			int ss = 1 << (PRECISION_BITS - 1);
			const int* k = &kkh[size_t(xx) * kh];
			for (int x = 0; x < bh[xx * 2 + 1]; x++)
				ss += in[size_t(y) * w + x + bh[xx * 2]] * k[x];
			tmp[size_t(y) * W + xx] = clip8(ss);
		}
	for (int yy = 0; yy < H; yy++) {
		const int* k = &kkv[size_t(yy) * kv];
		for (int xx = 0; xx < W; xx++) {
			int ss = 1 << (PRECISION_BITS - 1);
			for (int y = 0; y < bv[yy * 2 + 1]; y++)
				ss += tmp[size_t(y + bv[yy * 2]) * W + xx] * k[y];
			out[size_t(yy) * W + xx] = clip8(ss);
		}
	}
	return out;
}

// --- art/flyingfox.py ----------------------------------------------------

const int SS = 4;

// flyingfox.py's numbers this recipe uses
const double FF_TOP_STOP_X = 3840, FF_TOP_STOP_Y = 840 - 80, FF_BOT_STOP_X = 3395, FF_BOT_STOP_Y = 1285 - 80;
const double FF_SLOPE = (FF_BOT_STOP_Y - FF_TOP_STOP_Y) / (FF_BOT_STOP_X - FF_TOP_STOP_X);
const int FF_BOT_POST_X = 3368;
const int FF_LANDING_X1 = 3512; // the landing's east end

double lineY(double x) {
	return FF_TOP_STOP_Y + (x - FF_TOP_STOP_X) * FF_SLOPE;
}

// LIGHT, normalised in float32 as numpy does
struct Light {
	float x, y, z;
	Light() {
		float a = -0.55f, b = -0.62f, c = 0.56f;
		float n = std::sqrt(a * a + b * b + c * c);
		x = a / n;
		y = b / n;
		z = c / n;
	}
};
const Light LIGHT;

float clampf(float v, float lo, float hi) {
	return std::min(std::max(v, lo), hi);
}
double clampd(double v, double lo, double hi) {
	return std::min(std::max(v, lo), hi);
}

// numpy's float32 remainder (fmod, then the sign of the divisor)
float pymodf(float a, float b) {
	float m = std::fmod(a, b);
	if (m != 0) {
		if ((b < 0) != (m < 0))
			m += b;
	} else
		m = std::copysign(0.0f, b);
	return m;
}

float lit(float nx, float ny, float nz) {
	return clampf(nx * LIGHT.x + ny * LIGHT.y + nz * LIGHT.z, 0, 1);
}

// smooth value noise in -1..1 at the given feature size (pixels), h x w
std::vector<float> noise(int h, int w, uint32_t seed, int scale) {
	int gh = h / scale + 2, gw = w / scale + 2;
	std::vector<float> g = uniformF32(seed, size_t(gh) * gw);
	std::vector<uint8_t> b(g.size());
	for (size_t i = 0; i < g.size(); i++)
		b[i] = uint8_t((g[i] + 1.0f) * 127.5f);
	std::vector<uint8_t> big = resizeBicubicL(b, gw, gh, gw * scale, gh * scale);
	std::vector<float> out(size_t(h) * w);
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++)
			out[size_t(y) * w + x] = float(big[size_t(y) * gw * scale + x]) / 127.5f - 1.0f;
	return out;
}

// smooth value noise in -1..1 at arbitrary coordinates (one unit is one
// feature); float64 as numpy makes it (float32 coordinates less int64 floors)
double cnoise(double u, double v, uint32_t seed) {
	static std::map<uint32_t, std::vector<float>> grids;
	auto it = grids.find(seed);
	if (it == grids.end())
		it = grids.emplace(seed, uniformF32(seed, 64 * 64)).first;
	const std::vector<float>& g = it->second;
	int64_t iu = int64_t(std::floor(u)), iv = int64_t(std::floor(v));
	double fu = u - double(iu), fv = v - double(iv);
	fu = fu * fu * (3 - 2 * fu);
	fv = fv * fv * (3 - 2 * fv);
	auto G = [&](int64_t i, int64_t j) {
		int64_t a = ((j % 64) + 64) % 64, b = ((i % 64) + 64) % 64;
		return double(g[size_t(a) * 64 + b]);
	};
	return G(iu, iv) * (1 - fu) * (1 - fv) + G(iu + 1, iv) * fu * (1 - fv)
		+ G(iu, iv + 1) * (1 - fu) * fv + G(iu + 1, iv + 1) * fu * fv;
}

// an RGBA canvas at SS times the world's resolution, addressed in world
// pixels from an origin
struct Canvas {
	int x0, y0, w, h;
	std::vector<float> rgb, a;
	Canvas(int x0_, int y0_, int w_, int h_)
		: x0(x0_), y0(y0_), w(w_), h(h_), rgb(size_t(w_) * SS * h_ * SS * 3, 0.0f), a(size_t(w_) * SS * h_ * SS, 0.0f) {}

	// the sample points in a box (clipped): i0..i1, j0..j1; false if empty
	bool grid(double bx0, double by0, double bx1, double by1, int& i0, int& j0, int& i1, int& j1) const {
		i0 = std::max(0, int((bx0 - x0) * SS));
		i1 = std::min(w * SS, int(std::ceil((bx1 - x0) * SS)));
		j0 = std::max(0, int((by0 - y0) * SS));
		j1 = std::min(h * SS, int(std::ceil((by1 - y0) * SS)));
		return i1 > i0 && j1 > j0;
	}
	float X(int i) const { return float(x0) + (float(i) + 0.5f) / float(SS); }
	float Y(int j) const { return float(y0) + (float(j) + 0.5f) / float(SS); }

	void put(int i, int j, const double col[3], float cov) {
		cov = clampf(cov, 0, 1);
		size_t p = size_t(j) * w * SS + i;
		for (int c = 0; c < 3; c++) {
			float& v = rgb[p * 3 + c];
			v = float(double(v * (1 - cov)) + col[c] * double(cov));
		}
		a[p] = a[p] + cov * (1 - a[p]);
	}

	Rgba image(uint32_t dappleSeed, double dapple, double mute, double dim) const {
		std::vector<float> n9 = noise(h, w, dappleSeed, 9), n4 = noise(h, w, dappleSeed + 1, 4);
		const int W = w * SS;
		const float fmute = float(mute), fdim = float(dim);
		Rgba out(w, h);
		for (int y = 0; y < h; y++)
			for (int x = 0; x < w; x++) {
				double blobs = double(n9[size_t(y) * w + x] + 0.5f * n4[size_t(y) * w + x]);
				double f = 1 + dapple * clampd(blobs, -1, 1);
				float sum[4] = {0, 0, 0, 0};
				for (int sy = 0; sy < SS; sy++)
					for (int sx = 0; sx < SS; sx++) {
						size_t p = size_t(y * SS + sy) * W + x * SS + sx;
						float c[3];
						for (int k = 0; k < 3; k++)
							c[k] = float(double(rgb[p * 3 + k]) * f);
						float lum = c[0] * 0.3f + c[1] * 0.59f + c[2] * 0.11f;
						for (int k = 0; k < 3; k++) {
							c[k] = (lum + (c[k] - lum) * fmute) * fdim;
							sum[k] += c[k] * a[p];
						}
						sum[3] += a[p];
					}
				float m[4];
				for (int k = 0; k < 4; k++)
					m[k] = sum[k] / float(SS * SS);
				uint8_t* o = out.at(x, y);
				for (int k = 0; k < 3; k++) {
					float v = m[3] > 1e-4f ? m[k] / std::max(m[3], 1e-4f) : 0.0f;
					o[k] = uint8_t(clampf(v, 0, 255));
				}
				o[3] = uint8_t(clampf(m[3] * 255.0f, 0, 255));
			}
		return out;
	}
};

enum Kind { SCAFFOLD,
	GREYCORD,
	BLUECORD };

// a round thing from p to q: 'scaffold' (the orange-red cane, node rings)
// or a cord ('greycord', 'bluecord': thin rope, plain twist)
void tube(Canvas& cv, double ax, double ay, double bx, double by, double width, Kind kind, int seed = 0) {
	double r = width / 2;
	int i0, j0, i1, j1;
	if (!cv.grid(std::min(ax, bx) - r - 1, std::min(ay, by) - r - 1, std::max(ax, bx) + r + 1, std::max(ay, by) + r + 1, i0, j0, i1, j1))
		return;
	double vx = bx - ax, vy = by - ay;
	double ln = std::max(std::hypot(vx, vy), 1e-6);
	double ux = vx / ln, uy = vy / ln;
	double nx = -uy, ny = ux; // across
	const float fax = float(ax), fay = float(ay), fbx = float(bx), fby = float(by), fr = float(r);
	const float fux = float(ux), fuy = float(uy), fnx = float(nx), fny = float(ny);
	const float sLo = float(-0.2), sHi = float(ln + 0.2), rr = float(r * r);
	for (int j = j0; j < j1; j++)
		for (int i = i0; i < i1; i++) {
			float X = cv.X(i), Y = cv.Y(j);
			float s = (X - fax) * fux + (Y - fay) * fuy;         // along, px
			float t = ((X - fax) * fnx + (Y - fay) * fny) / fr; // -1..1 across
			bool inside = s >= sLo && s <= sHi && std::fabs(t) <= 1;
			// round ends
			{
				float dx = X - fax, dy = Y - fay;
				inside = inside || dx * dx + dy * dy <= rr;
				dx = X - fbx;
				dy = Y - fby;
				inside = inside || dx * dx + dy * dy <= rr;
			}
			float cov = (inside ? 1.0f : 0.0f) * clampf((1 - std::fabs(t)) * fr * 1.6f + 0.4f, 0, 1);
			float tz = std::sqrt(clampf(1 - t * t, 0, 1));
			float shade = 0.32f + 0.9f * lit(t * fnx, t * fny, tz);
			double col[3];
			if (kind == SCAFFOLD) {
				// the bamboo scaffold by the desert lift: a saturated orange-red
				// cane, dark red-brown at the edges, a warm highlight, node rings
				const float o_dark[3] = {70, 18, 0}, o_mid[3] = {214, 96, 6}, o_lit[3] = {255, 178, 84};
				float sh = clampf(shade, 0, 1.2f);
				float q = (t + 0.45f) / 0.18f;
				float hl = std::exp(-(q * q)) * 0.7f;
				bool ring = std::fabs(pymodf(s + 9.0f + float(seed * 7), 30.0f) - 15.0f) > 14.1f;
				double grain = cnoise(s / 6.0f, (t + 1) * 4.0f, 210 + seed);
				for (int c = 0; c < 3; c++) {
					float base = o_dark[c] + (o_mid[c] - o_dark[c]) * clampf(sh, 0, 1);
					base = base + (o_lit[c] - base) * hl;
					if (ring)
						base = base * 0.55f;
					col[c] = double(base) * (1 + 0.1 * grain);
				}
			} else {
				const float period = 2.4f;
				float phase = pymodf((s + t * fr * 1.1f) / period, 1.0f);
				float strand = std::fabs(phase - 0.5f) * 2; // 1 at the grooves
				float crown = clampf(1 - strand / 0.45f, 0, 1);
				crown = crown * crown; // the middle of each strand
				double fibre = cnoise((s - t * fr * 1.1f) / 1.1f, phase * 6.0f, 120 + seed);
				double hue = cnoise(s / 30.0f, 0.5, 130 + seed);
				float sh32 = shade * (1 - 0.72f * std::pow(strand, 2.2f)) * (1 + 0.32f * crown * clampf(shade, 0, 1));
				double sh = double(sh32) * (1 + 0.16 * fibre);
				sh = std::pow(clampd(sh, 0, 1.4), 1.15);
				const float greycord[3] = {128, 136, 148}, bluecord[3] = {142, 156, 196}, tint[3] = {-10, -6, 4};
				const float* rope = kind == GREYCORD ? greycord : bluecord;
				for (int c = 0; c < 3; c++) {
					double base = double(rope[c]) * (1 + 0.1 * hue) + double(tint[c]) * clampd(-hue, 0, 1);
					col[c] = base * sh;
				}
			}
			cv.put(i, j, col, cov);
		}
}

// cord wound round a joint: a short tube of turns across a pole
void lashing(Canvas& cv, double x, double y, double w, double h, double angle, int seed, Kind kind) {
	int turns = std::max(3, int(h / 1.6));
	double ca = std::cos(angle), sa = std::sin(angle);
	for (int k = 0; k < turns; k++) {
		double off = (k - (turns - 1) / 2.0) * 1.6;
		double cx = x - sa * off, cy = y + ca * off;
		tube(cv, cx - ca * w / 2, cy - sa * w / 2, cx + ca * w / 2, cy + sa * w / 2, 1.7, kind, seed + k);
	}
}

// turns of blue-grey cord round a pole where another meets it
void wrapLashing(Canvas& cv, double x, double y, double w, double angle, int seed, int turns = 4) {
	lashing(cv, x, y, w + 1.5, turns * 1.8, angle, seed, BLUECORD);
}

// Pillow's alpha_composite of src over dst, src's top left at (dx, dy)
void alphaComposite(Rgba& dst, const Rgba& src, int dx, int dy) {
	const uint32_t P = 7;
	auto shiftDiv255 = [](uint32_t a) { return ((a >> 8) + a) >> 8; };
	for (int y = 0; y < src.h; y++)
		for (int x = 0; x < src.w; x++) {
			int X = x + dx, Y = y + dy;
			if (X < 0 || Y < 0 || X >= dst.w || Y >= dst.h)
				continue;
			const uint8_t* s = src.at(x, y);
			uint8_t* d = dst.at(X, Y);
			if (s[3] == 0)
				continue;
			uint32_t blend = uint32_t(d[3]) * (255 - s[3]);
			uint32_t outa255 = uint32_t(s[3]) * 255 + blend;
			uint32_t coef1 = uint32_t(s[3]) * 255 * 255 * (1u << P) / outa255;
			uint32_t coef2 = 255 * (1u << P) - coef1;
			for (int c = 0; c < 3; c++)
				d[c] = uint8_t(shiftDiv255(s[c] * coef1 + d[c] * coef2 + (0x80u << P)) >> P);
			d[3] = uint8_t(shiftDiv255(outa255 + 0x80));
		}
}

// --- art/treelift_top.py -------------------------------------------------

// the painted pulley: wheel centre, radius; its hook from the deck's underside
const double WHEEL_CX = 3576.0, WHEEL_CY = 1595.0, WHEEL_R = 15.0;
const double ROPE_X[2] = {3564.0, 3588.0}; // the two painted ropes below the wheel
const double ROPE_JOIN = 1612.0;           // where the painted ropes go on
const int CABLE_SRC0 = 1618, CABLE_SRC1 = 1738; // a clean stretch of the painted cables, repeated above
const double CABLE_LIFT[2] = {1.15, 1.0};  // west, east: brightness of the extension
const int DECK_ROW0 = 1540, DECK_ROW1 = 1567; // the upper deck's planks and logs: the cables pass behind them
// the patch over the old wheel and hook
const int CX0 = 3554, CY0 = 1562, CX1 = 3598, CY1 = 1612;
const int PATCH_DX = -46; // taken from this far beside it

// the new top
const int X0 = 3440, Y0 = 1120, X1 = 3620, Y1 = 1616;
const double ARM_X0 = 3456.0, ARM_X1 = 3606.0, ARM_Y = 1294.0; // from, to (x), at the landing beam's height
const double NEW_CX = 3576.0, NEW_CY = 1326.0;                  // the wheel's new centre

// the painted wheel, cut out (bronze rim, spokes, hub) from the background;
// its top left in the world
Rgba wheelImage(int& wx0, int& wy0) {
	int x0 = int(WHEEL_CX - WHEEL_R - 2), y0 = int(WHEEL_CY - WHEEL_R - 2);
	int x1 = int(WHEEL_CX + WHEEL_R + 3), y1 = int(WHEEL_CY + WHEEL_R + 3);
	wx0 = x0;
	wy0 = y0;
	Rgba bg = backgroundRegion(x0, y0, x1, y1);
	if (!bg.w)
		return bg;
	const float cx = float(WHEEL_CX), cy = float(WHEEL_CY);
	const float in = float(WHEEL_R - 0.5), out = float(WHEEL_R + 0.8);
	for (int y = 0; y < bg.h; y++)
		for (int x = 0; x < bg.w; x++) {
			uint8_t* p = bg.at(x, y);
			float d = std::hypot(float(x) + float(x0) + 0.5f - cx, float(y) + float(y0) + 0.5f - cy);
			float r = p[0], g = p[1], b = p[2];
			bool bronze = r > g * 1.02f && r - b > 18;
			double a = d <= in && (bronze || d < 4.5f) ? 255.0 : 0.0;
			if (d > in && d <= out && bronze)
				a = 160.0;
			p[3] = uint8_t(a);
		}
	return bg;
}

static Register ztlc("ztlc", [] {
	Rgba im = backgroundRegion(CX0 + PATCH_DX, CY0, CX1 + PATCH_DX, CY1);
	if (!im.w)
		return std::vector<Rgba>();
	// soft left and right edges, so the patch blends into what is beside it
	for (int x = 0; x < im.w; x++) {
		float edge = clampf(std::min(float(x), float(im.w - 1) - float(x)) / 5.0f, 0, 1);
		uint8_t a = uint8_t(edge * 255);
		for (int y = 0; y < im.h; y++)
			im.at(x, y)[3] = a;
	}
	return std::vector<Rgba>{im};
});

static Register ztla("ztla", [] {
	if (!backgroundRegion(X0, Y0, X0 + 1, Y0 + 1).w)
		return std::vector<Rgba>();
	Canvas cv(X0, Y0, X1 - X0, Y1 - Y0);
	const double ax0 = ARM_X0, ax1 = ARM_X1, ay = ARM_Y;
	// the stay: cord from the arm's tip up to the flying fox's stub post
	double postTop = lineY(FF_BOT_POST_X + 2) - 6;
	tube(cv, ax1 - 4, ay - 2, FF_BOT_POST_X + 3, postTop + 8, 2.0, BLUECORD, 61);
	// the arm: the landing's cane, lashed on where it overlaps the beam's end
	tube(cv, ax0, ay + 0.5, ax1, ay - 1.5, 5.4, SCAFFOLD, 62);
	wrapLashing(cv, ax0 + 10, ay + 0.2, 7.0, 1.5708, 63, 4);
	wrapLashing(cv, FF_LANDING_X1 - 2, ay - 0.3, 7.0, 1.5708, 64, 4);
	wrapLashing(cv, ax1 - 5, ay - 1.4, 6.5, 1.5708, 65, 3);
	// the hook: an iron strap from a lashing round the arm to the wheel's hub
	const double hx = NEW_CX, hy = NEW_CY;
	tube(cv, hx, ay + 1, hx, hy, 2.6, GREYCORD, 66);
	wrapLashing(cv, hx, ay - 1.0, 6.5, 1.5708, 67, 3);
	Rgba img = cv.image(73, 0.12, 0.9, 0.97);
	// the lift's two cables, from the wheel down to where the painted ones go
	// on: a clean cable from the painted one's cross-section (owner,
	// 2026-11-14): each column across it gets the painted cable's usual colour
	// there (the median of its gold pixels) and a solidity from how often that
	// column is gold; a faint shading along it keeps it from looking flat
	for (int which = 0; which < 2; which++) {
		int x0 = int(ROPE_X[which]) - 4;
		Rgba strip = backgroundRegion(x0, CABLE_SRC0, x0 + 9, CABLE_SRC1);
		const int n = strip.h;
		float prof[9][3] = {};
		float solid[9];
		for (int c = 0; c < 9; c++) {
			std::vector<float> on[3];
			for (int y = 0; y < n; y++) {
				const uint8_t* p = strip.at(c, y);
				float r = p[0], g = p[1], b = p[2];
				float gold = clampf((r - b - 45) / 30.0f, 0, 1) * (r > g * 0.95f ? 1.0f : 0.0f);
				if (gold > 0.5f)
					for (int k = 0; k < 3; k++)
						on[k].push_back(p[k]);
			}
			double frac = double(on[0].size()) / n;
			for (int k = 0; k < 3 && !on[k].empty(); k++) {
				std::vector<float>& v = on[k];
				std::sort(v.begin(), v.end());
				size_t m = v.size();
				prof[c][k] = m % 2 ? v[m / 2] : (v[m / 2 - 1] + v[m / 2]) / 2.0f;
			}
			solid[c] = float(clampd((frac - 0.25) / 0.45, 0, 1));
		}
		// (the west cable a little lighter: owner, 2026-11-14)
		const float lift = float(CABLE_LIFT[which]);
		for (int c = 0; c < 9; c++)
			for (int k = 0; k < 3; k++)
				prof[c][k] = clampf(prof[c][k] * lift, 0, 255);
		int yTop = int(hy + 9), yBot = int(ROPE_JOIN + 4);
		int L = yBot - yTop;
		std::vector<float> cable(size_t(L) * 9 * 4, 0.0f);
		for (int k = 0; k < L; k++) {
			int y = yTop + k;
			float along = float(1.0 + 0.04 * std::sin(k / 7.3) + 0.03 * std::sin(k / 2.9 + 1.0));
			for (int c = 0; c < 9; c++) {
				float* q = &cable[(size_t(k) * 9 + c) * 4];
				for (int ch = 0; ch < 3; ch++)
					q[ch] = clampf(prof[c][ch] * along, 0, 255);
				float a = solid[c] * 255.0f;
				if (DECK_ROW0 <= y && y <= DECK_ROW1)
					a = a * 0.0f; // behind the upper deck's planks and logs
				q[3] = a;
			}
		}
		for (int k = 0; k < 4; k++) // ease into the painted cable below
			for (int c = 0; c < 9; c++)
				cable[(size_t(L - 1 - k) * 9 + c) * 4 + 3] *= float((k + 1) / 5.0);
		Rgba cim(9, L);
		for (size_t i = 0; i < cable.size(); i++)
			cim.px[i] = uint8_t(cable[i]);
		alphaComposite(img, cim, x0 - X0, yTop - Y0);
	}
	int wx0, wy0;
	Rgba wheel = wheelImage(wx0, wy0);
	double dx = NEW_CX - WHEEL_CX, dy = NEW_CY - WHEEL_CY;
	alphaComposite(img, wheel, int(std::round(wx0 + dx - X0)), int(std::round(wy0 + dy - Y0)));
	return std::vector<Rgba>{img};
});

} // namespace

} // namespace MadeArt
