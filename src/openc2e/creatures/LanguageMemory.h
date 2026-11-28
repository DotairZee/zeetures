#pragma once

// What a creature remembers of each word, for the experimental language
// system (Language.h; Preferences > Creatures > Language). The word's
// strength stays in oldCreature::vocab, as before; this is everything else.
// Unused while the system is off.

#include <map>
#include <utility>
#include <vector>

struct WordMemory {
	bool set = false; // filled in (from vocab, the first time it is needed)
	float stability = 0; // how slowly it fades: spaced hearings and sleep raise it
	unsigned int lastHeard = 0; // world tick (World::worldtickcount)
	unsigned char best = 0; // the strongest it has been (relearning is quicker)
	// the game's names of things: which kind of thing it takes the word to
	// mean (-1 not yet), and how sure it is, 0-255
	signed char guess = -1;
	unsigned char sure = 0;
	// the things it has heard it for: the first (by UNID), and how many
	// different ones (up to 3): heard for one thing only, it means only that
	unsigned int firstThing = 0;
	unsigned char things = 0;
	// and the types of thing (Language::typeOf): the first, and how many
	// (up to 3). A name of the game's (a kind, "food") heard for one type
	// only means that type ("food": the cheese) until heard for another, which
	// a baby does not do: from childhood it broadens to the kind (Rosch et al.
	// 1976: the young learn the names of things before the names of kinds).
	// 0: not known (saves before 2026-11-27; the rule above then not used)
	unsigned int firstType = 0;
	unsigned char types = 0;
	unsigned char uses = 0; // times it has said the word itself (up to 255)
	unsigned char alone = 0; // times heard on its own (not run into a neighbour)
	bool sinceSleep = false; // heard since it last slept
	float fadeCarry = 0; // part of a point of strength still to fade
};

struct LanguageMemory {
	// how often it has spoken of each type of thing while looking at it
	// (Language::typeOf; to make up a name for one it has no name for)
	std::map<unsigned int, int> talkedOf;
	std::vector<WordMemory> words; // by word-list number, as vocab
	// pairs of words heard one after the other (the young run them together)
	std::map<std::pair<int, int>, int> pairs;
	unsigned int lastFade = 0; // world tick of the last fading pass
	bool wasAsleep = false;
	unsigned int sleptAt = 0; // world tick it fell asleep (a nap settles nothing)
};
