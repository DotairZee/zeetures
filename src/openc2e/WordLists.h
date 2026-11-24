#pragma once

#include <filesystem>
#include <string>
#include <vector>

// The words creatures learn and the teaching computers show (C2's word list,
// engine.wordlist: 82 slots, each with its meaning; owner, 2026-11-23). Each
// world can have its own (words.txt in its folder), or one list can be used
// in every world (the player's words.txt, with "words-all-worlds 1" in
// settings.txt); with neither, the game's own in the player's language.
// Changed in Preferences > Words, which can also fill it from another world
// or a preset: the game's own languages (read from the player's copy of the
// game), Nornish, and Zeetures's own draft translations (data/words).
//
// A slot keeps its meaning whatever it is called, so what creatures have
// learned stays learned under new words. They still understand the game's
// English words typed by the hand.
namespace WordLists {

constexpr size_t SLOTS = 82;

struct Preset {
	std::string id; // "game:de", "nornish", "draft:ja", "world:NAME"
	std::string name; // as shown
	std::vector<std::string> words;
	bool draft = false;
};

// the game's English list (what each slot means)
const std::vector<std::string>& english();
// the presets: the game's own languages, Nornish, our drafts
const std::vector<Preset>& presets();
// other worlds that have their own words, as presets
std::vector<Preset> worldPresets();

// at the start, once the world is chosen
void applyForWorld(const std::filesystem::path& worlddir);

bool allWorlds();
void setAllWorlds(bool on);

// change the words in use (and save them where they belong)
void setWord(size_t slot, const std::string& word);
void setWords(const std::vector<std::string>& words);
// the game's own list in the player's language (what a world starts with)
std::vector<std::string> defaultWords();

// a word in another slot that is the same (lower case), else -1; the game
// matches typed words to slots, so two slots should not share one
int clash(size_t slot, const std::string& word);

// words.txt: "* comment" lines, then "N word" lines
std::vector<std::string> readFile(const std::filesystem::path& file);
bool writeFile(const std::filesystem::path& file, const std::vector<std::string>& words, const std::string& title);

} // namespace WordLists
