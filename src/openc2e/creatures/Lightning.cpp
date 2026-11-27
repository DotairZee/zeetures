/*
 *  Lightning.cpp
 *  openc2e
 *
 *  What a lightning bolt does to what it strikes (see Lightning.h).
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 */

#include "Lightning.h"

#include "Agent.h"
#include "Camera.h"
#include "Engine.h"
#include "SpritePart.h"
#include "World.h"
#include "WorldSave.h"
#include "caosValue.h"
#include "common/Random.h"
#include "common/devenv.h"
#include "creatures/Breeding.h"
#include "creatures/CreatureAgent.h"
#include "creatures/CreatureSpeech.h"
#include "creatures/Disease.h"
#include "creatures/Exposure.h"
#include "creatures/Relationships.h"
#include "creatures/SkeletalCreature.h"
#include "creatures/Temperament.h"
#include "creatures/oldCreature.h"
#include "historyManager.h"

#include <algorithm>
#include <cmath>
#include <fmt/core.h>
#include <map>
#include <ostream>
#include <vector>

namespace Lightning {

namespace {

int s_strange = -1; // -1: not read yet

// C2 chemicals (drive d is chemical d + 1)
enum Chem { PAIN = 1, HUNGER = 3, COLD = 4, HOT = 5, TIRED = 6, SLEEPY = 7, LONELY = 8, FEAR = 10, ANGER = 12, SEX = 13, LIFE = 76, DANCING = 171 };

// The odds (copies of Home, 2026-11-26: storms keep forming over the
// desert's top, and a creature there was struck about nine times in 100
// game minutes, 200-325 strikes a run). Something strange on one strike in
// eight, at most once in ten minutes for a creature; a power far more
// rarely; Frankenstein's creature rarer still (a creature struck within
// half a minute of dying, one time in four).
const float SURPRISE = 0.12f;
const unsigned int SURPRISE_AGAIN = 6000;
const float POWER = 0.03f;
const unsigned int POWER_TICKS = 36000; // about an hour
const float FRANKENSTEIN = 0.25f;
const unsigned int STRUCK_AGAIN = 60; // one strike per bolt, however many segments touch it
const float STATIC_REACH = 50.0f;
const unsigned int SHOCK_AGAIN = 300;

// effects that last a while, and how long
enum Timed { SCORCH, GLOW, CHARGE, ELECTRO, FAST, SLOW, WIRED, SIZE, FROZEN, CONTRARY, FEARLESS, SKYFEAR, BACKWARDS, PITCH, PARROT, ROD, MAGNETIC, DANCE, NUM_TIMED };
const unsigned int DURATION[NUM_TIMED] = {3000, 6000, 600, 6000, 600, 1200, 3000, 6000, 36000, 3000, 3000, 36000, 1800, 6000, 1800, 3000, 600, 100};
const char* const TIMED_KEY[NUM_TIMED] = {"scorched", "glowing", "charged", "machines", "fast", "slow", "wired", "size", "frozen", "contrary", "fearless", "stormshy",
	"backwards", "pitch", "parrot", "rod", "magnetic", "dancing"};

enum Power { HEALER, GREEN, POLYGLOT, WATER, STORMSENSE, FEATHER, BEAST, SPARK, LEADER, FOODFINDER, WEATHERPROOF, LUCKY, NUM_POWERS };
const char* const POWER_NAME[NUM_POWERS] = {"Healer", "Green thumb", "Polyglot", "Water-breather", "Weatherwise", "Featherweight", "Beast-friend", "Spark-hands",
	"Leader", "Food-finder", "Weatherproof", "Lucky"};

struct Struck {
	unsigned int lastStruck = 0, lastSurprise = 0;
	std::array<unsigned int, NUM_TIMED> until{};
	int power = -1;
	unsigned int powerUntil = 0;
	bool frankenstein = false;
	int lifeHold = -1; // the Life chemical held (frozen in time, or Frankenstein's creature)
	float size = 1.0f, pitch = 1.0f;
	std::map<std::string, float> contraryWas; // affinities before they flipped
	float accgWas = -1; // its gravity before it turned featherweight
	bool featherOn = false; // (the lighter gravity on its body now; not saved)
	int lastHunger = -1; // (lucky)
	unsigned int parrotAt = 0, warnedAt = 0;
	bool photoWanted = false; // (risen: a photo once it is grown and has come round)
	unsigned int posingUntil = 0; // holding its photo pose until then
	std::string match; // the other Frankenstein's creature it found (both mortal now): they die together
	Look shownLook = NONE;
	float shownSize = 1.0f;
};
std::map<std::string, Struck> s_struck; // by moniker
std::map<std::string, int> s_germline; // moniker -> extra mutations owed to its next child
std::vector<AgentRef> s_seenBolts;
std::map<Agent*, unsigned int> s_thingStruck; // eggs and plants
std::map<std::string, unsigned int> s_shocked; // moniker -> last static shock felt
std::vector<AgentRef> s_photoWanted;
struct Couple {
	std::string names;
	unsigned int tick = 0;
};
std::vector<Couple> s_couples; // on the memorial stone (saved)
int s_lastRisenSex = -1; // the last to rise as Frankenstein's creature: 1 male, 2 female (saved); the next must be the other

bool watching() {
	static const bool w = devenv("OPENC2E_WATCH_LIGHTNING") != nullptr;
	return w;
}

oldCreature* creatureOf(Agent* a) {
	auto* ca = a ? dynamic_cast<CreatureAgent*>(a) : nullptr;
	return ca ? dynamic_cast<oldCreature*>(ca->getCreature()) : nullptr;
}

// (by its genome, checked against its body: a creature made younger is a
// new body with a copy of its genome, its moniker moved to it, so then by
// its body)
std::string monikerOf(oldCreature* c) {
	if (!c || !world.history)
		return std::string();
	std::string mon = world.history->findMoniker(c->getGenome());
	Agent* body = c->getAgent();
	if (!mon.empty() && (!body || world.history->getMoniker(mon).owner.get() == body))
		return mon;
	return body ? world.history->findMoniker(AgentRef(body)) : mon;
}

Struck* find(Agent* a) {
	if (s_struck.empty())
		return nullptr;
	auto it = s_struck.find(monikerOf(creatureOf(a)));
	return it == s_struck.end() ? nullptr : &it->second;
}

bool on(const Struck* s, int t) {
	return s && s->until[t] > world.tickcount;
}

bool hasPower(const Struck* s, int p) {
	return s && s->power == p && s->powerUntil > world.tickcount;
}

std::string wordAt(int w) {
	if (w < 0 || (size_t)w >= engine.wordlist.size())
		return "";
	std::string s = engine.wordlist[w];
	while (!s.empty() && s.back() == ' ')
		s.pop_back();
	return s;
}

bool overlaps(Agent* a, float x0, float y0, float x1, float y1) {
	return a->x < x1 && a->x + a->getWidth() > x0 && a->y < y1 && a->y + a->getHeight() > y0;
}

float cx(Agent* a) { return a->x + a->getWidth() / 2.0f; }
float cy(Agent* a) { return a->y + a->getHeight() / 2.0f; }

void setChem(oldCreature* c, int chem, int v) {
	int now = c->getChemical((unsigned char)chem);
	v = std::clamp(v, 0, 255);
	if (v > now)
		c->addChemical((unsigned char)chem, (unsigned char)(v - now));
	else if (v < now)
		c->subChemical((unsigned char)chem, (unsigned char)(now - v));
}

void addChem(oldCreature* c, int chem, int amount) {
	setChem(c, chem, c->getChemical((unsigned char)chem) + amount);
}

bool electric(int category) {
	return category == 2 || category == 8 || category == 27 || category == 28 || category == 29 || category == 32 || category == 33 || category == 35;
}

Look lookNow(const Struck& s) {
	if (s.frankenstein)
		return PALLID;
	if (on(&s, GLOW))
		return GLOWING;
	if (on(&s, SCORCH))
		return SCORCHED;
	return NONE;
}

// its pictures rebuilt when its look or size changes
void refreshLook(Agent* a, Struck& s) {
	Look l = lookNow(s);
	float k = on(&s, SIZE) ? s.size : 1.0f;
	if (l == s.shownLook && k == s.shownSize)
		return;
	s.shownLook = l;
	s.shownSize = k;
	if (auto* sc = dynamic_cast<SkeletalCreature*>(a))
		sc->skeletonInit();
}

void tell(const std::string& mon, const std::string& detail) {
	world.history->tell(mon, storyEntry::struck, "", detail);
}

void say(Agent* a, oldCreature* c, const std::string& meaning) {
	creatureSays(a, (int)c->getStage(), phraseFor(meaning, a, c));
}

// the effects that run while they last, or end
void endContrary(oldCreature* c, Struck& s) {
	for (auto& [other, was] : s.contraryWas)
		if (auto it = c->bonds.find(other); it != c->bonds.end())
			it->second.affinity = was;
	s.contraryWas.clear();
}

// Frankenstein's creature: its records and nature (risen, or made so by a test)
void markFrankenstein(std::shared_ptr<Agent> a, oldCreature* c, const std::string& mon, const std::string& from) {
	Struck& s = s_struck[mon];
	s.frankenstein = true;
	s.lifeHold = -1; // (held once it is grown: tick)
	s.photoWanted = true;
	static const char* const hers[] = {"Safie", "Agatha", "Justine", "Elizabeth", "Caroline", "Margaret"};
	static const char* const his[] = {"Adam", "Felix", "Henry", "Ernest", "William", "Robert", "Victor"};
	std::string name = c->isFemale() ? hers[rand_size_t(0, 5)] : his[rand_size_t(0, 6)];
	monikerData& m = world.history->getMoniker(mon);
	m.name = name;
	m.borntick = world.worldtickcount;
	m.bornknown = true;
	m.registered = false;
	world.history->tell(mon, storyEntry::struck, from, from.empty() ? "rose again as Frankenstein's creature" : "rose as Frankenstein's creature from the body of");
	size_t before = world.history->notices.size();
	world.history->logLife(lifeLogEntry::cameRound, mon, (int)c->getStage());
	if (world.history->notices.size() > before)
		world.history->notices.back().cause = "rose as Frankenstein's creature";
	if (!world.history->lifelog.empty() && world.history->lifelog.back().moniker == mon)
		world.history->lifelog.back().cause = "rose as Frankenstein's creature";
	// grown, whatever age it died at (owner, 2026-11-26)
	if ((int)c->getStage() != 4)
		WorldSave::RequestLifeStage(a, 4);
	s_lastRisenSex = c->isFemale() ? 2 : 1;
	refreshLook(a.get(), s);
	fmt::print("* {} rose as Frankenstein's creature, {}{}\n", mon, name, from.empty() ? "" : fmt::format(" (from the body of {})", from));
}

// Risen from a body just dead (owner, 2026-11-26: "like a new birth"): the
// dead creature's death stays in the records, and the body that rises is
// a new creature with a new moniker, the dead one its parent in the family
// tree; it starts with no words and no bonds (Shelley's creature learned
// its language anew, by listening), grown to an adult, and is named and
// photographed afresh.
void riseAsFrankenstein(std::shared_ptr<Agent> a, c2Creature* c, const std::string& was) {
	c->riseAgain();
	if (!c->isAlive())
		return;
	monikerData& old = world.history->getMoniker(was);
	old.genome.reset();
	old.owner = AgentRef();
	world.newMoniker(c->getGenome(), "risen", AgentRef(a));
	std::string mon = world.history->findMoniker(c->getGenome());
	if (mon.empty() || mon == was)
		return;
	monikerData& m = world.history->getMoniker(mon);
	m.moveToCreature(AgentRef(a));
	m.wasBorn();
	(c->isFemale() ? m.mother : m.father) = was; // (the same body: the same sex)
	historyManager::lineageGeneration++;
	// a mind and a tongue to learn afresh
	std::fill(c->vocab.begin(), c->vocab.end(), 0);
	c->ownNames.clear();
	c->lang = LanguageMemory{};
	c->bonds.clear();
	markFrankenstein(a, c, mon, was);
}

// something strange, besides the jolt
void surprise(std::shared_ptr<Agent> agent, oldCreature* c, const std::string& mon, Struck& s) {
	enum { KNOCKOUT, FORGET, KNOW, TEMPERAMENT, TIMESLIP, MUTAGEN, RAVENOUS, THROWN, HEAL, FEVER, OVERDUE, LOVESTRUCK, INSIGHT, INSTANT, TIMED0 = INSTANT };
	const int COUNT = INSTANT + NUM_TIMED;
	unsigned int now = world.tickcount;
	std::vector<int> known, unknown, half;
	for (size_t w = 0; w < engine.wordlist.size(); w++) {
		if (wordAt((int)w).empty())
			continue;
		int v = w < c->vocab.size() ? c->vocab[w] : 0;
		(v >= WORD_KNOWN ? known : unknown).push_back((int)w);
		if (v >= 40 && v < WORD_KNOWN)
			half.push_back((int)w);
	}
	std::string detail;
	for (int tries = 0; tries < 20 && detail.empty(); tries++) {
		int what = (int)rand_size_t(0, COUNT - 1);
		switch (what) {
			case KNOCKOUT:
				c->setUnconscious(true);
				// (its life story's line for that says lightning, not energy)
				if (auto& story = world.history->getMoniker(mon).story; !story.empty() && story.back().kind == storyEntry::unconscious)
					story.back().detail = "lightning";
				if (watching())
					fmt::print("* LIGHTNING {}: knocked out\n", mon);
				return;
			case FORGET: {
				if (known.empty())
					break;
				std::sort(known.begin(), known.end(), [&](int a, int b) { return c->vocab[a] < c->vocab[b]; });
				int n = (int)rand_size_t(1, 3);
				std::string lost;
				for (int i = 0; i < n && i < (int)known.size(); i++) {
					c->vocab[known[i]] = (unsigned char)(WORD_KNOWN - 40);
					lost += (lost.empty() ? "\"" : ", \"") + wordAt(known[i]) + "\"";
				}
				detail = "forgot " + lost;
				break;
			}
			case KNOW: {
				if (unknown.empty())
					break;
				int w = unknown[rand_size_t(0, unknown.size() - 1)];
				if (c->vocab.size() <= (size_t)w)
					c->vocab.resize(engine.wordlist.size(), 0);
				c->vocab[w] = (unsigned char)std::min(255, WORD_KNOWN + 30);
				detail = "suddenly knew the word \"" + wordAt(w) + "\"";
				break;
			}
			case TEMPERAMENT: {
				int t = (int)rand_size_t(0, NUM_TRAITS - 1);
				float d = rand_float(0.6f, 1.0f) * (rand_float(0, 1) < 0.5f ? -1.0f : 1.0f);
				c->temperamentDrift.shift[EXPERIENCE_HURT][t] += d;
				c->refreshTemperament();
				detail = fmt::format("came out of it {}", d > 0 ? Temperaments::highWord(t) : Temperaments::lowWord(t));
				break;
			}
			case TIMESLIP: {
				int stage = (int)c->getStage();
				int to = stage >= 6 ? 5 : stage == 0 ? 1 : (rand_float(0, 1) < 0.8f ? stage + 1 : stage - 1);
				WorldSave::RequestLifeStage(agent, to);
				detail = to > stage ? "was jolted older" : "was jolted younger";
				break;
			}
			case MUTAGEN: {
				int n = (int)rand_size_t(2, 4);
				s_germline[mon] += n;
				if (agent->babymoniker != 0)
					Exposure::addUnbornMutations(genomeNameFromInt(agent->babymoniker), n);
				detail = "will pass strange changes on to its children";
				break;
			}
			case RAVENOUS:
				addChem(c, HUNGER, 150);
				detail = "was suddenly ravenous";
				break;
			case THROWN:
				agent->vely = -18.0f;
				agent->velx = rand_float(0, 1) < 0.5f ? -6.0f : 6.0f;
				addChem(c, PAIN, 30);
				detail = "was thrown into the air";
				break;
			case HEAL:
				if (!Disease::isIll(mon))
					break;
				Disease::cure(mon);
				detail = "was shocked well again";
				break;
			case FEVER:
				if (!Disease::catchSome(agent.get()))
					break;
				detail = "came down with a fever";
				break;
			case OVERDUE:
				if (agent->babymoniker == 0 || c->getPregnant() == 0)
					break;
				c->setPregnant(1);
				detail = "was ready to lay her egg at once";
				break;
			case LOVESTRUCK: {
				addChem(c, SEX, 200);
				// smitten with the nearest grown one of its kind and the other sex
				Agent* best = nullptr;
				oldCreature* bestc = nullptr;
				float bestd = 400.0f;
				for (auto& b : world.agents) {
					oldCreature* d = b && b.get() != agent.get() && !b->isDying() && b->family == 4 && b->genus == agent->genus ? creatureOf(b.get()) : nullptr;
					if (!d || !d->isAlive() || d->isFemale() == c->isFemale() || (int)d->getStage() < 2)
						continue;
					float dist = std::hypot(cx(b.get()) - cx(agent.get()), cy(b.get()) - cy(agent.get()));
					if (dist < bestd)
						bestd = dist, best = b.get(), bestc = d;
				}
				if (bestc) {
					std::string other = monikerOf(bestc);
					Bond& bond = c->bonds[other];
					bond.affinity = std::min(1.0f, bond.affinity + 0.6f);
					bond.lastContact = world.worldtickcount;
					world.history->tell(mon, storyEntry::struck, other, "fell for");
					if (watching())
						fmt::print("* LIGHTNING {}: lovestruck by {}\n", mon, other);
					return;
				}
				detail = "was lovestruck";
				break;
			}
			case INSIGHT: {
				if (half.empty())
					break;
				for (int w : half)
					c->vocab[w] = (unsigned char)(WORD_KNOWN + 10);
				detail = fmt::format("suddenly understood {} words it had half learned", half.size());
				break;
			}
			default: {
				int t = what - TIMED0;
				if (t < 0 || t >= NUM_TIMED)
					break;
				if (t == FROZEN && s.frankenstein)
					break;
				s.until[t] = now + DURATION[t];
				switch (t) {
					case SCORCH: detail = "was frizzled and scorched"; break;
					case GLOW: detail = "glowed in the dark"; break;
					case CHARGE: detail = "was left charged with static"; break;
					case ELECTRO: detail = "took a liking to machines"; break;
					case FAST: detail = "ran like the wind"; break;
					case SLOW: detail = "was left dazed and slow"; break;
					case WIRED: detail = "was wide awake"; break;
					case SIZE:
						s.size = rand_float(0, 1) < 0.5f ? 0.75f : 1.3f;
						detail = s.size > 1.0f ? "grew" : "shrank";
						break;
					case FROZEN:
						s.lifeHold = c->getChemical(LIFE);
						detail = "stopped growing older";
						break;
					case CONTRARY:
						endContrary(c, s);
						for (auto& [other, b] : c->bonds) {
							s.contraryWas[other] = b.affinity;
							b.affinity = -b.affinity;
						}
						detail = "turned contrary";
						break;
					case FEARLESS: detail = "lost all fear"; break;
					case SKYFEAR: detail = "came to dread storms"; break;
					case BACKWARDS: detail = "began to talk backwards"; break;
					case PITCH:
						s.pitch = rand_float(0, 1) < 0.5f ? 1.35f : 0.75f;
						detail = s.pitch > 1.0f ? "squeaked" : "croaked";
						break;
					case PARROT: detail = "repeated everything it heard"; break;
					case ROD: detail = "drew the storms to it"; break;
					case MAGNETIC: detail = "drew things to it"; break;
					case DANCE:
						addChem(c, DANCING, 150);
						detail = "danced";
						break;
				}
				break;
			}
		}
	}
	if (detail.empty())
		return;
	tell(mon, detail);
	refreshLook(agent.get(), s);
	if (watching())
		fmt::print("* LIGHTNING {}: {}\n", mon, detail);
}

void givePower(std::shared_ptr<Agent> agent, oldCreature* c, const std::string& mon, Struck& s) {
	if (s.power >= 0 && s.powerUntil > world.tickcount)
		return; // (one at a time)
	s.power = (int)rand_size_t(0, NUM_POWERS - 1);
	s.powerUntil = world.tickcount + POWER_TICKS;
	tell(mon, "came away with a gift");
	if (watching())
		fmt::print("* LIGHTNING {}: power {}\n", mon, POWER_NAME[s.power]);
}

void strikeCreature(std::shared_ptr<Agent> agent, oldCreature* c) {
	std::string mon = monikerOf(c);
	if (mon.empty())
		return;
	Struck& s = s_struck[mon];
	unsigned int now = world.tickcount;
	if (s.lastStruck && now - s.lastStruck < STRUCK_AGAIN)
		return;
	s.lastStruck = now;
	if (watching())
		fmt::print("* LIGHTNING struck {} ({}) at {:.0f},{:.0f}\n", mon, c->isAlive() ? "alive" : "dead", agent->x, agent->y);
	if (!c->isAlive()) {
		// just died, and not of old age: it may rise again, changed
		// (and the other sex from the last that rose: owner, 2026-11-26)
		auto* c2 = dynamic_cast<c2Creature*>(c);
		int sex = c->isFemale() ? 2 : 1;
		if (strange() && c2 && (c2->kickstartState() == c2Creature::stillDead || c2->kickstartState() == c2Creature::primed) && sex != s_lastRisenSex &&
			rand_float(0, 1) < FRANKENSTEIN)
			riseAsFrankenstein(agent, c2, mon);
		return;
	}
	// Frankenstein's creature struck again: the end of it
	if (s.frankenstein) {
		tell(mon, "");
		s.frankenstein = false;
		s.lifeHold = -1;
		c->dieOf("lightning");
		fmt::print("* {} (Frankenstein's creature) was struck again, and died\n", mon);
		return;
	}
	if (!strange()) {
		tell(mon, "");
		return;
	}
	if (rand_float(0, 1) < POWER && !(s.power >= 0 && s.powerUntil > now)) {
		givePower(agent, c, mon, s);
		return;
	}
	if ((s.lastSurprise && now - s.lastSurprise < SURPRISE_AGAIN) || rand_float(0, 1) >= SURPRISE) {
		tell(mon, "");
		return;
	}
	s.lastSurprise = now;
	surprise(agent, c, mon, s);
}

void strikeThing(Agent* a) {
	unsigned int now = world.tickcount;
	auto it = s_thingStruck.find(a);
	if (it != s_thingStruck.end() && now - it->second < STRUCK_AGAIN)
		return;
	s_thingStruck[a] = now;
	if (a->genus == 5) { // an egg: its unborn changed
		std::string baby = genomeNameFromInt(a->var[0].getInt());
		if (watching())
			fmt::print("* LIGHTNING struck an egg ({}) at {:.0f},{:.0f}\n", baby, a->x, a->y);
		if (strange() && !baby.empty())
			Exposure::addUnbornMutations(baby, (int)rand_size_t(2, 4));
		return;
	}
	// a plant: it grows on, and fruits (its own timer, run on); at most once
	// in five minutes (one by the desert's top was struck again and again)
	static std::map<Agent*, unsigned int> fruited;
	if (auto f = fruited.find(a); f != fruited.end() && now - f->second < 3000)
		return;
	fruited[a] = now;
	if (watching())
		fmt::print("* LIGHTNING struck a plant ({} {} {}) at {:.0f},{:.0f}\n", (int)a->family, (int)a->genus, a->species, a->x, a->y);
	if (strange())
		for (int i = 0; i < 6; i++)
			a->queueScript(9);
}

void strikeAround(Agent* bolt) {
	const float pad = 4.0f;
	float x0 = bolt->x - pad, x1 = bolt->x + bolt->getWidth() + pad, y0 = bolt->y - pad, y1 = bolt->y + bolt->getHeight() + pad;
	for (auto& a : world.agents) {
		if (!a || a->isDying() || a.get() == bolt || !overlaps(a.get(), x0, y0, x1, y1))
			continue;
		if (a->family == 4) {
			if (oldCreature* c = creatureOf(a.get()))
				strikeCreature(a, c);
		} else if (a->family == 2 && a->genus == 5 && a->var[0].hasInt() && a->var[0].getInt() != 0) {
			auto* sp = dynamic_cast<SpritePart*>(a->part(0));
			if (!sp || sp->getPose() != 7) // (7: an empty shell)
				strikeThing(a.get());
		} else if (a->family == 2 && (a->category == 4 || a->category == 24)) {
			strikeThing(a.get());
		}
	}
}

// a short jig: arms up and a kick each way, turning about
void danceStep(Agent* a, unsigned int step) {
	auto* sc = dynamic_cast<SkeletalCreature*>(a);
	if (!sc)
		return;
	static const char* frames[4] = {"211111211" "3300XX", "211211111" "0033XX", "311111211" "3300XX", "311211111" "0033XX"};
	sc->setPose(frames[step % 4]);
}

} // namespace

bool strange() {
	if (s_strange < 0) {
		if (const char* e = devenv("OPENC2E_STRANGE_LIGHTNING"))
			s_strange = atoi(e) ? 1 : 0;
		else
			s_strange = readPlayerSetting("strange-lightning") == "0" ? 0 : 1;
	}
	return s_strange == 1;
}

void setStrange(bool on) {
	s_strange = on ? 1 : 0;
	writePlayerSetting("strange-lightning", on ? "1" : "0");
}

void reset() {
	s_struck.clear();
	s_germline.clear();
	s_seenBolts.clear();
	s_thingStruck.clear();
	s_shocked.clear();
	s_photoWanted.clear();
	s_lastRisenSex = -1;
	s_couples.clear();
}

void tick() {
	if (engine.version != 2 || !world.map || !world.history)
		return;
	// developer aid: OPENC2E_TEST_LIGHTNING=WHAT at tick 30, for the selected
	// creature (or the first norn): "scorch", "glow" or "plain" (looks, the
	// view following it), "surprise" (a strange effect), "power", or
	// "frankenstein"; OPENC2E_TEST_LIGHTNING_LOOK is the old name
	const char* test = devenv("OPENC2E_TEST_LIGHTNING");
	if (!test)
		test = devenv("OPENC2E_TEST_LIGHTNING_LOOK");
	if (test && world.tickcount == 30) {
		std::shared_ptr<Agent> pick = world.selectedcreature.lock();
		if (!pick)
			for (auto& a : world.agents)
				if (a && a->family == 4 && a->genus == 1 && creatureOf(a.get()) && creatureOf(a.get())->isAlive()) {
					pick = a;
					break;
				}
		if (oldCreature* c = pick ? creatureOf(pick.get()) : nullptr) {
			std::string mon = monikerOf(c), t = test;
			Struck& s = s_struck[mon];
			if (t == "glow")
				s.until[GLOW] = world.tickcount + 100000;
			else if (t == "scorch")
				s.until[SCORCH] = world.tickcount + 100000;
			else if (t == "surprise")
				surprise(pick, c, mon, s);
			else if (t == "power")
				givePower(pick, c, mon, s);
			else if (t == "frankenstein") {
				// (made one in its own body: a test of the rest; or, dead, risen)
				if (auto* c2 = dynamic_cast<c2Creature*>(c); c2 && !c->isAlive())
					riseAsFrankenstein(pick, c2, mon);
				else
					markFrankenstein(pick, c, mon, "");
			}
			refreshLook(pick.get(), s);
			world.selectCreature(pick);
			engine.camera->trackAgent(pick, 0, 0, flexible, none);
			fmt::print("* TEST LIGHTNING {} on {} at {:.0f},{:.0f}\n", t, mon, pick->x, pick->y);
		}
	}
	// (OPENC2E_TEST_LIGHTNING=rise: the first adult norn dies at tick 30 and
	// rises as Frankenstein's creature at tick 40)
	static AgentRef testRise;
	if (test && std::string(test) == "rise") {
		if (world.tickcount == 30)
			for (auto& a : world.agents)
				if (oldCreature* c = a && a->family == 4 && a->genus == 1 ? creatureOf(a.get()) : nullptr; c && c->isAlive() && (int)c->getStage() >= 3) {
					testRise = AgentRef(a);
					fmt::print("* TEST LIGHTNING rise: {} ({}) dies\n", monikerOf(c), world.history->getMoniker(monikerOf(c)).name);
					c->dieOf("a test");
					break;
				}
		if (world.tickcount == 40 && testRise)
			if (auto* c2 = dynamic_cast<c2Creature*>(creatureOf(testRise.get())); c2 && !c2->isAlive())
				riseAsFrankenstein(testRise.lock(), c2, monikerOf(c2));
	}
	// (OPENC2E_TEST_LIGHTNING=posephoto with OPENC2E_TEST_POSE="15 pose
	// letters": the first grown norn made Frankenstein's creature at tick 30,
	// held in that pose, and photographed at tick 150)
	static AgentRef posed;
	if (test && std::string(test) == "posephoto") {
		if (world.tickcount == 30)
			for (auto& a : world.agents)
				if (oldCreature* c = a && a->family == 4 && a->genus == 1 ? creatureOf(a.get()) : nullptr; c && c->isAlive() && (int)c->getStage() == 4 && !c->isAsleep()) {
					posed = AgentRef(a);
					markFrankenstein(a, c, monikerOf(c), "");
					s_struck[monikerOf(c)].photoWanted = false;
					break;
				}
		if (posed && world.tickcount > 30) {
			posed->velx = 0;
			posed->vely = 0;
			if (const char* pose = devenv("OPENC2E_TEST_POSE"))
				if (auto* sc = dynamic_cast<SkeletalCreature*>(posed.get()))
					sc->setPose(pose);
			if (world.tickcount == 150)
				s_photoWanted.push_back(posed);
		}
	}
	// (OPENC2E_TEST_LIGHTNING=couple: a grown male and female norn made
	// Frankenstein's creatures at tick 30 and matched; the male dies at tick 60)
	static AgentRef coupleHim;
	if (test && std::string(test) == "couple") {
		if (world.tickcount == 30) {
			std::shared_ptr<Agent> him, her;
			for (auto& a : world.agents)
				if (oldCreature* c = a && a->family == 4 && a->genus == 1 ? creatureOf(a.get()) : nullptr; c && c->isAlive() && (int)c->getStage() == 4) {
					if (!c->isFemale() && !him)
						him = a;
					else if (c->isFemale() && !her)
						her = a;
				}
			if (him && her) {
				markFrankenstein(him, creatureOf(him.get()), monikerOf(creatureOf(him.get())), "");
				markFrankenstein(her, creatureOf(her.get()), monikerOf(creatureOf(her.get())), "");
				bool matched = brideFound(monikerOf(creatureOf(him.get())), monikerOf(creatureOf(her.get())));
				coupleHim = AgentRef(him);
				fmt::print("* TEST LIGHTNING couple: {} and {} matched {}\n", monikerOf(creatureOf(him.get())), monikerOf(creatureOf(her.get())), matched);
			}
		}
		if (world.tickcount == 60 && coupleHim)
			if (oldCreature* c = creatureOf(coupleHim.get()); c && c->isAlive())
				c->dieOf("a test");
	}
	// each new bolt segment, once: what it touches is struck
	for (auto& a : world.agents) {
		if (!a || a->isDying() || a->family != 2 || a->genus != 14 || a->species != 4)
			continue;
		bool seen = false;
		for (auto& b : s_seenBolts)
			if (b.get() == a.get()) {
				seen = true;
				break;
			}
		if (seen)
			continue;
		s_seenBolts.push_back(AgentRef(a));
		strikeAround(a.get());
	}
	unsigned int now = world.tickcount;
	if (now % 200 == 0) {
		s_seenBolts.erase(std::remove_if(s_seenBolts.begin(), s_seenBolts.end(), [](const AgentRef& r) { return !r || r->isDying(); }), s_seenBolts.end());
		for (auto it = s_thingStruck.begin(); it != s_thingStruck.end();)
			it = now - it->second > 1000 ? s_thingStruck.erase(it) : std::next(it);
	}
	if (s_struck.empty())
		return;
	// the living with something going on
	struct Live {
		std::shared_ptr<Agent> a;
		oldCreature* c;
		Struck* s;
	};
	std::vector<Live> active;
	std::vector<std::pair<Agent*, oldCreature*>> all;
	bool every10 = now % 10 == 0;
	for (auto& a : world.agents) {
		oldCreature* c = a && !a->isDying() && a->family == 4 ? creatureOf(a.get()) : nullptr;
		if (!c || !c->isAlive())
			continue;
		if (every10)
			all.push_back({a.get(), c});
		auto it = s_struck.find(monikerOf(c));
		if (it != s_struck.end())
			active.push_back({a, c, &it->second});
	}
	for (auto& [a, c, s] : active) {
		if (s->posingUntil) {
			if (auto* sc = dynamic_cast<SkeletalCreature*>(a.get())) {
				sc->setPose(std::string(sc->facing() == 3 ? "3" : "2") + "31111111222211");
				sc->setFacialExpression(4);
				sc->setEyesClosed(false);
			}
			a->velx = 0;
			if (now >= s->posingUntil) {
				s->posingUntil = 0;
				s_photoWanted.push_back(AgentRef(a));
			}
		}
		// the jig, every few ticks
		if (on(s, DANCE) && now % 5 == 0)
			danceStep(a.get(), now / 5);
		if (!every10)
			continue;
		if (now % 50 == 0)
			refreshLook(a.get(), *s);
		if (on(s, WIRED)) {
			setChem(c, TIRED, 0);
			setChem(c, SLEEPY, 0);
		}
		if (on(s, FEARLESS))
			setChem(c, FEAR, 0);
		if ((on(s, FROZEN) || s->frankenstein) && s->lifeHold >= 0)
			setChem(c, LIFE, s->lifeHold);
		if (!on(s, CONTRARY) && !s->contraryWas.empty())
			endContrary(c, *s);
		if (hasPower(s, WEATHERPROOF)) {
			setChem(c, COLD, 0);
			setChem(c, HOT, 0);
		}
		if (hasPower(s, LUCKY)) {
			int h = c->getChemical(HUNGER);
			if (s->lastHunger >= 0 && h < s->lastHunger - 8)
				setChem(c, HUNGER, h - (s->lastHunger - h) / 2); // what it ate goes further
			s->lastHunger = c->getChemical(HUNGER);
		}
		// featherweight: a third of its gravity while it lasts, then back
		if (hasPower(s, FEATHER) && !s->featherOn) {
			s->accgWas = a->accg;
			a->accg = a->accg * 0.3f;
			s->featherOn = true;
		} else if (!hasPower(s, FEATHER) && s->featherOn) {
			a->accg = s->accgWas >= 0 ? s->accgWas : a->accg / 0.3f;
			s->accgWas = -1;
			s->featherOn = false;
		}
		if (s->power >= 0 && s->powerUntil <= now)
			s->power = -1;
		// Frankenstein's creature: grown, its life held from then on, and its
		// photo taken once it has come round
		if (s->frankenstein && (int)c->getStage() == 4) {
			if (s->lifeHold < 0)
				s->lifeHold = std::max<int>(c->getChemical(LIFE), 128);
			// for its photo it holds the pose two seconds (its own animation
			// would take a once-set pose back before the shutter): reaching up
			// for the storm that made it, head back, arms out (pose B), wide-eyed
			// with surprise (face 4); then the photo
			if (s->photoWanted && !c->isUnconscious()) {
				s->photoWanted = false;
				s->posingUntil = now + 20;
			}
		}
		// Frankenstein's creature: lonely, hardy, tough, afraid of fire
		if (s->frankenstein) {
			addChem(c, LONELY, 2);
			setChem(c, COLD, 0);
			setChem(c, PAIN, c->getChemical(PAIN) / 2);
			if (c->getChemical(HOT) > 64)
				addChem(c, FEAR, 15);
		}
		// storms: dreaded, drawn, or felt coming
		if (on(s, SKYFEAR) || on(s, ROD) || hasPower(s, STORMSENSE))
			for (auto& b : world.agents) {
				if (!b || b->isDying() || b->family != 2 || b->genus != 14 || b->species != 2)
					continue;
				float dx = cx(b.get()) - cx(a.get()), dy = cy(b.get()) - cy(a.get());
				if (on(s, SKYFEAR) && std::fabs(dx) < 600 && std::fabs(dy) < 700)
					addChem(c, FEAR, 25);
				if (on(s, ROD) && std::fabs(dx) < 1500 && std::fabs(dx) > 20)
					b->moveTo(b->x + (dx < 0 ? 3.0f : -3.0f), b->y);
				if (hasPower(s, STORMSENSE) && std::fabs(dx) < 700 && std::fabs(dy) < 800 && now - s->warnedAt > 900) {
					s->warnedAt = now;
					say(a.get(), c, "name:15"); // "weather!"
				}
			}
		// things near it, every so often
		bool magnetic = on(s, MAGNETIC);
		bool spark = hasPower(s, SPARK) && now % 150 == 0, green = hasPower(s, GREEN) && now % 300 == 0, beast = hasPower(s, BEAST);
		if (magnetic || spark || green || beast)
			for (auto& b : world.agents) {
				if (!b || b->isDying() || b->family != 2 || b->carriedby || b->invehicle)
					continue;
				float dx = cx(a.get()) - cx(b.get()), dy = cy(a.get()) - cy(b.get());
				float d = std::hypot(dx, dy);
				if (magnetic && d < 250 && d > 20 && (b->attr & 64) && (b->attr & 128) && b->getWidth() < 60)
					b->velx = std::clamp(b->velx + (dx < 0 ? -1.5f : 1.5f), -6.0f, 6.0f);
				if (spark && d < 80 && electric(b->category)) {
					b->queueScript(1, a.get());
					spark = false;
				}
				if (green && d < 200 && (b->category == 4 || b->category == 24))
					b->queueScript(9);
				if (beast && d < 150 && b->category >= 17 && b->category <= 20)
					b->velx = 0, b->vely = 0;
			}
		// others near it: static shocks, healing, following, fearing
		bool charged = on(s, CHARGE), healer = hasPower(s, HEALER) && now % 100 == 0, leader = hasPower(s, LEADER) && now % 100 == 0,
			 feared = s->frankenstein && now % 200 == 0;
		if (charged || healer || leader || feared) {
			std::string mon = monikerOf(c);
			for (auto& [b, d] : all) {
				if (b == a.get())
					continue;
				float dx = cx(b) - cx(a.get()), dy = cy(b) - cy(a.get());
				std::string other = monikerOf(d);
				if (charged && std::fabs(dx) < STATIC_REACH && std::fabs(dy) < STATIC_REACH) {
					unsigned int& last = s_shocked[other];
					if (!last || now - last >= SHOCK_AGAIN) {
						last = now;
						addChem(d, PAIN, 60);
						if (watching())
							fmt::print("* LIGHTNING static: {} shocked {}\n", mon, other);
					}
				}
				if (healer && std::fabs(dx) < 60 && std::fabs(dy) < 60 && Disease::isIll(other))
					Disease::cure(other);
				if (leader && std::fabs(dx) < 300 && std::fabs(dy) < 200) {
					Bond& bond = d->bonds[mon];
					bond.affinity = std::min(1.0f, bond.affinity + 0.01f);
				}
				// others start out a little afraid of it; one of its own kind
				// is drawn to it instead (owner, 2026-11-26)
				if (feared && std::fabs(dx) < 200 && std::fabs(dy) < 150) {
					Bond& bond = d->bonds[mon];
					if (infertile(other)) {
						bond.fear = 0;
						bond.affinity = std::min(0.8f, bond.affinity + 0.04f);
					} else if (bond.affinity < 0.3f)
						bond.fear = std::min(0.3f, bond.fear + 0.03f);
				}
			}
		}
	}
	// the two that found each other die together, and go on the memorial
	// stone beneath the incubator (owner, 2026-11-26)
	if (every10)
		for (auto& [mon, s] : s_struck) {
			if (s.match.empty())
				continue;
			auto it = s_struck.find(s.match);
			monikerData& m = world.history->getMoniker(mon);
			if (!m.died)
				continue;
			std::string other = s.match;
			s.match.clear();
			if (it != s_struck.end())
				it->second.match.clear();
			monikerData& o = world.history->getMoniker(other);
			if (!o.died)
				if (oldCreature* c = creatureOf(o.owner.get()); c && c->isAlive())
					c->dieOf("a broken heart");
			auto nameOf = [](monikerData& r, const std::string& mon) { return r.name.empty() ? mon : r.name; };
			s_couples.push_back({nameOf(m, mon) + " and " + nameOf(o, other), world.worldtickcount});
			fmt::print("* {} and {} (Frankenstein's creatures, matched) died together; on the memorial stone\n", mon, other);
			break; // (s_struck is not changed, but one pair a step is plenty)
		}
	// forgotten once nothing is left
	if (now % 500 == 0)
		for (auto it = s_struck.begin(); it != s_struck.end();) {
			const Struck& s = it->second;
			bool any = s.frankenstein || !s.match.empty() || (s.power >= 0 && s.powerUntil > now) || s.featherOn || s.shownLook != NONE || s.shownSize != 1.0f || !s.contraryWas.empty() ||
					   (s.lastSurprise && now - s.lastSurprise < SURPRISE_AGAIN) || now - s.lastStruck < 1000;
			for (int t = 0; t < NUM_TIMED; t++)
				any = any || s.until[t] > now;
			it = any ? std::next(it) : s_struck.erase(it);
		}
}

Look lookOf(Agent* creature) {
	const Struck* s = find(creature);
	return s ? lookNow(*s) : NONE;
}

float sizeFactor(Agent* creature) {
	const Struck* s = find(creature);
	return on(s, SIZE) ? s->size : 1.0f;
}

float paceFactor(Agent* creature) {
	const Struck* s = find(creature);
	return on(s, FAST) ? 2.0f : on(s, SLOW) ? 0.5f : 1.0f;
}

float pitchFactor(Agent* creature) {
	const Struck* s = find(creature);
	return on(s, PITCH) ? s->pitch : 1.0f;
}

bool backwards(Agent* creature) {
	return on(find(creature), BACKWARDS);
}

bool parrotNow(Agent* creature) {
	Struck* s = find(creature);
	if (!on(s, PARROT) || world.tickcount - s->parrotAt < 40)
		return false;
	s->parrotAt = world.tickcount;
	return true;
}

float learnFactor(oldCreature* listener) {
	if (s_struck.empty() || !listener)
		return 1.0f;
	auto it = s_struck.find(monikerOf(listener));
	if (it == s_struck.end())
		return 1.0f;
	return (hasPower(&it->second, POLYGLOT) || it->second.frankenstein) ? 3.0f : 1.0f;
}

float teachFactor(Agent* speaker) {
	return hasPower(find(speaker), POLYGLOT) ? 2.0f : 1.0f;
}

bool electrophile(Agent* creature) {
	return on(find(creature), ELECTRO);
}

bool waterBreather(Agent* creature) {
	return hasPower(find(creature), WATER);
}

bool foodFinder(Agent* creature) {
	return hasPower(find(creature), FOODFINDER);
}

bool plantEater(Agent* creature) {
	const Struck* s = find(creature);
	return s && s->frankenstein;
}

bool immortal(Agent* creature) {
	const Struck* s = find(creature);
	return s && s->frankenstein;
}

bool kindredDirection(Agent* creature, float& dx, float& dy) {
	const Struck* s = find(creature);
	if (!s || !s->frankenstein)
		return false;
	float best = 1e9f;
	for (auto& a : world.agents) {
		if (!a || a.get() == creature || a->isDying() || a->family != 4)
			continue;
		oldCreature* c = creatureOf(a.get());
		if (!c || !c->isAlive() || !infertile(monikerOf(c)))
			continue;
		float x = cx(a.get()) - cx(creature), y = cy(a.get()) - cy(creature);
		float d = std::hypot(x, y);
		if (d < best)
			best = d, dx = x, dy = y;
	}
	return best < 1e9f && best > 150.0f;
}

bool infertile(const std::string& moniker) {
	auto it = s_struck.find(moniker);
	return it != s_struck.end() && it->second.frankenstein;
}

bool brideFound(const std::string& a, const std::string& b) {
	if (!infertile(a) || !infertile(b))
		return false;
	for (auto& [mon, other] : {std::pair<std::string, std::string>{a, b}, {b, a}}) {
		Struck& s = s_struck[mon];
		s.frankenstein = false;
		s.lifeHold = -1;
		s.match = other;
		world.history->tell(mon, storyEntry::mortalAgain, other);
	}
	fmt::print("* {} and {} (Frankenstein's creatures) found each other, and became mortal again\n", a, b);
	return true;
}

void slapped(Agent* creature) {
	const Struck* s = find(creature);
	if (s && s->frankenstein)
		if (oldCreature* c = creatureOf(creature))
			addChem(c, ANGER, 80); // gentle, until spurned
}

std::vector<std::string> statusOf(Agent* creature) {
	std::vector<std::string> out;
	const Struck* s = find(creature);
	if (s) {
		if (s->frankenstein)
			out.push_back("Frankenstein's creature");
		if (s->power >= 0 && s->powerUntil > world.tickcount)
			out.push_back(POWER_NAME[s->power]);
		static const char* const NAME[NUM_TIMED] = {"Scorched", "Glowing", "Charged", "Machine-mad", "Quick-footed", "Dazed", "Wide awake", "", "Ageless",
			"Contrary", "Fearless", "Storm-shy", "Backwards", "", "Parrot", "Lightning rod", "Magnetic", "Dancing"};
		for (int t = 0; t < NUM_TIMED; t++) {
			if (!on(s, t))
				continue;
			if (t == SIZE)
				out.push_back(s->size > 1.0f ? "Giant" : "Tiny");
			else if (t == PITCH)
				out.push_back(s->pitch > 1.0f ? "Squeaky" : "Croaky");
			else
				out.push_back(NAME[t]);
		}
	}
	if (oldCreature* c = creatureOf(creature))
		if (auto it = s_germline.find(monikerOf(c)); it != s_germline.end() && it->second > 0)
			out.push_back("Strange blood");
	return out;
}

// (player-visible text: British spelling; how long each lasts follows
// DURATION and POWER_TICKS, at ten ticks a second)
std::string describe(const std::string& name) {
	static const std::map<std::string, std::string> TEXT = {
		{"Frankenstein's creature", "Brought back to life by lightning. It cannot die of age or failing organs and cannot have young; it eats only plants, is lonely, and seeks others of its kind. A mate of its own kind breaks the spell for both; another strike ends it for good."},
		{"Healer", "Ill creatures close to it are cured. (About an hour.)"},
		{"Green thumb", "Plants near it grow and fruit faster. (About an hour.)"},
		{"Polyglot", "Learns words three times as fast, and others learn words from it twice as fast. (About an hour.)"},
		{"Water-breather", "Can breathe under water. (About an hour.)"},
		{"Weatherwise", "Senses a storm coming and says so. (About an hour.)"},
		{"Featherweight", "Falls slowly, as light as a feather. (About an hour.)"},
		{"Beast-friend", "Small animals near it, such as bees and butterflies, stop and stay. (About an hour.)"},
		{"Spark-hands", "Machines near it now and then switch on by themselves. (About an hour.)"},
		{"Leader", "Others near it grow fond of it. (About an hour.)"},
		{"Food-finder", "Can smell the nearest food, wherever it is. (About an hour.)"},
		{"Weatherproof", "Feels neither cold nor heat. (About an hour.)"},
		{"Lucky", "Its meals go half as far again. (About an hour.)"},
		{"Scorched", "Frizzled and sooty from the strike; only its look. (About five minutes.)"},
		{"Glowing", "Glows pale and bright; only its look. (About ten minutes.)"},
		{"Charged", "Full of static: creatures that come close get small shocks. (About a minute.)"},
		{"Machine-mad", "Drawn to machines and anything electric. (About ten minutes.)"},
		{"Quick-footed", "Walks twice as fast. (About a minute.)"},
		{"Dazed", "Walks at half its pace. (About two minutes.)"},
		{"Wide awake", "Feels no tiredness or sleepiness. (About five minutes.)"},
		{"Giant", "A third bigger than it was. (About ten minutes.)"},
		{"Tiny", "Shrunk to three quarters of its size. (About ten minutes.)"},
		{"Ageless", "Stops ageing for a while. (About an hour.)"},
		{"Contrary", "Its feelings for others are turned round: friends seem foes, and foes friends. (About five minutes.)"},
		{"Fearless", "Feels no fear. (About five minutes.)"},
		{"Storm-shy", "Afraid of storm clouds. (About an hour.)"},
		{"Backwards", "Its words come out backwards. (About three minutes.)"},
		{"Squeaky", "Its voice is higher. (About ten minutes.)"},
		{"Croaky", "Its voice is deeper. (About ten minutes.)"},
		{"Parrot", "Says back what it hears. (About three minutes.)"},
		{"Lightning rod", "Storm clouds drift towards it. (About five minutes.)"},
		{"Magnetic", "Small things near it drift towards it. (About a minute.)"},
		{"Dancing", "Breaks into a jig. (A few seconds.)"},
		{"Strange blood", "Its next baby will be born with a few new mutations."},
	};
	auto it = TEXT.find(name);
	return it == TEXT.end() ? std::string() : it->second;
}

std::vector<std::pair<std::string, unsigned int>> couplesOnStone() {
	std::vector<std::pair<std::string, unsigned int>> out;
	for (auto& c : s_couples)
		out.push_back({c.names, c.tick});
	return out;
}

std::shared_ptr<Agent> takePhotoRequest() {
	while (!s_photoWanted.empty()) {
		AgentRef r = s_photoWanted.back();
		s_photoWanted.pop_back();
		if (r && !r->isDying())
			return r.lock();
	}
	return nullptr;
}

int takeGermline(const std::string& moniker) {
	auto it = s_germline.find(moniker);
	if (it == s_germline.end())
		return 0;
	int n = it->second;
	s_germline.erase(it);
	return n;
}

// Saved: owed mutations, Frankenstein's creatures, powers and effects still
// running (as ticks left). Not: who was struck when (a load starts afresh).
void save(std::ostream& out) {
	unsigned int now = world.tickcount;
	if (s_lastRisenSex > 0)
		out << "lightning-last-risen " << s_lastRisenSex << "\n";
	for (auto& c : s_couples)
		out << "lightning-couple " << c.tick << " " << c.names << "\n";
	for (auto& [mon, n] : s_germline)
		if (n > 0)
			out << "lightning-germline " << mon << " " << n << "\n";
	for (auto& [mon, s] : s_struck) {
		if (s.frankenstein)
			out << "lightning-frankenstein " << mon << " " << s.lifeHold << "\n";
		if (!s.match.empty())
			out << "lightning-match " << mon << " " << s.match << "\n";
		if (s.power >= 0 && s.powerUntil > now)
			out << "lightning-power " << mon << " " << s.power << " " << (s.powerUntil - now) << "\n";
		for (int t = 0; t < NUM_TIMED; t++)
			if (s.until[t] > now)
				out << "lightning-effect " << mon << " " << TIMED_KEY[t] << " " << (s.until[t] - now) << " "
					<< (t == SIZE ? s.size : t == PITCH ? s.pitch : t == FROZEN ? (float)s.lifeHold : 0.0f) << "\n";
	}
}

bool readLine(const std::string& key, std::istringstream& line) {
	std::string mon;
	if (key == "lightning-last-risen") {
		line >> s_lastRisenSex;
		return true;
	}
	if (key == "lightning-couple") {
		Couple c;
		if (line >> c.tick) {
			std::getline(line >> std::ws, c.names);
			s_couples.push_back(c);
		}
		return true;
	}
	if (key == "lightning-match") {
		std::string other;
		if (line >> mon >> other)
			s_struck[mon].match = other;
		return true;
	}
	if (key == "lightning-germline") {
		int n = 0;
		if (line >> mon >> n && n > 0)
			s_germline[mon] = n;
		return true;
	}
	if (key == "lightning-frankenstein") {
		int hold = -1;
		if (line >> mon) {
			line >> hold;
			Struck& s = s_struck[mon];
			s.frankenstein = true;
			s.lifeHold = hold; // (-1: not grown yet when saved)
		}
		return true;
	}
	if (key == "lightning-power") {
		int p = -1;
		unsigned int left = 0;
		if (line >> mon >> p >> left && p >= 0 && p < NUM_POWERS) {
			Struck& s = s_struck[mon];
			s.power = p;
			s.powerUntil = world.tickcount + left;
		}
		return true;
	}
	if (key == "lightning-effect") {
		std::string what;
		unsigned int left = 0;
		float v = 0;
		if (line >> mon >> what >> left) {
			line >> v;
			for (int t = 0; t < NUM_TIMED; t++)
				if (what == TIMED_KEY[t]) {
					Struck& s = s_struck[mon];
					s.until[t] = world.tickcount + left;
					if (t == SIZE)
						s.size = v > 0 ? v : 1.0f;
					else if (t == PITCH)
						s.pitch = v > 0 ? v : 1.0f;
					else if (t == FROZEN)
						s.lifeHold = (int)v;
				}
		}
		return true;
	}
	return false;
}

} // namespace Lightning
