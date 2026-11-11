/*
 *  CreatureMind.h
 *  openc2e
 *
 *  A narrow, pluggable interface for C1/C2 creature decision-making.
 *
 *  Each brain tick the engine fills a MindPerception (what the creature
 *  sees, its drives and chemistry, stimuli received since the last tick)
 *  and asks the mind for a MindOutput (what to attend to, which action to
 *  take). The world's own creature scripts (event 16 + decision) carry out
 *  the action. Minds never touch engine internals directly.
 *
 *  Select a mind with the --mind command-line option. Without it, the
 *  original oldBrain code path is used unchanged.
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

#include "AgentRef.h"
#include "lifestage.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

struct SeenAgent {
	int category; // world category index (0-39 for C1/C2)
	AgentRef agent;
	float distance; // between centres, in world pixels
	float x = 0, y = 0; // the agent's centre, in world pixels
	float edible = 0; // scent: how nourishing its eat script is, 0-1 (see edibleScent)
	bool reachable = true; // overlaps the creature's height, so walking can reach it
	bool activatable = true; // its attributes let it be activated (C1/C2 attr 4)
};

// An instinct from the genome, translated into neutral terms (see
// gene-translation.md). For C2: "when these conditions hold, doing this action
// is rewarded (or punished), this strongly".
struct InnateCondition {
	enum Kind { Drive, Attention, HeardVerb, HeardNoun, Sense } kind;
	int index; // drive number, object category, word number or sense number
};
struct InnateInstinct {
	std::vector<InnateCondition> conditions;
	int decision = 0;
	bool reward = true; // false: punishment
	float strength = 0; // 0-1
	std::string signature; // identifies the gene, so it is applied once
};

// A stimulus sent to the creature by a world script. Two forms exist in
// C1/C2 scripts: a numbered stimulus (STM# WRIT) and a full description
// (STIM WRIT / STIM FROM). The engine passes them to the mind unchanged
// and applies nothing itself; what a stimulus does is the mind's business.
struct StimulusEvent {
	bool numbered = false; // true: only 'id' is meaningful
	unsigned int id = 0; // numbered form: stimulus number
	int significance = 0, input = 0, intensity = 0, features = 0; // full form
	std::array<int, 4> chem{}; // full form: chemical numbers (0 = none)
	std::array<int, 4> amount{}; // full form: amounts for each chemical
	AgentRef source; // the agent whose script sent it, if known
	std::string command; // e.g. "STM# WRIT", for logging
	// What the body actually received (either form, after the creature's
	// stimulus genes and sleep): chemical numbers and amounts, 0 = none.
	std::array<int, 4> feltChem{};
	std::array<int, 4> feltAmount{};
};

// An action the world's creature scripts make available (event 16 + decision).
struct MindAction {
	int decision;
	bool needsObject; // the script acts on the attended object (_it_, appr, aim:)
};

struct MindPerception {
	int creatureId = 0; // the creature agent's UNID, for logging
	unsigned int tick = 0; // creature age in brain ticks
	lifestage stage = baby;
	bool asleep = false;
	bool dreaming = false;
	bool female = false;
	std::array<unsigned char, 17> drives{}; // C2 has 17, C1 uses the first 16
	const unsigned char* chemicals = nullptr; // 256 levels, read-only
	std::vector<SeenAgent> visible; // at most one agent per category
	std::vector<StimulusEvent> stimuli; // received since the last mind tick
	bool actionFinished = true; // the creature's current action script has stopped
	const std::vector<MindAction>* actions = nullptr; // read from the world's scripts
	int currentAttention = -1; // category index, or -1
	int currentDecision = -1; // decision index, or -1
	float x = 0, y = 0; // the creature's centre, in world pixels
	bool riding = false; // carried by a vehicle (a lift, a boat)
	// food smell (C2, World::tickFoodScent): in this room, and the strongest
	// through the doors on each side
	float scentHere = 0, scentWest = 0, scentEast = 0, scentUp = 0, scentDown = 0;
	// C2: urges from smelled scents (pheromones.txt; creatures/Pheromones.h):
	// follow a scent up its gradient or move away from it. drive >= 0: worth
	// relieving that drive by `weight`; -1: a fixed want of `weight`.
	struct ScentUrge {
		std::string scent;
		bool avoid = false;
		int drive = -1;
		float weight = 1.0f;
		float here = 0, west = 0, east = 0, up = 0, down = 0;
	};
	std::vector<ScentUrge> scentUrges;
	// water (C2 room type 2): whether the creature is in it, and which way the
	// nearest dry room lies through open doors (10 west, 11 east, -1 up,
	// 1 down, 0 none found) and how many rooms away
	bool inWater = false;
	float breath = 1.0f; // breath left while under water (1 full, 0 none: suffocating)
	bool waterWest = false, waterEast = false; // the room through the door on that side is water (or a fall lands in water)
	float dropWest = 0, dropEast = 0; // how far the floor drops stepping through that side (0: no drop)
	// a wall right ahead on that side: the foot is at the room's edge and no
	// doorway it fits through goes on at walking height (nor a drop)
	bool wallWest = false, wallEast = false;
	int dryWay = 0, dryRooms = 0;
	std::vector<InnateInstinct> newInstincts; // genome instincts expressed since the last tick
};

struct MindOutput {
	int attentionCategory = -1; // -1 = no change
	int decision = -1; // -1 = no change; otherwise run creature event 16 + decision
	int sleep = -1; // -1 = no change, 0 = wake, 1 = fall asleep
	int expression = -1; // -1 = no change; else facial expression slot 0-5 in the head sprites
	// An object whose use keeps doing nothing for the creature: for a while,
	// notice others of its kind first (the engine notices one object per
	// category; without this a creature in the airlock only ever saw the sea
	// door, never the land door that lets it out).
	AgentRef passOver;
	// Something to say (a meaning, worded by the engine for the creature's
	// age: CreatureSpeech.h), or empty.
	std::string say;
};

class CreatureMind {
  public:
	virtual ~CreatureMind() = default;
	virtual std::string name() const = 0;
	virtual void born(const MindPerception&) {}
	virtual MindOutput tick(const MindPerception&) = 0;
	virtual void died(const MindPerception&) {}
	virtual std::string debugState() const { return {}; }
	// World saving: the mind's learned state as text, and restoring it.
	virtual std::string saveState() const { return {}; }
	virtual void loadState(const std::string&) {}
};

// Returns nullptr for an empty or unknown name.
std::unique_ptr<CreatureMind> createCreatureMind(const std::string& name);

// The names createCreatureMind() accepts, for --help and error messages.
std::string availableCreatureMinds();
