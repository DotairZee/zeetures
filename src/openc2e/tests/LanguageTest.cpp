// The experimental language system's rules (creatures/Language.h): phrase
// length by words known and age, fading and settling, alike kinds, who
// teaches how much, and how a guessed meaning is kept or changed.

#include "openc2e/creatures/Language.h"

#include <gtest/gtest.h>

using namespace Language;

TEST(Language, PhrasesGrowWithWordsKnown) {
	// an adult's phrases lengthen with its words
	EXPECT_EQ(phraseLimit(4, 0), 1);
	EXPECT_EQ(phraseLimit(4, 11), 1);
	EXPECT_EQ(phraseLimit(4, 12), 2);
	EXPECT_EQ(phraseLimit(4, 25), 3);
	EXPECT_EQ(phraseLimit(4, 40), 4);
	// age is a ceiling: babies 1, children 2, adolescents 3, the senile 3
	EXPECT_EQ(phraseLimit(0, 80), 1);
	EXPECT_EQ(phraseLimit(1, 80), 2);
	EXPECT_EQ(phraseLimit(2, 80), 3);
	EXPECT_EQ(phraseLimit(6, 80), 3);
	// and never less than one word
	EXPECT_EQ(phraseLimit(-1, 0), 1);
}

TEST(Language, StrengthWordsComeLast) {
	EXPECT_FALSE(qualifiersAllowed(1, 80)); // children: not yet
	EXPECT_FALSE(qualifiersAllowed(4, 29));
	EXPECT_TRUE(qualifiersAllowed(4, 30));
}

TEST(Language, SettledWordsFadeSlowerAndAreKept) {
	EXPECT_FLOAT_EQ(fadePerMinute(1), 2.0f);
	EXPECT_FLOAT_EQ(fadePerMinute(0), 2.0f); // (never faster than a word heard once)
	EXPECT_LT(fadePerMinute(4), fadePerMinute(2));
	EXPECT_FALSE(settled(7.9f));
	EXPECT_TRUE(settled(8));
}

TEST(Language, AlikeKinds) {
	EXPECT_EQ(likeKind(6), likeKind(25)); // food, fruit
	EXPECT_EQ(likeKind(18), likeKind(20)); // bug, critter
	EXPECT_NE(likeKind(13), likeKind(6)); // toy, food
	EXPECT_EQ(likeKind(0), -1); // "me": like nothing
	EXPECT_EQ(likeKind(1), -1); // the hand
}

TEST(Language, WhoTeachesHowMuch) {
	Weighing w;
	w.source = FROM_HAND;
	w.listenerStage = 4;
	w.directed = true;
	float spokenTo = weightFor(w);
	w.directed = false;
	EXPECT_LT(weightFor(w), spokenTo); // overheard: less
	// babies: the hand much, machines little
	w.listenerStage = 0;
	w.directed = true;
	EXPECT_GT(weightFor(w), spokenTo);
	w.source = FROM_MACHINE;
	EXPECT_LT(weightFor(w), 0.5f);
	// older creatures: machines more than before
	w.listenerStage = 3;
	EXPECT_GT(weightFor(w), 1.0f);
	// adolescents: peers over family
	Weighing peer;
	peer.source = FROM_CREATURE;
	peer.listenerStage = 2;
	peer.speakerStage = 2;
	Weighing family = peer;
	family.kin = true;
	family.speakerStage = 4;
	EXPECT_GT(weightFor(peer), weightFor(family));
	// a child: family over others
	peer.listenerStage = family.listenerStage = 1;
	EXPECT_GT(weightFor(family), weightFor(peer));
}

TEST(Language, GuessesAreKeptOrChanged) {
	int guess = -1, sure = 0;
	updateGuess(guess, sure, -1); // nothing in view: no guess
	EXPECT_EQ(guess, -1);
	updateGuess(guess, sure, 13); // heard with a toy: a toy
	EXPECT_EQ(guess, 13);
	int first = sure;
	updateGuess(guess, sure, 13); // again with a toy: surer
	EXPECT_GT(sure, first);
	// with something else, less sure, then changed
	updateGuess(guess, sure, 4);
	EXPECT_EQ(guess, 13);
	updateGuess(guess, sure, 4);
	updateGuess(guess, sure, 4);
	EXPECT_EQ(guess, 4);
	// sureness has a ceiling
	for (int i = 0; i < 20; i++)
		updateGuess(guess, sure, 4);
	EXPECT_EQ(sure, 255);
}

// Names for particular things (owner, 2026-11-27: "model 2"; Rosch et al.
// 1976, Markman 1990)

TEST(Language, TypesAreTheirClassifier) {
	EXPECT_EQ(typeKey(2, 25, 16), (2u << 24) | (25u << 16) | 16u); // the mushroom
	EXPECT_NE(typeKey(2, 25, 16), typeKey(2, 25, 17));
	EXPECT_NE(typeKey(2, 6, 16), typeKey(2, 25, 16));
}

TEST(Language, TheYoungLearnNamesOfThingsFastest) {
	// a baby and a child learn a thing's name faster than an adult, and a
	// baby faster than it learns anything else
	EXPECT_GT(itemNamePace(0), itemNamePace(4));
	EXPECT_GT(itemNamePace(1), itemNamePace(0));
	EXPECT_GT(itemNamePace(1), itemNamePace(4));
	EXPECT_GT(itemNamePace(4), itemNamePace(6));
	EXPECT_FLOAT_EQ(itemNamePace(-3), itemNamePace(0));
}

TEST(Language, KindWordsBroadenFromChildhood) {
	EXPECT_FALSE(broadensAt(0)); // a baby keeps "food" as the cheese's name
	EXPECT_TRUE(broadensAt(1));
	EXPECT_TRUE(broadensAt(5));
}

TEST(Language, OneNameForAThingOneThingForAName) {
	ItemNames names;
	const unsigned int cheese = typeKey(2, 6, 2), mushroom = typeKey(2, 25, 16);
	strengthenItemName(names, "cheese", cheese, 100, 100, 40);
	EXPECT_EQ(itemNameIn(names, "cheese", 128), 0u); // not known yet
	strengthenItemName(names, "cheese", cheese, 100, 100, 40);
	EXPECT_EQ(itemNameIn(names, "cheese", 128), cheese);
	// the same word heard for the mushroom: its hold on the cheese weakens
	strengthenItemName(names, "cheese", mushroom, 60, 60, 40);
	EXPECT_EQ(names["cheese"][cheese], 140);
	EXPECT_EQ(itemNameIn(names, "cheese", 128), cheese);
	// another name for the cheese: "cheese" fades as its name
	strengthenItemName(names, "fromage", cheese, 150, 150, 40);
	EXPECT_EQ(itemNameIn(names, "cheese", 128), 0u);
	EXPECT_EQ(itemNameIn(names, "fromage", 128), cheese);
}

TEST(Language, ItemNamesForgetTheLeastFamiliarPastTheMost) {
	ItemNames names;
	strengthenItemName(names, "a", typeKey(2, 6, 1), 200, 0, 2);
	strengthenItemName(names, "b", typeKey(2, 6, 2), 50, 0, 2);
	strengthenItemName(names, "c", typeKey(2, 6, 3), 150, 0, 2);
	EXPECT_EQ(names.size(), 2u);
	EXPECT_EQ(names.count("b"), 0u); // the least familiar went
	EXPECT_EQ(names.count("c"), 1u); // never the one just heard
}
