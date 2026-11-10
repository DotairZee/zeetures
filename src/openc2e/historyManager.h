/*
 *  historyManager.h
 *  openc2e
 *
 *  Created by Alyssa Milburn on Wed 26 Apr 2006.
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

#ifndef _HISTORYMANAGER_H
#define _HISTORYMANAGER_H

#include "AgentRef.h"
#include "creatures/lifestage.h"

#include <map>
#include <memory>
#include <vector>

using std::shared_ptr;
using std::weak_ptr;

class genomeFile;

struct historyevent {
	unsigned int eventno;
	lifestage stage;
	std::string photo; // TODO: correct?
	std::string monikers[2];
	std::string networkid;
	unsigned int timestamp;
	int tage;
	std::string usertext;
	std::string worldname;
	std::string worldmoniker;
	unsigned int worldtick;
	historyevent(unsigned int eno, class CreatureAgent* = 0);
};

enum monikerstatus { referenced = 1,
	creature = 2,
	borncreature = 3,
	exported = 4,
	dead = 5,
	deadandkilled = 6,
	unreferenced = 7 };

class monikerData {
	friend class historyManager;

  private:
	monikerstatus status;
	std::string moniker;

  public:
	weak_ptr<genomeFile> genome;
	AgentRef owner;
	std::vector<historyevent> events;
	int gender;
	int variant;
	std::string name;
	unsigned char genus;
	bool warpveteran;
	unsigned int no_crossover_points, no_point_mutations;

	// the owner's records (Owner's and Observation Kits; saved with the world)
	unsigned int borntick = 0; // world tick count when born, or when first seen
	bool bornknown = false; // false: borntick is when this world first saw it
	bool registered = false; // birth registered (Owner's Kit)
	std::vector<std::string> photos; // pictures in the world folder's photos/
	std::string notes;
	// its death, if it has died (the dead are not saved; the life log keeps them)
	bool died = false;
	unsigned int diedtick = 0; // world tick count
	int diedstage = -1;
	// its parents' monikers (set by a conceived or spliced event; kept with the world)
	std::string mother, father;

	void init(std::string, std::shared_ptr<genomeFile>);
	// the records of a creature no longer in the world (restored from a save)
	void initRecord(std::string moniker, unsigned char genus, int gender);
	const std::string& getName() const { return moniker; }
	historyevent& addEvent(unsigned int event, std::string moniker1 = "", std::string moniker2 = "");
	void moveToAgent(AgentRef a);
	void moveToCreature(AgentRef c);
	void transferCreature(AgentRef c); // same creature, new body (openc2e: life stage changed at will)
	void wasBorn();
	void hasDied();
	monikerstatus getStatus();
};

// Births, deaths and pregnancies, for the interface to announce (status-bar
// notices) and the Owner's Kit to log. The log is kept with the world.
struct lifeLogEntry {
	enum { birth = 0,
		death = 1,
		pregnancy = 2 };
	int kind = birth;
	unsigned int tick = 0; // world tick count
	std::string moniker;
	std::string name; // its name at the time, or empty
	int genus = 1; // 1 norn, 2 grendel, 3 ettin
	bool female = false;
	int stage = 0; // life stage at the time
	unsigned int age = 0; // world ticks lived, for a death
	bool ageknown = true; // false: counted from when this world first saw it
	bool oldage = false; // died at the end of its senile stage
};

class historyManager {
  private:
	std::map<std::string, monikerData> monikers;

  public:
	std::vector<lifeLogEntry> lifelog;
	std::vector<lifeLogEntry> notices; // not yet shown by the interface (it takes them)
	bool quiet = false; // restoring creatures from a save: nothing is new
	// default names ("Norn 0005"): how many of each genus this world has had
	std::map<int, unsigned int> namecount;
	bool namesgiven = false; // the creatures there were at load have their numbers
	std::string nextDefaultName(int genus);
	// every moniker's records, living or not
	std::map<std::string, monikerData>& all() { return monikers; }
	// gives a creature's records back the moniker it was saved with (false if taken)
	bool renameMoniker(const std::string& from, const std::string& to);
	monikerData& addRecord(const std::string& moniker, unsigned char genus, int gender);
	// records a birth, death or pregnancy (unless quiet)
	void logLife(int kind, const std::string& moniker, int stage, bool oldage = false);

	std::string newMoniker(std::shared_ptr<genomeFile>);
	bool hasMoniker(std::string);
	monikerData& getMoniker(std::string);
	std::string findMoniker(std::shared_ptr<genomeFile>);
	std::string findMoniker(AgentRef);
	void delMoniker(std::string);
};

#endif
/* vim: set noet: */
