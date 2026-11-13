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

#include <array>
#include <vector>
#include <fmt/core.h>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <algorithm>
#include <imgui.h>
#include <imgui_internal.h>
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
	std::vector<std::vector<uint8_t>> genomes;
	int x = 0;
	int y = 0;
	bool taken = false;
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

static void PlaceEggInHatchery(C2EggInfo& egg);

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
	egg.frame = rand_int32(0, 10);
	egg.moniker = std::filesystem::path(path).stem().string().substr(0, 4);
	egg.genomes.push_back(d);
	egg.breed = BreedNameOfGenome(d);
	PlaceEggInHatchery(egg);
	s_c2eggs.push_back(egg);
	s_import_message = fmt::format("Made an egg from {}.", name);
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

static void TakeC2Egg(size_t index) {
	C2EggInfo& egg = s_c2eggs[index];
	if (egg.taken)
		return;
	std::vector<std::vector<uint8_t>> genomes = egg.genomes;
	int gender = egg.gender;
	if (gender == 0) // decided now: either, or the one its breed has
		gender = egg.sexes == 1 ? 1 : egg.sexes == 2 ? 2 : rand_int32(1, 2);
	// tokens 4 characters, unique per Hatchery egg: q + 2 digits + m/d
	std::string err;
	if (!LayC2Egg(genomes, gender, egg.frame, fmt::format("q{:02d}", (int)index % 100), err)) {
		fmt::print("Couldn't hatch egg {}: {}\n", egg.file.filename().string(), err);
		return;
	}
	egg.taken = true;
}

// Finds a spot for an egg in the Hatchery tray, at least 20 px from the
// other eggs (gives up after 50 tries).
static void PlaceEggInHatchery(C2EggInfo& egg) {
	if (s_omelette.empty())
		return;
	int frame = std::clamp(egg.frame, 0, 16);
	int x = 0, y = 0, tries = 0;
	while (x == 0 && tries < 50) {
		x = rand_int32(80, 240 - s_omelette[frame].width);
		y = rand_int32(110, 230 - s_omelette[frame].height);
		for (auto& other : s_c2eggs) {
			if (&other == &egg || other.taken || other.x == 0)
				continue;
			float xdist = x - other.x, ydist = y - other.y;
			if (xdist * xdist + ydist * ydist < 20 * 20)
				x = 0; // too close: try again
		}
		tries++;
	}
	egg.x = x ? x : 160;
	egg.y = y ? y : 170;
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

		// one egg of every breed in the game, of a sex drawn at random
		// from those the breed has pictures for (owner, 2026-11-12); before,
		// the ten eggs of C2's Eggs folder. Each is the breed's genome (its
		// own, or the species' standard one) wearing the breed's parts, as
		// the Breeder's Kit lays them. Norns first, then grendels, ettins.
		s_c2eggs.clear();
		{
			std::vector<const BreedInfo*> breeds;
			for (auto& b : KnownBreeds())
				if (b.parts.count('a') && b.parts.count('b')) // a head and a body
					breeds.push_back(&b);
			auto kindOf = [](const BreedInfo* b) { return b->kind >= 0 ? b->kind : b->species; };
			std::stable_sort(breeds.begin(), breeds.end(), [&](const BreedInfo* l, const BreedInfo* r) {
				return kindOf(l) != kindOf(r) ? kindOf(l) < kindOf(r) : l->species != r->species ? l->species < r->species : l->slot < r->slot;
			});
			std::string kinds;
			for (const BreedInfo* b : breeds) {
				auto genome = BreedBaseGenome(*b);
				if (genome.empty())
					continue;
				C2EggInfo egg;
				egg.genomes.push_back(GenomeWithBreed(genome, *b));
				// its sex unknown until it is placed in the world (owner,
				// 2026-11-12), then drawn from the sexes its breed has
				egg.gender = 0;
				egg.sexes = (b->male ? 1 : 0) | (b->female ? 2 : 0);
				egg.frame = rand_int32(0, 10);
				egg.breed = b->name.empty() ? BreedLabel(*b) : b->name;
				egg.label = BreedLabel(*b);
				s_c2eggs.push_back(egg);
				kinds += fmt::format("{}{}", kinds.empty() ? "" : ", ", egg.label);
			}
			fmt::print("* Hatchery: {} eggs, one of each breed: {}\n", s_c2eggs.size(), kinds);
		}
		for (auto& egg : s_c2eggs)
			egg.x = 0;
		for (auto& egg : s_c2eggs)
			PlaceEggInHatchery(egg);
	}

	s_hatchery_loaded = true;
}

void UnloadHatchery() {
	s_hatcherybackground = {};
	s_hatcherymask = {};
	s_omelette = {};
	for (auto& e : s_c1eggs) {
		e.texture = {};
	}
	s_hatchery_loaded = false;
}

bool IsHatcheryEnabled() {
	return engine.version == 1 || engine.version == 2;
}

void SetHatcheryOpen(bool value) {
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
							   (engine.version == 2 ? ImGuiWindowFlags_MenuBar : 0) |
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
			if (ImGui::BeginMenuBar()) {
				if (ImGui::BeginMenu("File")) {
					if (ImGui::MenuItem("Import Genome...")) {
						static const SDL_DialogFileFilter filters[] = {{"Creatures 2 genomes", "gen"}, {"All files", "*"}};
						SDL_ShowOpenFileDialog(ImportGenomeChosen, nullptr, nullptr, filters, 2, nullptr, true);
					}
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Make an egg from a genome file (.gen) anywhere on disk.");
					ImGui::EndMenu();
				}
				ImGui::EndMenuBar();
			}
			{
				std::vector<std::string> picked;
				{
					std::lock_guard<std::mutex> lock(s_import_mutex);
					picked.swap(s_import_pending);
				}
				for (auto& f : picked)
					ImportGenome(f);
			}
			// the eggs as tiles (owner, 2026-11-12: the kits' own style, not
			// C2's incubator picture): each egg's own picture and its sex;
			// click one to send it into the world
			const float TW = 96, TH = 106;
			const int per = 5;
			ImDrawList* dl = ImGui::GetWindowDrawList();
			int shown = 0;
			for (size_t i = 0; i < s_c2eggs.size(); ++i) {
				auto& egg = s_c2eggs[i];
				if (egg.taken)
					continue;
				if (shown++ % per)
					ImGui::SameLine();
				ImVec2 at = ImGui::GetCursorScreenPos();
				ImGui::PushID((int)i);
				bool click = ImGui::InvisibleButton("egg", ImVec2(TW, TH));
				bool hov = ImGui::IsItemHovered();
				if (hov)
					ImGui::SetTooltip("%s egg (%s)\nClick to place it in the world. It hatches where it lies, or sooner in the incubator.",
						egg.gender == 2 ? "Female" : egg.gender == 1 ? "Male" : "Unknown-sex",
						!egg.label.empty() ? egg.label.c_str() : egg.breed.empty() ? egg.moniker.c_str() : egg.breed.c_str());
				ImGui::PopID();
				dl->AddRectFilled(at, at + ImVec2(TW, TH), hov ? IM_COL32(38, 40, 48, 255) : IM_COL32(26, 28, 34, 255), 5);
				dl->AddRect(at, at + ImVec2(TW, TH), hov ? IM_COL32(205, 170, 95, 255) : IM_COL32(70, 72, 80, 255), 5);
				if (!s_omelette.empty()) {
					auto& tex = s_omelette[std::clamp(egg.frame, 0, std::min(16, (int)s_omelette.size() - 1))];
					DrawTexture(tex, at + ImVec2((TW - tex.width) / 2, 8 + (TH - 32 - tex.height) / 2));
				}
				const char* sex = egg.gender == 2 ? "female" : egg.gender == 1 ? "male" : "sex unknown";
				ImU32 col = egg.gender == 2 ? IM_COL32(240, 150, 190, 255) : egg.gender == 1 ? IM_COL32(130, 180, 250, 255) : IM_COL32(170, 170, 170, 255);
				ImVec2 ts = ImGui::CalcTextSize(sex);
				dl->AddText(at + ImVec2((TW - ts.x) / 2, TH - ts.y - 6), col, sex);
				// its breed (shortened to fit the tile)
				std::string name = egg.breed.empty() ? egg.moniker : egg.breed;
				if (ImGui::CalcTextSize(name.c_str()).x > TW - 8) {
					while (name.size() > 2 && ImGui::CalcTextSize((name + ".").c_str()).x > TW - 8)
						name.pop_back();
					name += ".";
				}
				ImVec2 ns = ImGui::CalcTextSize(name.c_str());
				dl->AddText(at + ImVec2((TW - ns.x) / 2, TH - ts.y - ns.y - 8), IM_COL32(222, 224, 230, 255), name.c_str());
				if (click)
					TakeC2Egg(i);
			}
			if (!shown)
				KitNote("The Hatchery is empty: every breed's egg has been taken. It fills again when the world is next loaded; File > Import Genome adds more.");
			// (the breed dropdown, "As in the egg", was removed: unclear;
			// owner, 2026-11-11. Breeds are chosen in the Breeder's Kit.)
			// egg locator (owner, 2026-11-11): each click goes to the next
			// unhatched egg in the world
			{
				static size_t s_egg_next = 0;
				static std::string s_egg_line;
				auto eggs = KitUnhatchedEggs();
				std::string label = eggs.empty() ? "No creature eggs in the world" : fmt::format("Find eggs ({})", eggs.size());
				ImGui::BeginDisabled(eggs.empty());
				// developer aid: OPENC2E_TEST_FIND_EGG=1 presses it once
				static bool s_test_find = getenv("OPENC2E_TEST_FIND_EGG") != nullptr;
				bool pressed = ImGui::Button(label.c_str());
				if (s_test_find && !eggs.empty()) {
					s_test_find = false;
					pressed = true;
				}
				if (pressed && !eggs.empty()) {
					size_t i = s_egg_next % eggs.size();
					auto& egg = eggs[i];
					KitLookAt(egg);
					std::string genome = KitEggGenome(egg.get());
					const char* kind = "egg";
					s_egg_line = fmt::format("{} of {}: {}{} at {:.0f}, {:.0f}, {}", i + 1, eggs.size(), kind, genome.empty() ? "" : " (genome " + genome + ")",
						egg->x, egg->y, KitEggState(egg.get()));
					s_egg_next = i + 1;
				}
				ImGui::EndDisabled();
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
					ImGui::SetTooltip("Each click goes to the next creature's egg in the world that has not hatched\n(laid or taken from here; not the world's own grendel and ettin eggs).");
				if (!s_egg_line.empty()) {
					ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 320);
					ImGui::TextUnformatted(s_egg_line.c_str());
					ImGui::PopTextWrapPos();
				}
			}
			if (!s_import_message.empty())
				ImGui::TextUnformatted(s_import_message.c_str());
		}

		ImGui::End();
	}
}

} // namespace Openc2eImgui