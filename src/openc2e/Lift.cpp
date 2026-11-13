/*
 *  Lift.cpp
 *  openc2e
 *
 *  Created by Alyssa Milburn on Sat Dec 2 2006.
 *  Copyright (c) 2006 Alyssa Milburn. All rights reserved.
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

#include "Lift.h"

#include <cmath>
#include <fmt/core.h>
#include <string>

#include "Engine.h" // version
#include "World.h"
#include "caosValue.h"

Lift::Lift(std::string spritefile, unsigned int firstimage, unsigned int imagecount)
	: Vehicle(spritefile, firstimage, imagecount) {
	if (engine.version == 1) {
		alignwithcabin = true;
	} else {
		alignwithcabin = false;
	}
}

/*
 * TODO: this code is a first attempt and might be completely wrong
 */

bool Lift::fireScript(unsigned short event, Agent* from, caosValue one, caosValue two) {
	if (event == 1 || event == 2) {
		if (!liftAvailable())
			return false; // TODO: hack to make sure the lifts aren't activated when not ready
	}

	// select the next call button in the requested direction (1 down, 2 up).
	// Not only for the hand: a creature activating the lift must choose a
	// destination too, or the lift stops at once on the floor it is already
	// on and drops its passengers (creatures could never ride). The lift's own
	// activations (from tick(), answering a call button) already chose one.
	if (from != this) {
		// Standing still, the lift is at the stop nearest its cage, whatever
		// stop it last thought it was at. They disagreed after a world loaded
		// (the cage where the save or the world file left it), so a request
		// was refused ("no stop further up"), or the stop check in tick()
		// fired at once and snapped the cage, with its riders, to a far stop:
		// a rider on the west cage lift jumped 417 px (2026-11-12).
		if (engine.version == 2 && yvec == 0 && newbutton == currentbutton && !callbuttony.empty()) {
			unsigned int nearest = 0;
			for (unsigned int i = 1; i < callbuttony.size(); i++)
				if (std::fabs(callbuttony[i] - liftBottom()) < std::fabs(callbuttony[nearest] - liftBottom()))
					nearest = i;
			currentbutton = newbutton = nearest;
		}
		if (event == 1) {
			if (currentbutton + 1 == callbuttony.size())
				return false;
			currentbutton++;
			newbutton = currentbutton;
		} else if (event == 2) {
			if (currentbutton == 0)
				return false;
			currentbutton--;
			newbutton = currentbutton;
		}
	}

	return Agent::fireScript(event, from, one, two);
}

void Lift::tick() {
	Vehicle::tick();
	if (paused)
		return;

	// if we're moving..
	if (yvec != 0) {
		// are we beyond the call button y point?
		if (
			(yvec < 0 && liftBottom() <= callbuttony[currentbutton]) || // upwards
			(yvec > 0 && liftBottom() >= callbuttony[currentbutton]) // downwards
		) {
			// stop movement (and make sure we're in the right spot)
			yvec = 0;
			moveTo(x, callbuttony[currentbutton] - (alignwithcabin ? cabinbottom : getHeight()));

			// send deactivate event
			queueScript(0);
		}
	}

	if (liftAvailable() && newbutton != currentbutton) {
		currentbutton = newbutton;

		if (liftBottom() < callbuttony[currentbutton])
			queueScript(1, this);
		else
			queueScript(2, this);
	}
}

float Lift::liftBottom() {
	if (alignwithcabin)
		return y + cabinbottom;
	else
		return y + getHeight();
}

bool Lift::liftAvailable() const {
	return var[0].getInt() == 0;
}

/* vim: set noet: */
