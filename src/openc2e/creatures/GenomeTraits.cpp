/*
 *  GenomeTraits.cpp
 *  openc2e
 *
 *  Part of a creature's temperament read from its C2 genes. See
 *  GenomeTraits.h.
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

#include "GenomeTraits.h"
#include "common/devenv.h"

#include "PathResolver.h"
#include "World.h"
#include "fileformats/genomeFile.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fmt/core.h>
#include <map>
#include <memory>
#include <vector>

namespace GenomeTraits {

namespace {

const int NUM_DRIVES = 17;
const int DRIVE_CHEM = 148, RAISE_CHEM = 18;
// a trait moves this much per unit of log gain (a doubling, ln 2 = 0.69,
// is about half a step), at most this far either way
const float K = 0.7f, MAX_SHIFT = 1.5f;

// one drive's chemistry in a genome
struct DriveChem {
	float stimulus = 0, emitted = 0, received = 0;
	int halflife = -1;
};

template <typename F>
void eachGene(const genomeFile& g, F f) {
	// (biochemistry genes follow their organ gene in the list: all top level)
	for (auto& gp : g.genes)
		f(gp.get());
}

std::array<DriveChem, NUM_DRIVES> readChem(const genomeFile& g) {
	std::array<DriveChem, NUM_DRIVES> c{};
	eachGene(g, [&](gene* gn) {
		if (auto* s = dynamic_cast<creatureStimulusGene*>(gn)) {
			for (int i = 0; i < 4; i++) {
				int d = (int)s->drives[i] - RAISE_CHEM;
				if (d >= 0 && d < NUM_DRIVES)
					c[d].stimulus += s->amounts[i];
			}
		} else if (auto* e = dynamic_cast<bioEmitterGene*>(gn)) {
			int d = (int)e->chemical - DRIVE_CHEM;
			if (!(d >= 0 && d < NUM_DRIVES))
				d = (int)e->chemical - RAISE_CHEM;
			if (d >= 0 && d < NUM_DRIVES)
				c[d].emitted += e->gain;
		} else if (auto* r = dynamic_cast<bioReceptorGene*>(gn)) {
			// a receptor setting the drive locus (creature, drive levels)
			if (r->organ == 1 && r->tissue == 5 && r->locus < NUM_DRIVES)
				c[r->locus].received += r->gain;
		} else if (auto* h = dynamic_cast<bioHalfLivesGene*>(gn)) {
			for (int d = 0; d < NUM_DRIVES; d++)
				c[d].halflife = h->halflives[DRIVE_CHEM + d];
		}
	});
	return c;
}

int genusOf(const genomeFile& g) {
	int genus = -1;
	eachGene(g, [&](gene* gn) {
		if (auto* gg = dynamic_cast<creatureGenusGene*>(gn))
			if (genus < 0)
				genus = gg->genus;
	});
	return genus;
}

// the species' standard genome (the game's own, or the player's copy)
const genomeFile* standardFor(int genus, std::string* name) {
	static std::map<int, std::shared_ptr<genomeFile>> cache;
	static const char* names[3] = {"norn", "gren", "ettn"};
	if (genus < 0 || genus > 2)
		return nullptr;
	if (name)
		*name = names[genus];
	auto it = cache.find(genus);
	if (it == cache.end()) {
		std::shared_ptr<genomeFile> g;
		try {
			g = world.loadGenome(names[genus]);
		} catch (std::exception&) {
		}
		it = cache.emplace(genus, g).first;
	}
	return it->second.get();
}

float logRatio(float a, float b) {
	return std::log((a + 1.0f) / (b + 1.0f));
}

} // namespace

std::array<float, 17> driveGains(const genomeFile& g, std::string* species) {
	std::array<float, 17> gain{};
	const genomeFile* ref = standardFor(genusOf(g), species);
	if (!ref)
		return gain;
	auto mine = readChem(g), theirs = readChem(*ref);
	for (int d = 0; d < NUM_DRIVES; d++) {
		float v = logRatio(mine[d].stimulus, theirs[d].stimulus) + logRatio(mine[d].emitted, theirs[d].emitted) +
				  logRatio(mine[d].received, theirs[d].received);
		if (mine[d].halflife >= 0 && theirs[d].halflife >= 0)
			v += (mine[d].halflife - theirs[d].halflife) * std::log(2.0f) / 8.0f;
		gain[d] = v;
	}
	return gain;
}

std::array<float, 7> shift(const genomeFile& g) {
	std::array<float, 7> s{};
	static const bool off = devenv("OPENC2E_NO_GENOME_TRAITS") != nullptr;
	if (off)
		return s;
	auto gain = driveGains(g);
	enum { TIRED = 5, SLEEPY = 6, LONELY = 7, CROWDED = 8, FEAR = 9, BORED = 10, ANGER = 11, STRESS = 16 };
	s[0] = -K * gain[FEAR];
	s[1] = K * gain[BORED];
	s[2] = -K * 0.5f * (gain[TIRED] + gain[SLEEPY]);
	s[3] = K * (gain[LONELY] - 0.5f * gain[CROWDED]);
	s[4] = K * gain[ANGER];
	s[5] = K * gain[STRESS];
	for (auto& v : s)
		v = std::clamp(v, -MAX_SHIFT, MAX_SHIFT);
	return s;
}

void dumpIfAsked() {
	if (!devenv("OPENC2E_DUMP_GENE_TRAITS"))
		return;
	std::vector<std::array<float, 7>> all;
	int nonzero = 0;
	for (auto& path : findGeneticsFiles("*.gen")) {
		std::shared_ptr<genomeFile> g;
		try {
			g = world.loadGenome(path.stem().string());
		} catch (std::exception&) {
			continue;
		}
		if (!g)
			continue;
		std::string species;
		auto gain = driveGains(*g, &species);
		auto s = shift(*g);
		bool any = false;
		for (float v : s)
			any = any || std::fabs(v) > 0.01f;
		all.push_back(s);
		if (any) {
			nonzero++;
			fmt::print("* GENE TRAITS {} ({}): fear {:+.2f} bored {:+.2f} tired {:+.2f} sleepy {:+.2f} lonely {:+.2f} crowded {:+.2f} anger {:+.2f} stress {:+.2f} -> bold {:+.2f} curious {:+.2f} energy {:+.2f} social {:+.2f} temper {:+.2f} reactive {:+.2f}\n",
				path.stem().string(), species, gain[9], gain[10], gain[5], gain[6], gain[7], gain[8], gain[11], gain[16], s[0], s[1], s[2], s[3], s[4], s[5]);
		}
	}
	fmt::print("* GENE TRAITS: {} genomes, {} differ from their species' standard\n", all.size(), nonzero);
	for (int t = 0; t < 6; t++) {
		std::vector<float> v;
		for (auto& s : all)
			v.push_back(s[t]);
		if (v.empty())
			continue;
		std::sort(v.begin(), v.end());
		fmt::print("* GENE TRAITS trait {}: min {:+.2f} 5% {:+.2f} median {:+.2f} 95% {:+.2f} max {:+.2f}\n", t, v.front(), v[v.size() / 20],
			v[v.size() / 2], v[v.size() * 19 / 20], v.back());
	}
}

} // namespace GenomeTraits
