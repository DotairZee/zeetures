// The walkable glass tube's made art, part: the scene and the old tube painted out (art/tube_walk.py: put_scenery, old_tube, new_tube, paint_out; art/tube_walk_lib.py: quilt_fill, tone_correct, match_texture, restore_branches).
//
// Each function follows its Python line by line; the comments say where
// numpy's types or summation order decide the bits (see npcore.h). Proved
// by art/tube_walk_check_paint.py (OPENC2E_TW_CHECK_PAINT=DIR).

#include "tube_walk_paint.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <stdexcept>

// numpy, Pillow and CPython were built without fused multiply-add
// contraction (generic x86-64); so is this, whatever the flags
#if defined(__clang__)
#pragma clang fp contract(off)
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

namespace MadeArt {
namespace tubewalk {

namespace {

// numpy's a[start:stop] on an axis of n: negative indices count from the
// end, then both are clamped to [0, n] (an empty slice if stop < start)
void pyslice(long& s, long& e, long n) {
	if (s < 0)
		s += n;
	if (e < 0)
		e += n;
	s = std::clamp(s, 0L, n);
	e = std::clamp(e, 0L, n);
	if (e < s)
		e = s;
}

// a[y0:y1, x0:x1] (all channels), copied
template <typename T>
np::Img<T> crop(const np::Img<T>& a, long y0, long y1, long x0, long x1) {
	pyslice(y0, y1, a.h);
	pyslice(x0, x1, a.w);
	np::Img<T> o(int(y1 - y0), int(x1 - x0), a.c);
	for (long y = y0; y < y1; y++)
		std::copy(a.px(int(y), int(x0)), a.px(int(y), int(x0)) + (x1 - x0) * a.c, o.px(int(y - y0), 0));
	return o;
}

// Python's int(round(v)): round half to even
long pyround(double v) {
	return long(std::nearbyint(v));
}

// Python's a % b for b > 0
long pymod(long a, long b) {
	long r = a % b;
	return r < 0 ? r + b : r;
}

void need(bool ok, const char* what) {
	if (!ok)
		throw std::runtime_error(std::string("tube_walk_paint: ") + what);
}

// lib.blur(a, s): np.asarray(Image.fromarray(a.clip(0, 255).astype(np.uint8)).filter(GaussianBlur(s))).astype(np.float32)
ImgF blurF(const ImgF& a, double s) {
	return np::toF32(np::gaussianBlur(np::clipToU8(a), s));
}

// ex(-0.5 * (np.arange(-r, r + 1) / d) ** 2); k /= k.sum()
std::vector<double> gaussKernel(int r, double d) {
	std::vector<double> k;
	for (int i = -r; i <= r; i++) {
		double t = double(i) / d;
		k.push_back(np::ex(-0.5 * (t * t)));
	}
	double s = np::sum(k.data(), k.size());
	for (double& v : k)
		v = v / s;
	return k;
}

// each column's first and last row set in band (r.min(), r.max()), or -1
void bandSpan(const Mask& band, int x, int& t, int& b) {
	t = b = -1;
	for (int y = 0; y < band.h; y++)
		if (band(y, x)) {
			if (t < 0)
				t = y;
			b = y;
		}
}

} // namespace

// ---- tube_walk.py's constants -------------------------------------------------

std::vector<Scenery> sceneryFrom(int from, int to) {
	long s = from, e = to < 0 ? long(SCENERY.size()) : long(to);
	pyslice(s, e, long(SCENERY.size()));
	return std::vector<Scenery>(SCENERY.begin() + s, SCENERY.begin() + e);
}

double tube_length() {
	return np::hypot(double(END_X - X0), start_top + slope * (END_X - X0) - start_top);
}

namespace {
// the owner's circles, on the game's picture with top left 4780,1380
Poly circle(std::initializer_list<std::pair<int, int>> p) {
	Poly o;
	for (auto [x, y] : p)
		o.push_back({x + 4780, y + 1380});
	return o;
}
} // namespace

const std::vector<Poly> BRANCHES = {
	circle({{190, 70}, {215, 5}, {232, 10}, {226, 90}, {252, 180}, {225, 188}, {205, 140}}),
	circle({{308, 150}, {345, 122}, {378, 128}, {362, 188}, {318, 188}}),
	circle({{388, 70}, {405, 8}, {422, 14}, {452, 100}, {457, 182}, {442, 222}, {408, 218}, {414, 150}}),
};

// ---- the scene ------------------------------------------------------------------

GameInputs inputsFrom(const Rgba& bgRegion, const Rgba& bgtRegion, const std::vector<Rgba>& swmp) {
	GameInputs in;
	// np.asarray(region(...)).astype(np.float32)[..., :3]
	in.bg = np::toF32(np::fromRgba(bgRegion, 3));
	in.bgt = np::toF32(np::fromRgba(bgtRegion, 3));
	for (const Rgba& f : swmp)
		in.swmp.push_back(np::toF32(np::fromRgba(f, 4)));
	if (!swmp.empty()) {
		const Rgba& f = swmp[0];
		in.ta = Mask(f.h, f.w, 1, 0);
		for (int y = 0; y < f.h; y++)
			for (int x = 0; x < f.w; x++)
				in.ta(y, x) = f.at(x, y)[3] > 0;
	}
	return in;
}

bool readInputs(GameInputs& in) {
	Rgba bg = backgroundRegion(OX, OY, OX + W, OY + H);
	Rgba bgt = backgroundRegion(OX, OY - 300, OX + W, OY + H);
	std::vector<Rgba> swmp = gameSprite("swmp");
	if (bg.px.empty() || bgt.px.empty() || swmp.size() < 4)
		return false;
	in = inputsFrom(bg, bgt, swmp);
	return true;
}

void put_scenery(ImgF& out, const std::vector<ImgF>& swmp, const std::vector<Scenery>& frames, const Mask* cut) {
	for (const Scenery& s : frames) {
		need(s.frame < int(swmp.size()), "put_scenery: no such frame");
		const ImgF& f = swmp[size_t(s.frame)];
		const int h = f.h, w = f.w, oy = s.y - OY, ox = s.x - OX;
		need(oy >= 0 && ox >= 0 && oy + h <= out.h && ox + w <= out.w, "put_scenery: frame outside the scene");
		if (cut)
			need(oy + h <= cut->h && ox + w <= cut->w, "put_scenery: frame outside cut");
		for (int y = 0; y < h; y++)
			for (int x = 0; x < w; x++) {
				// a = f[..., 3:] / 255 (float32); a * ~cut (bool as 0/1)
				float a = f(y, x, 3) / 255.0f;
				if (cut)
					a = a * ((*cut)(oy + y, ox + x) ? 0.0f : 1.0f);
				float* r = out.px(oy + y, ox + x);
				for (int k = 0; k < 3; k++)
					r[k] = r[k] * (1.0f - a) + f(y, x, k) * a;
			}
	}
}

Scene makeScene(const GameInputs& in) {
	Scene sc;
	sc.tubecomp = in.bg;
	put_scenery(sc.tubecomp, in.swmp, sceneryFrom(0, 1));
	sc.scene = sc.tubecomp;
	put_scenery(sc.scene, in.swmp, sceneryFrom(1));
	return sc;
}

void old_tube(const ImgF& tubecomp, const Mask& ta, Mask& mask, ImgF& strip) {
	const int tw = ta.w;
	mask = Mask(H, W, 1, 0);
	for (int x = X0; x <= X1; x++) {
		int c = x - TX;
		if (!(0 <= c && c < tw))
			continue;
		int rmin = -1, rmax = -1;
		for (int y = 0; y < ta.h; y++)
			if (ta(y, c)) {
				if (rmin < 0)
					rmin = y;
				rmax = y;
			}
		if (rmin < 0)
			continue;
		int t = TY + rmin;
		int b = std::min(TY + rmax, int(top_of(x)) + 146);
		long y0 = std::max(0, t - 3 - OY), y1 = b + 3 - OY;
		pyslice(y0, y1, H);
		for (long y = y0; y < y1; y++)
			mask(int(y), x - OX) = 1;
	}
	strip = ImgF(SH, X1 - X0 + 1, 4, 0.0f);
	for (int x = X0; x <= X1; x++) {
		long t0 = pyround(top_of(x)) - 12;
		long y0 = t0 - OY, y1 = t0 - OY + SH;
		pyslice(y0, y1, H);
		need(y1 - y0 == SH, "old_tube: strip rows outside the scene");
		for (int r = 0; r < SH; r++) {
			float* s = strip.px(r, x - X0);
			const float* p = tubecomp.px(int(y0) + r, x - OX);
			s[0] = p[0];
			s[1] = p[1];
			s[2] = p[2];
			s[3] = mask(int(y0) + r, x - OX) ? 255.0f : 0.0f;
		}
	}
}

void new_tube(const ImgF& strip, ImgF& MOD, ImgF& pc) {
	MOD = crop(strip, 0, strip.h, 5630 - X0, 5930 - X0);
	// lo = MOD[80:, :, :3] (a view: lo[warm] = ... writes MOD)
	const int R0 = 80, lh = MOD.h - R0, lw = MOD.w;
	Mask warm(lh, lw, 1, 0);
	std::vector<float> rows; // lo[~warm & (alpha > 0)], (n, 3)
	for (int y = 0; y < lh; y++)
		for (int x = 0; x < lw; x++) {
			const float* p = MOD.px(R0 + y, x);
			bool op = p[3] > 0;
			warm(y, x) = (p[0] - p[2] > 18) && op;
			if (!warm(y, x) && op)
				rows.insert(rows.end(), p, p + 3);
		}
	need(!rows.empty(), "new_tube: no glass in the module");
	// ref = (n, 3).mean(axis=0), float32; ref.mean() a float32 scalar
	std::vector<float> ref = np::meanAxis0(rows.data(), rows.size() / 3, 3);
	const float refMean = np::meanFlat(ref.data(), 3);
	// lum = lo.mean(axis=2, keepdims=True), from lo before it changes
	ImgF lo = crop(MOD, R0, MOD.h, 0, lw);
	ImgF lo3 = np::channels(lo, 0, 3);
	ImgF lum = np::meanLast(lo3);
	for (int y = 0; y < lh; y++)
		for (int x = 0; x < lw; x++)
			if (warm(y, x))
				for (int k = 0; k < 3; k++)
					MOD(R0 + y, x, k) = lum(y, x) * ref[size_t(k)] / refMean;
	// the strip's first 20 columns, then the module, to int(length)
	const double length = tube_length();
	int n = 20, total = 20;
	while (n < length) {
		n += MOD.w;
		total += MOD.w;
	}
	const int pw = std::min(total, int(length));
	pc = ImgF(strip.h, pw, 4, 0.0f);
	for (int y = 0; y < strip.h; y++)
		for (int c = 0; c < pw; c++) {
			const float* s = c < 20 ? strip.px(y, c) : MOD.px(y, (c - 20) % MOD.w);
			std::copy(s, s + 4, pc.px(y, c));
		}
}

ImgF paint_out(const ImgF& scene, const ImgF& bg, const ImgF& bgt, const Mask& mask, const ImgF& strip, PaintSteps* steps) {
	std::vector<double>* en = steps ? &steps->energies : nullptr;
	std::vector<int64_t>* cn = steps ? &steps->energyCounts : nullptr;
	ImgF base = quilt_fill(scene, OX, OY, top_of, X0 - 4, X1 + 6, bgt, OY - 300, 3, en, cn);
	if (steps)
		steps->quilt = base;
	Mask band = mask;
	for (int x = X0 - 4; x < X1 + 6; x++) {
		long t = pyround(top_of(x));
		long y0 = t - 16 - OY, y1 = t + 152 - OY;
		pyslice(y0, y1, H);
		for (long y = y0; y < y1; y++)
			band(int(y), x - OX) = 1;
	}
	if (steps)
		steps->band = band;
	base = tone_correct(base, bg, band);
	if (steps)
		steps->toned = base;
	base = match_texture(base, bg, band);
	if (steps)
		steps->textured = base;
	ImgF mod = crop(strip, 0, strip.h, 5630 - X0, 5930 - X0);
	return restore_branches(base, scene, mod, top_of, OX, OY, BRANCHES);
}

// ---- tube_walk_lib.py -------------------------------------------------------------

ImgF quilt_fill(const ImgF& img, int ox, int oy, const TopOf& top_of, int xs0, int xs1, const ImgF& bgt, int oyt, uint64_t seed,
	std::vector<double>* energies, std::vector<int64_t>* counts) {
	np::Generator rng(seed);
	const int A = 300, BAND = 168, CTX = 8;
	const int SX0 = xs0 - 150, SX1 = xs1;
	const int Wd = SX1 - SX0;
	const int rows = A + BAND + 40;
	// S: each column's rows from A above the old top (t - 16), straightened
	ImgF S(rows, Wd, 3, 0.0f);
	for (int i = 0; i < Wd; i++) {
		const int x = SX0 + i;
		long t = pyround(top_of(x)) - 16;
		long y0 = t - A;
		long s0 = y0 - oyt, s1 = y0 - oyt + rows;
		pyslice(s0, s1, bgt.h);
		need(s1 - s0 == rows && x - ox >= 0 && x - ox < bgt.w, "quilt_fill: source rows outside bgt");
		for (int r = 0; r < rows; r++)
			std::copy(bgt.px(int(s0) + r, x - ox), bgt.px(int(s0) + r, x - ox) + 3, S.px(r, i));
	}
	ImgF out = S;
	const ImgF src = crop(S, A - 130, A, 0, Wd);
	const int F = 4; // the search runs at a quarter size
	// pool(a): a[:h // F * F, :w // F * F].reshape(..).mean(axis=(1, 3)), as npcore's blockMean
	auto pool = [&](const ImgF& a) { return np::blockMean(a, F); };
	const ImgF srcd = pool(src);
	const int BW = 64, OV = 24, BH = 96, VOV = 16;
	const int OVd = OV / F;

	// ((window - ref) ** 2).sum(axis=(1, 2, 3)) over sliding windows of
	// srcd rows [r0, r0 + nr): numpy lays the difference out row-major in
	// (rows, windows, bwd * 3), so each window's sum adds, row by row from 0,
	// the pairwise sum of that row's bwd * 3 squares
	std::vector<float> sq;
	auto windowEnergy = [&](int r0, const ImgF& ref, int bwd, std::vector<float>& e) {
		const int nr = ref.h, nwin = srcd.w - bwd + 1, m = bwd * 3;
		e.assign(size_t(nwin), 0.0f);
		sq.resize(size_t(m));
		for (int w = 0; w < nwin; w++) {
			float acc = 0.0f;
			for (int r = 0; r < nr; r++) {
				const float* a = srcd.px(r0 + r, w);
				const float* b = ref.px(r, 0);
				for (int i = 0; i < m; i++) {
					float d = a[i] - b[i];
					sq[size_t(i)] = d * d;
				}
				acc = acc + np::sum(sq.data(), size_t(m));
			}
			e[size_t(w)] = acc;
		}
	};

	std::vector<float> e32, eb;
	std::vector<double> e;
	std::vector<int64_t> idx;
	for (int band_row = 0; band_row < 2; band_row++) {
		const int b0 = band_row == 0 ? 0 : BH - VOV, b1 = band_row == 0 ? BH : 2 * BH - VOV;
		ImgF prev;
		bool havePrev = false;
		int x = xs0 - SX0;
		// rows taken: the block plus context (above for the first row, below for the second)
		const int top_ctx = band_row == 0 ? CTX : VOV;
		while (x < Wd) {
			int bw = std::min(BW, Wd - x);
			bw -= bw % F;
			if (bw < OV + F * 2)
				break;
			const int bwd = bw / F;
			const int Hb = top_ctx + (b1 - b0) + (band_row == 1 ? CTX : 0);
			const int Hd = Hb / F;
			// what the block must match, from the output so far
			const ImgF above_d = pool(crop(out, A + b0 - top_ctx, A + b0, x, x + bw));
			ImgF below_d;
			if (band_row == 1)
				below_d = pool(crop(S, A + BAND, A + BAND + CTX, x, x + bw));
			ImgF prevd;
			if (havePrev)
				prevd = pool(prev);
			bool haveBest = false;
			double bestE = 0;
			int bestR0 = 0, bestJ = 0;
			for (int r0d = 0; r0d < srcd.h - Hd + 1; r0d++) {
				// blk = srcd[r0d:r0d + Hd]; top = its first ad rows' windows
				windowEnergy(r0d, above_d, bwd, e32);
				if (band_row == 1) {
					const int bd = below_d.h;
					windowEnergy(r0d + Hd - bd, below_d, bwd, eb);
					for (size_t w = 0; w < e32.size(); w++)
						e32[w] = e32[w] + 0.5f * eb[w];
				}
				if (havePrev) {
					// blk[:, :OVd]'s one window against prevd[:Hd, :OVd]: one
					// contiguous (Hd, OVd, 3) block, a single pairwise sum
					std::vector<float> d;
					d.reserve(size_t(Hd) * OVd * 3);
					for (int r = 0; r < Hd; r++)
						for (int j = 0; j < OVd; j++)
							for (int k = 0; k < 3; k++) {
								float v = srcd(r0d + r, j, k) - prevd(r, j, k);
								d.push_back(v * v);
							}
					const float left = np::sum(d.data(), d.size());
					for (float& v : e32)
						v = v + left;
				}
				// e (float32) + 1e5 * near (float64) + 40.0 * |sx - x| (float64)
				const size_t nwin = e32.size();
				e.resize(nwin);
				for (size_t w = 0; w < nwin; w++) {
					long ad = std::labs(long(w) * F - x);
					e[w] = (double(e32[w]) + 1e5 * (ad < F * 2 ? 1.0 : 0.0)) + 40.0 * double(ad);
				}
				if (energies)
					energies->insert(energies->end(), e.begin(), e.end());
				if (counts)
					counts->push_back(int64_t(nwin));
				// k = np.argsort(e)[:3] (ties to the lower index; see the header)
				const size_t nk = std::min<size_t>(3, nwin);
				idx.resize(nwin);
				std::iota(idx.begin(), idx.end(), 0);
				std::partial_sort(idx.begin(), idx.begin() + long(nk), idx.end(), [&](int64_t a, int64_t b) {
					return e[size_t(a)] < e[size_t(b)] || (e[size_t(a)] == e[size_t(b)] && a < b);
				});
				const int64_t jj = idx[size_t(rng.integers(0, int64_t(nk)))];
				if (!haveBest || e[size_t(jj)] < bestE) {
					haveBest = true;
					bestE = e[size_t(jj)];
					bestR0 = r0d * F;
					bestJ = int(jj) * F;
				}
			}
			need(haveBest, "quilt_fill: no search");
			const ImgF blk = crop(src, bestR0, bestR0 + Hb, bestJ, bestJ + bw);
			const int dy0 = A + b0 - top_ctx;
			need(blk.h == Hb && blk.w == bw && dy0 + Hb <= out.h, "quilt_fill: block outside the source");
			std::vector<float> wcol(size_t(bw), 1.0f);
			if (havePrev) {
				std::vector<double> l = np::linspace(0, 1, OV);
				for (int i = 0; i < OV; i++)
					wcol[size_t(i)] = float(l[size_t(i)]);
			}
			std::vector<float> wrow(size_t(Hb), 1.0f);
			if (band_row == 0) {
				// into the rock above, done before the old edge
				std::vector<double> l = np::linspace(0, 1, top_ctx + 10);
				for (int i = 0; i < top_ctx + 10; i++)
					wrow[size_t(i)] = float(l[size_t(i)]);
			} else {
				std::vector<double> l = np::linspace(0, 1, top_ctx);
				for (int i = 0; i < top_ctx; i++)
					wrow[size_t(i)] = float(l[size_t(i)]);
				std::vector<double> l2 = np::linspace(1, 0, CTX * 2);
				for (int i = 0; i < CTX * 2; i++)
					wrow[size_t(Hb - CTX * 2 + i)] = float(l2[size_t(i)] * 0.6);
			}
			for (int r = 0; r < Hb; r++)
				for (int c = 0; c < bw; c++) {
					const float wg = wrow[size_t(r)] * wcol[size_t(c)];
					float* d = out.px(dy0 + r, x + c);
					const float* s = blk.px(r, c);
					for (int k = 0; k < 3; k++)
						d[k] = d[k] * (1.0f - wg) + s[k] * wg;
				}
			prev = crop(out, dy0, dy0 + Hb, x + bw - OV, x + bw);
			havePrev = true;
			x += bw - OV;
		}
	}
	ImgF res = img;
	for (int i = 0; i < Wd; i++) {
		const int xx = SX0 + i;
		if (xx < xs0)
			continue;
		long t = pyround(top_of(xx)) - 16;
		long y0 = t - A;
		long d0 = y0 + A - CTX - oy, d1 = y0 + A + BAND + CTX - oy;
		pyslice(d0, d1, res.h);
		need(d1 - d0 == BAND + 2 * CTX && xx - ox >= 0 && xx - ox < res.w, "quilt_fill: band outside the picture");
		for (int r = 0; r < BAND + 2 * CTX; r++)
			std::copy(out.px(A - CTX + r, i), out.px(A - CTX + r, i) + 3, res.px(int(d0) + r, xx - ox));
	}
	return res;
}

ImgF tone_correct(const ImgF& res, const ImgF& bg, const Mask& band, double sigma) {
	const ImgF soft_bg = blurF(bg, 5);
	const ImgF low_res = blurF(res, sigma);
	const int Wd = band.w;
	std::vector<float> tops(size_t(Wd) * 3, 0.0f), bots(size_t(Wd) * 3, 0.0f);
	std::vector<std::pair<int, int>> tb(size_t(Wd), {-1, -1});
	for (int x = 0; x < Wd; x++) {
		int t, b;
		bandSpan(band, x, t, b);
		if (t < 0)
			continue;
		tb[size_t(x)] = {t, b};
		const float* st = soft_bg.px(std::max(t - 6, 0), x);
		std::copy(st, st + 3, &tops[size_t(x) * 3]);
		// (a vine or bamboo just below: the tone above carried on instead)
		const int yb = std::min(b + 6, bg.h - 1);
		const float* pb = bg.px(yb, x);
		const bool vine = pb[0] - pb[2] > 25 && np::meanFlat(pb, 3) > 50;
		for (int k = 0; k < 3; k++)
			bots[size_t(x) * 3 + k] = vine ? tops[size_t(x) * 3 + k] * 0.8f : soft_bg(yb, x, k);
	}
	const std::vector<double> kern = gaussKernel(40, 16.0);
	for (std::vector<float>* arr : {&tops, &bots})
		for (int c = 0; c < 3; c++) {
			std::vector<double> lane(static_cast<size_t>(Wd));
			for (int x = 0; x < Wd; x++)
				lane[size_t(x)] = (*arr)[size_t(x) * 3 + c];
			std::vector<double> r = np::convolve(np::padEdge(lane, 40), kern, "valid");
			for (int x = 0; x < Wd; x++)
				(*arr)[size_t(x) * 3 + c] = float(r[size_t(x)]);
		}
	ImgF out = res;
	for (int x = 0; x < Wd; x++) {
		auto [t, b] = tb[size_t(x)];
		if (t < 0)
			continue;
		const int n = b - t + 1;
		for (int i = 0; i < n; i++) {
			double a = (double(i) + 0.5) / n;
			double w = a * a * (3 - 2 * a);
			for (int k = 0; k < 3; k++) {
				double target = double(tops[size_t(x) * 3 + k]) * (1 - w) + double(bots[size_t(x) * 3 + k]) * w;
				float hp = res(t + i, x, k) - low_res(t + i, x, k);
				out(t + i, x, k) = float(double(hp) + target);
			}
		}
	}
	return out;
}

ImgF match_texture(const ImgF& res, const ImgF& bg, const Mask& band, double gain_max) {
	auto highPass = [](const ImgF& a) {
		ImgF b = blurF(a, 2.5);
		ImgF o = a;
		for (size_t i = 0; i < o.v.size(); i++)
			o.v[i] = a.v[i] - b.v[i];
		return o;
	};
	const ImgF hp_res = highPass(res);
	const ImgF hp_bg = highPass(bg);
	const int Wd = band.w;
	std::vector<float> gain(size_t(Wd), 1.0f);
	std::vector<std::pair<int, int>> spans(size_t(Wd), {-1, -1});
	// a[y0:y1, x] of an (h, w, 3) picture, its values in C order
	auto column = [](const ImgF& a, long y0, long y1, int x) {
		pyslice(y0, y1, a.h);
		std::vector<float> v;
		for (long y = y0; y < y1; y++)
			v.insert(v.end(), a.px(int(y), x), a.px(int(y), x) + 3);
		return v;
	};
	for (int x = 0; x < Wd; x++) {
		int t, b;
		bandSpan(band, x, t, b);
		if (t < 0)
			continue;
		spans[size_t(x)] = {t, b};
		std::vector<float> above = column(hp_bg, std::max(t - 60, 0), t - 4, x);
		std::vector<float> inside = column(hp_res, t + 10, b - 10, x);
		if (above.size() / 3 > 10 && inside.size() / 3 > 10) {
			const float sa = np::stdFlat(above.data(), above.size());
			const float si = np::stdFlat(inside.data(), inside.size());
			// above.std() / max(inside.std(), 1e-3): float32 scalars, or
			// float64 when Python's max returns the 1e-3
			const double q = 1e-3 > double(si) ? double(sa) / 1e-3 : double(sa / si);
			gain[size_t(x)] = float(np::clip(q, 1.0, gain_max));
		}
	}
	const std::vector<double> kern = gaussKernel(25, 10.0);
	std::vector<double> lane(gain.begin(), gain.end());
	const std::vector<double> g = np::convolve(np::padEdge(lane, 25), kern, "valid"); // float64 from here
	ImgF out = res;
	for (int x = 0; x < Wd; x++) {
		auto [t, b] = spans[size_t(x)];
		if (t < 0)
			continue;
		const int n = b - t + 1;
		// hp_res[..] * (gain[x] - 1): a float64 scalar on a float32 array, so float32
		const float g1 = float(g[size_t(x)] - 1);
		for (int i = 0; i < n; i++) {
			double ramp = np::clip(double(std::min(i, n - 1 - i)) / 12.0, 0.0, 1.0);
			for (int k = 0; k < 3; k++) {
				float hg = hp_res(t + i, x, k) * g1;
				out(t + i, x, k) = float(double(res(t + i, x, k)) + double(hg) * ramp);
			}
		}
	}
	return out;
}

ImgF restore_branches(const ImgF& img, const ImgF& scene, const ImgF& strip_mod, const TopOf& top_of, int ox, int oy,
	const std::vector<Poly>& polys, int sh) {
	ImgF res = img;
	const int Hh = img.h, Ww = img.w;
	Img8 m(Hh, Ww, 1, 0);
	int xmin = 0, xmax = -1;
	bool first = true;
	for (const Poly& p : polys) {
		std::vector<std::pair<double, double>> pts;
		for (auto [x, y] : p) {
			pts.push_back({double(x - ox), double(y - oy)});
			xmin = first ? x : std::min(xmin, x);
			xmax = first ? x : std::max(xmax, x);
			first = false;
		}
		np::polygon(m, pts, 255);
	}
	Mask keep(Hh, Ww, 1, 0);
	for (int x = xmin; x <= xmax; x++) {
		long t0 = pyround(top_of(x)) - 12;
		const long refc = pymod(x - 5630, 300);
		long y0 = t0 - oy, y1 = t0 - oy + sh;
		pyslice(y0, y1, scene.h);
		need(refc < strip_mod.w && y1 - y0 <= strip_mod.h, "restore_branches: column outside the module");
		for (long y = y0; y < y1; y++) {
			const float* c = scene.px(int(y), x - ox);
			const float* r = strip_mod.px(int(y - y0), int(refc));
			// np.abs(col - ref[:len(col), :3]).sum(axis=1), float32
			float d[3];
			for (int k = 0; k < 3; k++)
				d[k] = std::fabs(c[k] - r[k]);
			float diff = 0.0f + np::sum(d, 3);
			keep(int(y), x - ox) = diff > 70;
		}
	}
	for (int y = 0; y < Hh; y++)
		for (int x = 0; x < Ww; x++)
			if (keep(y, x) && m(y, x) > 0)
				std::copy(scene.px(y, x), scene.px(y, x) + 3, res.px(y, x));
	return res;
}

// ---- the proof ------------------------------------------------------------------------

namespace {

namespace fs = std::filesystem;

// a file in dir whose name matches ignoring case, or empty
fs::path findNoCase(const fs::path& dir, const std::string& name) {
	auto lower = [](std::string s) {
		for (char& ch : s)
			ch = char(std::tolower((unsigned char)ch));
		return s;
	};
	std::error_code ec;
	for (const auto& ent : fs::directory_iterator(dir, ec))
		if (lower(ent.path().filename().string()) == lower(name))
			return ent.path();
	return {};
}

// an uncompressed .s16 as MadeArt.cpp reads it (the hook runs before the
// engine's data folders and file cache exist, so it reads the game itself)
struct S16File {
	uint32_t flags = 0;
	std::string data;
	struct Frame {
		uint32_t off;
		int w, h;
	};
	std::vector<Frame> frames;
	uint16_t at(const Frame& f, int x, int y) const {
		size_t o = f.off + 2 * (size_t(y) * f.w + x);
		return uint16_t(uint8_t(data[o]) | (uint8_t(data[o + 1]) << 8));
	}
	void rgb(uint16_t v, uint8_t* p) const {
		if (flags & 1) {
			p[0] = uint8_t(((v >> 11) & 31) << 3);
			p[1] = uint8_t(((v >> 5) & 63) << 2);
			p[2] = uint8_t((v & 31) << 3);
		} else {
			p[0] = uint8_t(((v >> 10) & 31) << 3);
			p[1] = uint8_t(((v >> 5) & 31) << 3);
			p[2] = uint8_t((v & 31) << 3);
		}
	}
};

S16File readS16File(const fs::path& p) {
	S16File s;
	std::ifstream in(p, std::ios::binary);
	need(bool(in), "check: can't read a game file");
	s.data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	auto u16 = [&](size_t o) { return uint32_t(uint8_t(s.data[o]) | (uint8_t(s.data[o + 1]) << 8)); };
	need(s.data.size() >= 6, "check: short .s16");
	s.flags = u16(0) | (u16(2) << 16);
	const uint32_t n = u16(4);
	for (uint32_t i = 0; i < n; i++) {
		size_t e = 6 + 8 * size_t(i);
		need(e + 8 <= s.data.size(), "check: short .s16");
		S16File::Frame f{u16(e) | (u16(e + 2) << 16), int(u16(e + 4)), int(u16(e + 6))};
		need(size_t(f.off) + size_t(f.w) * f.h * 2 <= s.data.size(), "check: short .s16");
		s.frames.push_back(f);
	}
	return s;
}

// backgroundRegion, from the file
Rgba regionOf(const S16File& bg, int x0, int y0, int x1, int y1) {
	const int TW = 144, TH = 150, DOWN = 16;
	need(bg.frames.size() == 58 * 16, "check: Back.s16 is not 58 x 16 tiles");
	Rgba out(x1 - x0, y1 - y0);
	for (int col = x0 / TW; col <= (x1 - 1) / TW; col++)
		for (int row = y0 / TH; row <= (y1 - 1) / TH; row++) {
			const auto& f = bg.frames[size_t(col) * DOWN + row];
			int tx = col * TW, ty = row * TH;
			for (int y = std::max(y0, ty); y < std::min(y1, ty + TH); y++)
				for (int x = std::max(x0, tx); x < std::min(x1, tx + TW); x++) {
					uint8_t* p = out.at(x - x0, y - y0);
					bg.rgb(bg.at(f, x - tx, y - ty), p);
					p[3] = 255;
				}
		}
	return out;
}

// gameSprite, from the file
std::vector<Rgba> spriteOf(const S16File& s) {
	std::vector<Rgba> out;
	for (const auto& f : s.frames) {
		Rgba im(f.w, f.h);
		for (int y = 0; y < f.h; y++)
			for (int x = 0; x < f.w; x++) {
				uint16_t v = s.at(f, x, y);
				uint8_t* p = im.at(x, y);
				s.rgb(v, p);
				p[3] = v == 0 ? 0 : 255;
			}
		out.push_back(std::move(im));
	}
	return out;
}

struct Out {
	fs::path dir;
	std::ofstream index;
	template <typename T>
	void put(const std::string& name, const char* dtype, const std::vector<T>& v) {
		std::ofstream f(dir / (name + ".bin"), std::ios::binary);
		f.write(reinterpret_cast<const char*>(v.data()), std::streamsize(v.size() * sizeof(T)));
		index << name << " " << dtype << " " << v.size() << "\n";
	}
	void f4(const std::string& name, const ImgF& a) { put(name, "f4", a.v); }
	void u8(const std::string& name, const Img8& a) { put(name, "u8", a.v); }
};

} // namespace

int checkPaint(const std::string& dirName, const std::string& c2dir) {
	fs::path images = findNoCase(c2dir, "Images");
	need(!images.empty(), "check: no Images folder in the game's folder");
	fs::path back = findNoCase(images, "Back.s16"), swmpPath = findNoCase(images, "swmp.s16");
	need(!back.empty() && !swmpPath.empty(), "check: no Back.s16 or swmp.s16");
	const S16File bgFile = readS16File(back);
	GameInputs in = inputsFrom(regionOf(bgFile, OX, OY, OX + W, OY + H), regionOf(bgFile, OX, OY - 300, OX + W, OY + H),
		spriteOf(readS16File(swmpPath)));
	need(in.swmp.size() >= 4, "check: swmp.s16 has under 4 frames");

	Scene sc = makeScene(in);
	Mask mask;
	ImgF strip;
	old_tube(sc.tubecomp, in.ta, mask, strip);
	ImgF MOD, pc;
	new_tube(strip, MOD, pc);
	PaintSteps steps;
	ImgF base = paint_out(sc.scene, in.bg, in.bgt, mask, strip, &steps);
	// put_scenery's cut= form, as make() uses it (DECK_RM: the old deck's place)
	Mask deck_rm(H, W, 1, 0);
	const int DECK_RM[][4] = {{5214, 1600, 5320, 1627}, {5262, 1508, 5294, 1612}, {4895, 1555, 4997, 1626}, {5207, 1550, 5300, 1612}, {5275, 1600, 5398, 1626}};
	for (const auto& r : DECK_RM)
		for (int y = r[1] - OY; y < r[3] - OY; y++)
			for (int x = r[0] - OX; x < r[2] - OX; x++)
				deck_rm(y, x) = 1;
	ImgF cut = base;
	put_scenery(cut, in.swmp, sceneryFrom(1), &deck_rm);

	Out o;
	o.dir = dirName;
	fs::create_directories(o.dir);
	o.index.open(o.dir / "index.txt");
	o.put("consts", "f8", std::vector<double>{start_top, slope, ang, tube_length(), top_of(4776), top_of(6035), tube_y(ENTRY), tube_y(5000.25)});
	o.f4("bg", in.bg);
	o.f4("bgt", in.bgt);
	o.u8("ta", in.ta);
	o.f4("tubecomp", sc.tubecomp);
	o.f4("scene", sc.scene);
	o.u8("mask", mask);
	o.f4("strip", strip);
	o.f4("MOD", MOD);
	o.f4("pc", pc);
	o.put("quilt_e", "f8", steps.energies);
	o.put("quilt_e_n", "i8", steps.energyCounts);
	o.f4("quilt", steps.quilt);
	o.u8("band", steps.band);
	o.f4("toned", steps.toned);
	o.f4("textured", steps.textured);
	o.f4("restored", base);
	o.f4("base", base);
	o.f4("scenery_cut", cut);
	o.index.close();
	return 0;
}

namespace {
// OPENC2E_TW_CHECK_PAINT=DIR: write the check at startup and exit
struct CheckAtStartup {
	CheckAtStartup() {
		const char* dir = std::getenv("OPENC2E_TW_CHECK_PAINT");
		if (!dir || !*dir)
			return;
		std::string c2;
		if (const char* g = std::getenv("OPENC2E_TW_CHECK_PAINT_C2"); g && *g)
			c2 = g;
		else if (const char* home = std::getenv("HOME"))
			c2 = std::string(home) + "/.steam/debian-installation/steamapps/common/Creatures The Albian Years/Creatures 2";
		try {
			checkPaint(dir, c2);
			std::fprintf(stderr, "tube_walk_paint: check written to %s\n", dir);
			std::exit(0);
		} catch (const std::exception& e) {
			std::fprintf(stderr, "tube_walk_paint: check failed: %s\n", e.what());
			std::exit(1);
		}
	}
} checkAtStartup;
} // namespace

} // namespace tubewalk
} // namespace MadeArt
