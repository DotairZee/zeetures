// The walkable glass tube's made art, part: the gene lab's new west end (art/tube_walk.py: lab_west_end; art/tube_walk_lib.py: Canvas, light_pools, rust_and_grime, trim_wall, lab_wall).
//
// Line by line as the Python, including its random streams' order (Python
// evaluates left to right, so `noise(a) + noise(b)` draws a first) and
// numpy 1.x's promotions: a float32 array with a Python or numpy scalar
// stays float32 (the scalar rounded to float32 first, comparisons too);
// with a float64 array it becomes float64; `f32 *= f64 array` is done in
// float64 and rounded back. Proof: art/tube_walk_check_lab.py.

#include "tube_walk_lab.h"
#include "common/devenv.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>

// numpy and Pillow were built without fused multiply-add contraction; so
// is this, whatever the flags (as npcore.cpp)
#if defined(__clang__)
#pragma clang fp contract(off)
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif

namespace MadeArt {
namespace tubewalk {

using np::Img8;
using np::ImgD;
using np::ImgF;

namespace {

const int OX = kLabSceneX, OY = kLabSceneY;

// CPython's float floor division (float_divmod), for `self.h // cells`
double pyFloorDiv(double vx, double wx) {
	double mod = std::fmod(vx, wx);
	double div = (vx - mod) / wx;
	if (mod != 0) {
		if ((wx < 0) != (mod < 0))
			div -= 1.0;
	}
	double floordiv;
	if (div != 0) {
		floordiv = std::floor(div);
		if (div - floordiv > 0.5)
			floordiv += 1.0;
	} else {
		floordiv = std::copysign(0.0, vx / wx);
	}
	return floordiv;
}

// a.sum(-1) over 3 channels as numpy adds them (pairwise_sum, n < 8: from
// -0.0, then onto the reduction's 0)
template <typename T>
T sum3(T a, T b, T c) {
	T r = T(-0.0);
	r += a;
	r += b;
	r += c;
	return T(0) + r;
}
template <typename T>
T mean3(const T* p) {
	return sum3(p[0], p[1], p[2]) / T(3);
}

std::array<double, 3> unit(double a, double b, double c) {
	double n = np::norm({a, b, c});
	return {a / n, b / n, c / n};
}
// lib.L (key light, upper left), lib.RIM (warm rim from the lab), lib.WARM
const std::array<double, 3>& keyLight() {
	static const std::array<double, 3> v = unit(-0.55, -0.55, 0.63);
	return v;
}
const std::array<double, 3>& rimLight() {
	static const std::array<double, 3> v = unit(0.9, -0.1, 0.25);
	return v;
}
const float WARM[3] = {255, 150, 80};

// a crop of a picture: rows [y0, y1), columns [x0, x1)
template <typename T>
np::Img<T> crop(const np::Img<T>& a, int y0, int y1, int x0, int x1) {
	np::Img<T> o(y1 - y0, x1 - x0, a.c);
	for (int y = y0; y < y1; y++)
		std::copy(a.px(y, x0), a.px(y, x0) + size_t(x1 - x0) * a.c, o.px(y - y0, 0));
	return o;
}

void needF64(const Canvas& c, const char* what) {
	if (!c.is64)
		throw std::logic_error(std::string("tube_walk_lab: ") + what + " is ported for a float64 canvas only (lab_wall's path)");
}

} // namespace

ImgF blur(const ImgF& a, double s) { return np::toF32(np::gaussianBlur(np::clipToU8(a), s)); }
ImgF blur(const ImgD& a, double s) { return np::toF32(np::gaussianBlur(np::clipToU8(a), s)); }

// ===========================================================================
// Canvas

Canvas::Canvas(int x0_, int y0_, int w_, int h_, uint64_t seed)
	: x0(x0_), y0(y0_), w(w_), h(h_), W(w_ * SS), H(h_ * SS), rng(seed) {
	rgbF = ImgF(H, W, 3);
	a = ImgF(H, W, 1);
	X = ImgF(H, W, 1);
	Y = ImgF(H, W, 1);
	for (int yy = 0; yy < H; yy++)
		for (int xx = 0; xx < W; xx++) {
			X(yy, xx) = float(x0) + (float(xx) + 0.5f) / float(SS);
			Y(yy, xx) = float(y0) + (float(yy) + 0.5f) / float(SS);
		}
}

ImgF Canvas::noise(double cells, double amp) {
	const int sh = std::max(2, int(pyFloorDiv(h, cells)) + 2);
	const int sw = std::max(2, int(pyFloorDiv(w, cells)) + 2);
	std::vector<double> r = rng.random(size_t(sh) * sw);
	Img8 small(sh, sw, 1);
	for (size_t i = 0; i < r.size(); i++)
		small.v[i] = np::u8(float(r[i]) * 255.0f);
	Img8 im = np::resize(small, W, H, np::Resample::Bicubic);
	ImgF o(H, W, 1);
	const float fa = float(amp);
	for (size_t i = 0; i < o.v.size(); i++)
		o.v[i] = ((float(im.v[i]) / 255.0f - 0.5f) * 2.0f) * fa;
	return o;
}

ImgD Canvas::shade(const ImgF& n, const ImgF& albedo, double spec, double shin, double rimk) {
	const auto& Lv = keyLight();
	const auto& R = rimLight();
	ImgD o(H, W, 3);
	const double spec255 = spec * 255;
	float rw[3];
	for (int k = 0; k < 3; k++)
		rw[k] = float(rimk) * WARM[k]; // rimk * WARM: float32
	for (size_t i = 0, m = size_t(H) * W; i < m; i++) {
		const float* nn = &n.v[i * 3];
		const float* al = &albedo.v[i * 3];
		const double n0 = nn[0], n1 = nn[1], n2 = nn[2];
		// (n * L) is float64 (L is a float64 array)
		const double nl = sum3(n0 * Lv[0], n1 * Lv[1], n2 * Lv[2]);
		const double diff = np::clip(nl, 0.0, 1.0);
		double r[3];
		r[0] = (2 * nl) * n0 - Lv[0];
		r[1] = (2 * nl) * n1 - Lv[1];
		r[2] = (2 * nl) * n2 - Lv[2];
		const double sp = np::pw(np::clip(sum3(r[0] * 0.0, r[1] * 0.0, r[2] * 1.0), 0.0, 1.0), shin);
		const double rim = np::pw(np::clip(sum3(n0 * R[0], n1 * R[1], n2 * R[2]), 0.0, 1.0), 3);
		const float am = mean3(al) / 128.0f; // albedo.mean(-1) / 128, float32
		const double dd = 0.22 + 0.85 * diff;
		for (int k = 0; k < 3; k++)
			o.v[i * 3 + k] = (double(al[k]) * dd + spec255 * sp) + (double(rw[k]) * rim) * double(am);
	}
	return o;
}

void Canvas::put(const Mask& mask, const ImgF& col) {
	if (is64)
		throw std::logic_error("tube_walk_lab: a float32 put on a float64 canvas is not ported");
	for (size_t i = 0, m = size_t(H) * W; i < m; i++) {
		const float mf = mask[i] ? 1.0f : 0.0f;
		for (int k = 0; k < 3; k++)
			rgbF.v[i * 3 + k] = rgbF.v[i * 3 + k] * (1.0f - mf) + col.v[i * 3 + k] * mf;
		a.v[i] = std::max(a.v[i], mf);
	}
}

void Canvas::put(const Mask& mask, const ImgD& col) {
	if (!is64) {
		// rgb * (1 - m) is float32, + col * m float64
		rgb = ImgD(H, W, 3);
		for (size_t i = 0, m = size_t(H) * W; i < m; i++) {
			const float mf = mask[i] ? 1.0f : 0.0f;
			for (int k = 0; k < 3; k++) {
				const float p = rgbF.v[i * 3 + k] * (1.0f - mf);
				rgb.v[i * 3 + k] = double(p) + col.v[i * 3 + k] * double(mf);
			}
			a.v[i] = std::max(a.v[i], mf);
		}
		is64 = true;
		rgbF = ImgF();
		return;
	}
	for (size_t i = 0, m = size_t(H) * W; i < m; i++) {
		const float mf = mask[i] ? 1.0f : 0.0f;
		for (int k = 0; k < 3; k++)
			rgb.v[i * 3 + k] = rgb.v[i * 3 + k] * double(1.0f - mf) + col.v[i * 3 + k] * double(mf);
		a.v[i] = std::max(a.v[i], mf);
	}
}

void Canvas::sphere(double cx, double cy, double r, std::array<double, 3> color, double spec) {
	const float fcx = float(cx), fcy = float(cy), fr = float(r);
	ImgF n(H, W, 3), alb(H, W, 3);
	Mask m(size_t(H) * W);
	for (size_t i = 0; i < m.size(); i++) {
		const float dx = (X.v[i] - fcx) / fr, dy = (Y.v[i] - fcy) / fr;
		const float d2 = dx * dx + dy * dy;
		n.v[i * 3] = dx;
		n.v[i * 3 + 1] = dy;
		n.v[i * 3 + 2] = std::sqrt(np::clip(1.0f - d2, 0.0f, 1.0f));
		m[i] = d2 <= 1.0f;
		for (int k = 0; k < 3; k++)
			alb.v[i * 3 + k] = float(color[k]);
	}
	put(m, shade(n, alb, spec, 30));
}

void Canvas::cylH(double cx0, double cx1, double cy, double r, std::array<double, 3> color, double spec, double tarnish) {
	const float fcy = float(cy), fr = float(r), fx0 = float(cx0), fx1 = float(cx1);
	ImgF n(H, W, 3), alb(H, W, 3);
	Mask m(size_t(H) * W);
	for (size_t i = 0; i < m.size(); i++) {
		const float dy = (Y.v[i] - fcy) / fr;
		m[i] = std::fabs(dy) <= 1.0f && X.v[i] >= fx0 && X.v[i] < fx1;
		n.v[i * 3] = 0.0f;
		n.v[i * 3 + 1] = dy;
		n.v[i * 3 + 2] = std::sqrt(np::clip(1.0f - dy * dy, 0.0f, 1.0f));
		for (int k = 0; k < 3; k++)
			alb.v[i * 3 + k] = float(color[k]);
	}
	ImgF nz = noise(9, 0.10);
	for (size_t i = 0; i < m.size(); i++)
		for (int k = 0; k < 3; k++)
			alb.v[i * 3 + k] *= 1.0f + nz.v[i];
	if (tarnish != 0) {
		ImgF t = noise(14, 1.0);
		const float tc[3] = {70, 90, 60}, ft = float(tarnish);
		for (size_t i = 0; i < m.size(); i++) {
			const float tt = np::clip(t.v[i], 0.0f, 1.0f) * ft;
			for (int k = 0; k < 3; k++)
				alb.v[i * 3 + k] = alb.v[i * 3 + k] * (1.0f - tt) + tc[k] * tt;
		}
	}
	put(m, shade(n, alb, spec, 26));
}

void Canvas::flangeH(double cx, double cy, double r, double fw, std::array<double, 3> color, int bolts) {
	const float fcx = float(cx), fcy = float(cy), fr = float(r), hw = float(fw / 2);
	ImgF n(H, W, 3), alb(H, W, 3);
	Mask m(size_t(H) * W);
	for (size_t i = 0; i < m.size(); i++) {
		const float dy = (Y.v[i] - fcy) / fr;
		m[i] = std::fabs(dy) <= 1.0f && std::fabs(X.v[i] - fcx) <= hw;
		n.v[i * 3] = 0.0f;
		n.v[i * 3 + 1] = dy;
		n.v[i * 3 + 2] = std::sqrt(np::clip(1.0f - dy * dy, 0.0f, 1.0f));
		for (int k = 0; k < 3; k++)
			alb.v[i * 3 + k] = float(color[k]);
	}
	put(m, shade(n, alb, 0.3, 20));
	for (int k = 0; k < bolts; k++) {
		const double by = cy + (double(k) - double(bolts - 1) / 2) * (r * 1.4 / double(std::max(1, bolts - 1)));
		sphere(cx, by, 1.6, {color[0] * 1.3, color[1] * 1.3, color[2] * 1.3});
	}
}

void Canvas::ibeamH(int bx0, int bx1, int by0, int by1, std::array<int, 3> color) {
	const int bh = by1 - by0;
	ImgF n(H, W, 3), alb(H, W, 3), v(H, W, 1);
	Mask m(size_t(H) * W);
	for (size_t i = 0; i < m.size(); i++) {
		m[i] = X.v[i] >= float(bx0) && X.v[i] < float(bx1) && Y.v[i] >= float(by0) && Y.v[i] < float(by1);
		v.v[i] = (Y.v[i] - float(by0)) / float(bh);
		float* nn = &n.v[i * 3];
		nn[0] = 0, nn[1] = 0, nn[2] = 1;
		if (v.v[i] < 0.2f)
			nn[0] = 0, nn[1] = -0.6f, nn[2] = 0.8f;
		if (v.v[i] > 0.8f)
			nn[0] = 0, nn[1] = 0.6f, nn[2] = 0.8f;
	}
	ImgF nz = noise(8, 0.12);
	for (size_t i = 0; i < m.size(); i++)
		for (int k = 0; k < 3; k++)
			alb.v[i * 3 + k] = float(color[k]) * (1.0f + nz.v[i]);
	ImgD col = shade(n, alb, 0.2, 12);
	for (size_t i = 0; i < m.size(); i++)
		if (v.v[i] >= 0.2f && v.v[i] <= 0.8f)
			for (int k = 0; k < 3; k++)
				col.v[i * 3 + k] *= 0.72;
	put(m, col);
	for (int rx = bx0 + 8; rx < bx1; rx += 22)
		sphere(rx, by0 + bh * 0.5, 1.6, {color[0] * 1.4, color[1] * 1.4, color[2] * 1.4});
}

void Canvas::grime(double amount) {
	needF64(*this, "grime");
	ImgF n24 = noise(24, 1.0);
	ImgF n6 = noise(6, 1.0);
	const float fa = float(amount);
	for (size_t i = 0, m = size_t(H) * W; i < m; i++) {
		const float g = np::clip(n24.v[i] + 0.3f * n6.v[i], -1.0f, 1.0f);
		const float f = 1.0f - fa * np::clip(g, 0.0f, 1.0f);
		for (int k = 0; k < 3; k++)
			rgb.v[i * 3 + k] *= double(f);
	}
	// darker toward the floor and in the corners
	for (size_t i = 0, m = size_t(H) * W; i < m; i++) {
		const float v = (Y.v[i] - float(y0)) / float(h);
		const float f = 1.0f - 0.35f * (v * v);
		for (int k = 0; k < 3; k++)
			rgb.v[i * 3 + k] *= double(f);
	}
}

std::pair<ImgD, ImgF> Canvas::result() const {
	needF64(*this, "result");
	ImgD r = np::blockMean(rgb, SS);
	for (auto& v : r.v)
		v = np::clip(v, 0.0, 255.0);
	return {r, np::blockMean(a, SS)};
}

// ===========================================================================
// light_pools, rust_and_grime, trim_wall

void lightPools(Canvas& c, const std::vector<LightPool>& pools, double ambient) {
	needF64(c, "light_pools");
	const size_t m = size_t(c.H) * c.W;
	std::vector<float> lum(m, float(ambient));
	std::vector<float> tint(m * 3, 0.0f);
	for (const LightPool& p : pools) {
		const float fpx = float(p.px), fpy = float(p.py), d = float(2 * p.rad * p.rad), fk = float(p.k);
		const float col[3] = {float(p.col[0]), float(p.col[1]), float(p.col[2])};
		for (size_t i = 0; i < m; i++) {
			const float dx = c.X.v[i] - fpx, dy = c.Y.v[i] - fpy;
			const float g = np::exp(-((dx * dx + dy * dy) / d));
			lum[i] += fk * g;
			for (int k = 0; k < 3; k++)
				tint[i * 3 + k] += g * col[k];
		}
	}
	for (size_t i = 0; i < m; i++)
		for (int k = 0; k < 3; k++) {
			const float warm = 1.0f + (0.25f * tint[i * 3 + k]) / 255.0f;
			c.rgb.v[i * 3 + k] = (c.rgb.v[i * 3 + k] * double(lum[i])) * double(warm);
		}
}

void rustAndGrime(Canvas& c, const Canvas::Mask& msk, double rust, double streaks, double chips) {
	needF64(c, "rust_and_grime");
	const size_t m = size_t(c.H) * c.W;
	ImgF n10 = c.noise(10, 1.0);
	ImgF n3 = c.noise(3, 0.6);
	ImgF n2 = c.noise(2, 1.0);
	const float frust = float(rust);
	const float rcb[3] = {120, 58, 22};
	for (size_t i = 0; i < m; i++) {
		const float mf = msk[i] ? 1.0f : 0.0f;
		const float blot = np::clip(n10.v[i] * 1.6f + n3.v[i] - 0.35f, 0.0f, 1.0f) * frust;
		const float rcf = 0.8f + 0.4f * np::clip(n2.v[i], -1.0f, 1.0f);
		const float k = blot * mf;
		for (int j = 0; j < 3; j++) {
			const float rc = rcb[j] * rcf;
			c.rgb.v[i * 3 + j] = c.rgb.v[i * 3 + j] * double(1.0f - k * 0.85f) + double((rc * k) * 0.85f);
		}
	}
	// streaks: wide, soft stains (a few columns of noise, blurred)
	const int sw = std::max(3, c.w / 14);
	std::vector<double> r = c.rng.random(size_t(5) * sw);
	Img8 small(5, sw, 1);
	for (size_t i = 0; i < r.size(); i++)
		small.v[i] = np::u8(float(r[i]) * 255.0f);
	Img8 st8 = np::gaussianBlur(np::resize(small, c.W, c.H, np::Resample::Bicubic), 4 * Canvas::SS);
	const float fst = float(streaks);
	for (size_t i = 0; i < m; i++) {
		const float mf = msk[i] ? 1.0f : 0.0f;
		float st = float(st8.v[i]) / 255.0f;
		st = np::clip((st - 0.5f) * 2.0f, 0.0f, 1.0f) * fst;
		const float f = 1.0f - (0.5f * st) * mf;
		for (int j = 0; j < 3; j++)
			c.rgb.v[i * 3 + j] *= double(f);
	}
	// chips: clustered spots of bare metal; rare dark ones
	ImgF sp = c.noise(1.5, 1.0);
	ImgF sp2 = c.noise(4, 1.0);
	const float cb[3] = {140, 136, 126}, fch = float(chips);
	const double keep = 1 - chips * 0.6;
	for (size_t i = 0; i < m; i++) {
		if (!msk[i])
			continue;
		if (sp.v[i] > 0.9f && sp2.v[i] > 0.2f) {
			for (int j = 0; j < 3; j++)
				c.rgb.v[i * 3 + j] = c.rgb.v[i * 3 + j] * keep + double((cb[j] * fch) * 0.6f);
		}
		if (sp.v[i] < -0.95f && sp2.v[i] < -0.3f) {
			for (int j = 0; j < 3; j++)
				c.rgb.v[i * 3 + j] = c.rgb.v[i * 3 + j] * 0.55;
		}
	}
}

void trimWall(Canvas& c, int x0, int y0, int x1, int y1, std::array<int, 3> plate, std::array<int, 3> trim,
	uint64_t seed, const std::vector<TrimLight>& light) {
	if (c.is64)
		throw std::logic_error("tube_walk_lab: trim_wall is ported for a float32 canvas only (lab_wall's path)");
	np::Generator rng(seed);
	const ImgF &X = c.X, &Y = c.Y;
	const size_t m = size_t(c.H) * c.W;
	Canvas::Mask M(m);
	for (size_t i = 0; i < m; i++)
		M[i] = X.v[i] >= float(x0) && X.v[i] < float(x1) && Y.v[i] >= float(y0) && Y.v[i] < float(y1);
	ImgF alb(c.H, c.W, 3);
	{
		ImgF a1 = c.noise(14, 0.18);
		ImgF a2 = c.noise(4, 0.08);
		ImgF a3 = c.noise(1.5, 0.05);
		for (size_t i = 0; i < m; i++) {
			const float f = ((1.0f + a1.v[i]) + a2.v[i]) + a3.v[i];
			for (int k = 0; k < 3; k++)
				alb.v[i * 3 + k] = float(plate[k]) * f;
		}
	}
	std::vector<float> lum(m, 0.75f);
	for (const TrimLight& l : light) {
		const float flx = float(l.lx), fly = float(l.ly), d = float(2 * l.rad * l.rad), fk = float(l.k);
		for (size_t i = 0; i < m; i++) {
			const float dx = X.v[i] - flx;
			const float dy = (Y.v[i] - fly) * 0.8f;
			lum[i] += fk * np::exp(-((dx * dx + dy * dy) / d));
		}
	}
	{
		ImgF col(c.H, c.W, 3);
		for (size_t i = 0; i < m; i++)
			for (int k = 0; k < 3; k++)
				col.v[i * 3 + k] = alb.v[i * 3 + k] * lum[i];
		c.put(M, col);
	}
	// the insets: recursive splits into irregular rectangles
	struct R {
		int x0, y0, x1, y1;
	};
	std::vector<R> rects = {{x0, y0, x1, y1}}, out;
	while (!rects.empty()) {
		R q = rects.back();
		rects.pop_back();
		const int w = q.x1 - q.x0, h = q.y1 - q.y0;
		if ((w < 70 && h < 60) || (w < 40 || h < 34) || (rng.random() < 0.25 && w < 110 && h < 110)) {
			out.push_back(q);
			continue;
		}
		if (w > h * 1.2 || (w >= 70 && rng.random() < 0.4)) {
			const int cut = q.x0 + int(w * rng.uniform(0.3, 0.7));
			rects.push_back({q.x0, q.y0, cut, q.y1});
			rects.push_back({cut, q.y0, q.x1, q.y1});
		} else {
			const int cut = q.y0 + int(h * rng.uniform(0.3, 0.7));
			rects.push_back({q.x0, q.y0, q.x1, cut});
			rects.push_back({q.x0, cut, q.x1, q.y1});
		}
	}
	const float tcol[3] = {float(trim[0]), float(trim[1]), float(trim[2])};
	c.crease.assign(m, 0);
	float* rgbF = c.rgbF.v.data();
	// strip_h / strip_v: a raised strip [a, b) along, [p, p + t) across
	auto strip = [&](bool horiz, int a, int b, int p, double t) {
		const float fp = float(p), pt = float(p + t), plit = float(p + 0.9), psh = float(p + t + 1.5);
		const float fa = float(a), fb = float(b);
		const float lo1[3] = {tcol[0] * 1.0f, tcol[1] * 1.0f, tcol[2] * 1.0f};
		const float lo2[3] = {tcol[0] * 1.35f, tcol[1] * 1.35f, tcol[2] * 1.35f};
		for (size_t i = 0; i < m; i++) {
			if (!M[i])
				continue;
			const float along = horiz ? X.v[i] : Y.v[i];
			const float across = horiz ? Y.v[i] : X.v[i];
			if (!(along >= fa && along < fb))
				continue;
			const bool mm = across >= fp && across < pt;
			const bool lit = mm && across < plit;
			const bool sh = across >= pt && across < psh;
			if (mm)
				for (int k = 0; k < 3; k++)
					rgbF[i * 3 + k] = lo1[k] * lum[i];
			if (lit)
				for (int k = 0; k < 3; k++)
					rgbF[i * 3 + k] = std::min(255.0f, lo2[k] * lum[i] + 6.0f);
			if (sh)
				for (int k = 0; k < 3; k++)
					rgbF[i * 3 + k] *= 0.45f;
			if (mm || sh)
				c.crease[i] = 1;
		}
	};
	auto stripH = [&](int xa, int xb, int y) { strip(true, xa, xb, y, 2.2); };
	auto stripV = [&](int x, int ya, int yb) { strip(false, ya, yb, x, 2.2); };
	for (const R& q : out) {
		const int inset = 3 + int(rng.integers(0, 3));
		const int ax0 = q.x0 + inset, ay0 = q.y0 + inset, ax1 = q.x1 - inset, ay1 = q.y1 - inset;
		if (ax1 - ax0 < 8 || ay1 - ay0 < 8)
			continue;
		// the inset sits back a little (darker), its trim round it
		for (size_t i = 0; i < m; i++)
			if (M[i] && X.v[i] >= float(ax0) && X.v[i] < float(ax1) && Y.v[i] >= float(ay0) && Y.v[i] < float(ay1))
				for (int k = 0; k < 3; k++)
					rgbF[i * 3 + k] *= 0.93f;
		std::vector<double> sides = rng.random(4);
		if (sides[0] > 0.35)
			stripH(ax0, ax1, ay0);
		if (sides[1] > 0.35)
			stripH(ax0, ax1, ay1 - 2);
		if (sides[2] > 0.35)
			stripV(ax0, ay0, ay1);
		if (sides[3] > 0.35)
			stripV(ax1 - 2, ay0, ay1);
		if (rng.random() < 0.15) // a paired line
			stripH(ax0 + 4, ax1 - 4, ay0 + 5);
	}
}

// ===========================================================================
// lab_wall

LabWall labWall(const Tap* tap) {
	const int X0 = kWallX0, X1 = kWallX1, Y0 = kWallY0;
	Canvas c(X0, Y0, X1 - X0, kWallY1 - Y0, 8);
	const size_t m = size_t(c.H) * c.W;
	auto tapRgb = [&](const char* name) {
		if (!tap)
			return;
		if (c.is64) {
			if (tap->f64)
				tap->f64(name, c.rgb);
		} else if (tap->f32) {
			tap->f32(name, c.rgbF);
		}
	};
	auto tapA = [&](const char* name) {
		if (tap && tap->f32)
			tap->f32(name, c.a);
	};
	trimWall(c, X0, Y0, X1, kWallFloor, {78, 66, 40}, {70, 46, 32}, 5, {{6085, 1575, 55, 0.3}, {6070, 1730, 60, 0.2}});
	tapRgb("w_trim_rgb");
	tapA("w_trim_a");
	if (tap && tap->u8) {
		Img8 cr(c.H, c.W, 1);
		cr.v = c.crease;
		tap->u8("w_trim_crease", cr);
	}
	// texture: fine grain, pitting and blotchy stains over the plating
	{
		ImgF a1 = c.noise(1.2, 0.10);
		ImgF a2 = c.noise(3, 0.08);
		for (size_t i = 0; i < m; i++)
			for (int k = 0; k < 3; k++)
				c.rgbF.v[i * 3 + k] *= (1.0f + a1.v[i]) + a2.v[i];
		ImgF a3 = c.noise(12, 1.0);
		for (size_t i = 0; i < m; i++) {
			const float f = np::clip(1.0f - 0.35f * np::clip(a3.v[i] - 0.2f, 0.0f, 1.0f), 0.0f, 1.0f);
			for (int k = 0; k < 3; k++)
				c.rgbF.v[i * 3 + k] = c.rgbF.v[i * 3 + k] * f;
		}
	}
	tapRgb("w_tex_rgb");
	c.ibeamH(X0, X1, 1499, 1517, {64, 70, 66});
	tapRgb("w_beam_rgb");
	tapA("w_beam_a");
	// the wall's wear and light first, so the pipe keeps its own
	{
		Canvas::Mask cov(m);
		for (size_t i = 0; i < m; i++)
			cov[i] = c.a.v[i] > 0.0f;
		rustAndGrime(c, cov, 0.3, 0.15, 0.12);
	}
	tapRgb("w_rust1_rgb");
	// a dirty lime-green wash over the plating, blotchy and uneven
	{
		const float lime[3] = {112, 150, 34};
		const float lm = np::meanFlat(lime, 3);
		float limeN[3];
		for (int k = 0; k < 3; k++)
			limeN[k] = lime[k] / lm;
		ImgF a1 = c.noise(16, 1.0);
		ImgF a2 = c.noise(5, 1.0);
		for (size_t i = 0; i < m; i++) {
			const float amt = np::clip((0.5f + 0.35f * a1.v[i]) + 0.15f * a2.v[i], 0.1f, 0.85f);
			double* p = &c.rgb.v[i * 3];
			const double lum = mean3(p);
			for (int k = 0; k < 3; k++)
				p[k] = p[k] * double(1.0f - amt) + (double(limeN[k]) * lum) * double(amt);
		}
	}
	tapRgb("w_lime_rgb");
	// the creases (trim strips and their shadow lines) rusty brown again, over the wash
	{
		const float rust[3] = {150, 74, 34};
		const float rm = np::meanFlat(rust, 3);
		float rustN[3];
		for (int k = 0; k < 3; k++)
			rustN[k] = rust[k] / rm;
		ImgF a1 = c.noise(3, 1.0);
		for (size_t i = 0; i < m; i++) {
			if (!c.crease[i])
				continue;
			double* p = &c.rgb.v[i * 3];
			const double cl = mean3(p);
			const double f = double(1.0f + 0.2f * a1.v[i]);
			for (int k = 0; k < 3; k++)
				p[k] = p[k] * 0.35 + ((double(rustN[k]) * cl) * 0.65) * f;
		}
	}
	tapRgb("w_crease_rgb");
	c.grime(0.3);
	tapRgb("w_grime_rgb");
	lightPools(c, {{6060, 1730, 40, 0.2, {255, 150, 80}}, {6110, 1760, 40, 0.2, {255, 90, 40}}, {6080, 1560, 40, 0.15, {255, 140, 80}}}, 0.55);
	tapRgb("w_pools_rgb");
	// the room's big rust pipe carried on west into the rock, a collar like
	// its own, and its shadow on the wall below
	for (size_t i = 0; i < m; i++) {
		if (!(std::fabs(c.Y.v[i] - 1670.0f) < 6.0f && c.X.v[i] < float(X1 + 1)))
			continue;
		const float f = 0.45f + 0.55f * np::clip(std::fabs(c.Y.v[i] - 1670.0f) / 6.0f, 0.0f, 1.0f);
		for (int k = 0; k < 3; k++)
			c.rgb.v[i * 3 + k] *= double(f);
	}
	c.cylH(X0, X1 + 1, 1646, 19, {110, 62, 40}, 0.3, 0.1);
	tapRgb("w_pipe_rgb");
	// a double ring, as the room's flange
	c.flangeH(6063, 1646, 21, 5, {70, 38, 24}, 3);
	c.flangeH(6073, 1646, 21, 5, {70, 38, 24}, 3);
	tapRgb("w_flange_rgb");
	{
		Canvas::Mask pm(m);
		for (size_t i = 0; i < m; i++)
			pm[i] = std::fabs(c.Y.v[i] - 1646.0f) <= 22.0f && c.X.v[i] < float(X1 + 1);
		rustAndGrime(c, pm, 0.12, 0.0, 0.08);
	}
	tapRgb("w_rust2_rgb");
	tapA("w_a");
	auto r = c.result();
	return {std::move(r.first), std::move(r.second)};
}

// ===========================================================================
// lab_west_end

void labWestEnd(ImgF& base, const ImgF& bg, const Tap* tap) {
	if (base.h != kLabSceneH || base.w != kLabSceneW || base.c != 3 || bg.h != kLabSceneH || bg.w != kLabSceneW || bg.c != 3)
		throw std::invalid_argument("tube_walk_lab: lab_west_end wants the scene's base and bg (840 x 2340 x 3)");
	auto tapF = [&](const char* name, const ImgF& v) {
		if (tap && tap->f32)
			tap->f32(name, v);
	};
	auto tapD = [&](const char* name, const ImgD& v) {
		if (tap && tap->f64)
			tap->f64(name, v);
	};
	auto tapM = [&](const char* name, const std::vector<uint8_t>& v, int hh, int ww) {
		if (tap && tap->u8) {
			Img8 o(hh, ww, 1);
			for (size_t i = 0; i < v.size(); i++)
				o.v[i] = v[i] ? 1 : 0;
			tap->u8(name, o);
		}
	};
	auto tapBase = [&](const char* name) {
		if (tap && tap->f32)
			tap->f32(name, crop(base, 227, 564, 1590, 1772));
	};
	// world-coordinate views
	auto B = [&](int y, int x) { return base.px(y - OY, x - OX); };
	auto G = [&](int y, int x) { return bg.px(y - OY, x - OX); };

	LabWall wall = labWall(nullptr);
	ImgD lw = std::move(wall.rgb);
	const ImgF la0 = std::move(wall.a);
	tapD("lab_wall_rgb", lw);
	tapF("lab_wall_a", la0);
	const int WX = kWallX0;
	const int LH = lw.h, LW = lw.w; // 336, 182

	// a fluid join with the room: the strip's colour graded toward the
	// room's own first columns over its last 60 px
	std::vector<double> ratio(size_t(LH) * 3);
	for (int r = 0; r < LH; r++) {
		std::vector<float> rl = np::meanAxis0(G(1478 + r, 6127), 30, 3);
		std::vector<double> rn = np::meanAxis0(lw.px(r, LW - 30), 30, 3);
		for (int k = 0; k < 3; k++)
			ratio[size_t(r) * 3 + k] = np::clip(double(rl[k] + 8.0f) / (rn[k] + 8.0), 0.6, 1.8);
	}
	for (int r = 1626 - 1478; r < 1673 - 1478; r++)
		for (int k = 0; k < 3; k++)
			ratio[size_t(r) * 3 + k] = 1.0;
	{
		std::vector<double> mu = np::meanAxis0(&ratio[size_t(1527 - 1478) * 3], 1627 - 1527, 3);
		for (int r = 1660 - 1478; r < LH; r++)
			for (int k = 0; k < 3; k++)
				ratio[size_t(r) * 3 + k] = mu[size_t(k)];
	}
	ImgF ratioF(LH, 3, 1);
	for (size_t i = 0; i < ratio.size(); i++)
		ratioF.v[i] = float(np::u8(np::clip(ratio[i] * 100, 0.0, 255.0))) / 100.0f;
	tapF("e_ratio", ratioF);
	for (int i = 0; i < LW; i++) {
		const float g = float(np::clip(double(i - (LW - 60)) / 60, 0.0, 1.0));
		for (int r = 0; r < LH; r++)
			for (int k = 0; k < 3; k++)
				lw(r, i, k) = lw(r, i, k) * double(1.0f + (ratioF.v[size_t(r) * 3 + k] - 1.0f) * g);
	}
	tapD("e_join_lw", lw);

	// bold like the room: the strip's brightness remapped to the room's wall,
	// each pixel keeping its colour
	{
		std::vector<float> room;
		for (int y = 1520; y < 1640; y++)
			for (int x = 6113; x < 6150; x++)
				room.push_back(mean3(G(y, x)));
		const ImgD pre = lw;
		ImgD lum(LH, LW, 1);
		std::vector<double> src;
		for (int r = 0; r < LH; r++)
			for (int x = 0; x < LW; x++) {
				lum(r, x) = mean3(lw.px(r, x));
				if (la0(r, x) > 0.5f)
					src.push_back(lum(r, x));
			}
		src = np::sorted(std::move(src));
		std::vector<float> dstF = np::sorted(std::move(room));
		std::vector<double> dst(dstF.begin(), dstF.end());
		const std::vector<double> xp = np::linspace(0, 1, int(dst.size()));
		const double div = double(std::max<int64_t>(1, int64_t(src.size()) - 1));
		ImgD nw(LH, LW, 1);
		for (size_t i = 0; i < lum.v.size(); i++) {
			const double q = double(np::searchsorted(src, lum.v[i], false)) / div;
			nw.v[i] = np::interp(np::clip(q, 0.0, 1.0), xp, dst);
		}
		tapD("e_hist_new", nw);
		for (size_t i = 0; i < lum.v.size(); i++) {
			const double f = (nw.v[i] + 1) / (lum.v[i] + 1);
			for (int k = 0; k < 3; k++)
				lw.v[i * 3 + k] = lw.v[i * 3 + k] * f;
		}
		for (size_t i = 0; i < lum.v.size(); i++) {
			double* p = &lw.v[i * 3];
			const double g = mean3(p);
			for (int k = 0; k < 3; k++)
				p[k] = np::clip(g + (p[k] - g) * 0.55, 0.0, 255.0);
		}
		for (auto& v : lw.v)
			v *= 0.8; // muted: a little darker than the room's lit wall
		// the pipe keeps its own round shading
		for (int r = 1624 - 1478; r < 1672 - 1478; r++)
			for (int x = 0; x < LW; x++)
				for (int k = 0; k < 3; k++)
					lw(r, x, k) = pre(r, x, k) * 0.92;
	}
	tapD("e_hist_lw", lw);

	// the upper wall graded into the room's warm lit wall beside it, per row,
	// in a long even ramp from x 6060 to x 6136
	for (int y = 1519; y < 1624; y++) {
		const int r = y - 1478;
		std::vector<float> tg = np::meanAxis0(G(y, 6127), 6150 - 6127, 3);
		std::vector<double> ow = np::meanAxis0(lw.px(r, 6080 - WX), 6100 - 6080, 3);
		double rt[3];
		for (int k = 0; k < 3; k++)
			rt[k] = np::clip(double(tg[size_t(k)] + 6.0f) / (ow[size_t(k)] + 6.0), 0.6, 2.2);
		for (int x = 6045; x < WX + LW; x++) {
			const double u = np::clip((x - 6060) / 76.0, 0.0, 1.0);
			for (int k = 0; k < 3; k++)
				lw(r, x - WX, k) *= 1 + (rt[k] - 1) * u;
		}
	}
	tapD("e_upper_lw", lw);

	// fade out across the room's first columns, the upper wall from x 6092;
	// below the pipe the panel wall cut cleanly at the room's next object
	ImgD fade(LH, LW, 1);
	for (int x = 0; x < LW; x++) {
		const int xs = WX + x;
		const double t = np::clip((xs - 6099) / 28.0, 0.0, 1.0);
		const double t2 = np::clip((xs - 6092) / 50.0, 0.0, 1.0);
		const double f = 1 - t * t * (3 - 2 * t), f2 = 1 - t2 * t2 * (3 - 2 * t2);
		for (int r = 0; r < LH; r++)
			fade(r, x) = (r >= 1519 - 1478 && r < 1624 - 1478) ? f2 : f;
	}
	for (int y = 1660; y < 1814; y++) {
		int e = y < 1703 ? 6170 : 6146;
		for (int x = 6105; x < 6170; x++)
			if (G(y, x)[1] > 20.0f) {
				e = x;
				break;
			}
		for (int x = 0; x < LW; x++)
			fade(y - 1478, x) = np::clip((e + 1 - (WX + x)) / 2.0, 0.0, 1.0);
	}
	tapD("e_fade", fade);
	ImgD la(LH, LW, 1);
	for (size_t i = 0; i < la.v.size(); i++)
		la.v[i] = double(la0.v[i]) * fade.v[i];
	// the room's blue cable, which hangs down through here, stays in front
	{
		Img8 blue(LH, LW, 1);
		for (int r = 0; r < LH; r++)
			for (int x = 0; x < LW; x++) {
				const float* p = G(1478 + r, WX + x);
				const bool b = p[2] > p[0] + 20.0f && p[2] > 60.0f;
				blue(r, x) = (b && x >= 6100 - WX) ? 255 : 0;
			}
		blue = np::maxFilter(blue, 3);
		std::vector<uint8_t> bm(blue.v.size());
		for (size_t i = 0; i < bm.size(); i++)
			bm[i] = blue.v[i] > 0;
		tapM("e_blue", bm, LH, LW);
		for (size_t i = 0; i < la.v.size(); i++)
			la.v[i] = la.v[i] * (bm[i] ? 0.0 : 1.0);
	}
	tapD("e_la", la);
	for (int r = 0; r < LH; r++)
		for (int x = 0; x < LW; x++) {
			float* p = B(1478 + r, WX + x);
			const double l = la(r, x);
			for (int k = 0; k < 3; k++)
				p[k] = float(double(p[k]) * (1 - l) + lw(r, x, k) * l);
		}
	tapBase("e_wall_base");

	// the wall fades into the room's rusty brown (its light and shade kept)
	// in a long, even ramp over x 6012-6128; not the pipe
	{
		std::vector<float> rr;
		size_t nrr = 0;
		for (int y = 1664; y < 1700; y++)
			for (int x = 6110; x < 6165; x++) {
				const float* p = G(y, x);
				if (p[0] > 2.0f * p[1] + 8.0f && p[2] < 30.0f) {
					rr.insert(rr.end(), p, p + 3);
					nrr++;
				}
			}
		std::vector<float> rust = np::meanAxis0(rr.data(), nrr, 3);
		if (tap && tap->f32) {
			ImgF t(1, 3, 1);
			t.v = rust;
			tap->f32("e_rust", t);
		}
		const int ys0 = 1519, ys1 = 1800, bx0 = 6005, bw = WX + LW - bx0, bh = ys1 - ys0;
		ImgD al(bh, bw, 1);
		for (int i = 0; i < bh; i++)
			for (int j = 0; j < bw; j++)
				al(i, j) = (i >= 1624 - ys0 && i < 1668 - ys0) ? 0.0 : la(ys0 - 1478 + i, bx0 - WX + j);
		ImgF lm(bh, bw, 1);
		std::vector<float> sel;
		for (int i = 0; i < bh; i++)
			for (int j = 0; j < bw; j++) {
				lm(i, j) = mean3(B(ys0 + i, bx0 + j));
				if (al(i, j) > 0.5)
					sel.push_back(lm(i, j));
			}
		// lm[al > 0.5].mean() + 1 is a float64 scalar, rounded to float32 against lm
		const float den = float(double(np::meanFlat(sel.data(), sel.size())) + 1);
		for (int i = 0; i < bh; i++)
			for (int j = 0; j < bw; j++) {
				const float rf = np::clip(0.55f + (0.45f * lm(i, j)) / den, 0.4f, 1.6f);
				const double u = np::clip((bx0 + j - 6012) / 116.0, 0.0, 1.0) * al(i, j);
				float* p = B(ys0 + i, bx0 + j);
				for (int k = 0; k < 3; k++)
					p[k] = float(double(p[k]) * (1 - u) + double(rust[size_t(k)] * rf) * u);
			}
	}
	tapBase("e_rustramp_base");

	// the pipe's surface taken from the room's own pipe, column by column
	np::Generator rng(12);
	{
		const ImgF src = crop(bg, 1626 - OY, 1668 - OY, 6112 - OX, 6122 - OX);
		const ImgF rings = crop(base, 1620 - OY, 1674 - OY, 6058 - OX, 6079 - OX); // (our two rings, kept)
		double walk = 0.0;
		for (int x = 5990; x < 6122; x++) {
			walk = 0.85 * walk + rng.normal(0, 0.6);
			const int k = std::abs(x * 3 + int(walk * 4)) % src.w;
			const float f = float(1 + 0.06 * np::tanh(walk));
			for (int y = 0; y < src.h; y++)
				for (int j = 0; j < 3; j++)
					B(1626 + y, x)[j] = src(y, k, j) * f;
		}
		tapBase("e_pipecols_base");
		// smoothed along the pipe's length
		const int SH = 1668 - 1626, SW = 6122 - 5990;
		std::vector<double> kern(17);
		for (int i = 0; i < 17; i++) {
			const double v = double(i - 8) / 1.6;
			kern[size_t(i)] = np::ex(-0.5 * (v * v));
		}
		const double ks = np::sum(kern.data(), kern.size());
		for (auto& v : kern)
			v /= ks;
		ImgF pad(SH, SW + 16, 3);
		for (int y = 0; y < SH; y++)
			for (int x = 0; x < SW + 16; x++) {
				const int sx = std::min(std::max(x - 8, 0), SW - 1);
				for (int j = 0; j < 3; j++)
					pad(y, x, j) = B(1626 + y, 5990 + sx)[j];
			}
		ImgF sm(SH, SW, 3);
		for (int i = 0; i < 17; i++) {
			const float kv = float(kern[size_t(i)]);
			for (int y = 0; y < SH; y++)
				for (int x = 0; x < SW; x++)
					for (int j = 0; j < 3; j++)
						sm(y, x, j) += kv * pad(y, x + i, j);
		}
		for (int y = 0; y < SH; y++)
			for (int x = 0; x < SW; x++)
				for (int j = 0; j < 3; j++)
					B(1626 + y, 5990 + x)[j] = sm(y, x, j);
		tapBase("e_pipesmooth_base");
		// the room pipe's fine grain, rust mottling in patches, a few darker spots
		std::vector<double> g0 = rng.normal(0, 1, size_t(SH) * SW);
		Img8 g8(SH, SW, 1);
		for (size_t i = 0; i < g0.size(); i++)
			g8.v[i] = np::u8(np::clip(float(g0[i]) * 40.0f + 128.0f, 0.0f, 255.0f));
		g8 = np::gaussianBlur(g8, 0.6);
		ImgF gr(SH, SW, 1);
		for (size_t i = 0; i < gr.v.size(); i++)
			gr.v[i] = float(g8.v[i]) / 128.0f - 1.0f;
		const int mw = std::max(4, SW / 8);
		std::vector<double> m0 = rng.random(size_t(6) * mw);
		Img8 m8(6, mw, 1);
		for (size_t i = 0; i < m0.size(); i++)
			m8.v[i] = np::u8(float(m0[i]) * 255.0f);
		m8 = np::resize(m8, SW, SH, np::Resample::Bicubic);
		ImgF mo(SH, SW, 1);
		for (size_t i = 0; i < mo.v.size(); i++)
			mo.v[i] = float(m8.v[i]) / 255.0f - 0.5f;
		std::vector<double> s0 = rng.random(size_t(SH) * SW);
		std::vector<uint8_t> sp(s0.size());
		for (size_t i = 0; i < sp.size(); i++)
			sp[i] = s0[i] > 0.985;
		tapF("e_gr", gr);
		tapF("e_mo", mo);
		tapM("e_sp", sp, SH, SW);
		for (int y = 0; y < SH; y++)
			for (int x = 0; x < SW; x++) {
				const size_t i = size_t(y) * SW + x;
				const float f = (1.0f + 0.28f * gr.v[i]) + 0.35f * mo.v[i];
				float* p = B(1626 + y, 5990 + x);
				for (int j = 0; j < 3; j++) {
					p[j] *= f;
					if (sp[i])
						p[j] = p[j] * 0.55f;
				}
			}
		tapBase("e_pipegrain_base");
		// the two rings back in front of the textured pipe
		for (int rc : {6063, 6073}) {
			const int a = rc - 2 - 6058;
			const int yb = rc == 6073 ? 1666 : 1660;
			const int rh = yb - 1627; // 33, or 39 with the collar's foot carried down
			const float add[3] = {22, 9, 3};
			std::vector<double> nz = rng.normal(0, 1, size_t(rh) * 5);
			for (int y = 0; y < rh; y++)
				for (int x = 0; x < 5; x++) {
					const double f = 1 + 0.12 * nz[size_t(y) * 5 + x];
					float* d = B(1627 + y, rc - 2 + x);
					for (int j = 0; j < 3; j++) {
						double v;
						if (yb == 1666) {
							// ring concatenated with its rows -7..-2 * linspace: float64
							const std::vector<double> lin = np::linspace(0.95, 0.7, 6);
							const double rv = y < 33 ? double(rings(7 + y, a + x, j)) : double(rings(7 + 26 + (y - 33), a + x, j)) * lin[size_t(y - 33)];
							v = np::clip(rv * 0.6 + double(add[j]), 0.0, 255.0) * f;
						} else {
							v = double(np::clip(rings(7 + y, a + x, j) * 0.6f + add[j], 0.0f, 255.0f)) * f;
						}
						d[j] = float(v);
					}
				}
			for (int y = 0; y < rh; y++) {
				float* d = B(1627 + y, rc - 2);
				for (int j = 0; j < 3; j++)
					d[j] = d[j] * 1.35f + 10.0f; // the lit rim
			}
			for (int y = 1628; y < yb; y++)
				for (int j = 0; j < 3; j++) {
					B(y, rc - 3)[j] *= 0.5f; // dark gap before it
					B(y, rc + 3)[j] *= 0.55f; // and after it
				}
		}
		tapBase("e_rings_base");
		// into the room's pipe gradually over the last 20 px before its flange
		for (int x = 6101; x < 6122; x++) {
			double w = (x - 6101) / 20.0;
			w = w * w * (3 - 2 * w);
			const float w1 = float(1 - w), w2 = float(w);
			for (int y = 1626; y < 1668; y++)
				for (int j = 0; j < 3; j++)
					B(y, x)[j] = B(y, x)[j] * w1 + G(y, x)[j] * w2;
		}
		tapBase("e_pipe_base");
	}

	// the beam across the top of the new wall: no rivets, the darker green of
	// the room's beam beside it (row by row), a smooth hand-over into it
	{
		const int by0 = 1477, BH = 1521 - 1477, bx0 = 5990, BW = 6128 - 5990;
		auto BM = [&](int r, int x) { return B(by0 + r, bx0 + x); };
		const ImgF bmpre = crop(base, by0 - OY, by0 - OY + BH, bx0 - OX, bx0 - OX + BW);
		ImgF med(BH, BW, 3);
		for (int j = 0; j < 3; j++) {
			Img8 ch(BH, BW, 1);
			for (int r = 0; r < BH; r++)
				for (int x = 0; x < BW; x++)
					ch(r, x) = np::u8(np::clip(BM(r, x)[j], 0.0f, 255.0f));
			ch = np::medianFilter(ch, 7);
			for (size_t i = 0; i < ch.v.size(); i++)
				med.v[i * 3 + j] = float(ch.v[i]);
		}
		Img8 hot(BH, BW, 1);
		for (int r = 0; r < BH; r++)
			for (int x = 0; x < BW; x++)
				hot(r, x) = (mean3(BM(r, x)) - mean3(med.px(r, x))) > 6.0f ? 255 : 0;
		hot = np::maxFilter(hot, 3);
		std::vector<uint8_t> hm(hot.v.size());
		for (size_t i = 0; i < hm.size(); i++)
			hm[i] = hot.v[i] > 0;
		tapM("e_hot", hm, BH, BW);
		for (int r = 0; r < BH; r++)
			for (int x = 0; x < BW; x++)
				if (hm[size_t(r) * BW + x])
					std::copy(med.px(r, x), med.px(r, x) + 3, BM(r, x));
		for (int r = 0; r < BH; r++) {
			std::vector<float> tgt = np::meanAxis0(G(by0 + r, 6130), 6185 - 6130, 3);
			std::vector<float> cur = np::meanAxis0(BM(r, 6030 - 5990), 6090 - 6030, 3);
			float f[3];
			for (int j = 0; j < 3; j++)
				f[j] = np::clip((tgt[size_t(j)] + 4.0f) / (cur[size_t(j)] + 4.0f), 0.4f, 1.6f);
			for (int x = 0; x < BW; x++)
				for (int j = 0; j < 3; j++)
					BM(r, x)[j] *= f[j];
		}
		{
			np::Generator g21(21);
			std::vector<double> nz = g21.normal(0, 1, size_t(BH) * BW);
			for (int r = 0; r < BH; r++)
				for (int x = 0; x < BW; x++) {
					const double f = 1 + 0.08 * nz[size_t(r) * BW + x];
					for (int j = 0; j < 3; j++)
						BM(r, x)[j] = float(double(BM(r, x)[j]) * f);
				}
		}
		tapBase("e_beamtone_base");
		for (int r = 1517 - by0; r < BH; r++)
			for (int x = 0; x < BW; x++)
				std::copy(bmpre.px(r, x), bmpre.px(r, x) + 3, BM(r, x));
		// the beam itself, rows 1497-1517: the room's own beam's median colour
		// per row, a fine grain and a slow variation
		np::Generator rng2(31);
		rng2.normal(0, 0.8, size_t(BW)); // (draws the mock-up spent on a column pick it no longer uses)
		const int PR = 1517 - 1497;
		ImgF prof(PR, 3, 1);
		for (int r = 0; r < PR; r++)
			for (int j = 0; j < 3; j++) {
				std::vector<float> lane;
				for (int x = 6122; x < 6196; x++)
					lane.push_back(G(1497 + r, x)[j]);
				prof(r, j) = np::median(std::move(lane), false);
			}
		tapF("e_prof", prof);
		std::vector<double> s0 = rng2.normal(128, 30, 24);
		Img8 s8(2, 12, 1);
		for (size_t i = 0; i < s0.size(); i++)
			s8.v[i] = np::u8(np::clip(s0[i], 0.0, 255.0));
		s8 = np::resize(s8, BW, PR, np::Resample::Bicubic);
		std::vector<double> nb = rng2.normal(0, 1, size_t(PR) * BW);
		for (int r = 0; r < PR; r++)
			for (int x = 0; x < BW; x++) {
				const size_t i = size_t(r) * BW + x;
				const float slow = float(s8.v[i]) / 128.0f - 1.0f;
				const double f = (1 + 0.10 * nb[i]) + double(0.12f * slow);
				for (int j = 0; j < 3; j++)
					BM(1497 - by0 + r, x)[j] = float(double(prof(r, j)) * f);
			}
		// the room's beam takes over over x 6116-6128; the rows above it over x 6100-6128
		for (int x = 6100; x < 6128; x++) {
			double w = np::clip((x - 6116) / 12.0, 0.0, 1.0);
			w = w * w * (3 - 2 * w);
			double wu = (x - 6100) / 28.0;
			wu = wu * wu * (3 - 2 * wu);
			const float a1 = float(1 - wu), a2 = float(wu), b1 = float(1 - w), b2 = float(w);
			for (int y = by0; y < 1497; y++)
				for (int j = 0; j < 3; j++)
					B(y, x)[j] = B(y, x)[j] * a1 + G(y, x)[j] * a2;
			for (int y = 1497; y < 1517; y++)
				for (int j = 0; j < 3; j++)
					B(y, x)[j] = B(y, x)[j] * b1 + G(y, x)[j] * b2;
		}
		tapBase("e_beam_base");
	}

	// the room's stone floor slab (y 1784-1797) carried west to the rock face
	for (int x = 5990; x < 6136; x++) {
		const double w = np::clip((6136 - x) / 10.0, 0.0, 1.0);
		const float w1 = float(1 - w), w2 = float(w);
		const int sx = 6160 + (x - 5990) % (6280 - 6160);
		for (int y = 1784; y < 1797; y++)
			for (int j = 0; j < 3; j++)
				B(y, x)[j] = B(y, x)[j] * w1 + G(y, sx)[j] * w2;
	}
}

// ===========================================================================
// the developer check (OPENC2E_TW_CHECK_LAB)

namespace {

struct Out {
	std::filesystem::path dir;
	std::ofstream index;
	template <typename T>
	void put(const std::string& name, const char* dtype, const std::vector<T>& v) {
		std::ofstream f(dir / (name + ".bin"), std::ios::binary);
		f.write(reinterpret_cast<const char*>(v.data()), std::streamsize(v.size() * sizeof(T)));
		index << name << " " << dtype << " " << v.size() << "\n";
	}
};

ImgF readF32(const std::filesystem::path& p, int h, int w, int c) {
	ImgF o(h, w, c);
	std::ifstream f(p, std::ios::binary);
	if (!f.read(reinterpret_cast<char*>(o.v.data()), std::streamsize(o.v.size() * sizeof(float))))
		throw std::runtime_error("can't read " + p.string() + " (run art/tube_walk_check_lab.py DIR --inputs C2_DIR first)");
	return o;
}

} // namespace

int labSelfCheck(const std::string& dirName) {
	Out o;
	o.dir = dirName;
	const ImgF base0 = readF32(o.dir / "in_base.bin", kLabSceneH, kLabSceneW, 3);
	const ImgF bg = readF32(o.dir / "in_bg.bin", kLabSceneH, kLabSceneW, 3);
	o.index.open(o.dir / "index.txt");
	Tap tap;
	tap.f32 = [&](const std::string& n, const ImgF& v) { o.put(n, "f4", v.v); };
	tap.f64 = [&](const std::string& n, const ImgD& v) { o.put(n, "f8", v.v); };
	tap.u8 = [&](const std::string& n, const Img8& v) { o.put(n, "u8", v.v); };

	// the canvas's own grid and streams
	{
		Canvas c(kWallX0, kWallY0, kWallX1 - kWallX0, kWallY1 - kWallY0, 8);
		tap.f32("cv_X", c.X);
		tap.f32("cv_Y", c.Y);
		tap.f32("cv_noise_9", c.noise(9, 0.10));
		tap.f32("cv_noise_1.2", c.noise(1.2, 0.1));
		tap.f32("cv_noise_1.5", c.noise(1.5, 1.0));
	}
	// lab_wall, step by step
	{
		LabWall w = labWall(&tap);
		tap.f64("wall_rgb", w.rgb);
		tap.f32("wall_a", w.a);
	}
	// lab_west_end
	{
		ImgF base = base0;
		labWestEnd(base, bg, &tap);
		tap.f32("out_base", base);
	}
	// blur
	{
		const ImgF a = crop(bg, 400, 520, 1500, 1700);
		tap.f32("blur_f32_rgb_5", blur(a, 5));
		ImgF a2 = a;
		for (auto& v : a2.v)
			v = v * 1.3f - 20.0f;
		tap.f32("blur_f32_rgb_2.5", blur(a2, 2.5));
		tap.f32("blur_f32_L_6", blur(np::meanLast(a), 6));
		ImgD a3 = np::toF64(a);
		for (auto& v : a3.v)
			v = v * 1.1;
		tap.f32("blur_f64_rgb_9", blur(a3, 9));
	}
	o.index.close();
	return 0;
}

namespace {
// OPENC2E_TW_CHECK_LAB=DIR: write the check at startup and exit
struct LabCheckAtStartup {
	LabCheckAtStartup() {
		const char* dir = devenv("OPENC2E_TW_CHECK_LAB");
		if (!dir || !*dir)
			return;
		try {
			labSelfCheck(dir);
			std::fprintf(stderr, "tube_walk_lab: check written to %s\n", dir);
			std::exit(0);
		} catch (const std::exception& e) {
			std::fprintf(stderr, "tube_walk_lab: check failed: %s\n", e.what());
			std::exit(1);
		}
	}
} labCheckAtStartup;
} // namespace

} // namespace tubewalk
} // namespace MadeArt
