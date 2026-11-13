#include "Hatchery.h"

/*
  C1 hatchery resources:
  hatchery.bmp and htchmask.bmp used for the background/foreground
  SCAN*.BMP and hdsk.wav used for egg disk animation
  EGG*.BMP and hegg.wav used for egg movement animation
  FAN*.BMP and hfan.wav used for the fan animation
  lightoff.bmp and hlgt.wav used for the light flickering
  GENSPIN.BMP, and hmle.wav/hfml.wav used for male/female animation (male.bmp and female.bmp also present)

  C2's Omelette.s16:
  * frames 0-16 are eggs - 11-16 are masked with black
  * frame 17 is an egg shadow
  * frames 22-37 is a rotating questionmark anim
  * frames 38-53 (female) and 54-69 are (male) are rotating gender symbol anims
  *
  * frame 18 and 19 are sides of the hatchery
  * frame 20 is the grabber
  * frame 21 is the middle bit of the side
*/


#include "Breeds.h"
#include "creatures/CreatureSpeech.h"
#include "Agent.h"
#include "C2Kits.h"
#include "C2ToolBar.h"
#include "C2KitsInternal.h"
#include "historyManager.h"
#include "Engine.h"
#include "creatures/BreedSlots.h"
#include "common/io/FileWriter.h"
#include "common/Ascii.h"
#include "PathResolver.h"
#include "Camera.h"
#include "ImGuiUtils.h"
#include "World.h"
#include "common/Random.h"
#include "fileformats/bmpImage.h"
#include "fileformats/c16Image.h"
#include "fileformats/pngImage.h"
#include "EmbeddedFiles.h"
#include "common/io/SpanReader.h"
#include "common/backend/Backend.h"
#include "creatures/BreedSlots.h"

#include <array>
#include <vector>
#include <fmt/core.h>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <algorithm>
#include <imgui.h>
#include <imgui_internal.h>
#include <chrono>
#include <map>
#include <cmath>
#include <cfloat>
#include <mutex>
#include <SDL3/SDL_dialog.h>

using namespace ImGuiUtils;

namespace Openc2eImgui {

static bool s_hatchery_open = false;
static bool s_hatchery_minimized = false;
static bool s_hatchery_focus = false;
static bool s_hatchery_loaded = false;

static Texture s_hatcherybackground;
static Texture s_hatcherymask;
static std::vector<Texture> s_omelette;

struct C1EggInfo {
	std::string name;
	Texture texture;
	int x;
	int y;
};

std::array<C1EggInfo, 6> s_c1eggs = {{
	{"hatchery/EGG0.bmp", {}, 50, 150},
	{"hatchery/EGG1.bmp", {}, 80, 145},
	{"hatchery/EGG2.bmp", {}, 110, 150},
	{"hatchery/EGG3.bmp", {}, 140, 140},
	{"hatchery/EGG4.bmp", {}, 170, 155},
	{"hatchery/EGG5.bmp", {}, 200, 150},
}};

// A C2 egg from the Eggs folder. File layout (read from the eggs themselves):
// "eggf", gender (1 male, 2 female, 0 unspecified), egg picture index
// (frame in Omelette.s16 and, x8, in the world's eggs.s16), 8 unused bytes,
// two 4-character monikers, 4 unused bytes, then one or two genomes, each
// as a 32-bit length followed by the genome file data ("dna2...").
struct C2EggInfo {
	std::filesystem::path file;
	int gender = 0;
	int frame = 0;
	std::string moniker;
	std::string breed; // its breed, named as in the Breeder's Kit (owner, 2026-11-12)
	std::string label; // the breed's full label ("Horse (Norn c)"), for the tooltip
	int sexes = 3; // the sexes it may hatch as (1 male, 2 female): its breed's pictures
	int group = 0; // its shelf: 0 norns, 1 grendels, 2 ettins, 3 C2's fourth set
	int species = -1, slot = -1; // its breed's picture set and slot (-1: from a genome file)
	bool imported = false; // made from a genome file (Import Genome)
	std::vector<std::vector<uint8_t>> genomes;
	bool taken = false; // placed in the world; back when the Hatchery next opens
	long long added = 0; // when its breed (or genome file) was added; 0: came with the game
	long long placed = 0; // when one was last placed in the world (hatchery-used.txt)
};

static std::vector<C2EggInfo> s_c2eggs;

// File > Import Genome: the desktop's file chooser (SDL3, through the
// xdg file-chooser portal where there is one) answers on its own thread,
// so its picks wait here until the Hatchery is next drawn.
static std::mutex s_import_mutex;
static std::vector<std::string> s_import_pending;
static std::string s_import_message;

static void SDLCALL ImportGenomeChosen(void*, const char* const* files, int) {
	if (!files)
		return; // error or cancelled
	std::lock_guard<std::mutex> lock(s_import_mutex);
	for (const char* const* f = files; *f; f++)
		s_import_pending.push_back(*f);
}

static long long NowSeconds() {
	return (long long)std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

// hatchery-used.txt in the user data folder: "seconds<TAB>label" lines,
// one per egg placed, for sorting by "Recently placed"
static std::map<std::string, long long> ReadPlacedLog() {
	std::map<std::string, long long> out;
	std::ifstream in(getUserDataDir() / "hatchery-used.txt");
	std::string line;
	while (std::getline(in, line)) {
		auto tab = line.find('\t');
		if (tab != std::string::npos)
			out[line.substr(tab + 1)] = atoll(line.c_str());
	}
	return out;
}

static void NotePlaced(const std::string& label, long long when) {
	std::ofstream out(getUserDataDir() / "hatchery-used.txt", std::ios::app);
	out << when << "\t" << label << "\n";
}

// an egg picture (Omelette.s16 frames 0-10) that stays the same for a
// breed from one opening to the next
static int StableEggFrame(const std::string& key) {
	uint32_t h = 2166136261u;
	for (unsigned char c : key)
		h = (h ^ c) * 16777619u;
	return (int)(h % 11);
}

// Makes a Hatchery egg from a C2 genome file ("dna2..."): one parent
// genome, so the creature is that genome (with the usual small mutations).
static void ImportGenome(const std::string& path) {
	std::ifstream in(path, std::ios::binary);
	std::vector<uint8_t> d((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	std::string name = std::filesystem::path(path).filename().string();
	if (d.size() < 8 || std::memcmp(d.data(), "dna2", 4) != 0) {
		s_import_message = fmt::format("{} is not a Creatures 2 genome.", name);
		return;
	}
	C2EggInfo egg;
	egg.file = path;
	egg.gender = 0;
	egg.frame = StableEggFrame(name);
	egg.moniker = std::filesystem::path(path).stem().string().substr(0, 4);
	egg.genomes.push_back(d);
	egg.breed = BreedNameOfGenome(d);
	if (egg.breed.empty())
		egg.breed = std::filesystem::path(path).stem().string();
	egg.label = egg.breed;
	egg.imported = true;
	egg.added = NowSeconds();
	egg.group = std::clamp(GenomeGenus(d), 0, 3);
	s_c2eggs.push_back(egg);
	s_import_message = fmt::format("Made an egg from {}.", name);
}

// An egg's genomes, written to the user data folder under short unique
// tokens for NEW: GENE: {mother's token, father's token}.
static std::pair<std::string, std::string> WriteEggGenomes(const std::vector<std::vector<uint8_t>>& genomes, const std::string& token) {
	std::string mum = token + "m";
	std::string dad = genomes.size() > 1 ? token + "d" : mum;
	{
		FileWriter w = createUserGeneticsFile(mum + ".gen");
		w.write(genomes[0].data(), genomes[0].size());
	}
	if (genomes.size() > 1) {
		FileWriter w = createUserGeneticsFile(dad + ".gen");
		w.write(genomes[1].data(), genomes[1].size());
	}
	return {mum, dad};
}

// Lays an egg in the world near the incubator: the world's own egg object
// (2 5 2), whose scripts develop and hatch it. Its genomes are written to
// the user data folder under short unique tokens for NEW: GENE. Used by the
// Hatchery and by the Breeder's Kit ("lay an egg of this breed").
bool LayC2Egg(const std::vector<std::vector<uint8_t>>& genomes, int gender, int frame, const std::string& token, std::string& err) {
	if (genomes.empty()) {
		err = "no genome";
		return false;
	}
	auto [mum, dad] = WriteEggGenomes(genomes, token);
	frame = std::clamp(frame, 0, 16);
	int x = rand_int32(5000, 5200);
	std::string script = fmt::format("new: simp eggs 8 {} 2000 0\n", frame * 8);
	script += "pose 0\n"; // start of development; the egg's timer script advances it
	script += "setv cls2 2 5 2\nsetv attr 195\n";
	script += fmt::format("new: gene tokn {} tokn {} obv0\n", mum, dad);
	script += fmt::format("setv obv1 {}\n", gender);
	script += "tick 2400\n";
	script += fmt::format("mvto {} 750\n", x);
	script += "setv grav 1\n";
	err = engine.executeNetwork(script);
	if (!err.empty())
		return false;
	engine.camera->trackAgent(AgentRef(), 0, 0, brittle, none);
	engine.camera->moveToGlobal(x - (int)engine.camera->getWidth() / 2, 750 - (int)engine.camera->getHeight() / 2, UiPanStyle());
	return true;
}

// Puts an egg in the incubator (3 7 1), as the hand does when it lets go
// of one in the machine's cabin: the egg boards it, and its drop script
// (the world's own, 2 5 2 5), finding itself carried by the incubator,
// hatches it at once. The incubator takes eggs only while it is open (its
// timer script closes it, clearing ATTR 8, while the world has as many
// creatures as HATL allows). False, with a reason, if it can't.
static bool PutC2EggInIncubator(const std::vector<std::vector<uint8_t>>& genomes, int gender, int frame, const std::string& token, std::string& err) {
	Agent* incubator = nullptr;
	for (auto& a : world.agents)
		if (a && a->family == 3 && a->genus == 7 && a->species == 1) {
			incubator = a.get();
			break;
		}
	if (!incubator) {
		err = "There is no incubator in this world.";
		return false;
	}
	if (!incubator->greedycabin()) {
		err = "The incubator is closed: the world has as many creatures as it allows.";
		return false;
	}
	if (genomes.empty()) {
		err = "The egg has no genome.";
		return false;
	}
	std::set<Agent*> before;
	for (auto& a : world.agents)
		if (a && a->family == 2 && a->genus == 5 && a->species == 2)
			before.insert(a.get());
	auto [mum, dad] = WriteEggGenomes(genomes, token);
	frame = std::clamp(frame, 0, 16);
	// made in the middle of the cabin, then made the incubator's passenger
	std::string script = "enum 3 7 1\n";
	script += "setv var0 posl addv var0 posr divv var0 2 setv var1 post addv var1 30 setv var2 targ\n";
	script += fmt::format("new: simp eggs 8 {} 2000 0\n", frame * 8);
	script += "pose 0\nsetv cls2 2 5 2\nsetv attr 195\n";
	script += fmt::format("new: gene tokn {} tokn {} obv0\n", mum, dad);
	script += fmt::format("setv obv1 {}\n", gender);
	script += "tick 2400\n";
	script += "setv var4 posr subv var4 posl divv var4 2 subv var0 var4\n";
	script += "mvto var0 var1\nsetv grav 1\n";
	script += "setv var3 targ targ var2 spas targ var3\n";
	script += "next\n";
	err = engine.executeNetwork(script);
	if (!err.empty())
		return false;
	Agent* egg = nullptr;
	for (auto& a : world.agents)
		if (a && a->family == 2 && a->genus == 5 && a->species == 2 && !before.count(a.get()))
			egg = a.get();
	if (!egg) {
		err = "The egg was not made.";
		return false;
	}
	bool boarded = egg->invehicle.get() == incubator;
	// the drop event needs an agent that dropped it (fireScript makes that
	// one let go first, and skips the event without one): the egg itself,
	// which holds nothing, so only the script runs (it doesn't use FROM)
	bool queued = boarded && egg->queueScript(5, AgentRef(egg));
	fmt::print("* Hatchery: egg in the incubator: boarded {}, drop script {}\n", boarded, queued);
	// (if it didn't board, it lies in the incubator's bay and hatches in time)
	engine.camera->trackAgent(AgentRef(), 0, 0, brittle, none);
	engine.camera->moveToGlobal((int)(incubator->x + incubator->getWidth() / 2) - (int)engine.camera->getWidth() / 2,
		(int)(incubator->y + incubator->getHeight() / 2) - (int)engine.camera->getHeight() / 2, UiPanStyle());
	return true;
}

static std::string s_status; // the last thing done, shown on the card

static void TakeC2Egg(size_t index, bool incubator) {
	C2EggInfo& egg = s_c2eggs[index];
	if (egg.taken)
		return;
	std::vector<std::vector<uint8_t>> genomes = egg.genomes;
	int gender = egg.gender;
	if (gender == 0) // decided now: either, or the one its breed has
		gender = egg.sexes == 1 ? 1 : egg.sexes == 2 ? 2 : rand_int32(1, 2);
	// tokens 4 characters, unique per Hatchery egg: q + 2 digits + m/d
	std::string token = fmt::format("q{:02d}", (int)index % 100);
	std::string err;
	if (incubator) {
		if (PutC2EggInIncubator(genomes, gender, egg.frame, token, err)) {
			egg.taken = true;
			egg.placed = NowSeconds();
			NotePlaced(egg.label, egg.placed);
			s_status = fmt::format("The {} egg is in the incubator, hatching now.", egg.breed);
			return;
		}
		fmt::print("* Hatchery: incubator: {}\n", err);
	}
	std::string why = err;
	if (!LayC2Egg(genomes, gender, egg.frame, token, err)) {
		fmt::print("Couldn't hatch egg {}: {}\n", egg.breed, err);
		s_status = fmt::format("Couldn't place the {} egg: {}", egg.breed, err);
		return;
	}
	egg.taken = true;
	egg.placed = NowSeconds();
	NotePlaced(egg.label, egg.placed);
	s_status = incubator ? fmt::format("{} The {} egg is beside it instead.", why, egg.breed)
						 : fmt::format("The {} egg is by the incubator. It hatches where it lies in a while, or at once in the incubator.", egg.breed);
}

// one egg of every breed in the game (owner, 2026-11-12), each the breed's
// genome (its own, or the species' standard one) wearing the breed's
// parts, as the Breeder's Kit lays them; its sex unknown until it is placed
// in the world, then drawn from the sexes its breed has. Eggs made from
// genome files are kept. Every egg is back, untaken (owner, 2026-11-12:
// replenished each time the Hatchery opens).
static void LoadC2Eggs() {
	std::vector<C2EggInfo> fromFiles;
	for (auto& e : s_c2eggs)
		if (e.imported) {
			e.taken = false;
			fromFiles.push_back(e);
		}
	s_c2eggs.clear();
	std::vector<const BreedInfo*> breeds;
	for (auto& b : KnownBreeds())
		if (b.parts.count('a') && b.parts.count('b')) // a head and a body
			breeds.push_back(&b);
	std::stable_sort(breeds.begin(), breeds.end(), [&](const BreedInfo* l, const BreedInfo* r) {
		return l->group() != r->group() ? l->group() < r->group() : l->species != r->species ? l->species < r->species : l->slot < r->slot;
	});
	std::string kinds;
	auto placed = ReadPlacedLog();
	for (const BreedInfo* b : breeds) {
		auto genome = BreedBaseGenome(*b);
		if (genome.empty())
			continue;
		C2EggInfo egg;
		egg.genomes.push_back(GenomeWithBreed(genome, *b));
		egg.gender = 0;
		egg.sexes = (b->male ? 1 : 0) | (b->female ? 2 : 0);
		egg.breed = b->name.empty() ? BreedLabel(*b) : b->name;
		egg.label = BreedLabel(*b);
		egg.frame = StableEggFrame(egg.label);
		egg.group = std::clamp(b->group(), 0, 3);
		egg.species = b->species;
		egg.slot = b->slot;
		// added: when its pack's files were installed (the game's own: 0)
		for (auto& f : b->files) {
			std::error_code ec;
			auto t = std::filesystem::last_write_time(getUserDataDir() / f, ec);
			if (!ec)
				egg.added = std::max(egg.added, (long long)std::chrono::duration_cast<std::chrono::seconds>(
													std::chrono::clock_cast<std::chrono::system_clock>(t).time_since_epoch())
													.count());
		}
		if (auto it = placed.find(egg.label); it != placed.end())
			egg.placed = it->second;
		s_c2eggs.push_back(egg);
		kinds += fmt::format("{}{}", kinds.empty() ? "" : ", ", egg.label);
	}
	// eggs on the same shelf look different: each keeps its own stable
	// picture unless one of the ten before it on the shelf (in breed order)
	// has it, then takes the next free one; so a shelf of up to 11 (the
	// pictures there are) has no two alike, and a longer one no two close
	for (int g = 0; g <= 3; g++) {
		std::vector<int> recent;
		for (auto& e : s_c2eggs) {
			if (e.group != g)
				continue;
			while (std::find(recent.begin(), recent.end(), e.frame) != recent.end())
				e.frame = (e.frame + 1) % 11;
			recent.push_back(e.frame);
			if (recent.size() > 10)
				recent.erase(recent.begin());
		}
	}
	for (auto& e : fromFiles)
		s_c2eggs.push_back(e);
	fmt::print("* Hatchery: {} eggs, one of each breed: {}\n", s_c2eggs.size(), kinds);
}

// ---- The nursery (owner, 2026-11-13) ----------------------------------------
// C2's Hatchery as an egg nursery: one rough-cut shelf per kind of
// creature on a draped hessian wall, each scrolled left and right, an egg
// in a woven nest per breed with a worn card label; the chosen egg lifts
// under a warm lamp, and its card on the right names it and places it.
// The wall, board and nests are our own procedural art (data/hatchery,
// make_art.py), built into the program; the labels, shadows and lamp are
// drawn here; the eggs are the player's own Omelette.s16.

static bool s_art_loaded = false;
static Texture s_wall, s_board, s_glow, s_cardglow, s_contact;
static std::vector<Texture> s_nests; // frames: variant 0 back, 0 front, 1 back ...
static int s_nest_rim = 18, s_nest_bottom = 38;
static bool s_refill = false; // opened again: every egg back on its shelf
static size_t s_selected = 0;
static int s_filter = -1; // the shelf shown alone, or -1 for all
static char s_search[64] = "";

// the order of the eggs on each shelf, like the Agent Injector's (owner,
// 2026-11-13); kept between runs (settings.txt, "hatchery-sort")
enum { HSORT_SLOT, HSORT_NAME, HSORT_NAME_DESC, HSORT_ADDED, HSORT_IMPORTED, HSORT_PLACED, HSORT_COUNT };
static const char* HSORT_NAMES[HSORT_COUNT] = {"Breed slot", "Name, A to Z", "Name, Z to A", "Recently added", "Imported first", "Recently placed"};
static int s_hsort = -1; // -1: not read yet

struct Shelf {
	float off = 0, target = 0;
	// a press on the shelf: a click if the mouse stays put, a drag that
	// scrolls the shelf if it moves (owner, 2026-11-13)
	bool pressed = false, dragged = false;
	float pressX = 0, pressOff = 0;
	int pressItem = -1;
};
static std::map<int, Shelf> s_shelves;

static const ImU32 kText = IM_COL32(222, 224, 228, 255);
static const ImU32 kDim = IM_COL32(140, 146, 156, 255);
static const ImU32 kGold = IM_COL32(205, 170, 95, 255);
static const ImU32 kCard = IM_COL32(28, 29, 33, 255);
static const ImU32 kEdge = IM_COL32(48, 48, 51, 255);
static const ImVec4 kTab(42 / 255.f, 79 / 255.f, 130 / 255.f, 1);
static const ImVec4 kTabOn(51 / 255.f, 105 / 255.f, 173 / 255.f, 1);
static const ImVec4 kFrame(31 / 255.f, 49 / 255.f, 75 / 255.f, 1);

static const float SLOT = 84, SHELF_STAND = 16;

static const char* ShelfTitle(int group) {
	switch (group) {
		case 0: return "Norns";
		case 1: return "Grendels";
		case 2: return "Ettins";
		default: return "Others";
	}
}
static const char* KindName(int group) {
	switch (group) {
		case 0: return "Norn";
		case 1: return "Grendel";
		case 2: return "Ettin";
		default: return "Creature";
	}
}

static std::vector<Texture> LoadEmbeddedPng(const char* name) {
	std::vector<Texture> out;
	auto bytes = EmbeddedFile(name);
	if (bytes.empty())
		return out;
	try {
		SpanReader r(bytes);
		for (auto& im : ReadPngFile(r))
			out.push_back(get_backend()->createTextureFromImage(im));
	} catch (std::exception& e) {
		fmt::print("* Hatchery: {}: {}\n", name, e.what());
	}
	return out;
}

// an RGBA picture made pixel by pixel
template <typename F>
static Texture MakeTexture(int w, int h, F pixel) {
	::Image im;
	im.width = w;
	im.height = h;
	im.format = if_rgba32;
	im.data = shared_array<uint8_t>(size_t(w) * h * 4);
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++)
			pixel(x, y, &im.data[(size_t(y) * w + x) * 4]);
	return get_backend()->createTextureFromImage(im);
}

// a soft warm glow (the lamp over the chosen egg)
static Texture MakeGlow(int size, float radius, float soft, float alpha) {
	return MakeTexture(size, size, [&](int x, int y, uint8_t* p) {
		float r = std::hypot(x + 0.5f - size / 2.f, y + 0.5f - size / 2.f);
		float a = alpha / (1 + std::exp((r - radius) / soft));
		p[0] = 255, p[1] = 196, p[2] = 110, p[3] = (uint8_t)std::clamp(a, 0.f, 255.f);
	});
}

// value noise for the labels' paper: 0..1, features about `cell` px across
static float Hash01(int x, int y, uint32_t seed) {
	uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + seed * 2246822519u;
	h = (h ^ (h >> 13)) * 1274126177u;
	return ((h ^ (h >> 16)) & 0xffff) / 65535.f;
}
static float ValueNoise(float x, float y, uint32_t seed) {
	int xi = (int)std::floor(x), yi = (int)std::floor(y);
	float fx = x - xi, fy = y - yi;
	fx = fx * fx * (3 - 2 * fx);
	fy = fy * fy * (3 - 2 * fy);
	float a = Hash01(xi, yi, seed), b = Hash01(xi + 1, yi, seed);
	float c = Hash01(xi, yi + 1, seed), d = Hash01(xi + 1, yi + 1, seed);
	return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}
static float Fbm(float x, float y, float cell, uint32_t seed, int octaves) {
	float out = 0, amp = 1, tot = 0;
	for (int o = 0; o < octaves; o++) {
		float c = std::max(1.5f, cell / (1 << o));
		out += amp * ValueNoise(x / c, y / c, seed + o * 17);
		tot += amp;
		amp *= 0.5f;
	}
	return out / tot;
}

static void LoadNurseryArt() {
	if (s_art_loaded)
		return;
	s_art_loaded = true;
	auto wall = LoadEmbeddedPng("wall.png");
	auto board = LoadEmbeddedPng("board.png");
	s_nests = LoadEmbeddedPng("nests.png");
	if (!wall.empty())
		s_wall = wall[0];
	if (!board.empty())
		s_board = board[0];
	auto meta = EmbeddedFile("nests.txt");
	std::string text(meta.begin(), meta.end());
	int v, rim, bottom;
	for (size_t at = 0; at < text.size();) {
		size_t end = text.find('\n', at);
		std::string line = text.substr(at, end == std::string::npos ? std::string::npos : end - at);
		if (sscanf(line.c_str(), "%d %d %d", &v, &rim, &bottom) == 3 && v == 0)
			s_nest_rim = rim, s_nest_bottom = bottom;
		if (end == std::string::npos)
			break;
		at = end + 1;
	}
	s_glow = MakeGlow(140, 50, 9, 110);
	s_cardglow = MakeGlow(240, 80, 13, 90);
	// the soft shadow a nest casts on the board
	s_contact = MakeTexture(84, 30, [](int x, int y, uint8_t* p) {
		float dx = (x + 0.5f - 42) / 31.f, dy = (y + 0.5f - 15) / 8.f;
		float r = std::sqrt(dx * dx + dy * dy);
		p[0] = p[1] = p[2] = 0;
		p[3] = (uint8_t)(110 / (1 + std::exp((r - 1) / 0.22f)));
	});
}

// ---- worn card labels -------------------------------------------------------

struct NurseryLabel {
	Texture tex;
	ImFont* font = nullptr;
	std::vector<std::string> lines;
	int w = 0, h = 0, lineh = 0;
};
static std::map<std::pair<std::string, bool>, NurseryLabel> s_labels;

static float TextWidth(ImFont* f, const std::string& s) {
	return f->CalcTextSizeA(f->FontSize, FLT_MAX, 0, s.c_str()).x;
}

static std::vector<std::string> WrapWords(const std::string& name, ImFont* f, float maxw) {
	std::vector<std::string> lines;
	std::string cur, word;
	auto flush = [&](const std::string& w) {
		std::string t = cur.empty() ? w : cur + " " + w;
		if (cur.empty() || TextWidth(f, t) <= maxw)
			cur = t;
		else {
			lines.push_back(cur);
			cur = w;
		}
	};
	for (char c : name + " ") {
		if (c == ' ') {
			if (!word.empty())
				flush(word);
			word.clear();
		} else
			word += c;
	}
	if (!cur.empty())
		lines.push_back(cur);
	return lines;
}

// A label for a breed: the whole name, on two lines if need be, in
// smaller type only if a word is too long (owner, 2026-11-12: never cut a
// name off); aged paper, browned at the frayed edges, foxed, one corner
// torn, pinned with a dull tack. `lit`: under the lamp.
static const NurseryLabel& GetLabel(const std::string& name, bool lit) {
	auto key = std::make_pair(name, lit);
	if (auto it = s_labels.find(key); it != s_labels.end())
		return it->second;
	NurseryLabel L;
	const float maxw = 64;
	for (int size : {11, 10, 9, 8}) {
		ImFont* f = NurseryFont(size);
		L.font = f ? f : ImGui::GetFont();
		L.lines = WrapWords(name, L.font, f ? maxw : 80);
		bool fits = L.lines.size() <= 2;
		for (auto& l : L.lines)
			fits = fits && TextWidth(L.font, l) <= maxw;
		if (fits || !f)
			break; // (at the smallest size a long word stays whole: the card grows)
	}
	float tw = 0;
	for (auto& l : L.lines)
		tw = std::max(tw, TextWidth(L.font, l));
	L.lineh = (int)L.font->FontSize + 2;
	L.w = (int)tw + 14 + 8;
	L.h = L.lineh * (int)L.lines.size() + 7 + 8;
	uint32_t seed = 2166136261u;
	for (unsigned char c : name)
		seed = (seed ^ c) * 16777619u;
	const int pw = L.w, ph = L.h;
	const bool tornRight = (seed >> 7) & 1;
	const float tornR = 4 + (seed >> 9) % 4;
	const float tackx = pw / 2.f, tacky = 3.5f;
	L.tex = MakeTexture(pw, ph, [&](int x, int y, uint8_t* p) {
		float edge = (float)std::min(std::min(x - 2, pw - 3 - x), std::min(y - 2, ph - 3 - y));
		float n = Fbm((float)x, (float)y, 6, seed, 3);
		float base[3] = {222, 204, 160};
		if (lit)
			base[0] = 240, base[1] = 216, base[2] = 156;
		float k = (0.86f + 0.18f * n) * std::clamp(0.72f + 0.06f * edge, 0.72f, 1.f); // browned edges
		float stain = std::clamp((Fbm((float)x, (float)y, 9, seed + 1, 2) - 0.62f) * 4, 0.f, 1.f);
		k *= 1 - 0.22f * stain; // foxing
		bool on = edge + (n - 0.5f) * 2.4f > 0; // a frayed outline
		float cx = tornRight ? pw - 3 : 2, cy = ph - 3;
		if (std::abs(x - cx) + std::abs(y - cy) < tornR)
			on = false; // the torn corner
		for (int i = 0; i < 3; i++)
			p[i] = (uint8_t)std::clamp(base[i] * k, 0.f, 255.f);
		p[3] = on ? 255 : 0;
		// the tack
		float tr = std::hypot(x + 0.5f - tackx, y + 0.5f - tacky);
		if (tr <= 2.6f) {
			bool rim = tr > 1.7f;
			p[0] = rim ? 78 : 132, p[1] = rim ? 60 : 108, p[2] = rim ? 30 : 64, p[3] = 255;
		}
	});
	return s_labels.emplace(key, std::move(L)).first->second;
}

static void DrawLabel(ImDrawList* dl, const std::string& name, bool lit, float cx, float top) {
	const NurseryLabel& L = GetLabel(name, lit);
	ImVec2 p(std::floor(cx - L.w / 2.f), std::floor(top));
	dl->AddImage(L.tex.as<ImTextureID>(), p + ImVec2(2, 2), p + ImVec2(L.w + 2, L.h + 2), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(0, 0, 0, 115));
	dl->AddImage(L.tex.as<ImTextureID>(), p, p + ImVec2(L.w, L.h));
	for (size_t i = 0; i < L.lines.size(); i++) {
		float lw = TextWidth(L.font, L.lines[i]);
		dl->AddText(L.font, L.font->FontSize, ImVec2(std::floor(p.x + (L.w - lw) / 2), p.y + 5 + i * L.lineh), IM_COL32(78, 54, 32, 225), L.lines[i].c_str());
	}
}

// ---- drawing helpers --------------------------------------------------------

// the wall, laid from `origin` and repeated downward, where it falls in a..b
static void DrawWall(ImDrawList* dl, ImVec2 origin, ImVec2 a, ImVec2 b, float alpha = 1) {
	if (!s_wall) {
		dl->AddRectFilled(a, b, IM_COL32(52, 40, 28, (int)(255 * alpha)));
		return;
	}
	float W = (float)s_wall.width, H = (float)s_wall.height;
	for (float y = a.y; y < b.y;) {
		float wy = std::fmod(y - origin.y, H);
		if (wy < 0)
			wy += H;
		float y1 = std::min(b.y, y + (H - wy));
		float wx0 = std::fmod(a.x - origin.x, W), wx1 = wx0 + (b.x - a.x);
		dl->AddImage(s_wall.as<ImTextureID>(), ImVec2(a.x, y), ImVec2(b.x, y1), ImVec2(wx0 / W, wy / H), ImVec2(wx1 / W, (wy + (y1 - y)) / H),
			IM_COL32(255, 255, 255, (int)(255 * alpha)));
		y = y1;
	}
}

// the board from `src` along it (repeating), `width` long, at `at`
static void DrawBoard(ImDrawList* dl, ImVec2 at, float src, float width) {
	if (!s_board) {
		dl->AddRectFilled(at, at + ImVec2(width, 48), IM_COL32(70, 50, 34, 255));
		return;
	}
	float W = (float)s_board.width, H = (float)s_board.height;
	for (float x = 0; x < width;) {
		float bx = std::fmod(src + x, W);
		float len = std::min(width - x, W - bx);
		dl->AddImage(s_board.as<ImTextureID>(), at + ImVec2(x, 0), at + ImVec2(x + len, H), ImVec2(bx / W, 0), ImVec2((bx + len) / W, 1));
		x += len;
	}
}

static void DrawChevron(ImDrawList* dl, ImVec2 c, bool right, ImU32 col) {
	float d = right ? 1 : -1;
	ImVec2 pts[3] = {c + ImVec2(-4 * d, -8), c + ImVec2(4 * d, 0), c + ImVec2(-4 * d, 8)};
	dl->AddPolyline(pts, 3, col, ImDrawFlags_None, 3);
}

static Texture* EggTexture(const C2EggInfo& egg) {
	if (s_omelette.empty())
		return nullptr;
	return &s_omelette[std::clamp(egg.frame, 0, std::min(16, (int)s_omelette.size() - 1))];
}

static bool EggMatches(const C2EggInfo& egg) {
	if (s_filter >= 0 && egg.group != s_filter)
		return false;
	if (!s_search[0])
		return true;
	std::string q = to_ascii_lowercase(std::string(s_search));
	return to_ascii_lowercase(egg.breed).find(q) != std::string::npos || to_ascii_lowercase(egg.label).find(q) != std::string::npos;
}

// the eggs on a shelf that match the search, in the chosen order
static std::vector<size_t> ShelfItems(int g) {
	std::vector<size_t> items;
	for (size_t i = 0; i < s_c2eggs.size(); i++)
		if (s_c2eggs[i].group == g && EggMatches(s_c2eggs[i]))
			items.push_back(i);
	auto lower = [](const C2EggInfo& e) { return to_ascii_lowercase(e.breed); };
	std::stable_sort(items.begin(), items.end(), [&](size_t a, size_t b) {
		const C2EggInfo &l = s_c2eggs[a], &r = s_c2eggs[b];
		switch (s_hsort) {
			case HSORT_NAME: return lower(l) < lower(r);
			case HSORT_NAME_DESC: return lower(l) > lower(r);
			case HSORT_ADDED:
				if (l.added != r.added)
					return l.added > r.added;
				return false;
			case HSORT_IMPORTED: {
				bool li = l.imported || (l.species >= 0 && FindBreed(l.species, l.slot) && FindBreed(l.species, l.slot)->imported);
				bool ri = r.imported || (r.species >= 0 && FindBreed(r.species, r.slot) && FindBreed(r.species, r.slot)->imported);
				if (li != ri)
					return li;
				return lower(l) < lower(r);
			}
			case HSORT_PLACED:
				if (l.placed != r.placed)
					return l.placed > r.placed;
				return lower(l) < lower(r);
			default: return false; // breed slot: as loaded
		}
	});
	return items;
}

static int NestVariant(const C2EggInfo& egg) {
	int n = std::max(1, (int)s_nests.size() / 2);
	return (StableEggFrame(egg.label + "#nest") * 7 + (int)egg.label.size()) % n;
}

// ---- the shelves -------------------------------------------------------------

// Draws the shelves in the nursery panel (the current child window).
// Returns the height they take.
static float DrawShelves(float NW, float NH) {
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 origin = ImGui::GetCursorScreenPos(); // scrolls with the shelves
	ImVec2 vis0 = ImGui::GetWindowPos(), vis1 = vis0 + ImGui::GetWindowSize();
	ImGuiIO& io = ImGui::GetIO();
	// the wall behind everything
	DrawWall(dl, origin, ImVec2(origin.x, vis0.y), ImVec2(origin.x + NW, vis1.y));

	float y = origin.y + 10;
	float x0 = origin.x, x1 = origin.x + NW;
	for (int g = 0; g <= 3; g++) {
		std::vector<size_t> items = ShelfItems(g);
		if (items.empty())
			continue;
		// the heading
		ImFont* hf = NurseryFont(15);
		const char* title = ShelfTitle(g);
		if (hf)
			dl->AddText(hf, hf->FontSize, ImVec2(x0 + 14, y), kGold, title);
		else
			dl->AddText(ImVec2(x0 + 14, y), kGold, title);
		float tw = hf ? TextWidth(hf, title) : ImGui::CalcTextSize(title).x;
		std::string count = fmt::format("{} breed{}", items.size(), items.size() == 1 ? "" : "s");
		dl->AddText(ImVec2(x0 + 22 + tw, y + 3), kDim, count.c_str());
		y += 18;
		const float SY = y + 52; // the board's top
		const float full = std::max(NW, 40 + items.size() * SLOT + 40);
		const float maxoff = std::max(0.f, full - NW);
		Shelf& st = s_shelves[g];

		// input: the whole band of the shelf is one control
		ImGui::SetCursorScreenPos(ImVec2(x0, SY - 52));
		ImGui::PushID(g);
		ImGui::InvisibleButton("shelf", ImVec2(NW, 118));
		bool hov = ImGui::IsItemHovered();
		ImGui::PopID();
		ImVec2 m = io.MousePos;
		// the wheel, or a trackpad's sideways swipe (MouseWheelH)
		if (hov && maxoff > 0) {
			if (io.MouseWheel != 0)
				st.target -= io.MouseWheel * SLOT;
			if (io.MouseWheelH != 0)
				st.target -= io.MouseWheelH * SLOT * 0.5f;
			ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
			ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelX);
		}
		auto itemAt = [&](ImVec2 at) {
			if (at.y >= SY + 64)
				return -1;
			float k = std::round((at.x - (x0 + 54 - st.off)) / SLOT);
			if (k >= 0 && k < (float)items.size() && std::abs(at.x - (x0 + 54 - st.off + k * SLOT)) < SLOT / 2)
				return (int)k;
			return -1;
		};
		bool onLeft = maxoff > 0 && m.x < x0 + 32 && m.y >= SY && m.y <= SY + 44;
		bool onRight = maxoff > 0 && m.x > x1 - 32 && m.y >= SY && m.y <= SY + 44;
		int hovered = hov && !onLeft && !onRight && !st.dragged ? itemAt(m) : -1;
		if (hov && ImGui::IsMouseClicked(0)) {
			if (onLeft)
				st.target -= 3 * SLOT;
			else if (onRight)
				st.target += 3 * SLOT;
			else {
				st.pressed = true;
				st.dragged = false;
				st.pressX = m.x;
				st.pressOff = st.off;
				st.pressItem = itemAt(m);
				if (ImGui::IsMouseDoubleClicked(0) && st.pressItem >= 0 && !s_c2eggs[items[st.pressItem]].taken) {
					st.pressed = false; // double-click: placed
					s_selected = items[st.pressItem];
					TakeC2Egg(items[st.pressItem], false);
				}
			}
		}
		if (st.pressed) {
			if (std::abs(m.x - st.pressX) > 4 && maxoff > 0)
				st.dragged = true;
			if (st.dragged)
				st.target = st.off = std::clamp(st.pressOff - (m.x - st.pressX), 0.f, maxoff);
			if (!ImGui::IsMouseDown(0)) {
				if (!st.dragged && st.pressItem >= 0)
					s_selected = items[st.pressItem];
				st.pressed = st.dragged = false;
			}
		}
		st.target = std::clamp(st.target, 0.f, maxoff);
		st.off += (st.target - st.off) * std::min(1.f, io.DeltaTime * 14);
		if (std::abs(st.target - st.off) < 0.5f)
			st.off = st.target;
		st.off = std::clamp(st.off, 0.f, maxoff);
		// developer aid: OPENC2E_TEST_HATCHERY_MOUSE=1 drives the first shelf
		// with real mouse events: drag 250 px left, click the egg under the
		// middle, swipe a trackpad sideways; prints the results
		if (getenv("OPENC2E_TEST_HATCHERY_MOUSE") && g == s_c2eggs[0].group) {
			static int f = 0;
			f++;
			ImVec2 mid(x0 + NW / 2, SY - 10);
			if (f == 60)
				io.AddMousePosEvent(mid.x + 120, mid.y);
			if (f == 62)
				io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
			if (f >= 64 && f <= 84)
				io.AddMousePosEvent(mid.x + 120 - 250 * (f - 64) / 20.f, mid.y);
			if (f == 86)
				io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
			if (f == 90)
				fmt::print("* Hatchery mouse test: after a 250 px drag left, shelf at {:.0f} (max {:.0f}); selected {}\n", st.off, maxoff, s_c2eggs[s_selected].breed);
			if (f == 92)
				io.AddMousePosEvent(mid.x, mid.y);
			if (f == 94)
				io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
			if (f == 96)
				io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
			if (f == 100)
				fmt::print("* Hatchery mouse test: clicked the middle egg: selected {}\n", s_c2eggs[s_selected].breed);
			if (f == 102)
				io.AddMouseWheelEvent(3, 0); // a swipe: wheel x positive scrolls left
			if (f == 130)
				fmt::print("* Hatchery mouse test: after a swipe, shelf at {:.0f}\n", st.off);
		}
		if (hovered >= 0 && !st.pressed) {
			const C2EggInfo& egg = s_c2eggs[items[hovered]];
			ImGui::SetTooltip("%s\n%s", egg.label.c_str(),
				egg.taken ? "In the world now." : "Click to choose it; double-click to place it in the world.");
		}

		// the shadow the shelf casts on the wall, then the board
		dl->AddRectFilledMultiColor(ImVec2(x0, SY + 26), ImVec2(x1, SY + 66), IM_COL32(0, 0, 0, 120), IM_COL32(0, 0, 0, 120), IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0));
		DrawBoard(dl, ImVec2(x0, SY), g * 517.f + st.off, NW);
		// the nests, eggs and labels
		for (size_t k = 0; k < items.size(); k++) {
			float cx = std::floor(x0 + 54 + k * SLOT - st.off);
			if (cx < x0 - SLOT || cx > x1 + SLOT)
				continue;
			const C2EggInfo& egg = s_c2eggs[items[k]];
			bool sel = items[k] == s_selected;
			int v = NestVariant(egg);
			float ny = SY + SHELF_STAND - s_nest_bottom;
			dl->AddImage(s_contact.as<ImTextureID>(), ImVec2(cx - 42, SY - 3), ImVec2(cx + 42, SY + 27));
			if (sel || (int)k == hovered)
				dl->AddImage(s_glow.as<ImTextureID>(), ImVec2(cx - 70, ny + s_nest_rim - 92), ImVec2(cx + 70, ny + s_nest_rim + 48), ImVec2(0, 0), ImVec2(1, 1),
					IM_COL32(255, 255, 255, sel ? 255 : 110));
			Texture* back = (size_t)(2 * v) < s_nests.size() ? &s_nests[2 * v] : nullptr;
			Texture* front = (size_t)(2 * v + 1) < s_nests.size() ? &s_nests[2 * v + 1] : nullptr;
			if (back)
				DrawTexture(*back, ImVec2(cx - back->width / 2, ny));
			if (!egg.taken)
				if (Texture* e = EggTexture(egg))
					DrawTexture(*e, ImVec2(cx - e->width / 2, ny + s_nest_rim - e->height + 20 - (sel ? 6 : 0)));
			if (front)
				DrawTexture(*front, ImVec2(cx - front->width / 2, ny));
			DrawLabel(dl, egg.breed, sel, cx, SY + 28);
		}
		// where the shelf goes on out of sight, its end fades into the wall
		const int FADE = 40;
		for (int side = 0; side < 2; side++) {
			bool more = side == 0 ? st.off > 0.5f : st.off < maxoff - 0.5f;
			if (!more)
				continue;
			for (int i = 0; i < FADE; i++) {
				float x = side == 0 ? x0 + i : x1 - 1 - i;
				DrawWall(dl, origin, ImVec2(x, SY - 52), ImVec2(x + 1, SY + 66), 0.85f * (1 - i / (float)FADE));
			}
		}
		// the arrows, where the shelf is longer than the view
		if (maxoff > 0) {
			for (int side = 0; side < 2; side++) {
				bool can = side == 0 ? st.off > 0.5f : st.off < maxoff - 0.5f;
				float ax = side == 0 ? x0 + 6 : x1 - 30;
				dl->AddRectFilled(ImVec2(ax, SY + 4), ImVec2(ax + 24, SY + 40), IM_COL32(20, 16, 12, can ? 200 : 90), 6);
				DrawChevron(dl, ImVec2(ax + 12, SY + 22), side == 1, can ? IM_COL32(240, 220, 180, 255) : IM_COL32(110, 100, 90, 255));
			}
		}
		y = SY + 72;
	}
	if (y == origin.y + 10)
		dl->AddText(ImVec2(x0 + 16, y), kText, s_search[0] ? "No breed matches." : "No eggs.");
	(void)NH;
	return y - origin.y;
}

// a shelf scrolled so this egg is in view, in the middle where it can be
static void ScrollShelfTo(size_t index, float NW) {
	if (index >= s_c2eggs.size())
		return;
	int g = s_c2eggs[index].group;
	auto items = ShelfItems(g);
	int n = (int)items.size(), k = 0;
	for (int j = 0; j < n; j++)
		if (items[j] == index)
			k = j;
	float full = std::max(NW, 40 + n * SLOT + 40);
	Shelf& st = s_shelves[g];
	st.target = std::clamp(54 + k * SLOT - NW / 2, 0.f, std::max(0.f, full - NW));
}

// ---- the chosen egg's card ---------------------------------------------------

static void CardRow(const char* key, const std::string& value, float CW) {
	ImGui::SetCursorPosX(16);
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(kDim));
	ImGui::TextUnformatted(key);
	ImGui::PopStyleColor();
	ImGui::SameLine(90);
	ImGui::PushTextWrapPos(CW - 16);
	ImGui::TextUnformatted(value.c_str());
	ImGui::PopTextWrapPos();
}

static std::string AgesOf(const std::set<int>& stages) {
	static const char* names[] = {"baby", "child", "adolescent", "youth", "adult", "old", "senile"};
	auto name = [&](int s) { return s >= 0 && s < 7 ? std::string(names[s]) : fmt::format("stage {}", s); };
	if (stages.empty())
		return "none";
	int lo = *stages.begin(), hi = *stages.rbegin();
	if (stages.size() == 1)
		return name(lo);
	if ((int)stages.size() == hi - lo + 1)
		return name(lo) + " to " + name(hi);
	std::string out;
	for (int s : stages)
		out += (out.empty() ? "" : ", ") + name(s);
	return out;
}

static void DrawCard(float CW, float NH) {
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 p0 = ImGui::GetWindowPos();
	dl->AddRectFilled(p0, p0 + ImVec2(CW, NH), kCard, 6);
	dl->AddRect(p0, p0 + ImVec2(CW, NH), kEdge, 6);
	if (s_selected >= s_c2eggs.size()) {
		ImGui::SetCursorPos(ImVec2(16, 16));
		ImGui::TextUnformatted("Choose an egg.");
		return;
	}
	C2EggInfo& egg = s_c2eggs[s_selected];
	const BreedInfo* b = egg.species >= 0 ? FindBreed(egg.species, egg.slot) : nullptr;
	float mid = std::floor(p0.x + CW / 2);
	// the egg, large, under the lamp
	dl->AddImage(s_cardglow.as<ImTextureID>(), ImVec2(mid - 90, p0.y - 6), ImVec2(mid + 90, p0.y + 174), ImVec2(0, 0), ImVec2(1, 1),
		IM_COL32(255, 255, 255, egg.taken ? 70 : 255));
	if (egg.taken) {
		// taken: its nest, empty, as on the shelf
		int v = NestVariant(egg);
		for (int layer = 0; layer < 2; layer++)
			if (size_t(2 * v + layer) < s_nests.size()) {
				Texture& t = s_nests[2 * v + layer];
				ImVec2 sz(t.width * 2.f, t.height * 2.f);
				ImVec2 at(mid - std::floor(sz.x / 2), p0.y + 118 - s_nest_bottom * 2);
				dl->AddImage(t.as<ImTextureID>(), at, at + sz);
			}
	} else if (Texture* e = EggTexture(egg)) {
		float sc = std::floor(std::min(2.f, 110.f / std::max(1, e->height)));
		sc = std::max(1.f, sc);
		ImVec2 sz(e->width * sc, e->height * sc);
		ImVec2 at(mid - std::floor(sz.x / 2), p0.y + 14 + std::floor((110 - sz.y) / 2));
		dl->AddImage(e->as<ImTextureID>(), at, at + sz);
	}
	ImGui::SetCursorPos(ImVec2(16, 134));
	if (ImFont* f = NurseryFont(17)) {
		ImGui::PushFont(f);
		ImGui::PushTextWrapPos(CW - 16);
		ImGui::TextUnformatted(egg.breed.c_str());
		ImGui::PopTextWrapPos();
		ImGui::PopFont();
	} else
		ImGui::TextUnformatted(egg.breed.c_str());
	ImGui::SetCursorPosX(16);
	std::string sub;
	if (b)
		sub = fmt::format("{} breed {}", KindName(egg.group), breedSlotName(b->slot));
	else
		sub = fmt::format("{}, from a genome file", KindName(egg.group));
	ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(kGold), "%s", sub.c_str());
	ImGui::Dummy(ImVec2(1, 6));
	CardRow("Sex", egg.gender == 1 ? "male" : egg.gender == 2 ? "female" : egg.sexes == 1 ? "male (its breed has only male pictures)" : egg.sexes == 2 ? "female (its breed has only female pictures)" : "unknown until it hatches", CW);
	std::string genome;
	if (egg.imported)
		genome = egg.file.filename().string();
	else if (b && !b->genome.empty() && !findGeneticsFile(b->genome).empty())
		genome = b->genome;
	else
		genome = b && b->species == 1 ? "gren.gen" : b && b->species == 2 ? "ettn.gen" : "norn.gen";
	CardRow("Genome", genome, CW);
	if (b) {
		const char* credit = b->imported ? PackCredit(b->source) : nullptr;
		CardRow("By", credit ? credit : b->imported ? (b->source.empty() ? "imported" : b->source) : "Creature Labs", CW);
		// the life stages it has its own sprites for; at the others a
		// creature wears the nearest younger set
		CardRow("Sprites", b->stages.size() >= 7 ? "every age" : AgesOf(b->stages), CW);
	}
	if (egg.taken) {
		ImGui::Dummy(ImVec2(1, 6));
		ImGui::SetCursorPosX(16);
		ImGui::PushTextWrapPos(CW - 16);
		ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(kGold), "In the world now.");
		ImGui::PopTextWrapPos();
	}
	if (!s_status.empty()) {
		ImGui::Dummy(ImVec2(1, 6));
		ImGui::SetCursorPosX(16);
		ImGui::PushTextWrapPos(CW - 16);
		ImGui::TextUnformatted(s_status.c_str());
		ImGui::PopTextWrapPos();
	}
	// the buttons, at the foot
	ImGui::BeginDisabled(egg.taken);
	ImGui::SetCursorPos(ImVec2(16, NH - 84));
	ImGui::PushStyleColor(ImGuiCol_Button, kTabOn);
	if (ImGui::Button("Place in the world", ImVec2(CW - 32, 28)))
		TakeC2Egg(s_selected, false);
	ImGui::PopStyleColor();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("Lay it by the incubator. It hatches where it lies after a while, or at once if put in the incubator.");
	ImGui::SetCursorPos(ImVec2(16, NH - 48));
	ImGui::PushStyleColor(ImGuiCol_Button, kTab);
	if (ImGui::Button("Put in the incubator", ImVec2(CW - 32, 28)))
		TakeC2Egg(s_selected, true);
	ImGui::PopStyleColor();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("It hatches at once.");
	ImGui::EndDisabled();
}

// ---- the whole window's C2 contents -------------------------------------------

static void DrawNursery() {
	LoadNurseryArt();
	if (s_refill) {
		s_refill = false;
		std::string chosen = s_selected < s_c2eggs.size() ? s_c2eggs[s_selected].label : "";
		LoadC2Eggs();
		fmt::print("* Hatchery: opened again: every egg is back\n");
		s_selected = 0;
		for (size_t i = 0; i < s_c2eggs.size(); i++)
			if (s_c2eggs[i].label == chosen)
				s_selected = i;
		s_status.clear();
	}
	{
		std::vector<std::string> picked;
		{
			std::lock_guard<std::mutex> lock(s_import_mutex);
			picked.swap(s_import_pending);
		}
		for (auto& f : picked) {
			ImportGenome(f);
			s_status = s_import_message;
			if (!s_c2eggs.empty() && s_c2eggs.back().imported)
				s_selected = s_c2eggs.size() - 1;
		}
	}
	const float NW = 670, CW = 320;
	// the top row: a chip per shelf, search, finding eggs, importing
	int counts[4] = {0, 0, 0, 0};
	for (auto& e : s_c2eggs)
		counts[std::clamp(e.group, 0, 3)]++;
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10);
	for (int g = -1; g <= 3; g++) {
		if (g >= 0 && !counts[g])
			continue;
		std::string label = g < 0 ? fmt::format("All {}", s_c2eggs.size()) : fmt::format("{} {}", ShelfTitle(g), counts[g]);
		ImGui::PushStyleColor(ImGuiCol_Button, s_filter == g ? kTabOn : kFrame);
		if (ImGui::Button(label.c_str()))
			s_filter = g;
		ImGui::PopStyleColor();
		ImGui::SameLine();
	}
	ImGui::PopStyleVar();
	ImGui::SetNextItemWidth(150);
	ImGui::InputTextWithHint("##search", "Search breeds...", s_search, sizeof(s_search));
	if (s_hsort < 0) {
		std::string v = readPlayerSetting("hatchery-sort");
		s_hsort = v.empty() ? HSORT_SLOT : std::clamp(atoi(v.c_str()), 0, HSORT_COUNT - 1);
		// developer aid: OPENC2E_HATCHERY_SORT=n (not saved)
		if (const char* t = getenv("OPENC2E_HATCHERY_SORT"))
			s_hsort = std::clamp(atoi(t), 0, HSORT_COUNT - 1);
	}
	ImGui::SameLine(0, 12);
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Sort:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(140);
	if (ImGui::BeginCombo("##hsort", HSORT_NAMES[s_hsort])) {
		for (int k = 0; k < HSORT_COUNT; k++)
			if (ImGui::Selectable(HSORT_NAMES[k], k == s_hsort)) {
				s_hsort = k;
				if (!getenv("OPENC2E_HATCHERY_SORT"))
					writePlayerSetting("hatchery-sort", std::to_string(k));
			}
		ImGui::EndCombo();
	}
	{
		auto eggs = KitUnhatchedEggs();
		std::string find = eggs.empty() ? "No eggs in the world" : fmt::format("Find eggs in world ({})", eggs.size());
		const char* import = "Import Genome...";
		float bw = ImGui::CalcTextSize(find.c_str()).x + ImGui::CalcTextSize(import).x + 4 * ImGui::GetStyle().FramePadding.x + ImGui::GetStyle().ItemSpacing.x;
		ImGui::SameLine(NW + 12 + CW - bw);
		static size_t s_egg_next = 0;
		ImGui::BeginDisabled(eggs.empty());
		// developer aid: OPENC2E_TEST_FIND_EGG=1 presses it once
		static bool s_test_find = getenv("OPENC2E_TEST_FIND_EGG") != nullptr;
		bool pressed = ImGui::Button(find.c_str());
		if (s_test_find && !eggs.empty()) {
			s_test_find = false;
			pressed = true;
		}
		if (pressed && !eggs.empty()) {
			size_t i = s_egg_next % eggs.size();
			auto& egg = eggs[i];
			KitLookAt(egg);
			std::string genome = KitEggGenome(egg.get());
			s_status = fmt::format("Egg {} of {}{} at {:.0f}, {:.0f}: {}", i + 1, eggs.size(), genome.empty() ? "" : " (genome " + genome + ")",
				egg->x, egg->y, KitEggState(egg.get()));
			s_egg_next = i + 1;
		}
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Each click goes to the next creature's egg in the world that has not hatched\n(laid or taken from here; not the world's own grendel and ettin eggs).");
		ImGui::SameLine();
		if (ImGui::Button(import)) {
			static const SDL_DialogFileFilter filters[] = {{"Creatures 2 genomes", "gen"}, {"All files", "*"}};
			SDL_ShowOpenFileDialog(ImportGenomeChosen, nullptr, nullptr, filters, 2, nullptr, true);
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Make an egg from a genome file (.gen) anywhere on disk.");
	}
	ImGui::Dummy(ImVec2(1, 2));

	// the panel's height: the shelves', within what the screen allows; the
	// card needs about 390
	static float s_content = 620;
	float maxH = std::max(480.f, ImGui::GetMainViewport()->WorkSize.y - 160);
	float NH = std::clamp(s_content, 390.f, std::min(628.f, maxH));

	// developer aid: OPENC2E_TEST_HATCHERY="select=Frog,taken=Ron,search=no,
	// filter=1,place|incubator" (applied once the shelves are drawn)
	static bool s_test_done = false;
	static const char* s_test = getenv("OPENC2E_TEST_HATCHERY");
	if (s_test && !s_test_done && !s_c2eggs.empty()) {
		s_test_done = true;
		std::string all = s_test;
		std::string action;
		for (size_t at = 0; at <= all.size();) {
			size_t end = all.find(',', at);
			std::string item = all.substr(at, end == std::string::npos ? std::string::npos : end - at);
			std::string key = item.substr(0, item.find('=')), val = item.find('=') == std::string::npos ? "" : item.substr(item.find('=') + 1);
			auto find = [&](const std::string& n) {
				for (size_t i = 0; i < s_c2eggs.size(); i++)
					if (to_ascii_lowercase(s_c2eggs[i].breed) == to_ascii_lowercase(n))
						return i;
				return (size_t)-1;
			};
			if (key == "select" && find(val) != (size_t)-1) {
				s_selected = find(val);
				ScrollShelfTo(s_selected, NW);
				s_shelves[s_c2eggs[s_selected].group].off = s_shelves[s_c2eggs[s_selected].group].target;
			} else if (key == "taken" && find(val) != (size_t)-1)
				s_c2eggs[find(val)].taken = true;
			else if (key == "search")
				snprintf(s_search, sizeof(s_search), "%s", val.c_str());
			else if (key == "filter")
				s_filter = atoi(val.c_str());
			else if (key == "place" || key == "incubator" || key == "reopen")
				action = action.empty() || key != "reopen" ? key : action + "+reopen";
			if (end == std::string::npos)
				break;
			at = end + 1;
		}
		if (!action.empty() && s_selected < s_c2eggs.size()) {
			fmt::print("* Hatchery test: {} {}\n", action, s_c2eggs[s_selected].label);
			if (action.rfind("place", 0) == 0 || action.rfind("incubator", 0) == 0)
				TakeC2Egg(s_selected, action.rfind("incubator", 0) == 0);
			fmt::print("* Hatchery test: {}\n", s_status);
			if (action.find("reopen") != std::string::npos) { // closed and opened again
				SetHatcheryOpen(false);
				SetHatcheryOpen(true);
			}
		}
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
	ImGui::BeginChild("nursery", ImVec2(NW, NH), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
	s_content = DrawShelves(NW, NH);
	ImGui::SetCursorPos(ImVec2(0, s_content));
	ImGui::Dummy(ImVec2(1, 1));
	ImGui::EndChild();
	ImGui::SameLine(0, 12);
	ImGui::BeginChild("card", ImVec2(CW, NH), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	DrawCard(CW, NH);
	ImGui::EndChild();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
}

void LoadHatchery() {
	if (engine.version == 1) {
		s_hatcherybackground = LoadImageWithTransparentTopLeft("hatchery/hatchery.bmp")[0];
		s_hatcherymask = LoadImageWithTransparentTopLeft("hatchery/htchmask.bmp")[0];

		for (auto& e : s_c1eggs) {
			e.texture = LoadImageWithTransparentTopLeft(e.name)[0];
		}
	} else if (engine.version == 2) {
		s_omelette = LoadImageWithTransparentTopLeft("Applet Data/Omelette.s16");
		LoadC2Eggs();
	}

	s_hatchery_loaded = true;
}

void UnloadHatchery() {
	s_hatcherybackground = {};
	s_hatcherymask = {};
	s_omelette = {};
	s_labels.clear();
	for (auto& e : s_c1eggs) {
		e.texture = {};
	}
	s_hatchery_loaded = false;
}

bool IsHatcheryEnabled() {
	return engine.version == 1 || engine.version == 2;
}

void SetHatcheryOpen(bool value) {
	if (value && !s_hatchery_open && s_hatchery_loaded)
		s_refill = true; // opened again: every egg back (owner, 2026-11-12)
	s_hatchery_open = value;
	if (value)
		s_hatchery_minimized = false; // opened again from the toolbar: shown
}

// C2: minimized to a chip on the status bar, like the kits (owner, 2026-11-11)
void DrawHatcheryChip() {
	if (engine.version != 2 || !s_hatchery_open || !s_hatchery_minimized)
		return;
	if (KitChipButton("Hatchery", 0, "Restore the Hatchery")) {
		s_hatchery_minimized = false;
		s_hatchery_focus = true;
	}
}

void DrawHatchery() {
	if (!(engine.version == 1 || engine.version == 2)) {
		return;
	}

	// developer aid: with OPENC2E_OPEN_KITS, OPENC2E_KITS_MINIMIZED starts it minimized
	static bool s_test_min = getenv("OPENC2E_KITS_MINIMIZED") != nullptr;
	if (s_test_min && s_hatchery_open) {
		s_test_min = false;
		s_hatchery_minimized = true;
	}
	if (s_hatchery_open && !s_hatchery_minimized && s_hatchery_focus) {
		ImGui::SetNextWindowFocus();
		s_hatchery_focus = false;
	}
	if (s_hatchery_open && !s_hatchery_minimized && ImGui::Begin("Hatchery", &s_hatchery_open,
								   ImGuiWindowFlags_NoScrollbar |
								   ImGuiWindowFlags_NoScrollWithMouse |
								   ImGuiWindowFlags_NoCollapse |
								   // fits what it holds (a remembered smaller size
								   // hid the Find eggs button, 2026-11-11)
								   ImGuiWindowFlags_AlwaysAutoResize)) {
		if (engine.version == 2 && KitTitleMinimizeButton())
			s_hatchery_minimized = true;
		if (!s_hatchery_loaded) {
			LoadHatchery();
		}

		if (engine.version == 1) {
			// background
			ImVec2 p = ImGui::GetCursorScreenPos();
			DrawTexture(s_hatcherybackground, p);
			ImGui::Dummy(ImVec2(320, 240));

			/* fan animation */
			for (unsigned int i = 0; i < 4; i++) {
				// TODO
			}

			/* 'off' state for the light */
			// TODO

			/* eggs */
			for (auto& e : s_c1eggs) {
				if (!e.texture)
					continue;

				DrawTexture(e.texture, p + ImVec2(e.x, e.y));
			}

			/* mask which goes over the eggs */
			DrawTexture(s_hatcherymask, p + ImVec2(0, 168));

			/* gender marker animation */
			// TODO
		} else if (engine.version == 2) {
			DrawNursery();
		}

		ImGui::End();
	}
}

} // namespace Openc2eImgui