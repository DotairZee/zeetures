/*
 *  MetaRoom.h
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

#ifndef _C2E_METAROOM_H
#define _C2E_METAROOM_H

#include "serfwd.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

class creaturesImage;
class Map;

class MetaRoom {
  protected:
	friend Map;
	FRIEND_SERIALIZE(MetaRoom)
	unsigned int xloc, yloc, wid, hei, fullwid, fullhei;
	std::map<std::string, std::shared_ptr<creaturesImage> > backgrounds;
	std::string firstback;
	std::string current_background;
	bool wraps;

	// Lookup grid for roomAt/roomsAt (performance, 2026-11-16): for each
	// 32-pixel cell, the indices into rooms of those whose bounding box
	// touches it, in the rooms' own order, so a lookup tests a handful of
	// rooms instead of all of them and answers exactly as the full scan did.
	// Rebuilt when rooms are added or any room's bounds change.
	static constexpr unsigned int gridCell = 32;
	std::vector<std::vector<unsigned int>> roomGrid;
	unsigned int gridCols = 0, gridRows = 0;
	size_t gridRoomCount = (size_t)-1;
	unsigned int gridGeneration = 0;
	void buildRoomGrid();
	// the cell's room indices for a point, or null outside the grid
	const std::vector<unsigned int>* roomCell(float x, float y);

	MetaRoom() {}

  public:
	std::vector<std::shared_ptr<class Room> > rooms;

	unsigned int x() { return xloc; }
	unsigned int y() { return yloc; }
	unsigned int width() { return wid; }
	unsigned int height() { return hei; }
	unsigned int fullwidth() { return fullwid; }
	unsigned int fullheight() { return fullhei; }
	bool wraparound() { return wraps; }
	void setWraparound(bool w) { wraps = !!w; }

	unsigned int addRoom(std::shared_ptr<class Room>);

	void addBackground(std::string);
	bool hasBackground(std::string) const;
	void setBackground(std::string);
	std::shared_ptr<creaturesImage> getCurrentBackground();
	std::string getCurrentBackgroundName() const;
	std::vector<std::string> backgroundList();

	std::shared_ptr<Room> nextFloorFromPoint(float x, float y);

	std::shared_ptr<Room> roomAt(float x, float y);
	std::vector<std::shared_ptr<Room> > roomsAt(float x, float y);

	std::string music;

	unsigned int id;

	~MetaRoom();
};

#endif
/* vim: set noet: */
