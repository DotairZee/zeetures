#include "C2Kits.h"

#include "C2KitsInternal.h"
#include "Hatchery.h"
#include "AgentInjector.h"
#include "ChemicalNamesManager.h"
#include "Engine.h"
#include "PathResolver.h"
#include "World.h"
#include "Camera.h"
#include "creatures/Creature.h"
#include "historyManager.h"
#include "CobManager.h"
#include "common/Ascii.h"
#include "fileformats/c2cobfile.h"
#include "common/backend/Backend.h"
#include "creatures/CreatureAgent.h"
#include "creatures/oldCreature.h"
#include "AgentHelpers.h"
#include "Room.h"
#include "MetaRoom.h"
#include "Map.h"
#include "common/creaturesImage.h"
#include "SpritePart.h"
#include "caosValue.h"
#include "fileformats/ImageUtils.h"
#include "fileformats/genomeFile.h"

#include <algorithm>
#include <cfloat>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <tuple>

#include <cstdarg>
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
	bool resizable = false; // a list the player may size; otherwise fits its page
};

static const KitDef s_kits[] = {
	{"health", "Health Kit", "Health", 3, DrawHealthKit},
	{"science", "Science Kit", "Science", 4, DrawScienceKit},
	{"breeders", "Breeder's Kit", "Breeder's", 5, DrawBreedersKit},
	{"owners", "Owner's Kit", "Owner's", 2, DrawOwnersKit},
	{"observation", "Observation Kit", "Observation", 6, DrawObservationKit},
	{"chronicles", "Chronicles Kit", "Chronicles", 8, DrawChroniclesKit, true},
	{"graveyard", "Graveyard Kit", "Graveyard", 9, DrawGraveyardKit},
	{"mind", "Mind Kit", "Mind", 12, DrawMindKit},
	{"ecology", "Ecology Kit", "Ecology", 1, DrawEcologyKit},
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
		else if (std::strncmp(line, "Alarm=", 6) == 0)
			KitAlarmsReadLine(line + 6);
		return;
	}
	auto* st = (KitState*)entry;
	if (sscanf(line, "Open=%d", &v) == 1)
		st->open = v != 0;
	else if (sscanf(line, "Minimized=%d", &v) == 1)
		st->minimized = v != 0;
}

static void KitsWriteAll(ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf) {
	buf->appendf("[%s][settings]\nFahrenheit=%d\n", handler->TypeName, s_fahrenheit ? 1 : 0);
	KitAlarmsWrite(buf);
	buf->append("\n");
	for (auto& k : s_kits) {
		auto& st = s_state[k.id];
		buf->appendf("[%s][%s]\nOpen=%d\nMinimized=%d\n\n", handler->TypeName, k.id, st.open ? 1 : 0, st.minimized ? 1 : 0);
	}
}

// developer aid: OPENC2E_OPEN_KITS="health,science" selects the first norn (or other creature)
// (if none is selected)
// and opens those kits (for headless snapshots; the kits' ini file is then
// neither read nor written); OPENC2E_KIT_PAGE="Drives" picks a page
static bool s_test_kits = false;

static void OpenTestKits() {
	const char* s = getenv("OPENC2E_OPEN_KITS");
	if (!s)
		return;
	s_test_kits = true;
	for (int pass = 0; pass < 2 && !world.selectedcreature; pass++) // a norn, or else any creature
		for (auto& a : world.agents)
			if (a && dynamic_cast<CreatureAgent*>(a.get()) && (pass == 1 || a->genus == 1)) {
				world.selectedcreature = a;
				break;
			}
	std::string list = s;
	if (list.find("hatchery") != std::string::npos)
		SetHatcheryOpen(true);
	if (list.find("injector") != std::string::npos)
		SetAgentInjectorOpen(true);
	for (auto& k : s_kits)
		if (list.find(k.id) != std::string::npos) {
			OpenC2Kit(k.id);
			if (getenv("OPENC2E_KITS_MINIMIZED"))
				s_state[k.id].minimized = true;
		}
}

static std::string s_want_page; // a page to turn to (OpenC2KitAt)
static int s_want_frames = 0;

void OpenC2KitAt(const char* id, const char* page) {
	OpenC2Kit(id);
	s_want_page = page ? page : "";
	s_want_frames = 3;
}

bool KitTab(const char* label) {
	const char* want = getenv("OPENC2E_KIT_PAGE");
	static int frames = 0;
	bool force = want && std::strcmp(want, label) == 0 && frames++ < 3;
	if (!s_want_page.empty() && s_want_page == label) {
		force = true;
		if (--s_want_frames <= 0)
			s_want_page.clear();
	}
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

bool KitNeedsCreature() {
	if (KitCreature())
		return true;
	ImGui::Dummy(ImVec2(360, 30));
	KitNote("No creature is selected. Pick one from the list above, or click a creature in the world.");
	ImGui::Dummy(ImVec2(360, 30));
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

// ---- the kits' own look (owner, 2026-11-12: one style everywhere, not C2's
// panel art; flat panels, bars and cards in the window's own colors) ----

ImU32 KitLevelColor(float level, bool need) {
	// green, amber, red: a need is good when low, anything else when high
	float good = need ? 1.0f - level : level;
	if (good > 0.7f)
		return IM_COL32(77, 204, 77, 255);
	if (good > 0.4f)
		return IM_COL32(230, 179, 51, 255);
	return IM_COL32(225, 72, 58, 255);
}

void KitBar(float level, const char* label, float width, bool need) {
	ImVec2 p = ImGui::GetCursorScreenPos();
	float h = ImGui::GetFrameHeight();
	ImDrawList* dl = ImGui::GetWindowDrawList();
	level = std::clamp(level, 0.0f, 1.0f);
	dl->AddRectFilled(p, p + ImVec2(width, h), ImGui::GetColorU32(ImGuiCol_FrameBg), 2);
	if (level > 0)
		dl->AddRectFilled(p, p + ImVec2(std::max(2.0f, width * level), h), KitLevelColor(level, need), 2);
	if (label && *label) {
		ImVec2 ts = ImGui::CalcTextSize(label);
		ImVec2 tp = p + ImVec2(6, (h - ts.y) / 2);
		dl->AddText(tp + ImVec2(1, 1), IM_COL32(0, 0, 0, 170), label);
		dl->AddText(tp, IM_COL32(255, 255, 255, 255), label);
	}
	ImGui::Dummy(ImVec2(width, h));
}

void KitHeading(const char* text) {
	ImGui::Spacing();
	ImGui::TextColored(ImVec4(1.0f, 0.88f, 0.55f, 1), "%s", text);
	ImVec2 p = ImGui::GetCursorScreenPos();
	float w = ImGui::GetContentRegionAvail().x;
	ImGui::GetWindowDrawList()->AddLine(p + ImVec2(0, -2), p + ImVec2(w, -2), IM_COL32(255, 225, 140, 60));
}

void KitStatTile(const char* id, const char* head, const std::string& big, const char* unit, const char* sub, ImU32 subColor, float width, const std::vector<float>* trend) {
	const float h = 74;
	ImVec2 p = ImGui::GetCursorScreenPos();
	ImDrawList* dl = ImGui::GetWindowDrawList();
	dl->AddRectFilled(p, p + ImVec2(width, h), IM_COL32(26, 28, 34, 255), 5);
	dl->AddRect(p, p + ImVec2(width, h), IM_COL32(70, 72, 80, 255), 5);
	dl->AddText(p + ImVec2(9, 6), ImGui::GetColorU32(ImGuiCol_TextDisabled), head);
	// the reading, at twice the size
	ImFont* f = ImGui::GetFont();
	float fs = ImGui::GetFontSize() * 2;
	dl->AddText(f, fs, p + ImVec2(9, 22), IM_COL32(235, 238, 242, 255), big.c_str());
	float bw = f->CalcTextSizeA(fs, FLT_MAX, 0, big.c_str()).x;
	if (unit && *unit)
		dl->AddText(p + ImVec2(13 + bw, 22 + fs - ImGui::GetFontSize() - 2), ImGui::GetColorU32(ImGuiCol_TextDisabled), unit);
	if (sub && *sub)
		dl->AddText(p + ImVec2(9, h - ImGui::GetFontSize() - 6), subColor, sub);
	// a trend line at the right, newest last
	if (trend && trend->size() >= 2) {
		float lo = *std::min_element(trend->begin(), trend->end()), hi = *std::max_element(trend->begin(), trend->end());
		if (hi - lo < 1e-3f) {
			lo -= 1;
			hi += 1;
		}
		float x0 = p.x + width - 64, x1 = p.x + width - 9, y0 = p.y + h - 26, y1 = p.y + h - 8;
		std::vector<ImVec2> pts;
		for (size_t i = 0; i < trend->size(); i++)
			pts.push_back(ImVec2(x0 + (x1 - x0) * i / (trend->size() - 1), y1 - (y1 - y0) * ((*trend)[i] - lo) / (hi - lo)));
		dl->AddPolyline(pts.data(), (int)pts.size(), IM_COL32(110, 160, 230, 255), 0, 2);
	}
	ImGui::PushID(id);
	ImGui::InvisibleButton("tile", ImVec2(width, h));
	ImGui::PopID();
}

void KitCreaturePhoto(Agent* a, ImVec2 size) {
	ImVec2 p = ImGui::GetCursorScreenPos();
	ImDrawList* dl = ImGui::GetWindowDrawList();
	monikerData* m = a ? KitRecordsOf(a) : nullptr;
	Texture t = m && !m->photos.empty() ? KitPhotoTexture(engine.currentworlddir / m->photos.back()) : Texture();
	if (t)
		dl->AddImage(t.as<ImTextureID>(), p, p + size);
	else {
		dl->AddRectFilled(p, p + size, IM_COL32(18, 18, 20, 255));
		const char* none = "no photo yet";
		ImVec2 ns = ImGui::CalcTextSize(none);
		dl->AddText(p + ImVec2((size.x - ns.x) / 2, (size.y - ns.y) / 2), IM_COL32(110, 110, 110, 255), none);
	}
	dl->AddRect(p - ImVec2(1, 1), p + size + ImVec2(1, 1), IM_COL32(205, 170, 95, 255));
	ImGui::Dummy(size);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", t ? "Its latest photo (Owner's Kit, Photos)" : "Take one in the Owner's Kit, Photos");
}

bool KitBeginCard(const char* id, ImVec2 size) {
	ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(28, 29, 33, 255));
	ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(205, 170, 95, 255));
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 2);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 10));
	bool open = ImGui::BeginChild(id, size, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding | (size.y == 0 ? ImGuiChildFlags_AutoResizeY : 0));
	return open;
}

void KitEndCard() {
	ImGui::EndChild();
	ImGui::PopStyleVar(3);
	ImGui::PopStyleColor(2);
}

// ---- creatures and their records -----------------------------------------------

std::vector<std::shared_ptr<Agent>> KitLivingCreatures() {
	std::vector<std::shared_ptr<Agent>> out;
	for (auto& a : world.agents) {
		if (!a || a->isDying())
			continue;
		auto* ca = dynamic_cast<CreatureAgent*>(a.get());
		if (ca && ca->getCreature() && ca->getCreature()->isAlive())
			out.push_back(a);
	}
	return out;
}

c2Creature* KitC2Of(Agent* a) {
	auto* ca = dynamic_cast<CreatureAgent*>(a);
	return ca ? dynamic_cast<c2Creature*>(ca->getCreature()) : nullptr;
}

monikerData* KitRecordsOf(Agent* a) {
	auto* ca = dynamic_cast<CreatureAgent*>(a);
	if (!ca || !ca->getCreature() || !world.history)
		return nullptr;
	std::string m = world.history->findMoniker(ca->getCreature()->getGenome());
	if (m.empty() || !world.history->hasMoniker(m))
		return nullptr;
	return &world.history->getMoniker(m);
}

std::string KitGivenName(Agent* a) {
	monikerData* m = KitRecordsOf(a);
	return m ? m->name : std::string();
}

const char* KitSpeciesWord(Agent* a) {
	switch (a ? a->genus : 1) {
		case 2: return "grendel";
		case 3: return "ettin";
		default: return "norn";
	}
}

std::string KitCreatureName(Agent* a) {
	monikerData* m = KitRecordsOf(a);
	if (m && !m->name.empty())
		return m->name;
	return fmt::format("Unnamed {}", KitSpeciesWord(a));
}

static constexpr unsigned int TICKS_PER_PART = 3600, PARTS_PER_DAY = 5, DAYS_PER_SEASON = 4, SEASONS_PER_YEAR = 4;

unsigned int KitAgeTicks(monikerData& m) {
	unsigned int now = m.died ? m.diedtick : world.worldtickcount; // the dead age no more
	return now >= m.borntick ? now - m.borntick : 0;
}

std::string KitAgeText(unsigned int ticks, bool withMinutes) {
	unsigned int parts = ticks / TICKS_PER_PART;
	unsigned int days = parts / PARTS_PER_DAY;
	parts %= PARTS_PER_DAY;
	unsigned int minutes = ticks / 600; // C2 runs 10 world ticks a second
	std::string game = days ? fmt::format("{} day{} {} part{}", days, days == 1 ? "" : "s", parts, parts == 1 ? "" : "s")
					   : parts ? fmt::format("{} part{} of a day", parts, parts == 1 ? "" : "s")
							   : std::string("under a part of a day");
	return withMinutes ? fmt::format("{} ({} min of play)", game, minutes) : game;
}

std::string KitDateText(unsigned int tick) {
	// counted back from today's calendar
	long long nowParts = (((long long)world.year * SEASONS_PER_YEAR + world.season) * DAYS_PER_SEASON + world.dayofseason) * PARTS_PER_DAY + world.timeofday;
	long long ago = world.worldtickcount >= tick ? (world.worldtickcount - tick) / TICKS_PER_PART : 0;
	long long p = std::max(0LL, nowParts - ago);
	long long day = p / PARTS_PER_DAY;
	int dayOfSeason = (int)(day % DAYS_PER_SEASON);
	long long season = day / DAYS_PER_SEASON;
	static const char* seasons[] = {"Spring", "Summer", "Autumn", "Winter"};
	return fmt::format("{}, day {}, year {:03d}", seasons[season % SEASONS_PER_YEAR], dayOfSeason + 1, season / SEASONS_PER_YEAR);
}

float KitOrganHealth(c2Creature* c, int* damaged, float* average, int* weakest) {
	float sum = 0, worst = 1.0f;
	int n = 0, bad = 0, worstAt = -1;
	auto& organs = c->getOrgans();
	for (size_t i = 0; i < organs.size(); i++) {
		auto& o = organs[i];
		float start = o->getInitialLifeforce();
		if (start <= 0)
			continue;
		float h = std::clamp(o->getShortTermLifeforce() / start, 0.0f, 1.0f);
		sum += h;
		n++;
		if (h < 0.75f)
			bad++;
		if (h < worst || worstAt < 0) {
			worst = h;
			worstAt = (int)i;
		}
	}
	if (damaged)
		*damaged = bad;
	if (average)
		*average = n ? sum / n : 1.0f;
	if (weakest)
		*weakest = worstAt;
	return n ? worst : 1.0f;
}

std::string KitWhere(Agent* a) {
	if (!a)
		return {};
	return KitWhereAt(a->x + a->getWidth() / 2.0f, a->y + a->getHeight() / 2.0f);
}

std::string KitWhereAt(float cx, float cy) {
	const World::FavoritePlace* best = nullptr;
	float bestd = 1e30f;
	for (auto& f : world.favoriteplaces) {
		if (f.name.empty())
			continue;
		// places store the camera's top-left; their middle is about 400 x 250 in
		float dx = cx - (f.x + 400), dy = cy - (f.y + 250);
		float d = dx * dx + dy * dy;
		if (d < bestd) {
			bestd = d;
			best = &f;
		}
	}
	if (best && bestd < 900.0f * 900.0f)
		return fmt::format("near {}", best->name);
	// otherwise its part of the metaroom: fifths across, thirds down
	MetaRoom* m = world.map && cx >= 0 && cy >= 0 ? world.map->metaRoomAt((unsigned int)cx, (unsigned int)cy) : nullptr;
	if (m && m->width() > 0 && m->height() > 0) {
		static const char* const ACROSS[5] = {"far west", "west", "middle", "east", "far east"};
		int i = std::clamp((int)((cx - m->x()) / m->width() * 5), 0, 4), j = std::clamp((int)((cy - m->y()) / m->height() * 3), 0, 2);
		return fmt::format("{}in the {}", j == 0 ? "high up " : j == 2 ? "low down " : "", ACROSS[i]);
	}
	if (best)
		return fmt::format("far from {}", best->name);
	return fmt::format("{:.0f}, {:.0f}", cx, cy);
}

// a small picture of a metaroom's background, made once per background
Texture KitMapPicture(MetaRoom* m, int& w, int& h) {
	static std::map<std::string, std::tuple<Texture, int, int>> cache;
	auto bk = m ? m->getCurrentBackground() : nullptr;
	if (!bk || bk->images.empty())
		return {};
	auto it = cache.find(bk->getName());
	if (it != cache.end()) {
		w = std::get<1>(it->second);
		h = std::get<2>(it->second);
		return std::get<0>(it->second);
	}
	::Image rgb = ImageUtils::ToRGB24(bk->images[0]);
	const int W = rgb.width, H = rgb.height;
	w = 660;
	h = std::max(1, (int)std::lround((double)H * w / W));
	::Image out;
	out.width = w;
	out.height = h;
	out.format = if_rgba32;
	out.data = shared_array<uint8_t>(size_t(w) * h * 4);
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			int x0 = x * W / w, x1 = std::max(x0 + 1, (x + 1) * W / w), ya = y * H / h, yb = std::max(ya + 1, (y + 1) * H / h);
			int sum[3] = {0, 0, 0}, n = 0;
			for (int yy = ya; yy < yb; yy += 2)
				for (int xx = x0; xx < x1; xx += 2) {
					const uint8_t* p = rgb.data.data() + (size_t(yy) * W + xx) * 3;
					sum[0] += p[0], sum[1] += p[1], sum[2] += p[2], n++;
				}
			uint8_t* d = out.data.data() + (size_t(y) * w + x) * 4;
			for (int c = 0; c < 3; c++)
				d[c] = (uint8_t)(sum[c] / std::max(1, n) * 45 / 100); // dimmed, so the marks stand out
			d[3] = 255;
		}
	Texture t = get_backend()->createTextureFromImage(out);
	cache[bk->getName()] = {t, w, h};
	return t;
}

void KitGoTo(const std::shared_ptr<Agent>& a) {
	if (!a)
		return;
	world.selectCreature(a);
	engine.camera->trackAgent(AgentRef(), 0, 0, brittle, none);
	int cx = (int)(a->x + a->getWidth() / 2.0f) - (int)engine.camera->getWidth() / 2;
	int cy = (int)(a->y + a->getHeight() / 2.0f) - (int)engine.camera->getHeight() / 2;
	engine.camera->moveToGlobal(cx, cy, jump);
}

void KitLookAt(const std::shared_ptr<Agent>& a) {
	if (!a)
		return;
	engine.camera->trackAgent(AgentRef(), 0, 0, brittle, none);
	int cx = (int)(a->x + a->getWidth() / 2.0f) - (int)engine.camera->getWidth() / 2;
	int cy = (int)(a->y + a->getHeight() / 2.0f) - (int)engine.camera->getHeight() / 2;
	engine.camera->moveToGlobal(cx, cy, jump);
}

std::string KitCategoryNoun(int category) {
	size_t w = 16 + (size_t)category;
	std::string s = category >= 0 && w < engine.wordlist.size() ? engine.wordlist[w] : "";
	while (!s.empty() && (s.back() == ' ' || s.back() == '\r' || s.back() == '\n'))
		s.pop_back();
	while (!s.empty() && s.front() == ' ')
		s.erase(s.begin());
	if (s.empty())
		return fmt::format("kind {}", category);
	s[0] = (char)toupper((unsigned char)s[0]);
	return s;
}

// ---- layout shared by the kits -----------------------------------------------------

void KitCreatureHeader() {
	auto all = KitLivingCreatures();
	Agent* sel = KitCreatureAgent();
	if (all.empty() && !sel)
		return;
	std::vector<std::pair<std::string, std::shared_ptr<Agent>>> list;
	for (auto& a : all)
		list.push_back({KitCreatureName(a.get()), a});
	std::sort(list.begin(), list.end(), [](auto& x, auto& y) { return x.first < y.first; });
	int at = -1;
	for (size_t i = 0; i < list.size(); i++)
		if (list[i].second.get() == sel)
			at = (int)i;
	int n = (int)list.size();
	ImGui::BeginDisabled(n == 0 || (n == 1 && at == 0));
	if (ImGui::ArrowButton("##prevcreature", ImGuiDir_Left))
		KitGoTo(list[at < 0 ? n - 1 : (at + n - 1) % n].second);
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("The previous creature");
	ImGui::SameLine(0, 2);
	if (ImGui::ArrowButton("##nextcreature", ImGuiDir_Right))
		KitGoTo(list[at < 0 ? 0 : (at + 1) % n].second);
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("The next creature");
	ImGui::EndDisabled();
	ImGui::SameLine();
	auto describe = [](Agent* a) {
		c2Creature* c = KitC2Of(a);
		std::string kind = fmt::format("{} {}", c && c->isFemale() ? "female" : "male", KitSpeciesWord(a));
		if (c && !c->isAlive())
			kind += ", dead";
		return fmt::format("{}  ({})", KitCreatureName(a), kind);
	};
	std::string preview = sel ? describe(sel) : std::string("Choose a creature");
	ImGui::SetNextItemWidth(260);
	if (ImGui::BeginCombo("##creature", preview.c_str())) {
		for (auto& [name, a] : list) {
			ImGui::PushID(a.get());
			if (ImGui::Selectable(describe(a.get()).c_str(), a.get() == sel))
				KitGoTo(a);
			ImGui::PopID();
		}
		ImGui::EndCombo();
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("The creature these pages show; choose another here or click one in the world");
}

// A note wraps at the window's width and does not count toward it, so a
// window fits its other content (a wrapped line otherwise holds a window at
// whatever width it had, after a wider page).
static void kitNoteV(const ImVec4& color, const char* fmt, va_list args) {
	ImGuiWindow* w = ImGui::GetCurrentWindow();
	float keepMax = w->DC.CursorMaxPos.x, keepIdeal = w->DC.IdealMaxPos.x;
	ImGui::PushStyleColor(ImGuiCol_Text, color);
	ImGui::PushTextWrapPos(0.0f); // the window's width
	ImGui::TextWrappedV(fmt, args);
	ImGui::PopTextWrapPos();
	ImGui::PopStyleColor();
	w->DC.CursorMaxPos.x = keepMax;
	w->DC.IdealMaxPos.x = keepIdeal;
}

void KitNote(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	kitNoteV(ImGui::GetStyle().Colors[ImGuiCol_TextDisabled], fmt, args);
	va_end(args);
}

void KitNoteColored(const ImVec4& color, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	kitNoteV(color, fmt, args);
	va_end(args);
}

void KitCenterNext(float width) {
	float spare = ImGui::GetContentRegionAvail().x - width;
	if (spare > 1)
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + spare / 2);
}

// ---- eggs ------------------------------------------------------------------------

std::string KitEggGenome(Agent* egg) {
	if (!egg || !egg->var[0].hasInt() || egg->var[0].getInt() == 0)
		return "";
	int v = egg->var[0].getInt();
	std::string name((const char*)&v, 4);
	for (char ch : name)
		if (!std::isalnum((unsigned char)ch))
			return "";
	return name;
}

std::vector<std::shared_ptr<Agent>> KitUnhatchedEggs() {
	std::vector<std::shared_ptr<Agent>> out;
	for (auto& a : world.agents) {
		// creatures' eggs only (2 5 2: laid by a creature or taken from the
		// Hatchery); not the world's own grendel and ettin eggs (2 5 3 and
		// 2 5 4, which it makes itself; owner, 2026-11-11)
		if (!a || a->isDying() || a->family != 2 || a->genus != 5 || a->species != 2)
			continue;
		// hatched: an empty shell waiting to be removed (pose 7)
		int pose = -1;
		if (auto* sp = dynamic_cast<SpritePart*>(a->part(0)))
			pose = sp->getPose();
		if (pose == 7)
			continue;
		out.push_back(a);
	}
	return out;
}

std::shared_ptr<Agent> KitEggOf(const std::string& genome) {
	if (genome.empty())
		return nullptr;
	for (auto& a : KitUnhatchedEggs())
		if (KitEggGenome(a.get()) == genome)
			return a;
	return nullptr;
}

std::shared_ptr<Agent> KitHatchedFrom(const std::string& genome) {
	if (genome.empty())
		return nullptr;
	for (auto& [mon, m] : world.history->all()) {
		Agent* a = m.owner.get();
		if (!a || a->isDying())
			continue;
		for (auto& ev : m.events)
			if (ev.eventno == 2 && ev.monikers[1] == genome)
				return AgentRef(a).lock();
	}
	return nullptr;
}

std::string KitEggState(Agent* egg) {
	if (!egg)
		return "";
	if (egg->carriedby)
		return "being carried";
	auto room = roomContainingAgent(AgentRef(egg));
	if (room && room->type == 2)
		return "in water: it hatches once it is on dry land";
	if (egg->timerrate == 0)
		return "not counting down yet";
	return "counting down to hatching";
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

void KitCobShelf(const char* subfolder, const char* hint, const char* added) {
	static std::map<std::string, int> selectedBy;
	static std::map<std::string, std::string> messageBy;
	int& selected = selectedBy.try_emplace(subfolder, -1).first->second;
	std::string& message = messageBy[subfolder];
	auto& items = shelfItems(subfolder);
	if (items.empty()) {
		ImGui::TextDisabled("Nothing found in Applet Data/%s.", subfolder);
		return;
	}
	// the items as tiles, four to a row: each one's own picture, its name
	const float TW = 124, TH = 92;
	const int per = 4;
	ImDrawList* dl = ImGui::GetWindowDrawList();
	for (size_t i = 0; i < items.size(); i++) {
		auto& m = items[i];
		if (i % per)
			ImGui::SameLine();
		ImVec2 at = ImGui::GetCursorScreenPos();
		ImGui::PushID((int)i);
		if (ImGui::InvisibleButton("item", ImVec2(TW, TH)))
			selected = (int)i;
		bool hov = ImGui::IsItemHovered();
		ImGui::PopID();
		bool sel = (int)i == selected;
		dl->AddRectFilled(at, at + ImVec2(TW, TH), sel ? IM_COL32(48, 44, 30, 255) : hov ? IM_COL32(38, 40, 48, 255) : IM_COL32(26, 28, 34, 255), 5);
		dl->AddRect(at, at + ImVec2(TW, TH), sel ? IM_COL32(205, 170, 95, 255) : IM_COL32(70, 72, 80, 255), 5, 0, sel ? 2.0f : 1.0f);
		if (m.picture) {
			float w = (float)m.picture.width, h = (float)m.picture.height;
			float sc = std::min(1.0f, std::min((TW - 16) / w, (TH - 30) / h));
			ImVec2 sz(w * sc, h * sc);
			ImVec2 ip = at + ImVec2((TW - sz.x) / 2, 6 + (TH - 30 - sz.y) / 2);
			dl->AddImage(m.picture.as<ImTextureID>(), ip, ip + sz);
		}
		// the name, cut to fit
		std::string name = m.name;
		while (name.size() > 3 && ImGui::CalcTextSize(name.c_str()).x > TW - 10)
			name = name.substr(0, name.size() - 4) + "...";
		ImVec2 ts = ImGui::CalcTextSize(name.c_str());
		dl->AddText(at + ImVec2((TW - ts.x) / 2, TH - ts.y - 6), IM_COL32(222, 224, 228, 255), name.c_str());
		if (hov)
			ImGui::SetTooltip("%s", m.name.c_str());
	}
	ImGui::Spacing();
	const float width = per * TW + (per - 1) * ImGui::GetStyle().ItemSpacing.x;
	if (selected >= 0 && selected < (int)items.size()) {
		auto& m = items[selected];
		KitBeginCard("about", ImVec2(width, 0));
		ImGui::TextColored(ImVec4(1.0f, 0.88f, 0.55f, 1), "%s", m.name.c_str());
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width - 24);
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
		KitEndCard();
	} else {
		KitNote("%s", hint);
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
		ImGui::SetTooltip("Minimise to the status bar");
	ImGui::PopClipRect();
	return pressed;
}

// Default names ("Norn 0005": the fifth norn this world has had). Births
// take the next number (Creature::born); the creatures a world already had
// when it was first loaded with this get theirs here, oldest first, named
// ones included (they keep their names but are counted).
static void GiveDefaultNames() {
	if (!world.history)
		return;
	if (world.history->namesgiven) {
		// after that, any creature still without a name gets the next number
		// (a hatchling from before births were recorded, or an imported one)
		static int frame = 0;
		if (++frame % 30 || ImGui::IsAnyItemActive()) // not while a name is being typed
			return;
		for (auto& a : KitLivingCreatures())
			if (monikerData* m = KitRecordsOf(a.get()); m && m->name.empty())
				m->name = world.history->nextDefaultName(m->genus);
		return;
	}
	world.history->namesgiven = true;
	std::vector<std::pair<monikerData*, std::string>> all;
	for (auto& a : KitLivingCreatures())
		if (monikerData* m = KitRecordsOf(a.get()))
			all.push_back({m, m->getName()});
	std::sort(all.begin(), all.end(), [](auto& x, auto& y) {
		return std::tie(x.first->borntick, x.second) < std::tie(y.first->borntick, y.second);
	});
	for (auto& [m, mon] : all) {
		std::string n = world.history->nextDefaultName(m->genus);
		if (m->name.empty())
			m->name = n;
	}
}

void DrawC2Kits() {
	if (engine.version != 2)
		return;
	GiveDefaultNames();
	SetUpKitSettings();
	KitAlarmsTick();
	EcologyKitTick();
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
		if (k.resizable)
			ImGui::SetNextWindowSize(ImVec2(960, 360), ImGuiCond_FirstUseEver);
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar;
		if (!k.resizable)
			flags |= ImGuiWindowFlags_AlwaysAutoResize;
		if (ImGui::Begin(title.c_str(), &open, flags)) {
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

bool KitTitleMinimizeButton() {
	return TitleMinimizeButton();
}

bool KitChipButton(const char* chip, int iconIndex, const char* tooltip) {
	float icon = ImGui::GetFontSize() + 2;
	std::string label = fmt::format("{}##chip", chip);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x + icon + 2, 1));
	bool pressed = ImGui::Button(label.c_str());
	ImGui::PopStyleVar();
	ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
	drawAppletIcon(iconIndex, ImVec2(mn.x + 4, mn.y + (mx.y - mn.y - icon * 21 / 22) / 2), icon);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", tooltip);
	return pressed;
}

void DrawC2KitChips() {
	if (engine.version != 2)
		return;
	DrawHatcheryChip();
	DrawAgentInjectorChip();
	for (auto& k : s_kits) {
		auto& st = s_state[k.id];
		if (!st.open || !st.minimized)
			continue;
		ImGui::PushID(k.id);
		std::string tip = fmt::format("Restore the {}", k.title);
		if (KitChipButton(k.chip, k.icon, tip.c_str())) {
			st.minimized = false;
			s_focus_next = k.id;
			ImGui::MarkIniSettingsDirty();
		}
		ImGui::PopID();
	}
}

} // namespace Openc2eImgui
