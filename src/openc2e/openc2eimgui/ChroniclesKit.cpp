// The Owner's Kit Life Story page and the Chronicles Kit (openc2e additions,
// owner 2026-11-11). The Life Story tells one creature's own life in dated
// lines (historyManager.h, storyEntry). The Chronicles Kit, after C2's own
// kit of that name, keeps the whole world's history of births, deaths and
// pregnancies (moved here from the Owner's Kit) and statistics over every
// creature the world has had.

#include "Agent.h"
#include "C2Kits.h"
#include "C2KitsInternal.h"
#include "World.h"
#include "creatures/CreatureAgent.h"
#include "creatures/CreatureSpeech.h"
#include "creatures/oldCreature.h"
#include "historyManager.h"

#include <algorithm>
#include <fmt/format.h>
#include <functional>
#include <imgui.h>
#include <map>

namespace Openc2eImgui {

namespace {

const char* stageNames[7] = {"baby", "child", "adolescent", "youth", "adult", "old", "senile"};

// a creature's name, living or not
std::string nameOf(const std::string& mon) {
	if (mon.empty() || !world.history->hasMoniker(mon))
		return "a creature";
	monikerData& m = world.history->getMoniker(mon);
	if (m.owner)
		return KitCreatureName(m.owner.get());
	if (!m.name.empty())
		return m.name;
	return fmt::format("{} {}", m.genus == 2 ? "grendel" : m.genus == 3 ? "ettin" : "norn", mon);
}

std::string sentence(const monikerData& self, const storyEntry& e) {
	std::string other = nameOf(e.other);
	switch (e.kind) {
		case storyEntry::born: {
			std::string s = "Hatched";
			if (!self.mother.empty() && !self.father.empty())
				s += fmt::format(", the child of {} and {}", nameOf(self.mother), nameOf(self.father));
			else if (!self.mother.empty() || !self.father.empty())
				s += fmt::format(", the child of {}", nameOf(self.mother.empty() ? self.father : self.mother));
			return s + ".";
		}
		case storyEntry::stage: {
			int st = std::clamp(atoi(e.detail.c_str()), 0, 6);
			bool vowel = std::string("aeiou").find(stageNames[st][0]) != std::string::npos;
			return fmt::format("Became {} {}.", vowel ? "an" : "a", stageNames[st]);
		}
		case storyEntry::pregnant:
			return fmt::format("Became pregnant by {}.", other);
		case storyEntry::fathered:
			return fmt::format("Fathered a baby with {}.", other);
		case storyEntry::laidEgg:
			return "Laid her egg.";
		case storyEntry::childHatched:
			return fmt::format("Their child {} hatched.", other);
		case storyEntry::firstWord:
			return fmt::format("Learned a first word: \"{}\".", e.detail);
		case storyEntry::words:
			return fmt::format("Knew {} words.", e.detail);
		case storyEntry::allWords:
			return "Learned every word at once (the Stone of Knowledge).";
		case storyEntry::befriended:
			return fmt::format("Became friends with {}.", other);
		case storyEntry::enemy:
			return fmt::format("Came to dislike {}.", other);
		case storyEntry::fears:
			return fmt::format("Came to fear {}.", other);
		case storyEntry::photo:
			return "Was photographed.";
		case storyEntry::died:
			return e.detail.empty() ? "Died." : fmt::format("Died of {}.", e.detail);
		case storyEntry::unconscious:
			return "Fell unconscious (out of energy).";
		case storyEntry::cameRound:
			return "Came round.";
	}
	return "";
}

oldCreature* oldOf(Agent* a) {
	auto* ca = dynamic_cast<CreatureAgent*>(a);
	return ca ? dynamic_cast<oldCreature*>(ca->getCreature()) : nullptr;
}

} // namespace

void LifeStoryPage() {
	if (!KitNeedsCreature())
		return;
	Agent* a = KitCreatureAgent();
	monikerData* m = KitRecordsOf(a);
	if (!m)
		return;
	std::string name = KitCreatureName(a);
	ImGui::Text("The life of %s", name.c_str());
	ImGui::Separator();

	// the lines, oldest first; a hatching from before stories were kept is
	// told from the records
	std::vector<storyEntry> lines = m->story;
	bool hasBorn = std::any_of(lines.begin(), lines.end(), [](const storyEntry& e) { return e.kind == storyEntry::born; });
	if (!hasBorn) {
		storyEntry b;
		b.tick = m->borntick;
		b.kind = storyEntry::born;
		lines.insert(lines.begin(), b);
	}
	std::stable_sort(lines.begin(), lines.end(), [](const storyEntry& x, const storyEntry& y) { return x.tick < y.tick; });
	if (ImGui::BeginTable("story", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit, ImVec2(620, 300))) {
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("When", 0, 200);
		ImGui::TableSetupColumn("What happened", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();
		for (size_t i = 0; i < lines.size(); i++) {
			const storyEntry& e = lines[i];
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextDisabled("%s", KitDateText(e.tick).c_str());
			ImGui::TableNextColumn();
			std::string s = sentence(*m, e);
			if (e.kind == storyEntry::born && !hasBorn && !m->bornknown)
				s = "First seen in this world.";
			ImGui::TextWrapped("%s", s.c_str());
		}
		ImGui::EndTable();
	}
	KitNote("Stories are kept from 11 November 2026 on; before that only hatching and family were recorded.");
}

namespace {

void StatisticsPage() {
	auto& all = world.history->all();
	unsigned int now = world.worldtickcount;
	// counts by kind
	struct Count {
		int living = 0, died = 0, hatched = 0;
	};
	std::map<int, Count> counts;
	std::map<std::string, int> children;
	for (auto& [mon, m] : all) {
		if (m.genus < 1 || m.genus > 3)
			continue;
		Count& c = counts[m.genus];
		if (m.died)
			c.died++;
		else if (m.owner)
			c.living++;
		if (m.bornknown)
			c.hatched++;
		for (const std::string* p : {&m.mother, &m.father})
			if (!p->empty())
				children[*p]++;
	}
	ImGui::TextUnformatted("Every creature this world has had");
	if (ImGui::BeginTable("counts", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingFixedFit)) {
		ImGui::TableSetupColumn("", 0, 120);
		ImGui::TableSetupColumn("Living", 0, 80);
		ImGui::TableSetupColumn("Died", 0, 80);
		ImGui::TableSetupColumn("Hatched here", 0, 110);
		ImGui::TableHeadersRow();
		const char* kinds[4] = {"", "Norns", "Grendels", "Ettins"};
		for (int g = 1; g <= 3; g++) {
			Count& c = counts[g];
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(kinds[g]);
			ImGui::TableNextColumn();
			ImGui::Text("%d", c.living);
			ImGui::TableNextColumn();
			ImGui::Text("%d", c.died);
			ImGui::TableNextColumn();
			ImGui::Text("%d", c.hatched);
		}
		ImGui::EndTable();
	}
	ImGui::Spacing();

	// records: the top three of each
	std::map<std::string, int> depthMemo;
	std::function<int(const std::string&, int)> depth = [&](const std::string& mon, int guard) -> int {
		if (mon.empty() || guard > 30 || !world.history->hasMoniker(mon))
			return 0;
		if (auto it = depthMemo.find(mon); it != depthMemo.end())
			return it->second;
		monikerData& m = world.history->getMoniker(mon);
		int d = 1 + std::max(depth(m.mother, guard + 1), depth(m.father, guard + 1));
		return depthMemo[mon] = d;
	};
	struct Rec {
		std::string title;
		std::vector<std::pair<double, std::string>> rows; // value, text
	};
	std::vector<Rec> recs(6);
	recs[0].title = "Longest lives";
	recs[1].title = "Oldest living";
	recs[2].title = "Most children";
	recs[3].title = "Most generations back";
	recs[4].title = "Most words known";
	recs[5].title = "Most friends";
	for (auto& [mon, m] : all) {
		if (m.genus < 1 || m.genus > 3)
			continue;
		std::string who = nameOf(mon);
		if (m.died && m.bornknown && m.diedtick > m.borntick)
			recs[0].rows.push_back({(double)(m.diedtick - m.borntick), fmt::format("{}: {}", who, KitAgeText(m.diedtick - m.borntick))});
		if (!m.died && m.owner && now > m.borntick)
			recs[1].rows.push_back({(double)(now - m.borntick), fmt::format("{}: {}{}", who, KitAgeText(now - m.borntick), m.bornknown ? "" : " (or more)")});
		if (children[mon] > 0)
			recs[2].rows.push_back({(double)children[mon], fmt::format("{}: {}", who, children[mon])});
		int d = depth(mon, 0) - 1;
		if (d > 0)
			recs[3].rows.push_back({(double)d, fmt::format("{}: {} generation{}", who, d, d == 1 ? "" : "s")});
		if (oldCreature* c = m.owner ? oldOf(m.owner.get()) : nullptr) {
			int w = wordsKnown(c);
			if (w > 0)
				recs[4].rows.push_back({(double)w, fmt::format("{}: {}", who, w)});
			int f = 0;
			for (auto& [o, b] : c->bonds)
				f += b.friend_ ? 1 : 0;
			if (f > 0)
				recs[5].rows.push_back({(double)f, fmt::format("{}: {}", who, f)});
		}
	}
	if (ImGui::BeginTable("records", 2, ImGuiTableFlags_SizingStretchSame)) {
		for (size_t i = 0; i < recs.size(); i++) {
			if (i % 2 == 0)
				ImGui::TableNextRow();
			ImGui::TableNextColumn();
			auto& r = recs[i];
			std::sort(r.rows.begin(), r.rows.end(), [](auto& x, auto& y) { return x.first > y.first; });
			ImGui::TextUnformatted(r.title.c_str());
			if (r.rows.empty())
				ImGui::TextDisabled("  none yet");
			for (size_t k = 0; k < r.rows.size() && k < 3; k++)
				ImGui::Text("  %zu. %s", k + 1, r.rows[k].second.c_str());
			ImGui::Spacing();
		}
		ImGui::EndTable();
	}
}

} // namespace

void DrawChroniclesKit() {
	if (ImGui::BeginTabBar("pages")) {
		if (KitTab("History")) {
			WorldHistoryPage();
			ImGui::EndTabItem();
		}
		if (KitTab("Statistics")) {
			StatisticsPage();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

} // namespace Openc2eImgui
