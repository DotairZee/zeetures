#include "C2Kits.h"

#include "C2KitsInternal.h"
#include "ChemicalNamesManager.h"
#include "Engine.h"
#include "PathResolver.h"
#include "World.h"
#include "CobManager.h"
#include "common/Ascii.h"
#include "fileformats/c2cobfile.h"
#include "common/backend/Backend.h"
#include "creatures/CreatureAgent.h"
#include "creatures/oldCreature.h"
#include "fileformats/ImageUtils.h"
#include "fileformats/genomeFile.h"

#include <filesystem>
#include <fstream>
#include <tuple>

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
	{"breeders", "Breeder's Kit", "Breeder's", 5, DrawBreedersKit},
};

struct KitState {
	bool open = false;
	bool minimized = false;
};

static std::map<std::string, KitState> s_state;
static bool s_fahrenheit = false;

bool KitFahrenheit() {
	return s_fahrenheit;
}

void KitToggleFahrenheit() {
	s_fahrenheit = !s_fahrenheit;
	ImGui::MarkIniSettingsDirty();
}
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

static int s_settings_entry; // stands for the [settings] section

static void* KitsReadOpen(ImGuiContext*, ImGuiSettingsHandler*, const char* name) {
	if (std::strcmp(name, "settings") == 0)
		return &s_settings_entry;
	const KitDef* k = findKit(name);
	return k ? (void*)&s_state[k->id] : nullptr;
}

static void KitsReadLine(ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line) {
	int v;
	if (entry == &s_settings_entry) {
		if (sscanf(line, "Fahrenheit=%d", &v) == 1)
			s_fahrenheit = v != 0;
		return;
	}
	auto* st = (KitState*)entry;
	if (sscanf(line, "Open=%d", &v) == 1)
		st->open = v != 0;
	else if (sscanf(line, "Minimized=%d", &v) == 1)
		st->minimized = v != 0;
}

static void KitsWriteAll(ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf) {
	buf->appendf("[%s][settings]\nFahrenheit=%d\n\n", handler->TypeName, s_fahrenheit ? 1 : 0);
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

// ---- shelves of COBs (Doctor, Aphrodisiacs) -------------------------------------

std::vector<std::string> KitAppletDataFiles(const std::string& sub, const std::string& ext) {
	namespace fs = std::filesystem;
	std::vector<std::string> out;
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
						out.push_back(f.path().string());
			}
		}
	}
	std::sort(out.begin(), out.end());
	return out;
}

struct ShelfItem {
	std::string name, description, file;
	Texture picture;
};

static std::vector<ShelfItem>& shelfItems(const std::string& sub) {
	static std::map<std::string, std::vector<ShelfItem>> cache;
	auto it = cache.find(sub);
	if (it != cache.end())
		return it->second;
	auto& list = cache[sub];
	for (auto& f : KitAppletDataFiles(sub, ".cob")) {
		try {
			c2cobfile cob(f);
			for (auto b : cob.blocks) {
				if (b->type != "agnt")
					continue;
				cobAgentBlock a(b);
				ShelfItem m;
				m.name = a.name;
				m.description = a.description;
				m.file = f;
				if (a.thumbnail.width > 0)
					m.picture = get_backend()->createTextureFromImage(a.thumbnail);
				list.push_back(m);
				break;
			}
		} catch (std::exception& e) {
			fmt::print("Kits: couldn't read {}: {}\n", f, e.what());
		}
	}
	return list;
}

void KitCobShelf(const char* picture, const char* subfolder, const std::vector<float>& shelves, const std::vector<float>& columns, const char* hint, const char* added) {
	static std::map<std::string, int> selectedBy;
	static std::map<std::string, std::string> messageBy;
	int& selected = selectedBy.try_emplace(subfolder, -1).first->second;
	std::string& message = messageBy[subfolder];
	auto& items = shelfItems(subfolder);
	Texture bg = KitPicture(picture);
	ImVec2 p = ImGui::GetCursorScreenPos();
	DrawTexture(bg, p);
	ImGui::Dummy(ImVec2(bg ? bg.width : 314, bg ? bg.height : 195));
	ImVec2 after = ImGui::GetCursorScreenPos();
	size_t per = columns.size();
	for (size_t i = 0; i < items.size() && i < per * shelves.size(); i++) {
		auto& m = items[i];
		float w = m.picture ? (float)m.picture.width : 24, h = m.picture ? (float)m.picture.height : 24;
		float sc = std::min(1.0f, std::min(80.0f / w, 52.0f / h)); // fit a shelf
		ImVec2 sz(w * sc, h * sc);
		ImVec2 at = p + ImVec2(columns[i % per] - sz.x / 2, shelves[i / per] - sz.y);
		ImGui::SetCursorScreenPos(at);
		ImGui::PushID((int)i);
		if (ImGui::InvisibleButton("item", sz))
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
	ImGui::SetCursorScreenPos(after);
	if (items.empty()) {
		ImGui::TextDisabled("Nothing found in Applet Data/%s.", subfolder);
		return;
	}
	if (selected >= 0 && selected < (int)items.size()) {
		auto& m = items[selected];
		ImGui::TextUnformatted(m.name.c_str());
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + (bg ? bg.width : 314));
		ImGui::TextDisabled("%s", m.description.c_str());
		ImGui::PopTextWrapPos();
		if (ImGui::Button("Add to the world")) {
			try {
				CobManager cm;
				cm.inject(CobManager::CobFileInfo(m.name, m.file));
				message = fmt::format(fmt::runtime(added), m.name);
			} catch (std::exception& e) {
				message = fmt::format("Couldn't add {}: {}", m.name, e.what());
			}
		}
	} else {
		ImGui::TextDisabled("%s", hint);
	}
	if (!message.empty())
		ImGui::TextUnformatted(message.c_str());
}

// ---- gene notes ---------------------------------------------------------------------

// plain text from a note's RTF: font and color tables dropped, \par as a
// new line, other control words and braces removed
static std::string rtfToText(const std::string& r) {
	std::string out;
	int skipDepth = -1, depth = 0;
	for (size_t i = 0; i < r.size(); i++) {
		char ch = r[i];
		if (ch == '{') {
			depth++;
			if (skipDepth < 0 && (r.compare(i, 9, "{\\fonttbl") == 0 || r.compare(i, 10, "{\\colortbl") == 0))
				skipDepth = depth;
			continue;
		}
		if (ch == '}') {
			if (depth == skipDepth)
				skipDepth = -1;
			depth--;
			continue;
		}
		if (skipDepth >= 0)
			continue;
		if (ch == '\\') {
			size_t j = i + 1;
			while (j < r.size() && isalpha((unsigned char)r[j]))
				j++;
			std::string word = r.substr(i + 1, j - i - 1);
			while (j < r.size() && (isdigit((unsigned char)r[j]) || r[j] == '-'))
				j++;
			if (j < r.size() && r[j] == ' ')
				j++;
			if (word == "par")
				out += '\n';
			else if (word.empty() && j < r.size() && r[j] == '\'' && j + 2 < r.size()) { // \'hh, a character by number
				out += (char)std::stoi(r.substr(j + 1, 2), nullptr, 16);
				j += 3;
			} else if (word.empty() && j < r.size()) // an escaped character
				out += r[j++];
			i = j - 1;
			continue;
		}
		if (ch == '\r' || ch == '\n')
			continue;
		out += ch;
	}
	while (!out.empty() && isspace((unsigned char)out.back()))
		out.pop_back();
	while (!out.empty() && isspace((unsigned char)out.front()))
		out.erase(out.begin());
	return out;
}

// "002 HEART - Organ" -> "Heart"; words in capitals of 4 letters or more
// get ordinary capitalization ("CNS" stays)
static std::string tidyCaption(std::string c) {
	if (c.size() > 4 && isdigit((unsigned char)c[0]) && isdigit((unsigned char)c[1]) && isdigit((unsigned char)c[2]) && c[3] == ' ')
		c = c.substr(4);
	auto dash = c.rfind(" - ");
	if (dash != std::string::npos)
		c = c.substr(0, dash);
	while (!c.empty() && c.back() == ' ')
		c.pop_back();
	std::string out;
	size_t i = 0;
	while (i < c.size()) {
		size_t j = i;
		while (j < c.size() && c[j] != ' ')
			j++;
		std::string w = c.substr(i, j - i);
		int letters = 0;
		bool allcaps = true;
		for (char ch : w)
			if (isalpha((unsigned char)ch)) {
				letters++;
				if (islower((unsigned char)ch))
					allcaps = false;
			}
		if (allcaps && letters >= 4) {
			bool first = true;
			for (char& ch : w)
				if (isalpha((unsigned char)ch)) {
					ch = first ? ch : (char)tolower((unsigned char)ch);
					first = false;
				}
		}
		out += w;
		if (j < c.size())
			out += ' ';
		i = j + 1;
	}
	return out;
}

using NoteKey = std::tuple<int, int, int>; // type, subtype, place among its kind (from 1)

static const std::map<NoteKey, KitGeneNote>& notesFile(const std::string& name) {
	static std::map<std::string, std::map<NoteKey, KitGeneNote>> cache;
	auto it = cache.find(name);
	if (it != cache.end())
		return it->second;
	auto& notes = cache[name];
	auto path = findGeneticsFile(name);
	if (path.empty())
		return notes;
	std::ifstream in(path, std::ios::binary);
	std::vector<uint8_t> d((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	auto u16 = [&](size_t p) { return p + 2 <= d.size() ? (int)(d[p] | (d[p + 1] << 8)) : -1; };
	size_t p = 2; // a version number, then the count
	int count = u16(p);
	p += 2;
	for (int i = 0; i < count && p + 10 <= d.size(); i++) {
		int type = u16(p), sub = u16(p + 2);
		int which = (int)(d[p + 4] | (d[p + 5] << 8) | (d[p + 6] << 16) | ((uint32_t)d[p + 7] << 24));
		p += 8;
		int len = u16(p);
		p += 2;
		if (len < 0 || p + len > d.size())
			break;
		std::string caption(d.begin() + p, d.begin() + p + len);
		p += len;
		len = u16(p);
		p += 2;
		if (len < 0 || p + len > d.size())
			break;
		std::string comment(d.begin() + p, d.begin() + p + len);
		p += len;
		notes[{type, sub, which}] = {tidyCaption(caption), rtfToText(comment)};
	}
	return notes;
}

const KitGeneNote* KitNoteFor(gene* g) {
	c2Creature* c = KitCreature();
	Agent* a = KitCreatureAgent();
	if (!c || !a || !g)
		return nullptr;
	(void)a;
	int genus = c->getGenus(); // 0 norn, 1 grendel, 2 ettin
	const char* file = genus == 1 ? "gren.gno" : genus == 2 ? "ettn.gno" : "norn.gno";
	auto& notes = notesFile(file);
	// each gene's place among its kind, worked out once per genome
	static genomeFile* s_genome = nullptr;
	static std::map<gene*, NoteKey> s_keys;
	auto genome = c->getGenome();
	if (genome.get() != s_genome) {
		s_genome = genome.get();
		s_keys.clear();
		std::map<std::pair<int, int>, int> seen;
		if (genome)
			for (auto& gp : genome->genes) {
				int t = gp->geneType(), st = gp->geneSubtype();
				s_keys[gp.get()] = {t, st, ++seen[{t, st}]};
			}
	}
	auto k = s_keys.find(g);
	if (k == s_keys.end())
		return nullptr;
	auto n = notes.find(k->second);
	return n == notes.end() ? nullptr : &n->second;
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
