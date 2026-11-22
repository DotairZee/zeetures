// zvtk: art/volcano_trunk.py. The rock trunk at the top of the volcano's
// west side, drawn in front of creatures (owner, 2026-11-13: creatures
// walking over the volcano's top showed through it). The walk over the top
// (rooms 142, 233 and 143, floor y 693) passes behind the grey trunk rising
// from the volcano's shoulder toward the palm; the background is one flat
// picture, so creatures were drawn over it. This cuts the trunk out of the
// background along a hand-traced outline, soft at the edges.
// additions/volcano-trunk.cos places it at world X0, Y0.
//
// The recipe's mask is drawn with PIL (ImageDraw polygon at 4x, LANCZOS
// resize, GaussianBlur 0.7); those are reimplemented here as Pillow 10's C
// code does them (Draw.c polygon fill, Resample.c's fixed-point 8-bit
// passes, BoxBlur.c's extended box blur), so the mask is the same.

#include "MadeArt.h"

#include <algorithm>
#include <cmath>

namespace MadeArt {

namespace {

const int X0 = 7300, Y0 = 500, X1 = 7530, Y1 = 712;

// the trunk's outline in world coordinates: down its west edge (below the
// bananas), along the shoulder at the floor, up its east edge (the shadowed
// gap before the rock behind it)
const int OUTLINE[][2] = {
	{7448, 500}, {7446, 530}, {7440, 548}, {7428, 564}, {7414, 590}, {7394, 620}, {7370, 655}, {7350, 684},
	{7322, 706}, {7496, 706},
	{7495, 650}, {7499, 600}, {7504, 550}, {7509, 500},
};

// an 8-bit picture ('L')
struct Grey {
	int w, h;
	std::vector<uint8_t> v;
	Grey(int w_, int h_)
		: w(w_), h(h_), v(size_t(w_) * h_, 0) {}
	uint8_t& at(int x, int y) { return v[size_t(y) * w + x]; }
};

// ---- ImageDraw.polygon(fill=ink), Pillow's polygon_generic with hline8 ----

struct Edge {
	int xmin, ymin, xmax, ymax, x0, y0;
	float dx;
};

void hline(Grey& im, int x0, int y, int x1, uint8_t ink) {
	if (y < 0 || y >= im.h)
		return;
	if (x0 < 0)
		x0 = 0;
	else if (x0 >= im.w)
		return;
	if (x1 < 0)
		return;
	else if (x1 >= im.w)
		x1 = im.w - 1;
	for (int x = x0; x <= x1; x++)
		im.at(x, y) = ink;
}

int roundUp(float f) {
	return int(f >= 0.0 ? std::floor(f + 0.5f) : -std::floor(std::fabs(f) + 0.5f));
}
int roundDown(float f) {
	return int(f >= 0.0 ? std::ceil(f - 0.5f) : -std::ceil(std::fabs(f) - 0.5f));
}

void fillPolygon(Grey& im, const std::vector<std::pair<int, int>>& pts, uint8_t ink) {
	std::vector<Edge> e;
	auto addEdge = [&](int x0, int y0, int x1, int y1) {
		Edge g;
		g.xmin = std::min(x0, x1);
		g.xmax = std::max(x0, x1);
		g.ymin = std::min(y0, y1);
		g.ymax = std::max(y0, y1);
		g.dx = y0 == y1 ? 0.0f : float(x1 - x0) / (y1 - y0);
		g.x0 = x0;
		g.y0 = y0;
		e.push_back(g);
	};
	size_t n = pts.size();
	for (size_t i = 0; i + 1 < n; i++)
		addEdge(pts[i].first, pts[i].second, pts[i + 1].first, pts[i + 1].second);
	if (pts[n - 1] != pts[0])
		addEdge(pts[n - 1].first, pts[n - 1].second, pts[0].first, pts[0].second);

	int ymin = im.h - 1, ymax = 0;
	std::vector<const Edge*> table;
	for (auto& g : e) {
		ymin = std::min(ymin, g.ymin);
		ymax = std::max(ymax, g.ymax);
		if (g.ymin == g.ymax) { // flat edges are drawn as they are
			hline(im, g.xmin, g.ymin, g.xmax, ink);
			continue;
		}
		table.push_back(&g);
	}
	ymin = std::max(ymin, 0);
	ymax = std::min(ymax, im.h);
	std::vector<float> xx;
	for (int y = ymin; y <= ymax; y++) {
		xx.clear();
		for (auto* g : table) {
			if (y < g->ymin || y > g->ymax)
				continue;
			xx.push_back((y - g->y0) * g->dx + g->x0);
			if (y == g->ymax && y < ymax) // an edge's end counts twice
				xx.push_back(xx.back());
			// (Pillow's "connect discontiguous corners" step only acts where
			// two edges on the same side meet at a peak; this outline has none)
		}
		std::sort(xx.begin(), xx.end());
		for (size_t i = 1; i < xx.size(); i += 2) {
			int xs = roundUp(xx[i - 1]), xe = roundDown(xx[i]);
			if (xe < xs)
				continue;
			hline(im, xs, y, xe, ink);
		}
	}
}

// ---- Image.resize(LANCZOS): Resample.c, 8 bits a pixel ----

double sinc(double x) {
	if (x == 0.0)
		return 1.0;
	x = x * 3.14159265358979323846; // M_PI
	return std::sin(x) / x;
}
double lanczos(double x) {
	return -3.0 <= x && x < 3.0 ? sinc(x) * sinc(x / 3) : 0.0;
}

const int PRECISION_BITS = 32 - 8 - 2;

// each output pixel's first input pixel, count and fixed-point weights
struct Coeffs {
	int ksize;
	std::vector<int> bounds; // xmin, count
	std::vector<int32_t> k;
};

Coeffs precompute(int inSize, int outSize) {
	Coeffs c;
	double scale = double(float(inSize) - 0.0f) / outSize, filterscale = std::max(scale, 1.0);
	double support = 3.0 * filterscale;
	c.ksize = int(std::ceil(support)) * 2 + 1;
	c.bounds.resize(size_t(outSize) * 2);
	c.k.assign(size_t(outSize) * c.ksize, 0);
	std::vector<double> kk(c.ksize);
	for (int xx = 0; xx < outSize; xx++) {
		double center = 0.0 + (xx + 0.5) * scale, ww = 0.0, ss = 1.0 / filterscale;
		int xmin = std::max(int(center - support + 0.5), 0);
		int xmax = std::min(int(center + support + 0.5), inSize) - xmin;
		std::fill(kk.begin(), kk.end(), 0.0);
		for (int x = 0; x < xmax; x++) {
			kk[x] = lanczos((x + xmin - center + 0.5) * ss);
			ww += kk[x];
		}
		for (int x = 0; x < xmax; x++)
			if (ww != 0.0)
				kk[x] /= ww;
		for (int x = 0; x < c.ksize; x++) // normalize_coeffs_8bpc
			c.k[size_t(xx) * c.ksize + x] = kk[x] < 0 ? int32_t(-0.5 + kk[x] * (1 << PRECISION_BITS)) : int32_t(0.5 + kk[x] * (1 << PRECISION_BITS));
		c.bounds[xx * 2] = xmin;
		c.bounds[xx * 2 + 1] = xmax;
	}
	return c;
}

uint8_t clip8(int in) {
	return uint8_t(std::clamp(in >> PRECISION_BITS, 0, 255));
}

Grey resizeLanczos(const Grey& in, int w, int h) {
	Coeffs ch = precompute(in.w, w), cv = precompute(in.h, h);
	Grey tmp(w, in.h); // horizontal pass first
	for (int y = 0; y < in.h; y++)
		for (int xx = 0; xx < w; xx++) {
			int xmin = ch.bounds[xx * 2], xmax = ch.bounds[xx * 2 + 1];
			const int32_t* k = &ch.k[size_t(xx) * ch.ksize];
			int ss = 1 << (PRECISION_BITS - 1);
			for (int x = 0; x < xmax; x++)
				ss += in.v[size_t(y) * in.w + x + xmin] * k[x];
			tmp.at(xx, y) = clip8(ss);
		}
	Grey out(w, h);
	for (int yy = 0; yy < h; yy++) {
		int ymin = cv.bounds[yy * 2], ymax = cv.bounds[yy * 2 + 1];
		const int32_t* k = &cv.k[size_t(yy) * cv.ksize];
		for (int xx = 0; xx < w; xx++) {
			int ss = 1 << (PRECISION_BITS - 1);
			for (int y = 0; y < ymax; y++)
				ss += tmp.at(xx, y + ymin) * k[y];
			out.at(xx, yy) = clip8(ss);
		}
	}
	return out;
}

// ---- ImageFilter.GaussianBlur(radius): BoxBlur.c, three box passes ----

float gaussianBlurRadius(float radius, int passes) {
	float sigma2 = radius * radius / passes;
	float L = float(std::sqrt(12.0 * sigma2 + 1.0)); // box length
	float l = float(std::floor((L - 1.0) / 2.0)); // integer part of box radius
	float a = (2 * l + 1) * (l * (l + 1) - 3 * sigma2); // fractional part
	a /= 6 * (sigma2 - (l + 1) * (l + 1));
	return l + a;
}

// one pass along a line of n pixels, step apart: an extended box (edges repeat)
void boxLine(uint8_t* p, int n, int step, float floatRadius) {
	int radius = int(floatRadius);
	uint32_t ww = uint32_t(float(1 << 24) / (floatRadius * 2 + 1));
	uint32_t fw = ((1 << 24) - (radius * 2 + 1) * ww) / 2;
	std::vector<uint8_t> in(n);
	for (int i = 0; i < n; i++)
		in[i] = p[size_t(i) * step];
	auto px = [&](int i) { return uint32_t(in[std::clamp(i, 0, n - 1)]); };
	for (int x = 0; x < n; x++) {
		uint32_t acc = 0;
		for (int i = x - radius; i <= x + radius; i++)
			acc += px(i);
		uint32_t bulk = acc * ww + (px(x - radius - 1) + px(x + radius + 1)) * fw;
		p[size_t(x) * step] = uint8_t((bulk + (1 << 23)) >> 24);
	}
}

void gaussianBlur(Grey& im, float radius) {
	const int passes = 3;
	float r = gaussianBlurRadius(radius, passes);
	for (int i = 0; i < passes; i++)
		for (int y = 0; y < im.h; y++)
			boxLine(&im.v[size_t(y) * im.w], im.w, 1, r);
	for (int i = 0; i < passes; i++)
		for (int x = 0; x < im.w; x++)
			boxLine(&im.v[x], im.h, im.w, r);
}

} // namespace

static Register zvtk("zvtk", [] {
	Rgba rgb = backgroundRegion(X0, Y0, X1, Y1);
	if (!rgb.w)
		return std::vector<Rgba>();
	const int w = rgb.w, h = rgb.h, ss = 4;
	Grey m(w * ss, h * ss);
	std::vector<std::pair<int, int>> pts;
	for (auto& p : OUTLINE)
		pts.push_back({(p[0] - X0) * ss, (p[1] - Y0) * ss});
	fillPolygon(m, pts, 255);
	Grey a = resizeLanczos(m, w, h);
	gaussianBlur(a, 0.7f);
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			uint8_t* p = rgb.at(x, y);
			float r = p[0], g = p[1], b = p[2];
			// sky showing between leaves at the west edge stays out
			bool sky = b > r + 8 && b > g && b > 140;
			p[3] = sky ? 0 : a.at(x, y);
		}
	return std::vector<Rgba>{rgb};
});

} // namespace MadeArt
