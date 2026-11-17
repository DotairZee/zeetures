/*
 *  Temperament.cpp
 *  openc2e
 *
 *  Inborn temperament. See Temperament.h.
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

#include "Temperament.h"
#include "PathResolver.h"
#include <fstream>
#include <filesystem>

#include <algorithm>
#include <cstdlib>
#include <fmt/core.h>
#include <random>
#include <sstream>
#include <vector>

int Temperament::percent(int t) const {
	// the normal distribution's share below z
	return (int)std::lround(50.0 * (1.0 + std::erf(z[t] / std::sqrt(2.0))));
}

namespace Temperaments {

namespace {

const char* names[NUM_TRAITS] = {"boldness", "curiosity", "energy", "sociability", "temper", "reactivity", "steadiness"};
const char* lows[NUM_TRAITS] = {"shy", "incurious", "placid", "aloof", "easygoing", "steady-nerved", "flighty"};
const char* highs[NUM_TRAITS] = {"bold", "curious", "energetic", "sociable", "quick to anger", "highly strung", "persistent"};

// How much each trait follows one hidden "proactive" factor (bold animals
// tend to be curious and active, a little more aggressive, less reactive).
// Design guesses from the animal-personality literature, not measured values.
const float loading[NUM_TRAITS] = {0.6f, 0.5f, 0.4f, 0.0f, 0.3f, -0.4f, 0.0f};

std::mt19937& rng() {
	static std::mt19937 g{std::random_device{}()};
	return g;
}

} // namespace

const char* traitName(int t) {
	return t >= 0 && t < NUM_TRAITS ? names[t] : "?";
}
const char* lowWord(int t) {
	return t >= 0 && t < NUM_TRAITS ? lows[t] : "?";
}
const char* highWord(int t) {
	return t >= 0 && t < NUM_TRAITS ? highs[t] : "?";
}

// one draw with the traits' links (unclipped)
static std::array<float, NUM_TRAITS> draw() {
	std::normal_distribution<float> n(0.0f, 1.0f);
	std::array<float, NUM_TRAITS> z{};
	float f = n(rng());
	for (int i = 0; i < NUM_TRAITS; i++)
		z[i] = loading[i] * f + std::sqrt(1.0f - loading[i] * loading[i]) * n(rng());
	// sociable creatures run a little less to temper
	z[TEMPER] -= 0.15f * z[SOCIABILITY];
	return z;
}

Temperament roll() {
	Temperament t;
	auto z = draw();
	for (int i = 0; i < NUM_TRAITS; i++)
		t.z[i] = std::clamp(z[i], -2.0f, 2.0f);
	return t;
}

Temperament child(const Temperament& mum, const Temperament& dad) {
	// h2 of the parents' average; chance makes up the rest of the variance
	// (the average's variance is 1/2, so h2 * h2 / 2 of it is passed on)
	const float h2 = 0.4f;
	const float chance = std::sqrt(1.0f - h2 * h2 / 2.0f);
	Temperament t;
	auto z = draw();
	for (int i = 0; i < NUM_TRAITS; i++)
		t.z[i] = std::clamp(h2 * (mum.z[i] + dad.z[i]) / 2.0f + chance * z[i], -2.0f, 2.0f);
	return t;
}

static std::filesystem::path fileFor(const std::string& genomeName) {
	return getUserDataDir() / "Genetics" / (genomeName + ".temperament");
}

bool load(const std::string& genomeName, Temperament& t) {
	if (genomeName.empty())
		return false;
	std::ifstream in(fileFor(genomeName));
	std::string line;
	return std::getline(in, line) && fromLine(line, t);
}

void store(const std::string& genomeName, const Temperament& t) {
	if (genomeName.empty())
		return;
	std::ofstream out(fileFor(genomeName), std::ios::binary);
	out << toLine(t) << "\n";
}

std::string describe(const Temperament& t) {
	std::vector<std::pair<float, int>> strong;
	for (int i = 0; i < NUM_TRAITS; i++)
		if (std::fabs(t.z[i]) >= 0.9f)
			strong.push_back({std::fabs(t.z[i]), i});
	if (strong.empty())
		return "even-tempered";
	std::sort(strong.rbegin(), strong.rend());
	auto word = [&](int i) { return std::string(t.z[i] > 0 ? highs[i] : lows[i]); };
	std::string s = word(strong[0].second);
	if (strong.size() >= 2)
		s += (strong.size() == 2 ? " and " : ", ") + word(strong[1].second);
	if (strong.size() >= 3)
		s += " and " + word(strong[2].second);
	return s;
}

std::string toLine(const Temperament& t) {
	std::string s;
	for (int i = 0; i < NUM_TRAITS; i++)
		s += fmt::format("{}{:.3f}", i ? " " : "", t.z[i]);
	return s;
}

bool fromLine(const std::string& line, Temperament& t) {
	std::istringstream l(line);
	Temperament r;
	for (int i = 0; i < NUM_TRAITS; i++)
		if (!(l >> r.z[i]))
			return false;
	for (auto& z : r.z)
		z = std::clamp(z, -2.0f, 2.0f);
	t = r;
	return true;
}

} // namespace Temperaments

namespace SeasonalRhythm {

static std::mt19937& rng() {
	static std::mt19937 g{std::random_device{}()};
	return g;
}

float roll() {
	std::normal_distribution<float> n(0.0f, 1.0f);
	return std::clamp(n(rng()), -2.0f, 2.0f);
}

float child(float mum, float dad) {
	// the parents' average keeps half the population's variance; chance
	// adds the other half back (sd sqrt(1/2)), so the spread holds
	std::normal_distribution<float> n(0.0f, 0.7071f);
	return std::clamp((mum + dad) / 2.0f + n(rng()), -2.0f, 2.0f);
}

int percent(float z) {
	return (int)std::lround(50.0 * (1.0 + std::erf(z / std::sqrt(2.0))));
}

const char* describe(float z) {
	if (z >= 1.0f)
		return "a strong seasonal rhythm: it fattens up in autumn and saves itself in winter";
	if (z >= 0.3f)
		return "a marked seasonal rhythm";
	if (z > -0.3f)
		return "an average seasonal rhythm";
	if (z > -1.0f)
		return "a slight seasonal rhythm";
	return "almost no seasonal rhythm: it lives the same all year";
}

float stageWeight(int effect, int stage) {
	static const float w[NUM_SEASONAL_EFFECTS][7] = {
		{0.0f, 0.0f, 0.6f, 1.0f, 1.0f, 0.6f, 0.0f}, // breeding
		{1.5f, 1.3f, 1.0f, 0.8f, 0.8f, 1.2f, 1.5f}, // bonds
		{1.2f, 1.0f, 0.8f, 0.8f, 1.0f, 1.3f, 1.5f}, // rest
		{1.5f, 1.5f, 1.2f, 1.0f, 0.7f, 0.5f, 0.3f}, // play
	};
	if (effect < 0 || effect >= NUM_SEASONAL_EFFECTS)
		return 0.0f;
	return w[effect][std::clamp(stage, 0, 6)];
}

float fertilityFactor(int season, float mumZ, int mumStage, float dadZ, int dadStage) {
	if (!behaviorOn() || season < 0 || season > 3)
		return 1.0f;
	// spring, summer, autumn, winter
	static const float swing[4] = {0.6f, 0.1f, -0.3f, -0.6f};
	float s = std::min(2.0f, (strength(mumZ) + strength(dadZ)) / 2.0f);
	float w = (stageWeight(SEASON_BREEDING, mumStage) + stageWeight(SEASON_BREEDING, dadStage)) / 2.0f;
	return std::max(0.1f, 1.0f + swing[season] * s * w);
}

bool behaviorOn() {
	static const bool off = getenv("OPENC2E_NO_SEASONAL_BEHAVIOR") != nullptr;
	return !off;
}

static std::filesystem::path fileFor(const std::string& genomeName) {
	return getUserDataDir() / "Genetics" / (genomeName + ".season");
}

bool load(const std::string& genomeName, float& z) {
	if (genomeName.empty())
		return false;
	std::ifstream in(fileFor(genomeName));
	float v;
	if (!(in >> v))
		return false;
	z = std::clamp(v, -2.0f, 2.0f);
	return true;
}

void store(const std::string& genomeName, float z) {
	if (genomeName.empty())
		return;
	std::ofstream out(fileFor(genomeName), std::ios::binary);
	out << fmt::format("{:.3f}\n", z);
}

} // namespace SeasonalRhythm
