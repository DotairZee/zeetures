/*
 *  Relationships.h
 *  openc2e
 *
 *  C2 under a mind: what each creature feels about each other creature it
 *  has met (owner, 2026-11-11). Our own design, drawn from animal bonding
 *  research (pair bonds from time together and mating, grooming and food
 *  sharing as social currency, bad events weighing more than good ones) and
 *  kept per direction: A's feeling for B is not B's for A.
 *
 *  A bond grows when the other does something the creature feels as help
 *  (a reward, a drive eased: a kiss, a tickle, food given) and shrinks,
 *  three times as hard, when it feels harm (pain, injury, punishment), which
 *  also adds fear. Time spent together makes the other familiar, and liked
 *  when the creature was lonely. Recent feeling fades in minutes; the slow
 *  "affinity" (the meter) fades only after long apart, slowest for mates.
 *
 *  Bonds act back: a liked partner of the other sex raises sex drive and
 *  the chance of conceiving, two who dislike or fear each other do not
 *  conceive, friends nearby ease fear and stress, and a bonded mate away
 *  makes the creature lonely. Children raised together are less drawn to
 *  each other as mates later.
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
#include <map>
#include <string>

class Agent;
class CreatureAgent;
class oldCreature;

// One creature's feeling about another (oldCreature::bonds, by moniker).
struct Bond {
	float familiarity = 0; // 0-1: time spent together
	float recent = 0; // -1..1: how the last minutes went (fades fast)
	float affinity = 0; // -1..1: the meter (slow)
	float fear = 0; // 0-1
	unsigned int lastContact = 0; // world tick last together or dealt with
	unsigned int lastHarm = 0;
	bool friend_ = false, enemy = false; // labels (with a gap, so they do not flicker)
	bool mate = false; // they conceived together
	bool coReared = false; // together as children
	std::map<std::string, int> events; // what happened, for the map: "kissed me" 3
};

namespace Relationships {

// A creature felt a stimulus another agent sent: if that agent is a
// creature, the creature's bond to it moves.
void felt(Agent* creature, oldCreature* c, Agent* source, const std::array<int, 4>& chems, const std::array<int, 4>& amounts);

// A world tick: every few seconds, time together, fading, labels, and the
// bonds' effects on the body (buffering, missing a mate, attraction).
void tick();

// A pair conceived: each marks the other as its mate.
void conceived(CreatureAgent* mum, CreatureAgent* dad);

// For conception: 0 if either dislikes or fears the other, else 1 and up
// for a liked partner (lower for two raised together).
float mateFactor(CreatureAgent* a, CreatureAgent* b);

// "mate", "friend", "enemy", "feared", "fond", "wary", "known" or "".
std::string label(const Bond& b);

// The bond of `c` to the creature with moniker `other`, or null.
const Bond* bondOf(oldCreature* c, const std::string& other);

// World saving: a bond as one line's fields, and back.
std::string toLine(const std::string& other, const Bond& b);
bool fromLine(const std::string& fields, std::string& other, Bond& b);

} // namespace Relationships
