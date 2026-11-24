#include "openc2e/PixelText.h"

#include <gtest/gtest.h>

// The world's text in any language (PixelText): UTF-8, with the game's own
// Windows-1252 text still read right; right-to-left runs and Arabic letters
// as drawn; lower case and the voice's a-z spelling.

TEST(PixelText, DecodesUtf8AndTheGamesOwnText) {
	EXPECT_EQ(PixelText::decode("dr\xc3\xbc" "ck"), U"drück");
	// a Windows-1252 byte that is no part of a UTF-8 letter: the game's ü
	EXPECT_EQ(PixelText::decode("dr\xfc" "ck"), U"drück");
	EXPECT_EQ(PixelText::decode("\x80"), U"€");
	EXPECT_EQ(PixelText::decode("я"), U"я");
	EXPECT_EQ(PixelText::encode(U"わたし"), "わたし");
}

TEST(PixelText, LettersAndBackspace) {
	EXPECT_EQ(PixelText::length("я очень"), 7u);
	EXPECT_EQ(PixelText::dropLast("饿饿"), "饿");
	EXPECT_EQ(PixelText::dropLast(""), "");
}

TEST(PixelText, LowerCase) {
	EXPECT_EQ(PixelText::lower("DRÜCK"), "drück");
	EXPECT_EQ(PixelText::lower("ГОЛОДНЫЙ"), "голодный");
	EXPECT_EQ(PixelText::lower("ΠΟΛΎ"), "πολύ");
	EXPECT_EQ(PixelText::lower("İ"), "i");
	EXPECT_EQ(PixelText::lower("ŁÓDŹ"), "łódź");
}

TEST(PixelText, RightToLeft) {
	// a Hebrew line is turned around; English in it keeps its own order
	EXPECT_EQ(PixelText::visualOrder(U"אבג"), U"גבא");
	EXPECT_EQ(PixelText::visualOrder(U"אב Norn"), U"Norn בא");
	EXPECT_EQ(PixelText::visualOrder(U"me Norn"), U"me Norn");
	// Arabic letters take their joined forms: alef (isolated), then lam and
	// alef as one letter
	std::u32string shaped = PixelText::visualOrder(U"لا");
	EXPECT_EQ(shaped, U"ﻻ");
	// beh, then alef: beh initial, alef final; turned around for drawing
	EXPECT_EQ(PixelText::visualOrder(U"با"), U"ﺎﺑ");
}

TEST(PixelText, VoiceSpelling) {
	EXPECT_EQ(PixelText::voiceSpelling("Drück"), "druck");
	EXPECT_EQ(PixelText::voiceSpelling("толкни"), "tolkni");
	EXPECT_EQ(PixelText::voiceSpelling("πολύ"), "poly");
	EXPECT_EQ(PixelText::voiceSpelling("tôi rất đói"), "toi rat doi");
	// other alphabets: made-up syllables, always the same
	std::string a = PixelText::voiceSpelling("わたし"), b = PixelText::voiceSpelling("わたし");
	EXPECT_EQ(a, b);
	EXPECT_EQ(a.size(), 6u);
	for (char c : a)
		EXPECT_TRUE(c >= 'a' && c <= 'z');
}
