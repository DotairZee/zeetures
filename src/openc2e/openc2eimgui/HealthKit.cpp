// The Health Kit (Creatures 2): Fitness, Mind, Drives and Doctor pages, in
// the kits' own style (owner, 2026-11-12: not C2's panel art).
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
#include "historyManager.h"
#include "creatures/Disease.h"
#include "creatures/Lightning.h"
#include "creatures/CreatureAgent.h"
#include "creatures/Kinds.h"
#include "Agent.h"
#include "World.h"
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

// recent readings of the selected creature, sampled while the page shows
// (one every 3 s, the last 40: two minutes), for the tiles' trend lines
struct Trend {
	Agent* who = nullptr;
	double last = -1e9;
	std::vector<float> temp, pulse;
};
static Trend s_trend;

static void sampleTrend(Agent* a, float t, int bpm) {
	if (s_trend.who != a)
		s_trend = Trend{a};
	double now = ImGui::GetTime();
	if (now - s_trend.last < 3.0)
		return;
	s_trend.last = now;
	s_trend.temp.push_back(t);
	s_trend.pulse.push_back((float)bpm);
	for (auto* v : {&s_trend.temp, &s_trend.pulse})
		if (v->size() > 40)
			v->erase(v->begin());
}

static std::string organName(c2Creature* c, int i) {
	auto& organs = c->getOrgans();
	if (i < 0 || i >= (int)organs.size())
		return "an organ";
	if (const KitGeneNote* note = KitNoteFor(organs[i]->getGene()); note && !note->caption.empty())
		return note->caption;
	return fmt::format("Organ {}", i + 1);
}

static float organHealth(c2Creature* c, int i) {
	auto& o = c->getOrgans()[i];
	float start = o->getInitialLifeforce();
	return start > 0 ? std::clamp(o->getShortTermLifeforce() / start, 0.0f, 1.0f) : 1.0f;
}

static void FitnessPage() {
	if (!KitNeedsCreature())
		return;
	c2Creature* c = KitCreature();
	Agent* a = KitCreatureAgent();
	const float W = 540;
	if (!c->isAlive())
		KitNoteColored(ImVec4(1, 0.5f, 0.5f, 1), "This creature has died.");

	// overall: its weakest organ (what failing kills it; the status bar
	// shows the same)
	int damaged = 0, weakest = -1;
	float avg = 1;
	float h = KitOrganHealth(c, &damaged, &avg, &weakest);
	std::string weakName = organName(c, weakest);
	KitHeading("Overall health");
	KitBar(h, fmt::format("{:.0f}%", h * 100).c_str(), W);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%d organs, %.0f%% on average; %d under 75%%", (int)c->getOrgans().size(), avg * 100, damaged);
	std::string lower = weakName;
	for (auto& ch : lower)
		ch = (char)std::tolower((unsigned char)ch);
	if (h >= 0.9f)
		KitNote("Healthy. Its weakest organ, %s, is at %.0f%%.", weakName.c_str(), h * 100);
	else if (lower.find("arrow") != std::string::npos)
		KitNote("Held back by its weakest organ, %s: the ageing clock, which wears down with age in every creature. Nothing to treat.", weakName.c_str());
	else if (h < 0.4f)
		KitNoteColored(ImVec4(1, 0.6f, 0.5f, 1), "Held back by its weakest organ, %s, which is failing. The Doctor page's medicines may help.", weakName.c_str());
	else
		KitNote("Held back by its weakest organ, %s.", weakName.c_str());

	// the readings
	float t = temperatureOf(c);
	int bpm = pulseOf(c);
	sampleTrend(a, t, bpm);
	ImGui::Spacing();
	const float tw = (W - 2 * ImGui::GetStyle().ItemSpacing.x) / 3;
	const char* feel = t >= 38.5f ? "fever" : t >= 37.8f ? "warm" : t <= 35.5f ? "chilled" : "normal";
	ImU32 good = IM_COL32(110, 210, 110, 255), warn = IM_COL32(240, 175, 70, 255);
	bool f = KitFahrenheit();
	KitStatTile("temp", "Body temperature", fmt::format("{:.1f}", f ? t * 9 / 5 + 32 : t), f ? "\xc2\xb0""F" : "\xc2\xb0""C", feel, std::string(feel) == "normal" ? good : warn, tw, &s_trend.temp);
	if (ImGui::IsItemClicked())
		KitToggleFahrenheit();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("37 \xc2\xb0""C is normal. Click for %s.\nThe line: the last two minutes with this page open.", f ? "Celsius" : "Fahrenheit");
	ImGui::SameLine();
	const char* beat = !c->isAlive() ? "none" : c->isAsleep() ? "asleep" : bpm > 100 ? "racing" : bpm > 75 ? "quickened" : "resting";
	KitStatTile("pulse", "Pulse", c->isAlive() ? fmt::format("{}", bpm) : "-", "bpm", beat, bpm > 100 ? warn : good, tw, &s_trend.pulse);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Beats a minute: about 60 at rest, faster with fright or effort, slower asleep.\nThe line: the last two minutes with this page open.");
	ImGui::SameLine();
	{
		static const char* stages[] = {"baby", "child", "adolescent", "youth", "adult", "old", "senile"};
		int st = (int)c->getStage();
		monikerData* m = KitRecordsOf(a);
		std::string age = m ? KitAgeText(KitAgeTicks(*m), false) : std::string();
		KitStatTile("stage", "Life stage", st >= 0 && st < 7 ? stages[st] : "?", "", age.c_str(), IM_COL32(170, 200, 240, 255), tw);
	}

	// illness (Disease.h; owner, 2026-11-21): what it has, how far its
	// antibodies have come, what it is immune to, or what a grendel carries
	if (c->isAlive() && world.history && !Disease::strains().empty()) {
		std::string mon = world.history->findMoniker(c->getGenome());
		KitHeading("Illness");
		auto ill = Disease::illnessesOf(mon);
		if (a->genus == 2) {
			if (const Disease::Strain* s = Disease::strain(Disease::carriedBy(mon)))
				KitNote("Carries %s, which it spreads wherever it goes. Grendels do not fall ill from it.", s->name().c_str());
			else
				KitNote("Carries no germs.");
		} else if (ill.empty())
			KitNote("Not ill.");
		for (auto& i : ill) {
			const Disease::Strain* s = Disease::strain(i.strain);
			if (!s)
				continue;
			unsigned int ticks = world.worldtickcount >= i.since ? world.worldtickcount - i.since : 0;
			std::string howLong = KitAgeText(ticks, false);
			KitNoteColored(ImVec4(1, 0.75f, 0.45f, 1), "Ill with %s for %s.", s->name().c_str(), howLong.c_str());
			float ab = c->getChemical((unsigned char)(240 + s->antigen)) / 90.0f;
			KitBar(std::min(1.0f, ab), fmt::format("antibodies {:.0f}%", std::min(1.0f, ab) * 100).c_str(), W);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Its body makes antibody %d against the germs' antigen; at 100%% the illness is over.\nWarmth, food and vitamin C help it along.", s->antigen);
		}
		if (a->genus != 2) {
			std::string immune;
			for (auto& s : Disease::strains())
				if (c->getChemical((unsigned char)(240 + s.antigen)) >= 60)
					immune += (immune.empty() ? "" : ", ") + s.name();
			if (!immune.empty())
				KitNote("Immune for now to %s.", immune.c_str());
		}
	}

	// the weakest organs
	KitHeading("Weakest organs");
	auto& organs = c->getOrgans();
	std::vector<std::pair<float, int>> order;
	for (int i = 0; i < (int)organs.size(); i++)
		if (organs[i]->getInitialLifeforce() > 0)
			order.push_back({organHealth(c, i), i});
	std::sort(order.begin(), order.end());
	if (ImGui::BeginTable("weakest", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit, ImVec2(W, 0))) {
		ImGui::TableSetupColumn("Organ", ImGuiTableColumnFlags_WidthFixed, W - 250);
		ImGui::TableSetupColumn("Health", ImGuiTableColumnFlags_WidthFixed, 240);
		ImGui::TableHeadersRow();
		for (size_t k = 0; k < order.size() && k < 5; k++) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextWrapped("%s", organName(c, order[k].second).c_str());
			ImGui::TableNextColumn();
			KitBar(order[k].first, fmt::format("{:.0f}%", order[k].first * 100).c_str(), 236);
		}
		ImGui::EndTable();
	}
	ImGui::Spacing();
	if (ImGui::Button(fmt::format("All {} organs in the Science Kit", (int)organs.size()).c_str()))
		OpenC2KitAt("science", "Physical");
}

// ---- Mind ------------------------------------------------------------------------

static void MindPage() {
	if (!KitNeedsCreature())
		return;
	c2Creature* c = KitCreature();
	const float W = 540;

	KitHeading("Doing now");
	KitBeginCard("doing", ImVec2(W, 0));
	ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + W - 24);
	ImGui::TextUnformatted(DescribeCreatureActivity(KitCreatureAgent()).c_str());
	ImGui::PopTextWrapPos();
	KitEndCard();

	// what it wants most: the three strongest drives
	KitHeading("Strongest needs");
	const auto& names = KitChemicalNames();
	std::vector<std::pair<int, int>> d;
	for (int i = 0; i < 17; i++)
		d.push_back({c->getDrive(i), i});
	std::sort(d.rbegin(), d.rend());
	for (int k = 0; k < 3; k++) {
		std::string n = names.size() > (size_t)(d[k].second + 1) ? names[d[k].second + 1] : fmt::format("Drive {}", d[k].second);
		KitBar(d[k].first / 255.0f, n.c_str(), W, true);
	}
	KitNote("All its needs are on the Drives page.");
	ImGui::Spacing();

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
	if (!KitNeedsCreature())
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
	KitCobShelf("Medicine", "Click a medicine to see what it is.", "{} added. Encourage your creature to take it.");
}

void DrawHealthKit() {
	KitCreatureHeader();
	// what lightning has done to it, by name, and nothing at all until it has
	// (owner, 2026-11-26: "I want it to be a surprise"; Lightning.h); resting
	// the pointer on a name says what it does (owner, 2026-11-26)
	if (Agent* a = KitCreatureAgent()) {
		std::vector<std::string> names = Lightning::statusOf(a);
		if (!names.empty()) {
			ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 214, 102, 255));
			float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
			for (size_t i = 0; i < names.size(); i++) {
				std::string label = names[i] + (i + 1 < names.size() ? "," : "");
				if (i > 0) {
					ImGui::SameLine();
					// (to the next line when it would not fit)
					if (ImGui::GetCursorScreenPos().x + ImGui::CalcTextSize(label.c_str()).x > right)
						ImGui::NewLine();
				}
				ImGui::TextUnformatted(label.c_str());
				if (ImGui::IsItemHovered()) {
					std::string what = Lightning::describe(names[i]);
					if (!what.empty()) {
						ImGui::PopStyleColor();
						ImGui::BeginTooltip();
						ImGui::PushTextWrapPos(ImGui::GetFontSize() * 22.0f);
						ImGui::TextUnformatted(what.c_str());
						ImGui::PopTextWrapPos();
						ImGui::EndTooltip();
						ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 214, 102, 255));
					}
				}
			}
			ImGui::PopStyleColor();
		}
	}
	// what its kind is and means (Kinds.h; owner, 2026-11-26): grendels and
	// ettins by name, the details on the pointer; and a norn that has learned
	// to be wary of grendels
	if (Agent* a = KitCreatureAgent()) {
		CreatureAgent* ca = dynamic_cast<CreatureAgent*>(a);
		oldCreature* oc = ca ? dynamic_cast<oldCreature*>(ca->getCreature()) : nullptr;
		std::vector<std::string> traits = Kinds::traits(oc);
		std::string name = Kinds::kindName(oc);
		if (name.empty() && !traits.empty())
			name = "Wary of grendels";
		if (!name.empty()) {
			ImGui::TextDisabled("%s", name.c_str());
			if (ImGui::IsItemHovered() && !traits.empty()) {
				ImGui::BeginTooltip();
				ImGui::PushTextWrapPos(ImGui::GetFontSize() * 24.0f);
				for (auto& t : traits)
					ImGui::BulletText("%s", t.c_str());
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}
		}
	}
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
