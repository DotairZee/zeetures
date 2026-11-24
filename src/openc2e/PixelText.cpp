#include "PixelText.h"

#include "EmbeddedFiles.h"
#include "Engine.h"
#include "common/backend/Backend.h"
#include "common/creaturesImage.h"
#include "creatures/CreatureSpeech.h" // readPlayerSetting

#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>
#include <zlib.h>

namespace PixelText {

namespace {

// --- the fonts (data/fonts, .zpf: see scripts/pack_pixel_fonts.py) ---

#pragma pack(push, 1)
struct Entry {
	uint32_t cp;
	int8_t advance, xoff, top;
	uint8_t width, height, flags;
	uint32_t offset;
};
#pragma pack(pop)
static_assert(sizeof(Entry) == 14);

struct Font {
	int ascent = 0, descent = 0;
	std::vector<uint8_t> raw;
	const Entry* entries = nullptr;
	size_t count = 0;
	const uint8_t* bits = nullptr;

	const Entry* find(char32_t c) const {
		const Entry* end = entries + count;
		const Entry* e = std::lower_bound(entries, end, c, [](const Entry& a, char32_t b) { return a.cp < b; });
		return e != end && e->cp == c ? e : nullptr;
	}
};

const Font* loadFont(const std::string& name) {
	static std::map<std::string, std::unique_ptr<Font>> fonts;
	auto it = fonts.find(name);
	if (it != fonts.end())
		return it->second.get();
	auto f = std::make_unique<Font>();
	auto data = EmbeddedFile(name);
	if (data.size() > 8 && memcmp(data.data(), "ZPFZ", 4) == 0) {
		uLongf size = data[4] | (data[5] << 8) | (data[6] << 16) | ((uLongf)data[7] << 24);
		f->raw.resize(size);
		if (uncompress(f->raw.data(), &size, data.data() + 8, data.size() - 8) == Z_OK && size >= 8) {
			f->ascent = f->raw[0];
			f->descent = f->raw[1];
			uint32_t n;
			memcpy(&n, f->raw.data() + 4, 4);
			if (8 + n * sizeof(Entry) <= size) {
				f->entries = reinterpret_cast<const Entry*>(f->raw.data() + 8);
				f->count = n;
				f->bits = f->raw.data() + 8 + n * sizeof(Entry);
			}
		}
	}
	const Font* p = f.get();
	fonts[name] = std::move(f);
	return p;
}

// each size: Fusion Pixel first, then what fills its gaps, nearest in size
// first; and where the baseline sits in the game's 12 pixel line
struct Chain {
	std::vector<const Font*> fonts;
	int baseline;
};

const Chain& chainFor(Face f) {
	static std::map<int, Chain> chains;
	auto it = chains.find(f);
	if (it != chains.end())
		return it->second;
	std::vector<const char*> names;
	int baseline = 10;
	switch (f) {
		case PX8:
			names = {"fusion-8.zpf", "fixed-5x8.zpf", "fusion-10.zpf", "fixed-6x13.zpf", "fusion-12.zpf", "unifont-part.zpf"};
			baseline = 8;
			break;
		case PX12:
			names = {"fusion-12.zpf", "unifont-part.zpf"};
			baseline = 10;
			break;
		default:
			names = {"fusion-10.zpf", "fixed-6x10.zpf", "fixed-6x13.zpf", "fusion-12.zpf", "unifont-part.zpf"};
			baseline = 9;
			break;
	}
	Chain c;
	c.baseline = baseline;
	for (auto n : names) {
		const Font* font = loadFont(n);
		if (font->count)
			c.fonts.push_back(font);
	}
	return chains[f] = c;
}

const Entry* findGlyph(const Chain& c, char32_t cp, const Font** from) {
	for (const Font* f : c.fonts)
		if (const Entry* e = f->find(cp)) {
			*from = f;
			return e;
		}
	return nullptr;
}

// --- each letter's picture, by font and colour, made when first drawn ---

struct Sheet {
	std::shared_ptr<creaturesImage> image = std::make_shared<creaturesImage>("pixeltext");
	std::unordered_map<char32_t, unsigned int> frames;
};

// the picture sheet a letter is drawn from, and its frame in it
creaturesImage& frameFor(const Font* font, const Entry* e, uint32_t rgb, unsigned int& frame) {
	static std::map<std::pair<const Font*, uint32_t>, Sheet> sheets;
	Sheet& s = sheets[{font, rgb}];
	auto it = s.frames.find(e->cp);
	if (it != s.frames.end()) {
		frame = it->second;
		return *s.image;
	}
	Image im;
	im.width = std::max<int>(1, e->width);
	im.height = std::max<int>(1, e->height);
	im.format = if_rgba32;
	im.data = shared_array<uint8_t>(im.width * im.height * 4);
	memset(im.data.data(), 0, im.width * im.height * 4);
	int stride = (e->width + 7) / 8;
	for (int y = 0; y < e->height; y++)
		for (int x = 0; x < e->width; x++)
			if (font->bits[e->offset + y * stride + x / 8] & (0x80 >> (x % 8))) {
				uint8_t* p = im.data.data() + (y * im.width + x) * 4;
				p[0] = (rgb >> 16) & 0xff;
				p[1] = (rgb >> 8) & 0xff;
				p[2] = rgb & 0xff;
				p[3] = 255;
			}
	s.image->images.push_back(im);
	s.image->textures.emplace_back();
	frame = s.image->images.size() - 1;
	s.frames[e->cp] = frame;
	return *s.image;
}

int s_face = -1;

// Windows-1252's letters 0x80-0x9F
const char32_t cp1252[32] = {0x20AC, 0xFFFD, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0xFFFD, 0x017D, 0xFFFD,
	0xFFFD, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0xFFFD, 0x017E, 0x0178};

int toCp1252(char32_t c) {
	if (c < 0x80 || (c >= 0xA0 && c <= 0xFF))
		return (int)c;
	for (int i = 0; i < 32; i++)
		if (cp1252[i] == c)
			return 0x80 + i;
	return -1;
}

bool isMark(char32_t c) {
	return (c >= 0x0300 && c <= 0x036F) || (c >= 0x0591 && c <= 0x05C7 && c != 0x05BE && c != 0x05C0 && c != 0x05C3 && c != 0x05C6) || (c >= 0x064B && c <= 0x065F) || c == 0x0670 || c == 0x0E31 || (c >= 0x0E34 && c <= 0x0E3A) || (c >= 0x0E47 && c <= 0x0E4E);
}

bool isRtl(char32_t c) {
	return (c >= 0x0590 && c <= 0x08FF) || (c >= 0xFB1D && c <= 0xFDFF) || (c >= 0xFE70 && c <= 0xFEFF);
}

bool isStrongLtr(char32_t c) {
	if (isRtl(c) || isMark(c))
		return false;
	if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
		return true;
	return c >= 0xC0 && c != 0xD7 && c != 0xF7 && !(c >= 0x2000 && c <= 0x2BFF) && !(c >= 0x3000 && c <= 0x303F) && !(c >= 0xFF00 && c <= 0xFF20);
}

// Arabic: each letter's isolated form in Arabic Presentation Forms (A or B;
// final, initial and medial follow it), and whether it joins the letter
// after it (dual-joining) or only the one before
struct ArabicLetter {
	char32_t letter, isolated;
	bool dual;
};
const ArabicLetter arabic[] = {{0x0621, 0xFE80, false}, {0x0622, 0xFE81, false}, {0x0623, 0xFE83, false}, {0x0624, 0xFE85, false}, {0x0625, 0xFE87, false},
	{0x0626, 0xFE89, true}, {0x0627, 0xFE8D, false}, {0x0628, 0xFE8F, true}, {0x0629, 0xFE93, false}, {0x062A, 0xFE95, true}, {0x062B, 0xFE99, true},
	{0x062C, 0xFE9D, true}, {0x062D, 0xFEA1, true}, {0x062E, 0xFEA5, true}, {0x062F, 0xFEA9, false}, {0x0630, 0xFEAB, false}, {0x0631, 0xFEAD, false},
	{0x0632, 0xFEAF, false}, {0x0633, 0xFEB1, true}, {0x0634, 0xFEB5, true}, {0x0635, 0xFEB9, true}, {0x0636, 0xFEBD, true}, {0x0637, 0xFEC1, true},
	{0x0638, 0xFEC5, true}, {0x0639, 0xFEC9, true}, {0x063A, 0xFECD, true}, {0x0641, 0xFED1, true}, {0x0642, 0xFED5, true}, {0x0643, 0xFED9, true},
	{0x0644, 0xFEDD, true}, {0x0645, 0xFEE1, true}, {0x0646, 0xFEE5, true}, {0x0647, 0xFEE9, true}, {0x0648, 0xFEED, false}, {0x0649, 0xFEEF, false},
	{0x064A, 0xFEF1, true}, {0x067E, 0xFB56, true}, {0x0686, 0xFB7A, true}, {0x0698, 0xFB8A, false}, {0x06A9, 0xFB8E, true}, {0x06AF, 0xFB92, true},
	{0x06CC, 0xFBFC, true}};

const ArabicLetter* arabicLetter(char32_t c) {
	for (auto& a : arabic)
		if (a.letter == c)
			return &a;
	return nullptr;
}

std::u32string shapeArabic(const std::u32string& s) {
	std::u32string out;
	auto neighbour = [&](size_t i, int step) -> const ArabicLetter* {
		for (long j = (long)i + step; j >= 0 && j < (long)s.size(); j += step) {
			if (isMark(s[j]))
				continue;
			if (s[j] == 0x0640)
				return &arabic[5]; // the joining stroke (tatweel) joins both ways
			return arabicLetter(s[j]);
		}
		return nullptr;
	};
	for (size_t i = 0; i < s.size(); i++) {
		const ArabicLetter* a = arabicLetter(s[i]);
		if (!a || a->letter == 0x0621) {
			out += s[i];
			continue;
		}
		const ArabicLetter* prev = neighbour(i, -1);
		bool joinsPrev = prev && prev->dual;
		// lam with alef: one letter
		if (a->letter == 0x0644 && i + 1 < s.size()) {
			char32_t lig = 0;
			switch (s[i + 1]) {
				case 0x0622: lig = 0xFEF5; break;
				case 0x0623: lig = 0xFEF7; break;
				case 0x0625: lig = 0xFEF9; break;
				case 0x0627: lig = 0xFEFB; break;
			}
			if (lig) {
				out += joinsPrev ? lig + 1 : lig;
				i++;
				continue;
			}
		}
		const ArabicLetter* next = neighbour(i, 1);
		bool joinsNext = a->dual && next && next->letter != 0x0621;
		if (joinsPrev && joinsNext)
			out += a->isolated + 3;
		else if (joinsPrev)
			out += a->isolated + 1;
		else if (joinsNext)
			out += a->isolated + 2;
		else
			out += a->isolated;
	}
	return out;
}

} // namespace

// --- choices ---

Face face() {
	if (s_face < 0) {
		std::string v = readPlayerSetting("world-font");
		s_face = v == "original" ? ORIGINAL : v == "12" ? PX12 : v == "8" ? PX8 : PX10;
	}
	return (Face)s_face;
}

void setFace(Face f) {
	s_face = f;
	writePlayerSetting("world-font", f == ORIGINAL ? "original" : std::to_string((int)f));
}

bool usingGameFont() {
	return engine.version < 2 || face() == ORIGINAL;
}

// --- text ---

std::u32string decode(std::string_view s) {
	std::u32string out;
	size_t i = 0;
	while (i < s.size()) {
		unsigned char b = s[i];
		int n = b < 0x80 ? 0 : (b & 0xE0) == 0xC0 ? 1 : (b & 0xF0) == 0xE0 ? 2 : (b & 0xF8) == 0xF0 ? 3 : -1;
		bool ok = n >= 0 && i + n < s.size();
		char32_t c = n == 0 ? b : n == 1 ? (b & 0x1F) : n == 2 ? (b & 0x0F) : (b & 0x07);
		if (ok && n > 0) {
			for (int k = 1; ok && k <= n; k++) {
				unsigned char cb = s[i + k];
				if ((cb & 0xC0) != 0x80)
					ok = false;
				else
					c = (c << 6) | (cb & 0x3F);
			}
			// overlong or out of range: not UTF-8
			if (ok && ((n == 1 && c < 0x80) || (n == 2 && c < 0x800) || (n == 3 && (c < 0x10000 || c > 0x10FFFF))))
				ok = false;
		}
		if (n == 0) {
			out += c;
			i++;
		} else if (ok) {
			out += c;
			i += n + 1;
		} else {
			// the game's own text (Windows-1252)
			out += (b >= 0x80 && b < 0xA0) ? cp1252[b - 0x80] : (char32_t)b;
			i++;
		}
	}
	return out;
}

std::string encode(char32_t c) {
	std::string s;
	if (c < 0x80) {
		s += (char)c;
	} else if (c < 0x800) {
		s += (char)(0xC0 | (c >> 6));
		s += (char)(0x80 | (c & 0x3F));
	} else if (c < 0x10000) {
		s += (char)(0xE0 | (c >> 12));
		s += (char)(0x80 | ((c >> 6) & 0x3F));
		s += (char)(0x80 | (c & 0x3F));
	} else {
		s += (char)(0xF0 | (c >> 18));
		s += (char)(0x80 | ((c >> 12) & 0x3F));
		s += (char)(0x80 | ((c >> 6) & 0x3F));
		s += (char)(0x80 | (c & 0x3F));
	}
	return s;
}

std::string encode(const std::u32string& s) {
	std::string out;
	for (char32_t c : s)
		out += encode(c);
	return out;
}

std::u32string visualOrder(const std::u32string& in) {
	bool anyRtl = false;
	for (char32_t c : in)
		if (isRtl(c)) {
			anyRtl = true;
			break;
		}
	if (!anyRtl)
		return in;
	std::u32string s = shapeArabic(in);
	// clusters: a letter and the marks after it stay together
	std::vector<std::u32string> clusters;
	for (char32_t c : s) {
		if (isMark(c) && !clusters.empty())
			clusters.back() += c;
		else
			clusters.push_back(std::u32string(1, c));
	}
	// each cluster's direction: strong letters their own; others (spaces,
	// punctuation) that of the letters on both sides if they agree, else the
	// line's (its first strong letter's)
	int base = 0;
	std::vector<int> dir(clusters.size(), 0); // 1 rtl, -1 ltr, 0 neutral
	for (size_t i = 0; i < clusters.size(); i++) {
		char32_t c = clusters[i][0];
		dir[i] = isRtl(c) ? 1 : isStrongLtr(c) ? -1 : 0;
		if (!base && dir[i])
			base = dir[i];
	}
	if (!base)
		base = -1;
	for (size_t i = 0; i < clusters.size(); i++) {
		if (dir[i])
			continue;
		int before = base, after = base;
		for (long j = (long)i - 1; j >= 0; j--)
			if (dir[j]) {
				before = dir[j];
				break;
			}
		for (size_t j = i + 1; j < clusters.size(); j++)
			if (dir[j]) {
				after = dir[j];
				break;
			}
		dir[i] = before == after ? before : base;
	}
	// runs against the line's direction are turned around; a right-to-left
	// line is turned around as a whole first
	std::vector<std::u32string> order;
	if (base > 0) {
		order.assign(clusters.rbegin(), clusters.rend());
		std::reverse(dir.begin(), dir.end());
	} else {
		order = clusters;
	}
	for (size_t i = 0; i < order.size();) {
		if (dir[i] == -base) {
			size_t j = i;
			while (j < order.size() && dir[j] == -base)
				j++;
			std::reverse(order.begin() + i, order.begin() + j);
			i = j;
		} else {
			i++;
		}
	}
	std::u32string out;
	for (auto& c : order)
		out += c;
	return out;
}

namespace {
char32_t lowerOne(char32_t c) {
	if (c < 0x80)
		return (c >= 'A' && c <= 'Z') ? c + 32 : c;
	if ((c >= 0xC0 && c <= 0xDE && c != 0xD7))
		return c + 32;
	if (c == 0x130)
		return 'i';
	if (c == 0x178)
		return 0xFF;
	if ((c >= 0x100 && c <= 0x137) || (c >= 0x14A && c <= 0x177) || (c >= 0x1E00 && c <= 0x1EFF))
		return (c % 2 == 0) ? c + 1 : c;
	if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E))
		return (c % 2 == 1) ? c + 1 : c;
	if (c >= 0x391 && c <= 0x3AB && c != 0x3A2)
		return c + 32;
	if (c == 0x386)
		return 0x3AC;
	if (c >= 0x388 && c <= 0x38A)
		return c + 37;
	if (c == 0x38C)
		return 0x3CC;
	if (c == 0x38E || c == 0x38F)
		return c + 63;
	if (c >= 0x410 && c <= 0x42F)
		return c + 32;
	if (c >= 0x400 && c <= 0x40F)
		return c + 80;
	if ((c >= 0x460 && c <= 0x481) || (c >= 0x48A && c <= 0x4BF) || (c >= 0x4D0 && c <= 0x52F))
		return (c % 2 == 0) ? c + 1 : c;
	return c;
}
} // namespace

std::string lower(std::string_view s) {
	std::u32string u = decode(s);
	for (auto& c : u)
		c = lowerOne(c);
	return encode(u);
}

std::string dropLast(const std::string& s) {
	std::u32string u = decode(s);
	if (!u.empty())
		u.pop_back();
	return encode(u);
}

size_t length(std::string_view s) {
	return decode(s).size();
}


namespace {
// Latin letters with accents, by code point from 0xC0, and 0x100-0x17F
const char* latin1 = "aaaaaaaceeeeiiiidnooooo ouuuuytsaaaaaaaceeeeiiiidnooooo ouuuuyty";
const char* latinA = "aaaaaaccccccccddddeeeeeeeeeegggggggghhhhiiiiiiiiiiiijjkkkllllllllllnnnnnnnnnoooooooorrrrrrssssssssttttttuuuuuuuuuuuuwwyyyzzzzzzs";

const char* greekOrCyrillic(char32_t c) {
	switch (c) {
		case 0x3B1: case 0x3AC: return "a";
		case 0x3B2: return "v";
		case 0x3B3: return "g";
		case 0x3B4: return "d";
		case 0x3B5: case 0x3AD: return "e";
		case 0x3B6: return "z";
		case 0x3B7: case 0x3AE: return "i";
		case 0x3B8: return "th";
		case 0x3B9: case 0x3AF: case 0x3CA: case 0x390: return "i";
		case 0x3BA: return "k";
		case 0x3BB: return "l";
		case 0x3BC: return "m";
		case 0x3BD: return "n";
		case 0x3BE: return "x";
		case 0x3BF: case 0x3CC: return "o";
		case 0x3C0: return "p";
		case 0x3C1: return "r";
		case 0x3C2: case 0x3C3: return "s";
		case 0x3C4: return "t";
		case 0x3C5: case 0x3CD: case 0x3CB: case 0x3B0: return "y";
		case 0x3C6: return "f";
		case 0x3C7: return "ch";
		case 0x3C8: return "ps";
		case 0x3C9: case 0x3CE: return "o";
		case 0x430: return "a";
		case 0x431: return "b";
		case 0x432: return "v";
		case 0x433: case 0x491: case 0x493: return "g";
		case 0x434: return "d";
		case 0x435: case 0x44D: return "e";
		case 0x451: return "yo";
		case 0x436: return "zh";
		case 0x437: return "z";
		case 0x438: case 0x456: return "i";
		case 0x439: case 0x44B: return "y";
		case 0x43A: case 0x49B: return "k";
		case 0x43B: return "l";
		case 0x43C: return "m";
		case 0x43D: case 0x4A3: return "n";
		case 0x43E: case 0x4E9: return "o";
		case 0x43F: return "p";
		case 0x440: return "r";
		case 0x441: return "s";
		case 0x442: return "t";
		case 0x443: case 0x45E: case 0x4AF: case 0x4B1: return "u";
		case 0x444: return "f";
		case 0x445: return "kh";
		case 0x446: return "ts";
		case 0x447: return "ch";
		case 0x448: return "sh";
		case 0x449: return "shch";
		case 0x44A: case 0x44C: return "";
		case 0x44E: return "yu";
		case 0x44F: return "ya";
		case 0x454: return "ye";
		case 0x457: return "yi";
		case 0x452: return "dj";
		case 0x45B: return "c";
		case 0x45F: return "dz";
		case 0x459: return "lj";
		case 0x45A: return "nj";
		case 0x458: return "j";
		case 0x4D9: return "a";
		case 0x4BB: return "h";
	}
	return nullptr;
}
} // namespace

std::string voiceSpelling(std::string_view text) {
	static const char* cons = "bdfghklmnprstvz";
	static const char* vow = "aeiou";
	std::string out;
	for (char32_t c : decode(lower(text))) {
		if (c < 0x80) {
			out += (char)c;
		} else if (c >= 0xC0 && c <= 0xFF) {
			char b = latin1[c - 0xC0];
			out += b == ' ' ? ' ' : b;
		} else if (c >= 0x100 && c <= 0x17F) {
			out += latinA[c - 0x100];
		} else if (c == 0x1A1 || c == 0x1A0) {
			out += 'o';
		} else if (c == 0x1B0 || c == 0x1AF) {
			out += 'u';
		} else if (c >= 0x218 && c <= 0x21B) {
			out += c < 0x21A ? 's' : 't';
		} else if (c >= 0x1EA0 && c <= 0x1EF9) {
			out += c < 0x1EB8 ? 'a' : c < 0x1EC8 ? 'e' : c < 0x1ECC ? 'i' : c < 0x1EE4 ? 'o' : c < 0x1EF2 ? 'u' : 'y';
		} else if (const char* g = greekOrCyrillic(c)) {
			out += g;
		} else if (isMark(c) || (c >= 0x2000 && c <= 0x206F) || (c >= 0x3000 && c <= 0x303F)) {
			out += ' ';
		} else {
			// another alphabet: a made-up syllable for each letter
			out += cons[c % 15];
			out += vow[(c / 15) % 5];
		}
	}
	return out;
}

// --- drawing ---

bool canDraw(char32_t c, creaturesImage* gameFont) {
	if (usingGameFont()) {
		int b = toCp1252(c);
		return gameFont && b >= 0 && (unsigned int)b < gameFont->numframes();
	}
	const Font* f;
	return c == ' ' || findGlyph(chainFor(face()), c, &f);
}

namespace {
// lays out a line: calls put(font, entry, x, y) for each letter; returns the width
template <typename Put>
int layout(std::string_view text, Put put) {
	const Chain& chain = chainFor(face());
	std::u32string vis = visualOrder(decode(text));
	int x = 0, right = 0;
	int prevX = 0, prevW = 0;
	bool havePrev = false;
	for (char32_t c : vis) {
		const Font* font = nullptr;
		const Entry* e = findGlyph(chain, c, &font);
		if (!e)
			continue;
		if (e->flags & 1) {
			if (havePrev)
				put(font, e, prevX + (prevW - (int)e->width) / 2, chain.baseline + e->top);
			continue;
		}
		int gx = x + e->xoff;
		if (e->width)
			put(font, e, gx, chain.baseline + e->top);
		prevX = gx;
		prevW = e->width;
		havePrev = true;
		right = std::max(right, gx + (int)e->width);
		x += e->advance;
	}
	return std::max(right, x - 1);
}
} // namespace

int width(std::string_view text, creaturesImage* gameFont) {
	if (usingGameFont()) {
		int w = 0;
		for (char32_t c : decode(text)) {
			int b = toCp1252(c);
			if (gameFont && b >= 0 && (unsigned int)b < gameFont->numframes())
				w += gameFont->width(b) + 1;
		}
		return w > 0 ? w - 1 : 0;
	}
	return layout(text, [](const Font*, const Entry*, int, int) {});
}

int lineHeight(creaturesImage* gameFont) {
	if (usingGameFont())
		return gameFont && gameFont->numframes() > 'A' ? gameFont->height('A') : 12;
	return 12;
}

void draw(RenderTarget* r, int x, int y, std::string_view text, uint32_t rgb, creaturesImage* gameFont) {
	if (usingGameFont()) {
		if (!gameFont)
			return;
		int pos = 0;
		for (char32_t c : decode(text)) {
			int b = toCp1252(c);
			if (b < 0 || (unsigned int)b >= gameFont->numframes())
				continue;
			r->renderCreaturesImage(*gameFont, b, x + pos, y);
			pos += gameFont->width(b) + 1;
		}
		return;
	}
	layout(text, [&](const Font* font, const Entry* e, int gx, int gy) {
		unsigned int frame;
		creaturesImage& sheet = frameFor(font, e, rgb, frame);
		r->renderCreaturesImage(sheet, frame, x + gx, y + gy);
	});
}

bool uiGlyph(char32_t c, Glyph& out) {
	const Chain& chain = chainFor(PX12);
	const Font* f = nullptr;
	const Entry* e = findGlyph(chain, c, &f);
	if (!e)
		return false;
	out.advance = e->advance;
	out.xoff = e->xoff;
	out.top = e->top;
	out.width = e->width;
	out.height = e->height;
	out.mark = e->flags & 1;
	out.bits = f->bits + e->offset;
	return true;
}

} // namespace PixelText
