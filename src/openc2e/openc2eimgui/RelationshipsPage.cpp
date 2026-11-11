// The Owner's Kit Relationships page (openc2e addition, owner 2026-11-11):
// what the selected creature feels about every other living creature, and
// what each feels back (creatures/Relationships.h). A map with the creature
// in the middle (the better it likes someone, the nearer they sit; the
// unmet wait on the rim) and a list.

#include "Agent.h"
#include "C2Kits.h"
#include "C2KitsInternal.h"
#include "World.h"
#include "creatures/Breeding.h"
#include "creatures/CreatureAgent.h"
#include "creatures/Relationships.h"
#include "creatures/oldCreature.h"
#include "historyManager.h"

#include <algorithm>
#include <cmath>
#include <fmt/format.h>
#include <imgui.h>

namespace Openc2eImgui {

namespace {

oldCreature* oldOf(Agent* a) {
	CreatureAgent* ca = dynamic_cast<CreatureAgent*>(a);
	return ca ? dynamic_cast<oldCreature*>(ca->getCreature()) : nullptr;
}

// blue for liking, orange for disliking (readable for red-green colour
// blindness), grey for neither; stronger feeling, stronger colour
ImU32 feelingColour(const Bond* b, float alpha = 1.0f) {
	if (!b)
		return IM_COL32(150, 150, 150, (int)(110 * alpha));
	float a = std::clamp(b->affinity, -1.0f, 1.0f);
	float s = std::min(1.0f, std::fabs(a) * 1.4f + 0.15f);
	ImVec4 grey(0.6f, 0.6f, 0.6f, alpha), like(0.25f, 0.55f, 0.95f, alpha), dislike(0.95f, 0.55f, 0.15f, alpha);
	ImVec4 to = a >= 0 ? like : dislike;
	ImVec4 c(grey.x + (to.x - grey.x) * s, grey.y + (to.y - grey.y) * s, grey.z + (to.z - grey.z) * s, alpha);
	return ImGui::ColorConvertFloat4ToU32(c);
}

std::string tags(const Bond* mine, const Bond* theirs, bool kin) {
	std::string t;
	auto add = [&](const std::string& s) { t += (t.empty() ? "" : ", ") + s; };
	if (mine && mine->mate)
		add("mate");
	if (kin)
		add("kin");
	if (mine) {
		std::string l = Relationships::label(*mine);
		if (!l.empty() && l != "mate")
			add(l);
	}
	if (theirs && theirs->fear > 0.5f)
		add("fears us");
	return t;
}

std::string eventsText(const Bond* b) {
	if (!b || b->events.empty())
		return "  nothing yet";
	std::vector<std::pair<int, std::string>> ev;
	for (auto& [what, n] : b->events)
		ev.push_back({n, what});
	std::sort(ev.rbegin(), ev.rend());
	std::string s;
	for (auto& [n, what] : ev)
		s += fmt::format("  {} x{}\n", what, n);
	return s;
}

} // namespace

void RelationshipsPage() {
	if (!KitNeedsCreature("Applet Data/Owner.bmp"))
		return;
	Agent* self = KitCreatureAgent();
	oldCreature* me = oldOf(self);
	if (!me)
		return;
	std::string myMon = world.history->findMoniker(me->getGenome());
	std::string myName = KitCreatureName(self);

	struct Row {
		std::shared_ptr<Agent> agent;
		std::string mon, name;
		const Bond* mine; // what this creature feels for them
		const Bond* theirs; // what they feel for it
		bool kin;
	};
	std::vector<Row> rows;
	for (auto& a : KitLivingCreatures()) {
		if (a.get() == self)
			continue;
		oldCreature* c = oldOf(a.get());
		if (!c)
			continue;
		std::string mon = world.history->findMoniker(c->getGenome());
		rows.push_back({a, mon, KitCreatureName(a.get()), Relationships::bondOf(me, mon), Relationships::bondOf(c, myMon), closeKin(myMon, mon)});
	}
	std::sort(rows.begin(), rows.end(), [](const Row& x, const Row& y) {
		float ax = x.mine ? x.mine->affinity : -2.0f, ay = y.mine ? y.mine->affinity : -2.0f;
		return ax != ay ? ax > ay : x.name < y.name;
	});

	if (rows.empty()) {
		ImGui::TextWrapped("%s is the only creature in the world.", myName.c_str());
		return;
	}

	// ---- the map ----
	const float size = 360, R = 150, rIn = 46, rRim = 168;
	ImVec2 origin = ImGui::GetCursorScreenPos();
	ImVec2 centre(origin.x + size / 2 + 20, origin.y + size / 2 + 10);
	ImDrawList* dl = ImGui::GetWindowDrawList();
	dl->AddCircle(centre, R, IM_COL32(120, 120, 120, 60), 64, 1.0f);
	dl->AddCircle(centre, rRim, IM_COL32(120, 120, 120, 40), 64, 1.0f);
	int hovered = -1;
	const float step = 2.0f * 3.14159265f / (float)rows.size();
	std::vector<ImVec2> spots(rows.size());
	for (size_t i = 0; i < rows.size(); i++) {
		const Row& r = rows[i];
		// the better liked, the nearer; enemies out by the ring; not yet met: the rim
		float rad = r.mine ? rIn + (1.0f - std::clamp(r.mine->affinity, -1.0f, 1.0f)) / 2.0f * (R - rIn) : rRim;
		float ang = -3.14159265f / 2 + step * (float)i;
		ImVec2 p(centre.x + std::cos(ang) * rad, centre.y + std::sin(ang) * rad);
		spots[i] = p;
		// the line in two halves: the near half is how the selected creature
		// feels, the far half how they feel back; thicker for more familiar
		ImVec2 mid((centre.x + p.x) / 2, (centre.y + p.y) / 2);
		float wMine = 1.0f + 4.0f * (r.mine ? r.mine->familiarity : 0.0f);
		float wTheirs = 1.0f + 4.0f * (r.theirs ? r.theirs->familiarity : 0.0f);
		dl->AddLine(centre, mid, feelingColour(r.mine), wMine);
		dl->AddLine(mid, p, feelingColour(r.theirs), wTheirs);
	}
	for (size_t i = 0; i < rows.size(); i++) {
		const Row& r = rows[i];
		ImVec2 p = spots[i];
		bool feared = r.mine && r.mine->fear > 0.5f;
		dl->AddCircleFilled(p, 8, IM_COL32(235, 235, 225, 255));
		dl->AddCircle(p, 8, feared ? IM_COL32(170, 60, 200, 255) : feelingColour(r.mine), 16, feared ? 3.0f : 2.0f);
		if (r.mine && r.mine->mate)
			dl->AddCircle(p, 12, IM_COL32(240, 90, 150, 255), 16, 2.0f);
		std::string t = r.name;
		ImVec2 ts = ImGui::CalcTextSize(t.c_str());
		bool right = p.x >= centre.x || p.x - 12 - ts.x < origin.x + 2; // no room on the left: the right
		ImVec2 tp(right ? p.x + 12 : p.x - 12 - ts.x, p.y - ts.y / 2);
		dl->AddText(tp, IM_COL32(230, 230, 230, 255), t.c_str());
		ImVec2 mouse = ImGui::GetIO().MousePos;
		if (std::hypot(mouse.x - p.x, mouse.y - p.y) < 11)
			hovered = (int)i;
	}
	dl->AddCircleFilled(centre, 13, IM_COL32(250, 220, 120, 255));
	ImVec2 ns = ImGui::CalcTextSize(myName.c_str());
	dl->AddText(ImVec2(centre.x - ns.x / 2, centre.y + 16), IM_COL32(255, 230, 150, 255), myName.c_str());
	ImGui::InvisibleButton("relmap", ImVec2(size + 40, size + 20));
	if (hovered >= 0) {
		const Row& r = rows[hovered];
		ImGui::BeginTooltip();
		ImGui::Text("%s and %s%s", myName.c_str(), r.name.c_str(), r.kin ? " (close kin)" : "");
		ImGui::Separator();
		auto side = [&](const char* who, const Bond* b) {
			if (!b) {
				ImGui::Text("%s: has not met them", who);
				return;
			}
			ImGui::Text("%s: liking %+.2f, recently %+.2f, familiar %.0f%%, fear %.0f%% %s", who, b->affinity, b->recent, b->familiarity * 100,
				b->fear * 100, Relationships::label(*b).c_str());
			ImGui::TextUnformatted(eventsText(b).c_str());
		};
		side(fmt::format("{} feels", myName).c_str(), r.mine);
		side(fmt::format("{} feels", r.name).c_str(), r.theirs);
		ImGui::TextDisabled("Click to select %s", r.name.c_str());
		ImGui::EndTooltip();
		if (ImGui::IsMouseClicked(0))
			KitGoTo(r.agent);
	}
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
	ImGui::TextWrapped("Nearer: better liked; on the rim: not met. Line: near half %s's feeling, far half theirs; blue liking, orange dislike, thicker more familiar. Pink ring: mate; purple: feared.",
		myName.c_str());
	ImGui::PopStyleColor();

	// ---- the list ----
	if (ImGui::BeginTable("rels", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp)) {
		ImGui::TableSetupColumn("Creature");
		ImGui::TableSetupColumn(fmt::format("{} feels", myName).c_str());
		ImGui::TableSetupColumn("They feel");
		ImGui::TableSetupColumn("Familiar");
		ImGui::TableSetupColumn("Fear");
		ImGui::TableSetupColumn("Status");
		ImGui::TableHeadersRow();
		for (auto& r : rows) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			if (ImGui::Selectable(fmt::format("{}##{}", r.name, r.mon).c_str(), false, ImGuiSelectableFlags_SpanAllColumns))
				KitGoTo(r.agent);
			ImGui::TableNextColumn();
			ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(feelingColour(r.mine)), "%s", r.mine ? fmt::format("{:+.2f}", r.mine->affinity).c_str() : "-");
			ImGui::TableNextColumn();
			ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(feelingColour(r.theirs)), "%s", r.theirs ? fmt::format("{:+.2f}", r.theirs->affinity).c_str() : "-");
			ImGui::TableNextColumn();
			ImGui::Text("%s", r.mine ? fmt::format("{:.0f}%", r.mine->familiarity * 100).c_str() : "-");
			ImGui::TableNextColumn();
			ImGui::Text("%s", r.mine && r.mine->fear > 0.05f ? fmt::format("{:.0f}%", r.mine->fear * 100).c_str() : "");
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(tags(r.mine, r.theirs, r.kin).c_str());
		}
		ImGui::EndTable();
	}
}

} // namespace Openc2eImgui
