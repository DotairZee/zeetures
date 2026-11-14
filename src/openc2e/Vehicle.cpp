/*
 *  Vehicle.cpp
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

#include "Vehicle.h"
#include "creatures/SkeletalCreature.h"
#include <fmt/core.h>
#include <cstdlib>

#include "Engine.h"
#include "Map.h"
#include "Room.h"
#include "World.h"

#include <algorithm>
#include <cassert>

Vehicle::Vehicle(unsigned int family, unsigned int genus, unsigned int species, unsigned int plane,
	std::string spritefile, unsigned int firstimage, unsigned int imagecount)
	: CompoundAgent(family, genus, species, plane, spritefile, firstimage, imagecount) {
	capacity = 0;
	bump = 0;

	cabinleft = 0;
	cabintop = 0;
	cabinright = getWidth();
	cabinbottom = getHeight();
	cabinplane = 1; // TODO: is this sane? seems to be the default in c2e
}

Vehicle::Vehicle(std::string spritefile, unsigned int firstimage, unsigned int imagecount)
	: CompoundAgent(spritefile, firstimage, imagecount) {
	capacity = 0;
	bump = 0;

	// TODO: set cabin bounds? we don't know width/height at this point..
	cabinplane = 1;
}

void Vehicle::tick() {
	CompoundAgent::tick();
	if (paused)
		return;

	// move by xvec/yvec!
	moveTo(x + xvec / 256.0, y + yvec / 256.0);

	if (engine.version == 2)
		fitCreaturePassengers();
}

// C2: creatures riding stay inside the cabin as drawn (owner, 2026-11-13:
// "a lot of cases where I see creatures halfway hanging out of lifts"). A
// creature's box is a fixed 50 px wide from its x, but its picture can be
// far wider (a grendel in a cage lift stuck out past the cage's side), and
// it was only ever pushed in by the box, once, when it boarded. Its pose
// changes as it rides, and a ride does not move it to keep its feet down,
// so its feet also sank below the cabin's floor. Each tick: its picture is
// kept within the cabin's sides (centred when it is wider), and feet below
// the cabin's floor are raised to it.
void Vehicle::fitCreaturePassengers() {
	for (auto& p : passengers) {
		auto* sc = dynamic_cast<SkeletalCreature*>(p.get());
		if (!sc || sc->isDying())
			continue;
		float left = x + cabinleft, right = x + cabinright;
		float w = (float)sc->drawnWidth();
		float nx = sc->x;
		if (w >= right - left)
			nx = left + (right - left - w) / 2.0f;
		else if (nx + w > right)
			nx = right - w;
		else if (nx < left)
			nx = left;
		float ny = sc->y;
		float floor = y + cabinbottom;
		if (ny + sc->getHeight() > floor)
			ny = floor - sc->getHeight();
		if (nx != sc->x || ny != sc->y)
			sc->moveTo(nx, ny);
	}
}

void Vehicle::carry(AgentRef passenger) {
	// TODO: 'return' is not a good idea here, because the callung function already does stuff

	if (passenger->carriedby)
		return; // TODO: change to assert?
	if (passenger->invehicle)
		return; // TODO: change to assert?

	int cabinwidth = cabinright - cabinleft;
	int cabinheight = cabinbottom - cabintop;

	if (engine.version > 2) {
		// reject if passenger is too big
		if ((int)passenger->getWidth() > cabinwidth)
			return;
		if ((int)passenger->getHeight() > cabinheight)
			return;
	}

	static const bool watch = getenv("OPENC2E_WATCH_CREATURES") != nullptr;
	float beforex = passenger->x;
	// push into our cabin
	// TODO: should we use moveTo here?
	if (passenger->x + passenger->getWidth() > (x + cabinright))
		passenger->x = x + cabinright - passenger->getWidth();
	if (passenger->x < (x + cabinleft))
		passenger->x = x + cabinleft;
	if (engine.version > 1) {
		// TODO: not sure if this is good for too-high agents, if it's possible for them to exist (see comment above)
		if (passenger->y + passenger->getHeight() > (y + cabinbottom))
			passenger->y = y + cabinbottom - passenger->getHeight();
		if (passenger->y < (y + cabintop))
			passenger->y = y + cabintop;
	} else {
		passenger->y = y + cabinbottom - passenger->getHeight();
	}

	if (watch)
		fmt::print("* WATCH VEHICLE {} {} {} at ({:.0f}, {:.0f}) cabin {}..{} x {}..{} y: passenger {} {} {} x {:.0f} -> {:.0f}\n", (int)family, (int)genus, species, x, y,
			cabinleft, cabinright, cabintop, cabinbottom, (int)passenger->family, (int)passenger->genus, passenger->species, beforex, passenger->x);
	passengers.push_back(passenger);
	passenger->invehicle = this;

	if (engine.version >= 3)
		passenger->queueScript(121, this); // Vehicle Pickup, TODO: is this valid call?
}

void Vehicle::drop(AgentRef passenger) {
	std::vector<AgentRef>::iterator i = std::find(passengers.begin(), passengers.end(), passenger);
	assert(i != passengers.end());
	assert(passenger->invehicle == AgentRef(this));
	passengers.erase(i);

	passenger->beDropped();
	assert(passenger->invehicle != AgentRef(this));

	// A creature let out with its feet in water, where dry floor lies just
	// above, stands on that floor. C2's submarine lets its passengers out at
	// the east dock with their feet below its berth (room 146, floor 906), in
	// the sea under it, then throws them upward; the berth's doorway down to
	// the sea is too small for a creature to pass back up, so they sank to
	// the seabed instead of stepping onto the dock (owner, 2026-11-10).
	if (engine.version == 2 && passenger->family == 4 && world.map) {
		float fx = passenger->x + passenger->getWidth() / 2.0f;
		float fy = passenger->y + passenger->getHeight();
		std::shared_ptr<Room> here = world.map->roomAt(fx, fy);
		if (here && here->type == 2) {
			for (float y = fy - 1; y >= fy - 80; y--) {
				std::shared_ptr<Room> above = world.map->roomAt(fx, y);
				if (above && above != here && above->type != 2) {
					float floor = above->floorYatX(fx);
					if (floor > fy - 80 && floor <= fy)
						passenger->moveTo(passenger->x, floor - passenger->getHeight() - 1);
					break;
				}
			}
		}
	}

	if (engine.version >= 3)
		passenger->queueScript(122, this); // Vehicle Drop, TODO: is this valid call?
}

void Vehicle::adjustCarried(float xoffset, float yoffset) {
	Agent::adjustCarried(xoffset, yoffset);

	for (auto& passenger : passengers) {
		if (!passenger)
			continue; // TODO: muh
		passenger->moveTo(passenger->x + xoffset, passenger->y + yoffset);
	}
}

void Vehicle::kill() {
	// TODO: sane?
	while (passengers.size() > 0) {
		if (passengers[0])
			dropCarried(passengers[0]);
		else
			passengers.erase(passengers.begin());
	}

	Agent::kill();
}

/* vim: set noet: */
