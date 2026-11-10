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
#include "CobManager.h"
#include "CompoundAgent.h"
#include "DullPart.h"
#include "SimpleAgent.h"
#include "Vehicle.h"
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
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <system_error>
#include <typeinfo>

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

// Objects a script asks to keep: OV99 set to KEEP_MARK. They are saved and
// restored like eggs (sprite, picture, plane, attributes, timer, place and
// object variables), so things scripts make at load time, such as plants
// that grow over game days, carry on between sessions.
static const int KEEP_MARK = 7461;
static bool isKept(Agent* a) {
	return engine.version <= 2 && !isEgg(a) && a->var[99].hasInt() && a->var[99].getInt() == KEEP_MARK;
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

static std::string escapeLine(const std::string& t);

// Objects added by hand from the Agent Injector (owner, 2026-11-10), and
// what they make later (Agent::injectedFrom). Unlike kept objects these can
// be compound objects and vehicles, so each is saved in full: its parts,
// their pictures and animations, hotspots and their functions, a vehicle's
// cabin, physics, pickup points, and object variables, including ones that
// point to other objects (another saved object, or one from the world file,
// found again by kind and place). Their COB's scripts are installed again at
// load (world.injectedCobs), since scripts are not saved. Only plain C2
// objects, vehicles and compound objects are saved this way.
static bool isInjected(Agent* a) {
	if (engine.version != 2 || a->injectedFrom.empty() || isEgg(a) || isKept(a))
		return false;
	const std::type_info& t = typeid(*a);
	return t == typeid(SimpleAgent) || t == typeid(CompoundAgent) || t == typeid(Vehicle);
}

static void savePart(std::ostream& out, CompoundPart* cp) {
	SpritePart* p = dynamic_cast<SpritePart*>(cp);
	if (!p || !p->getSprite())
		return;
	out << fmt::format("part {} {} {} {} {} {} {} {} {} {} anim {}", cp->id, p->getSprite()->getName(), p->getFirstImg(), cp->x, cp->y,
		(int)cp->zorder, p->getBase(), p->getPose(), (int)p->framerate, p->getFrameNo(), p->animation.size());
	for (auto f : p->animation)
		out << " " << (int)f;
	out << "\n";
}

static void saveInjected(std::ostream& out, int& count) {
	for (auto& name : world.injectedCobs)
		out << "injected " << escapeLine(name) << "\n";
	std::map<Agent*, int> ids;
	for (auto& a : world.agents)
		if (a && !a->isDying() && isInjected(a.get())) {
			int n = (int)ids.size();
			ids[a.get()] = n;
		}
	for (auto& a : world.agents) {
		if (!a || a->isDying() || !ids.count(a.get()))
			continue;
		Agent* ag = a.get();
		CompoundAgent* comp = dynamic_cast<CompoundAgent*>(ag);
		Vehicle* vhcl = dynamic_cast<Vehicle*>(ag);
		out << "iobject\n";
		out << "id " << ids[ag] << "\n";
		out << "from " << escapeLine(ag->injectedFrom) << "\n";
		out << "kind " << (vhcl ? "vhcl" : comp ? "comp" : "simp") << "\n";
		out << "classifier " << (int)ag->family << " " << (int)ag->genus << " " << ag->species << "\n";
		out << "plane " << (int)ag->zorder << "\n";
		out << fmt::format("pos {} {}\n", ag->x, ag->y);
		out << fmt::format("vel {} {}\n", ag->velx, ag->vely);
		out << "attr " << ag->attr << "\n";
		out << "actv " << ag->actv << "\n";
		out << "clac " << ag->clac[0] << " " << ag->clac[1] << " " << ag->clac[2] << " " << ag->clik << "\n";
		out << "timer " << ag->timerrate << " " << (ag->timerrate ? ag->getTimerPhase() : 0) << "\n";
		// (C2 physics only: friction, perm and elas are not used by C2
		// objects, and openc2e leaves them unset for them)
		out << fmt::format("physics {} {} {} {} {} {} {} {} {}\n", ag->accg, ag->aero, ag->rest, ag->size, ag->thrt, ag->range,
			ag->falling ? 1 : 0, ag->paused ? 1 : 0, ag->visible ? 1 : 0);
		for (auto& [pose, pt] : ag->carried_points)
			out << "puhl " << pose << " " << pt.first << " " << pt.second << "\n";
		for (auto& [pose, pt] : ag->carry_points)
			out << "pupt " << pose << " " << pt.first << " " << pt.second << "\n";
		if (ag->has_custom_core_size)
			out << fmt::format("core {} {} {} {}\n", ag->custom_core_ytop, ag->custom_core_ybottom, ag->custom_core_xleft, ag->custom_core_xright);
		for (int i = 0; i < 100; i++) {
			const caosValue& v = ag->var[i];
			if (v.hasInt()) {
				if (v.getInt() != 0) // a new object's variables start at 0
					out << "var " << i << " i " << v.getInt() << "\n";
			} else if (v.hasFloat())
				out << fmt::format("var {} f {}\n", i, v.getFloat());
			else if (v.hasString())
				out << "var " << i << " s " << escapeLine(v.getString()) << "\n";
			else if (v.hasAgent()) {
				Agent* t = v.getAgent().get();
				if (!t)
					continue;
				if (ids.count(t))
					out << "var " << i << " a " << ids[t] << "\n";
				else
					out << fmt::format("var {} near {} {} {} {} {}\n", i, (int)t->family, (int)t->genus, t->species, t->x, t->y);
			}
		}
		if (comp) {
			out << "comp " << comp->getSpriteFile() << " " << comp->getFirstImage() << " " << comp->getImageCount() << "\n";
			for (auto* p : comp->parts)
				savePart(out, p);
			for (int i = 0; i < 6; i++) {
				auto& h = comp->hotspots[i];
				auto& f = comp->hotspotfunctions[i];
				// (an unbound function's message and mask are never set)
				out << "spot " << i << " " << h.left << " " << h.top << " " << h.right << " " << h.bottom << " " << f.hotspot << " "
					<< (f.hotspot >= 0 ? f.message : 0) << " " << (f.hotspot >= 0 ? (int)f.mask : 3) << "\n";
			}
		} else {
			savePart(out, ag->part(0));
		}
		if (vhcl)
			out << "cabin " << vhcl->cabinleft << " " << vhcl->cabintop << " " << vhcl->cabinright << " " << vhcl->cabinbottom << " " << vhcl->cabinplane << " "
				<< vhcl->getCapacity() << " " << vhcl->getBump() << " " << vhcl->xvec << " " << vhcl->yvec << "\n";
		out << "end\n";
		count++;
	}
}

struct SavedPart {
	unsigned int id = 0, firstimg = 0, base = 0, pose = 0, frameno = 0;
	std::string sprite;
	int x = 0, y = 0, z = 0, framerate = 0;
	std::vector<unsigned char> anim;
};

struct SavedInjected {
	int id = -1, family = 0, genus = 0, species = 0, plane = 0;
	std::string from, kind = "simp", compsprite;
	unsigned int compfirst = 0, compcount = 0;
	float x = 0, y = 0, velx = 0, vely = 0, accg = 0, range = 0;
	unsigned int attr = 0, timer = 0, since = 0;
	int actv = 0, clac[3] = {-1, -1, -1}, clik = -1, aero = 0, rest = 0, size = 0, thrt = 0;
	bool falling = false, paused = false, visible = true;
	std::map<unsigned int, std::pair<int, int>> puhl, pupt;
	bool core = false;
	float coretop = 0, corebottom = 0, coreleft = 0, coreright = 0;
	struct Var {
		int n;
		std::string type, value;
		int family = 0, genus = 0, species = 0;
		float x = 0, y = 0;
	};
	std::vector<Var> vars;
	std::vector<SavedPart> parts;
	struct Spot {
		int l = -1, t = -1, r = -1, b = -1, func = -1, message = 0, mask = 3;
	};
	Spot spots[6];
	bool hasCabin = false;
	int cabin[5] = {0, 0, 0, 0, 1};
	unsigned int capacity = 0, bump = 0;
	int xvec = 0, yvec = 0;
};

static void readInjectedLine(SavedInjected& o, const std::string& key, std::istringstream& l, const std::function<std::string()>& rest) {
	if (key == "id")
		l >> o.id;
	else if (key == "from")
		o.from = rest();
	else if (key == "kind")
		l >> o.kind;
	else if (key == "classifier")
		l >> o.family >> o.genus >> o.species;
	else if (key == "plane")
		l >> o.plane;
	else if (key == "pos")
		l >> o.x >> o.y;
	else if (key == "vel")
		l >> o.velx >> o.vely;
	else if (key == "attr")
		l >> o.attr;
	else if (key == "actv")
		l >> o.actv;
	else if (key == "clac")
		l >> o.clac[0] >> o.clac[1] >> o.clac[2] >> o.clik;
	else if (key == "timer")
		l >> o.timer >> o.since;
	else if (key == "physics")
		l >> o.accg >> o.aero >> o.rest >> o.size >> o.thrt >> o.range >> o.falling >> o.paused >> o.visible;
	else if (key == "puhl" || key == "pupt") {
		unsigned int pose;
		int px, py;
		if (l >> pose >> px >> py)
			(key == "puhl" ? o.puhl : o.pupt)[pose] = {px, py};
	} else if (key == "core") {
		o.core = (bool)(l >> o.coretop >> o.corebottom >> o.coreleft >> o.coreright);
	} else if (key == "var") {
		SavedInjected::Var v;
		l >> v.n >> v.type;
		if (v.type == "s")
			v.value = rest();
		else if (v.type == "near")
			l >> v.family >> v.genus >> v.species >> v.x >> v.y;
		else
			l >> v.value;
		if (v.n >= 0 && v.n < 100)
			o.vars.push_back(v);
	} else if (key == "comp") {
		l >> o.compsprite >> o.compfirst >> o.compcount;
	} else if (key == "part") {
		SavedPart p;
		std::string animkey;
		size_t n = 0;
		if (l >> p.id >> p.sprite >> p.firstimg >> p.x >> p.y >> p.z >> p.base >> p.pose >> p.framerate >> p.frameno >> animkey >> n) {
			for (size_t i = 0; i < n; i++) {
				int f;
				if (l >> f)
					p.anim.push_back((unsigned char)f);
			}
			o.parts.push_back(p);
		}
	} else if (key == "spot") {
		int i;
		SavedInjected::Spot sp;
		if (l >> i >> sp.l >> sp.t >> sp.r >> sp.b >> sp.func >> sp.message >> sp.mask && i >= 0 && i < 6)
			o.spots[i] = sp;
	} else if (key == "cabin") {
		o.hasCabin = (bool)(l >> o.cabin[0] >> o.cabin[1] >> o.cabin[2] >> o.cabin[3] >> o.cabin[4] >> o.capacity >> o.bump >> o.xvec >> o.yvec);
	}
}

static void applyPart(SpritePart* p, const SavedPart& s) {
	p->setBase(s.base);
	p->setPose(s.pose);
	p->setFramerate((unsigned char)s.framerate);
	p->animation.assign(s.anim.begin(), s.anim.end());
	if (!p->animation.empty() && s.frameno < p->animation.size() && p->animation[s.frameno] != 255)
		p->setFrameNo(s.frameno);
}

static void restoreInjected(std::vector<SavedInjected>& objects) {
	std::map<int, Agent*> made;
	std::vector<std::pair<Agent*, SavedInjected*>> all;
	for (auto& o : objects) {
		try {
			if (o.parts.empty())
				continue;
			Agent* ag = nullptr;
			if (o.kind == "simp") {
				const SavedPart& p0 = o.parts[0];
				SimpleAgent* a = new SimpleAgent(o.family, o.genus, o.species, o.plane, p0.sprite, p0.firstimg, 0);
				a->finishInit();
				if (auto* sp = dynamic_cast<SpritePart*>(a->part(0)))
					applyPart(sp, p0);
				ag = a;
			} else {
				CompoundAgent* a = o.kind == "vhcl" ? new Vehicle(o.compsprite, o.compfirst, o.compcount) : new CompoundAgent(o.compsprite, o.compfirst, o.compcount);
				a->setClassifier(o.family, o.genus, o.species);
				a->finishInit();
				if (o.plane != 0)
					a->setZOrder(o.plane);
				for (auto& sp : o.parts) {
					DullPart* p = new DullPart(a, sp.id, sp.sprite, sp.firstimg, sp.x, sp.y, sp.z);
					a->addPart(p);
					applyPart(p, sp);
				}
				for (int i = 0; i < 6; i++) {
					auto& h = o.spots[i];
					a->setHotspotLoc(i, h.l, h.t, h.r, h.b);
					a->setHotspotFunc(i, h.func);
					a->setHotspotFuncDetails(i, (uint16_t)h.message, (uint8_t)h.mask);
				}
				if (Vehicle* v = dynamic_cast<Vehicle*>(a)) {
					if (o.hasCabin) {
						v->setCabinRect(o.cabin[0], o.cabin[1], o.cabin[2], o.cabin[3]);
						v->cabinplane = o.cabin[4];
						v->setCapacity(o.capacity);
						v->setBump(o.bump);
						v->xvec = o.xvec;
						v->yvec = o.yvec;
					}
				}
				ag = a;
			}
			ag->setClassifier(o.family, o.genus, o.species);
			ag->moveTo(o.x, o.y);
			ag->setAttributes(o.attr);
			ag->actv = o.actv;
			for (int i = 0; i < 3; i++)
				ag->clac[i] = o.clac[i];
			ag->clik = o.clik;
			ag->timerrate = o.timer;
			ag->setTimerPhase(o.since);
			ag->velx = o.velx;
			ag->vely = o.vely;
			ag->accg = o.accg;
			ag->aero = o.aero;
			ag->rest = o.rest;
			ag->size = o.size;
			ag->thrt = o.thrt;
			ag->range = o.range;
			ag->falling = o.falling;
			ag->paused = o.paused;
			ag->visible = o.visible;
			ag->carried_points = o.puhl;
			ag->carry_points = o.pupt;
			if (o.core) {
				ag->has_custom_core_size = true;
				ag->custom_core_ytop = o.coretop;
				ag->custom_core_ybottom = o.corebottom;
				ag->custom_core_xleft = o.coreleft;
				ag->custom_core_xright = o.coreright;
			}
			ag->injectedFrom = o.from;
			made[o.id] = ag;
			all.push_back({ag, &o});
		} catch (std::exception& e) {
			fmt::print("* Couldn't restore an injected object ({} {} {}): {}\n", o.family, o.genus, o.species, e.what());
		}
	}
	// object variables last, once every object they may point to exists
	for (auto& [ag, o] : all) {
		for (auto& v : o->vars) {
			caosValue& dest = ag->var[v.n];
			if (v.type == "i")
				dest.setInt(std::atoi(v.value.c_str()));
			else if (v.type == "f")
				dest.setFloat((float)std::atof(v.value.c_str()));
			else if (v.type == "s")
				dest.setString(v.value);
			else if (v.type == "a") {
				auto it = made.find(std::atoi(v.value.c_str()));
				if (it != made.end())
					dest.setAgent(it->second);
			} else if (v.type == "near") {
				// an object from the world file: the nearest of its kind
				Agent* best = nullptr;
				float bestd = 300.0f * 300.0f;
				for (auto& a : world.agents) {
					if (!a || a->isDying() || a->family != v.family || a->genus != v.genus || a->species != v.species)
						continue;
					float d = (a->x - v.x) * (a->x - v.x) + (a->y - v.y) * (a->y - v.y);
					if (d <= bestd) {
						bestd = d;
						best = a.get();
					}
				}
				if (best)
					dest.setAgent(best);
			}
		}
	}
	// as for objects from the world file: "enter scope"
	for (auto& [ag, o] : all)
		ag->queueScript(7);
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
	// the calendar: world ticks, part of the day, day of the season, season,
	// year (without this every session began again in spring of year 0)
	out << "calendar " << world.worldtickcount << " " << world.timeofday << " " << world.dayofseason << " " << world.season << " " << world.year << "\n";
	// default names: how many of each kind this world has had
	for (auto& [genus, n] : world.history->namecount)
		out << "names " << genus << " " << n << "\n";
	out << "names-given " << (world.history->namesgiven ? 1 : 0) << "\n";
	// the life log (Owner's Kit): kind, tick, genus, female, stage, age, age
	// known, old age, moniker, then the name
	for (auto& e : world.history->lifelog)
		out << "life " << e.kind << " " << e.tick << " " << e.genus << " " << (e.female ? 1 : 0) << " " << e.stage << " " << e.age << " "
			<< (e.ageknown ? 1 : 0) << " " << (e.oldage ? 1 : 0) << " " << (e.moniker.empty() ? "-" : e.moniker) << " " << escapeLine(e.name) << "\n";

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
			{
				std::string mon = world.history->findMoniker(c->getGenome());
				monikerData& m = world.history->getMoniker(mon);
				out << "moniker " << mon << "\n";
				if (!m.mother.empty() || !m.father.empty())
					out << "parents " << (m.mother.empty() ? "-" : m.mother) << " " << (m.father.empty() ? "-" : m.father) << "\n";
				if (!m.name.empty())
					out << "name " << escapeLine(m.name) << "\n";
				out << "registered " << (m.registered ? 1 : 0) << "\n";
				out << "born " << m.borntick << " " << (m.bornknown ? 1 : 0) << "\n";
				for (auto& ph : m.photos)
					out << "photo " << escapeLine(ph) << "\n";
				if (!m.notes.empty())
					out << "notes " << escapeLine(m.notes) << "\n";
			}
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
		} else if (isEgg(a.get()) || isKept(a.get())) {
			SpritePart* sp = dynamic_cast<SpritePart*>(a->part(0));
			if (!sp)
				continue;
			if (isKept(a.get())) {
				out << "object\n";
				out << "frames " << (int)sp->getSprite()->numframes() - (int)sp->getFirstImg() << "\n";
				out << "plane " << (int)a->zorder << "\n"; // signed: planes can be below 0
			} else {
				out << "egg\n";
			}
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
	int ninjected = 0;
	saveInjected(out, ninjected);
	// the records of the dead (Owner's Kit family tree, life log): no body,
	// just who they were
	int nrecords = 0;
	for (auto& [mon, m] : world.history->all()) {
		if (!m.died)
			continue;
		out << "record\n";
		out << "moniker " << mon << "\n";
		out << "genus " << (int)m.genus << " " << m.gender << "\n";
		if (!m.mother.empty() || !m.father.empty())
			out << "parents " << (m.mother.empty() ? "-" : m.mother) << " " << (m.father.empty() ? "-" : m.father) << "\n";
		if (!m.name.empty())
			out << "name " << escapeLine(m.name) << "\n";
		out << "registered " << (m.registered ? 1 : 0) << "\n";
		out << "born " << m.borntick << " " << (m.bornknown ? 1 : 0) << "\n";
		out << "died " << m.diedtick << " " << m.diedstage << "\n";
		for (auto& ph : m.photos)
			out << "photo " << escapeLine(ph) << "\n";
		if (!m.notes.empty())
			out << "notes " << escapeLine(m.notes) << "\n";
		out << "end\n";
		nrecords++;
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
	fmt::print("* Saved world: {} creatures, {} eggs, {} injected objects, {} records of the dead\n", ncreatures, neggs, ninjected, nrecords);
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
	// the owner's records (monikerData)
	std::string name, notes;
	bool registered = false, hasborn = false, bornknown = false;
	unsigned int born = 0;
	std::vector<std::string> photos;
	std::string moniker, mother, father; // as saved (monikers used to be new each load)
	// a record only (a creature that has died): its kind, sex and death
	bool record = false;
	int genus = 1, gender = -1, diedstage = -1;
	unsigned int diedtick = 0;
};

// one line of free text in state.txt: backslashes and line breaks escaped
static std::string escapeLine(const std::string& t) {
	std::string o;
	for (char ch : t) {
		if (ch == '\\')
			o += "\\\\";
		else if (ch == '\n')
			o += "\\n";
		else if (ch != '\r')
			o += ch;
	}
	return o;
}

static std::string unescapeLine(const std::string& t) {
	std::string o;
	for (size_t i = 0; i < t.size(); i++) {
		if (t[i] == '\\' && i + 1 < t.size()) {
			o += t[i + 1] == 'n' ? '\n' : t[i + 1];
			i++;
		} else {
			o += t[i];
		}
	}
	return o;
}

// the rest of a line after its key and one space
static std::string restOfLine(std::istringstream& l) {
	std::string rest;
	std::getline(l, rest);
	if (!rest.empty() && rest[0] == ' ')
		rest.erase(0, 1);
	return rest;
}

struct SavedEgg {
	bool kept = false; // a kept object rather than an egg
	int frames = 8, plane = 2000;
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
	if (s.ageing) {
		bool wasquiet = world.history->quiet;
		world.history->quiet = true; // not a new birth
		c->born();
		world.history->quiet = wasquiet;
	}
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
	Agent* a = buildCreature(genome, s, revive_atp);
	if (!a)
		return;
	std::string mon = world.history->findMoniker(genome);
	if (!s.moniker.empty() && world.history->renameMoniker(mon, s.moniker))
		mon = s.moniker; // the same moniker as before, so parents and the log still find it
	monikerData& m = world.history->getMoniker(mon);
	m.name = s.name;
	m.mother = s.mother;
	m.father = s.father;
	m.registered = s.registered;
	m.photos = s.photos;
	m.notes = s.notes;
	if (s.hasborn) {
		m.borntick = s.born;
		m.bornknown = s.bornknown;
	} else {
		m.bornknown = false; // saved before birth times were kept: counts from now
	}
}

static void restoreEgg(const SavedEgg& e) {
	std::string script = fmt::format("new: simp {} {} {} {} 0\n", e.sprite, e.frames, e.firstimg, e.plane);
	script += fmt::format("pose {}\n", e.pose);
	script += fmt::format("setv cls2 {} {} {}\nsetv attr {}\n", e.family, e.genus, e.species, e.attr);
	for (auto& v : e.vars)
		script += v + "\n";
	if (e.timer)
		script += fmt::format("tick {}\n", e.timer);
	script += fmt::format("mvto {} {}\n", e.x, e.y);
	if (!e.kept)
		script += "setv grav 1\n";
	std::string err = engine.executeNetwork(script);
	if (!err.empty())
		fmt::print("* Couldn't restore an egg: {}\n", err);
}

bool LoadWorldState(const fs::path& worlddir) {
	world.injectedCobs.clear();
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
	std::vector<lifeLogEntry> lifelog;
	std::vector<SavedCreature> records;
	std::map<int, unsigned int> namecount;
	bool namesgiven = false;
	int camx = -1, camy = -1;
	bool relays_saved = false;
	SavedCreature* c = nullptr;
	SavedEgg* e = nullptr;
	std::vector<SavedInjected> injected;
	std::vector<std::string> injectedCobs;
	SavedInjected* io = nullptr;
	while (std::getline(in, line)) {
		std::istringstream l(line);
		std::string key;
		l >> key;
		if (key == "chemical-relays") {
			relays_saved = true;
		} else if (key == "camera") {
			l >> camx >> camy;
		} else if (key == "calendar") {
			unsigned int wt, tod, dos, sea, yr;
			if (l >> wt >> tod >> dos >> sea >> yr) {
				world.worldtickcount = wt;
				world.timeofday = tod % 5;
				world.dayofseason = dos % 4;
				world.season = sea % 4;
				world.year = yr;
			}
		} else if (key == "life") {
			lifeLogEntry le;
			int female = 0, ageknown = 1, oldage = 0;
			if (l >> le.kind >> le.tick >> le.genus >> female >> le.stage >> le.age >> ageknown >> oldage >> le.moniker) {
				le.female = female != 0;
				le.ageknown = ageknown != 0;
				le.oldage = oldage != 0;
				le.name = unescapeLine(restOfLine(l));
				lifelog.push_back(le);
			}
		} else if (key == "names") {
			int genus;
			unsigned int n;
			if (l >> genus >> n)
				namecount[genus] = n;
		} else if (key == "names-given") {
			l >> namesgiven;
		} else if (key == "injected" && !c && !e && !io) {
			injectedCobs.push_back(unescapeLine(restOfLine(l)));
		} else if (key == "creature" || key == "record") {
			auto& list = key == "creature" ? creatures : records;
			list.emplace_back();
			c = &list.back();
			c->record = key == "record";
			e = nullptr;
			io = nullptr;
		} else if (key == "egg" || key == "object") {
			eggs.emplace_back();
			e = &eggs.back();
			e->kept = key == "object";
			c = nullptr;
			io = nullptr;
		} else if (key == "iobject") {
			injected.emplace_back();
			io = &injected.back();
			c = nullptr;
			e = nullptr;
		} else if (key == "end") {
			c = nullptr;
			e = nullptr;
			io = nullptr;
		} else if (io) {
			readInjectedLine(*io, key, l, [&] { return unescapeLine(restOfLine(l)); });
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
			else if (key == "moniker")
				l >> c->moniker;
			else if (key == "parents") {
				l >> c->mother >> c->father;
				if (c->mother == "-")
					c->mother.clear();
				if (c->father == "-")
					c->father.clear();
			} else if (key == "genus")
				l >> c->genus >> c->gender;
			else if (key == "died")
				l >> c->diedtick >> c->diedstage;
			else if (key == "name")
				c->name = unescapeLine(restOfLine(l));
			else if (key == "registered")
				l >> c->registered;
			else if (key == "born") {
				l >> c->born >> c->bornknown;
				c->hasborn = true;
			} else if (key == "photo")
				c->photos.push_back(unescapeLine(restOfLine(l)));
			else if (key == "notes")
				c->notes = unescapeLine(restOfLine(l));
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
			else if (key == "frames")
				l >> e->frames;
			else if (key == "plane")
				l >> e->plane;
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
		if (a && (oldCreatureOf(a.get()) || isEgg(a.get()) || isKept(a.get()) || isInjected(a.get())))
			remove.push_back(a);
	for (auto& a : remove)
		a->kill();

	world.history->lifelog = lifelog;
	world.history->notices.clear();
	world.history->namecount = namecount;
	world.history->namesgiven = namesgiven;
	for (auto& s : creatures) {
		try {
			restoreCreature(savedir, s, !relays_saved);
		} catch (std::exception& ex) {
			fmt::print("* Couldn't restore a creature ({}): {}\n", s.genome, ex.what());
		}
	}
	for (auto& r : records) {
		if (r.moniker.empty() || world.history->hasMoniker(r.moniker))
			continue;
		monikerData& m = world.history->addRecord(r.moniker, (unsigned char)r.genus, r.gender);
		m.name = r.name;
		m.registered = r.registered;
		m.photos = r.photos;
		m.notes = r.notes;
		m.borntick = r.born;
		m.bornknown = r.bornknown;
		m.died = true;
		m.diedtick = r.diedtick;
		m.diedstage = r.diedstage;
		m.mother = r.mother;
		m.father = r.father;
	}
	for (auto& s : eggs)
		restoreEgg(s);
	// objects added by hand from the Agent Injector: their scripts, then
	// the objects (none of a COB that is no longer there)
	std::set<std::string> missing;
	for (auto& name : injectedCobs) {
		std::string err;
		if (CobManager::reinstallScripts(name, err)) {
			world.injectedCobs.push_back(name);
		} else {
			missing.insert(name);
			fmt::print("* Injected object \"{}\" not restored: {}\n", name, err);
		}
	}
	injected.erase(std::remove_if(injected.begin(), injected.end(), [&](const SavedInjected& o) { return missing.count(o.from) > 0; }), injected.end());
	restoreInjected(injected);
	if (camx >= 0 && camy >= 0)
		engine.camera->moveToGlobal(camx, camy, jump);
	int nkept = (int)std::count_if(eggs.begin(), eggs.end(), [](const SavedEgg& e) { return e.kept; });
	fmt::print("* Restored saved world: {} creatures, {} eggs, {} kept objects, {} injected objects ({} add-ons)\n", creatures.size(), (int)eggs.size() - nkept, nkept,
		injected.size(), world.injectedCobs.size());
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

