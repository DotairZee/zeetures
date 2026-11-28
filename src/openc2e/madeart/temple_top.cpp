// ztcl, ztpc, ztbc and ztbl: art/temple_top.py. The stone temple's top
// floor (owner, 2026-11-27): the cage lift goes up onto the slab across the
// temple's front, its painted mechanism under the slab is painted out (a
// new pulley of our own hangs above the new stop: art/temple_pulley.py), the
// column at the slab's west end is drawn in front of creatures, and the
// great bell swings. additions/temple-top.cos places them; the Python
// recipe's docstring says what each picture is. Its numbers are kept.

#include "MadeArt.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace MadeArt {

namespace {

const int FLOOR = 396;
const int CL_X0 = 4074, CL_Y0 = 407, CL_X1 = 4148, CL_Y1 = 607;
const int MX0 = 4210, MY0 = 474, MX1 = 4282, MY1 = 562;
const float WHEEL_X = 4257.5f, WHEEL_Y = 505.5f, WHEEL_R = 9.5f;
const int CABLES[2] = {4221, 4263};
const int GRAIN_X0 = 4306;
// the cables from the wheel's middle down (the pulley on its swan-neck
// bracket, art/temple_pulley.py, ztgp, draws the wheel over their tops)
const int PC_Y0 = 196;
const int BX0 = 4548, BY0 = 106, BX1 = 4776, BY1 = 288;
const float PIVOT_X = 4662.0f, PIVOT_Y = 118.0f;
const int BELL_TOP = 113, BELL_BOTTOM = 278, COVER_TOP = 113;
const float ANGLES[7] = {0.0f, 0.9f, 1.8f, 2.6f, -0.9f, -1.8f, -2.6f};

// a picture in floats: colour and coverage
struct Pic {
	int w = 0, h = 0;
	std::vector<std::array<float, 3>> c;
	std::vector<float> a;
	Pic() = default;
	Pic(int w_, int h_)
		: w(w_), h(h_), c(size_t(w_) * h_, {0, 0, 0}), a(size_t(w_) * h_, 0) {}
	std::array<float, 3>& at(int x, int y) { return c[size_t(y) * w + x]; }
	const std::array<float, 3>& at(int x, int y) const { return c[size_t(y) * w + x]; }
	float& al(int x, int y) { return a[size_t(y) * w + x]; }
	float al(int x, int y) const { return a[size_t(y) * w + x]; }
};

using Mask = std::vector<char>; // w * h, row by row

// the background, x0..x1 and y0..y1 exclusive, opaque (empty if unreadable)
Pic rgb(int x0, int y0, int x1, int y1) {
	Rgba im = backgroundRegion(x0, y0, x1, y1);
	if (!im.w)
		return Pic();
	Pic p(im.w, im.h);
	for (int y = 0; y < im.h; y++)
		for (int x = 0; x < im.w; x++) {
			const uint8_t* s = im.at(x, y);
			p.at(x, y) = {float(s[0]), float(s[1]), float(s[2])};
			p.al(x, y) = 1;
		}
	return p;
}

Rgba rgbaOf(const Pic& p) {
	Rgba out(p.w, p.h);
	for (int y = 0; y < p.h; y++)
		for (int x = 0; x < p.w; x++) {
			uint8_t* d = out.at(x, y);
			for (int k = 0; k < 3; k++)
				d[k] = uint8_t(std::clamp(p.at(x, y)[k] + 0.5f, 0.0f, 255.0f));
			d[3] = uint8_t(std::clamp(p.al(x, y) * 255 + 0.5f, 0.0f, 255.0f));
		}
	return out;
}

Mask dilate(const Mask& m, int w, int h, int n = 1) {
	Mask cur = m;
	for (int i = 0; i < n; i++) {
		Mask d = cur;
		for (int y = 0; y < h; y++)
			for (int x = 0; x < w; x++) {
				if (cur[size_t(y) * w + x])
					continue;
				if ((y > 0 && cur[size_t(y - 1) * w + x]) || (y < h - 1 && cur[size_t(y + 1) * w + x]) ||
					(x > 0 && cur[size_t(y) * w + x - 1]) || (x < w - 1 && cur[size_t(y) * w + x + 1]))
					d[size_t(y) * w + x] = 1;
			}
		cur = d;
	}
	return cur;
}

Mask invert(const Mask& m) {
	Mask o(m.size());
	for (size_t i = 0; i < m.size(); i++)
		o[i] = !m[i];
	return o;
}

// fill the hole from its edges inward: each pass, every hole pixel with a
// known neighbour (of eight) takes their mean
void inpaint(Pic& img, const Mask& hole) {
	const int w = img.w, h = img.h;
	Mask known = invert(hole);
	for (int round = 0; round < 400; round++) {
		bool all = true;
		for (char k : known)
			all = all && k;
		if (all)
			break;
		std::vector<std::pair<size_t, std::array<float, 3>>> grow;
		for (int y = 0; y < h; y++)
			for (int x = 0; x < w; x++) {
				if (known[size_t(y) * w + x])
					continue;
				std::array<float, 3> acc{0, 0, 0};
				int n = 0;
				for (int dy = -1; dy <= 1; dy++)
					for (int dx = -1; dx <= 1; dx++) {
						if (!dx && !dy)
							continue;
						int xx = x + dx, yy = y + dy;
						if (xx < 0 || yy < 0 || xx >= w || yy >= h || !known[size_t(yy) * w + xx])
							continue;
						for (int k = 0; k < 3; k++)
							acc[k] += img.at(xx, yy)[k];
						n++;
					}
				if (n)
					grow.push_back({size_t(y) * w + x, {acc[0] / n, acc[1] / n, acc[2] / n}});
			}
		for (auto& [i, c] : grow) {
			img.c[i] = c;
			known[i] = 1;
		}
	}
}

// ---- the column ----

Pic column() {
	Pic a = rgb(CL_X0, CL_Y0, CL_X1, CL_Y1);
	if (!a.w)
		return a;
	const int w = a.w, h = a.h;
	Mask m(size_t(w) * h, 0);
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			int wx = x + CL_X0, wy = y + CL_Y0;
			bool warm = a.at(x, y)[0] > a.at(x, y)[2] + 10;
			bool inside = wx >= 4078 && wx <= 4143 && !(wy > 432 && wy < 576 && (wx < 4082 || wx > 4140));
			m[size_t(y) * w + x] = warm && inside;
		}
	m = dilate(invert(dilate(invert(m), w, h)), w, h); // drop specks
	for (size_t i = 0; i < m.size(); i++)
		a.a[i] = m[i] ? 1.0f : 0.0f;
	return a;
}

// ---- the mechanism and its old place ----

std::vector<std::pair<int, Mask>> components(const Mask& m, int w, int h) {
	Mask seen(m.size(), 0);
	std::vector<std::pair<int, Mask>> out;
	for (int y0 = 0; y0 < h; y0++)
		for (int x0 = 0; x0 < w; x0++) {
			size_t i0 = size_t(y0) * w + x0;
			if (!m[i0] || seen[i0])
				continue;
			std::vector<std::pair<int, int>> stack{{y0, x0}};
			seen[i0] = 1;
			Mask piece(m.size(), 0);
			int n = 0;
			while (!stack.empty()) {
				auto [y, x] = stack.back();
				stack.pop_back();
				piece[size_t(y) * w + x] = 1;
				n++;
				const int nb[4][2] = {{y - 1, x}, {y + 1, x}, {y, x - 1}, {y, x + 1}};
				for (auto& q : nb) {
					int yy = q[0], xx = q[1];
					if (yy >= 0 && yy < h && xx >= 0 && xx < w && m[size_t(yy) * w + xx] && !seen[size_t(yy) * w + xx]) {
						seen[size_t(yy) * w + xx] = 1;
						stack.push_back({yy, xx});
					}
				}
			}
			out.push_back({n, piece});
		}
	return out;
}

void mechanismMask(const Pic& a, Mask& keep, Mask& cable) {
	const int w = a.w, h = a.h;
	Mask m(size_t(w) * h, 0);
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			auto& p = a.at(x, y);
			float lum = 0.3f * p[0] + 0.59f * p[1] + 0.11f * p[2];
			bool metal = p[2] < 0.5f * p[1] || lum < 26;
			float dx = x + MX0 - WHEEL_X, dy = y + MY0 - WHEEL_Y;
			m[size_t(y) * w + x] = metal || dx * dx + dy * dy <= WHEEL_R * WHEEL_R;
		}
	keep.assign(m.size(), 0);
	for (auto& [n, piece] : components(m, w, h))
		if (n >= 25)
			for (size_t i = 0; i < piece.size(); i++)
				keep[i] |= piece[i];
	cable.assign(m.size(), 0);
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++)
			for (int cx : CABLES)
				if (x + MX0 >= cx - 1 && x + MX0 <= cx + 2 && y + MY0 > 516)
					cable[size_t(y) * w + x] = 1;
}

// the two cables, as the painted ones are where they meet them (y 562):
// two black columns with a soft one on their west side, over p from row
// `from` down
const int CABLE_COLS[2] = {4221, 4261};
void cables(Pic& p, int x0, int from) {
	const std::pair<int, float> cols[3] = {{-1, 0.65f}, {0, 1.0f}, {1, 1.0f}};
	for (int cx : CABLE_COLS)
		for (auto [dx, k] : cols) {
			int x = cx + dx - x0;
			if (x < 0 || x >= p.w)
				continue;
			for (int y = std::max(0, from); y < p.h; y++) {
				auto& c = p.at(x, y);
				const float ink[3] = {14, 12, 10};
				for (int i = 0; i < 3; i++)
					c[i] = c[i] * (1 - k) + ink[i] * k;
				p.al(x, y) = std::max(p.al(x, y), k);
			}
		}
}

std::vector<Rgba> oldPlace() {
	Pic a = rgb(MX0, MY0, MX1, MY1);
	Pic g = rgb(GRAIN_X0, MY0, GRAIN_X0 + (MX1 - MX0), MY1);
	if (!a.w || !g.w)
		return {};
	const int w = a.w, h = a.h;
	Mask m, cable;
	mechanismMask(a, m, cable);
	Mask hole(m.size());
	for (size_t i = 0; i < m.size(); i++)
		hole[i] = m[i] || cable[i];
	hole = dilate(hole, w, h, 2);
	Pic filled = a;
	inpaint(filled, hole);
	// stone grain from plain stones further east (np.roll: wrapping)
	auto G = [&](int x, int y) -> const std::array<float, 3>& { return g.at(((x % w) + w) % w, ((y % h) + h) % h); };
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++)
			if (hole[size_t(y) * w + x])
				for (int k = 0; k < 3; k++) {
					float grain = G(x, y)[k] - (G(x, y - 1)[k] + G(x, y + 1)[k] + G(x - 1, y)[k] + G(x + 1, y)[k]) / 4;
					filled.at(x, y)[k] += 0.6f * grain;
				}
	Pic out(w, MY1 - PC_Y0);
	const int off = MY0 - PC_Y0;
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			out.at(x, y + off) = filled.at(x, y);
			out.al(x, y + off) = hole[size_t(y) * w + x] ? 1.0f : 0.0f;
		}
	cables(out, MX0, 0);
	return {rgbaOf(out)};
}

// ---- the bell ----

Mask bellMask(const Pic& a) {
	const int w = a.w, h = a.h;
	Mask m(size_t(w) * h, 0);
	auto sky = [&](int x, int y) { return a.at(x, y)[2] > a.at(x, y)[0] + 12; };
	const int cx = int(PIVOT_X) - BX0;
	for (int y = BELL_TOP - BY0; y <= BELL_BOTTOM - BY0; y++) {
		int wy = y + BY0, x0, x1;
		if (wy < 214) {
			x0 = cx;
			while (x0 > 0 && !sky(x0 - 1, y))
				x0--;
			x1 = cx;
			while (x1 < w - 1 && !sky(x1 + 1, y))
				x1++;
			if (wy < 133)
				x0 = std::max(x0, 4606 - BX0), x1 = std::min(x1, 4720 - BX0);
			else
				x0 = std::max(x0, 4575 - BX0), x1 = std::min(x1, 4749 - BX0);
		} else if (wy < 224)
			x0 = 4576 - BX0, x1 = 4748 - BX0;
		else if (wy < 270)
			x0 = 4579 - BX0, x1 = 4746 - BX0;
		else
			x0 = 4576 - BX0, x1 = 4748 - BX0;
		for (int x = x0; x <= x1; x++)
			m[size_t(y) * w + x] = 1;
	}
	return m;
}

std::vector<Rgba> bellCover() {
	Pic a = rgb(BX0, BY0, BX1, BY1);
	if (!a.w)
		return {};
	const int w = a.w, h = a.h;
	Mask hole = dilate(bellMask(a), w, h);
	for (int y = 0; y < COVER_TOP - BY0; y++)
		for (int x = 0; x < w; x++)
			hole[size_t(y) * w + x] = 0;
	Pic out = a;
	for (int y = 0; y < h; y++) {
		int x0 = -1, x1 = -1;
		for (int x = 0; x < w; x++)
			if (hole[size_t(y) * w + x]) {
				if (x0 < 0)
					x0 = x;
				x1 = x;
			}
		if (x0 < 0)
			continue;
		if (y + BY0 < 214) {
			std::array<float, 3> L{0, 0, 0}, R{0, 0, 0};
			int nl = 0, nr = 0;
			for (int x = std::max(0, x0 - 6); x < x0; x++, nl++)
				for (int k = 0; k < 3; k++)
					L[k] += a.at(x, y)[k];
			for (int x = x1 + 1; x < std::min(w, x1 + 7); x++, nr++)
				for (int k = 0; k < 3; k++)
					R[k] += a.at(x, y)[k];
			for (int k = 0; k < 3; k++)
				L[k] /= std::max(1, nl), R[k] /= std::max(1, nr);
			for (int x = x0; x <= x1; x++) {
				float t = float(x - x0) / std::max(1, x1 - x0);
				for (int k = 0; k < 3; k++)
					out.at(x, y)[k] = L[k] * (1 - t) + R[k] * t;
			}
		} else {
			const int mid = (x0 + x1) / 2;
			for (int x = x0; x <= x1; x++) {
				int sx = x <= mid ? x0 - 1 - ((x - x0) % 18) : x1 + 1 + ((x1 - x) % 18);
				out.at(x, y) = a.at(std::clamp(sx, 0, w - 1), y);
			}
		}
	}
	for (size_t i = 0; i < hole.size(); i++)
		out.a[i] = hole[i] ? 1.0f : 0.0f;
	return {rgbaOf(out)};
}

std::vector<Rgba> bellFrames() {
	Pic a = rgb(BX0, BY0, BX1, BY1);
	if (!a.w)
		return {};
	const int w = a.w, h = a.h;
	Mask bm = bellMask(a);
	std::vector<float> m(bm.size()), mm(bm.size());
	for (size_t i = 0; i < bm.size(); i++)
		m[i] = bm[i] ? 1.0f : 0.0f;
	auto M = [&](int x, int y) { return m[size_t(((y % h) + h) % h) * w + ((x % w) + w) % w]; };
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			float soft = (M(x, y) + M(x, y - 1) + M(x, y + 1) + M(x - 1, y) + M(x + 1, y)) / 5;
			float v = M(x, y);
			mm[size_t(y) * w + x] = v > 0 ? std::max(v * 0.85f, soft) : soft * 0.5f;
		}
	const float px = PIVOT_X - BX0, py = PIVOT_Y - BY0;
	std::vector<Rgba> frames;
	for (float deg : ANGLES) {
		const float t = deg * 3.14159265358979f / 180.0f, ct = std::cos(t), st = std::sin(t);
		Pic f(w, h);
		for (int y = 0; y < h; y++)
			for (int x = 0; x < w; x++) {
				float dx = x - px, dy = y - py;
				float sx = px + dx * ct + dy * st, sy = py - dx * st + dy * ct;
				int x0 = int(std::floor(sx)), y0 = int(std::floor(sy));
				float fx = sx - x0, fy = sy - y0;
				std::array<float, 3> c{0, 0, 0};
				float al = 0;
				const int qx[4] = {x0, x0 + 1, x0, x0 + 1}, qy[4] = {y0, y0, y0 + 1, y0 + 1};
				const float qw[4] = {(1 - fx) * (1 - fy), fx * (1 - fy), (1 - fx) * fy, fx * fy};
				for (int q = 0; q < 4; q++) {
					if (qx[q] < 0 || qy[q] < 0 || qx[q] >= w || qy[q] >= h)
						continue;
					float mv = mm[size_t(qy[q]) * w + qx[q]];
					for (int k = 0; k < 3; k++)
						c[k] += a.at(qx[q], qy[q])[k] * mv * qw[q];
					al += mv * qw[q];
				}
				if (al > 0)
					f.at(x, y) = {c[0] / al, c[1] / al, c[2] / al};
				f.al(x, y) = std::clamp((al - 0.25f) / 0.5f, 0.0f, 1.0f);
			}
		frames.push_back(rgbaOf(f));
	}
	return frames;
}

} // namespace

static Register ztcl("ztcl", [] {
	Pic c = column();
	return c.w ? std::vector<Rgba>{rgbaOf(c)} : std::vector<Rgba>();
});
static Register ztpc("ztpc", oldPlace);
static Register ztbc("ztbc", bellCover);
static Register ztbl("ztbl", bellFrames);

} // namespace MadeArt
