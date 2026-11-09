// The Observation Kit (Creatures 2): Details (every creature in the world:
// name, kind, age, health, pregnancy, what it is doing and where) and
// Alarms (warn when a chemical in a creature goes above or below a level).
// Alarms are kept in kits.ini and checked whether or not the kit is open; a
// note pops up over the world when one goes off.

#include "C2EyeView.h"
#include "Agent.h"
#include "C2KitsInternal.h"
#include "Engine.h"
#include "World.h"
#include "creatures/oldCreature.h"
#include "historyManager.h"

#include <algorithm>
#include <cstring>
#include <deque>
#include <fmt/format.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <map>

using namespace ImGuiUtils;

namespace Openc2eImgui {

// ---- Details ---------------------------------------------------------------------

static void DetailsPage() {
	auto creatures = KitLivingCreatures();
	if (creatures.empty()) {
		Texture t = KitPicture("Applet Data/Observation.bmp");
		if (t) {
			ImVec2 p = ImGui::GetCursorScreenPos();
			DrawTexture(t, p);
			ImGui::Dummy(ImVec2(t.width, t.height));
		}
		ImGui::TextDisabled("No creatures in this world.");
		return;
	}
	ImGui::Text("%d creature%s. Click a row to select it; Go to look at it.", (int)creatures.size(), creatures.size() == 1 ? "" : "s");
	static const char* stages[] = {"baby", "child", "adolescent", "youth", "adult", "old", "senile"};
	ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable;
	if (!ImGui::BeginTable("creatures", 8, flags, ImVec2(0, ImGui::GetContentRegionAvail().y)))
		return;
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, 120);
	ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, 96);
	ImGui::TableSetupColumn("Age", ImGuiTableColumnFlags_WidthFixed, 96);
	ImGui::TableSetupColumn("Health", ImGuiTableColumnFlags_WidthFixed, 64);
	ImGui::TableSetupColumn("Preg.", ImGuiTableColumnFlags_WidthFixed, 44);
	ImGui::TableSetupColumn("Doing", ImGuiTableColumnFlags_WidthStretch, 2.0f);
	ImGui::TableSetupColumn("Where", ImGuiTableColumnFlags_WidthStretch, 1.2f);
	ImGui::TableSetupColumn("", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed, 26);
	ImGui::TableHeadersRow();

	struct Row {
		std::shared_ptr<Agent> a;
		std::string name, kind, age, doing, where;
		unsigned int ageTicks = 0;
		float health = 0;
		int pregnant = 0;
	};
	std::vector<Row> rows;
	for (auto& a : creatures) {
		Row r;
		r.a = a;
		r.name = KitCreatureName(a.get());
		c2Creature* c = KitC2Of(a.get());
		r.kind = fmt::format("{} {}", c && c->isFemale() ? "female" : "male", KitSpeciesWord(a.get()));
		if (monikerData* m = KitRecordsOf(a.get())) {
			r.ageTicks = KitAgeTicks(*m);
			unsigned int days = r.ageTicks / 18000;
			int st = c ? (int)c->getStage() : -1;
			r.age = fmt::format("{} ({} d{})", st >= 0 && st < 7 ? stages[st] : "?", days, m->bornknown ? "" : "+");
		}
		if (c) {
			r.health = KitOrganHealth(c);
			r.pregnant = c->getPregnant();
		}
		r.doing = DescribeCreatureActivity(a.get());
		r.where = KitWhere(a.get());
		rows.push_back(r);
	}
	if (ImGuiTableSortSpecs* sort = ImGui::TableGetSortSpecs()) {
		if (sort->SpecsCount > 0) {
			auto& sp = sort->Specs[0];
			bool asc = sp.SortDirection == ImGuiSortDirection_Ascending;
			std::stable_sort(rows.begin(), rows.end(), [&](const Row& x, const Row& y) {
				bool less;
				switch (sp.ColumnIndex) {
					case 1: less = x.kind < y.kind; break;
					case 2: less = x.ageTicks < y.ageTicks; break;
					case 3: less = x.health < y.health; break;
					case 4: less = x.pregnant < y.pregnant; break;
					case 5: less = x.doing < y.doing; break;
					case 6: less = x.where < y.where; break;
					default: less = x.name < y.name; break;
				}
				return asc ? less : !less && !(x.name == y.name);
			});
		}
	}
	for (auto& r : rows) {
		ImGui::PushID(r.a.get());
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		bool selected = world.selectedcreature.get() == r.a.get();
		if (ImGui::Selectable(r.name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
			world.selectCreature(r.a);
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(r.kind.c_str());
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(r.age.c_str());
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("days of game time%s", r.age.find('+') != std::string::npos ? "; + counted from when this world first saw it" : "");
		ImGui::TableNextColumn();
		ImVec4 col = r.health > 0.75f ? ImVec4(0.3f, 0.8f, 0.3f, 1) : r.health > 0.4f ? ImVec4(0.9f, 0.7f, 0.2f, 1) : ImVec4(0.9f, 0.3f, 0.2f, 1);
		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, col);
		ImGui::ProgressBar(r.health, ImVec2(-1, 0), fmt::format("{:.0f}%", r.health * 100).c_str());
		ImGui::PopStyleColor();
		ImGui::TableNextColumn();
		if (r.pregnant)
			ImGui::Text("%d%%", r.pregnant * 100 / 255);
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(r.doing.c_str());
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(r.where.c_str());
		ImGui::TableNextColumn();
		if (ImGui::SmallButton("Go"))
			KitGoTo(r.a);
		ImGui::PopID();
	}
	ImGui::EndTable();
}

// ---- Alarms -----------------------------------------------------------------------

struct Alarm {
	std::string who = "*"; // a creature's name, or * for any creature
	int chem = 99;
	bool above = false; // false: below
	int level = 50;
	bool on = true;
	std::map<Agent*, bool> tripped; // per creature: gone off, until it comes back
};

struct AlarmEvent {
	std::string text, when;
	AgentRef who;
	double shownAt = 0;
};

static std::vector<Alarm> s_alarms;
static std::deque<AlarmEvent> s_log;
static unsigned int s_lastCheck = 0;

void KitAlarmsReadLine(const char* line) {
	// who|chem|above|level|on (names cannot hold '|')
	std::string l = line;
	std::vector<std::string> f;
	size_t pos = 0;
	while (true) {
		size_t bar = l.find('|', pos);
		f.push_back(l.substr(pos, bar - pos));
		if (bar == std::string::npos)
			break;
		pos = bar + 1;
	}
	if (f.size() < 5)
		return;
	Alarm a;
	a.who = f[0].empty() ? "*" : f[0];
	a.chem = std::clamp(std::atoi(f[1].c_str()), 1, 255);
	a.above = std::atoi(f[2].c_str()) != 0;
	a.level = std::clamp(std::atoi(f[3].c_str()), 0, 255);
	a.on = std::atoi(f[4].c_str()) != 0;
	s_alarms.push_back(a);
}

void KitAlarmsWrite(ImGuiTextBuffer* buf) {
	for (auto& a : s_alarms)
		buf->appendf("Alarm=%s|%d|%d|%d|%d\n", a.who.c_str(), a.chem, a.above ? 1 : 0, a.level, a.on ? 1 : 0);
}

static std::string chemName(int i) {
	const auto& n = KitChemicalNames();
	return (i >= 0 && i < (int)n.size()) ? n[i] : fmt::format("<{}>", i);
}

void KitAlarmsTick() {
	// developer aid: OPENC2E_TEST_ALARM="who|chem|above|level|on" adds one
	static bool testAlarm = true;
	if (testAlarm) {
		testAlarm = false;
		if (const char* t = getenv("OPENC2E_TEST_ALARM"))
			KitAlarmsReadLine(t);
	}
	if (s_alarms.empty() || world.tickcount - s_lastCheck < 10)
		return;
	s_lastCheck = world.tickcount;
	auto creatures = KitLivingCreatures();
	for (auto& al : s_alarms) {
		if (!al.on)
			continue;
		for (auto& a : creatures) {
			std::string name = KitCreatureName(a.get());
			if (al.who != "*" && al.who != name)
				continue;
			c2Creature* c = KitC2Of(a.get());
			if (!c)
				continue;
			int v = c->getChemical((unsigned char)al.chem);
			bool cond = al.above ? v > al.level : v < al.level;
			bool& tripped = al.tripped[a.get()];
			if (cond && !tripped) {
				tripped = true;
				AlarmEvent e;
				e.text = fmt::format("{}: {} {} {} ({})", name, chemName(al.chem), al.above ? "above" : "below", al.level, v);
				e.when = KitDateText(world.worldtickcount);
				e.who = AgentRef(a);
				e.shownAt = ImGui::GetTime();
				s_log.push_front(e);
				if (s_log.size() > 50)
					s_log.pop_back();
			} else if (!cond) {
				tripped = false;
			}
		}
	}
	// a note over the world for each new alarm (10 seconds)
	double now = ImGui::GetTime();
	int shown = 0;
	for (auto& e : s_log) {
		if (now - e.shownAt > 10 || shown >= 3)
			continue;
		ImGuiIO& io = ImGui::GetIO();
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 16, io.DisplaySize.y - 60 - shown * 64), ImGuiCond_Always, ImVec2(1, 1));
		ImGui::SetNextWindowBgAlpha(0.92f);
		std::string id = fmt::format("##alarm{}", (const void*)&e);
		if (ImGui::Begin(id.c_str(), nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav)) {
			ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "Alarm");
			ImGui::SameLine();
			ImGui::TextUnformatted(e.text.c_str());
			if (auto sp = e.who.lock()) {
				if (ImGui::SmallButton("Go to it"))
					KitGoTo(sp);
				ImGui::SameLine();
			}
			if (ImGui::SmallButton("Dismiss"))
				e.shownAt = -100;
		}
		ImGui::End();
		shown++;
	}
}

static void AlarmsPage() {
	ImGui::TextWrapped("An alarm goes off when a chemical in a creature passes a level, and again after it has come back. Alarms are checked while the kit is closed too.");
	const auto& groups = KitChemicalGroups();
	int remove = -1;
	if (ImGui::BeginTable("alarms", 6, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
		ImGui::TableSetupColumn("On");
		ImGui::TableSetupColumn("Creature", 0, 140);
		ImGui::TableSetupColumn("Chemical", 0, 170);
		ImGui::TableSetupColumn("", 0, 80);
		ImGui::TableSetupColumn("Level", 0, 140);
		ImGui::TableSetupColumn("");
		ImGui::TableHeadersRow();
		auto creatures = KitLivingCreatures();
		for (size_t i = 0; i < s_alarms.size(); i++) {
			auto& al = s_alarms[i];
			bool changed = false;
			ImGui::PushID((int)i);
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			changed |= ImGui::Checkbox("##on", &al.on);
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-1);
			if (ImGui::BeginCombo("##who", al.who == "*" ? "any creature" : al.who.c_str())) {
				if (ImGui::Selectable("any creature", al.who == "*"))
					al.who = "*", changed = true;
				for (auto& a : creatures) {
					std::string n = KitCreatureName(a.get());
					if (n.find('|') != std::string::npos)
						continue;
					if (ImGui::Selectable(n.c_str(), al.who == n))
						al.who = n, changed = true;
				}
				ImGui::EndCombo();
			}
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-1);
			if (ImGui::BeginCombo("##chem", fmt::format("{} {}", al.chem, chemName(al.chem)).c_str(), ImGuiComboFlags_HeightLarge)) {
				for (auto& g : groups) {
					if (g.first == "All")
						continue;
					ImGui::SeparatorText(g.first.c_str());
					for (int ch : g.second)
						if (ImGui::Selectable(fmt::format("{} {}##{}{}", ch, chemName(ch), g.first, ch).c_str(), ch == al.chem))
							al.chem = ch, changed = true;
				}
				ImGui::SeparatorText("All");
				for (int ch = 1; ch < 256; ch++)
					if (ImGui::Selectable(fmt::format("{} {}##all{}", ch, chemName(ch), ch).c_str(), ch == al.chem))
						al.chem = ch, changed = true;
				ImGui::EndCombo();
			}
			ImGui::TableNextColumn();
			int dir = al.above ? 1 : 0;
			ImGui::SetNextItemWidth(-1);
			if (ImGui::Combo("##dir", &dir, "below\0above\0"))
				al.above = dir == 1, changed = true;
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-1);
			changed |= ImGui::SliderInt("##level", &al.level, 0, 255);
			ImGui::TableNextColumn();
			if (ImGui::SmallButton("Remove"))
				remove = (int)i;
			if (changed) {
				al.tripped.clear();
				ImGui::MarkIniSettingsDirty();
			}
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	if (remove >= 0) {
		s_alarms.erase(s_alarms.begin() + remove);
		ImGui::MarkIniSettingsDirty();
	}
	if (ImGui::Button("Add an alarm")) {
		Alarm a; // any creature, ATP below 50
		s_alarms.push_back(a);
		ImGui::MarkIniSettingsDirty();
	}
	ImGui::SeparatorText("Gone off");
	if (s_log.empty())
		ImGui::TextDisabled("Nothing yet.");
	for (size_t i = 0; i < s_log.size() && i < 20; i++) {
		auto& e = s_log[i];
		ImGui::PushID((int)i);
		ImGui::TextDisabled("%s", e.when.c_str());
		ImGui::SameLine();
		ImGui::TextUnformatted(e.text.c_str());
		if (auto sp = e.who.lock()) {
			ImGui::SameLine();
			if (ImGui::SmallButton("Go"))
				KitGoTo(sp);
		}
		ImGui::PopID();
	}
}

void DrawObservationKit() {
	if (ImGui::BeginTabBar("pages")) {
		if (KitTab("Details")) {
			DetailsPage();
			ImGui::EndTabItem();
		}
		if (KitTab("Alarms")) {
			AlarmsPage();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

} // namespace Openc2eImgui
