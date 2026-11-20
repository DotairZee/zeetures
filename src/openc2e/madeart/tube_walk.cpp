// The walkable glass tube's made art, part: the recipe's assembly and its pictures ztwb, ztwf, ztwr, ztwg (art/tube_walk.py: make and the output layers).
//
// make() follows the Python line by line; the comments give numpy's type at
// each step where it decides a result (NumPy 1.x promotion: a float32 array
// with a Python float, or a 0-d float64, stays float32, the scalar rounded to
// float32 first; float32 with a float64 array becomes float64, as does
// anything with np.percentile's result).

#include "tube_walk.h"

#include "tube_walk_lab.h"
#include "tube_walk_paint.h"
#include "tube_walk_parts.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fmt/format.h>
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

constexpr double kPi = 3.14159265358979323846;

// math.radians(x): CPython's x * (pi / 180)
double radians(double x) {
	return x * (kPi / 180.0);
}

void need(bool ok, const char* what) {
	if (!ok)
		throw std::runtime_error(std::string("tube_walk: ") + what);
}

// owner, 2026-11-19 (zoomed-out view): stalactites under the cave arch:
// (x, y, width, length, lean), from the ceiling band's lower edge, and inside the arch
const std::vector<Stalactite> STALACTITES = {
	{5392, 1326, 16, 58, 0.05}, {5455, 1336, 10, 34, -0.03}, {5512, 1302, 18, 70, 0.04},
	{5650, 1318, 12, 40, 0.0}, {5790, 1326, 17, 62, 0.03}, {5838, 1350, 9, 30, 0.0},
	{5655, 1394, 7, 22, 0.0}, {5672, 1384, 10, 40, 0.0}, {5694, 1378, 17, 78, 0.03},
	{5716, 1380, 11, 48, -0.02}, {5740, 1390, 8, 28, 0.0}};

// the cage lift's old top deck: what is taken out of the scene (world x0, y0, x1, y1)
struct WBox {
	int x0, y0, x1, y1;
};
const WBox DECK_RM[] = {
	{5214, 1600, 5320, 1627}, // the old beam past the pulley's housing
	{5262, 1508, 5294, 1612}, // the top stop's caller
	{4895, 1555, 4997, 1626}, // box A and the deck's west end
	{5207, 1550, 5300, 1612}, // box D
	{5275, 1600, 5398, 1626}}; // the deck's east end

// a scene-sized float32 plane or bool mask
ImgF plane(float fill = 0) {
	return ImgF(H, W, 1, fill);
}
Mask mask0() {
	return Mask(H, W, 1, 0);
}

// a copy of a[y0:y1, x0:x1] (all channels)
template <typename T>
np::Img<T> crop(const np::Img<T>& a, int y0, int y1, int x0, int x1) {
	need(y0 >= 0 && x0 >= 0 && y1 <= a.h && x1 <= a.w && y0 <= y1 && x0 <= x1, "crop out of range");
	np::Img<T> o(y1 - y0, x1 - x0, a.c);
	for (int y = y0; y < y1; y++)
		std::memcpy(o.px(y - y0, 0), a.px(y, x0), sizeof(T) * size_t(x1 - x0) * a.c);
	return o;
}

// a[y0:, x0:] = b
template <typename T>
void paste(np::Img<T>& a, const np::Img<T>& b, int y0, int x0) {
	need(y0 >= 0 && x0 >= 0 && y0 + b.h <= a.h && x0 + b.w <= a.w && a.c == b.c, "paste out of range");
	for (int y = 0; y < b.h; y++)
		std::memcpy(a.px(y0 + y, x0), b.px(y, 0), sizeof(T) * size_t(b.w) * b.c);
}

// np.abs(a - b).sum(-1) > 0.5, float32
Mask changedPx(const ImgF& a, const ImgF& b) {
	ImgF d(a.h, a.w, a.c);
	for (size_t i = 0; i < d.v.size(); i++)
		d.v[i] = std::fabs(a.v[i] - b.v[i]);
	ImgF s = np::sumLast(d);
	Mask m(a.h, a.w, 1, 0);
	for (size_t i = 0; i < m.v.size(); i++)
		m.v[i] = s.v[i] > 0.5f;
	return m;
}

// Image.fromarray(m * 255).filter(MaxFilter(size)) > 0
Mask maxMask(const Mask& m, int size) {
	Mask o = np::maxFilter(m, size);
	for (auto& v : o.v)
		v = v > 0;
	return o;
}

// make()'s results: the final scene and the layers' masks
struct Made {
	ImgF o, bg;
	Mask tube, rings, collar, front;
	ImgF rock;
};

bool make(Made& out) {
	GameInputs in;
	if (!readInputs(in))
		return false;
	const ImgF& bg = in.bg;
	Scene sc = makeScene(in);
	Mask mask;
	ImgF strip;
	old_tube(sc.tubecomp, in.ta, mask, strip);
	ImgF MOD, pc;
	new_tube(strip, MOD, pc);

	ImgF base = paint_out(sc.scene, bg, in.bgt, mask, strip);
	labWestEnd(base, bg);

	Mask deck_rm = mask0();
	for (const auto& b : DECK_RM)
		for (int y = b.y0 - OY; y < b.y1 - OY; y++)
			for (int x = b.x0 - OX; x < b.x1 - OX; x++)
				deck_rm(y, x) = 1;
	int ymin = H, ymax = -1, xmin = W, xmax = -1;
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++)
			if (deck_rm(y, x)) {
				ymin = std::min(ymin, y), ymax = std::max(ymax, y);
				xmin = std::min(xmin, x), xmax = std::max(xmax, x);
			}
	const DeckBox deck_box{ymin - 6, ymax + 7, xmin - 6, xmax + 7};
	Mask front = load_front();
	for (size_t i = 0; i < front.v.size(); i++)
		front.v[i] = front.v[i] && !deck_rm.v[i];
	ImgF o = base;
	stalactites(o, bg, STALACTITES, OX, OY);
	ImgF before = o; // what the tube's glass is laid over
	Mask keep = mask0();
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++)
			keep(y, x) = front(y, x) || x + OX >= ENTRY + 30;
	const std::pair<double, double> lt{double(X0), start_top - 12};
	paste_rot_masked(o, pc, ang, lt, keep);
	dock_join(o, base, MOD, X0, start_top, slope, front, OX, OY, 56, 8, -34);

	// where the tube's rings land: the same paste, only the ring columns white
	Mask rings_mask = mask0();
	{
		ImgF pcr = pc;
		const int pw_ = pc.w, ph = pc.h;
		Mask rc8(1, pw_, 1, 0);
		for (int x = 0; x < pw_; x++) {
			bool top = false, bot = false;
			for (int y = 0; y < std::min(4, ph); y++)
				top = top || pc(y, x, 3) > 0;
			for (int y = 152; y < ph; y++)
				bot = bot || pc(y, x, 3) > 0;
			rc8(0, x) = (top && bot) ? 255 : 0;
		}
		Mask rc = maxMask(rc8, 3);
		for (int y = 0; y < ph; y++)
			for (int x = 0; x < pw_; x++)
				for (int k = 0; k < 3; k++)
					pcr(y, x, k) = rc(0, x) ? 255.0f : 0.0f;
		ImgF ringimg(H, W, 3, 0.0f);
		paste_rot_masked(ringimg, pcr, ang, lt, keep);
		ImgF rm = np::meanLast(ringimg);
		for (int y = 0; y < H; y++)
			for (int x = 0; x < W; x++)
				rings_mask(y, x) = x >= X0 + 60 - OX && rm(y, x) > 128.0f;
	}

	// the dock's upright collar's old place, seen through the glass: a smooth fill
	Img8 ocIm(1584 - 1411, 4781 - 4746, 1, 0);
	np::roundedRectangle(ocIm, 0, 0, 4781 - 4746 - 1, 1584 - 1411 - 1, 10, 255);
	{
		const int ys0 = 1405 - OY, ys1 = 1590 - OY, xs0 = 4740 - OX, xs1 = 4788 - OX;
		ImgF F = crop(before, ys0, ys1, xs0, xs1);
		Mask M(F.h, F.w, 1, 0);
		for (int y = 0; y < ocIm.h; y++)
			for (int x = 0; x < ocIm.w; x++)
				M(1411 - 1405 + y, 4746 - 4740 + x) = ocIm(y, x) > 0;
		smooth_fill(F, M);
		paste(before, F, ys0, xs0);
	}
	// and the old deck's place, seen through the glass
	deck_fill(before, deck_rm, deck_box, 61);

	// the tube's inside: the cave behind it, muted, dithered with the glass
	const Mask tp = changedPx(o, before);
	const double th = radians(ang);
	const double ty0 = start_top - 12;
	// v = -(xx + OX - X0) * sin(th) + (yy + OY - ty0) * cos(th), float32
	ImgF v = plane();
	{
		const float sn = float(std::sin(th)), cs = float(std::cos(th)), t0 = float(ty0);
		for (int y = 0; y < H; y++)
			for (int x = 0; x < W; x++) {
				const float a = -((float(x) + float(OX)) - float(X0));
				const float b = (float(y) + float(OY)) - t0;
				v(y, x) = a * sn + b * cs;
			}
	}
	Mask inn = mask0();
	ImgF edge = plane();
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++) {
			const float vv = v(y, x);
			inn(y, x) = tp(y, x) && !rings_mask(y, x) && vv >= 16.0f && vv <= 132.0f && !front(y, x);
			edge(y, x) = np::clip(std::min(vv - 16.0f, 132.0f - vv) / 5.0f, 0.0f, 1.0f);
		}
	// the glass's own light and shade, row by row: the median of MOD's
	// columns that are not rings
	const int mh = MOD.h, mw = MOD.w;
	std::vector<uint8_t> ringc(mw, 0);
	for (int x = 0; x < mw; x++) {
		bool top = false, bot = false;
		for (int y = 0; y < std::min(4, mh); y++)
			top = top || MOD(y, x, 3) > 0;
		for (int y = 152; y < mh; y++)
			bot = bot || MOD(y, x, 3) > 0;
		ringc[x] = top && bot;
	}
	ImgF prof(mh, 1, 3);
	for (int r = 0; r < mh; r++)
		for (int k = 0; k < 3; k++) {
			std::vector<float> lane;
			for (int x = 0; x < mw; x++)
				if (!ringc[x])
					lane.push_back(MOD(r, x, k));
			prof(r, 0, k) = np::median(lane);
		}
	// P = prof[np.clip(v, 0, MOD.shape[0] - 1).astype(int)], float32
	ImgF P(H, W, 3);
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++) {
			const int idx = int(np::clip(v(y, x), 0.0f, float(mh - 1)));
			for (int k = 0; k < 3; k++)
				P(y, x, k) = prof(idx, 0, k);
		}
	std::vector<double> grain;
	{
		np::Generator g(41);
		grain = g.normal(0, 1, size_t(H) * W);
	}
	ImgD cave(H, W, 3);
	{
		const ImgF bl = blur(before, 6);
		const double tint[3] = {0.86, 1.0, 0.95};
		for (size_t i = 0; i < cave.v.size(); i++) {
			const float relief = before.v[i] + (before.v[i] - bl.v[i]) * 0.9f; // float32
			cave.v[i] = double(relief * 0.62f) * tint[i % 3]; // float64 from here
		}
	}
	{
		const ImgD cg = np::meanLast(cave);
		for (size_t i = 0; i < cave.v.size(); i++) {
			const double g = cg.v[i / 3];
			cave.v[i] = g + (cave.v[i] - g) * 0.5;
		}
	}
	double cm = 30.0;
	{
		std::vector<double> sel;
		for (size_t p = 0; p < inn.v.size(); p++)
			if (inn.v[p])
				for (int k = 0; k < 3; k++)
					sel.push_back(cave.v[p * 3 + k]);
		if (!sel.empty())
			cm = np::meanFlat(sel.data(), sel.size());
	}
	for (auto& c : cave.v)
		c = (cm + (c - cm) * 0.7) * 0.88;
	double pct[3];
	for (int k = 0; k < 3; k++) {
		std::vector<float> lane;
		for (int r = 16; r < std::min(132, mh); r++)
			lane.push_back(prof(r, 0, k));
		pct[k] = np::percentile(lane, 35);
	}
	// the 4x4 Bayer pattern, float32
	float bay[4][4];
	{
		const int b[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
		for (int i = 0; i < 4; i++)
			for (int j = 0; j < 4; j++)
				bay[i][j] = float(b[i][j]) / 16.0f + float(1.0 / 32);
	}
	{
		const ImgF Pm = np::meanLast(P);
		for (int y = 0; y < H; y++)
			for (int x = 0; x < W; x++) {
				const size_t p = size_t(y) * W + x;
				const float share = 0.55f + 0.2f * (1.0f - np::clip(Pm.v[p] / 60.0f, 0.0f, 1.0f));
				const bool caveSide = bay[y % 4][x % 4] < share;
				const float gr = 1.0f + 0.04f * float(grain[p]);
				const float w = float(inn.v[p]) * edge.v[p];
				for (int k = 0; k < 3; k++) {
					const size_t i = p * 3 + k;
					const double hl = std::max(double(P.v[i]) - pct[k], 0.0);
					const double glass = double(P.v[i] * 0.85f) + hl * 0.6;
					double nw = caveSide ? cave.v[i] + hl * 0.35 : glass * 0.9 + cave.v[i] * 0.15;
					nw *= double(gr);
					o.v[i] = float(double(o.v[i] * (1.0f - w)) + nw * double(w));
				}
			}
	}
	cave = ImgD();
	P = ImgF();

	// the dock's collar ring at the tube's angle: cut out by its rounded
	// outline, the gap filled, and drawn again turned
	const int CX0 = 4748, CY0 = 1413, CX1 = 4780, CY1 = 1582;
	Img8 cmk(CY1 - CY0, CX1 - CX0, 1, 0);
	np::roundedRectangle(cmk, 0, 0, CX1 - CX0 - 1, CY1 - CY0 - 1, 9, 255);
	const ImgF col = crop(base, CY0 - OY, CY1 - OY, CX0 - OX, CX1 - OX); // (the glass now runs under it)
	{
		const int ys0 = 1405 - OY, ys1 = 1595 - OY, xs0 = 4738 - OX, xs1 = 4790 - OX;
		// tp2: inside the glass's height there (sheared, not turned, at the collar)
		const float st = float(start_top - 12), sl = float(slope);
		ImgF F = crop(o, ys0, ys1, xs0, xs1);
		Mask M0(F.h, F.w, 1, 0);
		for (int y = 0; y < ocIm.h; y++)
			for (int x = 0; x < ocIm.w; x++)
				M0(1411 - 1405 + y, 4746 - 4738 + x) = ocIm(y, x) > 0 ? 255 : 0;
		Mask M = maxMask(M0, 5);
		size_t count = 0;
		for (int y = 0; y < F.h; y++)
			for (int x = 0; x < F.w; x++) {
				const int sy = ys0 + y, sx = xs0 + x;
				const float m = std::min((float(sx) + float(OX)) - float(X0), 0.0f);
				const float vs = (float(sy) + float(OY)) - (st + sl * m);
				const bool tp2 = vs >= 9.0f && vs <= 147.0f;
				M(y, x) = M(y, x) && !tp2;
				count += M(y, x);
			}
		smooth_fill(F, M);
		np::Generator g(52);
		const std::vector<double> z = g.normal(0, 1, count);
		size_t j = 0;
		for (size_t i = 0; i < M.v.size(); i++)
			if (M.v[i]) {
				const double f = 1 + 0.05 * z[j++];
				for (int k = 0; k < 3; k++)
					F.v[i * 3 + k] = float(double(F.v[i * 3 + k]) * f);
			}
		paste(o, F, ys0, xs0);
	}
	// the collar turned about its top: its cap stays, its foot swings west
	Mask collar_mask = mask0();
	{
		Img8 rimIn(col.h, col.w, 4);
		for (int y = 0; y < col.h; y++)
			for (int x = 0; x < col.w; x++) {
				for (int k = 0; k < 3; k++)
					rimIn(y, x, k) = np::u8(np::clip(col(y, x, k), 0.0f, 255.0f));
				const float cma = float(cmk(y, x)) / 255.0f;
				rimIn(y, x, 3) = np::u8(np::clip(cma * 255.0f, 0.0f, 255.0f));
			}
		const ImgF ra = np::toF32(np::rotate(rimIn, -ang, np::Resample::Bicubic, true));
		const int w0 = CX1 - CX0, h0 = CY1 - CY0;
		const double pvx = w0 / 2.0, pvy = 6.0; // the pivot, in the collar's own picture
		const double dx = pvx - w0 / 2.0, dy = pvy - h0 / 2.0;
		const double rx = dx * std::cos(th) - dy * std::sin(th) + ra.w / 2.0;
		const double ry = dx * std::sin(th) + dy * std::cos(th) + ra.h / 2.0;
		const int px = int(std::nearbyint(CX0 + pvx - rx)), py = int(std::nearbyint(CY0 + pvy - ry));
		need(py - OY >= 0 && px - OX >= 0 && py - OY + ra.h <= H && px - OX + ra.w <= W, "the collar falls outside the scene");
		for (int y = 0; y < ra.h; y++)
			for (int x = 0; x < ra.w; x++) {
				float* d = o.px(py - OY + y, px - OX + x);
				const float aa = np::pw(ra(y, x, 3) / 255.0f, 0.8);
				for (int k = 0; k < 3; k++)
					d[k] = d[k] * (1.0f - aa) + ra(y, x, k) * aa;
				collar_mask(py - OY + y, px - OX + x) = ra(y, x, 3) > 128.0f;
			}
	}

	const Mask tube_px = changedPx(o, before); // where the tube's glass was drawn
	before = ImgF();
	// the old deck's place outside the glass, filled from around it
	{
		Mask inside = mask0();
		for (int y = 0; y < H; y++)
			for (int x = 0; x < W; x++)
				inside(y, x) = v(y, x) >= 9.0f && v(y, x) <= 147.0f && x + OX >= X0 - 34;
		deck_fill(o, deck_rm, deck_box, 62, &inside);
	}
	// what was painted or placed over the old tube, back in front
	for (size_t p = 0; p < front.v.size(); p++)
		if (front.v[p])
			for (int k = 0; k < 3; k++)
				o.v[p * 3 + k] = sc.scene.v[p * 3 + k];
	put_scenery(o, in.swmp, sceneryFrom(1), &deck_rm);

	// the rock: its edge wraps the tube, an overhang over the exit into the lab
	const double tt = tube_y(ENTRY); // the tube's top edge at the entry
	const double tb = tt + 136; // its bottom edge
	const double sl = std::tan(radians(ang));
	auto across = [&](double f) { // a point across the tube's mouth, tilted with it
		return std::pair<double, double>(double(ENTRY + 10) + 8 * std::sin(f * 7) - (f - 0.5) * 136 * sl, tt + f * 136);
	};
	std::vector<std::pair<double, double>> outline = {
		{ENTRY - 30, 1470}, {5960, 1400}, {6030, 1392}, {EXIT_X + 62, 1478},
		{EXIT_X + 66, LAB_FLOOR - 142}, {EXIT_X + 42, LAB_FLOOR - 128}, {EXIT_X + 22, LAB_FLOOR - 104},
		{EXIT_X + 10, LAB_FLOOR - 64}, {EXIT_X + 5, LAB_FLOOR + 20},
		{EXIT_X - 20, LAB_FLOOR + 60}, {ENTRY - 34, LAB_FLOOR + 60},
		{ENTRY - 30, tb + 46}, {ENTRY - 56, tb + 18}, {ENTRY - 50, tb + 4}, {ENTRY - 14, tb - 4}};
	for (double f : {0.92, 0.75, 0.55, 0.35, 0.18, 0.06})
		outline.push_back(across(f));
	for (auto p : std::vector<std::pair<double, double>>{{ENTRY - 18, tt + 2}, {ENTRY - 58, tt - 6}, {ENTRY - 52, tt - 24}, {ENTRY - 30, tt - 44}})
		outline.push_back(p);
	// the rock's shadow on the glass just before it goes in
	for (int x = ENTRY - 70; x < ENTRY + 20; x++) {
		const double f = np::clip(double(x - (ENTRY - 70)) / 80, 0.0, 1.0);
		const int y0 = int(tube_y(x) - 6), y1 = int(tube_y(x) + 140);
		const float s = float(1 - 0.55 * f * f);
		for (int y = std::max(0, y0 - OY); y < std::min(H, y1 - OY); y++)
			for (int k = 0; k < 3; k++)
				o(y, x - OX, k) *= s;
	}
	// textured like the earth below the lab
	const ImgF earth_sample = crop(bg, 1802 - OY, 1990 - OY, 6436 - OX, 6660 - OX);
	const RockEarth re = render_rock_earth(blob(outline, 5, 61), earth_sample, 11);
	const int rx = re.x0, ry = re.y0;
	const ImgD& rr = re.rgb;
	const ImgF& ra = re.alpha;
	const int rh = ra.h, rw = ra.w;
	need(rr.h == rh && rr.w == rw, "rock sizes");
	ImgF reg = crop(o, ry - OY, ry - OY + rh, rx - OX, rx - OX + rw);
	// its top meets the cave's pebbly earth and the lab's corner: feathered
	// above y 1525, the earth's own texture carried down into the rock
	Img8 cur(rh, rw, 1);
	for (size_t i = 0; i < cur.v.size(); i++)
		cur.v[i] = np::u8(ra.v[i] * 255.0f);
	ImgF dist(rh, rw, 1, 0.0f);
	for (int it = 0; it < 30; it++) {
		cur = np::minFilter(cur, 3);
		for (size_t i = 0; i < cur.v.size(); i++)
			dist.v[i] += cur.v[i] > 128 ? 1.0f : 0.0f;
	}
	ImgF zone(rh, rw, 1);
	{
		Img8 z8(rh, rw, 1);
		for (int y = 0; y < rh; y++) {
			const double z = np::clip(double(1565 - (y + ry)) / 75.0, 0.0, 1.0) * 1.0;
			const uint8_t u = np::u8(z * 255);
			for (int x = 0; x < rw; x++)
				z8(y, x) = u;
		}
		const Img8 zb = np::gaussianBlur(z8, 4);
		for (size_t i = 0; i < zone.v.size(); i++)
			zone.v[i] = float(zb.v[i]) / 255.0f;
	}
	// (not against the pale arch rock on the left: a feather there reads as fog)
	{
		const ImgF rm = np::meanLast(reg);
		Img8 p8(rh, rw, 1);
		for (size_t i = 0; i < p8.v.size(); i++)
			p8.v[i] = np::u8(np::clip((rm.v[i] - 60.0f) / 30.0f, 0.0f, 1.0f) * 255.0f);
		const Img8 pb = np::gaussianBlur(np::maxFilter(p8, 9), 5);
		for (size_t i = 0; i < zone.v.size(); i++)
			zone.v[i] = zone.v[i] * (1.0f - float(pb.v[i]) / 255.0f);
	}
	ImgF ra2(rh, rw, 1);
	{
		const ImgD rrm = np::meanLast(rr);
		const ImgF em = np::meanLast(reg);
		for (size_t p = 0; p < ra2.v.size(); p++) {
			const float d = dist.v[p];
			ra2.v[p] = ra.v[p] * (1.0f - zone.v[p] * (1.0f - np::clip(d / 20.0f, 0.0f, 1.0f)));
			const float mix = zone.v[p] * 0.85f * (1.0f - np::clip(d / 55.0f, 0.0f, 1.0f));
			const double ratio = np::clip((rrm.v[p] + 10) / double(em.v[p] + 10.0f), 0.6, 1.6);
			const double sq = std::sqrt(ratio);
			for (int k = 0; k < 3; k++) {
				const size_t i = p * 3 + k;
				const float e = reg.v[i];
				const double carry = double(e) * sq; // the earth's texture, in the rock's light
				const double rr2 = rr.v[i] * double(1.0f - mix) + carry * double(mix);
				reg.v[i] = float(double(e * (1.0f - ra2.v[p])) + rr2 * double(ra2.v[p]));
			}
		}
	}
	paste(o, reg, ry - OY, rx - OX);
	ImgF rock = plane();
	paste(rock, ra2, ry - OY, rx - OX);
	// the overhang's shadow on the lab's new wall, just inside the exit
	for (int x = EXIT_X + 6; x < EXIT_X + 50; x++) {
		const double f = 1 - double(x - EXIT_X - 6) / 44;
		const float s = float(1 - 0.45 * f);
		for (int y = int(LAB_FLOOR - 124) - OY; y < LAB_FLOOR - OY; y++)
			for (int k = 0; k < 3; k++)
				o(y, x - OX, k) *= s;
	}
	out.o = std::move(o);
	out.bg = in.bg;
	out.tube = tube_px;
	out.rings = rings_mask;
	out.collar = collar_mask;
	out.front = front;
	out.rock = std::move(rock);
	return true;
}

// layer(img8, m): img8 where m, transparent elsewhere, cropped to m
Rgba layer(const Img8& o8, const Mask& m) {
	int y0 = H, y1 = 0, x0 = W, x1 = 0;
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++)
			if (m(y, x)) {
				y0 = std::min(y0, y), y1 = std::max(y1, y + 1);
				x0 = std::min(x0, x), x1 = std::max(x1, x + 1);
			}
	need(y1 > y0 && x1 > x0, "an empty layer");
	Rgba out(x1 - x0, y1 - y0);
	for (int y = y0; y < y1; y++)
		for (int x = x0; x < x1; x++) {
			uint8_t* p = out.at(x - x0, y - y0);
			const uint8_t* s = o8.px(y, x);
			p[0] = s[0];
			p[1] = s[1];
			p[2] = s[2];
			p[3] = m(y, x) ? 255 : 0;
		}
	return out;
}

} // namespace

Pictures makePictures() {
	Pictures pics;
	std::vector<Rgba> genc = gameSprite("genc");
	if (genc.empty())
		return pics;
	const auto t0 = std::chrono::steady_clock::now();
	Made m;
	if (!make(m))
		return pics;
	const Img8 o8 = np::clipToU8(m.o);
	// behind creatures: where the scene differs from the background, a couple of px wider
	Mask changed = mask0();
	for (size_t p = 0; p < changed.v.size(); p++) {
		bool d = false;
		for (int k = 0; k < 3; k++)
			d = d || o8.v[p * 3 + k] != np::u8(m.bg.v[p * 3 + k]);
		changed.v[p] = d ? 255 : 0;
	}
	changed = maxMask(changed, 5);
	// in front of creatures: the glass on a checkerboard (world x + y even)
	Mask tube_front = mask0();
	Mask rockm = mask0();
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++) {
			const bool checker = (x + OX + y + OY) % 2 == 0;
			tube_front(y, x) = (m.tube(y, x) && checker) || m.rings(y, x) || m.collar(y, x) || m.front(y, x);
			rockm(y, x) = m.rock(y, x) > 0.5f;
		}
	pics.ztwb = layer(o8, changed);
	pics.ztwf = layer(o8, tube_front);
	pics.ztwr = layer(o8, rockm);
	// the gene lab's top beam, its west end taken off (transparent west of world x 6125)
	pics.ztwg = genc[0];
	for (int y = 0; y < pics.ztwg.h; y++)
		for (int x = 0; x < std::min(6125 - 5995, pics.ztwg.w); x++)
			pics.ztwg.at(x, y)[3] = 0;
	pics.ok = true;
	const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	fmt::print("* Made art: the walkable tube's scene (ztwb, ztwf, ztwr, ztwg) in {:.1f} s\n", secs);
	return pics;
}

namespace {

// all four come from one make(): made once, kept
const Pictures& pictures() {
	static const Pictures p = makePictures();
	return p;
}

std::vector<Rgba> one(const Rgba Pictures::*which) {
	const Pictures& p = pictures();
	if (!p.ok)
		return {};
	return {p.*which};
}

static Register ztwb("ztwb", [] { return one(&Pictures::ztwb); });
static Register ztwf("ztwf", [] { return one(&Pictures::ztwf); });
static Register ztwr("ztwr", [] { return one(&Pictures::ztwr); });
static Register ztwg("ztwg", [] { return one(&Pictures::ztwg); });

} // namespace

} // namespace tubewalk
} // namespace MadeArt
