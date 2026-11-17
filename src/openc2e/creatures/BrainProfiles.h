/*
 *  BrainProfiles.h
 *  openc2e
 *
 *  C2 genomes with brain add-ons (owner, 2026-11-17: people should be able
 *  to bring in old custom genomes and have them "function approximately as
 *  intended"; genome-translation-options.md, option 3). Custom genomes such
 *  as Lummox JR's Nova Subterra and Lis Morris and Chris Double's Canny
 *  Norns add lobes to C2's ten. The original brain is not run (the
 *  project's ground rule), so each add-on lobe is given a role our mind has
 *  settings for: learning which things are worth attending to, or which to
 *  stay clear of; noticing needs easing or worsening; searching for what it
 *  needs; weighing what an action will do. A bigger concept lobe is more
 *  memory. Each role multiplies some of the mind's settings.
 *
 *  Roles and known genomes are listed in brain-profiles.txt (Zeetures's
 *  built-in one, or the player's own: Additions.h), so players and pack
 *  makers can read and change them. An add-on lobe no profile names gets a
 *  role from its wiring (what it reads, what reads it, which chemicals
 *  teach it), or none.
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

#include <string>
#include <utility>
#include <vector>

class genomeFile;

namespace BrainProfiles {

struct AddOn {
	int lobe = -1; // its place among the genome's lobe genes (10 and up: added)
	std::string role; // a role from brain-profiles.txt, or "" (none)
	std::string what; // the role's description ("learns which things are worth attending to")
	bool fromProfile = false; // named by a known genome's profile (else read from its wiring)
	std::string why; // how its role was found (wiring), for the Breeder's Kit
};

struct Result {
	std::vector<std::string> genomes; // the known genomes whose add-ons it has ("Nova Subterra, by Lummox JR")
	std::vector<AddOn> addOns;
	float memory = 1.0f; // concept lobe size against the game's (640 cells), for the memory setting
	// the mind's settings, as multipliers (name, factor): every role's once,
	// then the known genomes' own
	std::vector<std::pair<std::string, float>> settings;
	bool any() const { return !addOns.empty() || memory > 1.05f; }
};

Result read(const genomeFile& genome);
// a few plain lines, for the Breeder's Kit and the log
std::vector<std::string> describe(const Result& r);

} // namespace BrainProfiles
