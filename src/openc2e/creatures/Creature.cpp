/*
 *  Creature.cpp
 *  openc2e
 *
 *  Created by Alyssa Milburn on Tue May 25 2004.
 *  Copyright (c) 2004-2006 Alyssa Milburn. All rights reserved.
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

#include "Creature.h"

#include "Catalogue.h"
#include "Engine.h"
#include "CreatureAgent.h"
#include "World.h"
#include "common/Random.h"
#include "Room.h"
#include "SkeletalCreature.h"
#include "Map.h"
#include "c2eBrain.h"
#include "c2eCreature.h"
#include "common/throw_ifnot.h"
#include "historyManager.h"
#include "oldBrain.h"
#include "oldCreature.h"

#include <algorithm>
#include <cassert>
#include <memory>

Creature::Creature(std::shared_ptr<genomeFile> g, bool is_female, unsigned char _variant, CreatureAgent* a) {
	assert(g);
	genome = g;

	female = is_female;
	genus = 0; // TODO: really, we shouldn't do this, and should instead later assert that a genus was set
	variant = _variant;
	stage = baby;

	parent = a;
	assert(parent);
	parentagent = dynamic_cast<Agent*>(parent);
	assert(parentagent);

	alive = true; // ?
	asleep = false; // ?
	dreaming = false; // ?
	tickage = false;
	zombie = false;

	age = ticks = 0;

	attn = decn = -1;

	for (unsigned short& i : tintinfo)
		i = 128;
}

Creature::~Creature() {
}

void Creature::finishInit() {
	processGenes();
}

bool Creature::shouldProcessGene(gene* g) {
	geneFlags& flags = g->header.flags;

	// non-expressed genes are to be ignored
	if (flags.notexpressed)
		return false;

	// gender-specific genes are only to be processed if they are of this
	if (flags.femaleonly && !female)
		return false;
	if (flags.maleonly && female)
		return false;

	// obviously we only switch on at the stage in question
	if (g->header.switchontime != stage)
		return false;

	// TODO: header.variant?

	return true;
}

bool Creature::isGeneExpressed(gene* g) {
	geneFlags& flags = g->header.flags;
	if (flags.notexpressed)
		return false;
	if (flags.femaleonly && !female)
		return false;
	if (flags.maleonly && female)
		return false;
	return g->header.switchontime <= stage;
}

void Creature::processGenes() {
	for (auto& gene : genome->genes) {
		if (shouldProcessGene(gene.get()))
			addGene(gene.get());
	}
}

void oldCreature::processGenes() {
	brain->processGenes();
	Creature::processGenes();
}

void c2Creature::processGenes() {
	// Biochemistry genes belong to the organ they follow in the genome. Walk
	// the whole genome in order (genes of earlier stages included) so that a
	// gene switching on at a later life stage joins that organ, not whichever
	// organ happens to be last. Genes after the brain organ (not modelled)
	// keep going to the organ before it, as before.
	brain->processGenes();
	std::shared_ptr<c2Organ> current;
	for (auto& gp : genome->genes) {
		gene* g = gp.get();
		if (organGene* o = dynamic_cast<organGene*>(g)) {
			if (!o->isBrain()) {
				current = nullptr;
				for (auto& org : organs)
					if (org->getGene() == o)
						current = org;
			}
		}
		if (!shouldProcessGene(g))
			continue;
		addingorgan = current;
		addGene(g);
		if (organGene* o = dynamic_cast<organGene*>(g))
			if (!o->isBrain() && !organs.empty() && organs.back()->getGene() == o)
				current = organs.back();
	}
	addingorgan = nullptr;

	// each organ turns only its new genes into reactions, emitters and
	// receptors (it used to redo all of them at every life stage, so an adult
	// ran every reaction five times over)
	for (auto& organ : organs) {
		organ->processGenes();
	}
}

void c2eCreature::processGenes() {
	// brain must be processed first (to create loci etc)
	// organs should be processed last, because new ones will be created by normal processGenes()

	brain->processGenes();
	Creature::processGenes();
	for (auto& organ : organs) {
		organ->processGenes();
	}
}

void Creature::addGene(gene* g) {
	if (typeid(*g) == typeid(creatureInstinctGene)) {
		unprocessedinstincts.push_back((creatureInstinctGene*)g);
	} else if (typeid(*g) == typeid(creatureGenusGene)) {
		// TODO: mmh, genus changes after setup shouldn't be valid
		genus = ((creatureGenusGene*)g)->genus;
		parentagent->genus = genus + 1;
	} else if (typeid(*g) == typeid(creaturePigmentGene)) {
		creaturePigmentGene& p = *((creaturePigmentGene*)g);
		// TODO: we don't sanity-check
		tintinfo[p.color] = p.amount;
	} else if (typeid(*g) == typeid(creaturePigmentBleedGene)) {
		creaturePigmentBleedGene& p = *((creaturePigmentBleedGene*)g);
		tintinfo[3] = p.rotation;
		tintinfo[4] = p.swap;
	}
}

void Creature::ageCreature() {
	if (stage >= senile) {
		diedofoldage = true;
		die();
		return;
	} //previously we just returned

	stage = (lifestage)((int)stage + 1);
	processGenes();

	assert(parent);
	parent->creatureAged();
#ifndef _CREATURE_STANDALONE
	world.history->getMoniker(world.history->findMoniker(genome)).addEvent(4, "", ""); // aged event
#endif
}

void Creature::setAsleep(bool a) {
	// TODO: skeletalcreature might need to close eyes? or should that just be done during the skeletal update?
	if (!a && dreaming)
		setDreaming(false);
	asleep = a;
}

void Creature::setDreaming(bool d) {
	if (d && !asleep)
		setAsleep(true);
	dreaming = d;
}

void Creature::born() {
	// C1/C2: a newborn becomes the selected creature if none is selected.
	if (engine.version < 3 && !world.selectedcreature)
		world.selectCreature(AgentRef(parentagent).lock());

	parent->creatureBorn();

	// TODO: life event?
#ifndef _CREATURE_STANDALONE
	{
		std::string mon = world.history->findMoniker(genome);
		monikerData& m = world.history->getMoniker(mon);
		m.wasBorn();
		m.addEvent(3, "", ""); // born event, parents..
		if (!world.history->quiet) { // a real birth, not a creature restored from a save
			m.borntick = world.worldtickcount;
			m.bornknown = true;
			if (m.name.empty())
				m.name = world.history->nextDefaultName(m.genus);
			world.history->logLife(lifeLogEntry::birth, mon, (int)stage);
		}
	}
#endif

	tickage = true;
}

void Creature::die() {
	parent->creatureDied();

	// TODO: life event?
#ifndef _CREATURE_STANDALONE
	{
		std::string mon = world.history->findMoniker(genome);
		monikerData& m = world.history->getMoniker(mon);
		m.hasDied();
		m.addEvent(7, "", ""); // died event
		if (!m.died) {
			m.died = true;
			m.diedtick = world.worldtickcount;
			m.diedstage = (int)stage;
			world.history->logLife(lifeLogEntry::death, mon, (int)stage, diedofoldage);
		}
	}
#endif
	// TODO: disable brain/biochemistry updates

	// force die script
	parentagent->stopScript();
	parentagent->queueScript(72); // Death script in c1, c2 and c2e

	// skeletalcreature eyes, also? see setAsleep comment
	alive = false;
}

void Creature::tick() {
	ticks++;

	if (!alive)
		return;

	if (tickage)
		age++;
}

/*
 * oldCreature contains the shared elements of C1 of C2 (creatures are mostly identical in both games)
 */
oldCreature::oldCreature(std::shared_ptr<genomeFile> g, bool is_female, unsigned char _variant, CreatureAgent* a)
	: Creature(g, is_female, _variant, a) {
	if (!engine.mindname.empty()) {
		mind = createCreatureMind(engine.mindname);
		// C1/C2 agents fall into categories 0-39 (see World::findCategory)
		chosenagents.resize(40);
	}

	biochemticks = 0;
	halflives = 0;

	for (unsigned char& i : floatingloci)
		i = 0;
	for (unsigned char& i : lifestageloci)
		i = 0;
	for (unsigned char& i : involaction)
		i = 0;
	for (unsigned char& chemical : chemicals)
		chemical = 0;

	for (unsigned int& i : involactionlatency)
		i = 0;

	muscleenergy = 0;
	fertile = pregnant = receptive = 0;
	dead = 0;

	brain = 0; // just in case
}

c1Creature::c1Creature(std::shared_ptr<genomeFile> g, bool is_female, unsigned char _variant, CreatureAgent* a)
	: oldCreature(g, is_female, _variant, a) {
	assert(g->getVersion() == 1);

	for (unsigned char& sense : senses)
		sense = 0;
	for (unsigned char& i : gaitloci)
		i = 0;
	for (unsigned char& drive : drives)
		drive = 0;

	// TODO: chosenagents size

	brain = new oldBrain(this);
	finishInit();
	brain->init();
}

c2Creature::c2Creature(std::shared_ptr<genomeFile> g, bool is_female, unsigned char _variant, CreatureAgent* a)
	: oldCreature(g, is_female, _variant, a) {
	assert(g->getVersion() == 2);

	for (unsigned char& sense : senses)
		sense = 0;
	for (unsigned char& i : gaitloci)
		i = 0;
	for (unsigned char& drive : drives)
		drive = 0;

	mutationchance = 0;
	mutationdegree = 0;

	// TODO: chosenagents size

	brain = new oldBrain(this);
	finishInit();
	brain->init();
}

c2eCreature::c2eCreature(std::shared_ptr<genomeFile> g, bool is_female, unsigned char _variant, CreatureAgent* a)
	: Creature(g, is_female, _variant, a) {
	assert(g->getVersion() == 3);

	for (float& chemical : chemicals)
		chemical = 0.0f;

	// initialise loci
	for (float& i : lifestageloci)
		i = 0.0f;
	muscleenergy = 0.0f;
	for (float& i : floatingloci)
		i = 0.0f;
	fertile = pregnant = ovulate = receptive = chanceofmutation = degreeofmutation = 0.0f;
	dead = 0.0f;
	for (float& i : involaction)
		i = 0.0f;
	for (float& i : gaitloci)
		i = 0.0f;
	for (float& sense : senses)
		sense = 0.0f;
	for (float& drive : drives)
		drive = 0.0f;

	for (unsigned int& i : involactionlatency)
		i = 0;

	halflives = 0;

	if (!catalogue.hasTag("Action Script To Neuron Mappings"))
		throw Exception("c2eCreature was unable to read the 'Action Script To Neuron Mappings' catalogue tag");
	const std::vector<std::string>& mappinginfotag = catalogue.getTag("Action Script To Neuron Mappings");
	for (const auto& i : mappinginfotag)
		mappinginfo.push_back(std::stoi(i));

	// TODO: should we really hard-code this?
	chosenagents.resize(40);

	brain = new c2eBrain(this);
	finishInit();
	brain->init();
}

unsigned int c1Creature::getGait() {
	unsigned int gait = 0;

	for (unsigned int i = 1; i < 8; i++)
		if (gaitloci[i] > gaitloci[gait])
			gait = i;

	return gait;
}

unsigned int c2Creature::getGait() {
	unsigned int gait = 0;

	// the courting walk (gait 13, which a norn's genes set from its sex drive
	// chemical) comes readily as a youth or adult, less as an adolescent or
	// when old, never as a baby, child or when senile (owner, 2026-11-10:
	// vigor with age)
	static const float courting[7] = {0.0f, 0.0f, 0.6f, 1.0f, 1.0f, 0.4f, 0.0f};
	auto level = [&](unsigned int i) {
		float v = gaitloci[i];
		if (i == 13 && (int)stage >= 0 && (int)stage < 7)
			v *= courting[(int)stage];
		return v;
	};
	for (unsigned int i = 1; i < 16; i++)
		if (level(i) > level(gait))
			gait = i;

	return gait;
}

unsigned int c2eCreature::getGait() {
	unsigned int gait = 0;

	for (unsigned int i = 1; i < 16; i++)
		if (gaitloci[i] > gaitloci[gait])
			gait = i;

	return gait;
}

void c1Creature::tick() {
	// TODO: should we tick some things even if dead?
	if (!alive) {
		notifyMindOfDeath();
		return;
	}

	senses[0] = 255; // always-on
	senses[1] = (asleep ? 255 : 0); // asleep
	senses[2] = 0; // air coldness (TODO)
	senses[3] = 0; // air hotness (TODO)
	senses[4] = 0; // light level (TODO)
	senses[5] = 0; // crowdedness (TODO)

	tickBrain();
	tickBiochemistry();

	// lifestage checks
	for (unsigned int i = 0; i < 7; i++) {
		if ((lifestageloci[i] != 0) && (stage == (lifestage)i))
			ageCreature();
	}

	// C1/C2: the world's egg-laying script takes the baby's genome name from
	// BABY and sets it to 0 as the egg is laid; that ends the pregnancy
	if (pregnant != 0 && parentagent && parentagent->babymoniker == 0)
		pregnant = 0;

	watchPregnancy();

	if (dead != 0)
		die();

	Creature::tick();
}

// C2 creatures breathe through the genome: an emitter on the air-quality locus
// (sensorimotor 9) makes Air, which the genome turns into oxygen. The engine's
// part is the air quality. With the head in a sea room (room type 2, "in the
// sea") there is no air: the genome then runs short of oxygen and the creature
// suffocates. Bubbles rise from the head while it is under water, using the
// world's own bubble object (classifier 2 1 27, sprite "bubs").
void c2Creature::updateUnderwater() {
	if (!parentagent || !world.map)
		return;
	// the mouth (point 1 of the head part's attachments), or near the top
	// of the body if the creature has no skeleton
	float headx = parentagent->x + parentagent->getWidth() / 2.0f;
	float heady = parentagent->y + 10.0f;
	if (SkeletalCreature* sc = dynamic_cast<SkeletalCreature*>(parentagent)) {
		headx = parentagent->x + sc->attachmentX(1, 1);
		heady = parentagent->y + sc->attachmentY(1, 1);
	}
	std::shared_ptr<Room> room = world.map->roomAt(headx, heady);
	bool underwater = room && room->type == 2;
	// Standing on dry floor, the head is not under water, even where the
	// box of a water room beside the floor's edge reaches higher than the
	// head: at the end of the green walkway east of the incubator (room 14,
	// floor 892) a creature facing the sea's room (31, up to y 669) blew
	// bubbles (owner, 2026-11-10; 22 such edges on the map). Only with the
	// feet in water too (or falling, or swimming: no foot room) is it under.
	if (underwater)
		if (SkeletalCreature* sc = dynamic_cast<SkeletalCreature*>(parentagent))
			if (std::shared_ptr<Room> feet = sc->footRoom(); feet && feet->type != 2)
				underwater = false;
	// Inside a vehicle (the submarine) it breathes the vehicle's air: a norn
	// carried under the sea blew bubbles (owner, 2026-11-10)
	if (underwater && parentagent->invehicle)
		underwater = false;
	if (underwater) {
		if (breathHeld < breathTicks)
			breathHeld++; // holding its breath: the air sense stays as it was
		else
			senses[9] = 0; // out of breath
	}
	if (!underwater) {
		breathHeld = breathHeld > 100 ? breathHeld - 100 : 0; // breathing again
		bubbletimer = 0;
		return;
	}
	if (bubbletimer > 0) {
		bubbletimer--;
		return;
	}
	bubbletimer = rand_int32(12, 40);
	std::string caos = fmt::format("inst new: simp bubs 10 0 8000 0 setv cls2 2 1 27 mvto {} {} mesg writ targ 8",
		(int)headx - 6, (int)heady - 12);
	std::string result = engine.executeNetwork(caos);
	if (!result.empty())
		fmt::print("* Couldn't make a bubble: {}\n", result);
}

void c2Creature::tick() {
	// TODO: should we tick some things even if dead?
	if (!alive) {
		notifyMindOfDeath();
		return;
	}

	senses[0] = 255; // always-on
	senses[1] = (asleep ? 255 : 0); // asleep
	senses[2] = 0; // air coldness: below the comfort band, see below
	senses[3] = 0; // air hotness: above it
	senses[4] = 0; // light level: the room's (rooms keep the light level the world gives them)
	if (parentagent && world.map) {
		if (std::shared_ptr<Room> r = world.map->roomAt(parentagent->x + parentagent->getWidth() / 2.0f, parentagent->y + parentagent->getHeight() / 2.0f)) {
			senses[4] = (unsigned char)std::clamp(r->lite, 0, 255);
			// Room temperature (0-255) to the genome's air coldness/hotness
			// senses (its emitters fire above 8). Comfortable from 50 to 140,
			// three per degree beyond. The hot side agrees with real C2 (a
			// newborn heated up in a ~170 room, not at 89-114); the cold side
			// is our choice: real C2 creatures felt no cold even at 30.
			senses[2] = (unsigned char)std::clamp((50 - r->temp) * 3, 0, 255);
			senses[3] = (unsigned char)std::clamp((r->temp - 140) * 3, 0, 255);
		}
	}
	senses[5] = 0; // crowdedness (TODO)
	senses[6] = 0; // radiation (TODO)
	senses[7] = 0; // time of day (TODO)
	senses[8] = 0; // season (TODO)
	senses[9] = 255; // air quality: full, unless the head is under water (below)
	updateUnderwater();
	senses[10] = 0; // slope up (TODO)
	senses[11] = 0; // slope down (TODO)
	senses[12] = 0; // wind towards (TODO)
	senses[13] = 0; // wind behind (TODO)

	tickBrain();
	// TODO: update brain organ every 0.4ms (ie: when brain is processed)!
	tickBiochemistry();

	// lifestage checks
	for (unsigned int i = 0; i < 7; i++) {
		if ((lifestageloci[i] != 0) && (stage == (lifestage)i))
			ageCreature();
	}

	static const bool watchChem = getenv("OPENC2E_WATCH_CHEM") != nullptr;
	if (watchChem)
		watchChemistry();

	// C1/C2: the world's egg-laying script takes the baby's genome name from
	// BABY and sets it to 0 as the egg is laid; that ends the pregnancy
	if (pregnant != 0 && parentagent && parentagent->babymoniker == 0)
		pregnant = 0;

	watchPregnancy();

	if (dead != 0)
		die();

	Creature::tick();
}

void c2Creature::watchChemistry() {
	const int ATP = 99;
	chemHistory.emplace_back(chemicals, chemicals + 256);
	organHistory.push_back(getOrganState());
	if (chemHistory.size() > 400) {
		chemHistory.pop_front();
		organHistory.pop_front();
	}
	if (chemDumped || chemicals[ATP] >= 30)
		return;
	unsigned char peak = 0;
	for (auto& h : chemHistory)
		peak = std::max(peak, h[ATP]);
	if (peak < 180)
		return;
	chemDumped = true;
	int unid = parentagent ? parentagent->getUNID() : -1;
	size_t n = chemHistory.size();
	fmt::print("* CHEM COLLAPSE unid {} tick {} asleep {} dead-locus {} ATP {} (peak {}) over the last {} ticks; every 20th tick, oldest first:\n",
		unid, world.tickcount, (bool)asleep, (int)dead, (int)chemicals[ATP], (int)peak, n);
	for (int c = 1; c < 256; c++) {
		int lo = 255, hi = 0;
		for (auto& h : chemHistory) {
			lo = std::min(lo, (int)h[c]);
			hi = std::max(hi, (int)h[c]);
		}
		if (hi - lo < 30 && c != ATP)
			continue;
		std::string row;
		for (size_t i = 0; i < n; i += 20)
			row += fmt::format(" {}", (int)chemHistory[i][c]);
		row += fmt::format(" | now {}", (int)chemHistory[n - 1][c]);
		fmt::print("*   chem {:3}:{}\n", c, row);
	}
	for (size_t o = 0; 2 * o + 1 < organHistory.back().size(); o++) {
		float first = organHistory.front()[2 * o + 1], last = organHistory.back()[2 * o + 1];
		float sfirst = organHistory.front()[2 * o], slast = organHistory.back()[2 * o];
		if (std::fabs(first - last) > 1.0f || std::fabs(sfirst - slast) > first * 0.05f || last <= 0.5f)
			fmt::print("*   organ {}: short-term life {:.0f} -> {:.0f}, long-term {:.0f} -> {:.0f}\n", o, sfirst, slast, first, last);
	}
}

void c2eCreature::tick() {
	// TODO: should we tick some things even if dead?
	if (!alive)
		return;

	// TODO: update muscleenergy

	senses[0] = 1.0f; // always-on
	senses[1] = (asleep ? 1.0f : 0.0f); // asleep
	// space for old C2 senses: hotness, coldness, light level
	senses[5] = 0.0f; // crowedness (TODO)
	// space for old C2 senses: radiation, time of day, season
	senses[9] = 1.0f; // air quality (TODO)
	senses[10] = 0.0f; // steepness of upcoming slope (up) (TODO)
	senses[11] = 0.0f; // steepness of upcoming slope (down) (TODO)
	// space for old C2 senses: oncoming wind, wind from behind

	tickBrain();
	tickBiochemistry();

	// lifestage checks
	for (unsigned int i = 0; i < 7; i++) {
		if ((lifestageloci[i] != 0.0f) && (stage == (lifestage)i))
			ageCreature();
	}

	if (dead != 0.0f)
		die();

	Creature::tick();
}

void oldCreature::watchPregnancy() {
	bool now = pregnant != 0;
	if (pregnancywatched && now && !waspregnant) {
#ifndef _CREATURE_STANDALONE
		std::string mon = world.history->findMoniker(genome);
		if (world.history->hasMoniker(mon)) {
			world.history->getMoniker(mon).addEvent(8, "", ""); // became pregnant
			world.history->logLife(lifeLogEntry::pregnancy, mon, (int)stage);
		}
#endif
	}
	waspregnant = now;
	pregnancywatched = true;
}

void oldCreature::addGene(gene* g) {
	Creature::addGene(g);
	if (typeid(*g) == typeid(bioInitialConcentrationGene)) {
		// initialise chemical levels
		bioInitialConcentrationGene* b = (bioInitialConcentrationGene*)(g);
		chemicals[b->chemical] = b->quantity;
	} else if (typeid(*g) == typeid(bioHalfLivesGene)) {
		bioHalfLivesGene* d = dynamic_cast<bioHalfLivesGene*>(g);
		assert(d);
		halflives = d;
	}
}

void c1Creature::addGene(gene* g) {
	oldCreature::addGene(g);

	if (typeid(*g) == typeid(bioReactionGene)) {
		reactions.push_back(std::shared_ptr<c1Reaction>(new c1Reaction((bioReactionGene*)g)));
	} else if (typeid(*g) == typeid(bioEmitterGene)) {
		emitters.push_back(c1Emitter((bioEmitterGene*)g, this));
	} else if (typeid(*g) == typeid(bioReceptorGene)) {
		receptors.push_back(c1Receptor((bioReceptorGene*)g, this));
	}
}

void c2Creature::addGene(gene* g) {
	oldCreature::addGene(g);

	if (typeid(*g) == typeid(organGene)) {
		// create organ
		organGene* o = dynamic_cast<organGene*>(g);
		assert(o);
		if (!o->isBrain()) { // TODO: handle brain organ
			organs.push_back(std::shared_ptr<c2Organ>(new c2Organ(this, o)));
		}
	} else if (typeid(*g) == typeid(bioReactionGene) || typeid(*g) == typeid(bioEmitterGene) || typeid(*g) == typeid(bioReceptorGene)) {
		std::shared_ptr<c2Organ> organ = addingorgan ? addingorgan : (organs.empty() ? nullptr : organs.back());
		THROW_IFNOT(organ);
		organ->genes.push_back(g);
	}
}

void c2eCreature::addGene(gene* g) {
	Creature::addGene(g);

	if (typeid(*g) == typeid(bioInitialConcentrationGene)) {
		// initialise chemical levels
		bioInitialConcentrationGene* b = (bioInitialConcentrationGene*)(g);
		chemicals[b->chemical] = b->quantity / 255.0f; // TODO: correctness unchecked
	} else if (typeid(*g) == typeid(organGene)) {
		// create organ
		organGene* o = dynamic_cast<organGene*>(g);
		assert(o);
		if (!o->isBrain()) { // TODO: handle brain organ
			organs.push_back(std::shared_ptr<c2eOrgan>(new c2eOrgan(this, o)));
		}
	} else if (typeid(*g) == typeid(bioReactionGene)) {
		THROW_IFNOT(organs.size() > 0);
		organs.back()->genes.push_back(g);
	} else if (typeid(*g) == typeid(bioEmitterGene)) {
		THROW_IFNOT(organs.size() > 0);
		organs.back()->genes.push_back(g);
	} else if (typeid(*g) == typeid(bioReceptorGene)) {
		THROW_IFNOT(organs.size() > 0);
		organs.back()->genes.push_back(g);
	} else if (typeid(*g) == typeid(bioHalfLivesGene)) {
		bioHalfLivesGene* d = dynamic_cast<bioHalfLivesGene*>(g);
		assert(d);
		halflives = d;
	}
}

void c2eCreature::adjustDrive(unsigned int id, float value) {
	assert(id < 20);
	drives[id] += value;

	if (drives[id] < 0.0f)
		drives[id] = 0.0f;
	else if (drives[id] > 1.0f)
		drives[id] = 1.0f;
}

/* vim: set noet: */
