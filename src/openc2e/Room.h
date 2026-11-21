/*
 *  Room.h
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

#pragma once

#include <algorithm>

#include "common/Color.h"
#include "physics.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

constexpr int CA_COUNT = 20;

class Map;
class Room;
class RenderTarget;

struct RoomDoor {
	int perm;
	// open at any time this session: a door scripts open and shut (the
	// airlock's) still joins its rooms when shut, for telling sealed pockets
	// from rooms closed for the moment
	bool everOpen = false;
};

class Room {
  private:
	friend Map;
	std::map<std::weak_ptr<Room>, RoomDoor, std::owner_less<std::weak_ptr<Room>>> doors;

  public:
	// every doorway shut (a sealed pocket of rock in the room map, or an
	// airlock with both doors closed)
	bool allDoorsShut() const {
		if (doors.empty())
			return false;
		for (auto& d : doors)
			if (d.second.perm > 0)
				return false;
		return true;
	}

	unsigned int x_left, x_right, y_left_ceiling, y_right_ceiling;
	unsigned int y_left_floor, y_right_floor;
	// raised whenever any room's bounds are set: MetaRoom's lookup grid is
	// rebuilt when it changes
	static unsigned int boundsGeneration;

	std::vector<std::pair<unsigned int, unsigned int>> floorpoints;

	Line left, right, top, bottom;
	int type = 0;

	// Creatures 2
	int temp = 0, lite = 0, radn = 0, ontr = 0, intr = 0, pres = 0, hsrc = 0, lsrc = 0, rsrc = 0, psrc = 0;
	float tempf = -1.0f; // C2 climate (World::tickClimate): temperature with fractions; -1 until first step
	// C2 air (World::tickPressure): pressure with fractions (-1 until first
	// step) and the world file's pressure it settles back toward
	float presf = -1.0f;
	int presBase = -1;
	// C2 radiation (World::tickRadiation): with fractions (-1 until first step)
	float radnf = -1.0f;
	// C2 nutrient cycle (World::tickNutrients): organic and inorganic
	// nutrients with fractions (-1 until first step), and the levels the
	// world file gave the room, which each settles back toward (-1: not yet
	// known)
	float ontrf = -1.0f, intrf = -1.0f;
	int ontrBase = -1, intrBase = -1;
	// day and night (World::tickDaylight): the room's own light (the world
	// file's, or what a script last set), how far outdoors it is (0-1, -1
	// not yet known) and the light last given it
	int liteBase = -1, liteShown = -1;
	float outdoors = -1.0f;
	int floorvalue = 0, dropstatus = 0;
	int windx, windy;

	std::string music;

	unsigned int id;

	float ca[CA_COUNT], catemp[CA_COUNT];

	// C2: smell of food spreading through the room system (World::tickFoodScent)
	float foodscent = 0, foodscentnext = 0;
	// C2: the scents of pheromones.txt, by channel (creatures/Pheromones.h)
	std::vector<float> scent, scentnext;
	// C2: each creature's own scent (its signature, by moniker), where any
	// of it is (creatures/Pheromones.h)
	std::map<std::string, float> sig, signext;

	// C2: water too deep to wade (owner, 2026-11-10: the path to the cave
	// behind the pond's waterfall runs along the pond's bed). Creatures do
	// not walk from dry ground into water, nor does the mind count a way
	// through it; water no deeper than WADE_DEPTH px (only the pond by the
	// incubator, room 19, 53 px; an adult's head stays above it) is walked
	// through like ground. Breathing is judged as always, by the head.
	static constexpr unsigned int WADE_DEPTH = 55;
	bool deepWater() const {
		return type == 2 && std::max(y_left_floor, y_right_floor) - std::min(y_left_ceiling, y_right_ceiling) > WADE_DEPTH;
	}
	bool shallowWater() const { return type == 2 && !deepWater(); }

	bool containsPoint(float x, float y);
	bool containsPoint(Point p);

	float floorYatX(float x);
	std::vector<std::pair<std::shared_ptr<Room>, RoomDoor>> getDoors();

	Room();
	Room(unsigned int x_l, unsigned int x_r, unsigned int y_l_t, unsigned int y_r_t, unsigned int y_l_b, unsigned int y_r_b);
	// move or resize the room, keeping its wall lines (used for containment
	// and collisions) in step with the corner numbers
	void setBounds(unsigned int x_l, unsigned int x_r, unsigned int y_l_t, unsigned int y_r_t,
		unsigned int y_l_b, unsigned int y_r_b);
	void tick();
	void postTick();
	void resetTick();

	void renderBorders(RenderTarget* surf, int xoffset, int yoffset, Color col);
};

/* vim: set noet: */
