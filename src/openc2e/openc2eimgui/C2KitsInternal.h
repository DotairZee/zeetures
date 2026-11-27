#pragma once

// Shared pieces for the kit pages (C2Kits.cpp, HealthKit.cpp, ScienceKit.cpp).

#include "ImGuiUtils.h"

#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include <filesystem>
#include <memory>

class Agent;
class MetaRoom;
class c2Creature;
class gene;
class monikerData;
struct ImGuiTextBuffer;

namespace Openc2eImgui {

// The kits' art comes from the player's own Creatures 2 ("Applet Data"),
// loaded at runtime and never shipped. Pictures are cached by name.
Texture KitPicture(const std::string& name); // a .bmp drawn as it is
Texture KitPhotoTexture(const std::filesystem::path& path); // a photo in the world folder (cached)
void KitLayoutFollowsWorld(); // C2Kits.cpp: after "Save World As", the copy's kits.ini
void RemakeC1Genome(); // BreedPacks.cpp: the C1 grendels' genome, as the world's personalities setting asks
void DrawGraveyardKit();
void DrawMindKit(); // MindKit.cpp
void DrawEcologyKit(); // EcologyKit.cpp
void EcologyKitTick(); // records the Monitor's pinned rooms, whatever is open
void OpenGraveyardAt(const std::string& moniker); // the Graveyard Kit at that creature's stone
const std::vector<Texture>& KitSprites(const std::string& name); // .s16 frames, transparent background

// The files in a zip (stored or deflated), names without folders; false
// with a reason if it is not a zip it can read (BreedersKit.cpp).
bool KitReadZip(const std::vector<uint8_t>& zip, std::vector<std::pair<std::string, std::vector<uint8_t>>>& out, std::string& err);

// The files in an old add-on installer (a Windows program with its files
// appended; read, never run), as "Boney Grendels.exe" (BreedersKit.cpp).
bool KitReadInstaller(const std::vector<uint8_t>& exe, std::vector<std::pair<std::string, std::vector<uint8_t>>>& out, std::string& err);

// The selected creature, if it is a C2 creature; and its agent.
c2Creature* KitCreature();
Agent* KitCreatureAgent();

// The game's chemical names (allchemicals.str) and the Applet Data
// ChemGroups (with "All" first).
const std::vector<std::string>& KitChemicalNames();
const std::vector<std::pair<std::string, std::vector<int>>>& KitChemicalGroups();

// For pages that need a creature: when none is selected, says so and
// returns false.
bool KitNeedsCreature();

// Draws a number in the kit's green LED digits (led.s16: 0-9, 10 blank,
// 11 minus, 12 degrees C, 13 degrees F, 14 point) from a string of digits,
// '-', '.', 'C', 'F' and spaces; returns the width drawn.
float KitLed(ImVec2 p, const std::string& text);

// A horizontal level bar, 0-1, with a label on it: flat, in the window's
// frame color, filled green, amber or red (KitLevelColor). A "need" bar is
// green when low and turns red as it fills; otherwise full is green.
void KitBar(float level, const char* label, float width = 192, bool need = false);
// The kits' own look (one style for every page; no C2 panel art):
// green, amber or red for a level 0-1 (a need is good when low)
ImU32 KitLevelColor(float level, bool need = false);
// a section heading: warm text with a faint rule under it
void KitHeading(const char* text);
// a temperament trait's bar in its own color (Owner's Kit, Temperament page)
void TraitBar(int trait, int percent, const char* label, float width);
// a reading in a small panel: heading, the value large with its unit, a
// line under it in a color, and optionally a trend line (oldest first)
void KitStatTile(const char* id, const char* head, const std::string& big, const char* unit, const char* sub, ImU32 subColor, float width, const std::vector<float>* trend = nullptr);
// a card: a gold-edged panel (as the Family page's cards); size.y 0 fits
// its contents. Always call KitEndCard.
bool KitBeginCard(const char* id, ImVec2 size);
void KitEndCard();
// the creature's latest photo (Owner's Kit), framed, at that size; a blank
// frame when it has none
void KitCreaturePhoto(Agent* a, ImVec2 size);

// Gene notes: the captions and comments C2's genetics files give each gene
// (Genetics/norn.gno, gren.gno, ettn.gno, by the creature's genus). Notes
// are matched by a gene's place among the genes of its kind, so they fit the
// species' original genome; duplicated or deleted genes shift them. Caption
// "HEART", comment in plain text (from RTF). Null when there is no note.
struct KitGeneNote {
	std::string caption, comment;
};
const KitGeneNote* KitNoteFor(gene* g);

// Temperatures: Celsius or Fahrenheit (click a temperature to switch; kept
// in kits.ini)
bool KitFahrenheit();
void KitToggleFahrenheit();

// Applet Data files of one kind in a subfolder ("Medicine", ".cob")
std::vector<std::string> KitAppletDataFiles(const std::string& sub, const std::string& ext);

// The COBs of an Applet Data subfolder as tiles (each one's own picture and
// name; click one to read about it, then add it to the world). added: the
// message after adding, with {} for its name.
void KitCobShelf(const char* subfolder, const char* hint, const char* added);

// ---- creatures and their records ----
// every living C1/C2 creature in the world
std::vector<std::shared_ptr<Agent>> KitLivingCreatures();
c2Creature* KitC2Of(Agent* a);
// the owner's records for a creature (name, birth, registration, photos)
monikerData* KitRecordsOf(Agent* a);
// its name, or "Unnamed norn" and so on
std::string KitCreatureName(Agent* a);
const char* KitSpeciesWord(Agent* a); // "norn", "grendel", "ettin"
// ages and dates: C2 time runs a part of the day per 3600 world ticks, 5
// parts a day, 4 days a season, 4 seasons a year
unsigned int KitAgeTicks(monikerData& m);
std::string KitAgeText(unsigned int ticks, bool minutes = true); // "2 days 3 parts (54 min of play)"
std::string KitDateText(unsigned int worldtick); // "Summer, day 3, year 001"
// a creature's health, 0-1: its weakest organ's short-term life force
// against that organ's start (what failing kills it; the status bar shows
// the same). Optionally: how many are under 75%, the mean of all organs,
// and which organ is weakest (its place in getOrgans(), or -1).
float KitOrganHealth(c2Creature* c, int* damaged = nullptr, float* average = nullptr, int* weakest = nullptr);
// where an agent is: near a favorite place ("near The Incubator"), or
// else its part of the metaroom ("low down in the east")
std::string KitWhere(Agent* a);
std::string KitWhereAt(float x, float y); // the same for a point in the world
// a small picture (660 px wide, dimmed) of a metaroom's background, made
// once per background; w and h are its size
Texture KitMapPicture(MetaRoom* m, int& w, int& h);
// selects a creature and moves the camera to it
void KitGoTo(const std::shared_ptr<Agent>& a);
// the game's word for a kind of thing (catalogue words 16 on: "Toy",
// "Egg"), capitalized; "kind 40" when it has none
std::string KitCategoryNoun(int category);

// ---- layout shared by the kits ----
// The creature kits' header: the selected creature's name in a list of
// every living creature, with arrows for the previous and next one
void KitCreatureHeader();
// Text that wraps at the window's width rather than widening it (hints and
// notes): dim, or in a color
void KitNote(const char* fmt, ...) IM_FMTARGS(1);
void KitNoteColored(const ImVec4& color, const char* fmt, ...) IM_FMTARGS(2);
// Moves the cursor so the next item, this wide, is centered in the window
// (pictures and charts; text stays at the left)
void KitCenterNext(float width);

// ---- alarms (Observation Kit), checked every frame whatever is open ----
void KitAlarmsTick();
void BreedPacksTick(); // BreedPacks.cpp: Zeetures's recommended breed packs, fetched and imported
void KitAlarmsReadLine(const char* line);
void KitAlarmsWrite(ImGuiTextBuffer* buf);

// ---- which notices show on the status bar (owner, 2026-11-21: "there should
// be a checkbox in the kit that determines whether the notification should
// show in the status bar ... for all notices and respective kits") ----
// kind: a lifeLogEntry kind. A notice switched off is still written in the
// life log; it only does not appear on the status bar. Kept in kits.ini.
bool KitNoticeShown(int kind);
// a row of checkboxes at the bottom of a kit, one per kind it owns

// a page tab (ImGui::BeginTabItem; the test aid can pick one)
bool KitTab(const char* label);
void RelationshipsPage(); // Owner's Kit (RelationshipsPage.cpp)
void LifeStoryPage(); // Owner's Kit (ChroniclesKit.cpp)
void WorldHistoryPage(); // Chronicles Kit (OwnersKit.cpp)
void DrawChroniclesKit();

void DrawHealthKit();
void DrawScienceKit();
void DrawBreedersKit();
void DrawOwnersKit();
// photos lightning asked for (Lightning::takePhotoRequest), each frame
void TakeRequestedPhotos();
void DrawObservationKit();

} // namespace Openc2eImgui
