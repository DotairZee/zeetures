/*
 *  Map.h
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

#ifndef _OPENC2E_MAP_H
#define _OPENC2E_MAP_H

#include "physics.h"
#include "serfwd.h"

#include <memory>
#include <string>
#include <vector>

class Room;
class MetaRoom;

class Map {
  protected:
	FRIEND_SERIALIZE(Map)
	unsigned int width, height;
	std::vector<std::unique_ptr<MetaRoom>> metarooms;
	std::vector<std::shared_ptr<Room>> rooms;

	friend class MetaRoom;

  public:
	const std::vector<std::shared_ptr<Room>>& getRooms() const { return rooms; }
	/* Get a room, any room.
	 *
	 * For those times when you really, really need a room.
	 */
	MetaRoom* getFallbackMetaroom();

	unsigned int room_base, metaroom_base;

	Map();
	~Map();

	void Reset();
	void SetMapDimensions(unsigned int, unsigned int);
	unsigned int getWidth() { return width; }
	unsigned int getHeight() { return height; }

	MetaRoom* addMetaRoom(int x, int y, int width, int height, const std::string& background, bool wrap = false);
	MetaRoom* getMetaRoom(unsigned int);

	bool hasDoor(const std::shared_ptr<Room>& r1, const std::shared_ptr<Room>& r2);
	void setDoorPerm(const std::shared_ptr<Room>& r1, const std::shared_ptr<Room>& r2, int perm);
	int getDoorPerm(const std::shared_ptr<Room>& r1, const std::shared_ptr<Room>& r2);
	// counts the door as one that opens (RoomDoor::everOpen) though it is
	// shut now: a script can open it
	void markDoorOpenable(const std::shared_ptr<Room>& r1, const std::shared_ptr<Room>& r2);

	// C2's per-side neighbor lists (RMN#, RMNR, RMND, DELN). A room's links
	// are one map (Room::doors), not lists by side: the side of each linked
	// room is told from where the two rooms sit (0 left, 1 right, 2 up,
	// 3 down; a wrapping metaroom's far end counts as next door), and a
	// side's links are ordered along that edge (top to bottom, left to
	// right), then by room number. Only the given room's own links are read
	// or changed; the other room keeps its link back.
	int sideOf(Room& from, Room& to);
	std::vector<std::shared_ptr<Room>> neighbours(const std::shared_ptr<Room>& r, int side);
	void removeNeighbours(const std::shared_ptr<Room>& r, int side);
	// the permeability of r's own link to other (other's link back unchanged)
	void setOwnDoorPerm(const std::shared_ptr<Room>& r, const std::shared_ptr<Room>& other, int perm);
	// r's link to oldroom now leads to newroom, keeping its permeability
	void relinkNeighbour(const std::shared_ptr<Room>& r, const std::shared_ptr<Room>& oldroom, const std::shared_ptr<Room>& newroom);

	unsigned int getMetaRoomCount();
	std::shared_ptr<Room> getRoom(unsigned int);
	unsigned int getRoomCount();

	MetaRoom* metaRoomAt(unsigned int, unsigned int);
	// the wrapping metaroom that a point up to one width past either of its
	// side edges belongs to (null if none)
	MetaRoom* wrappingMetaRoomNear(float x, float y);
	std::shared_ptr<Room> roomAt(float, float);
	std::vector<std::shared_ptr<Room>> roomsAt(float, float);

	bool collideLineWithRoomSystem(Point src, Point dest, std::shared_ptr<Room>& room, Point& where, Line& wall, unsigned int& walldir, int perm);
	bool collideLineWithRoomBoundaries(Point src, Point dest, std::shared_ptr<Room> room, std::shared_ptr<Room>& newroom, Point& where, Line& wall, unsigned int& walldir, int perm);

	void tick();
};

#endif
/* vim: set noet: */
