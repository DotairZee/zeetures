/*
 *  Map.cpp
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

#include "Map.h"

#include "Engine.h"
#include "MetaRoom.h"
#include "Room.h"
#include "common/Exception.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <fmt/core.h>
#include <memory>

MetaRoom* Map::getFallbackMetaroom() {
	return metarooms.size() == 0 ? NULL : metarooms[0].get();
}

Map::Map() {
	width = 0;
	height = 0;
	room_base = 0;
	metaroom_base = 0;
}

Map::~Map() = default;

void Map::Reset() {
	metarooms.clear();
	// todo: metarooms should be responsible for deleting rooms, so use the following instead of clear:
	// assert(rooms.empty());
	rooms.clear();
}

void Map::SetMapDimensions(unsigned int w, unsigned int h) {
	// todo: check for outlying metarooms
	width = w;
	height = h;
}

MetaRoom* Map::addMetaRoom(int _x, int _y, int _width, int _height, const std::string& back, bool wrap) {
	// todo: check if it's outlying
	std::unique_ptr<MetaRoom> m(new MetaRoom);
	while (getMetaRoom(metaroom_base))
		metaroom_base++;
	m->id = metaroom_base++;
	m->xloc = _x;
	m->yloc = _y;
	m->wid = _width;
	m->hei = _height;
	m->wraps = wrap;
	if (!back.empty()) {
		m->addBackground(back);
	}
	metarooms.emplace_back(std::move(m));
	return metarooms.back().get();
}

MetaRoom* Map::getMetaRoom(unsigned int room) {
	for (auto& metaroom : metarooms)
		if (metaroom->id == room)
			return metaroom.get();
	return nullptr;
}

bool Map::hasDoor(const std::shared_ptr<Room>& r1, const std::shared_ptr<Room>& r2) {
	auto i = r1->doors.find(r2);
	if (i == r1->doors.end()) {
		return false;
	}
	return true;
}

void Map::setDoorPerm(const std::shared_ptr<Room>& r1, const std::shared_ptr<Room>& r2, int perm) {
	if (hasDoor(r1, r2)) {
		r1->doors[r2].perm = perm;
		r2->doors[r1].perm = perm;
	} else {
		r1->doors[r2] = RoomDoor{perm};
		r2->doors[r1] = RoomDoor{perm};
	}
	if (perm > 0)
		r1->doors[r2].everOpen = r2->doors[r1].everOpen = true;
}

void Map::markDoorOpenable(const std::shared_ptr<Room>& r1, const std::shared_ptr<Room>& r2) {
	if (hasDoor(r1, r2))
		r1->doors[r2].everOpen = r2->doors[r1].everOpen = true;
}

int Map::sideOf(Room& from, Room& to) {
	float fx = ((float)from.x_left + from.x_right) / 2.0f, tx = ((float)to.x_left + to.x_right) / 2.0f;
	float fy = ((float)from.y_left_ceiling + from.y_right_ceiling + from.y_left_floor + from.y_right_floor) / 4.0f;
	float ty = ((float)to.y_left_ceiling + to.y_right_ceiling + to.y_left_floor + to.y_right_floor) / 4.0f;
	bool left = to.x_right <= from.x_left, right = to.x_left >= from.x_right;
	if (left || right) {
		// across a wrapping metaroom's seam, the far end is the near side
		MetaRoom* m = metaRoomAt((unsigned int)fx, (unsigned int)fy);
		if (m && m->wraparound() && std::fabs(tx - fx) > m->width() / 2.0f)
			std::swap(left, right);
		return left ? 0 : 1;
	}
	return ty < fy ? 2 : 3;
}

std::vector<std::shared_ptr<Room>> Map::neighbours(const std::shared_ptr<Room>& r, int side) {
	std::vector<std::shared_ptr<Room>> out;
	for (auto& [other, door] : r->getDoors())
		if (sideOf(*r, *other) == side)
			out.push_back(other);
	auto along = [side](Room& n) {
		return side < 2 ? ((float)n.y_left_ceiling + n.y_right_ceiling + n.y_left_floor + n.y_right_floor) / 4.0f
						: ((float)n.x_left + n.x_right) / 2.0f;
	};
	std::sort(out.begin(), out.end(), [&](auto& a, auto& b) {
		float pa = along(*a), pb = along(*b);
		return pa != pb ? pa < pb : a->id < b->id;
	});
	return out;
}

void Map::removeNeighbours(const std::shared_ptr<Room>& r, int side) {
	for (auto& n : neighbours(r, side))
		r->doors.erase(n);
}

void Map::setOwnDoorPerm(const std::shared_ptr<Room>& r, const std::shared_ptr<Room>& other, int perm) {
	r->doors[other].perm = perm;
	if (perm > 0)
		r->doors[other].everOpen = true;
}

void Map::relinkNeighbour(const std::shared_ptr<Room>& r, const std::shared_ptr<Room>& oldroom, const std::shared_ptr<Room>& newroom) {
	auto i = r->doors.find(oldroom);
	if (i == r->doors.end() || oldroom == newroom)
		return;
	RoomDoor d = i->second;
	r->doors.erase(i);
	r->doors[newroom] = d;
}

int Map::getDoorPerm(const std::shared_ptr<Room>& r1, const std::shared_ptr<Room>& r2) {
	if (!hasDoor(r1, r2)) {
		throw Exception("Door doesn't exist!");
	}
	return r1->doors[r2].perm;
}

std::shared_ptr<Room> Map::getRoom(unsigned int r) {
	for (auto& room : rooms)
		if (room->id == r)
			return room;
	return std::shared_ptr<Room>();
}

unsigned int Map::getMetaRoomCount() {
	return metarooms.size();
}

unsigned int Map::getRoomCount() {
	return rooms.size();
}

void Map::tick() {
	if (engine.version < 3)
		return; // TODO: tick rooms in C2

	// Three passes..
	for (auto& metaroom : metarooms)
		for (auto& r : metaroom->rooms)
			r->tick();
	for (auto& metaroom : metarooms)
		for (auto& r : metaroom->rooms)
			r->postTick();
	for (auto& metaroom : metarooms)
		for (auto& r : metaroom->rooms)
			r->resetTick();
}

MetaRoom* Map::metaRoomAt(unsigned int x, unsigned int y) {
	for (auto& m : metarooms) {
		if (x >= m->x() && y >= m->y()) {
			if (x <= m->x() + m->width() && y <= m->y() + m->height()) {
				return m.get();
			}
		}
	}
	return nullptr;
}

MetaRoom* Map::wrappingMetaRoomNear(float x, float y) {
	for (auto& m : metarooms) {
		int left = (int)m->x(), right = left + (int)m->width();
		if (m->wraparound() && y >= (int)m->y() && y <= (int)m->y() + (int)m->height() && x >= left - (int)m->width() && x < right + (int)m->width())
			return m.get();
	}
	return nullptr;
}

std::shared_ptr<Room> Map::roomAt(float _x, float _y) {
	MetaRoom* m = metaRoomAt((unsigned int)_x, (unsigned int)_y); // TODO: good casts?
	if (!m)
		return std::shared_ptr<Room>();
	return m->roomAt(_x, _y);
}

std::vector<std::shared_ptr<Room>> Map::roomsAt(float _x, float _y) {
	MetaRoom* m = metaRoomAt((unsigned int)_x, (unsigned int)_y); // TODO: good casts?
	if (!m)
		return std::vector<std::shared_ptr<Room>>();
	return m->roomsAt(_x, _y);
}

bool Map::collideLineWithRoomSystem(Point src, Point dest, std::shared_ptr<Room>& room, Point& where, Line& wall, unsigned int& walldir, int perm) {
	std::shared_ptr<Room> newRoom;

	where = src;

	while (true) {
		if (!collideLineWithRoomBoundaries(where, dest, room, newRoom, where, wall, walldir, perm))
			return false; // failure
		if (!newRoom)
			return true; // collision
		if (where == dest)
			return false; // got there

		assert(newRoom != room); // tsk

		room = newRoom;
	}
}

/*
 * poss. optimisation: skip checking the rest of the lines if our distance is 0?
 */
bool Map::collideLineWithRoomBoundaries(Point src, Point dest, std::shared_ptr<Room> room, std::shared_ptr<Room>& newroom, Point& where, Line& wall, unsigned int& walldir, int perm) {
	assert(room);
	// TODO: this assert fails. why? 'where' is presumably outside the dest room sometimes.. mmh
	//assert(room->containsPoint(src.x, src.y));
	if (src == dest)
		return false;

	newroom.reset();

	/* if (room->containsPoint(dest.x, dest.y)) {
		where = dest;
		return false;
	}*/

	float distance = 100000000.0f; // TODO: lots.
	bool foundsomething = false;
	// bool previousroom = newroom != nullptr;
	Point oldpoint = where;
	Line movement(src, dest);

	Line x[4] = {room->left, room->right, room->top, room->bottom};
	for (unsigned int i = 0; i < 4; i++) {
		/*if (previousroom)
			if (x[i].containsPoint(oldpoint)) continue; */

		Point temppoint;
		if (x[i].intersect(movement, temppoint)) {
			if (!movement.containsPoint(temppoint)) {
				// (at most a few times a run)
				static int said = 0;
				if (said < 20 && ++said) {
					fmt::print("physics bug: intersect wasn't on movement line at ({}, {})\n", temppoint.x, temppoint.y);
					fmt::print("room line: ({}, {}) to ({}, {})\n", x[i].getStart().x, x[i].getStart().y, x[i].getEnd().x, x[i].getEnd().y);
					fmt::print("movement line: ({}, {}) to ({}, {}){}\n", src.x, src.y, dest.x, dest.y, said == 20 ? " (no more of these this run)" : "");
				}
				return false; // go away
			}
			assert(movement.containsPoint(temppoint));

			//if (temppoint == src) return false; // for debug use: sneakily fail all movement between rooms

			// see if this is nearer than any previous points we've found
			double distx = temppoint.x - src.x;
			double disty = temppoint.y - src.y;
			double d = distx * distx + disty * disty;
			if (d > distance)
				continue;

			// work out which room is next along our movement vector
			// TODO: this code utterly sucks, doesn't work properly
			float newx, newy;
			/*if (temppoint == src) { // TODO: this is not an accurate check!! likely cause of falling through PERM
				// we might be on a PERM line! check backwards.
				newx = temppoint.x + (src.x <= dest.x ? (src.x == dest.x ? 0.0 : -0.5) : 0.5);
				newy = temppoint.y + (src.y <= dest.y ? (src.y == dest.y ? 0.0 : -0.5) : 0.5);
			} else {*/
			newx = temppoint.x + (src.x <= dest.x ? (src.x == dest.x ? 0.0 : 0.5) : -0.5);
			newy = temppoint.y + (src.y <= dest.y ? (src.y == dest.y ? 0.0 : 0.5) : -0.5);
			//}

			if (room->containsPoint(newx, newy)) { // if a little along our movement vector is still in our room, forget it
				/* fmt::print("physics debug: next room is original room at ({}, {})\n", temppoint.x, temppoint.y);
				fmt::print("room line: ({}, {}) to ({}, {})\n", x[i].getStart().x, x[i].getStart().y, x[i].getEnd().x, x[i].getEnd().y);
				fmt::print("movement line: ({}, {}) to ({}, {})\n", movement.getStart().x, movement.getStart().y, movement.getEnd().x, movement.getEnd().y);
				return false;*/
				continue; // continue, bad collision
			}

			std::shared_ptr<Room> nextroom;
			bool foundroom = false;

			for (auto r = room->doors.begin(); r != room->doors.end(); r++) {
				std::shared_ptr<Room> otherroom = r->first.lock();
				assert(otherroom);
				// assert(r->second);
				if (otherroom->containsPoint(newx, newy)) {
					// this is our next room!
					foundroom = true;
					// if boundary perm is going to let us through..
					if (perm <= r->second.perm) // TODO: right?
						nextroom = otherroom;
					break;
				}
			}

			if (!foundroom && !nextroom)
				nextroom = roomAt(newx, newy);

			/*if (temppoint == src) { // if we're just doing the backwards PERM check
				if (!foundroom || nextroom != 0) {
					// either there's no previous room or PERM let us into the previous room..
					// ..so forget this collision
					break;
				}
			}*/

			std::shared_ptr<Room> z = roomAt(temppoint.x, temppoint.y); // TODO: evil performance-killing debug check
			if (!z) {
				// TODO: commented out this error message for sake of fuzzie's sanity, but it's still an issue
				/* fmt::print("physics bug: fell out of room system at ({}, {})\n", where.x, where.y);
				fmt::print("room line: ({}, {}) to ({}, {})\n", x[i].getStart().x, x[i].getStart().y, x[i].getEnd().x, x[i].getEnd().y);
				fmt::print("movement line: ({}, {}) to ({}, {})\n", movement.getStart().x, movement.getStart().y, movement.getEnd().x, movement.getEnd().y); */
				return false; // go away
			}

			// it is nearer and not the same room, so make it our priority
			distance = d;
			foundsomething = true;
			where = temppoint;
			wall = x[i];
			walldir = i;
			newroom = nextroom;
		}
	}

	if (!foundsomething) {
		if (room->containsPoint(dest.x, dest.y)) {
			where = dest;
		} else {
			where = src; // TODO: can't we assume this anyway?
			// TODO: commented out this error message for sake of fuzzie's sanity, but it's still an issue
			/* fmt::print("physics bug: didn't collide with a line, nor stay in the current room!\n");
			fmt::print("movement line: ({}, {}) to ({}, {})\n", movement.getStart().x, movement.getStart().y, movement.getEnd().x,  movement.getEnd().y); */
		}
		return false;
	}

	return true;
}

/* vim: set noet: */
