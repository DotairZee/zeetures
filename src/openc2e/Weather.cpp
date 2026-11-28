/*
 *  Weather.cpp
 *  openc2e (Zeetures)
 *
 *  Zeetures' own weather: see Weather.h. Everything here is drawn by the
 *  engine from code (clouds, raindrops, snowflakes, hailstones, fog, dust,
 *  leaves, lightning); no picture of the game's is used.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 */

#include "Weather.h"

#include "Agent.h"
#include "Bubble.h"
#include "Camera.h"
#include "PointerAgent.h"
#include "creatures/Creature.h"
#include "creatures/CreatureAgent.h"
#include "historyManager.h"
#include "Engine.h"
#include "Map.h"
#include "MetaRoom.h"
#include "Room.h"
#include "SkyTint.h"
#include "SoundManager.h"
#include "World.h"
#include "caosValue.h"
#include "common/Random.h"
#include "common/backend/Backend.h"
#include "common/creaturesImage.h"
#include "common/devenv.h"
#include "creatures/CreatureSpeech.h"
#include "creatures/Lightning.h"
#include "creatures/Pheromones.h"
#include "fileformats/ImageUtils.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fmt/core.h>
#include <map>
#include <set>
#include <ostream>
#include <vector>

namespace Weather {

namespace {

// ---- the spells -------------------------------------------------------------

const char* const NAMES[SPELL_COUNT] = {"Clear skies", "Light showers", "Showers", "Thunderstorm", "Torrential rain", "Fog", "Snow",
	"Heatwave", "Wind storm"};

// the odds of each spell by season (spring, summer, autumn, winter), in
// percent; the proposal's table, with showers split into light and full,
// and a wind storm now and then
const int ODDS[4][SPELL_COUNT] = {
	{28, 17, 18, 15, 10, 7, 0, 0, 5},
	{40, 8, 7, 20, 5, 0, 0, 15, 5},
	{23, 17, 18, 15, 15, 7, 0, 0, 5},
	{27, 10, 10, 5, 5, 13, 25, 0, 5},
};

// how much cloud each spell brings (owner, 2026-11-27: milder weather,
// thinner and paler clouds, never past "mostly solid", 0.95, the
// thunderstorm's)
float cloudFor(int s, bool withHail) {
	switch (s) {
		case LIGHT_SHOWERS: return 0.40f;
		case SHOWERS: return 0.55f;
		case THUNDERSTORM: return withHail ? 0.85f : 0.95f;
		case TORRENTIAL: return 0.80f;
		case FOG: return 0.45f;
		case SNOW: return 0.60f;
		case WINDSTORM: return 0.30f;
		default: return 0.0f;
	}
}

int s_on = -1;
int s_spell = CLEAR;
bool s_hail = false;
unsigned int s_until = 0; // world tick (worldtickcount) the spell ends
// how far in the spell is (0 to 1, eased in at its start and out at its
// end, so the clouds and rain come and go)
float s_mix = 0.0f;
int s_next = -1; // a spell waiting for the last to fade out
bool s_suppressed = false; // the game's own weather held off

const unsigned int TICKS_PER_DAY = 3600 * 5; // (the Kits' day: five parts)

int pickSpell(bool& withHail) {
	int season = std::clamp((int)world.season, 0, 3);
	int r = (int)rand_uint32(0, 99), acc = 0;
	int s = CLEAR;
	for (int i = 0; i < SPELL_COUNT; i++) {
		acc += ODDS[season][i];
		if (r < acc) {
			s = i;
			break;
		}
	}
	withHail = s == THUNDERSTORM && season == 1 && rand_float(0, 1) < 0.35f;
	return s;
}

void begin(int s, bool withHail, unsigned int length) {
	s_spell = s;
	s_hail = withHail;
	s_until = world.worldtickcount + length;
	s_mix = 0.0f;
	fmt::print("* Weather: {}{} for {} ticks\n", NAMES[s], withHail ? " (with hail)" : "", length);
}

unsigned int spellLength(int s) {
	// the fierce ones (a wind storm, a thunderstorm, torrential rain) blow
	// over in a sixth of a day to half a day, about 5 to 15 minutes; the rest
	// last half a day to a day and a half (owner, 2026-11-27: a wind storm
	// lasting the whole 45-minute evening was "nothing but dust storms")
	if (s == WINDSTORM || s == THUNDERSTORM || s == TORRENTIAL)
		return (unsigned int)rand_uint32(TICKS_PER_DAY / 6, TICKS_PER_DAY / 2);
	return (unsigned int)rand_uint32(TICKS_PER_DAY / 2, TICKS_PER_DAY * 3 / 2);
}

// ---- where weather can be (worked out once per background) -------------------

struct Bits {
	int W = 0, H = 0;
	std::vector<uint64_t> b;
	void init(int w, int h) {
		W = w;
		H = h;
		b.assign((size_t(w) * h + 63) / 64, 0);
	}
	bool get(int x, int y) const {
		if (W == 0 || y < 0 || y >= H)
			return false;
		x %= W;
		if (x < 0)
			x += W;
		size_t i = size_t(y) * W + x;
		return (b[i >> 6] >> (i & 63)) & 1;
	}
	void set(int x, int y, bool v) {
		if (x < 0 || x >= W || y < 0 || y >= H)
			return;
		size_t i = size_t(y) * W + x;
		if (v)
			b[i >> 6] |= uint64_t(1) << (i & 63);
		else
			b[i >> 6] &= ~(uint64_t(1) << (i & 63));
	}
};

enum SurfaceKind : uint8_t { NONE = 0, FLOOR = 1, ROOF = 2, WATER = 3, ROCK = 4 };

struct Places {
	bool built = false;
	int W = 0, H = 0, mx = 0, my = 0;
	bool eden = false;
	std::vector<float> s0; // per column: the first surface falling things meet
	std::vector<uint8_t> s0kind;
	Bits precip; // where rain, hail and snow can be
	Bits wind; // where the wind blows
	std::vector<int> hazeBottom; // per column: the snow haze down to here
	std::vector<int> hazeFade; // per column: where it starts fading out (dithered) toward hazeBottom
	std::vector<uint8_t> lum; // the background's brightness (for the planks test)
};
std::map<std::string, Places> s_places;

float bottomAt(const Room& r, float x) {
	float t = (x - (float)r.x_left) / std::max(1.0f, (float)r.x_right - (float)r.x_left);
	return (float)r.y_left_floor + ((float)r.y_right_floor - (float)r.y_left_floor) * t;
}
float ceilAt(const Room& r, float x) {
	float t = (x - (float)r.x_left) / std::max(1.0f, (float)r.x_right - (float)r.x_left);
	return (float)r.y_left_ceiling + ((float)r.y_right_ceiling - (float)r.y_left_ceiling) * t;
}
// the floor line: the floor points (x from the left wall, height above the
// room's bottom), else the straight floor
float floorAt(const Room& r, float x) {
	if (r.floorpoints.empty())
		return bottomAt(r, x);
	float bottom = (float)std::max(r.y_left_floor, r.y_right_floor);
	std::vector<std::pair<float, float>> pts;
	for (auto& [dx, h] : r.floorpoints)
		pts.push_back({(float)r.x_left + (float)dx, bottom - (float)h});
	std::sort(pts.begin(), pts.end());
	if (x <= pts.front().first)
		return pts.front().second;
	for (size_t i = 1; i < pts.size(); i++)
		if (x <= pts[i].first) {
			auto [x0, y0] = pts[i - 1];
			auto [x1, y1] = pts[i];
			return y0 + (y1 - y0) * (x - x0) / std::max(1.0f, x1 - x0);
		}
	return pts.back().second;
}

bool pointIn(const std::vector<std::pair<int, int>>& poly, float x, float y) {
	bool in = false;
	for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
		float xi = (float)poly[i].first, yi = (float)poly[i].second, xj = (float)poly[j].first, yj = (float)poly[j].second;
		if ((yi > y) != (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi)
			in = !in;
	}
	return in;
}
float through(const std::vector<std::pair<int, int>>& pts, float x) {
	if (x <= pts.front().first)
		return (float)pts.front().second;
	for (size_t i = 1; i < pts.size(); i++)
		if (x <= pts[i].first) {
			float x0 = (float)pts[i - 1].first, y0 = (float)pts[i - 1].second, x1 = (float)pts[i].first, y1 = (float)pts[i].second;
			return y0 + (y1 - y0) * (x - x0) / std::max(1.0f, x1 - x0);
		}
	return (float)pts.back().second;
}

void build(Places& P, MetaRoom* m) {
	P.built = true;
	auto bkgd = m->getCurrentBackground();
	if (!bkgd || bkgd->images.empty() || !bkgd->images[0])
		return;
	auto t0 = std::chrono::steady_clock::now();
	Image rgb = ImageUtils::ToRGB24(bkgd->images[0]);
	const int W = rgb.width, H = rgb.height;
	P.W = W;
	P.H = H;
	P.mx = (int)m->x();
	P.my = (int)m->y();
	const uint8_t* px = rgb.data.data();
	P.lum.resize(size_t(W) * H);
	for (size_t i = 0; i < P.lum.size(); i++)
		P.lum[i] = (uint8_t)((px[i * 3] + px[i * 3 + 1] + px[i * 3 + 2]) / 3);
	auto skyAt = [&](int x, int y) {
		if (x < 0 || x >= W || y < 0 || y >= H)
			return false;
		const uint8_t* p = px + (size_t(y) * W + x) * 3;
		return (int)p[2] - (int)p[0] > 18 && P.lum[size_t(y) * W + x] > 100;
	};
	// Eden's map (the outlines traced from its background apply)
	P.eden = W == 8352 && H == 2400;

	// the rooms of this metaroom, in its own terms (from its corner)
	std::vector<std::shared_ptr<Room>> rooms;
	for (auto& r : world.map->getRooms())
		if (r && r->x_right > (unsigned)P.mx && r->x_left < (unsigned)(P.mx + W) && r->y_left_floor > (unsigned)P.my)
			rooms.push_back(r);
	std::vector<std::vector<Room*>> cols(W);
	for (auto& r : rooms)
		for (int x = std::max(0, (int)r->x_left - P.mx); x <= std::min(W - 1, (int)r->x_right - P.mx); x++)
			cols[x].push_back(r.get());
	std::map<Room*, std::shared_ptr<Room>> shared;
	for (auto& r : rooms)
		shared[r.get()] = r;
	auto doorPerm = [&](Room* a, Room* b) {
		auto& ra = shared[a];
		auto& rb = shared[b];
		return ra && rb && world.map->hasDoor(ra, rb) ? std::max(0, world.map->getDoorPerm(ra, rb)) : 0;
	};

	// walkways the picture test misses, traced by eye (Eden): room id ->
	// the planks' x range
	std::map<unsigned int, std::pair<int, int>> walkways;
	if (P.eden)
		walkways = {{218, {1240, 1553}}, {193, {1553, 2140}}, {4, {2146, 2480}}, {6, {3077, 3768}}};
	// the planks test: along an open floor (one a room below opens into),
	// something solid shows on the floor line where the brightness steps
	// there, more than just above or below it
	std::map<Room*, std::vector<bool>> runs;
	auto structureRun = [&](Room* r) -> std::vector<bool>& {
		auto it = runs.find(r);
		if (it != runs.end())
			return it->second;
		int xl = std::max(0, (int)r->x_left - P.mx), xr = std::min(W - 1, (int)r->x_right - P.mx);
		int n = xr - xl + 1;
		std::vector<float> e(std::max(0, n), 0.0f);
		for (int i = 0; i < n; i++) {
			int x = xl + i, y = (int)floorAt(*r, (float)(x + P.mx)) - P.my;
			if (y < 30 || y >= H - 32 || skyAt(x, y + 4))
				continue;
			auto g = [&](int a, int b) {
				float mx_ = 0;
				for (int yy = y - 28 + a; yy < y - 28 + b; yy++)
					mx_ = std::max(mx_, (float)std::abs((int)P.lum[size_t(yy + 1) * W + x] - (int)P.lum[size_t(yy) * W + x]));
				return mx_;
			};
			float line = g(25, 42);
			float c0 = g(0, 10), c1 = g(10, 20), c2 = g(46, 56);
			float ctrl = std::max(std::min(c0, c1), std::min(std::max(c0, c1), c2)); // (the median of three)
			e[i] = (line >= 20 && line >= 2.2f * ctrl) ? line : 0.0f;
		}
		std::vector<bool> ok(std::max(0, n), false);
		const int k = 15;
		for (int i = 0; i < n; i++) {
			std::vector<float> win;
			for (int j = i - k / 2; j <= i + k / 2; j++)
				win.push_back(e[std::clamp(j, 0, n - 1)]);
			std::nth_element(win.begin(), win.begin() + win.size() / 2, win.end());
			ok[i] = win[win.size() / 2] >= 20;
		}
		for (int i = 0; i < n;) { // (runs under 30 px dropped)
			if (!ok[i]) {
				i++;
				continue;
			}
			int j = i;
			while (j < n && ok[j])
				j++;
			if (j - i < 30)
				for (int q = i; q < j; q++)
					ok[q] = false;
			i = j;
		}
		std::vector<bool> out(std::max(0, n), false);
		if (auto w = walkways.find(r->id); w != walkways.end()) {
			for (int i = 0; i < n; i++)
				out[i] = xl + i >= w->second.first && xl + i <= w->second.second;
		} else {
			int count = 0, first = -1, last = -1;
			for (int i = 0; i < n; i++)
				if (ok[i]) {
					count++;
					if (first < 0)
						first = i;
					last = i;
				}
			// shown along most of its run: from its first plank to its last
			if (n > 0 && count >= 0.35f * n)
				for (int i = first; i <= last; i++)
					out[i] = true;
		}
		return runs[r] = out;
	};

	// every surface in each column, and the first from the top
	std::vector<std::vector<std::pair<float, uint8_t>>> cand(W);
	for (int x = 0; x < W; x++) {
		const float wx = (float)(x + P.mx);
		for (Room* r : cols[x]) {
			if (r->type == 1 || r->type == 3) {
				float yb = bottomAt(*r, wx), fy = floorAt(*r, wx);
				bool opened = false;
				for (Room* q : cols[x])
					if (q != r && std::fabs(ceilAt(*q, wx) - yb) <= 3 && std::max(doorPerm(r, q), doorPerm(q, r)) >= 50)
						opened = true;
				if (opened) {
					auto& run = structureRun(r);
					int i = x - std::max(0, (int)r->x_left - P.mx);
					if (i < 0 || i >= (int)run.size() || !run[i])
						continue;
				}
				cand[x].push_back({fy - (float)P.my, FLOOR});
			} else if (r->type == 0) {
				cand[x].push_back({ceilAt(*r, wx) - (float)P.my, ROOF});
			} else if (r->type == 2) {
				cand[x].push_back({ceilAt(*r, wx) - (float)P.my, WATER});
			}
		}
	}
	std::vector<std::pair<int, int>> spire, mound, coneR, rimInner, openPoly, rockSpire, rockCone;
	// the curved walkway with the teeth: the floor line creatures and objects
	// stand on (room 143's), where rain lands (owner, 2026-11-27: "make it
	// so they hit where the plane on which creatures and objects exist is";
	// a traced line stopped them short of it); the trace only if that room
	// is missing
	Room* walkRoom = nullptr;
	if (P.eden)
		for (auto& r : rooms)
			if (r->id == 143 && r->x_left <= 7600 && r->x_right >= 8000)
				walkRoom = r.get();
	auto walkY = [&](float x) {
		if (walkRoom)
			return floorAt(*walkRoom, x + (float)P.mx) - (float)P.my;
		static const std::vector<std::pair<int, int>> w = {{7450, 702}, {7600, 690}, {7800, 662}, {7950, 628}, {8010, 615}};
		return through(w, x);
	};
	if (P.eden) {
		// the seas' surfaces as drawn
		for (auto [a, b, y] : {std::array<int, 3>{360, 986, 678}, std::array<int, 3>{6538, 7098, 690}})
			for (int x = a; x <= b && x < W; x++) {
				auto& c = cand[x];
				c.erase(std::remove_if(c.begin(), c.end(), [&](auto& p) { return p.first <= y; }), c.end());
				c.push_back({(float)y, WATER});
			}
		// the volcano (the owner's outline, 2026-11-27): open to the sky from
		// the leaning spire's inner face, along the curved walkway with the
		// teeth, up the inside of the cone's right wall to its peak
		spire = {{7385, 660}, {7417, 600}, {7450, 533}, {7483, 467}, {7515, 410}, {7537, 375}, {7587, 377}};
		mound = {{7590, 540}, {7637, 527}, {7683, 530}, {7723, 560}, {7777, 563}, {7804, 553}};
		coneR = {{7940, 160}, {7960, 165}, {7985, 220}, {8015, 300}, {8050, 400}, {8095, 500}, {8140, 600}, {8165, 650}};
		rimInner = {{7945, 170}, {7960, 270}, {7980, 370}, {7995, 470}, {8000, 570}, {8005, 615}};
		for (auto* pts : {&spire, &mound, &coneR})
			for (int x = pts->front().first; x <= pts->back().first && x < W; x++)
				cand[x].push_back({through(*pts, (float)x), ROCK});
		openPoly = {{7587, 377}, {7590, 500}, {7585, 600}, {7580, (int)walkY(7580)}};
		for (int x = 7590; x < 8006; x += 8)
			openPoly.push_back({x, (int)walkY((float)x)});
		for (auto it = rimInner.rbegin(); it != rimInner.rend(); ++it)
			openPoly.push_back(*it);
		openPoly.push_back({7940, 160});
		rockSpire = spire;
		for (auto p : {std::pair<int, int>{7590, 500}, {7585, 600}, {7580, 690}, {7560, 705}, {7385, 705}})
			rockSpire.push_back(p);
		rockCone = coneR;
		rockCone.push_back({8180, 700});
		rockCone.push_back({8005, 700});
		for (auto it = rimInner.rbegin(); it != rimInner.rend(); ++it)
			rockCone.push_back(*it);
	}
	P.s0.assign(W, (float)H);
	P.s0kind.assign(W, NONE);
	for (int x = 0; x < W; x++)
		if (!cand[x].empty()) {
			auto best = *std::min_element(cand[x].begin(), cand[x].end());
			P.s0[x] = best.first;
			P.s0kind[x] = best.second;
		}
	// the sweep for holes: a short run of surface standing high above its
	// neighbours (a stray bit of floor up in a tree's crown) gives way to the
	// next surface down
	for (int pass = 0; pass < 4; pass++) {
		bool found = false;
		for (int x = 0; x < W;) {
			int j = x;
			while (j + 1 < W && std::fabs(P.s0[j + 1] - P.s0[j]) < 4)
				j++;
			if (j - x < 90) {
				float l = x > 0 ? P.s0[x - 1] : (float)H, r = j + 1 < W ? P.s0[j + 1] : (float)H;
				if (P.s0[x] < std::min(l, r) - 60 && P.s0kind[x] != ROCK) {
					for (int c = x; c <= j; c++) {
						float best = (float)H;
						uint8_t kind = NONE;
						for (auto& [y, k] : cand[c])
							if (y > P.s0[c] + 3 && y < best) {
								best = y;
								kind = k;
							}
						P.s0[c] = best;
						P.s0kind[c] = kind;
					}
					found = true;
				}
			}
			x = j + 1;
		}
		if (!found)
			break;
	}
	// where rain, hail and snow can be: open air above the first surface,
	// and the volcano's crater; not its rock
	P.precip.init(W, H);
	for (int x = 0; x < W; x++)
		for (int y = 0; y < std::min(H, (int)P.s0[x]); y++)
			P.precip.set(x, y, true);
	// where the wind blows: open outdoor air, under bridges and platforms
	// too (their planks are thin), and the spaces under the dead tree's
	// platform and the long rope bridge, which the map marks indoor (Eden)
	P.wind.init(W, H);
	for (int x = 0; x < W; x++)
		for (int y = 0; y < std::min(H, (int)P.s0[x]); y++)
			P.wind.set(x, y, true);
	// (room 63, under the long rope bridge, is open air for the snow haze;
	// the wind is kept above the bridge's walking line further down)
	const std::vector<unsigned int> underWalkways = P.eden ? std::vector<unsigned int>{12, 63} : std::vector<unsigned int>{};
	const std::vector<unsigned int> bridges = P.eden ? std::vector<unsigned int>{4, 5, 6, 138, 193, 218} : std::vector<unsigned int>{};
	for (auto& rr : rooms) {
		Room* r = rr.get();
		bool under = std::find(underWalkways.begin(), underWalkways.end(), r->id) != underWalkways.end();
		if (r->type != 1 && r->type != 3 && !under)
			continue;
		bool bridge = std::find(bridges.begin(), bridges.end(), r->id) != bridges.end();
		for (int x = std::max(0, (int)r->x_left - P.mx); x <= std::min(W - 1, (int)r->x_right - P.mx); x++) {
			float wx = (float)(x + P.mx), fy = floorAt(*r, wx), yb = bottomAt(*r, wx);
			float lower = (bridge || yb - fy < 16) ? yb : fy;
			for (int y = std::max(0, (int)ceilAt(*r, wx) - P.my); y < std::min(H, (int)lower - P.my); y++)
				P.wind.set(x, y, true);
		}
	}
	if (P.eden) {
		for (int x = 7380; x <= 8180 && x < W; x++)
			for (int y = 150; y <= 710 && y < H; y++) {
				bool open = pointIn(openPoly, (float)x, (float)y);
				bool rock = !open && (pointIn(rockSpire, (float)x, (float)y) || pointIn(rockCone, (float)x, (float)y));
				// (the walkway catches what falls in the crater)
				bool belowWalk = x >= 7580 && x <= 8005 && (float)y >= walkY((float)x);
				P.precip.set(x, y, (open && !belowWalk) || (!open && !rock && !belowWalk && P.precip.get(x, y)));
				if (rock)
					P.precip.set(x, y, false);
				P.wind.set(x, y, (open && !belowWalk) || (!rock && P.wind.get(x, y)));
			}
	}
	// the snow haze, per column: through the open air from the top, under
	// platforms and bridges too (gaps of a plank's thickness passed), down
	// to the true ground and 40 px into its face; not under a roof
	P.hazeBottom.assign(W, 0);
	P.hazeFade.assign(W, 0);
	// (Eden: the valley under the long rope bridge, from the big waterfall to
	// the incubator's wall, is open air down to its floor, though the map
	// marks some of it indoor and has thick floor lines across it; owner,
	// 2026-11-27, circling where the haze stopped short)
	const int valleyX0 = 2650, valleyX1 = 3900, valleyFloor = 1747;
	for (int x = 0; x < W; x++) {
		int last = -1, gap = 0;
		const bool valley = P.eden && x + P.mx >= valleyX0 && x + P.mx <= valleyX1;
		for (int y = 0; y < H; y++) {
			if (P.s0kind[x] == ROOF && y >= (int)P.s0[x])
				break;
			if (P.wind.get(x, y) || P.precip.get(x, y)) {
				last = y;
				gap = 0;
			} else if (valley && y + P.my < valleyFloor)
				gap = 0;
			else if (++gap > 24)
				break;
		}
		// (fading out over 70 px from the last open air, into the ground's face:
		// owner, 2026-11-27, "dither and fade" rather than cut off)
		P.hazeFade[x] = last < 0 ? 0 : last - 10;
		P.hazeBottom[x] = last < 0 ? 0 : std::min(H, last + 60);
		// the wind (dust, leaves and fog) only in that same air: open from
		// the sky down through bridges and platforms to the true ground,
		// never below it in caves and rooms the map calls outdoors (owner,
		// 2026-11-27: "I don't want it below the surface")
		for (int y = std::max(0, last + 1); y < H; y++)
			P.wind.set(x, y, false);
		// (nor east of the valley under the long rope bridge, by the stone
		// arches: the owner's line the snow haze stops at too, below)
		if (P.eden && x + P.mx > 3700 && x + P.mx < 4300)
			for (int y = std::max(0, 845 - P.my); y < std::min(H, 1770 - P.my); y++) {
				float xc = 3901.0f + (3834.0f - 3901.0f) * (y + P.my - 860) / 670.0f;
				if (x + P.mx > xc)
					P.wind.set(x, y, false);
			}
	}
	// Eden's volcano (owner, 2026-11-27, circling it): the haze covers the
	// rims' rock from their tops down to the walks beside them (the bridge
	// level west, y 712; the beach east, y 600), and in the crater it ends
	// at the walkway creatures walk on, not deep in the rock below it
	if (P.eden) {
		for (int x = std::max(0, 7380 - P.mx); x <= std::min(W - 1, 8230 - P.mx); x++) {
			const int wx = x + P.mx;
			int& hb = P.hazeBottom[x];
			int& hf = P.hazeFade[x];
			if (wx >= 7580 && wx <= 8005) {
				hb = (int)walkY((float)wx) - P.my + 4;
				hf = hb - 50;
			} else {
				hb = std::max(hb, (wx < 7580 ? 712 : 600) - P.my);
				hf = std::max(hf, hb - 60);
			}
		}
	}
	// the reach smoothed along the map, so the haze shows no seams: a median
	// over 25 columns takes out narrow notches (where room shapes leave a
	// gap under a bridge, the haze stopped just under its deck for a few
	// columns: a dark line down the valley), then an average over 41 turns
	// real steps (a cliff beside open air) into short slopes rather than
	// vertical edges (owner, 2026-11-27: "watch for patchiness")
	{
		auto smooth = [&](std::vector<int>& v) {
			std::vector<int> med(W), out(W);
			std::vector<int> win;
			for (int x = 0; x < W; x++) {
				win.clear();
				for (int k = -12; k <= 12; k++)
					win.push_back(v[std::clamp(x + k, 0, W - 1)]);
				std::nth_element(win.begin(), win.begin() + win.size() / 2, win.end());
				med[x] = win[win.size() / 2];
			}
			for (int x = 0; x < W; x++) {
				long sum = 0;
				for (int k = -20; k <= 20; k++)
					sum += med[std::clamp(x + k, 0, W - 1)];
				out[x] = (int)(sum / 41);
			}
			v.swap(out);
		};
		smooth(P.hazeBottom);
		smooth(P.hazeFade);
	}
	// developer aid: OPENC2E_WEATHER_HAZE=x0,x1 prints each column's snow
	// haze reach there, with its first surface
	if (const char* e = devenv("OPENC2E_WEATHER_HAZE")) {
		int x0 = 0, x1 = 0;
		if (sscanf(e, "%d,%d", &x0, &x1) == 2)
			for (int x = std::max(0, x0); x <= std::min(W - 1, x1); x++)
				fmt::print("* Weather haze x {} reaches {} (fade from {}), first surface {} kind {}\n", x, P.hazeBottom[x], P.hazeFade[x], (int)P.s0[x], (int)P.s0kind[x]);
	}
	// the long rope bridge over the waterfall valley (rooms 4, 5 and 6): the
	// wind blows only above its walking line, not through the valley under
	// it (owner, 2026-11-27, "limited to above the walking area of the bridge");
	// after the snow haze is worked out, which still reaches the valley floor
	if (P.eden)
		for (auto& rr : rooms) {
			Room* r = rr.get();
			if (r->id != 4 && r->id != 5 && r->id != 6)
				continue;
			for (int x = std::max(0, (int)r->x_left - P.mx); x <= std::min(W - 1, (int)r->x_right - P.mx); x++)
				for (int y = std::max(0, (int)floorAt(*r, (float)(x + P.mx)) - P.my + 1); y < H; y++)
					P.wind.set(x, y, false);
		}
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
	fmt::print("* Weather: places worked out for {}x{} ({}) in {} ms\n", W, H, P.eden ? "Eden's map" : "another map", ms);
}

Places* placesFor(MetaRoom* m) {
	if (!m)
		return nullptr;
	auto bkgd = m->getCurrentBackground();
	if (!bkgd)
		return nullptr;
	Places& P = s_places[bkgd->getName()];
	if (!P.built)
		build(P, m);
	return P.W > 0 ? &P : nullptr;
}

// ---- pictures, all drawn here ------------------------------------------------

const float BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
inline bool dither(int x, int y, float a) {
	return a > (BAYER[y & 3][x & 3] + 0.5f) / 16.0f;
}

Image newImage(int w, int h) {
	Image im;
	im.width = w;
	im.height = h;
	im.format = if_rgba32;
	im.data = shared_array<uint8_t>(size_t(w) * h * 4);
	std::fill(im.data.data(), im.data.data() + size_t(w) * h * 4, 0);
	return im;
}
void put(Image& im, int x, int y, int r, int g, int b) {
	if (x < 0 || y < 0 || x >= im.width || y >= im.height)
		return;
	uint8_t* p = im.data.data() + (size_t(y) * im.width + x) * 4;
	p[0] = (uint8_t)std::clamp(r, 0, 255);
	p[1] = (uint8_t)std::clamp(g, 0, 255);
	p[2] = (uint8_t)std::clamp(b, 0, 255);
	p[3] = 255;
}
std::shared_ptr<creaturesImage> picture(const std::string& name, std::vector<Image> frames) {
	auto c = std::make_shared<creaturesImage>(name);
	c->images = std::move(frames);
	c->textures.resize(c->images.size());
	return c;
}

// a shaded ball (snowflakes, hailstones): lit from the upper left, its
// edge dithered
Image ball(int size, std::array<int, 3> lit, std::array<int, 3> shade) {
	Image im = newImage(size, size);
	float c = (size - 1) / 2.0f, R = size / 2.0f;
	for (int y = 0; y < size; y++)
		for (int x = 0; x < size; x++) {
			float dx = (x - c) / R, dy = (y - c) / R, d = std::sqrt(dx * dx + dy * dy);
			if (d > 1.05f || !dither(x, y, std::clamp((1.05f - d) * 4.0f, 0.0f, 1.0f)))
				continue;
			float l = std::clamp(0.55f - 0.45f * (dx + dy) * 0.7f, 0.0f, 1.0f);
			put(im, x, y, (int)(shade[0] + (lit[0] - shade[0]) * l), (int)(shade[1] + (lit[1] - shade[1]) * l), (int)(shade[2] + (lit[2] - shade[2]) * l));
		}
	return im;
}

// a little leaf blowing in the wind, two frames (turning)
Image leaf(int kind, int frame) {
	Image im = newImage(6, 4);
	std::array<int, 3> col = kind == 0 ? std::array<int, 3>{96, 140, 60} : kind == 1 ? std::array<int, 3>{150, 120, 60} : std::array<int, 3>{120, 90, 50};
	const char* rows[2][4] = {{".##...", "#####.", ".####.", "...#.."}, {"..##..", ".####.", ".####.", "..##.."}};
	for (int y = 0; y < 4; y++)
		for (int x = 0; x < 6; x++)
			if (rows[frame][y][x] == '#') {
				float l = y < 2 ? 1.15f : 0.8f;
				put(im, x, y, (int)(col[0] * l), (int)(col[1] * l), (int)(col[2] * l));
			}
	return im;
}

// smooth value noise
inline uint32_t hash2(int x, int y, uint32_t seed) {
	uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + seed * 2246822519u;
	h = (h ^ (h >> 13)) * 1274126177u;
	return h ^ (h >> 16);
}
inline float lattice(int x, int y, uint32_t seed) {
	return (hash2(x, y, seed) & 0xffff) / 65535.0f;
}
float vnoise(float x, float y, uint32_t seed, int wrapX = 0) {
	int xi = (int)std::floor(x), yi = (int)std::floor(y);
	float fx = x - xi, fy = y - yi;
	fx = fx * fx * (3 - 2 * fx);
	fy = fy * fy * (3 - 2 * fy);
	auto L = [&](int a, int b) {
		if (wrapX > 0) {
			a %= wrapX;
			if (a < 0)
				a += wrapX;
		}
		return lattice(a, b, seed);
	};
	float a = L(xi, yi), b = L(xi + 1, yi), c = L(xi, yi + 1), d = L(xi + 1, yi + 1);
	return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}

// value noise that repeats exactly every `period` px along x, with lattice
// cells of about `scale` px (stretched a little so a whole number of them
// fit): the cloud deck wraps around the world, and noise whose cells did
// not divide its length jumped where the ends met, a straight line drifting
// across the sky (owner, 2026-11-27: "the edge of the clouds is a hard line")
float pnoise(float px, float scale, float y, uint32_t seed, int period) {
	int cells = std::max(1, (int)std::lround(period / scale));
	return vnoise(px * cells / period, y, seed, cells);
}

// ---- clouds: flat, layered, torn decks (owner, 2026-11-27: not globular),
// across the sky on top of everything, in ordered dither ----------------------

const int DECK_H = 480; // the deck's height, from the top of the world
const int TILE = 512;
struct CloudTile {
	std::shared_ptr<creaturesImage> pic;
	int level = -1;
	unsigned int used = 0;
};
std::map<int, CloudTile> s_cloudTiles; // by deck tile index
float s_cloudShown = 0.0f; // the density drawn (eased toward the spell's)
float s_cloudDrift = 0.0f; // px the deck has drifted
unsigned int s_frame = 0;

// the deck's density at a point of the deck (its own x, wrapping), 0-1
float deckField(float dx, float y, int period) {
	// stretched along the horizontal, with a warp for torn edges and streaks
	float warp = (pnoise(dx, 260.0f, y / 90.0f, 7, period) - 0.5f) * 120.0f;
	float sx = dx + warp;
	float n = 0.55f * pnoise(sx, 420.0f, y / 70.0f, 11, period) + 0.28f * pnoise(sx, 170.0f, y / 30.0f, 12, period) +
			  0.17f * pnoise(sx, 60.0f, y / 13.0f, 13, period);
	// the deck from the top of the world down to a ragged underside about
	// 330 px down: over the sky, above the tall things along the surface
	// (lower, it hid the dead tree's trunk), thickest in its middle
	float under = 330.0f + (pnoise(sx, 150.0f, 3.0f, 21, period) - 0.5f) * 140.0f;
	float v = std::clamp((y + 30.0f) / 80.0f, 0.0f, 1.0f) * std::clamp((under - y) / 100.0f, 0.0f, 1.0f);
	return n * (0.45f + 0.55f * v) * std::clamp((under - y) / 40.0f, 0.0f, 1.0f);
}

// a tile's field and fine texture, worked out once (the costly part) and
// kept: a change in the weather's density only re-colours it
struct TileField {
	std::vector<float> f, tex;
};
std::map<int, TileField> s_fields;
const TileField& fieldFor(int tile, int period) {
	auto it = s_fields.find(tile);
	if (it != s_fields.end())
		return it->second;
	TileField tf;
	tf.f.resize(size_t(TILE) * DECK_H);
	tf.tex.resize(size_t(TILE) * DECK_H);
	for (int y = 0; y < DECK_H; y++)
		for (int x = 0; x < TILE; x++) {
			float dx = (float)(tile * TILE + x);
			tf.f[size_t(y) * TILE + x] = deckField(dx, (float)y, period);
			tf.tex[size_t(y) * TILE + x] = (pnoise(dx, 9.0f, y / 4.0f, 31, period) - 0.5f) * 14.0f;
		}
	return s_fields[tile] = std::move(tf);
}

std::shared_ptr<creaturesImage> makeCloudTile(int tile, float density, int period) {
	const TileField& tf = fieldFor(tile, period);
	Image im = newImage(TILE, DECK_H);
	// the density to clear: denser weather, more of the field is cloud
	const float cut = 0.56f - 0.46f * density;
	// tones: paler and lighter in mild weather, slate in storms
	const float dark = std::clamp((density - 0.35f) / 0.6f, 0.0f, 1.0f);
	for (int y = 0; y < DECK_H; y++)
		for (int x = 0; x < TILE; x++) {
			float f = tf.f[size_t(y) * TILE + x];
			float a = std::clamp((f - cut) / 0.22f, 0.0f, 1.0f) * std::min(1.0f, 0.5f + density * 0.55f);
			if (a <= 0 || !dither(x, y, a))
				continue;
			// lighter tops and edges, darker heavy bellies
			float belly = std::clamp((f - cut) / 0.35f, 0.0f, 1.0f) * std::clamp(y / 330.0f, 0.0f, 1.0f);
			float top[3] = {222 - 40 * dark, 226 - 38 * dark, 236 - 30 * dark};
			float bot[3] = {150 - 80 * dark, 156 - 80 * dark, 172 - 76 * dark};
			float t = std::clamp(0.25f + belly * 0.9f, 0.0f, 1.0f);
			float tex = tf.tex[size_t(y) * TILE + x];
			put(im, x, y, (int)(top[0] + (bot[0] - top[0]) * t + tex), (int)(top[1] + (bot[1] - top[1]) * t + tex), (int)(top[2] + (bot[2] - top[2]) * t + tex));
		}
	return picture("zeetures weather clouds", {im});
}

// ---- falling and blowing things -------------------------------------------------

enum Kind : uint8_t { RAIN, SNOWFLAKE, HAILSTONE, DUST, LEAF };
struct Particle {
	float x, y, vx, vy;
	uint8_t kind, look;
	float phase;
	int bounces;
};
std::vector<Particle> s_parts;
struct Splash {
	float x, y;
	float age;
	uint8_t kind; // 0 rain, 1 hail lying
};
std::vector<Splash> s_splashes;

std::shared_ptr<creaturesImage> s_flakes, s_stones, s_leaves, s_white, s_warm, s_glow;
std::vector<std::shared_ptr<creaturesImage>> s_fogBanks;

void makePictures() {
	if (s_flakes)
		return;
	s_flakes = picture("zeetures weather snow", {ball(3, {255, 255, 255}, {200, 212, 232}), ball(4, {255, 255, 255}, {196, 208, 230}), ball(6, {255, 255, 255}, {190, 204, 228})});
	s_stones = picture("zeetures weather hail", {ball(4, {250, 252, 255}, {150, 165, 190}), ball(5, {248, 250, 255}, {140, 156, 184})});
	s_leaves = picture("zeetures weather leaves", {leaf(0, 0), leaf(0, 1), leaf(1, 0), leaf(1, 1), leaf(2, 0), leaf(2, 1)});
	// a white checker for the lightning's flash (half the pixels)
	{
		Image im = newImage(64, 64);
		for (int y = 0; y < 64; y++)
			for (int x = 0; x < 64; x++)
				if (dither(x, y, 0.5f))
					put(im, x, y, 235, 238, 255);
		s_white = picture("zeetures weather flash", {im});
	}
	// the heatwave's warm light, multiplied over the view
	{
		Image im = newImage(1, 1);
		put(im, 0, 0, 255, 236, 206);
		s_warm = picture("zeetures weather warmth", {im});
	}
	// the glow where lightning lands
	{
		Image im = newImage(90, 30);
		for (int y = 0; y < 30; y++)
			for (int x = 0; x < 90; x++) {
				float dx = (x - 45) / 45.0f, dy = (y - 15) / 15.0f, d = std::sqrt(dx * dx + dy * dy);
				if (d < 1 && dither(x, y, (1 - d) * 0.9f))
					put(im, x, y, 255, (int)(170 + 60 * (1 - d)), (int)(90 + 80 * (1 - d)));
			}
		s_glow = picture("zeetures weather strike glow", {im});
	}
	// fog: soft flat puffs, dithered, at three strengths (full, and two
	// thinner for the edges of the open air they drift through)
	for (int v = 0; v < 4; v++)
		for (int k = 0; k < 3; k++) {
			const int w = 140, h = 44;
			const float strength = k == 0 ? 1.0f : k == 1 ? 0.6f : 0.3f;
			Image im = newImage(w, h);
			for (int y = 0; y < h; y++)
				for (int x = 0; x < w; x++) {
					// (round, soft all the way out: square-edged puffs in rows
					// read as stripes)
					float dx = (x - w / 2.0f) / (w / 2.0f), dy = (y - h / 2.0f) / (h / 2.0f);
					float d2 = dx * dx + dy * dy;
					float n = vnoise(x / 40.0f + v * 13, y / 26.0f, 41 + v);
					float a = std::clamp((1.0f - d2) * (1.0f - d2) * (0.45f + 0.55f * n), 0.0f, 1.0f) * 0.6f * strength;
					if (dither(x, y, a))
						put(im, x, y, 214 + (int)(n * 20), 218 + (int)(n * 18), 226 + (int)(n * 14));
				}
			s_fogBanks.push_back(picture("zeetures weather fog", {im}));
		}
}

// ---- lightning --------------------------------------------------------------

struct Bolt {
	std::vector<std::vector<std::pair<float, float>>> strokes; // the main bolt, then its forks
	float gx = 0, gy = 0; // where it lands
	float age = 0; // ms
};
std::vector<Bolt> s_bolts;

void boltPath(std::vector<std::pair<float, float>>& out, float x0, float y0, float x1, float y1, float rough) {
	out.push_back({x0, y0});
	std::vector<std::pair<float, float>> pts = {{x0, y0}, {x1, y1}};
	for (int pass = 0; pass < 6; pass++) {
		std::vector<std::pair<float, float>> next;
		for (size_t i = 0; i + 1 < pts.size(); i++) {
			auto [ax, ay] = pts[i];
			auto [bx, by] = pts[i + 1];
			next.push_back(pts[i]);
			float len = std::hypot(bx - ax, by - ay);
			next.push_back({(ax + bx) / 2 + rand_float(-1, 1) * len * rough, (ay + by) / 2 + rand_float(-0.3f, 0.3f) * len * rough});
		}
		next.push_back(pts.back());
		pts.swap(next);
	}
	out = pts;
}

void strike(Places& P, bool nearView) {
	// where: open ground under the sky (a floor the rain reaches), near the
	// view half the time
	int x = -1;
	for (int tries = 0; tries < 40 && x < 0; tries++) {
		int c;
		if (nearView && engine.camera) {
			int cx = (int)engine.camera->getX() - P.mx, cw = (int)engine.camera->getWidth();
			c = cx + (int)rand_uint32(0, std::max(1, cw) - 1);
		} else
			c = (int)rand_uint32(0, P.W - 1);
		c %= P.W;
		if (c < 0)
			c += P.W;
		if (P.s0kind[c] == FLOOR && P.s0[c] > 120 && P.s0[c] < P.H - 10)
			x = c;
	}
	if (x < 0)
		return;
	Bolt b;
	b.gx = (float)x;
	b.gy = P.s0[x];
	float topY = std::min(260.0f, b.gy - 80.0f);
	float topX = b.gx + rand_float(-120, 120);
	std::vector<std::pair<float, float>> main;
	boltPath(main, topX, topY, b.gx, b.gy, 0.18f);
	b.strokes.push_back(main);
	int forks = (int)rand_uint32(2, 4);
	for (int f = 0; f < forks; f++) {
		auto [fx, fy] = main[rand_uint32(main.size() / 8, main.size() * 2 / 3)];
		float len = rand_float(40, 110);
		float ang = rand_float(0.3f, 1.1f) * (rand_float(0, 1) < 0.5f ? -1 : 1);
		std::vector<std::pair<float, float>> fork;
		boltPath(fork, fx, fy, fx + std::sin(ang) * len, fy + std::cos(ang) * len * 0.8f, 0.25f);
		b.strokes.push_back(fork);
	}
	s_bolts.push_back(b);
	// what it strikes: Zeetures' lightning effects (Lightning.h), and the
	// jolt C2's own storms give a creature under a bolt
	const float wx = b.gx + P.mx, wy = b.gy + P.my;
	Lightning::strikeArea(wx - 26, wy - 150, wx + 26, wy + 6);
	engine.executeNetwork(fmt::format("enum 4 0 0 doif posr gt {} and posl lt {} and posb gt {} and post lt {} stim writ targ 0 255 0 0 1 200 0 0 0 0 0 0 endi next\n",
		(int)wx - 26, (int)wx + 26, (int)wy - 150, (int)wy + 6));
	Sound s = soundmanager.playSound("thun");
	if (s)
		s.setPosition(wx, wy - 100, 1, 1);
	fmt::print("* Weather: lightning struck at {:.0f},{:.0f}\n", wx, wy);
}

// ---- the game's own weather, held off while ours is on -----------------------

void holdOffStockWeather(bool hold) {
	for (auto& a : world.agents) {
		if (!a || a->isDying() || a->family != 2 || a->genus != 14)
			continue;
		if (a->species == 10) // the evaporators: they make the clouds
			a->paused = hold;
		else if (hold && (a->species == 1 || a->species == 2 || a->species == 3 || a->species == 4))
			a->kill(); // (clouds, storm clouds, raindrops, bolts)
	}
	s_suppressed = hold;
}

Sound s_rainSound;
float s_rainPlaying = 0;

} // namespace

// ---- public -----------------------------------------------------------------

bool on() {
	if (s_on < 0) {
		if (const char* e = devenv("OPENC2E_WEATHER"); e && std::string(e) == "0")
			s_on = 0;
		else
			s_on = readPlayerSetting("new-weather") == "0" ? 0 : 1;
	}
	return s_on == 1;
}

void setOn(bool o) {
	s_on = o ? 1 : 0;
	writePlayerSetting("new-weather", o ? "1" : "0");
	if (!o) {
		holdOffStockWeather(false);
		s_parts.clear();
		s_splashes.clear();
		s_bolts.clear();
	}
}

Spell spell() {
	return (Spell)s_spell;
}
const char* spellName(int s) {
	return s >= 0 && s < SPELL_COUNT ? NAMES[s] : "";
}
bool hail() {
	return s_hail;
}
unsigned int ticksLeft() {
	return s_until > world.worldtickcount ? s_until - world.worldtickcount : 0;
}

void setSpell(Spell s) {
	bool h = s == THUNDERSTORM && world.season == 1 && rand_float(0, 1) < 0.35f;
	begin(s, h, spellLength(s));
	s_next = -1;
}

void reset() {
	s_spell = CLEAR;
	s_hail = false;
	s_until = 0;
	s_mix = 0;
	s_next = -1;
	s_parts.clear();
	s_splashes.clear();
	s_bolts.clear();
	s_cloudTiles.clear();
	s_cloudShown = 0;
}

// ---- the temple bell's Easter eggs (owner, 2026-11-27) -------------------------
// The great bell on the temple top (additions/temple-top.cos, 2 3 274) counts
// each ring by hand or rope in its OV10; this watches it. A ring:
// - calls the colony: its rope gives off the "bell" scent for two minutes,
//   which creatures awake follow up to the temple (pheromones.txt);
// - in a thunderstorm, draws the lightning: a strike on the bell tower within
//   a few seconds, which rings the bell, and the storm's strikes go there for
//   30 s (a lightning rod you play like an instrument);
// - three within 6 s call up other weather (a heatwave turns to showers, rain
//   or snow clears, anything else gives way to the season's chance), once an
//   in-game day;
// - as a shower ends, or within 90 s after, brings a rainbow over the temple
//   for two and a half minutes;
// - in fog, comes back as a faint echo (zbee) 1.4 s later.
// And at dawn and at dusk the bell rings itself, softly (event 200, zbes),
// and creatures asleep near the temple wake a little sooner (41, Sleepiness
// Decrease, 40).
namespace {
const float BELL_X = 4662.0f, BELL_Y = 200.0f, BELL_TOP = 62.0f; // the bell; where the arch holds it
AgentRef s_bell, s_bellRope;
int s_bellTaps = -1; // its OV11 (taps by the hand) last seen
std::vector<std::pair<int, unsigned int>> s_tune; // (note, world tick)
int s_bellRings = -1; // its OV10 last seen
std::vector<std::pair<unsigned int, int>> s_ringTicks; // the latest rings: world tick, and the note tapped (-1 the rope)
unsigned int s_stormBellUntil = 0, s_stormStrikeAt = 0, s_lastWeatherCall = 0;
unsigned int s_rainEndedAt = 0, s_rainbowFrom = 0, s_rainbowUntil = 0, s_echoAt = 0;
int s_lastTimeOfDay = -1;

bool rainy(int s) {
	return s == LIGHT_SHOWERS || s == SHOWERS || s == THUNDERSTORM || s == TORRENTIAL;
}

void findBell() {
	s_bell = s_bellRope = AgentRef();
	for (auto& a : world.agents)
		if (a && !a->isDying() && a->family == 2 && a->genus == 3) {
			if (a->species == 274)
				s_bell = a;
			else if (a->species == 275)
				s_bellRope = a;

		}
}

// a strike on the bell tower: the bolt to the arch over the bell, its
// thunder, and the bell rung by it
void strikeBell() {
	MetaRoom* m = world.map->metaRoomAt((unsigned int)BELL_X, (unsigned int)BELL_Y);
	Places* P = m ? placesFor(m) : nullptr;
	if (!P)
		return;
	Bolt b;
	b.gx = BELL_X - P->mx;
	b.gy = BELL_TOP - P->my;
	std::vector<std::pair<float, float>> main;
	boltPath(main, b.gx + rand_float(-90, 90), b.gy - 240, b.gx, b.gy, 0.18f);
	b.strokes.push_back(main);
	for (int f = 0; f < 2; f++) {
		auto [fx, fy] = main[rand_uint32(main.size() / 6, main.size() / 2)];
		std::vector<std::pair<float, float>> fork;
		float ang = rand_float(0.4f, 1.0f) * (f ? 1 : -1);
		boltPath(fork, fx, fy, fx + std::sin(ang) * 70, fy + std::cos(ang) * 60, 0.25f);
		b.strokes.push_back(fork);
	}
	s_bolts.push_back(b);
	Sound s = soundmanager.playSound("thun");
	if (s)
		s.setPosition(BELL_X, BELL_TOP - 100, 1, 1);
	if (s_bell)
		s_bell->queueScript(201);
	fmt::print("* Weather: lightning struck the temple bell\n");
}

void bellRung(int note) {
	const unsigned int now = std::max(1u, world.worldtickcount);
	s_ringTicks.push_back({now, note});
	s_ringTicks.erase(std::remove_if(s_ringTicks.begin(), s_ringTicks.end(), [&](const std::pair<unsigned int, int>& r) { return now - r.first > 60; }), s_ringTicks.end());
	// calling the colony
	if (s_bellRope)
		Pheromones::markFor(s_bellRope.get(), "bell", 1.0f, 1200);
	if (!on())
		return;
	// the storm bell
	if (s_spell == THUNDERSTORM && s_mix > 0.3f) {
		s_stormBellUntil = now + 300;
		if (!s_stormStrikeAt)
			s_stormStrikeAt = now + rand_uint32(20, 60);
	}
	// the rainbow chime
	const bool showerEnding = rainy(s_spell) && world.worldtickcount >= s_until;
	const bool showerEnded = !rainy(s_spell) && s_rainEndedAt && now - s_rainEndedAt < 900;
	if ((showerEnding || showerEnded) && world.timeofday != 4 && now >= s_rainbowUntil) {
		s_rainbowFrom = now;
		s_rainbowUntil = now + 1500;
		fmt::print("* Weather: the temple bell brought a rainbow\n");
	}
	// the echo in the fog
	if (s_spell == FOG && s_mix > 0.3f)
		s_echoAt = now + 14;
	// the weather call: three rings in 6 s of one note (the rope's, or
	// three taps in one place: no tune has three of a note running, so
	// playing one does not call the weather), once a day
	const size_t nr = s_ringTicks.size();
	const bool sameThree = nr >= 3 && s_ringTicks[nr - 1].second == s_ringTicks[nr - 2].second && s_ringTicks[nr - 2].second == s_ringTicks[nr - 3].second;
	if (sameThree && (!s_lastWeatherCall || now - s_lastWeatherCall >= TICKS_PER_DAY)) {
		int to;
		if (s_spell == HEATWAVE)
			to = SHOWERS;
		else if (rainy(s_spell) || s_spell == SNOW)
			to = CLEAR;
		else {
			bool h;
			to = pickSpell(h);
			for (int tries = 0; tries < 20 && to == s_spell; tries++)
				to = pickSpell(h);
			if (to == s_spell)
				to = s_spell == CLEAR ? SHOWERS : CLEAR;
		}
		s_next = to;
		s_until = world.worldtickcount; // (this one fades out first)
		s_lastWeatherCall = now;
		s_ringTicks.clear();
		fmt::print("* Weather: the temple bell called up {}\n", NAMES[to]);
	}
}

// ---- the great bell's hidden tunes (owner, 2026-11-27) ----
// The hand taps the great bell (temple-top.cos counts its taps in OV11): where
// on it says the note, higher the higher up the bell, as a real bell sounds
// higher toward its crown: lip D, skirt F, waist A, shoulder B, crown high D
// (notes 0-4; our own tones zbl0-zbl4). (A rack of chimes was tried and
// found too busy for the temple top.) The last notes tapped, each within 4 s
// of the one before, are matched against the tunes below, played as on a
// five-holed ocarina; a tune played, the great bell answers softly, and:
// - the storm tune calls up a thunderstorm;
// - the sun tune brings the day (night turns to dawn; rain or snow clears);
// - the tune of ages finds the oldest ancestor of a living creature (the one
//   selected, else the one nearest the bell), as a status-bar notice;
// - the lullaby calms creatures near the temple and makes them drowsy;
// - the critters' tune calls bugs and critters up to the temple top;
// - the playful tune perks creatures nearby up;
// - the healing tune eases pain and injury nearby;
// - and three travelling tunes fly the view to the treehouse forest, the
//   volcano and the east sea.
// The tunes are named only here, by what they do: none is named in the game.

struct Tune {
	const char* what;
	std::vector<int> notes;
};
const Tune TUNES[] = {
	{"storm", {0, 1, 4, 0, 1, 4}},
	{"sun", {2, 1, 4, 2, 1, 4}},
	{"ages", {2, 0, 1, 2, 0, 1}},
	{"lullaby", {3, 4, 2, 3, 4, 2}},
	{"critters", {4, 3, 2, 4, 3, 2}},
	{"play", {1, 2, 3, 1, 2, 3}},
	{"healing", {3, 2, 1, 3, 2, 1}},
	{"forest", {0, 4, 3, 2, 3, 2}},
	{"volcano", {1, 0, 1, 0, 2, 1, 2, 1}},
	{"sea", {0, 1, 2, 2, 3}},
};

// creatures within reach of the temple (x 3200-6200, above the caves)
void stimNearTemple(const std::array<int, 4>& chems, const std::array<int, 4>& amounts) {
	engine.executeNetwork(fmt::format("enum 4 0 0 doif posx gt 3200 and posx lt 6200 and posy lt 1300 stim writ targ 0 255 0 0 {} {} {} {} {} {} {} {} endi next\n",
		chems[0], amounts[0], chems[1], amounts[1], chems[2], amounts[2], chems[3], amounts[3]));
}

void tuneOfAges() {
	// whose: the selected creature, else the one nearest the bell
	Agent* who = world.selectedcreature.get();
	if (!who || !dynamic_cast<CreatureAgent*>(who)) {
		float best = 1e30f;
		for (auto& a : world.agents)
			if (a && !a->isDying() && dynamic_cast<CreatureAgent*>(a.get())) {
				float d = std::hypot(a->x - 4500, a->y - 350);
				if (d < best)
					best = d, who = a.get();
			}
	}
	auto* ca = who ? dynamic_cast<CreatureAgent*>(who) : nullptr;
	if (!ca || !world.history)
		return;
	std::string mon = world.history->findMoniker(ca->getCreature()->getGenome());
	if (mon.empty())
		return;
	// back through the parents, generation by generation: the furthest, and
	// of those the first hatched
	std::string oldest = mon;
	int depth = 0, best = 0;
	std::vector<std::pair<std::string, int>> todo{{mon, 0}};
	std::set<std::string> seen;
	while (!todo.empty()) {
		auto [m, d] = todo.back();
		todo.pop_back();
		if (!seen.insert(m).second || !world.history->hasMoniker(m))
			continue;
		monikerData& md = world.history->getMoniker(m);
		if (d > best || (d == best && d > 0 && md.borntick < world.history->getMoniker(oldest).borntick))
			best = d, oldest = m;
		for (const std::string& p : {md.mother, md.father})
			if (!p.empty())
				todo.push_back({p, d + 1});
	}
	depth = best;
	auto nameOf = [](const std::string& m) {
		monikerData& md = world.history->getMoniker(m);
		return md.name.empty() ? m : md.name;
	};
	lifeLogEntry e;
	e.kind = lifeLogEntry::ancestor;
	e.tick = world.worldtickcount;
	e.moniker = oldest;
	monikerData& od = world.history->getMoniker(oldest);
	e.name = od.name;
	e.genus = std::max(1, (int)od.genus);
	e.female = od.gender == 2;
	e.cause = depth == 0 ? fmt::format("the bell's tune of ages: {} is the first of its line", nameOf(mon))
						 : fmt::format("the bell's tune of ages: {}'s line goes back {} generation{} to {}", nameOf(mon), depth, depth == 1 ? "" : "s", nameOf(oldest));
	world.history->lifelog.push_back(e);
	world.history->notices.push_back(e);
}

void playTune(const Tune& tune) {
	const std::string what = tune.what;
	fmt::print("* Weather: the temple bell played the {} tune\n", what);
	if (s_bell)
		s_bell->queueScript(200); // (the great bell answers)
	const unsigned int now = std::max(1u, world.worldtickcount);
	auto callUp = [&](int to) {
		if (!on() || s_spell == to)
			return;
		s_next = to;
		s_until = now;
	};
	if (what == "storm")
		callUp(THUNDERSTORM);
	else if (what == "sun") {
		if (world.timeofday == 4) { // night: it dawns
			world.timeofday = 0;
			if (++world.dayofseason == 4) {
				world.dayofseason = 0;
				if (++world.season == 4)
					world.season = 0, world.year++;
			}
		}
		if (rainy(s_spell) || s_spell == SNOW || s_spell == FOG)
			callUp(CLEAR);
	} else if (what == "ages")
		tuneOfAges();
	else if (what == "lullaby")
		stimNearTemple({44, 24, 0, 0}, {40, 30, 0, 0}); // Fear Decrease, Sleepiness Increase
	else if (what == "play")
		stimNearTemple({36, 42, 45, 0}, {40, 30, 30, 0}); // Pleasure-need, Loneliness and Boredom Decrease
	else if (what == "healing")
		stimNearTemple({35, 48, 0, 0}, {60, 40, 0, 0}); // Pain and Injury Decrease
	else if (what == "critters") {
		int n = 0;
		for (auto& a : world.agents)
			if (n < 5 && a && !a->isDying() && a->family == 2 && (a->genus == 18 || a->genus == 20) && !a->carriedby) {
				a->moveTo(4420 + n * 70.0f, 300.0f - a->getHeight());
				n++;
			}
	} else if (engine.camera) {
		const float to[3][2] = {{3460, 1450}, {7790, 480}, {6900, 860}};
		const int i = what == "forest" ? 0 : what == "volcano" ? 1 : 2;
		engine.camera->moveTo((int)(to[i][0] - engine.camera->getWidth() / 2.0f), (int)(to[i][1] - engine.camera->getHeight() / 2.0f), smoothscroll);
	}
}

void noteStruck(int note) {
	const unsigned int now = std::max(1u, world.worldtickcount);
	if (!s_tune.empty() && now - s_tune.back().second > 40)
		s_tune.clear();
	s_tune.push_back({note, now});
	for (const Tune& t : TUNES) {
		const size_t n = t.notes.size();
		if (s_tune.size() < n)
			continue;
		bool match = true;
		for (size_t i = 0; i < n && match; i++)
			match = s_tune[s_tune.size() - n + i].first == t.notes[i];
		if (match) {
			s_tune.clear();
			playTune(t);
			return;
		}
	}
	if (s_tune.size() > 16)
		s_tune.erase(s_tune.begin());
}

void bellEggs(bool weatherOn) {
	if (world.tickcount % 50 == 7 && (!s_bell || !s_bellRope))
		findBell();

	if (!s_bell)
		return;
	const unsigned int now = std::max(1u, world.worldtickcount);
	// taps by the hand first (each also a ring, OV10): the note from where
	// the hand is on the bell, higher the higher up
	const int taps = s_bell->var[11].hasInt() ? s_bell->var[11].getInt() : 0;
	const int rings = s_bell->var[10].hasInt() ? s_bell->var[10].getInt() : 0;
	if (s_bellTaps < 0 || taps < s_bellTaps)
		s_bellTaps = taps;
	if (s_bellRings < 0 || rings < s_bellRings)
		s_bellRings = rings;
	while (s_bellTaps < taps) {
		s_bellTaps++;
		const float hy = world.hand() ? (float)world.hand()->pointerY() : 200.0f;
		const int note = hy < 150 ? 4 : hy < 185 ? 3 : hy < 215 ? 2 : hy < 250 ? 1 : 0;
		Sound s = soundmanager.playSound(fmt::format("zbl{}", note));
		if (s)
			s.setPosition(BELL_X - 85, 113, 170, 165);
		if (s_bellRings < rings)
			s_bellRings++;
		bellRung(note);
		noteStruck(note);
	}
	// the rest: the rope
	while (s_bellRings < rings) {
		s_bellRings++;
		bellRung(-1);
	}
	if (weatherOn && s_stormStrikeAt && now >= s_stormStrikeAt) {
		s_stormStrikeAt = 0;
		if (s_spell == THUNDERSTORM)
			strikeBell();
	}
	if (s_echoAt && now >= s_echoAt) {
		s_echoAt = 0;
		Sound s = soundmanager.playSound("zbee");
		if (s)
			s.setPosition(BELL_X - 600, BELL_Y, 1, 1); // (from the fog off to the west)
	}
	// the hour bell
	const int tod = (int)world.timeofday;
	if (tod != s_lastTimeOfDay) {
		if (s_lastTimeOfDay >= 0 && (tod == 0 || tod == 3)) {
			s_bell->queueScript(200);
			engine.executeNetwork(fmt::format("enum 4 0 0 doif posx gt {} and posx lt {} and posy lt {} and aslp eq 1 stim writ targ 0 255 0 0 41 40 0 0 0 0 0 0 endi next\n",
				(int)BELL_X - 1500, (int)BELL_X + 1500, 1300));
		}
		s_lastTimeOfDay = tod;
	}
}
} // namespace

void tick() {
	if (engine.version != 2 || !world.map)
		return;
	if (!on()) {
		if (s_suppressed)
			holdOffStockWeather(false);
		bellEggs(false); // (its hour and its call need no weather)
		return;
	}
	if (world.tickcount % 20 == 1 || !s_suppressed)
		holdOffStockWeather(true);
	// developer aid: OPENC2E_WEATHER=NAME starts with that spell ("snow",
	// "thunderstorm", "hail", "torrential", "showers", "light", "fog",
	// "heatwave", "wind", "clear")
	static bool forced = false;
	if (!forced) {
		forced = true;
		if (const char* e = devenv("OPENC2E_WEATHER"); e && std::string(e) != "0" && std::string(e) != "1") {
			std::string w = e;
			int s = w == "snow" ? SNOW : w == "thunderstorm" || w == "hail" ? THUNDERSTORM : w == "torrential" ? TORRENTIAL : w == "showers" ? SHOWERS : w == "light" ? LIGHT_SHOWERS : w == "fog" ? FOG : w == "heatwave" ? HEATWAVE : w == "wind" ? WINDSTORM : CLEAR;
			begin(s, w == "hail", 1000000);
			s_mix = 1.0f;
		}
	}
	// the next spell when this one ends (it fades out first)
	if (s_until == 0) {
		bool h;
		int s = pickSpell(h);
		begin(s, h, spellLength(s));
	}
	const bool ending = world.worldtickcount >= s_until;
	if (ending) {
		s_mix = std::max(0.0f, s_mix - 1.0f / 300.0f);
		if (s_mix <= 0) {
			if (rainy(s_spell))
				s_rainEndedAt = std::max(1u, world.worldtickcount);
			bool h;
			int s = pickSpell(h);
			if (s_next >= 0) { // (the temple bell's call)
				s = s_next;
				h = s == THUNDERSTORM && world.season == 1 && rand_float(0, 1) < 0.35f;
				s_next = -1;
			}
			begin(s, h, spellLength(s));
		}
	} else
		s_mix = std::min(1.0f, s_mix + 1.0f / 400.0f);
	// lightning, in thunderstorms (about every 15 s at the storm's height;
	// developer aid: OPENC2E_WEATHER_STRIKE=N, every N ticks, in view)
	static const int strikeEvery = devenv("OPENC2E_WEATHER_STRIKE") ? std::max(1, atoi(devenv("OPENC2E_WEATHER_STRIKE"))) : 0;
	if (strikeEvery && s_spell == THUNDERSTORM && world.tickcount % strikeEvery == 0) {
		if (MetaRoom* m = engine.camera ? engine.camera->getMetaRoom() : nullptr)
			if (Places* P = placesFor(m))
				strike(*P, true);
	} else if (s_spell == THUNDERSTORM && s_mix > 0.75f && rand_uint32(0, 149) == 0) {
		if (world.worldtickcount < s_stormBellUntil)
			strikeBell();
		else if (world.map->getMetaRoom(0) || engine.camera)
			if (MetaRoom* m = engine.camera ? engine.camera->getMetaRoom() : nullptr)
				if (Places* P = placesFor(m))
					strike(*P, rand_float(0, 1) < 0.6f);
	}
	bellEggs(true);
}

void draw(RenderTarget* surface, MetaRoom* m, int adjustx, int adjusty) {
	if (engine.version != 2 || !on() || !surface || !m)
		return;
	Places* P = placesFor(m);
	if (!P)
		return;
	makePictures();
	s_frame++;
	// developer aid: OPENC2E_WEATHER_PROFILE prints, every 300 frames, the
	// average time drawing the weather took
	static const bool profile = devenv("OPENC2E_WEATHER_PROFILE") != nullptr;
	static double spentMs = 0;
	static int spentFrames = 0;
	auto drawStart = std::chrono::steady_clock::now();
	struct Report {
		std::chrono::steady_clock::time_point start;
		~Report() {
			if (!profile)
				return;
			spentMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
			if (++spentFrames == 300) {
				fmt::print("* Weather: {:.2f} ms a frame to draw ({} drops)\n", spentMs / spentFrames, s_parts.size());
				spentMs = 0;
				spentFrames = 0;
			}
		}
	} report{drawStart};
	// time since the last frame, at the world's own pace (frozen while paused)
	static auto last = std::chrono::steady_clock::now();
	auto now = std::chrono::steady_clock::now();
	float dt = std::min(100.0f, std::chrono::duration<float, std::milli>(now - last).count());
	last = now;
	if (world.paused)
		dt = 0;
	else if (world.ticktime > 0)
		dt *= 100.0f / (float)world.ticktime;
	const float sec = dt / 1000.0f;
	const int vw = surface->getWidth(), vh = surface->getHeight();
	const int vx = adjustx - P->mx, vy = adjusty - P->my; // the view, in the background's terms
	const float mix = s_mix;
	const int sp = s_spell;
	auto toScreen = [&](float x, float y) { return std::pair<int, int>((int)std::lround(x - vx), (int)std::lround(y - vy)); };


	// how much of each kind of weather, now
	float rain = 0, snow = 0, hailN = 0, dust = 0, fog = 0;
	switch (sp) {
		case LIGHT_SHOWERS: rain = 110; break;
		case SHOWERS: rain = 260; break;
		case THUNDERSTORM:
			rain = s_hail ? 380 : 600;
			hailN = s_hail ? 70 : 0;
			break;
		case TORRENTIAL: rain = 950; break;
		case SNOW: snow = 320; break;
		case FOG: fog = 1; break;
		case WINDSTORM: dust = 220; break;
		default: break;
	}
	const float area = (vw * vh) / (800.0f * 600.0f);
	const float slant = sp == TORRENTIAL || sp == THUNDERSTORM ? 0.22f : sp == SHOWERS ? 0.08f : 0.04f;

	// the heatwave's warm light over the view
	if (sp == HEATWAVE && mix > 0.02f) {
		RenderOptions o;
		o.multiply = true;
		o.override_drawsize = true;
		o.overridden_drawwidth = vw;
		o.overridden_drawheight = vh;
		// (eased in: the colour toward white as it comes and goes)
		Image& im = s_warm->images[0];
		uint8_t* p = im.data.data();
		uint8_t want[3] = {255, (uint8_t)(255 - 19 * mix), (uint8_t)(255 - 49 * mix)};
		if (p[1] != want[1] || p[2] != want[2]) {
			p[0] = want[0];
			p[1] = want[1];
			p[2] = want[2];
			s_warm->textures.clear();
			s_warm->textures.resize(1);
		}
		surface->renderCreaturesImage(*s_warm, 0, 0, 0, o);
	}

	// fog: rows of soft puffs drifting east through the open air, where the
	// wind storm's dust blows (owner, 2026-11-27: fog moves sideways, "should
	// go in the same areas as the dust storm"): under bridges and platforms
	// and over the open ground, not into rock, rooms or water. A puff thins
	// where part of it would be outside that air, and is gone where most is.
	if (fog > 0 && mix > 0.02f) {
		const int SP = 90, RH = 30, PW = 140, PH = 44;
		const double tsec = world.worldtickcount / 10.0;
		for (int row = std::max(0, (vy - PH) / RH); row * RH < std::min(P->H, vy + vh); row++) {
			uint32_t rh = hash2(row, 7, 53);
			const double speed = 10.0 + (rh & 15), phase = (double)((rh >> 4) & 127);
			const double moved = phase + tsec * speed;
			const int y0 = row * RH + (int)((rh >> 12) & 15) - 8;
			for (long k = (long)std::floor((vx - PW - moved) / SP); k * SP + moved < vx + vw; k++) {
				uint32_t h = hash2((int)k, row, 51);
				if ((h & 0xff) / 255.0f > mix * 0.8f)
					continue;
				const int x = (int)std::floor((k * SP + moved) / 4.0) * 4; // (on the 4 px grid)
				const int y = y0 + (int)((h >> 16) & 7) * 4 - 14; // (staggered)
				int in = 0;
				const int pts[7][2] = {{15, 22}, {40, 22}, {70, 22}, {100, 22}, {125, 22}, {70, 8}, {70, 36}};
				for (auto& q : pts) {
					int c = ((x + q[0]) % P->W + P->W) % P->W, yy = y + q[1];
					if (yy >= 0 && yy < P->H && P->wind.get(c, yy))
						in++;
				}
				const int level = in == 7 ? 0 : in >= 5 ? 1 : in >= 3 ? 2 : -1;
				if (level < 0)
					continue;
				auto [sx, sy] = toScreen((float)x, (float)y);
				if (sx > vw || sx + PW < 0 || sy > vh || sy + PH < 0)
					continue;
				surface->renderCreaturesImage(*s_fogBanks[(h >> 8) % 4 * 3 + level], 0, sx, sy);
			}
		}
	}

	// the particles: keep as many as the weather wants in view
	const size_t wantRain = (size_t)(rain * mix * area), wantSnow = (size_t)(snow * mix * area), wantHail = (size_t)(hailN * mix * area),
				 wantDust = (size_t)(dust * mix * area);
	auto count = [&](uint8_t k) { return (size_t)std::count_if(s_parts.begin(), s_parts.end(), [&](const Particle& p) { return p.kind == k; }); };
	auto spawn = [&](uint8_t k, bool anywhere) {
		for (int tries = 0; tries < 6; tries++) {
			Particle p{};
			p.kind = k;
			p.phase = rand_float(0, 6.28f);
			if (k == DUST || k == LEAF) {
				p.x = (float)vx + (anywhere ? rand_float(-60, vw + 60) : rand_float(-120, -10));
				p.y = (float)vy + rand_float(0, vh);
				p.vx = rand_float(380, 820) * (k == LEAF ? 0.7f : 1.0f);
				p.vy = rand_float(-20, 20);
				if (!P->wind.get((int)p.x, (int)p.y))
					continue;
			} else {
				p.x = (float)vx + rand_float(-120, vw + 120);
				p.y = anywhere ? (float)vy + rand_float(-40, vh) : (float)vy - rand_float(5, 60);
				if (!P->precip.get((int)p.x, (int)std::max(0.0f, p.y)))
					continue;
				if (k == RAIN) {
					p.vy = rand_float(520, 720) * (sp == TORRENTIAL ? 1.2f : 1.0f);
					p.vx = p.vy * slant;
				} else if (k == SNOWFLAKE) {
					p.vy = rand_float(35, 85);
					p.vx = rand_float(-10, 14);
				} else {
					p.vy = rand_float(600, 760);
					p.vx = p.vy * slant * 0.5f;
				}
			}
			p.look = (uint8_t)rand_uint32(0, 255);
			s_parts.push_back(p);
			return;
		}
	};
	const bool fresh = s_parts.empty();
	for (auto [k, want] : {std::pair<uint8_t, size_t>{RAIN, wantRain}, {SNOWFLAKE, wantSnow}, {HAILSTONE, wantHail}, {DUST, wantDust}, {LEAF, wantDust / 6}}) {
		size_t have = count(k);
		size_t add = want > have ? std::min(want - have, (size_t)(fresh ? want : 40)) : 0;
		// (filled out anywhere in view, not only from above: the weather is
		// already everywhere when the view gets there; owner, 2026-11-27,
		// "it should be there and running, regardless of the camera")
		for (size_t i = 0; i < add; i++)
			spawn(k, true);
		if (have > want) { // (fewer as the weather eases)
			size_t drop = std::min(have - want, (size_t)20);
			for (size_t i = 0; i < s_parts.size() && drop; i++)
				if (s_parts[i].kind == k) {
					s_parts[i] = s_parts.back();
					s_parts.pop_back();
					drop--;
				}
		}
	}

	// move them, and draw them
	const Color rainCol = sp == TORRENTIAL ? Color{188, 198, 220, 255} : Color{208, 216, 232, 255};
	const Color dustCol = {214, 196, 150, 255};
	for (size_t i = 0; i < s_parts.size();) {
		Particle& p = s_parts[i];
		p.phase += sec * 2.0f;
		float ox = p.x, oy = p.y;
		p.x += (p.vx + (p.kind == SNOWFLAKE ? std::sin(p.phase) * 18.0f : 0.0f)) * sec;
		p.y += (p.vy + (p.kind == DUST || p.kind == LEAF ? std::sin(p.phase * 1.7f) * 30.0f : 0.0f)) * sec;
		bool gone = false, left = false; // (left: the view moved away from it)
		if (p.kind == DUST || p.kind == LEAF) {
			if (p.x > vx + vw + 150 || p.x < vx - 300 || p.y < vy - 100 || p.y > vy + vh + 100)
				gone = left = true;
			else if (!P->wind.get((int)p.x, (int)p.y))
				gone = true;
		} else if (!P->precip.get((int)p.x, (int)std::max(0.0f, p.y))) {
			// it lands: a splash, a bounce, a stone lying
			// (where it met the surface: a fast drop moves tens of px a frame,
			// and its splash showed under the volcano's walkway)
			float ly = p.y;
			while (ly > oy && !P->precip.get((int)p.x, (int)std::max(0.0f, ly - 1)))
				ly -= 1;
			if (p.kind == RAIN && rand_uint32(0, 2) == 0)
				s_splashes.push_back({p.x, ly, 0, 0});
			if (p.kind == HAILSTONE && p.bounces == 0) {
				p.bounces = 1;
				p.y = oy;
				p.vy = -p.vy * 0.3f;
				p.vx *= 0.5f;
				i++;
				continue;
			}
			if (p.kind == HAILSTONE && s_splashes.size() < 600)
				s_splashes.push_back({p.x, ly, 0, 1});
			gone = true;
		} else if (p.y > vy + vh + 40 || p.y < vy - 200 || p.x < vx - 200 || p.x > vx + vw + 200) {
			gone = true;
			left = p.y < vy + vh || p.x < vx - 200 || p.x > vx + vw + 200; // (not just fallen past the view's foot)
		}
		if (p.kind == HAILSTONE && p.bounces == 1)
			p.vy += 1800.0f * sec;
		if (gone) {
			uint8_t k = p.kind;
			s_parts[i] = s_parts.back();
			s_parts.pop_back();
			// one the view moved away from comes back anywhere in the new
			// view, already falling or blowing; one that landed, from above
			spawn(k, left);
			continue;
		}
		auto [sx, sy] = toScreen(p.x, p.y);
		if (sx >= -10 && sx < vw + 10 && sy >= -30 && sy < vh + 10) {
			switch (p.kind) {
				case RAIN: {
					float len = 10.0f + (p.look & 7) + (sp == TORRENTIAL ? 6 : 0);
					float k = len / std::max(1.0f, p.vy);
					surface->renderLine((float)sx, (float)sy, (float)sx - p.vx * k, (float)sy - len, rainCol);
					break;
				}
				case SNOWFLAKE:
					surface->renderCreaturesImage(*s_flakes, p.look % 3, sx, sy);
					break;
				case HAILSTONE:
					surface->renderCreaturesImage(*s_stones, p.look % 2, sx, sy);
					break;
				case DUST: {
					float len = 14.0f + (p.look & 31);
					surface->renderLine((float)sx, (float)sy, (float)sx - len, (float)sy, dustCol);
					break;
				}
				case LEAF:
					surface->renderCreaturesImage(*s_leaves, (p.look % 3) * 2 + ((int)(p.phase * 3) & 1), sx, sy);
					break;
			}
		}
		(void)ox;
		i++;
	}
	// splashes (rain) and hailstones lying a while
	for (size_t i = 0; i < s_splashes.size();) {
		Splash& s = s_splashes[i];
		s.age += dt;
		float life = s.kind == 0 ? 120.0f : (sp == THUNDERSTORM && s_hail ? 14000.0f : 4000.0f);
		if (s.age > life) {
			s_splashes[i] = s_splashes.back();
			s_splashes.pop_back();
			continue;
		}
		auto [sx, sy] = toScreen(s.x, s.y);
		if (sx >= -8 && sx < vw + 8 && sy >= -8 && sy < vh + 8) {
			if (s.kind == 0) {
				surface->renderLine((float)sx, (float)sy - 1, (float)sx - 3, (float)sy - 4, rainCol);
				surface->renderLine((float)sx, (float)sy - 1, (float)sx + 3, (float)sy - 4, rainCol);
			} else
				surface->renderCreaturesImage(*s_stones, 0, sx - 2, sy - 3);
		}
		i++;
	}

	// snow lying on the open ground, deeper the longer it snows, and a cold
	// white haze over the outdoors (owner, 2026-11-27: "a muting/white overlay")
	static float lying = 0;
	if (sp == SNOW)
		lying = std::min(8.0f, lying + sec * 0.25f * mix);
	else
		lying = std::max(0.0f, lying - sec * 0.5f);
	if (lying > 0.3f) {
		for (int sx = 0; sx < vw; sx++) {
			int c = ((sx + vx) % P->W + P->W) % P->W;
			if (P->s0kind[c] != FLOOR && P->s0kind[c] != ROCK)
				continue;
			float h = lying * (0.7f + 0.5f * vnoise((float)(sx + vx) / 23.0f, 1.0f, 61));
			int sy = (int)P->s0[c] - vy;
			if (sy < -10 || sy > vh + 10)
				continue;
			surface->renderLine((float)sx, (float)sy - h, (float)sx, (float)sy - h * 0.45f, Color{252, 253, 255, 255});
			surface->renderLine((float)sx, (float)sy - h * 0.45f, (float)sx, (float)sy + 1, Color{196, 208, 228, 255});
		}
	}
	if (sp == SNOW && mix > 0.02f) {
		// (a checker of pale pixels over the outdoors: the open air and the
		// ground's face just below its surface, none under rock or roofs;
		// made per tile, from the weather's own map, so it is even
		// everywhere: owner, 2026-11-27, "watch for patchiness")
		// (tiles keyed by where they are drawn, not wrapped: the world is
		// not a whole number of tiles wide, so a view across Eden's east
		// edge, which wraps round to its west end, showed a tile's worth of
		// the west end shifted 96 px, and a 96 px strip with no haze at all,
		// owner, 2026-11-27; each column is the world's own, wrapped)
		static std::map<std::pair<int, int>, std::pair<int, std::shared_ptr<creaturesImage>>> tiles;
		static std::map<std::pair<int, int>, std::vector<float>> shares;
		static const Places* tilesFor = nullptr;
		if (tilesFor != P) {
			tiles.clear();
			shares.clear();
			tilesFor = P;
		}
		const int T = 256, level = (int)std::lround(mix * 10);
		for (int ty = std::max(0, vy / T); ty <= (vy + vh) / T && ty * T < P->H; ty++)
			for (int tx = (int)std::floor(vx / (float)T); tx <= (int)std::floor((vx + vw) / (float)T); tx++) {
				const int wtx = tx;
				auto& slot = tiles[{wtx, ty}];
				if (slot.first != level || (!slot.second && level > 0 && slot.first == 0)) {
					// how much haze each pixel takes (0-1), worked out once per
					// tile and kept: down each column to its reach, fading over
					// the last 70 px; and sideways from a column that reaches
					// further, fading over 64 px, so a step between columns (a
					// cliff beside open air) thins out instead of ending in a
					// line (owner, 2026-11-27: "watch for patchiness")
					auto sh = shares.find({wtx, ty});
					if (sh == shares.end()) {
						const int R = 64;
						auto col = [&](int c) { return ((c % P->W) + P->W) % P->W; };
						auto vert = [&](int c, int y) {
							int bottom = P->hazeBottom[c], fade = P->hazeFade[c];
							if (y >= bottom)
								return 0.0f;
							return y <= fade ? 1.0f : 1.0f - (float)(y - fade) / std::max(1, bottom - fade);
						};
						std::vector<float> share(size_t(T) * T, 0.0f);
						for (int xx = 0; xx < T; xx++) {
							int c = col(wtx * T + xx);
							int reach = 0; // the deepest reach within R columns
							for (int k = -R; k <= R; k++)
								reach = std::max(reach, P->hazeBottom[col(c + k)]);
							for (int yy = 0; yy < T; yy++) {
								int y = ty * T + yy;
								if (y >= reach)
									break;
								float v = vert(c, y);
								if (v < 1.0f)
									for (int k = 1; k <= R && v < 1.0f; k++) {
										float w = 1.0f - (float)k / R;
										if (w <= v)
											break;
										v = std::max(v, std::max(vert(col(c - k), y), vert(col(c + k), y)) * w);
									}
								// (Eden: the valley under the long rope bridge ends
								// on the east along the owner's line, from x 3901
								// under the bridge to 3834 at the valley floor,
								// short of the stone arches: owner, 2026-11-27)
								if (P->eden) {
									const int wx = c + P->mx, wy = y + P->my;
									if (wy > 845 && wy < 1770 && wx > 3700 && wx < 4300) {
										float xc = 3901.0f + (3834.0f - 3901.0f) * (wy - 860) / 670.0f;
										v *= std::clamp((xc - wx) / 12.0f, 0.0f, 1.0f);
									}
								}
								share[size_t(yy) * T + xx] = v;
							}
						}
						sh = shares.emplace(std::make_pair(wtx, ty), std::move(share)).first;
					}
					Image im = newImage(T, T);
					bool any = false;
					const float a = 0.34f * (level / 10.0f);
					for (int yy = 0; yy < T; yy++)
						for (int xx = 0; xx < T; xx++) {
							float k = sh->second[size_t(yy) * T + xx];
							if (k > 0 && dither(xx, yy, a * k)) {
								put(im, xx, yy, 232, 238, 248);
								any = true;
							}
						}
					slot = {level, any ? picture("zeetures weather snow haze", {im}) : nullptr};
				}
				if (slot.second)
					surface->renderCreaturesImage(*slot.second, 0, tx * T - vx, ty * T - vy);
			}
	}

	// the rainbow the temple bell brings after a shower (its Easter eggs,
	// above): seven dithered bands in an arc over the temple, only where the
	// painted sky shows, fading in over 10 s and out over 15 s
	if (s_rainbowUntil && world.worldtickcount < s_rainbowUntil) {
		const unsigned int now = world.worldtickcount;
		const float ramp = std::min({1.0f, (now - s_rainbowFrom) / 100.0f, (s_rainbowUntil - now) / 150.0f});
		const int level = (int)std::lround(ramp * 10);
		static std::map<std::pair<int, int>, std::pair<int, std::shared_ptr<creaturesImage>>> rbTiles;
		static const Places* rbFor = nullptr;
		if (rbFor != P) {
			rbTiles.clear();
			rbFor = P;
		}
		const int T = 256;
		const float CX = BELL_X - P->mx, CY = 1150.0f - P->my, RO = 1000.0f, BAND = 9.0f;
		const float RI = RO - 7 * BAND;
		static const int bands[7][3] = {{232, 64, 60}, {242, 150, 52}, {244, 228, 84}, {92, 200, 92}, {72, 132, 232}, {92, 80, 200}, {150, 84, 204}};
		for (int ty = std::max(0, vy / T); ty <= (vy + vh) / T && ty * T < P->H; ty++)
			for (int tx = (int)std::floor(vx / (float)T); tx <= (int)std::floor((vx + vw) / (float)T); tx++) {
				// (a tile the arc misses: nothing)
				float nx = std::clamp(CX, (float)tx * T, (float)tx * T + T), ny = std::clamp(CY, (float)ty * T, (float)ty * T + T);
				float near = std::hypot(nx - CX, ny - CY);
				float far = 0;
				for (int cx : {tx * T, tx * T + T})
					for (int cy : {ty * T, ty * T + T})
						far = std::max(far, std::hypot(cx - CX, cy - CY));
				if (near > RO || far < RI || ty * T > CY)
					continue;
				auto& slot = rbTiles[{tx, ty}];
				if (slot.first != level) {
					Image im = newImage(T, T);
					bool any = false;
					for (int yy = 0; yy < T; yy++)
						for (int xx = 0; xx < T; xx++) {
							int x = tx * T + xx, y = ty * T + yy;
							float d = std::hypot(x - CX, y - CY);
							if (d >= RO || d <= RI || y >= CY)
								continue;
							float tt = (RO - d) / (RO - RI);
							float w = std::sqrt(std::sin(3.14159f * tt)) * std::clamp((CY - 250 - y) / 350.0f, 0.0f, 1.0f);
							int c = ((x % P->W) + P->W) % P->W;
							if (w <= 0 || y < 0 || y >= P->H || !P->precip.get(c, y))
								continue;
							float out = SkyTint::outdoors(m, x + P->mx, y + P->my, x + P->mx + 1, y + P->my + 1);
							if (out >= 0 && out < 0.6f)
								continue;
							if (!dither(xx, yy, 0.5f * w * level / 10.0f))
								continue;
							const int* col = bands[std::clamp((int)(tt * 7), 0, 6)];
							put(im, xx, yy, col[0], col[1], col[2]);
							any = true;
						}
					slot = {level, any ? picture("zeetures weather rainbow", {im}) : nullptr};
				}
				if (slot.second)
					surface->renderCreaturesImage(*slot.second, 0, tx * T - vx, ty * T - vy);
			}
	}

	// the clouds, over everything (owner, 2026-11-27), drifting with the wind
	float want = cloudFor(sp, s_hail) * mix;
	s_cloudShown += (want - s_cloudShown) * std::min(1.0f, sec * 0.5f);
	s_cloudDrift += sec * (sp == WINDSTORM ? 40.0f : 8.0f);
	if (s_cloudShown > 0.02f && vy < DECK_H) {
		const int level = (int)std::lround(s_cloudShown * 20); // (remade in steps of 0.05)
		const int period = ((P->W + TILE - 1) / TILE) * TILE;
		const int tiles = period / TILE;
		// (in steps of 4 px, so its checker stays in step with every other
		// dithered layer's, all on the world's grid: drifting a pixel at a
		// time it shimmered against them; owner, 2026-11-27)
		const float drift = std::floor(std::fmod(s_cloudDrift, (float)period) / 4.0f) * 4.0f;
		for (int sx0 = (int)std::floor((vx + drift) / (float)TILE) * TILE; sx0 < vx + drift + vw; sx0 += TILE) {
			int t = ((sx0 / TILE) % tiles + tiles) % tiles;
			CloudTile& ct = s_cloudTiles[t];
			if (!ct.pic || ct.level != level) {
				ct.pic = makeCloudTile(t, level / 20.0f, period);
				ct.level = level;
			}
			ct.used = s_frame;
			surface->renderCreaturesImage(*ct.pic, 0, (int)std::lround(sx0 - drift - vx), -vy);
		}
	}

	// lightning: the bolt, a second stroke, a flash over the view, the glow
	// where it landed
	for (size_t i = 0; i < s_bolts.size();) {
		Bolt& b = s_bolts[i];
		b.age += dt;
		if (b.age > 3000) {
			s_bolts[i] = s_bolts.back();
			s_bolts.pop_back();
			continue;
		}
		bool shown = b.age < 70 || (b.age > 130 && b.age < 300);
		if (shown) {
			for (size_t s = 0; s < b.strokes.size(); s++) {
				auto& pts = b.strokes[s];
				for (size_t k = 0; k + 1 < pts.size(); k++) {
					auto [ax, ay] = toScreen(pts[k].first, pts[k].second);
					auto [bx, by] = toScreen(pts[k + 1].first, pts[k + 1].second);
					if (s == 0) {
						for (int o : {-2, 2})
							surface->renderLine((float)ax + o, (float)ay, (float)bx + o, (float)by, Color{150, 166, 255, 255});
						surface->renderLine((float)ax + 1, (float)ay, (float)bx + 1, (float)by, Color{255, 255, 255, 255});
					}
					surface->renderLine((float)ax, (float)ay, (float)bx, (float)by, Color{255, 255, 255, 255});
				}
			}
		}
		if (b.age < 90) { // the flash (its checker on the world's grid too)
			const int ox = -(((vx % 64) + 64) % 64), oy = -(((vy % 64) + 64) % 64);
			for (int y = oy; y < vh; y += 64)
				for (int x = ox; x < vw; x += 64)
					surface->renderCreaturesImage(*s_white, 0, x, y);
		}
		if (b.age < 2500 && (b.age < 1200 || ((int)b.age / 120) % 2 == 0)) {
			auto [gx, gy] = toScreen(b.gx, b.gy);
			surface->renderCreaturesImage(*s_glow, 0, gx - 45 - (((gx - 45 + vx) % 4) + 4) % 4, gy - 18 - (((gy - 18 + vy) % 4) + 4) % 4);
		}
		i++;
	}

	// the rain's sound while rain falls in view
	float rainHere = (sp == SHOWERS || sp == LIGHT_SHOWERS || sp == THUNDERSTORM || sp == TORRENTIAL) ? mix : 0.0f;
	if (rainHere > 0.2f && wantRain > 20) {
		if (!s_rainSound)
			s_rainSound = soundmanager.playSound("rai2", true);
		if (s_rainSound)
			s_rainSound.setPosition((float)adjustx + vw / 2.0f, (float)adjusty + vh / 2.0f, 1, 1);
	} else if (s_rainSound) {
		s_rainSound.stop();
		s_rainSound = {};
	}
}

bool shimmering() {
	return engine.version == 2 && on() && s_spell == HEATWAVE && s_mix > 0.05f;
}

// The heatwave's shimmer (owner, 2026-11-27: "I'm not seeing the heatwave
// distortions"): the world, drawn off-screen, is copied onto the window, then
// the air just above the open ground copied again in rows 3 px tall, each
// moved a pixel or two sideways, the most near the ground, waving with time.
// The same row moves the same way the length of the view, so the bands meet.
void shimmer(RenderTarget* window, RenderTarget* drawn, float zoom, int camx, int camy) {
	const int zw = drawn->getWidth(), zh = drawn->getHeight();
	window->blitRenderTarget(drawn, Rect2f(0, 0, zw * zoom, zh * zoom));
	MetaRoom* m = engine.camera ? engine.camera->getMetaRoom() : nullptr;
	Places* P = placesFor(m);
	if (!P)
		return;
	const int vx = camx - P->mx, vy = camy - P->my;
	const float t = ((float)world.worldtickcount + engine.tickFraction()) * 0.1f; // (seconds, at the world's pace)
	const float mix = std::clamp(s_mix, 0.0f, 1.0f);
	const int B = 32, ROW = 3, BAND = 110;
	// speech bubbles stay steady (owner, 2026-11-27): the parts of rows they
	// cover are left where they are
	struct Box {
		int x0, y0, x1, y1;
	};
	std::vector<Box> keep;
	for (auto& a : world.agents)
		if (a && !a->isDying() && dynamic_cast<Bubble*>(a.get())) {
			int ax = (int)a->x - camx, ay = (int)a->y - camy;
			keep.push_back({ax - 3, ay - 3, ax + (int)a->getWidth() + 3, ay + (int)a->getHeight() + 3});
		}
	for (int bx = 0; bx < zw; bx += B) {
		int c = ((vx + bx + B / 2) % P->W + P->W) % P->W;
		if (P->s0kind[c] != FLOOR)
			continue;
		const int ground = (int)P->s0[c] - vy;
		const int top = std::max(0, ground - BAND), bottom = std::min(zh, ground + 2);
		for (int y = top - (((top + vy) % ROW) + ROW) % ROW; y < bottom; y += ROW) {
			if (y < 0)
				continue;
			const float k = std::clamp((float)(y - (ground - BAND)) / BAND, 0.0f, 1.0f) * mix;
			const float wy = (float)(y + vy);
			const float wave = std::sin(t * 2.6f + wy * 0.33f) * 1.3f + std::sin(t * 4.1f - wy * 0.71f) * 0.7f;
			const int dx = (int)std::lround(wave * k);
			if (dx == 0)
				continue;
			// the row's piece, less any part a bubble covers
			std::vector<std::pair<int, int>> pieces = {{bx, std::min(bx + B, zw)}};
			for (auto& k : keep) {
				if (y + ROW <= k.y0 || y >= k.y1)
					continue;
				std::vector<std::pair<int, int>> next;
				for (auto [a, b] : pieces) {
					if (k.x1 <= a || k.x0 >= b) {
						next.push_back({a, b});
						continue;
					}
					if (k.x0 > a)
						next.push_back({a, k.x0});
					if (k.x1 < b)
						next.push_back({k.x1, b});
				}
				pieces.swap(next);
			}
			for (auto [a, b] : pieces)
				if (b > a)
					window->blitRenderTargetPart(drawn, Rect2f((float)a, (float)y, (float)(b - a), (float)ROW), Rect2f((a + dx) * zoom, y * zoom, (b - a) * zoom, ROW * zoom));
		}
	}
}

void save(std::ostream& out) {
	if (s_until == 0)
		return;
	out << "weather-spell " << s_spell << " " << (s_hail ? 1 : 0) << " " << ticksLeft() << " " << s_mix << "\n";
}

bool readLine(const std::string& key, std::istringstream& l) {
	if (key == "weather-spell") {
		int s = 0, h = 0;
		unsigned int left = 0;
		float mix = 1;
		if (l >> s >> h >> left) {
			l >> mix;
			s_spell = std::clamp(s, 0, SPELL_COUNT - 1);
			s_hail = h != 0;
			s_until = world.worldtickcount + std::max(1u, left);
			s_mix = std::clamp(mix, 0.0f, 1.0f);
		}
		return true;
	}
	return false;
}

} // namespace Weather
