/*
 *  Relationships.cpp
 *  openc2e
 *
 *  Bonds between creatures. See Relationships.h.
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

#include "Relationships.h"
#include "common/devenv.h"

#include "Agent.h"
#include "Engine.h"
#include "World.h"
#include "creatures/Breeding.h"
#include "creatures/CreatureAgent.h"
#include "creatures/oldCreature.h"
#include "historyManager.h"

#include <algorithm>
#include <cmath>
#include <fmt/core.h>
#include <sstream>
#include <vector>

namespace Relationships {

namespace {

const unsigned int STEP_TICKS = 50; // a step every 5 seconds
const float HELP_GAIN = 0.5f; // a full-strength reward moves recent feeling this much
const float HARM_WEIGHT = 3.0f; // harm counts this many times as much as help
const float AFFINITY_SHARE = 0.4f; // of each event's move, how much reaches the slow meter
const unsigned int APART = 3000; // ticks without contact before the meter fades (5 minutes)

// C2 chemicals (stimuli): 18-34 raise drives 0-16, 35-51 ease them; 52
// reward, 53 punishment. Body chemicals: drive d is chemical d + 1.
const int PAIN_UP = 18, INJURY_UP = 31, EASE0 = 35, REWARD = 52, PUNISH = 53;
const int CHEM_LONELY = 8, CHEM_FEAR = 10, CHEM_SEX = 13, CHEM_STRESS = 17;

const char* driveName(int d) {
	static const char* names[17] = {"pain", "pleasure", "hunger", "cold", "heat", "tiredness", "sleepiness", "loneliness",
		"crowdedness", "fear", "boredom", "anger", "sex drive", "injury", "breathlessness", "thirst", "stress"};
	return d >= 0 && d < 17 ? names[d] : "a need";
}

bool watching() {
	static const bool w = devenv("OPENC2E_WATCH_BONDS") != nullptr;
	return w;
}

CreatureAgent* creatureAgentOf(Agent* a) {
	return dynamic_cast<CreatureAgent*>(a);
}

oldCreature* creatureOf(Agent* a) {
	CreatureAgent* ca = creatureAgentOf(a);
	return ca ? dynamic_cast<oldCreature*>(ca->getCreature()) : nullptr;
}

std::string monikerOf(oldCreature* c) {
	return c ? world.history->findMoniker(c->getGenome()) : std::string();
}

// close enough to be together: about a room's width on the same floor
bool together(Agent* a, Agent* b) {
	float ax = a->x + a->getWidth() / 2.0f, bx = b->x + b->getWidth() / 2.0f;
	float ay = a->y + a->getHeight(), by = b->y + b->getHeight();
	return std::fabs(ax - bx) < 350.0f && std::fabs(ay - by) < 150.0f;
}

void move(Bond& b, float x) {
	b.recent = std::clamp(b.recent + x, -1.0f, 1.0f);
	// the meter moves less as it nears the end it is moving toward
	float room = x > 0 ? 1.0f - b.affinity : 1.0f + b.affinity;
	b.affinity = std::clamp(b.affinity + x * AFFINITY_SHARE * std::max(0.1f, room), -1.0f, 1.0f);
}

void relabel(Bond& b) {
	if (!b.friend_ && b.affinity > 0.5f && b.familiarity > 0.3f)
		b.friend_ = true;
	else if (b.friend_ && b.affinity < 0.35f)
		b.friend_ = false;
	if (!b.enemy && b.affinity < -0.5f)
		b.enemy = true;
	else if (b.enemy && b.affinity > -0.35f)
		b.enemy = false;
}

// relabels, and tells the creature's story of a new friend, enemy or fear
void relabelAndTell(const std::string& self, const std::string& other, Bond& b, bool wasFeared) {
	bool wasFriend = b.friend_, wasEnemy = b.enemy;
	relabel(b);
	if (!wasFriend && b.friend_)
		world.history->tell(self, storyEntry::befriended, other);
	if (!wasEnemy && b.enemy)
		world.history->tell(self, storyEntry::enemy, other);
	if (!wasFeared && b.fear > 0.5f)
		world.history->tell(self, storyEntry::fears, other);
}

struct Living {
	Agent* body;
	CreatureAgent* ca;
	oldCreature* c;
	std::string moniker;
};

std::vector<Living> living() {
	std::vector<Living> out;
	for (auto& a : world.agents) {
		if (!a || a->isDying())
			continue;
		oldCreature* c = creatureOf(a.get());
		if (!c || !c->isAlive())
			continue;
		std::string m = monikerOf(c);
		if (!m.empty())
			out.push_back({a.get(), creatureAgentOf(a.get()), c, m});
	}
	return out;
}

} // namespace

// how long after a creature puts something down it is still its gift
// witnesses this near (px, half as far up or down) may defend a friend, for
// this long (world ticks)
static const float DEFEND_SEE = 400.0f;
static const unsigned int DEFEND_TICKS = 200;
static const unsigned int GIFT_WINDOW = 1200;

void felt(Agent* creature, oldCreature* c, Agent* source, const std::array<int, 4>& chems, const std::array<int, 4>& amounts) {
	oldCreature* other = creatureOf(source);
	// a gift: food (or anything) another creature carried over and put down
	// not long ago, felt from the thing itself, counts as from that creature
	// (the bring of tending and sharing is credited to the bringer)
	bool gift = false;
	static const bool noGifts = devenv("OPENC2E_NO_GIFT_CREDIT") != nullptr; // (to compare)
	if (!other && source && !noGifts && source->lastDroppedBy && world.worldtickcount - source->lastDroppedTick < GIFT_WINDOW) {
		Agent* giver = source->lastDroppedBy.get();
		if (giver && giver != creature && creatureOf(giver)) {
			other = creatureOf(giver);
			source = giver;
			gift = true;
		}
	}
	if (!creature || !c || !other || source == creature || engine.version != 2)
		return;
	std::string them = monikerOf(other);
	if (them.empty())
		return;
	float help = 0, harm = 0, fear = 0;
	int eased = -1, easedAmount = 0, rewarded = 0, hurt = 0, punished = 0;
	for (size_t k = 0; k < 4; k++) {
		int ch = chems[k], am = amounts[k];
		if (am <= 0)
			continue;
		float f = am / 255.0f;
		if (ch == REWARD) {
			help += f;
			rewarded += am;
		} else if (ch >= EASE0 && ch < EASE0 + 17) {
			help += 0.7f * f;
			if (am > easedAmount) {
				easedAmount = am;
				eased = ch - EASE0;
			}
		} else if (ch == PAIN_UP || ch == INJURY_UP) {
			harm += f;
			fear += 0.6f * f;
			hurt += am;
		} else if (ch == PUNISH) {
			harm += 0.7f * f;
			punished += am;
		}
	}
	if (help <= 0 && harm <= 0)
		return;
	Bond& b = c->bonds[them];
	unsigned int now = world.worldtickcount;
	// temperament: sociable creatures warm faster, hot-tempered ones sour
	// harder, shy ones take more fright
	const Temperament& t = c->temperament;
	float x = help * HELP_GAIN * t.factor(SOCIABILITY, 0.3f) - harm * HELP_GAIN * HARM_WEIGHT * t.factor(TEMPER, 0.2f);
	fear *= t.factor(BOLDNESS, -0.3f);
	// making up after a fight counts double for a while
	if (x > 0 && b.lastHarm && now - b.lastHarm < 600)
		x *= 2.0f;
	bool wasFeared = b.fear > 0.5f;
	move(b, x);
	b.fear = std::clamp(b.fear + fear, 0.0f, 1.0f);
	b.lastContact = now;
	std::string what;
	if (hurt)
		what = "hurt me";
	else if (punished)
		what = "punished me";
	else if (rewarded)
		what = "pleased me";
	else if (gift)
		what = std::string("brought me something that eased my ") + driveName(eased);
	else
		what = std::string("eased my ") + driveName(eased);
	if (harm > 0)
		b.lastHarm = now;
	b.events[what]++;
	relabelAndTell(monikerOf(c), them, b, wasFeared);
	// witnesses: creatures near the one hurt who count it a friend may stand
	// up for it against the one who hurt it (owner's idea, 2026-11-11: defend
	// friends; the mind decides whether, HomeostatMind::addDefend)
	static const bool noDefend = devenv("OPENC2E_NO_DEFEND") != nullptr; // (to compare)
	if (harm > 0 && !gift && !noDefend) {
		std::string victim = monikerOf(c);
		float vx = creature->x + creature->getWidth() / 2.0f, vy = creature->y + creature->getHeight() / 2.0f;
		for (auto& ag : world.agents) {
			if (!ag || ag.get() == creature || ag.get() == source || ag->isDying())
				continue;
			oldCreature* w = creatureOf(ag.get());
			if (!w)
				continue;
			float wx = ag->x + ag->getWidth() / 2.0f, wy = ag->y + ag->getHeight() / 2.0f;
			if (std::fabs(wx - vx) > DEFEND_SEE || std::fabs(wy - vy) > DEFEND_SEE / 2)
				continue;
			auto it = w->bonds.find(victim);
			if (it == w->bonds.end() || !(it->second.friend_ || it->second.mate) || it->second.affinity < 0.3f)
				continue;
			w->defendFrom = AgentRef(source);
			w->defendUntil = now + DEFEND_TICKS;
			w->defendFor = victim;
		}
	}
	if (watching())
		fmt::print("* BOND {} -> {}: {} ({:+.3f}) recent {:+.2f} affinity {:+.2f} fear {:.2f}\n", monikerOf(c), them, what, x, b.recent,
			b.affinity, b.fear);
}

void tick() {
	static unsigned int counter = 0;
	if (engine.version != 2 || ++counter % STEP_TICKS != 0)
		return;
	unsigned int now = world.worldtickcount;
	std::vector<Living> all = living();
	for (auto& me : all) {
		bool lonely = me.c->getChemical(CHEM_LONELY) > 64;
		bool friendNear = false, mateAway = false, attracted = false;
		for (auto& them : all) {
			if (them.body == me.body)
				continue;
			bool near = together(me.body, them.body);
			auto it = me.c->bonds.find(them.moniker);
			if (!near && it == me.c->bonds.end())
				continue; // not met
			Bond& b = me.c->bonds[them.moniker];
			if (near) {
				b.familiarity += 0.004f * (1.0f - b.familiarity);
				b.lastContact = now;
				if (lonely) // company eased a need
					move(b, 0.01f * me.c->temperament.factor(SOCIABILITY, 0.3f));
				if ((int)me.c->getStage() <= 1 && (int)them.c->getStage() <= 1 && b.familiarity > 0.3f)
					b.coReared = true;
				if (b.friend_ || b.mate)
					friendNear = true;
				// a liked partner of the other sex, both grown, not barred kin
				if (b.affinity > 0.6f && them.body->genus == me.body->genus && them.c->isFemale() != me.c->isFemale() &&
					(int)me.c->getStage() >= 2 && (int)me.c->getStage() <= 5 && (int)them.c->getStage() >= 2 &&
					(int)them.c->getStage() <= 5 && !kinBarred(me.ca, them.ca))
					attracted = true;
			} else if (b.mate)
				mateAway = true;
			// recent feeling and fear fade in minutes; the meter only after long apart
			b.recent *= 0.97f;
			b.fear *= 0.985f;
			if (now - b.lastContact > APART) {
				b.affinity *= b.mate ? 0.9999f : 0.9995f;
				b.familiarity *= 0.9998f;
			}
			relabelAndTell(me.moniker, them.moniker, b, true);
		}
		// a friend nearby eases fear and stress; a mate away is missed; a
		// liked partner stirs desire
		if (friendNear) {
			me.c->subChemical(CHEM_FEAR, 1);
			me.c->subChemical(CHEM_STRESS, 1);
		}
		if (mateAway)
			me.c->addChemical(CHEM_LONELY, 1);
		if (attracted)
			me.c->addChemical(CHEM_SEX, 2);
	}
	// the dead are forgotten slowly like the absent; bonds to creatures no
	// longer in the world stay (the map shows only the living)
	if (watching() && counter % (STEP_TICKS * 12) == 0)
		for (auto& me : all)
			for (auto& [m, b] : me.c->bonds)
				fmt::print("* BONDS {} -> {}: familiarity {:.2f} recent {:+.2f} affinity {:+.2f} fear {:.2f} {}\n", me.moniker, m,
					b.familiarity, b.recent, b.affinity, b.fear, label(b));
}

void conceived(CreatureAgent* mum, CreatureAgent* dad) {
	oldCreature* cm = mum ? dynamic_cast<oldCreature*>(mum->getCreature()) : nullptr;
	oldCreature* cd = dad ? dynamic_cast<oldCreature*>(dad->getCreature()) : nullptr;
	if (!cm || !cd)
		return;
	Bond& a = cm->bonds[monikerOf(cd)];
	Bond& b = cd->bonds[monikerOf(cm)];
	a.mate = b.mate = true;
	a.events["had a baby with me"]++;
	b.events["had a baby with me"]++;
	move(a, 0.2f);
	move(b, 0.2f);
	a.lastContact = b.lastContact = world.worldtickcount;
}

float mateFactor(CreatureAgent* a, CreatureAgent* b) {
	oldCreature* ca = a ? dynamic_cast<oldCreature*>(a->getCreature()) : nullptr;
	oldCreature* cb = b ? dynamic_cast<oldCreature*>(b->getCreature()) : nullptr;
	if (!ca || !cb)
		return 1.0f;
	const Bond* ab = bondOf(ca, monikerOf(cb));
	const Bond* ba = bondOf(cb, monikerOf(ca));
	float f = 1.0f;
	for (const Bond* x : {ab, ba}) {
		if (!x)
			continue;
		if (x->enemy || x->affinity < -0.3f || x->fear > 0.5f)
			return 0.0f;
		f *= 1.0f + 0.5f * std::max(0.0f, x->affinity);
		if (x->coReared)
			f *= 0.5f;
	}
	return f;
}

std::string label(const Bond& b) {
	if (b.mate)
		return "mate";
	if (b.enemy)
		return "enemy";
	if (b.fear > 0.5f)
		return "feared";
	if (b.friend_)
		return "friend";
	if (b.affinity > 0.2f)
		return "fond";
	if (b.affinity < -0.2f)
		return "wary";
	if (b.familiarity > 0.1f)
		return "known";
	return "";
}

const Bond* bondOf(oldCreature* c, const std::string& other) {
	if (!c)
		return nullptr;
	auto it = c->bonds.find(other);
	return it == c->bonds.end() ? nullptr : &it->second;
}

// MONIKER familiarity recent affinity fear lastContact lastHarm flags events
// (flags: f friend, e enemy, m mate, c co-reared, - none; events as
// what=count with spaces as underscores)
std::string toLine(const std::string& other, const Bond& b) {
	std::string flags;
	if (b.friend_)
		flags += 'f';
	if (b.enemy)
		flags += 'e';
	if (b.mate)
		flags += 'm';
	if (b.coReared)
		flags += 'c';
	if (flags.empty())
		flags = "-";
	std::string line = fmt::format("{} {:.4f} {:.4f} {:.4f} {:.4f} {} {} {}", other, b.familiarity, b.recent, b.affinity, b.fear,
		b.lastContact, b.lastHarm, flags);
	for (auto& [what, n] : b.events) {
		std::string w = what;
		std::replace(w.begin(), w.end(), ' ', '_');
		line += fmt::format(" {}={}", w, n);
	}
	return line;
}

bool fromLine(const std::string& fields, std::string& other, Bond& b) {
	std::istringstream l(fields);
	std::string flags;
	if (!(l >> other >> b.familiarity >> b.recent >> b.affinity >> b.fear >> b.lastContact >> b.lastHarm >> flags))
		return false;
	b.friend_ = flags.find('f') != std::string::npos;
	b.enemy = flags.find('e') != std::string::npos;
	b.mate = flags.find('m') != std::string::npos;
	b.coReared = flags.find('c') != std::string::npos;
	std::string ev;
	while (l >> ev) {
		size_t eq = ev.rfind('=');
		if (eq == std::string::npos)
			continue;
		std::string what = ev.substr(0, eq);
		std::replace(what.begin(), what.end(), '_', ' ');
		b.events[what] = atoi(ev.substr(eq + 1).c_str());
	}
	return true;
}

} // namespace Relationships
