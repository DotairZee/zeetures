/*
 *  GameFolder.cpp
 *  Zeetures: finding the player's own Creatures 2 folder
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

#include "GameFolder.h"

#include "PathResolver.h"
#include "common/case_insensitive_filesystem.h"

#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_misc.h>
#include <SDL3/SDL_timer.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fmt/core.h>
#include <fstream>
#include <mutex>
#include <optional>
#include <regex>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace GameFolder {

namespace {

// where "How to Get It" leads: the README's download section, which says
// where to buy the game and how to get its files on a Mac
const char* kHowToGetIt = "https://github.com/DotairZee/zeetures#download-and-play";

// the remembered folder, in the user folder above the per-game ones
const char* kRememberedFile = "game-folder.txt";

fs::path s_current;
bool s_currentFromCommandLine = false;

std::string utf8(const fs::path& p) {
	auto s = p.u8string();
	return std::string(s.begin(), s.end());
}

fs::path fromUtf8(const std::string& s) {
	return fs::path(std::u8string(s.begin(), s.end()));
}

bool isDirectory(const fs::path& p) {
	std::error_code ec;
	return !p.empty() && fs::is_directory(p, ec);
}

bool isCreatures2(const fs::path& p) {
	return isDirectory(p) && case_insensitive_filesystem::exists(p / "creatures2.exe");
}

bool isCreatures1(const fs::path& p) {
	return isDirectory(p) && case_insensitive_filesystem::exists(p / "creatures.exe");
}

fs::path homeDir() {
	try {
		return homeDirectory();
	} catch (std::exception&) {
		return {};
	}
}

fs::path rememberedFile() {
	try {
		return storageBaseDirectory() / kRememberedFile;
	} catch (std::exception&) {
		return {};
	}
}

fs::path readRemembered() {
	fs::path file = rememberedFile();
	if (file.empty())
		return {};
	std::ifstream in(file, std::ios::binary);
	std::string line;
	while (std::getline(in, line)) {
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		if (line.empty() || line[0] == '#')
			continue;
		return fromUtf8(line);
	}
	return {};
}

void remember(const fs::path& folder) {
	fs::path file = rememberedFile();
	if (file.empty())
		return;
	std::ofstream out(file, std::ios::binary | std::ios::trunc);
	out << "# The Creatures 2 folder Zeetures plays from. Change it with\n"
		   "# File > Creatures 2 Folder, or write another folder on the next line.\n"
		<< utf8(folder) << "\n";
	if (out)
		fmt::print("* Remembered the Creatures 2 folder in {}\n", utf8(file));
}

// subfolders of dir whose names contain "creature", down to depth levels;
// for the usual game folders (Steam's common, GOG Games, ~/Games)
fs::path searchNamed(const fs::path& dir, int depth) {
	if (depth <= 0 || !isDirectory(dir))
		return {};
	std::vector<fs::path> subdirs;
	std::error_code ec;
	for (auto it = fs::directory_iterator(dir, fs::directory_options::skip_permission_denied, ec);
		 !ec && it != fs::directory_iterator(); it.increment(ec)) {
		std::error_code ec2;
		if (!it->is_directory(ec2))
			continue;
		std::string name = utf8(it->path().filename());
		if (name.empty() || name[0] == '.')
			continue;
		subdirs.push_back(it->path());
	}
	std::sort(subdirs.begin(), subdirs.end());
	for (auto& sub : subdirs) {
		std::string name = utf8(sub.filename());
		std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::tolower(c); });
		if (name.find("creature") != std::string::npos) {
			fs::path found = normalize(sub);
			if (!found.empty())
				return found;
		}
	}
	for (auto& sub : subdirs) {
		fs::path found = searchNamed(sub, depth - 1);
		if (!found.empty())
			return found;
	}
	return {};
}

#ifdef _WIN32
std::wstring regString(HKEY root, const wchar_t* key, const wchar_t* value) {
	wchar_t buf[2048];
	DWORD size = sizeof(buf);
	if (RegGetValueW(root, key, value, RRF_RT_REG_SZ, nullptr, buf, &size) == ERROR_SUCCESS)
		return buf;
	return {};
}

fs::path envPath(const wchar_t* name) {
	const wchar_t* v = _wgetenv(name);
	return v ? fs::path(v) : fs::path();
}
#endif

std::vector<fs::path> steamRoots() {
	std::vector<fs::path> roots;
#ifdef _WIN32
	for (auto& s : {regString(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath"),
			 regString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath"),
			 regString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Valve\\Steam", L"InstallPath")}) {
		if (!s.empty())
			roots.push_back(fs::path(s));
	}
	for (auto* env : {L"ProgramFiles(x86)", L"ProgramFiles"}) {
		fs::path p = envPath(env);
		if (!p.empty())
			roots.push_back(p / "Steam");
	}
#else
	fs::path home = homeDir();
	if (home.empty())
		return roots;
#ifdef __APPLE__
	roots.push_back(home / "Library/Application Support/Steam");
#else
	if (const char* xdg = getenv("XDG_DATA_HOME"); xdg && *xdg)
		roots.push_back(fs::path(xdg) / "Steam");
	roots.push_back(home / ".local/share/Steam");
	roots.push_back(home / ".steam/steam");
	roots.push_back(home / ".steam/root");
	roots.push_back(home / ".steam/debian-installation");
	roots.push_back(home / ".var/app/com.valvesoftware.Steam/.local/share/Steam");
	roots.push_back(home / "snap/steam/common/.local/share/Steam");
#endif
#endif
	return roots;
}

// every Steam library folder: each root, and the libraries its
// libraryfolders.vdf lists ("path" "D:\\Games\\SteamLibrary")
std::vector<fs::path> steamLibraries() {
	std::vector<fs::path> libraries;
	auto add = [&](const fs::path& p) {
		if (isDirectory(p) && std::find(libraries.begin(), libraries.end(), p) == libraries.end())
			libraries.push_back(p);
	};
	static const std::regex pathLine(R"re(^\s*"path"\s+"(.*)"\s*$)re");
	for (auto& root : steamRoots()) {
		add(root);
		for (auto& vdf : {root / "steamapps/libraryfolders.vdf", root / "config/libraryfolders.vdf"}) {
			std::ifstream in(vdf, std::ios::binary);
			std::string line;
			while (std::getline(in, line)) {
				if (!line.empty() && line.back() == '\r')
					line.pop_back();
				std::smatch m;
				if (!std::regex_match(line, m, pathLine))
					continue;
				std::string p = m[1];
				std::string unescaped;
				for (size_t i = 0; i < p.size(); i++) {
					if (p[i] == '\\' && i + 1 < p.size())
						i++;
					unescaped += p[i];
				}
				add(fromUtf8(unescaped));
			}
		}
	}
	return libraries;
}

std::vector<fs::path> steamGameFolders() {
	std::vector<fs::path> folders;
	for (auto& lib : steamLibraries())
		folders.push_back(lib / "steamapps/common/Creatures The Albian Years");
	return folders;
}

fs::path detect() {
	// Steam: "Creatures: The Albian Years" (app 1818340) in any library
	for (auto& f : steamGameFolders()) {
		if (fs::path found = normalize(f); !found.empty())
			return found;
	}

#ifdef __APPLE__
	// Steam for Mac won't install the game, but downloads its files with
	// the console's download_depot (see the README); copied to ~/Games as
	// the README says, or still where Steam put them
	fs::path home = homeDir();
	if (!home.empty()) {
		for (auto& f : {home / "Games/Creatures The Albian Years",
				 home / "Library/Application Support/Steam/Steam.AppBundle/Steam/Contents/MacOS/steamapps/content/app_1818340/depot_1818341"}) {
			if (fs::path found = normalize(f); !found.empty())
				return found;
		}
		if (fs::path found = searchNamed(home / "Games", 3); !found.empty())
			return found;
	}
#elif defined(_WIN32)
	// GOG: every game GOG Galaxy or its installers registered, then the
	// usual folders
	for (auto* key : {L"SOFTWARE\\WOW6432Node\\GOG.com\\Games", L"SOFTWARE\\GOG.com\\Games"}) {
		HKEY games;
		if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, key, 0, KEY_READ, &games) != ERROR_SUCCESS)
			continue;
		wchar_t name[256];
		for (DWORD i = 0;; i++) {
			DWORD len = 256;
			if (RegEnumKeyExW(games, i, name, &len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
				break;
			std::wstring sub = std::wstring(key) + L"\\" + name;
			std::wstring path = regString(HKEY_LOCAL_MACHINE, sub.c_str(), L"path");
			if (path.empty())
				continue;
			if (fs::path found = normalize(fs::path(path)); !found.empty()) {
				RegCloseKey(games);
				return found;
			}
		}
		RegCloseKey(games);
	}
	std::vector<fs::path> places{"C:\\GOG Games"};
	for (auto* env : {L"ProgramFiles(x86)", L"ProgramFiles"}) {
		fs::path p = envPath(env);
		if (!p.empty()) {
			places.push_back(p / "GOG Galaxy" / "Games");
			places.push_back(p);
		}
	}
	for (auto& place : places) {
		if (fs::path found = searchNamed(place, 2); !found.empty())
			return found;
	}
#else
	// GOG and other copies under Wine, Heroic or Lutris
	fs::path home = homeDir();
	if (!home.empty()) {
		if (fs::path found = searchNamed(home / "Games", 3); !found.empty())
			return found;
		for (auto& place : {home / ".wine/drive_c/GOG Games",
				 home / ".wine/drive_c/Program Files (x86)",
				 home / ".wine/drive_c/Program Files"}) {
			if (fs::path found = searchNamed(place, 2); !found.empty())
				return found;
		}
	}
#endif
	return {};
}

// where the folder chooser opens: Steam's games, or the home folder
std::string chooserStart() {
	for (auto& lib : steamLibraries()) {
		fs::path common = lib / "steamapps/common";
		if (isDirectory(common))
			return utf8(common);
	}
	fs::path home = homeDir();
	return home.empty() ? std::string() : utf8(home);
}

// SDL's folder chooser answers in a callback, perhaps on another thread
struct Choice {
	std::mutex lock;
	bool open = false;
	bool done = false;
	bool failed = false;
	fs::path folder;
};

void SDLCALL onChosen(void* userdata, const char* const* filelist, int) {
	auto* c = static_cast<Choice*>(userdata);
	std::lock_guard<std::mutex> l(c->lock);
	if (!filelist)
		c->failed = true; // (no chooser on this system)
	else if (filelist[0])
		c->folder = fromUtf8(filelist[0]);
	c->done = true;
}

std::string notCreatures2Message(const fs::path& chosen) {
	if (isCreatures1(chosen))
		return fmt::format("{}\n\nis Creatures 1's folder. Zeetures plays Creatures 2: choose the Creatures 2 folder beside it.", utf8(chosen));
	return fmt::format("{}\n\nis not a Creatures 2 folder. Choose the folder that holds Creatures2.exe, Eden.sfc and the Images folder (or the Creatures The Albian Years folder that holds it).", utf8(chosen));
}

} // namespace

fs::path normalize(const fs::path& p) {
	if (isCreatures2(p))
		return p;
	if (isCreatures2(p / "Creatures 2"))
		return p / "Creatures 2";
	return {};
}

fs::path find() {
	fs::path remembered = readRemembered();
	if (!remembered.empty()) {
		if (fs::path found = normalize(remembered); !found.empty()) {
			fmt::print("* Creatures 2 folder (remembered): {}\n", utf8(found));
			return found;
		}
		fmt::print("* The remembered Creatures 2 folder {} is not there any more; looking again\n", utf8(remembered));
	}
	fs::path found = detect();
	if (!found.empty()) {
		fmt::print("* Creatures 2 folder (found): {}\n", utf8(found));
		remember(found);
	}
	return found;
}

fs::path ask() {
	bool started = SDL_InitSubSystem(SDL_INIT_VIDEO);
	fs::path answer;
	std::string message =
		"Zeetures plays Creatures 2 from your own copy of the game, and could not find one on this computer.\n\n"
		"Choose your Creatures 2 folder: the one that holds Creatures2.exe, Eden.sfc and the Images folder. "
		"The Creatures The Albian Years folder that holds it will do too. Zeetures remembers it.\n\n"
		"Creatures 2 comes with Creatures: The Albian Years on Steam, and is also sold on GOG. "
		"\"How to Get It\" opens the instructions, including how to get the files on a Mac.";
	while (true) {
		const SDL_MessageBoxButtonData buttons[] = {
			{SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Choose Folder..."},
			{0, 2, "How to Get It"},
			{SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Quit"},
		};
		const SDL_MessageBoxData box = {
			SDL_MESSAGEBOX_INFORMATION | SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT,
			nullptr,
			"Zeetures: where is Creatures 2?",
			message.c_str(),
			3, buttons, nullptr};
		int pressed = 0;
		if (!SDL_ShowMessageBox(&box, &pressed)) {
			fmt::print(stderr, "Couldn't show the Creatures 2 folder window: {}\n", SDL_GetError());
			break;
		}
		if (pressed == 2) {
			SDL_OpenURL(kHowToGetIt);
			continue;
		}
		if (pressed != 1)
			break; // Quit, or the window closed

		Choice choice;
		std::string start = chooserStart();
		SDL_ShowOpenFolderDialog(onChosen, &choice, nullptr, start.empty() ? nullptr : start.c_str(), false);
		while (true) {
			{
				std::lock_guard<std::mutex> l(choice.lock);
				if (choice.done)
					break;
			}
			SDL_PumpEvents();
			SDL_Delay(20);
		}
		if (choice.failed) {
			std::string text = fmt::format(
				"This computer has no folder chooser Zeetures can use ({}).\n\n"
				"Write the path of your Creatures 2 folder on a line of its own in\n{}\nand start Zeetures again.",
				SDL_GetError(), utf8(rememberedFile()));
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Zeetures", text.c_str(), nullptr);
			break;
		}
		if (choice.folder.empty())
			continue; // cancelled: back to the first window
		answer = normalize(choice.folder);
		if (!answer.empty()) {
			remember(answer);
			break;
		}
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Zeetures", notCreatures2Message(choice.folder).c_str(), nullptr);
	}
	if (started)
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	return answer;
}

namespace {
Choice s_another;
}

void chooseAnother(SDL_Window* window) {
	{
		std::lock_guard<std::mutex> l(s_another.lock);
		if (s_another.open)
			return;
		s_another.open = true;
		s_another.done = s_another.failed = false;
		s_another.folder.clear();
	}
	std::string start = s_current.empty() ? chooserStart() : utf8(s_current.parent_path());
	SDL_ShowOpenFolderDialog(onChosen, &s_another, window, start.empty() ? nullptr : start.c_str(), false);
}

void pollChooseAnother(SDL_Window* window) {
	fs::path chosen;
	bool failed;
	{
		std::lock_guard<std::mutex> l(s_another.lock);
		if (!s_another.open || !s_another.done)
			return;
		s_another.open = false;
		chosen = s_another.folder;
		failed = s_another.failed;
	}
	if (failed) {
		std::string text = fmt::format("This computer has no folder chooser Zeetures can use ({}).\n\n"
									   "Write the path of your Creatures 2 folder on a line of its own in\n{}",
			SDL_GetError(), utf8(rememberedFile()));
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Zeetures", text.c_str(), window);
		return;
	}
	if (chosen.empty())
		return; // cancelled
	fs::path folder = normalize(chosen);
	if (folder.empty()) {
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Zeetures", notCreatures2Message(chosen).c_str(), window);
		return;
	}
	remember(folder);
	std::string text = fmt::format("Zeetures will play from this folder the next time it starts:\n\n{}", utf8(folder));
	if (s_currentFromCommandLine)
		text += "\n\n(This time it was started with -d, which still comes first.)";
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Zeetures", text.c_str(), window);
}

void setCurrent(const fs::path& folder, bool fromCommandLine) {
	s_current = folder;
	s_currentFromCommandLine = fromCommandLine;
}

const fs::path& currentFolder() {
	return s_current;
}

} // namespace GameFolder
