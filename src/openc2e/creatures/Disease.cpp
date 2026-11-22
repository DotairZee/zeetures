/*
 *  Disease.cpp
 *  openc2e
 *
 *  Bacteria and disease for Creatures 2 worlds. See Disease.h.
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

#include "Disease.h"
#include "common/devenv.h"

#include "Agent.h"
#include "Engine.h"
#include "Map.h"
#include "Room.h"
#include "World.h"
#include "common/Random.h"
#include "creatures/CreatureAgent.h"
#include "creatures/oldCreature.h"
#include "historyManager.h"

#include <algorithm>
#include <cmath>
#include <fmt/core.h>
#include <map>
#include <ostream>

namespace Disease {

namespace {

// ---- tuning (a nuisance, not a killer: owner, 2026-11-21) ----
const unsigned int stepTicks = 50; // the work is done every 50 world ticks (5 s)
const float caveLevel = 25.0f; // germs a pitch-dark room keeps of each cave strain
const float darkLight = 80.0f; // rooms darker than this keep cave germs, the darker the more
const float growth = 0.02f; // share of the gap to a room's level made up each step
const float decay = 0.004f; // share lost each step in the dark, more in light
const float seep = 0.002f; // share of the difference passed through an open doorway each step
const float rotShare = 0.5f; // rotting food (organic nutrient above the room's own) feeds rot strains
const float rotLevel = 40.0f;
const float shedIll = 0.8f, shedCarrier = 1.5f; // germs a step from an ill creature, a carrying grendel
const float catchRate = 0.015f; // chance a step at full germs, without antibodies
const int immuneAntibody = 60; // this much of a strain's antibody and it cannot be caught
const int recoveredAntibody = 90; // and this much ends an illness
const unsigned int longestIllness = 36000; // ticks (an hour): over by then whatever the antibodies
const int antigenDose = 3, toxinDose = 16; // a step, while ill (histamine A must pass 58 to set off a sneeze: 12 never did)
const int mostAtOnce = 2; // illnesses one creature can have at once
const float sneezeReach = 160.0f, sneezeHeight = 90.0f, sneezeCatch = 0.015f;
const float rottenCatch = 0.3f; // chance of catching a rot strain from eating one rotten fruit, without antibodies
const unsigned int newStrainEvery = 30000; // ticks between chances of a new strain
const float newStrainChance = 0.3f;
const size_t mostStrains = 8, fewestStrains = 4;

bool off() {
	static const bool o = devenv("OPENC2E_NO_DISEASE") != nullptr;
	return o;
}
bool watching() {
	static const bool w = devenv("OPENC2E_WATCH_DISEASE") != nullptr;
	return w;
}

std::vector<Strain> s_strains;
int s_nextId = 1;
unsigned int s_lastNew = 0;
std::map<int, std::map<int, float>> s_germs; // room -> strain -> germs (0-255)
std::map<std::string, std::vector<Illness>> s_ill; // moniker -> illnesses
std::map<std::string, int> s_carrier; // grendel moniker -> strain

int antibodyChem(const Strain& s) {
	return 240 + s.antigen;
}
int antigenChem(const Strain& s) {
	return 248 + s.antigen;
}

Strain* findStrain(int id) {
	for (auto& s : s_strains)
		if (s.id == id)
			return &s;
	return nullptr;
}

void seed() {
	// the first strains, on antigens no creature is born immune to (the
	// standard genomes start with antibodies 0, 1, 3 and 4)
	s_strains = {
		{1, 2, 232, 0}, // a sneezing cold, from the caves
		{2, 5, 233, 1}, // a cough, from rotting food
		{3, 6, 235, 2}, // a fever, carried by grendels
		{4, 7, 234, 0}, // sleeping sickness, from the caves
	};
	s_nextId = 5;
	s_lastNew = world.worldtickcount; // (the first new strain an hour or so on, not at once)
}

struct Body {
	Agent* agent = nullptr;
	oldCreature* c = nullptr;
	std::string moniker;
	std::shared_ptr<Room> room;
	float cx = 0, cy = 0;
};

std::vector<Body> livingCreatures() {
	std::vector<Body> out;
	if (!world.history || !world.map)
		return out;
	for (auto& a : world.agents) {
		auto* ca = a ? dynamic_cast<CreatureAgent*>(a.get()) : nullptr;
		if (!ca || a->isDying())
			continue;
		auto* c = dynamic_cast<oldCreature*>(ca->getCreature());
		if (!c || !c->isAlive())
			continue;
		Body b;
		b.agent = a.get();
		b.c = c;
		b.moniker = world.history->findMoniker(c->getGenome());
		if (b.moniker.empty())
			continue;
		b.cx = a->x + a->getWidth() / 2.0f;
		b.cy = a->y + a->getHeight() / 2.0f;
		b.room = world.map->roomAt(b.cx, b.cy);
		out.push_back(b);
	}
	return out;
}

void announce(int kind, const std::string& mon, oldCreature* c, const Strain& s) {
	if (!world.history || !world.history->hasMoniker(mon))
		return;
	size_t before = world.history->notices.size();
	world.history->logLife(kind, mon, (int)c->getStage());
	if (world.history->notices.size() > before) {
		world.history->notices.back().cause = s.name();
		world.history->lifelog.back().cause = s.name();
	}
}

bool canCatch(oldCreature* c, Agent* a, const std::string& mon, const Strain& s) {
	if (a->genus == 2) // grendels carry, they do not fall ill
		return false;
	auto& ill = s_ill[mon];
	if ((int)ill.size() >= mostAtOnce)
		return false;
	for (auto& i : ill)
		if (i.strain == s.id)
			return false;
	return c->getChemical((unsigned char)antibodyChem(s)) < immuneAntibody;
}

void catchIt(const Body& b, const Strain& s, const char* how) {
	s_ill[b.moniker].push_back({s.id, world.worldtickcount});
	if (watching())
		fmt::print("* DISEASE {} caught {} ({}) at {:.0f},{:.0f}\n", b.moniker, s.name(), how, b.cx, b.cy);
	announce(lifeLogEntry::fellIll, b.moniker, b.c, s);
}

void addGerms(int room, int strain, float amount) {
	float& g = s_germs[room][strain];
	g = std::clamp(g + amount, 0.0f, 255.0f);
}

void roomsStep() {
	const auto& rooms = world.map->getRooms();
	std::map<int, std::map<int, float>> next = s_germs;
	for (auto& rp : rooms) {
		Room& r = *rp;
		int id = (int)r.id;
		float dark = std::clamp(1.0f - (float)r.lite / darkLight, 0.0f, 1.0f);
		float rot = std::min(rotLevel, std::max(0.0f, (float)(r.ontr - std::max(0, r.ontrBase))) * rotShare);
		bool water = r.type == 2;
		for (auto& s : s_strains) {
			float g = 0;
			auto it = s_germs.find(id);
			if (it != s_germs.end()) {
				auto jt = it->second.find(s.id);
				if (jt != it->second.end())
					g = jt->second;
			}
			float level = 0;
			if (!water) {
				if (s.home == 0)
					level = caveLevel * dark;
				else if (s.home == 1)
					level = rot * (0.5f + 0.5f * dark);
			}
			float d = 0;
			if (g < level)
				d += growth * (level - g);
			d -= g * decay * (1.0f + (float)r.lite / 64.0f) * (water ? 4.0f : 1.0f);
			// seeping through open doorways (half the difference, each way)
			for (auto& [n, door] : r.getDoors()) {
				if (!n || door.perm <= 0 || n.get() == &r)
					continue;
				float gn = 0;
				auto nt = s_germs.find((int)n->id);
				if (nt != s_germs.end()) {
					auto kt = nt->second.find(s.id);
					if (kt != nt->second.end())
						gn = kt->second;
				}
				float open = std::min(door.perm, 255) / 255.0f;
				d += seep * open * 0.5f * (gn - g);
			}
			if (g + d > 0.05f || next[id].count(s.id))
				next[id][s.id] = std::clamp(g + d, 0.0f, 255.0f);
		}
	}
	// drop what has gone
	for (auto it = next.begin(); it != next.end();) {
		for (auto jt = it->second.begin(); jt != it->second.end();)
			jt = jt->second < 0.05f ? it->second.erase(jt) : std::next(jt);
		it = it->second.empty() ? next.erase(it) : std::next(it);
	}
	s_germs.swap(next);
}

void newStrains() {
	if (world.worldtickcount < s_lastNew + newStrainEvery)
		return;
	s_lastNew = world.worldtickcount;
	if (rand_float(0, 1) >= newStrainChance || s_strains.empty())
		return;
	// one strain changes its antigen: a new strain, the old one goes on
	const Strain& from = s_strains[rand_uint32(0, (uint32_t)s_strains.size() - 1)];
	Strain s = from;
	s.id = s_nextId++;
	do
		s.antigen = rand_int32(0, 7);
	while (s.antigen == from.antigen);
	s_strains.push_back(s);
	if (watching())
		fmt::print("* DISEASE new strain {} from {}\n", s.name(), from.name());
	// too many: the ones that have died out go, oldest first
	while (s_strains.size() > mostStrains) {
		auto gone = s_strains.end();
		for (auto it = s_strains.begin(); it != s_strains.end() && s_strains.size() > fewestStrains; ++it) {
			int sid = it->id;
			bool alive = false;
			for (auto& [room, m] : s_germs)
				if (m.count(sid) && m.at(sid) >= 1.0f)
					alive = true;
			for (auto& [mon, v] : s_ill)
				for (auto& i : v)
					if (i.strain == sid)
						alive = true;
			for (auto& [mon, c] : s_carrier)
				if (c == sid)
					alive = true;
			if (!alive) {
				gone = it;
				break;
			}
		}
		if (gone == s_strains.end())
			break;
		s_strains.erase(gone);
	}
}

} // namespace

std::string Strain::name() const {
	const char* what = toxin == 232 ? "a sneezing cold" : toxin == 233 ? "a cough" : toxin == 234 ? "sleeping sickness" : "a fever";
	std::string letters;
	int n = id;
	while (n > 0) {
		letters.insert(letters.begin(), (char)('A' + (n - 1) % 26));
		n = (n - 1) / 26;
	}
	return fmt::format("{} (strain {})", what, letters);
}

void reset() {
	s_strains.clear();
	s_nextId = 1;
	s_lastNew = 0;
	s_germs.clear();
	s_ill.clear();
	s_carrier.clear();
}

void tick() {
	if (off() || engine.version != 2 || !world.map || !world.history)
		return;
	if (world.tickcount % stepTicks != 0)
		return;
	if (s_strains.empty())
		seed();
	roomsStep();
	newStrains();

	auto bodies = livingCreatures();
	std::map<std::string, bool> alive;
	for (auto& b : bodies) {
		alive[b.moniker] = true;
		int room = b.room ? (int)b.room->id : -1;
		if (b.agent->genus == 2) {
			// a grendel carries a strain for life (the grendel strains,
			// else any), and sheds it
			auto it = s_carrier.find(b.moniker);
			if (it == s_carrier.end() || !findStrain(it->second)) {
				std::vector<int> pick;
				for (auto& s : s_strains)
					if (s.home == 2)
						pick.push_back(s.id);
				if (pick.empty())
					for (auto& s : s_strains)
						pick.push_back(s.id);
				s_carrier[b.moniker] = pick[rand_uint32(0, (uint32_t)pick.size() - 1)];
				it = s_carrier.find(b.moniker);
			}
			if (room >= 0)
				addGerms(room, it->second, shedCarrier);
			continue;
		}
		// ill: the germs feed antigen and toxin into the blood; over once the
		// antibody has built up
		auto& ill = s_ill[b.moniker];
		for (auto it = ill.begin(); it != ill.end();) {
			const Strain* s = findStrain(it->strain);
			if (!s) {
				it = ill.erase(it);
				continue;
			}
			bool over = b.c->getChemical((unsigned char)antibodyChem(*s)) >= recoveredAntibody || world.worldtickcount >= it->since + longestIllness;
			if (over) {
				if (watching())
					fmt::print("* DISEASE {} got over {} after {} ticks\n", b.moniker, s->name(), world.worldtickcount - it->since);
				announce(lifeLogEntry::recovered, b.moniker, b.c, *s);
				it = ill.erase(it);
				continue;
			}
			b.c->addChemical((unsigned char)antigenChem(*s), antigenDose);
			b.c->addChemical((unsigned char)s->toxin, toxinDose);
			if (room >= 0)
				addGerms(room, s->id, shedIll);
			++it;
		}
		// catching one from the room
		if (room < 0)
			continue;
		auto gt = s_germs.find(room);
		if (gt == s_germs.end())
			continue;
		for (auto& [sid, g] : gt->second) {
			const Strain* s = findStrain(sid);
			if (!s || g < 1.0f || !canCatch(b.c, b.agent, b.moniker, *s))
				continue;
			float ab = (float)b.c->getChemical((unsigned char)antibodyChem(*s));
			float p = (g / 255.0f) * catchRate * (1.0f - ab / (float)immuneAntibody);
			if (rand_float(0, 1) < p)
				catchIt(b, *s, "from the room");
		}
	}
	// the dead and the gone are ill no more (and carry nothing)
	for (auto it = s_ill.begin(); it != s_ill.end();)
		it = (!alive.count(it->first) || it->second.empty()) ? s_ill.erase(it) : std::next(it);
	for (auto it = s_carrier.begin(); it != s_carrier.end();)
		it = !alive.count(it->first) ? s_carrier.erase(it) : std::next(it);
}

void ateRotten(Agent* eater) {
	if (off() || engine.version != 2 || !eater || !world.history || !world.map)
		return;
	for (auto& b : livingCreatures()) {
		if (b.agent != eater)
			continue;
		for (auto& s : s_strains) {
			if (s.home != 1 || !canCatch(b.c, b.agent, b.moniker, s))
				continue;
			float ab = (float)b.c->getChemical((unsigned char)antibodyChem(s));
			if (rand_float(0, 1) < rottenCatch * (1.0f - ab / (float)immuneAntibody))
				catchIt(b, s, "from rotten fruit");
		}
		return;
	}
}

void sneeze(Agent* sneezer) {
	if (off() || engine.version != 2 || !sneezer || !world.history || !world.map)
		return;
	auto* ca = dynamic_cast<CreatureAgent*>(sneezer);
	auto* c = ca ? dynamic_cast<oldCreature*>(ca->getCreature()) : nullptr;
	if (!c)
		return;
	std::string mon = world.history->findMoniker(c->getGenome());
	std::vector<int> what;
	for (auto& i : s_ill[mon])
		what.push_back(i.strain);
	if (sneezer->genus == 2 && s_carrier.count(mon))
		what.push_back(s_carrier[mon]);
	if (what.empty())
		return;
	float sx = sneezer->x + sneezer->getWidth() / 2.0f, sy = sneezer->y + sneezer->getHeight() / 2.0f;
	if (auto r = world.map->roomAt(sx, sy))
		for (int sid : what)
			addGerms((int)r->id, sid, 5.0f);
	for (auto& b : livingCreatures()) {
		if (b.agent == sneezer || std::fabs(b.cx - sx) > sneezeReach || std::fabs(b.cy - sy) > sneezeHeight)
			continue;
		for (int sid : what)
			if (const Strain* s = findStrain(sid))
				if (canCatch(b.c, b.agent, b.moniker, *s) && rand_float(0, 1) < sneezeCatch)
					catchIt(b, *s, "a sneeze");
	}
	if (watching())
		fmt::print("* DISEASE {} sneezed at {:.0f},{:.0f}\n", mon, sx, sy);
}

bool isIll(const std::string& moniker) {
	auto it = s_ill.find(moniker);
	return it != s_ill.end() && !it->second.empty();
}

std::vector<Illness> illnessesOf(const std::string& moniker) {
	auto it = s_ill.find(moniker);
	return it == s_ill.end() ? std::vector<Illness>() : it->second;
}

const Strain* strain(int id) {
	return findStrain(id);
}

const std::vector<Strain>& strains() {
	return s_strains;
}

int carriedBy(const std::string& moniker) {
	auto it = s_carrier.find(moniker);
	return it == s_carrier.end() ? 0 : it->second;
}

float germsIn(int room, int strain) {
	auto it = s_germs.find(room);
	if (it == s_germs.end())
		return 0;
	auto jt = it->second.find(strain);
	return jt == it->second.end() ? 0 : jt->second;
}

std::string illnessWords(const std::string& moniker) {
	std::string out;
	for (auto& i : illnessesOf(moniker))
		if (const Strain* s = findStrain(i.strain))
			out += (out.empty() ? "" : " and ") + s->name();
	return out;
}

void save(std::ostream& out) {
	if (s_strains.empty())
		return;
	out << "disease-next " << s_nextId << " " << s_lastNew << "\n";
	for (auto& s : s_strains)
		out << "disease-strain " << s.id << " " << s.antigen << " " << s.toxin << " " << s.home << "\n";
	for (auto& [room, m] : s_germs)
		for (auto& [sid, g] : m)
			if (g >= 0.05f)
				out << fmt::format("disease-germs {} {} {:.2f}\n", room, sid, g);
	for (auto& [mon, v] : s_ill)
		for (auto& i : v)
			out << "disease-ill " << mon << " " << i.strain << " " << i.since << "\n";
	for (auto& [mon, sid] : s_carrier)
		out << "disease-carrier " << mon << " " << sid << "\n";
}

bool readLine(const std::string& key, std::istringstream& l) {
	if (key == "disease-next") {
		l >> s_nextId >> s_lastNew;
	} else if (key == "disease-strain") {
		Strain s;
		if (l >> s.id >> s.antigen >> s.toxin >> s.home)
			s_strains.push_back(s);
	} else if (key == "disease-germs") {
		int room, sid;
		float g;
		if (l >> room >> sid >> g)
			s_germs[room][sid] = g;
	} else if (key == "disease-ill") {
		std::string mon;
		Illness i;
		if (l >> mon >> i.strain >> i.since)
			s_ill[mon].push_back(i);
	} else if (key == "disease-carrier") {
		std::string mon;
		int sid;
		if (l >> mon >> sid)
			s_carrier[mon] = sid;
	} else
		return false;
	return true;
}

} // namespace Disease
