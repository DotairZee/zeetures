#pragma once

// The walkable glass tube's made art, part: the gene lab's new west end (art/tube_walk.py: lab_west_end; art/tube_walk_lib.py: Canvas, light_pools, rust_and_grime, trim_wall, lab_wall).
// A pixel-exact port of the Python recipe (2026-11-19); see npcore.h for the
// numpy/Pillow operations it is built on.
//
// Every function here follows its Python namesake step by step, with the
// same random streams in the same order and numpy 1.x's float32 / float64
// promotions at each step (noted where they decide a result). Proof:
// art/tube_walk_check_lab.py, through OPENC2E_TW_CHECK_LAB=DIR (below).

#include "npcore.h"

#include <array>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace MadeArt {
namespace tubewalk {

// tube_walk_lib.py WALL_X0, WALL_X1, WALL_Y0, WALL_Y1, WALL_FLOOR: the new
// wall strip in world coordinates (x 5990-6172, y 1478-1814; plating to 1798)
constexpr int kWallX0 = 5990, kWallX1 = 6172, kWallY0 = 1478, kWallY1 = 1814;
constexpr int kWallFloor = 1798;

// tube_walk.py's scene, which lab_west_end's base and bg cover: OX, OY, W, H
constexpr int kLabSceneX = 4400, kLabSceneY = 1250, kLabSceneW = 2340, kLabSceneH = 840;

// a developer's view of the intermediate arrays (OPENC2E_TW_CHECK_LAB);
// each callback may be empty. Masks go to u8 as 0 / 1.
struct Tap {
	std::function<void(const std::string&, const np::ImgF&)> f32;
	std::function<void(const std::string&, const np::ImgD&)> f64;
	std::function<void(const std::string&, const np::Img8&)> u8;
};

// lib.blur(a, s): a.clip(0, 255) to uint8, Pillow's GaussianBlur(s), back
// to float32 (c 1 or 3)
np::ImgF blur(const np::ImgF& a, double s);
np::ImgF blur(const np::ImgD& a, double s);

// lib.Canvas: world-placed, supersampled (SS = 2) RGB and coverage, lit
// like the lab. Its rgb starts float32 and becomes float64 at the first
// put of a float64 colour (any put of a shaded surface), as numpy's does;
// rgbF holds it while it is float32 (is64 false), rgb after. Only what the
// recipe has, on the dtype path lab_wall takes.
class Canvas {
  public:
	static constexpr int SS = 2;
	using Mask = std::vector<uint8_t>; // H x W, C order, 0 / 1

	int x0, y0, w, h, W, H;
	np::ImgF rgbF; // (H, W, 3) float32, while !is64
	np::ImgD rgb; // (H, W, 3) float64, once is64
	bool is64 = false;
	np::ImgF a; // (H, W) float32 coverage
	np::ImgF X, Y; // (H, W) float32 world coordinates of each sample
	np::Generator rng;
	Mask crease; // trim_wall's c.crease (set by trimWall)

	Canvas(int x0, int y0, int w, int h, uint64_t seed = 1);

	// noise(cells, amp): float32 (H, W), a bicubic-upsized grid of draws
	np::ImgF noise(double cells, double amp = 1.0);
	// shade(n, albedo, spec, shin, rimk): float32 normals and albedo
	// (H, W, 3), the lit colour float64 (H, W, 3)
	np::ImgD shade(const np::ImgF& n, const np::ImgF& albedo, double spec = 0.35, double shin = 24, double rimk = 0.35);
	// put(mask, col): a float32 colour keeps rgb float32; float64 makes it float64
	void put(const Mask& mask, const np::ImgF& col);
	void put(const Mask& mask, const np::ImgD& col);
	void sphere(double cx, double cy, double r, std::array<double, 3> color, double spec = 0.5);
	void cylH(double x0, double x1, double cy, double r, std::array<double, 3> color, double spec = 0.45, double tarnish = 0.0);
	void flangeH(double cx, double cy, double r, double w, std::array<double, 3> color, int bolts = 3);
	// color: ints, as the recipe passes them ([64, 70, 66])
	void ibeamH(int x0, int x1, int y0, int y1, std::array<int, 3> color);
	void grime(double amount = 0.25);
	// result(): (rgb 1x float64 clipped to 0..255 (h, w, 3), alpha float32 (h, w))
	std::pair<np::ImgD, np::ImgF> result() const;
};

// light_pools(c, pools, ambient): pools of (px, py, rad, k, (r, g, b))
struct LightPool {
	double px, py, rad, k;
	std::array<double, 3> col;
};
void lightPools(Canvas& c, const std::vector<LightPool>& pools, double ambient = 0.5);

// rust_and_grime(c, m, rust, streaks, chips): m an (H, W) mask
void rustAndGrime(Canvas& c, const Canvas::Mask& m, double rust = 0.5, double streaks = 0.5, double chips = 0.3);

// trim_wall(c, x0, y0, x1, y1, plate, trim, seed, light): light of
// (lx, ly, rad, k); its inner strip_h / strip_v included
struct TrimLight {
	double lx, ly, rad, k;
};
void trimWall(Canvas& c, int x0, int y0, int x1, int y1, std::array<int, 3> plate, std::array<int, 3> trim,
	uint64_t seed = 3, const std::vector<TrimLight>& light = {{6080, 1580, 60, 0.5}});

// lib.lab_wall(): the new wall strip, (rgb float64 (336, 182, 3) clipped
// 0..255, alpha float32 (336, 182)) at 1x over kWallX0..X1, kWallY0..Y1
struct LabWall {
	np::ImgD rgb;
	np::ImgF a;
};
LabWall labWall(const Tap* tap = nullptr);

// tube_walk.py lab_west_end(base, bg): the gene lab's new west wall strip
// joined to the room, drawn into base in place. base and bg are the scene,
// float32 (kLabSceneH, kLabSceneW, 3) from (kLabSceneX, kLabSceneY): base
// what make() has made so far (paint_out's result), bg the game's
// background there (region(...).astype(np.float32)[..., :3]).
void labWestEnd(np::ImgF& base, const np::ImgF& bg, const Tap* tap = nullptr);

// developer aid: with OPENC2E_TW_CHECK_LAB=DIR set, any openc2e program
// reads DIR/in_base.bin and DIR/in_bg.bin (written by
// art/tube_walk_check_lab.py DIR --inputs C2_DIR), writes this module's
// results and intermediates to DIR at startup and exits; the script then
// compares them with the recipe's.
int labSelfCheck(const std::string& dir);

} // namespace tubewalk
} // namespace MadeArt
