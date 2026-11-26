/*
 *  BrainOrgan.h
 *  openc2e
 *
 *  The brain as an organ of the body (owner, 2026-11-26: "I do want to have
 *  our mind attached or associated with it"; "Yes, cost 5"). C2 genomes have
 *  a brain organ gene, and the original game charged it 22 ATP each time it
 *  ran (half as often as the other organs); openc2e never made it, so our
 *  bodies spent about 23 ATP a biochemistry step against the original's 34.
 *  Ours costs 5 (about 2.5 ATP a step, +11%): a brain the size of a
 *  primate's share of the body's energy (about 9%; other mammals 3-5%, an
 *  adult human 20-25%), as the research suggests, not the original's 32%.
 *
 *  - It runs every other biochemistry step and costs the same whatever the
 *    mind is doing (thinking hard adds little to the brain's use), a quarter
 *    less asleep (deep sleep lowers it 20-45%).
 *  - It is paid last, after every other organ, and only while ATP is at 60
 *    or more: it is never what empties the store. Unpaid, or short of
 *    glucose, the creature grows dazed (Impairment, 0-255): it learns more
 *    slowly, chooses less surely, and walks slowly. A baby's brain is hit
 *    hardest (three times an adult's), as the young brain uses the largest
 *    share of energy. Once fed it clears within a few minutes, faster
 *    asleep.
 *
 *  Switch OPENC2E_NO_BRAIN_ORGAN (no cost, never dazed); OPENC2E_WATCH_BRAIN
 *  logs each creature's brain every 600 ticks; OPENC2E_COUNT_ENERGY counts
 *  it as "brain".
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

class Agent;
class Creature;
class c2Creature;

namespace BrainOrgan {

const int energyCost = 5; // ATP each time it runs, awake
const int clockRate = 127; // (shown as an organ's clock: half the speed of the others)

bool off();

// one biochemistry step of a creature, after its other organs
void step(c2Creature& c);

// how dazed it is, 0 (clear) to 1 (as dazed as can be)
float daze(Creature* c);
float dazeOf(Agent* creatureAgent);

void reset(); // at each world load, before the save is read
void save(std::ostream& out);
bool readLine(const std::string& key, std::istringstream& line);

} // namespace BrainOrgan
