/*
 *  Kinds.cpp
 *  openc2e
 *
 *  Grendels' and ettins' roles: see Kinds.h.
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

#include "creatures/Kinds.h"

#include "Agent.h"
#include "World.h"
#include "common/devenv.h"
#include "creatures/CreatureAgent.h"
#include "creatures/Pheromones.h"
#include "creatures/oldCreature.h"
#include "fileformats/genomeFile.h"
#include "historyManager.h"

#include <algorithm>
#include <cmath>
#include <fmt/core.h>
#include <map>
#include <ostream>

namespace Kinds {

namespace {

// wariness: gained on falling ill within this long of being near a grendel,
// by this much, and fading by half in about an hour of play
const unsigned int NEAR_GRENDEL_TICKS = 3000; // five minutes
const float WARY_GAIN = 0.5f, WARY_AT = 0.25f;
const float WARY_FADE = 0.999f; // a step of 50 ticks (half in about 35,000 ticks)
const float NEAR_GRENDEL_SCENT = 0.05f, NEAR_GRENDEL_PX = 300.0f;
// an ettin's hoard: a place where it has been at ease, uncrowded, for this
// long (Crowded, chemical 9, under this)
const unsigned int HOARD_QUIET_TICKS = 600;
const int HOARD_CROWDED_UNDER = 40;

std::map<std::string, float> s_wary;
std::map<std::string, unsigned int> s_nearGrendel; // moniker: last tick near a grendel
struct Spot {
	float x = 0, y = 0;
};
std::map<std::string, Spot> s_hoards; // ettin moniker: its hoard
std::map<std::string, unsigned int> s_quietSince;
bool s_mate[4][2] = {}; // [agent genus][female]: an adolescent-to-old lives

oldCreature* creatureOf(Agent* a) {
	CreatureAgent* ca = dynamic_cast<CreatureAgent*>(a);
	return ca ? dynamic_cast<oldCreature*>(ca->getCreature()) : nullptr;
}

std::string monikerOf(oldCreature* c) {
	return c && world.history ? world.history->findMoniker(c->getGenome()) : std::string();
}

} // namespace

bool off() {
	static const bool o = devenv("OPENC2E_NO_KINDS") != nullptr;
	return o;
}

bool watching() {
	static const bool w = devenv("OPENC2E_WATCH_KINDS") != nullptr;
	return w;
}

bool reproGenes(const genomeFile* g) {
	if (!g)
		return false;
	static std::map<const genomeFile*, bool> cache;
	auto it = cache.find(g);
	if (it != cache.end())
		return it->second;
	bool any = false;
	for (auto& gn : g->genes)
		if (auto* r = dynamic_cast<bioReceptorGene*>(gn.get()))
			if (r->organ == 1 && r->tissue == 2 && !r->header.flags.notexpressed)
				any = true;
	if (cache.size() > 4000)
		cache.clear();
	cache[g] = any;
	return any;
}

bool isBoney(oldCreature* c) {
	return c && c->getGenus() == 1 && reproGenes(c->getGenome().get());
}

bool isBulbous(oldCreature* c) {
	return c && c->getGenus() == 1 && !reproGenes(c->getGenome().get());
}

bool sterileGenes(oldCreature* c) {
	return c && (c->getGenus() == 1 || c->getGenus() == 2) && !reproGenes(c->getGenome().get());
}

bool mateAlive(int agentGenus, bool female) {
	return agentGenus >= 1 && agentGenus <= 3 && s_mate[agentGenus][female ? 1 : 0];
}

float wariness(oldCreature* c) {
	auto it = s_wary.find(monikerOf(c));
	return it == s_wary.end() ? 0.0f : it->second;
}

bool wary(oldCreature* c) {
	return !off() && wariness(c) >= WARY_AT;
}

void noteCaught(oldCreature* c) {
	if (off() || !c || c->getGenus() == 1)
		return; // (grendels carry germs without falling ill)
	std::string mon = monikerOf(c);
	auto it = s_nearGrendel.find(mon);
	if (mon.empty() || it == s_nearGrendel.end() || world.tickcount > it->second + NEAR_GRENDEL_TICKS)
		return;
	float& w = s_wary[mon];
	w = std::min(1.0f, w + WARY_GAIN);
	if (watching())
		fmt::print("* KINDS {} fell ill soon after being near a grendel: wariness {:.2f}\n", mon, w);
}

void tick() {
	if (off() || world.tickcount % 50 != 0)
		return;
	// who could be a mate, and who is near a grendel
	bool mate[4][2] = {};
	std::vector<std::pair<float, float>> grendels;
	for (auto& a : world.agents) {
		oldCreature* c = a && !a->isDying() ? creatureOf(a.get()) : nullptr;
		if (!c || !c->isAlive())
			continue;
		int st = (int)c->getStage();
		if (a->genus >= 1 && a->genus <= 3 && st >= 2 && st <= 5)
			mate[a->genus][c->isFemale() ? 1 : 0] = true;
		if (a->genus == 2)
			grendels.push_back({a->x + a->getWidth() / 2.0f, a->y + a->getHeight() / 2.0f});
	}
	std::copy(&mate[0][0], &mate[0][0] + 8, &s_mate[0][0]);
	for (auto& [mon, w] : s_wary)
		w *= WARY_FADE;
	std::vector<Pheromones::PlaceEmission> places;
	for (auto& a : world.agents) {
		oldCreature* c = a && !a->isDying() ? creatureOf(a.get()) : nullptr;
		if (!c || !c->isAlive())
			continue;
		std::string mon = monikerOf(c);
		if (mon.empty())
			continue;
		float cx = a->x + a->getWidth() / 2.0f, cy = a->y + a->getHeight() / 2.0f;
		if (a->genus != 2) {
			bool near = Pheromones::levelAt("grendel-kind", a.get()) > NEAR_GRENDEL_SCENT;
			for (auto& [gx, gy] : grendels)
				near = near || std::hypot(gx - cx, gy - cy) < NEAR_GRENDEL_PX;
			if (near)
				s_nearGrendel[mon] = world.tickcount;
		}
		// an ettin finds its hoard spot: somewhere it has been at ease
		if (a->genus == 3 && c->getStage() >= 2 && !s_hoards.count(mon) && !a->carriedby && !a->invehicle) {
			if (c->getChemical(9) < HOARD_CROWDED_UNDER) {
				auto q = s_quietSince.emplace(mon, world.tickcount).first;
				if (world.tickcount >= q->second + HOARD_QUIET_TICKS) {
					s_hoards[mon] = {cx, a->y + a->getHeight() - 4.0f};
					if (watching())
						fmt::print("* KINDS ettin {} made its hoard at {:.0f},{:.0f}\n", mon, cx, a->y + a->getHeight());
				}
			} else
				s_quietSince.erase(mon);
		}
	}
	for (auto& [mon, spot] : s_hoards)
		places.push_back({"ettin-hoard", spot.x, spot.y, 1.0f});
	Pheromones::setPlaceEmissions(places);
}

void reset() {
	s_wary.clear();
	s_nearGrendel.clear();
	s_hoards.clear();
	s_quietSince.clear();
	std::fill(&s_mate[0][0], &s_mate[0][0] + 8, false);
	Pheromones::setPlaceEmissions({});
}

void save(std::ostream& out) {
	for (auto& [mon, w] : s_wary)
		if (w >= 0.01f)
			out << "kinds-wary " << mon << " " << w << "\n";
	for (auto& [mon, s] : s_hoards)
		out << "kinds-hoard " << mon << " " << s.x << " " << s.y << "\n";
}

bool readLine(const std::string& key, std::istringstream& l) {
	std::string mon;
	if (key == "kinds-wary") {
		float w;
		if (l >> mon >> w)
			s_wary[mon] = std::clamp(w, 0.0f, 1.0f);
		return true;
	}
	if (key == "kinds-hoard") {
		Spot s;
		if (l >> mon >> s.x >> s.y)
			s_hoards[mon] = s;
		return true;
	}
	return false;
}

std::string kindName(oldCreature* c) {
	if (!c)
		return {};
	if (c->getGenus() == 1)
		return isBoney(c) ? "Boney grendel" : "Bulbous grendel";
	if (c->getGenus() == 2)
		return "Ettin";
	return {};
}

// (player-visible: British spelling)
std::vector<std::string> traits(oldCreature* c) {
	std::vector<std::string> t;
	if (!c || off())
		return t;
	Agent* a = c->getAgent();
	int g = c->getGenus();
	if (g == 1 && !isBoney(c)) {
		t.push_back("Carries germs without falling ill, and spreads them.");
		t.push_back("Never tires; prowls at night, and lies up at the grendels' lair by day.");
		t.push_back("Angry at norns, and a bully to ettins; won over by long kindness, it may defend its friends.");
		t.push_back("Takes eggs it finds to the lair.");
	} else if (g == 1) {
		t.push_back("Carries germs without falling ill, and spreads them.");
		t.push_back("About by day, home to the lair at night; follows others of its kind in a pack.");
		t.push_back("A scavenger: goes for the weak, the young, the ill and the dead, and runs from the strong.");
		t.push_back("A thief: snatches food and runs home with it.");
		t.push_back("Learns quickly, and can be won over more easily than a bulbous grendel.");
		t.push_back("Mothers stay near the eggs.");
	} else if (g == 2) {
		t.push_back("Gathers gadgets and toys, and leaves them in a hoard somewhere quiet.");
		t.push_back("Carries eggs left lying about to the incubator.");
		t.push_back("Drawn to machines, and switches them on.");
		t.push_back("Carries food to where creatures are hungry.");
		t.push_back("Keeps away from crowds and from grendels.");
	}
	if ((g == 1 || g == 2) && a) {
		bool female = c->isFemale();
		int st = (int)c->getStage();
		std::string kind = g == 1 ? "grendel" : "ettin";
		if (st < 2)
			t.push_back("Too young to have young.");
		else if (st > 5)
			t.push_back("Too old to have young.");
		else if (mateAlive(a->genus, !female))
			t.push_back(fmt::format("Fertile: {} {} of the other sex lives.", kind == "ettin" ? "an" : "a", kind));
		else
			t.push_back(fmt::format("Not fertile until {} {} of the other sex appears.", kind == "ettin" ? "an" : "a", kind));
	}
	if (g == 0 && wary(c))
		t.push_back("Wary of grendels: it fell ill after being near one, and keeps away from their scent.");
	return t;
}

} // namespace Kinds
