#include "WorldChooser.h"
#include "ImGuiUtils.h"
#include "common/devenv.h"

#include "Engine.h"
#include "PathResolver.h"
#include "GameFolder.h"
#include "creatures/CreatureSpeech.h"
#include "creatures/Breeding.h"
#include "common/Random.h"
#include "common/Ascii.h"
#include "common/backend/Backend.h"
#include "common/backend/BackendEvent.h"

#include <algorithm>
#include <fmt/core.h>
#include <fstream>
#include <imgui.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_process.h>
#include <cstdio>
#include <ctime>
#include <sstream>
#include <cstdlib>
#include <system_error>
#ifdef __linux__
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace Openc2eImgui {

// C1/C2 worlds, each a folder under <user data>/Worlds/<name>/ holding the
// world file it starts from (world.sfc, copied from a game world file such as
// Eden.sfc) and, later, the saved state.

fs::path GetWorldsDirectory() {
	return getUserDataDir() / "Worlds";
}

fs::path WorldFile(const fs::path& worlddir) {
	return worlddir / "world.sfc";
}

std::vector<WorldEntry> ListWorlds() {
	std::vector<WorldEntry> result;
	std::error_code ec;
	for (auto& entry : fs::directory_iterator(GetWorldsDirectory(), ec)) {
		if (entry.is_directory() && fs::exists(WorldFile(entry.path())))
			result.push_back({entry.path().filename().string(), entry.path()});
	}
	std::sort(result.begin(), result.end(), [](const WorldEntry& a, const WorldEntry& b) {
		return to_ascii_lowercase(a.name) < to_ascii_lowercase(b.name);
	});
	return result;
}

std::optional<WorldEntry> FindWorld(const std::string& name) {
	for (auto& w : ListWorlds())
		if (to_ascii_lowercase(w.name) == to_ascii_lowercase(name))
			return w;
	return std::nullopt;
}

std::string Sha256Text(const std::string& text); // Updater.cpp

// lock.txt: "zeetures-world-lock 1 SALT HASH", the hash of salt + password
static bool readLock(const fs::path& dir, std::string& salt, std::string& hash) {
	std::ifstream in(dir / "lock.txt");
	std::string tag;
	int version = 0;
	return in >> tag >> version >> salt >> hash && tag == "zeetures-world-lock";
}

bool WorldProtected(const fs::path& dir) {
	std::string salt, hash;
	return readLock(dir, salt, hash);
}

bool WorldPasswordRight(const fs::path& dir, const std::string& password) {
	std::string salt, hash;
	if (!readLock(dir, salt, hash))
		return true;
	return Sha256Text(salt + password) == hash;
}

bool SetWorldPassword(const fs::path& dir, const std::string& password) {
	std::error_code ec;
	if (password.empty()) {
		fs::remove(dir / "lock.txt", ec);
		return !WorldProtected(dir);
	}
	std::string salt;
	for (int i = 0; i < 16; i++)
		salt += fmt::format("{:02x}", rand_int32(0, 255) ^ (int)((std::time(nullptr) >> (i % 8)) & 255));
	{
		std::ofstream out(dir / "lock.txt", std::ios::binary);
		out << "zeetures-world-lock 1 " << salt << " " << Sha256Text(salt + password) << "\n";
	}
	return WorldPasswordRight(dir, password) && WorldProtected(dir);
}

// Where Creatures 2 itself keeps the worlds it has played (its own
// Documents folder): under Steam's Proton on Linux, next to the game's
// Steam library; in the Documents folder on Windows
static std::vector<fs::path> PlayedWorldFolders() {
	std::vector<fs::path> result;
	std::error_code ec;
	fs::path game = GameFolder::currentFolder();
	for (fs::path p = game; !p.empty() && p != p.parent_path(); p = p.parent_path())
		if (p.filename() == "steamapps") {
			result.push_back(p / "compatdata/1818340/pfx/drive_c/users/steamuser/Documents/Creatures/Creatures 2");
			break;
		}
#ifdef _WIN32
	if (const char* home = std::getenv("USERPROFILE"))
		result.push_back(fs::path(home) / "Documents" / "Creatures" / "Creatures 2");
#endif
	std::vector<fs::path> found;
	for (auto& f : result)
		if (fs::is_directory(f, ec))
			found.push_back(f);
	return found;
}

bool IsPlayedWorld(const fs::path& tmpl) {
	for (auto& f : PlayedWorldFolders())
		if (tmpl.parent_path() == f)
			return true;
	return false;
}

std::vector<fs::path> ListWorldTemplates() {
	// .sfc files in the game's own data folders (not the user folder);
	// Eden.sfc, the game's fresh starting world, first
	std::vector<fs::path> result;
	auto dirs = getMainDirectories();
	fs::path user = getUserDataDir();
	for (auto& dir : dirs) {
		if (dir == user)
			continue;
		std::error_code ec;
		for (auto& entry : fs::directory_iterator(dir, ec)) {
			if (entry.is_regular_file() && to_ascii_lowercase(entry.path().extension().string()) == ".sfc")
				result.push_back(entry.path());
		}
	}
	// and the worlds played in Creatures 2 itself, after the game's own
	std::vector<fs::path> played;
	for (auto& dir : PlayedWorldFolders()) {
		std::error_code ec;
		for (auto& entry : fs::directory_iterator(dir, ec))
			if (entry.is_regular_file() && to_ascii_lowercase(entry.path().extension().string()) == ".sfc")
				played.push_back(entry.path());
	}
	std::sort(played.begin(), played.end());
	std::sort(result.begin(), result.end(), [](const fs::path& a, const fs::path& b) {
		bool aeden = to_ascii_lowercase(a.filename().string()) == "eden.sfc";
		bool beden = to_ascii_lowercase(b.filename().string()) == "eden.sfc";
		if (aeden != beden)
			return aeden;
		return a < b;
	});
	result.insert(result.end(), played.begin(), played.end());
	return result;
}

static bool ValidWorldName(const std::string& name, std::string& error) {
	if (name.empty()) {
		error = "Please give the world a name.";
		return false;
	}
	for (char c : name) {
		if (!(is_ascii_alnum(c) || c == ' ' || c == '-' || c == '_' || c == '\'')) {
			error = "Names can use letters, numbers, spaces, - _ and '.";
			return false;
		}
	}
	// (on every system, so worlds can be copied between them: Windows drops
	// spaces at the end of a folder's name, so "Home " would open Home's
	// folder, and it keeps some names for devices)
	if (name.front() == ' ' || name.back() == ' ') {
		error = "Names can't start or end with a space.";
		return false;
	}
	{
		std::string stem = to_ascii_lowercase(name);
		static const char* const reserved[] = {"con", "prn", "aux", "nul"};
		bool bad = std::find(std::begin(reserved), std::end(reserved), stem) != std::end(reserved);
		if (stem.size() == 4 && (stem.rfind("com", 0) == 0 || stem.rfind("lpt", 0) == 0) && stem[3] >= '0' && stem[3] <= '9')
			bad = true;
		if (bad) {
			error = "That name is kept by Windows for devices; please choose another.";
			return false;
		}
	}
	if (FindWorld(name)) {
		error = "There is already a world with that name.";
		return false;
	}
	return true;
}

bool CreateWorld(const std::string& name, const fs::path& tmpl, std::string& error) {
	if (!ValidWorldName(name, error))
		return false;
	fs::path dir = GetWorldsDirectory() / name;
	std::error_code ec;
	fs::create_directories(dir, ec);
	if (!ec)
		fs::copy_file(tmpl, WorldFile(dir), fs::copy_options::overwrite_existing, ec);
	if (ec) {
		error = fmt::format("Couldn't create the world: {}", ec.message());
		return false;
	}
	// a world played in Creatures 2: its creatures' genomes come too (from
	// the Genetics folder beside it, those whose moniker is in the world
	// file), so they can be brought in at its first opening
	int genomes = 0;
	if (fs::is_directory(tmpl.parent_path() / "Genetics", ec) && to_ascii_lowercase(tmpl.filename().string()) != "eden.sfc") {
		std::ifstream in(tmpl, std::ios::binary);
		std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		for (auto& entry : fs::directory_iterator(tmpl.parent_path() / "Genetics", ec)) {
			std::string stem = entry.path().stem().string();
			if (stem.size() != 4 || to_ascii_lowercase(entry.path().extension().string()) != ".gen" || bytes.find(stem) == std::string::npos)
				continue;
			std::error_code ec2;
			fs::create_directories(dir / "Genetics", ec2);
			fs::copy_file(entry.path(), dir / "Genetics" / entry.path().filename(), fs::copy_options::overwrite_existing, ec2);
			if (!ec2)
				genomes++;
		}
	}
	// its preferences start at the defaults (CreatureSpeech.h)
	startWorldSettings(dir);
	fmt::print("* Created world '{}' from {}{}\n", name, tmpl.filename().string(), genomes ? fmt::format(" (with {} creatures' genomes)", genomes) : "");
	return true;
}

bool CopyWorldExtras(const fs::path& from, const fs::path& to, std::string& error) {
	for (const char* name : {"photos", "departed", "Additions"}) {
		std::error_code ec;
		if (!fs::is_directory(from / name, ec))
			continue;
		fs::copy(from / name, to / name, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
		if (ec) {
			error = fmt::format("Couldn't copy the world's {} folder: {}", name, ec.message());
			return false;
		}
	}
	// its preferences and kit layout too (CreatureSpeech.h)
	for (const char* name : {"settings.txt", "kits.ini"}) {
		std::error_code ec;
		if (fs::exists(from / name, ec))
			fs::copy_file(from / name, to / name, fs::copy_options::overwrite_existing, ec);
	}
	return true;
}

// The command line to start this program again in the named world: the same
// options, with --world NAME in place of any --world given before.
static std::vector<std::string> RelaunchArgs(const std::string& name) {
	std::vector<std::string> args;
	const auto& orig = engine.getCommandLineArgs();
	for (size_t i = 0; i < orig.size(); i++) {
		if (orig[i] == "--world" && i + 1 < orig.size()) {
			i++;
			continue;
		}
		if (orig[i].rfind("--world=", 0) == 0)
			continue;
		args.push_back(orig[i]);
	}
	args.push_back("--world");
	args.push_back(name);
	return args;
}

// Windows and macOS (and Linux with OPENC2E_TEST_SPAWN_RELAUNCH set, to test
// this path): start a new copy of the program with SDL, after this one has
// saved and shut down, then leave. The program's own path is argv[0] made
// absolute (Windows and macOS give the full path when started from Explorer,
// Finder or a shortcut), or the base folder plus its name when it was found
// on the PATH.
static bool SpawnRelaunch(std::vector<std::string> args) {
	if (args.empty())
		return false;
	fs::path exe = args[0];
	std::error_code ec;
	if (!exe.has_parent_path()) {
		if (const char* base = SDL_GetBasePath())
			exe = fs::path(base) / exe;
	}
	exe = fs::absolute(exe, ec);
	if (ec)
		return false;
	args[0] = exe.string();
	std::vector<const char*> argv;
	for (auto& a : args)
		argv.push_back(a.c_str());
	argv.push_back(nullptr);
	SDL_Process* child = SDL_CreateProcess(argv.data(), false);
	if (!child) {
		fmt::print(stderr, "Couldn't start {}: {}\n", args[0], SDL_GetError());
		return false;
	}
	SDL_DestroyProcess(child); // (leaves it running)
	return true;
}

void RelaunchIntoWorld(const std::string& name) {
	std::vector<std::string> args = RelaunchArgs(name);
	fmt::print("* Switching to world '{}'\n", name);
	engine.shutdown();
#ifdef __linux__
	if (!devenv("OPENC2E_TEST_SPAWN_RELAUNCH")) {
		std::vector<char*> argv;
		for (auto& a : args)
			argv.push_back(a.data());
		argv.push_back(nullptr);
		std::fflush(stdout); // (exec doesn't flush)
		std::fflush(stderr);
		execv("/proc/self/exe", argv.data());
		fmt::print(stderr, "Couldn't restart into world '{}'\n", name);
		_exit(1);
	}
#endif
	bool ok = SpawnRelaunch(args);
	if (!ok)
		fmt::print(stderr, "Couldn't restart into world '{}': run again with --world \"{}\"\n", name, name);
	std::fflush(stdout); // (_Exit doesn't flush)
	std::fflush(stderr);
	std::_Exit(ok ? 0 : 1);
}

// The new-world form, shared by the start-up chooser and the File menu.
// Returns the created world's name, or "" while still being filled in.
// With reset = true it only clears the form and draws nothing.
std::string DrawNewWorldForm(bool reset) {
	static char s_name[64] = "";
	static int s_template = 0;
	static std::string s_error;
	static std::vector<fs::path> s_templates;
	static bool s_focus = false;
	if (reset)
		s_focus = true;
	if (reset || s_templates.empty()) {
		s_templates = ListWorldTemplates();
		s_template = 0;
		s_error.clear();
		if (reset)
			s_name[0] = 0;
	}
	if (reset)
		return ""; // reset only; may be called outside an ImGui frame

	ImGui::Text("World name:");
	ImGui::SetNextItemWidth(260);
	if (ImGui::IsWindowAppearing() || s_focus)
		ImGui::SetKeyboardFocusHere(); // (ready to type the name)
	s_focus = false;
	bool enter = ImGui::InputText("##worldname", s_name, sizeof(s_name), ImGuiInputTextFlags_EnterReturnsTrue);
	ImGui::Text("Start from:");
	ImGui::SetNextItemWidth(260);
	std::string preview = s_templates.empty() ? "(no world files found)" : s_templates[s_template].filename().string();
	if (ImGui::BeginCombo("##template", preview.c_str())) {
		for (int i = 0; i < (int)s_templates.size(); i++) {
			std::string label = s_templates[i].filename().string();
			if (to_ascii_lowercase(label) == "eden.sfc")
				label += "  (a fresh Albia)";
			else if (IsPlayedWorld(s_templates[i]))
				label += "  (played in Creatures 2)";
			if (ImGui::Selectable(label.c_str(), i == s_template))
				s_template = i;
		}
		ImGui::EndCombo();
	}
	if (!s_error.empty())
		ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "%s", s_error.c_str());
	if ((ImGui::Button("Create World") || enter) && !s_templates.empty()) {
		if (CreateWorld(s_name, s_templates[s_template], s_error)) {
			std::string created = s_name;
			s_name[0] = 0;
			return created;
		}
	}
	return "";
}

// (a setting is one word, and a world's name may have spaces: kept as slashes,
// which no name has)
static std::string worldSetting(const std::string& key) {
	std::string v = readPlayerSetting(key);
	std::replace(v.begin(), v.end(), '/', ' ');
	return v;
}

static void setWorldSetting(const std::string& key, std::string name) {
	std::replace(name.begin(), name.end(), ' ', '/');
	writePlayerSetting(key, name);
}

std::string DefaultWorld() {
	return worldSetting("default-world");
}

void SetDefaultWorld(const std::string& name) {
	setWorldSetting("default-world", name);
}

// The world opened without asking, if the player has chosen one (the
// chooser's checkbox, File > Open This World at Launch; a player's wish
// passed on by the owner, 2026-11-22). While it opens, a setting says so: if
// that start never finished, the next one shows the chooser, so a world that
// will not open can be got away from.
void DefaultWorldOpened() {
	if (!readPlayerSetting("default-world-opening").empty())
		writePlayerSetting("default-world-opening", "");
}

// ---- File > World Picker -------------------------------------------------------

namespace {
bool s_picker_open = false;
std::string s_picker_select;
} // namespace

void OpenWorldPicker(const std::string& select) {
	s_picker_open = true;
	s_picker_select = select;
}

void DrawWorldPicker() {
	// developer aids: OPENC2E_OPEN_KITS with "worldpicker" opens it;
	// OPENC2E_TEST_WORLD_PICKER="NAME,PASSWORD" protects that world, checks
	// the password right and wrong, unprotects it, then puts it in the Trash
	// (headless tests in a throwaway home), printing each step
	static bool s_test_done = false;
	if (!s_test_done) {
		s_test_done = true;
		if (const char* k = devenv("OPENC2E_OPEN_KITS"); k && std::string(k).find("worldpicker") != std::string::npos)
			OpenWorldPicker();
		if (const char* t = devenv("OPENC2E_TEST_WORLD_PICKER")) {
			std::string spec = t;
			size_t comma = spec.find(',');
			auto w = FindWorld(spec.substr(0, comma));
			std::string pw = comma == std::string::npos ? "secret" : spec.substr(comma + 1);
			if (w) {
				bool before = WorldProtected(w->dir);
				fmt::print("* WORLD PICKER test {}: protected {}\n", w->name, before);
				bool set = SetWorldPassword(w->dir, pw);
				bool locked = WorldProtected(w->dir);
				fmt::print("* WORLD PICKER test: protect {}, protected {}\n", set, locked);
				bool right = WorldPasswordRight(w->dir, pw), wrong = WorldPasswordRight(w->dir, pw + "x");
				fmt::print("* WORLD PICKER test: right password {}, wrong password {}\n", right, wrong);
				bool unset = SetWorldPassword(w->dir, "");
				locked = WorldProtected(w->dir);
				fmt::print("* WORLD PICKER test: unprotect {}, protected {}\n", unset, locked);
				SetWorldPassword(w->dir, pw);
				locked = WorldProtected(w->dir);
				bool trashed = moveToTrash(w->dir);
				bool still = (bool)FindWorld(w->name);
				fmt::print("* WORLD PICKER test: protected again {}; to the Trash {}, still there {}\n", locked, trashed, still);
			} else
				fmt::print("* WORLD PICKER test: no world {}\n", spec);
		}
	}
	if (!s_picker_open)
		return;
	static int s_selected = -1;
	static char s_pw[128] = "", s_pw2[128] = "";
	enum Asking { NONE,
		OPEN,
		PROTECT,
		UNPROTECT,
		DELETE };
	static int s_asking = NONE;
	static std::string s_message;
	auto worlds = ListWorlds();
	// (developer aid: OPENC2E_TEST_PICKER_ASK=delete or protect opens it
	// asking that, for snapshots)
	static bool s_test_ask = true;
	if (s_test_ask) {
		s_test_ask = false;
		if (const char* t = devenv("OPENC2E_TEST_PICKER_ASK"))
			s_asking = std::string(t) == "protect" ? PROTECT : DELETE;
	}
	if (!s_picker_select.empty()) {
		for (int i = 0; i < (int)worlds.size(); i++)
			if (worlds[i].name == s_picker_select)
				s_selected = i;
		s_picker_select.clear();
		s_asking = NONE;
		s_message.clear();
	}
	if (s_selected < 0 || s_selected >= (int)worlds.size()) {
		s_selected = 0;
		for (int i = 0; i < (int)worlds.size(); i++)
			if (worlds[i].name == engine.currentworld)
				s_selected = i;
	}
	auto ask = [&](int what) {
		s_asking = what;
		s_pw[0] = s_pw2[0] = 0;
		s_message.clear();
	};
	ImGuiIO& io = ImGui::GetIO();
	ImGui::SetNextWindowPos(ImGuiUtils::GameWindowPos() + ImVec2(io.DisplaySize.x / 2, io.DisplaySize.y / 2), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGuiUtils::KeepInGameWindow();
	// (sized to what it holds, so a question or password box opens below
	// the buttons instead of out of sight; owner, 2026-11-27)
	if (!ImGui::Begin("World Picker", &s_picker_open, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::End();
		return;
	}
	ImGui::TextUnformatted("Your worlds:");
	const float pickerWidth = 400;
	ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + pickerWidth);
	if (ImGui::BeginListBox("##pickerworlds", ImVec2(pickerWidth, 180))) {
		for (int i = 0; i < (int)worlds.size(); i++) {
			const auto& w = worlds[i];
			std::string label = w.name;
			if (w.name == engine.currentworld)
				label += "  (open now)";
			if (WorldProtected(w.dir))
				label += "  (protected)";
			if (w.name == DefaultWorld())
				label += "  (opens at launch)";
			if (ImGui::Selectable((label + "##" + w.name).c_str(), i == s_selected, ImGuiSelectableFlags_AllowDoubleClick)) {
				if (i != s_selected)
					ask(NONE);
				s_selected = i;
				if (ImGui::IsMouseDoubleClicked(0) && w.name != engine.currentworld)
					ask(OPEN);
			}
		}
		ImGui::EndListBox();
	}
	if (worlds.empty()) {
		ImGui::PopTextWrapPos();
		ImGui::End();
		return;
	}
	const WorldEntry w = worlds[s_selected];
	const bool current = w.name == engine.currentworld;
	const bool locked = WorldProtected(w.dir);
	// Open: at once, or after its password
	ImGui::BeginDisabled(current);
	if (ImGui::Button("Open"))
		ask(OPEN);
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip(current ? "This is the world you are playing." : "Saves this world and opens that one (Zeetures starts again in it).");
	ImGui::SameLine();
	if (ImGui::Button(locked ? "Change Password..." : "Protect..."))
		ask(PROTECT);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("A password asked before the world opens here, is deleted, or is unprotected.\nIt keeps it from being opened in Zeetures; its files are not encrypted.");
	ImGui::SameLine();
	ImGui::BeginDisabled(!locked);
	if (ImGui::Button("Unprotect..."))
		ask(UNPROTECT);
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(current);
	if (ImGui::Button("Delete..."))
		ask(DELETE);
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip(current ? "Open another world first to delete this one." : "Puts the world in the Trash, with its save, photos and settings.");
	if (s_asking == OPEN && !locked) {
		ImGui::PopTextWrapPos();
		ImGui::End();
		RelaunchIntoWorld(w.name);
		return;
	}
	if (s_asking != NONE) {
		ImGui::Separator();
		const bool needOld = locked && (s_asking == OPEN || s_asking == UNPROTECT || s_asking == DELETE || s_asking == PROTECT);
		if (s_asking == DELETE)
			ImGui::TextWrapped("Delete %s? It goes to the Trash with its save, photos and settings, and can be put back from there.", w.name.c_str());
		else if (s_asking == PROTECT)
			ImGui::TextWrapped(locked ? "A new password for %s:" : "A password for %s, asked before it opens, is deleted or is unprotected:", w.name.c_str());
		else if (s_asking == UNPROTECT)
			ImGui::TextWrapped("Take the password off %s:", w.name.c_str());
		else
			ImGui::TextWrapped("%s is protected.", w.name.c_str());
		static char s_old[128] = "";
		if (needOld) {
			ImGui::SetNextItemWidth(220);
			ImGui::InputText("Its password##pickerold", s_old, sizeof(s_old), ImGuiInputTextFlags_Password);
		}
		if (s_asking == PROTECT) {
			ImGui::SetNextItemWidth(220);
			ImGui::InputText("New password", s_pw, sizeof(s_pw), ImGuiInputTextFlags_Password);
			ImGui::SetNextItemWidth(220);
			ImGui::InputText("Again", s_pw2, sizeof(s_pw2), ImGuiInputTextFlags_Password);
		}
		const char* go = s_asking == OPEN ? "Open it" : s_asking == PROTECT ? "Protect" : s_asking == UNPROTECT ? "Unprotect" : "Delete";
		if (ImGui::Button(go)) {
			if (needOld && !WorldPasswordRight(w.dir, s_old))
				s_message = "That is not its password.";
			else if (s_asking == PROTECT && (std::string(s_pw).empty() || std::string(s_pw) != s_pw2))
				s_message = std::string(s_pw).empty() ? "Type a password." : "The two passwords are not the same.";
			else {
				int what = s_asking;
				s_old[0] = 0;
				ask(NONE);
				if (what == OPEN) {
					ImGui::PopTextWrapPos();
					ImGui::End();
					RelaunchIntoWorld(w.name);
					return;
				}
				if (what == PROTECT)
					s_message = SetWorldPassword(w.dir, s_pw) ? w.name + " is protected." : "Couldn't protect it.";
				else if (what == UNPROTECT)
					s_message = SetWorldPassword(w.dir, "") ? w.name + " is no longer protected." : "Couldn't unprotect it.";
				else if (what == DELETE && current)
					s_message = "Open another world first to delete this one.";
				else if (what == DELETE) {
					if (moveToTrash(w.dir)) {
						if (DefaultWorld() == w.name)
							SetDefaultWorld("");
						s_message = w.name + " is in the Trash.";
						s_selected = -1;
					} else
						s_message = "Couldn't move it to the Trash.";
				}
				s_pw[0] = s_pw2[0] = 0;
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) {
			s_old[0] = 0;
			ask(NONE);
		}
	}
	if (!s_message.empty())
		ImGui::TextWrapped("%s", s_message.c_str());
	ImGui::PopTextWrapPos();
	ImGui::End();
}

std::string RunWorldChooser() {
	std::string chosen;
	bool quit = false;
	bool creating = ListWorlds().empty();
	std::string byDefault = DefaultWorld();
	if (!byDefault.empty() && !FindWorld(byDefault))
		byDefault.clear(); // (renamed or deleted since)
	const bool failed = !byDefault.empty() && worldSetting("default-world-opening") == byDefault;
	// (a protected world is never opened without its password: the chooser
	// asks, with it selected)
	const bool lockedDefault = !byDefault.empty() && WorldProtected(FindWorld(byDefault)->dir);
	if (!byDefault.empty() && !failed && !lockedDefault) {
		setWorldSetting("default-world-opening", byDefault);
		return byDefault;
	}
	DefaultWorldOpened();
	bool always = failed || lockedDefault; // (kept ticked, not lost)
	DrawNewWorldForm(true);

	get_backend()->run([&] {
		BackendEvent e;
		while (get_backend()->pollEvent(e)) {
			if (e.type == eventquit)
				quit = true;
		}
		if (quit)
			return false;
		get_backend()->getMainRenderTarget()->renderClear();

		ImGuiIO& io = ImGui::GetIO();
		ImGui::SetNextFrameWantCaptureMouse(true); // (the pointer shows everywhere: there is no hand yet)
		ImGui::SetNextWindowPos(ImGuiUtils::GameWindowPos() + ImVec2(io.DisplaySize.x / 2, io.DisplaySize.y / 2), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(360, 0));
		ImGuiUtils::KeepInGameWindow();
		ImGui::Begin("Choose a World", nullptr, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
		static int s_selected = 0;
		static char s_password[128] = "";
		static std::string s_asking = lockedDefault && !failed ? byDefault : std::string(); // the protected world being opened
		static bool s_wrong = false;
		static bool s_focus_pw = !s_asking.empty();
		auto worlds = ListWorlds();
		if (static bool first = true; first) {
			first = false;
			for (int i = 0; i < (int)worlds.size(); i++)
				if (worlds[i].name == byDefault)
					s_selected = i;
		}
		if (!creating && !worlds.empty()) {
			if (failed) {
				ImGui::TextWrapped("%s was not opened by itself this time, as Zeetures closed while opening it the last time.", byDefault.c_str());
				ImGui::Spacing();
			}
			ImGui::Text("Your worlds:");
			if (ImGui::BeginListBox("##worlds", ImVec2(-1, 160))) {
				for (int i = 0; i < (int)worlds.size(); i++) {
					if (ImGui::Selectable(worlds[i].name.c_str(), i == s_selected, ImGuiSelectableFlags_AllowDoubleClick)) {
						s_selected = i;
						if (ImGui::IsMouseDoubleClicked(0))
							chosen = worlds[i].name;
						s_asking.clear();
					}
				}
				ImGui::EndListBox();
			}
			s_selected = std::clamp(s_selected, 0, (int)worlds.size() - 1);
			ImGui::Checkbox("Always open this world at launch", &always);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Skips this window from now on.\nFile > Open This World at Launch turns it off again.");
			if (ImGui::Button("Open"))
				chosen = worlds[s_selected].name;
			// a protected world: its password first
			if (!chosen.empty() && WorldProtected(worlds[s_selected].dir) && s_asking != chosen) {
				s_asking = chosen;
				s_password[0] = 0;
				s_wrong = false;
				s_focus_pw = true;
				chosen.clear();
			}
			if (!s_asking.empty()) {
				ImGui::Separator();
				ImGui::TextWrapped("%s is protected. Its password:", s_asking.c_str());
				if (s_focus_pw) {
					ImGui::SetKeyboardFocusHere();
					s_focus_pw = false;
				}
				bool enter = ImGui::InputText("##worldpw", s_password, sizeof(s_password), ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue);
				if (s_wrong)
					ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "That is not its password.");
				if (ImGui::Button("Open it") || enter) {
					auto w = FindWorld(s_asking);
					if (w && WorldPasswordRight(w->dir, s_password)) {
						chosen = s_asking;
						s_asking.clear();
					} else
						s_wrong = true;
					s_password[0] = 0;
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("New World..."))
				creating = true;
			ImGui::SameLine();
			if (ImGui::Button("Quit"))
				quit = true;
		} else {
			ImGui::TextWrapped(worlds.empty() ? "Welcome! Name your first world." : "New world");
			ImGui::Separator();
			std::string created = DrawNewWorldForm(false);
			if (!created.empty())
				chosen = created;
			if (!worlds.empty()) {
				ImGui::SameLine();
				if (ImGui::Button("Back"))
					creating = false;
			}
			ImGui::SameLine();
			if (ImGui::Button("Quit"))
				quit = true;
		}
		ImGui::End();
		return chosen.empty() && !quit;
	});

	if (quit)
		return "";
	if (always) {
		SetDefaultWorld(chosen);
		setWorldSetting("default-world-opening", chosen);
	} else if (!DefaultWorld().empty())
		SetDefaultWorld("");
	return chosen;
}

} // namespace Openc2eImgui
