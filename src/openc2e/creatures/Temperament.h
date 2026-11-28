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

// Drift with experience while young (owner, 2026-11-17: young and tapering,
// up to half a step for each kind of experience and about a full step for
// several together, weighted by the evidence). Early hardship in rats moves
// anxiety-like behavior by about 0.3-0.5 standard deviations, more when
// hardships stack (Wang et al. 2020; Bonapersona et al. 2019); care and
// handling make calmer adults; winner and loser effects in fights are real
// but fade within days (Yan et al. 2024); personality is most changeable
// early and settles by early adulthood (Bleidorn et al. 2022). Each kind of
// experience keeps its own shift of each trait; a creature's temperament is
// its inborn one plus these shifts.
enum TemperamentExperience {
	EXPERIENCE_HURT, // hurt or frightened: more reactive, less bold, less sociable
	EXPERIENCE_COMFORT, // comforted, played with: calmer, bolder, more sociable
	EXPERIENCE_FIGHT, // in a fight: quicker to anger (partly fades back)
	EXPERIENCE_EXPLORE, // the new turned out well (+) or badly (-): more or less curious
	NUM_EXPERIENCES
};
struct TemperamentDrift {
	std::array<std::array<float, NUM_TRAITS>, NUM_EXPERIENCES> shift{};
	float fightRecent = 0; // the fading part of the fight shift (of temper)
	bool any() const;
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

// How changeable a creature of this life stage is (0 baby, 1 child, 2
// adolescent, 3 youth ...): 1, 0.6, 0.25, then 0 (settled).
float plasticity(int stage);
// An experience of a kind, of a size (about 1 for a hard slap, a bad fall,
// a pat; negative for EXPERIENCE_EXPLORE gone badly), at a life stage.
// Returns true when it moved the temperament.
bool experience(TemperamentDrift& d, int kind, float size, int stage);
// the fights' fading part eases back, per mind tick
void fadeFights(TemperamentDrift& d);
// the inborn temperament plus the drift (each trait at most about a full
// step from where it was born, within the usual +-2)
Temperament withDrift(const Temperament& born, const TemperamentDrift& d);
std::string driftLine(const TemperamentDrift& d);
bool driftFromLine(const std::string& line, TemperamentDrift& d);

// World saving: "z0 z1 ... z6", and back.
std::string toLine(const Temperament& t);
bool fromLine(const std::string& line, Temperament& t);

// Temperament and learning (owner, 2026-11-27; the project's reports/
// "Temperament and learning proposal.md"): each trait speeds up or slows
// down some kinds of learning, smoothly from the lowest creature to the
// highest, by a factor exp(k z) for each trait that touches a channel (1 for
// an average creature), the product clamped to 0.5-2. Every trait pays off
// at both ends: quick learners relearn slowly, the curious learn the new
// and the incurious the familiar, the sociable learn from others and the
// aloof from their own trying (Sih & Del Giudice 2012: animal personality
// trades speed against accuracy). OPENC2E_TEMPERAMENT_LEARNING=0 turns it
// off (every factor 1), for comparison runs. Nothing is saved: it all
// follows from the traits.
enum LearningChannel {
	LEARN_NEW_THINGS, // its first tries at a kind of thing: bold +.20, curious +.20, energetic +.15
	LEARN_OWN_DOING, // all it learns from its own doing: aloof (sociability -.20), flighty (steadiness -.15)
	LEARN_DANGER, // the danger line (pain, injury, fear): reactive +.30
	LEARN_PLACES, // food places it sees for itself: curious +.20, energetic +.15
	LEARN_WATCHING, // what it sees others do: sociable +.25
	LEARN_FROM_STRANGERS, // watching, and words from, creatures not its kin: bold +.20
	LEARN_FROM_FAMILY, // words from its close kin: bold -.15 (the shy learn most at home)
	LEARN_WORDS_NEW, // a word still new to it: curious +.30
	LEARN_WORDS_CREATURES, // words from other creatures: sociable +.25
	LEARN_WORDS_MACHINES, // words from machines: sociable -.20
	LEARN_WORDS_VERBS, // verbs, heard while doing: energetic +.15
	LEARN_WORDS_BURSTS, // a word heard again at once: the placid (energy -.15) take more from it
	SAY_READINESS, // how soon it says a word it is learning: bold +.10 (the shy need a firmer hold)
	LEARN_COLONY_WORDS, // letting its other names go when understood: sociable +.25
	LEARN_HABITUATION, // the familiar growing dull: curious +.20
	LEARN_FEAR_FADES, // a fear proved wrong fading: bold +.25, reactive -.30
	LEARN_REVERSAL, // relearning what it knew well: bold -.20, steady -.20
	LEARN_FORGIVING, // making up after harm: temper -.25
	LEARN_GRUDGES, // harm souring a bond: temper +.30
	LEARN_FROM_SLAPS, // what a slap teaches: temper -.30
	LEARN_FIGHTS, // what hitting gets it: temper +.20
	LEARN_RESERVE_NOVELTY, // its brain's reserve from new words and places: curious +.25
	LEARN_GROWTH_TURNS, // its brain's growth from back-and-forth talk: sociable +.20
	LEARN_SETTLING, // a word heard again after a while settling: steady +.20, energetic -.10
	LEARN_SETTLING_FAMILIAR, // the same for a word it already knows: curious -.15
	LEARN_SLEEP_SETTLING, // sleep settling the day's words: energetic -.20
	LEARN_SHAPED_BY_HURT, // while young, hurt shaping its temperament: reactive +.20
	LEARN_SHAPED_BY_COMFORT, // while young, comfort shaping it: reactive +.20 (differential susceptibility)
	NUM_LEARNING_CHANNELS
};
// whether temperament shapes learning (OPENC2E_TEMPERAMENT_LEARNING=0 off)
bool learningOn();
// the channel's k for each trait (the table above)
float learningK(int channel, int trait);
// the factor for a channel (1 average; 0.5-2), from a temperament or its z
float learningFactor(const std::array<float, NUM_TRAITS>& z, int channel);
inline float learningFactor(const Temperament& t, int channel) { return learningFactor(t.z, channel); }
// Stress and learning, an inverted U (Lupien et al. 2007): a little stress
// sharpens learning, a lot dulls it, and the peak sits at lower stress the
// more reactive the creature. Stress 0-1; a mild 0.8-1.08.
float stressLearning(float reactivityZ, float stress);
// One line for the Owner's Kit: how it learns, from its two strongest
// learning effects ("learns danger quickly and lets go of a fear slowly"),
// or "" when none stands out.
std::string learningLine(const Temperament& t);

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
