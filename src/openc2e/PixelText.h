#pragma once

#include <cstdint>
#include <string>
#include <string_view>

class RenderTarget;
class creaturesImage;

// The letters of text drawn in the world (owner, 2026-11-23): creatures'
// speech bubbles, the teaching computers' screens (blackboards) and the
// world's notices. C2 drew them with its own font (CHARSET.DTA: the 128
// ASCII letters, so even its own German and French words lost their
// accents); Zeetures draws them with Fusion Pixel at 12, 10 or 8 pixels
// (the player's choice, Advanced Preferences), which has the Latin
// alphabets, Greek, Cyrillic, Japanese, Chinese and Korean, and for what it
// lacks small public-domain fonts (Hebrew, Thai) and GNU Unifont (Arabic and
// the rest). The game's own font can still be chosen (plain English letters
// only). Fonts: data/fonts (scripts/pack_pixel_fonts.py), built in.
//
// Text is UTF-8; a byte that is not part of a UTF-8 letter is taken as
// Windows-1252, the game's own text (its world files and scripts).
namespace PixelText {

enum Face {
	ORIGINAL = 0, // the game's own font (CHARSET.DTA)
	PX12 = 12,
	PX10 = 10,
	PX8 = 8,
};
// the player's choice ("world-font" in settings.txt: original, 12, 10 or 8;
// 10 if none). C1 always uses its own.
Face face();
void setFace(Face f);
bool usingGameFont();

std::u32string decode(std::string_view s);
std::string encode(const std::u32string& s);
std::string encode(char32_t c);
// the text as drawn: Arabic letters in their joined forms, right-to-left
// runs (Hebrew, Arabic) turned around
std::u32string visualOrder(const std::u32string& s);
// lower case, for comparing words (Latin, Greek, Cyrillic)
std::string lower(std::string_view s);
// the word in plain a-z letters for the creatures' voice, which speaks only
// those (Latin letters without their accents, Greek and Cyrillic spelled
// out, other alphabets as made-up syllables, always the same)
std::string voiceSpelling(std::string_view s);
// one letter fewer at the end (Backspace), and the number of letters
std::string dropLast(const std::string& s);
size_t length(std::string_view s);

// Drawing one line: the top of the line box at y, 12 pixels tall (the game's
// line). gameFont is the game's own font, coloured, used when it is chosen.
int width(std::string_view text, creaturesImage* gameFont);
int lineHeight(creaturesImage* gameFont);
void draw(RenderTarget* r, int x, int y, std::string_view text, uint32_t rgb, creaturesImage* gameFont);
// whether the chosen font draws this letter (else it is left out)
bool canDraw(char32_t c, creaturesImage* gameFont);

// A letter's pixels in the 12 pixel font (for the program's own windows,
// whose font lacks most alphabets): false if it has none. Rows top to
// bottom, top: from the baseline (negative is up).
struct Glyph {
	int advance = 0, xoff = 0, top = 0, width = 0, height = 0;
	bool mark = false;
	const uint8_t* bits = nullptr; // rows of ceil(width / 8) bytes, top bit leftmost
};
bool uiGlyph(char32_t c, Glyph& out);

} // namespace PixelText
