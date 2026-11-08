/*
 *  AgentHelpers.h
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

#ifndef AGENTHELPERS_H
#define AGENTHELPERS_H

#include <memory>
#include <vector>

class Agent;
class AgentRef;
class MetaRoom;
class Room;

bool agentIsVisible(Agent* seeing, Agent* a, float ownerx, float ownery, MetaRoom* ownermeta, std::shared_ptr<Room> ownerroom);
bool agentIsVisible(Agent* seeing, Agent* dest);
bool agentPartlyVisible(Agent* seeing, Agent* dest); // any part of it, not only its centre
std::vector<std::shared_ptr<Agent> > getVisibleList(Agent* seeing, unsigned char family, unsigned char genus, unsigned short species);

bool agentsTouching(Agent* first, Agent* second);
std::shared_ptr<Room> roomContainingAgent(AgentRef agent);

// how nourishing an object's eat script makes it smell, 0-1
float edibleScent(Agent* a);

#endif

/* vim: set noet: */
