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
