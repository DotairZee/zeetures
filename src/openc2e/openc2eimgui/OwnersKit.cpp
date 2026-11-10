// The Owner's Kit (Creatures 2): birth registration (a name, the birth
// certificate), photographs, the family and the life log of births, deaths
// and pregnancies (also announced as notices on the status bar). C2's own art from the player's
// Applet Data; our layout. Names, registration, birth times, photos and
// notes are kept with the world (WorldSave, monikerData).

#include "Agent.h"
#include "C2Kits.h"
#include "C2KitsInternal.h"
#include "Camera.h"
#include "Engine.h"
#include "World.h"
#include "common/backend/Backend.h"
#include "creatures/CreatureAgent.h"
#include "creatures/oldCreature.h"
#include "fileformats/ImageUtils.h"
#include "historyManager.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fmt/format.h>
#include <imgui.h>
#include <map>

using namespace ImGuiUtils;
namespace fs = std::filesystem;

namespace Openc2eImgui {

static const ImU32 INK = IM_COL32(74, 46, 22, 255);
static const char* s_stages[] = {"baby", "child", "adolescent", "youth", "adult", "old", "senile"};

static const char* stageWord(int st) {
	return st >= 0 && st < 7 ? s_stages[st] : "?";
}

// ---- Registration ------------------------------------------------------------------

static void RegistrationPage() {
	if (!KitNeedsCreature("Applet Data/Owner.bmp"))
		return;
	Agent* a = KitCreatureAgent();
	c2Creature* c = KitCreature();
	monikerData* m = KitRecordsOf(a);
	if (!m) {
		ImGui::TextDisabled("This creature has no records.");
		return;
	}
	Texture cert = KitPicture("Applet Data/Certificate.bmp");
	ImVec2 p = ImGui::GetCursorScreenPos();
	DrawTexture(cert, p);
	ImDrawList* dl = ImGui::GetWindowDrawList();

	// the name, written on the certificate
	static char name[64];
	static monikerData* editing = nullptr;
	if (editing != m) {
		editing = m;
		std::snprintf(name, sizeof(name), "%s", m->name.c_str());
	}
	dl->AddText(p + ImVec2(34, 100), INK, "This is to certify the birth of");
	ImGui::SetCursorScreenPos(p + ImVec2(30, 118));
	ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(255, 240, 210, 90));
	ImGui::PushStyleColor(ImGuiCol_Text, INK);
	ImGui::SetNextItemWidth(200);
	if (ImGui::InputTextWithHint("##name", "type a name", name, sizeof(name)))
		m->name = name;
	ImGui::PopStyleColor(2);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Its name (kept with the world)");

	std::string what = fmt::format("a {} {}", c->isFemale() ? "female" : "male", KitSpeciesWord(a));
	dl->AddText(p + ImVec2(34, 148), INK, what.c_str());
	std::string born = fmt::format("{} {}", m->bornknown ? "born" : "first seen", KitDateText(m->borntick));
	dl->AddText(p + ImVec2(34, 166), INK, born.c_str());
	std::string age = "age " + KitAgeText(KitAgeTicks(*m));
	age = age.substr(0, age.find(" ("));		  // game time on the certificate
	dl->AddText(p + ImVec2(34, 184), INK, age.c_str());
	int st = m->died ? m->diedstage : (int)c->getStage();
	std::string stage = fmt::format("life stage: {}", stageWord(st));
	dl->AddText(p + ImVec2(34, 202), INK, stage.c_str());
	std::string mon = "moniker " + world.history->findMoniker(c->getGenome());
	dl->AddText(p + ImVec2(34, 220), IM_COL32(74, 46, 22, 150), mon.c_str());
	if (m->died) // the details go below (the seal leaves no room)
		dl->AddText(p + ImVec2(34, 238), IM_COL32(120, 20, 20, 255), "DECEASED");

	if (m->registered) {
		dl->AddText(p + ImVec2(34, 262), IM_COL32(160, 20, 20, 255), "REGISTERED");
	} else {
		ImGui::SetCursorScreenPos(p + ImVec2(30, 258));
		if (ImGui::Button("Register this birth"))
			m->registered = true;
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Seal the certificate. The name can still be changed.");
	}
	ImGui::SetCursorScreenPos(p + ImVec2(0, cert ? cert.height + 4 : 324));
	if (m->died) {
		std::string age = KitAgeText(KitAgeTicks(*m));
		age = age.substr(0, age.find(" ("));
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + (cert ? cert.width : 320));
		ImGui::TextColored(ImVec4(0.85f, 0.45f, 0.45f, 1), "Died %s, aged %s%s, a%s %s.", KitDateText(m->diedtick).c_str(), m->bornknown ? "" : "at least ",
			age.c_str(), st == 4 || st == 5 ? "n" : "", stageWord(st));
		ImGui::PopTextWrapPos();
	}

	// notes
	static char notes[1024];
	static monikerData* noting = nullptr;
	if (noting != m) {
		noting = m;
		std::snprintf(notes, sizeof(notes), "%s", m->notes.c_str());
	}
	ImGui::TextDisabled("Notes");
	if (ImGui::InputTextMultiline("##notes", notes, sizeof(notes), ImVec2(320, 60)))
		m->notes = notes;
}

// ---- Photographs --------------------------------------------------------------------

static constexpr int PHOTO_W = 240, PHOTO_H = 180;

class PhotoCamera : public Camera {
  public:
	unsigned int getWidth() const override { return PHOTO_W; }
	unsigned int getHeight() const override { return PHOTO_H; }
};

static std::map<std::string, Texture> s_photos; // by full path

static Texture photoTexture(const fs::path& path) {
	auto key = path.string();
	auto it = s_photos.find(key);
	if (it != s_photos.end())
		return it->second;
	Texture t;
	std::error_code ec;
	if (fs::exists(path, ec)) {
		try {
			MultiImage im = ImageUtils::ReadImage(path);
			if (!im.empty())
				t = get_backend()->createTextureFromImage(im[0]);
		} catch (std::exception&) {
		}
	}
	s_photos[key] = t;
	return t;
}

// photo_<world tick>_<n>.bmp: when it was taken
static unsigned int photoTick(const std::string& file) {
	unsigned int t = 0;
	std::sscanf(fs::path(file).filename().string().c_str(), "photo_%u", &t);
	return t;
}

static std::string takePhoto(Agent* a, monikerData& m) {
	if (engine.currentworlddir.empty())
		return "This world has no folder to keep photos in.";
	static std::unique_ptr<PhotoCamera> cam;
	static std::shared_ptr<RenderTarget> target;
	if (!cam)
		cam = std::make_unique<PhotoCamera>();
	if (!target)
		target = get_backend()->newRenderTarget(PHOTO_W, PHOTO_H);
	float cx = a->x + a->getWidth() / 2.0f, cy = a->y + a->getHeight() / 2.0f;
	cam->moveToGlobal((int)cx - PHOTO_W / 2, (int)cy - PHOTO_H / 2 - 10, jump);
	world.drawWorld(cam.get(), target.get(), true, a); // no other creature in the picture
	fs::path dir = engine.currentworlddir / "photos";
	std::error_code ec;
	fs::create_directories(dir, ec);
	std::string file;
	for (int n = 0; n < 1000; n++) {
		file = fmt::format("photo_{}_{}.bmp", world.worldtickcount, n);
		if (!fs::exists(dir / file, ec))
			break;
	}
	if (!target->saveToFile((dir / file).string()))
		return "Couldn't save the photo.";
	m.photos.push_back("photos/" + file);
	return "Photo taken.";
}

static void PhotosPage() {
	if (!KitNeedsCreature("Applet Data/Owner.bmp"))
		return;
	Agent* a = KitCreatureAgent();
	monikerData* m = KitRecordsOf(a);
	if (!m)
		return;
	static std::string message;
	static int big = -1;
	static monikerData* shown = nullptr;
	if (shown != m) {
		shown = m;
		big = -1;
		message.clear();
	}
	// developer aid: OPENC2E_TEST_PHOTO=1 takes one photo at once
	static bool testPhoto = getenv("OPENC2E_TEST_PHOTO") != nullptr;
	if (testPhoto) {
		testPhoto = false;
		message = takePhoto(a, *m);
		big = (int)m->photos.size() - 1;
	}
	if (ImGui::Button("Take a photo"))
		message = takePhoto(a, *m), big = (int)m->photos.size() - 1;
	ImGui::SameLine();
	ImGui::Text("%s: %d photo%s", KitCreatureName(a).c_str(), (int)m->photos.size(), m->photos.size() == 1 ? "" : "s");
	if (!message.empty()) {
		ImGui::SameLine();
		ImGui::TextDisabled("%s", message.c_str());
	}

	// the album: a film strip of thumbnails, three to a row
	auto& film = KitSprites("Applet Data/film.s16");
	const float cellW = 136, thumbW = 120, thumbH = 90;
	ImVec2 p = ImGui::GetCursorScreenPos();
	ImDrawList* dl = ImGui::GetWindowDrawList();
	int rows = ((int)m->photos.size() + 2) / 3;
	float rowH = thumbH + 36;
	dl->AddRectFilled(p, p + ImVec2(cellW * 3, std::max(1, rows) * rowH), IM_COL32(18, 18, 20, 255));
	for (size_t i = 0; i < m->photos.size(); i++) {
		ImVec2 cell = p + ImVec2(cellW * (i % 3), rowH * (i / 3));
		if (film.size() > 2) {
			DrawTexture(film[2], cell);
			DrawTexture(film[2], cell + ImVec2(0, rowH - 16));
		}
		Texture t = photoTexture(engine.currentworlddir / m->photos[i]);
		ImVec2 at = cell + ImVec2((cellW - thumbW) / 2, 18);
		if (t)
			dl->AddImage(t.as<ImTextureID>(), at, at + ImVec2(thumbW, thumbH));
		else
			dl->AddText(at + ImVec2(10, 36), IM_COL32(160, 160, 160, 255), "(missing)");
		ImGui::SetCursorScreenPos(at);
		ImGui::PushID((int)i);
		if (ImGui::InvisibleButton("photo", ImVec2(thumbW, thumbH)))
			big = (int)i;
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Taken %s", KitDateText(photoTick(m->photos[i])).c_str());
		ImGui::PopID();
		if ((int)i == big)
			dl->AddRect(at - 2, at + ImVec2(thumbW, thumbH) + 2, IM_COL32(255, 220, 120, 255), 0, 0, 2);
	}
	ImGui::SetCursorScreenPos(p + ImVec2(0, std::max(1, rows) * rowH + 4));
	if (m->photos.empty())
		ImGui::TextDisabled("No photos yet. The camera frames the creature where it stands, with no other creature in the way.");

	// the chosen photo, full size
	if (big >= 0 && big < (int)m->photos.size()) {
		Texture t = photoTexture(engine.currentworlddir / m->photos[big]);
		if (t)
			ImGui::Image(t.as<ImTextureID>(), ImVec2(t.width, t.height));
		ImGui::TextDisabled("Taken %s", KitDateText(photoTick(m->photos[big])).c_str());
		ImGui::SameLine();
		if (ImGui::SmallButton("Delete..."))
			ImGui::OpenPopup("delete photo");
		if (ImGui::BeginPopup("delete photo")) {
			ImGui::Text("Delete this photo for good?");
			if (ImGui::Button("Delete it")) {
				fs::path f = engine.currentworlddir / m->photos[big];
				std::error_code ec;
				fs::remove(f, ec);
				s_photos.erase(f.string());
				m->photos.erase(m->photos.begin() + big);
				big = -1;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Keep it"))
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}
	}
}

// ---- Life log, and the notices on the status bar ------------------------------------

static const char* kindWord(int kind) {
	switch (kind) {
		case lifeLogEntry::birth: return "Born";
		case lifeLogEntry::death: return "Died";
		default: return "Pregnant";
	}
}

static const char* genusWord(int genus) {
	switch (genus) {
		case 2: return "grendel";
		case 3: return "ettin";
		default: return "norn";
	}
}

// its name now (it may have been named since), else then, else "Unnamed norn"
static std::string entryName(const lifeLogEntry& e) {
	if (world.history->hasMoniker(e.moniker) && !world.history->getMoniker(e.moniker).name.empty())
		return world.history->getMoniker(e.moniker).name;
	if (!e.name.empty())
		return e.name;
	return fmt::format("Unnamed {}", genusWord(e.genus));
}

static std::string entryDetail(const lifeLogEntry& e) {
	std::string who = fmt::format("{} {}", e.female ? "female" : "male", genusWord(e.genus));
	switch (e.kind) {
		case lifeLogEntry::birth: return "a " + who;
		case lifeLogEntry::death: {
			std::string age = KitAgeText(e.age);
			age = age.substr(0, age.find(" ("));
			return fmt::format("{}, aged {}{}, {}{}", who, e.ageknown ? "" : "at least ", age, stageWord(e.stage), e.oldage ? ": old age" : "");
		}
		default: return fmt::format("{}, {}", who, stageWord(e.stage));
	}
}

// the creature's body, if it is still in the world
static std::shared_ptr<Agent> entryBody(const lifeLogEntry& e) {
	if (!world.history->hasMoniker(e.moniker))
		return nullptr;
	Agent* a = world.history->getMoniker(e.moniker).owner.get();
	if (!a || a->isDying() || !dynamic_cast<CreatureAgent*>(a))
		return nullptr;
	return AgentRef(a).lock();
}

static void followEntry(const lifeLogEntry& e) {
	auto body = entryBody(e);
	if (!body) { // gone: its line in the log is all there is
		OpenC2KitAt("owners", "Life log");
		return;
	}
	KitGoTo(body);
	if (e.kind == lifeLogEntry::pregnancy)
		OpenC2KitAt("breeders", "Reproductive");
	else
		OpenC2KitAt("owners", "Registration");
}

static void LifeLogPage() {
	auto& log = world.history->lifelog;
	if (log.empty()) {
		ImGui::TextDisabled("No births, deaths or pregnancies yet. They are logged here as they happen.");
		return;
	}
	ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;
	if (ImGui::BeginTable("log", 5, flags, ImVec2(800, 300))) {
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("When", 0, 180);
		ImGui::TableSetupColumn("", 0, 60);
		ImGui::TableSetupColumn("Who", 0, 120);
		ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("", 0, 30);
		ImGui::TableHeadersRow();
		for (int i = (int)log.size() - 1; i >= 0; i--) {
			auto& e = log[i];
			ImGui::PushID(i);
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(KitDateText(e.tick).c_str());
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(kindWord(e.kind));
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(entryName(e).c_str());
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(entryDetail(e).c_str());
			ImGui::TableNextColumn();
			if (entryBody(e) && ImGui::SmallButton("Go"))
				followEntry(e);
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
}

static std::vector<lifeLogEntry> s_notices; // shown on the status bar until clicked or dismissed

void DrawLifeNotices(float room) {
	if (engine.version != 2 || !world.history)
		return;
	static historyManager* seen = nullptr;
	if (seen != world.history.get()) { // another world
		seen = world.history.get();
		s_notices.clear();
	}
	for (auto& e : world.history->notices)
		s_notices.push_back(e);
	world.history->notices.clear();
	// developer aid: OPENC2E_TEST_NOTICES=1 announces one of each for the
	// selected creature (not logged); =follow also follows the last one
	static bool test = getenv("OPENC2E_TEST_NOTICES") != nullptr;
	if (test && world.selectedcreature) {
		test = false;
		std::string mon = world.history->findMoniker(AgentRef(world.selectedcreature));
		for (int k = 0; k < 3; k++) {
			lifeLogEntry e;
			e.kind = k;
			e.tick = world.worldtickcount;
			e.moniker = mon;
			e.genus = world.selectedcreature->genus;
			s_notices.push_back(e);
		}
		if (std::string(getenv("OPENC2E_TEST_NOTICES")) == "follow") // as if the pregnancy notice were clicked
			followEntry(s_notices.back());
	}
	// OPENC2E_TEST_DEATH=1: the selected creature dies (logged and announced)
	static bool testDeath = getenv("OPENC2E_TEST_DEATH") != nullptr;
	if (testDeath && world.selectedcreature) {
		testDeath = false;
		if (auto* ca = dynamic_cast<CreatureAgent*>(world.selectedcreature.get()))
			if (ca->getCreature()->isAlive())
				ca->getCreature()->die();
	}
	if (s_notices.empty())
		return;

	// the newest that fit in the room (at most three), the rest behind "+N"
	auto label = [](const lifeLogEntry& e, int i) { return fmt::format("{}: {}##notice{}", kindWord(e.kind), entryName(e), i); };
	const ImGuiStyle& style = ImGui::GetStyle();
	float used = ImGui::CalcTextSize("+99").x + style.FramePadding.x * 2 + style.ItemSpacing.x;
	int first = (int)s_notices.size();
	while (first > 0 && (int)s_notices.size() - first < 3) {
		float w = ImGui::CalcTextSize(label(s_notices[first - 1], first - 1).c_str(), nullptr, true).x + style.FramePadding.x * 2 + style.ItemSpacing.x;
		if (used + w > room && first < (int)s_notices.size())
			break;
		used += w;
		first--;
	}
	if (first > 0) {
		std::string more = fmt::format("+{}##morenotices", first);
		if (ImGui::SmallButton(more.c_str())) {
			s_notices.erase(s_notices.begin(), s_notices.begin() + first);
			OpenC2KitAt("owners", "Life log");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%d older notice%s: click to see the life log", first, first == 1 ? "" : "s");
	}
	int remove = -1;
	for (int i = first; i < (int)s_notices.size(); i++) {
		auto& e = s_notices[i];
		ImVec4 col = e.kind == lifeLogEntry::birth ? ImVec4(0.20f, 0.52f, 0.25f, 1) : e.kind == lifeLogEntry::death ? ImVec4(0.30f, 0.30f, 0.36f, 1)
																										  : ImVec4(0.70f, 0.30f, 0.50f, 1);
		ImGui::PushStyleColor(ImGuiCol_Button, col);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(col.x + 0.12f, col.y + 0.12f, col.z + 0.12f, 1));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(col.x + 0.2f, col.y + 0.2f, col.z + 0.2f, 1));
		if (ImGui::SmallButton(label(e, i).c_str())) {
			followEntry(e);
			remove = i;
		}
		ImGui::PopStyleColor(3);
		if (ImGui::IsItemHovered()) {
			const char* kit = e.kind == lifeLogEntry::pregnancy ? "Breeder's" : "Owner's";
			ImGui::SetTooltip("%s %s\n%s\nClick to go to it and open the %s Kit; right-click to dismiss.",
				kindWord(e.kind), KitDateText(e.tick).c_str(), entryDetail(e).c_str(), kit);
		}
		if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
			remove = i;
	}
	if (remove >= 0)
		s_notices.erase(s_notices.begin() + remove);
}

// ---- Family tree, with each member's photos ---------------------------------------

static monikerData* recordOf(const std::string& mon) {
	return !mon.empty() && world.history->hasMoniker(mon) ? &world.history->getMoniker(mon) : nullptr;
}

static std::shared_ptr<Agent> bodyOf(monikerData* m) {
	Agent* a = m ? m->owner.get() : nullptr;
	if (!a || a->isDying() || !dynamic_cast<CreatureAgent*>(a))
		return nullptr;
	return AgentRef(a).lock();
}

static std::string recordName(monikerData* m) {
	return m->name.empty() ? fmt::format("Unnamed {}", genusWord(m->genus)) : m->name;
}

// "female adult" (at death, for the dead), then its age or when it died
static std::pair<std::string, std::string> recordStatus(monikerData* m) {
	auto body = bodyOf(m);
	c2Creature* c = body ? KitC2Of(body.get()) : nullptr;
	bool female = c ? c->isFemale() : m->gender == 2;
	const char* sex = c || m->gender > 0 ? (female ? "female " : "male ") : "";
	if (m->died)
		return {fmt::format("died: {}{}", sex, stageWord(m->diedstage)), KitDateText(m->diedtick)};
	if (c) {
		std::string age = KitAgeText(KitAgeTicks(*m));
		age = age.substr(0, age.find(" ("));
		return {fmt::format("{}{}", sex, stageWord((int)c->getStage())), (m->bornknown ? "age " : "at least ") + age};
	}
	return {"no longer in the world", ""};
}

static std::string fitText(std::string t, float w) {
	if (ImGui::CalcTextSize(t.c_str()).x <= w)
		return t;
	while (!t.empty() && ImGui::CalcTextSize((t + "..").c_str()).x > w)
		t.pop_back();
	return t + "..";
}

static const float CARD_W = 176, THUMB_W = 168, THUMB_H = 126, CARD_H = THUMB_H + 58, CARD_GAP = 14, UNKNOWN_H = 40;
static std::map<std::string, int> s_cardPhoto; // which photo each card shows (-1 the newest)
static std::string s_album; // whose photos the album popup shows
static int s_albumPhoto = 0;
static bool s_openAlbum = false;

// one member of the family: their photo (arrows when there are several;
// click for the album), name, and alive or when they died. Returns the card's
// top-left and bottom-right.
static std::pair<ImVec2, ImVec2> familyCard(const std::string& mon, ImVec2 at, bool self, const char* id) {
	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImU32 gray = IM_COL32(150, 150, 150, 255);
	monikerData* m = recordOf(mon);
	ImVec2 end = at + ImVec2(CARD_W, m ? CARD_H : UNKNOWN_H);
	dl->AddRectFilled(at, end, IM_COL32(30, 30, 34, 255), 4);
	dl->AddRect(at, end, self ? IM_COL32(255, 220, 120, 255) : IM_COL32(90, 90, 100, 255), 4, 0, self ? 2.0f : 1.0f);
	ImVec2 tp = at + ImVec2((CARD_W - THUMB_W) / 2, 4);
	if (!m) { // a short card: nothing known
		dl->AddText(at + ImVec2(CARD_W / 2 - ImGui::CalcTextSize("unknown").x / 2, UNKNOWN_H / 2 - 7), gray, "unknown");
		return {at, end};
	}
	ImGui::PushID(id);
	int n = (int)m->photos.size();
	int& pick = s_cardPhoto[mon];
	if (pick < 0 || pick >= n)
		pick = n - 1; // the newest
	Texture t = n ? photoTexture(engine.currentworlddir / m->photos[pick]) : Texture();
	if (t) {
		dl->AddImage(t.as<ImTextureID>(), tp, tp + ImVec2(THUMB_W, THUMB_H));
	} else {
		dl->AddRectFilled(tp, tp + ImVec2(THUMB_W, THUMB_H), IM_COL32(18, 18, 20, 255));
		const char* none = n ? "(missing)" : "no photo";
		dl->AddText(tp + ImVec2(THUMB_W / 2 - ImGui::CalcTextSize(none).x / 2, THUMB_H / 2 - 7), gray, none);
	}
	ImGui::SetCursorScreenPos(tp);
	if (ImGui::InvisibleButton("photo", ImVec2(THUMB_W, THUMB_H)) && n) {
		s_album = mon;
		s_albumPhoto = pick;
		s_openAlbum = true;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(n ? "%s: click to see the photo%s (%d)" : "%s: no photos yet", recordName(m).c_str(), n == 1 ? "" : "s", n);
	if (n > 1) { // flip through them on the card
		ImGui::SetCursorScreenPos(tp + ImVec2(2, THUMB_H - 20));
		if (ImGui::ArrowButton("prev", ImGuiDir_Left))
			pick = (pick + n - 1) % n;
		std::string count = fmt::format("{}/{}", pick + 1, n);
		ImVec2 cs = ImGui::CalcTextSize(count.c_str());
		ImVec2 cp = tp + ImVec2(THUMB_W / 2 - cs.x / 2, THUMB_H - 17);
		dl->AddRectFilled(cp - ImVec2(3, 1), cp + cs + ImVec2(3, 1), IM_COL32(0, 0, 0, 160), 3);
		dl->AddText(cp, IM_COL32(230, 230, 230, 255), count.c_str());
		ImGui::SetCursorScreenPos(tp + ImVec2(THUMB_W - 22, THUMB_H - 20));
		if (ImGui::ArrowButton("next", ImGuiDir_Right))
			pick = (pick + 1) % n;
	}
	auto body = bodyOf(m);
	bool go = body && !self && !m->died;
	auto [status, when] = recordStatus(m);
	ImU32 sub = m->died ? IM_COL32(210, 120, 120, 255) : gray;
	dl->AddText(at + ImVec2(6, THUMB_H + 8), m->died ? IM_COL32(200, 200, 200, 255) : IM_COL32(240, 240, 240, 255), fitText(recordName(m), CARD_W - (go ? 40 : 12)).c_str());
	dl->AddText(at + ImVec2(6, THUMB_H + 24), sub, fitText(status, CARD_W - 12).c_str());
	dl->AddText(at + ImVec2(6, THUMB_H + 40), sub, fitText(when, CARD_W - 12).c_str());
	if (go) {
		ImGui::SetCursorScreenPos(at + ImVec2(CARD_W - 32, THUMB_H + 6));
		if (ImGui::SmallButton("Go"))
			KitGoTo(body);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Select %s and go to it", recordName(m).c_str());
	}
	ImGui::PopID();
	return {at, end};
}

// a line from the bottom middle of one card to the top middle of another,
// with a bend halfway
static void familyLine(const std::pair<ImVec2, ImVec2>& from, const std::pair<ImVec2, ImVec2>& to) {
	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImU32 col = IM_COL32(170, 150, 110, 255);
	ImVec2 a((from.first.x + from.second.x) / 2, from.second.y), b((to.first.x + to.second.x) / 2, to.first.y);
	float mid = (a.y + b.y) / 2;
	dl->AddLine(a, ImVec2(a.x, mid), col, 2);
	dl->AddLine(ImVec2(a.x, mid), ImVec2(b.x, mid), col, 2);
	dl->AddLine(ImVec2(b.x, mid), b, col, 2);
}

static void familyAlbum() {
	if (s_openAlbum) {
		ImGui::OpenPopup("family album");
		s_openAlbum = false;
	}
	if (!ImGui::BeginPopup("family album"))
		return;
	monikerData* m = recordOf(s_album);
	if (!m || m->photos.empty()) {
		ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
		return;
	}
	int n = (int)m->photos.size();
	s_albumPhoto = std::clamp(s_albumPhoto, 0, n - 1);
	ImGui::Text("%s", recordName(m).c_str());
	Texture t = photoTexture(engine.currentworlddir / m->photos[s_albumPhoto]);
	if (t)
		ImGui::Image(t.as<ImTextureID>(), ImVec2(t.width, t.height));
	else
		ImGui::TextDisabled("(this photo's file is missing)");
	if (ImGui::ArrowButton("prev", ImGuiDir_Left))
		s_albumPhoto = (s_albumPhoto + n - 1) % n;
	ImGui::SameLine();
	ImGui::Text("%d of %d, taken %s", s_albumPhoto + 1, n, KitDateText(photoTick(m->photos[s_albumPhoto])).c_str());
	ImGui::SameLine();
	if (ImGui::ArrowButton("next", ImGuiDir_Right))
		s_albumPhoto = (s_albumPhoto + 1) % n;
	// all of them, small
	for (int i = 0; i < n; i++) {
		Texture th = photoTexture(engine.currentworlddir / m->photos[i]);
		if (i % 6)
			ImGui::SameLine();
		ImGui::PushID(i);
		if (th) {
			if (ImGui::ImageButton("thumb", th.as<ImTextureID>(), ImVec2(60, 45)))
				s_albumPhoto = i;
		} else if (ImGui::Button("?", ImVec2(68, 53)))
			s_albumPhoto = i;
		ImGui::PopID();
	}
	ImGui::EndPopup();
}

static void FamilyPage() {
	if (!KitNeedsCreature("Applet Data/Owner.bmp"))
		return;
	Agent* a = KitCreatureAgent();
	monikerData* m = KitRecordsOf(a);
	if (!m)
		return;
	std::string self = m->getName();
	auto parentsOf = [](const std::string& mon) -> std::pair<std::string, std::string> {
		monikerData* r = recordOf(mon);
		return r ? std::make_pair(r->mother, r->father) : std::make_pair(std::string(), std::string());
	};
	auto [mother, father] = parentsOf(self);
	auto [mm, mf] = parentsOf(mother);
	auto [fm, ff] = parentsOf(father);
	bool parents = !mother.empty() || !father.empty();
	bool grandparents = !mm.empty() || !mf.empty() || !fm.empty() || !ff.empty();
	std::vector<std::string> children;
	for (auto& [mon, r] : world.history->all())
		if (!mon.empty() && (r.mother == self || r.father == self))
			children.push_back(mon);
	std::sort(children.begin(), children.end(), [](auto& x, auto& y) { return recordOf(x)->borntick < recordOf(y)->borntick; });
	// developer aid: OPENC2E_TEST_ALBUM=1 opens this creature's album
	static bool testAlbum = getenv("OPENC2E_TEST_ALBUM") != nullptr;
	if (testAlbum && !m->photos.empty()) {
		testAlbum = false;
		s_album = self;
		s_albumPhoto = 0;
		s_openAlbum = true;
	}
	bool engineered = false;
	for (auto& e : m->events)
		if (e.eventno == 2)
			engineered = true;

	// four columns: grandparents in all four, each parent over its two,
	// this creature in the middle, children below in rows of four
	const float col = CARD_W + CARD_GAP, W = 4 * col - CARD_GAP, ROW = CARD_H + 44;
	const ImU32 label = IM_COL32(190, 170, 130, 255);
	// the tree scrolls when it is taller than the screen allows
	float height = (grandparents + parents + 1) * ROW + (children.empty() ? 22 : ROW * ((children.size() + 3) / 4));
	float room = std::max(300.0f, ImGui::GetIO().DisplaySize.y - 300);
	ImGui::BeginChild("tree", ImVec2(W + (height > room ? ImGui::GetStyle().ScrollbarSize + 4 : 0), std::min(height, room)));
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 o = ImGui::GetCursorScreenPos();
	float y = o.y;
	std::pair<ImVec2, ImVec2> gp[4], pa[2], me;
	if (grandparents) {
		dl->AddText(ImVec2(o.x, y), label, "Grandparents");
		const std::string* g[4] = {&mm, &mf, &fm, &ff};
		for (int i = 0; i < 4; i++)
			gp[i] = familyCard(*g[i], ImVec2(o.x + i * col, y + 18), false, fmt::format("gp{}", i).c_str());
		y += ROW;
	}
	if (parents) {
		dl->AddText(ImVec2(o.x, y), label, "Parents");
		pa[0] = familyCard(mother, ImVec2(o.x + col / 2, y + 18), false, "mother");
		pa[1] = familyCard(father, ImVec2(o.x + 2.5f * col, y + 18), false, "father");
		if (grandparents)
			for (int i = 0; i < 4; i++)
				if (recordOf(i < 2 ? (i ? mf : mm) : (i == 2 ? fm : ff)))
					familyLine(gp[i], pa[i / 2]);
		y += ROW;
	}
	me = familyCard(self, ImVec2(o.x + 1.5f * col, y + 18), true, "self");
	if (parents)
		for (int i = 0; i < 2; i++)
			if (recordOf(i ? father : mother))
				familyLine(pa[i], me);
	if (!parents) {
		std::string why = engineered ? "No parents: it hatched from an egg the Hatchery or Breeder's Kit laid." : "No parents recorded.";
		ImGui::SetCursorScreenPos(ImVec2(o.x, y));
		ImGui::PushTextWrapPos(o.x + 1.5f * col - 8 - ImGui::GetWindowPos().x);
		ImGui::TextDisabled("%s", why.c_str());
		ImGui::PopTextWrapPos();
	}
	y += ROW;
	if (!children.empty()) {
		dl->AddText(ImVec2(o.x, y), label, children.size() == 1 ? "Child" : "Children");
		for (size_t i = 0; i < children.size(); i++) {
			size_t row = i / 4, inrow = std::min<size_t>(4, children.size() - row * 4);
			float x0 = o.x + (W - (inrow * col - CARD_GAP)) / 2;
			auto c = familyCard(children[i], ImVec2(x0 + (i % 4) * col, y + 18 + row * ROW), false, fmt::format("child{}", i).c_str());
			if (row == 0)
				familyLine(me, c);
		}
		y += ROW * ((children.size() + 3) / 4);
	} else {
		dl->AddText(ImVec2(o.x, y), IM_COL32(140, 140, 140, 255), "No children yet.");
		y += 22;
	}
	ImGui::SetCursorScreenPos(o);
	ImGui::Dummy(ImVec2(W, y - o.y));
	familyAlbum();
	ImGui::EndChild();
	ImGui::TextDisabled("Click a picture for that creature's photos; Go selects a living relative.");
}

void DrawOwnersKit() {
	if (ImGui::BeginTabBar("pages")) {
		if (KitTab("Registration")) {
			RegistrationPage();
			ImGui::EndTabItem();
		}
		if (KitTab("Photos")) {
			PhotosPage();
			ImGui::EndTabItem();
		}
		if (KitTab("Family")) {
			FamilyPage();
			ImGui::EndTabItem();
		}
		if (KitTab("Life log")) {
			LifeLogPage();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

} // namespace Openc2eImgui
