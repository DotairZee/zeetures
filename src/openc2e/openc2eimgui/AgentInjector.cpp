#include "AgentInjector.h"

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
static Texture s_cob_texture;
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

// objects built into every world (builtin-cobs.txt) and ones that clash
// with the project's changes (hidden-cobs.txt) are not offered
static void hideListed() {
	auto built = CobManager::readNameList(getUserDataDir() / "Additions" / "builtin-cobs.txt");
	auto hidden = CobManager::readNameList(getUserDataDir() / "Additions" / "hidden-cobs.txt");
	auto& objs = s_object_injector.objects;
	objs.erase(std::remove_if(objs.begin(), objs.end(), [&](auto& o) {
		return std::find(built.begin(), built.end(), o.name) != built.end() || std::find(hidden.begin(), hidden.end(), o.name) != hidden.end();
	}),
		objs.end());
	if (s_object_selected_index >= objs.size())
		s_object_selected_index = 0;
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
	s_cob_texture = {};
	if (getenv("OPENC2E_TEST_IMPORT_OBJECTS"))
		for (auto& line : s_import_report)
			fmt::print("* IMPORT {}\n", line);
}

static void DrawAgentPreview(Texture& texture) {
	ImVec2 size = ImVec2(std::max<float>(300, ImGui::GetContentRegionAvail().x), 211);
	ImDrawList* drawlist = ImGui::GetWindowDrawList();
	ImVec2 p = ImGui::GetCursorScreenPos();
	// should be approx 300 x 170. Alima.bmp is 312 x 211

	ImRect bb(p, p + size);
	drawlist->AddRectFilled(
		bb.Min,
		bb.Max,
		IM_COL32(0, 0, 0, 255),
		ImGui::GetStyle().FrameRounding);

	if (texture) {
		int w = texture.width, h = texture.height;
		// w *= io.DisplayFramebufferScale.x;
		// h *= io.DisplayFramebufferScale.y;
		ImVec2 tex_p = p + size / 2 - ImVec2(w, h) / 2;
		drawlist->AddImage(texture.as<ImTextureID>(), tex_p, tex_p + ImVec2(w, h));
	}

	ImGui::Dummy(size);
}

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
	if (ImGui::Button("Agent Injector##chip")) {
		s_agent_injector_minimized = false;
		s_agent_injector_focus = true;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Restore the Agent Injector");
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
	if (ImGuiUtils::BeginWindow("Agent Injector", &s_agent_injector_open)) {
		if (engine.version == 2 && KitTitleMinimizeButton())
			s_agent_injector_minimized = true;
		if (!s_read_agents) {
			s_object_injector.update();
			hideListed();
			s_read_agents = true;
		}
		importPicked();

		{
			ImGui::BeginChild("Objects", ImVec2(300, 100), true);
			for (size_t i = 0; i < s_object_injector.objects.size(); ++i) {
				auto obj = s_object_injector.objects[i];
				if (ImGui::Selectable(obj.name.c_str(), i == s_object_selected_index)) {
					s_cob_texture = {};
					s_object_selected_index = i;
				}
			}
			ImGui::EndChild();
		}

		if (!s_cob_texture && s_object_selected_index < s_object_injector.objects.size()) {
			s_cob_texture = get_backend()->createTextureFromImage(
				s_object_injector.getPicture(s_object_injector.objects[s_object_selected_index]));
		}
		DrawAgentPreview(s_cob_texture);

		if (ImGui::Button("Add Object") && s_object_selected_index < s_object_injector.objects.size()) {
			s_object_injector.inject(s_object_injector.objects[s_object_selected_index], true);
		}
		ImGui::SameLine();
		if (s_object_selected_index < s_object_injector.objects.size() && s_object_injector.objects[s_object_selected_index].is_removable) {
			if (ImGui::Button("Remove Object")) {
				s_object_injector.remove(s_object_injector.objects[s_object_selected_index], true);
			}
		} else {
			ImGuiUtils::DisabledButton("Remove Object");
		}
		ImGui::SameLine();
		if (ImGui::Button("Import...")) {
			static const SDL_DialogFileFilter filters[] = {{"Objects (COB files, zips, add-on installers)", "cob;zip;exe"}, {"All files", "*"}};
			SDL_ShowOpenFileDialog(ObjectFilesChosen, nullptr, nullptr, filters, 2, nullptr, true);
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Add COB files, or zips of them, to this list (copied into your own data folder)");
		ImGui::TextDisabled("Objects you add are saved with the world.");
		for (auto& line : s_import_report)
			ImGui::TextWrapped("%s", line.c_str());

		ImGuiUtils::EndWindow();
	}
}

} // namespace Openc2eImgui