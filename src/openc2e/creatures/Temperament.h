/*
 *  Temperament.h
 *  openc2e
 *
 *  C2 under a mind: each creature's inborn temperament (owner, 2026-11-11:
 *  creatures should not start as blank slates). Our own design, from the
 *  research on temperament and animal personality: every creature has some
 *  of every trait, each a continuous score (no types), drawn at hatching
 *  regardless of genome (genetically identical animals still differ, from
 *  developmental chance), with the traits loosely linked as animal
 *  personalities are (bold ones tend to be curious and active).
 *
 *  Scores are z-scores: 0 is average, +1 more than about 84 in 100
 *  creatures, clipped to +-2 so no creature is too extreme to live. Each
 *  trait changes the mind's and body's settings by a factor exp(k z),
 *  leaving the survival needs (pain, hunger, cold, heat, injury,
 *  breathing, thirst) alone.
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

#include <array>
#include <cmath>
#include <string>

// The traits, in order:
//   Boldness     shy ... bold: less fear, more drawn to the new and to strangers
//   Curiosity    incurious ... curious: novelty worth more, boredom bites harder
//   Energy       placid ... energetic: walks faster, grows restless sooner
//   Sociability  aloof ... sociable: loneliness bites harder, crowds less;
//                bonds grow faster
//   Temper       easygoing ... hot-tempered: anger bites harder; harm sours
//                bonds more, fights are forgiven slower
//   Reactivity   steady-nerved ... reactive: moods swing, stress bites harder,
//                bad outcomes are learned faster than good ones
//   Steadiness   flighty ... steady: sticks with what it is doing
enum TemperamentTrait { BOLDNESS, CURIOSITY, ENERGY, SOCIABILITY, TEMPER, REACTIVITY, STEADINESS, NUM_TRAITS };

struct Temperament {
	std::array<float, NUM_TRAITS> z{};

	// a factor exp(k z) for trait t
	float factor(int t, float k) const { return std::exp(k * z[t]); }
	// the share of creatures with less of trait t (50% is average)
	int percent(int t) const;
};

namespace Temperaments {

const char* traitName(int t); // "boldness"
// low and high words for a trait ("shy", "bold")
const char* lowWord(int t);
const char* highWord(int t);

// A new temperament, drawn at random.
Temperament roll();

// A child's temperament, partly passed on (2026-11-17, the TODO's "partial
// inheritance"): heritability 0.4 for every trait, about what twin and
// animal studies find for personality. The child gets 0.4 of its parents'
// average and the rest from chance, drawn with the traits' usual links, so
// a child correlates with each parent about 0.2 and the population's
// spread holds from one generation to the next.
Temperament child(const Temperament& mum, const Temperament& dad);

// The temperament beside a genome (Genetics/NAME.temperament, written at
// conception), if there is one; the game's own eggs and older genomes have
// none and draw one at hatching.
bool load(const std::string& genomeName, Temperament& t);
void store(const std::string& genomeName, const Temperament& t);

// A few words: the one or two strongest traits ("bold and curious, quick
// to anger"), or "even-tempered" when none stands out.
std::string describe(const Temperament& t);

// World saving: "z0 z1 ... z6", and back.
std::string toLine(const Temperament& t);
bool fromLine(const std::string& line, Temperament& t);

} // namespace Temperaments

// Seasonal rhythm (owner, 2026-11-16: should creatures "eat more in autumn
// to get through winter?"; chose that it be inherited): how strongly a
// creature fattens in autumn and saves in winter. A z-score like the
// traits (0 average, clipped to +-2), but passed on rather than drawn at
// hatching: a child's is its parents' average plus chance (as a trait made
// by many genes is, with half the spread of the whole population), kept in
// a small file beside the child's genome in the user data folder's
// Genetics (NAME.season), so a creature made from that genome anywhere has
// it. A creature from a genome with none (the game's own eggs, older
// genomes) draws one at random. Saved with the world.
namespace SeasonalRhythm {

float roll();
float child(float mum, float dad);
// the cycle's strength, exp(0.5 z): about 0.37 to 2.7, 1 on average
inline float strength(float z) { return std::exp(0.5f * z); }
int percent(float z); // the share of creatures with less (50% average)
const char* describe(float z); // "a strong seasonal rhythm", ...
// Seasonal behavior (owner, 2026-11-16: a spring breeding season, winter
// bonds, winter rest and summer play, "life stage as a mediating
// factor"): how much each effect applies at each life stage (0 baby,
// 1 child, 2 adolescent, 3 youth, 4 adult, 5 old, 6 senile), times the
// creature's seasonal strength. Breeding is for the grown; bonds matter
// most to the young and the old (they gain most from company and warmth,
// as small and weak animals do from huddling); rest weighs on the old;
// play and exploring belong to the young (juvenile play in animals).
enum SeasonalEffect { SEASON_BREEDING, SEASON_BONDS, SEASON_REST, SEASON_PLAY, NUM_SEASONAL_EFFECTS };
float stageWeight(int effect, int stage);
// the change to the chance of conceiving this season (1 none): spring up,
// winter down, from both parents' strengths and stages
float fertilityFactor(int season, float mumZ, int mumStage, float dadZ, int dadStage);
// whether seasonal behavior is on (OPENC2E_NO_SEASONAL_BEHAVIOR turns it off)
bool behaviorOn();

// the value beside a genome (Genetics/NAME.season), if there is one
bool load(const std::string& genomeName, float& z);
void store(const std::string& genomeName, float z);

} // namespace SeasonalRhythm
