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


#include "Engine.h"
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
	PlaceEggInHatchery(egg);
	s_c2eggs.push_back(egg);
	s_import_message = fmt::format("Made an egg from {}.", name);
}

static std::vector<std::filesystem::path> findC2EggFiles() {
	std::vector<std::filesystem::path> result;
	for (auto& dir : getMainDirectories()) {
		std::error_code ec;
		for (auto& sub : std::filesystem::directory_iterator(dir, ec)) {
			std::string name = to_ascii_lowercase(sub.path().filename().string());
			if (name != "eggs" || !sub.is_directory())
				continue;
			for (auto& f : std::filesystem::directory_iterator(sub.path(), ec)) {
				if (to_ascii_lowercase(f.path().extension().string()) == ".egg")
					result.push_back(f.path());
			}
		}
	}
	std::sort(result.begin(), result.end());
	return result;
}

static bool readC2Egg(const std::filesystem::path& file, C2EggInfo& egg) {
	std::ifstream in(file, std::ios::binary);
	std::vector<uint8_t> d((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	if (d.size() < 30 || std::memcmp(d.data(), "eggf", 4) != 0)
		return false;
	egg.file = file;
	egg.gender = d[4];
	egg.frame = d[5];
	egg.moniker = std::string(reinterpret_cast<const char*>(&d[14]), 4);
	egg.moniker.erase(std::find(egg.moniker.begin(), egg.moniker.end(), '\0'), egg.moniker.end());
	size_t pos = 26;
	while (pos + 4 <= d.size() && egg.genomes.size() < 2) {
		uint32_t len = d[pos] | (d[pos + 1] << 8) | (d[pos + 2] << 16) | ((uint32_t)d[pos + 3] << 24);
		pos += 4;
		if (len < 4 || pos + len > d.size() || std::memcmp(&d[pos], "dna", 3) != 0)
			break;
		egg.genomes.emplace_back(d.begin() + pos, d.begin() + pos + len);
		pos += len;
	}
	return !egg.genomes.empty();
}

// Sends a Hatchery egg into the world near the incubator: the world's own egg
// object (2 5 2), whose scripts develop and hatch it. The egg's genomes are
// written to the user data folder under short unique tokens for NEW: GENE.
static void TakeC2Egg(size_t index) {
	C2EggInfo& egg = s_c2eggs[index];
	if (egg.taken)
		return;
	std::string mum = fmt::format("qh{:02d}", (int)index % 100);
	std::string dad = egg.genomes.size() > 1 ? fmt::format("qi{:02d}", (int)index % 100) : mum;
	{
		FileWriter w = createUserGeneticsFile(mum + ".gen");
		w.write(egg.genomes[0].data(), egg.genomes[0].size());
	}
	if (egg.genomes.size() > 1) {
		FileWriter w = createUserGeneticsFile(dad + ".gen");
		w.write(egg.genomes[1].data(), egg.genomes[1].size());
	}

	int frame = std::clamp(egg.frame, 0, 16);
	int x = rand_int32(5000, 5200);
	std::string script = fmt::format("new: simp eggs 8 {} 2000 0\n", frame * 8);
	script += "pose 0\n"; // start of development; the egg's timer script advances it
	script += "setv cls2 2 5 2\nsetv attr 195\n";
	script += fmt::format("new: gene tokn {} tokn {} obv0\n", mum, dad);
	script += fmt::format("setv obv1 {}\n", egg.gender);
	script += "tick 2400\n";
	script += fmt::format("mvto {} 750\n", x);
	script += "setv grav 1\n";
	std::string err = engine.executeNetwork(script);
	if (!err.empty()) {
		fmt::print("Couldn't hatch egg {}: {}\n", egg.file.filename().string(), err);
		return;
	}
	egg.taken = true;
	engine.camera->trackAgent(AgentRef(), 0, 0, brittle, none);
	engine.camera->moveToGlobal(x - (int)engine.camera->getWidth() / 2, 750 - (int)engine.camera->getHeight() / 2, jump);
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
		s_hatcherybackground = LoadImageWithTransparentTopLeft("Applet Data/Hatchery.bmp")[0];
		s_omelette = LoadImageWithTransparentTopLeft("Applet Data/Omelette.s16");

		s_c2eggs.clear();
		for (auto& f : findC2EggFiles()) {
			C2EggInfo egg;
			if (readC2Egg(f, egg))
				s_c2eggs.push_back(egg);
			if (s_c2eggs.size() == 10)
				break; // the hatchery holds ten
		}
		fmt::print("* Hatchery: {} eggs from the Eggs folder\n", s_c2eggs.size());
		for (auto& egg : s_c2eggs)
			egg.x = 0;
		for (auto& egg : s_c2eggs)
			PlaceEggInHatchery(egg);
		std::sort(s_c2eggs.begin(), s_c2eggs.end(), [](const auto& l, const auto& r) { return l.y < r.y; });
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
}

void DrawHatchery() {
	if (!(engine.version == 1 || engine.version == 2)) {
		return;
	}

	if (s_hatchery_open && ImGui::Begin("Hatchery", &s_hatchery_open,
							   (engine.version == 2 ? ImGuiWindowFlags_MenuBar : 0) |
								   ImGuiWindowFlags_NoScrollbar |
								   ImGuiWindowFlags_NoScrollWithMouse |
								   ImGuiWindowFlags_NoCollapse)) {
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
			// background
			ImVec2 p = ImGui::GetCursorScreenPos();
			DrawTexture(s_hatcherybackground, p);
			ImGui::Dummy(ImVec2(320, 275));

			// eggs: click one to send it into the world
			for (size_t i = 0; i < s_c2eggs.size(); ++i) {
				auto& egg = s_c2eggs[i];
				if (egg.taken)
					continue;
				auto& tex = s_omelette[std::clamp(egg.frame, 0, 16)];
				DrawTexture(tex, p + ImVec2(egg.x, egg.y));
				ImGui::SetCursorScreenPos(p + ImVec2(egg.x, egg.y));
				ImGui::PushID((int)i);
				if (ImGui::InvisibleButton("egg", ImVec2(tex.width, tex.height)))
					TakeC2Egg(i);
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s egg (%s)\nClick to place it in the world, then put it in the incubator to hatch.",
						egg.gender == 2 ? "Female" : egg.gender == 1 ? "Male" : "Unknown-sex", egg.moniker.c_str());
				ImGui::PopID();
			}
			ImGui::SetCursorScreenPos(p + ImVec2(0, 275));
			ImGui::Dummy(ImVec2(0, 0));
			if (!s_import_message.empty())
				ImGui::TextUnformatted(s_import_message.c_str());
			// TODO: gender markers and animation
			// TODO: place shadows

			// leftsideimg
			DrawTexture(s_omelette[18], p + ImVec2(75, 176), 0.5);
			// rightsideimg
			DrawTexture(s_omelette[19], p + ImVec2(181, 162), 0.5);
			// grabberimg
			DrawTexture(s_omelette[20], p + ImVec2(150, 93));
			// midsideimg
			DrawTexture(s_omelette[21], p + ImVec2(141, 177));
		}

		ImGui::End();
	}
}

} // namespace Openc2eImgui