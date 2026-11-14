/*
 *  Agent.cpp
 *  openc2e
 *
 *  Created by Alyssa Milburn on Tue May 25 2004.
 *  Copyright (c) 2004 Alyssa Milburn. All rights reserved.
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

#include "Profile.h"
#include "Agent.h"
#ifndef _WIN32
#include <dlfcn.h>
#include <execinfo.h>
#endif
#include <cstring>

#include "AgentHelpers.h"
#include "Camera.h"
#include "Engine.h"
#include "Map.h"
#include "MetaRoom.h"
#include "PathResolver.h"
#include "Room.h"
#include "Scriptorium.h"
#include "SoundManager.h"
#include "SpritePart.h"
#include "Vehicle.h"
#include "VoiceData.h"
#include "World.h"
#include "caosVM.h"
#include "caosScript.h"
#include "common/audio/AudioBackend.h"
#include "common/creaturesImage.h"
#include "common/io/FileReader.h"
#include "common/throw_ifnot.h"

#include <array>
#include <cassert>
#include <tuple>
#include <fmt/core.h>
#include <memory>

#ifndef M_PI
#define M_PI 3.14159265358979323846 /* pi */
#endif

bool caosValueCompare::operator()(const caosValue& v1, const caosValue& v2) const {
	if (v1.getType() == v2.getType())
		return v1 < v2;
	else
		return v1.getType() < v2.getType();
}

Agent::Agent(unsigned char family_, unsigned char genus_, unsigned short species_, unsigned int zorder_)
	: timerrate(0), vm(0), attr(0), visible(true) {
	initialized = false;
	lifecount = 0;

	setClassifier(family_, genus_, species_);

	lastScript = -1;

	x = 0;
	y = 0; // note that c2e agents are moved in finishInit
	zorder = zorder_;

	has_custom_core_size = false;

	velx = 0.0f;
	vely = 0.0f;

	avel = 0.0f;
	fvel = 0.0f;
	svel = 0.0f;
	admp = 0.0f;
	fdmp = 0.0f;
	sdmp = 0.0f;
	spin = 0.0f;

	spritesperrotation = 0;
	numberrotations = 0;

	wasmoved = false;

	if (engine.version == 2) {
		accg = 10;
		aero = 20;
		rest = 40;

		size = 127; // TODO: correct default?
		thrt = 0;
		falling = false; // TODO: it looks like grav should be 0, but make sure!
	} else if (engine.version > 2) {
		accg = 0.3f;
		aero = 0;
		elas = 0;

		perm = 50; // TODO: correct default?
		falling = true;
	}

	moved_last_tick = false;

	range = 500;

	// TODO: is this the correct default?
	clac[0] = 0; // message# for activate 1
	if (engine.version < 3) {
		// TODO: is this the correct default? (this is equivalent to bhvr click == 0)
		clac[0] = -1;
		clac[1] = -1;
		clac[2] = -1;
	}
	clik = -1;

	dying = false;
	unid = -1;

	paused = displaycore = false;

	cr_can_push = cr_can_pull = cr_can_stop = cr_can_hit = cr_can_eat = cr_can_pickup = false; // TODO: check this
	imsk_key_down = imsk_key_up = imsk_mouse_move = imsk_mouse_down = imsk_mouse_up = imsk_mouse_wheel = imsk_translated_char = false;

	emitca_index = -1;
	emitca_amount = 0.0f;

	objp = {}; // not strictly necessary
}

void Agent::finishInit() {
	// lc2e, at least, seems to position agents centered on (-9876,-9876) to begin with
	// TODO: where should we place agents in other games? is this code right at all anyway?
	// (bear in mind that there are no parts present for some C1/C2 agents when finishInit is called, atm)
	if (engine.version > 2 && !engine.bmprenderer) { // TODO: need to think about bmp-specific code some more
		x = -9876.0f + (getWidth() / 2.0f);
		y = -9876.0f + (getHeight() / 2.0f);
	}

	// shared_from_this() can only be used if these is at least one extant
	// std::shared_ptr which owns this
	world.agents.push_front(std::shared_ptr<Agent>(this));
	agents_iter = world.agents.begin();

	if (engine.version > 2 && findScript(10))
		queueScript(10); // constructor

	if (!voice && engine.version == 3) {
		setVoice("DefaultVoice");
	}

	initialized = true;
}

void Agent::zotstack() {
	// Zap the VM stack.
	for (auto& i : vmstack) {
		world.freeVM(i);
	}
	vmstack.clear();
}

// the C++ call stack, as "module+offset" frames for addr2line (names where the
// dynamic symbol table has them), skipping the logger's own frames
static std::string engineStack() {
#ifndef _WIN32
	void* frames[20];
	int n = backtrace(frames, 20);
	std::string out;
	for (int i = 2; i < n; i++) {
		Dl_info info;
		if (dladdr(frames[i], &info) && info.dli_fname) {
			const char* mod = strrchr(info.dli_fname, '/');
			out += fmt::format(" {}+{:#x}", mod ? mod + 1 : info.dli_fname, (uintptr_t)frames[i] - (uintptr_t)info.dli_fbase);
		} else
			out += fmt::format(" {}", frames[i]);
	}
	return out;
#else
	return {};
#endif
}

void Agent::logBigMove(float nx, float ny) {
	std::string by = "the engine";
	if (g_runningVM && g_runningVM->currentscript) {
		auto& sc = g_runningVM->currentscript;
		Agent* o = g_runningVM->owner.get();
		by = fmt::format("script {} {} {} {}{}", sc->fmly, sc->gnus, sc->spcs, sc->scrp,
			o ? fmt::format(" of {} {} {} at {:.0f},{:.0f}", (int)o->family, (int)o->genus, o->species, o->x, o->y) : std::string(" (no owner: CAOS from outside)"));
	} else if (carriedby)
		by = fmt::format("its carrier {} {} {}", (int)carriedby->family, (int)carriedby->genus, carriedby->species);
	fmt::print("* BIG MOVE {} {} {} unid {} from {:.0f},{:.0f} to {:.0f},{:.0f} by {}{}\n", (int)family, (int)genus, species, unid, x, y, nx, ny, by,
		invehicle ? fmt::format(" (in vehicle {} {} {})", (int)invehicle->family, (int)invehicle->genus, invehicle->species) : std::string());
	// which engine code moved it (2026-11-13: a grendel at the world's seam
	// was moved 489 px up by no script and not by the pocket safety net):
	// resolve with addr2line -f -C -e build/openc2e OFFSET
	if (by == "the engine")
		fmt::print("* BIG MOVE stack:{}\n", engineStack());
}

void Agent::moveTo(float _x, float _y, bool force) {
	// Move ourselves to the specified location.
	wasmoved = true;

	// if we're being carried and aren't being forced to move (prbly by our carrier), forget it
	if (carriedby && !force)
		return;

	// TODO: what if we move while being carried? doomy explosions ensue, that's what!
	float xoffset = _x - x;
	float yoffset = _y - y;

	// C2: a creature moved far in one go is logged with what moved it (a
	// norn went from the land at 5856,557 to the east seabed in 18 ticks and
	// nothing in the logs said how; owner, 2026-11-12)
	if (engine.version == 2 && engine.worldready && (std::fabs(xoffset) > 300 || std::fabs(yoffset) > 300) && std::fabs(xoffset) < 4000 && family == 4)
		logBigMove(_x, _y);

	x = _x;
	y = _y;

	// handle wraparound
	// TODO: this is perhaps non-ideal
	if (engine.version < 3 && xoffset != 0.0f) {
		// TODO: it'd be nice to handle multiple metarooms
		MetaRoom* m = world.map->getFallbackMetaroom();
		assert(m);

		// (at or past the far edge counts: an object whose corner sat exactly
		// on x = width never wrapped, found no room, and fell out of the world)
		if (x < m->x()) {
			x += m->width();
		} else if (x >= m->x() + m->width()) {
			x -= m->width();
		}
	}

	for (auto& i : floated) {
		assert(i);
		i->moveTo(i->x + xoffset, i->y + yoffset);
	}

	adjustCarried(xoffset, yoffset);
}

void Agent::floatTo(AgentRef a) {
	std::vector<AgentRef>::iterator i = std::find(floated.begin(), floated.end(), a);
	THROW_IFNOT(i == floated.end()); // loops are bad, mmkay

	if (floatable())
		floatRelease();
	floatingagent = a;
	if (floatable())
		floatSetup();
}

void Agent::floatTo(float x, float y) {
	if (floatingagent) {
		moveTo(floatingagent->x + x, floatingagent->y + y);
	} else {
		moveTo(engine.camera->getX() + x, engine.camera->getY() + y);
	}
}

void Agent::floatSetup() {
	if (floatingagent)
		floatingagent->addFloated(this);
	else
		engine.camera->addFloated(this);
}

void Agent::floatRelease() {
	if (floatingagent) {
		floatingagent->delFloated(this);
	} else
		engine.camera->delFloated(this);
}

void Agent::addFloated(AgentRef a) {
	assert(a);
	assert(a != floatingagent); // loops are bad, mmkay
	floated.push_back(a);
}

void Agent::delFloated(AgentRef a) {
	assert(a);
	std::vector<AgentRef>::iterator i = std::find(floated.begin(), floated.end(), a);
	//if (i == floated.end()) return;
	assert(i != floated.end());
	floated.erase(i);
}

std::shared_ptr<script> Agent::findScript(unsigned short event) {
	return world.scriptorium->getScript(family, genus, species, event);
}

#include "PointerAgent.h"
#include "creatures/Creature.h"
#include "creatures/CreatureAgent.h"
bool Agent::fireScript(unsigned short event, Agent* from, caosValue one, caosValue two) {
	static const bool watcheat = getenv("OPENC2E_WATCH_CREATURES") != nullptr;
	if (watcheat && event == 12 && engine.version == 2 && family == 2)
		fmt::print("* WATCH EATEN {} {} {} by {}\n", (int)family, (int)genus, species, from ? from->getUNID() : -1);
	// Start running the specified script on the VM of this agent, with FROM set to the provided agent.

	if (dying)
		return false;

	CreatureAgent* c = 0;
	// The cr_can_* creature permissions come from BHVR, which only exists in
	// C3/DS. C1/C2 objects have no such permissions, so do not enforce them
	// there (otherwise C1/C2 creatures can never push, pull, hit, eat or pick
	// anything up).
	if (engine.version >= 3 && (event <= 3 || event == 4 || event == 12 || event == 13 || event == 14))
		c = dynamic_cast<CreatureAgent*>(from);

	switch (event) {
		case 0: // deactivate
			if (c && !cr_can_stop)
				return false;
			// TODO: not sure if this is the right place to do this.
			actv = event;
			break;
		case 1: // activate 1
			if (c && !cr_can_push)
				return false;
			// TODO: not sure if this is the right place to do this.
			actv = event;
			break;
		case 2: // activate 2
			if (c && !cr_can_pull)
				return false;
			// TODO: not sure if this is the right place to do this.
			actv = event;
			break;
		case 3: // hit
			if (c && !cr_can_hit)
				return false;
			break;
		case 4: // pickup
			if (c && !cr_can_pickup)
				return false;
			if (!from)
				return false;
			if (from == world.hand()) {
				if (!mouseable())
					return false;
			} else if (!c) {
				// TODO: valid check for vehicles?
				if (!carryable())
					return false;
			}
			from->addCarried(this); // TODO: correct behaviour?
			break;
		case 5: // drop
			if (!from)
				return false;
			// TODO: this check isn't very good for vehicles ;p
			// if (from != carriedby) return false;
			from->dropCarried(this); // TODO: correct?
			break;
		case 12: // eat
			if (c && !cr_can_eat)
				return false;
			break;
		case 13: // hold hands with pointer
			if (c) {
				// TODO
			}
			break;
		case 14: // stop holding hands with pointer
			if (c) {
				// TODO
			}
			break;
		case 92: // TODO: hack for 'UI Mouse Down' event - we need a real event system!
			fmt::print("faking event 92 on {}\n", identify());
			;
			CompoundPart* p = world.partAt(world.hand()->pointerX(), world.hand()->pointerY());
			if (!p || p->getParent() != this) // if something is horridly broken here, return
				return false; // was THROW_IFNOT(p && p->getParent() == this);
			p->handleClick(world.hand()->pointerX() - p->x - p->getParent()->x, world.hand()->pointerY() - p->y - p->getParent()->y);
			// TODO: we're [obviously] missing firing the pointer script here, but it's a hack for now
			break;
	}

	bool ranscript = false;
	static const bool trace_events = getenv("OPENC2E_TRACE_EVENTS") != nullptr;

	std::shared_ptr<script> s;
	// C2: a creature's action on another creature runs the world's
	// creature-on-creature script (classifier 0 0 0: activate 1 a kiss and,
	// between a willing grown pair, mating; activate 2 a tickle; deactivate a
	// push; hit a hit) rather than its own script for acting on an object,
	// which only sent the other creature a message it has no script for
	// (found 2026-11-10, starting on breeding)
	if (engine.version == 2 && family == 4 && event >= 16 && event < 32)
		if (CreatureAgent* ca = dynamic_cast<CreatureAgent*>(this))
			if (Creature* c = ca->getCreature()) {
				AgentRef focus = c->getAttentionFocus();
				if (focus && focus.get() != this && dynamic_cast<CreatureAgent*>(focus.get()))
					s = world.scriptorium->getScript(0, 0, 0, event);
			}
	if (!s)
		s = findScript(event);
	if (s) {
		bool madevm = false;
		if (!vm) {
			madevm = true;
			vm = world.getVM(this);
		}

		bool fired = vm->fireScript(s, event == 9, from);
		// developer aid: OPENC2E_TIMER_STATS=N prints, N ticks into the session, each
		// kind's timer rate and how many of its timer events ran or were
		// skipped because the last run had not finished
		if (event == 9) {
			static const char* ts = getenv("OPENC2E_TIMER_STATS");
			if (ts) {
				static std::map<std::tuple<int, int, int>, std::array<long, 3>> stats;
				auto& st = stats[{family, genus, species}];
				st[fired ? 0 : 1]++;
				st[2] = timerrate;
				static bool printed = false;
				if (!printed && world.tickcount >= (unsigned int)atoi(ts)) {
					printed = true;
					for (auto& [k, v] : stats)
						fmt::print("* TIMER {} {} {} rate {} ran {} skipped {}\n", std::get<0>(k), std::get<1>(k), std::get<2>(k), v[2], v[0], v[1]);
				}
			}
		}
		if (fired) {
			lastScript = event;
			zotstack();
			vm->setVariables(one, two);

			// TODO: we should set _it_ in a more sensible way
			CreatureAgent* a = dynamic_cast<CreatureAgent*>(this);
			if (a) {
				Creature* c = a->getCreature();
				assert(c);
				vm->_it_ = c->getAttentionFocus();
			}

			vmTick();
			ranscript = true;
		} else if (madevm) {
			world.freeVM(vm);
			vm = 0;
		}
	}

	switch (event) {
		case 5:
			if (invehicle)
				break;
			if (engine.version > 1)
				break;

			// Creatures 1 drop snapping
			// TODO: this probably doesn't belong here, but it has to be run after the
			// drop script starts (see for instance C1 carrots, which change pose)
			MetaRoom* m = world.map->metaRoomAt(x, y);
			if (!m)
				break;
			std::shared_ptr<Room> r = m->nextFloorFromPoint(x, y);
			if (!r)
				break;
			moveTo(x, r->bottom.pointAtX(x).y - getHeight());

			break;
	}

	// (creatures' own events too with OPENC2E_TRACE_CREATURE_EVENTS)
	static const bool trace_creatures = getenv("OPENC2E_TRACE_CREATURE_EVENTS") != nullptr;
	if (((trace_events && family != 4) || (trace_creatures && family == 4)) && event != 9)
		fmt::print("[event] {} {} {} unid {} event {} from {} -> {}\n", (int)family, (int)genus, species, unid, event,
			from ? fmt::format("{} {} {}", (int)from->family, (int)from->genus, from->species) : std::string("none"),
			ranscript ? "ran" : (findScript(event) ? "refused or failed" : "no script"));
	return ranscript;
}

bool Agent::queueScript(unsigned short event) {
	return queueScript(event, {});
}

bool Agent::queueScript(unsigned short event, AgentRef from) {
	return queueScript(event, from, {});
}

bool Agent::queueScript(unsigned short event, AgentRef from, caosValue p0) {
	return queueScript(event, from, p0, {});
}

bool Agent::queueScript(unsigned short event, AgentRef from, caosValue p0, caosValue p1) {
	// Queue a script for execution on the VM of this agent.

	if (dying)
		return false;

	// only bother firing the event if either it exists, or it's one with engine code attached
	// TODO: why don't we do the engine checks/etc here?
	switch (event) {
		default:
			if (!findScript(event))
				return false;

		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 12:
		case 13:
		case 14:
		case 92:
			world.queueScript(event, this, from, p0, p1);
	}

	return true;
}

int Agent::handleClick(float clickx, float clicky) {
	// Handle a mouse click.
	if (!activateable())
		return -1;
	// TODO: do something with clickx and clicky?
	(void)clickx;
	(void)clicky;

	// old-style click handling (c1/c2)
	if (engine.version < 3) {
		int action = -1;

		// look up the relevant action for our ACTV state from the clac table
		if ((unsigned int)actv < 3)
			action = clac[(unsigned int)actv];

		if (action != -1) {
			return calculateScriptId(action);
		}

		return -1;
	}

	// new-style click handling (c2e)
	if (clik != -1) {
		return -1; // TODO: handle CLIK
	} else if (clac[0] != -1) {
		return calculateScriptId(clac[0]);
	}

	return -1;
}

void Agent::playAudio(std::string filename, bool controlled, bool loop) {
	assert(!dying);

	sound.stop();
	sound = soundmanager.playSound(filename, loop);
	if (!sound) {
		unhandledException(fmt::format("Couldn't play sound '{}'", filename), false);
	}
	updateAudio(sound);
	if (!controlled) {
		sound = {};
	}
}

bool agentOnCamera(Agent* targ, bool checkall = false); // caosVM_camera.cpp

void Agent::updateAudio(Sound s) {
	s.setPosition(x, y, getWidth(), getHeight());
	// TODO: setVelocity?
}

Point const Agent::boundingBoxPoint(unsigned int n) {
	if (has_custom_core_size)
		return boundingBoxPoint(n, Point(x + custom_core_xleft, y + custom_core_ytop), custom_core_xright - custom_core_xleft, custom_core_ybottom - custom_core_ytop);
	else
		return boundingBoxPoint(n, Point(x, y), getWidth(), getHeight());
}

Point const Agent::boundingBoxPoint(unsigned int n, Point in, float w, float h) {
	Point p;

	switch (n) {
		case 0: // left
			p.x = in.x;
			p.y = in.y + (h / 2.0f);
			break;

		case 1: // right
			p.x = in.x + w;
			p.y = in.y + (h / 2.0f);
			break;

		case 2: // top
			p.x = in.x + (w / 2.0f);
			p.y = in.y;
			break;

		case 3: // bottom
			p.x = in.x + (w / 2.0f);
			p.y = in.y + h;
			break;

		default:
			throw Exception("Agent::boundingBoxPoint got unknown direction");
	}

	return p;
}

bool Agent::validInRoomSystem() {
	// Return true if this agent is inside the world room system at present, or false if it isn't.

	// TODO: c1
	if (engine.version == 1)
		return true;

	if (has_custom_core_size)
		return validInRoomSystem(Point(x + custom_core_xleft, y + custom_core_ytop), custom_core_xright - custom_core_xleft, custom_core_ybottom - custom_core_ytop, perm);
	else
		return validInRoomSystem(Point(x, y), getWidth(), getHeight(), perm);
}

bool Agent::validInRoomSystem(Point p, float w, float h, int testperm) {
	// Return true if this agent is inside the world room system at the specified point, or false if it isn't.
	MetaRoom* m = world.map->metaRoomAt(p.x, p.y);
	if (!m)
		return false;

	for (unsigned int i = 0; i < 4; i++) {
		Point src, dest;
		switch (i) {
			case 0:
				src = boundingBoxPoint(0, p, w, h);
				dest = boundingBoxPoint(3, p, w, h);
				break; // left to bottom
			case 1:
				src = boundingBoxPoint(1, p, w, h);
				dest = boundingBoxPoint(3, p, w, h);
				break; // right to bottom
			case 2:
				src = boundingBoxPoint(2, p, w, h);
				dest = boundingBoxPoint(0, p, w, h);
				break; // top to left
			case 3:
				src = boundingBoxPoint(2, p, w, h);
				dest = boundingBoxPoint(1, p, w, h);
				break; // top to right
		}

		if (engine.version == 2) {
			// Creatures 2 physics

			int dx = dest.x - src.x;
			int dy = dest.y - src.y;

			Point deltapt(0, 0);
			double delta = 1000000000;
			bool collided = false;

			// TODO: check suffercollisions?

			// TODO: muh, provided direction here is kinda a hack
			findCollisionInDirection((i < 2) ? 3 : 2, m, src, dx, dy, deltapt, delta, collided, true);
			if (collided)
				return false;
		} else {
			// Creatures 3 physics

			float srcx = src.x, srcy = src.y;

			std::shared_ptr<Room> ourRoom = m->roomAt(srcx, srcy);
			if (!ourRoom)
				return false;

			unsigned int dir;
			Line wall;
			Point newsrc = src;
			bool collision = world.map->collideLineWithRoomSystem(src, dest, ourRoom, newsrc, wall, dir, testperm);

			if (collision && newsrc != dest)
				return false;
		}
	}

	return true;
}

void Agent::physicsTick() {
	if (engine.version == 1)
		return; // C1 has no physics, and different attributes.

	if (carriedby)
		return; // We don't move when carried, so what's the point?

	if (engine.version == 2) {
		// Creatures 2 physics is different.
		physicsTickC2();
		return;
	}

	if (!falling)
		return; // TODO: there are probably all sorts of issues here, untested

	if (!wasmoved)
		return; // some agents are created outside INST and get autokilled if we try physics on them before they move

	if (invehicle)
		return; // TODO: c2e verhicle physics

	// set destination point based on velocities
	float destx = x + velx;
	float desty = y + vely;

	if (rotatable()) {
		// TODO: the real engine seems to reset velx/vely, so i do that here, but why?
		velx = 0.0f;
		vely = 0.0f;

		// TODO: which order should these be in?

		// calculate forwards velocity
		float forward_x = fvel * sinf(spin * (M_PI * 2));
		float forward_y = fvel * -cosf(spin * (M_PI * 2));

		// calculate sideways velocity
		// TODO: this sideways velocity code is untested
		float sideways_x = svel * cosf(spin * (M_PI * 2));
		float sideways_y = svel * -sinf(spin * (M_PI * 2));

		// set destination based on forward/sideways velocity
		destx = x + forward_x + sideways_x;
		desty = y + forward_y + sideways_y;

		// modify spin based on angular velocity
		spin = fmodf(spin + avel, 1.0f);
		if (spin < 0.0f)
			spin += 1.0f;
	}

	if (sufferphysics()) {
		// TODO: falling behaviour needs looking at more closely..
		// .. but it shouldn't be 'false' by default on non-physics agents, so..
		//falling = false;
		// increase speed according to accg
		// TODO: should we be changing vely first, instead of after a successful move (below)?
		desty += accg;
	}

	if (suffercollisions()) {
		float lastdistance = 1000000.0f;
		bool collided = false;
		Line wall; // only valid when collided
		unsigned int collidedirection = 0; // only valid when collided
		Point bestmove;

		// iterate through all four points of the bounding box
		for (unsigned int i = 0; i < 4; i++) {
			// this mess is because we want to start with the bottom point - DOWN (3) - before the others, for efficiency
			Point src = boundingBoxPoint((i == 0) ? 3 : i - 1);

			// store values
			float srcx = src.x, srcy = src.y;

			std::shared_ptr<Room> ourRoom = world.map->roomAt(srcx, srcy);
			if (!ourRoom) {
				if (!outofroomsystem) {
					// we're out of the room system, physics bug, but let's try MVSFing back in to cover for fuzzie's poor programming skills
					static bool tryingmove;
					tryingmove = false; // avoid infinite loop
					if (!tryingmove && tryMoveToPlaceAround(x, y)) {
						//fmt::print("{} was out of room system due to a physics bug but we hopefully moved it back in..\n", identity());
						tryingmove = true;
						physicsTick();
						return;
					}

					// didn't work!
					unhandledException(fmt::format("{} out of room system at ({}, {})", identify(), srcx, srcy), false);
				}
				markOutOfRoomSystem();
				falling = false;
				return; // out of room system
			}

			Point dest(destx + (srcx - x), desty + (srcy - y));
			unsigned int local_collidedirection;
			Line local_wall;

			// this changes src to the point at which we end up
			bool local_collided = world.map->collideLineWithRoomSystem(src, dest, ourRoom, src, local_wall, local_collidedirection, perm);

			float dist;
			if (src.x == srcx && src.y == srcy)
				dist = 0.0f;
			else {
				float xdiff = src.x - srcx;
				float ydiff = src.y - srcy;
				dist = xdiff * xdiff + ydiff * ydiff;
			}

			if (dist >= lastdistance) {
				assert(i != 0); // this had better not be our first collision!
				continue; // further away than a previous collision
			}

			lastdistance = dist;
			bestmove.x = x + (src.x - srcx);
			bestmove.y = y + (src.y - srcy);
			collidedirection = local_collidedirection;
			wall = local_wall;
			collided = local_collided;

			if (dist == 0.0f)
				break; // no point checking any more, is there?
		}

		// *** do actual movement
		if (lastdistance != 0.0f) {
			moveTo(bestmove.x, bestmove.y);

			if (collided) {
				lastcollidedirection = collidedirection;
				queueScript(6, 0, velx, vely); // TODO: include this? .. we need to include SOMETHING, c3 ball checks for <3

				if (elas != 0) {
					if (wall.getType() == HORIZONTAL) {
						vely = -vely;
					} else if (wall.getType() == VERTICAL) {
						velx = -velx;
					} else {
						// line starts always have a lower x value than the end
						float xdiff = wall.getEnd().x - wall.getStart().x;
						float ydiff = wall.getEnd().y - wall.getStart().y;
						float fvelx = velx, fvely = vely;

						// calculate input/slope angles
						double inputangle;
						if (fvelx == 0.0f) {
							if (fvely > 0.0f)
								inputangle = M_PI / 2.0;
							else
								inputangle = 3 * (M_PI / 2.0);
						} else {
							inputangle = atan(fvely / fvelx);
						}
						double slopeangle = atan(-ydiff / xdiff); // xdiff != 0 because wall isn't vertical

						// calculate output angle
						double outputangle = slopeangle + (slopeangle - inputangle) + M_PI;

						// turn back into component velocities
						double vectorlength = sqrt(fvelx * fvelx + fvely * fvely);
						float xoutput = cos(outputangle) * vectorlength;
						float youtput = sin(outputangle) * vectorlength;

						velx = xoutput;
						vely = -youtput;
					}

					if (elas != 100.0f) {
						velx = velx * (elas / 100.0f);
						vely = vely * (elas / 100.0f);
					}
				} else
					vely = 0;
			} else if (sufferphysics() && accg != 0) {
				vely = vely + accg;
			}
		} else {
			// TODO: correct?
			if (sufferphysics()) {
				if (velx == 0.0f && vely == 0.0f)
					falling = false;
			}
			velx = 0;
			vely = 0;
		}
	} else {
		moveTo(destx, desty);
		if (sufferphysics())
			vely = vely + accg;
	}

	if (sufferphysics() && (aero != 0)) {
		// reduce speed according to AERO
		// TODO: aero should be an integer!
		velx = velx - (velx * (aero / 100.0f));
		vely = vely - (vely * (aero / 100.0f));
	}

	if (rotatable()) {
		avel -= avel * admp;
		fvel -= fvel * fdmp;
		svel -= svel * sdmp;
	}
}

std::shared_ptr<Room> const Agent::bestRoomAt(unsigned int tryx, unsigned int tryy, unsigned int direction, MetaRoom* m, std::shared_ptr<Room> exclude) {
	std::vector<std::shared_ptr<Room> > rooms = m->roomsAt(tryx, tryy);

	std::shared_ptr<Room> r;

	if (rooms.size() == 0)
		return std::shared_ptr<Room>();
	if (rooms.size() == 1)
		r = rooms[0];
	else if (rooms.size() > 1) {
		unsigned int j;
		for (j = 0; j < rooms.size(); j++) {
			if (rooms[j] == exclude)
				continue;

			if (direction == 0) { // left
				if (rooms[j]->x_left == tryx)
					break;
			} else if (direction == 1) { // right
				if (rooms[j]->x_right == tryx)
					break;
			} else if (direction == 2) { // top
				if (rooms[j]->y_left_ceiling == tryy)
					break;
			} else { // down
				if (rooms[j]->y_left_floor == tryy)
					break;
			}
		}
		if (j == rooms.size())
			j = 0;
		r = rooms[j];
	}

	if (r == exclude)
		return std::shared_ptr<Room>();
	else
		return r;
}

// Creatures 2 collision finding
void Agent::findCollisionInDirection(unsigned int i, class MetaRoom* m, Point src, int& dx, int& dy, Point& deltapt, double& delta, bool& collided, bool followrooms) {
	src.x = (int)src.x;
	src.y = (int)src.y;

	if (m->wraparound()) {
		if (src.x > m->x() + m->width() || (dx > 0 && src.x == m->x() + m->width()))
			src.x -= m->width();
		else if (src.x < m->x() || (dx < 0 && src.x == m->x()))
			src.x += m->width();
	}

	// TODO: caching rooms affects behaviour - work out if that's a problem
	std::shared_ptr<Room> room = roomcache[i].lock();
	if (!room || !room->containsPoint(src.x, src.y)) {
		room = bestRoomAt(src.x, src.y, i, m, std::shared_ptr<Room>());
		roomcache[i] = room;
	}

	if (!room) { // out of room system
		if (!outofroomsystem)
			unhandledException(fmt::format("{} out of room system at ({}, {})", identify(), src.x, src.y), false);
		falling = false;
		markOutOfRoomSystem();
		return;
	}

	int lastdirection = 0;

	bool steep = abs(dy) > abs(dx);

	int signdx = dx < 0 ? -1 : 1;
	int signdy = dy < 0 ? -1 : 1;

	Line l(Point(0, 0), Point(dx, dy));

	Point lastpoint(0, 0);

	Vehicle* vehicle = 0;
	if (invehicle)
		vehicle = dynamic_cast<Vehicle*>(invehicle.get());

	for (int loc = 0; loc <= abs(steep ? dy : dx); loc++) {
		Point p = steep ? l.pointAtY(loc * signdy) : l.pointAtX(loc * signdx);
		p.x = (int)p.x;
		p.y = (int)p.y;

		if (vehicle) {
			if (src.x + p.x < vehicle->x + vehicle->cabinleft) {
				lastdirection = 0;
				collided = true;
				break;
			}
			if (src.x + p.x > vehicle->x + vehicle->cabinright) {
				lastdirection = 1;
				collided = true;
				break;
			}
			if (src.y + p.y < vehicle->y + vehicle->cabintop) {
				lastdirection = 2;
				collided = true;
				break;
			}
			if (src.y + p.y > vehicle->y + vehicle->cabinbottom) {
				lastdirection = 3;
				collided = true;
				break;
			}

			lastpoint = p;

			continue;
		}

		bool trycollisions = false;

		bool endofroom = false;

		if (src.x + p.x < room->x_left) {
			if (i != 1 && dx < 0) {
				trycollisions = true;
				lastdirection = 0;
			}
			endofroom = true;
		}
		if (src.x + p.x > room->x_right) {
			if (i != 0 && dx > 0) {
				trycollisions = true;
				lastdirection = 1;
			}
			endofroom = true;
		}
		if (src.y + p.y < room->y_left_ceiling) {
			if (i != 3 && dy < 0) {
				trycollisions = true;
				lastdirection = 2;
			}
			endofroom = true;
		}
		if (src.y + p.y > room->y_left_floor) {
			if (i != 2 && dy > 0) {
				trycollisions = true;
				lastdirection = 3;
			}
			endofroom = true;
		}

		// find the next room, if necessary, and work out whether we should move into it or break out
		if (endofroom) {
			if (m->wraparound()) {
				if (dx > 0 && src.x + p.x >= m->x() + m->width())
					src.x -= m->width();
				else if (dx < 0 && src.x + p.x <= m->x())
					src.x += m->width();
			}

			std::shared_ptr<Room> newroom = bestRoomAt(src.x + p.x, src.y + p.y, i, m, room);

			bool collision = false;

			// collide if we're out of the room system
			if (!newroom) {
				collision = true;
			}
			// collide if there's no new room connected to this one
			else if (!world.map->hasDoor(room, newroom)) {
				collision = true;
			}
			// collide if the PERM between this room and the new room is smaller than or equal to our size
			else if (size > world.map->getDoorPerm(room, newroom)) {
				collision = true;
			}

			if (collision && (trycollisions || (!newroom))) {
				collided = true;
				break;
			}

			// move to the new room and keep going!
			room = newroom;
		}

		if (room->floorpoints.size() && i == 3 && dy >= 0 && size > room->floorvalue) { // TODO: Hack!
			// TODO: we don't check floorYatX isn't returning a 'real' room floor, but floorpoints should cover the whole floor anyway
			int floory = room->floorYatX(src.x + p.x);

			// never collide when the top point of an object is below the floor.
			Point top = boundingBoxPoint(2);

			// TODO: consider steep floors
			if (src.y + p.y > floory && top.y < floory) {
				collided = true;
				lastdirection = 3;
				break;
			}
		}

		if ((!followrooms) && endofroom)
			break;

		lastpoint = p;
	}

	double length2 = (lastpoint.x * lastpoint.x) + (lastpoint.y * lastpoint.y);
	if (length2 < delta) {
		// TODO: !followrooms is a horrible way to detect a non-physics call
		if (collided && followrooms)
			lastcollidedirection = lastdirection;
		deltapt = lastpoint;
		delta = length2;
	}
}

// An object found outside the room map (its physics stops until it is back
// in). The core box is drawn for it only on request (OPENC2E_SHOW_CORES, or
// the CAOS debug command): it used to be drawn for good, a magenta diamond
// round the object that players saw (owner, 2026-11-11).
void Agent::markOutOfRoomSystem() {
	outofroomsystem = true;
	static const bool show = getenv("OPENC2E_SHOW_CORES") != nullptr;
	if (show)
		displaycore = true;
}

void Agent::physicsTickC2() {
	int dx = round(velx), dy = round(vely);

	if (dx != 0 || dy != 0)
		falling = true;

	if (falling && sufferphysics()) {
		dy += round(accg);
	}

	falling = true;

	if (dy == 0 && dx == 0) { // nothing to do
		if (round(vely) == 0) {
			// really no motion
			falling = false;
		} else {
			// y motion cancelled by gravity
			vely = dy;
		}
		return;
	}
	vely = dy;

	// An object under its room's floor line (the SSFC surface) never meets
	// that line, since the floor check below only catches an object crossing
	// it from above; it rests on the room's rectangular bottom instead, out
	// of sight inside the ground (the world file starts some critters there).
	// Lift a moving object with gravity onto the line, where the ground is
	// solid (floor value 0). Where the floor value is above 0 the room is
	// meant to be used beneath its line (a lift shaft whose bottom is the
	// lift's lower stop, as creatures walk it): lifting there popped a norn
	// that stumbled at the desert lift's foot up to the desert (owner,
	// 2026-11-09).
	if (suffercollisions() && sufferphysics()) {
		float cx = x + getWidth() / 2.0f, bottom = y + getHeight();
		std::shared_ptr<Room> r = world.map->roomAt(cx, bottom - 1);
		// (not in water: a water room's floor line is a bed for creatures to
		// wade along, and near the banks it rises to the surface; fish
		// swimming there were set on it, out of the water, and stuck at the
		// pond's edge by the waterfall; owner, 2026-11-10)
		// In water only what lies wholly under the bed is lifted (a ball that
		// sank into the bog's painted earth; owner, 2026-11-11): a fish
		// swimming near a bank never is.
		bool buriedInWater = r && r->type == 2 && r->floorpoints.size() && r->floorvalue == 0 && y > r->floorYatX(cx) + 1;
		if (r && (r->type != 2 || buriedInWater) && r->floorpoints.size() && r->floorvalue == 0 && size > r->floorvalue) {
			float floory = r->floorYatX(cx);
			// Not where water lies under the room: there the line is a
			// boardwalk over it, not the top of the ground, and what is under
			// the line came up out of the water (fish leaping at the surface
			// were set on the boardwalk over the sea by the airlock, room 14
			// over room 123; owner, 2026-11-10)
			float span = std::max(1.0f, (float)r->x_right - (float)r->x_left);
			float roombottom = r->y_left_floor + ((float)r->y_right_floor - (float)r->y_left_floor) * (cx - r->x_left) / span;
			std::shared_ptr<Room> under = world.map->roomAt(cx, roombottom + 1);
			bool overWater = under && under != r && under->type == 2;
			if (!overWater && bottom > floory + 1 && floory > r->y_left_ceiling)
				moveTo(x, floory - getHeight());
		}
	}

	Point deltapt(0, 0);
	double delta = 1000000000;

	bool collided = false;

	// back inside the room map: no longer out of it
	if (outofroomsystem && validInRoomSystem())
		outofroomsystem = false;

	if (suffercollisions()) {
		MetaRoom* m = world.map->metaRoomAt(x, y);
		// C2: the top of the map is a ceiling. The east seabed's launcher
		// throws a creature so hard that its top went above y 0, out of every
		// room ("out of room system") until it fell back (2026-11-12): it
		// bumps the top and falls instead
		if (!m && engine.version == 2 && y < 0) {
			if (MetaRoom* top = world.map->metaRoomAt(x, 0)) {
				moveTo(x, (float)top->y());
				if (vely < 0)
					vely = 0;
				m = top;
			}
		}
		// and the bottom of the map is a floor: the bottom pool's floor (room
		// 43, y 2399) is the map's edge, and a walking pumpkin (Halloween
		// pack, 2 6 3) that sank to it slipped 2 px below, out of every room,
		// and stuck there raising errors (2026-11-13)
		if (m && engine.version == 2) {
			float bottom = (float)(m->y() + m->height() - 1);
			if (y + getHeight() > bottom) {
				moveTo(x, bottom - getHeight());
				if (vely > 0)
					vely = 0;
			}
		}
		if (!m) {
			if (!outofroomsystem)
				unhandledException(fmt::format("{} out of room system at ({}, {})", identify(), x, y), false);
			falling = false;
			markOutOfRoomSystem();
			return;
		}

		for (unsigned int i = 0; i < 4; i++) {
			Point src = boundingBoxPoint(i);
			findCollisionInDirection(i, m, src, dx, dy, deltapt, delta, collided, true);
		}
	} else {
		deltapt.x = dx;
		deltapt.y = dy;
	}

	if (collided && (round(velx) != 0 || round(vely) != 0) && moved_last_tick) {
		if (lastcollidedirection >= 2) // up and down
			vely = -(round(vely) - (rest * round(vely)) / 100);
		else
			velx = -(round(velx) - (rest * round(velx)) / 100);
		queueScript(6, 0);
	}
	if ((int)deltapt.x == 0 && (int)deltapt.y == 0) {
		if (!moved_last_tick) {
			falling = false;
			velx = 0;
			vely = 0;
		}
		moved_last_tick = false;
	} else {
		moved_last_tick = true;
		moveTo(x + (int)deltapt.x, y + (int)deltapt.y);
		if (sufferphysics()) {
			int fricx = (aero * round(velx)) / 100;
			int fricy = (aero * round(vely)) / 100;
			if (abs(round(velx)) > 0 && fricx == 0)
				fricx = (round(velx) < 0) ? -1 : 1;
			if (abs(round(vely)) > 0 && fricy == 0)
				fricy = (round(vely) < 0) ? -1 : 1;
			velx = round(velx) - fricx;
			vely = round(vely) - fricy;
		}
	}
}

void Agent::tick() {
	// sanity checks to stop ticks on dead agents
	LifeAssert la(this);
	if (dying)
		return;

	// reposition audio
	if (sound) {
		updateAudio(sound);
	}

	// don't tick paused agents
	if (paused)
		return;

	// CA updates
	if (emitca_index != -1 && emitca_amount != 0.0f) {
		assert(0 <= emitca_index && emitca_index <= 19);
		std::shared_ptr<Room> r = world.map->roomAt(x, y);
		if (r) {
			r->catemp[emitca_index] += emitca_amount;
			/*if (r->catemp[emitca_index] <= 0.0f) r->catemp[emitca_index] = 0.0f;
			else if (r->catemp[emitca_index] >= 1.0f) r->catemp[emitca_index] = 1.0f;*/
		}
	}

	// TODO: this might be in the wrong place - note that parts are ticked *before* Agent::tick is called
	if (rotatable() && spritesperrotation != 0 && numberrotations > 1) {
		SpritePart* p = dynamic_cast<SpritePart*>(part(0));
		if (p) {
			// change base according to our SPIN and the ROTN settings
			unsigned int rotation = 0;
			rotation = (spin - (0.5f / numberrotations)) * numberrotations; // TODO: verify correctness
			p->setBase(spritesperrotation * rotation);
		}
	}

	// tick the physics engine
	physicsTick();
	if (dying)
		return; // in case we were autokilled

	// update the timer if needed, and then queue a timer event if necessary
	if (timerrate) {
		tickssincelasttimer++;
		if (tickssincelasttimer == timerrate) {
			queueScript(9); // TODO: include this?
			tickssincelasttimer = 0;
		}
		assert(timerrate > tickssincelasttimer);
	}

	// tick the agent VM
	if (vm)
		vmTick();

	// some silly hack to handle delayed voices
	tickVoices();
}

void Agent::unhandledException(std::string info, bool wasscript) {
	// TODO: do something with this? empty the stack?
	if (world.autostop) {
		// autostop is mostly for Creatures Village, which is full of buggy scripts
		stopScript();
	} else if (world.autokill && !dynamic_cast<CreatureAgent*>(this)) { // don't autokill creatures! TODO: someday we probably should :)
		kill();
		if (wasscript)
			fmt::print("{} was autokilled during script {} due to: {}\n", identify(), lastScript, info);
		else
			fmt::print("{} was autokilled due to: {}\n", identify(), info);
	} else {
		stopScript();
		if (wasscript)
			fmt::print("{} caused an exception during script {}: {}\n", identify(), lastScript, info);
		else
			fmt::print("{} caused an exception: {}\n", identify(), info);
	}
}

void Agent::vmTick() {
	// Tick the VM associated with this agent.
	PROFILE_SCOPE(family == 4 ? "scripts: creatures" : "scripts: objects");

	assert(vm); // There must *be* a VM to tick.
	LifeAssert la(this); // sanity check

	// If we're out of timeslice, give ourselves some more (depending on the game type).
	if (!vm->timeslice) {
		// 1 is known to be correct for C1. C2 inherited that value without evidence;
		// at 1, C2 scripts that change several parts in a row (e.g. the incubator's
		// two door halves, "part 0 anim [...] part 1 anim [...]") start them on
		// different ticks and the parts drift visibly out of step. 5 (as in c2e)
		// keeps such groups together. OPENC2E_C2_TIMESLICE overrides it for testing.
		static const int c2_timeslice = [] {
			const char* s = getenv("OPENC2E_C2_TIMESLICE");
			return (s && atoi(s) > 0) ? atoi(s) : 5;
		}();
		vm->timeslice = (engine.version == 1) ? 1 : (engine.version == 2) ? c2_timeslice : 5;
	}

	// Keep trying to run VMs while we don't run out of timeslice, end up with a blocked VM, or a VM stops.
	while (vm && vm->timeslice && !vm->isBlocking() && !vm->stopped()) {
		assert(vm->timeslice > 0);

		// Tell the VM to tick (using all available timeslice), catching exceptions as necessary.
		try {
			vm->tick();
		} catch (invalidAgentException& e) {
			// try letting the exception script handle it
			if (!queueScript(255))
				unhandledException(e.prettyPrint(), true);
			else
				stopScript(); // we still want current script to die
		} catch (Exception& e) {
			unhandledException(e.prettyPrint(), true);
		}
		// If the VM stopped, it's done.
		if (vm && vm->stopped()) {
			world.freeVM(vm);
			vm = NULL;
		}
	}

	// Zot any remaining timeslice, since we're done now.
	if (vm)
		vm->timeslice = 0;

	// If there's no current VM but there's one on the call stack, a previous VM must have finished,
	// pop one from the call stack to run next time.
	if (!vm && !vmstack.empty()) {
		vm = vmstack.front();
		vmstack.pop_front();
	}
}

Agent::~Agent() {
	assert(lifecount == 0);

	if (!initialized)
		return;
	if (!dying) {
		// we can't do kill() here because we can't do anything which might try using our std::shared_ptr
		// (since this could be during world destruction)

		if (vm) {
			vm->stop();
			world.freeVM(vm);
			vm = 0;
		}

		zotstack();

		if (sound) {
			sound.stop();
			sound = {};
		}
	}
}

void Agent::kill() {
	assert(!dying);
	if (floatable())
		floatRelease();
	if (carrying)
		dropCarried(carrying);
	if (carriedby)
		carriedby->drop(this);
	// TODO: should the carried agent really be responsible for dropping from vehicle?
	if (invehicle)
		invehicle->drop(this);

	dying = true; // what a world, what a world...

	if (vm) {
		vm->stop();
		world.freeVM(vm);
		vm = 0;
	}

	if (sound) {
		sound.stop();
		sound = {};
	}

	// these may cause the agent to be destroyed, if they have the only reference
	zotstack();
	agents_iter->reset();
}

unsigned int Agent::getZOrder() const {
	if (invehicle) {
		// TODO: take notice of cabp in c2e, at least. also, stacking .. ?
		Vehicle* v = dynamic_cast<Vehicle*>(invehicle.get());
		assert(v);
		return v->getZOrder() + v->cabinplane;
		// TODO: Vehicle should probably rearrange zorder of passengers if ever moved
	} else if (carriedby) {
		// TODO: check for overflow
		// TODO: is adding our own zorder here correct behaviour? someone should check
		if (engine.version > 1)
			return carriedby->carriesInFront() ? carriedby->getZOrder() + 1 : carriedby->getZOrder() - 100;
		else
			return carriedby->getZOrder();
	} else {
		return zorder;
	}
}

void Agent::setZOrder(unsigned int z) {
	if (dying)
		return;
	zorder = z;
}

int Agent::getUNID() const {
	if (unid != -1)
		return unid;
	return unid = world.newUNID(const_cast<Agent*>(this));
}

#include "Catalogue.h"

std::string Agent::identify() const {
	std::string buf = fmt::format("{} {} {}", (int)family, (int)genus, species);
	const std::string n = catalogue.getAgentName(family, genus, species);
	if (n.size()) {
		buf += " (" + n + ")";
	}
	/*if (unid != -1) {
		buf += " unid ";
		buf += to_string(unid);
	} else {
		buf += " (no unid assigned)";
	} */
	return buf;
}

void Agent::pushVM(caosVM* newvm) {
	assert(newvm);
	if (vm)
		vmstack.push_front(vm);
	vm = newvm;
}

bool Agent::vmStopped() {
	return (!vm || vm->stopped());
}

void Agent::stopScript() {
	zotstack();
	if (vm)
		vm->stop();
}

void Agent::addCarried(AgentRef a) {
	assert(a);

	// TODO: muh, vehicle drop needs more thought
	if (a->invehicle) {
		Vehicle* v = dynamic_cast<Vehicle*>(a->invehicle.get());
		assert(v);
		v->dropCarried(a);
	}

	carry(a);

	// TODO: this doesn't reorder children or anything..
	a->setZOrder(a->zorder);

	// fire 'Got Carried Agent'
	if (engine.version >= 3)
		queueScript(124, a); // TODO: is this the correct param?
}

void Agent::carry(AgentRef a) {
	assert(a);

	// TODO: check for infinite loops (eg, us carrying an agent which is carrying us) :)

	if (carrying)
		dropCarried(carrying);

	carrying = a;

	a->carriedby = AgentRef(this);
	// TODO: move carrying agent to the right position
	adjustCarried(0, 0);
}

bool Agent::beDropped() {
	AgentRef wascarriedby = carriedby;
	carriedby = AgentRef(0);
	bool wasinvehicle = invehicle;
	invehicle = AgentRef(0);

	// TODO: this doesn't reorder children or anything..
	setZOrder(zorder);

	// TODO: no idea if this is right, it tries to re-enable gravity when dropping agents
	falling = true;

	/* if our carrying agent was in a vehicle, we stay in the vehicle -- unless it
	   is the vehicle itself dropping us (we were in it), or the vehicle would just
	   take us straight back and Vehicle::drop's check fails (an abort) */
	if (wascarriedby && wascarriedby->invehicle && !wasinvehicle) {
		wascarriedby->invehicle->addCarried(this);
		// TODO: how to handle not-invehicle case, where vehicle has failed to pick us up?
		if (invehicle)
			return true;
	}

	if (!wasinvehicle) { // ie, we're not being dropped by a vehicle
		// TODO: check for vehicles in a saner manner?
		for (auto& agent : world.agents) {
			std::shared_ptr<Agent> a = agent;
			if (!a)
				continue;
			Vehicle* v = dynamic_cast<Vehicle*>(a.get());
			if (!v)
				continue;

			/* check whether our *centre* lies inside the vehicle cabin */
			int xpt = x + getWidth() / 2;
			if (xpt >= v->x + v->cabinleft && xpt <= v->x + v->cabinright) {
				int ypt = y + getHeight() / 2;
				if (ypt >= v->y + v->cabintop && ypt <= v->y + v->cabinbottom) {
					v->addCarried(this);
					// TODO: how to handle not-invehicle case, where vehicle has failed to pick us up?
					if (invehicle)
						return true;
				}
			}
		}
	}

	if (engine.version > 1) {
		// TODO: maybe think about this some more - does this belong here?
		tryMoveToPlaceAround(x, y);
	}

	// TODO: return value is not used anywhere yet?
	return true;
}

void Agent::dropCarried(AgentRef a) {
	drop(a);

	// TODO: this doesn't reorder children or anything..
	a->setZOrder(a->zorder);

	// fire 'Lost Carried Agent'
	if (engine.version >= 3)
		queueScript(125, a); // TODO: is this the correct param?
}

void Agent::drop(AgentRef a) {
	if (!carrying)
		return;
	assert(carrying == a);

	a->beDropped();
	carrying = AgentRef(0);
}

std::pair<int, int> Agent::getCarryPoint() {
	unsigned int ourpose = 0;

	SpritePart* s;
	if ((s = dynamic_cast<SpritePart*>(part(0))))
		ourpose = s->getBase() + s->getPose();

	std::pair<int, int> pos(0, 0);

	std::map<unsigned int, std::pair<int, int> >::iterator i = carry_points.find(ourpose);
	if (i != carry_points.end())
		pos = i->second;

	return pos;
}

std::pair<int, int> Agent::getCarriedPoint() {
	unsigned int theirpose = 0;

	SpritePart* s;
	if ((s = dynamic_cast<SpritePart*>(part(0))))
		theirpose = s->getBase() + s->getPose();

	std::pair<int, int> pos(0, 0);

	std::map<unsigned int, std::pair<int, int> >::iterator i = carried_points.find(theirpose);
	if (i != carried_points.end()) {
		pos = i->second;
	} else if (s && engine.version == 2 && family == 2 && genus == 5) {
		// C2: an egg is held by the top of the egg in its picture (its
		// topmost painted pixel), not by the corner of the picture's empty
		// box above it (owner, 2026-11-12), by the hand and by creatures
		static std::map<std::pair<const creaturesImage*, unsigned int>, std::pair<int, int>> s_tops;
		auto key = std::make_pair((const creaturesImage*)s->getSprite().get(), s->getCurrentSprite());
		auto t = s_tops.find(key);
		if (t == s_tops.end()) {
			std::pair<int, int> top(0, 0);
			unsigned int w = s->getWidth(), h = s->getHeight();
			bool found = false;
			for (unsigned int yy = 0; yy < h && !found; yy++) {
				unsigned int first = w, last = 0;
				for (unsigned int xx = 0; xx < w; xx++)
					if (!s->transparentAt(xx, yy)) {
						first = std::min(first, xx);
						last = xx;
					}
				if (first < w) {
					top = {(int)(first + last) / 2, (int)yy};
					found = true;
				}
			}
			t = s_tops.emplace(key, top).first;
		}
		pos = t->second;
	} else if (s && engine.version > 2) {
		// TODO: why fudge pickup location here and not in default carried points or something?
		// (this is nornagon's code which i moved - fuzzie)
		pos.first = s->getSprite()->width(s->getCurrentSprite()) / 2;
	}

	return pos;
}

void Agent::adjustCarried(float unusedxoffset, float unusedyoffset) {
	// Adjust the position of the agent we're carrying.
	// TODO: this doesn't actually position the carried agent correctly, sigh
	(void)unusedxoffset;
	(void)unusedyoffset;

	if (!carrying)
		return;

	int xoffset = 0, yoffset = 0;
	if (engine.version < 3 && world.hand() == this) {
		// this appears to produce correct behaviour in the respective games, don't ask me  -nornagon
		if (engine.version == 2)
			yoffset = world.hand()->getHeight() / 2 - 2;
		else
			yoffset = world.hand()->getHeight() / 2 - 3;
	}

	std::pair<int, int> carrypoint = getCarryPoint();
	xoffset += carrypoint.first;
	yoffset += carrypoint.second;

	std::pair<int, int> carriedpoint = carrying->getCarriedPoint();
	xoffset -= carriedpoint.first;
	yoffset -= carriedpoint.second;

	if (xoffset != 0 || yoffset != 0)
		carrying->moveTo(x + xoffset, y + yoffset, true);
}

void Agent::setClassifier(unsigned char f, unsigned char g, unsigned short s) {
	family = f;
	genus = g;
	species = s;

	category = world.findCategory(family, genus, species);
}

bool Agent::tryMoveToPlaceAround(float x, float y) {
	if (!suffercollisions()) {
		moveTo(x, y);
		return true;
	}

	// TODO: fix for has_custom_core_size case

	// second hacky attempt, move from side to side (+/- width) and up (- height) a little
	unsigned int trywidth = getWidth() * 2;
	if (trywidth < 100)
		trywidth = 100;
	unsigned int tryheight = getHeight() * 2;
	if (tryheight < 100)
		tryheight = 100;
	for (unsigned int xadjust = 0; xadjust < trywidth; xadjust++) {
		for (unsigned int yadjust = 0; yadjust < tryheight; yadjust++) {
			if (validInRoomSystem(Point(x - xadjust, y - yadjust), getWidth(), getHeight(), perm))
				moveTo(x - xadjust, y - yadjust);
			else if ((xadjust != 0) && validInRoomSystem(Point(x + xadjust, y - yadjust), getWidth(), getHeight(), perm))
				moveTo(x + xadjust, y - yadjust);
			else
				continue;
			return true;
		}
	}

	return false;
}

void Agent::join(unsigned int outid, AgentRef dest, unsigned int inid) {
	assert(dest);
	assert(outports.find(outid) != outports.end());
	assert(dest->inports.find(inid) != dest->inports.end());
	outports[outid]->dests.push_back(std::pair<AgentRef, unsigned int>(dest, inid));
	dest->inports[inid]->source = this;
	dest->inports[inid]->sourceid = outid;
}

void Agent::setVoice(std::string name) {
	if (engine.version < 3) {
		auto voicepath = findMainDirectoryFile(name + ".vce");
		if (voicepath.empty()) {
			throw Exception(fmt::format("can't find {}.vce", name));
		}
		FileReader f(voicepath);
		voice = std::shared_ptr<VoiceData>(new VoiceData(f));
	} else {
		voice = std::shared_ptr<VoiceData>(new VoiceData(name));
	}
}

void Agent::speak(std::string sentence, float pitch, float pace) {
	if (!voice)
		throw Exception("can't speak without voice");
	std::vector<VoiceEntry> syllables = voice->GetSyllablesFor(sentence);
	if (pending_voices.size() == 0) {
		ticks_until_next_voice = 1; // because tickVoices subtracts one every time
	}
	voice_pitch = pitch;
	for (const auto& entry : syllables) {
		// TODO: voice delay values are the same across games, even though C3/DS
		// run more ticks per second! Are C3/DS voices really "faster"?
		// (a slower or brisker voice for a creature's age: pace)
		unsigned int delay = std::max(1u, (unsigned int)std::lround(entry.delay_ticks / std::max(0.2f, pace)));
		pending_voices.push_back(std::pair<std::string, unsigned int>(entry.name, delay));
	}
}

void Agent::tickVoices() {
	if (pending_voices.size() == 0) {
		return;
	}

	ticks_until_next_voice--;
	if (ticks_until_next_voice == 0) {
		auto it = pending_voices.begin();
		if (it->first == "") {
			// just delay for number of ticks
		} else {
			// uncontrolled audio is easier, we shall hope no-one notices
			auto voice = soundmanager.playVoice(it->first, voice_pitch);
			if (!voice) {
				unhandledException(fmt::format("Couldn't play voice '{}'", it->first), false);
			}
			updateAudio(voice);
		}
		ticks_until_next_voice = it->second;
		pending_voices.erase(it);
	}
}

/* vim: set noet: */
