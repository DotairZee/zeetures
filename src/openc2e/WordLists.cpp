#include "WordLists.h"

#include "Engine.h"
#include "PathResolver.h"
#include "PixelText.h"
#include "VoiceData.h"
#include "common/io/FileReader.h"
#include "creatures/CreatureSpeech.h" // readPlayerSetting
#include "openc2eimgui/WorldChooser.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <span>
#include <sstream>

// the draft translations (data/words), built in (scripts/embed_files.py)
std::span<const uint8_t> EmbeddedWordList(const std::string& name);
std::vector<std::string> EmbeddedWordListNames();

namespace fs = std::filesystem;

namespace WordLists {

namespace {

fs::path s_worlddir;
std::vector<std::string> s_default; // the game's list in the player's language
bool s_haveDefault = false;
int s_allWorlds = -1;

fs::path playerFile() {
	return getUserDataDir() / "words.txt";
}

fs::path worldFile() {
	return s_worlddir.empty() ? fs::path() : s_worlddir / "words.txt";
}

std::vector<std::string> fitted(std::vector<std::string> w) {
	w.resize(std::max(english().size(), SLOTS));
	return w;
}

// the list in use: the engine's, and its English -> word table (the world
// file's own words are English: SFCFile)
void setLive(const std::vector<std::string>& words) {
	engine.wordlist = fitted(words);
	engine.wordlist_translations.clear();
	auto& en = english();
	for (size_t i = 0; i < en.size() && i < engine.wordlist.size(); i++)
		if (!en[i].empty() && en[i] != engine.wordlist[i])
			engine.wordlist_translations[en[i]] = engine.wordlist[i];
}

void save() {
	if (allWorlds())
		writeFile(playerFile(), engine.wordlist, "Zeetures words for every world");
	else if (!s_worlddir.empty())
		writeFile(worldFile(), engine.wordlist, "Zeetures words for the world " + engine.currentworld);
}

// a word in a creature's own tongue, as a norn says it (the game's norn
// voice, female.vce: its syllable sounds' names; as nativeWord)
std::string nornish(VoiceData& voice, const std::string& word) {
	std::string out;
	for (auto& e : voice.GetSyllablesFor(word)) {
		std::string syl = PixelText::lower(e.name.substr(0, 3));
		syl.erase(std::remove(syl.begin(), syl.end(), '_'), syl.end());
		if (!syl.empty())
			out += (out.empty() ? "" : "-") + syl;
	}
	return out;
}

} // namespace

const std::vector<std::string>& english() {
	static std::vector<std::string> en;
	static bool read = false;
	if (!read) {
		read = true;
		en = engine.gameWordList("en");
		for (auto& w : en) // (the game pads some with spaces)
			while (!w.empty() && w.back() == ' ')
				w.pop_back();
	}
	return en;
}

std::vector<std::string> defaultWords() {
	if (!s_haveDefault) {
		s_default = engine.wordlist;
		s_haveDefault = true;
	}
	return s_default;
}

const std::vector<Preset>& presets() {
	static std::vector<Preset> list;
	static bool made = false;
	if (made)
		return list;
	made = true;
	// the game's own languages, from the player's copy
	static const std::pair<const char*, const char*> game[] = {{"en", "English"}, {"de", "Deutsch (German)"}, {"fr", "Français (French)"},
		{"it", "Italiano (Italian)"}, {"nl", "Nederlands (Dutch)"}, {"es", "Español (Spanish)"}};
	for (auto& [code, name] : game) {
		std::vector<std::string> w = engine.gameWordList(code);
		if (w.empty())
			continue;
		for (auto& s : w)
			while (!s.empty() && s.back() == ' ')
				s.pop_back();
		list.push_back({std::string("game:") + code, std::string(name) + ", from the game", fitted(w)});
	}
	// Nornish: the creatures' own tongue (norn voice)
	auto vce = findMainDirectoryFile("female.vce");
	if (!vce.empty()) {
		try {
			FileReader f(vce);
			VoiceData voice(f);
			std::vector<std::string> w;
			for (auto& e : english())
				w.push_back(e.empty() ? std::string() : nornish(voice, e));
			list.push_back({"nornish", "Nornish (the creatures' own tongue)", fitted(w)});
		} catch (std::exception&) {
		}
	}
	// our drafts (data/words/CODE.txt; the first line names the language)
	std::vector<Preset> drafts;
	for (auto& name : EmbeddedWordListNames()) {
		auto bytes = EmbeddedWordList(name);
		std::string text(bytes.begin(), bytes.end());
		std::istringstream in(text);
		std::string first;
		std::getline(in, first);
		std::string title = first.size() > 2 && first[0] == '*' ? first.substr(2) : name;
		std::vector<std::string> w(SLOTS);
		std::string line;
		while (std::getline(in, line)) {
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			if (line.empty() || line[0] == '*')
				continue;
			size_t sp = line.find(' ');
			size_t n = std::strtoul(line.substr(0, sp).c_str(), nullptr, 10);
			if (n < w.size())
				w[n] = sp == std::string::npos ? std::string() : line.substr(sp + 1);
		}
		drafts.push_back({"draft:" + name.substr(0, name.find('.')), title + ", draft", fitted(w), true});
	}
	std::sort(drafts.begin(), drafts.end(), [](const Preset& a, const Preset& b) { return a.name < b.name; });
	list.insert(list.end(), drafts.begin(), drafts.end());
	return list;
}

std::vector<Preset> worldPresets() {
	std::vector<Preset> out;
	for (auto& w : Openc2eImgui::ListWorlds()) {
		if (w.dir == s_worlddir)
			continue;
		auto words = readFile(w.dir / "words.txt");
		if (!words.empty())
			out.push_back({"world:" + w.name, w.name, fitted(words)});
	}
	return out;
}

namespace {
fs::path customDir() {
	return getUserDataDir() / "word-lists";
}

// the name as a file name: letters of any alphabet kept, a few characters
// no system allows left out
fs::path customFile(const std::string& name) {
	std::string clean;
	for (char c : name)
		if (!strchr("/\\:*?\"<>|", c) && (unsigned char)c >= 0x20)
			clean += c;
	while (!clean.empty() && (clean.back() == ' ' || clean.back() == '.'))
		clean.pop_back();
	while (!clean.empty() && clean.front() == ' ')
		clean.erase(0, 1);
	return clean.empty() ? fs::path() : customDir() / fs::u8path(clean + ".txt");
}
} // namespace

std::vector<Preset> customPresets() {
	std::vector<Preset> out;
	std::error_code ec;
	for (auto& e : fs::directory_iterator(customDir(), ec)) {
		if (e.path().extension() != ".txt")
			continue;
		auto words = readFile(e.path());
		if (!words.empty()) {
			std::string name = (const char*)e.path().stem().u8string().c_str();
			out.push_back({"custom:" + name, name, fitted(words)});
		}
	}
	std::sort(out.begin(), out.end(), [](const Preset& a, const Preset& b) { return PixelText::lower(a.name) < PixelText::lower(b.name); });
	return out;
}

bool customExists(const std::string& name) {
	fs::path f = customFile(name);
	std::error_code ec;
	return !f.empty() && fs::exists(f, ec);
}

bool saveCustom(const std::string& name) {
	fs::path f = customFile(name);
	return !f.empty() && writeFile(f, engine.wordlist, "Zeetures words: " + name);
}

void deleteCustom(const std::string& name) {
	fs::path f = customFile(name);
	std::error_code ec;
	if (!f.empty())
		fs::remove(f, ec);
}

void applyForWorld(const fs::path& worlddir) {
	defaultWords();
	s_worlddir = worlddir;
	std::vector<std::string> words;
	if (allWorlds())
		words = readFile(playerFile());
	if (words.empty() && !worlddir.empty())
		words = readFile(worldFile());
	if (words.empty())
		words = s_default;
	setLive(words);
}

bool allWorlds() {
	if (s_allWorlds < 0)
		s_allWorlds = readPlayerSetting("words-all-worlds") == "1";
	return s_allWorlds == 1;
}

void setAllWorlds(bool on) {
	if (on == allWorlds())
		return;
	s_allWorlds = on;
	writePlayerSetting("words-all-worlds", on ? "1" : "0");
	// the first time: the words in use now become every world's
	if (on && readFile(playerFile()).empty())
		writeFile(playerFile(), engine.wordlist, "Zeetures words for every world");
	applyForWorld(s_worlddir);
}

void setWord(size_t slot, const std::string& word) {
	std::vector<std::string> w = engine.wordlist;
	if (slot >= w.size())
		return;
	w[slot] = word;
	setLive(w);
	save();
}

void setWords(const std::vector<std::string>& words) {
	setLive(words);
	save();
}

int clash(size_t slot, const std::string& word) {
	std::string w = PixelText::lower(word);
	if (w.empty())
		return -1;
	for (size_t i = 0; i < engine.wordlist.size(); i++)
		if (i != slot && PixelText::lower(engine.wordlist[i]) == w)
			return (int)i;
	return -1;
}

std::vector<std::string> readFile(const fs::path& file) {
	std::ifstream in(file, std::ios::binary);
	if (!in)
		return {};
	std::vector<std::string> w;
	std::string line;
	bool any = false;
	while (std::getline(in, line)) {
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		if (line.empty() || line[0] == '*')
			continue;
		size_t sp = line.find(' ');
		char* end = nullptr;
		unsigned long n = std::strtoul(line.c_str(), &end, 10);
		if (end == line.c_str() || n >= 1000)
			continue;
		if (w.size() <= n)
			w.resize(n + 1);
		w[n] = sp == std::string::npos ? std::string() : line.substr(sp + 1);
		any = true;
	}
	return any ? w : std::vector<std::string>();
}

bool writeFile(const fs::path& file, const std::vector<std::string>& words, const std::string& title) {
	std::error_code ec;
	fs::create_directories(file.parent_path(), ec);
	std::ofstream out(file, std::ios::binary);
	if (!out)
		return false;
	out << "* " << title << "\n";
	out << "* Each line: a slot of the game's word list (0-81) and its word. Changed in Preferences > Words.\n";
	auto& en = english();
	for (size_t i = 0; i < words.size(); i++) {
		out << i;
		if (!words[i].empty())
			out << " " << words[i];
		out << "\n";
	}
	(void)en;
	return (bool)out;
}

} // namespace WordLists
