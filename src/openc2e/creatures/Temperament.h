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

// A few words: the one or two strongest traits ("bold and curious, quick
// to anger"), or "even-tempered" when none stands out.
std::string describe(const Temperament& t);

// World saving: "z0 z1 ... z6", and back.
std::string toLine(const Temperament& t);
bool fromLine(const std::string& line, Temperament& t);

} // namespace Temperaments
