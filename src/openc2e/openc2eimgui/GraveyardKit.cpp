// The Graveyard Kit (C2; openc2e's own layout on C2's gravestone picture from
// the player's Applet Data; owner, 2026-11-11). After C2's Creature
// Graveyard: a gravestone for each creature that has died, with a photo the
// owner chooses from its album set into the stone, its name, when it hatched
// and died, how old it was, what it died of, and a few words the owner
// writes. Step through the dead, or pick one by name; a death notice on the
// status bar opens its stone. Epitaphs and photo choices are kept with the
// world (records of the dead).

#include "C2Kits.h"
#include "common/devenv.h"
#include "C2KitsInternal.h"
#include "Engine.h"
#include "World.h"
#include "historyManager.h"

#include <algorithm>
#include <cstring>
#include <fmt/format.h>
#include <imgui.h>

namespace Openc2eImgui {

namespace {

std::string s_selected; // the moniker whose stone is shown
char s_epitaph[160] = "";
std::string s_epitaphFor; // whose epitaph s_epitaph holds

std::vector<std::string> theDead() {
	std::vector<std::pair<unsigned int, std::string>> d;
	for (auto& [mon, m] : world.history->all())
		if (m.died && m.genus >= 1 && m.genus <= 3)
			d.push_back({m.diedtick, mon});
	std::sort(d.rbegin(), d.rend()); // newest first
	std::vector<std::string> out;
	for (auto& [t, mon] : d)
		out.push_back(mon);
	return out;
}

std::string nameOf(const monikerData& m, const std::string& mon) {
	if (!m.name.empty())
		return m.name;
	return fmt::format("{} {}", m.genus == 2 ? "Grendel" : m.genus == 3 ? "Ettin" : "Norn", mon);
}

std::string causeOf(const std::string& mon) {
	auto& log = world.history->lifelog;
	for (auto it = log.rbegin(); it != log.rend(); ++it)
		if (it->kind == lifeLogEntry::death && it->moniker == mon) {
			if (!it->cause.empty())
				return it->cause;
			if (it->oldage)
				return "old age";
		}
	return "";
}

} // namespace

void OpenGraveyardAt(const std::string& moniker) {
	s_selected = moniker;
	OpenC2Kit("graveyard");
}

static void graveyardNoticeSwitches() {
}

void DrawGraveyardKit() {
	// developer aid: OPENC2E_GRAVE=<moniker> opens at that creature's stone
	static bool s_test = true;
	if (s_test) {
		s_test = false;
		if (const char* g = devenv("OPENC2E_GRAVE"))
			s_selected = g;
	}
	auto dead = theDead();
	if (dead.empty()) {
		ImGui::Dummy(ImVec2(360, 20));
		KitNote("No creature has died in this world yet.");
		ImGui::Dummy(ImVec2(360, 20));
		graveyardNoticeSwitches();
		return;
	}
	int idx = 0;
	for (size_t i = 0; i < dead.size(); i++)
		if (dead[i] == s_selected)
			idx = (int)i;
	s_selected = dead[idx];
	monikerData& m = world.history->getMoniker(s_selected);

	// stepping through the stones, or picking one by name
	const float W = 480.0f;
	if (ImGui::ArrowButton("##prev", ImGuiDir_Left) && idx > 0)
		s_selected = dead[--idx];
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Newer");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(W - 2 * ImGui::GetFrameHeight() - 2 * ImGui::GetStyle().ItemSpacing.x);
	std::string label = fmt::format("{}  ({} of {})", nameOf(m, s_selected), idx + 1, dead.size());
	if (ImGui::BeginCombo("##who", label.c_str())) {
		for (size_t i = 0; i < dead.size(); i++) {
			auto& d = world.history->getMoniker(dead[i]);
			std::string cause = causeOf(dead[i]);
			std::string line = fmt::format("{}{}##{}", nameOf(d, dead[i]), cause.empty() ? "" : " (" + cause + ")", dead[i]);
			if (ImGui::Selectable(line.c_str(), (int)i == idx))
				s_selected = dead[i];
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	if (ImGui::ArrowButton("##next", ImGuiDir_Right) && idx + 1 < (int)dead.size())
		s_selected = dead[++idx];
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Older");
	monikerData& r = world.history->getMoniker(s_selected);

	// the memorial card: its photo, and what is remembered of it
	std::string photo = r.gravephoto;
	if (photo.empty() && !r.photos.empty())
		photo = r.photos.back();
	KitBeginCard("memorial", ImVec2(W, 0));
	ImDrawList* dl = ImGui::GetWindowDrawList();
	const float pw = 160, ph = 120;
	ImVec2 p0 = ImGui::GetCursorScreenPos();
	Texture t = photo.empty() ? Texture() : KitPhotoTexture(engine.currentworlddir / photo);
	if (t)
		dl->AddImage(t.as<ImTextureID>(), p0, p0 + ImVec2(pw, ph));
	else {
		dl->AddRectFilled(p0, p0 + ImVec2(pw, ph), IM_COL32(18, 18, 20, 255));
		const char* none = "no photo";
		ImVec2 ns = ImGui::CalcTextSize(none);
		dl->AddText(p0 + ImVec2((pw - ns.x) / 2, (ph - ns.y) / 2), IM_COL32(110, 110, 110, 255), none);
	}
	dl->AddRect(p0 - ImVec2(1, 1), p0 + ImVec2(pw + 1, ph + 1), IM_COL32(205, 170, 95, 255));
	ImGui::Dummy(ImVec2(pw, ph));
	ImGui::SameLine(0, 16);
	ImGui::BeginGroup();
	ImGui::TextColored(ImVec4(1.0f, 0.88f, 0.55f, 1), "%s", nameOf(r, s_selected).c_str());
	const ImVec4 dim = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
	ImGui::TextColored(dim, "%s", r.bornknown ? "Hatched" : "First seen");
	ImGui::SameLine(90);
	ImGui::TextUnformatted(KitDateText(r.borntick).c_str());
	ImGui::TextColored(dim, "Died");
	ImGui::SameLine(90);
	ImGui::TextUnformatted(KitDateText(r.diedtick).c_str());
	if (r.diedtick > r.borntick) {
		std::string age = KitAgeText(r.diedtick - r.borntick);
		age = age.substr(0, age.find(" ("));
		ImGui::TextColored(dim, "Aged");
		ImGui::SameLine(90);
		ImGui::Text("%s%s", r.bornknown ? "" : "at least ", age.c_str());
	}
	std::string cause = causeOf(s_selected);
	if (!cause.empty()) {
		ImGui::TextColored(dim, "Cause");
		ImGui::SameLine(90);
		ImGui::TextColored(ImVec4(0.95f, 0.6f, 0.55f, 1), "%s", cause.c_str());
	}
	ImGui::EndGroup();
	if (!r.epitaph.empty()) {
		ImGui::Spacing();
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + W - 24);
		ImGui::TextColored(ImVec4(0.86f, 0.75f, 0.55f, 1), "\xe2\x80\x9c%s\xe2\x80\x9d", r.epitaph.c_str());
		ImGui::PopTextWrapPos();
	}
	KitEndCard();
	ImGui::Spacing();

	// the owner's words and photo
	if (s_epitaphFor != s_selected) {
		s_epitaphFor = s_selected;
		std::snprintf(s_epitaph, sizeof(s_epitaph), "%s", r.epitaph.c_str());
	}
	ImGui::SetNextItemWidth(W - 70);
	if (ImGui::InputTextWithHint("Epitaph", "a few words to remember it by", s_epitaph, sizeof(s_epitaph)))
		r.epitaph = s_epitaph;
	if (r.photos.empty()) {
		ImGui::TextDisabled("No photos of %s were taken.", nameOf(r, s_selected).c_str());
	} else {
		int cur = (int)r.photos.size() - 1;
		for (size_t i = 0; i < r.photos.size(); i++)
			if (r.photos[i] == photo)
				cur = (int)i;
		ImGui::SetNextItemWidth(W - 70);
		std::string plabel = fmt::format("Photo {} of {}", cur + 1, r.photos.size());
		if (ImGui::BeginCombo("Photo", plabel.c_str())) {
			for (size_t i = 0; i < r.photos.size(); i++)
				if (ImGui::Selectable(fmt::format("Photo {}##ph{}", i + 1, i).c_str(), (int)i == cur))
					r.gravephoto = r.photos[i];
			ImGui::EndCombo();
		}
	}
	graveyardNoticeSwitches();
}

} // namespace Openc2eImgui
