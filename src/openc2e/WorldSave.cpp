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

static void restoreCreature(const fs::path& savedir, const SavedCreature& s) {
	auto genome = std::make_shared<genomeFile>();
	{
		FileReader r(savedir / "genomes" / s.genome);
		r >> *genome;
	}
	SkeletalCreature* a = new SkeletalCreature(4);
	oldCreature* c;
	if (engine.version == 1)
		c = new c1Creature(genome, s.female, (unsigned char)s.variant, a);
	else
		c = new c2Creature(genome, s.female, (unsigned char)s.variant, a);
	a->setCreature(c);
	a->finishInit();
	a->genome_slots[0] = genome;
	world.newMoniker(genome, "saved", a);
	world.history->getMoniker(world.history->findMoniker(genome)).moveToCreature(a);

	for (int i = 0; i < s.stage; i++)
		c->ageCreature(); // also updates genes and appearance for each stage
	if (s.ageing)
		c->born();
	c->restoreLifeCounters(s.ticks, s.age);
	for (size_t i = 0; i < s.chemicals.size() && i < 256; i++)
		c->setChemicalLevel((unsigned char)i, (unsigned char)s.chemicals[i]);
	c->setOrganState(s.organs);
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
	SavedCreature* c = nullptr;
	SavedEgg* e = nullptr;
	while (std::getline(in, line)) {
		std::istringstream l(line);
		std::string key;
		l >> key;
		if (key == "camera") {
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
			restoreCreature(savedir, s);
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
