#include "AgentInjector.h"
#include "Agent.h"
#include <ctime>
#include <map>
#include <set>
#include "creatures/CreatureSpeech.h"

#include "C2Kits.h"
#include "C2KitsInternal.h"
#include "CobManager.h"
#include "PathResolver.h"
#include "common/Ascii.h"
#include "Engine.h"
#include "ImGuiUtils.h"
#include "World.h"
#include "common/backend/Backend.h"
#include "common/backend/BackendTexture.h"

#include <SDL3/SDL_dialog.h>
#include <algorithm>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <imgui.h>
#include <mutex>
#include <imgui_internal.h>

namespace Openc2eImgui {

static bool s_read_agents = false;
// each listed object's thumbnail, made when first drawn (by file and name)
static std::map<std::string, Texture> s_thumbs;
static size_t s_object_selected_index = 0;

static CobManager s_object_injector;

static bool s_agent_injector_open = false;
static bool s_agent_injector_minimized = false; // C2: a chip on the status bar, like the kits
static bool s_agent_injector_focus = false;

// ---- importing objects (owner, 2026-11-09) -------------------------------------
// COB files, and zips of them (with any loose sprites or sounds), are copied
// into the user data folder (Objects, Images, Sounds), never the game's own
// folder, so they show in the list. Nothing is overwritten: a COB whose name
// is taken gets " (2)" and so on; a sprite or sound whose name is taken by a
// different file is left out and named in the report (renaming it would break
// the scripts that use it).
namespace fs = std::filesystem;
static std::mutex s_pick_mutex;
static std::vector<std::string> s_picked;
static std::vector<std::string> s_import_report;

static void SDLCALL ObjectFilesChosen(void*, const char* const* files, int) {
	if (!files)
		return;
	std::lock_guard<std::mutex> lock(s_pick_mutex);
	for (const char* const* f = files; *f; f++)
		s_picked.push_back(*f);
}

static std::vector<uint8_t> readAllBytes(const fs::path& p) {
	std::ifstream in(p, std::ios::binary);
	return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

// install one file by its kind; a line for the report
static std::string installObjectFile(const std::string& name, const std::vector<uint8_t>& data) {
	std::string ext = to_ascii_lowercase(fs::path(name).extension().string());
	fs::path dir;
	bool renameable = false;
	if (ext == ".cob") {
		dir = getUserDataDir() / "Objects";
		renameable = true;
	} else if (ext == ".s16" || ext == ".c16" || ext == ".spr") {
		dir = getUserDataDir() / "Images";
	} else if (ext == ".wav" || ext == ".mng") {
		dir = getUserDataDir() / "Sounds";
	} else {
		return fmt::format("{}: left out (not an object, sprite or sound)", name);
	}
	std::error_code ec;
	fs::create_directories(dir, ec);
	fs::path dest = dir / fs::path(name).filename();
	auto same = [&](const fs::path& p) { return readAllBytes(p) == data; };
	// a COB the game's own Objects folder already has, unchanged
	if (renameable) {
		fs::path existing = findCobFile(fs::path(name).filename());
		if (!existing.empty() && fs::exists(existing, ec)) {
			if (same(existing))
				return fmt::format("{}: already there", name);
			// a different COB under the game's name: this one gets a new name
			// (unless an earlier import already gave it one)
			for (int n = 2; fs::exists(dest, ec) || dest.filename() == fs::path(name).filename(); n++) {
				if (fs::exists(dest, ec) && dest.filename() != fs::path(name).filename() && same(dest))
					return fmt::format("{}: already there (as {})", name, dest.filename().string());
				dest = dir / fmt::format("{} ({}){}", fs::path(name).stem().string(), n, ext);
			}
		}
	}
	// a sprite or sound the game already has under that name
	if (!renameable) {
		fs::path existing = ext == ".wav" || ext == ".mng" ? findSoundFile(fs::path(name).filename()) : findImageFile(fs::path(name).filename());
		if (!existing.empty() && fs::exists(existing, ec)) {
			if (same(existing))
				return fmt::format("{}: already there", name);
			return fmt::format("{}: left out (another file of that name is there; objects that use it may look or sound wrong)", name);
		}
	}
	if (fs::exists(dest, ec)) {
		if (same(dest))
			return fmt::format("{}: already there", name);
		if (!renameable)
			return fmt::format("{}: left out (another file of that name is there)", name);
		for (int n = 2; fs::exists(dest, ec); n++) {
			dest = dir / fmt::format("{} ({}){}", fs::path(name).stem().string(), n, ext);
			if (fs::exists(dest, ec) && same(dest))
				return fmt::format("{}: already there (as {})", name, dest.filename().string());
		}
	}
	std::ofstream out(dest, std::ios::binary);
	out.write((const char*)data.data(), data.size());
	if (!out)
		return fmt::format("{}: couldn't write it", name);
	return fmt::format("{}: added{}", name, dest.filename() != fs::path(name).filename() ? fmt::format(" as {}", dest.filename().string()) : "");
}

// ones that clash with the project's changes (hidden-cobs.txt) are never
// offered; objects built into every world (builtin-cobs.txt) and ones the
// player uninstalled (uninstalled-cobs.txt) have their own tabs (owner,
// 2026-11-13), so they can be put back
static std::set<std::string> s_builtin, s_uninstalled;
enum { SET_INJECTOR, SET_BUILTIN, SET_UNINSTALLED };
static int s_set = SET_INJECTOR;

static void readLists() {
	auto b = CobManager::readNameList(CobManager::builtinListFile());
	auto u = CobManager::readNameList(CobManager::uninstalledListFile());
	s_builtin = std::set<std::string>(b.begin(), b.end());
	s_uninstalled = std::set<std::string>(u.begin(), u.end());
}

static int setOf(const std::string& name) {
	if (s_builtin.count(name))
		return SET_BUILTIN;
	if (s_uninstalled.count(name))
		return SET_UNINSTALLED;
	return SET_INJECTOR;
}

static void hideListed() {
	auto hidden = CobManager::readNameList(getUserDataDir() / "Additions" / "hidden-cobs.txt");
	auto& objs = s_object_injector.objects;
	objs.erase(std::remove_if(objs.begin(), objs.end(), [&](auto& o) {
		return std::find(hidden.begin(), hidden.end(), o.name) != hidden.end();
	}),
		objs.end());
	if (s_object_selected_index >= objs.size())
		s_object_selected_index = 0;
	readLists();
}

static std::string s_list_message; // what the last move between tabs did

static std::string todayText() {
	std::time_t t = std::time(nullptr);
	char buf[32];
	std::strftime(buf, sizeof(buf), "%Y-%m-%d", std::localtime(&t));
	return buf;
}

// built into every world: in builtin-cobs.txt, so every world gets it at
// each load. In this world, the ones the player added by hand become the
// built-in ones (not saved with the world any more, so a load doesn't make
// a second set); with none here, one is made now.
static void buildIntoWorlds(const CobManager::CobFileInfo& o) {
	if (!CobManager::addToNameList(CobManager::builtinListFile(), o.name, "added from the Agent Injector, " + todayText())) {
		s_list_message = fmt::format("Couldn't write {}.", CobManager::builtinListFile().string());
		return;
	}
	int n = CobManager::adoptHandAdded(o.name);
	if (n == 0) {
		try {
			s_object_injector.inject(o, false);
		} catch (std::exception& e) {
			s_list_message = fmt::format("{} is built into every world from the next load (adding it now failed: {}).", o.name, e.what());
			readLists();
			return;
		}
	}
	s_list_message = n ? fmt::format("{} is built into every world. The {} you added here are now the built-in ones.", o.name, n)
					   : fmt::format("{} is built into every world, and was added to this one now.", o.name);
	readLists();
}

static void takeOutOfWorlds(const CobManager::CobFileInfo& o) {
	CobManager::removeFromNameList(CobManager::builtinListFile(), o.name);
	s_list_message = fmt::format("{} is back in the injector. Worlds no longer get it at each load; the ones in this world go when it next loads.", o.name);
	readLists();
}

static void uninstall(const CobManager::CobFileInfo& o) {
	if (!CobManager::addToNameList(CobManager::uninstalledListFile(), o.name, "")) {
		s_list_message = fmt::format("Couldn't write {}.", CobManager::uninstalledListFile().string());
		return;
	}
	s_list_message = fmt::format("{} is uninstalled: it is under Uninstalled, where Reinstall puts it back.", o.name);
	readLists();
}

static void reinstall(const CobManager::CobFileInfo& o) {
	CobManager::removeFromNameList(CobManager::uninstalledListFile(), o.name);
	s_list_message = fmt::format("{} is back in the injector.", o.name);
	readLists();
}

static void importPicked() {
	std::vector<std::string> picked;
	{
		std::lock_guard<std::mutex> lock(s_pick_mutex);
		picked.swap(s_picked);
	}
	if (picked.empty())
		return;
	s_import_report.clear();
	for (auto& path : picked) {
		std::string ext = to_ascii_lowercase(fs::path(path).extension().string());
		if (ext == ".zip" || ext == ".exe") {
			std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
			std::string err, ierr;
			auto bytes = readAllBytes(path);
			if (!(ext == ".exe" && Openc2eImgui::KitReadInstaller(bytes, files, ierr)) && !Openc2eImgui::KitReadZip(bytes, files, err)) {
				if (ext == ".exe")
					err = ierr;
				s_import_report.push_back(fmt::format("{}: {}", fs::path(path).filename().string(), err));
				continue;
			}
			for (auto& [name, data] : files)
				if (!name.empty())
					s_import_report.push_back(installObjectFile(name, data));
		} else {
			s_import_report.push_back(installObjectFile(fs::path(path).filename().string(), readAllBytes(path)));
		}
	}
	s_object_injector.update();
	hideListed();
	s_thumbs.clear();
	if (getenv("OPENC2E_TEST_IMPORT_OBJECTS"))
		for (auto& line : s_import_report)
			fmt::print("* IMPORT {}\n", line);
}

static Texture& thumbOf(const CobManager::CobFileInfo& o) {
	std::string key = o.filename.string() + "|" + o.name;
	auto it = s_thumbs.find(key);
	if (it == s_thumbs.end()) {
		Image img = s_object_injector.getPicture(o);
		Texture t;
		if (img.width > 0 && img.height > 0)
			t = get_backend()->createTextureFromImage(img);
		it = s_thumbs.emplace(key, t).first;
	}
	return it->second;
}

// a thumbnail fitted into a box (on black, as the COBs draw them), centred
static void drawThumb(ImDrawList* dl, const CobManager::CobFileInfo& o, ImVec2 at, float box) {
	dl->AddRectFilled(at, at + ImVec2(box, box), IM_COL32(0, 0, 0, 255), 3);
	Texture& t = thumbOf(o);
	if (!t)
		return;
	float k = std::min(1.0f, std::min(box / t.width, box / t.height));
	ImVec2 sz(t.width * k, t.height * k);
	ImVec2 p = at + ImVec2((box - sz.x) / 2, (box - sz.y) / 2);
	dl->AddImage(t.as<ImTextureID>(), p, p + sz);
}

// what kind of thing an object is: the game's word for its category, as a
// heading (plural) or a label; respelt where the game's word reads oddly
static std::string kindWord(int category, bool plural) {
	if (category < 0)
		return plural ? "Other objects" : "object";
	std::string w = to_ascii_lowercase(KitCategoryNoun(category));
	static const std::map<std::string, std::pair<const char*, const char*>> names = {
		{"badcritter", {"pest", "Pests"}}, {"implement", {"gadget", "Gadgets"}}, {"mover", {"vehicle", "Vehicles"}},
		{"dispensor", {"dispenser", "Dispensers"}}, {"badplant", {"bad plant", "Bad plants"}}, {"badbug", {"bad bug", "Bad bugs"}},
		{"mediabox", {"media box", "Media boxes"}}, {"leftright", {"switch", "Switches"}}, {"cliff edge", {"cliff edge", "Cliff edges"}},
		{"nature", {"nature", "Nature"}}, {"food", {"food", "Food"}}, {"medicine", {"medicine", "Medicine"}}, {"detritus", {"detritus", "Detritus"}},
		{"weather", {"weather", "Weather"}}, {"machinery", {"machine", "Machines"}},
	};
	auto n = names.find(w);
	if (n != names.end())
		return plural ? n->second.second : n->second.first;
	if (!plural)
		return w;
	std::string h = w;
	h[0] = (char)std::toupper((unsigned char)h[0]);
	return h + "s";
}

// the order kinds come in when sorted by kind: living things first, then
// things to play with and use, then the rest by name
static int kindRank(int category) {
	static const char* const order[] = {"critter", "badcritter", "bug", "badbug", "plant", "badplant", "nest", "toy", "implement",
		"dispensor", "mover", "machine", "food", "drink", "medicine"};
	std::string w = category < 0 ? std::string() : to_ascii_lowercase(KitCategoryNoun(category));
	for (int i = 0; i < (int)(sizeof(order) / sizeof(*order)); i++)
		if (w == order[i])
			return i;
	return category < 0 ? 1000 : 100;
}

static std::string dateText(long long secs) {
	if (secs <= 0)
		return "unknown";
	std::time_t t = (std::time_t)secs;
	std::tm tm{};
	localtime_r(&t, &tm);
	static const char* const months[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September",
		"October", "November", "December"};
	return fmt::format("{} {} {}", tm.tm_mday, months[tm.tm_mon], 1900 + tm.tm_year);
}

// the injector's sort and view, kept between runs (settings.txt)
enum { SORT_NAME, SORT_NAME_DESC, SORT_KIND, SORT_ADDED, SORT_IMPORTED, SORT_IN_WORLD, SORT_USED, SORT_COUNT };
static const char* const SORT_NAMES[SORT_COUNT] = {"Name, A to Z", "Name, Z to A", "Kind of object", "Recently added", "Imported first",
	"In the world now", "Recently used"};
static int s_sort = -1; // -1: not read yet
static bool s_tiles = false;
static char s_search[64] = "";

bool IsAgentInjectorEnabled() {
	return engine.version == 1 || engine.version == 2;
}

void SetAgentInjectorOpen(bool is_open) {
	s_agent_injector_open = is_open;
	if (is_open)
		s_agent_injector_minimized = false; // opened again from the toolbar: shown
}

// C2: minimized to a chip on the status bar, like the kits (owner, 2026-11-11)
void DrawAgentInjectorChip() {
	if (engine.version != 2 || !s_agent_injector_open || !s_agent_injector_minimized)
		return;
	if (KitChipButton("Agent Injector", 7, "Restore the Agent Injector")) {
		s_agent_injector_minimized = false;
		s_agent_injector_focus = true;
	}
}

void DrawAgentInjector() {
	// developer aid: OPENC2E_TEST_IMPORT_OBJECTS="a.cob;b.zip" opens the
	// injector and imports those files as if picked
	static bool testImport = true;
	if (testImport) {
		testImport = false;
		if (const char* files = getenv("OPENC2E_TEST_IMPORT_OBJECTS")) {
			std::string list = files;
			size_t start = 0;
			while (start <= list.size()) {
				size_t end = list.find(';', start);
				if (end == std::string::npos)
					end = list.size();
				if (end > start) {
					std::lock_guard<std::mutex> lock(s_pick_mutex);
					s_picked.push_back(list.substr(start, end - start));
				}
				start = end + 1;
			}
			s_agent_injector_open = true;
		}
	}
	// developer aid: OPENC2E_TEST_INJECT="Name;Name" adds those objects by
	// hand once the world is running (for testing that they are saved)
	static bool testInject = getenv("OPENC2E_TEST_INJECT") != nullptr;
	if (testInject && world.tickcount > 5) {
		testInject = false;
		CobManager cobs;
		cobs.update();
		std::string list = getenv("OPENC2E_TEST_INJECT");
		size_t start = 0;
		while (start < list.size()) {
			size_t end = list.find(';', start);
			if (end == std::string::npos)
				end = list.size();
			std::string name = list.substr(start, end - start);
			auto it = std::find_if(cobs.objects.begin(), cobs.objects.end(), [&](auto& o) { return o.name == name; });
			if (it == cobs.objects.end())
				fmt::print("* Test inject: no object \"{}\"\n", name);
			else {
				cobs.inject(*it, true);
				fmt::print("* Test inject: added \"{}\"\n", name);
			}
			start = end + 1;
		}
	}
	// developer aid: with OPENC2E_OPEN_KITS, OPENC2E_KITS_MINIMIZED starts it minimized
	static bool s_test_min = getenv("OPENC2E_KITS_MINIMIZED") != nullptr;
	if (s_test_min && s_agent_injector_open) {
		s_test_min = false;
		s_agent_injector_minimized = true;
	}
	if (s_agent_injector_minimized)
		return;
	if (s_agent_injector_open && s_agent_injector_focus) {
		ImGui::SetNextWindowFocus();
		s_agent_injector_focus = false;
	}
	// fits what it holds (a remembered or default size cut off the lines
	// under the buttons; owner, 2026-11-12)
	if (ImGuiUtils::BeginWindow("Agent Injector", &s_agent_injector_open, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar)) {
		if (engine.version == 2 && KitTitleMinimizeButton())
			s_agent_injector_minimized = true;
		if (!s_read_agents) {
			s_object_injector.update();
			hideListed();
			s_read_agents = true;
			// developer aid: OPENC2E_DUMP_COB_PICTURES=<folder> saves each
			// listed object's thumbnail there (<index>.bmp, names in list.txt)
			if (const char* dir = getenv("OPENC2E_DUMP_COB_PICTURES")) {
				std::ofstream list(fs::path(dir) / "list.txt");
				for (size_t i = 0; i < s_object_injector.objects.size(); i++) {
					auto& obj = s_object_injector.objects[i];
					Image img = s_object_injector.getPicture(obj);
					list << i << "\t" << obj.name << "\t" << obj.filename.string() << "\n";
					if (img.width <= 0 || img.height <= 0)
						continue;
					Texture t = get_backend()->createTextureFromImage(img);
					auto rt = get_backend()->newRenderTarget(img.width, img.height);
					rt->renderTexture(t, Rect2i(0, 0, img.width, img.height), Rect2f(0, 0, img.width, img.height));
					rt->saveToFile((fs::path(dir) / fmt::format("{}.bmp", i)).string());
				}
			}
		}
		importPicked();

		if (s_sort < 0) {
			std::string v = readPlayerSetting("injector-sort");
			s_sort = v.empty() ? SORT_NAME : std::clamp(atoi(v.c_str()), 0, SORT_COUNT - 1);
			s_tiles = readPlayerSetting("injector-view") == "tiles";
			// developer aid: OPENC2E_INJECTOR_VIEW="sort,tiles|list[,search]"
			if (const char* v2 = getenv("OPENC2E_INJECTOR_VIEW")) {
				std::string t = v2;
				s_sort = std::clamp(atoi(t.c_str()), 0, SORT_COUNT - 1);
				s_tiles = t.find("tiles") != std::string::npos;
				auto c = t.find(',', t.find(',') + 1);
				if (c != std::string::npos)
					std::snprintf(s_search, sizeof(s_search), "%s", t.c_str() + c + 1);
			}
		}
		auto& objs = s_object_injector.objects;
		// how many of each the player has added to this world
		std::map<std::string, int> inWorld;
		for (auto& a : world.agents)
			if (a && !a->injectedFrom.empty() && !a->isDying())
				inWorld[a->injectedFrom]++;
		std::map<std::string, long long> used;
		if (s_sort == SORT_USED)
			for (auto& o : objs)
				used[o.name] = CobManager::lastUsed(o.name);

		// the toolbar: search, sort, view, import
		ImGui::SetNextItemWidth(220);
		ImGui::InputTextWithHint("##search", "Search objects...", s_search, sizeof(s_search));
		ImGui::SameLine(0, 14);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Sort:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(170);
		if (ImGui::BeginCombo("##sort", SORT_NAMES[s_sort])) {
			for (int k = 0; k < SORT_COUNT; k++)
				if (ImGui::Selectable(SORT_NAMES[k], k == s_sort)) {
					s_sort = k;
					writePlayerSetting("injector-sort", std::to_string(k));
				}
			ImGui::EndCombo();
		}
		ImGui::SameLine(0, 14);
		for (int v = 0; v < 2; v++) {
			bool on = s_tiles == (v == 1);
			if (on)
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
			if (ImGui::Button(v ? "Tiles" : "List")) {
				s_tiles = v == 1;
				writePlayerSetting("injector-view", s_tiles ? "tiles" : "list");
			}
			if (on)
				ImGui::PopStyleColor();
			ImGui::SameLine(0, 2);
		}
		const float listW = s_tiles ? 456 : 340, detailW = 420, paneH = 470;
		ImGui::SameLine(listW + detailW + ImGui::GetStyle().ItemSpacing.x - 80);
		if (ImGui::Button("Import...")) {
			static const SDL_DialogFileFilter filters[] = {{"Objects (COB files, zips, add-on installers)", "cob;zip;exe"}, {"All files", "*"}};
			SDL_ShowOpenFileDialog(ObjectFilesChosen, nullptr, nullptr, filters, 2, nullptr, true);
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Add COB files, or zips of them, to this list (copied into your own data folder)");

		// the shown objects: matching the search, in the chosen order
		std::string q = to_ascii_lowercase(std::string(s_search));
		std::vector<size_t> shown;
		for (size_t i = 0; i < objs.size(); i++) {
			auto& o = objs[i];
			if (setOf(o.name) != s_set)
				continue;
			if (!q.empty() && to_ascii_lowercase(o.name).find(q) == std::string::npos && to_ascii_lowercase(o.description).find(q) == std::string::npos &&
				to_ascii_lowercase(kindWord(o.category, false)).find(q) == std::string::npos)
				continue;
			shown.push_back(i);
		}
		auto byName = [&](size_t a, size_t b) { return to_ascii_lowercase(objs[a].name) < to_ascii_lowercase(objs[b].name); };
		std::stable_sort(shown.begin(), shown.end(), [&](size_t a, size_t b) {
			auto& x = objs[a];
			auto& y = objs[b];
			switch (s_sort) {
				case SORT_NAME_DESC: return byName(b, a);
				case SORT_KIND:
					if (kindRank(x.category) != kindRank(y.category))
						return kindRank(x.category) < kindRank(y.category);
					if (x.category != y.category)
						return kindWord(x.category, true) < kindWord(y.category, true);
					return byName(a, b);
				case SORT_ADDED:
					if (x.added != y.added)
						return x.added > y.added;
					return byName(a, b);
				case SORT_IMPORTED:
					if (x.imported != y.imported)
						return x.imported;
					return byName(a, b);
				case SORT_IN_WORLD:
					if (inWorld[x.name] != inWorld[y.name])
						return inWorld[x.name] > inWorld[y.name];
					return byName(a, b);
				case SORT_USED:
					if (used[x.name] != used[y.name])
						return used[x.name] > used[y.name];
					return byName(a, b);
				default: return byName(a, b);
			}
		});
		if (s_object_selected_index >= objs.size() && !objs.empty())
			s_object_selected_index = 0;
		// the details follow the search: a chosen object it hides gives way
		// to the first one shown
		if (!shown.empty() && std::find(shown.begin(), shown.end(), s_object_selected_index) == shown.end())
			s_object_selected_index = shown[0];
		bool chosenShown = std::find(shown.begin(), shown.end(), s_object_selected_index) != shown.end();
		// developer aid: OPENC2E_TEST_INJECTOR_LIST="build=Name,uninstall=Name,
		// takeout=Name,reinstall=Name,set=N,select=Name" (applied once, in order;
		// these write the real lists: back them up first)
		static const char* s_test_list = getenv("OPENC2E_TEST_INJECTOR_LIST");
		if (s_test_list && world.tickcount > 5) {
			std::string all = s_test_list;
			s_test_list = nullptr;
			for (size_t at = 0; at <= all.size();) {
				size_t end = all.find(',', at);
				std::string item = all.substr(at, end == std::string::npos ? std::string::npos : end - at);
				auto eq = item.find('=');
				std::string key = item.substr(0, eq), val = eq == std::string::npos ? "" : item.substr(eq + 1);
				auto it = std::find_if(objs.begin(), objs.end(), [&](auto& o) { return o.name == val; });
				if (key == "set")
					s_set = std::clamp(atoi(val.c_str()), 0, 2);
				else if (it == objs.end())
					fmt::print("* Injector test: no object \"{}\"\n", val);
				else if (key == "build")
					buildIntoWorlds(*it);
				else if (key == "uninstall")
					uninstall(*it);
				else if (key == "takeout")
					takeOutOfWorlds(*it);
				else if (key == "reinstall")
					reinstall(*it);
				else if (key == "select")
					s_object_selected_index = it - objs.begin();
				if (!s_list_message.empty() && key != "set" && key != "select")
					fmt::print("* Injector test: {}: {}\n", key, s_list_message);
				if (end == std::string::npos)
					break;
				at = end + 1;
			}
		}

		// the tabs: the injector, objects built into every world, uninstalled
		{
			int counts[3] = {0, 0, 0};
			for (auto& o : objs)
				counts[setOf(o.name)]++;
			static const char* const names[3] = {"In the injector", "Built into every world", "Uninstalled"};
			static const char* const tips[3] = {"Objects to add to a world by hand",
				"Added to every world each time one loads (builtin-cobs.txt); not offered in the injector",
				"Taken out of the injector; their files are kept, and Reinstall puts them back"};
			for (int k = 0; k < 3; k++) {
				bool on = s_set == k;
				if (on)
					ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
				if (ImGui::Button(fmt::format("{} ({})##set{}", names[k], counts[k], k).c_str()) && s_set != k) {
					s_set = k;
					s_list_message.clear();
				}
				if (on)
					ImGui::PopStyleColor();
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s", tips[k]);
				if (k < 2)
					ImGui::SameLine(0, 4);
			}
		}

		// the objects: a list (with kind headings when sorted by kind) or tiles
		ImGui::BeginChild("Objects", ImVec2(listW, paneH), true);
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImU32 GOLD = IM_COL32(205, 170, 95, 255), DIM = IM_COL32(140, 146, 156, 255), GREEN = IM_COL32(88, 196, 104, 255);
		if (shown.empty())
			KitNote(objs.empty() ? "No objects. Import adds COB files."
					: !q.empty()     ? "Nothing matches the search."
					: s_set == SET_BUILTIN ? "Nothing is built into every world yet. Choose an object in the injector and Build into every world."
					: s_set == SET_UNINSTALLED ? "Nothing is uninstalled."
											: "Every object is built in or uninstalled.");
		if (!s_tiles) {
			int lastKind = -2;
			for (size_t n = 0; n < shown.size(); n++) {
				size_t i = shown[n];
				auto& o = objs[i];
				if (s_sort == SORT_KIND && o.category != lastKind) {
					lastKind = o.category;
					int count = 0;
					for (size_t m : shown)
						count += objs[m].category == o.category;
					ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(GOLD));
					ImGui::Text("%s", kindWord(o.category, true).c_str());
					ImGui::PopStyleColor();
					ImGui::SameLine(listW - 40);
					ImGui::TextDisabled("%d", count);
				}
				ImGui::PushID((int)i);
				ImVec2 at = ImGui::GetCursorScreenPos();
				if (ImGui::Selectable("##obj", i == s_object_selected_index, 0, ImVec2(0, 34)))
					s_object_selected_index = i;
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
					s_object_injector.inject(o, true);
					CobManager::noteUsed(o.name);
				}
				ImGui::PopID();
				drawThumb(dl, o, at + ImVec2(4, 2), 30);
				dl->AddText(at + ImVec2(42, 9), ImGui::GetColorU32(ImGuiCol_Text), o.name.c_str());
				// a badge: added to this world, or imported by the player
				std::string badge = inWorld[o.name] ? fmt::format("in world {}", inWorld[o.name]) : o.imported ? "imported" : "";
				if (!badge.empty()) {
					ImVec2 bs = ImGui::CalcTextSize(badge.c_str());
					ImVec2 b0 = at + ImVec2(listW - bs.x - 30, 9);
					dl->AddRectFilled(b0 - ImVec2(4, 1), b0 + bs + ImVec2(4, 1), inWorld[o.name] ? IM_COL32(30, 66, 38, 255) : IM_COL32(70, 58, 30, 255), 4);
					dl->AddText(b0, inWorld[o.name] ? GREEN : GOLD, badge.c_str());
				}
			}
		} else {
			const float TW = 104, TH = 118;
			const int per = 4;
			for (size_t n = 0; n < shown.size(); n++) {
				size_t i = shown[n];
				auto& o = objs[i];
				if (n % per)
					ImGui::SameLine(0, 4);
				ImGui::PushID((int)i);
				ImVec2 at = ImGui::GetCursorScreenPos();
				if (ImGui::InvisibleButton("tile", ImVec2(TW, TH)))
					s_object_selected_index = i;
				bool hov = ImGui::IsItemHovered();
				if (hov && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
					s_object_injector.inject(o, true);
					CobManager::noteUsed(o.name);
				}
				ImGui::PopID();
				bool sel = i == s_object_selected_index;
				dl->AddRectFilled(at, at + ImVec2(TW, TH), sel ? IM_COL32(51, 86, 140, 255) : hov ? IM_COL32(38, 40, 48, 255) : IM_COL32(28, 29, 33, 255), 5);
				dl->AddRect(at, at + ImVec2(TW, TH), sel || hov ? GOLD : IM_COL32(48, 48, 51, 255), 5);
				drawThumb(dl, o, at + ImVec2((TW - 72) / 2, 6), 72);
				std::string name = o.name;
				if (ImGui::CalcTextSize(name.c_str()).x > TW - 6) {
					while (name.size() > 2 && ImGui::CalcTextSize((name + ".").c_str()).x > TW - 6)
						name.pop_back();
					name += ".";
				}
				ImVec2 ns = ImGui::CalcTextSize(name.c_str());
				dl->AddText(at + ImVec2((TW - ns.x) / 2, 82), ImGui::GetColorU32(ImGuiCol_Text), name.c_str());
				std::string kw = inWorld[o.name] ? fmt::format("in world {}", inWorld[o.name]) : kindWord(o.category, false);
				ImVec2 ks = ImGui::CalcTextSize(kw.c_str());
				dl->AddText(at + ImVec2((TW - ks.x) / 2, 98), inWorld[o.name] ? GREEN : o.imported ? GOLD : DIM, kw.c_str());
				if (hov)
					ImGui::SetTooltip("%s\nDouble-click to add one to the world", o.name.c_str());
			}
		}
		ImGui::EndChild();

		// the chosen object
		ImGui::SameLine();
		ImGui::BeginChild("Details", ImVec2(detailW, paneH), true);
		if (s_object_selected_index < objs.size() && chosenShown) {
			auto& o = objs[s_object_selected_index];
			ImDrawList* dd = ImGui::GetWindowDrawList();
			ImVec2 p = ImGui::GetCursorScreenPos();
			drawThumb(dd, o, p, 150);
			dd->AddRect(p - ImVec2(1, 1), p + ImVec2(151, 151), GOLD);
			ImGui::SetCursorScreenPos(p + ImVec2(166, 0));
			ImGui::BeginGroup();
			ImGui::SetWindowFontScale(1.3f);
			ImGui::TextUnformatted(o.name.c_str());
			ImGui::SetWindowFontScale(1.0f);
			std::string kind = kindWord(o.category, false);
			kind[0] = (char)std::toupper((unsigned char)kind[0]);
			ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(GOLD), "%s", kind.c_str());
			ImGui::Spacing();
			auto row = [](const char* k, const std::string& v) {
				ImGui::TextDisabled("%s", k);
				ImGui::SameLine(110);
				ImGui::TextUnformatted(v.c_str());
			};
			row("From", o.imported ? "you (imported)" : "Creatures 2");
			row("Added", dateText(o.added));
			row("In this world", inWorld[o.name] ? fmt::format("{} you added", inWorld[o.name]) : std::string("none you added"));
			row("Makes", o.category >= 0 ? fmt::format("{} {} {}", o.family, o.genus, o.species) : std::string("scripts only"));
			ImGui::EndGroup();
			ImGui::SetCursorScreenPos(p + ImVec2(0, 160));
			ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + detailW - 24);
			ImGui::TextWrapped("%s", o.description.empty() ? "No description." : o.description.c_str());
			ImGui::PopTextWrapPos();
			// the buttons at the foot of the pane
			static std::string s_confirm; // the move awaiting a yes: "build" or "uninstall"
			float footY = paneH - 3 * ImGui::GetFrameHeightWithSpacing() - 34;
			if (ImGui::GetCursorPosY() < footY)
				ImGui::SetCursorPosY(footY);
			int set = setOf(o.name);
			ImGui::TextDisabled("%s", set == SET_INJECTOR ? "Objects you add are saved with the world."
									  : set == SET_BUILTIN ? "Added to every world each time one loads."
														   : "Uninstalled: not offered in the injector.");
			if (set == SET_INJECTOR) {
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
				if (ImGui::Button("Add to world")) {
					s_object_injector.inject(o, true);
					CobManager::noteUsed(o.name);
				}
				ImGui::PopStyleColor();
				ImGui::SameLine();
			}
			if (set != SET_UNINSTALLED) {
				if (o.is_removable) {
					if (ImGui::Button("Remove from world"))
						s_object_injector.remove(o, true);
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Runs the object's own removal script: every one of it goes, not only those you added");
				} else
					ImGuiUtils::DisabledButton("Remove from world");
			}
			if (set == SET_INJECTOR) {
				if (ImGui::Button("Build into every world"))
					s_confirm = "build";
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Every world gets it each time one loads, and it leaves the injector");
				ImGui::SameLine();
				if (ImGui::Button("Uninstall"))
					s_confirm = "uninstall";
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Take it out of the injector (its file is kept; Reinstall puts it back)");
			} else if (set == SET_BUILTIN) {
				if (ImGui::Button("Take out of every world"))
					takeOutOfWorlds(o);
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Back to the injector; worlds stop getting it at each load");
			} else {
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
				if (ImGui::Button("Reinstall"))
					reinstall(o);
				ImGui::PopStyleColor();
			}
			if (!s_confirm.empty())
				ImGui::OpenPopup("Confirm##injector");
			if (ImGui::BeginPopupModal("Confirm##injector", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
				ImGui::PushTextWrapPos(380);
				if (s_confirm == "build") {
					int here = 0;
					for (auto& a : world.agents)
						if (a && a->injectedFrom == o.name)
							here++;
					ImGui::TextWrapped("Build %s into every world?", o.name.c_str());
					ImGui::TextWrapped("Every world will get it each time it loads, and it leaves the injector. %s",
						here ? "The ones you added to this world become the built-in ones." : "One is added to this world now.");
					ImGui::TextDisabled("You can take it out again from Built into every world.");
				} else {
					ImGui::TextWrapped("Uninstall %s?", o.name.c_str());
					ImGui::TextWrapped("It leaves the injector; its file is kept. Any you added to worlds stay.");
					ImGui::TextDisabled("You can put it back from Uninstalled.");
				}
				ImGui::PopTextWrapPos();
				if (ImGui::Button(s_confirm == "build" ? "Build it in" : "Uninstall it")) {
					if (s_confirm == "build")
						buildIntoWorlds(o);
					else
						uninstall(o);
					s_confirm.clear();
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel")) {
					s_confirm.clear();
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
		}
		ImGui::EndChild();

		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + listW + detailW);
		if (!s_list_message.empty())
			ImGui::TextWrapped("%s", s_list_message.c_str());
		for (auto& line : s_import_report)
			ImGui::TextWrapped("%s", line.c_str());
		ImGui::PopTextWrapPos();

		ImGuiUtils::EndWindow();
	}
}

} // namespace Openc2eImgui