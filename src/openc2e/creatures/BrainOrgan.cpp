/*
 *  BrainOrgan.cpp
 *  openc2e
 *
 *  The brain as an organ of the body. See BrainOrgan.h.
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

#include "BrainOrgan.h"

#include "Agent.h"
#include "World.h"
#include "common/devenv.h"
#include "creatures/CreatureAgent.h"
#include "creatures/oldCreature.h"
#include "historyManager.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fmt/core.h>
#include <map>
#include <ostream>

namespace BrainOrgan {

namespace {

// ---- tuning ----
const int atpFloor = 60; // paid only while ATP is at least this
const float asleepShare = 0.75f; // of the cost, asleep
const int glucoseLow = 8; // glucose (70) under this: short of fuel even if ATP holds
const float unpaidDaze = 2.0f; // daze each unpaid run (times the stage's vulnerability)
const float lowGlucoseDaze = 1.0f; // each run short of glucose
const float clearAwake = 0.5f; // daze cleared each paid run (255 in about 3.5 minutes)
const float clearAsleep = 1.0f;
// how hard a shortfall hits by life stage (baby .. senile): the young brain
// uses the largest share of the body's energy (Kuzawa et al. 2014)
const std::array<float, 7> vulnerability{3.0f, 2.0f, 1.2f, 1.0f, 0.8f, 1.2f, 1.5f};

const int ATP = 99, ADP = 100, GLUCOSE = 70;

std::map<std::string, float> s_saved; // moniker -> daze, read from the save

bool watching() {
	static const bool w = devenv("OPENC2E_WATCH_BRAIN") != nullptr;
	return w;
}

} // namespace

bool off() {
	static const bool o = devenv("OPENC2E_NO_BRAIN_ORGAN") != nullptr;
	return o;
}

void step(c2Creature& c) {
	if (off() || !c.isAlive())
		return;
	if (!c.brainLoaded) {
		c.brainLoaded = true;
		// developer aid: OPENC2E_TEST_BRAIN_DAZE=N starts every creature this
		// dazed (to watch it clear)
		static const char* testDaze = devenv("OPENC2E_TEST_BRAIN_DAZE");
		if (testDaze)
			c.brainDaze = std::clamp((float)atof(testDaze), 0.0f, 255.0f);
		if (world.history && !s_saved.empty()) {
			auto it = s_saved.find(world.history->findMoniker(c.getGenome()));
			if (it != s_saved.end()) {
				c.brainDaze = it->second;
				s_saved.erase(it);
			}
		}
	}
	// half the speed of the other organs
	c.brainTurn = !c.brainTurn;
	if (!c.brainTurn)
		return;
	int stage = std::clamp((int)c.getStage(), 0, 6);
	float before = c.brainDaze;
	if (c.getChemical(ATP) >= atpFloor) {
		c.brainCarry += energyCost * (c.isAsleep() ? asleepShare : 1.0f);
		int pay = (int)c.brainCarry;
		c.brainCarry -= pay;
		if (pay > 0) {
			c.subChemical(ATP, (unsigned char)pay);
			c.addChemical(ADP, (unsigned char)pay);
		}
		if (c.getChemical(GLUCOSE) < glucoseLow)
			c.brainDaze += lowGlucoseDaze * vulnerability[stage];
		else
			c.brainDaze -= c.isAsleep() ? clearAsleep : clearAwake;
	} else {
		// spared: the body keeps what ATP it has, and the mind pays instead
		c.brainCarry = 0.0f;
		c.brainDaze += unpaidDaze * vulnerability[stage];
	}
	c.brainDaze = std::clamp(c.brainDaze, 0.0f, 255.0f);
	if (watching() && world.history && ((before < 64) != (c.brainDaze < 64)))
		fmt::print("* BRAIN {} t{} {} (daze {:.0f}, ATP {}, glucose {})\n", world.history->findMoniker(c.getGenome()), world.tickcount,
			c.brainDaze >= 64 ? "dazed" : "clear again", c.brainDaze, (int)c.getChemical(ATP), (int)c.getChemical(GLUCOSE));
}

float daze(Creature* c) {
	auto* oc = dynamic_cast<oldCreature*>(c);
	return oc && !off() ? std::clamp(oc->brainDaze / 255.0f, 0.0f, 1.0f) : 0.0f;
}

float dazeOf(Agent* creatureAgent) {
	auto* ca = dynamic_cast<CreatureAgent*>(creatureAgent);
	return ca ? daze(ca->getCreature()) : 0.0f;
}

void reset() {
	s_saved.clear();
}

void save(std::ostream& out) {
	if (!world.history)
		return;
	for (auto& a : world.agents)
		if (auto* ca = a && !a->isDying() ? dynamic_cast<CreatureAgent*>(a.get()) : nullptr)
			if (auto* c = dynamic_cast<oldCreature*>(ca->getCreature()); c && c->isAlive() && c->brainDaze >= 0.5f) {
				std::string mon = world.history->findMoniker(c->getGenome());
				if (!mon.empty())
					out << fmt::format("brain-daze {} {:.1f}\n", mon, c->brainDaze);
			}
}

bool readLine(const std::string& key, std::istringstream& l) {
	if (key != "brain-daze")
		return false;
	std::string who;
	float d;
	if (l >> who >> d)
		s_saved[who] = std::clamp(d, 0.0f, 255.0f);
	return true;
}

} // namespace BrainOrgan
