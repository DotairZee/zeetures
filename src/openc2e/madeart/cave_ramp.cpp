// zcrp: art/cave_ramp.py. Planks sloping from the cave lift's dock down into
// the stream (owner, 2026-11-14: "a ramp down into that water where
// creatures can wade", "really look at the dock and model it after that").
// Built the way the dock is: its own deck boards (rounded ends, dark gaps)
// on its weathered grey log, cut from the background (only the boards' and
// the log's pixels), laid end to end along the slope (about 17 degrees) from
// where the dock's last board ends (x 2329). Under the water they take on
// the water's colour, more with depth, the surface line crosses them, and
// where the water meets the bed they stop. tools/additions/water-pocket.cos
// places it at X0, Y0.
//
// The recipe tilts the strip with PIL (4x LANCZOS up, BICUBIC rotate with
// expand, LANCZOS down, all on premultiplied RGBa as PIL does for RGBA);
// those are reimplemented below as Pillow's C code does them (Resample.c,
// Geometry.c, Convert.c, AlphaComposite.c), to the bit.

#include "MadeArt.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace MadeArt {

namespace {

const int X0 = 2300, Y0 = 2290, X1 = 2600, Y1 = 2410;
const double SLOPE = 0.30;            // px down per px east (about 17 degrees)
const int START_X = 2332, START_Y = 2315; // just past the dock's last board (it ends at 2329), a gap as between its boards
const int DECK_Y0 = 2315, DECK_Y1 = 2326; // the dock's boards, top to bottom
const int LOG_Y0 = 2326, LOG_Y1 = 2335;   // its log (the stringer) under them
const int BOARDS[4][2] = {{2114, 2137}, {2189, 2218}, {2297, 2329}, {2085, 2113}}; // clean boards (no rope, cables)
const int LOG_X0 = 2190, LOG_X1 = 2295; // a clean stretch of the log
const int GAP = 3;                      // between boards, as on the dock
const int LENGTH = 260;                 // along the slope (the bed hides the rest)

// float RGBA, as the recipe's float32 arrays
struct FImg {
	int w = 0, h = 0;
	std::vector<float> v;
	FImg() = default;
	FImg(int w_, int h_)
		: w(w_), h(h_), v(size_t(w_) * h_ * 4, 0.0f) {}
	float* at(int x, int y) { return &v[(size_t(y) * w + x) * 4]; }
	const float* at(int x, int y) const { return &v[(size_t(y) * w + x) * 4]; }
};

FImg toFloat(const Rgba& im) {
	FImg f(im.w, im.h);
	for (size_t i = 0; i < im.px.size(); i++)
		f.v[i] = im.px[i];
	return f;
}

// the dock's boards, each with its own alpha (the dark gaps and the rock
// above its rounded corners left out)
std::vector<FImg> boardTiles() {
	std::vector<FImg> tiles;
	for (auto& b : BOARDS) {
		Rgba src = backgroundRegion(b[0], DECK_Y0, b[1], DECK_Y1);
		if (!src.w)
			return {};
		FImg t = toFloat(src);
		for (int y = 0; y < t.h; y++)
			for (int x = 0; x < t.w; x++) {
				float* p = t.at(x, y);
				float r = p[0], g = p[1], bl = p[2];
				float lum = (r + g + bl) / 3.0f;
				bool wood = (r - bl > 14) || (lum < 60); // the board's wood, lit or in shade
				float a = wood ? 255.0f : 0.0f;
				if (lum < 12)
					a = 0.0f; // the gaps' near-black
				// the top row: only the lit wood (above it is rock of the same hue)
				if (y == 0 && !((r - bl > 30) && (lum > 50)))
					a = 0.0f;
				p[3] = a;
			}
		tiles.push_back(std::move(t));
	}
	return tiles;
}

FImg mirrored(const FImg& t) {
	FImg m(t.w, t.h);
	for (int y = 0; y < t.h; y++)
		for (int x = 0; x < t.w; x++)
			std::copy(t.at(t.w - 1 - x, y), t.at(t.w - 1 - x, y) + 4, m.at(x, y));
	return m;
}

// the ramp laid out flat: boards on top, the log under them
FImg strip() {
	std::vector<FImg> boards = boardTiles();
	Rgba logSrc = backgroundRegion(LOG_X0, LOG_Y0, LOG_X1, LOG_Y1); // opaque
	if (boards.empty() || !logSrc.w)
		return FImg();
	FImg log = toFloat(logSrc), logM = mirrored(log);
	const int bh = DECK_Y1 - DECK_Y0, lh = LOG_Y1 - LOG_Y0, H = bh + lh;
	FImg out(LENGTH, H);
	// the log, end to end (each stretch mirrored every other time, so the
	// grain does not visibly repeat)
	for (int x = 0, k = 0; x < LENGTH; k++) {
		const FImg& t = k % 2 == 0 ? log : logM;
		int n = std::min(t.w, LENGTH - x);
		for (int y = 0; y < lh; y++)
			for (int i = 0; i < n; i++)
				std::copy(t.at(i, y), t.at(i, y) + 4, out.at(x + i, bh + y));
		x += n;
	}
	// the boards, with the dock's dark gaps between them
	const int nb = int(boards.size());
	for (int x = 0, k = 0; x < LENGTH; k++) {
		FImg t = boards[k % nb];
		if ((k / nb) % 2)
			t = mirrored(t);
		int n = std::min(t.w, LENGTH - x);
		for (int y = 0; y < bh; y++)
			for (int i = 0; i < n; i++) {
				const float* s = t.at(i, y);
				float* o = out.at(x + i, y);
				double m = s[3] / 255.0;
				for (int c = 0; c < 3; c++)
					o[c] = float(o[c] * (1 - m) + s[c] * m);
				o[3] = std::max(o[3], s[3]);
			}
		x += n;
		int g = std::min(GAP, LENGTH - x);
		// the gap: dark wood in shadow, as on the dock
		for (int y = 2; y < bh; y++)
			for (int i = 0; i < g; i++) {
				float* o = out.at(x + i, y);
				o[0] = o[1] = o[2] = 14.0f;
				o[3] = 255.0f;
			}
		x += GAP;
	}
	return out;
}

// --- PIL, as Pillow 10's C code does it ---

// RGBA to RGBa (Convert.c rgbA2rgba): colour times alpha, rounded
void premultiply(Rgba& im) {
	for (size_t i = 0; i < im.px.size(); i += 4) {
		unsigned a = im.px[i + 3];
		for (int c = 0; c < 3; c++) {
			unsigned t = im.px[i + c] * a + 128;
			im.px[i + c] = uint8_t(((t >> 8) + t) >> 8);
		}
	}
}

// RGBa to RGBA (Convert.c rgba2rgbA): divided back, truncated
void unpremultiply(Rgba& im) {
	for (size_t i = 0; i < im.px.size(); i += 4) {
		unsigned a = im.px[i + 3];
		if (a == 0 || a == 255)
			continue;
		for (int c = 0; c < 3; c++)
			im.px[i + c] = uint8_t(std::min(255u, 255u * im.px[i + c] / a));
	}
}

// Resample.c: Lanczos (support 3) weights for one axis, in fixed point
const int PRECISION_BITS = 32 - 8 - 2;

double sincFilter(double x) {
	if (x == 0.0)
		return 1.0;
	x = x * M_PI;
	return std::sin(x) / x;
}

double lanczosFilter(double x) {
	if (-3.0 <= x && x < 3.0)
		return sincFilter(x) * sincFilter(x / 3);
	return 0.0;
}

struct Coeffs {
	int ksize = 0;
	std::vector<int> bounds; // first input, count, per output
	std::vector<int> kk;
};

Coeffs lanczosCoeffs(int inSize, int outSize) {
	Coeffs c;
	double scale = double(inSize) / outSize;
	double filterscale = scale < 1.0 ? 1.0 : scale;
	double support = 3.0 * filterscale;
	c.ksize = int(std::ceil(support)) * 2 + 1;
	c.bounds.resize(size_t(outSize) * 2);
	c.kk.assign(size_t(outSize) * c.ksize, 0);
	std::vector<double> k(c.ksize);
	for (int xx = 0; xx < outSize; xx++) {
		double center = (xx + 0.5) * scale;
		double ww = 0.0;
		double ss = 1.0 / filterscale;
		int xmin = int(center - support + 0.5);
		if (xmin < 0)
			xmin = 0;
		int xmax = int(center + support + 0.5);
		if (xmax > inSize)
			xmax = inSize;
		xmax -= xmin;
		for (int x = 0; x < xmax; x++) {
			double w = lanczosFilter((x + xmin - center + 0.5) * ss);
			k[x] = w;
			ww += w;
		}
		for (int x = 0; x < xmax; x++) {
			if (ww != 0.0)
				k[x] /= ww;
			double v = k[x] * (1 << PRECISION_BITS);
			c.kk[size_t(xx) * c.ksize + x] = k[x] < 0 ? int(-0.5 + v) : int(0.5 + v);
		}
		c.bounds[xx * 2] = xmin;
		c.bounds[xx * 2 + 1] = xmax;
	}
	return c;
}

uint8_t clip8(int v) {
	v >>= PRECISION_BITS;
	return uint8_t(v < 0 ? 0 : v > 255 ? 255 : v);
}

// Image.resize(size, LANCZOS) of an RGBA picture: premultiplied, the
// horizontal pass then the vertical, each to 8 bits
Rgba resizeLanczos(Rgba im, int w, int h) {
	premultiply(im);
	Coeffs ch = lanczosCoeffs(im.w, w);
	Rgba tmp(w, im.h);
	for (int y = 0; y < im.h; y++)
		for (int xx = 0; xx < w; xx++) {
			int xmin = ch.bounds[xx * 2], n = ch.bounds[xx * 2 + 1];
			const int* k = &ch.kk[size_t(xx) * ch.ksize];
			for (int c = 0; c < 4; c++) {
				int ss = 1 << (PRECISION_BITS - 1);
				for (int x = 0; x < n; x++)
					ss += im.at(x + xmin, y)[c] * k[x];
				tmp.at(xx, y)[c] = clip8(ss);
			}
		}
	Coeffs cv = lanczosCoeffs(im.h, h);
	Rgba out(w, h);
	for (int yy = 0; yy < h; yy++) {
		int ymin = cv.bounds[yy * 2], n = cv.bounds[yy * 2 + 1];
		const int* k = &cv.kk[size_t(yy) * cv.ksize];
		for (int x = 0; x < w; x++)
			for (int c = 0; c < 4; c++) {
				int ss = 1 << (PRECISION_BITS - 1);
				for (int y = 0; y < n; y++)
					ss += tmp.at(x, y + ymin)[c] * k[y];
				out.at(x, yy)[c] = clip8(ss);
			}
	}
	unpremultiply(out);
	return out;
}

// Python's round(v, 15) (correctly rounded, as printf is)
double round15(double v) {
	char buf[64];
	std::snprintf(buf, sizeof buf, "%.15f", v);
	return std::strtod(buf, nullptr);
}

// Geometry.c's cubic (a = -1) through four samples
double cubic(double v1, double v2, double v3, double v4, double d) {
	double p1 = v2;
	double p2 = -v1 + v3;
	double p3 = 2 * (v1 - v2) + v3 - v4;
	double p4 = -v1 + v2 - v3 + v4;
	return p1 + d * (p2 + d * (p3 + d * p4));
}

// Image.rotate(angle, BICUBIC, expand=True) of an RGBA picture: Image.py's
// affine matrix, then Geometry.c's generic transform with its bicubic
// filter, on premultiplied RGBa
Rgba rotateBicubic(Rgba im, double angle) {
	premultiply(im);
	const int w = im.w, h = im.h;
	angle = std::fmod(angle, 360.0);
	if (angle < 0)
		angle += 360.0;
	double cx = w / 2.0, cy = h / 2.0;
	double rad = -(angle * (M_PI / 180.0)); // math.radians
	double m[6] = {round15(std::cos(rad)), round15(std::sin(rad)), 0.0, round15(-std::sin(rad)), round15(std::cos(rad)), 0.0};
	auto tf = [&](double x, double y, double& ox, double& oy) {
		double a = m[0] * x + m[1] * y + m[2];
		double b = m[3] * x + m[4] * y + m[5];
		ox = a;
		oy = b;
	};
	tf(-cx, -cy, m[2], m[5]);
	m[2] += cx;
	m[5] += cy;
	double xs[4], ys[4];
	const double corners[4][2] = {{0, 0}, {double(w), 0}, {double(w), double(h)}, {0, double(h)}};
	for (int i = 0; i < 4; i++)
		tf(corners[i][0], corners[i][1], xs[i], ys[i]);
	int nw = int(std::ceil(*std::max_element(xs, xs + 4)) - std::floor(*std::min_element(xs, xs + 4)));
	int nh = int(std::ceil(*std::max_element(ys, ys + 4)) - std::floor(*std::min_element(ys, ys + 4)));
	tf(-(nw - w) / 2.0, -(nh - h) / 2.0, m[2], m[5]);
	Rgba out(nw, nh);
	auto xclip = [&](int x) { return x < 0 ? 0 : x < w ? x : w - 1; };
	for (int y = 0; y < nh; y++)
		for (int x = 0; x < nw; x++) {
			double xin = x + 0.5, yin = y + 0.5;
			double sx = m[0] * xin + m[1] * yin + m[2];
			double sy = m[3] * xin + m[4] * yin + m[5];
			if (sx < 0.0 || sx >= w || sy < 0.0 || sy >= h)
				continue; // filled with 0
			sx -= 0.5;
			sy -= 0.5;
			int ix = sx < 0.0 ? int(std::floor(sx)) : int(sx);
			int iy = sy < 0.0 ? int(std::floor(sy)) : int(sy);
			double dx = sx - ix, dy = sy - iy;
			ix--;
			iy--;
			int x0 = xclip(ix), x1 = xclip(ix + 1), x2 = xclip(ix + 2), x3 = xclip(ix + 3);
			for (int c = 0; c < 4; c++) {
				auto row = [&](int yy) {
					const uint8_t* r = im.at(0, yy);
					return cubic(r[x0 * 4 + c], r[x1 * 4 + c], r[x2 * 4 + c], r[x3 * 4 + c], dx);
				};
				double v1 = row(iy < 0 ? 0 : iy < h ? iy : h - 1);
				double v2 = iy + 1 >= 0 && iy + 1 < h ? row(iy + 1) : v1;
				double v3 = iy + 2 >= 0 && iy + 2 < h ? row(iy + 2) : v2;
				double v4 = iy + 3 >= 0 && iy + 3 < h ? row(iy + 3) : v3;
				double v = cubic(v1, v2, v3, v4, dy);
				out.at(x, y)[c] = v <= 0.0 ? 0 : v >= 255.0 ? 255 : uint8_t(v);
			}
		}
	unpremultiply(out);
	return out;
}

// --- the recipe ---

std::vector<Rgba> ramp() {
	Rgba bgSrc = backgroundRegion(X0, Y0, X1, Y1);
	FImg flat = strip();
	if (!bgSrc.w || !flat.w)
		return {};
	FImg bg = toFloat(bgSrc);
	const int w = bg.w, h = bg.h;
	// tilt it: rotate about its top-left corner, down to the east
	double ang = std::atan(SLOPE) * (180.0 / M_PI); // math.degrees
	Rgba im(flat.w, flat.h);
	for (size_t i = 0; i < flat.v.size(); i++)
		im.px[i] = uint8_t(std::clamp(flat.v[i], 0.0f, 255.0f));
	Rgba big = resizeLanczos(im, im.w * 4, im.h * 4);
	Rgba rot = rotateBicubic(big, -ang);
	rot = resizeLanczos(rot, std::max(1, rot.w / 4), std::max(1, rot.h / 4));
	// where the strip's top-left corner went in the rotated picture
	double th = ang * (M_PI / 180.0); // math.radians
	int fh = flat.h;
	double ox = fh * std::sin(th); // expand put the corner this far right
	// alpha_composite onto a clear canvas: over nothing, a pixel with any
	// alpha comes through as it is, one with none stays clear
	int dx = int(std::nearbyint(START_X - X0 - ox)), dy = int(std::nearbyint(double(START_Y - Y0)));
	FImg c(w, h);
	for (int y = 0; y < rot.h; y++)
		for (int x = 0; x < rot.w; x++) {
			int tx = x + dx, ty = y + dy;
			const uint8_t* s = rot.at(x, y);
			if (tx < 0 || ty < 0 || tx >= w || ty >= h || s[3] == 0)
				continue;
			std::copy(s, s + 4, c.at(tx, ty));
		}
	// the stream: its water, its surface line, the bed below it
	std::vector<char> water(size_t(w) * h);
	std::vector<float> surface(w, float(Y1));
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			const float* p = bg.at(x, y);
			float r = p[0], g = p[1], b = p[2];
			int yy = y + Y0;
			water[size_t(y) * w + x] = (g >= r) && (b >= r * 0.8f) && (r + g + b > 30) && (b < 140) && (yy < 2396);
		}
	for (int x = 0; x < w; x++)
		for (int y = 0; y < h; y++)
			if (water[size_t(y) * w + x]) {
				surface[x] = float(Y0 + y);
				break;
			}
	Rgba out(w, h);
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			int yy = y + Y0;
			bool above = yy < surface[x];
			bool wet = water[size_t(y) * w + x];
			const float* p = c.at(x, y);
			const float* q = bg.at(x, y);
			// only above the water (where it leaves the dock) or in it: into the bed it vanishes
			float alpha = p[3] * float(above || wet);
			// under the water: the water's own colour, more with depth
			double d = double(yy) - double(surface[x]);
			double depth = std::clamp(d / 40.0, 0.0, 1.0);
			double mix = (0.22 + 0.48 * depth) * (above ? 0.0 : 1.0);
			// the surface line crosses the planks: the water's own top rows over them
			bool line = d >= 0 && d < 2;
			uint8_t* o = out.at(x, y);
			for (int k = 0; k < 3; k++) {
				double v = double(p[k]) * (1 - mix) + double(q[k]) * mix;
				if (line)
					v = v * 0.35 + double(q[k] * 0.65f);
				o[k] = uint8_t(std::clamp(v, 0.0, 255.0));
			}
			o[3] = uint8_t(std::clamp(alpha, 0.0f, 255.0f));
		}
	return {out};
}

} // namespace

static Register zcrp("zcrp", ramp);

} // namespace MadeArt
