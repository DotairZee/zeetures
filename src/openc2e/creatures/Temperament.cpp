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
#include "common/devenv.h"
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

namespace {
// what each kind of experience does to each trait (boldness, curiosity,
// energy, sociability, temper, reactivity, steadiness), and how strongly
// the kind counts (the evidence: hurt and comfort fully, fights less,
// exploring least)
const float effect[NUM_EXPERIENCES][NUM_TRAITS] = {
	{-0.7f, 0.0f, 0.0f, -0.5f, 0.0f, 1.0f, 0.0f}, // hurt
	{0.6f, 0.0f, 0.0f, 0.6f, 0.0f, -1.0f, 0.0f}, // comfort
	{0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f}, // fights
	{0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}, // exploring (signed)
};
const float weight[NUM_EXPERIENCES] = {1.0f, 1.0f, 0.5f, 0.35f};
const float STEP = 0.003f; // one full-size experience, before stage, weight and saturation (0.03 moved babies near the cap in minutes)
const float KIND_CAP = 0.5f; // each kind's shift of a trait, at most
const float TOTAL_CAP = 1.0f; // all kinds together
const float FIGHT_LASTING = 0.3f; // the share of a fight's shift that stays
const float FIGHT_FADE = 0.9995f; // the rest, per mind tick (half-life about 1400 mind ticks)
} // namespace

float plasticity(int stage) {
	static const float p[7] = {1.0f, 0.6f, 0.25f, 0.0f, 0.0f, 0.0f, 0.0f};
	return stage >= 0 && stage < 7 ? p[stage] : 0.0f;
}

bool experience(TemperamentDrift& d, int kind, float size, int stage) {
	if (kind < 0 || kind >= NUM_EXPERIENCES)
		return false;
	float step = STEP * weight[kind] * plasticity(stage) * std::clamp(size, -2.0f, 2.0f);
	if (step == 0.0f)
		return false;
	bool moved = false;
	for (int t = 0; t < NUM_TRAITS; t++) {
		float delta = step * effect[kind][t];
		if (delta == 0.0f)
			continue;
		float& s = d.shift[kind][t];
		// harder to move further the more it has moved that way already
		float room = 1.0f - std::fabs(s) / KIND_CAP * ((s > 0) == (delta > 0) ? 1.0f : 0.0f);
		delta *= std::max(0.0f, room);
		if (kind == EXPERIENCE_FIGHT) {
			s += delta * FIGHT_LASTING;
			d.fightRecent = std::clamp(d.fightRecent + delta * (1.0f - FIGHT_LASTING), -KIND_CAP, KIND_CAP);
		} else
			s += delta;
		s = std::clamp(s, -KIND_CAP, KIND_CAP);
		moved |= delta != 0.0f;
	}
	return moved;
}

void fadeFights(TemperamentDrift& d) {
	d.fightRecent *= FIGHT_FADE;
	if (std::fabs(d.fightRecent) < 1e-4f)
		d.fightRecent = 0.0f;
}

Temperament withDrift(const Temperament& born, const TemperamentDrift& d) {
	Temperament t = born;
	for (int i = 0; i < NUM_TRAITS; i++) {
		float total = 0.0f;
		for (int k = 0; k < NUM_EXPERIENCES; k++)
			total += d.shift[k][i];
		if (i == TEMPER)
			total += d.fightRecent;
		t.z[i] = std::clamp(born.z[i] + std::clamp(total, -TOTAL_CAP, TOTAL_CAP), -2.0f, 2.0f);
	}
	return t;
}

std::string driftLine(const TemperamentDrift& d) {
	std::string s;
	for (int k = 0; k < NUM_EXPERIENCES; k++)
		for (int i = 0; i < NUM_TRAITS; i++)
			s += fmt::format("{}{:.4f}", s.empty() ? "" : " ", d.shift[k][i]);
	return s + fmt::format(" {:.4f}", d.fightRecent);
}

bool driftFromLine(const std::string& line, TemperamentDrift& d) {
	std::istringstream l(line);
	TemperamentDrift r;
	for (int k = 0; k < NUM_EXPERIENCES; k++)
		for (int i = 0; i < NUM_TRAITS; i++)
			if (!(l >> r.shift[k][i]))
				return false;
	l >> r.fightRecent;
	d = r;
	return true;
}

// ---- temperament and learning ----------------------------------------------

namespace {
// k for each channel and trait (boldness, curiosity, energy, sociability,
// temper, reactivity, steadiness): the proposal's moderate values
const float LEARNING_K[NUM_LEARNING_CHANNELS][NUM_TRAITS] = {
	{0.20f, 0.20f, 0.15f, 0.0f, 0.0f, 0.0f, 0.0f}, // new things
	{0.0f, 0.0f, 0.0f, -0.20f, 0.0f, 0.0f, -0.15f}, // own doing
	{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.30f, 0.0f}, // danger
	{0.0f, 0.20f, 0.15f, 0.0f, 0.0f, 0.0f, 0.0f}, // places
	{0.0f, 0.0f, 0.0f, 0.25f, 0.0f, 0.0f, 0.0f}, // watching
	{0.20f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}, // from strangers
	{-0.15f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}, // from family
	{0.0f, 0.30f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}, // new words
	{0.0f, 0.0f, 0.0f, 0.25f, 0.0f, 0.0f, 0.0f}, // words from creatures
	{0.0f, 0.0f, 0.0f, -0.20f, 0.0f, 0.0f, 0.0f}, // words from machines
	{0.0f, 0.0f, 0.15f, 0.0f, 0.0f, 0.0f, 0.0f}, // verbs
	{0.0f, 0.0f, -0.15f, 0.0f, 0.0f, 0.0f, 0.0f}, // bursts
	{0.10f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}, // saying
	{0.0f, 0.0f, 0.0f, 0.25f, 0.0f, 0.0f, 0.0f}, // colony words
	{0.0f, 0.20f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}, // habituation
	{0.25f, 0.0f, 0.0f, 0.0f, 0.0f, -0.30f, 0.0f}, // fear fades
	{-0.20f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -0.20f}, // reversal
	{0.0f, 0.0f, 0.0f, 0.0f, -0.25f, 0.0f, 0.0f}, // forgiving
	{0.0f, 0.0f, 0.0f, 0.0f, 0.30f, 0.0f, 0.0f}, // grudges
	{0.0f, 0.0f, 0.0f, 0.0f, -0.30f, 0.0f, 0.0f}, // slaps
	{0.0f, 0.0f, 0.0f, 0.0f, 0.20f, 0.0f, 0.0f}, // fights
	{0.0f, 0.25f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}, // reserve from novelty
	{0.0f, 0.0f, 0.0f, 0.20f, 0.0f, 0.0f, 0.0f}, // growth from turns
	{0.0f, 0.0f, -0.10f, 0.0f, 0.0f, 0.0f, 0.20f}, // settling
	{0.0f, -0.15f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}, // settling, familiar
	{0.0f, 0.0f, -0.20f, 0.0f, 0.0f, 0.0f, 0.0f}, // sleep settling
	{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.20f, 0.0f}, // shaped by hurt
	{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.20f, 0.0f}, // shaped by comfort
};
} // namespace

bool learningOn() {
	static const bool on = [] {
		const char* e = devenv("OPENC2E_TEMPERAMENT_LEARNING");
		return !(e && std::string(e) == "0");
	}();
	return on;
}

float learningK(int channel, int trait) {
	if (channel < 0 || channel >= NUM_LEARNING_CHANNELS || trait < 0 || trait >= NUM_TRAITS)
		return 0.0f;
	return LEARNING_K[channel][trait];
}

float learningFactor(const std::array<float, NUM_TRAITS>& z, int channel) {
	if (!learningOn() || channel < 0 || channel >= NUM_LEARNING_CHANNELS)
		return 1.0f;
	float sum = 0.0f;
	for (int t = 0; t < NUM_TRAITS; t++)
		sum += LEARNING_K[channel][t] * std::clamp(z[t], -2.0f, 2.0f);
	return std::clamp(std::exp(sum), 0.5f, 2.0f);
}

float stressLearning(float reactivityZ, float stress) {
	if (!learningOn())
		return 1.0f;
	// the peak: stress 0.3 for an average creature, lower the more reactive
	float peak = 0.3f * std::exp(-0.3f * std::clamp(reactivityZ, -2.0f, 2.0f));
	float x = std::max(0.0f, stress) / peak;
	return std::clamp(1.0f + 0.08f * x * (2.0f - x), 0.8f, 1.08f);
}

std::string learningLine(const Temperament& t) {
	if (!learningOn())
		return "";
	// what the player is told, for the channels a player can see at work:
	// the words for a high factor, then a low one (British in the game)
	struct Telling {
		int channel;
		const char* high;
		const char* low;
	};
	static const Telling tellings[] = {
		{LEARN_NEW_THINGS, "picks up new things quickly", "is slow to pick up new things"},
		{LEARN_DANGER, "learns danger quickly", "is slow to learn what is dangerous"},
		{LEARN_FEAR_FADES, "lets go of a fear quickly", "lets go of a fear slowly"},
		{LEARN_REVERSAL, "relearns readily when things change", "is slow to relearn when things change"},
		{LEARN_PLACES, "learns where food is quickly", "is slow to learn where food is"},
		{LEARN_WATCHING, "learns a lot by watching others", "learns little by watching others"},
		{LEARN_WORDS_NEW, "learns new words quickly", "is slow to learn new words"},
		{LEARN_WORDS_CREATURES, "learns words best from other creatures", "learns few words from other creatures"},
		{LEARN_FROM_FAMILY, "learns words best from family", "learns words from strangers as readily as from family"},
		{LEARN_WORDS_MACHINES, "learns words well from machines", "learns few words from machines"},
		{LEARN_GRUDGES, "holds a grudge", "rarely holds a grudge"},
		{LEARN_FROM_SLAPS, "learns from a slap", "grows cross rather than careful when slapped"},
		{LEARN_SETTLING, "keeps what it learns", "forgets what it learns sooner"},
		{LEARN_HABITUATION, "tires of the familiar quickly", "is content with the familiar"},
	};
	std::vector<std::pair<float, std::string>> found;
	for (auto& tl : tellings) {
		float f = learningFactor(t, tl.channel);
		float strength = std::fabs(std::log(f));
		if (strength >= 0.15f) // (about 16% faster or slower)
			found.push_back({strength, f > 1.0f ? tl.high : tl.low});
	}
	std::stable_sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
	if (found.empty())
		return "";
	std::string line = found[0].second;
	if (found.size() > 1)
		line += " and " + found[1].second;
	return line;
}

} // namespace Temperaments

bool TemperamentDrift::any() const {
	for (auto& k : shift)
		for (float f : k)
			if (f != 0.0f)
				return true;
	return fightRecent != 0.0f;
}

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
	static const bool off = devenv("OPENC2E_NO_SEASONAL_BEHAVIOR") != nullptr;
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
