/*
 *  Exposure.cpp
 *  openc2e
 *
 *  Radiation and heavy metals in C2 creatures. See Exposure.h.
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

#include "Exposure.h"
#include "common/devenv.h"

#include "Agent.h"
#include "Engine.h"
#include "Map.h"
#include "PathResolver.h"
#include "Room.h"
#include "SpritePart.h"
#include "caosValue.h"
#include "World.h"
#include "common/Random.h"
#include "common/io/FileWriter.h"
#include "creatures/Breeding.h"
#include "creatures/CreatureAgent.h"
#include "creatures/oldCreature.h"
#include "fileformats/genomeFile.h"
#include "historyManager.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fmt/core.h>
#include <map>
#include <ostream>

namespace Exposure {

namespace {

// ---- tuning ----
const unsigned int stepTicks = 50; // every 50 world ticks (5 s)
const float minutesPerStep = stepTicks / 600.0f; // (10 ticks a second)
const float background = 10.0f; // radiation up to this is harmless (most rooms are 0-24)
const float doseHalfLife = 18000.0f; // ticks: a game day
const float fertilityDose = 1000.0f; // the dose that halves the chance to conceive
const float mutationDose = 1500.0f; // radiation-minutes before birth for one more mutation (an egg at the volcano's mouth, 56: about one a game day)
const int mostMutations = 10; // (a child's usual mutations are about three)
const int metalsFelt = 16; // heavy metals above this hurt (the genome's own receptor's threshold)
// (the body's part runs every 10 ticks: pain fades fast, so a pinch every 5 s
// was hardly felt)
const unsigned int bodyTicks = 10;
const float painPerStep = 1.0f / 10.0f; // pain chemical every 10 ticks per heavy metal above 16
const float wearPerStep = 0.2f; // organ injury every 10 ticks at heavy metals 255 (in proportion below)

bool off() {
	static const bool o = devenv("OPENC2E_NO_EXPOSURE") != nullptr;
	return o;
}
bool watching() {
	static const bool w = devenv("OPENC2E_WATCH_EXPOSURE") != nullptr;
	return w;
}

std::map<std::string, float> s_dose; // creature moniker -> radiation dose
std::map<std::string, float> s_unborn; // baby genome name -> dose before birth

std::string nameOf(int v) {
	if (v == 0)
		return "";
	std::string name((const char*)&v, 4);
	for (char ch : name)
		if (!std::isalnum((unsigned char)ch))
			return "";
	return name;
}

float radiationAt(Agent* a) {
	float cx = a->x + a->getWidth() / 2.0f, cy = a->y + a->getHeight() / 2.0f;
	auto r = world.map->roomAt(cx, cy);
	return r ? (float)std::clamp(r->radn, 0, 255) : 0.0f;
}

// the organs the genome marks for heavy metals: those with a receptor on
// chemical 140 that sets their injury (organ locus 2/0/2)
bool marksMetals(c2Organ& o) {
	for (gene* g : o.genes)
		if (auto* rc = dynamic_cast<bioReceptorGene*>(g); rc && rc->chemical == 140 && rc->organ == 2 && rc->locus == 2)
			return true;
	return false;
}

void metals(c2Creature* c, const std::string& mon) {
	int hm = c->getChemical(140);
	if (hm <= metalsFelt)
		return;
	float over = (float)(hm - metalsFelt);
	// felt: a dull pain growing with the level (Pain, chemical 1)
	c->addChemical(1, (unsigned char)std::clamp((int)std::lround(over * painPerStep), 0, 255));
	// and the marked organ worn down
	float wear = wearPerStep * over / (255.0f - metalsFelt);
	for (auto& o : c->getOrgans())
		if (o && o->getLongTermLifeforce() > 0.5f && marksMetals(*o))
			o->applyInjury(wear);
	if (watching() && world.tickcount % 3000 == 0)
		fmt::print("* EXPOSURE {} heavy metals {} pain {}\n", mon, hm, (int)c->getChemical(1));
}

} // namespace

void reset() {
	s_dose.clear();
	s_unborn.clear();
}

void tick() {
	if (off() || engine.version != 2 || !world.map || !world.history)
		return;
	// heavy metals: pain and wear, every 10 ticks
	if (world.tickcount % bodyTicks == 0)
		for (auto& a : world.agents)
			if (auto* ca = a && !a->isDying() ? dynamic_cast<CreatureAgent*>(a.get()) : nullptr)
				if (auto* c = dynamic_cast<c2Creature*>(ca->getCreature()); c && c->isAlive())
					metals(c, world.history->findMoniker(c->getGenome()));
	if (world.tickcount % stepTicks != 0)
		return;
	const float fade = std::pow(0.5f, stepTicks / doseHalfLife);
	std::map<std::string, bool> alive;
	for (auto& a : world.agents) {
		if (!a || a->isDying())
			continue;
		if (auto* ca = dynamic_cast<CreatureAgent*>(a.get())) {
			auto* c = dynamic_cast<c2Creature*>(ca->getCreature());
			if (!c || !c->isAlive())
				continue;
			std::string mon = world.history->findMoniker(c->getGenome());
			if (mon.empty())
				continue;
			alive[mon] = true;
			float extra = std::max(0.0f, radiationAt(a.get()) - background) * minutesPerStep;
			float& d = s_dose[mon];
			d = d * fade + extra;
			// carrying a baby: it shares its mother's radiation
			if (std::string baby = nameOf(a->babymoniker); !baby.empty() && extra > 0)
				s_unborn[baby] += extra;
			continue;
		}
		// an egg not yet hatched (C2: 2 5 x; OBV0 its baby's genome)
		if (a->family == 2 && a->genus == 5 && a->var[0].hasInt()) {
			std::string baby = nameOf(a->var[0].getInt());
			if (baby.empty())
				continue;
			if (auto* sp = dynamic_cast<SpritePart*>(a->part(0)); sp && sp->getPose() == 7)
				continue; // an empty shell
			float extra = std::max(0.0f, radiationAt(a.get()) - background) * minutesPerStep;
			if (extra > 0)
				s_unborn[baby] += extra;
		}
	}
	if (watching() && world.tickcount % 3000 == 0) {
		for (auto& [mon, d] : s_dose)
			if (d >= 50)
				fmt::print("* EXPOSURE {} radiation dose {:.0f} (fertility x{:.2f})\n", mon, d, fertilityFactor(mon));
		for (auto& [name, d] : s_unborn)
			fmt::print("* EXPOSURE unborn {} dose {:.0f}\n", name, d);
	}
	for (auto it = s_dose.begin(); it != s_dose.end();)
		it = (!alive.count(it->first) || it->second < 0.5f) ? s_dose.erase(it) : std::next(it);
}

float doseOf(const std::string& moniker) {
	auto it = s_dose.find(moniker);
	return it == s_dose.end() ? 0.0f : it->second;
}

float fertilityFactor(const std::string& moniker) {
	if (off())
		return 1.0f;
	return 1.0f / (1.0f + doseOf(moniker) / fertilityDose);
}

std::shared_ptr<genomeFile> atHatching(const std::string& name, std::shared_ptr<genomeFile> g) {
	auto it = s_unborn.find(name);
	if (off() || !g || it == s_unborn.end())
		return g;
	float dose = it->second;
	s_unborn.erase(it);
	// each share of the dose one more mutation; the remainder a chance of one
	// (developer aid: OPENC2E_EXPOSURE_MUTATION_DOSE=N, the dose for one mutation, for tests)
	static const float perMutation = devenv("OPENC2E_EXPOSURE_MUTATION_DOSE") ? (float)atof(devenv("OPENC2E_EXPOSURE_MUTATION_DOSE")) : mutationDose;
	float shares = dose / std::max(perMutation, 1.0f);
	int count = (int)shares + (rand_float(0, 1) < shares - std::floor(shares) ? 1 : 0);
	count = std::min(count, mostMutations);
	if (count <= 0)
		return g;
	int made = 0;
	auto mutated = mutatedCopy(*g, count, &made);
	if (!mutated || made <= 0)
		return g;
	// kept in its genome file, so its own children inherit them
	try {
		std::filesystem::path dir = getUserDataDir() / "Genetics";
		FileWriter f(dir / (name + ".gen"));
		f << *mutated;
	} catch (std::exception& e) {
		fmt::print("* Couldn't write {}'s genome after radiation: {}\n", name, e.what());
	}
	fmt::print("* {} hatched with {} more mutation{} from radiation before birth (dose {:.0f})\n", name, made, made == 1 ? "" : "s", dose);
	return mutated;
}

void save(std::ostream& out) {
	for (auto& [mon, d] : s_dose)
		if (d >= 0.5f)
			out << fmt::format("exposure-dose {} {:.1f}\n", mon, d);
	for (auto& [name, d] : s_unborn)
		if (d >= 0.5f)
			out << fmt::format("exposure-unborn {} {:.1f}\n", name, d);
}

bool readLine(const std::string& key, std::istringstream& l) {
	std::string who;
	float d;
	if (key == "exposure-dose") {
		if (l >> who >> d)
			s_dose[who] = d;
	} else if (key == "exposure-unborn") {
		if (l >> who >> d)
			s_unborn[who] = d;
	} else
		return false;
	return true;
}

} // namespace Exposure
