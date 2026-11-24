/*
 *  BrainProfiles.cpp
 *  openc2e
 *
 *  Roles for C2 genomes' brain add-ons. See BrainProfiles.h.
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

#include "BrainProfiles.h"

#include "Additions.h"
#include "fileformats/genomeFile.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <fmt/core.h>
#include <map>
#include <set>
#include <sstream>

namespace BrainProfiles {

namespace {

// the game's lobes, in the C2 CAOS guide's order (lobe genes 0-9)
const int STANDARD_LOBES = 10;
const int DRIVE_LOBE = 1, DECISION_LOBE = 6, ATTENTION_LOBE = 7, CONCEPT_LOBE = 8;
const float GAME_CONCEPT_CELLS = 640.0f; // the game's norn, grendel and ettin

struct Role {
	std::string what;
	std::vector<std::pair<std::string, float>> settings;
};
struct KnownLobe {
	int x, y, w, h;
	std::string role;
};
struct Known {
	std::string name, by;
	std::vector<KnownLobe> lobes;
	std::vector<std::pair<std::string, float>> settings;
};
struct Table {
	std::map<std::string, Role> roles;
	std::vector<Known> genomes;
	std::vector<std::string> memorySettings;
};

std::string trim(const std::string& s) {
	size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
	return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

std::vector<std::string> split(const std::string& s, char sep) {
	std::vector<std::string> out;
	std::string part;
	std::istringstream in(s);
	while (std::getline(in, part, sep))
		out.push_back(trim(part));
	return out;
}

std::vector<std::pair<std::string, float>> settingsFrom(const std::string& s) {
	std::vector<std::pair<std::string, float>> out;
	std::istringstream in(s);
	std::string name;
	float f;
	while (in >> name >> f)
		out.push_back({name, f});
	return out;
}

// brain-profiles.txt:
//   role NAME | what it does | setting factor setting factor ...
//   memory setting setting ...
//   genome NAME | BY | x y w h ROLE ; x y w h ROLE ... | setting factor ...
const Table& table() {
	static Table t;
	static bool read = false;
	if (read)
		return t;
	read = true;
	std::istringstream in(Additions::text("brain-profiles.txt"));
	std::string line;
	while (std::getline(in, line)) {
		size_t hash = line.find('#');
		if (hash != std::string::npos)
			line = line.substr(0, hash);
		line = trim(line);
		if (line.empty())
			continue;
		std::string first = line.substr(0, line.find(' '));
		std::string rest = trim(line.substr(first.size()));
		if (first == "role") {
			auto p = split(rest, '|');
			if (p.size() >= 2)
				t.roles[p[0]] = Role{p[1], p.size() > 2 ? settingsFrom(p[2]) : decltype(Role::settings){}};
		} else if (first == "memory") {
			std::istringstream l(rest);
			std::string n;
			while (l >> n)
				t.memorySettings.push_back(n);
		} else if (first == "genome") {
			auto p = split(rest, '|');
			if (p.size() < 3)
				continue;
			Known k{p[0], p[1], {}, {}};
			for (auto& l : split(p[2], ';')) {
				std::istringstream ls(l);
				KnownLobe kl;
				if (ls >> kl.x >> kl.y >> kl.w >> kl.h >> kl.role)
					k.lobes.push_back(kl);
			}
			if (p.size() > 3)
				k.settings = settingsFrom(p[3]);
			t.genomes.push_back(k);
		}
	}
	return t;
}

} // namespace

bool laterLobe(const genomeFile& genome, const gene* g) {
	if (!g || g->header.generation == 0)
		return false;
	for (auto& e : genome.genes) {
		if (e.get() == g)
			break;
		if (dynamic_cast<oldBrainLobeGene*>(e.get()) && e->note.which == g->note.which && e->header.generation < g->header.generation)
			return true;
	}
	return false;
}

Result read(const genomeFile& genome) {
	const Table& t = table();
	Result r;
	std::vector<oldBrainLobeGene*> lobes;
	std::vector<bioReceptorGene*> brainReceptors;
	// chemicals made as needs change: products of reactions that use a drive
	// chemical or its increase or decrease chemical (C2: drives 1-17, their
	// increases 18-34, decreases 35-51), and what emitters reading the drive
	// levels make (Canny's echo chemicals 200-216 are made each from a
	// drive and its decrease, as the drive eases)
	std::set<int> driveMade;
	auto isDriveChem = [](int c) { return c >= 1 && c <= 51; };
	// (the lobes in the brain's order: duplicated lobes after the rest, as
	// oldBrain places them)
	for (int pass = 0; pass < 2; pass++)
		for (auto& g : genome.genes)
			if (auto* l = dynamic_cast<oldBrainLobeGene*>(g.get()); l && laterLobe(genome, l) == (pass == 1))
				lobes.push_back(l);
	for (auto& g : genome.genes) {
		if (dynamic_cast<oldBrainLobeGene*>(g.get()))
			continue;
		else if (auto* rc = dynamic_cast<bioReceptorGene*>(g.get()); rc && rc->organ == 0)
			brainReceptors.push_back(rc);
		else if (auto* re = dynamic_cast<bioReactionGene*>(g.get())) {
			if (isDriveChem(re->reactant[0]) || isDriveChem(re->reactant[1]))
				for (int k = 2; k < 4; k++)
					if (re->reactant[k] != 0 && !isDriveChem(re->reactant[k]))
						driveMade.insert(re->reactant[k]);
		} else if (auto* em = dynamic_cast<bioEmitterGene*>(g.get()); em && em->organ == 1 && em->tissue == 5)
			driveMade.insert(em->chemical);
	}
	if (lobes.size() > (size_t)CONCEPT_LOBE)
		r.memory = std::max(1.0f, lobes[CONCEPT_LOBE]->width * lobes[CONCEPT_LOBE]->height / GAME_CONCEPT_CELLS);

	auto reads = [&](size_t i, int src) {
		const auto* l = lobes[i];
		return (l->dendrite1.max > 0 && l->dendrite1.srclobe == src) || (l->dendrite2.max > 0 && l->dendrite2.srclobe == src);
	};
	// whether lobe i's output reaches lobe target, directly or through other add-ons
	std::function<bool(size_t, int, int)> reaches = [&](size_t i, int target, int depth) {
		for (size_t j = 0; j < lobes.size(); j++)
			if (reads(j, (int)i) && ((int)j == target || (j >= (size_t)STANDARD_LOBES && depth < 4 && reaches(j, target, depth + 1))))
				return true;
		return false;
	};
	std::function<bool(size_t, int)> readsDrives = [&](size_t i, int depth) {
		if (reads(i, DRIVE_LOBE))
			return true;
		for (size_t j = STANDARD_LOBES; j < lobes.size(); j++)
			if (j != i && reads(i, (int)j) && depth < 4 && readsDrives(j, depth + 1))
				return true;
		return false;
	};

	// known genomes whose every add-on lobe this genome has
	std::set<size_t> whole;
	for (size_t k = 0; k < t.genomes.size(); k++) {
		const Known& kn = t.genomes[k];
		bool all = !kn.lobes.empty();
		for (auto& kl : kn.lobes) {
			bool found = false;
			for (size_t i = STANDARD_LOBES; i < lobes.size(); i++)
				found |= lobes[i]->x == kl.x && lobes[i]->y == kl.y && lobes[i]->width == kl.w && lobes[i]->height == kl.h;
			all &= found;
		}
		if (all) {
			whole.insert(k);
			r.genomes.push_back(kn.by.empty() ? kn.name : fmt::format("{}, by {}", kn.name, kn.by));
		}
	}

	for (size_t i = STANDARD_LOBES; i < lobes.size(); i++) {
		const auto* l = lobes[i];
		AddOn a;
		a.lobe = (int)i;
		// named by a known genome (any, so a hybrid's inherited lobes keep theirs)
		for (auto& kn : t.genomes)
			for (auto& kl : kn.lobes)
				if (a.role.empty() && l->x == kl.x && l->y == kl.y && l->width == kl.w && l->height == kl.h) {
					a.role = kl.role;
					a.fromProfile = true;
					a.why = kn.name;
				}
		if (a.role.empty()) {
			// from its wiring
			int slot0 = -1, slot1 = -1;
			std::set<int> feeding; // chemicals its cells take in
			for (auto* rc : brainReceptors) {
				if (rc->tissue != i)
					continue;
				if (rc->locus == 13)
					slot0 = rc->chemical;
				else if (rc->locus == 14)
					slot1 = rc->chemical;
				else if (rc->locus >= 19)
					feeding.insert(rc->chemical);
			}
			size_t fromDrives = std::count_if(feeding.begin(), feeding.end(), [&](int c) { return driveMade.count(c) > 0; });
			bool toAttention = reaches(i, ATTENTION_LOBE, 0), toDecision = reaches(i, DECISION_LOBE, 0);
			// (its output also reaches the brain through the perception lobe
			// when its gene's perception flag is set)
			bool toPerception = l->perceptflag != 0;
			if (toAttention && reads(i, DRIVE_LOBE)) {
				a.role = "search";
				a.why = "reads the drives and feeds attention: what it needs, then what to look for";
			} else if (toAttention) {
				bool punished = slot1 >= 0 && slot0 < 0;
				a.role = punished ? "object-bad" : "object-good";
				a.why = punished ? "feeds attention, taught by punishment" : "feeds attention";
			} else if (toDecision) {
				a.role = "choice";
				a.why = "feeds the decision lobe";
			} else if (readsDrives(i, 0)) {
				a.role = "drive-change";
				a.why = "reads the drives";
			} else if (!feeding.empty() && fromDrives * 2 >= feeding.size()) {
				a.role = "drive-change";
				a.why = fmt::format("fed by chemicals made as its needs change ({} of {}){}", fromDrives, feeding.size(), toPerception ? ", into perception" : "");
			} else if (!feeding.empty())
				a.why = toPerception ? "fed by chemicals, into perception" : "a chemical relay that nothing reads";
			else
				a.why = toPerception ? "goes into perception only" : "nothing reads it";
		}
		if (!a.role.empty()) {
			auto it = t.roles.find(a.role);
			if (it == t.roles.end()) {
				a.why += fmt::format(" (role \"{}\" is not in brain-profiles.txt)", a.role);
				a.role.clear();
			} else
				a.what = it->second.what;
		}
		r.addOns.push_back(a);
	}

	// settings: each role once, then the known genomes' own, then memory
	std::set<std::string> roles;
	for (auto& a : r.addOns)
		if (!a.role.empty() && roles.insert(a.role).second)
			for (auto& s : t.roles.at(a.role).settings)
				r.settings.push_back(s);
	for (size_t k : whole)
		for (auto& s : t.genomes[k].settings)
			r.settings.push_back(s);
	if (r.memory > 1.05f)
		for (auto& n : t.memorySettings)
			r.settings.push_back({n, std::min(2.0f, std::sqrt(r.memory))});
	return r;
}

std::vector<std::string> describe(const Result& r) {
	std::vector<std::string> out;
	for (auto& g : r.genomes)
		out.push_back(fmt::format("Known brain: {}", g));
	for (auto& a : r.addOns) {
		if (!a.role.empty())
			out.push_back(fmt::format("Added lobe {}: {} ({}{})", a.lobe, a.what, a.fromProfile ? "as in " : "", a.why));
		else
			out.push_back(fmt::format("Added lobe {}: no counterpart in Zeetures's mind ({})", a.lobe, a.why));
	}
	if (r.memory > 1.05f)
		out.push_back(fmt::format("Concept lobe {:.1f}x the game's: more memory", r.memory));
	return out;
}

} // namespace BrainProfiles
