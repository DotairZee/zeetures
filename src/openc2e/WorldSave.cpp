/*
 *  WorldSave.cpp
 *  openc2e
 *
 *  Saving and restoring C1/C2 world state in openc2e's own format (stage 2:
 *  creatures, their bodies and minds, and eggs). A world folder holds the
 *  world file it started from (world.sfc); its saved state lives in
 *  save/state.txt with the creatures' genomes in save/genomes/. On load, the
 *  world file is read as usual and the saved creatures and eggs then replace
 *  the ones the world file contains. Other objects still start as they are in
 *  the world file.
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

#include "WorldSave.h"

#include "Agent.h"
#include "Camera.h"
#include "Engine.h"
#include "SpritePart.h"
#include "World.h"
#include "caosValue.h"
#include "common/creaturesImage.h"
#include "common/io/FileReader.h"
#include "common/io/FileWriter.h"
#include "creatures/CreatureAgent.h"
#include "creatures/CreatureMind.h"
#include "creatures/SkeletalCreature.h"
#include "creatures/oldCreature.h"
#include "fileformats/genomeFile.h"
#include "historyManager.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fmt/core.h>
#include <fstream>
#include <memory>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace WorldSave {

static constexpr const char* MAGIC = "openc2e-world-save 1";

static fs::path stateFile(const fs::path& worlddir) {
	return worlddir / "save" / "state.txt";
}

bool HasSavedState(const fs::path& worlddir) {
	return !worlddir.empty() && fs::exists(stateFile(worlddir));
}

static bool isEgg(Agent* a) {
	return engine.version <= 2 && a->family == 2 && a->genus == 5;
}

static oldCreature* oldCreatureOf(Agent* a) {
	CreatureAgent* ca = dynamic_cast<CreatureAgent*>(a);
	return ca ? dynamic_cast<oldCreature*>(ca->getCreature()) : nullptr;
}

static std::string timestamp() {
	std::time_t t = std::time(nullptr);
	char buf[32];
	std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::localtime(&t));
	return buf;
}

bool SaveWorldState(const fs::path& worlddir, std::string& error) {
	if (worlddir.empty()) {
		error = "No world folder is open.";
		return false;
	}
	// write into save.new/, then swap it in, so a failed save never damages
	// the previous one
	fs::path tmp = worlddir / "save.new";
	std::error_code ec;
	fs::remove_all(tmp, ec);
	fs::create_directories(tmp / "genomes", ec);
	if (ec) {
		error = "Couldn't create the save folder: " + ec.message();
		return false;
	}

	std::ofstream out(tmp / "state.txt");
	out << MAGIC << "\n";
	out << "saved " << timestamp() << "\n";
	out << "chemical-relays 1\n"; // body chemistry relays were running when saved
	out << "camera " << engine.camera->getX() << " " << engine.camera->getY() << "\n";

	int ncreatures = 0, neggs = 0;
	for (auto& a : world.agents) {
		if (!a || a->isDying())
			continue;
		oldCreature* c = oldCreatureOf(a.get());
		if (c) {
			if (!c->isAlive())
				continue; // the dead are not saved
			std::string genomename = fmt::format("c{}.gen", ncreatures);
			{
				FileWriter w(tmp / "genomes" / genomename);
				w << *c->getGenome();
			}
			out << "creature\n";
			out << "genome " << genomename << "\n";
			out << "female " << (c->isFemale() ? 1 : 0) << "\n";
			out << "variant " << c->getVariant() << "\n";
			out << "selected " << (world.selectedcreature == a.get() ? 1 : 0) << "\n";
			out << "pos " << a->x << " " << a->y << "\n";
			out << "stage " << (int)c->getStage() << "\n";
			out << "age " << c->getAge() << "\n";
			out << "ticks " << c->getTicks() << "\n";
			out << "ageing " << (c->isAgeing() ? 1 : 0) << "\n";
			out << "asleep " << (c->isAsleep() ? 1 : 0) << "\n";
			out << "chemicals";
			for (int i = 0; i < 256; i++)
				out << " " << (int)c->getChemical(i);
			out << "\norgans";
			for (float f : c->getOrganState())
				out << " " << f;
			out << "\n";
			if (CreatureMind* m = c->getMind()) {
				out << "mind " << m->name() << "\n";
				out << "mindstate\n"
					<< m->saveState() << "endmindstate\n";
			}
			out << "end\n";
			ncreatures++;
		} else if (isEgg(a.get())) {
			SpritePart* sp = dynamic_cast<SpritePart*>(a->part(0));
			if (!sp)
				continue;
			out << "egg\n";
			out << "classifier " << (int)a->family << " " << (int)a->genus << " " << a->species << "\n";
			out << "sprite " << sp->getSprite()->getName() << " " << sp->getFirstImg() << " " << sp->getPose() << "\n";
			out << "attr " << a->attr << "\n";
			out << "timer " << a->timerrate << "\n";
			out << "pos " << a->x << " " << a->y << "\n";
			for (int i = 0; i < 100; i++) {
				const caosValue& v = a->var[i];
				if (v.hasInt())
					out << "var " << i << " i " << v.getInt() << "\n";
				else if (v.hasFloat())
					out << "var " << i << " f " << v.getFloat() << "\n";
			}
			out << "end\n";
			neggs++;
		}
	}
	out.close();
	if (!out) {
		error = "Couldn't write the save file.";
		return false;
	}

	fs::path dest = worlddir / "save";
	fs::path old = worlddir / "save.old";
	fs::remove_all(old, ec);
	if (fs::exists(dest))
		fs::rename(dest, old, ec);
	fs::rename(tmp, dest, ec);
	if (ec) {
		error = "Couldn't put the new save in place: " + ec.message();
		return false;
	}
	fs::remove_all(old, ec);
	fmt::print("* Saved world: {} creatures, {} eggs\n", ncreatures, neggs);
	return true;
}

struct SavedCreature {
	std::string genome, mind, mindstate;
	bool female = false, selected = false, ageing = false, asleep = false;
	int variant = 0, stage = 0;
	float x = 0, y = 0;
	unsigned int age = 0, ticks = 0;
	std::vector<int> chemicals;
	std::vector<float> organs;
};

struct SavedEgg {
	int family = 2, genus = 5, species = 0;
	std::string sprite = "eggs";
	unsigned int firstimg = 0, pose = 0, attr = 195, timer = 0;
	float x = 0, y = 0;
	std::vector<std::string> vars; // "setv ovNN value" lines
};

// Saves made before chemical relay lobes ran (see oldBrain::tickChemicalRelays)
// can hold creatures whose ATP ran out because of that missing relay. With no
// ATP, C2 organs do nothing, and the relay alone cannot restart them (the
// enzyme it releases is itself emitted by an organ). When loading such a save,
// a living creature with no ATP gets its genome's starting ATP back, once.
static unsigned char genomeInitialLevel(genomeFile& g, unsigned char chemical, unsigned char fallback) {
	for (auto& gn : g.genes)
		if (auto* ic = dynamic_cast<bioInitialConcentrationGene*>(gn.get()))
			if (ic->chemical == chemical)
				return ic->quantity;
	return fallback;
}

// Builds a creature from a genome and a saved state. With a moniker, the new
// body takes over that existing moniker (and its history) instead of a new one.
static Agent* buildCreature(std::shared_ptr<genomeFile> genome, const SavedCreature& s, bool revive_atp, const std::string& moniker = "") {
	SkeletalCreature* a = new SkeletalCreature(4);
	oldCreature* c;
	if (engine.version == 1)
		c = new c1Creature(genome, s.female, (unsigned char)s.variant, a);
	else
		c = new c2Creature(genome, s.female, (unsigned char)s.variant, a);
	a->setCreature(c);
	a->finishInit();
	a->genome_slots[0] = genome;
	if (moniker.empty()) {
		world.newMoniker(genome, "saved", a);
		world.history->getMoniker(world.history->findMoniker(genome)).moveToCreature(a);
	} else {
		world.history->getMoniker(moniker).transferCreature(a);
	}

	for (int i = 0; i < s.stage; i++)
		c->ageCreature(); // also updates genes and appearance for each stage
	if (s.ageing)
		c->born();
	c->restoreLifeCounters(s.ticks, s.age);
	for (size_t i = 0; i < s.chemicals.size() && i < 256; i++)
		c->setChemicalLevel((unsigned char)i, (unsigned char)s.chemicals[i]);
	c->setOrganState(s.organs);
	if (revive_atp && engine.version == 2 && c->isAlive() && c->getChemical(99) == 0) { // 99 = ATP
		unsigned char atp = genomeInitialLevel(*genome, 99, 200);
		c->setChemicalLevel(99, atp);
		fmt::print("* Restarted a creature's metabolism (ATP 0 -> {}) after loading an older save\n", (int)atp);
	}
	if (s.asleep)
		c->setAsleep(true);
	a->moveTo(s.x, s.y, true);
	if (CreatureMind* m = c->getMind()) {
		if (m->name() == s.mind)
			m->loadState(s.mindstate);
		else if (!s.mind.empty())
			fmt::print("* Note: creature saved with mind '{}' loaded with '{}'; learned state not restored\n", s.mind, m->name());
	}
	if (s.selected)
		world.selectCreature(AgentRef(a).lock());
	return a;
}

static void restoreCreature(const fs::path& savedir, const SavedCreature& s, bool revive_atp) {
	auto genome = std::make_shared<genomeFile>();
	{
		FileReader r(savedir / "genomes" / s.genome);
		r >> *genome;
	}
	buildCreature(genome, s, revive_atp);
}

static void restoreEgg(const SavedEgg& e) {
	std::string script = fmt::format("new: simp {} 8 {} 2000 0\n", e.sprite, e.firstimg);
	script += fmt::format("pose {}\n", e.pose);
	script += fmt::format("setv cls2 {} {} {}\nsetv attr {}\n", e.family, e.genus, e.species, e.attr);
	for (auto& v : e.vars)
		script += v + "\n";
	if (e.timer)
		script += fmt::format("tick {}\n", e.timer);
	script += fmt::format("mvto {} {}\nsetv grav 1\n", e.x, e.y);
	std::string err = engine.executeNetwork(script);
	if (!err.empty())
		fmt::print("* Couldn't restore an egg: {}\n", err);
}

bool LoadWorldState(const fs::path& worlddir) {
	if (!HasSavedState(worlddir))
		return false;
	fs::path savedir = worlddir / "save";
	std::ifstream in(stateFile(worlddir));
	std::string line;
	std::getline(in, line);
	if (line != MAGIC) {
		fmt::print("* Save file has an unknown format; starting from the world file\n");
		return false;
	}

	std::vector<SavedCreature> creatures;
	std::vector<SavedEgg> eggs;
	int camx = -1, camy = -1;
	bool relays_saved = false;
	SavedCreature* c = nullptr;
	SavedEgg* e = nullptr;
	while (std::getline(in, line)) {
		std::istringstream l(line);
		std::string key;
		l >> key;
		if (key == "chemical-relays") {
			relays_saved = true;
		} else if (key == "camera") {
			l >> camx >> camy;
		} else if (key == "creature") {
			creatures.emplace_back();
			c = &creatures.back();
			e = nullptr;
		} else if (key == "egg") {
			eggs.emplace_back();
			e = &eggs.back();
			c = nullptr;
		} else if (key == "end") {
			c = nullptr;
			e = nullptr;
		} else if (c) {
			if (key == "genome")
				l >> c->genome;
			else if (key == "female")
				l >> c->female;
			else if (key == "variant")
				l >> c->variant;
			else if (key == "selected")
				l >> c->selected;
			else if (key == "pos")
				l >> c->x >> c->y;
			else if (key == "stage")
				l >> c->stage;
			else if (key == "age")
				l >> c->age;
			else if (key == "ticks")
				l >> c->ticks;
			else if (key == "ageing")
				l >> c->ageing;
			else if (key == "asleep")
				l >> c->asleep;
			else if (key == "chemicals") {
				int v;
				while (l >> v)
					c->chemicals.push_back(v);
			} else if (key == "organs") {
				float v;
				while (l >> v)
					c->organs.push_back(v);
			} else if (key == "mind")
				l >> c->mind;
			else if (key == "mindstate") {
				while (std::getline(in, line) && line != "endmindstate")
					c->mindstate += line + "\n";
			}
		} else if (e) {
			if (key == "classifier")
				l >> e->family >> e->genus >> e->species;
			else if (key == "sprite")
				l >> e->sprite >> e->firstimg >> e->pose;
			else if (key == "attr")
				l >> e->attr;
			else if (key == "timer")
				l >> e->timer;
			else if (key == "pos")
				l >> e->x >> e->y;
			else if (key == "var") {
				int n;
				std::string type, value;
				l >> n >> type >> value;
				if (n >= 0 && n < 100)
					e->vars.push_back(fmt::format("setv ov{:02d} {}", n, value));
			}
		}
	}

	// the saved creatures and eggs replace those in the world file
	std::vector<std::shared_ptr<Agent>> remove;
	for (auto& a : world.agents)
		if (a && (oldCreatureOf(a.get()) || isEgg(a.get())))
			remove.push_back(a);
	for (auto& a : remove)
		a->kill();

	for (auto& s : creatures) {
		try {
			restoreCreature(savedir, s, !relays_saved);
		} catch (std::exception& ex) {
			fmt::print("* Couldn't restore a creature ({}): {}\n", s.genome, ex.what());
		}
	}
	for (auto& s : eggs)
		restoreEgg(s);
	if (camx >= 0 && camy >= 0)
		engine.camera->moveToGlobal(camx, camy, jump);
	fmt::print("* Restored saved world: {} creatures, {} eggs\n", creatures.size(), eggs.size());
	return true;
}

} // namespace WorldSave

// Changing a creature's life stage at will (owner request, 2026-11-08). C1/C2
// life stages follow a "life" chemical: the genome's ageing receptors (organ
// 1, tissue 0, loci 0-6) each fire when it falls to their threshold, and the
// one for the current stage moves the creature on. So besides the stage, the
// chemical is set to where the new stage begins (the previous stage's
// threshold; the starting level for a baby), or the creature would at once
// age back or keep its old timing.
static int lifeLevelForStage(oldCreature* c, int stage) {
	std::shared_ptr<genomeFile> g = c->getGenome();
	int chem = -1, threshold = -1;
	for (auto& gn : g->genes) {
		auto* r = dynamic_cast<bioReceptorGene*>(gn.get());
		if (!r || r->organ != 1 || r->tissue != 0)
			continue;
		if (r->header.flags.notexpressed)
			continue;
		chem = r->chemical;
		if (stage > 0 && r->locus == stage - 1)
			threshold = r->threshold;
	}
	if (chem < 0)
		return -1;
	if (stage == 0)
		threshold = WorldSave::genomeInitialLevel(*g, (unsigned char)chem, 255);
	if (threshold < 0)
		return -1;
	return (chem << 8) | threshold;
}

namespace WorldSave {

bool SetCreatureLifeStage(std::shared_ptr<Agent>& agent, int stage, std::string& error) {
	oldCreature* c = agent ? oldCreatureOf(agent.get()) : nullptr;
	if (!c || !c->isAlive()) {
		error = "not a living C1/C2 creature";
		return false;
	}
	stage = std::clamp(stage, 0, 6);
	int current = (int)c->getStage();
	if (stage == current)
		return true;
	if (stage > current) {
		while ((int)c->getStage() < stage)
			c->ageCreature(); // genes and appearance for each stage, as natural ageing
	} else {
		// genes that have switched on cannot be switched off: rebuild the
		// creature at the younger stage with its chemistry, organs and mind
		SavedCreature s;
		s.female = c->isFemale();
		s.variant = c->getVariant();
		s.selected = world.selectedcreature.lock() == agent;
		s.x = agent->x;
		s.y = agent->y;
		s.stage = stage;
		s.age = c->getAge();
		s.ticks = c->getTicks();
		s.ageing = c->isAgeing();
		s.asleep = c->isAsleep();
		for (int i = 0; i < 256; i++)
			s.chemicals.push_back(c->getChemical(i));
		s.organs = c->getOrganState();
		if (CreatureMind* m = c->getMind()) {
			s.mind = m->name();
			s.mindstate = m->saveState();
		}
		std::shared_ptr<genomeFile> genome = c->getGenome();
		std::string moniker = world.history->findMoniker(genome);
		agent->kill();
		Agent* a = buildCreature(genome, s, false, moniker);
		c = oldCreatureOf(a);
		agent = AgentRef(a).lock();
	}
	int life = lifeLevelForStage(c, stage);
	if (life >= 0)
		c->setChemicalLevel((unsigned char)(life >> 8), (unsigned char)(life & 255));
	fmt::print("* Creature life stage {} -> {}{}\n", current, (int)c->getStage(),
		life >= 0 ? fmt::format(" (life chemical {} set to {})", life >> 8, life & 255) : "");
	return true;
}

} // namespace WorldSave

// Changing life stage "naturally" (owner request): one stage at a time, with
// a short pause between stages. Older: the life chemical is lowered to where
// the current stage ends, so the genome's own ageing receptor fires and the
// creature ages exactly as it would naturally (genes, body, life event); if
// the genome has no such receptor, it is aged directly after a while.
// Younger has no natural process; each stage back rebuilds the creature (see
// SetCreatureLifeStage), at the same pace.
namespace WorldSave {

struct StageChange {
	AgentRef creature;
	int target = 0;
	unsigned int wait = 0; // ticks before the next step
	int stepFrom = -1; // stage when the current ageing step began (-1: none)
	unsigned int stepTicks = 0;
};
static std::vector<StageChange> s_stagechanges;
static const unsigned int stagePause = 20; // ticks between stages (2 s in C2)
static const unsigned int stepTimeout = 60; // ticks to wait for the ageing receptor

void RequestLifeStage(std::shared_ptr<Agent> creature, int stage) {
	if (!creature || !oldCreatureOf(creature.get()))
		return;
	s_stagechanges.erase(std::remove_if(s_stagechanges.begin(), s_stagechanges.end(),
							 [&](StageChange& sc) { return sc.creature.lock() == creature; }),
		s_stagechanges.end());
	StageChange sc;
	sc.creature = AgentRef(creature);
	sc.target = std::clamp(stage, 0, 6);
	s_stagechanges.push_back(sc);
}

void TickLifeStageChanges() {
	for (auto it = s_stagechanges.begin(); it != s_stagechanges.end();) {
		std::shared_ptr<Agent> a = it->creature.lock();
		oldCreature* c = a ? oldCreatureOf(a.get()) : nullptr;
		if (!c || !c->isAlive() || (int)c->getStage() == it->target) {
			it = s_stagechanges.erase(it);
			continue;
		}
		int stage = (int)c->getStage();
		if (it->stepFrom >= 0) {
			// waiting for the body to age itself
			if (stage != it->stepFrom) {
				it->stepFrom = -1;
				it->wait = stagePause;
			} else if (++it->stepTicks >= stepTimeout) {
				c->ageCreature();
				it->stepFrom = -1;
				it->wait = stagePause;
			}
			++it;
			continue;
		}
		if (it->wait > 0) {
			it->wait--;
			++it;
			continue;
		}
		if (it->target > stage) {
			int life = lifeLevelForStage(c, stage + 1);
			if (life >= 0 && c->getChemical((unsigned char)(life >> 8)) > (life & 255))
				c->setChemicalLevel((unsigned char)(life >> 8), (unsigned char)(life & 255));
			it->stepFrom = stage;
			it->stepTicks = 0;
		} else {
			std::string err;
			if (!SetCreatureLifeStage(a, stage - 1, err)) {
				it = s_stagechanges.erase(it);
				continue;
			}
			it->creature = AgentRef(a);
			it->wait = stagePause;
		}
		++it;
	}
}

} // namespace WorldSave

