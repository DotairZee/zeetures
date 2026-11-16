#pragma once

// Art made on the player's machine from their own copy of Creatures 2
// (owner, 2026-11-15: the world additions are essential in every default
// world). Some of Zeetures's additions need pictures cut from the game's
// own (a stretch of background drawn again in front of creatures, a door
// recoloured, rock shaded into a passage). Zeetures cannot ship those, so
// the engine makes them when a script first asks for one by name, from the
// game's Back.s16 and sprites: C++ ports of the Python recipes in the
// repository's art/ folder (each port says which, and keeps its numbers).
//
// A picture of the same name in the player's own folders wins (getImage
// looks there first), so a player who made them with the Python recipes
// keeps theirs.

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

class creaturesImage;

namespace MadeArt {

// an RGBA picture, rows top to bottom, 4 bytes a pixel
struct Rgba {
	int w = 0, h = 0;
	std::vector<uint8_t> px;
	Rgba() = default;
	Rgba(int w_, int h_)
		: w(w_), h(h_), px(size_t(w_) * h_ * 4, 0) {}
	uint8_t* at(int x, int y) { return &px[(size_t(y) * w + x) * 4]; }
	const uint8_t* at(int x, int y) const { return &px[(size_t(y) * w + x) * 4]; }
};

// float planes, for recipes that work in floats as numpy does
struct Plane {
	int w = 0, h = 0;
	std::vector<float> v;
	Plane() = default;
	Plane(int w_, int h_, float fill = 0)
		: w(w_), h(h_), v(size_t(w_) * h_, fill) {}
	float& operator()(int x, int y) { return v[size_t(y) * w + x]; }
	float operator()(int x, int y) const { return v[size_t(y) * w + x]; }
};

// the game's background (Images/Back.s16: 58 x 16 tiles of 144 x 150,
// column by column), x0..x1 and y0..y1 exclusive, opaque; as art/
// background.py region(). Empty if the game's background can't be read.
Rgba backgroundRegion(int x0, int y0, int x1, int y1);

// a sprite of the game's own (Images/<name>.s16, the game's folder only):
// every frame, transparent where the file has 0; as art/s16.py read().
// Empty if missing.
std::vector<Rgba> gameSprite(const std::string& name);

// a recipe: the frames of one picture, or none if it can't be made
using Maker = std::function<std::vector<Rgba>()>;

// recipes register themselves (madeart/*.cpp: a static Register).
// replacesGame: the picture takes the place of the game's own sprite of the
// same name (as a file in the user data folder's Overrides/Images would; one
// there still wins), e.g. wfal, the waterfall without its rock
struct Register {
	Register(const std::string& name, Maker maker, bool replacesGame = false);
};

// whether a made picture takes the place of the game's own of this name
// (and the player has no Overrides/Images file for it)
bool replacesGame(const std::string& name);

// whether a picture of this name can be made here (C2, a recipe for it)
bool canMake(const std::string& name);

// the picture, made once and kept; null if it can't be made
std::shared_ptr<creaturesImage> make(const std::string& name);

// the frames, made fresh (tests: OPENC2E_DUMP_MADE_ART)
std::vector<Rgba> frames(const std::string& name);

// every recipe's name
std::vector<std::string> names();

// developer aid: OPENC2E_DUMP_MADE_ART=DIR writes every made picture to DIR
// as NAME.rgba (a header line "W H FRAMES", then the RGBA bytes, frames one
// after another), for comparing with the Python recipes' output
void dumpIfAsked();

} // namespace MadeArt
