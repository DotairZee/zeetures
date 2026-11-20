#pragma once

// The walkable glass tube's made art, part: the recipe's assembly and its pictures ztwb, ztwf, ztwr, ztwg (art/tube_walk.py: make and the output layers).
// A pixel-exact port of the Python recipe (2026-11-19); see npcore.h for the
// numpy/Pillow operations it is built on.
//
// make() calls the parts in tube_walk_paint.h (the scene, the old tube
// painted out), tube_walk_lab.h (the gene lab's new west end) and
// tube_walk_parts.h (the tube's pieces, the stalactites, the rock) in the
// recipe's order, with everything between and around them here. The four
// pictures come from one make(), so it runs once and its pictures are kept
// (tube_walk.cpp registers ztwb, ztwf, ztwr and ztwg with MadeArt).
//
// Proof: OPENC2E_DUMP_MADE_ART=DIR with the engine, then
// `python3 art/compare_made.py DIR USER/Images ztwb ztwf ztwr ztwg` against
// `python3 art/tube_walk.py C2_DIR USER` (max difference 0, every channel).

#include "npcore.h"

#include <vector>

namespace MadeArt {
namespace tubewalk {

// the recipe's four pictures, RGBA, alpha 0 or 255, each as its PNG
// (ztwb, ztwf, ztwr: the scene cropped to their masks; ztwg: genc.s16 frame
// 0 less its west end); empty if the game's pictures can't be read
struct Pictures {
	Rgba ztwb, ztwf, ztwr, ztwg;
	bool ok = false;
};
Pictures makePictures();

} // namespace tubewalk
} // namespace MadeArt
