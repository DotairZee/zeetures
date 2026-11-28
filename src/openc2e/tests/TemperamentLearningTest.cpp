// Temperament and learning (creatures/Temperament.h, LearningChannel): each
// channel's factor is 1 for an average creature, moves smoothly with each
// trait, mirrors itself between the low and high ends, stays within 0.5-2,
// and every trait pays off at both ends; the stress curve peaks at lower
// stress the more reactive the creature; the Owner's Kit line names the
// strongest effects.

#include "openc2e/creatures/Temperament.h"

#include <cmath>
#include <gtest/gtest.h>
#include <string>

using namespace Temperaments;

namespace {
Temperament with(int trait, float z) {
	Temperament t;
	t.z[trait] = z;
	return t;
}
} // namespace

TEST(TemperamentLearning, AverageIsOne) {
	Temperament average;
	for (int ch = 0; ch < NUM_LEARNING_CHANNELS; ch++)
		EXPECT_FLOAT_EQ(learningFactor(average, ch), 1.0f) << "channel " << ch;
}

TEST(TemperamentLearning, CuriousLearnNewWordsFaster) {
	// the Reddit question: a curious creature does learn new words faster
	EXPECT_GT(learningFactor(with(CURIOSITY, 1.0f), LEARN_WORDS_NEW), 1.3f);
	EXPECT_LT(learningFactor(with(CURIOSITY, -1.0f), LEARN_WORDS_NEW), 0.75f);
	// and keeps what it knows less deeply than the incurious
	EXPECT_LT(learningFactor(with(CURIOSITY, 1.0f), LEARN_SETTLING_FAMILIAR), 1.0f);
	EXPECT_GT(learningFactor(with(CURIOSITY, -1.0f), LEARN_SETTLING_FAMILIAR), 1.0f);
}

TEST(TemperamentLearning, SmoothAndMirrored) {
	// a single-trait channel: rises steadily with the trait, and the low end
	// divides by what the high end multiplies by
	float last = 0.0f;
	for (float z = -2.0f; z <= 2.0f; z += 0.25f) {
		float f = learningFactor(with(REACTIVITY, z), LEARN_DANGER);
		EXPECT_GT(f, last);
		last = f;
		float mirror = learningFactor(with(REACTIVITY, -z), LEARN_DANGER);
		EXPECT_NEAR(f * mirror, 1.0f, 1e-4f);
	}
	// the moderate strength: about x1.35 a step up the reactivity scale
	EXPECT_NEAR(learningFactor(with(REACTIVITY, 1.0f), LEARN_DANGER), std::exp(0.30f), 1e-4f);
}

TEST(TemperamentLearning, ClampedToHalfAndDouble) {
	Temperament extreme;
	for (auto& z : extreme.z)
		z = 2.0f;
	Temperament opposite;
	for (auto& z : opposite.z)
		z = -2.0f;
	for (int ch = 0; ch < NUM_LEARNING_CHANNELS; ch++) {
		EXPECT_LE(learningFactor(extreme, ch), 2.0f);
		EXPECT_GE(learningFactor(extreme, ch), 0.5f);
		EXPECT_LE(learningFactor(opposite, ch), 2.0f);
		EXPECT_GE(learningFactor(opposite, ch), 0.5f);
	}
	// (scores beyond the usual +-2 count as 2)
	EXPECT_FLOAT_EQ(learningFactor(with(CURIOSITY, 5.0f), LEARN_WORDS_NEW), learningFactor(with(CURIOSITY, 2.0f), LEARN_WORDS_NEW));
}

TEST(TemperamentLearning, EveryTraitPaysOffAtBothEnds) {
	// for each trait that touches learning at all, the high end is faster at
	// something and the low end is faster at something else
	for (int trait = 0; trait < NUM_TRAITS; trait++) {
		bool any = false, highWins = false, lowWins = false;
		for (int ch = 0; ch < NUM_LEARNING_CHANNELS; ch++) {
			float k = learningK(ch, trait);
			any |= k != 0.0f;
			highWins |= k > 0.0f;
			lowWins |= k < 0.0f;
		}
		if (!any)
			continue;
		// temper's payoff at the low end is forgiving and taking correction
		EXPECT_TRUE(highWins) << traitName(trait);
		EXPECT_TRUE(lowWins) << traitName(trait);
	}
}

TEST(TemperamentLearning, StressCurvePeaksLowerForTheReactive) {
	// no stress: no change; a little: a lift; a lot: a fall
	EXPECT_FLOAT_EQ(stressLearning(0.0f, 0.0f), 1.0f);
	EXPECT_GT(stressLearning(0.0f, 0.3f), 1.05f);
	EXPECT_LT(stressLearning(0.0f, 0.9f), 0.95f);
	// mild everywhere
	for (float zr : {-2.0f, 0.0f, 2.0f})
		for (float s = 0.0f; s <= 1.0f; s += 0.05f) {
			EXPECT_GE(stressLearning(zr, s), 0.8f);
			EXPECT_LE(stressLearning(zr, s), 1.08f);
		}
	// at the same middling stress the reactive are already past their peak,
	// the steady-nerved still before theirs
	EXPECT_LT(stressLearning(2.0f, 0.45f), stressLearning(-2.0f, 0.45f));
}

TEST(TemperamentLearning, KitLine) {
	Temperament average;
	EXPECT_EQ(learningLine(average), "");
	// a highly reactive creature: danger learned fast, fears let go slowly
	std::string line = learningLine(with(REACTIVITY, 2.0f));
	EXPECT_NE(line.find("learns danger quickly"), std::string::npos) << line;
	EXPECT_NE(line.find("lets go of a fear slowly"), std::string::npos) << line;
	// a very shy creature: its strongest effect is letting go of a fear slowly
	EXPECT_EQ(learningLine(with(BOLDNESS, -2.0f)).rfind("lets go of a fear slowly", 0), 0u);
	// and it learns words best from family
	EXPECT_GT(learningFactor(with(BOLDNESS, -2.0f), LEARN_FROM_FAMILY), 1.3f);
}
