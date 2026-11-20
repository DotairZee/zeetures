#pragma once

// The walkable glass tube's made art, part: the scene and the old tube painted out (art/tube_walk.py: put_scenery, old_tube, new_tube, paint_out; art/tube_walk_lib.py: quilt_fill, tone_correct, match_texture, restore_branches).
// A pixel-exact port of the Python recipe (2026-11-19); see npcore.h for the
// numpy/Pillow operations it is built on.
//
// Names, arguments and array shapes follow the Python, so make()'s lines
// map one to one:
//
//     GameInputs in;  readInputs(in);                       // bg, bgt, swmp, ta
//     Scene sc = makeScene(in);                         // tubecomp, scene
//     Mask mask; ImgF strip;  old_tube(sc.tubecomp, in.ta, mask, strip);
//     ImgF MOD, pc;           new_tube(strip, MOD, pc);
//     ImgF base = paint_out(sc.scene, in.bg, in.bgt, mask, strip);
//     ...
//     put_scenery(o, in.swmp, sceneryFrom(1), &deck_rm);   // the cut= form
//
// Arrays are npcore's (numpy's C order, (h, w, c)): pictures are float32
// ImgF with c 3 (RGB) or 4 (RGBA); a bool array is a Mask, an Img8 with c 1
// holding 0 or 1. Everything is in scene coordinates (world x - OX, y - OY)
// unless a name says world.
//
// Proof: OPENC2E_TW_CHECK_PAINT=DIR (with OPENC2E_TW_CHECK_PAINT_C2=the
// game's folder, default the Steam copy) makes any openc2e program run this
// module on the game's real pictures at startup, write every intermediate
// to DIR and exit; `python3 art/tube_walk_check_paint.py DIR` runs the
// recipe itself and compares (max difference 0, floats bit for bit).

#include "npcore.h"

#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace MadeArt {
namespace tubewalk {

using np::Img8;
using np::ImgD;
using np::ImgF;
using Mask = Img8; // a bool array: c 1, values 0 or 1

// ---- tube_walk.py's constants -----------------------------------------------

constexpr int OX = 4400, OY = 1250, W = 2340, H = 840; // the scene, in world coordinates

// swmp.s16 frames at their world top left: 0 the old tube's front piece,
// 1-3 the scaffold's front pieces (SCENERY in tube_walk.py)
struct Scenery {
	int frame, x, y;
};
inline const std::vector<Scenery> SCENERY = {{0, 4735, 1410}, {1, 4933, 1556}, {2, 5042, 1467}, {3, 5212, 1555}};
// SCENERY[from:to] (Python's slice: to < 0 means all)
std::vector<Scenery> sceneryFrom(int from, int to = -1);

// the old tube
constexpr int TX = 4735, TY = 1410; // its front piece (swmp frame 0)
constexpr int X0 = 4780, X1 = 6030; // its span (dock side to cap)
constexpr int SH = 170; // rows of the straightened tube
constexpr int LAB_FLOOR = 1798;

// the old tube's top edge (world y at world x); Python's top_of, which is
// only ever called with an int x there (the int arithmetic is exact in double)
inline double top_of(double x) {
	return 1438 + (x - 4835) * (1496 - 1438) / double(5935 - 4835);
}

// the new tube
constexpr int WD = 110; // the lab widened this much into the rock
constexpr int EXIT_X = 6113 - WD; // the lab's new west edge
constexpr int END_X = 5965; // where the tube's end lies, inside the rock
constexpr int FLOOR_OFF = 134; // the walking line: 2 px above the glass bottom
constexpr int ENTRY = 5928; // where the tube meets the rock
inline const double start_top = top_of(X0);
inline const double slope = (LAB_FLOOR - FLOOR_OFF - start_top) / (EXIT_X + 20 - X0);
// math.degrees(math.atan(slope)): CPython's x * (180 / pi)
inline const double ang = std::atan(slope) * (180.0 / 3.14159265358979323846);

inline double tube_y(double x) {
	return start_top + slope * (x - X0);
}

// new_tube's length: math.hypot(END_X - X0, start_top + slope * (END_X - X0) - start_top)
double tube_length();

// the owner's circles round the branches that crossed the old tube (world
// points; BRANCHES in tube_walk.py)
using Poly = std::vector<std::pair<int, int>>;
extern const std::vector<Poly> BRANCHES;

// ---- the scene ----------------------------------------------------------------

// make()'s inputs from the game
struct GameInputs {
	ImgF bg; // region(OX, OY, OX + W, OY + H), RGB (H, W, 3)
	ImgF bgt; // region(OX, OY - 300, OX + W, OY + H), RGB (H + 300, W, 3)
	std::vector<ImgF> swmp; // swmp.s16's frames, RGBA (h, w, 4)
	Mask ta; // np.asarray(swmp_im[0])[..., 3] > 0
};
// read from the player's game (backgroundRegion, gameSprite); false if the
// background or swmp.s16 (4 frames) can't be read
bool readInputs(GameInputs& in);
// the same from pictures already read: the two background regions (as
// backgroundRegion gives them) and swmp.s16's frames (as gameSprite)
GameInputs inputsFrom(const Rgba& bgRegion, const Rgba& bgtRegion, const std::vector<Rgba>& swmp);

// put_scenery(out, swmp, frames, cut): each frame composited at its place
// (reg * (1 - a) + f * a, a = alpha / 255, float32), less cut (a world-sized
// Mask, H x W) when given. Throws if a frame does not fit in out.
void put_scenery(ImgF& out, const std::vector<ImgF>& swmp, const std::vector<Scenery>& frames, const Mask* cut = nullptr);

// make()'s scene: tubecomp = bg with the old tube's front piece over it
// (SCENERY[:1]); scene = tubecomp with the scaffold's pieces (SCENERY[1:])
struct Scene {
	ImgF tubecomp, scene;
};
Scene makeScene(const GameInputs& in);

// old_tube(tubecomp, ta) -> (mask, strip): the old tube's outline (H x W)
// and the tube straightened, (SH, X1 - X0 + 1, 4), alpha 255 inside mask
void old_tube(const ImgF& tubecomp, const Mask& ta, Mask& mask, ImgF& strip);

// new_tube(strip) -> (MOD, pc): the clean module (strip columns 5630-5930,
// (SH, 300, 4), its warm specks taken out) and the new tube's piece, the
// strip's first 20 columns then MOD repeated, int(tube_length()) wide
void new_tube(const ImgF& strip, ImgF& MOD, ImgF& pc);

// what paint_out's sub-steps returned, for the proof (optional)
struct PaintSteps {
	ImgF quilt; // quilt_fill's result
	Mask band; // the band handed to tone_correct and match_texture
	ImgF toned; // tone_correct's
	ImgF textured; // match_texture's
	std::vector<double> energies; // quilt_fill's searches: every np.argsort input, in order
	std::vector<int64_t> energyCounts; // the length of each
};

// paint_out(scene, bg, bgt, mask, strip) -> base: the old tube painted out
// (quilt_fill, tone_correct, match_texture, restore_branches), (H, W, 3)
ImgF paint_out(const ImgF& scene, const ImgF& bg, const ImgF& bgt, const Mask& mask, const ImgF& strip, PaintSteps* steps = nullptr);

// ---- tube_walk_lib.py ---------------------------------------------------------

using TopOf = std::function<double(double)>;

// quilt_fill(img, OX, OY, top_of, xs0, xs1, bgt, OYT, seed): the band
// [top_of(x) - 8, top_of(x) + 160) for x in [xs0, xs1) filled with patches
// from just above it (image quilting in the straightened frame; numpy's
// default_rng(seed) picks among each search's three best). energies /
// counts, when given, receive every search's energies (np.argsort's input).
// argsort: numpy's quicksort is not stable (on the reference machine it is
// x86-simd-sort's AVX-512 argsort), so tied energies would order as numpy's
// implementation happens to; here ties go to the lower index. The game's
// real picture has no ties among any search's best three (the check
// reports this with --ties), so the result is the same.
ImgF quilt_fill(const ImgF& img, int ox, int oy, const TopOf& top_of, int xs0, int xs1, const ImgF& bgt, int oyt, uint64_t seed = 3,
	std::vector<double>* energies = nullptr, std::vector<int64_t>* counts = nullptr);

// tone_correct(res, bg, band, sigma): each band column's broad light and
// shade replaced by a smooth gradient from the tones above it to below it
ImgF tone_correct(const ImgF& res, const ImgF& bg, const Mask& band, double sigma = 9);

// match_texture(res, bg, band, gain_max): the fill's fine texture raised to
// the strength of the bark above it, column by column
ImgF match_texture(const ImgF& res, const ImgF& bg, const Mask& band, double gain_max = 2.6);

// restore_branches(img, scene, strip_mod, top_of, OX, OY, polys, SH):
// inside the polygons (world points), the scene's pixels where they differ
// from clean glass (strip_mod: the strip's columns 5630-5930, (SH, 300, 4))
ImgF restore_branches(const ImgF& img, const ImgF& scene, const ImgF& strip_mod, const TopOf& top_of, int ox, int oy,
	const std::vector<Poly>& polys, int sh = 170);

// developer aid behind OPENC2E_TW_CHECK_PAINT: run the module on the game in
// c2dir and write every intermediate to dir (art/tube_walk_check_paint.py)
int checkPaint(const std::string& dir, const std::string& c2dir);

} // namespace tubewalk
} // namespace MadeArt
