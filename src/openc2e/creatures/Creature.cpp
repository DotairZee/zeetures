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
	world.history->tell(world.history->findMoniker(genome), storyEntry::stage, "", std::to_string((int)stage));
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
			world.history->tell(mon, storyEntry::born);
			// and in each parent's story: a child of theirs hatched
			for (const std::string* p : {&m.mother, &m.father})
				if (!p->empty())
					world.history->tell(*p, storyEntry::childHatched, mon);
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
			std::string cause = diedofoldage ? "old age" : !diedof.empty() ? diedof : "unknown causes";
			size_t before = world.history->notices.size();
			world.history->logLife(lifeLogEntry::death, mon, (int)stage, diedofoldage);
			if (world.history->notices.size() > before) {
				world.history->notices.back().cause = cause;
				world.history->lifelog.back().cause = cause;
			}
			world.history->tell(mon, storyEntry::died, "", cause);
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
	// each creature its own temperament (a saved world puts back its own)
	temperament = Temperaments::roll();
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
	keepInRoomSystem();

	if (dead != 0) {
		// the genome's own death signal (C2's "dead" locus): its chemistry
		// says it is dead
		if (diedof.empty())
			diedof = "illness";
		die();
	}

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
	// a creature that breathes water (owner, 2026-11-11: frog norns are
	// amphibious) keeps its air: no breath held, no bubbles
	if (underwater && breathesWater())
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
	static const bool fastBubbles = getenv("OPENC2E_TEST_BUBBLES") != nullptr; // developer aid: a bubble every few ticks
	if (fastBubbles)
		bubbletimer = 3;
	if (getenv("OPENC2E_DEBUG_BUBBLES"))
		if (SkeletalCreature* sc = dynamic_cast<SkeletalCreature*>(parentagent))
			fmt::print("* BUBBLE agent {:.0f},{:.0f} size {}x{} mouth {},{} neck {},{} -> {:.0f},{:.0f}\n", parentagent->x, parentagent->y, parentagent->getWidth(),
				parentagent->getHeight(), sc->attachmentX(1, 1), sc->attachmentY(1, 1), sc->attachmentX(1, 0), sc->attachmentY(1, 0), headx, heady);
	// marked as a breath bubble (OV01 7777): the world makes those rise
	// (World::tickBreathBubbles; the game's bubble object has no movement
	// of its own, so bubbles hung where they were blown while the creature
	// sank or walked on: owner, 2026-11-11)
	// where the bubble starts (owner, 2026-11-11): beside the head on the
	// side it faces, at its snout (SkeletalCreature::bubbleSpot); a creature
	// without a skeleton, near its middle top
	float bx = headx - 5, by = heady - 5;
	if (SkeletalCreature* sc = dynamic_cast<SkeletalCreature*>(parentagent))
		sc->bubbleSpot(bx, by);
	std::string caos = fmt::format("inst new: simp bubs 10 0 8000 0 setv cls2 2 1 27 setv ov01 7777 mvto {} {} mesg writ targ 8",
		(int)bx, (int)by);
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
	updateConsciousness();

	// developer aid: with OPENC2E_WATCH_CREATURES, organ health every 400 ticks
	static const bool watchorgans = getenv("OPENC2E_WATCH_CREATURES") != nullptr;
	if (watchorgans && ticks % 400 == 0 && !organs.empty()) {
		float minpct = 100.0f, sumpct = 0.0f;
		int deadorgans = 0;
		for (auto& o : organs) {
			float pct = o->getInitialLifeforce() > 0 ? 100.0f * o->getLongTermLifeforce() / o->getInitialLifeforce() : 0.0f;
			minpct = std::min(minpct, pct);
			sumpct += pct;
			if (o->getLongTermLifeforce() <= 0.5f)
				deadorgans++;
		}
		fmt::print("* WATCH ORGANS t{} unid {} atp {} min {:.1f}% mean {:.1f}% dead {}/{}\n", ticks, parentagent ? parentagent->getUNID() : -1,
			(int)chemicals[99], minpct, sumpct / organs.size(), deadorgans, organs.size());
	}

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
	keepInRoomSystem();

	if (dead != 0) {
		// the genome's own death signal (C2's "dead" locus): its chemistry
		// says it is dead
		if (diedof.empty())
			diedof = "illness";
		die();
	}

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

// A creature found outside every room (inside the ground, under a floor),
// not held and not riding, is lifted to the nearest room straight above it,
// a dry one if there is one within reach, and stood on its floor. An old norn
// swimming in the underground pond sank through its bed into the rock
// (owner, 2026-11-11); the floor check in Agent::physicsTick leaves water
// rooms alone, so nothing brought her back. Checked about twice a second.
void oldCreature::keepInRoomSystem() {
#ifndef _CREATURE_STANDALONE
	if (!parentagent || !world.map || parentagent->carriedby || parentagent->invehicle)
		return;
	if (keepcheck > 0) {
		keepcheck--;
		return;
	}
	keepcheck = 5;
	Agent* a = parentagent;
	float cx = a->x + a->getWidth() / 2.0f, cy = a->y + a->getHeight() / 2.0f;
	if (world.map->roomAt(cx, cy) || world.map->roomAt(cx, a->y + a->getHeight() - 2))
		return;
	std::shared_ptr<Room> dry, wet;
	float found = 0;
	for (float yy = cy - 10; yy > cy - 800 && !dry; yy -= 10) {
		std::shared_ptr<Room> r = world.map->roomAt(cx, yy);
		if (!r)
			continue;
		if (r->type == 2) {
			if (!wet)
				wet = r;
		} else {
			dry = r;
			found = yy;
		}
	}
	std::shared_ptr<Room> r = dry ? dry : wet;
	if (!r)
		return; // nothing above: leave it (the hand can still move it)
	(void)found;
	float span = std::max(1.0f, (float)r->x_right - (float)r->x_left);
	float bottom = r->y_left_floor + ((float)r->y_right_floor - (float)r->y_left_floor) * (cx - r->x_left) / span;
	if (r->floorpoints.size() && r->type != 2)
		bottom = std::min(bottom, r->floorYatX(cx));
	float oldx = a->x, oldy = a->y;
	a->moveTo(a->x, bottom - a->getHeight() - 1);
	a->velx = 0;
	a->vely = 0;
	fmt::print("* {} was inside the ground at {:.0f},{:.0f}; lifted to the floor of room {} ({}) at {:.0f},{:.0f}\n",
		world.history->findMoniker(genome), oldx, oldy, r->id, r->type == 2 ? "water" : "dry", a->x, a->y);
#endif
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
	if (now && parentagent && parentagent->babymoniker != 0)
		lastbaby = parentagent->babymoniker;
	// the pregnancy over: she has laid her egg. The status bar announces it
	// and goes to the egg (owner, 2026-11-11: a laid egg was lost). The egg
	// carries the baby's genome name (OBV0, from BABY, which the world's
	// laying script clears just before it makes the egg)
	if (pregnancywatched && !now && waspregnant && isAlive()) {
		std::string mon = world.history->findMoniker(genome);
		world.history->tell(mon, storyEntry::laidEgg);
#ifndef _CREATURE_STANDALONE
		std::string child = lastbaby ? std::string((const char*)&lastbaby, 4) : "";
		if (world.history->hasMoniker(mon)) {
			size_t before = world.history->notices.size();
			world.history->logLife(lifeLogEntry::eggLaid, mon, (int)stage);
			if (world.history->notices.size() > before) {
				world.history->notices.back().child = child;
				world.history->lifelog.back().child = child;
			}
		}
#endif
		lastbaby = 0;
	}
	waspregnant = now;
	pregnancywatched = true;
}

// Death and the rescue window (owner, 2026-11-11). A body out of energy
// (ATP, chemical 99, at 4 or less) for 300 ticks falls unconscious: it lies
// down (C2's own faint, creature event 70, run again as it ends) and its
// mind stops. While it is unconscious its organs go on failing at the rate
// its genome gives (the organs' damage at zero energy), so it can be saved
// only by mending the cause: out of the water, fed (food the hand drops on
// it), a Health Kit injection. Once ATP stays at 40 or more for 100 ticks
// it comes round. If an organ's lasting health reaches nothing first, it
// dies of it: "drowning" if it was under water and out of breath,
// "starvation" if hungry, else "organ failure".
void c2Creature::updateConsciousness() {
#ifndef _CREATURE_STANDALONE
	if (!alive || !parentagent)
		return;
	// death: an organ has failed
	for (auto& o : organs)
		if (o->getInitialLifeforce() > 0.0f && o->getLongTermLifeforce() <= 0.5f) {
			if (breathHeld >= breathTicks)
				diedof = "drowning";
			else if (drives[2] >= 216) // hunger (C2 drive 2) at 0.85 or more
				diedof = "starvation";
			else
				diedof = "organ failure";
			fmt::print("* {} died of {} (an organ failed)\n", world.history->findMoniker(genome), diedof);
			unconscious = false;
			die();
			return;
		}
	unsigned char atp = chemicals[99];
	std::string mon = world.history->findMoniker(genome);
	if (!unconscious) {
		noEnergyTicks = atp <= 4 ? noEnergyTicks + 1 : 0;
		if (noEnergyTicks >= 300) {
			unconscious = true;
			recoveringTicks = 0;
			asleep = false;
			parentagent->queueScript(70); // C2's faint: lying down, groaning
			world.history->logLife(lifeLogEntry::unconscious, mon, (int)stage);
			world.history->tell(mon, storyEntry::unconscious);
			fmt::print("* {} fell unconscious (out of energy)\n", mon);
		}
		return;
	}
	recoveringTicks = atp >= 40 ? recoveringTicks + 1 : 0;
	if (getenv("OPENC2E_WATCH_CREATURES") && ticks % 20 == 0)
		fmt::print("* WATCH UNCONSCIOUS {} atp {} recovering {} vm {}\n", mon, (int)atp, recoveringTicks, parentagent->vmStopped() ? "idle" : "running");
	if (recoveringTicks >= 100) {
		unconscious = false;
		noEnergyTicks = 0;
		world.history->logLife(lifeLogEntry::cameRound, mon, (int)stage);
		world.history->tell(mon, storyEntry::cameRound);
		fmt::print("* {} came round\n", mon);
		return;
	}
	// keep lying down: the faint again whenever it ends
	if (parentagent->vmStopped() && !parentagent->carriedby)
		parentagent->queueScript(70);
#endif
}

bool c2Creature::breathesWater() {
	if (waterbreather < 0) {
		waterbreather = 0;
		// C2's bodies get their oxygen (chemical 95) from air (96), which an
		// emitter makes from the air sense (creature, sensorimotor, locus 9,
		// which the engine empties under water). A body whose air or oxygen
		// does not depend on that sense breathes water: some emitter makes
		// air or oxygen from anything else (the Frog Norn pack moved its air
		// emitter to the always-on sense, locus 0), or nothing reads the air
		// sense at all. Read from the genome as it is, so old genomes need no
		// changes (owner, 2026-11-11).
		if (engine.version == 2 && genome) {
			bool fromAirSense = false, fromElsewhere = false, airSenseRead = false;
			for (auto& g : genome->genes) {
				auto* e = dynamic_cast<bioEmitterGene*>(g.get());
				if (!e)
					continue;
				bool airSense = e->organ == 1 && e->tissue == 4 && e->locus == 9;
				airSenseRead = airSenseRead || airSense;
				if (e->chemical == 95 || e->chemical == 96)
					(airSense ? fromAirSense : fromElsewhere) = true;
			}
			waterbreather = (fromElsewhere || !airSenseRead) ? 1 : 0;
			(void)fromAirSense;
			if (waterbreather)
				fmt::print("* {} breathes water as well as air (its genome's air does not come from the air sense)\n", world.history->findMoniker(genome));
		}
	}
	return waterbreather == 1;
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
