// Zeetures's recommended breed packs (owner, 2026-11-15: "do whatever is
// needed to make it so Zeetures ... gets those two breeds"): Boney Grendels
// (Creature Labs' own C2 release) and Frog Norns V3 (Alex, Norns of the
// Round Table, from Nornenmeister's V2). Zeetures does not carry or host
// them: on the player's machine, the first time a C2 world runs without
// them, it fetches each from the community archive that already hosts it
// (eem.foo) with the system's curl, and imports it as the owner's Home has
// it: Boney in its own grendel slot c, the Frog pack in place of the game's
// own frog norn (norn slot l, in the Overrides folder; the frog-kiss and
// every world use it). Bring-your-own-data, as with the game itself.
//
// Off with the setting "breed-packs off" (settings.txt) or
// OPENC2E_NO_BREED_PACKS. OPENC2E_BREED_PACK_DIR=folder takes the packs'
// files from that folder instead of downloading them (tests, offline).

#include "C2KitsInternal.h"
#include "Engine.h"
#include "PathResolver.h"
#include "World.h"

#include <atomic>
#include <cstdlib>
#include <fmt/format.h>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <memory>
#include <string>
#include <system_error>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

std::string readPlayerSetting(const std::string& key); // CreatureSpeech.h


namespace Openc2eImgui {

bool ImportBreedPackQuietly(const std::string& path, const std::string& name, bool inPlaceOfGame, std::string& message); // BreedersKit.cpp

namespace {

struct Pack {
	const char* title;
	const char* credit;
	const char* page;
	const char* url;
	const char* file; // the name the archive gives it
	const char* genome; // in the user data folder's Genetics once imported
	const char* magic; // what the file starts with
	const char* breedName;
	bool inPlaceOfGame;
};

const Pack kPacks[] = {
	{"Boney Grendels", "Creature Labs", "https://eem.foo/archive/downloads/1fad-boney-grendels",
		"https://eem.foo/archive/downloads/1fad?download=true", "Boney Grendels.exe", "bone.gen", "MZ", "Boney", false},
	{"Frog Norns V3", "Alex (Norns of the Round Table), from Nornenmeister's V2", "https://eem.foo/archive/downloads/2538-frog-norns-v3",
		"https://eem.foo/archive/downloads/2538?download=true", "2538-FrogNornV3_1.zip", "frog.gen", "PK", "Frog", true},
};
constexpr int kCount = sizeof(kPacks) / sizeof(kPacks[0]);

enum State { unchecked,
	installed,
	downloading,
	downloaded,
	failed };

struct Job {
	std::atomic<int> state{unchecked};
	fs::path path;
	std::string note; // shown to the player
};

Job s_jobs[kCount];
std::string s_status; // the latest line for the player
double s_status_until = 0;

bool enabled() {
	if (getenv("OPENC2E_NO_BREED_PACKS"))
		return false;
	return readPlayerSetting("breed-packs") != "off";
}

bool startsWith(const fs::path& p, const char* magic) {
	std::ifstream in(p, std::ios::binary);
	char b[2] = {0, 0};
	in.read(b, 2);
	return in && b[0] == magic[0] && b[1] == magic[1];
}

#ifndef _WIN32
std::string quoted(const std::string& s) {
	std::string q = "'";
	for (char c : s)
		q += c == '\'' ? std::string("'\\''") : std::string(1, c);
	return q + "'";
}
#endif

// on a thread: the system's curl (Windows 10 and later, macOS and most
// Linux systems have one), the pack's page as the referrer (the archive
// sends a plain request back to the page)
void download(int i) {
	const Pack& p = kPacks[i];
	Job& j = s_jobs[i];
	fs::path part = j.path;
	part += ".part";
#ifdef _WIN32
	// Windows: curl.exe started directly, with no window. std::system went
	// through cmd.exe, whose console window flashed up over the game for
	// each download. The part file's path as wide text, for a user folder
	// whose name is not plain ASCII; the page and address are ASCII.
	auto wide = [](const std::string& s) { return std::wstring(s.begin(), s.end()); };
	std::wstring cmd = L"curl.exe -fsSL --max-time 600 -A Zeetures -e \"" + wide(p.page) + L"\" -o \"" + part.wstring() + L"\" \"" + wide(p.url) + L"\"";
	int rc = -1;
	STARTUPINFOW si{};
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi{};
	if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
		WaitForSingleObject(pi.hProcess, INFINITE);
		DWORD code = 1;
		if (GetExitCodeProcess(pi.hProcess, &code))
			rc = (int)code;
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
	}
#else
	std::string cmd = fmt::format("curl -fsSL --max-time 600 -A Zeetures -e {} -o {} {}",
		quoted(p.page), quoted(part.string()), quoted(p.url));
	int rc = std::system(cmd.c_str());
#endif
	std::error_code ec;
	if (rc == 0 && fs::exists(part, ec) && startsWith(part, p.magic)) {
		fs::rename(part, j.path, ec);
		if (!ec) {
			j.state = downloaded;
			return;
		}
	}
	fs::remove(part, ec);
	j.note = fmt::format("Couldn't download {} (from {}). You can get it from that page and import it in the Breeder's Kit.", p.title, p.page);
	j.state = failed;
}

void say(const std::string& s) {
	fmt::print("* Breed packs: {}\n", s);
	s_status = s;
	s_status_until = ImGui::GetTime() + 10;
}

} // namespace

void BreedPacksTick() {
	if (engine.version != 2 || world.tickcount < 20)
		return;
	static bool on = enabled();
	if (!on)
		return;
	fs::path main = getUserDataDir();
	const char* localDir = getenv("OPENC2E_BREED_PACK_DIR");
	for (int i = 0; i < kCount; i++) {
		const Pack& p = kPacks[i];
		Job& j = s_jobs[i];
		int st = j.state;
		if (st == unchecked) {
			std::error_code ec;
			if (fs::exists(main / "Genetics" / p.genome, ec)) {
				j.state = installed;
				continue;
			}
			if (localDir) {
				j.path = fs::path(localDir) / p.file;
				j.state = fs::exists(j.path, ec) ? downloaded : failed;
				continue;
			}
			fs::create_directories(main / "Downloads", ec);
			j.path = main / "Downloads" / p.file;
			if (fs::exists(j.path, ec) && startsWith(j.path, p.magic)) {
				j.state = downloaded; // fetched before, not yet imported
				continue;
			}
			j.state = downloading;
			say(fmt::format("Downloading {} from eem.foo...", p.title));
			std::thread(download, i).detach();
		} else if (st == downloaded) {
			std::string message;
			bool ok = ImportBreedPackQuietly(j.path.string(), p.breedName, p.inPlaceOfGame, message);
			std::error_code ec;
			if (ok && fs::exists(main / "Genetics" / p.genome, ec)) {
				j.state = installed;
				say(fmt::format("{} ({}) added: {}", p.title, p.credit, message));
				engine.injectMissingBuiltinCobs();
			} else {
				j.state = failed;
				say(fmt::format("Couldn't import {}: {}", p.title, message));
			}
		} else if (st == failed && !j.note.empty()) {
			say(j.note);
			j.note.clear();
		}
	}
	// the latest line, briefly, above the status bar
	if (!s_status.empty() && ImGui::GetTime() < s_status_until) {
		ImGuiViewport* vp = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 8, vp->WorkPos.y + vp->WorkSize.y - 48), ImGuiCond_Always, ImVec2(0, 1));
		ImGui::SetNextWindowBgAlpha(0.85f);
		ImGui::Begin("##breedpacks", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
		ImGui::PushTextWrapPos(520);
		ImGui::TextUnformatted(s_status.c_str());
		ImGui::PopTextWrapPos();
		ImGui::End();
	}
}

} // namespace Openc2eImgui
