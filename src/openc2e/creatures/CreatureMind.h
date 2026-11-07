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
};

struct MindOutput {
	int attentionCategory = -1; // -1 = no change
	int decision = -1; // -1 = no change; otherwise run creature event 16 + decision
	int sleep = -1; // -1 = no change, 0 = wake, 1 = fall asleep
	int expression = -1; // -1 = no change; else facial expression slot 0-5 in the head sprites
};

class CreatureMind {
  public:
	virtual ~CreatureMind() = default;
	virtual std::string name() const = 0;
	virtual void born(const MindPerception&) {}
	virtual MindOutput tick(const MindPerception&) = 0;
	virtual void died(const MindPerception&) {}
	virtual std::string debugState() const { return {}; }
};

// Returns nullptr for an empty or unknown name.
std::unique_ptr<CreatureMind> createCreatureMind(const std::string& name);

// The names createCreatureMind() accepts, for --help and error messages.
std::string availableCreatureMinds();
