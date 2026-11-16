// a60a and a60a.att: art/ettinhead.py. The female ettin's head rebuilt from
// the male ettin's (owner, 2026-11-10: the female ettin's head is ugly;
// rework the male head with the female's red eyes and slightly more
// feminine hair). It takes the place of the game's own a60a; the body data
// is the male's (Body Data/a20a.att) as it is.
//
// Frame by frame (120 frames: views, expressions, eyes shut):
// - eyes: the dark maroon eye pixels become a glossy red ramp, keeping each
//   eye's shading and its white highlight;
// - hair: the pale, grayish locks are warmed toward gold and brightened;
// - locks: wherever hair hangs at the head's outer edge (its lowest pixels in
//   a column, with nothing below), it grows a few pixels longer, darkening
//   toward the tips, with the outline moved down to the new ends. Frames get
//   margin at the bottom only, so the body data's points (from each frame's
//   top-left) still fit.
//
// The recipe works in numpy float32 (with a few steps in float64, as numpy
// promotes them) and saves an .s16; the frames here are what the engine
// would read back from that file (565, opaque black kept as 1 << 5).

#include "MadeArt.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace MadeArt {

namespace {

const int LOCK = 3; // how much longer the hanging locks grow, in pixels
const int PAD = LOCK + 1; // margin added at the bottom of each frame

const float EYE_RAMP[5][3] = {{52, 0, 4}, {128, 6, 12}, {196, 22, 26}, {236, 70, 62}, {255, 150, 140}};
const double HAIR_HUE = 0.12;
const float HAIR_SAT = 0.40f, HAIR_LIFT = 1.22f;

// numpy's float32 remainder (the sign of the divisor)
float pyMod(float a, float b) {
	float m = std::fmod(a, b);
	if (m != 0) {
		if ((b < 0) != (m < 0))
			m += b;
	} else
		m = std::copysign(0.0f, b);
	return m;
}

// the eye ramp at t (float32 in, float64 inside as numpy promotes, float32 out)
void ramp(float t, float* out) {
	t = std::min(std::max(t, 0.0f), 1.0f) * 4.0f;
	int i = std::min(int(t), 3);
	double f = double(t) - i;
	for (int c = 0; c < 3; c++)
		out[c] = float(double(EYE_RAMP[i][c]) * (1 - f) + double(EYE_RAMP[i + 1][c]) * f);
}

// colorsys.hsv_to_rgb(HAIR_HUE, s, v) with s, v numpy float32 scalars
// (the arithmetic goes to float64), as float32
void hsvToRgb(float s, float v, float* out) {
	double h = HAIR_HUE;
	double r, g, b;
	if (s == 0.0f) {
		r = g = b = v;
	} else {
		int i = int(h * 6.0);
		double f = (h * 6.0) - i;
		double p = double(v) * (1.0 - double(s));
		double q = double(v) * (1.0 - double(s) * f);
		double t = double(v) * (1.0 - double(s) * (1.0 - f));
		switch (i % 6) {
			case 0: r = v, g = t, b = p; break;
			case 1: r = q, g = v, b = p; break;
			case 2: r = p, g = v, b = t; break;
			case 3: r = p, g = q, b = v; break;
			case 4: r = t, g = p, b = v; break;
			default: r = v, g = p, b = q; break;
		}
	}
	out[0] = float(r);
	out[1] = float(g);
	out[2] = float(b);
}

// the mask without its (4-connected) pieces smaller than n pixels
std::vector<char> blobsAtLeast(const std::vector<char>& mask, int W, int H, size_t n) {
	std::vector<char> seen(mask.size(), 0), keep(mask.size(), 0);
	std::vector<std::pair<int, int>> stack, piece;
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++) {
			if (!mask[size_t(y) * W + x] || seen[size_t(y) * W + x])
				continue;
			stack.assign(1, {y, x});
			piece.clear();
			seen[size_t(y) * W + x] = 1;
			while (!stack.empty()) {
				auto [cy, cx] = stack.back();
				stack.pop_back();
				piece.push_back({cy, cx});
				const int ns[4][2] = {{cy + 1, cx}, {cy - 1, cx}, {cy, cx + 1}, {cy, cx - 1}};
				for (auto& nb : ns) {
					int ny = nb[0], nx = nb[1];
					if (ny >= 0 && ny < H && nx >= 0 && nx < W && mask[size_t(ny) * W + nx] && !seen[size_t(ny) * W + nx]) {
						seen[size_t(ny) * W + nx] = 1;
						stack.push_back({ny, nx});
					}
				}
			}
			if (piece.size() >= n)
				for (auto& [py, px] : piece)
					keep[size_t(py) * W + px] = 1;
		}
	return keep;
}

// the mask grown by r in every direction, wrapping round the edges (np.roll)
std::vector<char> grownWrapped(const std::vector<char>& mask, int W, int H, int r) {
	std::vector<char> out(mask.size(), 0);
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++) {
			char any = 0;
			for (int dy = -r; dy <= r && !any; dy++)
				for (int dx = -r; dx <= r && !any; dx++) {
					int sy = ((y - dy) % H + H) % H, sx = ((x - dx) % W + W) % W;
					any = mask[size_t(sy) * W + sx];
				}
			out[size_t(y) * W + x] = any;
		}
	return out;
}

Rgba rework(const Rgba& im) {
	const int W = im.w, H0 = im.h, H = H0 + PAD;
	const size_t N = size_t(W) * H;
	// the frame in float32, with PAD empty rows at the bottom
	std::vector<float> rgb(N * 3, 0.0f), alpha(N, 0.0f);
	for (int y = 0; y < H0; y++)
		for (int x = 0; x < W; x++) {
			const uint8_t* p = im.at(x, y);
			size_t i = size_t(y) * W + x;
			for (int c = 0; c < 3; c++)
				rgb[i * 3 + c] = p[c];
			alpha[i] = p[3];
		}
	std::vector<char> opaque(N);
	std::vector<float> hh(N), ss(N), vv(N);
	for (size_t i = 0; i < N; i++) {
		opaque[i] = alpha[i] > 0;
		// hsv(), in float32
		float r = rgb[i * 3] / 255.0f, g = rgb[i * 3 + 1] / 255.0f, b = rgb[i * 3 + 2] / 255.0f;
		float mx = std::max(std::max(r, g), b), mn = std::min(std::min(r, g), b);
		float v = mx;
		float s = mx > 0 ? (mx - mn) / std::max(mx, 1e-6f) : 0.0f;
		float h = 0.0f;
		float d = std::max(mx - mn, 1e-6f);
		if (mx == r)
			h = pyMod((g - b) / d, 6.0f);
		if (mx == g)
			h = (b - r) / d + 2.0f;
		if (mx == b)
			h = (r - g) / d + 4.0f;
		hh[i] = pyMod(h / 6.0f, 1.0f);
		ss[i] = s;
		vv[i] = v;
	}
	// the eyes: a pure deep red core (hue near 0, almost no green or blue)
	// in blobs of 4 pixels or more, and the near-red ring around them; the
	// skin's dark shading (under the chin) is orange-brown and stays skin
	std::vector<char> core(N);
	for (size_t i = 0; i < N; i++)
		core[i] = opaque[i] && (hh[i] < 0.03f || hh[i] > 0.95f) && ss[i] > 0.6f && vv[i] < 0.6f && vv[i] > 0.08f;
	core = blobsAtLeast(core, W, H, 4);
	std::vector<char> ring = grownWrapped(core, W, H, 1);
	std::vector<char> eye(N);
	for (size_t i = 0; i < N; i++)
		eye[i] = core[i] || (ring[i] && opaque[i] && (hh[i] < 0.045f || hh[i] > 0.95f) && ss[i] > 0.45f && vv[i] < 0.5f);
	// the eye's white highlight: bright and pale, beside eye pixels
	std::vector<char> nearEye = grownWrapped(eye, W, H, 2);
	std::vector<char> shine(N), hair(N);
	for (size_t i = 0; i < N; i++) {
		shine[i] = opaque[i] && nearEye[i] && vv[i] > 0.75f && ss[i] < 0.3f && !eye[i];
		hair[i] = opaque[i] && !eye[i] && !shine[i] && vv[i] > 0.30f && ss[i] < 0.36f && ((hh[i] > 0.07f && hh[i] < 0.40f) || ss[i] < 0.12f);
	}

	const float SHINE[3] = {255.0f * 0.15f, 200.0f * 0.15f, 190.0f * 0.15f};
	for (size_t i = 0; i < N; i++) {
		float* p = &rgb[i * 3];
		if (eye[i]) // eyes: shading kept, as a red ramp
			ramp(vv[i] / 0.6f * 0.92f, p);
		else if (shine[i])
			for (int c = 0; c < 3; c++)
				p[c] = p[c] * 0.85f + SHINE[c];
		else if (hair[i]) {
			// hair: warmed toward gold and brightened, lightness kept
			float hv = std::min(std::max(vv[i] * HAIR_LIFT, 0.0f), 1.0f);
			float hs = std::min(std::max(HAIR_SAT * (0.6f + 0.6f * ss[i] / 0.36f), 0.0f), 0.55f);
			float gold[3];
			hsvToRgb(hs, hv, gold);
			for (int c = 0; c < 3; c++)
				p[c] = gold[c] * 255.0f;
		}
	}

	// locks: a column whose lowest opaque pixels are hair (with at most an
	// outline pixel under them) hangs free: grow it LOCK pixels longer
	std::vector<float> newRgb = rgb, newAlpha = alpha;
	auto at = [W](int x, int y) { return size_t(y) * W + x; };
	auto lowest = [&](int x) {
		for (int y = H - 1; y >= 0; y--)
			if (alpha[at(x, y)] > 0)
				return y;
		return -1;
	};
	// (the recipe's scalar comparisons, v < 0.30, are in float64)
	auto isOutline = [&](int x, int y) { return double(vv[at(x, y)]) < 0.30 && !hair[at(x, y)]; };
	// taper at the ends of a run of hanging columns
	auto hangs = [&](int xx) {
		if (xx < 0 || xx >= W)
			return false;
		int y2 = lowest(xx);
		if (y2 < 0)
			return false;
		if (isOutline(xx, y2))
			y2 -= 1;
		return y2 >= 1 && hair[at(xx, y2)];
	};
	for (int x = 0; x < W; x++) {
		int yb = lowest(x);
		if (yb < 0)
			continue;
		int topHair = yb;
		bool outline = isOutline(x, yb);
		if (outline)
			topHair = yb - 1;
		if (topHair < 2 || !(hair[at(x, topHair)] && hair[at(x, topHair - 1)]))
			continue;
		int length = (hangs(x - 1) && hangs(x + 1)) ? LOCK : LOCK - 1;
		const float* base = &rgb[at(x, topHair) * 3];
		for (int k = 1; k <= length; k++) {
			float m = float(1 - 0.12 * k);
			for (int c = 0; c < 3; c++)
				newRgb[at(x, topHair + k) * 3 + c] = base[c] * m;
			newAlpha[at(x, topHair + k)] = 255;
		}
		if (outline) {
			for (int c = 0; c < 3; c++)
				newRgb[at(x, topHair + length + 1) * 3 + c] = rgb[at(x, yb) * 3 + c];
			newAlpha[at(x, topHair + length + 1)] = 255;
		}
	}

	// to uint8 (clipped, truncated), then through the .s16 as s16.py
	// write() and read() would: 565, alpha below 128 transparent, opaque
	// pure black kept as 1 << 5
	Rgba out(W, H);
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++) {
			size_t i = at(x, y);
			unsigned ch[4];
			for (int c = 0; c < 3; c++)
				ch[c] = unsigned(std::min(std::max(newRgb[i * 3 + c], 0.0f), 255.0f));
			ch[3] = unsigned(std::min(std::max(newAlpha[i], 0.0f), 255.0f));
			uint16_t v = uint16_t(((ch[0] >> 3) << 11) | ((ch[1] >> 2) << 5) | (ch[2] >> 3));
			if (v == 0)
				v = 1 << 5;
			if (ch[3] < 128)
				v = 0;
			uint8_t* p = out.at(x, y);
			p[0] = uint8_t(((v >> 11) & 31) << 3);
			p[1] = uint8_t(((v >> 5) & 63) << 2);
			p[2] = uint8_t((v & 31) << 3);
			p[3] = v == 0 ? 0 : 255;
		}
	return out;
}

std::vector<Rgba> femaleHead() {
	std::vector<Rgba> male = gameSprite("a20a");
	std::vector<Rgba> female;
	for (auto& im : male)
		female.push_back(rework(im));
	return female;
}

} // namespace

static Register a60a("a60a", femaleHead, true);
static RegisterBodyData a60aAtt("a60a.att", [] { return gameFile("Body Data", "a20a.att"); });

} // namespace MadeArt
