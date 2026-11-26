#pragma once

// The experimental language system (owner, 2026-11-25: "build it all", as an
// option in Preferences, marked experimental; off, language is as it was).
// Drawn from research on how children and groups learn to talk (the
// project's reports/Language acquisition for Zeetures.md; CITATIONS.md):
//
// - Grounding: a name of a thing is taken to mean the kind of thing in view
//   when it is heard (what the listener is looking at, or, from childhood,
//   what the speaker shows), and later hearings confirm or change the guess.
//   Naming what it already looks at teaches most. Unsure, it says "toy?".
// - Talk grows with words known: one word, then two, three, four; the
//   strength words last; each verb joins other words once it has been used
//   alone a few times. It understands a word before it can say it.
// - Who teaches whom: speech aimed at it teaches more than overheard talk;
//   babies learn little from machines and much from the hand and family;
//   adolescents take words from their peers; the old pause for words.
// - Memory: words fade unless heard again after a while; spaced hearings and
//   sleep settle them, and a settled word is kept for life; a forgotten word
//   comes back quickly. Repeating a word over and over teaches little more.
// - A colony's own words: with no word for a thing, a creature may make one
//   up in its own tongue; agreeing on a word, two creatures drop their other
//   words for that thing, so a colony settles on one, and places apart drift
//   into their own; the young keep to the commonest word.
// - Mistakes: a young creature calls a thing by the word for a similar kind
//   it knows; a word heard for one thing only means just that thing.
// - Growing up out loud: babies babble ("ba-ba"), more when spoken to; the
//   young run words heard together into one ("eatfood") until heard apart.

#include "LanguageMemory.h"

#include <functional>
#include <string>
#include <vector>

class Agent;
class oldCreature;

namespace Language {

// Preferences > Creatures > Language: the system on ("language-experimental"
// 1 in settings.txt), and whether words fade ("language-fading", on unless 0)
bool experimental();
void setExperimental(bool on);
bool fading();
void setFading(bool on);

// ---- rules (no world needed; tested in LanguageTest.cpp) ----
// the most words in a phrase, by life stage (0 baby ... 6 senile) and words known
int phraseLimit(int stage, int wordsKnown);
// whether the strength words (very ... extremely) are used yet
bool qualifiersAllowed(int stage, int wordsKnown);
// points of strength (0-255) a word loses a minute, by its stability
float fadePerMinute(float stability);
// a word this settled is kept for life
bool settled(float stability);
// kinds of thing alike enough for the young to confuse (C2's categories): a
// group number, or -1
int likeKind(int kind);
// how much a hearing teaches, before the life stage's rate: the source's
// weight for this listener (see weightFor)
enum Source { FROM_HAND, FROM_CREATURE, FROM_MACHINE, FROM_MACHINE_FAR, FROM_SELF };
struct Weighing {
	Source source = FROM_CREATURE;
	int listenerStage = 4, speakerStage = 4;
	bool directed = false; // spoken to it (named, nearest, or looked at)
	bool kin = false; // the speaker is close kin
};
float weightFor(const Weighing& w);
// a hearing's effect on the guessed meaning: the kind in view (-1 none);
// returns the new guess and sureness
void updateGuess(int& guess, int& sure, int kindInView);

// ---- the creature's side ----
struct Hearing {
	Agent* speaker = nullptr;
	Source source = FROM_CREATURE;
	Agent* about = nullptr; // what the speaker shows or looks at
	bool directed = false;
	bool kin = false;
};
// a word heard (hearWord, when on): its strength goes up by what this
// returns, and its memory (guess, stability) is updated
int heard(Agent* listener, oldCreature* c, int word, int amount, const Hearing& h);
// it said the word itself: a little stronger, and more settled
void said(oldCreature* c, int word);
// fading and sleep's settling (each mind tick; cheap: once a minute at most)
void tick(Agent* creature, oldCreature* c);
// the word's memory, filled in from vocab the first time
::WordMemory& memoryOf(oldCreature* c, int word);
// understands the word (half way to knowing it; off: knows it)
bool understands(oldCreature* c, int word);
// the kind of thing it takes a name of the game's (words 16-55) to mean, -1
// if it does not know; (off: the word's own kind)
int meaningOf(oldCreature* c, int word);
// heard for one thing only, and that is not the thing meant
bool tooNarrow(oldCreature* c, int word, Agent* thing);
// what it calls a kind of thing: a word of the game's, or a name of its
// own (made up now, if it may), and whether unsure or borrowed
struct Naming {
	int word = -1;
	std::string own;
	bool unsure = false, borrowed = false, coined = false;
};
Naming nameFor(Agent* creature, oldCreature* c, int kind, bool mayCoin);
// two creatures agreed on a name: its other names for the kind fade
void agreed(oldCreature* c, const std::string& word, int kind);
// the young: words heard in order (to run neighbours together)
void heardInOrder(oldCreature* c, const std::vector<int>& words);
// the word run together with its usual neighbour ("eatfood"), and which
// word that is; empty when not
std::string runTogether(oldCreature* c, int word, int& partner);
// a baby's babble, by how much it has been spoken to
std::string babble(Agent* creature, oldCreature* c);
// the old: a pause before a word, now and then
bool searchingForWord(oldCreature* c);

} // namespace Language
