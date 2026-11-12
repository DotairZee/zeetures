// The Health Kit (Creatures 2): Fitness, Mind, Drives and Doctor pages.
// C2's own art from the player's Applet Data; our layout and readings.
// "Brain" in C2 showed lobe activity; our creatures run their own mind, so
// that page is "Mind" here.

#include "C2EyeView.h"
#include "C2Kits.h"
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

static void FitnessPage() {
	if (!KitNeedsCreature("Applet Data/HealthCover.bmp"))
		return;
	c2Creature* c = KitCreature();
	Texture bg = KitPicture("Applet Data/Fitness.bmp");
	KitCenterNext(320);
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
	if (KitFahrenheit())
		KitLed(p + ImVec2(24, 162), fmt::format("{:.1f}F", t * 9 / 5 + 32));
	else
		KitLed(p + ImVec2(28, 162), fmt::format("{:.1f}C", t));
	{
		ImVec2 keep = ImGui::GetCursorScreenPos();
		ImGui::SetCursorScreenPos(p + ImVec2(20, 136));
		if (ImGui::InvisibleButton("tempunits", ImVec2(94, 92)))
			KitToggleFahrenheit();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Click for %s", KitFahrenheit() ? "Celsius" : "Fahrenheit");
		ImGui::SetCursorScreenPos(keep);
	}
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
	int damaged = 0, weakest = -1;
	float avg = 1;
	float h = KitOrganHealth(c, &damaged, &avg, &weakest);
	std::string weakName = "an organ";
	if (weakest >= 0 && weakest < (int)c->getOrgans().size())
		if (const KitGeneNote* note = KitNoteFor(c->getOrgans()[weakest]->getGene()); note && !note->caption.empty())
			weakName = note->caption;
	std::string tip = fmt::format("Health is its weakest organ, {}, at {:.0f}% of its life force\n(what failing kills it; the status bar shows the same)\n{} organs, {:.0f}% on average; {} under 75%\nDetails in the Science Kit's Physical page.",
		weakName, h * 100, (int)c->getOrgans().size(), avg * 100, damaged);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", tip.c_str());
	ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + 244));
	KitBar(h, fmt::format("Health {:.0f}% (weakest organ)", h * 100).c_str(), 320);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", tip.c_str());
	if (!c->isAlive())
		ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "This creature has died.");
}

// ---- Mind ------------------------------------------------------------------------

static void MindPage() {
	if (!KitNeedsCreature("Applet Data/HealthCover.bmp"))
		return;
	c2Creature* c = KitCreature();
	Texture bg = KitPicture("Applet Data/Brain.bmp");
	KitCenterNext(320);
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

	if (c->getMind()) {
		if (ImGui::Button("Open the Mind Kit"))
			OpenC2Kit("mind");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Its feelings, choices, lessons, memories and mood");
	} else
		KitNote("This creature runs the original brain.");
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
	KitNote("The longer and redder the bar, the stronger the need.");
}

// ---- Doctor ----------------------------------------------------------------------

static void DoctorPage() {
	// three shelves (their tops at y 66, 128, 178), three medicines on each
	KitCobShelf("Applet Data/Shelves.bmp", "Medicine", {66, 128, 178}, {60, 157, 254}, "Click a medicine to see what it is.", "{} added. Encourage your creature to take it.");
}

void DrawHealthKit() {
	KitCreatureHeader();
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
