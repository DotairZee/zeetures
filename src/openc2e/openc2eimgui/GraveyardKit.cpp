// The Graveyard Kit (C2; openc2e's own layout on C2's gravestone picture from
// the player's Applet Data; owner, 2026-11-11). After C2's Creature
// Graveyard: a gravestone for each creature that has died, with a photo the
// owner chooses from its album set into the stone, its name, when it hatched
// and died, how old it was, what it died of, and a few words the owner
// writes. Step through the dead, or pick one by name; a death notice on the
// status bar opens its stone. Epitaphs and photo choices are kept with the
// world (records of the dead).

#include "C2Kits.h"
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

// text centred at y, engraved: a light edge below, dark letters
void engraved(ImDrawList* dl, ImVec2 origin, float width, float y, float size, const std::string& text, ImU32 col) {
	ImFont* font = ImGui::GetFont();
	ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, width - 40, text.c_str());
	ImVec2 p(origin.x + (width - ts.x) / 2, origin.y + y);
	dl->AddText(font, size, ImVec2(p.x + 1, p.y + 1), IM_COL32(168, 168, 154, 230), text.c_str(), nullptr, width - 40);
	dl->AddText(font, size, p, col, text.c_str(), nullptr, width - 40);
}

} // namespace

void OpenGraveyardAt(const std::string& moniker) {
	s_selected = moniker;
	OpenC2Kit("graveyard");
}

void DrawGraveyardKit() {
	// developer aid: OPENC2E_GRAVE=<moniker> opens at that creature's stone
	static bool s_test = true;
	if (s_test) {
		s_test = false;
		if (const char* g = getenv("OPENC2E_GRAVE"))
			s_selected = g;
	}
	auto dead = theDead();
	if (dead.empty()) {
		Texture cover = KitPicture("Applet Data/GraveyardCover.bmp");
		if (cover)
			ImGui::Image(cover.as<ImTextureID>(), ImVec2((float)cover.width, (float)cover.height));
		ImGui::TextWrapped("No creature has died in this world yet.");
		return;
	}
	int idx = 0;
	for (size_t i = 0; i < dead.size(); i++)
		if (dead[i] == s_selected)
			idx = (int)i;
	s_selected = dead[idx];
	monikerData& m = world.history->getMoniker(s_selected);

	// stepping through the stones, or picking one by name
	const float W = 360.0f;
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

	// the stone
	Texture stone = KitPicture("Applet Data/grave.bmp");
	ImVec2 o = ImGui::GetCursorScreenPos();
	const float H = stone ? W * stone.height / stone.width : 450.0f;
	ImDrawList* dl = ImGui::GetWindowDrawList();
	if (stone)
		dl->AddImage(stone.as<ImTextureID>(), o, ImVec2(o.x + W, o.y + H));
	else
		dl->AddRectFilled(o, ImVec2(o.x + W, o.y + H), IM_COL32(96, 98, 90, 255), 40.0f);
	ImGui::Dummy(ImVec2(W, H));

	// the photo, set into the stone over the carved emblem
	std::string photo = r.gravephoto;
	if (photo.empty() && !r.photos.empty())
		photo = r.photos.back();
	float y = H * 0.24f;
	if (!photo.empty()) {
		Texture t = KitPhotoTexture(engine.currentworlddir / photo);
		if (t) {
			const float pw = 120, ph = 100;
			ImVec2 p0(o.x + (W - pw) / 2, o.y + y);
			// cropped to the frame's shape from the middle of the photo
			float aspect = (float)t.width / t.height, frame = pw / ph;
			ImVec2 uv0(0, 0), uv1(1, 1);
			if (aspect > frame) {
				float k = frame / aspect;
				uv0.x = (1 - k) / 2;
				uv1.x = 1 - uv0.x;
			} else {
				float k = aspect / frame;
				uv0.y = (1 - k) / 2;
				uv1.y = 1 - uv0.y;
			}
			dl->AddRectFilled(ImVec2(p0.x - 6, p0.y - 6), ImVec2(p0.x + pw + 6, p0.y + ph + 6), IM_COL32(70, 72, 64, 255), ph / 2 + 6);
			dl->AddImageRounded(t.as<ImTextureID>(), p0, ImVec2(p0.x + pw, p0.y + ph), uv0, uv1, IM_COL32_WHITE, ph / 2);
		}
	}

	// the words on it
	ImU32 ink = IM_COL32(30, 32, 27, 255);
	float ty = H * 0.50f;
	engraved(dl, o, W, ty, 26, nameOf(r, s_selected), ink);
	ty += 36;
	engraved(dl, o, W, ty, 14, std::string(r.bornknown ? "Hatched " : "First seen ") + KitDateText(r.borntick), ink);
	ty += 19;
	engraved(dl, o, W, ty, 14, "Died " + KitDateText(r.diedtick), ink);
	ty += 19;
	if (r.diedtick > r.borntick) {
		std::string age = KitAgeText(r.diedtick - r.borntick);
		age = age.substr(0, age.find(" ("));
		engraved(dl, o, W, ty, 14, std::string(r.bornknown ? "aged " : "aged at least ") + age, ink);
		ty += 24;
	}
	std::string cause = causeOf(s_selected);
	if (!cause.empty()) {
		engraved(dl, o, W, ty, 16, "Died of " + cause, ink);
		ty += 30;
	}
	if (!r.epitaph.empty())
		engraved(dl, o, W, ty, 15, "“" + r.epitaph + "”", IM_COL32(22, 24, 20, 255));

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
}

} // namespace Openc2eImgui
