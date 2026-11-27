/*
 *  Pheromones.h
 *  openc2e
 *
 *  C2 under a mind: scents creatures and objects give off, carried by the
 *  room system (spreading through doorways, fading), smelled by creatures.
 *  What gives off which scent, and what smelling one does, is data: the user
 *  data folder's pheromones.txt (see the project's additions copy for
 *  the format and the rules for the game's breeds). Our own design.
 *
 *  A smelled scent can act on the body (primer: it adds a chemical, so it
 *  can feed the genome's own chemistry, such as the mating pheromones) or
 *  on behaviour (releaser: the mind is urged to follow the scent up its
 *  gradient, or to move away from it). A creature does not smell a scent it
 *  is giving off itself (adapted to its own odor), so a forager laying a
 *  trail is not drawn by it, while others are.
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

#pragma once

#include <array>
#include <string>
#include <vector>

class Agent;
class oldCreature;
class Room;

namespace Pheromones {

// One urge a scent gives a creature now (a releaser rule whose conditions
// hold): follow the scent up its gradient, or move away from it. drive >= 0:
// the urge is worth what relieving that drive (C2 drive number) by `weight`
// would be worth; -1: a fixed want of `weight` (the mind's units).
struct Urge {
	int channel = 0;
	bool avoid = false;
	int drive = -1;
	float weight = 1.0f;
	bool tend = false; // the source is tended (tickled, brought food or a toy)
	// "bring CATEGORY[,CATEGORY]": things of these kinds are picked up and
	// carried up the scent, and put down where it is at least dropAt
	std::vector<int> bring;
	float dropAt = 0.5f;
};

// scent laid at a place each step (Kinds.h: an ettin's hoard spot)
struct PlaceEmission {
	std::string scent;
	float x = 0, y = 0, strength = 1;
};
void setPlaceEmissions(const std::vector<PlaceEmission>& e);

// a named scent where the creature or object is (0-1; 0 if no such scent)
float levelAt(const std::string& scent, Agent* a);

// Reads pheromones.txt (at each world load). Without the file there are no
// scents and nothing here does anything.
void load();
int channelCount();
std::string channelName(int channel);

// A world tick (every few ticks it fades, spreads and lays scents, and the
// creatures in each room smell them: the chemical rules).
void tick();

// A creature felt a stimulus another agent sent (the chemicals its body
// received): its marking rules may mark that agent with a scent, which the
// agent then gives off wherever it goes for a while (harm: pain, injury or
// punishment; help: reward or a drive eased; or listed chemicals).
void felt(Agent* creature, oldCreature* c, Agent* source, const std::array<int, 4>& chems, const std::array<int, 4>& amounts);

// The behaviour rules that apply to this creature now, for the mind.
std::vector<Urge> urgesFor(Agent* creature, oldCreature* c);

// Signature scents (owner, 2026-11-14: "every creature needs to have its
// own scent", so one longing for a particular creature can follow it): every
// living creature lays its own scent, by moniker, which spreads and fades
// like the others. Its level in a room (0 where there is none).
float signatureIn(const ::Room& room, const std::string& moniker);

} // namespace Pheromones
