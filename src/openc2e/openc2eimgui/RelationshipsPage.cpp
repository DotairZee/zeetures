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
#include "common/UiPinch.h"

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

// blue for liking, orange for disliking (readable for red-green color
// blindness), gray for neither; stronger feeling, stronger color
ImU32 feelingColor(const Bond* b, float alpha = 1.0f) {
	if (!b)
		return IM_COL32(150, 150, 150, (int)(110 * alpha));
	float a = std::clamp(b->affinity, -1.0f, 1.0f);
	float s = std::min(1.0f, std::fabs(a) * 1.4f + 0.15f);
	ImVec4 gray(0.6f, 0.6f, 0.6f, alpha), like(0.25f, 0.55f, 0.95f, alpha), dislike(0.95f, 0.55f, 0.15f, alpha);
	ImVec4 to = a >= 0 ? like : dislike;
	ImVec4 c(gray.x + (to.x - gray.x) * s, gray.y + (to.y - gray.y) * s, gray.z + (to.z - gray.z) * s, alpha);
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
	if (!KitNeedsCreature())
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
	// It zooms (mouse wheel, or pinch on a touchpad) about the pointer and
	// is dragged to move about (owner, 2026-11-16: with 150 creatures it
	// needs zooming in); a click without a drag still selects a creature.
	// The view is kept while the kit is open, for every creature.
	static float zoom = 1.0f;
	static ImVec2 pan(0, 0);
	// developer aid: OPENC2E_TEST_RELMAP="zoom,dx,dy" starts the view zoomed
	// and moved, for a snapshot
	static bool testView = [] {
		if (const char* t = getenv("OPENC2E_TEST_RELMAP"))
			sscanf(t, "%f,%f,%f", &zoom, &pan.x, &pan.y);
		return true;
	}();
	(void)testView;
	const float size = 360, R = 150, rIn = 46, rRim = 168;
	const float left = ImGui::GetCursorPosX(); // the words below start at the left
	KitCenterNext(size + 40); // the map in the middle of the window
	ImGui::BeginChild("relmapview", ImVec2(size + 40, size + 20), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImVec2 origin = ImGui::GetCursorScreenPos();
	ImGui::SetNextItemAllowOverlap(); // (the Reset view button sits on it)
	ImGui::InvisibleButton("relmap", ImVec2(size + 40, size + 20));
	const bool overMap = ImGui::IsItemHovered();
	const ImGuiIO& io = ImGui::GetIO();
	ImVec2 base(origin.x + size / 2 + 20, origin.y + size / 2 + 10); // the centre at zoom 1, no pan
	// zoom about the pointer: wheel steps of 15%, pinches as they come
	float pinch = UiPinch::Take();
	float zf = 1.0f;
	if (overMap) {
		if (io.MouseWheel != 0)
			zf *= std::pow(1.15f, io.MouseWheel);
		zf *= pinch;
	}
	if (zf != 1.0f) {
		float nz = std::clamp(zoom * zf, 1.0f, 8.0f);
		ImVec2 m = io.MousePos;
		// the map point under the pointer stays under it
		ImVec2 mp((m.x - base.x - pan.x) / zoom, (m.y - base.y - pan.y) / zoom);
		zoom = nz;
		pan = ImVec2(m.x - base.x - mp.x * zoom, m.y - base.y - mp.y * zoom);
	}
	// drag to move about (a press on the map that moves more than a few px)
	static bool dragging = false;
	if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0, 4.0f)) {
		dragging = true;
		pan.x += io.MouseDelta.x;
		pan.y += io.MouseDelta.y;
	}
	bool clicked = ImGui::IsItemDeactivated() && !dragging;
	if (!ImGui::IsItemActive())
		dragging = dragging && ImGui::IsMouseDown(0);
	if (zoom <= 1.0001f)
		pan = ImVec2(0, 0); // (at full view, nothing to move)
	auto at = [&](ImVec2 p) { return ImVec2(base.x + pan.x + (p.x - base.x) * zoom, base.y + pan.y + (p.y - base.y) * zoom); };
	const ImVec2 center = at(base);
	const float dot = 8.0f * std::min(zoom, 2.0f);
	ImDrawList* dl = ImGui::GetWindowDrawList();
	dl->AddCircle(center, R * zoom, IM_COL32(120, 120, 120, 60), 64, 1.0f);
	dl->AddCircle(center, rRim * zoom, IM_COL32(120, 120, 120, 40), 64, 1.0f);
	int hovered = -1;
	const float step = 2.0f * 3.14159265f / (float)rows.size();
	std::vector<ImVec2> spots(rows.size());
	for (size_t i = 0; i < rows.size(); i++) {
		const Row& r = rows[i];
		// the better liked, the nearer; enemies out by the ring; not yet met: the rim
		float rad = r.mine ? rIn + (1.0f - std::clamp(r.mine->affinity, -1.0f, 1.0f)) / 2.0f * (R - rIn) : rRim;
		float ang = -3.14159265f / 2 + step * (float)i;
		ImVec2 p = at(ImVec2(base.x + std::cos(ang) * rad, base.y + std::sin(ang) * rad));
		spots[i] = p;
		// the line in two halves: the near half is how the selected creature
		// feels, the far half how they feel back; thicker for more familiar
		ImVec2 mid((center.x + p.x) / 2, (center.y + p.y) / 2);
		float wMine = 1.0f + 4.0f * (r.mine ? r.mine->familiarity : 0.0f);
		float wTheirs = 1.0f + 4.0f * (r.theirs ? r.theirs->familiarity : 0.0f);
		dl->AddLine(center, mid, feelingColor(r.mine), wMine);
		dl->AddLine(mid, p, feelingColor(r.theirs), wTheirs);
	}
	// names for mates, the feared and the best liked (more as the map is
	// zoomed in), each only where it doesn't cover a name already placed
	// (with 150 creatures every name overlapped into a smear: owner's
	// screenshot, 2026-11-16); pointing at any dot still names it
	std::vector<bool> named(rows.size(), false);
	{
		const ImVec2 clipMin = origin, clipMax(origin.x + size + 40, origin.y + size + 20);
		std::vector<size_t> order;
		for (size_t i = 0; i < rows.size(); i++)
			if (rows[i].mine && (rows[i].mine->mate || rows[i].mine->fear > 0.5f))
				order.push_back(i);
		int cap = (int)std::lround(8.0f * zoom * zoom), liked = 0;
		for (size_t i = 0; i < rows.size(); i++)
			if (rows.size() <= 16 || (rows[i].mine && liked++ < cap))
				order.push_back(i);
		std::vector<ImVec4> placed; // x0 y0 x1 y1
		for (size_t i : order) {
			if (named[i])
				continue;
			ImVec2 p = spots[i];
			if (p.x < clipMin.x || p.x > clipMax.x || p.y < clipMin.y || p.y > clipMax.y)
				continue;
			ImVec2 ts = ImGui::CalcTextSize(rows[i].name.c_str());
			bool right = p.x >= center.x || p.x - 12 - ts.x < origin.x + 2;
			ImVec4 box(right ? p.x + dot + 4 : p.x - dot - 4 - ts.x, p.y - ts.y / 2, 0, 0);
			box.z = box.x + ts.x;
			box.w = box.y + ts.y;
			bool clear = true;
			for (auto& b : placed)
				clear &= box.z < b.x - 2 || box.x > b.z + 2 || box.w < b.y || box.y > b.w;
			if (clear) {
				named[i] = true;
				placed.push_back(box);
			}
		}
	}
	for (size_t i = 0; i < rows.size(); i++) {
		const Row& r = rows[i];
		ImVec2 p = spots[i];
		bool feared = r.mine && r.mine->fear > 0.5f;
		dl->AddCircleFilled(p, dot, IM_COL32(235, 235, 225, 255));
		dl->AddCircle(p, dot, feared ? IM_COL32(170, 60, 200, 255) : feelingColor(r.mine), 16, feared ? 3.0f : 2.0f);
		if (r.mine && r.mine->mate)
			dl->AddCircle(p, dot + 4, IM_COL32(240, 90, 150, 255), 16, 2.0f);
		ImVec2 mouse = io.MousePos;
		if (overMap && !dragging && std::hypot(mouse.x - p.x, mouse.y - p.y) < dot + 3)
			hovered = (int)i;
	}
	// the names last, over every dot, on a dark backing so they read
	for (size_t i = 0; i < rows.size(); i++) {
		if (!named[i])
			continue;
		ImVec2 p = spots[i];
		const std::string& t = rows[i].name;
		ImVec2 ts = ImGui::CalcTextSize(t.c_str());
		bool right = p.x >= center.x || p.x - 12 - ts.x < origin.x + 2; // no room on the left: the right
		ImVec2 tp(right ? p.x + dot + 4 : p.x - dot - 4 - ts.x, p.y - ts.y / 2);
		dl->AddRectFilled(ImVec2(tp.x - 2, tp.y), ImVec2(tp.x + ts.x + 2, tp.y + ts.y), IM_COL32(20, 22, 30, 200), 3.0f);
		dl->AddText(tp, IM_COL32(230, 230, 230, 255), t.c_str());
	}
	dl->AddCircleFilled(center, 13 * std::min(zoom, 2.0f), IM_COL32(250, 220, 120, 255));
	ImVec2 ns = ImGui::CalcTextSize(myName.c_str());
	dl->AddText(ImVec2(center.x - ns.x / 2, center.y + 16 * std::min(zoom, 2.0f)), IM_COL32(255, 230, 150, 255), myName.c_str());
	// back to the whole map
	if (zoom > 1.0001f) {
		ImGui::SetCursorScreenPos(ImVec2(origin.x + 4, origin.y + 4));
		if (ImGui::SmallButton("Reset view")) {
			zoom = 1.0f;
			pan = ImVec2(0, 0);
		}
		ImGui::SameLine();
		ImGui::TextDisabled("%.1fx", zoom);
	}
	ImGui::EndChild();
	ImGui::SetCursorPosX(left);
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
		if (clicked)
			KitGoTo(r.agent);
	}
	KitNote("Nearer: better liked; on the rim: not met. Line: near half %s's feeling, far half theirs; blue liking, orange dislike, thicker more familiar. Pink ring: mate; purple: feared. Names shown for mates, the feared and the best liked; point at any other to see who it is. Wheel or pinch to zoom, drag to move about.",
		myName.c_str());

	// ---- the list ----
	// about ten rows, scrolling (owner, 2026-11-16: the page ran on for every
	// creature in the world)
	float listH = ImGui::GetFrameHeightWithSpacing() * 11;
	if (ImGui::BeginTable("rels", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY,
			ImVec2(0, listH))) {
		ImGui::TableSetupScrollFreeze(0, 1);
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
			ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(feelingColor(r.mine)), "%s", r.mine ? fmt::format("{:+.2f}", r.mine->affinity).c_str() : "-");
			ImGui::TableNextColumn();
			ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(feelingColor(r.theirs)), "%s", r.theirs ? fmt::format("{:+.2f}", r.theirs->affinity).c_str() : "-");
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
