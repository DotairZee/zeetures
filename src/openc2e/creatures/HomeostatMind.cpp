/*
 *  HomeostatMind.cpp
 *  openc2e
 *
 *  Mind model v0 ("homeostat"): an original creature mind built from
 *  published research on motivation and learning. See mind-model-v0.md in
 *  the project notes for the design and references.
 *
 *  1. Drive and reward: overall drive D combines the body's 17 drives
 *     nonlinearly (severe needs dominate); an action's reward is the drop
 *     in D it produced (homeostatic reinforcement learning).
 *  2. Expectations: per (object category, action), the expected change in
 *     each drive, learned with the delta rule after every action.
 *  3. Wanting and choice: predicted drop in D given current needs, plus a
 *     curiosity bonus, minus a distance cost; softmax choice whose
 *     temperature falls as arousal rises; a commitment bonus for the
 *     current choice.
 *  4. Habituation: categories whose interactions change nothing lose
 *     attention, recovering over time.
 *  5. Affect: valence (running average of reward) and arousal (D) shape
 *     exploration.
 *  6. Instincts: weak innate expectations, overwritten by experience.
 *  7. Facial expression: chosen from affect (Anger, distress, recent mood,
 *     longer-term valence, overall drive).
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

#include "CreatureMind.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fmt/core.h>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr int NUM_DRIVES = 17;

// Labels for log lines only (the order of drives in the owner's
// allchemicals.str, chemicals 1-17).
const char* const DRIVE_NAMES[NUM_DRIVES] = {
	"Pain", "Pleasure", "Hunger", "Cold", "Hot", "Tired", "Sleepy", "Lonely",
	"Crowded", "Fear", "Bored", "Anger", "Sex", "Injury", "Suffocate", "Thirst", "Stress"};
enum Drive { PAIN = 0,
	PLEASURE,
	HUNGER,
	COLD,
	HOT,
	TIRED,
	SLEEPY,
	LONELY,
	CROWDED,
	FEAR,
	BORED,
	ANGER,
	SEX,
	INJURY,
	SUFFOCATE,
	THIRST,
	STRESS };

// Action labels for log lines only, as read from the owner's world scripts.
std::string actionName(int decision) {
	switch (decision) {
		case 0: return "idle";
		case 1: return "activate1";
		case 2: return "activate2";
		case 3: return "deactivate";
		case 4: return "approach";
		case 6: return "pickup";
		case 7: return "drop";
		case 8: return "express";
		case 10: return "travel-a";
		case 11: return "travel-b";
		case 12: return "eat";
		case 13: return "hit";
		case 25: return "sleep";
		case 26: return "walk-a";
		case 27: return "walk-b";
		default: return fmt::format("action{}", decision);
	}
}

// Every tunable number in one place, so they can become genes later.
struct Params {
	float driveExpN = 3.0f; // D = (sum w_i d_i^n)^(1/m), n > m
	float driveExpM = 2.0f;
	std::array<float, NUM_DRIVES> weights;
	float learningRate = 0.3f; // delta rule
	// Felt effects: stimuli tell the body directly what an interaction did
	// (C2 chemicals 18-34 raise drive 1-17, 35-51 lower it), like a taste.
	// A drive an action's stimuli touched is learned from what was felt; the
	// others from the change observed beyond drift, more slowly, since that
	// mixes in whatever else was going on.
	int driveIncreaseChem = 18, driveDecreaseChem = 35;
	float unfeltLearningShare = 0.25f;
	float curiosity = 0.02f; // novelty bonus scale
	float boredomCuriosityGain = 2.0f; // Boredom raises curiosity
	float temperature = 0.015f; // softmax base temperature
	float arousalFocus = 2.0f; // high D lowers temperature
	float commitment = 0.01f; // bonus for repeating the current choice
	float distanceCost = 0.00002f; // per pixel to the object
	int maxEpisodeTicks = 12; // looping actions are evaluated this often
	// Sleep: the world's sleep script keeps the creature asleep and resting
	// until it is rested, then wakes it. The mind lets it run, and wakes the
	// creature early only when another need rises sharply while it sleeps.
	int sleepDecision = 25;
	int maxSleepTicks = 3000;
	float wakeRise = 0.25f; // another drive has risen this much since falling asleep...
	float wakeLevel = 0.6f; // ...and is at least this high
	int noSleepAfterWaking = 100; // ticks before sleeping again after an urgent waking
	// Woken by cold or heat: the place is the trouble, not sleeping. Do not
	// sleep again near there for a while (so the creature sleeps somewhere
	// else), and do not learn that sleeping itself chills or overheats (a
	// creature woken by the cold at the west edge came to rank sleep so low
	// that it never slept again and ran out of energy)
	float badSleepNear = 300.0f;
	float poisonPain = 1.0f; // felt Pain per unit of poison taken in (see the stimulus loop)
	unsigned int badSleepTicks = 1200;
	int maxObjectEpisodeTicks = 150; // actions on an object (walk there, then act): at most this long
	int stallTicks = 25; // ...and given up if not getting closer for this long
	float habituationRate = 0.7f; // multiplier after an uneventful interaction
	float habituationRecovery = 0.01f; // per tick, back toward 1
	float eventfulThreshold = 0.02f; // total |drive change| that counts as eventful
	float valenceRate = 0.1f; // running average of reward
	float driftRate = 0.1f; // running average of passive drive change per tick
	size_t choiceSetSize = 5; // softmax over the top few candidates only

	// Genome instincts (gene-translation.md): an instinct is an innate
	// expectation. At full strength it predicts this much change in its drive,
	// and counts as this many remembered experiences (resilience): learning
	// starts slow and speeds up as real experience outweighs it.
	float instinctDriveChange = 0.25f;
	float legacyInstinctResilience = 12.0f; // legacy genes have one number; scaled by strength squared
	float anyObjectInstinctWeight = 1.5f; // an instinct naming no object is a weak hunch about objects in general

	// Seeking: when something wanted is out of sight, head for where it was
	// last seen (a first sense of place).
	float seekDiscount = 0.6f; // remembered relief counts for this share of seen relief
	float seekDistanceCost = 0.00015f; // per pixel to walk: something wanted 1000 px away loses 0.15
	float seekMinWant = 0.01f;
	float seekMaxHeight = 200.0f; // walking west/east cannot change floors
	float seekArrived = 40.0f; // closer than this and still not seen: forget the place

	// Changing floors (a first cognitive map). Innate: lifts (C2 category 27,
	// "lifts and up/down movers") and call buttons (2) are for changing floors;
	// which action goes which way is learned from rides, as links.
	int liftCategory = 27, buttonCategory = 2;

	// Edible scent (perceived): smelling nourishing makes eating expected to
	// relieve hunger, up to this much at full scent, weighted as this many
	// remembered experiences; no food smell removes the vague hope of a hunch.
	int eatDecision = 12;
	float scentHungerRelief = 0.4f;
	float scentWeight = 6.0f;

	// Following food smell through the rooms (World::tickFoodScent): when no
	// reachable food is in view, walk or ride toward stronger smell. Valued
	// as this share of the relief eating smelled food is expected to bring.
	float scentFollowShare = 0.5f;
	float scentStronger = 1.1f; // a side must smell this much stronger than here
	float scentFloorStep = 300.0f; // how far up or down a floor change aims
	float floorChangeDiscount = 0.5f; // share of the target's value for each step toward it
	float atPlace = 50.0f; // close enough to a lift's position to use it
	float sameLevel = 120.0f; // heights this close count as the same floor
	float minRide = 60.0f; // a ride shorter than this did not change floors

	// Blocked ways: walking west or east toward something that got the
	// creature nowhere means that way is blocked from here, for a while.
	// Food finding (2026-11-08). A food map: places where food was seen or a
	// meal eased hunger, kept with a confidence that falls when the creature
	// arrives and finds nothing (food regrows, so places are not forgotten
	// at once), and saved with the creature.
	float foodSeeEdible = 0.3f; // a visible object smelling this nourishing marks a food place
	float foodMerge = 200.0f; // places this close (same floor) are one place
	size_t foodPlaceMax = 8;
	float foodArrived = 60.0f; // this close to a place: look whether food is there
	unsigned int foodRecheckTicks = 200; // a place is judged empty at most this often
	// Travel is valued by time (owner's idea), with walking speed taken as
	// constant: a remembered meal keeps half its value per travelHalf pixels
	// of walking (a floor change counts as floorChangeDistance pixels), and
	// hunger makes the creature more patient: travelHalf * (1 + hungerPatience
	// * hunger^2), so a starving creature will cross the world for food.
	float travelHalf = 700.0f;
	float hungerPatience = 6.0f;
	float floorChangeDistance = 400.0f;
	// Foraging: hungry, nothing in sight, nothing remembered worth the trip,
	// no smell to follow: search, heading for the part of this floor visited
	// least recently, keeping the heading for a while.
	float forageHunger = 0.45f;
	float forageShare = 0.35f; // worth this share of an expected meal
	unsigned int forageCommit = 150; // mind ticks to keep a heading
	float visitBinWidth = 400.0f, visitBinHeight = 300.0f;
	int forageLookBins = 4; // how far ahead (in bins) staleness is judged
	// Noticing lack of progress: a search heading that has not carried the
	// creature foragePass pixels after forageStuckTicks is turned around (both
	// ways stuck: another floor); a food trip that has not come foodTripGain
	// pixels closer in foodTripTicks makes that place "out of reach from
	// here": its confidence halves and it is not tried for foodCooldown ticks.
	// Restlessness (owner's idea): the longer a creature stays hungry without
	// a meal that eases it, the sooner and harder it searches, and the more
	// readily it leaves its own floor. It builds over restlessRamp mind ticks
	// spent at hunger restlessHunger or more, and resets with a good meal.
	float restlessHunger = 0.3f;
	float restlessRamp = 1500.0f; // about 10 game minutes
	float forageHungerRestless = 0.25f; // forage threshold at full restlessness
	float restlessBoost = 2.0f; // forage worth up to (1 + boost) times as much
	float restlessLeaveFloor = 0.6f; // chance per new heading of trying another floor, at full restlessness
	unsigned int floorSearched = 1500; // both ways visited this recently: the floor counts as searched

	float foragePass = 80.0f;
	unsigned int forageStuckTicks = 60;
	float foodTripGain = 100.0f;
	unsigned int foodTripTicks = 300, foodCooldown = 600;

	// Moving attention on: an object used this many times in a row with no
	// benefit (no drop in overall drive, no ride to another floor) is passed
	// over for a while, so the next object of its kind gets noticed.
	int passOverAfter = 2;
	float passOverReward = 0.005f;

	// Escaping water (owner request): in water, getting out is worth the
	// relief of clearing Suffocate plus escapeUrgency (so it leaves before
	// suffocating). With a dry room reachable through open doors it heads
	// that way; without one it tries the devices in view (exits that worked
	// before first). Reaching dry land within exitWindow ticks of using an
	// object makes that object, there, a remembered exit (saved).
	float escapeUrgency = 0.06f;
	float escapeBreathUrgency = 0.3f; // plus this times (1 - breath left)^2
	float escapeDeviceShare = 0.7f; // trying a device: this share of the escape's worth
	float exitBonus = 1.5f; // a remembered exit nearby: this many times the escape's worth
	float exitNear = 400.0f;
	unsigned int exitWindow = 400;
	size_t exitMax = 8;
	// Devices that did nothing for an escape (passed over) are not tried again
	// for this long (mind ticks); with none left untried in view, the creature
	// searches the room, walking away from the ones it has tried (in the
	// airlock's west end the land door is out of sight)
	unsigned int triedFor = 400;
	float searchShare = 0.6f; // searching: this share of the escape's worth
	// Devices that flooded the creature (it was dry, used one, and was in water
	// soon after): not used again while dry near where that happened (the
	// airlock's sea door, or its pump once drained, which floods it again)
	float flooderNear = 200.0f;
	// devices worth trying to get out (C2 categories are genus numbers):
	// call buttons (2), machines such as the airlock pump (3), doors such as
	// the airlock's (9), lifts (27)
	std::vector<int> escapeDevices{2, 3, 9, 27};

	// Boats (owner request): walking a trip into water is avoided; with a boat
	// in view, rowing that way is worth the trip. C2 boats (category 31): the
	// first activate rows east (back west on a second press), the second rows
	// west, and the boat puts its passengers out on the far shore. Rides are
	// learned (decision and how far east or west it carried the creature), so
	// the innate guess gives way to experience.
	int boatCategory = 31;
	// Travel links (owner's idea: think of boats as the airlock exit is
	// thought of): using a device, then soon being somewhere quite different
	// (a ride ending linkMove px or more away, or on another floor), is
	// remembered as "this object, used here, takes me there" (saved). A
	// creature heading somewhere uses a link that starts within linkNear of
	// it and ends at least linkGain px closer to where it wants to be.
	float linkMove = 300.0f;
	float linkNear = 350.0f;
	float linkGain = 400.0f;
	unsigned int linkWindow = 600;
	size_t linkMax = 16;
	float linkShare = 0.95f; // a known link: this share of the trip's worth
	float boatShare = 0.9f; // a boat crossing: this share of the trip's worth
	// Drops: walking off a ledge or dock more than cliffDrop px high is
	// avoided when searching or following smell, unless the trip is headed
	// that much lower anyway.
	float cliffDrop = 150.0f;
	float boatCallShare = 0.8f; // calling a boat over with a switch: this share of the crossing's
	// Learning what a switch does: a button pressed to call a boat, then a
	// boat within reach within boatCallWait ticks, is a known boat switch
	// (saved); a button that brought no boat counts against itself, and one
	// with more failures than successes is not pressed for boats again.
	unsigned int boatCallWait = 200;
	float boatCallKnownBonus = 1.2f; // a known boat switch: worth this much more
	float boatRide = 200.0f; // a ride carrying the creature this far counts
	unsigned int boatNoRide = 60; // pressed a boat, no ride this many ticks later: try the other action
	unsigned int boatFlip = 300; // ...for this long

	float blockedMove = 3.0f; // moved less than this in a walking episode (walking covers about 10)
	float blockedNear = 40.0f; // still this close to where it got stuck
	unsigned int blockedTicks = 600;

	// 7. Facial expression from affect. Slots refer to the head sprites'
	// expression rows (in the owner's norn heads: 0 neutral, 1 happy,
	// 2 sad, 3 angry); other breeds may differ, so these belong in a breed file.
	int faceNeutral = 0, faceHappy = 1, faceSad = 2, faceAngry = 3;
	float angryAt = 0.5f; // Anger drive level (0-1)
	float distressAt = 0.6f; // max of Pain, Fear, Lonely (0-1)
	float moodRate = 0.5f; // how fast the recent mood follows each reward
	float moodDecay = 0.95f; // per tick, recent mood fades toward neutral
	float happyMood = 0.04f, sadMood = -0.05f; // recent-mood thresholds
	float sadValence = -0.03f; // longer-term valence threshold
	float contentValence = 0.01f, contentDrive = 0.3f; // calm and content

	// Needs are not equal: what keeps the body alive outweighs comfort and
	// company. With D's cube, weight 1.5 against 0.5 makes Hunger at 0.5
	// count as much as Boredom at about 0.72.
	float survivalWeight = 1.5f; // Pain, Hunger, Cold, Hot, Injury, Suffocate, Thirst
	float comfortWeight = 0.5f; // Lonely, Crowded, Bored
	// Pressing survival needs quiet curiosity: at the strongest survival need
	// s, novelty is scaled by 1 - survivalCuriosityDamp * s.
	float survivalCuriosityDamp = 0.8f;

	Params() {
		weights.fill(1.0f);
		weights[SEX] = 0.3f;
		for (int i : {PAIN, HUNGER, COLD, HOT, INJURY, SUFFOCATE, THIRST})
			weights[i] = survivalWeight;
		for (int i : {LONELY, CROWDED, BORED})
			weights[i] = comfortWeight;
	}
};

// Weak innate expectations, keyed by decision number (instincts).
// Values are expected drive changes on the 0-1 scale.
std::array<float, NUM_DRIVES> instinctFor(int decision) {
	std::array<float, NUM_DRIVES> e{};
	switch (decision) {
		case 12: // eat
			e[HUNGER] = -0.05f;
			break;
		case 25: // sleep
			e[TIRED] = -0.05f;
			e[SLEEPY] = -0.05f;
			break;
		default:
			break;
	}
	return e;
}

struct Expectation {
	std::array<float, NUM_DRIVES> delta{}; // expected change in each drive
	int count = 0; // times experienced
	float prior = 0; // innate weight, in remembered experiences (from instincts)
};

struct Place {
	float x = 0, y = 0;
	unsigned int tick = 0;
};

// A ride that changed floors: at this lift position, this action took the
// creature from one height to another.
struct Link {
	float x = 0, fromY = 0, toY = 0;
	int decision = 0;
	int uses = 0;
};

struct FoodPlace {
	float x = 0, y = 0;
	unsigned int tick = 0; // last seen or eaten there
	float confidence = 0.5f;
	int meals = 0;
	unsigned int checked = 0; // last time arriving there was judged
	unsigned int cooldownUntil = 0; // out of reach from here: not tried until then
};

struct Candidate {
	int category; // -1 for actions that need no object
	int decision;
	float distance;
	float want; // predicted drop in D
	float novelty;
	float score;
	int seekCategory = -1; // >= 0: travelling toward where this category was last seen
	bool chain = false; // a step toward something else (changing floors): no drive learning
	bool scent = false; // following food smell
	bool food = false; // heading for a remembered food place
	bool forage = false; // searching for food
	bool escape = false; // getting out of water
	bool boatCall = false; // pressing a button to call a boat
};

struct CallButton {
	float x = 0, y = 0; // where it was pressed from
	int worked = 0, failed = 0;
};

struct BoatLink {
	int decision = 0;
	float dx = 0; // how far east (positive) or west the ride carried the creature
	int rides = 0;
};

struct TravelLink {
	float fromX = 0, fromY = 0, toX = 0, toY = 0;
	int category = -1, decision = -1;
	int uses = 0;
};

struct ExitLink {
	float x = 0, y = 0; // where the object was used
	int category = -1, decision = -1;
	int uses = 0;
};

class HomeostatMind : public CreatureMind {
  public:
	HomeostatMind() {
		habituation.fill(1.0f);
	}

	std::string name() const override { return "homeostat"; }

	void born(const MindPerception& p) override {
		rng.seed((unsigned int)p.creatureId);
		std::string acts;
		for (auto& a : *p.actions)
			acts += fmt::format(" {}{}", actionName(a.decision), a.needsObject ? "*" : "");
		fmt::print("[homeostat] creature {} born ({} {}); actions (* = on an object):{}\n",
			p.creatureId, p.stage, p.female ? "female" : "male", acts);
	}

	void died(const MindPerception& p) override {
		fmt::print("[homeostat] creature {} died at brain tick {}; {}\n", p.creatureId, p.tick, debugState());
	}

	MindOutput tick(const MindPerception& p) override {
		MindOutput out;
		if (pendingPassOver) {
			out.passOver = pendingPassOver;
			pendingPassOver = AgentRef();
		}
		std::array<float, NUM_DRIVES> d = normalize(p.drives);
		float D = overallDrive(d);

		for (auto& h : habituation)
			h += prm.habituationRecovery * (1.0f - h);

		for (auto& inst : p.newInstincts)
			applyInstinct(p, inst);
		visited[{(int)std::floor(p.x / prm.visitBinWidth), (int)std::floor(p.y / prm.visitBinHeight)}] = p.tick;
		if (lastTick != 0 && p.tick > lastTick) {
			if (d[HUNGER] >= prm.restlessHunger)
				hungryTicks += (float)(p.tick - lastTick);
			else if (d[HUNGER] < prm.restlessHunger * 0.5f)
				hungryTicks = 0; // fed
		}
		lastTick = p.tick;
		if (wasInWater && !p.inWater && lastObjectUse.category >= 0 && p.tick < lastObjectUseTick + prm.exitWindow) {
			bool known = false;
			for (auto& e : exits)
				if (e.category == lastObjectUse.category && e.decision == lastObjectUse.decision && std::fabs(e.x - lastObjectUse.x) < 200.0f && std::fabs(e.y - lastObjectUse.y) < prm.sameLevel) {
					e.uses++;
					known = true;
				}
			if (!known) {
				if (exits.size() >= prm.exitMax)
					exits.erase(exits.begin());
				ExitLink e = lastObjectUse;
				e.uses = 1;
				exits.push_back(e);
			}
			fmt::print("[homeostat] creature {} t{}: out of the water; remembers {}@{} at {:.0f},{:.0f} as an exit\n", p.creatureId, p.tick,
				actionName(lastObjectUse.decision), lastObjectUse.category, lastObjectUse.x, lastObjectUse.y);
		}
		// (only working a device counts: activate 1 or 2, deactivate; eating
		// or approaching near one taught creatures nonsense flooders)
		if (!wasInWater && p.inWater && lastObjectUse.category >= 0 && lastObjectUse.decision >= 1 && lastObjectUse.decision <= 3 && p.tick < lastObjectUseTick + prm.exitWindow) {
			bool known = false;
			for (auto& f : flooders)
				if (f.category == lastObjectUse.category && f.decision == lastObjectUse.decision && std::fabs(f.x - lastObjectUse.x) < prm.flooderNear && std::fabs(f.y - lastObjectUse.y) < prm.sameLevel) {
					f.uses++;
					known = true;
				}
			if (!known) {
				if (flooders.size() >= prm.exitMax)
					flooders.erase(flooders.begin());
				ExitLink f = lastObjectUse;
				f.uses = 1;
				flooders.push_back(f);
			}
			fmt::print("[homeostat] creature {} t{}: in the water; remembers {}@{} at {:.0f},{:.0f} as a flooder\n", p.creatureId, p.tick,
				actionName(lastObjectUse.decision), lastObjectUse.category, lastObjectUse.x, lastObjectUse.y);
		}
		wasInWater = p.inWater;
		// boat rides
		if (p.riding && !wasRiding) {
			rideStartX = p.x;
			rideDecision = (current.category == prm.boatCategory) ? current.decision : -1;
		} else if (!p.riding && wasRiding && rideDecision >= 0 && std::fabs(p.x - rideStartX) > prm.boatRide) {
			BoatLink& bl = boatLinks[rideDecision];
			bl.decision = rideDecision;
			bl.dx = (bl.dx * bl.rides + (p.x - rideStartX)) / (bl.rides + 1);
			bl.rides++;
			fmt::print("[homeostat] creature {} t{}: boat ride with {} took it {:.0f} px\n", p.creatureId, p.tick, actionName(rideDecision), p.x - rideStartX);
		}
		// travel links: a device use followed by a ride (or a floor change)
		// that ends somewhere quite different
		if (pendingLink.category >= 0) {
			if (p.riding)
				pendingRode = true;
			float moved = std::fabs(p.x - pendingLink.fromX) + std::fabs(p.y - pendingLink.fromY);
			bool landed = pendingRode ? !p.riding : std::fabs(p.y - pendingLink.fromY) > prm.minRide;
			if (landed && moved >= prm.linkMove && p.tick < pendingLinkTick + prm.linkWindow) {
				bool known = false;
				for (auto& tl : travelLinks)
					if (tl.category == pendingLink.category && tl.decision == pendingLink.decision &&
						std::fabs(tl.fromX - pendingLink.fromX) < 200.0f && std::fabs(tl.fromY - pendingLink.fromY) < prm.sameLevel) {
						tl.toX = (tl.toX * tl.uses + p.x) / (tl.uses + 1);
						tl.toY = (tl.toY * tl.uses + p.y) / (tl.uses + 1);
						tl.uses++;
						known = true;
					}
				if (!known) {
					if (travelLinks.size() >= prm.linkMax)
						travelLinks.erase(travelLinks.begin());
					TravelLink tl = pendingLink;
					tl.toX = p.x;
					tl.toY = p.y;
					tl.uses = 1;
					travelLinks.push_back(tl);
				}
				fmt::print("[homeostat] creature {} t{}: learned travel link {}@{} from {:.0f},{:.0f} to {:.0f},{:.0f}\n", p.creatureId, p.tick,
					actionName(pendingLink.decision), pendingLink.category, pendingLink.fromX, pendingLink.fromY, p.x, p.y);
				pendingLink.category = -1;
			} else if (p.tick >= pendingLinkTick + prm.linkWindow)
				pendingLink.category = -1;
		}
		if (callPending) {
			bool boatHere = false;
			for (auto& sa : p.visible)
				if (sa.category == prm.boatCategory && sa.reachable)
					boatHere = true;
			bool done = boatHere || p.tick > callPressTick + prm.boatCallWait;
			if (done) {
				CallButton* cb = nullptr;
				for (auto& c : callButtons)
					if (std::fabs(c.x - callPressX) < 200.0f && std::fabs(c.y - callPressY) < prm.sameLevel)
						cb = &c;
				if (!cb) {
					if (callButtons.size() >= 16)
						callButtons.erase(callButtons.begin());
					callButtons.push_back({callPressX, callPressY, 0, 0});
					cb = &callButtons.back();
				}
				(boatHere ? cb->worked : cb->failed)++;
				fmt::print("[homeostat] creature {} t{}: pressed a button at {:.0f},{:.0f} to call a boat: {}\n", p.creatureId, p.tick, callPressX, callPressY,
					boatHere ? "the boat came (a boat switch)" : "no boat came");
				callPending = false;
			}
		}
		if (p.riding)
			boatTryDecision = -1; // the press worked
		else if (boatTryDecision >= 0 && p.tick > boatTryTick + prm.boatNoRide) {
			boatFlipUntil = p.tick + prm.boatFlip; // nothing happened: try the other one
			boatTryDecision = -1;
		}
		wasRiding = p.riding;
		bool foodInSight = false;
		for (auto& s : p.visible)
			if (s.edible >= prm.foodSeeEdible) {
				rememberFood(s.x, s.y, p.tick, 0.6f, false);
				foodInSight = true;
			}
		for (auto& fp : foodPlaces) {
			if (std::fabs(fp.x - p.x) < prm.foodArrived && std::fabs(fp.y - p.y) < prm.sameLevel && !foodInSight &&
				p.tick >= fp.checked + prm.foodRecheckTicks) {
				fp.confidence *= 0.5f; // came here and found nothing (for now)
				fp.checked = p.tick;
			}
		}
		size_t foodBefore = foodPlaces.size();
		foodPlaces.erase(std::remove_if(foodPlaces.begin(), foodPlaces.end(), [](const FoodPlace& f) { return f.confidence < 0.05f; }), foodPlaces.end());
		if (foodPlaces.size() != foodBefore)
			foodTrip = -1; // indices moved
		for (auto& s : p.visible) {
			places[s.category] = {s.x, s.y, p.tick};
			auto sc = scents.find(s.category);
			if (sc == scents.end() || s.edible > sc->second + 0.05f) {
				scents[s.category] = s.edible;
				applyScent(s.category, s.edible);
			}
		}

		// 7. facial expression from current affect; only sent when it changes
		mood *= prm.moodDecay;
		int face = chooseFace(d, D);
		if (face != lastFace) {
			out.expression = face;
			fmt::print("[homeostat] creature {} t{}: face {} -> {} (mood {:+.3f}, valence {:+.3f}, Anger {:.2f})\n",
				p.creatureId, p.tick, lastFace, face, mood, valence, d[ANGER]);
			lastFace = face;
		}

		for (auto& s : p.stimuli) {
			stimuliSeen++;
			for (int k = 0; k < 4; k++) {
				int c = s.feltChem[k];
				float amt = s.feltAmount[k] / 255.0f;
				if (c >= prm.driveIncreaseChem && c < prm.driveIncreaseChem + NUM_DRIVES) {
					felt[c - prm.driveIncreaseChem] += amt;
					feltAny[c - prm.driveIncreaseChem] = true;
				} else if (c >= prm.driveDecreaseChem && c < prm.driveDecreaseChem + NUM_DRIVES) {
					felt[c - prm.driveDecreaseChem] -= amt;
					feltAny[c - prm.driveDecreaseChem] = true;
				} else if (amt > 0 && (c == 140 || c == 141 || c == 143 || c == 144 || c == 145 || (c >= 234 && c <= 239) || c == 170)) {
					// a poison taken in is felt as harm, like a bitter, burning
					// taste (alcohol at half): one bite of the cyanide berries
					// teaches the creature to leave them alone
					felt[PAIN] += amt * (c == 170 ? 0.5f : 1.0f) * prm.poisonPain;
					feltAny[PAIN] = true;
				}
			}
			if (s.numbered)
				fmt::print("[homeostat] creature {} stimulus {} #{}\n", p.creatureId, s.command, s.id);
			else
				fmt::print("[homeostat] creature {} stimulus {} chems {}:{} {}:{} {}:{} {}:{}\n", p.creatureId, s.command,
					s.chem[0], s.amount[0], s.chem[1], s.amount[1], s.chem[2], s.amount[2], s.chem[3], s.amount[3]);
		}

		if (!p.actions || p.actions->empty())
			return out;

		// The body's drives read all zero until its chemistry has run once
		// (just born or just loaded): nothing to decide or learn from yet.
		if (!drivesSeen) {
			bool any = false;
			for (auto v : p.drives)
				any = any || v != 0;
			if (!any)
				return out;
			drivesSeen = true;
		}

		// Asleep: let the sleep script run unless something urgent wakes us.
		if (inEpisode && current.category < 0 && current.decision == prm.sleepDecision) {
			episodeTicks++;
			int urgent = wakeReason(d);
			// asleep in water (it rose round the creature, or it dozed off in
			// the shallows): wake at once, as for suffocation; two batch norns
			// drowned asleep in the seas
			if (p.inWater && urgent < 0)
				urgent = SUFFOCATE;
			bool done = p.actionFinished && episodeTicks > 1;
			if (!done && urgent < 0 && episodeTicks < prm.maxSleepTicks)
				return out;
			endEpisode(p, d, D, done ? "finished" : urgent >= 0 ? DRIVE_NAMES[urgent] : "time");
			if (urgent >= 0) {
				fmt::print("[homeostat] creature {} t{}: woken by {}\n", p.creatureId, p.tick, DRIVE_NAMES[urgent]);
				noSleepUntil = p.tick + prm.noSleepAfterWaking;
				if (urgent == COLD || urgent == HOT) {
					badSleepX = p.x;
					badSleepY = p.y;
					badSleepUntil = p.tick + prm.badSleepTicks;
				}
			}
		}

		// Is the current episode over?
		if (inEpisode) {
			episodeTicks++;
			bool objectGone = current.category >= 0 && !isVisible(p, current.category);
			bool finished = p.actionFinished && episodeTicks > 1;
			int limit = current.category >= 0 ? prm.maxObjectEpisodeTicks : prm.maxEpisodeTicks;
			bool stalled = false;
			if (current.category >= 0 && !objectGone) {
				float dist = 0;
				for (auto& s : p.visible)
					if (s.category == current.category)
						dist = s.distance;
				if (dist < closest - 5.0f) {
					closest = dist;
					lastProgress = episodeTicks;
				} else if (episodeTicks - lastProgress >= prm.stallTicks) {
					stalled = true;
				}
			}
			if (finished || objectGone || stalled || episodeTicks >= limit) {
				endEpisode(p, d, D, finished ? "finished" : objectGone ? "object gone" : stalled ? "stalled" : "time");
			} else {
				return out; // let the current action run
			}
		}

		// Riding a lift (or another vehicle): stay put until it lets us off,
		// then remember where it took us.
		if (p.riding) {
			if (!riding) {
				riding = true;
				rideFromX = p.x;
				rideFromY = p.y;
			}
			return out;
		}
		if (riding) {
			riding = false;
			learnRide(p);
		}

		// Choose the next action.
		std::vector<Candidate> candidates = evaluate(p, d, D);
		bool badSleepHere = p.tick < badSleepUntil && std::fabs(p.x - badSleepX) < prm.badSleepNear && std::fabs(p.y - badSleepY) < prm.sameLevel;
		if (p.tick < noSleepUntil || badSleepHere || p.inWater) // (never sleep in water)
			candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
								 [&](const Candidate& c) { return c.category < 0 && c.decision == prm.sleepDecision; }),
				candidates.end());
		if (!p.inWater && !flooders.empty())
			candidates.erase(std::remove_if(candidates.begin(), candidates.end(), [&](const Candidate& c) { return isFlooder(p, c.category, c.decision); }),
				candidates.end());
		if (candidates.empty())
			return out;
		Candidate choice = pick(candidates, D);
		// Asleep but not choosing to sleep (an interrupted sleep script, or
		// a world saved mid-sleep): wake up first, or no other action can run.
		bool chooseSleep = choice.category < 0 && choice.decision == prm.sleepDecision;
		if (p.asleep && !chooseSleep)
			out.sleep = 0;
		logChoice(p, choice, candidates, D);

		if (choice.category == prm.liftCategory)
			lastLiftDecision = choice.decision;
		current = choice;
		inEpisode = true;
		episodeAgent = AgentRef();
		if (choice.category >= 0)
			for (auto& s : p.visible)
				if (s.category == choice.category)
					episodeAgent = s.agent;
		if (choice.category == prm.boatCategory || std::find(prm.escapeDevices.begin(), prm.escapeDevices.end(), choice.category) != prm.escapeDevices.end()) {
			pendingLink = {p.x, p.y, 0, 0, choice.category, choice.decision, 0};
			pendingLinkTick = p.tick;
			pendingRode = false;
		}
		if (choice.boatCall) {
			callPressX = p.x;
			callPressY = p.y;
			callPressTick = p.tick;
			callPending = true;
		}
		if (choice.category == prm.boatCategory && boatTryDecision < 0) {
			boatTryDecision = choice.decision;
			boatTryTick = p.tick;
		}
		if (choice.category >= 0 && std::find(prm.escapeDevices.begin(), prm.escapeDevices.end(), choice.category) != prm.escapeDevices.end()) {
			// (only devices can be exits: leaving the water just after hitting a
			// norn taught one creature that hitting norns is a way out)
			lastObjectUse = {p.x, p.y, choice.category, choice.decision, 0};
			lastObjectUseTick = p.tick;
		}
		episodeTicks = 0;
		closest = 1e9f;
		lastProgress = 0;
		startDrives = d;
		startD = D;
		startX = p.x;
		startY = p.y;
		felt.fill(0.0f);
		feltAny.fill(false);
		if (chooseSleep)
			sleepStartDrives = d;

		if (choice.category >= 0)
			out.attentionCategory = choice.category;
		out.decision = choice.decision;
		return out;
	}

	// World saving: one fact per line, readable and hand-editable.
	std::string saveState() const override {
		std::string s = fmt::format("statever 4\nvalence {}\nmood {}\nepisodes {}\nstimuli {}\n", valence, mood, episodes, stimuliSeen);
		s += "drift";
		for (float f : drift)
			s += fmt::format(" {}", f);
		s += "\nhabituation";
		for (float f : habituation)
			s += fmt::format(" {}", f);
		s += "\n";
		for (auto& [key, e] : expectations) {
			// key = (category + 1) * 64 + decision
			s += fmt::format("expect {} {} {}", key / 64 - 1, key % 64, e.count);
			for (float f : e.delta)
				s += fmt::format(" {}", f);
			s += "\n";
			if (e.prior > 0)
				s += fmt::format("prior {} {} {}\n", key / 64 - 1, key % 64, e.prior);
		}
		for (auto& [decision, e] : innateDefaults) {
			s += fmt::format("innate {} {}", decision, e.prior);
			for (float f : e.delta)
				s += fmt::format(" {}", f);
			s += "\n";
		}
		for (auto& sig : appliedInstincts)
			s += fmt::format("instinct {}\n", sig);
		for (auto& l : links)
			s += fmt::format("link {} {} {} {} {}\n", l.x, l.fromY, l.toY, l.decision, l.uses);
		for (auto& [category, edible] : scents)
			s += fmt::format("scent {} {}\n", category, edible);
		for (auto& fp : foodPlaces)
			s += fmt::format("food {} {} {} {} {}\n", fp.x, fp.y, fp.tick, fp.confidence, fp.meals);
		s += fmt::format("restless {}\n", hungryTicks);
		for (auto& e : exits)
			s += fmt::format("exit {} {} {} {} {}\n", e.x, e.y, e.category, e.decision, e.uses);
		for (auto& f : flooders)
			s += fmt::format("flooder {} {} {} {} {}\n", f.x, f.y, f.category, f.decision, f.uses);
		for (auto& [decision, bl] : boatLinks)
			s += fmt::format("boat {} {} {}\n", decision, bl.dx, bl.rides);
		for (auto& tl : travelLinks)
			s += fmt::format("tlink {} {} {} {} {} {} {}\n", tl.fromX, tl.fromY, tl.toX, tl.toY, tl.category, tl.decision, tl.uses);
		for (auto& cb : callButtons)
			s += fmt::format("callbtn {} {} {} {}\n", cb.x, cb.y, cb.worked, cb.failed);
		return s;
	}

	void loadState(const std::string& state) override {
		std::istringstream in(state);
		std::string line;
		int version = 0;
		while (std::getline(in, line)) {
			std::istringstream l(line);
			std::string what;
			l >> what;
			if (what == "statever")
				l >> version;
			else if (what == "prior") {
				int category, decision;
				float prior;
				l >> category >> decision >> prior;
				if (l)
					expectations[(category + 1) * 64 + decision].prior = prior;
			} else if (what == "innate") {
				int decision;
				Expectation e;
				l >> decision >> e.prior;
				for (float& f : e.delta)
					l >> f;
				if (l)
					innateDefaults[decision] = e;
			} else if (what == "scent") {
				int category;
				float edible;
				l >> category >> edible;
				if (l)
					scents[category] = edible;
			} else if (what == "callbtn") {
				CallButton cb;
				l >> cb.x >> cb.y >> cb.worked >> cb.failed;
				if (l && callButtons.size() < 16)
					callButtons.push_back(cb);
			} else if (what == "tlink") {
				TravelLink tl;
				l >> tl.fromX >> tl.fromY >> tl.toX >> tl.toY >> tl.category >> tl.decision >> tl.uses;
				if (l && travelLinks.size() < prm.linkMax)
					travelLinks.push_back(tl);
			} else if (what == "boat") {
				BoatLink bl;
				l >> bl.decision >> bl.dx >> bl.rides;
				if (l)
					boatLinks[bl.decision] = bl;
			} else if (what == "exit") {
				ExitLink e;
				l >> e.x >> e.y >> e.category >> e.decision >> e.uses;
				if (l && exits.size() < prm.exitMax)
					exits.push_back(e);
			} else if (what == "flooder") {
				ExitLink f;
				l >> f.x >> f.y >> f.category >> f.decision >> f.uses;
				if (l && f.decision >= 1 && f.decision <= 3 && flooders.size() < prm.exitMax)
					flooders.push_back(f);
			} else if (what == "restless") {
				l >> hungryTicks;
			} else if (what == "food") {
				FoodPlace fp;
				l >> fp.x >> fp.y >> fp.tick >> fp.confidence >> fp.meals;
				if (l && foodPlaces.size() < prm.foodPlaceMax)
					foodPlaces.push_back(fp);
			} else if (what == "link") {
				Link lk;
				l >> lk.x >> lk.fromY >> lk.toY >> lk.decision >> lk.uses;
				if (l)
					links.push_back(lk);
			} else if (what == "instinct") {
				std::string sig;
				l >> sig;
				appliedInstincts.insert(sig);
			} else if (what == "valence")
				l >> valence;
			else if (what == "mood")
				l >> mood;
			else if (what == "episodes")
				l >> episodes;
			else if (what == "stimuli")
				l >> stimuliSeen;
			else if (what == "drift")
				for (float& f : drift)
					l >> f;
			else if (what == "habituation")
				for (float& f : habituation)
					l >> f;
			else if (what == "expect") {
				int category, decision;
				Expectation e;
				l >> category >> decision >> e.count;
				for (float& f : e.delta)
					l >> f;
				if (l) {
					float prior = expectations.count((category + 1) * 64 + decision) ? expectations[(category + 1) * 64 + decision].prior : 0.0f;
					e.prior = prior;
					expectations[(category + 1) * 64 + decision] = e;
				}
			}
		}
		if (version == 2 || version == 3) {
			// Learned before felt effects (from drive changes alone, which mix
			// in everything else going on) and, before version 3, while sleep
			// was broken. Mostly noise: start learning again from the instincts
			// (they are applied again) and smells; rides between floors are kept.
			expectations.clear();
			innateDefaults.clear();
			appliedInstincts.clear();
			scents.clear();
			fmt::print("[homeostat] relearning from scratch (expectations saved before felt effects)\n");
		}
		if (version < 2) {
			// Saved before body chemistry ran (frozen bodies: every action seemed
			// to do nothing). That experience misleads, so start learning afresh.
			expectations.clear();
			drift.fill(0.0f);
			habituation.fill(1.0f);
			fmt::print("[homeostat] discarded learning saved before body chemistry worked\n");
		}
	}

	std::string debugState() const override {
		return fmt::format("homeostat: {} episodes, valence {:+.3f}, {} stimuli, {} expectations, instincts {} active {} dormant",
			episodes, valence, stimuliSeen, expectations.size(), activeInstincts, dormantInstincts);
	}

  private:
	Params prm;
	std::map<int, Expectation> expectations; // key: (category + 1) * 64 + decision
	std::map<int, Expectation> innateDefaults; // by decision: instincts about any object
	std::set<std::string> appliedInstincts;
	int activeInstincts = 0, dormantInstincts = 0;
	std::map<int, Place> places; // where each category was last seen
	std::vector<Link> links; // learned rides between floors
	std::vector<FoodPlace> foodPlaces; // the food map
	std::map<std::pair<int, int>, unsigned int> visited; // (x bin, y bin) -> last mind tick there
	std::vector<ExitLink> exits; // objects that got the creature out of water
	std::vector<ExitLink> flooders; // objects that got the creature into water
	std::vector<std::pair<AgentRef, unsigned int>> triedDevices; // passed over while escaping, and when
	std::map<int, BoatLink> boatLinks; // decision -> learned ride
	std::vector<TravelLink> travelLinks;
	std::vector<CallButton> callButtons; // buttons tried for calling boats
	float callPressX = 0, callPressY = 0; // a boat-call press waiting for a boat
	unsigned int callPressTick = 0;
	bool callPending = false;
	TravelLink pendingLink; // the latest device use, while it may still turn into a link
	unsigned int pendingLinkTick = 0;
	bool pendingRode = false;
	bool wasRiding = false;
	float rideStartX = 0;
	int rideDecision = -1;
	int boatTryDecision = -1; // last boat action pressed, and when
	unsigned int boatTryTick = 0;
	unsigned int boatFlipUntil = 0; // until then, use the other action than learned/guessed
	bool wasInWater = false;
	ExitLink lastObjectUse; // the most recent object action, for exit learning
	unsigned int lastObjectUseTick = 0;
	AgentRef episodeAgent; // the object this episode acts on
	AgentRef lastUselessAgent;
	int uselessInARow = 0;
	AgentRef pendingPassOver;
	int forageHeading = 0; // 10 west, 11 east, 0 none
	unsigned int forageUntil = 0;
	float forageStartX = 0, forageStartY = 0;
	unsigned int forageStartTick = 0;
	int forageStuckFlips = 0; // turnarounds in a row without progress
	int forageVertical = 1; // which way to try another floor: -1 up, 1 down
	float hungryTicks = 0; // time hungry without a meal that eased it (restlessness)
	unsigned int lastTick = 0;
	int foodTrip = -1; // index in foodPlaces of the place being headed for
	float foodTripBest = 0;
	unsigned int foodTripSince = 0;
	std::map<int, float> scents; // strongest edible scent smelled, by category
	bool riding = false;
	float rideFromX = 0, rideFromY = 0;
	int lastLiftDecision = -1;
	std::array<float, 40> habituation;
	float valence = 0.0f;
	std::mt19937 rng;

	bool inEpisode = false;
	float closest = 1e9f; // nearest the creature has been to its target this episode
	int lastProgress = 0;
	Candidate current{};
	int episodeTicks = 0;
	std::array<float, NUM_DRIVES> startDrives{};
	float startD = 0.0f;
	unsigned int episodes = 0, stimuliSeen = 0;
	std::array<float, NUM_DRIVES> drift{}; // baseline drive change per tick
	std::array<float, NUM_DRIVES> sleepStartDrives{};
	unsigned int noSleepUntil = 0;
	float badSleepX = 0, badSleepY = 0; // where a sleep was cut short by cold or heat
	unsigned int badSleepUntil = 0;
	bool drivesSeen = false;
	std::array<float, NUM_DRIVES> felt{}; // drive changes this episode's stimuli delivered
	std::array<bool, NUM_DRIVES> feltAny{};
	float startX = 0;
	float startY = 0; // where the episode began (height)
	struct Blocked {
		float x = 0;
		unsigned int until = 0;
	};
	Blocked blocked[2]; // [0] west, [1] east

	bool isBlocked(const MindPerception& p, int decision) const {
		const Blocked& b = blocked[decision == 10 ? 0 : 1];
		return p.tick < b.until && std::fabs(p.x - b.x) < prm.blockedNear;
	}
	float mood = 0.0f; // short-term affect: fast-following, decaying average of reward
	int lastFace = -1;

	// A bodily need that has risen sharply since falling asleep (hunger,
	// cold, pain from a slap...), or -1. Boredom or loneliness do not wake.
	int wakeReason(const std::array<float, NUM_DRIVES>& d) const {
		int worst = -1;
		for (int i : {PAIN, HUNGER, COLD, HOT, FEAR, INJURY, SUFFOCATE, THIRST}) {
			if (d[i] >= prm.wakeLevel && d[i] - sleepStartDrives[i] >= prm.wakeRise && (worst < 0 || d[i] > d[worst]))
				worst = i;
		}
		return worst;
	}

	int chooseFace(const std::array<float, NUM_DRIVES>& d, float D) const {
		float distress = std::max({d[PAIN], d[FEAR], d[LONELY]});
		if (d[ANGER] >= prm.angryAt)
			return prm.faceAngry;
		if (distress >= prm.distressAt || mood <= prm.sadMood || valence <= prm.sadValence)
			return prm.faceSad;
		if (mood >= prm.happyMood || (valence >= prm.contentValence && D <= prm.contentDrive))
			return prm.faceHappy;
		return prm.faceNeutral;
	}

	static std::array<float, NUM_DRIVES> normalize(const std::array<unsigned char, 17>& raw) {
		std::array<float, NUM_DRIVES> d{};
		for (int i = 0; i < NUM_DRIVES; i++)
			d[i] = raw[i] / 255.0f;
		return d;
	}

	// 1. Overall drive: severe needs dominate several mild ones.
	float overallDrive(const std::array<float, NUM_DRIVES>& d) const {
		float sum = 0.0f;
		for (int i = 0; i < NUM_DRIVES; i++)
			sum += prm.weights[i] * std::pow(std::clamp(d[i], 0.0f, 1.0f), prm.driveExpN);
		return std::pow(sum, 1.0f / prm.driveExpM);
	}

	static bool isVisible(const MindPerception& p, int category) {
		for (auto& s : p.visible)
			if (s.category == category)
				return true;
		return false;
	}

	Expectation& expectation(int category, int decision) {
		int key = (category + 1) * 64 + decision;
		auto it = expectations.find(key);
		if (it == expectations.end()) {
			// A new category starts from what this action has done on other
			// categories so far; the instinct is the fallback before any
			// experience with the action (6. instincts as starting points).
			Expectation e;
			e.delta = instinctFor(decision);
			auto def = innateDefaults.find(decision);
			if (def != innateDefaults.end()) {
				e.delta = def->second.delta;
				e.prior = def->second.prior;
			}
			int total = 0;
			std::array<float, NUM_DRIVES> sum{};
			for (auto& [k, other] : expectations) {
				if (k % 64 == decision && other.count > 0) {
					for (int i = 0; i < NUM_DRIVES; i++)
						sum[i] += other.delta[i] * other.count;
					total += other.count;
				}
			}
			if (total > 0) {
				float w = total + e.prior;
				for (int i = 0; i < NUM_DRIVES; i++)
					e.delta[i] = (sum[i] + e.delta[i] * e.prior) / w;
			}
			it = expectations.emplace(key, e).first;
		}
		return it->second;
	}

	// 3. Wanting: predicted drop in D, given current needs.
	float predictedRelief(const std::array<float, NUM_DRIVES>& d, float D, const Expectation& e) const {
		std::array<float, NUM_DRIVES> after{};
		for (int i = 0; i < NUM_DRIVES; i++)
			after[i] = std::clamp(d[i] + e.delta[i], 0.0f, 1.0f);
		return D - overallDrive(after);
	}

	std::vector<Candidate> evaluate(const MindPerception& p, const std::array<float, NUM_DRIVES>& d, float D) {
		std::vector<Candidate> out;
		// 5. Affect: negative valence dampens curiosity; Boredom raises it;
		// a pressing survival need quiets it.
		float survival = std::max({d[PAIN], d[HUNGER], d[COLD], d[HOT], d[INJURY], d[SUFFOCATE], d[THIRST]});
		float curiosity = prm.curiosity * (1.0f + prm.boredomCuriosityGain * d[BORED]) * std::clamp(1.0f + 10.0f * valence, 0.2f, 1.5f)
			* (1.0f - prm.survivalCuriosityDamp * std::clamp(survival, 0.0f, 1.0f));
		auto add = [&](int category, int decision, float distance, float edible) {
			// what smells of poison is never eaten (a grendel's single bite of
			// the game's cyanide berries was enough to collapse it)
			if (decision == prm.eatDecision && edible < 0.0f)
				return;
			Expectation& e = expectation(category, decision);
			Candidate c{category, decision, distance, 0, 0, 0};
			c.want = predictedRelief(d, D, e);
			// What does not smell of food is not expected to ease hunger (a
			// seed among fruit; the smell is read from what eating does), though
			// eating it may still be pleasant.
			if (decision == prm.eatDecision && edible < 0.05f && e.delta[HUNGER] < 0.0f) {
				Expectation unscented = e;
				unscented.delta[HUNGER] = 0.0f;
				c.want = predictedRelief(d, D, unscented);
			}
			c.novelty = curiosity / std::sqrt(1.0f + e.count);
			c.score = c.want + c.novelty - prm.distanceCost * distance;
			if (category >= 0 && category < (int)habituation.size()) {
				// 4. habituation scales the pull of an object
				float pull = c.want + c.novelty;
				c.score = pull * habituation[category] - prm.distanceCost * distance;
			}
			if (inEpisodeJustEnded && category == current.category && decision == current.decision)
				c.score += prm.commitment;
			out.push_back(c);
		};
		for (auto& a : *p.actions) {
			if (a.needsObject) {
				for (auto& s : p.visible)
					if (s.reachable)
						add(s.category, a.decision, s.distance, s.edible);
			} else {
				add(-1, a.decision, 0.0f, 0.0f);
			}
		}
		addSeekCandidates(p, d, D, out);
		inEpisodeJustEnded = false;
		return out;
	}
	bool inEpisodeJustEnded = false;

	// How much the best action on a category would help, counting only what
	// the creature has experienced or a specific instinct names: a vague hunch
	// ("eat anything when hungry") is reason to try what is at hand, not to
	// travel across the world for it.
	float knownRelief(const MindPerception& p, const std::array<float, NUM_DRIVES>& d, float D, int category) {
		float best = 0.0f;
		for (auto& a : *p.actions) {
			if (!a.needsObject)
				continue;
			Expectation& e = expectation(category, a.decision);
			if (e.count == 0 && e.prior <= 0.0f)
				continue;
			best = std::max(best, predictedRelief(d, D, e));
		}
		return best;
	}

	void rememberFood(float x, float y, unsigned int tick, float confidence, bool meal) {
		for (auto& fp : foodPlaces)
			if (std::fabs(fp.x - x) < prm.foodMerge && std::fabs(fp.y - y) < prm.sameLevel) {
				fp.x = (fp.x * 3.0f + x) / 4.0f;
				fp.tick = tick;
				fp.confidence = std::max(fp.confidence, confidence);
				fp.meals += meal ? 1 : 0;
				return;
			}
		if (foodPlaces.size() >= prm.foodPlaceMax) {
			// forget the least valuable: low confidence, few meals, long ago
			auto worst = std::min_element(foodPlaces.begin(), foodPlaces.end(), [](const FoodPlace& a, const FoodPlace& b) {
				return a.confidence * (1 + a.meals) < b.confidence * (1 + b.meals);
			});
			foodPlaces.erase(worst);
			foodTrip = -1; // indices moved
		}
		foodPlaces.push_back({x, y, tick, confidence, meal ? 1 : 0, 0});
	}

	// What eating is expected to do for hunger: a typical meal, or the best
	// meal of something food-smelling it has had, if that was more.
	float expectedMealRelief(const std::array<float, NUM_DRIVES>& d, float D) {
		Expectation typical;
		typical.delta[HUNGER] = -prm.scentHungerRelief * 0.6f;
		for (auto& [key, e] : expectations) {
			auto sc = scents.find(key / 64 - 1);
			if (key % 64 == prm.eatDecision && e.count > 0 && sc != scents.end() && sc->second >= 0.2f)
				typical.delta[HUNGER] = std::min(typical.delta[HUNGER], std::max(e.delta[HUNGER], -prm.scentHungerRelief * 1.5f));
		}
		return predictedRelief(d, D, typical);
	}

	float travelDiscount(float distance, int floors, float hunger) const {
		float half = prm.travelHalf * (1.0f + prm.hungerPatience * hunger * hunger);
		return std::pow(0.5f, (distance + floors * prm.floorChangeDistance) / half);
	}

	// Heading for the best place on the food map (same floor: walk; another
	// floor: steps toward changing floors).
	void addFoodSeek(const MindPerception& p, const std::array<float, NUM_DRIVES>& d, float D, bool canWest, bool canEast, std::vector<Candidate>& out) {
		for (auto& s : p.visible)
			if (s.reachable && s.edible > 0.1f)
				return; // food at hand
		float meal = expectedMealRelief(d, D);
		if (meal < prm.seekMinWant)
			return;
		const FoodPlace* bestSame = nullptr;
		const FoodPlace* bestOther = nullptr;
		float wantSame = 0.0f, wantOther = 0.0f;
		// progress on the current trip
		if (foodTrip >= 0 && foodTrip < (int)foodPlaces.size()) {
			FoodPlace& t = foodPlaces[foodTrip];
			float dist = std::fabs(t.x - p.x) + std::fabs(t.y - p.y);
			if (dist < foodTripBest - prm.foodTripGain) {
				foodTripBest = dist;
				foodTripSince = p.tick;
			} else if (p.tick > foodTripSince + prm.foodTripTicks) {
				t.confidence *= 0.5f; // out of reach from here, for now
				t.cooldownUntil = p.tick + prm.foodCooldown;
				foodTrip = -1;
			}
		}
		for (auto& fp : foodPlaces) {
			if (p.tick < fp.cooldownUntil)
				continue;
			float dx = std::fabs(fp.x - p.x), dy = std::fabs(fp.y - p.y);
			bool same = dy < prm.sameLevel;
			if (same && dx < prm.foodArrived)
				continue; // here already
			int floors = same ? 0 : std::max(1, (int)std::lround(dy / prm.scentFloorStep));
			float want = meal * fp.confidence * travelDiscount(dx, floors, d[HUNGER]);
			if (same && want > wantSame) {
				wantSame = want;
				bestSame = &fp;
			} else if (!same && want > wantOther) {
				wantOther = want;
				bestOther = &fp;
			}
		}
		// a known link toward the best place, if it saves a long way
		{
			const FoodPlace* target = (bestSame && wantSame >= wantOther) ? bestSame : bestOther;
			float want = std::max(wantSame, wantOther);
			Candidate t{};
			t.food = true;
			if (target && want >= prm.seekMinWant)
				addLinkToward(p, target->x, target->y, want, t, out);
		}
		auto startTrip = [&](const FoodPlace* fp) {
			int idx = (int)(fp - foodPlaces.data());
			if (idx != foodTrip) {
				foodTrip = idx;
				foodTripBest = std::fabs(fp->x - p.x) + std::fabs(fp->y - p.y);
				foodTripSince = p.tick;
			}
		};
		if (bestSame && wantSame >= prm.seekMinWant && wantSame >= wantOther) {
			startTrip(bestSame);
			int decision = bestSame->x < p.x ? 10 : 11;
			if (waterAhead(p, decision)) {
				Candidate t{};
				t.food = true;
				addBoatInstead(p, decision, wantSame, t, out);
				return;
			}
			if (!((decision == 10 && !canWest) || (decision == 11 && !canEast) || isBlocked(p, decision) || cliffAhead(p, decision, bestSame->y))) {
				Candidate c{-1, decision, std::fabs(bestSame->x - p.x), wantSame, 0, wantSame};
				c.chain = true;
				c.food = true;
				out.push_back(c);
				return;
			}
		}
		if (bestOther && wantOther >= prm.seekMinWant) {
			startTrip(bestOther);
			size_t before = out.size();
			addFloorChange(p, wantOther, bestOther->y, -1, canWest, canEast, out);
			for (size_t i = before; i < out.size(); i++)
				out[i].food = true;
		}
	}

	// Foraging: searching when nothing better is known.
	void addForage(const MindPerception& p, const std::array<float, NUM_DRIVES>& d, float D, bool canWest, bool canEast, std::vector<Candidate>& out) {
		float restless = std::clamp(hungryTicks / prm.restlessRamp, 0.0f, 1.0f);
		float threshold = prm.forageHunger - (prm.forageHunger - prm.forageHungerRestless) * restless;
		if (d[HUNGER] < threshold)
			return;
		for (auto& c : out)
			if (c.food || c.scent)
				return; // something better to go on
		for (auto& s : p.visible)
			if (s.edible > 0.1f)
				return;
		float want = expectedMealRelief(d, D) * prm.forageShare * (1.0f + prm.restlessBoost * restless) * (d[HUNGER] - threshold) / (1.0f - threshold);
		if (want < prm.seekMinWant)
			return;
		auto open = [&](int dir) { return ((dir == 10 && canWest) || (dir == 11 && canEast)) && !isBlocked(p, dir) && !waterAhead(p, dir) && !cliffAhead(p, dir); };
		// a known link to somewhere not visited lately: a way to search further
		{
			const TravelLink* bestLink = nullptr;
			float bestStale = 3000.0f; // ticks since visited
			for (auto& tl : travelLinks) {
				if (std::fabs(tl.fromX - p.x) > prm.linkNear || std::fabs(tl.fromY - p.y) > prm.sameLevel)
					continue;
				auto it = visited.find({(int)std::floor(tl.toX / prm.visitBinWidth), (int)std::floor(tl.toY / prm.visitBinHeight)});
				float stale = it == visited.end() ? 100000.0f : (float)(p.tick - it->second);
				if (stale > bestStale) {
					bestStale = stale;
					bestLink = &tl;
				}
			}
			if (bestLink) {
				Candidate t{};
				t.forage = true;
				addLinkToward(p, bestLink->toX, bestLink->toY, want, t, out);
			}
		}
		// water ahead with a boat in view: rowing across is a way to search further
		for (int dir : {10, 11})
			if (waterAhead(p, dir)) {
				Candidate t{};
				t.forage = true;
				addBoatInstead(p, dir, want, t, out);
			}
		if (forageHeading != 0 && p.tick > forageStartTick + prm.forageStuckTicks) {
			float progress = forageHeading == 10 ? forageStartX - p.x : p.x - forageStartX;
			if (progress < prm.foragePass) {
				forageStuckFlips++;
				int other = forageHeading == 10 ? 11 : 10;
				forageHeading = forageStuckFlips >= 2 || !open(other) ? 0 : other; // both ways stuck: another floor
				forageUntil = p.tick + prm.forageCommit;
			} else
				forageStuckFlips = 0;
			forageStartX = p.x;
			forageStartTick = p.tick;
		}
		if (forageStuckFlips >= 2) {
			// both ways stuck on this floor: look for food on another one
			size_t before = out.size();
			addFloorChange(p, want, p.y + forageVertical * prm.scentFloorStep, -1, canWest, canEast, out);
			for (size_t i = before; i < out.size(); i++)
				out[i].forage = true;
			if (std::fabs(p.y - forageStartY) > prm.minRide) {
				forageStuckFlips = 0; // reached another floor
				forageHeading = 0;
			}
			return;
		}
		if (forageHeading == 0 || p.tick >= forageUntil || !open(forageHeading)) {
			forageStartX = p.x;
			forageStartY = p.y;
			forageStartTick = p.tick;
			// staleness of the next few bins each way on this floor
			int bx = (int)std::floor(p.x / prm.visitBinWidth), by = (int)std::floor(p.y / prm.visitBinHeight);
			auto stale = [&](int dir) {
				float total = 0.0f;
				for (int k = 1; k <= prm.forageLookBins; k++) {
					auto it = visited.find({bx + (dir == 10 ? -k : k), by});
					total += it == visited.end() ? 10000.0f : (float)(p.tick - it->second);
				}
				return total;
			};
			// another floor? the less recently visited of the floors above and
			// below; more likely the longer the creature has gone hungry, and
			// certain once this floor has been searched both ways
			auto staleRow = [&](int dy) {
				float total = 0.0f;
				for (int k = -prm.forageLookBins; k <= prm.forageLookBins; k++) {
					auto it = visited.find({bx + k, by + dy});
					total += it == visited.end() ? 10000.0f : (float)(p.tick - it->second);
				}
				return total;
			};
			forageVertical = staleRow(-1) >= staleRow(1) ? -1 : 1;
			float searched = (float)prm.floorSearched * prm.forageLookBins;
			bool floorDone = stale(10) < searched && stale(11) < searched;
			float roll = (float)((p.tick * 2654435761u) % 1000u) / 1000.0f;
			if (floorDone || roll < prm.restlessLeaveFloor * restless) {
				forageStuckFlips = 2; // look on another floor (handled next tick)
				forageStartY = p.y;
				forageHeading = 0;
				forageUntil = p.tick + prm.forageCommit;
				return;
			}
			int choice = 0;
			if (open(10) && open(11))
				choice = stale(10) >= stale(11) ? 10 : 11;
			else if (open(10))
				choice = 10;
			else if (open(11))
				choice = 11;
			forageHeading = choice;
			forageUntil = p.tick + prm.forageCommit;
		}
		if (forageHeading != 0) {
			Candidate c{-1, forageHeading, 0, want, 0, want};
			c.chain = true;
			c.forage = true;
			out.push_back(c);
		} else {
			// both ways blocked: try another floor
			size_t before = out.size();
			addFloorChange(p, want, p.y + forageVertical * prm.scentFloorStep, -1, canWest, canEast, out);
			for (size_t i = before; i < out.size(); i++)
				out[i].forage = true;
		}
	}

	// Which boat action goes east (dir 11) or west (10): learned rides first,
	// then the innate guess (activate 1 east, activate 2 west; C2 decisions 1 and 2).
	int boatDecisionFor(int dir, unsigned int tick) const {
		int d = dir == 11 ? 1 : 2; // decisions: 1 activate 1, 2 activate 2
		for (auto& [decision, bl] : boatLinks)
			if (bl.rides > 0 && ((dir == 11 && bl.dx > 0) || (dir == 10 && bl.dx < 0))) {
				d = decision;
				break;
			}
		// a boat already moored on the far side ignores "that way": after a
		// press that gave no ride, try the other action for a while
		return tick < boatFlipUntil ? 3 - d : d;
	}

	// A trip walking dir would go into water: if a boat is in view, offer
	// rowing that way instead (and do not offer the walk).
	bool cliffAhead(const MindPerception& p, int dir, float targetY = -1e9f) const {
		float drop = dir == 10 ? p.dropWest : dir == 11 ? p.dropEast : 0.0f;
		if (drop <= prm.cliffDrop)
			return false;
		return !(targetY > p.y + drop * 0.5f); // a trip going that much lower may take the drop
	}
	bool waterAhead(const MindPerception& p, int dir) const {
		return !p.inWater && ((dir == 10 && p.waterWest) || (dir == 11 && p.waterEast));
	}
	void addBoatInstead(const MindPerception& p, int dir, float want, Candidate tmpl, std::vector<Candidate>& out) {
		bool reachableBoat = false; // seen from below or above, a boat cannot be boarded
		bool reachableButton = false;
		for (auto& sa : p.visible) {
			if (sa.category == prm.boatCategory && sa.reachable)
				reachableBoat = true;
			if (sa.category == prm.buttonCategory && sa.reachable)
				reachableButton = true;
		}
		if (!reachableBoat) {
			// no boat on this side: a button within reach may call one over
			// (the boat call switches; lift buttons too, harmlessly)
			const CallButton* known = nullptr;
			for (auto& cb : callButtons)
				if (std::fabs(cb.x - p.x) < 300.0f && std::fabs(cb.y - p.y) < prm.sameLevel)
					known = &cb;
			if (known && known->failed > known->worked)
				reachableButton = false; // this button does not call boats
			if (reachableButton) {
				float w = want * prm.boatShare * prm.boatCallShare * (known && known->worked > 0 ? prm.boatCallKnownBonus : 1.0f);
				Candidate c{prm.buttonCategory, 1, 0, w, 0, w};
				c.boatCall = true;
				c.chain = true;
				c.food = tmpl.food;
				c.forage = tmpl.forage;
				c.scent = tmpl.scent;
				out.push_back(c);
			}
			return;
		}
		Candidate c{prm.boatCategory, boatDecisionFor(dir, p.tick), 0, want * prm.boatShare, 0, want * prm.boatShare};
		c.chain = true;
		c.food = tmpl.food;
		c.forage = tmpl.forage;
		c.scent = tmpl.scent;
		out.push_back(c);
	}

	// A known travel link that starts near here and ends much closer to the
	// goal: go and use its object (seen and reachable). Returns whether one
	// was offered.
	bool addLinkToward(const MindPerception& p, float goalX, float goalY, float want, Candidate tmpl, std::vector<Candidate>& out) {
		const TravelLink* best = nullptr;
		float bestGain = prm.linkGain;
		float here = std::fabs(goalX - p.x) + std::fabs(goalY - p.y) * 2.0f;
		for (auto& tl : travelLinks) {
			if (std::fabs(tl.fromX - p.x) > prm.linkNear || std::fabs(tl.fromY - p.y) > prm.sameLevel)
				continue;
			bool reachable = false;
			for (auto& sa : p.visible)
				if (sa.category == tl.category && sa.reachable)
					reachable = true;
			if (!reachable)
				continue;
			float gain = here - (std::fabs(goalX - tl.toX) + std::fabs(goalY - tl.toY) * 2.0f);
			if (gain > bestGain) {
				bestGain = gain;
				best = &tl;
			}
		}
		if (!best)
			return false;
		Candidate c{best->category, best->decision, 0, want * prm.linkShare, 0, want * prm.linkShare};
		c.chain = true;
		c.food = tmpl.food;
		c.forage = tmpl.forage;
		out.push_back(c);
		return true;
	}

	void addEscape(const MindPerception& p, const std::array<float, NUM_DRIVES>& d, float D, bool canWest, bool canEast, std::vector<Candidate>& out) {
		if (!p.inWater)
			return;
		Expectation clear;
		clear.delta[SUFFOCATE] = -d[SUFFOCATE];
		float worth = predictedRelief(d, D, clear) + prm.escapeUrgency + prm.escapeBreathUrgency * (1.0f - p.breath) * (1.0f - p.breath);
		auto mark = [&](size_t from) {
			for (size_t i = from; i < out.size(); i++) {
				out[i].escape = true;
				out[i].chain = true; // no drive learning from the steps themselves
			}
		};
		auto tried = [&](const AgentRef& a) {
			for (auto& t : triedDevices)
				if (t.first && t.first == a && p.tick < t.second + prm.triedFor)
					return true;
			return false;
		};
		auto shownTried = [&](int category) {
			for (auto& s : p.visible)
				if (s.category == category)
					return tried(s.agent);
			return false;
		};
		// a remembered exit in view and near
		for (auto& e : exits) {
			if (std::fabs(e.x - p.x) > prm.exitNear || std::fabs(e.y - p.y) > prm.sameLevel || !isVisible(p, e.category) || shownTried(e.category))
				continue;
			Candidate c{e.category, e.decision, 0, worth * prm.exitBonus, 0, worth * prm.exitBonus};
			c.escape = true;
			out.push_back(c);
			return;
		}
		if (p.dryWay == 10 || p.dryWay == 11) {
			if (!((p.dryWay == 10 && !canWest) || (p.dryWay == 11 && !canEast) || isBlocked(p, p.dryWay))) {
				Candidate c{-1, p.dryWay, 0, worth, 0, worth};
				out.push_back(c);
				mark(out.size() - 1);
				return;
			}
		} else if (p.dryWay == -1 || p.dryWay == 1) {
			size_t before = out.size();
			addFloorChange(p, worth, p.y + p.dryWay * prm.scentFloorStep, -1, canWest, canEast, out);
			mark(before);
			if (out.size() > before)
				return;
		}
		// no open way out: try the devices in view (passing over handles the
		// ones that do nothing)
		bool offered = false;
		float triedX = 0;
		int triedSeen = 0;
		for (auto& s : p.visible) {
			if (std::find(prm.escapeDevices.begin(), prm.escapeDevices.end(), s.category) != prm.escapeDevices.end()) {
				if (tried(s.agent)) {
					triedX += s.x;
					triedSeen++;
					continue;
				}
				for (int decision : {1, 2}) { // activate 1 and 2
					bool can = false;
					for (auto& a : *p.actions)
						if (a.needsObject && a.decision == decision)
							can = true;
					if (!can)
						continue;
					Candidate c{s.category, decision, s.distance, worth * prm.escapeDeviceShare, 0, worth * prm.escapeDeviceShare};
					c.escape = true;
					out.push_back(c);
					offered = true;
				}
			}
		}
		// every device in view already tried: search the room, walking away
		// from them (the other way if that side is closed)
		if (!offered && triedSeen > 0) {
			int away = (triedX / triedSeen) < p.x ? 11 : 10;
			for (int dir : {away, away == 10 ? 11 : 10}) {
				if ((dir == 10 && !canWest) || (dir == 11 && !canEast) || isBlocked(p, dir))
					continue;
				Candidate c{-1, dir, 0, worth * prm.searchShare, 0, worth * prm.searchShare};
				out.push_back(c);
				mark(out.size() - 1);
				break;
			}
		}
	}

	// A device that once flooded the creature, used the same way near the
	// same place
	bool isFlooder(const MindPerception& p, int category, int decision) const {
		if (category < 0)
			return false;
		for (auto& f : flooders)
			if (f.category == category && f.decision == decision && std::fabs(f.x - p.x) < prm.flooderNear && std::fabs(f.y - p.y) < prm.sameLevel)
				return true;
		return false;
	}

	// Seeking: for each remembered place whose category is out of sight, how
	// much would the best action on it help? If enough, offer walking toward
	// it (C2 decisions 10 and 11: travel west and east).
	void addSeekCandidates(const MindPerception& p, const std::array<float, NUM_DRIVES>& d, float D, std::vector<Candidate>& out) {
		bool canWest = false, canEast = false;
		for (auto& a : *p.actions) {
			if (!a.needsObject && a.decision == 10)
				canWest = true;
			if (!a.needsObject && a.decision == 11)
				canEast = true;
		}
		if (!canWest && !canEast)
			return;
		std::vector<int> forget;
		std::vector<int> offLevel;
		for (auto& [category, place] : places) {
			if (isVisible(p, category))
				continue;
			float dx = place.x - p.x, dy = place.y - p.y;
			if (std::fabs(dy) > prm.seekMaxHeight) {
				offLevel.push_back(category);
				continue;
			}
			if (std::fabs(dx) < prm.seekArrived) {
				forget.push_back(category); // here, and it is gone
				continue;
			}
			float want = knownRelief(p, d, D, category) * prm.seekDiscount;
			if (want < prm.seekMinWant)
				continue;
			int decision = dx < 0 ? 10 : 11;
			if ((decision == 10 && !canWest) || (decision == 11 && !canEast) || isBlocked(p, decision))
				continue;
			Candidate c{-1, decision, std::fabs(dx), want, 0, 0};
			c.score = want - prm.seekDistanceCost * std::fabs(dx);
			c.seekCategory = category;
			out.push_back(c);
		}
		for (int category : forget)
			places.erase(category);

		// The most wanted thing remembered on another floor: take steps
		// toward changing floors.
		float bestWant = 0.0f;
		int bestCategory = -1;
		for (int category : offLevel) {
			float best = knownRelief(p, d, D, category);
			if (best > bestWant) {
				bestWant = best;
				bestCategory = category;
			}
		}
		if (bestCategory >= 0 && bestWant * prm.floorChangeDiscount >= prm.seekMinWant)
			addFloorChange(p, bestWant * prm.floorChangeDiscount, places[bestCategory].y, bestCategory, canWest, canEast, out);

		addEscape(p, d, D, canWest, canEast, out);
		addFoodSeek(p, d, D, canWest, canEast, out);
		addScentFollow(p, d, D, canWest, canEast, out);
		addForage(p, d, D, canWest, canEast, out);
	}

	void addScentFollow(const MindPerception& p, const std::array<float, NUM_DRIVES>& d, float D, bool canWest, bool canEast, std::vector<Candidate>& out) {
		// food already within reach: no need to follow a smell
		for (auto& s : p.visible)
			if (s.reachable && s.edible > 0.1f)
				return;
		// what eating smelled food is expected to do: at a typical scent, or
		// what the best food it has eaten so far did, if that was more
		Expectation typical;
		typical.delta[HUNGER] = -prm.scentHungerRelief * 0.6f;
		for (auto& [key, e] : expectations) {
			auto sc = scents.find(key / 64 - 1);
			if (key % 64 == prm.eatDecision && e.count > 0 && sc != scents.end() && sc->second >= 0.2f)
				typical.delta[HUNGER] = std::min(typical.delta[HUNGER], std::max(e.delta[HUNGER], -prm.scentHungerRelief * 1.5f));
		}
		float want = predictedRelief(d, D, typical) * prm.scentFollowShare;
		if (want < prm.seekMinWant)
			return;
		float threshold = p.scentHere * prm.scentStronger + 0.01f;
		struct Side {
			float scent;
			int kind; // 10 west, 11 east, -1 up, 1 down
		};
		Side sides[4] = {{p.scentWest, 10}, {p.scentEast, 11}, {p.scentUp, -1}, {p.scentDown, 1}};
		const Side* best = nullptr;
		for (auto& side : sides)
			if (side.scent > threshold && (!best || side.scent > best->scent))
				best = &side;
		if (!best)
			return;
		if (best->kind == 10 || best->kind == 11) {
			if (waterAhead(p, best->kind)) {
				Candidate t{};
				t.scent = true;
				addBoatInstead(p, best->kind, want, t, out);
				return;
			}
			if ((best->kind == 10 && !canWest) || (best->kind == 11 && !canEast) || isBlocked(p, best->kind) || cliffAhead(p, best->kind))
				return;
			Candidate c{-1, best->kind, 0, want, 0, want};
			c.chain = true;
			c.scent = true;
			out.push_back(c);
		} else {
			size_t before = out.size();
			addFloorChange(p, want, p.y + best->kind * prm.scentFloorStep, -1, canWest, canEast, out);
			for (size_t i = before; i < out.size(); i++)
				out[i].scent = true;
		}
	}

	void addFloorChange(const MindPerception& p, float want, float targetY, int target, bool canWest, bool canEast, std::vector<Candidate>& out) {
		float direction = targetY > p.y ? 1.0f : -1.0f; // +1 down, -1 up
		auto walkTo = [&](float x) {
			int decision = x < p.x ? 10 : 11;
			if ((decision == 10 && !canWest) || (decision == 11 && !canEast) || isBlocked(p, decision))
				return;
			Candidate c{-1, decision, std::fabs(x - p.x), want, 0, 0};
			c.score = want - prm.seekDistanceCost * std::fabs(x - p.x);
			c.seekCategory = target;
			c.chain = true;
			out.push_back(c);
		};
		auto actOn = [&](int category, int decision, float share) {
			Candidate c{category, decision, 0, want * share, 0, 0};
			c.score = c.want;
			c.chain = true;
			out.push_back(c);
		};
		const SeenAgent* lift = nullptr;
		const SeenAgent* button = nullptr;
		for (auto& s : p.visible) {
			if (s.category == prm.liftCategory)
				lift = &s;
			// only a call button on this floor, within reach: far-east norns
			// on a ledge pressed the call button of the deck below them over
			// a thousand times
			if (s.category == prm.buttonCategory && s.reachable && std::fabs(s.y - p.y) < prm.sameLevel)
				button = &s;
		}
		// a known link from this floor in the right direction, nearest first
		const Link* link = nullptr;
		for (auto& l : links) {
			if (std::fabs(l.fromY - p.y) > prm.sameLevel || (l.toY - l.fromY) * direction <= 0)
				continue;
			if (!link || std::fabs(l.x - p.x) < std::fabs(link->x - p.x))
				link = &l;
		}
		bool liftHere = lift && std::fabs(lift->x - p.x) < prm.atPlace * 1.5f && std::fabs(lift->y - p.y) < prm.sameLevel;
		if (link) {
			if (std::fabs(link->x - p.x) > prm.atPlace)
				walkTo(link->x);
			else if (liftHere)
				actOn(prm.liftCategory, link->decision, 1.0f);
			else if (button)
				actOn(prm.buttonCategory, 1, 0.8f); // call the lift
			return;
		}
		// no link yet: go to the nearest lift and try what it does
		if (lift) {
			if (std::fabs(lift->x - p.x) > prm.atPlace)
				walkTo(lift->x);
			else if (liftHere) {
				actOn(prm.liftCategory, 1, 0.6f);
				actOn(prm.liftCategory, 2, 0.6f);
			} else if (button)
				actOn(prm.buttonCategory, 1, 0.8f);
		} else if (button) {
			actOn(prm.buttonCategory, 1, 0.5f);
		}
	}

	void applyScent(int category, float edible) {
		Expectation& e = expectation(category, prm.eatDecision);
		if (edible <= 0.0f) {
			if (e.count == 0 && e.delta[HUNGER] < 0.0f)
				e.delta[HUNGER] = 0.0f; // does not smell like food
			return;
		}
		float target = -prm.scentHungerRelief * edible;
		float old = e.count + e.prior;
		e.delta[HUNGER] = (e.delta[HUNGER] * old + target * prm.scentWeight) / (old + prm.scentWeight);
		e.prior += prm.scentWeight;
	}

	void learnRide(const MindPerception& p) {
		float dy = p.y - rideFromY;
		if (std::fabs(dy) < prm.minRide || lastLiftDecision < 0)
			return;
		for (auto& l : links) {
			if (std::fabs(l.x - rideFromX) < prm.atPlace && std::fabs(l.fromY - rideFromY) < prm.sameLevel &&
				std::fabs(l.toY - p.y) < prm.sameLevel && l.decision == lastLiftDecision) {
				l.uses++;
				return;
			}
		}
		links.push_back({rideFromX, rideFromY, p.y, lastLiftDecision, 1});
		fmt::print("[homeostat] creature {} learned a ride: {} at x {:.0f} takes it from y {:.0f} to y {:.0f}\n", p.creatureId,
			actionName(lastLiftDecision), rideFromX, rideFromY, p.y);
	}

	// 6. Genome instincts become innate expectations (gene-translation.md).
	void applyInstinct(const MindPerception& p, const InnateInstinct& inst) {
		if (appliedInstincts.count(inst.signature))
			return;
		appliedInstincts.insert(inst.signature);
		std::vector<int> drives, categories;
		bool dormant = false;
		for (auto& c : inst.conditions) {
			if (c.kind == InnateCondition::Drive && c.index >= 0 && c.index < NUM_DRIVES)
				drives.push_back(c.index);
			else if (c.kind == InnateCondition::Attention)
				categories.push_back(c.index);
			else
				dormant = true; // heard words and senses: waiting for language and senses
		}
		if (drives.empty())
			dormant = true; // reward with no drive: waiting for the approval feeling
		if (dormant) {
			dormantInstincts++;
			return;
		}
		std::array<float, NUM_DRIVES> delta{};
		for (int dr : drives)
			delta[dr] = (inst.reward ? -1.0f : 1.0f) * prm.instinctDriveChange * inst.strength;
		float weight = 1.0f + (prm.legacyInstinctResilience - 1.0f) * inst.strength * inst.strength;

		bool needsObject = false;
		for (auto& a : *p.actions)
			if (a.decision == inst.decision)
				needsObject = a.needsObject;
		auto blend = [&](Expectation& e) {
			float old = e.count + e.prior;
			for (int i = 0; i < NUM_DRIVES; i++)
				e.delta[i] = (e.delta[i] * old + delta[i] * weight) / (old + weight);
			e.prior += weight;
		};
		if (!needsObject) {
			blend(expectation(-1, inst.decision));
		} else if (!categories.empty()) {
			for (int c : categories)
				blend(expectation(c, inst.decision));
		} else {
			// any object: a weak hunch, the starting point for categories not
			// yet met and added to those already known; experience with each
			// kind of object soon outweighs it
			// The hunch sets the starting expectation but adds no weight
			// against experience: one disappointing try should undo it.
			weight = std::min(weight, prm.anyObjectInstinctWeight);
			auto hunch = [&](Expectation& e) {
				float old = e.count + e.prior;
				for (int i = 0; i < NUM_DRIVES; i++)
					e.delta[i] = (e.delta[i] * old + delta[i] * weight) / (old + weight);
			};
			hunch(innateDefaults[inst.decision]);
			for (auto& [key, e] : expectations)
				if (key % 64 == inst.decision && key / 64 - 1 >= 0)
					hunch(e);
		}
		activeInstincts++;
		fmt::print("[homeostat] creature {} instinct: {} {} when{} (strength {:.2f}, weight {:.1f})\n", p.creatureId,
			actionName(inst.decision), inst.reward ? "rewarded" : "punished",
			[&] { std::string w; for (int dr : drives) w += std::string(" ") + DRIVE_NAMES[dr]; for (int c : categories) w += fmt::format(" seeing {}", c); return w; }(),
			inst.strength, weight);
	}

	// Softmax choice; temperature falls as arousal (D) rises.
	// Softmax over the top few candidates (an attention bottleneck, so many
	// mediocre options cannot outweigh a clearly better one together).
	Candidate pick(std::vector<Candidate> all, float D) {
		// What to do, then what to do it to: keep only each action's best
		// object, so one action on many objects cannot crowd out the rest.
		std::map<int, Candidate> bestPerAction;
		for (auto& c : all) {
			auto it = bestPerAction.find(c.decision);
			if (it == bestPerAction.end() || c.score > it->second.score)
				bestPerAction[c.decision] = c;
		}
		std::vector<Candidate> cands;
		for (auto& [decision, c] : bestPerAction)
			cands.push_back(c);
		std::sort(cands.begin(), cands.end(), [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
		if (cands.size() > prm.choiceSetSize)
			cands.resize(prm.choiceSetSize);
		float tau = prm.temperature / (1.0f + prm.arousalFocus * D);
		std::vector<double> w;
		for (auto& c : cands)
			w.push_back(std::exp((c.score - cands[0].score) / tau));
		std::discrete_distribution<size_t> dist(w.begin(), w.end());
		return cands[dist(rng)];
	}

	// 1 and 2: reward and learning at the end of an episode.
	void endEpisode(const MindPerception& p, const std::array<float, NUM_DRIVES>& d, float D, const char* why) {
		if (current.decision == prm.eatDecision && current.category >= 0 && felt[HUNGER] < -0.05f)
		{
			rememberFood(p.x, p.y, p.tick, 1.0f, true); // a meal here eased hunger
			hungryTicks = 0;
		}
		inEpisode = false;
		inEpisodeJustEnded = true;
		episodes++;
		std::array<float, NUM_DRIVES> change{};
		float total = 0.0f;
		for (int i = 0; i < NUM_DRIVES; i++) {
			change[i] = d[i] - startDrives[i];
			total += std::fabs(change[i]);
		}
		float reward = startD - D;

		// an object that keeps doing nothing: notice another of its kind
		// (not for steps toward a destination, such as pressing a lift's call
		// button: their benefit comes later, from the ride)
		if (current.category >= 0 && episodeAgent && !current.chain && !current.food && !current.forage && !current.scent) {
			bool moved = std::fabs(p.y - startY) > prm.minRide || std::fabs(p.x - startX) > 300.0f;
			if (reward < prm.passOverReward && !moved) {
				uselessInARow = (episodeAgent == lastUselessAgent) ? uselessInARow + 1 : 1;
				lastUselessAgent = episodeAgent;
				if (uselessInARow >= prm.passOverAfter) {
					pendingPassOver = episodeAgent;
					if (current.escape) {
						triedDevices.erase(std::remove_if(triedDevices.begin(), triedDevices.end(), [&](auto& t) { return !t.first || t.first == episodeAgent || p.tick >= t.second + prm.triedFor; }), triedDevices.end());
						triedDevices.push_back({episodeAgent, p.tick});
					}
					uselessInARow = 0;
				}
			} else
				uselessInARow = 0;
		}

		// Learn what the action did beyond the passage of time: subtract the
		// baseline drift expected over this many ticks, then update the
		// baseline itself.
		int ticks = std::max(episodeTicks, 1);
		std::array<float, NUM_DRIVES> effect{};
		for (int i = 0; i < NUM_DRIVES; i++) {
			// (the passage of time cannot take a drive past its limits)
			float passiveChange = std::clamp(startDrives[i] + drift[i] * ticks, 0.0f, 1.0f) - startDrives[i];
			effect[i] = change[i] - passiveChange;
			drift[i] += prm.driftRate * (change[i] / ticks - drift[i]);
		}

		if ((current.decision == 10 || current.decision == 11) && current.category < 0 && (current.seekCategory >= 0 || current.chain) &&
			std::fabs(p.x - startX) < prm.blockedMove) {
			blocked[current.decision == 10 ? 0 : 1] = {p.x, p.tick + prm.blockedTicks};
			fmt::print("[homeostat] creature {} t{}: the way {} is blocked at x {:.0f}\n", p.creatureId, p.tick,
				current.decision == 10 ? "west" : "east", p.x);
		}

		if (current.category < 0 && current.decision == prm.sleepDecision) {
			// the place's warmth is not something sleeping does (see badSleepNear)
			for (int i : {COLD, HOT})
				if (!feltAny[i])
					effect[i] = 0.0f;
		}
		if (current.seekCategory < 0 && !current.chain) {
			Expectation& e = expectation(current.category, current.decision);
			// an innate prior counts as remembered experience: learning is slow
			// while it outweighs real experience and speeds up as that grows
			float rate = prm.learningRate * (e.count + 1.0f) / (e.count + 1.0f + e.prior);
			for (int i = 0; i < NUM_DRIVES; i++) {
				if (feltAny[i]) {
					// a drive cannot go below empty or above full
					float target = std::clamp(startDrives[i] + felt[i], 0.0f, 1.0f) - startDrives[i];
					e.delta[i] += rate * (target - e.delta[i]);
				} else {
					e.delta[i] += rate * prm.unfeltLearningShare * (effect[i] - e.delta[i]);
				}
			}
			e.count++;
		}

		// Mood follows how much better or worse this went than just letting
		// time pass (reward relative to the drift baseline).
		std::array<float, NUM_DRIVES> passive{};
		for (int i = 0; i < NUM_DRIVES; i++)
			passive[i] = std::clamp(startDrives[i] + (change[i] - effect[i]), 0.0f, 1.0f);
		float advantage = overallDrive(passive) - D;
		mood += prm.moodRate * (advantage - mood);

		valence += prm.valenceRate * (reward - valence);

		if (current.category >= 0 && current.category < (int)habituation.size()) {
			if (total < prm.eventfulThreshold)
				habituation[current.category] *= prm.habituationRate;
			else
				habituation[current.category] = 1.0f; // dishabituation
		}

		// the three largest drive changes, for the log
		std::array<int, NUM_DRIVES> idx;
		for (int i = 0; i < NUM_DRIVES; i++)
			idx[i] = i;
		std::sort(idx.begin(), idx.end(), [&](int a, int b) { return std::fabs(change[a]) > std::fabs(change[b]); });
		std::string changes;
		for (int k = 0; k < 3; k++) {
			if (std::fabs(change[idx[k]]) < 0.005f)
				break;
			changes += fmt::format(" {} {:+.2f}", DRIVE_NAMES[idx[k]], change[idx[k]]);
		}
		std::sort(idx.begin(), idx.end(), [&](int a, int b) { return std::fabs(effect[a]) > std::fabs(effect[b]); });
		std::string effects;
		for (int k = 0; k < 3; k++) {
			if (std::fabs(effect[idx[k]]) < 0.005f)
				break;
			effects += fmt::format(" {} {:+.2f}", DRIVE_NAMES[idx[k]], effect[idx[k]]);
		}
		std::string feltText;
		for (int i = 0; i < NUM_DRIVES; i++)
			if (feltAny[i])
				feltText += fmt::format(" {} {:+.2f}", DRIVE_NAMES[i], felt[i]);
		fmt::print("[homeostat] creature {} t{}: {}@{} ended ({}, {} ticks): reward {:+.3f}{} | beyond drift:{} | felt:{} | D {:.2f} valence {:+.3f}\n",
			p.creatureId, p.tick, current.seekCategory >= 0 ? fmt::format("seek{}:{}", current.seekCategory, actionName(current.decision)) : actionName(current.decision),
			current.category, why, episodeTicks, reward,
			changes.empty() ? " (no drive change)" : changes, effects.empty() ? " nothing" : effects, feltText.empty() ? " nothing" : feltText, D, valence);
	}

	void logChoice(const MindPerception& p, const Candidate& choice, std::vector<Candidate> cands, float D) {
		std::sort(cands.begin(), cands.end(), [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
		std::string top;
		for (size_t i = 0; i < cands.size() && i < 3; i++)
			top += fmt::format(" {}@{} {:+.3f}", actionName(cands[i].decision), cands[i].category, cands[i].score);
		std::string needs;
		for (int i = 0; i < NUM_DRIVES; i++)
			if (p.drives[i] >= 64)
				needs += fmt::format(" {} {}", DRIVE_NAMES[i], (int)p.drives[i]);
		fmt::print("[homeostat] creature {} t{}: chose {}@{} (want {:+.3f} novelty {:+.3f}) | top:{} | needs:{} | D {:.2f} | smell {:.2f} W {:.2f} E {:.2f} U {:.2f} D {:.2f} | at {:.0f},{:.0f} | restless {:.2f} | food map {}\n",
			p.creatureId, p.tick, choice.boatCall ? fmt::format("callboat:{}", actionName(choice.decision)) : choice.escape ? fmt::format("escape:{}", actionName(choice.decision)) : choice.food ? fmt::format("food:{}", actionName(choice.decision)) : choice.forage ? fmt::format("forage:{}", actionName(choice.decision)) : choice.scent ? fmt::format("scent:{}", actionName(choice.decision)) : choice.seekCategory >= 0 ? fmt::format("seek{}:{}", choice.seekCategory, actionName(choice.decision)) : actionName(choice.decision),
			choice.category, choice.want, choice.novelty,
			top, needs.empty() ? " none high" : needs, D, p.scentHere, p.scentWest, p.scentEast, p.scentUp, p.scentDown, p.x, p.y, std::clamp(hungryTicks / prm.restlessRamp, 0.0f, 1.0f),
			[&] { std::string m; for (auto& fp : foodPlaces) m += fmt::format(" {:.0f},{:.0f}x{:.2f}", fp.x, fp.y, fp.confidence); return m.empty() ? std::string(" none") : m; }());
	}
};

} // namespace

std::unique_ptr<CreatureMind> createHomeostatMind() {
	return std::make_unique<HomeostatMind>();
}
