// The Ecology Kit (C2; openc2e's own layout, owner 2026-11-12). After C2's
// Ecology Kit: a map of the world with every room shaded by one of the
// rooms' measures (light, temperature, air pressure, radiation, organic and
// inorganic nutrients; wind too, when the world has any), in the color scale
// and pins of the player's own EcologyKit.exe (bitmap resources read at
// runtime, never shipped). Three pages:
//   Map: hover a room for its readings, click to go there, right-click to
//     pin it; where the creatures and the food are.
//   Monitor: up to seven pinned rooms graphed over the last 15 minutes, and
//     the time of day.
//   Census: what is in the world now, by the game's own kinds of thing, and
//     how the food is spread from west to east.
// In this engine temperature (World::tickClimate), the nutrients
// (World::tickNutrients) and outdoor light (World::tickDaylight) change by
// themselves; the other measures keep the
// world file's values unless a script sets them. Scents include food smell.

#include "C2Kits.h"
#include "C2ToolBar.h"
#include "C2KitsInternal.h"
#include "Camera.h"
#include "SpritePart.h"
#include "historyManager.h"
#include "Engine.h"
#include "Map.h"
#include "MetaRoom.h"
#include "PathResolver.h"
#include "Room.h"
#include "Scriptorium.h"
#include "World.h"
#include "common/backend/Backend.h"
#include "creatures/CreatureAgent.h"
#include "creatures/Creature.h"
#include "creatures/Pheromones.h"
#include "fileformats/ImageUtils.h"
#include "fileformats/peFile.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include "creatures/Disease.h"
#include <fmt/format.h>
#include <imgui.h>
#include <map>
#include <memory>

namespace Openc2eImgui {

namespace {

// ---- the measures -----------------------------------------------------------

enum Measure { LIGHT,
	TEMPERATURE,
	PRESSURE,
	RADIATION,
	ORGANIC,
	INORGANIC,
	WIND,
	MEASURES };

const char* const MEASURE_NAME[MEASURES] = {"Light", "Temperature", "Air pressure", "Radiation", "Organic nutrients", "Inorganic nutrients", "Wind"};
const char* const MEASURE_TIP[MEASURES] = {
	"How bright each room is: outdoors it follows day and night (lamp-lit rooms,\ncaves and indoor rooms keep their light); the dark makes creatures sleepy",
	"How warm each room is (this changes: heat spreads through doorways\nand comes from fires, the volcano and heaters)",
	"The air pressure in each room",
	"How much radiation there is in each room",
	"Organic nutrients in each room's soil: rotting food and the dead add them,\nthey break down into minerals (faster where warm) and seep downhill",
	"Inorganic nutrients in each room's soil (minerals): fruit bushes grow\nfaster where there are more, and draw them up",
	"How hard the wind blows in each room",
};

int rawValueOf(const Room& r, int m) {
	switch (m) {
		case LIGHT: return r.lite;
		case TEMPERATURE: return r.temp;
		case PRESSURE: return r.pres;
		case RADIATION: return r.radn;
		case ORGANIC: return r.ontr;
		case INORGANIC: return r.intr;
		case WIND: return std::min(255, (int)std::lround(std::sqrt((double)r.windx * r.windx + (double)r.windy * r.windy)));
	}
	return 0;
}

int valueOf(const Room& r, int m) {
	return std::clamp(rawValueOf(r, m), 0, 255); // a few rooms carry values outside the scale
}

// a room's temperature (0-255) in degrees, as the status bar shows it
std::string degrees(float v) {
	float c = v * 80.0f / 255.0f - 15.0f;
	if (KitFahrenheit())
		return fmt::format("{}°F", (int)std::lround(c * 9 / 5.0f + 32));
	return fmt::format("{}°C", (int)std::lround(c));
}

const char* level(int v) {
	return v < 64 ? "very low" : v < 112 ? "low" : v < 160 ? "medium" : v < 208 ? "high" : "very high";
}

std::string valueText(float v, int m) {
	if (m == TEMPERATURE)
		return degrees(v);
	return fmt::format("{} ({:.0f})", level((int)v), v);
}

bool worldHasWind() {
	if (!world.map)
		return false;
	for (auto& r : world.map->getRooms())
		if (r->windx || r->windy)
			return true;
	return false;
}

// ---- the player's own Ecology Kit pictures ---------------------------------

struct EcoArt {
	bool loaded = false;
	std::vector<ImU32> scale; // the color scale, low to high
	Texture scaleTex; // the same, as C2 drew it
	Texture pins; // 8 balls of 16 x 16; 0 is the empty one
};

EcoArt& art() {
	static EcoArt a;
	if (a.loaded)
		return a;
	a.loaded = true;
	auto path = findMainDirectoryFile("EcologyKit.exe");
	if (!path.empty()) {
		try {
			peFile exe(path);
			Image grad = exe.getBitmap(135);
			if (grad.width > 0) {
				Image rgb = ImageUtils::ToRGB24(grad);
				for (int x = 0; x < rgb.width; x++) {
					const uint8_t* p = rgb.data.data() + (size_t(rgb.height / 2) * rgb.width + x) * 3;
					a.scale.push_back(IM_COL32(p[0], p[1], p[2], 255));
				}
				a.scaleTex = get_backend()->createTextureFromImage(grad);
			}
			Image pins = exe.getBitmap(134);
			if (pins.width > 0) {
				pins.colorkey = ImageUtils::GetPixelColor(pins, 0, 0);
				a.pins = get_backend()->createTextureFromImage(pins);
			}
		} catch (...) {
		}
	}
	if (a.scale.empty()) // no EcologyKit.exe: a plain blue-to-red scale
		for (int i = 0; i < 200; i++) {
			float t = i / 199.0f;
			a.scale.push_back(IM_COL32((int)(255 * t), (int)(200 * (1 - std::abs(t - 0.5f) * 2)), (int)(255 * (1 - t)), 255));
		}
	return a;
}

ImU32 colorFor(float v, int alpha) {
	auto& s = art().scale;
	size_t i = (size_t)std::clamp((int)std::lround(v / 255.0f * (s.size() - 1)), 0, (int)s.size() - 1);
	return (s[i] & ~IM_COL32_A_MASK) | ((ImU32)alpha << IM_COL32_A_SHIFT);
}

// the pins' colors, for their lines on the graph
const ImU32 PIN_COLOR[8] = {IM_COL32(200, 200, 200, 255), IM_COL32(40, 200, 40, 255), IM_COL32(230, 40, 40, 255),
	IM_COL32(70, 90, 255, 255), IM_COL32(230, 40, 230, 255), IM_COL32(230, 230, 30, 255), IM_COL32(30, 220, 220, 255),
	IM_COL32(170, 170, 170, 255)};

void drawPin(ImDrawList* dl, ImVec2 c, int color) {
	auto& a = art();
	if (a.pins) {
		float u0 = color * 16.0f / a.pins.width, u1 = (color + 1) * 16.0f / a.pins.width;
		dl->AddImage(a.pins.as<ImTextureID>(), ImVec2(c.x - 8, c.y - 8), ImVec2(c.x + 8, c.y + 8), ImVec2(u0, 0), ImVec2(u1, 1));
	} else {
		dl->AddCircleFilled(c, 7, PIN_COLOR[color], 16);
		dl->AddCircle(c, 7, IM_COL32(0, 0, 0, 255), 16);
	}
}

// ---- pinned rooms and their history ----------------------------------------

const unsigned int SAMPLE_TICKS = 30; // 3 seconds at C2's 10 ticks a second
const size_t SAMPLES = 300; // 15 minutes

struct Pin {
	std::weak_ptr<Room> room;
	unsigned int id = 0;
	int color = 1; // 1-7
	std::deque<std::array<float, MEASURES>> history; // oldest first
};

std::vector<Pin> s_pins;
std::weak_ptr<Room> s_pinsWorld; // the first room of the map the pins belong to
int s_measure = TEMPERATURE;
bool s_showCreatures = true, s_showFood = false;
// Scents on the map (owner, 2026-11-15: the scent overlay belongs in the
// Ecology Kit, not the View menu): the scent shown in place of a measure
// (a channel of pheromones.txt), or -1; and whether the world view is
// tinted by it too (World::showScent)
int s_scent = -1;
// germs (Disease.h; owner, 2026-11-21: "we should be able to see disease
// spread in the Ecology Kit"): a strain's id, 0 every strain together, -1 none
int s_germ = -1;

float germsOf(const Room& r) {
	if (s_germ == 0) {
		float sum = 0;
		for (auto& st : Disease::strains())
			sum += Disease::germsIn((int)r.id, st.id);
		return std::min(sum, 255.0f);
	}
	return Disease::germsIn((int)r.id, s_germ);
}
bool s_scentOnWorld = false;

// a scent's level in a room (0-1); one past the last pheromones.txt
// channel is food smell (World::tickFoodScent)
float scentOf(const Room& r, int ch) {
	if (ch == Pheromones::channelCount())
		return std::clamp(r.foodscent, 0.0f, 1.0f);
	return ch >= 0 && (size_t)ch < r.scent.size() ? r.scent[ch] : 0.0f;
}
std::string scentName(int ch) {
	return ch == Pheromones::channelCount() ? std::string("Food smell") : Pheromones::channelName(ch);
}

std::array<float, MEASURES> readings(const Room& r) {
	std::array<float, MEASURES> out;
	for (int m = 0; m < MEASURES; m++)
		out[m] = m == TEMPERATURE && r.tempf >= 0 ? r.tempf : (float)valueOf(r, m);
	return out;
}

Pin* pinOf(const std::shared_ptr<Room>& r) {
	for (auto& p : s_pins)
		if (p.room.lock() == r)
			return &p;
	return nullptr;
}

// the pins are kept with the world (saved in its state file)
void savePins() {
	if (!world.history)
		return;
	world.history->ecologypins.clear();
	for (auto& p : s_pins)
		world.history->ecologypins.push_back({p.id, p.color});
}

void addPin(const std::shared_ptr<Room>& r, int color) {
	Pin p;
	p.room = r;
	p.id = r->id;
	p.color = color;
	p.history.push_back(readings(*r));
	s_pins.push_back(p);
}

void togglePin(const std::shared_ptr<Room>& r) {
	if (!r)
		return;
	for (size_t i = 0; i < s_pins.size(); i++)
		if (s_pins[i].room.lock() == r) {
			s_pins.erase(s_pins.begin() + i);
			savePins();
			return;
		}
	if (s_pins.size() >= 7)
		return;
	int color = 1;
	for (int c = 1; c <= 7; c++) { // the first color not in use
		bool used = false;
		for (auto& q : s_pins)
			used |= q.color == c;
		if (!used) {
			color = c;
			break;
		}
	}
	addPin(r, color);
	savePins();
}

// ---- food -------------------------------------------------------------------

// things a creature can eat: their kind has an eat script (C2 event 12)
bool isFood(Agent* a) {
	static std::map<uint32_t, bool> known;
	uint32_t key = ((uint32_t)a->family << 24) | ((uint32_t)a->genus << 16) | a->species;
	auto it = known.find(key);
	if (it != known.end())
		return it->second;
	bool food = a->family == 2 && world.scriptorium->hasOwnScript(a->family, a->genus, a->species, 12);
	known[key] = food;
	return food;
}

bool isLoose(Agent* a) {
	return a && !a->isDying() && !dynamic_cast<CreatureAgent*>(a);
}

// ---- the map ------------------------------------------------------------------

MetaRoom* shownMetaRoom() {
	if (!world.map)
		return nullptr;
	MetaRoom* m = engine.camera ? engine.camera->getMetaRoom() : nullptr;
	if (!m && world.map->getMetaRoomCount() > 0)
		m = world.map->getMetaRoom(0);
	return m;
}

std::string roomTitle(const Room& r) {
	float cx = (r.x_left + r.x_right) / 2.0f, cy = (std::min(r.y_left_ceiling, r.y_right_ceiling) + std::max(r.y_left_floor, r.y_right_floor)) / 2.0f;
	return fmt::format("Room {}{}, {}", r.id, r.type == 2 ? " (water)" : "", KitWhereAt(cx, cy));
}

void goToRoom(const Room& r) {
	if (!engine.camera)
		return;
	float cx = (r.x_left + r.x_right) / 2.0f, cy = (std::min(r.y_left_ceiling, r.y_right_ceiling) + std::max(r.y_left_floor, r.y_right_floor)) / 2.0f;
	engine.camera->trackAgent(AgentRef(), 0, 0, brittle, none);
	engine.camera->moveToGlobal((int)cx - (int)engine.camera->getWidth() / 2, (int)cy - (int)engine.camera->getHeight() / 2, UiPanStyle());
}

float roomValue(const Room& r, int m) {
	return m == TEMPERATURE && r.tempf >= 0 ? r.tempf : (float)valueOf(r, m);
}

// the range the map's colors span: the lowest to the highest value in the
// metaroom, at least 40 wide (so a world with little difference is not shown
// as one of great contrast)
std::pair<float, float> rangeOf(MetaRoom* m, int measure) {
	float lo = 255, hi = 0;
	for (auto& r : m->rooms) {
		float v = roomValue(*r, measure);
		lo = std::min(lo, v), hi = std::max(hi, v);
	}
	if (lo > hi)
		return {0, 255};
	if (hi - lo < 40) {
		float mid = (lo + hi) / 2;
		lo = std::clamp(mid - 20, 0.0f, 215.0f);
		hi = lo + 40;
	}
	return {lo, hi};
}

void legend(int m, float width, float lo, float hi) {
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 o = ImGui::GetCursorScreenPos();
	auto& a = art();
	if (a.scaleTex)
		dl->AddImage(a.scaleTex.as<ImTextureID>(), o, ImVec2(o.x + width, o.y + 14));
	else
		for (int i = 0; i < (int)width; i++)
			dl->AddLine(ImVec2(o.x + i, o.y), ImVec2(o.x + i, o.y + 14), colorFor(i * 255.0f / width, 255));
	ImGui::Dummy(ImVec2(width, 14));
	float x0 = ImGui::GetCursorPosX();
	auto label = [&](float v) { return m == TEMPERATURE ? degrees(v) : fmt::format("{:.0f}", v); };
	std::string la = label(lo), lb = label((lo + hi) / 2), lc = label(hi);
	// (placed by cursor position: SameLine's offset counts from the
	// window's left, not this group's, which put the labels past the bar)
	ImGui::TextDisabled("%s", la.c_str());
	ImGui::SameLine();
	ImGui::SetCursorPosX(x0 + width / 2 - ImGui::CalcTextSize(lb.c_str()).x / 2);
	ImGui::TextDisabled("%s", lb.c_str());
	ImGui::SameLine();
	ImGui::SetCursorPosX(x0 + width - ImGui::CalcTextSize(lc.c_str()).x);
	ImGui::TextDisabled("%s", lc.c_str());
}

// the Monitor page's graph: the world's own measures, as radio buttons
void measureChooser(bool wind) {
	for (int m = 0; m < MEASURES; m++) {
		if (m == WIND && !wind)
			continue;
		if (ImGui::RadioButton(MEASURE_NAME[m], s_measure == m))
			s_measure = m;
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", MEASURE_TIP[m]);
	}
}

// the Map page's choice, a dropdown above the map (owner, 2026-11-15: the
// tall list of choices beside the map left the space under the map empty):
// the world's measures, then the scents (pheromones.txt)
void showChooser(bool wind) {
	int n = Pheromones::channelCount();
	if (s_scent > n)
		s_scent = -1;
	if (s_germ > 0 && !Disease::strain(s_germ))
		s_germ = -1;
	std::string current = s_germ == 0 ? std::string("Germs: every strain")
		: s_germ > 0 ? "Germs: " + Disease::strain(s_germ)->name()
		: s_scent >= 0 ? "Scent: " + scentName(s_scent) : MEASURE_NAME[s_measure];
	ImGui::SetNextItemWidth(230);
	if (ImGui::BeginCombo("##show", current.c_str(), ImGuiComboFlags_HeightLarge)) {
		for (int m = 0; m < MEASURES; m++) {
			if (m == WIND && !wind)
				continue;
			if (ImGui::Selectable(MEASURE_NAME[m], s_scent < 0 && s_germ < 0 && s_measure == m))
				s_measure = m, s_scent = -1, s_germ = -1;
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", MEASURE_TIP[m]);
		}
		ImGui::SeparatorText("Scents");
		if (ImGui::Selectable("Food smell##scent", s_scent == n))
			s_scent = n, s_germ = -1;
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("How strongly each room smells of food: food lying within reach gives it off, it spreads\nthrough doorways and fades, and hungry creatures follow it toward food out of sight");
		if (n > 0) {
			for (int ch = 0; ch < n; ch++) {
				std::string name = Pheromones::channelName(ch);
				if (ImGui::Selectable((name + "##scent").c_str(), s_scent == ch))
					s_scent = ch, s_germ = -1;
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("How much of the scent \"%s\" each room holds: creatures give it off\nand others follow or avoid it (the rules are in pheromones.txt)", name.c_str());
			}
		}
		if (!Disease::strains().empty()) {
			ImGui::SeparatorText("Germs");
			if (ImGui::Selectable("Every strain##germ", s_germ == 0))
				s_germ = 0, s_scent = -1;
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Germs of every strain in each room: dark caves keep them, rotting food feeds them,\nlight kills them, they seep through doorways; ill creatures and grendels shed them.\nOn the map, ill creatures are ringed in red");
			for (auto& st : Disease::strains()) {
				if (ImGui::Selectable((st.name() + "##germ").c_str(), s_germ == st.id))
					s_germ = st.id, s_scent = -1;
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s: antigen %d; %s", st.name().c_str(), st.antigen,
						st.home == 0 ? "lives on in dark caves" : st.home == 1 ? "feeds on rotting food" : "carried by grendels");
			}
		}
		ImGui::EndCombo();
	}
}

void MapPage() {
	MetaRoom* m = shownMetaRoom();
	int w = 0, h = 0;
	Texture pic = m ? KitMapPicture(m, w, h) : Texture();
	if (!m || !pic) {
		ImGui::TextWrapped("No map to show.");
		return;
	}
	bool wind = worldHasWind();
	if (s_measure == WIND && !wind)
		s_measure = TEMPERATURE;

	// one row of choices above the map
	ImGui::AlignTextToFramePadding();
	ImGui::TextDisabled("Show");
	ImGui::SameLine();
	showChooser(wind);
	ImGui::SameLine(0, 20);
	ImGui::Checkbox("Creatures", &s_showCreatures);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Where each creature is (click one to go to it)");
	ImGui::SameLine();
	ImGui::Checkbox("Food", &s_showFood);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Everything a creature can eat, where it is now");
	if (Pheromones::channelCount() > 0) {
		ImGui::SameLine();
		ImGui::BeginDisabled(s_scent < 0);
		ImGui::Checkbox("Tint the world too", &s_scentOnWorld);
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip(s_scent < 0 ? "Choose a scent under Show first" : "Tint the rooms in the main view by the chosen scent,\nto watch trails form where the creatures are");
	}
	world.showScent = s_scentOnWorld && s_scent >= 0 ? s_scent : -1;

	// the map as wide as the tab, keeping its shape (owner, 2026-11-15): the
	// kit's window fits what it holds, so the map sets the width, about 880
	// px, or the row of choices above it where that is wider
	float scale = std::clamp(std::max(ImGui::GetContentRegionAvail().x, 880.0f) / std::max(w, 1), 0.5f, 4.0f);
	w = (int)std::lround(w * scale), h = (int)std::lround(h * scale);

	ImGui::BeginGroup();
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 o = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton("##map", ImVec2((float)w, (float)h), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
	bool hovered = ImGui::IsItemHovered();
	bool leftClick = ImGui::IsItemClicked(ImGuiMouseButton_Left), rightClick = ImGui::IsItemClicked(ImGuiMouseButton_Right);
	dl->AddImage(pic.as<ImTextureID>(), o, ImVec2(o.x + w, o.y + h));
	float sx = (float)w / m->width(), sy = (float)h / m->height();
	auto at = [&](float x, float y) { return ImVec2(o.x + (x - m->x()) * sx, o.y + (y - m->y()) * sy); };
	ImVec2 mouse = ImGui::GetIO().MousePos;
	float wx = m->x() + (mouse.x - o.x) / sx, wy = m->y() + (mouse.y - o.y) / sy;

	auto [lo, hi] = rangeOf(m, s_measure);
	if (s_germ >= 0) { // germs: from none to the most in a room (0-255)
		float peak = 0.0f;
		for (auto& r : m->rooms)
			peak = std::max(peak, germsOf(*r));
		lo = 0.0f;
		hi = std::max(peak, 5.0f);
	} else if (s_scent >= 0) { // a scent: from none to the strongest room (levels 0-1, shown as 0-100)
		float peak = 0.0f;
		for (auto& r : m->rooms)
			peak = std::max(peak, scentOf(*r, s_scent));
		lo = 0.0f;
		hi = std::max(peak, 0.05f) * 100.0f;
	}
	auto shown = [&](const Room& r) { return s_germ >= 0 ? germsOf(r) : s_scent >= 0 ? scentOf(r, s_scent) * 100.0f : roomValue(r, s_measure); };
	std::shared_ptr<Room> under;
	dl->PushClipRect(o, ImVec2(o.x + w, o.y + h), true);
	for (auto& r : m->rooms) {
		ImVec2 p[4] = {at(r->x_left, r->y_left_ceiling), at(r->x_right, r->y_right_ceiling), at(r->x_right, r->y_right_floor), at(r->x_left, r->y_left_floor)};
		float v = (shown(*r) - lo) / (hi - lo) * 255.0f;
		dl->AddConvexPolyFilled(p, 4, colorFor(v, 140));
		if (hovered && r->containsPoint(wx, wy))
			under = r;
	}
	if (under) {
		ImVec2 p[4] = {at(under->x_left, under->y_left_ceiling), at(under->x_right, under->y_right_ceiling), at(under->x_right, under->y_right_floor), at(under->x_left, under->y_left_floor)};
		dl->AddPolyline(p, 4, IM_COL32(255, 255, 255, 255), ImDrawFlags_Closed, 1.5f);
	}

	if (s_showFood)
		for (auto& a : world.agents)
			if (isLoose(a.get()) && isFood(a.get())) {
				ImVec2 c = at(a->x + a->getWidth() / 2.0f, a->y + a->getHeight() / 2.0f);
				dl->AddRectFilled(ImVec2(c.x - 1.5f, c.y - 1.5f), ImVec2(c.x + 1.5f, c.y + 1.5f), IM_COL32(150, 255, 120, 255));
			}

	for (auto& p : s_pins)
		if (auto r = p.room.lock()) {
			float cy = (std::min(r->y_left_ceiling, r->y_right_ceiling) + std::max(r->y_left_floor, r->y_right_floor)) / 2.0f;
			drawPin(dl, at((r->x_left + r->x_right) / 2.0f, cy), p.color);
		}

	std::shared_ptr<Agent> creatureUnder;
	if (s_showCreatures) {
		Agent* selected = world.selectedcreature.get();
		for (auto& a : KitLivingCreatures()) {
			ImVec2 c = at(a->x + a->getWidth() / 2.0f, a->y + a->getHeight() / 2.0f);
			std::string kind = KitSpeciesWord(a.get());
			ImU32 col = kind == "grendel" ? IM_COL32(120, 220, 90, 255) : kind == "ettin" ? IM_COL32(240, 160, 60, 255) : IM_COL32(255, 240, 200, 255);
			dl->AddCircleFilled(c, 4.5f, col, 12);
			dl->AddCircle(c, 4.5f, IM_COL32(0, 0, 0, 255), 12, 1.5f);
			if (a.get() == selected)
				dl->AddCircle(c, 8, IM_COL32(255, 255, 255, 255), 16, 1.5f);
			if (s_germ >= 0 && world.history)
				if (auto* ca = dynamic_cast<CreatureAgent*>(a.get()))
					if (Disease::isIll(world.history->findMoniker(ca->getCreature()->getGenome())))
						dl->AddCircle(c, 6.5f, IM_COL32(255, 60, 60, 255), 14, 2.0f);
			float dx = mouse.x - c.x, dy = mouse.y - c.y;
			if (hovered && dx * dx + dy * dy < 49)
				creatureUnder = a;
		}
	}
	dl->PopClipRect();

	if (creatureUnder) {
		ImGui::SetTooltip("%s (%s)\nClick to go to it", KitCreatureName(creatureUnder.get()).c_str(), KitSpeciesWord(creatureUnder.get()));
		if (leftClick)
			KitGoTo(creatureUnder);
	} else if (under) {
		std::string tip = roomTitle(*under) + "\n";
		for (int k = 0; k < MEASURES; k++) {
			if (k == WIND && !wind)
				continue;
			float v = roomValue(*under, k);
			tip += fmt::format("\n{}: {}", MEASURE_NAME[k], valueText(v, k));
		}
		std::string scents;
		for (int ch = 0; ch <= Pheromones::channelCount(); ch++)
			if (float sv = scentOf(*under, ch); sv >= 0.01f)
				scents += fmt::format("\n  {}: {:.0f}", scentName(ch), sv * 100.0f);
		if (!scents.empty())
			tip += "\nScents (out of 100):" + scents;
		std::string germs;
		for (auto& st : Disease::strains())
			if (float g = Disease::germsIn((int)under->id, st.id); g >= 0.5f)
				germs += fmt::format("\n  {}: {:.0f}", st.name(), g);
		if (!germs.empty())
			tip += "\nGerms:" + germs;
		Pin* p = pinOf(under);
		tip += p ? "\n\nClick to go there; right-click to unpin it" : s_pins.size() < 7 ? "\n\nClick to go there; right-click to pin it for the Monitor" : "\n\nClick to go there (seven rooms are pinned already)";
		ImGui::SetTooltip("%s", tip.c_str());
		if (leftClick)
			goToRoom(*under);
		if (rightClick)
			togglePin(under);
	}

	ImGui::BeginGroup();
	legend(s_scent >= 0 || s_germ >= 0 ? LIGHT : s_measure, 260, lo, hi);
	ImGui::EndGroup();
	ImGui::SameLine(0, 24);
	ImGui::BeginGroup();
	ImGui::TextDisabled(s_germ >= 0 ? "none to the room with the most germs" : s_scent >= 0 ? "none to the strongest room (out of 100)" : "lowest room to highest");
	ImGui::TextDisabled(s_germ >= 0 ? "changes as germs grow, die and spread"
			: s_scent == Pheromones::channelCount() ? "changes as food comes and goes"
			: s_scent >= 0 ? "changes as creatures give it off"
			: s_measure == TEMPERATURE ? "changes as heat spreads"
			: s_measure == LIGHT ? "changes with day and night outdoors"
			: (s_measure == ORGANIC || s_measure == INORGANIC) ? "changes as things rot, break down and grow"
			: "stays as the world began");
	ImGui::EndGroup();
	ImGui::EndGroup();
}

// ---- the monitor ------------------------------------------------------------

void graph(int m, float W, float H) {
	ImDrawList* dl = ImGui::GetWindowDrawList();
	const float axisW = 56;
	ImVec2 o = ImGui::GetCursorScreenPos();
	ImVec2 g0(o.x + axisW, o.y + 6), g1(o.x + W, o.y + H);
	ImGui::Dummy(ImVec2(W, H + 20));
	dl->AddRectFilled(g0, g1, IM_COL32(30, 33, 41, 255));
	// the range: the values seen, with some room, at least 40 wide
	float lo = 255, hi = 0;
	for (auto& p : s_pins)
		for (auto& s : p.history)
			lo = std::min(lo, s[m]), hi = std::max(hi, s[m]);
	if (lo > hi)
		lo = 0, hi = 255;
	float mid = (lo + hi) / 2, half = std::max(20.0f, (hi - lo) / 2 * 1.2f);
	lo = std::max(0.0f, mid - half), hi = std::min(255.0f, mid + half);
	if (hi - lo < 40) // clipped at an end of the scale
		lo <= 0 ? hi = 40.0f : lo = 215.0f;
	auto yOf = [&](float v) { return g1.y - (v - lo) / (hi - lo) * (g1.y - g0.y); };
	for (int i = 0; i <= 4; i++) {
		float v = lo + (hi - lo) * i / 4;
		float y = yOf(v);
		if (i > 0 && i < 4)
			dl->AddLine(ImVec2(g0.x, y), ImVec2(g1.x, y), IM_COL32(48, 52, 64, 255));
		std::string t = m == TEMPERATURE ? degrees(v) : fmt::format("{:.0f}", v);
		ImVec2 ts = ImGui::CalcTextSize(t.c_str());
		dl->AddText(ImVec2(g0.x - 6 - ts.x, y - ts.y / 2), IM_COL32(150, 156, 170, 255), t.c_str());
	}
	dl->AddText(ImVec2(g0.x, g1.y + 3), IM_COL32(150, 156, 170, 255), "15 minutes ago");
	ImVec2 nw = ImGui::CalcTextSize("now");
	dl->AddText(ImVec2(g1.x - nw.x, g1.y + 3), IM_COL32(150, 156, 170, 255), "now");
	float step = (g1.x - g0.x) / (SAMPLES - 1);
	for (auto& p : s_pins) {
		std::vector<ImVec2> pts;
		size_t n = p.history.size();
		for (size_t i = 0; i < n; i++)
			pts.push_back(ImVec2(g1.x - (n - 1 - i) * step, yOf(p.history[i][m])));
		if (pts.size() == 1)
			dl->AddCircleFilled(pts[0], 3, PIN_COLOR[p.color]);
		else
			dl->AddPolyline(pts.data(), (int)pts.size(), PIN_COLOR[p.color], 0, 2.0f);
	}
}

void MonitorPage() {
	bool wind = worldHasWind();
	if (s_measure == WIND && !wind)
		s_measure = TEMPERATURE;
	// the time of day
	static const char* const PARTS[5] = {"Dawn", "Morning", "Midday", "Dusk", "Night"};
	unsigned int left = 3600 - world.worldtickcount % 3600;
	float minutes = left * world.ticktime / 60000.0f;
	std::string in = minutes < 1 ? "under a minute" : fmt::format("{:.0f} minute{}", minutes, std::lround(minutes) == 1 ? "" : "s");
	ImGui::Text("%s, %s. %s in %s.", PARTS[world.timeofday % 5], KitDateText(world.worldtickcount).c_str(), PARTS[(world.timeofday + 1) % 5], in.c_str());
	ImGui::Separator();

	if (s_pins.empty()) {
		KitNoteColored(ImGui::GetStyle().Colors[ImGuiCol_Text], "No rooms are pinned. On the Map page, right-click a room to pin it here (up to seven) and see how it changes.");
		Agent* sel = world.selectedcreature.get();
		if (sel && world.map) {
			auto r = world.map->roomAt(sel->x + sel->getWidth() / 2.0f, sel->y + sel->getHeight() / 2.0f);
			if (r && ImGui::Button(fmt::format("Pin the room {} is in", KitCreatureName(sel)).c_str()))
				togglePin(r);
		}
		return;
	}
	ImGui::BeginGroup();
	ImGui::TextDisabled("Graph");
	measureChooser(wind);
	ImGui::EndGroup();
	ImGui::SameLine(0, 16);
	ImGui::BeginGroup();
	graph(s_measure, 520, 200);
	ImGui::EndGroup();

	ImGui::Spacing();
	int remove = -1;
	if (ImGui::BeginTable("pins", 4, ImGuiTableFlags_SizingFixedFit)) {
		for (size_t i = 0; i < s_pins.size(); i++) {
			auto& p = s_pins[i];
			auto r = p.room.lock();
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImVec2 c = ImGui::GetCursorScreenPos();
			drawPin(ImGui::GetWindowDrawList(), ImVec2(c.x + 8, c.y + 8), p.color);
			ImGui::Dummy(ImVec2(16, 16));
			ImGui::TableNextColumn();
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(r ? roomTitle(*r).c_str() : "(gone)");
			ImGui::TableNextColumn();
			if (!p.history.empty()) {
				float now = p.history.back()[s_measure], then = p.history.front()[s_measure];
				std::string change;
				if (p.history.size() > 1 && std::abs(now - then) >= 1)
					change = s_measure == TEMPERATURE ? fmt::format("  ({}{:.1f}° in {:.0f} min)", now > then ? "+" : "", (now - then) * 80.0f / 255.0f * (KitFahrenheit() ? 1.8f : 1.0f), (p.history.size() - 1) * SAMPLE_TICKS * world.ticktime / 60000.0f)
													  : fmt::format("  ({}{:.0f})", now > then ? "+" : "", now - then);
				ImGui::Text("%s%s", valueText(now, s_measure).c_str(), change.c_str());
			}
			ImGui::TableNextColumn();
			ImGui::PushID((int)i);
			if (r && ImGui::SmallButton("Go"))
				goToRoom(*r);
			ImGui::SameLine();
			if (ImGui::SmallButton("Unpin"))
				remove = (int)i;
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	if (remove >= 0) {
		s_pins.erase(s_pins.begin() + remove);
		savePins();
	}
}

// ---- the census -------------------------------------------------------------

void CensusPage() {
	// the creatures
	std::map<std::string, int> kinds;
	for (auto& a : KitLivingCreatures())
		kinds[KitSpeciesWord(a.get())]++;
	std::string line;
	for (auto& [k, n] : kinds)
		line += fmt::format("{}{} {}{}", line.empty() ? "" : ", ", n, k, n == 1 ? "" : "s");
	ImGui::Text("Living creatures: %s", line.empty() ? "none" : line.c_str());
	// eggs: the creatures' own (the Hatchery's list) and the grendel and
	// ettin eggs the world makes for itself (2 5 3, 2 5 4), apart from the
	// empty shells left after hatching
	{
		int ownEggs = (int)KitUnhatchedEggs().size(), worldEggs = 0, shells = 0;
		for (auto& a : world.agents) {
			if (!a || a->isDying() || a->family != 2 || a->genus != 5)
				continue;
			auto* sp = dynamic_cast<SpritePart*>(a->part(0));
			if (sp && sp->getPose() == 7)
				shells++;
			else if (a->species == 3 || a->species == 4)
				worldEggs++;
		}
		KitNote("Creature eggs: %d to hatch (the Hatchery's list), %d grendel and ettin egg%s the world made itself, %d empty shell%s.",
			ownEggs, worldEggs, worldEggs == 1 ? "" : "s", shells, shells == 1 ? "" : "s");
	}
	ImGui::Spacing();

	// everything else, by the game's own kinds of thing
	struct Count {
		int all = 0, food = 0;
	};
	std::map<int, Count> counts;
	int other = 0;
	std::vector<Agent*> food;
	for (auto& a : world.agents) {
		if (!isLoose(a.get()) || a->family < 2 || a->family > 3)
			continue;
		bool f = isFood(a.get());
		if (f)
			food.push_back(a.get());
		if (a->category < 2 || a->category > 35) {
			other++;
			continue;
		}
		auto& c = counts[a->category];
		c.all++;
		c.food += f;
	}
	std::vector<std::pair<int, int>> order; // count, category
	for (auto& [cat, c] : counts)
		order.push_back({c.all, cat});
	std::sort(order.rbegin(), order.rend());
	ImGui::TextDisabled("Everything else in the world, by the kinds creatures know:");
	const float W = 600;
	if (ImGui::BeginTable("census", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
			ImVec2(W, std::min(420.0f, ((order.size() + 1) / 2 + 1) * (ImGui::GetTextLineHeight() + 4) + 6)))) {
		ImGui::TableSetupColumn("Kind");
		ImGui::TableSetupColumn("Number", ImGuiTableColumnFlags_WidthFixed, 60);
		ImGui::TableSetupColumn("To eat", ImGuiTableColumnFlags_WidthFixed, 60);
		ImGui::TableSetupColumn("Kind ");
		ImGui::TableSetupColumn("Number ", ImGuiTableColumnFlags_WidthFixed, 60);
		ImGui::TableSetupColumn("To eat ", ImGuiTableColumnFlags_WidthFixed, 60);
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableHeadersRow();
		size_t rows = (order.size() + 1) / 2;
		for (size_t i = 0; i < rows; i++) {
			ImGui::TableNextRow();
			for (size_t half = 0; half < 2; half++) {
				size_t k = i + half * rows;
				if (k >= order.size()) {
					ImGui::TableNextColumn();
					ImGui::TableNextColumn();
					ImGui::TableNextColumn();
					continue;
				}
				auto& c = counts[order[k].second];
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(KitCategoryNoun(order[k].second).c_str());
				ImGui::TableNextColumn();
				ImGui::Text("%d", c.all);
				ImGui::TableNextColumn();
				if (c.food)
					ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.5f, 1), "%d", c.food);
			}
		}
		ImGui::EndTable();
	}
	if (other)
		ImGui::TextDisabled("and %d other thing%s (scenery and parts of machines)", other, other == 1 ? "" : "s");
	ImGui::Spacing();

	// the food, west to east
	MetaRoom* m = shownMetaRoom();
	int w = 0, h = 0;
	Texture pic = m ? KitMapPicture(m, w, h) : Texture();
	if (!m || !pic)
		return;
	ImGui::Text("Food to eat: %zu. Where it is, west to east:", food.size());
	const int BANDS = 22;
	std::array<int, BANDS> per{};
	for (Agent* a : food) {
		float cx = a->x + a->getWidth() / 2.0f;
		if (cx < m->x() || cx >= m->x() + m->width())
			continue;
		per[std::clamp((int)((cx - m->x()) / m->width() * BANDS), 0, BANDS - 1)]++;
	}
	int most = std::max(1, *std::max_element(per.begin(), per.end()));
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 o = ImGui::GetCursorScreenPos();
	const float stripH = 60, barsH = 70, MW = W;
	// a strip of the map for where is where, then the bars below it
	dl->AddImage(pic.as<ImTextureID>(), o, ImVec2(o.x + MW, o.y + stripH));
	float bw = MW / BANDS;
	ImVec2 mouse = ImGui::GetIO().MousePos;
	std::string tip;
	for (int b = 0; b < BANDS; b++) {
		float x0 = o.x + b * bw, x1 = x0 + bw - 2;
		float top = o.y + stripH + 4 + barsH * (1 - (float)per[b] / most);
		ImU32 col = per[b] <= 2 ? IM_COL32(230, 90, 70, 255) : per[b] <= 6 ? IM_COL32(230, 200, 70, 255) : IM_COL32(120, 220, 110, 255);
		if (per[b] == 0)
			dl->AddRectFilled(ImVec2(x0, o.y + stripH + 4 + barsH - 2), ImVec2(x1, o.y + stripH + 4 + barsH), col);
		else
			dl->AddRectFilled(ImVec2(x0, top), ImVec2(x1, o.y + stripH + 4 + barsH), col);
		if (mouse.x >= x0 && mouse.x < x0 + bw && mouse.y >= o.y && mouse.y < o.y + stripH + 4 + barsH) {
			dl->AddRect(ImVec2(x0, o.y), ImVec2(x1, o.y + stripH), IM_COL32(255, 255, 255, 200));
			float midx = m->x() + (b + 0.5f) * m->width() / BANDS;
			tip = fmt::format("{} thing{} to eat here ({})", per[b], per[b] == 1 ? "" : "s", KitWhereAt(midx, m->y() + m->height() / 2.0f));
		}
	}
	ImGui::Dummy(ImVec2(MW, stripH + 4 + barsH + 4));
	if (!tip.empty())
		ImGui::SetTooltip("%s", tip.c_str());
	ImGui::TextDisabled("Red: two or fewer things to eat in that part of the world.");
}

} // namespace

void EcologyKitTick() {
	if (engine.version != 2 || !world.map)
		return;
	// another world, or this one reloaded: its rooms are new, and the pins
	// come back from the world's saved list
	auto& rooms = world.map->getRooms();
	if (rooms.empty())
		return;
	bool differ = world.history && world.history->ecologypins.size() != s_pins.size();
	for (size_t i = 0; world.history && !differ && i < s_pins.size(); i++)
		differ = world.history->ecologypins[i] != std::make_pair(s_pins[i].id, s_pins[i].color);
	if (differ || s_pinsWorld.expired() || s_pinsWorld.lock() != rooms.front()) {
		s_pinsWorld = rooms.front();
		s_pins.clear();
		if (world.history)
			for (auto& [id, color] : world.history->ecologypins)
				for (auto& r : rooms)
					if (r->id == id && !pinOf(r) && s_pins.size() < 7)
						addPin(r, std::clamp(color, 1, 7));
	}
	// developer aid: OPENC2E_ECO_PINS="19,120,200" pins those rooms at the start
	static bool s_testPins = true;
	if (s_testPins && world.worldtickcount > 5) {
		s_testPins = false;
		if (const char* ids = getenv("OPENC2E_ECO_PINS"))
			for (const char* p = ids; *p;) {
				unsigned int id = (unsigned int)strtoul(p, const_cast<char**>(&p), 10);
				for (auto& r : world.map->getRooms())
					if (r->id == id && !pinOf(r))
						togglePin(r);
				while (*p == ',' || *p == ' ')
					p++;
			}
	}
	static unsigned int last = 0;
	if (world.worldtickcount == last || world.worldtickcount % SAMPLE_TICKS != 0)
		return;
	last = world.worldtickcount;
	for (auto& p : s_pins)
		if (auto r = p.room.lock()) {
			p.history.push_back(readings(*r));
			while (p.history.size() > SAMPLES)
				p.history.pop_front();
		}
}

void DrawEcologyKit() {
	// developer aid: OPENC2E_ECO_MEASURE=n (0 light ... 5 inorganic) and
	// OPENC2E_ECO_FOOD=1 set the map's choices at the start
	static bool s_test = true;
	if (s_test) {
		s_test = false;
		if (const char* m = getenv("OPENC2E_ECO_MEASURE"))
			s_measure = std::clamp(atoi(m), 0, MEASURES - 1);
		s_showFood = getenv("OPENC2E_ECO_FOOD") != nullptr;
		// OPENC2E_ECO_SCENT=name shows that scent (and tints the world)
		if (const char* sc = getenv("OPENC2E_ECO_SCENT"))
			for (int ch = 0; ch <= Pheromones::channelCount(); ch++)
				if (scentName(ch) == sc)
					s_scent = ch, s_scentOnWorld = true;
		// OPENC2E_ECO_GERMS=n shows strain n's germs (0 every strain)
		if (const char* g = getenv("OPENC2E_ECO_GERMS"))
			s_germ = atoi(g);
	}
	if (!world.map) {
		KitNote("No world is loaded.");
		return;
	}
	if (ImGui::BeginTabBar("ecotabs")) {
		if (KitTab("Map")) {
			MapPage();
			ImGui::EndTabItem();
		}
		if (KitTab("Monitor")) {
			MonitorPage();
			ImGui::EndTabItem();
		}
		if (KitTab("Census")) {
			CensusPage();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

} // namespace Openc2eImgui
