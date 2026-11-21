/*
 *  MindGenes.cpp
 *  openc2e
 *
 *  The mind's own heritable settings. See MindGenes.h.
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

#include "MindGenes.h"

#include "PathResolver.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fmt/core.h>
#include <fstream>
#include <random>
#include <sstream>

namespace MindGeneSet {

namespace {

const char* names[NUM_MIND_GENES] = {"learning", "decisiveness", "patience", "wanderlust", "danger", "food", "water", "company"};
const char* lows[NUM_MIND_GENES] = {"slow to learn", "hesitant", "impatient", "homebody", "slow to learn danger", "slow to learn food",
	"slow to learn water", "slow to learn company"};
const char* highs[NUM_MIND_GENES] = {"quick to learn", "decisive", "patient", "wanderer", "quick to learn danger", "quick to learn food",
	"quick to learn water", "quick to learn company"};

std::mt19937& rng() {
	static std::mt19937 g{std::random_device{}()};
	return g;
}

} // namespace

const char* name(int g) {
	return g >= 0 && g < NUM_MIND_GENES ? names[g] : "?";
}
const char* lowWord(int g) {
	return g >= 0 && g < NUM_MIND_GENES ? lows[g] : "?";
}
const char* highWord(int g) {
	return g >= 0 && g < NUM_MIND_GENES ? highs[g] : "?";
}

MindGenes roll() {
	std::normal_distribution<float> n(0.0f, 1.0f);
	MindGenes m;
	for (auto& v : m.z)
		v = std::clamp(n(rng()), -2.0f, 2.0f);
	return m;
}

MindGenes child(const MindGenes& mum, const MindGenes& dad) {
	// as temperament (Temperaments::child): 0.4 of the parents' average,
	// chance making up the rest of the variance
	const float h2 = 0.4f;
	const float chance = std::sqrt(1.0f - h2 * h2 / 2.0f);
	std::normal_distribution<float> n(0.0f, 1.0f);
	MindGenes m;
	for (int i = 0; i < NUM_MIND_GENES; i++)
		m.z[i] = std::clamp(h2 * (mum.z[i] + dad.z[i]) / 2.0f + chance * n(rng()), -2.0f, 2.0f);
	return m;
}

static std::filesystem::path fileFor(const std::string& genomeName) {
	return getUserDataDir() / "Genetics" / (genomeName + ".mind");
}

bool load(const std::string& genomeName, MindGenes& m) {
	if (genomeName.empty())
		return false;
	std::ifstream in(fileFor(genomeName));
	std::string line;
	return std::getline(in, line) && fromLine(line, m);
}

void store(const std::string& genomeName, const MindGenes& m) {
	if (genomeName.empty())
		return;
	std::ofstream out(fileFor(genomeName), std::ios::binary);
	out << toLine(m) << "\n";
}

std::string toLine(const MindGenes& m) {
	std::string s;
	for (int i = 0; i < NUM_MIND_GENES; i++)
		s += fmt::format("{}{:.3f}", i ? " " : "", m.z[i]);
	return s;
}

bool fromLine(const std::string& line, MindGenes& m) {
	std::istringstream in(line);
	MindGenes r;
	for (auto& v : r.z)
		if (!(in >> v))
			return false;
	for (auto& v : r.z)
		v = std::clamp(v, -2.0f, 2.0f);
	m = r;
	return true;
}

} // namespace MindGeneSet
