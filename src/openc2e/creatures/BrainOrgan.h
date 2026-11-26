/*
 *  BrainOrgan.h
 *  openc2e
 *
 *  The brain as an organ of the body (owner, 2026-11-26: "I do want to have
 *  our mind attached or associated with it"; "Yes, cost 5"; then "Top
 *  seven", from the research report "Brain development for Zeetures").
 *
 *  Its energy. C2 genomes have a brain organ gene, and the original game
 *  charged it 22 ATP each time it ran (half as often as the other organs);
 *  openc2e never made it. Ours costs 5 (about 2.5 ATP a step, +11%): a
 *  primate's share of the body's energy (about 9%), not the original's 32%.
 *  - It runs every other biochemistry step and costs the same whatever the
 *    mind is doing, a quarter less asleep.
 *  - It is paid last, after every other organ, and only while ATP is at 60
 *    or more: it is never what empties the store. Unpaid, or short of
 *    glucose, the creature grows dazed (0-255): it learns more slowly,
 *    chooses less surely, and walks slowly; the young are hit hardest. Fed,
 *    it clears within a few minutes, faster asleep.
 *
 *  Its life: five numbers, worked out every 5 s (a norn's childhood is about
 *  70 minutes of play, so the human evidence is put on that clock).
 *  - Growth (newborn about 100): what the brain builds. Gains go first to a
 *    pending store and are kept by sleep (unslept, they fade: half in 15
 *    minutes for the young, slower later), scaled by the plasticity of its
 *    life stage (baby 1, child 0.8, adolescent 0.5, youth 0.25, adult 0.12,
 *    old 0.06, senile 0.03). From a little each hour of living, and most
 *    from being talked with: a turn of conversation (spoken to, it answers
 *    within 15 s, or it speaks and is answered) is worth three times being
 *    spoken to without a reply; overheard talk is worth nothing. Touch and
 *    care (the hand's tickles, other creatures' tickles and kindness) count
 *    as much as talk for a baby, half for a child, little after. Both
 *    saturate: a stream of them is worth little more than a steady few.
 *    Dazed (unfed), it builds nothing; chronically stressed while young,
 *    half.
 *  - Reserve: what holds off the symptoms of age. From new words, new
 *    places and new acquaintances, most from child to youth; long boredom
 *    or loneliness wears it down a little.
 *  - Damage: lasting harm. Age wears it in from youth, slowly as a youth or
 *    adult, faster old, fastest senile; exercise (walking: the trophin it
 *    makes) slows that by up to 60%, chronic stress when old speeds it as
 *    much, loneliness from adulthood a little. Oxygen gone (oxygen under 20)
 *    for more than 30 s damages it fast. Frankenstein's creatures do not
 *    age.
 *  - Resilience (128): how hard fear and pain strike, set while young:
 *    care, and stress that passes quickly, raise it; chronic stress lowers
 *    it. Fixed from youth on.
 *  - Stress: a slow hormone of fear, pain, slaps, harm, great hunger and
 *    loneliness, cleared by time, sleep, comfort and company. Over half is
 *    chronic.
 *  The mind reads Capacity (Growth - Damage: learns faster, or slower) and
 *  Symptoms (Damage - Reserve: walks slower, chooses less surely, forgets
 *  places, and at worst words).
 *
 *  Switch OPENC2E_NO_BRAIN_ORGAN (all of it); OPENC2E_NO_BRAIN_LIFE (only
 *  its life: the energy and daze stay); OPENC2E_WATCH_BRAIN logs;
 *  OPENC2E_TEST_BRAIN_DAZE=N starts every creature this dazed;
 *  OPENC2E_TEST_BRAIN="growth,reserve,damage,resilience" starts every
 *  creature with these; OPENC2E_COUNT_ENERGY counts it as "brain".
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

#include "AgentRef.h"

#include <iosfwd>
#include <sstream>
#include <string>

class Agent;
class Creature;
class c2Creature;
class oldCreature;

namespace BrainOrgan {

const int energyCost = 5; // ATP each time it runs, awake
const int clockRate = 127; // (shown as an organ's clock: half the speed of the others)

// a creature's brain life (on oldCreature; saved with the world)
struct Life {
	bool set = false; // given its starting values yet
	float growth = 100, pending = 0, reserve = 0, damage = 0, resilience = 128;
	float trophin = 0, stress = 0; // (trophin: steps, fading)
	float recentTurns = 0, recentCare = 0; // fading counts (saturation)
	// this 5 s: turns, one-way talk, care, slaps, harm, steps, new words
	float turns = 0, oneWay = 0, care = 0, slaps = 0, harm = 0, newWords = 0;
	int steps = 0;
	// talk: spoken to (directed) at, by; it last spoke at
	unsigned int spokenAt = 0, saidAt = 0;
	AgentRef spokenBy;
	bool spokenAnswered = true;
	float hypoxia = 0; // brain runs with oxygen gone
	int places = -1, partners = -1; // last counted (new ones add to Reserve)
	unsigned int stressRoseAt = 0; // stress over 0.2 since (an episode)
	bool wasChronic = false;
};

bool off();
bool lifeOff();

// one biochemistry step of a creature, after its other organs
void step(c2Creature& c);
// each world tick (its life is worked out every 5 s)
void tick();

// how dazed it is, 0 (clear) to 1 (as dazed as can be)
float daze(Creature* c);
float dazeOf(Agent* creatureAgent);
// how fast it learns for its brain: Capacity (about 0.6 to 1.35; 1 at birth)
float learning(Creature* c);
// Symptoms, 0 (none) to 1
float symptoms(Creature* c);
float symptomsOf(Agent* creatureAgent);
// how hard fear and pain strike it (Resilience: 1.25 to 0.75; 1 at 128)
float fearScale(Creature* c);

// what happens to it (CreatureSpeech, CreatureAI, Relationships, walking)
void spokenTo(oldCreature* listener, Agent* speaker, bool directed);
void spoke(oldCreature* c);
void touched(oldCreature* c, float amount); // care (1: a tickle)
void slapped(oldCreature* c);
void harmed(oldCreature* c, float amount);
void stepped(Creature* c);
void learnedWord(oldCreature* c);

void reset(); // at each world load, before the save is read
void save(std::ostream& out);
bool readLine(const std::string& key, std::istringstream& line);

} // namespace BrainOrgan
