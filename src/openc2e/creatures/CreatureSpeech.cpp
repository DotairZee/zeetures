/*
 *  CreatureSpeech.cpp
 *  openc2e
 *
 *  C1/C2 creatures speaking under a mind. See CreatureSpeech.h.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 */

#include "CreatureSpeech.h"
#include "BrainOrgan.h"
#include "Lightning.h"
#include "PixelText.h"
#include "WordLists.h"
#include "common/devenv.h"

#include "Agent.h"
#include "Bubble.h"
#include "Engine.h"
#include "PathResolver.h"
#include "VoiceData.h"
#include "World.h"
#include "PointerAgent.h"
#include "historyManager.h"
#include "common/Random.h"
#include "creatures/CreatureAgent.h"
#include "creatures/Breeding.h"
#include "creatures/oldCreature.h"
#include "creatures/Language.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fmt/core.h>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <vector>

namespace {

// the game's word list numbers (C2) for the words our meanings use
const int W_EAT = 12, W_HIT = 13, W_ME = 16, W_FOOD = 22, W_NORN = 52, W_DRIVE0 = 56, W_ANGRY = 67,
		  W_FRIENDLY = 68, W_HURT = 69, W_PAIN = 56, W_YES = 73, W_NO = 74, W_LOOK = 75, W_VERY = 77,
		  W_WHY = 8, W_REST = 9, W_WHAT = 76, W_BORED = 66;

bool watching() {
	static const bool watch = devenv("OPENC2E_WATCH_SPEECH") != nullptr;
	return watch;
}

// (any language's letters: PixelText)
std::string lower(const std::string& s) {
	return PixelText::lower(s);
}

// a line said, for finding words in it: lower case, punctuation as spaces,
// padded with a space at each end
std::string spacedText(const std::string& text) {
	std::u32string out = U" ";
	for (char32_t c : PixelText::decode(lower(text))) {
		bool letter = c < 0x80 ? std::isalnum((int)c) != 0 : !((c >= 0x2000 && c <= 0x206F) || (c >= 0x3000 && c <= 0x303F) || (c >= 0xFF00 && c <= 0xFF0F) || (c >= 0xFF1A && c <= 0xFF20) || c == 0xA0 || c == 0xBF || c == 0xA1 || c == 0xAB || c == 0xBB);
		out += letter ? c : U' ';
	}
	out += U' ';
	return PixelText::encode(out);
}

// where a word of the list is in such a line (npos: not there). Words of
// the languages written without spaces (Chinese, Japanese, Thai) are found
// anywhere in it; the rest only whole.
size_t findWord(const std::string& spaced, const std::string& word) {
	std::u32string w = PixelText::decode(word);
	bool unspaced = !w.empty() && ((w[0] >= 0x2E80 && w[0] <= 0x9FFF) || (w[0] >= 0xF900 && w[0] <= 0xFAFF) || (w[0] >= 0x0E00 && w[0] <= 0x0E7F) || (w[0] >= 0xFF66 && w[0] <= 0xFF9F));
	if (unspaced)
		return spaced.find(word);
	return spaced.find(" " + word + " ");
}

std::string trim(const std::string& s);

// the list's words said in such a line, each where it is first said, in the
// order said. A word found only inside a longer one also said is not
// counted (Vietnamese "buồn ngủ", sleepy, held "buồn", sad, so saying
// sleepy also said sad; the same for any language whose words share a first
// syllable): the longest match wins. A word said again on its own still
// counts.
std::vector<std::pair<size_t, int>> wordsSaid(const std::string& spaced) {
	struct Hit {
		size_t at, len;
		int w;
	};
	std::vector<Hit> hits; // every place each word is found
	auto& en = WordLists::english();
	for (size_t w = 0; w < engine.wordlist.size(); w++) {
		std::vector<std::string> forms;
		std::string word = lower(trim(engine.wordlist[w]));
		if (!word.empty())
			forms.push_back(word);
		if (w < en.size() && !en[w].empty() && lower(en[w]) != word)
			forms.push_back(lower(en[w]));
		for (auto& f : forms) {
			size_t from = 0;
			while (true) {
				size_t at = findWord(spaced.substr(from), f);
				if (at == std::string::npos)
					break;
				at += from;
				// (a whole word is found with the spaces round it: its own
				// span is inside those)
				bool spacedForm = spaced.compare(at, 1, " ") == 0;
				size_t start = spacedForm ? at + 1 : at;
				hits.push_back({start, f.size(), (int)w});
				from = start + 1;
			}
		}
	}
	std::vector<std::pair<size_t, int>> said;
	std::vector<bool> seen(engine.wordlist.size(), false);
	std::vector<std::pair<size_t, int>> firsts;
	for (auto& h : hits) {
		bool inside = false;
		for (auto& o : hits)
			if (o.w != h.w && o.len > h.len && o.at <= h.at && h.at + h.len <= o.at + o.len) {
				inside = true;
				break;
			}
		if (inside)
			continue;
		firsts.push_back({h.at, h.w});
	}
	std::sort(firsts.begin(), firsts.end());
	for (auto& [at, w] : firsts)
		if (!seen[(size_t)w]) {
			seen[(size_t)w] = true;
			said.push_back({at, w});
		}
	return said;
}

std::string trim(const std::string& s) {
	size_t a = s.find_first_not_of(' '), b = s.find_last_not_of(' ');
	return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

bool isWord(int w) {
	return w >= 0 && (size_t)w < engine.wordlist.size() && !trim(engine.wordlist[w]).empty();
}

oldCreature* creatureOf(Agent* a) {
	CreatureAgent* ca = dynamic_cast<CreatureAgent*>(a);
	return ca ? dynamic_cast<oldCreature*>(ca->getCreature()) : nullptr;
}

// (an agent with no picture yet, such as a blackboard just made: its corner)
float centreX(Agent* a) { return a->x + (a->part(0) ? a->getWidth() / 2.0f : 0.0f); }
float centreY(Agent* a) { return a->y + (a->part(0) ? a->getHeight() / 2.0f : 0.0f); }

// within earshot of a sound at (x, y)
bool inEarshot(Agent* listener, float x, float y) {
	return std::fabs(centreX(listener) - x) <= 350 && std::fabs(centreY(listener) - y) <= 150;
}

// how readily each life stage takes in words (baby ... senile)
float stageLearning(int stage) {
	static const float rate[7] = {0.75f, 1.25f, 1.1f, 1.0f, 0.9f, 0.7f, 0.4f};
	return (stage >= 0 && stage < 7) ? rate[stage] : 1.0f;
}

// a word with no voice to speak it: syllables of our own, always the same
std::string fallbackNative(const std::string& word) {
	static const char* syl[] = {"ba", "doo", "mi", "bo", "na", "lu"};
	uint32_t h = 2166136261u;
	for (char ch : word)
		h = (h ^ (unsigned char)ch) * 16777619u;
	std::string out = syl[h % 6];
	out += "-";
	out += syl[(h >> 8) % 6];
	return out;
}

// the word as the creature's bubble shows it: its own name for the thing
// (ownNames), the learned word, or its own tongue's
std::string showWord(Agent* creature, oldCreature* c, int w) {
	std::string word = isWord(w) ? trim(engine.wordlist[w]) : std::string();
	// (experimental: a thing by what it takes its words to mean)
	if (Language::experimental() && w > 16 && w < 56 && c) {
		Language::Naming n = Language::nameFor(creature, c, w - 16, false);
		if (!n.own.empty())
			return n.own;
		if (n.word >= 0)
			return trim(engine.wordlist[n.word]) + (n.unsure ? "?" : "");
		return attemptWord(creature, c, w);
	}
	if (w >= 16 && w < 56)
		if (std::string own = ownNameFor(c, w - 16); !own.empty())
			return own;
	if (knowsWord(c, w))
		return word;
	return attemptWord(creature, c, w);
}

// meaning "kind:a:b" split
std::vector<std::string> parts(const std::string& m) {
	std::vector<std::string> out;
	size_t start = 0;
	for (;;) {
		size_t c = m.find(':', start);
		out.push_back(m.substr(start, c == std::string::npos ? std::string::npos : c - start));
		if (c == std::string::npos)
			break;
		start = c + 1;
	}
	return out;
}

// one way of putting a meaning: its words and which one carries it
struct Wording {
	std::vector<int> words;
	int key; // index into words
};

std::map<int, AgentRef> bubbles; // each speaker's bubble, by UNID

// the player's settings file: "key value" lines
std::filesystem::path settingsFile() {
	return getUserDataDir() / "settings.txt";
}

// The file is read once and kept (2026-11-26: some settings were asked for
// every frame, which opened and read the file 60 times a second; on a
// Windows Documents folder watched by an antivirus or synced by OneDrive a
// single open can stall). writeSetting, the only writer, has it read again.
static bool s_settingsLoaded = false;
static std::filesystem::path s_settingsFor;
static std::map<std::string, std::string> s_settings;

std::string readSetting(const std::string& key) {
	std::filesystem::path file = settingsFile();
	if (!s_settingsLoaded || file != s_settingsFor) {
		s_settings.clear();
		std::ifstream in(file);
		std::string line;
		while (std::getline(in, line)) {
			std::istringstream l(line);
			std::string k, v;
			if (l >> k >> v)
				s_settings.emplace(k, v); // (the first line for a key, as before)
		}
		s_settingsLoaded = true;
		s_settingsFor = file;
	}
	auto it = s_settings.find(key);
	return it == s_settings.end() ? "" : it->second;
}

void writeSetting(const std::string& key, const std::string& value) {
	std::vector<std::string> lines;
	{
		std::ifstream in(settingsFile());
		std::string line;
		while (std::getline(in, line)) {
			std::istringstream l(line);
			std::string k;
			if (!(l >> k && k == key))
				lines.push_back(line);
		}
	}
	lines.push_back(key + " " + value);
	{
		std::ofstream out(settingsFile(), std::ios::binary);
		for (auto& l : lines)
			out << l << "\n";
	}
	s_settingsLoaded = false;
}

int s_bubbles_shown = -1; // -1: not read yet

} // namespace

std::string readPlayerSetting(const std::string& key) {
	return readSetting(key);
}

void writePlayerSetting(const std::string& key, const std::string& value) {
	writeSetting(key, value);
}

static int s_babyWords = -1; // -1: not read yet

bool babyWords() {
	if (s_babyWords < 0) {
		if (const char* e = devenv("OPENC2E_BABY_WORDS"))
			s_babyWords = atoi(e) ? 1 : 0;
		else
			s_babyWords = readSetting("baby-words") == "1" ? 1 : 0;
	}
	return s_babyWords == 1;
}

void setBabyWords(bool on) {
	s_babyWords = on ? 1 : 0;
	writeSetting("baby-words", on ? "1" : "0");
}

static int s_wordAttempts = -1;

bool wordAttempts() {
	if (s_wordAttempts < 0) {
		if (const char* e = devenv("OPENC2E_WORD_ATTEMPTS"))
			s_wordAttempts = atoi(e) ? 1 : 0;
		else
			s_wordAttempts = readSetting("word-attempts") == "0" ? 0 : 1;
	}
	return s_wordAttempts == 1;
}

void setWordAttempts(bool on) {
	s_wordAttempts = on ? 1 : 0;
	writeSetting("word-attempts", on ? "1" : "0");
}

// How well it knows a word, for its tries at it: past ATTEMPT_START (about
// a third of the way to knowing it) the word's own start runs into its
// tongue; past ATTEMPT_CLOSE, the word a little off.
static const int ATTEMPT_START = 48, ATTEMPT_CLOSE = 88;

static bool isVowel(char ch) {
	return ch == 'a' || ch == 'e' || ch == 'i' || ch == 'o' || ch == 'u' || ch == 'y';
}

// the same slip each time for one creature and one word
static uint32_t slipHash(oldCreature* c, const std::string& word) {
	uint32_t h = 2166136261u;
	std::string key = (c && world.history ? world.history->findMoniker(c->getGenome()) : std::string()) + "|" + word;
	for (char ch : key)
		h = (h ^ (uint8_t)ch) * 16777619u;
	return h;
}

std::string attemptWord(Agent* creature, oldCreature* c, int w) {
	std::string word = isWord(w) ? trim(engine.wordlist[w]) : std::string();
	int v = c && w >= 0 && (size_t)w < c->vocab.size() ? c->vocab[w] : 0;
	if (word.empty() || !wordAttempts() || v < ATTEMPT_START)
		return nativeWord(creature, word);
	std::string low = lower(word);
	if (v < ATTEMPT_CLOSE) {
		// its start (to the end of its first vowels, and the next sound if
		// more follows), then the rest in its own tongue: "han-nah-do"
		size_t i = 0;
		while (i < low.size() && !isVowel(low[i]))
			i++;
		while (i < low.size() && isVowel(low[i]))
			i++;
		if (i + 1 < low.size())
			i++;
		std::string start = low.substr(0, std::max<size_t>(1, i));
		std::string native = nativeWord(creature, word);
		std::vector<std::string> syl;
		for (size_t a = 0, b; a <= native.size(); a = b + 1) {
			b = native.find('-', a);
			if (b == std::string::npos)
				b = native.size();
			if (b > a)
				syl.push_back(native.substr(a, b - a));
		}
		std::string out = start;
		for (size_t k = syl.size() > 1 ? 1 : 0; k < syl.size() && k < 3; k++)
			out += "-" + syl[k];
		return out;
	}
	// nearly: R as W, S as Th (not "sh")
	std::string out;
	for (size_t i = 0; i < low.size(); i++) {
		char ch = low[i];
		if (ch == 'r')
			out += 'w';
		else if (ch == 's' && !(i + 1 < low.size() && low[i + 1] == 'h'))
			out += "th";
		else
			out += ch;
	}
	if (out != low)
		return out;
	// or one sound slipped, the last one it can: "hanb"
	static const std::map<char, char> slip = {{'d', 'b'}, {'t', 'p'}, {'k', 't'}, {'g', 'd'}, {'n', 'm'}, {'m', 'n'}, {'p', 't'}, {'b', 'd'},
		{'l', 'w'}, {'f', 'p'}, {'v', 'b'}, {'z', 'd'}, {'x', 'k'}, {'c', 't'}, {'h', 'w'}, {'j', 'd'}, {'w', 'b'}};
	std::vector<size_t> where;
	for (size_t i = 0; i < out.size(); i++)
		if (slip.count(out[i]))
			where.push_back(i);
	if (where.empty())
		return out + out.substr(out.size() - 1); // (all vowels: drawn out)
	// mostly the last sound, sometimes the first
	size_t at = (slipHash(c, low) % 3 == 0) ? where.front() : where.back();
	out[at] = slip.at(out[at]);
	return out;
}

static int s_population_limit = -1; // -1: not read yet

int populationLimit() {
	if (s_population_limit < 0) {
		// developer aid: OPENC2E_POPULATION_LIMIT=n starts with that limit
		// (not saved), for tests that must not touch the player's settings
		if (const char* e = devenv("OPENC2E_POPULATION_LIMIT"))
			return s_population_limit = std::max(0, atoi(e));
		// (Zeetures: 150 until the player sets one; 0 is no limit)
		std::string v = readSetting("population-limit");
		s_population_limit = v.empty() ? 150 : std::max(0, atoi(v.c_str()));
	}
	return s_population_limit;
}

void setPopulationLimit(int norns) {
	s_population_limit = std::max(0, norns);
	writeSetting("population-limit", std::to_string(s_population_limit));
}

static int s_autosave_minutes = -1; // -1: not read yet

int autosaveMinutes() {
	if (s_autosave_minutes < 0) {
		std::string v = readSetting("autosave-minutes");
		s_autosave_minutes = v.empty() ? 5 : std::max(0, atoi(v.c_str()));
	}
	return s_autosave_minutes;
}

void setAutosaveMinutes(int minutes) {
	s_autosave_minutes = std::max(0, minutes);
	writeSetting("autosave-minutes", std::to_string(s_autosave_minutes));
}

bool speechBubbleShowing(Agent* creature) {
	if (!creature)
		return false;
	auto it = bubbles.find(creature->getUNID());
	return it != bubbles.end() && it->second && !it->second->isDying();
}

bool speechBubblesShown() {
	if (s_bubbles_shown < 0)
		s_bubbles_shown = readSetting("speech-bubbles") == "0" ? 0 : 1;
	return s_bubbles_shown == 1;
}

void setSpeechBubblesShown(bool shown) {
	s_bubbles_shown = shown ? 1 : 0;
	writeSetting("speech-bubbles", shown ? "1" : "0");
	if (!shown) {
		for (auto& b : bubbles)
			if (b.second)
				b.second->kill();
		bubbles.clear();
	}
}

std::string nativeWord(Agent* creature, const std::string& word) {
	if (word.empty())
		return "";
	if (!creature || !creature->voice || !*creature->voice)
		return fallbackNative(lower(word));
	std::string out;
	for (auto& e : creature->voice->GetSyllablesFor(word)) {
		// the game's syllable sounds are named for the syllable, then where
		// it falls in a word (Doh0, Doh1, DohE; Do_0 for "do")
		std::string syl = lower(e.name.substr(0, 3));
		syl.erase(std::remove(syl.begin(), syl.end(), '_'), syl.end());
		if (syl.empty())
			continue;
		out += (out.empty() ? "" : "-") + syl;
	}
	return out.empty() ? fallbackNative(lower(word)) : out;
}

bool knowsWord(oldCreature* c, int word) {
	return c && word >= 0 && (size_t)word < c->vocab.size() && c->vocab[word] >= WORD_KNOWN;
}

int wordsKnown(oldCreature* c) {
	int n = 0;
	for (size_t i = 0; c && i < c->vocab.size(); i++)
		n += c->vocab[i] >= WORD_KNOWN && isWord((int)i);
	return n;
}

void hearWord(oldCreature* c, int word, int amount) {
	static const Language::Hearing plain;
	hearWord(c, word, amount, plain);
}

void hearWord(oldCreature* c, int word, int amount, const Language::Hearing& h) {
	if (!c || !isWord(word) || !c->isAlive() || c->isAsleep())
		return;
	if (Language::experimental())
		amount = Language::heard(c->getAgent(), c, word, amount, h);
	if (c->vocab.size() < engine.wordlist.size())
		c->vocab.resize(engine.wordlist.size(), 0);
	// struck by lightning, a parrot (Lightning.h): says back what it hears
	if (Agent* self = c->getAgent(); self && h.speaker != self && Lightning::parrotNow(self))
		creatureSays(self, (int)c->getStage(), wordUtterance(self, c, word));
	bool knew = c->vocab[word] >= WORD_KNOWN;
	int v = c->vocab[word] + (int)std::lround(amount * stageLearning((int)c->getStage()) * Lightning::learnFactor(c) * (1.0f - 0.9f * BrainOrgan::daze(c)) * BrainOrgan::learning(c));
	c->vocab[word] = (unsigned char)std::min(v, 255);
	if (!knew && c->vocab[word] >= WORD_KNOWN)
		BrainOrgan::learnedWord(c); // (a new word: its brain's Reserve)
	if (!knew && c->vocab[word] >= WORD_KNOWN) {
		int n = wordsKnown(c);
		std::string mon = world.history->findMoniker(c->getGenome());
		if (n == 1)
			world.history->tell(mon, storyEntry::firstWord, "", trim(engine.wordlist[word]));
		else if (n == 10 || n == 25 || n == 50)
			world.history->tell(mon, storyEntry::words, "", std::to_string(n));
		if (watching())
			fmt::print("* LEARNS \"{}\" ({} words known)\n", trim(engine.wordlist[word]), n);
	}
}

int ownNameMeans(oldCreature* c, const std::string& word) {
	if (!c)
		return -1;
	auto it = c->ownNames.find(word);
	if (it == c->ownNames.end())
		return -1;
	int best = -1, most = WORD_KNOWN - 1;
	for (auto& [category, v] : it->second)
		if (v > most)
			most = v, best = category;
	return best;
}

std::string ownNameFor(oldCreature* c, int category) {
	std::string best;
	int most = WORD_KNOWN - 1;
	if (c)
		for (auto& [word, for_] : c->ownNames)
			if (auto it = for_.find(category); it != for_.end() && it->second > most && ownNameMeans(c, word) == category)
				most = it->second, best = word;
	return best;
}

void hearOwnName(oldCreature* c, const std::string& word, int category, int amount) {
	const size_t mostNames = 40;
	if (!c || word.empty() || category < 1 || category > 35 || !c->isAlive() || c->isAsleep())
		return;
	bool knew = ownNameMeans(c, word) == category;
	int more = (int)std::lround(amount * stageLearning((int)c->getStage()));
	int& v = c->ownNames[word][category];
	v = std::min(255, v + more);
	// a word comes to mean the one thing, and a thing to have the one name:
	// what it meant before, and the thing's other names, grow fainter
	// (experimental: the young keep to one name, the commonest they hear:
	// Hudson Kam & Newport 2005)
	const int fainter = Language::experimental() && (int)c->getStage() <= 2 ? more : more / 2;
	for (auto& [w, for_] : c->ownNames)
		for (auto& [cat, other] : for_)
			if ((w == word) != (cat == category))
				other = std::max(0, other - fainter);
	if (c->ownNames.size() > mostNames) {
		auto least = c->ownNames.end();
		int fewest = 1 << 30;
		for (auto it = c->ownNames.begin(); it != c->ownNames.end(); ++it) {
			int most = 0;
			for (auto& [cat, n] : it->second)
				most = std::max(most, n);
			if (it->first != word && most < fewest)
				fewest = most, least = it;
		}
		if (least != c->ownNames.end())
			c->ownNames.erase(least);
	}
	if (!knew && ownNameMeans(c, word) == category) {
		if (wordsKnown(c) == 0 && ownNamesKnown(c) == 1)
			world.history->tell(world.history->findMoniker(c->getGenome()), storyEntry::firstWord, "", word);
		if (watching())
			fmt::print("* LEARNS the name \"{}\" for category {}\n", word, category);
	}
}

int ownNamesKnown(oldCreature* c) {
	int n = 0;
	if (c)
		for (auto& [word, for_] : c->ownNames)
			n += ownNameMeans(c, word) >= 0;
	return n;
}

void learnAllWords(oldCreature* c) {
	if (!c)
		return;
	c->vocab.assign(engine.wordlist.size(), 255);
	// (experimental: every word with its meaning, kept for life; filled in
	// afresh from vocab when next needed)
	c->lang.words.clear();
	world.history->tell(world.history->findMoniker(c->getGenome()), storyEntry::allWords);
	if (watching())
		fmt::print("* LEARNS every word ({})\n", wordsKnown(c));
}

void hearSpeech(Agent* speaker, const std::string& text, int amount) {
	if (!speaker || engine.version >= 3 || engine.wordlist.empty())
		return;
	// the words in the text: lower case, punctuation as spaces, each padded
	std::string t = spacedText(text);
	// (in the order said: the experimental system's young run neighbours together)
	std::vector<int> heard;
	for (auto& [at, w] : wordsSaid(t))
		heard.push_back(w);
	if (watching()) {
		std::string list;
		for (int w : heard)
			list += (list.empty() ? "" : ", ") + trim(engine.wordlist[(size_t)w]);
		fmt::print("* SAID \"{}\": words {}\n", trim(text), list.empty() ? "none" : list);
	}
	hearWords(speaker, heard, amount, t);
}

void hearWords(Agent* speaker, const std::vector<int>& heard, int amount, const std::string& text) {
	// (with no words it knows of, still heard as talk: its brain, BrainOrgan.h)
	if (!speaker || engine.version >= 3)
		return;
	amount = (int)std::lround(amount * Lightning::teachFactor(speaker)); // (a polyglot teaches faster: Lightning.h)
	float sx = centreX(speaker), sy = centreY(speaker);
	// family heard twice as well (owner, 2026-11-20: learning from parents,
	// "family most")
	oldCreature* sc = creatureOf(speaker);
	std::string from = sc ? world.history->findMoniker(sc->getGenome()) : std::string();
	static const bool noWatching = devenv("OPENC2E_NO_WATCHING") != nullptr; // (to compare)
	// (experimental: what the speaker shows or looks at, and at whom)
	Agent* about = nullptr;
	Agent* speakerFocus = sc ? sc->getAttentionFocus().get() : nullptr;
	if (Language::experimental()) {
		if (speaker == (Agent*)world.hand())
			about = handShows();
		else if (sc)
			about = speakerFocus && !speakerFocus->isDying() ? speakerFocus : nullptr;
	}
	for (auto& a : world.agents) {
		if (!a || a.get() == speaker || a->isDying())
			continue;
		oldCreature* c = creatureOf(a.get());
		if (!c || !inEarshot(a.get(), sx, sy))
			continue;
		int got = amount;
		// said to it (the hand: naming it, or close by; a creature: looking
		// at it), for its brain: a conversation's turns (BrainOrgan.h)
		bool directed;
		if (speaker == (Agent*)world.hand()) {
			std::string mon = world.history->findMoniker(c->getGenome());
			std::string name = mon.empty() || !world.history->hasMoniker(mon) ? std::string() : lower(trim(world.history->getMoniker(mon).name));
			const bool named = name.size() >= 2 && text.find(" " + name + " ") != std::string::npos;
			const bool close = std::hypot(centreX(a.get()) - world.hand()->pointerX(), centreY(a.get()) - world.hand()->pointerY()) < 200;
			directed = named || close;
		} else
			directed = speakerFocus == a.get();
		BrainOrgan::spokenTo(c, speaker, directed);
		if (heard.empty())
			continue;
		const bool kin = !noWatching && !from.empty() && closeKin(from, world.history->findMoniker(c->getGenome()));
		if (Language::experimental()) {
			// who it was said to, and about what (Language::weightFor)
			Language::Hearing h;
			h.speaker = speaker;
			h.kin = kin;
			if (speaker == (Agent*)world.hand()) {
				h.source = Language::FROM_HAND;
				h.about = about;
				// spoken to: named, or the hand close by
				h.directed = directed;
			} else {
				h.source = Language::FROM_CREATURE;
				h.about = about;
				h.directed = directed;
			}
			for (int w : heard)
				hearWord(c, w, got, h);
			Language::heardInOrder(c, heard);
			continue;
		}
		if (kin)
			got = amount * 2;
		for (int w : heard)
			hearWord(c, w, got);
	}
}

void hearMachineWord(Agent* machine, int word, bool aloud) {
	if (!machine || engine.version >= 3 || !isWord(word))
		return;
	float mx = centreX(machine), my = centreY(machine);
	// the machine's edges (a teaching computer is 157 px wide)
	float left = machine->x, right = machine->x + (machine->part(0) ? machine->getWidth() : 0.0f);
	float top = machine->y, bottom = machine->y + (machine->part(0) ? machine->getHeight() : 0.0f);
	for (auto& a : world.agents) {
		if (!a || a->isDying())
			continue;
		oldCreature* c = creatureOf(a.get());
		if (!c || !c->isAlive() || c->isAsleep())
			continue;
		float cx = centreX(a.get()), cy = centreY(a.get());
		float dx = std::max({0.0f, left - cx, cx - right}), dy = std::max({0.0f, top - cy, cy - bottom});
		bool looking = c->getAttentionFocus() == AgentRef(machine) || (dx <= 120 && dy <= 80);
		if (looking) {
			Language::Hearing h;
			h.speaker = machine;
			h.source = Language::FROM_MACHINE;
			hearWord(c, word, HEAR_MACHINE, h);
			// being taught, it says the word back: in its own tongue until it
			// has learned it, then the word itself (as C2's creatures repeat
			// what the machine shows)
			int stage = (int)c->getStage();
			if (stage >= 1 || (babyWords() && knowsWord(c, word))) {
				Utterance u;
				u.spoken = trim(engine.wordlist[word]);
				u.shown = knowsWord(c, word) ? u.spoken : attemptWord(a.get(), c, word);
				if (knowsWord(c, word))
					u.learned.push_back(word);
				creatureSays(a.get(), stage, u);
			}
		} else if (aloud && inEarshot(a.get(), mx, my)) {
			Language::Hearing h;
			h.speaker = machine;
			h.source = Language::FROM_MACHINE_FAR;
			hearWord(c, word, HEAR_MACHINE_FAR, h);
		}
	}
}

Utterance wordUtterance(Agent* creature, oldCreature* c, int word) {
	Utterance u;
	if (!isWord(word))
		return u;
	u.spoken = trim(engine.wordlist[word]);
	u.shown = showWord(creature, c, word);
	u.learned.push_back(word);
	return u;
}

Utterance phraseFor(const std::string& meaning, Agent* creature, oldCreature* c) {
	Utterance u;
	if (!creature || !c)
		return u;
	std::vector<std::string> p = parts(meaning);
	const std::string& kind = p[0];
	int a = p.size() > 1 ? atoi(p[1].c_str()) : 0;
	int b = p.size() > 2 ? atoi(p[2].c_str()) : 0;
	int stage = (int)c->getStage();
	std::string trail = stage >= 6 ? "..." : "";

	// says the given words: learned ones as words, the rest in its own tongue
	auto say = [&](const std::vector<int>& words, const std::string& end) {
		if (Language::experimental()) {
			// things by what it takes its words to mean ("toy?" for a ball,
			// a word borrowed from a like thing, or one made up), and the
			// young run words heard together into one
			for (size_t i = 0; i < words.size(); i++) {
				int w = words[i];
				if (!isWord(w))
					continue;
				std::string shown, spoken;
				if (w > 16 && w < 56) {
					Language::Naming n = Language::nameFor(creature, c, w - 16, true);
					if (!n.own.empty()) {
						shown = spoken = n.own;
						if (n.unsure)
							shown += "?";
						u.own.push_back({n.own, w - 16});
					} else if (n.word >= 0) {
						spoken = trim(engine.wordlist[n.word]);
						shown = spoken + (n.unsure ? "?" : "");
						u.learned.push_back(n.word);
					} else {
						spoken = trim(engine.wordlist[w]);
						shown = attemptWord(creature, c, w);
					}
				} else if (int partner; knowsWord(c, w) && !Language::runTogether(c, w, partner).empty() && i + 1 < words.size() && words[i + 1] == partner) {
					shown = spoken = Language::runTogether(c, w, partner);
					u.learned.push_back(w);
					u.learned.push_back(partner);
					i++;
				} else if (knowsWord(c, w)) {
					shown = spoken = trim(engine.wordlist[w]);
					u.learned.push_back(w);
				} else {
					spoken = trim(engine.wordlist[w]);
					shown = attemptWord(creature, c, w);
				}
				if (shown.empty())
					continue;
				u.shown += (u.shown.empty() ? "" : " ") + shown;
				u.spoken += (u.spoken.empty() ? "" : " ") + spoken;
			}
			// (unsure of its last word: the question mark is the ending)
			if (!u.shown.empty() && !(u.shown.back() == '?' && trail.empty()))
				u.shown += trail.empty() ? end : trail;
			return;
		}
		for (int w : words) {
			if (!isWord(w))
				continue;
			std::string shown = showWord(creature, c, w);
			if (shown.empty())
				continue;
			u.shown += (u.shown.empty() ? "" : " ") + shown;
			std::string own = w >= 16 && w < 56 ? ownNameFor(c, w - 16) : std::string();
			u.spoken += (u.spoken.empty() ? "" : " ") + (own.empty() ? trim(engine.wordlist[w]) : own);
			if (!own.empty())
				u.own.push_back({own, w - 16});
			else if (knowsWord(c, w))
				u.learned.push_back(w);
		}
		if (!u.shown.empty())
			u.shown += trail.empty() ? end : trail;
	};

	// babies: cries, and the sounds of their own tongue
	auto babySounds = [&]() {
		Utterance b;
		if (kind == "ow") {
			b.shown = b.spoken = rand_float(0, 1) < 0.5f ? "waah!" : "eee!";
			return b;
		}
		if (kind == "yum" || kind == "love") {
			b.shown = b.spoken = "mmm";
			return b;
		}
		if (kind == "grr") {
			b.shown = b.spoken = "nnngh!";
			return b;
		}
		// (experimental: babble, doubled syllables, more when spoken to)
		if (Language::experimental()) {
			b.shown = b.spoken = Language::babble(creature, c);
			return b;
		}
		// one short word of its tongue, not yet meaning anything
		static const int shortwords[] = {W_ME, W_EAT, W_HIT, W_YES, W_NO, 6, 5, 8, 21, 29, 34, 57, 60};
		int w = shortwords[rand_size_t(0, sizeof(shortwords) / sizeof(int) - 1)];
		if (isWord(w)) {
			b.spoken = trim(engine.wordlist[w]);
			b.shown = nativeWord(creature, b.spoken);
		}
		return b;
	};
	// With first words (babyWords), a baby that knows the word for what it
	// means says that one word (Fenson et al. 1994: first words come about
	// the first birthday, one at a time); not knowing it, or half the time
	// when hurt, pleased or cross, it cries and babbles as before.
	const bool firstWords = stage <= 0 && babyWords();
	if (stage <= 0 && (!firstWords || ((kind == "ow" || kind == "yum" || kind == "love" || kind == "grr") && rand_float(0, 1) < 0.5f)))
		return babySounds();

	std::vector<Wording> ways;
	std::string end;
	if (kind == "need") {
		if (a < 0 || a > 16)
			return u;
		std::vector<int> w = {W_ME};
		// how strongly, past 200 of 255: very, really, seriously, intensely,
		// extremely (C2's words for it, after "very"), the strongest it
		// knows up to that; knowing none, "very" in its own tongue
		if (b > 200) {
			static const int above[5] = {200, 212, 225, 235, 245};
			int graded = 0;
			while (graded < 4 && b > above[graded + 1])
				graded++;
			int q = W_VERY;
			for (int i = graded; i >= 0; i--)
				if (knowsWord(c, W_VERY + i)) {
					q = W_VERY + i;
					break;
				}
			w.push_back(q);
		}
		w.push_back(W_DRIVE0 + a);
		ways.push_back({w, (int)w.size() - 1});
		end = b > 200 ? "!" : "";
	} else if (kind == "ow") {
		ways = {{{W_PAIN}, 0}, {{W_HURT}, 0}, {{W_ME, W_HURT}, 1}};
		end = "!";
	} else if (kind == "yum") {
		ways = {{{W_FOOD}, 0}, {{W_EAT, W_FOOD}, 1}, {{W_YES, W_FOOD}, 1}};
		end = "!";
	} else if (kind == "grr") {
		ways = {{{W_ME, W_ANGRY}, 1}, {{W_NO}, 0}, {{W_HIT}, 0}};
		end = "!";
	} else if (kind == "love") {
		ways = {{{W_FRIENDLY}, 0}, {{W_YES}, 0}, {{W_ME, W_FRIENDLY}, 1}};
	} else if (kind == "greet") {
		if (a < 1 || a > 3)
			return u;
		ways = {{{W_NORN + a - 1}, 0}, {{W_LOOK, W_NORN + a - 1}, 1}};
		end = "!";
	} else if (kind == "doing") {
		// what it is doing, in words: "me push toy" (asked "what")
		int decision = c->getDecisionId();
		Agent* focus = c->getAttentionFocus().get();
		if (focus == creature || (focus && focus->isDying()))
			focus = nullptr;
		if (decision == 7 && creature->carrying)
			focus = creature->carrying.get(); // (dropping: what it holds)
		int verb;
		if (decision == 0 || decision == 8)
			verb = focus ? W_LOOK : W_REST; // watching it, or idling
		else if (decision >= 1 && decision <= 13)
			verb = decision; // C2's verbs are its decisions' words
		else if (decision == 25)
			verb = W_REST;
		else if (decision == 26 || decision == 27)
			verb = decision - 16; // walking west or east
		else
			verb = W_LOOK;
		std::vector<int> w = {W_ME, verb};
		if (focus && focus->category >= 0 && focus->category < 40 && isWord(16 + focus->category))
			w.push_back(16 + focus->category);
		ways.push_back({w, 1});
	} else if (kind == "name") {
		// a thing's name (owner, 2026-11-20: the young learn words by hearing
		// others name things): C2's noun for category a follows its 16 verbs
		int w = 16 + a;
		if (a < 0 || a > 39 || !isWord(w))
			return u;
		ways = {{{w}, 0}, {{W_LOOK, w}, 1}};
		end = "!";
	} else {
		// nothing in particular: trying out a word it knows, or chatter in
		// its own tongue
		std::vector<int> known;
		for (size_t w = 0; w < c->vocab.size(); w++)
			if (knowsWord(c, (int)w) && isWord((int)w))
				known.push_back((int)w);
		// or a name of its own for a thing (so names pass on)
		std::vector<int> named;
		for (int category = 1; category <= 35; category++)
			if (!ownNameFor(c, category).empty() && isWord(16 + category))
				named.push_back(16 + category);
		if (!named.empty() && rand_float(0, 1) < 0.25f) {
			say({named[rand_size_t(0, named.size() - 1)]}, "?");
			return u;
		}
		if (!known.empty() && rand_float(0, 1) < 0.35f) {
			say({known[rand_size_t(0, known.size() - 1)]}, "?");
			return u;
		}
		if (firstWords)
			return babySounds();
		// chatter: words it has not learned, in its own tongue. Picked from
		// the words it doesn't know; before, it drew at random until it had
		// enough unknown ones, which never ended for a creature that knows
		// every word (the Stone of Knowledge teaches them all) and froze the
		// game (2026-11-12, two force-quits). Knowing them all, it tries out
		// a word it knows instead.
		std::vector<int> unknown;
		for (size_t w = 0; w < engine.wordlist.size(); w++)
			if (isWord((int)w) && !knowsWord(c, (int)w))
				unknown.push_back((int)w);
		if (unknown.empty()) {
			if (!known.empty())
				say({known[rand_size_t(0, known.size() - 1)]}, "?");
			return u;
		}
		std::vector<int> chatter;
		int n = (int)rand_size_t(1, stage == 1 ? 1 : 3);
		for (int i = 0; i < n; i++)
			chatter.push_back(unknown[rand_size_t(0, unknown.size() - 1)]);
		say(chatter, "");
		return u;
	}

	// the wording it knows most of (ties at random)
	int best = 0, bestscore = -1;
	for (size_t i = 0; i < ways.size(); i++) {
		int score = 0;
		for (size_t j = 0; j < ways[i].words.size(); j++)
			score += knowsWord(c, ways[i].words[j]) ? ((int)j == ways[i].key ? 4 : 1) : 0;
		score = score * 8 + (int)rand_size_t(0, 7);
		if (score > bestscore) {
			bestscore = score;
			best = (int)i;
		}
	}
	const Wording& way = ways[best];
	// a baby: the one word, if it knows it
	if (firstWords) {
		// (or its try at it, nearly known: "han-nah-do")
		const int key = way.words[way.key];
		if (!knowsWord(c, key) && !(wordAttempts() && key >= 0 && (size_t)key < c->vocab.size() && c->vocab[key] >= ATTEMPT_START))
			return babySounds();
		say({way.words[way.key]}, end);
		return u;
	}
	if (Language::experimental()) {
		// as long as its words allow (Language::phraseLimit): the strength
		// words go first, then "me", then the rest but the key word; a verb
		// joins other words only once it has been said alone a few times
		// (Tomasello 2000: verb islands)
		const int known = wordsKnown(c);
		std::vector<int> words = way.words;
		const int key = words[way.key];
		if (!Language::qualifiersAllowed(stage, known))
			words.erase(std::remove_if(words.begin(), words.end(), [&](int w) { return w >= W_VERY && w <= W_VERY + 4 && w != key; }), words.end());
		const size_t limit = (size_t)Language::phraseLimit(stage, known);
		while (words.size() > limit) {
			auto it = std::find(words.begin(), words.end(), W_ME);
			if (it == words.end() || *it == key)
				it = std::find_if(words.begin(), words.end(), [&](int w) { return w != key; });
			if (it == words.end())
				break;
			words.erase(it);
		}
		if (words.size() > 1)
			for (int w : words)
				if (((w >= 0 && w <= 13) || w == W_LOOK) && knowsWord(c, w) && Language::memoryOf(c, w).uses < 3) {
					words = {key};
					break;
				}
		say(words, end);
		return u;
	}
	if (stage == 1)
		say({way.words[way.key]}, end); // children: the one word
	else
		say(way.words, end);
	return u;
}

float voicePitch(int stage) {
	static const float pitch[7] = {1.4f, 1.25f, 1.1f, 1.05f, 1.0f, 0.92f, 0.88f};
	return (stage >= 0 && stage < 7) ? pitch[stage] : 1.0f;
}

float voicePace(int stage) {
	static const float pace[7] = {0.8f, 1.0f, 1.1f, 1.15f, 1.0f, 0.85f, 0.7f};
	return (stage >= 0 && stage < 7) ? pace[stage] : 1.0f;
}

namespace {
std::deque<SpokenLine> s_history;
}

const std::deque<SpokenLine>& speechHistory() {
	return s_history;
}

void recordSpeech(Agent* speaker, const std::string& text) {
	if (!speaker || text.empty())
		return;
	s_history.push_front({AgentRef(speaker), text, world.worldtickcount});
	while (s_history.size() > 40)
		s_history.pop_back();
}

void handSays(const std::string& text) {
	Agent* hand = (Agent*)world.hand();
	if (!hand || text.empty() || engine.version >= 3)
		return;
	Bubble* b = Bubble::newBubble(hand, true, text);
	b->setTimeout(35);
	hearSpeech(hand, text, HEAR_HAND);
	recordSpeech(hand, text);
	answerHand(text);
}

void handSaysAgain() {
	for (auto& line : s_history)
		if (line.speaker && line.speaker.get() == (Agent*)world.hand()) {
			std::string text = line.text;
			handSays(text);
			return;
		}
}

// Being spoken to (a player, 2026-11-22: "the baby not looking at me, and
// there being no response to anything I say"; C2's manual has norns look,
// repeat and obey). Words are learned in hearWords, before this.
// what the hand is showing: what it holds, or points at (the smallest thing
// there: creatures in front, and scenery, don't count)
Agent* handShows() {
	Agent* hand = (Agent*)world.hand();
	if (!hand)
		return nullptr;
	Agent* shown = hand->carrying.get();
	if (!shown) {
		const float px = world.hand()->pointerX(), py = world.hand()->pointerY();
		float least = 1e9f;
		for (auto& a : world.agents) {
			if (!a || a->isDying() || a->category < 2 || a->category > 35 || a->invisible() || a->carriedby || !a->part(0))
				continue;
			const float w = a->getWidth(), h = a->getHeight();
			if (px >= a->x - 10 && px <= a->x + w + 10 && py >= a->y - 10 && py <= a->y + h + 10 && w * h < least)
				least = w * h, shown = a.get();
		}
	}
	return shown;
}

void answerHand(const std::string& text) {
	Agent* hand = (Agent*)world.hand();
	if (!hand || engine.version != 2 || engine.wordlist.empty())
		return;
	std::string t = spacedText(text);
	// the list's words in it, in the order said
	std::vector<std::pair<size_t, int>> said = wordsSaid(t);
	// the words of it, and a name it may be teaching: the one word of at
	// least two letters that is no word of the list, no creature's name and
	// no everyday filler
	std::vector<std::string> tokens;
	{
		std::istringstream in(t);
		for (std::string w; in >> w;)
			tokens.push_back(w);
	}
	auto listed = [&](const std::string& w) {
		for (auto& l : engine.wordlist)
			if (lower(trim(l)) == w)
				return true;
		for (auto& l : WordLists::english())
			if (lower(l) == w)
				return true;
		return false;
	};
	std::set<std::string> creatureNames;
	for (auto& a : world.agents)
		if (oldCreature* c = a && a->family == 4 ? creatureOf(a.get()) : nullptr) {
			std::string mon = world.history->findMoniker(c->getGenome());
			if (!mon.empty() && world.history->hasMoniker(mon)) {
				std::istringstream in(lower(world.history->getMoniker(mon).name));
				for (std::string w; in >> w;)
					creatureNames.insert(w);
			}
		}
	static const std::set<std::string> filler = {"hello", "hi", "hey", "good", "bad", "please", "thanks", "thank", "the", "an", "is", "it", "its",
		"this", "that", "my", "you", "your", "and", "to", "for", "of", "in", "on", "at", "am", "are", "do", "dont", "not", "ok", "okay", "norn", "norns"};
	std::string naming;
	int strange = 0;
	for (auto& w : tokens)
		if (w.size() >= 2 && std::isalpha((unsigned char)w[0]) && !listed(w) && !creatureNames.count(w) && !filler.count(w)) {
			naming = w;
			strange++;
		}
	if (strange != 1)
		naming.clear();

	struct Listener {
		Agent* a;
		oldCreature* c;
		float distance;
		bool named;
	};
	std::vector<Listener> all, named;
	const float hx = centreX(hand), hy = centreY(hand);
	for (auto& a : world.agents) {
		if (!a || a->isDying())
			continue;
		oldCreature* c = creatureOf(a.get());
		if (!c || !c->isAlive() || c->isAsleep() || c->isUnconscious() || !inEarshot(a.get(), hx, hy))
			continue;
		Listener l{a.get(), c, std::hypot(centreX(a.get()) - hx, centreY(a.get()) - hy), false};
		std::string mon = world.history->findMoniker(c->getGenome());
		std::string name = mon.empty() || !world.history->hasMoniker(mon) ? std::string() : lower(trim(world.history->getMoniker(mon).name));
		if (name.size() >= 2 && t.find(" " + name + " ") != std::string::npos) {
			l.named = true;
			named.push_back(l);
		}
		all.push_back(l);
	}
	// spoken to by name: only they are meant
	std::vector<Listener>& meant = named.empty() ? all : named;
	std::sort(meant.begin(), meant.end(), [](const Listener& a, const Listener& b) { return a.distance < b.distance; });
	// what the hand is showing: what it holds, or points at
	// (the smallest thing there: creatures in front, and scenery, don't count)
	Agent* shown = handShows();
	const int shownCategory = shown && shown != hand && shown->category >= 2 && shown->category <= 35 ? shown->category : -1;
	int rank = 0;
	for (auto& l : meant) {
		oldCreature::Spoken s;
		int named = -1; // the thing it takes a strange name to be for
		// names of its own it knows
		for (auto& w : tokens)
			if (int own = ownNameMeans(l.c, w); own >= 0)
				s.noun = own;
		if (!naming.empty()) {
			const unsigned int now = world.tickcount;
			auto lately = [&](unsigned int then, unsigned int within) { return then != 0 && now >= then && now - then < within; };
			if (ownNameMeans(l.c, naming) >= 0 && shownCategory < 0 && !lately(l.c->touchedTick, 100))
				named = -1; // (known, and nothing new shown: simply understood)
			else if (l.c->namingWord == naming && lately(l.c->namingTick, 150))
				named = l.c->namingCategory; // the same again
			else if (lately(l.c->touchedTick, 100))
				named = 1; // just touched by the hand: the hand
			else if (shownCategory >= 0)
				named = shownCategory;
			else if (Agent* it = l.c->getAttentionFocus().get(); it && it->category >= 2 && it->category <= 35 && std::hypot(centreX(it) - hx, centreY(it) - hy) < 90)
				named = it->category; // what it is looking at, the hand beside it
			else
				named = 1;
			if (named >= 0) {
				hearOwnName(l.c, naming, named, HEAR_HAND);
				l.c->namingWord = naming;
				l.c->namingCategory = named;
				l.c->namingTick = std::max(1u, now);
				s.noun = named;
			}
		}
		int praise = 0; // "yes" or "no", the last said (a word it knows)
		int question = 0; // "what" or "why", the last asked
		bool look = false;
		int understood = s.noun >= 0 ? 1 : 0; // words it took in (a name of its own, or one being taught, counts)
		for (auto& [at, w] : said) {
			// (experimental: understood before it can be said; off: known)
			if (!Language::understands(l.c, w))
				continue;
			understood++;
			if (w == W_LOOK)
				look = true;
			else if (w == W_WHAT || w == W_WHY)
				question = w == W_WHAT ? 1 : 2; // (a question: "why" is among C2's verbs, but no order)
			else if (w <= 13)
				s.verb = w;
			else if (w >= 16 && w < 16 + 40) {
				// the noun's category (experimental: the kind it takes the word
				// to mean, which may be wrong, and only the one thing it heard
				// it for if only one)
				int means = Language::meaningOf(l.c, w);
				if (means >= 0 && !Language::tooNarrow(l.c, w, shown))
					s.noun = means;
				else
					understood--;
			} else if (w == W_YES || w == W_NO)
				praise = w == W_YES ? 1 : -1;
		}
		// "yes" praises it and "no" blames it, as a pat or slap does, less
		// strongly (C2's manual: "Reward a creature", "Punish a creature")
		if (praise != 0) {
			l.c->hearPraise(praise > 0);
			if (watching())
				fmt::print("* HEARS \"{}\": {} {} {}\n", praise > 0 ? "yes" : "no", (int)l.a->family, (int)l.a->genus, l.a->getUNID());
		}
		s.named = l.named;
		s.tick = world.tickcount;
		l.c->spoken = s;
		// told to "look": at the thing the hand holds or points at, else the
		// one nearest the hand (of the kind named, if one is); it looks
		// round at it and names it, as the manual has it, "an easy way of
		// teaching your Norn the names of objects"
		Agent* lookAt = nullptr;
		if (look) {
			if (shown && shown != hand && shown != l.a && (s.noun < 0 || shown->category == s.noun))
				lookAt = shown;
			else {
				const float px = world.hand()->pointerX(), py = world.hand()->pointerY();
				float least = 200.0f;
				for (auto& a : world.agents) {
					if (!a || a.get() == l.a || a.get() == hand || a->isDying() || a->category < 2 || a->category > 39 || a->invisible() || !a->part(0) ||
						(a->carriedby && a->carriedby.get() != hand) || (s.noun >= 0 && a->category != s.noun))
						continue;
					float d = std::hypot(centreX(a.get()) - px, centreY(a.get()) - py);
					if (d < least)
						least = d, lookAt = a.get();
				}
			}
		}
		l.c->callTo(lookAt ? lookAt->category : s.noun >= 0 ? s.noun : 1); // the thing, or the hand
		if (lookAt)
			l.c->lookAt = AgentRef(lookAt);
		if (look && watching())
			fmt::print("* LOOK unid {}: at {}\n", l.a->getUNID(), lookAt ? fmt::format("{} {} {} (category {})", (int)lookAt->family, (int)lookAt->genus, lookAt->species, lookAt->category) : s.noun >= 0 ? fmt::format("category {}, none near the hand", s.noun) : std::string("the hand"));
		// the nearest two (or those named) say it back
		if (rank < 2 || l.named) {
			l.c->replyQuestion = question ? question : lookAt ? 3 : 0;
			// (experimental: understanding none of it, it shows it: "?", which
			// is not "no", so a puzzled creature is told from an unwilling one)
			if (Language::experimental() && understood <= 0 && !said.empty() && l.c->replyQuestion == 0 && naming.empty())
				l.c->replyQuestion = 4;
			l.c->replyWord = said.empty() ? -1 : said.back().second;
			// (a name it is being taught, or one of its own, over the rest)
			l.c->replyName = naming;
			if (naming.empty())
				for (auto& w : tokens)
					if (ownNameMeans(l.c, w) >= 0)
						l.c->replyName = w;
			l.c->replyAt = world.tickcount + 8 + 9 * rank + rand_int32(0, 5);
		}
		rank++;
	}
	if (watching())
		fmt::print("* HAND SAYS \"{}\": {} listening{}\n", text, meant.size(), named.empty() ? "" : " (named)");
}

void tickReply(Agent* creature, oldCreature* c) {
	if (c->replyAt == 0 || world.tickcount < c->replyAt)
		return;
	c->replyAt = 0;
	if (!c->isAlive() || c->isAsleep() || c->isUnconscious())
		return;
	int stage = (int)c->getStage();
	// asked what it is doing, or why: it says so (C2's manual: "You can
	// find out what your Norn is thinking and why!")
	if (c->replyQuestion != 0) {
		int q = c->replyQuestion;
		c->replyQuestion = 0;
		c->replyName.clear();
		Utterance u;
		if (q == 4) {
			u.shown = "?";
			u.spoken = "";
		} else if (q == 1)
			u = phraseFor("doing", creature, c);
		else if (q == 3)
			u = phraseFor(fmt::format("name:{}", c->calledCategory), creature, c); // (its noun)
		else {
			// the need its last choice was most for (the mind's view of
			// that choice: what it expected each need to ease); a choice
			// made for no need but the untried is boredom; failing a
			// view, its strongest need
			int need = -1;
			MindView v;
			if (CreatureMind* m = c->getMind(); m && m->view(v))
				for (auto& o : v.options)
					if (o.chosen) {
						float most = 0.02f;
						for (int i = 0; i < 17; i++)
							if (o.ease[i] > most)
								most = o.ease[i], need = i;
						if (need < 0 && o.novelty > 0)
							need = W_BORED - W_DRIVE0;
						break;
					}
			if (need < 0) {
				int most = 0;
				for (int i = 0; i < 17; i++)
					if ((int)c->getDrive(i) > most)
						most = (int)c->getDrive(i), need = i;
			}
			u = need < 0 ? phraseFor("babble", creature, c) : phraseFor(fmt::format("need:{}:{}", need, (int)c->getDrive(need)), creature, c);
		}
		if (watching())
			fmt::print("* ASKED \"{}\": unid {} decision {}\n", q == 1 ? "what" : q == 2 ? "why" : q == 3 ? "look" : "?", creature->getUNID(), c->getDecisionId());
		creatureSays(creature, stage, u);
		return;
	}
	if (!c->replyName.empty()) {
		Utterance u;
		u.spoken = c->replyName;
		int category = ownNameMeans(c, c->replyName);
		bool knows = (stage > 0 || babyWords()) && category >= 0;
		u.shown = knows ? u.spoken : nativeWord(creature, u.spoken);
		if (knows)
			u.own.push_back({c->replyName, category});
		c->replyName.clear();
		creatureSays(creature, stage, u);
		return;
	}
	if (c->replyWord < 0 || !isWord(c->replyWord)) {
		creatureSays(creature, stage, phraseFor("babble", creature, c));
		return;
	}
	// in its own tongue until it has learned the word (a baby always, unless
	// it says its first words: babyWords)
	Utterance u;
	u.spoken = trim(engine.wordlist[c->replyWord]);
	bool knows = (stage > 0 || babyWords()) && knowsWord(c, c->replyWord);
	u.shown = knows ? u.spoken : stage > 0 || babyWords() ? attemptWord(creature, c, c->replyWord) : nativeWord(creature, u.spoken);
	if (knows)
		u.learned.push_back(c->replyWord);
	creatureSays(creature, stage, u);
}

void creatureSays(Agent* creature, int stage, const Utterance& said) {
	if (!creature || said.shown.empty() || engine.version >= 3)
		return;
	if (oldCreature* sc = creatureOf(creature))
		BrainOrgan::spoke(sc); // (an answer makes a turn: BrainOrgan.h)
	Utterance u = said;
	// struck by lightning (Lightning.h): each word backwards ("dnah!")
	if (Lightning::backwards(creature)) {
		auto reverseWords = [](const std::string& t) {
			std::string out, word;
			auto flush = [&]() {
				std::reverse(word.begin(), word.end());
				out += word;
				word.clear();
			};
			for (char ch : t) {
				if (std::isalpha((unsigned char)ch) || ch == '-' || ch == '\'')
					word += ch;
				else {
					flush();
					out += ch;
				}
			}
			flush();
			return out;
		};
		u.shown = reverseWords(u.shown);
		u.spoken = reverseWords(u.spoken);
	}
	oldCreature* self = creatureOf(creature);
	if (Language::experimental() && self) {
		// the old reach for a word now and then; saying words is practice
		// (the pause comes before the last word, the one reached for)
		if (u.shown != "?" && Language::searchingForWord(self)) {
			size_t last = u.shown.rfind(' ');
			u.shown = last == std::string::npos ? "...um... " + u.shown : u.shown.substr(0, last + 1) + "...um... " + u.shown.substr(last + 1);
		}
		for (int w : u.learned)
			Language::said(self, w);
		// a name it tries out, it comes to know by using it
		for (auto& [word, category] : u.own)
			hearOwnName(self, word, category, 12);
	}
	// one bubble at a time for each speaker (none with View > Speech
	// Bubbles off, or while it rides out of sight)
	auto it = bubbles.find(creature->getUNID());
	if (it != bubbles.end() && it->second)
		it->second->kill();
	if (speechBubblesShown() && !Bubble::hiddenRider(creature)) {
		Bubble* b = Bubble::newBubble(creature, true, u.shown);
		b->setTimeout(35); // 3.5 s
		bubbles[creature->getUNID()] = AgentRef(b);
	}
	hearWords(creature, u.learned, HEAR_CREATURE);
	// and its own names for things: names spread, family hearing them twice
	// as well
	if (!u.own.empty()) {
		std::string from = world.history->findMoniker(creatureOf(creature)->getGenome());
		float sx = centreX(creature), sy = centreY(creature);
		for (auto& a : world.agents) {
			oldCreature* c = a && a.get() != creature && !a->isDying() ? creatureOf(a.get()) : nullptr;
			if (!c || !inEarshot(a.get(), sx, sy))
				continue;
			int amount = HEAR_CREATURE * (!from.empty() && closeKin(from, world.history->findMoniker(c->getGenome())) ? 2 : 1);
			for (auto& [word, category] : u.own) {
				// (experimental: a listener that already calls it so agrees,
				// and both drop their other names for it: the naming game)
				bool agrees = Language::experimental() && ownNameFor(c, category) == word;
				hearOwnName(c, word, category, amount);
				if (agrees) {
					Language::agreed(c, word, category);
					Language::agreed(self, word, category);
				}
			}
		}
	}
	recordSpeech(creature, u.shown);
	try {
		creature->speak(u.spoken.empty() ? u.shown : u.spoken, voicePitch(stage) * Lightning::pitchFactor(creature), voicePace(stage));
	} catch (std::exception&) {
		// no voice for this creature: the bubble alone
	}
	if (watching())
		fmt::print("* SAYS {} {} {} (stage {}): \"{}\" ({}){}\n", (int)creature->family, (int)creature->genus, creature->getUNID(), stage, u.shown, u.spoken, Bubble::hiddenRider(creature) ? " out of sight, no bubble" : "");
}
