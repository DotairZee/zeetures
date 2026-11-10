/*
 *  SkeletalCreature.cpp
 *  openc2e
 *
 *  Created by Alyssa Milburn on Thu 10 Mar 2005.
 *  Copyright (c) 2005-2006 Alyssa Milburn. All rights reserved.
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
/* 20th feb 2005, by fuzzie
   with thanks to vadim for helping out

   TODO:
	* hair tidiness/untidiness (offset by 0 to 2 multiplied by 16)
	* render hairs/ears .. at the moment we avoid them, we lack zorder sanity too
	* support clothes parts
	* interpolate between differing poses, since c2e seems to
	* sanity checks: eg check that sprites have enough frames at load time
*/

#include "SkeletalCreature.h"

#include "BreedSlots.h"
#include "common/Random.h"

#include "Creature.h"
#include "Engine.h"
#include "Map.h"
#include "MetaRoom.h"
#include "PathResolver.h"
#include "Room.h"
#include "World.h"
#include "caosValue.h"
#include "common/backend/Backend.h"
#include "common/creaturesImage.h"
#include "common/io/FileReader.h"
#include "common/io/IOException.h"
#include "common/throw_ifnot.h"
#include "imageManager.h"

#include <tuple>
#include <array>
#include <cmath>
#include <cassert>
#include <stdint.h>
#include <fmt/core.h>
#include <map>
#include <memory>
#include <typeinfo> // TODO: remove when genome system is fixed

struct bodypartinfo {
	char letter;
	int parent;
	int attorder;
};

bodypartinfo cee_bodyparts[17] = {
	{'b', -1, -1}, // body
	{'a', 0, 0}, // head - attached to body
	{'c', 0, 1}, // left thigh - attached to body
	{'f', 0, 2}, // right thigh - attached to body
	{'i', 0, 3}, // left humerus - attached to body
	{'k', 0, 4}, // right humerus - attached to body
	{'m', 0, 5}, // tail root - attached to body
	{'d', 2, 1}, // left shin - attached to left thigh
	{'g', 3, 1}, // right shin - attached to right thigh
	{'j', 4, 1}, // left radius - attached to left humerus
	{'l', 5, 1}, // right radius - attached to right humerus
	{'e', 7, 1}, // left foot - attached to left shin
	{'h', 8, 1}, // right foot - attached to right shin
	{'n', 6, 1}, // tail tip - attached to tail root
	// Creatures Village only:
	{'o', 1, 2}, // left ear - attached to head
	{'p', 1, 3}, // right ear - attached to head
	{'q', 1, 4} // hair - attached to head
};

unsigned int cee_zorder[4][17] = {
	{6, 13, 2, 7, 11, 4, 9, 0, 14, 1, 16, 15, 5, 3, 8, 10, 12},
	{6, 13, 3, 8, 12, 5, 10, 0, 15, 1, 16, 14, 2, 7, 11, 4, 9},
	{6, 13, 2, 4, 9, 5, 3, 7, 8, 10, 0, 11, 12, 16, 14, 15, 1},
	{2, 4, 9, 1, 14, 15, 16, 5, 3, 7, 8, 10, 11, 12, 0, 6, 13}};

// needed for setPose(string) at least .. maybe cee_bodyparts should be indexed by letter
unsigned int cee_lookup[17] = {1, 0, 2, 7, 11, 3, 8, 12, 4, 9, 5, 10, 6, 13, 14, 15, 16};

int SkeletalPartCount() {
	if (engine.gametype == "cv")
		return 17;
	else
		return 14;
}

SkeletalCreature::SkeletalCreature(unsigned char _family)
	: Agent(_family, 0, 0, 0) {
	facialexpression = 0;
	pregnancy = 0;
	eyesclosed = false;

	// TODO: set zorder randomly :) should be somewhere between 1000-2700, at a /guess/
	zorder = 1500;

	ticks = 0;
	gaitgene = 0;

	calculated = false;

	if (engine.version == 1) {
		setAttributes(64 + 4 + 2); // mouseable, activateable, groundbound(?!)
	} else if (engine.version == 2) {
		setAttributes(128 + 64 + 4 + 2); // mouseable, activateable, suffersphysics, sufferscollisions
		// default values from original engine
		size = 224;
		rest = 70;
		accg = 15;
		aero = 10;
	}

	for (auto& appearancegene : appearancegenes)
		appearancegene = 0;

	skeleton = new SkeletonPart(this);
}

SkeletalCreature::~SkeletalCreature() {
	delete skeleton;
}

std::string SkeletalCreature::dataString(unsigned int _stage, bool tryfemale, unsigned int dataspecies, unsigned int databreed) {
	std::string s;
	s += (char)('0' + dataspecies + (tryfemale ? 4 : 0));
	s += (char)('0' + _stage);
	if (engine.version == 1)
		s += (char)('0' + databreed);
	else
		s += breedSlotName((int)databreed); // a-z, then aa, ab ... (BreedSlots.h)
	return s;
}

void SkeletalCreature::processGenes() {
	std::shared_ptr<genomeFile> genome = creature->getGenome();

	for (auto& gene : genome->genes) {
		if (!creature->shouldProcessGene(gene.get()))
			continue;

		if (creatureAppearanceGene* x = dynamic_cast<creatureAppearanceGene*>(gene.get())) {
			if (x->part > 5)
				continue;
			appearancegenes[x->part] = x;
		} else if (creaturePoseGene* x = dynamic_cast<creaturePoseGene*>(gene.get())) {
			posegenes[x->poseno] = x;
		} else if (creatureGaitGene* x = dynamic_cast<creatureGaitGene*>(gene.get())) {
			gaitgenes[x->drive] = x;
		}
	}
}

creatureAppearanceGene* SkeletalCreature::appearanceGeneForPart(char x) {
	// TODO: tail madness?

	if (x == 'a' || x >= 'o') {
		// head
		return appearancegenes[0];
	} else if (x == 'b') {
		// body
		return appearancegenes[1];
	} else if (x >= 'c' && x <= 'h') {
		// legs
		return appearancegenes[2];
	} else if (x >= 'i' && x <= 'l') {
		// arms
		return appearancegenes[3];
	} else if (x == 'm' || x == 'n') {
		// tail
		return appearancegenes[4];
	}

	return 0;
}


// Life-stage scaling for breeds drawn at one age only (owner, 2026-11-10).
// The engine picks a creature's sprites by life stage, falling back to the
// nearest younger age it has, so a breed with only one set (Bony grendels,
// C2's own ettins) looks the same from hatching to old age. With
// OPENC2E_STAGE_SCALE set, such a breed's pictures (and with them every
// joint point in its body data, so the parts still meet) are scaled to the
// growth of a reference breed: the same species' breed with the most ages,
// or else the standard grendels (the owner's guideline). The age a one-set
// breed was drawn at is the reference age nearest its size; with no
// reference of its own species, it is taken as drawn adult.
static bool stageScaleOn() {
	static const bool on = getenv("OPENC2E_STAGE_SCALE") != nullptr;
	return on;
}

// the average frame size (mean of width and height) of a breed's body
// sprite at an age, or 0 if there is none
static float bodySetSize(int species, bool female, int age, int breed) {
	static std::map<std::tuple<int, bool, int, int>, float> cache;
	auto key = std::make_tuple(species, female, age, breed);
	if (auto it = cache.find(key); it != cache.end())
		return it->second;
	float size = 0;
	for (bool f : {female, false}) {
		std::string name = "b";
		name += (char)('0' + species + (f ? 4 : 0));
		name += (char)('0' + age);
		name += breedSlotName(breed);
		if (auto img = world.gallery->getImage(name)) {
			float sum = 0;
			unsigned int n = img->numframes();
			for (unsigned int i = 0; i < n; i++)
				sum += (img->width(i) + img->height(i)) / 2.0f;
			size = n ? sum / n : 0;
			break;
		}
		if (!f)
			break;
	}
	cache[key] = size;
	return size;
}

// The reference growth for a species: its breed with the most ages drawn
// (at least two), else the grendels'. ages[a] is that breed's body size at
// age a, 0 where it has none.
struct StageGrowth {
	int species = -1, breed = -1;
	std::array<float, 7> ages{};
	float at(int stage) const { // the size the engine would show at a stage
		for (int a = std::min(stage, 6); a >= 0; a--)
			if (ages[a] > 0)
				return ages[a];
		return 0;
	}
};
static const StageGrowth& stageGrowth(int species) {
	static std::map<int, StageGrowth> cache;
	if (auto it = cache.find(species); it != cache.end())
		return it->second;
	StageGrowth best;
	int bestcount = 1;
	// the species' standard breed, when it has several ages: C2's own norn
	// is breed h (norn.gen), its grendel breed a
	const int standard = species == 0 ? 7 : 0;
	{
		StageGrowth g;
		int count = 0;
		for (int a = 0; a <= 6; a++)
			if ((g.ages[a] = bodySetSize(species, false, a, standard)) > 0)
				count++;
		if (count >= 2) {
			g.species = species;
			g.breed = standard;
			return cache[species] = g;
		}
	}
	for (int breed = 0; breed < 26; breed++) {
		StageGrowth g;
		int count = 0;
		for (int a = 0; a <= 6; a++)
			if ((g.ages[a] = bodySetSize(species, false, a, breed)) > 0)
				count++;
		if (count > bestcount || (breed == 0 && count >= 2)) {
			bestcount = count;
			best = g;
			best.species = species;
			best.breed = breed;
			if (breed == 0)
				break; // the standard breed (a) is the guideline when it has ages
		}
	}
	if (best.breed < 0 && species != 1)
		best = stageGrowth(1); // the standard grendels as the guideline
	return cache[species] = best;
}

// the factor for a part from a breed with only one set of pictures, at a
// life stage (1 for breeds drawn at more than one age)
static float stageScale(int species, bool female, int breed, int stage) {
	if (!stageScaleOn() || engine.version != 2)
		return 1.0f;
	// grendels and ettins (owner); norns (C2's own frog norn, breed l) only
	// with OPENC2E_STAGE_SCALE=norns, until the owner has seen previews
	static const bool norns = getenv("OPENC2E_STAGE_SCALE") && std::string(getenv("OPENC2E_STAGE_SCALE")) == "norns";
	if (species != 1 && species != 2 && !(species == 0 && norns))
		return 1.0f;
	float own = bodySetSize(species, female, 0, breed);
	if (own <= 0)
		return 1.0f;
	for (int a = 1; a <= 6; a++)
		if (bodySetSize(species, female, a, breed) > 0)
			return 1.0f; // drawn at more than one age: its own pictures
	const StageGrowth& g = stageGrowth(species);
	if (g.breed < 0)
		return 1.0f;
	// the age it was drawn at: the reference age nearest its size, or adult
	int drawn = -1;
	if (g.species == species) {
		float bestd = 1e9f;
		for (int a = 0; a <= 6; a++)
			if (g.ages[a] > 0 && std::fabs(std::log(own / g.ages[a])) < bestd) {
				bestd = std::fabs(std::log(own / g.ages[a]));
				drawn = a;
			}
	} else {
		for (int a = 6; a >= 0 && drawn < 0; a--)
			if (g.ages[a] > 0)
				drawn = a;
	}
	if (drawn < 0 || g.at(drawn) <= 0)
		return 1.0f;
	static std::map<std::tuple<int, bool, int>, bool> logged;
	if (!logged[std::make_tuple(species, female, breed)]) {
		logged[std::make_tuple(species, female, breed)] = true;
		std::string f;
		for (int st = 0; st <= 6; st++)
			f += fmt::format(" {:.2f}", g.at(st) / g.at(drawn));
		fmt::print("* STAGE SCALE species {} {} breed {}: one set (body size {:.0f}), drawn as age {} of reference species {} breed {}; by stage 0-6:{}\n",
			species, female ? "female" : "male", breedSlotName(breed), own, drawn, g.species, breedSlotName(g.breed), f);
	}
	return g.at(stage) / g.at(drawn);
}

void SkeletalCreature::skeletonInit() {
	// TODO: the exception throwing in here needs some more thought
	// TODO: if we throw an exception when we need to kill the creature off, else segfault :/

	for (int i = 0; i < SkeletalPartCount(); i++) { // CV hackery
		if (engine.version == 1) // TODO: this is hackery to skip tails for C1
			if (i == 6 || i == 13)
				continue;

		// reset the existing image reference
		images[i].reset();

		// find the relevant gene
		char x = cee_bodyparts[i].letter;
		creatureAppearanceGene* partapp = appearanceGeneForPart(x);

		int partspecies, partvariant;
		partspecies = creature->getGenus();
		if (partapp) {
			if (engine.version > 1)
				partspecies = partapp->species;
			partvariant = partapp->variant;
		} else {
			partvariant = creature->getVariant();
		}

		int spe = partspecies;

		/*
		 * In order to find a sprite, we try our current stage first, then the stages below us,
		 * and then we try again with all lower variants, and if that fails for female sprites
		 * then we try the whole thing again but trying to find a male sprite.
		 */

		// find relevant sprite
		bool tryfemale = creature->isFemale();
		// (the file found, so its body data can be the matching one)
		int foundstage = -1, foundvar = -1;
		bool foundfemale = false;
		while (!images[i]) {
			int var = partvariant;
			while (var > -1 && !images[i]) {
				int stage_to_try = creature->getStage();
				while (stage_to_try > -1 && !images[i]) {
					images[i] = world.gallery->getImage(x + dataString(stage_to_try, tryfemale, spe, var));
					if (images[i]) {
						foundstage = stage_to_try;
						foundvar = var;
						foundfemale = tryfemale;
					}
					stage_to_try--;
				}
				var--;
			}
			if (!tryfemale)
				break;
			tryfemale = false;
		}
		if (!images[i])
			throw Exception(fmt::format("SkeletalCreature couldn't find an image for part {:c} of species {}, variant {}, stage {}", x, (int)partspecies, (int)partvariant, (int)creature->getStage()));

		// find relevant ATT data: the body data that goes with the sprite
		// found (same sex, age and breed). Looking it up as male only gave
		// female golden norns (breed k, whose male and female heads differ
		// in size and neck point) the male neck points: heads rode high.
		decltype(findBodyDataFile("")) attpath;
		if (foundstage >= 0)
			attpath = findBodyDataFile(x + dataString(foundstage, foundfemale, spe, foundvar) + ".att");
		int var = partvariant;
		while (var > -1 && attpath.empty()) {
			int stage_to_try = creature->getStage();
			while (stage_to_try > -1 && attpath.empty()) {
				attpath = findBodyDataFile(x + dataString(stage_to_try, false, spe, var) + ".att");
				stage_to_try--;
			}
			var--;
		}
		if (attpath.empty())
			throw Exception(fmt::format("SkeletalCreature couldn't find body data for part {:c} of species {}, variant {}, stage {}", x, (int)partspecies, (int)partvariant, creature->getStage()));

		// load ATT file
		try {
			FileReader attfile(attpath);
			att[i] = ReadAttFile(attfile);
		} catch (const IOException&) {
			throw Exception(fmt::format("SkeletalCreature couldn't load body data for part {:c} of species {}, variant {}, stage {} (tried file {})", x, (int)partspecies, (int)partvariant, creature->getStage(), attpath.string()));
		}

		images[i] = tintBodySprite(images[i]);

		// a breed drawn at one age only, scaled to the life stage: the
		// pictures and every joint point together, so the parts still meet
		float k = stageScale(spe, foundfemale, foundvar, creature->getStage());
		if (std::fabs(k - 1.0f) > 0.01f) {
			images[i] = world.gallery->scaled(images[i], k);
			for (auto& pose : att[i].attachments)
				for (auto& v : pose)
					v = (unsigned int)std::lround(v * k);
		}
	}
}

std::shared_ptr<creaturesImage> SkeletalCreature::tintBodySprite(std::shared_ptr<creaturesImage> s) {
	// TODO: don't bother tinting if we don't need to

	// TODO: work out tinting for other engine versions
	if (engine.version > 2) {
		std::shared_ptr<creaturesImage> newimage = world.gallery->tint(s, creature->getTint(0), creature->getTint(1), creature->getTint(2), creature->getTint(3), creature->getTint(4));
		return newimage;
	}

	return s;
}

void SkeletalCreature::render(RenderTarget* renderer, int xoffset, int yoffset) {
	bool mirror_body_parts = (world.variables["engine_mirror_creature_body_parts"] == 1);

	for (int j = 0; j < 17; j++) {
		int i = cee_zorder[posedirection][j];
		if (i >= SkeletalPartCount())
			continue; // CV hackery

		if (engine.version == 1) // TODO: this is hackery to skip tails for C1
			if (i == 6 || i == 13)
				continue;

		bodypartinfo* part = &cee_bodyparts[i];

		unsigned int ourpose = pose[i];

		bool mirror = false;
		if (i != 14 && i != 15 && mirror_body_parts && ourpose >= 4 && ourpose <= 7) {
			ourpose -= 4;
			mirror = true;
		}

		// adjust for pregnancy/facial expressions/etc as necessary
		if (part->parent == -1) { // body
			ourpose += (pregnancy * (engine.version < 3 ? 10 : 16));
		} else if (i == 1) { // head
			ourpose += ((eyesclosed || blinkticks > 0) ? (engine.version < 3 ? 10 : 16) : 0) + (facialexpression * (engine.version < 3 ? 20 : 32));
		} else if (i == 16) { // hair
			ourpose += 0; // TODO: 16 * hair
		}

		assert(images[i]);

		RenderOptions render_opts;
		render_opts.mirror = mirror;
		renderer->renderCreaturesImage(images[i], ourpose, partx[i] + adjustx + xoffset, party[i] + adjusty + yoffset, render_opts);

		if (displaycore) {
			// TODO: we draw a lot of points twice here :)
			Color color{0xff, 0x1e, 0xff, 0xcc};
			int atx = attachmentX(i, 0) + xoffset, aty = attachmentY(i, 0) + yoffset;
			renderer->renderLine(atx - 1, aty, atx + 1, aty, color);
			renderer->renderLine(atx, aty - 1, atx, aty + 1, color);
			atx = attachmentX(i, 1) + xoffset;
			aty = attachmentY(i, 1) + yoffset;
			renderer->renderLine(atx - 1, aty, atx + 1, aty, color);
			renderer->renderLine(atx, aty - 1, atx, aty + 1, color);
		}
	}
}

float SkeletalCreature::footX() {
	float fx = x + attachmentX(downfoot_left ? 11 : 12, 1);
	if (world.map) {
		MetaRoom* m = world.map->metaRoomAt(x, y);
		if (!m)
			m = world.map->wrappingMetaRoomNear(fx, y);
		if (m && m->wraparound()) {
			if (fx >= (int)m->x() + (int)m->width())
				fx -= m->width();
			else if (fx < (int)m->x())
				fx += m->width();
		}
	}
	return fx;
}

int SkeletalCreature::attachmentX(unsigned int part, unsigned int id) {
	return partx[part] + att[part].attachments[pose[part]][0 + (id * 2)] + adjustx;
}

bool SkeletalCreature::headTop(float& hx, float& hy) {
	if (!images[1])
		return false;
	hx = x + partx[1] + adjustx + images[1]->width(pose[1]) / 2.0f;
	hy = y + party[1] + adjusty;
	return true;
}

int SkeletalCreature::attachmentY(unsigned int part, unsigned int id) {
	return party[part] + att[part].attachments[pose[part]][1 + (id * 2)] + adjusty;
}

void SkeletalCreature::recalculateSkeleton() {
	int lowestx = 0, lowesty = 0, highestx = 0, highesty = 0;

	for (int i = 0; i < SkeletalPartCount(); i++) {
		if (engine.version == 1) // TODO: this is hackery to skip tails for C1
			if (i == 6 || i == 13)
				continue;

		bodypartinfo* part = &cee_bodyparts[i];

		if (part->parent == -1) {
			partx[i] = 0;
			party[i] = 0;
		} else {
			attFile& bodyattinfo = att[0];
			// attFile &attinfo = att[i];

			int attachx = att[i].attachments[pose[i]][0];
			int attachy = att[i].attachments[pose[i]][1];
			int x, y;

			if (part->parent == 0) { // linking to body
				x = bodyattinfo.attachments[pose[0]][part->attorder * 2];
				y = bodyattinfo.attachments[pose[0]][(part->attorder * 2) + 1];
			} else { // extra limb
				attFile& parentattinfo = att[part->parent];

				x = partx[part->parent] + parentattinfo.attachments[pose[part->parent]][part->attorder * 2];
				y = party[part->parent] + parentattinfo.attachments[pose[part->parent]][(part->attorder * 2) + 1];
			}

			x = x - attachx;
			y = y - attachy;

			partx[i] = x;
			party[i] = y;

			if (x < lowestx) {
				lowestx = x;
			}
			if (y < lowesty) {
				lowesty = y;
			}
			if (x + (int)images[i]->width(pose[i]) > highestx) {
				highestx = x + images[i]->width(pose[i]);
			}
			if (y + (int)images[i]->height(pose[i]) > highesty) {
				highesty = y + images[i]->height(pose[i]);
			}
		}
	}

	adjustx = -lowestx;
	adjusty = -lowesty;

	// TODO: muh, we should cooperate with physics system etc
	if (!carriedby && !invehicle && calculated) {
		int orig_footpart = (downfoot_left ? 11 : 12);
		float oldbottom = y + height; // height is the lower foot's y, from the last calculation
		// adjust location to match foot
		moveTo(x - (attachmentX(orig_footpart, 1) - oldfootx), y - (attachmentY(orig_footpart, 1) - oldfooty));
		if (engine.version == 2 && falling) {
			// While physics is in charge (falling, landing, bouncing), nothing snaps the
			// feet back to a floor, and keeping the previous down foot fixed lets the
			// other foot reach lower in the new pose. That pushed the body's bottom edge
			// through floors into the room below (e.g. through the pond bottom), where
			// physics then let the creature keep falling. Never let a pose change move
			// the lowest foot down while falling.
			float newlowest = std::max(attachmentY(11, 1), attachmentY(12, 1));
			if (y + newlowest > oldbottom)
				moveTo(x, oldbottom - newlowest);
		}
	}

	// work out which foot is down
	int leftfoot = attachmentY(11, 1);
	int rightfoot = attachmentY(12, 1);
	downfoot_left = (rightfoot < leftfoot);

	calculated = true;
	int orig_footpart = (downfoot_left ? 11 : 12);
	oldfootx = attachmentX(orig_footpart, 1);
	oldfooty = attachmentY(orig_footpart, 1);

	// recalculate width/height
	height = downfoot_left ? leftfoot : rightfoot;
	width = 50; // TODO: arbitary values bad

	// what it carries goes in front of or behind it with the hand (re-sorted
	// for drawing when that changes)
	if (bool front = carriesInFront(); front != carriedFront) {
		carriedFront = front;
		if (carrying)
			carrying->setZOrder(carrying->zorder);
	}

	// TODO: muh, we should cooperate with physics system etc
	/*if (carriedby || invehicle)
		downfootroom.reset();
	else
		snapDownFoot();*/
}

void SkeletalCreature::snapDownFoot() {
	// TODO: this isn't very well thought-out.

	int orig_footpart = (downfoot_left ? 11 : 12);
	float footx = x + attachmentX(orig_footpart, 1);
	float footy = y + attachmentY(orig_footpart, 1);

	MetaRoom* m = world.map->metaRoomAt(x, y);
	if (!m) {
		// past the edge of a wrapping world, which has no metaroom of its own
		// out there: find the wrapping metaroom it belongs to
		m = world.map->wrappingMetaRoomNear(footx, y);
	}
	if (!m)
		return; // TODO: exceptiony death

	// In a wrapping world (C2's Albia joins its east and west edges) openc2e
	// wraps an agent by its top-left corner, so a creature near the east edge
	// can have its foot (about 50 px right of that corner) past the map. Judge
	// rooms and floors with the foot's x wrapped into the map. Without this,
	// room lookups (which wrap on their own) gave such a creature the rooms at
	// the west end while its floors were measured at the out-of-range x, and
	// one fell through a solid floor and out of the room system.
	float wrapshift = 0;
	auto wrapFoot = [&](float fx) {
		if (m->wraparound()) {
			if (fx >= (int)m->x() + (int)m->width())
				return -(float)m->width();
			if (fx < (int)m->x())
				return (float)m->width();
		}
		return 0.0f;
	};
	wrapshift = wrapFoot(footx);
	footx += wrapshift;

	std::shared_ptr<Room> newroom;

	if (downfootroom) {
		if (downfootroom->containsPoint(footx, footy)) {
			newroom = downfootroom;
		} else {
			if (downfootroom->x_left <= footx && downfootroom->x_right >= footx) {
				newroom = downfootroom; // TODO, we're just forcing for now
			} else {
				float ydiff = 10000.0f; // TODO: big number
				const float unwrappedfootx = footx;
				for (auto& d : downfootroom->getDoors()) {
					std::shared_ptr<Room> thisroom = d.first;
					// A wrapping world (C2's Albia joins its east and west edges
					// through doors) has rooms just past the seam at the other end:
					// judge them with the foot's x wrapped to that end, or a creature
					// walking off the edge finds no room and is pushed back.
					footx = unwrappedfootx;
					if (m->wraparound()) {
						if (footx >= m->x() + (int)m->width() && thisroom->x_left < m->x() + (int)m->width() / 2)
							footx -= m->width();
						else if (footx < m->x() && thisroom->x_right > m->x() + (int)m->width() / 2)
							footx += m->width();
					}
					if (engine.version == 2 && size > d.second.perm)
						continue;
					// From dry ground a water room is a wall: creatures fall or are
					// dropped into water but never walk in (they walk out of it, up
					// the pond's ramp). Which way a walk action goes depends on the
					// creature's poses, so the mind's own check missed some: a norn
					// walked off the incubator bank into the pond and was trapped at
					// its far end (regression batch reg5).
					if (engine.version == 2 && thisroom->type == 2 && downfootroom->type != 2)
						continue;
					// A neighbouring floor far above the foot is a wall, not a step. Without
					// this, a creature at an edge whose level exit is closed to it (e.g. the
					// garden's left edge, where the door down to the watermill ramp is too
					// small for creatures) was moved into the open-air room above and left
					// to fall, dropping it into the pit below.
					// A room whose floor line can be walked beneath (floor value above
					// 0: a lift shaft whose bottom is the lift's lower stop) is judged
					// by its bottom edge, as walking in it is (below): its line at the
					// top of the shaft made the shaft's doorway a wall, so creatures
					// stepped in place at the desert lift's foot (owner, 2026-11-09).
					float thisfloor = thisroom->floorYatX(footx);
					if (engine.version == 2 && thisroom->floorvalue > 0 && thisfloor < footy - 40)
						thisfloor = thisroom->bottom.pointAtX(footx).y;
					if (engine.version == 2 && thisfloor < footy - 40)
						continue;
					if (thisroom->x_left <= footx && thisroom->x_right >= footx) {
						float thisydiff = fabs(footy - thisfloor);
						if (thisydiff < ydiff) {
							newroom = thisroom;
							ydiff = thisydiff;
						}
					}
				}
				footx = unwrappedfootx;
				// (crossing the seam needs no move here: the foot is judged
				// wrapped above, and moveTo wraps the creature itself)
			}
		}
	} else {
		newroom = bestRoomAt(footx, footy, 3, m, std::shared_ptr<Room>());

		// insane emergency handling
		float newfooty = footy;
		while (!newroom && newfooty > (footy - 500.0f)) {
			newroom = m->roomAt(footx, newfooty);
			newfooty--;
		}

		// TODO: give up here

		footy = newfooty;
	}

	bool newroomchosen = (newroom != downfootroom) && downfootroom;
	bool hadroom = downfootroom != nullptr;
	// developer aid: OPENC2E_WATCH_ROOMS prints each change of a creature's
	// foot room and which way it was found
	static const bool watchrooms = getenv("OPENC2E_WATCH_ROOMS") != nullptr;
	auto watchRoom = [&](const char* how, std::shared_ptr<Room> from, std::shared_ptr<Room> to) {
		if (watchrooms && from != to)
			fmt::print("* WATCH ROOM t{} unid {} {} foot ({:.0f}, {:.0f}) x {:.0f} room {} -> {}\n", world.tickcount, unid, how,
				footx, footy, x, from ? (int)from->id : -1, to ? (int)to->id : -1);
	};
	watchRoom(hadroom ? "walk" : "place", downfootroom, newroom);
	std::shared_ptr<Room> prevroom = downfootroom;
	downfootroom = newroom;

	if (!downfootroom /*|| !falling */) {
		// TODO: hackery to cope with scripts moving us, this needs handling correctly somewhere
		// how far the foot is from where it last stood (across the seam of a
		// wrapping world, the short way round)
		float movedx = lastgoodfootx - attachmentX(orig_footpart, 1) - x;
		if (m->wraparound())
			movedx -= std::round(movedx / (float)m->width()) * (float)m->width();
		float movedy = lastgoodfooty - attachmentY(orig_footpart, 1) - y;
		// A short jump of the foot into a room that the room it stood in has
		// no doorway to (open at the creature's size) is a wall, not a script
		// moving it: at the world's seam a grendel turning at the library's
		// west wall had its foot shift ~58 px west across the seam, and was
		// taken into the sealed pocket beyond (room 213), where it fell to the
		// map's bottom and was trapped (owner, 2026-11-10).
		bool walled = false;
		if (engine.version == 2 && prevroom && fabs(movedx) <= 150.0f && fabs(movedy) <= 150.0f) {
			std::shared_ptr<Room> here = bestRoomAt(footx, footy, 3, m, std::shared_ptr<Room>());
			if (here && here != prevroom && (!world.map->hasDoor(prevroom, here) || world.map->getDoorPerm(prevroom, here) < size)) {
				walled = true;
				watchRoom("walled", here, prevroom);
			}
		}
		if (walled)
			; // pushed back below
		else if (fabs(movedx) > 50.0f || fabs(movedy) > 50.0f) {
			// Retry once from the room the foot is in now. Only once: if the
			// retry also finds no room it may enter (e.g. every way on is a
			// wall), retrying again recursed until the stack overflowed and the
			// game vanished without a trace. After one retry, accept the room
			// the foot is in (a script moved us; pushing back would undo it).
			static bool retrying = false;
			std::shared_ptr<Room> here = bestRoomAt(footx, footy, 3, m, std::shared_ptr<Room>());
			if (here && !retrying) {
				watchRoom("retry", downfootroom, here);
				downfootroom = here;
				retrying = true;
				snapDownFoot();
				retrying = false;
				return;
			} else if (!here) {
				fmt::print(stderr, "Creature out of room system at ({}, {})!\n", footx, footy);
				// TODO: exceptiony death?
				return;
			}
			watchRoom("accept", downfootroom, here);
			downfootroom = here;
		}
	}

	if (!downfootroom) {
		// We fell out of the room system! How did that happen? Push ourselves back in, run collision script.
		// fmt::print("Creature out of room system at ({}, {}), pushing it back in.\n", footx, footy);

		// TODO: sucky code
		x = lastgoodfootx - attachmentX(orig_footpart, 1);
		wrapshift = wrapFoot(lastgoodfootx);
		footx = lastgoodfootx + wrapshift;
		footy = lastgoodfooty;
		downfootroom = m->roomAt(footx, footy);
		queueScript(6);

		if (!downfootroom) {
			fmt::print(stderr, "no down foot room! ({}, {})\n", footx, footy);
			// TODO: exceptiony death
			return;
		}
	}

	bool belowfloor = false;
	float newy = downfootroom->floorYatX(footx);
	if (engine.version == 2 && hadroom && y > newy && downfootroom->floorvalue > 0) {
		// TODO: hilar hack: cope with walking below floors.
		// C2 rooms with floor points have a ground line inside the room. Where that
		// ground is passable (a floor value above 0, e.g. a lift shaft whose bottom
		// is the stop below the ground), creatures may walk along the room's real
		// bottom beneath it. Where the floor value is 0 the ground is solid, nothing
		// is meant to be beneath it, and pinning a creature to the bottom buried it
		// in the scenery; there the creature stays on (or returns to) the ground line.
		belowfloor = true;
		if (getenv("OPENC2E_WATCH_CREATURES"))
			fmt::print("* WATCH BELOWFLOOR t{} unid {} foot ({:.0f}, {:.0f}) room {} floor {:.0f} -> room bottom {:.0f}\n",
				world.tickcount, unid, footx, footy, downfootroom->id, newy, downfootroom->bottom.pointAtX(footx).y);
		newy = downfootroom->bottom.pointAtX(footx).y;
	}

	if (engine.version > 1) {
		// TODO: hilar hack: enable gravity if we're snapping by much
		// (down only: a step up into the next room, up to the 40 px the door
		// rule above allows, is taken; as a fall, a creature at the top of the
		// boardwalk pool's bank fell back each time from the 33 px step onto
		// the planks, owner 2026-11-10)
		if (newroomchosen && (newy - (footy - y)) - y > 20) {
			falling = true;
			return;
		}
	}

	moveTo(x, newy - (footy - y));

	lastgoodfootx = footx - wrapshift; // kept in the creature's own (unwrapped) terms
	lastgoodfooty = footy;

	// Falling through a floor a creature is light enough to pass (C2: its size
	// against the floor value or the permeability of the door below). A 2008
	// change meant to extend this to C3 wrote "> 2" instead of ">= 2", so C2
	// creatures never fell through any floor: they stood on sea surfaces.
	if (engine.version >= 2) {
		if (engine.version == 2 && !belowfloor && downfootroom->floorpoints.size()) {
			// TODO: hilar hack: same as above for floorvalue
			if (size <= downfootroom->floorvalue) {
				falling = true;
				return;
			}
		} else {
			// TODO: hilar hack: same as above for perm
			std::shared_ptr<Room> downroom = world.map->roomAt(footx, downfootroom->y_left_floor + 1);
			if (world.map->hasDoor(downfootroom, downroom)) {
				int permsize = (engine.version == 2 ? size : perm);
				if (permsize <= world.map->getDoorPerm(downfootroom, downroom)) {
					falling = true;
					return;
				}
			}
		}
	}
}

void SkeletalCreature::setPose(unsigned int p) {
	posedirection = 0;
	for (int i = 0; i < SkeletalPartCount(); i++)
		pose[i] = p;
	recalculateSkeleton();
}

void SkeletalCreature::setPose(std::string s) {
	switch (s[0]) {
		case '?': // towards object of attention
			switch (direction) {
				case 0: posedirection = 3; break; // north, TODO
				case 1: posedirection = 2; break; // south, TODO
				case 2: posedirection = 0; break; // right
				case 3: posedirection = 1; break; // left
				default: assert(false);
			}
			break;
		case '!': // away from object of attention
			switch (direction) {
				case 0: posedirection = 2; break; // north, TODO
				case 1: posedirection = 3; break; // south, TODO
				case 2: posedirection = 1; break; // right
				case 3: posedirection = 0; break; // left
				default: assert(false);
			}
			break;
		case '0': posedirection = 3; break;
		case '1': posedirection = 2; break;
		case '2': posedirection = 0; break;
		case '3': posedirection = 1; break;
		case 'X': break; // do nothing
		default:
			fmt::print("internal warning: SkeletalCreature::setPose didn't understand direction {} in pose '{}'.\n", s[0], s);
			break;
	}

	for (int i = 0; i < 14; i++) {
		int newpose = -1;

		switch (s[i + 1]) {
			case '0': newpose = 0; break;
			case '1': newpose = 1; break;
			case '2': newpose = 2; break;
			case '3': newpose = 3; break;
			case '4': newpose = (engine.version < 3) ? 8 : 10; break; // 'to camera'
			case '?':
				assert(i == 0);
				{ // TODO
					// make the head look in the posedirection of _IT_
					float attachmenty = attachmentY(1, 0) + y; // head attachment point, which we'll use to 'look' from atm

					// TODO: this is horrible, but i have no idea how the head angle is calculated
					AgentRef attention = creature->getAttentionFocus();
					if (attention && attention->y > (attachmenty + 30))
						newpose = 0;
					else if (attention && attention->y < (attachmenty - 70))
						newpose = 3;
					else if (attention && attention->y < (attachmenty - 30))
						newpose = 2;
					else
						newpose = 1;
				}
				break;
			// TODO: '!' also?
			case 'X': continue; // do nothing
			default:
				fmt::print("internal warning: SkeletalCreature::setPose didn't understand {} in pose '{}'.\n", s[i + 1], s);
				continue;
		}

		assert(newpose != -1);

		if (newpose < 4) {
			// newpose gives the angle, now we need to add the offset for left/right/forward/back.

			if (engine.version < 3 && posedirection > 1)
				newpose = 6 + posedirection; // only one forward/back pose in c1/c2
			else
				newpose += (posedirection * 4);
		}

		pose[cee_lookup[i]] = newpose;

		// TODO: this is some hackery for CV,
		if (engine.gametype != "cv")
			continue;
		if (i == 0) { // head
			pose[14] = newpose;
			pose[15] = newpose; // ears
			pose[16] = newpose; // hair
		} else if (i == 1) {
			pose[6] = newpose; // tail root
			pose[13] = newpose; // tail tip
		}
	}

	recalculateSkeleton();
}

void SkeletalCreature::setPoseGene(unsigned int poseno) {
	std::map<unsigned int, creaturePoseGene*>::iterator i = posegenes.find(poseno);
	if (i == posegenes.end())
		return; // TODO: is there a better behaviour here?

	creaturePoseGene* g = i->second;
	assert(g->poseno == poseno);
	gaitgene = 0;
	walking = false; // TODO: doesn't belong here, does it? really the idea of a 'walking' bool is horrid
	setPose(g->getPoseString());
}

void SkeletalCreature::setGaitGene(unsigned int gaitdrive) { // TODO: not sure if this is *useful*
	std::map<unsigned int, creatureGaitGene*>::iterator i = gaitgenes.find(gaitdrive);
	if (i == gaitgenes.end())
		return; // TODO: is there a better behaviour here?

	creatureGaitGene* g = i->second;
	assert(g->drive == gaitdrive);

	if (g == gaitgene)
		return;

	// reset our gait details to default
	gaitgene = g;
	gaiti = 0;
	skeleton->animation.clear();
}

void SkeletalCreature::tick() {
	Agent::tick();

	if (paused)
		return;

	CreatureAgent::tick();

	eyesclosed = creature->isAsleep() || !creature->isAlive();

	// blink every 3-8 seconds (C2 ticks 10 times a second), eyes shut for
	// about a fifth of a second (the head picture is chosen when drawing)
	if (!eyesclosed) {
		if (blinkticks > 0)
			blinkticks--;
		else if (--blinkwait <= 0) {
			blinkticks = 2;
			blinkwait = rand_int32(30, 80);
		}
	}

	// TODO: every 2 ticks = correct? what about the engine var?
	ticks++;
	if (ticks % 2 == 0)
		return;

	//if (eyesclosed) return; // TODO: hack, this is wrong :)

	// TODO: hack!
	if (!eyesclosed && !creature->isZombie()) {
		if (approaching && approachtarget) {
			// TODO: more sane approaching skillz
			if (approachtarget->x < x)
				direction = 3;
			else
				direction = 2;
		}

		if (walking || approaching) {
			// TODO: we shouldn't bother with this unless it changed?
			setGaitGene(creature->getGait());

			// TODO: we should only do this if we're moving :-P
			gaitTick();
		}

		approaching = false;
	}

	// TODO: this kinda duplicates what physicsTick is doing below
	if (shoveticks > 0) {
		if (!falling && !carriedby && !invehicle) {
			float step = 2.0f + shoveticks * 1.2f; // 14 px, easing to 3
			moveTo(x + shovedir * step, y);
		}
		shoveticks--;
	}

	if ((engine.version == 1 || !falling) && !carriedby && !invehicle)
		snapDownFoot();
	else
		downfootroom.reset();
}

void SkeletalCreature::physicsTick() {
	// TODO: mmh

	if (engine.version > 1 && falling) {
		if (validInRoomSystem() || engine.version == 2) {
			Agent::physicsTick();
		} else if (!validInRoomSystem()) {
			falling = false;
		}
	}

	if ((engine.version == 1 || !falling) && !carriedby && !invehicle)
		snapDownFoot();
	else
		downfootroom.reset();

	// developer aid: OPENC2E_WATCH_CREATURES logs any standing creature whose
	// foot is below the floor of its foot room, and its position every 200 ticks
	static const bool watch = getenv("OPENC2E_WATCH_CREATURES") != nullptr;
	if (watch && !carriedby && !invehicle) {
		int fp = downfoot_left ? 11 : 12;
		float footx = x + attachmentX(fp, 1), footy = y + attachmentY(fp, 1);
		if (!falling && downfootroom && downfootroom->floorvalue == 0) {
			float floory = downfootroom->floorYatX(footx);
			if (footy > floory + 3)
				fmt::print("* WATCH SINK t{} unid {} genus {} foot ({:.0f}, {:.0f}) room {} floor {:.0f} depth {:.0f}\n",
					world.tickcount, unid, (int)genus, footx, footy, downfootroom->id, floory, footy - floory);
		}
		static std::map<int, int> lastroom;
		int nowroom = downfootroom ? (int)downfootroom->id : -1;
		auto lr = lastroom.find(unid);
		if (lr != lastroom.end() && lr->second != nowroom)
			fmt::print("* WATCH ROOM t{} unid {} {} -> {} foot ({:.0f}, {:.0f}) falling {} floor {:.0f}\n", world.tickcount, unid,
				lr->second, nowroom, footx, footy, (bool)falling, downfootroom ? downfootroom->floorYatX(footx) : -1.0f);
		lastroom[unid] = nowroom;
		static std::map<int, float> lastfooty;
		auto it = lastfooty.find(unid);
		if (it != lastfooty.end() && !falling && it->second - footy > 20)
			fmt::print("* WATCH JUMPUP t{} unid {} foot ({:.0f}, {:.0f}) up {:.0f} room {}\n", world.tickcount, unid, footx, footy,
				it->second - footy, downfootroom ? (int)downfootroom->id : -1);
		lastfooty[unid] = footy;
		if (world.tickcount % 200 == 0)
			fmt::print("* WATCH POS t{} unid {} genus {} foot ({:.0f}, {:.0f}) room {} falling {}\n",
				world.tickcount, unid, (int)genus, footx, footy, downfootroom ? (int)downfootroom->id : -1, (bool)falling);
	}
}

void SkeletalCreature::gaitTick() {
	if (!gaitgene)
		return;
	uint8_t pose = gaitgene->pose[gaiti];
	if (pose == 0) {
		if (gaiti == 0)
			return; // non-worky gait

		gaiti = 0;
		gaitTick();
		return;
	}

	std::map<unsigned int, creaturePoseGene*>::iterator i = posegenes.find(pose);
	if (i != posegenes.end()) {
		creaturePoseGene* poseg = i->second;
		assert(poseg->poseno == pose);
		setPose(poseg->getPoseString());
	}

	gaiti++;
	if (gaiti > 7)
		gaiti = 0;
}

CompoundPart* SkeletalCreature::part(unsigned int part_id) {
	// A C1/C2 creature is a single part; scripts that set PART for their own
	// object and then target a creature (C2's boats fling passengers ashore
	// with POSE 128 after PART 3) mean the creature itself. Throwing here
	// stopped the boat's script mid-trip with the boat still moving, so it
	// sailed round the world for ever.
	if (engine.version < 3)
		return skeleton;
	THROW_IFNOT(part_id == 0);
	return skeleton;
}

void SkeletalCreature::setZOrder(unsigned int plane) {
	Agent::setZOrder(plane);
	skeleton->zapZOrder();
	skeleton->addZOrder();
}

SkeletonPart::SkeletonPart(SkeletalCreature* p)
	: AnimatablePart(p, 0, 0, 0, 0) {
}

void SkeletonPart::tick() {
	updateAnimation();
}

void SkeletonPart::setPose(unsigned int p) {
	((SkeletalCreature*)parent)->setPoseGene(p);
}

void SkeletonPart::setFrameNo(unsigned int p) {
	assert(p < animation.size());
	frameno = p;
}

void SkeletonPart::partRender(class RenderTarget* renderer, int xoffset, int yoffset) {
	SkeletalCreature* c = dynamic_cast<SkeletalCreature*>(parent);
	c->render(renderer, xoffset, yoffset);
}

void SkeletalCreature::finishInit() {
	Agent::finishInit();

	processGenes();
	skeletonInit();
	setPose(0);
}

void SkeletalCreature::creatureAged() {
	// TODO: adjust position to account for any changes..

	processGenes();
	skeletonInit();
	recalculateSkeleton();
}

std::string SkeletalCreature::getFaceSpriteName() {
	// TODO: we should store the face sprite when we first search for sprites (since it
	// has to be the baby sprite), rather than this horrible hackery
	for (auto& gene : creature->getGenome()->genes) {
		//if ((*i)->header.switchontime != creature->getStage()) continue;

		if (creatureAppearanceGene* x = dynamic_cast<creatureAppearanceGene*>(gene.get())) {
			if (x->part == 0) {
				return std::string("a") + dataString(0, creature->isFemale(), x->species, x->variant);
			}
		}
	}

	THROW_IFNOT(false); // TODO: mmh
}

unsigned int SkeletalCreature::getFaceSpriteFrame() {
	return 9 + (eyesclosed ? 16 : 0) + (facialexpression * 32);
}

int SkeletalCreature::handleClick(float clickx, float clicky) {
	// TODO: muh, horror
	(void)clickx;

	clicky -= y;
	if (clicky >= getSkelHeight() / 2.0) {
		return 0; // slap
	} else {
		return 1; // tickle
	}
}

// C2: a creature carries in the hand on the viewer's side, drawn in front
// of its body: facing right its right forearm (part 10) is drawn over the
// body and its left (9) behind, facing left the other way round; facing
// toward or away from the viewer both are behind it. Before, everything was
// held in the left hand and drawn behind the body, so a toy gun jumped from
// the creature's hands to behind it as it turned (owner, 2026-11-10).
bool SkeletalCreature::carriesInFront() const {
	return engine.version == 2 && (posedirection == 0 || posedirection == 1);
}

std::pair<int, int> SkeletalCreature::getCarryPoint() {
	std::pair<int, int> carrypoint;

	int hand = (engine.version == 2 && posedirection == 0) ? 10 : 9;
	carrypoint.first = attachmentX(hand, 1);
	carrypoint.second = attachmentY(hand, 1);

	return carrypoint;
}

/* vim: set noet: */
