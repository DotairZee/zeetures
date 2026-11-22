/*
 *  Disease.h
 *  openc2e
 *
 *  Bacteria and disease for Creatures 2 worlds (owner, 2026-11-21; our own
 *  design from the game's manual: "Albia also has bacteria ... Some are
 *  trapped in caves and others are carried by other creatures, such as the
 *  Grendels ... If your Norns start coughing and sneezing, or running a
 *  fever, they could have caught a disease", and sneezing "spreads disease
 *  very quickly"). The genomes already carry the body's side: antigens
 *  (chemicals 248-255) wear organs, the immune organ turns each into its
 *  antibody (240-247) and makes fever toxin, histamine A and B set off
 *  sneezing and coughing. What was missing is the germs themselves.
 *
 *  A strain has an antigen and a toxin. Germs of each strain live in rooms
 *  (dark caves keep them, rotting food feeds them, light kills them, they
 *  seep through doorways); grendels carry a strain for life without falling
 *  ill and shed it; ill creatures shed theirs. A creature in a room with
 *  germs may catch a strain unless it has that strain's antibody. While ill
 *  the germs feed antigen and toxin into its blood; it is over once its
 *  antibody has built up. A sneeze (SNEZ) passes its strains to creatures
 *  in front of it. Now and then a strain changes its antigen, so old
 *  immunity stops protecting.
 *
 *  Switch OPENC2E_NO_DISEASE turns it all off. OPENC2E_WATCH_DISEASE logs
 *  infections, recoveries and sneezes.
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

#include <iosfwd>
#include <sstream>
#include <string>
#include <vector>

class Agent;

namespace Disease {

struct Strain {
	int id = 0; // 1, 2, ... (its letter: A, B, ...)
	int antigen = 0; // 0-7: chemical 248 + antigen
	int toxin = 232; // the chemical it makes: 232 histamine A (sneezing), 233 histamine B (coughing), 234 sleep toxin, 235 fever toxin
	int home = 0; // where it lives: 0 caves, 1 rotting food, 2 carried by grendels
	std::string name() const; // "a sneezing cold (strain A)"
};

// One illness a creature has now.
struct Illness {
	int strain = 0;
	unsigned int since = 0; // world tick it was caught
};

// at each world load, before the save is read
void reset();
// a world tick (every 50 ticks it does its work)
void tick();
// the creature sneezes (SNEZ): creatures in front of it may catch its illnesses
void sneeze(Agent* creature);

// a creature ate rotten fruit: it may catch what lives in rotting food
void ateRotten(Agent* creature);

// for the kits and the mind
bool isIll(const std::string& moniker);
std::vector<Illness> illnessesOf(const std::string& moniker);
const Strain* strain(int id);
const std::vector<Strain>& strains();
// a grendel's carried strain (0: none)
int carriedBy(const std::string& moniker);
// how much of a strain's germs is in a room (0-255)
float germsIn(int room, int strain);
// a creature died: what it was ill with, in words ("" if nothing)
std::string illnessWords(const std::string& moniker);

// the world save (WorldSave): lines starting "disease"
void save(std::ostream& out);
bool readLine(const std::string& key, std::istringstream& line);

} // namespace Disease
