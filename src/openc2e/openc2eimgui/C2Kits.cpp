#include "C2Kits.h"

#include "C2KitsInternal.h"
#include "ChemicalNamesManager.h"
#include "Engine.h"
#include "PathResolver.h"
#include "World.h"
#include "common/backend/Backend.h"
#include "creatures/CreatureAgent.h"
#include "creatures/oldCreature.h"
#include "fileformats/ImageUtils.h"

#include <cstring>
#include <fmt/format.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <map>

using namespace ImGuiUtils;

namespace Openc2eImgui {

// ---- the kits ---------------------------------------------------------------

struct KitDef {
	const char* id;
	const char* title; // window title
	const char* chip; // label on the status bar when minimized
	int icon; // picture in the game's applet toolbar strip (22 x 21 each)
	void (*draw)();
};

static const KitDef s_kits[] = {
	{"health", "Health Kit", "Health", 3, DrawHealthKit},
	{"science", "Science Kit", "Science", 4, DrawScienceKit},
};

struct KitState {
	bool open = false;
	bool minimized = false;
};

static std::map<std::string, KitState> s_state;
static std::string s_focus_next; // kit to bring to the front on its next draw

static const KitDef* findKit(const char* id) {
	for (auto& k : s_kits)
		if (std::strcmp(k.id, id) == 0)
			return &k;
	return nullptr;
}

bool IsC2KitAvailable(const char* id) {
	return engine.version == 2 && findKit(id) != nullptr;
}

void OpenC2Kit(const char* id) {
	if (!findKit(id))
		return;
	auto& st = s_state[id];
	st.open = true;
	st.minimized = false;
	s_focus_next = id;
	ImGui::MarkIniSettingsDirty();
}

// ---- kept between sessions -------------------------------------------------
// ImGui keeps each window's place and size in its ini file; the kits add
// whether each is open or minimized. The file is kits.ini in the user data
// folder (openc2e's own windows used no ini file before).

static void* KitsReadOpen(ImGuiContext*, ImGuiSettingsHandler*, const char* name) {
	const KitDef* k = findKit(name);
	return k ? (void*)&s_state[k->id] : nullptr;
}

static void KitsReadLine(ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line) {
	auto* st = (KitState*)entry;
	int v;
	if (sscanf(line, "Open=%d", &v) == 1)
		st->open = v != 0;
	else if (sscanf(line, "Minimized=%d", &v) == 1)
		st->minimized = v != 0;
}

static void KitsWriteAll(ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf) {
	for (auto& k : s_kits) {
		auto& st = s_state[k.id];
		buf->appendf("[%s][%s]\nOpen=%d\nMinimized=%d\n\n", handler->TypeName, k.id, st.open ? 1 : 0, st.minimized ? 1 : 0);
	}
}

// developer aid: OPENC2E_OPEN_KITS="health,science" selects the first norn
// and opens those kits (for headless snapshots; the kits' ini file is then
// neither read nor written); OPENC2E_KIT_PAGE="Drives" picks a page
static bool s_test_kits = false;

static void OpenTestKits() {
	const char* s = getenv("OPENC2E_OPEN_KITS");
	if (!s)
		return;
	s_test_kits = true;
	for (auto& a : world.agents)
		if (a && dynamic_cast<CreatureAgent*>(a.get()) && a->genus == 1) {
			world.selectedcreature = a;
			break;
		}
	std::string list = s;
	for (auto& k : s_kits)
		if (list.find(k.id) != std::string::npos) {
			OpenC2Kit(k.id);
			if (getenv("OPENC2E_KITS_MINIMIZED"))
				s_state[k.id].minimized = true;
		}
}

bool KitTab(const char* label) {
	const char* want = getenv("OPENC2E_KIT_PAGE");
	static int frames = 0;
	bool force = want && std::strcmp(want, label) == 0 && frames++ < 3;
	return ImGui::BeginTabItem(label, nullptr, force ? ImGuiTabItemFlags_SetSelected : 0);
}

static void SetUpKitSettings() {
	static bool done = false;
	if (done)
		return;
	done = true;
	OpenTestKits();
	if (s_test_kits)
		return;
	ImGuiSettingsHandler h;
	h.TypeName = "Openc2eKit";
	h.TypeHash = ImHashStr("Openc2eKit");
	h.ReadOpenFn = KitsReadOpen;
	h.ReadLineFn = KitsReadLine;
	h.WriteAllFn = KitsWriteAll;
	ImGui::AddSettingsHandler(&h);
	static std::string path;
	try {
		path = (getUserDataDir() / "kits.ini").string();
	} catch (...) {
		return;
	}
	ImGui::LoadIniSettingsFromDisk(path.c_str());
	ImGui::GetIO().IniFilename = path.c_str(); // saved every few seconds when changed, and on exit
}

// ---- shared pieces -----------------------------------------------------------

Texture KitPicture(const std::string& name) {
	static std::map<std::string, Texture> cache;
	auto it = cache.find(name);
	if (it != cache.end())
		return it->second;
	Texture t;
	auto path = findMainDirectoryFile(name);
	if (!path.empty()) {
		MultiImage images = ImageUtils::ReadImage(path);
		if (!images.empty())
			t = get_backend()->createTextureFromImage(images[0]);
	}
	cache[name] = t;
	return t;
}

const std::vector<Texture>& KitSprites(const std::string& name) {
	static std::map<std::string, std::vector<Texture>> cache;
	auto it = cache.find(name);
	if (it != cache.end())
		return it->second;
	return cache[name] = LoadImageWithTransparentTopLeft(name);
}

Agent* KitCreatureAgent() {
	return world.selectedcreature.get();
}

c2Creature* KitCreature() {
	auto* ca = dynamic_cast<CreatureAgent*>(world.selectedcreature.get());
	return ca ? dynamic_cast<c2Creature*>(ca->getCreature()) : nullptr;
}

static ChemicalNamesManager s_chemnames;

const std::vector<std::string>& KitChemicalNames() {
	return s_chemnames.getChemicalNames();
}

const std::vector<std::pair<std::string, std::vector<int>>>& KitChemicalGroups() {
	return s_chemnames.getChemicalGroups();
}

bool KitNeedsCreature(const char* cover) {
	if (KitCreature())
		return true;
	Texture t = KitPicture(cover);
	if (t) {
		ImVec2 p = ImGui::GetCursorScreenPos();
		DrawTexture(t, p);
		ImGui::Dummy(ImVec2(t.width, t.height));
	}
	ImGui::TextDisabled("Select a creature to use this page.");
	return false;
}

float KitLed(ImVec2 p, const std::string& text) {
	auto& led = KitSprites("Applet Data/led.s16");
	if (led.size() < 15)
		return 0;
	// a dark display plate, so unlit segments read as unlit
	{
		float w = 0;
		for (char ch : text)
			w += (ch == 'C' || ch == 'F' || ch == '.') ? led[ch == '.' ? 14 : 12].width + 1 : led[0].width + 1;
		ImGui::GetWindowDrawList()->AddRectFilled(p - ImVec2(3, 3), p + ImVec2(w + 2, 35), IM_COL32(4, 22, 6, 225), 3);
	}
	float x = 0;
	for (char ch : text) {
		int f;
		if (ch >= '0' && ch <= '9')
			f = ch - '0';
		else if (ch == '-')
			f = 11;
		else if (ch == 'C')
			f = 12;
		else if (ch == 'F')
			f = 13;
		else if (ch == '.')
			f = 14;
		else
			f = 10;
		// the small signs sit at the top of the digits, the point at the foot
		float dy = (f == 14) ? 32 - led[f].height : 0;
		DrawTexture(led[f], p + ImVec2(x, dy));
		x += led[f].width + 1;
	}
	return x;
}

void KitBar(float level, const char* label, float width, bool need) {
	Texture back = KitPicture("Applet Data/StatBack.bmp");
	Texture front = KitPicture("Applet Data/BigBarFront.bmp");
	ImVec2 p = ImGui::GetCursorScreenPos();
	float h = back ? (float)back.height : 18.0f;
	ImDrawList* dl = ImGui::GetWindowDrawList();
	level = std::clamp(level, 0.0f, 1.0f);
	if (back && front) {
		dl->AddImage(back.as<ImTextureID>(), p, p + ImVec2(width, h));
		if (need) // green when low, toward red as it fills
			dl->AddImage(front.as<ImTextureID>(), p, p + ImVec2(width * level, h), ImVec2(1, 0), ImVec2(1 - level, 1));
		else
			dl->AddImage(front.as<ImTextureID>(), p, p + ImVec2(width * level, h), ImVec2(0, 0), ImVec2(level, 1));
	} else {
		dl->AddRectFilled(p, p + ImVec2(width, h), IM_COL32(40, 20, 50, 255));
		dl->AddRectFilled(p, p + ImVec2(width * level, h), IM_COL32(60, 190, 60, 255));
	}
	dl->AddRect(p, p + ImVec2(width, h), IM_COL32(0, 0, 0, 160));
	if (label && *label) {
		ImVec2 ts = ImGui::CalcTextSize(label);
		ImVec2 tp = p + ImVec2(6, (h - ts.y) / 2);
		dl->AddText(tp + ImVec2(1, 1), IM_COL32(0, 0, 0, 200), label);
		dl->AddText(tp, IM_COL32(255, 255, 255, 255), label);
	}
	ImGui::Dummy(ImVec2(width, h));
}

// ---- windows and chips ---------------------------------------------------------

static Texture appletIcons() {
	static Texture t;
	if (!t)
		t = GetTextureFromExeFileWithTransparentTopLeft(0xe6);
	return t;
}

static void drawAppletIcon(int icon, ImVec2 p, float size) {
	Texture t = appletIcons();
	if (!t)
		return;
	float u0 = icon * 22.0f / t.width, u1 = (icon * 22.0f + 22) / t.width;
	ImGui::GetWindowDrawList()->AddImage(t.as<ImTextureID>(), p, p + ImVec2(size, size * 21 / 22), ImVec2(u0, 0), ImVec2(u1, 21.0f / t.height));
}

// a minimize button in the current window's title bar, left of its close
// button (drawn like ImGui's own title buttons)
static bool TitleMinimizeButton() {
	ImGuiContext& g = *GImGui;
	ImGuiWindow* w = ImGui::GetCurrentWindow();
	if (w->Flags & ImGuiWindowFlags_NoTitleBar)
		return false;
	ImRect tb = w->TitleBarRect();
	float sz = g.FontSize;
	ImVec2 pos(tb.Max.x - g.Style.FramePadding.x - sz * 2 - g.Style.ItemInnerSpacing.x, tb.Min.y + g.Style.FramePadding.y);
	ImRect bb(pos, pos + ImVec2(sz, sz));
	ImGuiID id = w->GetID("#MINIMIZE");
	ImGui::PushClipRect(tb.Min, tb.Max, false);
	ImGui::ItemAdd(bb, id);
	bool hovered, held;
	bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
	if (hovered || held)
		w->DrawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered));
	float pad = std::max(2.0f, sz * 0.25f);
	w->DrawList->AddLine(ImVec2(bb.Min.x + pad, bb.Max.y - pad - 0.5f), ImVec2(bb.Max.x - pad, bb.Max.y - pad - 0.5f), ImGui::GetColorU32(ImGuiCol_Text), 2.0f);
	if (hovered)
		ImGui::SetTooltip("Minimize to the status bar");
	ImGui::PopClipRect();
	return pressed;
}

void DrawC2Kits() {
	if (engine.version != 2)
		return;
	SetUpKitSettings();
	for (auto& k : s_kits) {
		auto& st = s_state[k.id];
		if (!st.open || st.minimized)
			continue;
		if (s_focus_next == k.id) {
			ImGui::SetNextWindowFocus();
			s_focus_next.clear();
		}
		if (s_test_kits)
			ImGui::SetNextWindowPos(ImVec2(8, 96), ImGuiCond_Always);
		else
			ImGui::SetNextWindowPos(ImVec2(120, 120), ImGuiCond_FirstUseEver);
		bool open = true;
		std::string title = fmt::format("{}###kit_{}", k.title, k.id);
		if (ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar)) {
			if (TitleMinimizeButton()) {
				st.minimized = true;
				ImGui::MarkIniSettingsDirty();
			}
			ImGui::PushID(k.id);
			k.draw();
			ImGui::PopID();
		}
		ImGui::End();
		if (!open) {
			st.open = false;
			ImGui::MarkIniSettingsDirty();
		}
	}
}

void DrawC2KitChips() {
	if (engine.version != 2)
		return;
	for (auto& k : s_kits) {
		auto& st = s_state[k.id];
		if (!st.open || !st.minimized)
			continue;
		ImGui::PushID(k.id);
		float icon = ImGui::GetFontSize() + 2;
		std::string label = fmt::format("{}##chip", k.chip);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x + icon + 2, 1));
		bool pressed = ImGui::Button(label.c_str());
		ImGui::PopStyleVar();
		ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
		drawAppletIcon(k.icon, ImVec2(mn.x + 4, mn.y + (mx.y - mn.y - icon * 21 / 22) / 2), icon);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Restore the %s", k.title);
		if (pressed) {
			st.minimized = false;
			s_focus_next = k.id;
			ImGui::MarkIniSettingsDirty();
		}
		ImGui::PopID();
	}
}

} // namespace Openc2eImgui
