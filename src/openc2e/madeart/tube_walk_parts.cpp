// The walkable glass tube's made art, part: the tube's parts (art/tube_walk.py: paste_rot_masked, blob, smooth_fill, deck_fill, load_front; art/tube_walk_lib.py: dock_join, stalactites, render_rock_earth).
//
// Each function follows its Python line by line; the comments give numpy's
// type at each step where it matters (NumPy 1.x promotion: a float32 array
// with a Python float stays float32, the scalar rounded to float32 first; a
// float32 array with a float64 array becomes float64). Proved by
// art/tube_walk_check_parts.py (OPENC2E_TW_CHECK_PARTS=DIR, see the header).

#include "tube_walk_parts.h"
#include "common/devenv.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
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

#include "tube_walk_front_data.inc"

using np::Img8;
using np::ImgD;
using np::ImgF;

const double kPi = 3.14159265358979323846; // M_PI (CPython's math.radians)
const int kSceneW = 2340, kSceneH = 840, kSceneOX = 4400, kSceneOY = 1250; // tube_walk.py W, H, OX, OY
const int kFrontX = 4780, kFrontY = 1460; // tube_walk.py FRONT_AT

// Python's round() to an int: half to even
int pyRound(double v) {
	return int(std::nearbyint(v));
}

void need(bool ok, const char* what) {
	if (!ok)
		throw std::runtime_error(std::string("tube_walk_parts: ") + what);
}

// tube_walk_lib.py blur(a, s): a float32 picture through 8 bits and Pillow's GaussianBlur
ImgF blur(const ImgF& a, double s) {
	return np::toF32(np::gaussianBlur(np::clipToU8(a), s));
}

// a float32 plane as an 8-bit "L" picture: (a * 255).astype(np.uint8), no clip
Img8 maskTo8(const Img8& m) {
	Img8 o(m.h, m.w, 1);
	for (size_t i = 0; i < m.v.size(); i++)
		o.v[i] = m.v[i] ? 255 : 0;
	return o;
}

} // namespace

// ===========================================================================
// tube_walk.py

void paste_rot_masked(ImgF& out, const ImgF& pc, double angleDeg, std::pair<double, double> leftTop,
	const Img8& keepOut, int ox, int oy) {
	need(pc.c == 4 && out.c == 3, "paste_rot_masked: pc RGBA, out RGB");
	// im = Image.fromarray(pc.clip(0, 255).astype(np.uint8), 'RGBA')
	Img8 im = np::clipToU8(pc);
	Img8 r = np::rotate(im, -angleDeg, np::Resample::Bicubic, true);
	const double th = angleDeg * (kPi / 180.0);
	const int w0 = im.w, h0 = im.h;
	const double dx = -w0 / 2.0, dy = -h0 / 2.0; // (PIL turns about the center)
	const double kx = dx * std::cos(th) - dy * std::sin(th) + r.w / 2.0;
	const double ky = dx * std::sin(th) + dy * std::cos(th) + r.h / 2.0;
	const int px = pyRound(leftTop.first - kx - ox), py = pyRound(leftTop.second - ky - oy);
	const int y0 = std::max(0, py), x0 = std::max(0, px);
	const int y1 = std::min(out.h, py + r.h), x1 = std::min(out.w, px + r.w);
	for (int y = y0; y < y1; y++)
		for (int x = x0; x < x1; x++) {
			const uint8_t* a = r.px(y - py, x - px);
			// al (float32 0/1) * ~keep_out
			const float al = a[3] > 128 ? 1.0f : 0.0f;
			const float sal = al * (keepOut(y, x) ? 0.0f : 1.0f);
			float* reg = out.px(y, x);
			for (int k = 0; k < 3; k++)
				reg[k] = reg[k] * (1.0f - sal) + float(a[k]) * sal;
		}
}

std::vector<std::pair<double, double>> blob(const std::vector<std::pair<double, double>>& points, double jitter, uint64_t seed) {
	np::Generator r(seed);
	std::vector<std::pair<double, double>> pts;
	const size_t m = points.size();
	for (size_t i = 0; i < m; i++) {
		const double ax = points[i].first, ay = points[i].second;
		const double bx = points[(i + 1) % m].first, by = points[(i + 1) % m].second;
		const int n = std::max(2, int(np::hypot(bx - ax, by - ay) / 14));
		for (int k = 0; k < n; k++) {
			const double t = double(k) / n;
			const double px = ax + (bx - ax) * t + r.uniform(-jitter, jitter);
			const double py = ay + (by - ay) * t + r.uniform(-jitter, jitter);
			pts.emplace_back(px, py);
		}
	}
	return pts;
}

void smooth_fill(ImgF& F, const Img8& M, int iters, double vk, double div) {
	const int h = F.h, w = F.w, c = F.c;
	need(M.h == h && M.w == w, "smooth_fill: mask size");
	// F[M] = F[~M].mean(axis=0): the outside's rows added in C order, / n (float32)
	std::vector<float> rest;
	std::vector<size_t> in;
	for (size_t i = 0, n = size_t(h) * w; i < n; i++) {
		if (M.v[i])
			in.push_back(i);
		else
			rest.insert(rest.end(), &F.v[i * c], &F.v[i * c] + c);
	}
	const std::vector<float> mean = np::meanAxis0(rest.data(), rest.size() / size_t(c), c);
	for (size_t i : in)
		for (int k = 0; k < c; k++)
			F.v[i * c + k] = mean[size_t(k)];
	// the np.roll neighbours of each filled pixel (wrapping at the edges)
	struct Nb {
		size_t self, l, r, u, d;
	};
	std::vector<Nb> nb;
	nb.reserve(in.size());
	for (size_t i : in) {
		const int y = int(i / size_t(w)), x = int(i % size_t(w));
		auto at = [&](int yy, int xx) { return (size_t(yy) * w + size_t(xx)) * c; };
		nb.push_back({i * c, at(y, (x - 1 + w) % w), at(y, (x + 1) % w), at((y - 1 + h) % h, x), at((y + 1) % h, x)});
	}
	const float vkf = float(vk), divf = float(div);
	std::vector<float> avg(nb.size() * c);
	for (int it = 0; it < iters; it++) {
		// avg = (roll(F, 1, 1) + roll(F, -1, 1) + vk * (roll(F, 1, 0) + roll(F, -1, 0))) / div
		for (size_t j = 0; j < nb.size(); j++)
			for (int k = 0; k < c; k++) {
				const float* v = F.v.data();
				avg[j * c + k] = ((v[nb[j].l + k] + v[nb[j].r + k]) + vkf * (v[nb[j].u + k] + v[nb[j].d + k])) / divf;
			}
		for (size_t j = 0; j < nb.size(); j++)
			for (int k = 0; k < c; k++)
				F.v[nb[j].self + k] = avg[j * c + k];
	}
}

void deck_fill(ImgF& img, const Img8& deckRm, DeckBox box, uint64_t seed, const Img8* inside) {
	const int y0 = std::max(0, box.y0), y1 = std::min(img.h, box.y1);
	const int x0 = std::max(0, box.x0), x1 = std::min(img.w, box.x1);
	if (y1 <= y0 || x1 <= x0)
		return;
	const int h = y1 - y0, w = x1 - x0, c = img.c;
	ImgF F(h, w, c);
	Img8 rm(h, w, 1);
	for (int y = 0; y < h; y++) {
		std::memcpy(F.px(y, 0), img.px(y0 + y, x0), sizeof(float) * size_t(w) * c);
		for (int x = 0; x < w; x++)
			rm(y, x) = deckRm(y0 + y, x0 + x);
	}
	// M = MaxFilter(5) of the deck's place, > 0; less inside
	Img8 M = np::maxFilter(maskTo8(rm), 5);
	size_t count = 0;
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			uint8_t& m = M(y, x);
			m = m > 0 ? 1 : 0;
			if (inside && (*inside)(y0 + y, x0 + x))
				m = 0;
			count += m;
		}
	if (!count)
		return;
	smooth_fill(F, M, 2500, 0.5, 3.0);
	// F[M] *= (1 + 0.06 * rng.normal(0, 1, (M.sum(), 1))): float64, cast back
	np::Generator g(seed);
	const std::vector<double> z = g.normal(0, 1, count);
	size_t j = 0;
	for (size_t i = 0; i < M.v.size(); i++)
		if (M.v[i]) {
			const double f = 1 + 0.06 * z[j++];
			for (int k = 0; k < c; k++)
				F.v[i * c + k] = float(double(F.v[i * c + k]) * f);
		}
	for (int y = 0; y < h; y++)
		std::memcpy(img.px(y0 + y, x0), F.px(y, 0), sizeof(float) * size_t(w) * c);
}

Img8 load_front() {
	Img8 front(kSceneH, kSceneW, 1, 0);
	const int oy = kFrontY - kSceneOY, ox = kFrontX - kSceneOX;
	size_t pos = 0;
	bool val = false;
	const size_t total = size_t(kFrontW) * kFrontH;
	for (uint32_t n : kFrontRuns) {
		need(pos + n <= total, "load_front: embedded mask overruns");
		if (val)
			for (size_t i = pos; i < pos + n; i++) {
				const int y = oy + int(i / kFrontW), x = ox + int(i % kFrontW);
				if (y < kSceneH && x < kSceneW)
					front(y, x) = 1;
			}
		pos += n;
		val = !val;
	}
	need(pos == total, "load_front: embedded mask short");
	return front;
}

// ===========================================================================
// tube_walk_lib.py

void dock_join(ImgF& o, const ImgF& base, const ImgF& MOD, int X0, double startTop, double slope,
	const Img8& keep, int OX, int OY, int n, int blend, int kstart) {
	need(MOD.c == 4 && o.c == 3 && base.c == 3, "dock_join: MOD RGBA, o and base RGB");
	const int SH = MOD.h;
	// the module's first ring (columns whose outline rises above the glass)
	int R = 0;
	bool any = false;
	for (int x = 0; x < std::min(60, MOD.w); x++)
		for (int y = 0; y < std::min(8, SH); y++)
			if (MOD(y, x, 3) > 0) {
				R = x;
				any = true;
				break;
			}
	if (!any)
		R = 0;
	n = std::max(n, R + 26);
	// the rotated tube's slanted end and first ring, by the collar: back to the background
	const int y0 = int(startTop - 12 - OY);
	{
		const int ya = std::max(0, y0 - 16), yb = std::min(o.h, y0 + SH + 30);
		const int xa = std::max(0, X0 - 26 - OX), xb = std::min(o.w, X0 + n - OX);
		for (int y = ya; y < yb; y++)
			for (int x = xa; x < xb; x++)
				for (int k = 0; k < 3; k++)
					o(y, x, k) = base(y, x, k);
	}
	std::vector<float> c(size_t(SH + 1) * 4);
	for (int k = kstart; k < n + blend; k++) {
		const int x = X0 + k;
		// the column the rotated tube shows here, mirrored back from just past the ring nearer the collar
		int s = k - 20;
		if (s <= R + 3)
			s = 2 * (R + 4) - s;
		s = std::min(std::max(s, 0), MOD.w - 1);
		const double top = startTop - 12 + slope * k;
		const int ti = int(std::floor(top));
		const double fr = top - ti;
		// sub-pixel shift: c[:SH] += col * (1 - fr); c[1:] += col * fr (float32)
		const float f0 = float(1 - fr), f1 = float(fr);
		std::fill(c.begin(), c.end(), 0.0f);
		for (int y = 0; y < SH; y++)
			for (int j = 0; j < 4; j++)
				c[size_t(y) * 4 + j] += MOD(y, s, j) * f0;
		for (int y = 1; y <= SH; y++)
			for (int j = 0; j < 4; j++)
				c[size_t(y) * 4 + j] += MOD(y - 1, s, j) * f1;
		const double w = k < n ? 1.0 : 1 - double(k - n + 1) / (blend + 1);
		const float wf = float(w);
		const int yy0 = ti - OY, cx = x - OX;
		need(cx >= 0 && cx < o.w && yy0 >= 0, "dock_join: column off the scene");
		const int rows = std::min(SH + 1, o.h - yy0);
		for (int y = 0; y < rows; y++) {
			const float a = c[size_t(y) * 4 + 3] / 255.0f;
			const float aa = (a * wf) * (keep(yy0 + y, cx) ? 0.0f : 1.0f);
			float* reg = o.px(yy0 + y, cx);
			for (int j = 0; j < 3; j++)
				reg[j] = reg[j] * (1.0f - aa) + c[size_t(y) * 4 + j] * aa;
		}
	}
}

ImgF fbm(np::Generator& rng, int H, int W, const std::vector<double>& cells, const std::vector<double>& amps) {
	ImgF out(H, W, 1, 0.0f);
	for (size_t i = 0; i < cells.size() && i < amps.size(); i++) {
		const int sh = std::max(2, int(H / cells[i]) + 2), sw = std::max(2, int(W / cells[i]) + 2);
		const std::vector<double> small = rng.random(size_t(sh) * sw);
		Img8 s8(sh, sw, 1);
		for (size_t j = 0; j < small.size(); j++)
			s8.v[j] = np::u8(float(small[j]) * 255.0f);
		const Img8 rs = np::resize(s8, W, H, np::Resample::Bicubic);
		const float a = float(amps[i]);
		for (size_t j = 0; j < out.v.size(); j++)
			out.v[j] += (float(rs.v[j]) / 255.0f - 0.5f) * a;
	}
	return out;
}

ImgF q8(const ImgF& a) {
	ImgF o(a.h, a.w, a.c);
	for (size_t i = 0; i < a.v.size(); i++)
		o.v[i] = float(np::u8(np::clip((a.v[i] + 0.5f) * 255.0f, 0.0f, 255.0f))) / 255.0f - 0.5f;
	return o;
}

namespace {

// a float32 plane through 8 bits: ((a + 0.5) * 255).clip(0, 255).astype(np.uint8)
Img8 plane8(const ImgF& a) {
	Img8 o(a.h, a.w, 1);
	for (size_t i = 0; i < a.v.size(); i++)
		o.v[i] = np::u8(np::clip((a.v[i] + 0.5f) * 255.0f, 0.0f, 255.0f));
	return o;
}

// np.asarray(im).astype(np.float32) / 255 - 0.5
ImgF unplane8(const Img8& a) {
	ImgF o(a.h, a.w, 1);
	for (size_t i = 0; i < a.v.size(); i++)
		o.v[i] = float(a.v[i]) / 255.0f - 0.5f;
	return o;
}

} // namespace

void stalactites(ImgF& o, const ImgF& bg, const std::vector<Stalactite>& items, int OX, int OY, uint64_t seed, int ss) {
	np::Generator rng(seed);
	std::vector<double> Lt = {-0.6, -0.45, 0.66};
	{
		const double nl = np::norm(Lt);
		for (double& v : Lt)
			v /= nl;
	}
	const float ssf = float(ss);
	for (const Stalactite& it : items) {
		const double x = it.x, y = it.y, wd = it.wd, ln = it.ln, lean = it.lean;
		// its color and grain: the rock right where it hangs from (median per channel, float32, * 1.3)
		float colr[3];
		{
			const int ya = int(y) - 10 - OY, yb = int(y) - 2 - OY;
			const int xa = int(x - wd / 2) - OX, xb = int(x + wd / 2) - OX;
			need(ya >= 0 && yb <= bg.h && xa >= 0 && xb <= bg.w && yb > ya && xb > xa, "stalactites: patch off the scene");
			for (int k = 0; k < 3; k++) {
				std::vector<float> lane;
				for (int yy = ya; yy < yb; yy++)
					for (int xx = xa; xx < xb; xx++)
						lane.push_back(bg(yy, xx, k));
				colr[k] = np::median(lane, false) * 1.3f;
			}
		}
		const int pad = 8;
		const int bw = int(wd + 2 * pad + std::fabs(lean) * ln + 6), bh = int(ln + pad + 4);
		const double mn = lean < 0 ? lean : 0.0;
		const int bx0 = int(x - bw / 2.0 + mn * ln * 0.5), by0 = int(y - 3);
		const int W = bw * ss, H = bh * ss;
		const size_t N = size_t(W) * H;
		const float xf = float(x), lnf = float(ln);
		// per pixel (float32 unless said): yy, xx, t, cx, r
		ImgF YY(H, W), XX(H, W), T(H, W), CX(H, W), R(H, W);
		for (int i = 0; i < H; i++)
			for (int j = 0; j < W; j++) {
				const float yy = float(i) / ssf, xx = float(j) / ssf;
				const float t = np::clip((yy - 3.0f) / lnf, 0.0f, 1.2f); // 0 at the root, 1 at the tip
				YY(i, j) = yy;
				XX(i, j) = xx;
				T(i, j) = t;
				// cx = (x - bx0) + lean * ln * pw(t, 1.6) + 1.2 * np.sin(t * 7 + x)
				CX(i, j) = (float(x - bx0) + float(lean * ln) * np::pw(t, 1.6)) + 1.2f * np::sin(t * 7.0f + xf);
				// r = wd / 2 * pw(np.clip(1 - t, 0, 1), 0.85) + 0.6
				float r = float(wd / 2) * np::pw(np::clip(1.0f - t, 0.0f, 1.0f), 0.85) + 0.6f;
				// r = r + wd * 0.45 * np.exp(-t * ln / 5.0)
				r = r + float(wd * 0.45) * np::exp((-t * lnf) / 5.0f);
				R(i, j) = r;
			}
		{
			const ImgF lumpy = fbm(rng, H, W, {double(5 * ss)}, {1.0});
			const float x07 = float(x * 0.7);
			for (size_t q = 0; q < N; q++) {
				float r = R.v[q] * (1.0f + 0.12f * lumpy.v[q]);
				const float t = T.v[q];
				r = r * (1.0f + 0.07f * np::sin((t * lnf) / 3.2f + x07));
				R.v[q] = t >= 1.0f ? 0.0f : r;
			}
		}
		// u, inside, the normal, diff (float64)
		ImgF U(H, W), Nn(H, W, 3);
		Img8 inside(H, W);
		for (size_t q = 0; q < N; q++) {
			const float u = (XX.v[q] - CX.v[q]) / std::max(R.v[q], 0.001f);
			U.v[q] = u;
			inside.v[q] = (std::fabs(u) <= 1.0f) && (YY.v[q] >= 3.0f) && (T.v[q] < 1.0f);
			Nn.v[q * 3] = u;
			Nn.v[q * 3 + 1] = 0.12f;
			Nn.v[q * 3 + 2] = std::sqrt(np::clip(1.0f - u * u, 0.0f, 1.0f));
		}
		{
			const ImgF nrm = np::normLast(Nn);
			for (size_t q = 0; q < N; q++)
				for (int k = 0; k < 3; k++)
					Nn.v[q * 3 + k] = Nn.v[q * 3 + k] / nrm.v[q];
		}
		ImgD diff;
		{
			ImgD nl(H, W, 3);
			for (size_t q = 0; q < N; q++)
				for (int k = 0; k < 3; k++)
					nl.v[q * 3 + k] = double(Nn.v[q * 3 + k]) * Lt[size_t(k)];
			diff = np::sumLast(nl);
			for (double& v : diff.v)
				v = np::clip(v, 0.0, 1.0);
		}
		const ImgF grain = fbm(rng, H, W, {double(6 * ss), 2.5 * ss}, {1.0, 0.8});
		const ImgF streak = q8(fbm(rng, H, W, {1.2 * ss}, {1.0}));
		// shade = (0.42 + 1.0 * pw(diff, 1.1)) * (1 + 0.35 * grain + 0.15 * streak); shade *= 1 - 0.25 * t (float64)
		ImgD shade(H, W);
		for (size_t q = 0; q < N; q++) {
			const float g = (1.0f + 0.35f * grain.v[q]) + 0.15f * streak.v[q];
			double s = (0.42 + 1.0 * np::pw(diff.v[q], 1.1)) * double(g);
			s *= double(1.0f - 0.25f * T.v[q]);
			shade.v[q] = s;
		}
		// the rock's own grain, from just above where it hangs, stretched down its length
		ImgF tex(H, W);
		{
			const int ya = int(y) - 26 - OY, xa = int(x) - 12 - OX;
			need(ya >= 0 && ya + 24 <= bg.h && xa >= 0 && xa + 24 <= bg.w, "stalactites: grain patch off the scene");
			ImgF tp(24, 24, 3);
			for (int i = 0; i < 24; i++)
				for (int j = 0; j < 24; j++)
					for (int k = 0; k < 3; k++)
						tp(i, j, k) = bg(ya + i, xa + j, k);
			const ImgF tpi = np::toF32(np::resize(np::clipToU8(tp), W, H, np::Resample::Bicubic));
			const ImgF tm = np::meanLast(tpi);
			const float mall = np::meanFlat(tpi);
			const float dv = double(mall) > 1.0 ? mall : 1.0f; // max(1.0, tpi.mean())
			for (size_t q = 0; q < N; q++)
				tex.v[q] = tm.v[q] / dv;
		}
		// flowstone: fine streaks running down its length
		ImgF fl = fbm(rng, H, W / 4 + 1, {1.0 * ss, 0.5 * ss}, {1.0, 0.6});
		fl = unplane8(np::resize(plane8(fl), W, H, np::Resample::Bilinear));
		fl = unplane8(np::resize(np::resize(plane8(fl), W, std::max(2, H / 10), np::Resample::Bilinear), W, H, np::Resample::Bicubic));
		double colr64[3];
		{
			const double cm[3] = {1.06, 1.0, 0.9};
			for (int k = 0; k < 3; k++)
				colr64[k] = double(colr[k]) * cm[k];
		}
		const float rimc[3] = {30, 28, 24};
		ImgD col(H, W, 3);
		ImgF A(H, W);
		for (size_t q = 0; q < N; q++) {
			const float t = T.v[q], u = U.v[q];
			const double inner = (shade.v[q] * double(0.3f + 0.7f * tex.v[q])) * double(1.0f + 0.5f * fl.v[q]);
			const float rim = np::clip(-u - 0.55f, 0.0f, 1.0f) * 2.0f;
			const float edge = np::clip(((1.0f - std::fabs(u)) * R.v[q]) / 1.4f, 0.0f, 1.0f); // dark outline
			const float ef = 0.35f + 0.65f * edge;
			for (int k = 0; k < 3; k++) {
				double cv = colr64[k] * inner;
				cv += double((rimc[k] * rim) * (1.0f - t)); // lit rim
				cv *= double(ef);
				col.v[q * 3 + k] = cv;
			}
			A.v[q] = inside.v[q] ? 1.0f : 0.0f;
		}
		// its shadow on the wall: the same shape moved down-right, soft
		ImgF sh;
		{
			const int ry = int(5 * ss), rx = int(4 * ss);
			Img8 s8(H, W, 1);
			for (int i = 0; i < H; i++)
				for (int j = 0; j < W; j++) {
					const int si = ((i - ry) % H + H) % H, sj = ((j - rx) % W + W) % W;
					s8(i, j) = np::u8(A(si, sj) * 255.0f);
				}
			const Img8 b = np::gaussianBlur(s8, 2.5 * ss);
			sh = ImgF(H, W);
			for (size_t q = 0; q < N; q++)
				sh.v[q] = float(b.v[q]) / 255.0f;
		}
		const ImgD rgb0 = np::blockMean(col, ss);
		const ImgF al0 = np::blockMean(A, ss);
		const ImgF sh0 = np::blockMean(sh, ss);
		const ImgD sd = np::blockMean(shade, ss);
		const int ry0 = by0 - OY, rx0 = bx0 - OX;
		need(ry0 >= 0 && rx0 >= 0 && ry0 + bh <= o.h && rx0 + bw <= o.w, "stalactites: cone off the scene");
		// the root grows out of the rock behind it: over its top 60% the wall's own pixels, given the cone's light
		std::vector<double> sdl;
		for (size_t q = 0; q < sd.v.size(); q++)
			if (al0.v[q] > 0.5f)
				sdl.push_back(sd.v[q]);
		const double med = np::median(sdl, true);
		const double mdv = med > 1e-3 ? med : 1e-3; // max(1e-3, median)
		for (int i = 0; i < bh; i++) {
			const double ty = double(i - 3) / ln;
			const double rb = np::pw(np::clip(1 - ty / 0.6, 0.0, 1.0), 1.1);
			for (int j = 0; j < bw; j++) {
				const size_t q = size_t(i) * bw + j;
				float* reg = o.px(ry0 + i, rx0 + j);
				const double ownf = np::clip(sd.v[q] / mdv, 0.55, 1.5);
				double rgb[3];
				for (int k = 0; k < 3; k++) {
					const double own = double(reg[k]) * ownf;
					rgb[k] = rgb0.v[q * 3 + k] * (1 - rb) + own * rb;
				}
				const float alf = al0.v[q];
				const float pa = np::pw(np::clip(1.0f - alf, 0.0f, 1.0f), 0.4);
				const double al = (double(alf) * (1 - 0.8 * rb * double(pa))) * np::clip(1 - rb * 0.45, 0.0, 1.0);
				const double shv = double(sh0.v[q]) * (1 - rb);
				const double dark = 1 - 0.45 * shv * (1 - al);
				for (int k = 0; k < 3; k++) {
					reg[k] = float(double(reg[k]) * dark);
					reg[k] = float(double(reg[k]) * (1 - al) + rgb[k] * al);
				}
			}
		}
	}
}

RockEarth render_rock_earth(const std::vector<std::pair<double, double>>& outline, const ImgF& sample, uint64_t seed, int ss, int patch) {
	need(!outline.empty() && sample.c == 3, "render_rock_earth: outline, RGB sample");
	np::Generator rng(seed);
	double mnx = outline[0].first, mxx = mnx, mny = outline[0].second, mxy = mny;
	for (const auto& p : outline) {
		mnx = std::min(mnx, p.first);
		mxx = std::max(mxx, p.first);
		mny = std::min(mny, p.second);
		mxy = std::max(mxy, p.second);
	}
	const int x0 = int(mnx) - 6, y0 = int(mny) - 6;
	const int w = int(mxx) + 7 - x0, h = int(mxy) + 7 - y0;
	const int W = w * ss, H = h * ss;
	Img8 m(H, W, 1, 0);
	{
		std::vector<std::pair<double, double>> pts;
		for (const auto& p : outline)
			pts.emplace_back((p.first - x0) * ss, (p.second - y0) * ss);
		np::polygon(m, pts, 255);
	}
	ImgF mm(H, W);
	for (size_t q = 0; q < mm.v.size(); q++)
		mm.v[q] = float(m.v[q]) / 255.0f;
	// distance-like depth from the edge
	ImgF depth(H, W, 1, 0.0f);
	{
		Img8 cur = m;
		for (int k = 0; k < 24; k++) {
			cur = np::minFilter(cur, 5);
			for (size_t q = 0; q < depth.v.size(); q++)
				depth.v[q] += cur.v[q] > 128 ? 1.0f : 0.0f;
		}
		Img8 d8(H, W, 1);
		for (size_t q = 0; q < depth.v.size(); q++)
			d8.v[q] = np::u8(depth.v[q] / 24.0f * 255.0f);
		const Img8 b = np::gaussianBlur(d8, 4 * ss);
		for (size_t q = 0; q < depth.v.size(); q++)
			depth.v[q] = float(b.v[q]) / 255.0f;
	}
	const ImgF depth1 = np::blockMean(depth, ss);
	const ImgF mm1 = np::blockMean(mm, ss);

	// the earth's surface: feathered patches from the sample, at 1x
	const int sh_ = sample.h, sw_ = sample.w;
	ImgF acc(h, w, 3, 0.0f), wt(h, w, 1, 0.0f);
	ImgF feather(patch, patch);
	for (int i = 0; i < patch; i++)
		for (int j = 0; j < patch; j++) {
			const float xx = float(j), yy = float(i);
			const float f = std::min(std::min(xx + 1.0f, float(patch) - xx), std::min(yy + 1.0f, float(patch) - yy)) / float(patch / 2.0);
			feather(i, j) = np::pw(np::clip(f, 0.0f, 1.0f), 1.5);
		}
	const int step = patch / 2;
	for (int py = -step; py < h; py += step)
		for (int px = -step; px < w; px += step) {
			const int jy = py + int(rng.integers(-6, 7));
			const int jx = px + int(rng.integers(-6, 7));
			const int sy = int(rng.integers(0, sh_ - patch));
			const int sx = int(rng.integers(0, sw_ - patch));
			const int a0 = std::max(0, jy), b0 = std::max(0, jx);
			const int a1 = std::min(h, jy + patch), b1 = std::min(w, jx + patch);
			if (a1 <= a0 || b1 <= b0)
				continue;
			for (int i = a0; i < a1; i++)
				for (int j = b0; j < b1; j++) {
					const float f = feather(i - jy, j - jx);
					for (int k = 0; k < 3; k++)
						acc(i, j, k) += sample(sy + i - jy, sx + j - jx, k) * f;
					wt(i, j) += f;
				}
		}
	ImgF alb(h, w, 3);
	for (int i = 0; i < h; i++)
		for (int j = 0; j < w; j++) {
			const float d = std::max(wt(i, j), 0.001f);
			for (int k = 0; k < 3; k++)
				alb(i, j, k) = acc(i, j, k) / d;
		}
	// (feathered averaging softens the grain a little: give it back)
	{
		const ImgF bl = blur(alb, 1.2);
		for (size_t q = 0; q < alb.v.size(); q++)
			alb.v[q] = bl.v[q] + (alb.v[q] - bl.v[q]) * 1.5f;
	}

	// a rounded mass: large soft lumps over the depth, lit from the upper left, gently
	const ImgF lump = fbm(rng, h, w, {40, 18}, {1.0, 0.5});
	ImgF height(h, w);
	for (size_t q = 0; q < height.v.size(); q++)
		height.v[q] = 40.0f * std::sqrt(np::clip(depth1.v[q], 0.0f, 1.0f)) + 12.0f * lump.v[q];
	ImgF gy, gx;
	np::gradient(height, gy, gx);
	const size_t N = size_t(h) * w;
	ImgF n(h, w, 3);
	for (size_t q = 0; q < N; q++) {
		n.v[q * 3] = -gx.v[q];
		n.v[q * 3 + 1] = -gy.v[q];
		n.v[q * 3 + 2] = 1.0f * 1.4f;
	}
	{
		const ImgF nrm = np::normLast(n);
		for (size_t q = 0; q < N; q++)
			for (int k = 0; k < 3; k++)
				n.v[q * 3 + k] = n.v[q * 3 + k] / nrm.v[q];
	}
	std::vector<double> L = {-0.55, -0.55, 0.63};
	{
		const double nl = np::norm(L);
		for (double& v : L)
			v = v / nl;
	}
	ImgD diff;
	{
		ImgD nl(h, w, 3);
		for (size_t q = 0; q < N; q++)
			for (int k = 0; k < 3; k++)
				nl.v[q * 3 + k] = double(n.v[q * 3 + k]) * L[size_t(k)];
		diff = np::sumLast(nl);
		for (double& v : diff.v)
			v = np::clip(v, 0.0, 1.0);
	}
	double dm;
	{
		std::vector<double> sel;
		for (size_t q = 0; q < N; q++)
			if (mm1.v[q] > 0.5f)
				sel.push_back(diff.v[q]);
		dm = np::meanFlat(sel.data(), sel.size());
	}
	ImgD rgb(h, w, 3);
	for (int i = 0; i < h; i++) {
		const double ey = np::clip(double(i) / h * 1.4 - 0.2, 0.0, 1.0);
		for (int j = 0; j < w; j++) {
			const size_t q = size_t(i) * w + j;
			const double shade = 1.0 + 0.55 * (diff.v[q] - dm);
			const float edge = np::clip(depth1.v[q] * 6.0f, 0.0f, 1.0f);
			const double ef = double(0.55f + 0.45f * edge);
			// a touch of the lab's warm light on the east side, low down
			const double east = np::clip((double(j) / w - 0.55) / 0.45, 0.0, 1.0) * ey;
			const double warm = 1 + 0.25 * east * double(np::clip(n.v[q * 3], 0.0f, 1.0f)) * 3;
			for (int k = 0; k < 3; k++)
				rgb.v[q * 3 + k] = ((double(alb.v[q * 3 + k]) * shade) * ef) * warm;
		}
	}
	// grayer toward the top: from halfway up, the color graded toward a neutral gray
	int top = -1, bot = -1;
	for (int i = 0; i < h; i++) {
		float mx = mm1(i, 0);
		for (int j = 1; j < w; j++)
			mx = std::max(mx, mm1(i, j));
		if (mx > 0.5f) {
			if (top < 0)
				top = i;
			bot = i;
		}
	}
	need(top >= 0, "render_rock_earth: empty outline");
	const double mid = (top + bot) / 2.0;
	const double gd = (mid - top > 1) ? mid - top : 1.0;
	const ImgD lum = np::meanLast(rgb);
	const double gm[3] = {0.98, 1.0, 1.04};
	for (int i = 0; i < h; i++) {
		double g = np::clip((mid - i) / gd, 0.0, 1.0);
		g = (g * g * (3 - 2 * g)) * 0.85;
		for (int j = 0; j < w; j++) {
			const size_t q = size_t(i) * w + j;
			for (int k = 0; k < 3; k++) {
				const double gray = lum.v[q] * 1.3 * gm[k];
				rgb.v[q * 3 + k] = rgb.v[q * 3 + k] * (1 - g) + gray * g;
			}
		}
	}
	for (double& v : rgb.v)
		v = np::clip(v, 0.0, 255.0);
	RockEarth res;
	res.x0 = x0;
	res.y0 = y0;
	res.rgb = std::move(rgb);
	res.alpha = mm1;
	return res;
}

// ===========================================================================
// the developer check (OPENC2E_TW_CHECK_PARTS)

namespace {

struct Arr {
	std::string dtype;
	std::vector<int> shape;
	std::string bytes;
};

struct Folder {
	std::filesystem::path dir;
	std::map<std::string, Arr> arrs;
	void read(const std::filesystem::path& d) {
		dir = d;
		std::ifstream idx(d / "index.txt");
		need(bool(idx), "check: no index.txt in the inputs");
		std::string line;
		while (std::getline(idx, line)) {
			std::istringstream ls(line);
			std::string name;
			Arr a;
			if (!(ls >> name >> a.dtype))
				continue;
			int s;
			while (ls >> s)
				a.shape.push_back(s);
			std::ifstream f(d / (name + ".bin"), std::ios::binary);
			std::ostringstream ss;
			ss << f.rdbuf();
			a.bytes = ss.str();
			arrs[name] = std::move(a);
		}
	}
	const Arr& get(const std::string& name, const char* dtype) const {
		auto it = arrs.find(name);
		need(it != arrs.end(), ("check: missing input " + name).c_str());
		need(it->second.dtype == dtype, ("check: wrong dtype for " + name).c_str());
		return it->second;
	}
	bool has(const std::string& name) const { return arrs.count(name) > 0; }
	template <typename T>
	np::Img<T> img(const std::string& name, const char* dtype) const {
		const Arr& a = get(name, dtype);
		need(a.shape.size() == 2 || a.shape.size() == 3, ("check: not a picture: " + name).c_str());
		np::Img<T> o(a.shape[0], a.shape[1], a.shape.size() == 3 ? a.shape[2] : 1);
		need(a.bytes.size() == o.v.size() * sizeof(T), ("check: short file " + name).c_str());
		std::memcpy(o.v.data(), a.bytes.data(), a.bytes.size());
		return o;
	}
	ImgF f32(const std::string& name) const { return img<float>(name, "f4"); }
	Img8 mask(const std::string& name) const { return img<uint8_t>(name, "u1"); }
	std::vector<double> f64(const std::string& name) const {
		const Arr& a = get(name, "f8");
		std::vector<double> v(a.bytes.size() / sizeof(double));
		std::memcpy(v.data(), a.bytes.data(), v.size() * sizeof(double));
		return v;
	}
	std::vector<std::pair<double, double>> points(const std::string& name) const {
		const std::vector<double> v = f64(name);
		std::vector<std::pair<double, double>> p;
		for (size_t i = 0; i + 1 < v.size(); i += 2)
			p.emplace_back(v[i], v[i + 1]);
		return p;
	}
};

struct Writer {
	std::filesystem::path dir;
	std::ofstream idx;
	void open(const std::filesystem::path& d) {
		dir = d;
		std::filesystem::create_directories(d);
		idx.open(d / "index.txt");
	}
	void put(const std::string& name, const char* dtype, const std::vector<int>& shape, const void* data, size_t bytes) {
		std::ofstream f(dir / (name + ".bin"), std::ios::binary);
		f.write(static_cast<const char*>(data), std::streamsize(bytes));
		idx << name << " " << dtype;
		for (int s : shape)
			idx << " " << s;
		idx << "\n";
	}
	template <typename T>
	void img(const std::string& name, const char* dtype, const np::Img<T>& a) {
		std::vector<int> shape = {a.h, a.w};
		if (a.c != 1)
			shape.push_back(a.c);
		put(name, dtype, shape, a.v.data(), a.v.size() * sizeof(T));
	}
};

} // namespace

int checkParts(const std::string& dirName) {
	const std::filesystem::path dir(dirName);
	Folder in;
	in.read(dir / "in");
	Writer out;
	out.open(dir / "out");
	std::ifstream cases(dir / "in" / "cases.txt");
	need(bool(cases), "check: no cases.txt in the inputs");
	std::string kind;
	int idx;
	while (cases >> kind >> idx) {
		const std::string p = kind + std::to_string(idx) + ".";
		std::fprintf(stderr, "tube_walk_parts: %s %d\n", kind.c_str(), idx);
		if (kind == "paste") {
			ImgF o = in.f32(p + "out");
			const std::vector<double> a = in.f64(p + "args");
			paste_rot_masked(o, in.f32(p + "pc"), a[0], {a[1], a[2]}, in.mask(p + "keep"));
			out.img(p + "out", "f4", o);
		} else if (kind == "smooth") {
			ImgF F = in.f32(p + "F");
			const std::vector<double> a = in.f64(p + "args");
			smooth_fill(F, in.mask(p + "M"), int(a[0]), a[1], a[2]);
			out.img(p + "F", "f4", F);
		} else if (kind == "deck") {
			ImgF img = in.f32(p + "img");
			const std::vector<double> a = in.f64(p + "args");
			Img8 inside;
			if (a[5] != 0)
				inside = in.mask(p + "inside");
			deck_fill(img, in.mask(p + "deck_rm"), {int(a[0]), int(a[1]), int(a[2]), int(a[3])}, uint64_t(a[4]), a[5] != 0 ? &inside : nullptr);
			out.img(p + "img", "f4", img);
		} else if (kind == "stal") {
			ImgF o = in.f32(p + "o");
			const std::vector<double> iv = in.f64(p + "items");
			const std::vector<double> a = in.f64(p + "args");
			std::vector<Stalactite> items;
			for (size_t i = 0; i + 4 < iv.size(); i += 5)
				items.push_back({iv[i], iv[i + 1], iv[i + 2], iv[i + 3], iv[i + 4]});
			stalactites(o, in.f32(p + "bg"), items, int(a[0]), int(a[1]), uint64_t(a[2]), int(a[3]));
			out.img(p + "o", "f4", o);
		} else if (kind == "dock") {
			ImgF o = in.f32(p + "o");
			const std::vector<double> a = in.f64(p + "args");
			dock_join(o, in.f32(p + "base"), in.f32(p + "MOD"), int(a[0]), a[1], a[2], in.mask(p + "keep"), int(a[3]), int(a[4]),
				int(a[5]), int(a[6]), int(a[7]));
			out.img(p + "o", "f4", o);
		} else if (kind == "blob") {
			const std::vector<double> a = in.f64(p + "args");
			const auto pts = blob(in.points(p + "points"), a[0], uint64_t(a[1]));
			std::vector<double> flat;
			for (const auto& q : pts) {
				flat.push_back(q.first);
				flat.push_back(q.second);
			}
			out.put(p + "pts", "f8", {int(pts.size()), 2}, flat.data(), flat.size() * sizeof(double));
		} else if (kind == "rock") {
			const std::vector<double> a = in.f64(p + "args");
			const RockEarth r = render_rock_earth(in.points(p + "outline"), in.f32(p + "sample"), uint64_t(a[0]), int(a[1]), int(a[2]));
			const int64_t xy[2] = {r.x0, r.y0};
			out.put(p + "xy", "i8", {2}, xy, sizeof(xy));
			out.img(p + "rgb", "f8", r.rgb);
			out.img(p + "alpha", "f4", r.alpha);
		} else if (kind == "front") {
			out.img(p + "front", "u1", load_front());
		} else if (kind == "fbm") {
			const std::vector<double> a = in.f64(p + "args");
			np::Generator g{uint64_t(a[0])};
			const ImgF f = fbm(g, int(a[1]), int(a[2]), in.f64(p + "cells"), in.f64(p + "amps"));
			out.img(p + "out", "f4", f);
			out.img(p + "q8", "f4", q8(f));
		} else {
			throw std::runtime_error("check: unknown case " + kind);
		}
	}
	return 0;
}

namespace {
// OPENC2E_TW_CHECK_PARTS=DIR: run the check at startup and exit
struct CheckAtStartup {
	CheckAtStartup() {
		const char* dir = devenv("OPENC2E_TW_CHECK_PARTS");
		if (!dir || !*dir)
			return;
		try {
			checkParts(dir);
			std::fprintf(stderr, "tube_walk_parts: results written to %s/out\n", dir);
			std::exit(0);
		} catch (const std::exception& e) {
			std::fprintf(stderr, "tube_walk_parts: check failed: %s\n", e.what());
			std::exit(1);
		}
	}
} checkAtStartup;
} // namespace

} // namespace tubewalk
} // namespace MadeArt
