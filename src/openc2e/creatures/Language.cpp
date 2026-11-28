#include "Language.h"

#include "Agent.h"
#include "Engine.h"
#include "World.h"
#include "historyManager.h"
#include "common/Random.h"
#include "common/devenv.h"
#include "creatures/CreatureAgent.h"
#include "creatures/CreatureSpeech.h"
#include "creatures/oldCreature.h"

#include <algorithm>
#include <cmath>
#include <fmt/core.h>

namespace Language {

namespace {

int s_experimental = -1, s_fading = -1; // -1: not read yet

bool watching() {
	static const bool watch = devenv("OPENC2E_WATCH_SPEECH") != nullptr;
	return watch;
}

std::string listWord(int w) {
	if (w < 0 || (size_t)w >= engine.wordlist.size())
		return {};
	std::string s = engine.wordlist[w];
	size_t a = s.find_first_not_of(' '), b = s.find_last_not_of(' ');
	return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

bool isNoun(int w) {
	return w >= 16 && w < 56;
}

unsigned int now() {
	return world.worldtickcount;
}

int stageOf(oldCreature* c) {
	return c ? (int)c->getStage() : 4;
}

oldCreature* creatureOf(Agent* a) {
	auto* ca = dynamic_cast<CreatureAgent*>(a);
	return ca ? dynamic_cast<oldCreature*>(ca->getCreature()) : nullptr;
}

// the kind of thing an agent is, as the names of things go (C2's categories
// 0-39), or -1
int kindOf(Agent* a) {
	if (!a || a->isDying() || a->category < 0 || a->category > 39)
		return -1;
	return a->category;
}

constexpr int WORD_STRENGTH_KNOWN = 128; // (CreatureSpeech.h's WORD_KNOWN)
constexpr unsigned int MASSED = 150; // heard again sooner than this (15 s): little more learned
constexpr unsigned int SPACED_LONG = 6000; // ten minutes or more: settles it more

} // namespace

bool experimental() {
	if (s_experimental < 0) {
		// developer aid: OPENC2E_LANGUAGE=1 (or 0) whatever the setting
		if (const char* e = devenv("OPENC2E_LANGUAGE"))
			s_experimental = atoi(e) ? 1 : 0;
		else
			s_experimental = readPlayerSetting("language-experimental") == "1" ? 1 : 0;
	}
	return s_experimental == 1;
}

void setExperimental(bool on) {
	s_experimental = on ? 1 : 0;
	writePlayerSetting("language-experimental", on ? "1" : "0");
}

bool fading() {
	if (s_fading < 0)
		s_fading = readPlayerSetting("language-fading") == "0" ? 0 : 1;
	return s_fading == 1;
}

void setFading(bool on) {
	s_fading = on ? 1 : 0;
	writePlayerSetting("language-fading", on ? "1" : "0");
}

// ---- rules ----

int phraseLimit(int stage, int wordsKnown) {
	// (Brown 1973: utterances lengthen as language grows, not by age alone;
	// age is a ceiling here)
	static const int byStage[7] = {1, 2, 3, 4, 4, 4, 3};
	int cap = stage >= 0 && stage < 7 ? byStage[stage] : 4;
	int byWords = wordsKnown < 12 ? 1 : wordsKnown < 25 ? 2 : wordsKnown < 40 ? 3 : 4;
	return std::max(1, std::min(cap, byWords));
}

bool qualifiersAllowed(int stage, int wordsKnown) {
	return stage >= 2 && wordsKnown >= 30;
}

float fadePerMinute(float stability) {
	// heard once (stability 1): about 2 points a minute, so a word just
	// known is half forgotten in about half an hour; each step of stability
	// slows it (Ebbinghaus; Vlach, Sandhofer & Kornell 2008: spacing)
	return 2.0f / std::max(1.0f, stability);
}

bool settled(float stability) {
	return stability >= 8.0f; // (Bahrick 1984: what survives the first years is kept)
}

int likeKind(int kind) {
	switch (kind) {
		case 3: // nature
		case 4: // plant
		case 15: // badplant
		case 22: // leaf
		case 23: // root
		case 24: // flower
			return 0;
		case 6: // food
		case 7: // drink
		case 21: // seed
		case 25: // fruit
			return 1;
		case 17: // badbug
		case 18: // bug
		case 19: // badcritter
		case 20: // critter
			return 2;
		case 2: // button
		case 9: // implement
		case 11: // detritus
		case 12: // medicine
		case 13: // toy
			return 3;
		case 8: // dispensor
		case 26: // mover
		case 27: // lift
		case 28: // computer
		case 29: // mediabox
		case 32: // incubator
		case 33: // teleporter
		case 35: // machine
			return 4;
		case 36: // Norn
		case 37: // Grendel
		case 38: // Ettin
		case 39: // Shee
			return 5;
		case 5: // egg
		case 16: // nest
			return 6;
		default:
			return -1;
	}
}

float weightFor(const Weighing& w) {
	float k = 1.0f;
	const bool baby = w.listenerStage <= 0, adolescent = w.listenerStage == 2 || w.listenerStage == 3;
	switch (w.source) {
		case FROM_HAND:
			// (Shneidman et al. 2013; Weisleder & Fernald 2013: speech to the
			// child teaches; overheard speech, a little: Akhtar 2005)
			k = w.directed ? 1.0f : 0.5f;
			if (baby)
				k *= 1.5f; // (Kuhl, Tsao & Liu 2003: live speech tunes infants)
			break;
		case FROM_CREATURE:
			k = w.directed ? 1.5f : 0.6f;
			if (w.kin)
				k *= adolescent ? 1.0f : baby ? 2.5f : 2.0f; // family most, until adolescence
			else if (adolescent && std::abs(w.speakerStage - w.listenerStage) <= 1)
				k *= 1.5f; // (Kerswill & Williams 2000: peers shape the young)
			if (w.speakerStage >= 5)
				k *= 1.3f; // the old: rich vocabularies (Verhaeghen 2003)
			break;
		case FROM_MACHINE:
		case FROM_MACHINE_FAR:
			k = w.source == FROM_MACHINE ? 1.0f : 0.5f;
			// (Kuhl, Tsao & Liu 2003: recordings taught infants nothing):
			// machines are for the older
			k *= baby ? 0.3f : w.listenerStage >= 2 ? 1.25f : 1.0f;
			break;
		case FROM_SELF:
			k = 1.0f;
			break;
	}
	return k;
}

void updateGuess(int& guess, int& sure, int kindInView) {
	// propose, then verify (Trueswell, Medina, Hafri & Gleitman 2013): one
	// guess kept; seen again with it, surer; with something else, less sure,
	// and switched when no longer sure at all
	if (kindInView < 0)
		return;
	if (guess < 0) {
		guess = kindInView;
		sure = 70;
	} else if (guess == kindInView)
		sure = std::min(255, sure + 45);
	else {
		sure -= 60;
		if (sure <= 0) {
			guess = kindInView;
			sure = 50;
		}
	}
}

// ---- the creature's side ----

// its temperament's effect on a kind of learning (Temperament.h,
// LearningChannel; 1 for an average creature)
static float byTemperament(oldCreature* c, int channel) {
	return c ? Temperaments::learningFactor(c->temperament, channel) : 1.0f;
}

// how much a hearing (or saying) after a while settles a word: the steady
// more, and a word it already knows more for the incurious
static float settling(oldCreature* c, const ::WordMemory& m) {
	float f = byTemperament(c, Temperaments::LEARN_SETTLING);
	if (m.best >= WORD_STRENGTH_KNOWN)
		f *= byTemperament(c, Temperaments::LEARN_SETTLING_FAMILIAR);
	return f;
}

::WordMemory& memoryOf(oldCreature* c, int word) {
	auto& words = c->lang.words;
	if (words.size() <= (size_t)word)
		words.resize(std::max((size_t)word + 1, engine.wordlist.size()));
	::WordMemory& m = words[word];
	if (!m.set) {
		// filled in from what it knew before the system was on: a word known
		// already keeps its meaning and is kept for life (a colony turning the
		// system on loses nothing); one part learned starts afresh
		m.set = true;
		int v = (size_t)word < c->vocab.size() ? c->vocab[word] : 0;
		m.best = (unsigned char)v;
		if (v > 0) {
			m.lastHeard = now();
			m.stability = v >= WORD_STRENGTH_KNOWN ? 8.0f : 1.0f;
			if (isNoun(word)) {
				m.guess = (signed char)(word - 16);
				m.sure = (unsigned char)std::min(255, v);
				m.things = 3;
			}
		}
	}
	return m;
}

int heard(Agent* listener, oldCreature* c, int word, int amount, const Hearing& h) {
	if (!c || word < 0)
		return amount;
	::WordMemory& m = memoryOf(c, word);
	const int stage = stageOf(c);
	oldCreature* sc = creatureOf(h.speaker);
	Weighing wt;
	wt.source = h.source;
	wt.listenerStage = stage;
	wt.speakerStage = sc ? stageOf(sc) : 4;
	wt.directed = h.directed;
	wt.kin = h.kin;
	float k = weightFor(wt);
	// its temperament: whom it learns from best (the sociable from other
	// creatures, the aloof from machines; the bold from strangers, the shy
	// from family), and a word still new to it (the curious)
	if (h.source == FROM_CREATURE)
		k *= byTemperament(c, Temperaments::LEARN_WORDS_CREATURES) *
			 byTemperament(c, h.kin ? Temperaments::LEARN_FROM_FAMILY : Temperaments::LEARN_FROM_STRANGERS);
	else if (h.source == FROM_MACHINE || h.source == FROM_MACHINE_FAR)
		k *= byTemperament(c, Temperaments::LEARN_WORDS_MACHINES);
	if ((int)m.best < WORD_STRENGTH_KNOWN)
		k *= byTemperament(c, Temperaments::LEARN_WORDS_NEW);

	// grounding: what this word is heard about
	const char* how = "";
	if (isNoun(word) && word != 16) { // ("me": whoever speaks)
		Agent* focus = c->getAttentionFocus().get();
		if (focus == listener)
			focus = nullptr;
		Agent* thing = nullptr;
		if (h.source == FROM_MACHINE || h.source == FROM_MACHINE_FAR) {
			// a teaching machine shows the thing with the word
			int guess = m.guess, sure = m.sure;
			updateGuess(guess, sure, word - 16);
			m.guess = (signed char)guess, m.sure = (unsigned char)sure;
			how = " (shown by a machine)";
		} else {
			// babies take a word for what they are looking at; from childhood
			// they follow what the speaker shows (Baldwin 1993; Tomasello &
			// Farrar 1986: naming what it already looks at teaches most)
			Agent* about = h.about && h.about != listener ? h.about : nullptr;
			// another creature names what it is looking at only when it means
			// that kind by the word; else ("me hungry", "yes food") it is
			// talking of something not in view, and teaches the sound only
			if (about && h.source == FROM_CREATURE)
				if (oldCreature* sc = creatureOf(h.speaker); sc && meaningOf(sc, word) != kindOf(about))
					about = nullptr;
			const bool aboutNothing = h.source == FROM_CREATURE && !about;
			if (aboutNothing)
				thing = nullptr;
			else if (stage <= 0 || !about)
				thing = focus;
			else
				thing = about;
			if (!thing && h.source == FROM_HAND && stage >= 1)
				thing = h.speaker; // the hand itself, in view as it speaks
			int kind = kindOf(thing);
			if (kind < 0) {
				k *= 0.3f; // heard about nothing in view: its sound, not its sense
				how = " (nothing in view)";
			} else {
				k *= thing == focus ? 2.5f : 1.5f;
				how = thing == focus ? " (what it looks at)" : " (what is shown)";
				// mutual exclusivity (Markman & Wachtel 1988; Lewis et al. 2020):
				// a new word for a thing it already has a word for is taken to
				// be for something else, more so the more words it knows
				bool named = false;
				if (m.guess < 0)
					for (size_t w = 16; w < 56 && w < c->lang.words.size(); w++)
						if ((int)w != word && c->lang.words[w].set && c->lang.words[w].guess == kind && c->lang.words[w].sure >= 128 && knowsWord(c, (int)w))
							named = true;
				if (named && rand_float(0, 1) < std::min(0.85f, 0.25f + wordsKnown(c) / 60.0f)) {
					k *= 0.5f;
					how = " (it has a word for that already)";
				} else {
					int guess = m.guess, sure = m.sure;
					updateGuess(guess, sure, kind);
					m.guess = (signed char)guess, m.sure = (unsigned char)sure;
					// the things heard for: one only, and it means only that
					unsigned int unid = thing->getUNID();
					if (m.things == 0)
						m.firstThing = unid, m.things = 1;
					else if (unid != m.firstThing && m.things < 3)
						m.things++;
				}
			}
		}
	} else if (word >= 0 && word <= 13 && word != 8) {
		// a verb: heard while doing it (the energetic learn by doing)
		k *= (c->getDecisionId() == word ? 2.0f : 0.7f) * byTemperament(c, Temperaments::LEARN_WORDS_VERBS);
	} else if (word >= 56 && word <= 72) {
		// a feeling: heard while feeling it
		k *= c->getDrive((unsigned int)(word - 56)) > 140 ? 2.0f : 0.6f;
	}

	// spacing (Vlach, Sandhofer & Kornell 2008; Childers & Tomasello 2002)
	const unsigned int t = now();
	const unsigned int since = m.lastHeard && t >= m.lastHeard ? t - m.lastHeard : 0;
	if (m.lastHeard == 0 || (int)m.best == 0)
		m.stability = std::max(m.stability, 1.0f);
	else if (since < MASSED)
		k *= std::min(1.0f, 0.35f * byTemperament(c, Temperaments::LEARN_WORDS_BURSTS)); // again at once: hardly more (a little more for the placid)
	else
		m.stability = std::min(10.0f, m.stability + (since >= SPACED_LONG ? 1.0f : 0.5f) * settling(c, m));
	// once known, quickly known again (savings: Bowers et al. 2009)
	int v = (size_t)word < c->vocab.size() ? c->vocab[word] : 0;
	if (m.best >= WORD_STRENGTH_KNOWN && v < WORD_STRENGTH_KNOWN)
		k *= 2.5f;
	m.lastHeard = std::max(1u, t);
	m.sinceSleep = true;
	// a verb heard (or said) in use comes to join other words (verb islands)
	if (((word >= 0 && word <= 13) || word == 75) && m.uses < 255)
		m.uses++;
	int got = std::max(0, (int)std::lround(amount * k));
	m.best = (unsigned char)std::min(255, std::max((int)m.best, v + got));
	if (watching() && got > 0)
		fmt::print("* HEARS {} \"{}\" x{:.2f}{}{}\n", c->getGenome() ? world.history->findMoniker(c->getGenome()) : std::string(), listWord(word), k, how,
			isNoun(word) && word != 16 && m.guess >= 0 ? fmt::format(", takes it for {} ({})", listWord(16 + m.guess), (int)m.sure) : std::string());
	return got;
}

void said(oldCreature* c, int word) {
	if (!c || word < 0 || (size_t)word >= engine.wordlist.size())
		return;
	::WordMemory& m = memoryOf(c, word);
	const unsigned int t = now();
	const unsigned int since = m.lastHeard && t >= m.lastHeard ? t - m.lastHeard : 0;
	// saying it is the best practice (retrieval: Karpicke & Roediger 2008)
	if (since >= MASSED)
		m.stability = std::min(10.0f, m.stability + 0.3f * settling(c, m));
	if (c->vocab.size() > (size_t)word)
		c->vocab[word] = (unsigned char)std::min(255, c->vocab[word] + 6);
	m.lastHeard = std::max(1u, t);
	m.sinceSleep = true;
	if (m.uses < 255)
		m.uses++;
}

void tick(Agent* creature, oldCreature* c) {
	if (!experimental() || !c || !c->isAlive())
		return;
	LanguageMemory& L = c->lang;
	// sleep settles what was heard that day (Williams & Horst 2014)
	bool asleep = c->isAsleep();
	const unsigned int t0 = now();
	if (asleep && !L.wasAsleep)
		L.sleptAt = t0;
	if (L.wasAsleep && !asleep && L.sleptAt && t0 >= L.sleptAt && t0 - L.sleptAt >= 600) {
		int n = 0;
		// (the placid settle more in their sleep: Temperament.h,
		// LEARN_SLEEP_SETTLING)
		const float slept = byTemperament(c, Temperaments::LEARN_SLEEP_SETTLING);
		for (auto& m : L.words)
			if (m.set && m.sinceSleep) {
				m.stability = std::min(10.0f, m.stability + slept);
				m.sinceSleep = false;
				n++;
			}
		if (watching() && n)
			fmt::print("* SLEPT ON {} words: {}\n", n, world.history->findMoniker(c->getGenome()));
	}
	L.wasAsleep = asleep;
	if (!fading())
		return;
	// fading, once a minute (each creature at its own moment)
	const unsigned int t = now();
	if (L.lastFade == 0 || t < L.lastFade) {
		L.lastFade = t - (creature ? creature->getUNID() % 600 : 0);
		return;
	}
	if (t - L.lastFade < 600)
		return;
	L.lastFade = t;
	for (size_t w = 0; w < c->vocab.size(); w++) {
		if (c->vocab[w] == 0)
			continue;
		::WordMemory& m = memoryOf(c, (int)w);
		if (settled(m.stability) || (m.lastHeard && t - m.lastHeard < 1200))
			continue;
		float loss = fadePerMinute(m.stability) + m.fadeCarry;
		int whole = (int)loss;
		m.fadeCarry = loss - whole;
		if (whole <= 0)
			continue;
		bool knew = c->vocab[w] >= WORD_STRENGTH_KNOWN;
		c->vocab[w] = (unsigned char)std::max(0, (int)c->vocab[w] - whole);
		if (knew && c->vocab[w] < WORD_STRENGTH_KNOWN && watching())
			fmt::print("* FORGETS \"{}\": {}\n", listWord((int)w), world.history->findMoniker(c->getGenome()));
	}
}

bool understands(oldCreature* c, int word) {
	if (!experimental())
		return knowsWord(c, word);
	// (Fenson et al. 1994: words understood run ahead of words said)
	return c && word >= 0 && (size_t)word < c->vocab.size() && c->vocab[word] >= WORD_STRENGTH_KNOWN / 2;
}

int meaningOf(oldCreature* c, int word) {
	if (!isNoun(word))
		return -1;
	if (!experimental())
		return word - 16;
	if (word == 16)
		return 0; // "me"
	::WordMemory& m = memoryOf(c, word);
	return m.guess;
}

bool tooNarrow(oldCreature* c, int word, Agent* thing) {
	if (!experimental() || !isNoun(word) || !thing)
		return false;
	::WordMemory& m = memoryOf(c, word);
	// (Rescorla 1980; underextension: a word heard for one thing is that thing's)
	return m.things == 1 && m.firstThing != 0 && thing->getUNID() != m.firstThing;
}

Naming nameFor(Agent* creature, oldCreature* c, int kind, bool mayCoin) {
	Naming n;
	if (!c || kind < 0 || kind > 39)
		return n;
	if (kind >= 1 && kind <= 35)
		if (std::string own = ownNameFor(c, kind); !own.empty()) {
			n.own = own;
			return n;
		}
	// the game's word it takes to mean this kind (the surest), among the
	// words it is ready to say (CreatureSpeech.h, canSay)
	int best = -1, sure = -1;
	for (int w = 16; w < 56; w++) {
		if (!canSay(c, w))
			continue;
		int means = meaningOf(c, w);
		::WordMemory& m = memoryOf(c, w);
		if (means == kind && (int)m.sure > sure)
			best = w, sure = m.sure;
	}
	if (best >= 0) {
		n.word = best;
		n.unsure = sure < 120;
		return n;
	}
	const int stage = stageOf(c);
	// the young borrow the word for a like kind (Clark 1973; Rescorla 1980:
	// about a third of early words are stretched)
	if (stage >= 1 && stage <= 2 && likeKind(kind) >= 0 && rand_float(0, 1) < 0.33f)
		for (int w = 16; w < 56; w++) {
			int means = knowsWord(c, w) ? meaningOf(c, w) : -1;
			if (means >= 0 && means != kind && likeKind(means) == likeKind(kind)) {
				n.word = w;
				n.borrowed = true;
				if (watching())
					fmt::print("* STRETCHES \"{}\" to {}: {}\n", listWord(w), listWord(16 + kind), world.history->findMoniker(c->getGenome()));
				return n;
			}
		}
	// a name for it it has heard but not yet learned: it tries that (the
	// naming game: a speaker uses its best name, Steels 1995)
	if (kind >= 1 && kind <= 35) {
		std::string best;
		int most = 23;
		for (auto& [w, for_] : c->ownNames)
			if (auto it = for_.find(kind); it != for_.end() && it->second > most)
				most = it->second, best = w;
		if (!best.empty()) {
			n.own = best;
			n.unsure = true;
			return n;
		}
	}
	// no word for it at all: make one up in its own tongue (Steels 2003;
	// Baronchelli et al. 2006: a population settles on invented names)
	if (mayCoin && stage >= 2 && kind >= 2 && kind <= 35 && c->ownNames.size() < 40 && rand_float(0, 1) < 0.15f) {
		// two sounds of its own voice, from two words of the list
		auto sound = [&]() {
			int w = (int)rand_size_t(0, engine.wordlist.size() ? engine.wordlist.size() - 1 : 0);
			std::string s = nativeWord(creature, listWord(w).empty() ? "ba" : listWord(w));
			size_t dash = s.find('-');
			return dash == std::string::npos ? s : s.substr(0, dash);
		};
		std::string word = sound() + sound();
		bool taken = word.size() < 3 || ownNameMeans(c, word) >= 0;
		for (size_t w = 0; w < engine.wordlist.size() && !taken; w++)
			taken = listWord((int)w) == word;
		if (!taken) {
			c->ownNames[word][kind] = 150;
			n.own = word;
			n.coined = true;
			if (watching())
				fmt::print("* COINS \"{}\" for {}: {}\n", word, listWord(16 + kind), world.history->findMoniker(c->getGenome()));
		}
	}
	return n;
}

void agreed(oldCreature* c, const std::string& word, int kind) {
	// the naming game's rule (Baronchelli et al. 2006): understood, both drop
	// their other names for the thing
	if (!c)
		return;
	for (auto& [w, for_] : c->ownNames)
		if (w != word)
			if (auto it = for_.find(kind); it != for_.end())
				it->second = std::max(0, it->second - (int)std::lround(80 * byTemperament(c, Temperaments::LEARN_COLONY_WORDS))); // (the sociable let go sooner)
}

void heardInOrder(oldCreature* c, const std::vector<int>& words) {
	if (!c || stageOf(c) > 1 || words.empty())
		return;
	auto& pairs = c->lang.pairs;
	if (words.size() == 1) {
		::WordMemory& m = memoryOf(c, words[0]);
		if (m.alone < 255)
			m.alone++;
		return;
	}
	for (size_t i = 0; i + 1 < words.size(); i++) {
		if (words[i] == words[i + 1])
			continue;
		pairs[{words[i], words[i + 1]}]++;
		// heard apart from each other elsewhere: not one word
	}
	if (pairs.size() > 24) {
		auto least = pairs.begin();
		for (auto it = pairs.begin(); it != pairs.end(); ++it)
			if (it->second < least->second)
				least = it;
		pairs.erase(least);
	}
}

std::string runTogether(oldCreature* c, int word, int& partner) {
	partner = -1;
	if (!c || stageOf(c) != 1)
		return {};
	// (Saffran, Aslin & Newport 1996: words are found as sounds that keep
	// together; the young run often-heard neighbours into one until heard apart)
	for (auto& [p, n] : c->lang.pairs)
		if (p.first == word && n >= 3 && knowsWord(c, p.second) && n > 2 * memoryOf(c, word).alone && n > 2 * memoryOf(c, p.second).alone) {
			partner = p.second;
			return listWord(word) + listWord(p.second);
		}
	return {};
}

std::string babble(Agent* creature, oldCreature* c) {
	// how much it has been spoken to (Oller et al. 1999; Cychosz et al. 2021:
	// canonical babbling comes in the first year; late babbling is a warning)
	int heardSum = 0;
	for (auto v : c->vocab)
		heardSum += v;
	if (heardSum < 60 && rand_float(0, 1) < 0.5f)
		return "mm";
	// a syllable of its tongue, doubled ("ba-ba"); later, two different
	auto syllable = [&]() {
		int w = (int)rand_size_t(0, engine.wordlist.size() ? engine.wordlist.size() - 1 : 0);
		std::string s = nativeWord(creature, listWord(w).empty() ? "ba" : listWord(w));
		size_t dash = s.find('-');
		return dash == std::string::npos ? s : s.substr(0, dash);
	};
	std::string a = syllable();
	if (heardSum < 400)
		return a + "-" + a;
	return a + "-" + syllable();
}

bool searchingForWord(oldCreature* c) {
	// (Burke et al. 1991: the old keep their words but more often reach for one)
	int stage = stageOf(c);
	return (stage == 5 && rand_float(0, 1) < 0.04f) || (stage >= 6 && rand_float(0, 1) < 0.1f);
}

} // namespace Language
