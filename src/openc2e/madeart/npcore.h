#pragma once

// npcore: the numpy and Pillow primitives the made-art ports call, each the
// same to the bit as NumPy 1.26.4 and Pillow 10.2.0 on the reference machine
// (x86-64, glibc; numpy's AVX512_SKX dispatch). Written for the port of
// art/tube_walk.py, shared by any port that wants it. art/npcore_check.py
// proves every function here against numpy and Pillow (max difference 0,
// floats compared bit for bit): run the engine (any of its programs) with
// OPENC2E_NPCORE_CHECK=DIR set, then `python3 art/npcore_check.py DIR`.
//
// Arrays are numpy's: C order, (h, w, c), indexed (y, x, k). c is 1 for a
// plane ("L"), 3 for RGB, 4 for RGBA. Img8 is a uint8 picture as Pillow and
// np.asarray see it; ImgF / ImgD are float32 / float64 arrays.
//
// NumPy 1.x promotion, which the modules must follow by hand (it decides
// float32 or float64 at each step; the reductions below follow it already):
// - array op array: the wider type (float32 * float64 array -> float64; a
//   np.array([...]) of Python floats is float64, so `n * L` with a float32 n
//   and L = np.array([-0.55, ...]) is float64).
// - array op Python scalar, or op a 0-d numpy scalar of the same kind
//   (np.float64(2.0), a float returned by np.median): the array's type
//   (float32 * 0.85 -> float32, the 0.85 rounded to float32 first).
// - scalar op scalar (np.float32(a) / 3, float32-scalar + Python float): the
//   default types, so float64; numpy scalar results are often float64 that
//   way and then cast back (see meanFlat).
// - int array / int -> float64; bool arrays act as 0/1 of the other type.
// - x ** 2 is an exact square (np.square); x ** 0.5 is np.sqrt; other
//   exponents go through np.power (see the note on transcendentals).
// - astype(np.uint8) truncates toward zero (u8() below); np.clip and
//   a.clip(lo, hi) are max then min, NaN passing through (clip() below).
//
// Transcendentals: numpy here does not use glibc for most of them, and its
// results differ from std:: by an ulp or two in a large share of inputs.
// - matched by std::: np.sqrt (IEEE), float64 np.sin / np.cos (glibc), and
//   everything in Python's math module (math.exp, math.sin, math.atan, ...).
// - ported here: np.tanh (f32, f64), np.exp / np.sin / np.cos on float32
//   (numpy's own SIMD code; on a CPU without AVX2/FMA3 numpy itself falls
//   back to libm for these, so the Python side is only exact there).
// - NOT portable (Intel SVML on this machine, libm or numpy's own code on
//   others, so even the Python reference differs between machines):
//   float64 np.exp, np.power with any exponent but 2, 0.5, 1, -1, 0
//   (x ** 0.85, x ** 3, x ** 24 ...), np.log, np.arctan2 and the rest of
//   SVML's set. Recipes call pw / ex (tube_walk_lib.py) instead, and the
//   ports call np::pw / np::ex.
// `python3 art/npcore_check.py DIR --transcendentals` measures the shares.
//
// Floating-point contraction must stay off (no a * b + c fused into an FMA):
// npcore.cpp turns it off for itself; a module built with -march flags that
// enable FMA must do the same (#pragma GCC optimize("fp-contract=off") or
// -ffp-contract=off).

#include "MadeArt.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace MadeArt {
namespace np {

// ---- arrays ------------------------------------------------------------

template <typename T>
struct Img {
	int h = 0, w = 0, c = 1;
	std::vector<T> v;
	Img() = default;
	Img(int h_, int w_, int c_ = 1, T fill = T())
		: h(h_), w(w_), c(c_), v(size_t(h_) * w_ * c_, fill) {}
	T& operator()(int y, int x, int k = 0) { return v[(size_t(y) * w + x) * c + k]; }
	const T& operator()(int y, int x, int k = 0) const { return v[(size_t(y) * w + x) * c + k]; }
	T* px(int y, int x) { return &v[(size_t(y) * w + x) * c]; }
	const T* px(int y, int x) const { return &v[(size_t(y) * w + x) * c]; }
	size_t size() const { return v.size(); }
	bool empty() const { return v.empty(); }
};
using Img8 = Img<uint8_t>; // uint8, as a Pillow picture
using ImgF = Img<float>; // float32
using ImgD = Img<double>; // float64

// ---- dtype conversions ---------------------------------------------------

// a.astype(np.uint8) of one value: truncated toward zero; out of range it
// wraps as numpy's C cast does here (256 -> 0, -1 -> 255, NaN and |v| >= 2^31 -> 0)
uint8_t u8(double v);
// np.clip(v, lo, hi) / a.clip(lo, hi): max(v, lo) then min(.., hi), NaN kept
float clip(float v, float lo, float hi);
double clip(double v, double lo, double hi);

Img8 toU8(const ImgF& a); // a.astype(np.uint8)
Img8 toU8(const ImgD& a);
// a.clip(0, 255).astype(np.uint8), the recipes' usual step into Pillow
Img8 clipToU8(const ImgF& a);
Img8 clipToU8(const ImgD& a);
ImgF toF32(const Img8& a); // np.asarray(im).astype(np.float32)
ImgF toF32(const ImgD& a); // a.astype(np.float32) (round to nearest)
ImgD toF64(const Img8& a);
ImgD toF64(const ImgF& a);
// one channel of a picture (k), or channels [k0, k0 + n)
template <typename T>
Img<T> channels(const Img<T>& a, int k0, int n = 1);

// MadeArt::Rgba (backgroundRegion, gameSprite) to a 3- or 4-channel Img8
Img8 fromRgba(const Rgba& im, int c = 3);
// an Img8 (c 1, 3 or 4) to MadeArt::Rgba (L as gray; opaque unless c is 4)
Rgba toRgba(const Img8& im);

// ---- numpy.random.default_rng(seed) ------------------------------------

// SeedSequence(seed) -> PCG64 -> Generator, NumPy 1.26. Every draw takes
// the same words of the stream as numpy, so mixed calls on one generator
// stay in step with Python (Generator.integers keeps half of a 64-bit word
// for its next 32-bit draw, across other calls, as numpy does).
class Generator {
  public:
	explicit Generator(uint64_t seed);

	// rng.random() / rng.random(n): [0, 1) doubles; an array is filled in C
	// order, so random((h, w)) is random(h * w) read row by row
	double random();
	std::vector<double> random(size_t n);
	// rng.uniform(lo, hi) / (lo, hi, n): lo + (hi - lo) * random()
	double uniform(double lo, double hi);
	std::vector<double> uniform(double lo, double hi, size_t n);
	// rng.integers(lo, hi) / (lo, hi, n): int64 in [lo, hi) (Lemire's method,
	// 32-bit draws when hi - lo - 1 fits 32 bits); integers(hi) is (0, hi)
	int64_t integers(int64_t lo, int64_t hi);
	int64_t integers(int64_t hi) { return integers(0, hi); }
	std::vector<int64_t> integers(int64_t lo, int64_t hi, size_t n);
	// rng.standard_normal() and rng.normal(loc, scale[, n]): numpy's ziggurat
	// for doubles; loc + scale * z
	double standardNormal();
	double normal(double loc, double scale);
	std::vector<double> normal(double loc, double scale, size_t n);

	// the raw stream (PCG64 XSL-RR), for anything else numpy draws
	uint64_t nextU64();
	uint32_t nextU32();
	double nextDouble(); // (nextU64() >> 11) * 2^-53

  private:
	uint64_t stHi = 0, stLo = 0, incHi = 0, incLo = 0;
	bool hasU32 = false;
	uint32_t u32 = 0;
	void step();
};

// ---- Pillow 10.2 ----------------------------------------------------------
// On Img8 pictures with c 1 ("L"), 3 ("RGB") or 4 ("RGBA").

enum class Resample { Nearest, Box, Bilinear, Hamming, Bicubic, Lanczos };

// Image.resize((W, H), resample): Pillow's two-pass fixed-point resampler
// (horizontal, then vertical; a pass is skipped when its size is unchanged);
// RGBA goes through premultiplied RGBa as Pillow does
Img8 resize(const Img8& im, int W, int H, Resample r);
// ImageFilter.GaussianBlur(radius): three extended box blurs across then
// three down (BoxBlur.c); radius is taken as a C float, as Pillow does
Img8 gaussianBlur(const Img8& im, double radius);
// ImageFilter.BoxBlur(radius)
Img8 boxBlur(const Img8& im, double radius);
// ImageFilter.RankFilter(size, rank): edges replicated, each band alone;
// MinFilter is rank 0, MaxFilter size*size-1, MedianFilter size*size/2
Img8 rankFilter(const Img8& im, int size, int rank);
Img8 minFilter(const Img8& im, int size);
Img8 maxFilter(const Img8& im, int size);
Img8 medianFilter(const Img8& im, int size);
// Image.rotate(angle, resample=BICUBIC, expand) (counterclockwise degrees;
// the recipe's rotate(-ang) turns down to the east): Image.py's affine
// matrix (rounded to 15 places), Geometry.c's bicubic; RGBA premultiplied;
// outside the source is 0. With expand the size grows to hold the corners.
// resample may be Nearest, Bilinear or Bicubic.
Img8 rotate(const Img8& im, double angle, Resample r = Resample::Bicubic, bool expand = true);

// ImageDraw on an "L" picture (c must be 1), ink a gray value
// .polygon(pts, fill=ink): coordinates truncated to int as Pillow does
void polygon(Img8& im, const std::vector<std::pair<double, double>>& pts, uint8_t ink);
// .rectangle((x0, y0, x1, y1), fill=ink): inclusive corners
void rectangle(Img8& im, int x0, int y0, int x1, int y1, uint8_t ink);
// .ellipse((x0, y0, x1, y1), fill=ink)
void ellipse(Img8& im, int x0, int y0, int x1, int y1, uint8_t ink);
// .pieslice((x0, y0, x1, y1), start, end, fill=ink)
void pieslice(Img8& im, int x0, int y0, int x1, int y1, float start, float end, uint8_t ink);
// .rounded_rectangle((x0, y0, x1, y1), radius=r, fill=ink), all corners
void roundedRectangle(Img8& im, int x0, int y0, int x1, int y1, int radius, uint8_t ink);

// ---- numpy reductions ----------------------------------------------------
// numpy adds float arrays pairwise (8 accumulators per block of 128) along
// the innermost axis it reduces, and one by one across an outer reduced
// axis. These follow it per case (all checked against numpy).

// np.sum over values a[0..n) in C order (no promotion: float32 sums in
// float32): numpy's buffered reduction hands its pairwise loop 8192 values
// at a time and adds the chunks' sums in order. This holds for any array or
// view summed whole (a.sum(), a[mask].sum(), a[y0:y1, x].sum()).
float sum(const float* a, size_t n);
double sum(const double* a, size_t n);
// a.mean() (axis None) returned as a scalar: float32 -> the float32 sum
// divided in float64 and rounded back, as numpy's scalar path does
float meanFlat(const float* a, size_t n);
double meanFlat(const double* a, size_t n);
// a.std() (axis None, ddof 0)
float stdFlat(const float* a, size_t n);
double stdFlat(const double* a, size_t n);
inline float meanFlat(const ImgF& a) { return meanFlat(a.v.data(), a.v.size()); }
inline double meanFlat(const ImgD& a) { return meanFlat(a.v.data(), a.v.size()); }

// a.sum(-1) / a.mean(-1) over the channels (keepdims or not: same values):
// (h, w, c) -> (h, w, 1); also for an (n, c) table stored as h = n, w = 1
ImgF sumLast(const ImgF& a);
ImgD sumLast(const ImgD& a);
ImgF meanLast(const ImgF& a);
ImgD meanLast(const ImgD& a);
// (n, c).mean(axis=0) / sum(axis=0) where the n rows are the outer axis
// (c values per row, rows one after another): added row by row, then / n
std::vector<float> meanAxis0(const float* rows, size_t n, int c);
std::vector<double> meanAxis0(const double* rows, size_t n, int c);
std::vector<float> sumAxis0(const float* rows, size_t n, int c);
std::vector<double> sumAxis0(const double* rows, size_t n, int c);
// a.reshape(h, ss, w, ss[, c]).mean(axis=(1, 3)): the supersampled canvas
// back to 1x (h = a.h / ss, w = a.w / ss)
ImgF blockMean(const ImgF& a, int ss);
ImgD blockMean(const ImgD& a, int ss);

// ---- numpy functions -------------------------------------------------------

// np.median of one lane of values (copied: it partitions). scalar: the
// result is a 0-d scalar (axis None) rather than an element of an array
// result (axis given); only the rounding of the even case's mean differs
float median(std::vector<float> lane, bool scalar = false);
double median(std::vector<double> lane, bool scalar = false);
// np.percentile(a, q, method='linear') of one lane. A float32 lane gives a
// float64 result (numpy interpolates with a float64 weight), so this
// returns double for both
double percentile(std::vector<float> lane, double q);
double percentile(std::vector<double> lane, double q);
// np.sort (ascending, NaN-free)
template <typename T>
std::vector<T> sorted(std::vector<T> a);
// np.searchsorted(sortedA, v, side): left -> first i with a[i] >= v,
// right -> first i with a[i] > v
template <typename T>
int64_t searchsorted(const std::vector<T>& sortedA, T v, bool right = false);
// np.interp(x, xp, fp) (float64; left fp[0], right fp[-1]): numpy's
// compiled_base.c, slope * (x - xp[j]) + fp[j]
double interp(double x, const std::vector<double>& xp, const std::vector<double>& fp);
std::vector<double> interp(const std::vector<double>& x, const std::vector<double>& xp, const std::vector<double>& fp);
// np.linspace(start, stop, n) (endpoint included; float64)
std::vector<double> linspace(double start, double stop, int n);
// np.convolve(a, k, mode) in float64 (float32 inputs are converted first,
// as numpy does when either side is float64): mode 'valid', 'same', 'full'
std::vector<double> convolve(const std::vector<double>& a, const std::vector<double>& k, const std::string& mode = "valid");
// np.pad(a, n, mode='edge') of a 1-D array
template <typename T>
std::vector<T> padEdge(const std::vector<T>& a, int n);
// np.gradient(f) of a 2-D float32 plane (spacing 1, edge_order 1): the
// change down the rows (gy) and across the columns (gx), float32
void gradient(const ImgF& f, ImgF& gy, ImgF& gx);
// np.tanh, as numpy computes it on this machine (its SIMD lookup-table
// polynomial; float64 for a Python float or float64 array)
double tanh(double x);
float tanh(float x);
// np.exp, np.sin, np.cos of float32 values (numpy's own SIMD algorithms:
// Cody-Waite reduction and polynomials; sin/cos fall back to glibc beyond
// |x| 117435.992 / 71476.0625, as numpy does)
float exp(float x);
float sin(float x);
float cos(float x);
// float64 np.sin / np.cos are glibc's, so std::sin / std::cos match them
inline double sin(double x) { return std::sin(x); }
inline double cos(double x) { return std::cos(x); }
// float64 np.exp is Intel SVML here and not ported (no silent float cast:
// call np::exp on a float, or use ex for a recipe's own deterministic exp)
double exp(double x) = delete;
// art/tube_walk_lib.py's pw(x, e) and ex(x), to the bit: x ** e and e ** x
// built only from IEEE-exact steps (frexp, ldexp, rint, + - * /), so the
// Python recipe gives the same bits on every machine and this matches it.
// Float64 inside (~1e-15 relative error on the recipes' ranges; float32 in
// gives the float64 result rounded to float32, as ** would). pw: x >= 0, or
// a negative x with an integer e; x == 0 gives 0 for e > 0.
double pw(double x, double e);
float pw(float x, double e);
double ex(double x);
float ex(float x);
// math.hypot(x, y) (CPython 3.12's correctly rounded vector_norm)
double hypot(double x, double y);
// np.linalg.norm(a, axis=-1[, keepdims]): sqrt of the sum of squares over
// the channels, in the array's own type: (h, w, c) -> (h, w, 1)
ImgF normLast(const ImgF& a);
ImgD normLast(const ImgD& a);
// np.linalg.norm(v) of a 1-D float64 vector (axis None): sqrt(v.dot(v)),
// the dot added in order (the system's reference BLAS)
double norm(const std::vector<double>& v);

// developer aid: with OPENC2E_NPCORE_CHECK=DIR set, any openc2e program
// writes every primitive's results on fixed inputs to DIR at startup and
// exits; art/npcore_check.py compares them with numpy and Pillow
int selfCheck(const std::string& dir);

// ---- templates -------------------------------------------------------------

template <typename T>
Img<T> channels(const Img<T>& a, int k0, int n) {
	Img<T> o(a.h, a.w, n);
	for (size_t i = 0, m = size_t(a.h) * a.w; i < m; i++)
		for (int k = 0; k < n; k++)
			o.v[i * n + k] = a.v[i * a.c + k0 + k];
	return o;
}

template <typename T>
std::vector<T> sorted(std::vector<T> a) {
	std::sort(a.begin(), a.end());
	return a;
}

template <typename T>
int64_t searchsorted(const std::vector<T>& a, T v, bool right) {
	auto it = right ? std::upper_bound(a.begin(), a.end(), v) : std::lower_bound(a.begin(), a.end(), v);
	return int64_t(it - a.begin());
}

template <typename T>
std::vector<T> padEdge(const std::vector<T>& a, int n) {
	std::vector<T> o;
	if (a.empty())
		return o;
	o.reserve(a.size() + 2 * size_t(n));
	o.insert(o.end(), size_t(n), a.front());
	o.insert(o.end(), a.begin(), a.end());
	o.insert(o.end(), size_t(n), a.back());
	return o;
}

} // namespace np
} // namespace MadeArt
