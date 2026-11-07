#include "WorldChooser.h"

#include "Engine.h"
#include "PathResolver.h"
#include "common/Ascii.h"
#include "common/backend/Backend.h"
#include "common/backend/BackendEvent.h"

#include <algorithm>
#include <fmt/core.h>
#include <imgui.h>
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
	std::sort(result.begin(), result.end(), [](const fs::path& a, const fs::path& b) {
		bool aeden = to_ascii_lowercase(a.filename().string()) == "eden.sfc";
		bool beden = to_ascii_lowercase(b.filename().string()) == "eden.sfc";
		if (aeden != beden)
			return aeden;
		return a < b;
	});
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
	fmt::print("* Created world '{}' from {}\n", name, tmpl.filename().string());
	return true;
}

void RelaunchIntoWorld(const std::string& name) {
#ifdef __linux__
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
	std::vector<char*> argv;
	for (auto& a : args)
		argv.push_back(a.data());
	argv.push_back(nullptr);
	fmt::print("* Switching to world '{}'\n", name);
	engine.shutdown();
	execv("/proc/self/exe", argv.data());
	fmt::print(stderr, "Couldn't restart into world '{}'\n", name);
	_exit(1);
#else
	fmt::print(stderr, "Switching worlds needs a restart: run again with --world \"{}\"\n", name);
#endif
}

// The new-world form, shared by the start-up chooser and the File menu.
// Returns the created world's name, or "" while still being filled in.
// With reset = true it only clears the form and draws nothing.
std::string DrawNewWorldForm(bool reset) {
	static char s_name[64] = "";
	static int s_template = 0;
	static std::string s_error;
	static std::vector<fs::path> s_templates;
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
	bool enter = ImGui::InputText("##worldname", s_name, sizeof(s_name), ImGuiInputTextFlags_EnterReturnsTrue);
	ImGui::Text("Start from:");
	ImGui::SetNextItemWidth(260);
	std::string preview = s_templates.empty() ? "(no world files found)" : s_templates[s_template].filename().string();
	if (ImGui::BeginCombo("##template", preview.c_str())) {
		for (int i = 0; i < (int)s_templates.size(); i++) {
			std::string label = s_templates[i].filename().string();
			if (to_ascii_lowercase(label) == "eden.sfc")
				label += "  (a fresh Albia)";
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

std::string RunWorldChooser() {
	std::string chosen;
	bool quit = false;
	bool creating = ListWorlds().empty();
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
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x / 2, io.DisplaySize.y / 2), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(360, 0));
		ImGui::Begin("Choose a World", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
		static int s_selected = 0;
		auto worlds = ListWorlds();
		if (!creating && !worlds.empty()) {
			ImGui::Text("Your worlds:");
			if (ImGui::BeginListBox("##worlds", ImVec2(-1, 160))) {
				for (int i = 0; i < (int)worlds.size(); i++) {
					if (ImGui::Selectable(worlds[i].name.c_str(), i == s_selected, ImGuiSelectableFlags_AllowDoubleClick)) {
						s_selected = i;
						if (ImGui::IsMouseDoubleClicked(0))
							chosen = worlds[i].name;
					}
				}
				ImGui::EndListBox();
			}
			s_selected = std::clamp(s_selected, 0, (int)worlds.size() - 1);
			if (ImGui::Button("Open"))
				chosen = worlds[s_selected].name;
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

	return quit ? "" : chosen;
}

} // namespace Openc2eImgui
