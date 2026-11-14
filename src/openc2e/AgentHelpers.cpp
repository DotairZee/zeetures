/*
 *  AgentHelpers.cpp
 *  openc2e
 *
 *  Created by Alyssa Milburn on Sun Jul 23 2006.
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

#include "Agent.h"
#include "SpritePart.h"
#include <set>
#include "Engine.h"
#include "Map.h"
#include "MetaRoom.h"
#include "Room.h"
#include "World.h"
#include "AgentHelpers.h"
#include "caosScript.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fmt/core.h>
#include <map>
#include <sstream>
#include <string>

#include <cassert>
#include <memory>

bool agentIsVisible(Agent* seeing, Agent* a, float ownerx, float ownery, MetaRoom* ownermeta, std::shared_ptr<Room> ownerroom) {
	assert(ownermeta && ownerroom);

	if (seeing == a)
		return false;

	// out of range: no need to look up rooms (performance, 2026-11-13: the
	// room lookups below scan every room, and creatures ran this for every
	// object in the world each time they looked about)
	float thisx = a->x + (a->getWidth() / 2.0f);
	float thisy = a->y + (a->getHeight() / 2.0f);
	{
		double dx = thisx - ownerx, dy = thisy - ownery;
		if (dx * dx + dy * dy > (double)seeing->range * seeing->range)
			return false;
	}

	// verify we're in the same metaroom as owner, and in a room
	MetaRoom* m = world.map->metaRoomAt(thisx, thisy);
	if (m != ownermeta)
		return false;
	std::shared_ptr<Room> r = world.map->roomAt(thisx, thisy);
	if (!r)
		return false;

	// compare squared distance with range
	double deltax = thisx - ownerx;
	deltax *= deltax;
	double deltay = thisy - ownery;
	deltay *= deltay;
	if ((deltax + deltay) > (seeing->range * seeing->range))
		return false;

	// do the actual visibiltiy check using a line between centers
	Point src(ownerx, ownery), dest(thisx, thisy);
	Line dummywall;
	unsigned int dummydir;
	std::shared_ptr<Room> newroom = ownerroom;
	Point where = src;
	bool collision = world.map->collideLineWithRoomSystem(src, dest, newroom, where, dummywall, dummydir, seeing->perm);
	if (collision && where != dest)
		return false;

	return true;
}

// Whether any part of an object can be seen, not only its centre: tries the
// centre, then the nearest point of its bounding box. Creatures use this (a
// door object straddling a wall, such as the airlock's land door, centred on
// the far side, is still seen from inside).
bool agentPartlyVisible(Agent* seeing, Agent* dest) {
	return agentPartlyVisible(seeing, dest, seeing->x + (seeing->getWidth() / 2.0f), seeing->y + (seeing->getHeight() / 2.0f));
}

bool agentPartlyVisible(Agent* seeing, Agent* dest, float ownerx, float ownery) {
	// its nearest point out of range: neither test below can pass (checked
	// first: they look up rooms; performance, 2026-11-13)
	{
		float px = std::clamp(ownerx, dest->x, dest->x + std::max(0.0f, dest->getWidth() - 1.0f));
		float py = std::clamp(ownery, dest->y, dest->y + std::max(0.0f, dest->getHeight() - 1.0f));
		double dx = px - ownerx, dy = py - ownery;
		if (dx * dx + dy * dy > (double)seeing->range * seeing->range)
			return false;
	}
	MetaRoom* ownermeta = world.map->metaRoomAt(ownerx, ownery);
	std::shared_ptr<Room> ownerroom = world.map->roomAt(ownerx, ownery);
	if (!ownermeta || !ownerroom || seeing == dest)
		return false;
	if (agentIsVisible(seeing, dest, ownerx, ownery, ownermeta, ownerroom))
		return true;
	float nx = std::clamp(ownerx, dest->x, dest->x + dest->getWidth() - 1.0f);
	float ny = std::clamp(ownery, dest->y, dest->y + dest->getHeight() - 1.0f);
	if (world.map->metaRoomAt(nx, ny) != ownermeta || !world.map->roomAt(nx, ny))
		return false;
	double dx = nx - ownerx, dy = ny - ownery;
	if (dx * dx + dy * dy > (double)seeing->range * seeing->range)
		return false;
	Point src(ownerx, ownery), dst(nx, ny);
	Line dummywall;
	unsigned int dummydir;
	std::shared_ptr<Room> newroom = ownerroom;
	Point where = src;
	bool collision = world.map->collideLineWithRoomSystem(src, dst, newroom, where, dummywall, dummydir, seeing->perm);
	return !collision || where == dst;
}

bool agentIsVisible(Agent* seeing, Agent* dest) {
	float ownerx = (seeing->x + (seeing->getWidth() / 2.0f));
	float ownery = (seeing->y + (seeing->getHeight() / 2.0f));
	MetaRoom* ownermeta = world.map->metaRoomAt(ownerx, ownery);
	std::shared_ptr<Room> ownerroom = world.map->roomAt(ownerx, ownery);
	if (!ownermeta)
		return false;
	if (!ownerroom)
		return false;

	return agentIsVisible(seeing, dest, ownerx, ownery, ownermeta, ownerroom);
}

std::vector<std::shared_ptr<Agent> > getVisibleList(Agent* seeing, unsigned char family, unsigned char genus, unsigned short species) {
	std::vector<std::shared_ptr<Agent> > agents;

	float ownerx = (seeing->x + (seeing->getWidth() / 2.0f));
	float ownery = (seeing->y + (seeing->getHeight() / 2.0f));
	MetaRoom* ownermeta = world.map->metaRoomAt(ownerx, ownery);
	std::shared_ptr<Room> ownerroom = world.map->roomAt(ownerx, ownery);
	if (!ownermeta)
		return agents;
	if (!ownerroom)
		return agents;

	for (auto& agent : world.agents) {
		std::shared_ptr<Agent> a = agent;
		if (!a)
			continue;

		// TODO: if owner is a creature, skip stuff with invisible attribute

		// verify species/genus/family
		if (species && species != a->species)
			continue;
		if (genus && genus != a->genus)
			continue;
		if (family && family != a->family)
			continue;

		if (agentIsVisible(seeing, a.get(), ownerx, ownery, ownermeta, ownerroom))
			agents.push_back(a);
	}

	return agents;
}

bool agentsTouching(Agent* first, Agent* second) {
	assert(first && second);

	// TODO: c2e docs say it only checks if bounding lines overlap, implement it like that?

	// this check should probably be integrated into line overlap check?
	if (first == second)
		return false;

	if (first->x < second->x) {
		if ((first->x + first->getWidth()) < second->x)
			return false;
	} else {
		if ((second->x + second->getWidth()) < first->x)
			return false;
	}

	if (first->y < second->y) {
		if ((first->y + first->getHeight()) < second->y)
			return false;
	} else {
		if ((second->y + second->getHeight()) < first->y)
			return false;
	}

	return true;
}

std::shared_ptr<Room> roomContainingAgent(AgentRef agent) {
	MetaRoom* m = world.map->metaRoomAt(agent->x, agent->y);
	if (!m)
		return std::shared_ptr<Room>();
	return m->roomAt(agent->x + (agent->getWidth() / 2.0f), agent->y + (agent->getHeight() / 2.0f));
}

// Edible scent: how nourishing an object smells. Read from the object's own
// eat script (event 12): the nutrients its stimulus gives the eater (glucose,
// fatty acid, glycogen, starch, fat, protein, amino acid) plus the direct
// "Hunger Decrease" chemical, summed and scaled to 0-1; -1 if it also gives a
// poison (it smells of poison: the mind never eats it). Being read from the scripts, it also works for add-on objects.
// Cached per script.
namespace {
struct FoodInfo {
	float scent = 0.0f;
	bool anyPose = true; // feeds whatever its pose
	std::set<int> poses; // else only in these (its stimulus sits inside "doif pose eq N")
};
} // namespace

static const FoodInfo& foodInfo(Agent* a) {
	static std::map<const script*, FoodInfo> cache;
	static const FoodInfo none;
	std::shared_ptr<script> sc = a->findScript(12);
	if (!sc)
		return none;
	auto it = cache.find(sc.get());
	if (it != cache.end())
		return it->second;
	std::istringstream in(sc->code);
	std::vector<std::string> tok;
	std::string t;
	while (in >> t) {
		std::transform(t.begin(), t.end(), t.begin(), [](unsigned char ch) { return std::tolower(ch); });
		tok.push_back(t);
	}
	auto number = [](const std::string& w, int& v) {
		if (w.empty() || !(std::isdigit((unsigned char)w[0]) || w[0] == '-'))
			return false;
		v = std::atoi(w.c_str());
		return true;
	};
	FoodInfo info;
	bool anyFeeding = false;
	int total = 0;
	bool poison = false;
	// the conditions the script is inside: a pose (N >= 0) or anything else (-1)
	std::vector<int> conds;
	for (size_t i = 0; i < tok.size(); i++) {
		if (tok[i] == "doif") {
			int n;
			if (i + 3 < tok.size() && tok[i + 1] == "pose" && tok[i + 2] == "eq" && number(tok[i + 3], n) && n >= 0)
				conds.push_back(n);
			else
				conds.push_back(-1);
			continue;
		}
		if (tok[i] == "else" && !conds.empty()) {
			conds.back() = -1;
			continue;
		}
		if (tok[i] == "endi" && !conds.empty()) {
			conds.pop_back();
			continue;
		}
		if (tok[i] != "stim" || i + 1 >= tok.size())
			continue;
		size_t j = i + 1;
		if (tok[j] == "writ")
			j += 2; // the receiving object
		else if (tok[j] == "shou" || tok[j] == "sign" || tok[j] == "tact")
			j += 1;
		else
			continue;
		j += 4; // significance, input, intensity, features
		int here = 0;
		for (int k = 0; k < 4 && j + 1 < tok.size(); k++, j += 2) {
			int chem, amount;
			if (!number(tok[j], chem) || !number(tok[j + 1], amount))
				break;
			if (chem == 37 || (chem >= 70 && chem <= 74) || chem == 79 || chem == 80)
				here += amount;
			// C2 poisons (heavy metals, cyanide, belladonna, geddonase,
			// glycotoxin, the toxins 234-239): food that carries them does
			// not smell like food (the game's poisonous berries give cyanide
			// with their starch, and norns starved... of ATP eating them)
			if (amount > 0 && (chem == 140 || chem == 141 || chem == 143 || chem == 144 || chem == 145 || (chem >= 234 && chem <= 239)))
				poison = true;
		}
		if (here <= 0)
			continue;
		total = std::max(total, here); // the most one bite gives
		int pose = -1;
		for (int c : conds)
			if (c >= 0)
				pose = c;
		if (pose < 0)
			anyFeeding = true;
		else
			info.poses.insert(pose);
	}
	info.anyPose = anyFeeding || info.poses.empty();
	info.scent = poison ? -1.0f : std::clamp(total / 255.0f, 0.0f, 1.0f);
	static const bool watch = getenv("OPENC2E_WATCH_CREATURES") != nullptr;
	if (watch) {
		std::string ps;
		for (int q : info.poses)
			ps += fmt::format(" {}", q);
		fmt::print("* WATCH SCENT {} {} {} category {} edible {:.2f}{}\n", (int)a->family, (int)a->genus, a->species, a->category, info.scent,
			info.anyPose ? std::string("") : " in poses" + ps);
	}
	return cache[sc.get()] = info;
}

// Edible scent: how nourishing an object smells. Read from the object's own
// eat script (event 12): the nutrients one bite's stimulus gives the eater
// (glucose, fatty acid, glycogen, starch, fat, protein, amino acid) plus the
// direct "Hunger Decrease" chemical, summed and scaled to 0-1; -1 if it also
// gives a poison (it smells of poison: the mind never eats it). Being read
// from the scripts, it also works for add-on objects. Cached per script.
float edibleScent(Agent* a) {
	return foodInfo(a).scent;
}

// Edible scent as the object is now (2026-11-14: of 1,767 meals in 51
// minutes 489 fed). C2 marks what creatures may do to an object with BHVR's
// touch value, and food that cannot be eaten (a tomato core or nut shell
// left after eating, rotting fruit, our fruit still on the bush) has no eat
// bit (64); and many C2 foods' eat scripts feed only in some poses (a
// tomato or nut only fresh, poses 0-1: a ripening nut, poses 2-3, still has
// the eat bit and gives nothing). Their scripts still name nourishing
// chemicals, so creatures went on "eating" them. Such food neither smells
// of food nor looks like it.
float edibleNow(Agent* a) {
	const FoodInfo& info = foodInfo(a);
	float scent = info.scent;
	// (developer aid: OPENC2E_IGNORE_TOUCH=1 turns this off, for comparison)
	static const bool ignore = getenv("OPENC2E_IGNORE_TOUCH") != nullptr;
	if (scent <= 0.0f || ignore || engine.version > 2)
		return scent;
	if (a->c2touch >= 0 && !(a->c2touch & 64))
		return 0.0f;
	if (!info.anyPose)
		if (auto* sp = dynamic_cast<SpritePart*>(a->part(0)); sp && !info.poses.count((int)sp->getPose()))
			return 0.0f;
	return scent;
}

