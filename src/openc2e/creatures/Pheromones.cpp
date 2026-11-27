/*
 *  Pheromones.cpp
 *  openc2e
 *
 *  Scents in the room system and what they do. See Pheromones.h and the
 *  format notes in pheromones.txt.
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

#include "Pheromones.h"
#include "common/devenv.h"
#include "Disease.h"

#include "Additions.h"

#include "Agent.h"
#include "Engine.h"
#include "Map.h"
#include "PathResolver.h"
#include "Room.h"
#include "World.h"
#include "common/Random.h"
#include "creatures/CreatureAgent.h"
#include "creatures/oldCreature.h"
#include "historyManager.h"

#include <algorithm>
#include <cmath>
#include <fmt/core.h>
#include "creatures/Kinds.h"
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

namespace Pheromones {

namespace {

const unsigned int STEP_TICKS = 5; // the scent field moves every 5 world ticks
const float DEPOSIT = 0.1f; // an emitter of strength 1 adds this much a step

struct Channel {
	std::string name;
	float spread = 0.12f; // share of the difference exchanged through a fully open door a step
	float fade = 0.02f; // share lost a step
	// habituation (pheromones.txt "scent NAME habituate T"; owner's idea,
	// 2026-11-15): a creature in the scent gets used to it over about T
	// ticks and its responses weaken by as much; out of it, it unlearns at
	// the same pace. 0: none
	float habituate = 0;
};

// a condition on the creature: a chemical above or below a level, carrying
// something, dead, asleep or awake
struct Condition {
	enum Kind { ChemAbove, ChemBelow, Carrying, Dead, Asleep, Awake, HereAbove, HereBelow, Ill, Day, Night, Wary, Empty } kind = ChemAbove;
	int chem = 0, level = 0;
	float strength = 0; // HereAbove/HereBelow: the rule's scent where the creature is, 0-1 (dose bands)
};

// which creatures (or objects) a rule is for
struct Who {
	bool object = false; // an object by classifier (only for emit)
	int family = 0, genus = 0, species = 0; // object classifier (0 = any)
	int creatureGenus = 0; // 0 any, 1 norn, 2 grendel, 3 ettin
	int breed = 0; // grendels: 0 any, 1 Boney ("grendel.boney"), 2 bulbous ("grendel.bulbous"), 3 Grouchy, the C1 grendels ("grendel.grouchy" or "grendel.c1"; Kinds.h)
	int sex = -1; // -1 either, 0 male, 1 female
	int stageFrom = 0, stageTo = 6;
};

struct Rule {
	Who who;
	enum Kind { Emit, Chem, Follow, Avoid, Mark } kind = Emit;
	int channel = 0;
	float strength = 0; // Emit, Mark: strength; Chem: amount a step at full scent; Follow/Avoid: weight
	int chem = 0; // Chem: the chemical
	int drive = -1; // Follow/Avoid: the drive the urge serves, or -1
	std::vector<int> felt; // Mark: the felt chemicals that set it off
	unsigned int ticks = 0; // Mark: how long the mark lasts
	bool tend = false; // Follow: the scent's source is to be tended ("... follow for lonely 1 tend")
	std::vector<int> bring; // Follow: "bring CATEGORY[,CATEGORY]" (Urge::bring)
	float dropAt = 0.5f; // ... "drop D"
	std::vector<Condition> when;
};

// an agent marked by a creature it harmed or helped: it gives off the scent,
// fading to nothing over the mark's time
struct MarkOn {
	AgentRef agent;
	int channel = 0;
	float strength = 0;
	unsigned int from = 0, until = 0; // world ticks
};

// signature scents: one per living creature, by moniker. Not clamped to 1
// (as the named scents are) so that the gradient toward the creature holds
// near it; spread and fade give a reach of about sqrt(spread/fade) rooms per
// e-fold (pheromones.txt: "signature spread S fade F strength X", or
// "signature off")
struct Signature {
	bool on = true;
	float spread = 0.2f, fade = 0.01f, strength = 1.0f;
} signature;

std::vector<Channel> channels;
std::vector<Rule> rules;
std::vector<PlaceEmission> placeEmissions;
std::vector<MarkOn> marks;
bool loaded = false;
// which channels each agent (by UNID) gave off in the last step
std::map<int, std::set<int>> emittedLast;

bool watching() {
	static const bool w = devenv("OPENC2E_WATCH_SCENTS") != nullptr;
	return w;
}

int channelIndex(const std::string& name) {
	for (size_t i = 0; i < channels.size(); i++)
		if (channels[i].name == name)
			return (int)i;
	return -1;
}

int driveIndex(const std::string& name) {
	static const char* names[17] = {"pain", "pleasure", "hunger", "cold", "hot", "tired", "sleepy", "lonely", "crowded",
		"fear", "bored", "anger", "sex", "injury", "suffocate", "thirst", "stress"};
	for (int i = 0; i < 17; i++)
		if (name == names[i])
			return i;
	return -1;
}

int stageIndex(const std::string& s) {
	static const char* names[7] = {"baby", "child", "adolescent", "youth", "adult", "old", "senile"};
	for (int i = 0; i < 7; i++)
		if (s == names[i])
			return i;
	return std::clamp(atoi(s.c_str()), 0, 6);
}

// "norn", "grendel:female", "any@2-5", "norn:male@adolescent-old"
bool parseWho(const std::string& tok, Who& w) {
	std::string s = tok;
	size_t at = s.find('@');
	if (at != std::string::npos) {
		std::string range = s.substr(at + 1);
		s = s.substr(0, at);
		size_t dash = range.find('-');
		w.stageFrom = stageIndex(range.substr(0, dash));
		w.stageTo = dash == std::string::npos ? w.stageFrom : stageIndex(range.substr(dash + 1));
	}
	size_t colon = s.find(':');
	if (colon != std::string::npos) {
		std::string sex = s.substr(colon + 1);
		s = s.substr(0, colon);
		if (sex == "female")
			w.sex = 1;
		else if (sex == "male")
			w.sex = 0;
		else
			return false;
	}
	size_t dot = s.find('.');
	if (dot != std::string::npos) {
		std::string breed = s.substr(dot + 1);
		s = s.substr(0, dot);
		if (s != "grendel")
			return false;
		if (breed == "boney")
			w.breed = 1;
		else if (breed == "bulbous")
			w.breed = 2;
		else if (breed == "c1" || breed == "grouchy")
			w.breed = 3;
		else
			return false;
	}
	if (s == "any" || s == "creature")
		w.creatureGenus = 0;
	else if (s == "norn")
		w.creatureGenus = 1;
	else if (s == "grendel")
		w.creatureGenus = 2;
	else if (s == "ettin")
		w.creatureGenus = 3;
	else
		return false;
	return true;
}

// "13>64", "105<20", "carrying", "dead", "asleep", "awake", "ill"
bool parseCondition(const std::string& t, Condition& c) {
	if (t == "carrying")
		c.kind = Condition::Carrying;
	else if (t == "dead")
		c.kind = Condition::Dead;
	else if (t == "asleep")
		c.kind = Condition::Asleep;
	else if (t == "ill") // has an illness (Disease.h)
		c.kind = Condition::Ill;
	else if (t == "awake")
		c.kind = Condition::Awake;
	else if (t == "day") // the world's time of day (dawn to dusk) or night
		c.kind = Condition::Day;
	else if (t == "night")
		c.kind = Condition::Night;
	else if (t == "wary") // has learned to link grendels with falling ill (Kinds.h)
		c.kind = Condition::Wary;
	else if (t == "empty") // carrying nothing
		c.kind = Condition::Empty;
	else if (t.rfind("here>", 0) == 0 || t.rfind("here<", 0) == 0) {
		// dose bands (owner's idea, 2026-11-15): one scent, different
		// responses at different strengths, e.g. drawn to it when faint,
		// fleeing it when strong
		c.kind = t[4] == '>' ? Condition::HereAbove : Condition::HereBelow;
		c.strength = (float)atof(t.substr(5).c_str());
	} else {
		size_t op = t.find_first_of("<>");
		if (op == std::string::npos || op == 0)
			return false;
		c.kind = t[op] == '>' ? Condition::ChemAbove : Condition::ChemBelow;
		c.chem = atoi(t.substr(0, op).c_str());
		c.level = atoi(t.substr(op + 1).c_str());
	}
	return true;
}

// a mark's trigger: harm (felt pain or injury rising, or punishment), help
// (reward, or any drive eased), or "felt:18,31" (those chemicals)
bool parseTrigger(const std::string& t, std::vector<int>& chems) {
	if (t == "harm") {
		chems = {18, 31, 53}; // C2: Pain up, Injury up, Punishment
		return true;
	}
	if (t == "help") {
		chems = {52}; // Reward
		for (int c = 35; c <= 51; c++) // C2: each drive eased
			chems.push_back(c);
		return true;
	}
	if (t.rfind("felt:", 0) == 0) {
		std::stringstream ss(t.substr(5));
		std::string n;
		while (std::getline(ss, n, ','))
			if (!n.empty())
				chems.push_back(atoi(n.c_str()));
		return !chems.empty();
	}
	return false;
}

void complain(int line, const std::string& text, const std::string& why) {
	fmt::print("Warning: pheromones.txt line {}: {} ({})\n", line, why, text);
}

oldCreature* creatureOf(Agent* a) {
	CreatureAgent* ca = dynamic_cast<CreatureAgent*>(a);
	return ca ? dynamic_cast<oldCreature*>(ca->getCreature()) : nullptr;
}

bool matches(const Who& w, Agent* a, oldCreature* c) {
	if (w.object)
		return !c && (w.family == 0 || a->family == w.family) && (w.genus == 0 || a->genus == w.genus) &&
			   (w.species == 0 || a->species == w.species);
	if (!c)
		return false;
	if (w.creatureGenus && a->genus != w.creatureGenus)
		return false;
	if ((w.breed == 1 && !Kinds::isBoney(c)) || (w.breed == 2 && !Kinds::isBulbous(c)) || (w.breed == 3 && !Kinds::isC1(c)))
		return false;
	if (w.sex >= 0 && (c->isFemale() ? 1 : 0) != w.sex)
		return false;
	int stage = (int)c->getStage();
	return stage >= w.stageFrom && stage <= w.stageTo;
}

// (here: the rule's own scent where the creature is, for the dose
// conditions; -1 where that does not apply, and the dose conditions fail)
bool holds(const std::vector<Condition>& when, Agent* a, oldCreature* c, float here = -1) {
	for (auto& k : when) {
		switch (k.kind) {
			case Condition::HereAbove:
				if (here < 0 || here <= k.strength)
					return false;
				break;
			case Condition::HereBelow:
				if (here < 0 || here >= k.strength)
					return false;
				break;
			case Condition::ChemAbove:
				if (!c || c->getChemical((unsigned char)k.chem) <= k.level)
					return false;
				break;
			case Condition::ChemBelow:
				if (!c || c->getChemical((unsigned char)k.chem) >= k.level)
					return false;
				break;
			case Condition::Carrying:
				if (!a->carrying)
					return false;
				break;
			case Condition::Dead:
				if (!c || c->isAlive())
					return false;
				break;
			case Condition::Asleep:
				if (!c || !c->isAsleep())
					return false;
				break;
			case Condition::Ill:
				if (!c || !world.history || !Disease::isIll(world.history->findMoniker(c->getGenome())))
					return false;
				break;
			case Condition::Awake:
				if (!c || c->isAsleep())
					return false;
				break;
			case Condition::Day:
				if (world.timeofday >= 4)
					return false;
				break;
			case Condition::Night:
				if (world.timeofday < 4)
					return false;
				break;
			case Condition::Wary:
				if (!c || !Kinds::wary(c))
					return false;
				break;
			case Condition::Empty:
				if (a->carrying)
					return false;
				break;
		}
	}
	return true;
}

// habituation by creature (UNID), a level per scent (see Channel::habituate)
std::map<int, std::vector<float>> habit;

// how strongly a scent at `level` is felt by a creature used to `h` of it
float felt(float level, float h) {
	return std::max(0.0f, level - h);
}

std::shared_ptr<Room> roomOf(Agent* a) {
	if (!world.map)
		return nullptr;
	return world.map->roomAt(a->x + a->getWidth() / 2.0f, a->y + a->getHeight() / 2.0f);
}

// one rule line (not a scent line); false, with a warning, if it is wrong
bool parseRule(int ln, const std::string& t, Rule& r) {
	std::istringstream l(t);
	std::vector<std::string> tok;
	std::string s;
	while (l >> s)
		tok.push_back(s);
	size_t i = 0;
	if (tok[0] == "object") {
		if (tok.size() < 4) {
			complain(ln, t, "object needs family genus species");
			return false;
		}
		r.who.object = true;
		r.who.family = atoi(tok[1].c_str());
		r.who.genus = atoi(tok[2].c_str());
		r.who.species = atoi(tok[3].c_str());
		i = 4;
	} else {
		if (!parseWho(tok[0], r.who)) {
			complain(ln, t, "unknown creature (any, norn, grendel, ettin, with :female/:male and @stages)");
			return false;
		}
		i = 1;
	}
	auto next = [&](std::string& out) {
		if (i >= tok.size())
			return false;
		out = tok[i++];
		return true;
	};
	auto scent = [&]() {
		std::string name;
		if (!next(name) || (r.channel = channelIndex(name)) < 0) {
			complain(ln, t, "scent not named in a 'scent' line");
			return false;
		}
		return true;
	};
	std::string verb, w;
	if (!next(verb)) {
		complain(ln, t, "too short");
		return false;
	}
	if (verb == "emit") {
		r.kind = Rule::Emit;
		if (!scent())
			return false;
		if (!next(w)) {
			complain(ln, t, "emit needs a strength");
			return false;
		}
		r.strength = (float)atof(w.c_str());
	} else if (verb == "mark") {
		r.kind = Rule::Mark;
		if (r.who.object) {
			complain(ln, t, "objects do not mark");
			return false;
		}
		if (!next(w) || !parseTrigger(w, r.felt)) {
			complain(ln, t, "mark needs harm, help or felt:N,N");
			return false;
		}
		if (!scent())
			return false;
		std::string st, tk;
		if (!next(st) || !next(tk)) {
			complain(ln, t, "mark needs a strength and a time in ticks");
			return false;
		}
		r.strength = (float)atof(st.c_str());
		r.ticks = (unsigned int)std::max(1, atoi(tk.c_str()));
	} else if (verb == "smell") {
		if (r.who.object) {
			complain(ln, t, "objects do not smell");
			return false;
		}
		if (!scent())
			return false;
		std::string what;
		if (!next(what)) {
			complain(ln, t, "smell needs chem, follow, avoid or bring");
			return false;
		}
		if (what == "chem") {
			r.kind = Rule::Chem;
			std::string ch, am;
			if (!next(ch) || !next(am)) {
				complain(ln, t, "chem needs a chemical and an amount");
				return false;
			}
			r.chem = atoi(ch.c_str());
			r.strength = (float)atof(am.c_str());
		} else if (what == "follow" || what == "avoid" || what == "bring") {
			r.kind = what == "avoid" ? Rule::Avoid : Rule::Follow;
			r.strength = 1.0f;
			if (what == "bring")
				i--; // (read below as "bring CATEGORY[,CATEGORY]")
			if (i < tok.size() && tok[i] == "for") {
				i++;
				std::string d;
				if (!next(d) || (r.drive = driveIndex(d)) < 0) {
					complain(ln, t, "unknown drive");
					return false;
				}
			}
			if (i < tok.size() && tok[i] != "when" && tok[i] != "tend" && tok[i] != "bring")
				r.strength = (float)atof(tok[i++].c_str());
			// "bring CATEGORY[,CATEGORY]" and "drop D": things of those kinds
			// are carried up the scent and put down where it is at least D
			// (owner, 2026-11-26: grendels' eggs to their lair, ettins' hoards,
			// eggs to the incubator, food to the hungry; Kinds.h)
			if (i < tok.size() && tok[i] == "bring") {
				if (r.kind != Rule::Follow) {
					complain(ln, t, "only a follow can bring");
					return false;
				}
				i++;
				std::string cats;
				if (!next(cats)) {
					complain(ln, t, "bring needs categories");
					return false;
				}
				std::stringstream cs(cats);
				std::string n;
				while (std::getline(cs, n, ','))
					if (!n.empty())
						r.bring.push_back(atoi(n.c_str()));
				if (i < tok.size() && tok[i] != "when" && tok[i] != "drop")
					r.strength = (float)atof(tok[i++].c_str());
				if (i + 1 < tok.size() && tok[i] == "drop") {
					r.dropAt = (float)atof(tok[i + 1].c_str());
					i += 2;
				}
			}
			// "tend": on reaching the scent's source (a baby, a child, the
			// senile), tickle it and bring it food or a toy (owner,
			// 2026-11-15; the mind's tend and bring)
			if (i < tok.size() && tok[i] == "tend") {
				if (r.kind != Rule::Follow) {
					complain(ln, t, "only a follow can tend");
					return false;
				}
				r.tend = true;
				i++;
			}
		} else {
			complain(ln, t, "smell needs chem, follow or avoid");
			return false;
		}
	} else {
		complain(ln, t, "expected emit, mark or smell");
		return false;
	}
	if (i < tok.size() && tok[i] == "when") {
		for (i++; i < tok.size(); i++) {
			if (tok[i] == "and")
				continue;
			Condition c;
			if (!parseCondition(tok[i], c)) {
				complain(ln, t, "unknown condition " + tok[i]);
				return false;
			}
			r.when.push_back(c);
		}
	} else if (i < tok.size()) {
		complain(ln, t, "unexpected " + tok[i]);
		return false;
	}
	return true;
}

} // namespace

void load() {
	loaded = true;
	channels.clear();
	signature = Signature();
	rules.clear();
	marks.clear();
	habit.clear();
	emittedLast.clear();
	// (OPENC2E_PHEROMONES=FILE reads another file: for comparing rule sets in test copies)
	const char* other = devenv("OPENC2E_PHEROMONES");
	// the player's own pheromones.txt, else Zeetures's built-in one (Additions.h)
	std::ifstream fin;
	std::istringstream builtin;
	if (other)
		fin.open(other);
	else
		builtin.str(Additions::text("pheromones.txt"));
	std::istream& in = other ? static_cast<std::istream&>(fin) : builtin;
	if (!in || (!other && builtin.str().empty()))
		return;
	std::string text;
	int line = 0;
	std::vector<std::pair<int, std::string>> pending; // rules, read after every scent is named
	while (std::getline(in, text)) {
		line++;
		size_t hash = text.find('#');
		if (hash != std::string::npos)
			text = text.substr(0, hash);
		std::istringstream l(text);
		std::string first;
		if (!(l >> first))
			continue;
		if (first == "signature") {
			std::string key;
			float v;
			while (l >> key) {
				if (key == "off")
					signature.on = false;
				else if (key == "on")
					signature.on = true;
				else if (l >> v) {
					if (key == "spread")
						signature.spread = std::clamp(v, 0.0f, 0.24f);
					else if (key == "fade")
						signature.fade = std::clamp(v, 0.0f, 1.0f);
					else if (key == "strength")
						signature.strength = std::max(0.0f, v);
				}
			}
			continue;
		}
		if (first == "scent") {
			Channel ch;
			if (!(l >> ch.name)) {
				complain(line, text, "a scent needs a name");
				continue;
			}
			std::string key;
			float v;
			while (l >> key >> v) {
				if (key == "spread")
					ch.spread = std::clamp(v, 0.0f, 0.24f); // more would overshoot between rooms
				else if (key == "fade")
					ch.fade = std::clamp(v, 0.0f, 1.0f);
				else if (key == "habituate")
					ch.habituate = std::max(0.0f, v);
			}
			if (channelIndex(ch.name) < 0)
				channels.push_back(ch);
		} else
			pending.push_back({line, text});
	}
	for (auto& [ln, t] : pending) {
		Rule r;
		if (parseRule(ln, t, r))
			rules.push_back(r);
	}
	fmt::print("* Pheromones: {} scents, {} rules\n", channels.size(), rules.size());
}

int channelCount() {
	if (!loaded)
		load();
	return (int)channels.size();
}

std::string channelName(int channel) {
	return channel >= 0 && channel < (int)channels.size() ? channels[channel].name : "";
}

void felt(Agent* creature, oldCreature* c, Agent* source, const std::array<int, 4>& chems, const std::array<int, 4>& amounts) {
	if (!loaded)
		load();
	if (!creature || !c || !source || source == creature || rules.empty())
		return;
	for (auto& r : rules) {
		if (r.kind != Rule::Mark || !matches(r.who, creature, c) || !holds(r.when, creature, c))
			continue;
		bool set = false;
		for (size_t k = 0; k < 4 && !set; k++)
			set = amounts[k] > 0 && std::find(r.felt.begin(), r.felt.end(), chems[k]) != r.felt.end();
		if (!set)
			continue;
		// a fresh mark of the same scent on the same agent replaces the old
		marks.erase(std::remove_if(marks.begin(), marks.end(),
						[&](const MarkOn& m) { return m.agent == AgentRef(source) && m.channel == r.channel; }),
			marks.end());
		MarkOn m;
		m.agent = AgentRef(source);
		m.channel = r.channel;
		m.strength = r.strength;
		m.from = world.tickcount;
		m.until = world.tickcount + r.ticks;
		marks.push_back(m);
		if (watching())
			fmt::print("* SCENT MARK {} on {} {} {} by creature {}\n", channels[r.channel].name, (int)source->family,
				(int)source->genus, source->species, creature->getUNID());
	}
}


// The signature scents' step: fade, spread through open doorways, laid by
// each living creature where it is; traces below a millionth are dropped.
// Each creature has a slot, an index into every room's values, given up when
// its scent is gone everywhere: the same sums as when each room kept a table
// by name, which was 7% of all play time (2026-11-21).
std::map<std::string, int> sigSlots;
std::vector<std::string> slotNames; // slot -> creature ("" free)

int slotFor(const std::string& who) {
	auto it = sigSlots.find(who);
	if (it != sigSlots.end())
		return it->second;
	size_t slot = 0;
	while (slot < slotNames.size() && !slotNames[slot].empty())
		slot++;
	if (slot == slotNames.size())
		slotNames.push_back(who);
	else
		slotNames[slot] = who;
	sigSlots[who] = (int)slot;
	return (int)slot;
}

void tickSignatures() {
	const auto& rooms = world.map->getRooms();
	if (!signature.on) {
		for (auto& r : rooms)
			r->sig.clear();
		return;
	}
	// who lays a scent this step, and where (first, so every slot exists)
	std::vector<std::pair<std::shared_ptr<Room>, int>> laid;
	for (auto& a : world.agents) {
		if (!a || a->family != 4 || a->isDying())
			continue;
		oldCreature* c = creatureOf(a.get());
		if (!c || !c->isAlive())
			continue;
		std::shared_ptr<Room> room = roomOf(a.get());
		if (!room)
			continue;
		std::string who = world.history->findMoniker(c->getGenome());
		if (!who.empty())
			laid.push_back({room, slotFor(who)});
	}
	const size_t n = slotNames.size();
	const float keep = 1.0f - signature.fade;
	for (auto& r : rooms) {
		r->sig.resize(n, 0.0f);
		r->signext.resize(n);
		for (size_t k = 0; k < n; k++)
			r->signext[k] = r->sig[k] * keep;
	}
	for (auto& r : rooms)
		for (auto& [nb, door] : r->getDoors()) {
			if (door.perm <= 0 || !nb || nb->sig.size() != n)
				continue;
			const float share = signature.spread * (door.perm / 255.0f);
			float* out = r->signext.data();
			const float *here = r->sig.data(), *there = nb->sig.data();
			for (size_t k = 0; k < n; k++)
				out[k] += share * (there[k] - here[k]);
		}
	for (auto& [room, slot] : laid)
		if ((size_t)slot < room->signext.size())
			room->signext[(size_t)slot] += signature.strength * DEPOSIT;
	std::vector<char> used(n, 0);
	for (auto& r : rooms)
		for (size_t k = 0; k < n; k++) {
			float v = r->signext[k];
			v = v > 1e-6f ? std::min(v, 1000.0f) : 0.0f;
			r->sig[k] = v;
			used[k] |= (char)(v > 0.0f);
		}
	for (size_t k = 0; k < n; k++)
		if (!used[k] && !slotNames[k].empty()) {
			sigSlots.erase(slotNames[k]);
			slotNames[k].clear();
		}
}

void tick() {
	static unsigned int counter = 0;
	if (!loaded)
		load();
	if (!world.map || ++counter % STEP_TICKS != 0)
		return;
	tickSignatures();
	if (channels.empty())
		return;
	const size_t n = channels.size();
	const auto& rooms = world.map->getRooms();
	// fade
	for (auto& r : rooms) {
		r->scent.resize(n, 0.0f);
		r->scentnext.resize(n, 0.0f);
		for (size_t c = 0; c < n; c++)
			r->scentnext[c] = r->scent[c] * (1.0f - channels[c].fade);
	}
	// spread through doorways, as far as each is open
	for (auto& r : rooms)
		for (auto& [nb, door] : r->getDoors()) {
			if (door.perm <= 0 || nb->scent.size() < n)
				continue;
			float open = door.perm / 255.0f;
			for (size_t c = 0; c < n; c++)
				r->scentnext[c] += channels[c].spread * open * (nb->scent[c] - r->scent[c]);
		}
	// laid by creatures and objects
	std::map<int, std::set<int>> emitted;
	auto lay = [&](Agent* a, int channel, float strength) {
		std::shared_ptr<Room> room = roomOf(a);
		if (!room || room->scentnext.size() < n)
			return;
		room->scentnext[channel] += strength * DEPOSIT;
		emitted[a->getUNID()].insert(channel);
	};
	for (auto& a : world.agents) {
		if (!a || a->isDying() || a->carriedby)
			continue;
		oldCreature* c = creatureOf(a.get());
		for (auto& r : rules)
			if (r.kind == Rule::Emit && matches(r.who, a.get(), c) && holds(r.when, a.get(), c))
				lay(a.get(), r.channel, r.strength);
	}
	// laid at places (Kinds.h: ettins' hoard spots)
	for (auto& pe : placeEmissions) {
		int ch = channelIndex(pe.scent);
		std::shared_ptr<Room> room = ch >= 0 ? world.map->roomAt(pe.x, pe.y) : nullptr;
		if (room && room->scentnext.size() >= n)
			room->scentnext[ch] += pe.strength * DEPOSIT;
	}
	// laid by marked agents, fading over the mark's time
	marks.erase(std::remove_if(marks.begin(), marks.end(),
					[](const MarkOn& m) { return !m.agent || m.agent->isDying() || world.tickcount >= m.until; }),
		marks.end());
	for (auto& m : marks) {
		float left = (float)(m.until - world.tickcount) / (float)std::max(1u, m.until - m.from);
		lay(m.agent.get(), m.channel, m.strength * left);
	}
	for (auto& r : rooms)
		for (size_t c = 0; c < n; c++)
			r->scent[c] = std::clamp(r->scentnext[c], 0.0f, 1.0f);
	emittedLast.swap(emitted);
	// smelled by creatures: the chemical rules (a share of a unit added with
	// that chance, so small amounts add up right)
	for (auto& a : world.agents) {
		if (!a || a->isDying())
			continue;
		oldCreature* c = creatureOf(a.get());
		if (!c || !c->isAlive())
			continue;
		std::shared_ptr<Room> room = roomOf(a.get());
		if (!room || room->scent.size() < n)
			continue;
		auto own = emittedLast.find(a->getUNID());
		// habituation: each scent that has it, followed toward what is here
		// (its own scent: not smelled, so unlearnt)
		std::vector<float>& h = habit[a->getUNID()];
		h.resize(n, 0.0f);
		for (size_t ch = 0; ch < n; ch++)
			if (channels[ch].habituate > 0) {
				bool ownScent = own != emittedLast.end() && own->second.count((int)ch);
				float level = ownScent ? 0.0f : room->scent[ch];
				h[ch] += (level - h[ch]) * std::min(1.0f, STEP_TICKS / channels[ch].habituate);
			}
		for (auto& r : rules) {
			if (r.kind != Rule::Chem || !matches(r.who, a.get(), c) || !holds(r.when, a.get(), c, room->scent[r.channel]))
				continue;
			if (own != emittedLast.end() && own->second.count(r.channel))
				continue; // its own scent
			float amount = r.strength * felt(room->scent[r.channel], h[r.channel]);
			if (amount <= 0.0f)
				continue;
			int whole = (int)amount;
			if (rand_float(0, 1) < amount - whole)
				whole++;
			if (whole > 0)
				c->addChemical((unsigned char)r.chem, (unsigned char)std::min(whole, 255));
		}
	}
	if (watching() && counter % (STEP_TICKS * 200) == 0) {
		for (size_t c = 0; c < n; c++) {
			int some = 0, full = 0, half = 0;
			float top = 0;
			for (auto& r : rooms) {
				some += r->scent[c] > 0.05f;
				half += r->scent[c] >= 0.5f;
				full += r->scent[c] >= 0.98f;
				top = std::max(top, r->scent[c]);
			}
			fmt::print("* WATCH SCENT {}: {} rooms, strongest {:.2f} ({} at half or more, {} at the cap)\n", channels[c].name, some, top, half, full);
		}
	}
}

std::vector<Urge> urgesFor(Agent* creature, oldCreature* c) {
	std::vector<Urge> out;
	if (!loaded)
		load();
	if (!creature || !c || channels.empty())
		return out;
	auto own = emittedLast.find(creature->getUNID());
	std::shared_ptr<Room> room = roomOf(creature);
	auto hit = habit.find(creature->getUNID());
	for (auto& r : rules) {
		float here = room && (size_t)r.channel < room->scent.size() ? room->scent[r.channel] : -1.0f;
		if ((r.kind != Rule::Follow && r.kind != Rule::Avoid) || !matches(r.who, creature, c) || !holds(r.when, creature, c, here))
			continue;
		if (own != emittedLast.end() && own->second.count(r.channel))
			continue;
		Urge u;
		u.channel = r.channel;
		u.avoid = r.kind == Rule::Avoid;
		u.drive = r.drive;
		u.weight = r.strength;
		// (used to the scent: the urge weakens as much as it is felt less)
		if (here > 0 && hit != habit.end() && (size_t)r.channel < hit->second.size())
			u.weight *= felt(here, hit->second[r.channel]) / here;
		u.tend = r.tend;
		u.bring = r.bring;
		u.dropAt = r.dropAt;
		out.push_back(u);
	}
	return out;
}

void setPlaceEmissions(const std::vector<PlaceEmission>& e) {
	placeEmissions = e;
}

float levelAt(const std::string& scent, Agent* a) {
	if (!loaded)
		load();
	int ch = channelIndex(scent);
	std::shared_ptr<Room> room = ch >= 0 && a ? roomOf(a) : nullptr;
	return room && (size_t)ch < room->scent.size() ? room->scent[(size_t)ch] : 0.0f;
}

float signatureIn(const Room& room, const std::string& moniker) {
	auto it = sigSlots.find(moniker);
	return it == sigSlots.end() || (size_t)it->second >= room.sig.size() ? 0.0f : room.sig[(size_t)it->second];
}

} // namespace Pheromones
