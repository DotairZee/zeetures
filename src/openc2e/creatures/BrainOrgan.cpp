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
#include "Engine.h"
#include "World.h"
#include "common/Random.h"
#include "common/devenv.h"
#include "creatures/CreatureAgent.h"
#include "creatures/CreatureMind.h"
#include "creatures/CreatureSpeech.h"
#include "creatures/Lightning.h"
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

// ---- energy ----
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
const int ATP = 99, ADP = 100, GLUCOSE = 70, OXYGEN = 95;

// ---- life ----
const unsigned int lifeTicks = 50; // worked out every 5 s
const float hoursPerStep = lifeTicks / 36000.0f; // (10 ticks a second)
// plasticity by life stage (baby .. senile)
const std::array<float, 7> plasticity{1.0f, 0.8f, 0.5f, 0.25f, 0.12f, 0.06f, 0.03f};
const float baseGrowth = 14.0f; // an hour of living, times plasticity
const float perTurn = 1.5f; // a turn of conversation, times plasticity
const float perOneWay = 0.5f; // spoken to without answering
const std::array<float, 7> careWeight{1.0f, 0.5f, 0.15f, 0, 0, 0, 0};
const float perCare = 1.2f; // a tickle, times careWeight
const float saturation = 10.0f; // recent turns (or care) halving each one's worth
const float recentHalfLife = 6000.0f; // ticks (10 minutes)
const unsigned int answerWithin = 150; // ticks (15 s): a reply makes a turn
const float pendingHalfLifeYoung = 9000.0f, pendingHalfLifeOlder = 21600.0f; // ticks awake (15, 36 min)
// Reserve: a new word, a new place, a new acquaintance, by life stage
const std::array<float, 7> noveltyWeight{0.25f, 1.0f, 1.0f, 1.0f, 0.5f, 0.25f, 0.25f};
const float perWord = 1.0f, perPlace = 0.3f, perPartner = 1.0f;
const float partnerFamiliar = 0.2f; // a bond this familiar is an acquaintance
const float reserveWear = 2.0f; // an hour of long boredom or loneliness
// Damage from age, an hour, by life stage
const std::array<float, 7> ageing{0, 0, 0, 0.2f * 14, 0.4f * 14, 1.0f * 14, 3.5f * 14};
const float trophinHalfLife = 6000.0f; // ticks (10 minutes)
const float trophinActive = 2500.0f; // this much: exercise's whole effect (a Home copy: creatures' medians 500-1500, the busiest 2900)
const float hypoxiaGrace = 75.0f; // brain runs (30 s) with oxygen gone
const float hypoxiaDamage = 0.6f; // each run past it
const int oxygenGone = 20;
// Stress: inputs an hour (each 0-1), cleared with a half-life
const float stressGain = 4.0f;
const float stressHalfLife = 6000.0f, stressHalfLifeAsleep = 2400.0f; // ticks
const float chronic = 0.5f;
const float perSlap = 0.08f, perHarm = 0.05f, perComfort = 0.05f;
const float resiliencePerEpisode = 3.0f; // stress that passed within 5 minutes, times plasticity
const float resilienceChronicLoss = 20.0f; // an hour chronic, times plasticity
const float resiliencePerCare = 0.3f; // a tickle, times careWeight

bool watching() {
	static const bool w = devenv("OPENC2E_WATCH_BRAIN") != nullptr;
	return w;
}

struct Saved {
	float daze = 0;
	Life life;
	bool hasLife = false;
};
std::map<std::string, Saved> s_saved; // moniker -> what the save had

float fade(float halfLife, float ticks) {
	return std::pow(0.5f, ticks / halfLife);
}

oldCreature* oldOf(Creature* c) {
	return dynamic_cast<oldCreature*>(c);
}

std::string monikerOf(oldCreature* c) {
	return world.history ? world.history->findMoniker(c->getGenome()) : std::string();
}

// starting values for a creature first seen (a newborn, or one already
// grown when this came in: what an ordinary life would have given it)
void startLife(oldCreature* c) {
	Life& L = c->brainLife;
	L.set = true;
	int stage = std::clamp((int)c->getStage(), 0, 6);
	static const std::array<float, 7> grown{0, 30, 50, 60, 80, 90, 90};
	static const std::array<float, 7> reserve{0, 10, 30, 50, 80, 100, 100};
	static const std::array<float, 7> damage{0, 0, 0, 5, 30, 70, 110};
	L.growth = 100.0f + rand_float(-10.0f, 10.0f) + grown[stage];
	L.reserve = reserve[stage];
	L.damage = damage[stage];
	L.resilience = 128.0f + rand_float(-10.0f, 10.0f);
	// (developer aid: OPENC2E_TEST_BRAIN="growth,reserve,damage,resilience")
	if (const char* t = devenv("OPENC2E_TEST_BRAIN")) {
		float v[4] = {L.growth, L.reserve, L.damage, L.resilience};
		std::string s = t;
		size_t at = 0;
		for (int k = 0; k < 4 && at <= s.size(); k++) {
			size_t comma = s.find(',', at);
			std::string part = s.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
			if (!part.empty())
				v[k] = (float)atof(part.c_str());
			if (comma == std::string::npos)
				break;
			at = comma + 1;
		}
		L.growth = v[0], L.reserve = v[1], L.damage = v[2], L.resilience = v[3];
	}
}

void load(oldCreature* c) {
	if (c->brainLoaded)
		return;
	c->brainLoaded = true;
	// developer aid: OPENC2E_TEST_BRAIN_DAZE=N starts every creature this
	// dazed (to watch it clear)
	static const char* testDaze = devenv("OPENC2E_TEST_BRAIN_DAZE");
	if (testDaze)
		c->brainDaze = std::clamp((float)atof(testDaze), 0.0f, 255.0f);
	if (!s_saved.empty()) {
		auto it = s_saved.find(monikerOf(c));
		if (it != s_saved.end()) {
			c->brainDaze = it->second.daze;
			if (it->second.hasLife)
				c->brainLife = it->second.life;
			s_saved.erase(it);
		}
	}
	if (!c->brainLife.set)
		startLife(c);
}

int partnersOf(oldCreature* c) {
	int n = 0;
	for (auto& [who, b] : c->bonds)
		if (b.familiarity >= partnerFamiliar)
			n++;
	return n;
}

// the 5 s update of one creature's brain life
void live(oldCreature* c) {
	Life& L = c->brainLife;
	const int stage = std::clamp((int)c->getStage(), 0, 6);
	const bool asleep = c->isAsleep();
	const float P = plasticity[stage];
	const float dz = std::clamp(c->brainDaze / 255.0f, 0.0f, 1.0f);
	auto drive = [&](int i) { return c->getDrive(i) / 255.0f; };

	// talk spoken to it and never answered: one-way
	if (!L.spokenAnswered && world.tickcount - L.spokenAt > answerWithin) {
		L.spokenAnswered = true;
		L.oneWay += 1.0f;
	}

	// ---- stress ----
	float in = 0.25f * drive(9) + 0.25f * drive(0) + 0.1f * drive(11);
	if (drive(2) > 0.8f)
		in += 0.15f * drive(2);
	if (drive(7) > 0.8f)
		in += 0.15f * drive(7);
	const float scale = fearScale(c);
	float before = L.stress;
	L.stress += (in * stressGain * hoursPerStep + L.slaps * perSlap + L.harm * perHarm) * scale;
	float hl = asleep ? stressHalfLifeAsleep : stressHalfLife;
	if (c->companyCount > 0)
		hl *= 0.7f; // company calms
	L.stress *= fade(hl, (float)lifeTicks);
	L.stress -= L.care * perComfort;
	L.stress = std::clamp(L.stress, 0.0f, 1.0f);
	const bool isChronic = L.stress > chronic;
	if (stage <= 2) {
		// the stress system is set while young (then fixed)
		if (before <= 0.2f && L.stress > 0.2f)
			L.stressRoseAt = world.tickcount;
		if (L.stressRoseAt && L.stress < 0.1f) {
			if (world.tickcount - L.stressRoseAt < 3000 && !L.wasChronic)
				L.resilience += resiliencePerEpisode * P; // it passed: inoculating
			L.stressRoseAt = 0;
			L.wasChronic = false;
		}
		if (isChronic) {
			L.resilience -= resilienceChronicLoss * P * hoursPerStep;
			L.wasChronic = true;
		}
		L.resilience += L.care * resiliencePerCare * careWeight[stage];
		L.resilience = std::clamp(L.resilience, 0.0f, 255.0f);
	}

	// ---- growth ----
	L.recentTurns *= fade(recentHalfLife, (float)lifeTicks);
	L.recentCare *= fade(recentHalfLife, (float)lifeTicks);
	float gain = baseGrowth * P * hoursPerStep;
	if (L.turns > 0 || L.oneWay > 0) {
		gain += (L.turns * perTurn + L.oneWay * perOneWay) * P / (1.0f + L.recentTurns / saturation);
		L.recentTurns += L.turns + L.oneWay / 3.0f;
	}
	if (L.care > 0 && careWeight[stage] > 0) {
		gain += L.care * perCare * careWeight[stage] / (1.0f + L.recentCare / saturation);
		L.recentCare += L.care;
	}
	gain *= 1.0f - dz; // unfed, it builds nothing
	if (isChronic && stage <= 2)
		gain *= 0.5f;
	L.pending += gain;
	if (asleep) {
		L.growth = std::min(255.0f, L.growth + L.pending); // kept
		L.pending = 0;
	} else
		L.pending *= fade(stage <= 1 ? pendingHalfLifeYoung : pendingHalfLifeOlder, (float)lifeTicks);

	// ---- reserve ----
	float nov = noveltyWeight[stage];
	L.reserve += L.newWords * perWord * nov;
	if (CreatureMind* m = c->getMind()) {
		int places = m->placesKnown();
		if (places >= 0) {
			if (L.places >= 0 && places > L.places)
				L.reserve += (places - L.places) * perPlace * nov;
			L.places = places;
		}
	}
	int partners = partnersOf(c);
	if (L.partners >= 0 && partners > L.partners && stage >= 1)
		L.reserve += (partners - L.partners) * perPartner * 0.5f;
	L.partners = partners;
	if (!asleep && (drive(10) > 0.7f || drive(7) > 0.7f))
		L.reserve -= reserveWear * hoursPerStep;
	L.reserve = std::clamp(L.reserve, 0.0f, 255.0f);

	// ---- damage from age ----
	L.trophin = L.trophin * fade(trophinHalfLife, (float)lifeTicks) + L.steps;
	if (!Lightning::immortal(c->getAgent())) {
		float active = std::clamp(L.trophin / trophinActive, 0.0f, 1.0f);
		float m = 1.0f - 0.6f * active;
		if (isChronic && stage >= 5)
			m += 0.6f;
		if (stage >= 4 && drive(7) > 0.7f)
			m += 0.3f;
		m = std::max(m, 0.3f);
		L.damage = std::min(255.0f, L.damage + ageing[stage] * m * hoursPerStep);
	}

	// ---- symptoms: words go last ----
	float s = symptoms(c);
	if (s > 0.4f && !asleep && rand_float(0, 1) < s * 0.1f) {
		std::vector<size_t> known;
		for (size_t w = 0; w < c->vocab.size(); w++)
			if (c->vocab[w] >= WORD_KNOWN)
				known.push_back(w);
		if (!known.empty()) {
			size_t w = known[rand_size_t(0, known.size() - 1)];
			c->vocab[w] = (unsigned char)std::max(0, (int)c->vocab[w] - 40);
		}
	}

	if (watching() && world.tickcount % 6000 == 0)
		fmt::print("* BRAIN LIFE {} t{} stage {} growth {:.0f} (pending {:.1f}) reserve {:.0f} damage {:.1f} resilience {:.0f} stress {:.2f} trophin {:.0f} turns {:.1f} care {:.1f} capacity x{:.2f} symptoms {:.2f} places {} words {}\n",
			monikerOf(c), world.tickcount, stage, L.growth, L.pending, L.reserve, L.damage, L.resilience, L.stress, L.trophin, L.recentTurns, L.recentCare, learning(c), s, L.places, wordsKnown(c));

	L.turns = L.oneWay = L.care = L.slaps = L.harm = L.newWords = 0;
	L.steps = 0;
}

Life* lifeOf(oldCreature* c) {
	if (!c || off() || lifeOff())
		return nullptr;
	return &c->brainLife;
}

} // namespace

bool off() {
	static const bool o = devenv("OPENC2E_NO_BRAIN_ORGAN") != nullptr;
	return o;
}

bool lifeOff() {
	static const bool o = devenv("OPENC2E_NO_BRAIN_LIFE") != nullptr;
	return o || off();
}

void step(c2Creature& c) {
	if (off() || !c.isAlive())
		return;
	load(&c);
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
	// oxygen gone: past a grace, lasting damage (a cliff: Damage)
	if (!lifeOff()) {
		Life& L = c.brainLife;
		if (c.getChemical(OXYGEN) < oxygenGone) {
			L.hypoxia += 1.0f;
			if (L.hypoxia > hypoxiaGrace) {
				L.damage = std::min(255.0f, L.damage + hypoxiaDamage);
				if (watching() && (int)L.hypoxia % 25 == 0)
					fmt::print("* BRAIN {} t{} oxygen gone {:.0f} s: damage {:.0f}\n", monikerOf(&c), world.tickcount, L.hypoxia * 0.4f, L.damage);
			}
		} else
			L.hypoxia = std::max(0.0f, L.hypoxia - 5.0f);
	}
	if (watching() && world.history && ((before < 64) != (c.brainDaze < 64)))
		fmt::print("* BRAIN {} t{} {} (daze {:.0f}, ATP {}, glucose {})\n", monikerOf(&c), world.tickcount,
			c.brainDaze >= 64 ? "dazed" : "clear again", c.brainDaze, (int)c.getChemical(ATP), (int)c.getChemical(GLUCOSE));
}

void tick() {
	if (lifeOff() || engine.version != 2 || !world.history || world.tickcount % lifeTicks != 0)
		return;
	for (auto& a : world.agents)
		if (auto* ca = a && !a->isDying() ? dynamic_cast<CreatureAgent*>(a.get()) : nullptr)
			if (auto* c = dynamic_cast<oldCreature*>(ca->getCreature()); c && c->isAlive()) {
				load(c);
				live(c);
			}
}

float daze(Creature* c) {
	auto* oc = oldOf(c);
	return oc && !off() ? std::clamp(oc->brainDaze / 255.0f, 0.0f, 1.0f) : 0.0f;
}

float dazeOf(Agent* creatureAgent) {
	auto* ca = dynamic_cast<CreatureAgent*>(creatureAgent);
	return ca ? daze(ca->getCreature()) : 0.0f;
}

float learning(Creature* c) {
	Life* L = lifeOf(oldOf(c));
	if (!L || !L->set)
		return 1.0f;
	return std::clamp(1.0f + (L->growth - L->damage - 100.0f) / 450.0f, 0.6f, 1.35f);
}

float symptoms(Creature* c) {
	Life* L = lifeOf(oldOf(c));
	if (!L || !L->set)
		return 0.0f;
	return std::clamp((L->damage - L->reserve) / 100.0f, 0.0f, 1.0f);
}

float symptomsOf(Agent* creatureAgent) {
	auto* ca = dynamic_cast<CreatureAgent*>(creatureAgent);
	return ca ? symptoms(ca->getCreature()) : 0.0f;
}

float fearScale(Creature* c) {
	Life* L = lifeOf(oldOf(c));
	if (!L || !L->set)
		return 1.0f;
	return 1.25f - 0.5f * std::clamp(L->resilience, 0.0f, 255.0f) / 255.0f;
}

void spokenTo(oldCreature* listener, Agent* speaker, bool directed) {
	Life* L = lifeOf(listener);
	if (!L || !directed || !speaker)
		return;
	unsigned int now = world.tickcount;
	// it spoke not long ago, and is answered: a turn
	if (L->saidAt && now - L->saidAt <= answerWithin) {
		L->turns += 1.0f;
		L->saidAt = 0;
		L->spokenAnswered = true;
		return;
	}
	if (!L->spokenAnswered)
		L->oneWay += 1.0f; // the one before went unanswered
	L->spokenAt = now;
	L->spokenBy = AgentRef(speaker);
	L->spokenAnswered = false;
}

void spoke(oldCreature* c) {
	Life* L = lifeOf(c);
	if (!L)
		return;
	unsigned int now = world.tickcount;
	// spoken to not long ago: its answer makes a turn
	if (!L->spokenAnswered && now - L->spokenAt <= answerWithin) {
		// (the one that spoke to it, if it hears the answer as said to
		// it, has its turn in spokenTo)
		L->turns += 1.0f;
		L->spokenAnswered = true;
		return;
	}
	L->saidAt = now;
}

void touched(oldCreature* c, float amount) {
	if (Life* L = lifeOf(c))
		L->care += std::max(0.0f, amount);
}

void slapped(oldCreature* c) {
	if (Life* L = lifeOf(c))
		L->slaps += 1.0f;
}

void harmed(oldCreature* c, float amount) {
	if (Life* L = lifeOf(c))
		L->harm += std::max(0.0f, amount);
}

void stepped(Creature* c) {
	if (Life* L = lifeOf(oldOf(c)))
		L->steps++;
}

void learnedWord(oldCreature* c) {
	if (Life* L = lifeOf(c))
		L->newWords += 1.0f;
}

void reset() {
	s_saved.clear();
}

void save(std::ostream& out) {
	if (!world.history)
		return;
	for (auto& a : world.agents)
		if (auto* ca = a && !a->isDying() ? dynamic_cast<CreatureAgent*>(a.get()) : nullptr)
			if (auto* c = dynamic_cast<oldCreature*>(ca->getCreature()); c && c->isAlive()) {
				std::string mon = monikerOf(c);
				if (mon.empty())
					continue;
				if (c->brainDaze >= 0.5f)
					out << fmt::format("brain-daze {} {:.1f}\n", mon, c->brainDaze);
				const Life& L = c->brainLife;
				if (L.set)
					out << fmt::format("brain-life {} {:.2f} {:.2f} {:.2f} {:.2f} {:.2f} {:.1f} {:.3f} {:.2f} {:.2f}\n", mon, L.growth, L.pending, L.reserve,
						L.damage, L.resilience, L.trophin, L.stress, L.recentTurns, L.recentCare);
			}
}

bool readLine(const std::string& key, std::istringstream& l) {
	std::string who;
	if (key == "brain-daze") {
		float d;
		if (l >> who >> d)
			s_saved[who].daze = std::clamp(d, 0.0f, 255.0f);
	} else if (key == "brain-life") {
		Life L;
		if (l >> who >> L.growth >> L.pending >> L.reserve >> L.damage >> L.resilience >> L.trophin >> L.stress >> L.recentTurns >> L.recentCare) {
			L.set = true;
			auto& s = s_saved[who];
			s.life = L;
			s.hasLife = true;
		}
	} else
		return false;
	return true;
}

} // namespace BrainOrgan
