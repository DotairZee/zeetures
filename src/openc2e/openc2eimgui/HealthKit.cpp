// The Health Kit (Creatures 2): Fitness, Mind, Drives and Doctor pages.
// C2's own art from the player's Applet Data; our layout and readings.
// "Brain" in C2 showed lobe activity; our creatures run their own mind, so
// that page is "Mind" here.

#include "C2EyeView.h"
#include "C2KitsInternal.h"
#include "CobManager.h"
#include "PathResolver.h"
#include "common/Ascii.h"
#include "creatures/CreatureMind.h"
#include "creatures/oldCreature.h"
#include "fileformats/c2cobfile.h"
#include "common/backend/Backend.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fmt/format.h>
#include <imgui.h>

using namespace ImGuiUtils;
namespace fs = std::filesystem;

namespace Openc2eImgui {

// ---- readings (our own; C2 showed a pulse and a body temperature) ----------

// beats a minute: resting 60, up with adrenaline (172) and exertion
// (Tiredness rising), down in sleep
static int pulseOf(c2Creature* c) {
	if (!c->isAlive())
		return 0;
	float bpm = 60 + c->getChemical(172) * 0.35f + c->getDrive(5) * 0.05f;
	if (c->isAsleep())
		bpm -= 12;
	return std::clamp((int)std::lround(bpm), 30, 199);
}

// degrees C: 37 normal, warmer with Hotness and fever toxin (235), cooler
// with Coldness
static float temperatureOf(c2Creature* c) {
	float t = 37.0f + (c->getDrive(4) - c->getDrive(3)) / 255.0f * 3.0f + c->getChemical(235) / 255.0f * 3.0f;
	return std::clamp(t, 30.0f, 43.0f);
}

// overall organ health: short-term life force against what each organ
// started with, 0-1
static float organHealth(c2Creature* c, int* damaged = nullptr) {
	float sum = 0;
	int n = 0, bad = 0;
	for (auto& o : c->getOrgans()) {
		float start = o->getInitialLifeforce();
		if (start <= 0)
			continue;
		float h = std::clamp(o->getShortTermLifeforce() / start, 0.0f, 1.0f);
		sum += h;
		n++;
		if (h < 0.75f)
			bad++;
	}
	if (damaged)
		*damaged = bad;
	return n ? sum / n : 0;
}

static void FitnessPage() {
	if (!KitNeedsCreature("Applet Data/HealthCover.bmp"))
		return;
	c2Creature* c = KitCreature();
	Texture bg = KitPicture("Applet Data/Fitness.bmp");
	ImVec2 p = ImGui::GetCursorScreenPos();
	DrawTexture(bg, p);
	ImGui::Dummy(ImVec2(320, 240));
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImU32 green = IM_COL32(170, 255, 170, 255);

	// the small screen: pulse, with a blip in time with it
	int bpm = pulseOf(c);
	std::string s = fmt::format("{:3d}", bpm);
	dl->AddText(p + ImVec2(33, 20), green, "PULSE");
	KitLed(p + ImVec2(34, 38), s);
	if (bpm > 0) {
		double beat = ImGui::GetTime() * bpm / 60.0;
		if (beat - std::floor(beat) < 0.18)
			dl->AddCircleFilled(p + ImVec2(84, 25), 3.0f, green);
	}
	dl->AddText(p + ImVec2(56, 76), green, "bpm");

	// the big screen: body temperature
	float t = temperatureOf(c);
	dl->AddText(p + ImVec2(28, 142), green, "BODY TEMP");
	KitLed(p + ImVec2(28, 162), fmt::format("{:.1f}C", t));
	const char* feel = t >= 38.5f ? "fever" : t >= 37.8f ? "warm" : t <= 35.5f ? "chilled" : "normal";
	dl->AddText(p + ImVec2(28, 202), green, feel);

	// the thermometer: hide the red above the level behind glass
	{
		float f = std::clamp((t - 33.0f) / 10.0f, 0.0f, 1.0f);
		float top = 97, bottom = 200;
		float level = bottom - f * (bottom - top);
		dl->AddRectFilled(p + ImVec2(285, top), p + ImVec2(299, level), IM_COL32(205, 215, 222, 235), 4.0f);
	}

	// the X-ray: the organs' state on hover
	ImGui::SetCursorScreenPos(p + ImVec2(128, 28));
	ImGui::InvisibleButton("xray", ImVec2(124, 140));
	int damaged = 0;
	float h = organHealth(c, &damaged);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%d organs, %.0f%% of their life force on average\n%d damaged (under 75%%)\nDetails in the Science Kit's Physical page.", (int)c->getOrgans().size(), h * 100, damaged);
	ImGui::SetCursorScreenPos(p + ImVec2(0, 244));
	KitBar(h, fmt::format("Organ health {:.0f}%", h * 100).c_str(), 320);
	if (!c->isAlive())
		ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "This creature has died.");
}

// ---- Mind ------------------------------------------------------------------------

static void MindPage() {
	if (!KitNeedsCreature("Applet Data/HealthCover.bmp"))
		return;
	c2Creature* c = KitCreature();
	Texture bg = KitPicture("Applet Data/Brain.bmp");
	ImVec2 p = ImGui::GetCursorScreenPos();
	DrawTexture(bg, p);
	ImGui::Dummy(ImVec2(320, 240));
	ImDrawList* dl = ImGui::GetWindowDrawList();

	// what it is doing, top
	std::string doing = DescribeCreatureActivity(KitCreatureAgent());
	dl->AddRectFilled(p + ImVec2(6, 6), p + ImVec2(314, 30), IM_COL32(0, 0, 20, 170), 3);
	dl->AddText(p + ImVec2(12, 11), IM_COL32(255, 240, 160, 255), doing.c_str());

	// what it wants most: the three strongest drives, bottom
	const auto& names = KitChemicalNames();
	std::vector<std::pair<int, int>> d;
	for (int i = 0; i < 17; i++)
		d.push_back({c->getDrive(i), i});
	std::sort(d.rbegin(), d.rend());
	dl->AddRectFilled(p + ImVec2(6, 168), p + ImVec2(314, 234), IM_COL32(0, 0, 20, 170), 3);
	dl->AddText(p + ImVec2(12, 172), IM_COL32(200, 220, 255, 255), "Strongest needs");
	for (int k = 0; k < 3; k++) {
		float y = 190 + k * 14;
		std::string n = names.size() > (size_t)(d[k].second + 1) ? names[d[k].second + 1] : fmt::format("Drive {}", d[k].second);
		dl->AddText(p + ImVec2(16, y), IM_COL32(255, 255, 255, 255), n.c_str());
		float w = 150 * d[k].first / 255.0f;
		dl->AddRectFilled(p + ImVec2(150, y + 3), p + ImVec2(150 + w, y + 11), IM_COL32(120, 200, 255, 230), 2);
		dl->AddRect(p + ImVec2(150, y + 3), p + ImVec2(300, y + 11), IM_COL32(120, 200, 255, 120), 2);
	}

	if (CreatureMind* m = c->getMind()) {
		ImGui::Text("Mind: %s", m->name().c_str());
		std::string st = m->debugState();
		if (!st.empty())
			ImGui::TextWrapped("%s", st.c_str());
	} else {
		ImGui::TextDisabled("This creature runs the original brain (no --mind).");
	}
	ImGui::TextDisabled("The full view of the mind will be the Neuroscience Kit.");
}

// ---- Drives ----------------------------------------------------------------------

static void DrivesPage() {
	if (!KitNeedsCreature("Applet Data/HealthCover.bmp"))
		return;
	c2Creature* c = KitCreature();
	const auto& names = KitChemicalNames();
	int top = 0;
	for (int i = 1; i < 17; i++)
		if (c->getDrive(i) > c->getDrive(top))
			top = i;
	if (ImGui::BeginTable("drives", 2, ImGuiTableFlags_SizingFixedFit)) {
		for (int i = 0; i < 17; i++) {
			ImGui::TableNextColumn();
			std::string n = names.size() > (size_t)(i + 1) ? names[i + 1] : fmt::format("Drive {}", i);
			if (i == top && c->getDrive(i) > 40)
				n += "  <";
			KitBar(c->getDrive(i) / 255.0f, n.c_str(), 192, true);
		}
		ImGui::EndTable();
	}
	ImGui::TextDisabled("The longer and redder the bar, the stronger the need.");
}

// ---- Doctor ----------------------------------------------------------------------

struct Medicine {
	std::string name, description;
	fs::path file;
	Texture picture;
};

static std::vector<fs::path> appletDataFiles(const std::string& sub, const std::string& ext) {
	std::vector<fs::path> out;
	for (auto& dir : getMainDirectories()) {
		std::error_code ec;
		for (auto& a : fs::directory_iterator(dir, ec)) {
			if (to_ascii_lowercase(a.path().filename().string()) != "applet data")
				continue;
			for (auto& b : fs::directory_iterator(a.path(), ec)) {
				if (to_ascii_lowercase(b.path().filename().string()) != to_ascii_lowercase(sub) || !b.is_directory())
					continue;
				for (auto& f : fs::directory_iterator(b.path(), ec))
					if (to_ascii_lowercase(f.path().extension().string()) == ext)
						out.push_back(f.path());
			}
		}
	}
	std::sort(out.begin(), out.end());
	return out;
}

static std::vector<Medicine>& medicines() {
	static std::vector<Medicine> list;
	static bool loaded = false;
	if (loaded)
		return list;
	loaded = true;
	for (auto& f : appletDataFiles("Medicine", ".cob")) {
		try {
			c2cobfile cob(f);
			for (auto b : cob.blocks) {
				if (b->type != "agnt")
					continue;
				cobAgentBlock a(b);
				Medicine m;
				m.name = a.name;
				m.description = a.description;
				m.file = f;
				if (a.thumbnail.width > 0)
					m.picture = get_backend()->createTextureFromImage(a.thumbnail);
				list.push_back(m);
				break;
			}
		} catch (std::exception& e) {
			fmt::print("Health Kit: couldn't read {}: {}\n", f.string(), e.what());
		}
	}
	return list;
}

static void DoctorPage() {
	static int selected = -1;
	static std::string message;
	auto& meds = medicines();
	Texture bg = KitPicture("Applet Data/Shelves.bmp");
	ImVec2 p = ImGui::GetCursorScreenPos();
	DrawTexture(bg, p);
	ImGui::Dummy(ImVec2(314, 195));
	// three shelves (their tops at y 66, 128, 178), three medicines on each
	const float shelf[3] = {66, 128, 178};
	const float col[3] = {60, 157, 254};
	for (size_t i = 0; i < meds.size() && i < 9; i++) {
		auto& m = meds[i];
		float w = m.picture ? (float)m.picture.width : 24, h = m.picture ? (float)m.picture.height : 24;
		float s = std::min(1.0f, std::min(80.0f / w, 52.0f / h)); // fit a shelf
		ImVec2 sz(w * s, h * s);
		ImVec2 at = p + ImVec2(col[i % 3] - sz.x / 2, shelf[i / 3] - sz.y);
		ImGui::SetCursorScreenPos(at);
		ImGui::PushID((int)i);
		if (ImGui::InvisibleButton("med", sz))
			selected = (int)i;
		bool hov = ImGui::IsItemHovered();
		ImGui::PopID();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		if ((int)i == selected || hov)
			dl->AddRectFilled(at - 3, at + sz + 3, (int)i == selected ? IM_COL32(255, 230, 120, 90) : IM_COL32(255, 255, 255, 50), 4);
		if (m.picture)
			dl->AddImage(m.picture.as<ImTextureID>(), at, at + sz);
		else
			dl->AddText(at, IM_COL32(0, 0, 0, 255), m.name.c_str());
		if (hov)
			ImGui::SetTooltip("%s", m.name.c_str());
	}
	ImGui::SetCursorScreenPos(p + ImVec2(0, 199));
	if (meds.empty()) {
		ImGui::TextDisabled("No medicines found in Applet Data/Medicine.");
		return;
	}
	if (selected >= 0 && selected < (int)meds.size()) {
		auto& m = meds[selected];
		ImGui::TextUnformatted(m.name.c_str());
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 314);
		ImGui::TextDisabled("%s", m.description.c_str());
		ImGui::PopTextWrapPos();
		if (ImGui::Button("Add to the world")) {
			try {
				CobManager cm;
				cm.inject(CobManager::CobFileInfo(m.name, m.file));
				message = fmt::format("{} added. Encourage your creature to take it.", m.name);
			} catch (std::exception& e) {
				message = fmt::format("Couldn't add {}: {}", m.name, e.what());
			}
		}
	} else {
		ImGui::TextDisabled("Click a medicine to see what it is.");
	}
	if (!message.empty())
		ImGui::TextUnformatted(message.c_str());
}

void DrawHealthKit() {
	if (ImGui::BeginTabBar("pages")) {
		if (KitTab("Fitness")) {
			FitnessPage();
			ImGui::EndTabItem();
		}
		if (KitTab("Mind")) {
			MindPage();
			ImGui::EndTabItem();
		}
		if (KitTab("Drives")) {
			DrivesPage();
			ImGui::EndTabItem();
		}
		if (KitTab("Doctor")) {
			DoctorPage();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

} // namespace Openc2eImgui
