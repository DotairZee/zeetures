/*
 *  Exposure.h
 *  openc2e
 *
 *  What radiation and heavy metals do to C2 creatures beyond the genome's own
 *  chemistry (owner, 2026-11-21: "make them matter"; "slow but serious, as
 *  pain"; "radiation should also affect fertility and mutation in offspring
 *  while in utero and in an egg"). Our own design.
 *
 *  - Heavy metals (chemical 140: made by every standard genome from
 *    radiation over 48, and taken in with some poisoned foods; they barely
 *    decay, and EDTA, 157, is all that removes them): above 16 they are
 *    felt as pain growing with the level, and they wear down the organ the
 *    genome marks for them (its heavy-metals receptor) in proportion, faster
 *    than it repairs, so a creature left at a high level for days can die.
 *  - Radiation dose: the radiation a creature has stood in adds to its dose,
 *    which fades by half in about a game day. A high dose lowers the chance
 *    of conceiving (Breeding.cpp).
 *  - Before birth: a baby's genome (by name) gathers the radiation around
 *    it, in its mother while she carries it and in its egg after; at
 *    hatching each share of that dose is one more mutation in its genome,
 *    kept in its genome file, so its own children inherit them.
 *
 *  Switch OPENC2E_NO_EXPOSURE; OPENC2E_WATCH_EXPOSURE logs.
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
#include <memory>
#include <sstream>
#include <string>

class genomeFile;
class oldCreature;

namespace Exposure {

void reset(); // at each world load, before the save is read
void tick(); // a world tick (every 50 ticks it does its work)

// a creature's radiation dose now (radiation-minutes, fading), and the share
// of its usual chance to conceive it leaves (1 none, toward 0 when high)
float doseOf(const std::string& moniker);
float fertilityFactor(const std::string& moniker);
// the dose a baby's genome has gathered before birth
float unbornDose(const std::string& genome);

// at hatching (NEW: CREA): the genome with the mutations its dose before birth
// gave it (written back to its genome file), or the same genome if none
std::shared_ptr<genomeFile> atHatching(const std::string& genome, std::shared_ptr<genomeFile> g);

void save(std::ostream& out);
bool readLine(const std::string& key, std::istringstream& line);

} // namespace Exposure
