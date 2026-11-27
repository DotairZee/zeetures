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
#include "PathResolver.h"
#include "common/Random.h"
#include "creatures/Breeding.h"
#include "fileformats/genomeFile.h"
#include "historyManager.h"
#include "common/io/FileReader.h"
#include "common/io/FileWriter.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
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
// C1 grendels' lone eggs: whether each adult was found to lay one (decided
// once, when first seen adult), and the tick it will
struct Lone {
	bool lays = false, done = false;
	unsigned int at = 0;
};
std::map<std::string, Lone> s_lone;
const unsigned int LONE_EARLIEST = 3000, LONE_LATEST = 18000; // 5-30 minutes after

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
	return c && c->getGenus() == 1 && !reproGenes(c->getGenome().get()) && !isC1Genome(c->getGenome().get());
}

bool sterileGenes(oldCreature* c) {
	return c && (c->getGenus() == 1 || c->getGenus() == 2) && !reproGenes(c->getGenome().get()) && !isC1(c);
}

bool isC1Genome(const genomeFile* g) {
	if (!g)
		return false;
	static std::map<const genomeFile*, bool> cache;
	auto it = cache.find(g);
	if (it != cache.end())
		return it->second;
	bool mark = false;
	for (auto& gn : g->genes)
		if (auto* ic = dynamic_cast<bioInitialConcentrationGene*>(gn.get()))
			if (ic->chemical == C1_MARK_CHEMICAL && ic->quantity == C1_MARK_AMOUNT)
				mark = true;
	if (cache.size() > 4000)
		cache.clear();
	cache[g] = mark;
	return mark;
}

bool isC1(oldCreature* c) {
	return c && c->getGenus() == 1 && isC1Genome(c->getGenome().get());
}

bool carriesGerms(Agent* a) {
	return a && a->genus == 2 && !isC1(creatureOf(a));
}

bool deafToHand(oldCreature* c) {
	return !off() && isC1(c);
}

// The C1 grendel genome (owner, 2026-11-27; reports/C1 grendel genome
// plan.md). Our own recipe on the player's gren.gen, read as data; each
// change is one of the eight traits:
// - looks: every appearance gene on the C1 pictures' grendel slot;
// - glutton: the hunger cells' emitters (organ 0, chemical 3 Hunger) at
//   twice the gain, and Hunger Decrease (37) fading faster (half-life 24,
//   was 40), so food eases it less;
// - deaf to the hand: the hand's speech (stimulus 10) does nothing, and the
//   obedience instincts (heard verb) go with the rest below;
// - touchy loner: another creature's speech (stimulus 11) angers it as much
//   as its pats do (Anger at least 48; C2's grendel already angers at pats
//   and slaps from creatures);
// - fearless: Fear (10) fades at once (half-life 16), as Stress already does;
// - about by day, asleep at night: Tiredness (6) lingers as a norn's (248,
//   was 16); its genes already make it sleepy in the dark;
// - blank slate: three instincts only (hit a norn when angry, eat when
//   hungry, rest when sleepy);
// - mostly barren: C2's grendel's reproduction stays off (lone eggs: tick);
// - and the marker gene (isC1Genome).
bool makeC1Genome(genomeFile& g, int slot) {
	if (g.getVersion() != 2)
		return false;
	bool hit = false, eat = false, rest = false;
	std::vector<std::unique_ptr<gene>> kept;
	for (auto& gn : g.genes) {
		if (auto* a = dynamic_cast<creatureAppearanceGene*>(gn.get())) {
			a->variant = (uint8_t)slot;
			a->species = 1;
		} else if (auto* in = dynamic_cast<creatureInstinctGene*>(gn.get())) {
			bool keep = false;
			if (!hit && in->action == 13 && in->lobes[0] == 2 && in->neurons[0] == 36 && in->lobes[1] == 1 && in->neurons[1] == 11)
				keep = hit = true;
			else if (!eat && in->action == 12 && in->lobes[0] == 1 && in->neurons[0] == 2 && in->lobes[1] == 0)
				keep = eat = true;
			else if (!rest && in->action == 9 && in->lobes[0] == 1 && in->neurons[0] == 6 && in->lobes[1] == 0)
				keep = rest = true;
			if (!keep)
				continue;
		} else if (auto* st = dynamic_cast<creatureStimulusGene*>(gn.get())) {
			if (st->stim % 46 == 10)
				for (int i = 0; i < 4; i++)
					st->drives[i] = st->amounts[i] = 0;
			if (st->stim % 46 == 11)
				for (int i = 0; i < 4; i++)
					if (st->drives[i] == 12 && st->amounts[i] < 48)
						st->amounts[i] = 48;
		} else if (auto* h = dynamic_cast<bioHalfLivesGene*>(gn.get())) {
			h->halflives[6] = 248;
			h->halflives[10] = 16;
			h->halflives[37] = 24;
		} else if (auto* e = dynamic_cast<bioEmitterGene*>(gn.get())) {
			if (e->organ == 0 && e->chemical == 3)
				e->gain = (uint8_t)std::min(255, e->gain * 2);
		}
		kept.push_back(std::move(gn));
	}
	g.genes = std::move(kept);
	auto mark = std::make_unique<bioInitialConcentrationGene>(g.getVersion());
	mark->chemical = C1_MARK_CHEMICAL;
	mark->quantity = C1_MARK_AMOUNT;
	mark->header.flags._mutable = mark->header.flags.dupable = mark->header.flags.delable = false;
	mark->header.flags.notexpressed = true;
	mark->note.which = 1;
	g.genes.push_back(std::move(mark));
	return hit && eat && rest;
}

bool writeC1Genome(int slot, std::string& why) {
	// the game's own grendel genome, never one the player put in their own
	// Genetics folder under its name
	std::filesystem::path src;
	const std::string user = getUserDataDir().lexically_normal().string();
	for (auto& f : findGeneticsFiles("gren.gen"))
		if (f.lexically_normal().string().rfind(user, 0) != 0)
			src = f;
	if (src.empty()) {
		why = "the game's grendel genome (gren.gen) was not found";
		return false;
	}
	genomeFile g;
	try {
		FileReader in(src);
		in >> g;
	} catch (std::exception& e) {
		why = fmt::format("couldn't read gren.gen: {}", e.what());
		return false;
	}
	if (!makeC1Genome(g, slot)) {
		why = "gren.gen is not the genome the recipe expects";
		return false;
	}
	std::filesystem::path dir = getUserDataDir() / "Genetics";
	std::error_code ec;
	std::filesystem::create_directories(dir, ec);
	try {
		FileWriter out(dir / (std::string(C1_GENOME) + ".gen"));
		out << g;
	} catch (std::exception& e) {
		why = fmt::format("couldn't write {}.gen: {}", C1_GENOME, e.what());
		return false;
	}
	return true;
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
		// (C1 grendels are no mate for the others, and no carrier to keep
		// away from)
		const bool c1 = isC1(c);
		if (a->genus >= 1 && a->genus <= 3 && st >= 2 && st <= 5 && !c1)
			mate[a->genus][c->isFemale() ? 1 : 0] = true;
		if (a->genus == 2 && !c1)
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
		// a C1 grendel's lone egg: decided once, when first seen adult;
		// about one adult in three lays one (females only, two in three of
		// them), 5-30 minutes later, through the usual carrying and laying
		if (isC1(c) && c->getStage() >= 4 && c->getStage() <= 5) {
			auto [it, fresh] = s_lone.emplace(mon, Lone{});
			Lone& l = it->second;
			if (fresh) {
				// (developer aid: OPENC2E_TEST_LONE_EGG, every grown female
				// lays one, 100 ticks later)
				static const bool test = devenv("OPENC2E_TEST_LONE_EGG") != nullptr;
				l.lays = c->isFemale() && (test || rand_float(0, 1) < 2.0f / 3.0f);
				l.at = world.tickcount + (test ? 100 : LONE_EARLIEST + (unsigned int)rand_int32(0, LONE_LATEST - LONE_EARLIEST));
				if (watching())
					fmt::print("* KINDS C1 grendel {} grown: {}\n", mon, l.lays ? fmt::format("will lay a lone egg at tick {}", l.at) : std::string("no lone egg"));
			}
			if (l.lays && !l.done && world.tickcount >= l.at) {
				auto* ca = dynamic_cast<CreatureAgent*>(a.get());
				if (ca && layLoneEgg(ca)) {
					l.done = true;
					if (watching())
						fmt::print("* KINDS C1 grendel {} carries a lone egg\n", mon);
				} else
					l.at = world.tickcount + 600; // (not now: a minute later)
			}
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
	s_lone.clear();
	std::fill(&s_mate[0][0], &s_mate[0][0] + 8, false);
	Pheromones::setPlaceEmissions({});
}

void save(std::ostream& out) {
	for (auto& [mon, w] : s_wary)
		if (w >= 0.01f)
			out << "kinds-wary " << mon << " " << w << "\n";
	for (auto& [mon, s] : s_hoards)
		out << "kinds-hoard " << mon << " " << s.x << " " << s.y << "\n";
	for (auto& [mon, l] : s_lone)
		out << "kinds-lone " << mon << " " << (int)l.lays << " " << (int)l.done << " " << l.at << "\n";
}

bool readLine(const std::string& key, std::istringstream& l) {
	std::string mon;
	if (key == "kinds-wary") {
		float w;
		if (l >> mon >> w)
			s_wary[mon] = std::clamp(w, 0.0f, 1.0f);
		return true;
	}
	if (key == "kinds-lone") {
		Lone lo;
		int lays = 0, done = 0;
		if (l >> mon >> lays >> done >> lo.at) {
			lo.lays = lays != 0;
			lo.done = done != 0;
			s_lone[mon] = lo;
		}
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
		return isC1(c) ? C1_KIND_NAME : isBoney(c) ? "Boney grendel" : "Bulbous grendel";
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
	if (g == 1 && isC1(c)) {
		t.push_back("A glutton: always hungry, and a thief that carries food off to the lair.");
		t.push_back("Deaf to the hand: learns no words from it and takes no orders.");
		t.push_back("A loner: other creatures' pats, slaps and chatter anger it; only ettins are company.");
		t.push_back("Fearless, and never stressed.");
		t.push_back("Clean: carries no germs, though it can fall ill.");
		t.push_back("About by day, asleep at night.");
		t.push_back("Born knowing little, and learns quickly.");
		t.push_back("Mostly barren: now and then a grown female lays a lone egg of her own.");
		return t;
	}
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
