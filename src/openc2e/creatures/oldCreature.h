/*
 *  oldCreature.h
 *  openc2e
 *
 *  Created by Alyssa Milburn on Sat May 15 2010.
 *  Copyright (c) 2004-2010 Alyssa Milburn. All rights reserved.
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

#ifndef __OLDCREATURE_H
#define __OLDCREATURE_H

#include "Creature.h"
#include "CreatureMind.h"
#include "Relationships.h"
#include "Temperament.h"

#include <cassert>
#include <map>
#include <memory>

// c1/c2

class oldCreature : public Creature {
  protected:
	std::vector<std::pair<AgentRef, unsigned int>> passedover; // objects to notice last, until the tick given (see MindOutput::passOver)

  protected:
	// biochemistry
	unsigned char chemicals[256];
	unsigned int fallSeenTick = 0; // the latest fall of its own told to the mind

	class oldBrain* brain;

	unsigned int biochemticks;
	bioHalfLivesGene* halflives;

	// loci
	unsigned char floatingloci[8];
	unsigned char muscleenergy;
	unsigned char lifestageloci[7];
	unsigned char fertile, receptive, pregnant;
	// pregnancy starting (announced and logged); not on the first tick, when
	// a creature restored from a save may already be pregnant
	bool pregnancywatched = false, waspregnant = false;
	unsigned int keepcheck = 0; // ticks to the next out-of-the-ground check
	// where its middle stood (map x) the last time the check found it in the
	// world, and when: a creature rescued from a pocket goes back to that side
	float lastWorldMidX = 0;
	unsigned int lastWorldTick = 0;
	bool haveLastWorld = false;
	void keepInRoomSystem();
	unsigned int lastbaby = 0; // the baby's genome name (as BABY holds it) while pregnant: the egg's, once laid
	void watchPregnancy();
	unsigned char dead;
	unsigned char involaction[8];
	unsigned int involactionlatency[8];

	void addGene(gene*);
	void tickBrain();
	virtual void tickBiochemistry();

	// pluggable mind (--mind); null means the original oldBrain path
	std::unique_ptr<CreatureMind> mind;
	std::vector<StimulusEvent> pendingstimuli;
	bool mindborn = false, minddied = false;
	std::vector<MindAction> mindactions; // built on the first mind tick
	void buildMindActions();
	void tickMind();
	bool wasriding = false;
	MindPerception buildPerception();
	void notifyMindOfDeath();
	void tickBody(); // a dead creature's body (see Creature.cpp)
	unsigned int deadTicks = 0;
	void applyStimulusChemistry(StimulusEvent& s);

	oldCreature(std::shared_ptr<genomeFile> g, bool is_female, unsigned char _variant, CreatureAgent* a);

	void processGenes();

	AgentRef selectRepresentativeAgent(int type, std::vector<AgentRef> possibles);

  public:
	// TODO: inline?
	unsigned int calculateTickMask(unsigned char);
	unsigned int calculateMultiplier(unsigned char);

	void addChemical(unsigned char id, unsigned char val);
	void subChemical(unsigned char id, unsigned char val);
	unsigned char getChemical(unsigned char id) { return chemicals[id]; }
	void setChemicalLevel(unsigned char id, unsigned char v) { chemicals[id] = v; } // world loading
	virtual float breathLeft() const { return 1.0f; } // 1 = full breath, 0 = none left (under water)
	virtual bool breathesWater() { return false; } // amphibious (C2: see c2Creature)
	// C2: out of energy for a while, the creature falls unconscious (see
	// c2Creature::updateConsciousness): it lies still and does not think
	bool unconscious = false;
	bool isUnconscious() const { return unconscious; }
	bool mateInSight = false; // the sight cell for a possible mate (feedSensoryCells)
	virtual void setUnconscious(bool u) { unconscious = u; } // UNCS (CAOS); c2Creature faints or brings round
	virtual std::vector<float> getOrganState() { return {}; }
	virtual void setOrganState(const std::vector<float>&) {}

	// TODO: is it really worth having drives outside oldCreature?
	virtual unsigned char getDrive(unsigned int id) = 0;

	void handleStimulus(unsigned int id);
	void handleStimulus(const StimulusEvent& s);
	CreatureMind* getMind() { return mind.get(); }
	// reproductive loci, 0-255 (Breeder's Kit)
	unsigned char getFertile() const { return fertile; }
	unsigned char getReceptive() const { return receptive; }
	unsigned char getPregnant() const { return pregnant; }
	// C2: the reproductive switches (fertile, receptive) sum the signed
	// outputs of all their receptors each biochemistry tick, then clip; see
	// c2Organ::processReceptor
	int reproSum[2] = {0, 0};
	bool reproSet[2] = {false, false};
	unsigned char* reproLocus(int i) { return i == 0 ? &fertile : &receptive; }
	// conception (MATE) and a restored pregnancy set it; laying the egg clears it
	void setPregnant(unsigned char p) { pregnant = p; }
	void feedSensoryCells(); // C2 under a mind: see CreatureAI.cpp
	// C2 under a mind: the replaced brain's cells the genome's emitters read
	// (senses, drives, decision), set from what the creature perceives and
	// does; see CreatureAI.cpp
	void updateBrainCells(struct MindPerception& p);
	std::array<float, 32> senseFromStimuli{}; // sense cells lit by stimuli, fading
	AgentRef lastIt;
	float lastItDistance = -1;
	unsigned int lastWallBumps = 0;
	bool brainCellsStarted = false;
	int rewardChem = -1, punishChem = -1; // read from the genome on first use
	// its genome's brain add-ons (BrainProfiles.h), read at birth
	std::vector<std::pair<std::string, float>> brainSettings;
	bool brainProfileRead = false;
	void tickInvoluntaryActions(); // under a mind: see CreatureAI.cpp
	void sayNeed(); // SAYN: see CreatureAI.cpp
	// words heard and learned: for each word of the game's word list
	// (engine.wordlist), how familiar it is, 0-255 (known at WORD_KNOWN); see
	// CreatureSpeech.h. Empty until the first word is heard.
	std::vector<unsigned char> vocab;
	// what it feels about each creature it has met, by moniker
	// (creatures/Relationships.h); saved with the world
	std::map<std::string, Bond> bonds;
	// a friend nearby was just hurt by this creature (Relationships::felt):
	// the mind may stand up for it until defendUntil (world ticks)
	AgentRef defendFrom;
	unsigned int defendUntil = 0;
	std::string defendFor;
	// the owner's fertility setting for it (Breeder's Kit, owner 2026-11-14):
	// -1 the world's (Hatchery), 0 fertile, 1 infertile; saved
	int fertilityOverride = -1;
	// inborn temperament, drawn at hatching (creatures/Temperament.h); saved
	Temperament temperament;
	// the one it was born with, and how life has shifted it while young
	// (Temperament.h, TemperamentDrift); temperament is the two together
	Temperament temperamentBorn;
	TemperamentDrift temperamentDrift;
	std::array<float, NUM_TRAITS> driftLogged{};
	void temperamentExperience(int kind, float size); // CreatureAI.cpp
	// how strongly it fattens in autumn and saves in winter, inherited
	// (Temperament.h, SeasonalRhythm); saved with the world
	float seasonalZ = SeasonalRhythm::roll();
	// winter thrift: a share of biochemistry steps skipped (Biochemistry.cpp)
	float thriftCarry = 0.0f;
	// how many creatures it is bonded to are within radius (affinity at
	// least minAffinity), up to most; worked out every 20 ticks (company
	// warms in the cold, Creature.cpp)
	int companyNear(float radius, float minAffinity, int most);
	int companyCount = 0;
	unsigned int companyAt = 0;
	int lastNeedSaid = -2;
	unsigned int lastNeedTick = 0;

	oldBrain* getBrain() { return brain; }

	unsigned char* getLocusPointer(bool receptor, unsigned char o, unsigned char t, unsigned char l);

	void setInvolActionLatency(unsigned int id, unsigned int n) {
		assert(id < 8);
		involactionlatency[id] = n;
	}
};

// c1

struct c1Reaction {
	explicit c1Reaction(bioReactionGene*);
	bioReactionGene* data;
};

struct c1Receptor {
	c1Receptor(bioReceptorGene*, class c1Creature*);
	bioReceptorGene* data;
	unsigned char* locus;
};

struct c1Emitter {
	c1Emitter(bioEmitterGene*, class c1Creature*);
	bioEmitterGene* data;
	unsigned char* locus;
};

class c1Creature : public oldCreature {
  protected:
	std::vector<std::shared_ptr<c1Reaction> > reactions;
	std::vector<c1Receptor> receptors;
	std::vector<c1Emitter> emitters;

	// loci
	unsigned char senses[6];
	unsigned char gaitloci[8];
	unsigned char drives[16];

	// developer aid: OPENC2E_WATCH_CHEM keeps each creature's last 400 ticks
	// of chemistry and organ life force, and prints them once if its ATP
	// collapses (from 180 or more to under 30)
	std::deque<std::vector<unsigned char>> chemHistory;
	std::deque<std::vector<float>> organHistory;
	bool chemDumped = false;
	void watchChemistry();

	void addGene(gene*);
	void tickBiochemistry();
	void processReaction(c1Reaction&);
	void processEmitter(c1Emitter&);
	void processReceptor(c1Receptor&);

  public:
	c1Creature(std::shared_ptr<genomeFile> g, bool is_female, unsigned char _variant, CreatureAgent* a);

	void tick();

	unsigned char getDrive(unsigned int id) {
		assert(id < 16);
		return drives[id];
	}

	unsigned char* getLocusPointer(bool receptor, unsigned char o, unsigned char t, unsigned char l);

	unsigned int getGait();
};

// c2

struct c2Reaction {
	explicit c2Reaction(bioReactionGene*);
	bioReactionGene* data;
	float carry = 0; // fraction of a unit left over (smooth rate curve)
};

struct c2Receptor {
	c2Receptor(bioReceptorGene*, class c2Organ*);
	bioReceptorGene* data;
	bool processed;
	unsigned char lastvalue;
	unsigned char* locus;
	unsigned int* receptors;
};

struct c2Emitter {
	c2Emitter(bioEmitterGene*, class c2Organ*);
	bioEmitterGene* data;
	unsigned char* locus;
};

class c2Organ {
  protected:
	friend struct c2Receptor;
	friend struct c2Emitter;

	class c2Creature* parent;
	organGene* ourGene;

	std::vector<std::shared_ptr<c2Reaction> > reactions;
	std::vector<c2Receptor> receptors;
	std::vector<c2Emitter> emitters;

	// data
	unsigned char energycost, atpdamagecoefficient;

	// variables
	float lifeforce, shorttermlifeforce, longtermlifeforce;
	float zeroenergydamage; // short-term life force lost per activation without enough ATP

	// locuses
	unsigned char damagerate, repairrate, clockrate, injurytoapply;
	unsigned int clockratereceptors, repairratereceptors, injuryreceptors;

	unsigned int biotick, biochemticks;
	size_t genesprocessed = 0; // genes[] entries already turned into reactions/emitters/receptors

	void processReaction(c2Reaction&);
	void processEmitter(c2Emitter&);
	void processReceptor(c2Receptor&, bool checkchem);

	unsigned char* getLocusPointer(bool receptor, unsigned char o, unsigned char t, unsigned char l, unsigned int** receptors);

  public:
	c2Organ(c2Creature* p, organGene* g);
	void tick();
	organGene* getGene() { return ourGene; }

	void processGenes();

	unsigned char getClockRate() { return clockrate; }
	unsigned char getRepairRate() { return repairrate; }
	unsigned char getDamageRate() { return damagerate; }
	unsigned char getEnergyCost() { return energycost; }
	unsigned char getInjuryToApply() { return injurytoapply; }
	float getInitialLifeforce() { return lifeforce; }
	float getShortTermLifeforce() { return shorttermlifeforce; }
	float getLongTermLifeforce() { return longtermlifeforce; }
	unsigned char getATPDamageCoefficient() { return atpdamagecoefficient; }

	size_t getReceptorCount() { return receptors.size(); }
	size_t getEmitterCount() { return emitters.size(); }
	size_t getReactionCount() { return reactions.size(); }
	const std::vector<std::shared_ptr<c2Reaction>>& getReactions() const { return reactions; }

	void applyInjury(float);
	void restoreLifeforce(float shortterm, float longterm) {
		shorttermlifeforce = shortterm;
		longtermlifeforce = longterm;
	}

	std::vector<gene*> genes;
};

class c2Creature : public oldCreature {
  protected:
	// biochemistry
	std::vector<std::shared_ptr<c2Organ> > organs;

	// loci
	unsigned char senses[14];
	int bubbletimer = 0;
	// Breath-holding (owner's choice, 2026-11-08): under water a creature
	// still gets air for breathTicks (5 minutes at C2's 10 ticks a second)
	// before its air runs out; surfacing refills the breath in a few seconds.
	unsigned int breathHeld = 0;
	static constexpr unsigned int breathTicks = 3000; // (constexpr: std::min takes it by reference, and a debug build then needs a definition)
	// Breathes water as well as air: its genome has no emitter on the air
	// sense (C2: creature, sensorimotor, locus 9), so being under water
	// raises no suffocation in its body (the Frog Norn pack's genome; every
	// stock genome has one). Read from the genome once.
	int waterbreather = -1;
	unsigned int noEnergyTicks = 0, recoveringTicks = 0; // (updateConsciousness)
	void updateConsciousness();

  public:
	void setUnconscious(bool u) override; // faints now, or brings round (UNCS)

  protected:

  public:
	bool breathesWater() override;

  protected:
	void updateUnderwater();
	unsigned char gaitloci[16];
	unsigned char drives[17];
	unsigned char mutationchance, mutationdegree;

	// developer aid: OPENC2E_WATCH_CHEM keeps each creature's last 400 ticks
	// of chemistry and organ life force, and prints them once if its ATP
	// collapses (from 180 or more to under 30)
	std::deque<std::vector<unsigned char>> chemHistory;
	std::deque<std::vector<float>> organHistory;
	bool chemDumped = false;
	void watchChemistry();

	void addGene(gene*);
	void tickBiochemistry();
	void processGenes();
	std::shared_ptr<c2Organ> addingorgan; // organ the gene being added follows in the genome

  public:
	c2Creature(std::shared_ptr<genomeFile> g, bool is_female, unsigned char _variant, CreatureAgent* a);

	void tick();

	// the organs, for the Health and Science Kits (read only)
	const std::vector<std::shared_ptr<c2Organ>>& getOrgans() const { return organs; }
	unsigned char getDrive(unsigned int id) {
		assert(id < 17);
		return drives[id];
	}
	float breathLeft() const override { return 1.0f - std::min(breathHeld, breathTicks) / (float)breathTicks; }

	unsigned char* getLocusPointer(bool receptor, unsigned char o, unsigned char t, unsigned char l);

	std::vector<float> getOrganState() override {
		std::vector<float> v;
		for (auto& o : organs) {
			v.push_back(o->getShortTermLifeforce());
			v.push_back(o->getLongTermLifeforce());
		}
		return v;
	}
	void setOrganState(const std::vector<float>& v) override {
		for (size_t i = 0; i < organs.size() && 2 * i + 1 < v.size(); i++)
			organs[i]->restoreLifeforce(v[2 * i], v[2 * i + 1]);
	}

	unsigned int getGait();
};

#endif

/* vim: set noet: */
