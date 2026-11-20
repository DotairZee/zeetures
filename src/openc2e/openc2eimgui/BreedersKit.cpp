// The Breeder's Kit (Creatures 2): Reproductive and Aphrodisiacs pages, as
// C2 had, plus Breeds (owner, 2026-11-09): the breeds installed for each
// species, a portrait of each, laying an egg of a breed, and importing C2
// breed packs (files, a folder, a zip or a COB) into the user data folder,
// moved to a free breed slot when theirs is taken. Slots run a-z, then aa,
// ab ... (creatures/BreedSlots.h). C2's own art from the player's Applet
// Data; our layout.

#include "Agent.h"
#include "Breeds.h"
#include "creatures/BrainProfiles.h"
#include "C2KitsInternal.h"
#include "Hatchery.h"
#include "PathResolver.h"
#include "common/case_insensitive_filesystem.h"
#include "World.h"
#include "creatures/Breeding.h"
#include "common/Ascii.h"
#include "common/Random.h"
#include "creatures/BreedSlots.h"
#include "creatures/oldCreature.h"
#include "creatures/CreatureAgent.h"
#include "creatures/Temperament.h"
#include "historyManager.h"
#include "common/io/SpanReader.h"
#include "common/io/FileReader.h"
#include "Engine.h"
#include "fileformats/c2cobfile.h"
#include "fileformats/genomeFile.h"

#include <SDL3/SDL_dialog.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <functional>
#include <imgui.h>
#include <map>
#include <mutex>
#include <set>
#include <zlib.h>

using namespace ImGuiUtils;
namespace fs = std::filesystem;

namespace Openc2eImgui {

// Who made the breed packs the owner imported (owner, 2026-11-11: credit
// third-party creators by name), matched on the pack's file name. From the
// packs' own files where they name their authors, else creatures.wiki
// (Kai Norn and Scorpio Norn pages, read 2026-11-12). The Boney Grendels
// were an official Creatures 2 release, later updated by Lis Morris (owner,
// 2026-11-12; the wiki files them with the official grendel breeds).
const char* PackCredit(const std::string& source) {
	static const std::pair<const char*, const char*> credits[] = {
		{"FrogNorn", "Alex (Norns of the Round Table): Frog Norn V3, an overhaul of Nornenmeister's V2 Frog Norns"},
		{"Midsummer", "Martina Hilsenbeck (Nimue) and Amanora; genome by Katira"},
		{"goatnorn", "Angela Allan"},
		{"Kai", "Wafuru (pictures, and the milk bottle); genome by Lis Morris"},
		{"Scorpio", "Wafuru (pictures and body data); genome by Mannkind"},
		{"Boney", "Creature Labs, an official Creatures 2 release; updated by Lis Morris"},
	};
	std::string lower = source;
	for (auto& c : lower)
		c = (char)std::tolower((unsigned char)c);
	for (auto& [key, who] : credits) {
		std::string k = key;
		for (auto& c : k)
			c = (char)std::tolower((unsigned char)c);
		if (lower.find(k) != std::string::npos)
			return who;
	}
	return nullptr;
}

// ---- Reproductive --------------------------------------------------------------

static void ReproductivePage() {
	if (!KitNeedsCreature())
		return;
	c2Creature* c = KitCreature();
	Agent* a = KitCreatureAgent();
	bool female = c->isFemale();
	int preg = c->getPregnant();
	int genus = c->getGenus() + 1; // the genome's genus: 0 norn, 1 grendel, 2 ettin

	// its photo, beside the readings
	const float W = 540, PW = 160, PH = 120, BW = W - PW - 20;
	KitCreaturePhoto(a, ImVec2(PW, PH));
	ImGui::SameLine(0, 20);
	ImGui::BeginGroup();
	// with its life stage (owner, 2026-11-13): creatures mate from
	// adolescence (the world's kiss script asks for CAGE 2 or more of both)
	static const char* stages[] = {"baby", "child", "adolescent", "youth", "adult", "old", "senile"};
	int stage = (int)c->getStage();
	ImGui::TextColored(ImVec4(0.86f, 0.70f, 0.40f, 1), "%s %s, %s", female ? "Female" : "Male", genus == 2 ? "grendel" : genus == 3 ? "ettin" : "norn",
		stage >= 0 && stage < 7 ? stages[stage] : "?");
	if (stage < 2)
		ImGui::TextDisabled("Too young to breed: creatures mate from adolescence.");
	KitBar(c->getDrive(12) / 255.0f, "Sex drive", BW, true);
	KitBar(c->getFertile() / 255.0f, female ? "Fertility" : "Fertility (sperm)", BW);
	// the owner's fertility switch for this creature (owner, 2026-11-14:
	// under its fertility reading): ticked, it cannot conceive or father; it
	// overrides the Hatchery's "All creatures fertile" until set back
	{
		bool infertile = !fertileBySetting(c);
		if (ImGui::Checkbox("Infertile", &infertile))
			c->fertilityOverride = infertile ? 1 : 0;
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Ticked, this creature cannot conceive or father young.\nUntick it to let its fertility cycle carry on.");
		ImGui::SameLine(0, 12);
		if (c->fertilityOverride >= 0) {
			if (ImGui::SmallButton("Follow the Hatchery"))
				c->fertilityOverride = -1;
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Its own setting is in use. Click to follow the Hatchery's \"All creatures fertile\" again.");
		} else
			ImGui::TextDisabled(world.allFertile ? "(as the Hatchery)" : "(as the Hatchery: none fertile)");
	}
	if (female)
		KitBar(c->getReceptive() / 255.0f, "Receptiveness", BW);
	ImGui::EndGroup();


	// pregnancy: how far along
	if (female) {
		KitHeading("Pregnancy");
		if (preg) {
			KitBar(preg / 255.0f, fmt::format("{:.0f}% of the way to laying", preg * 100.0f / 255).c_str(), W, false);
			KitNote("The egg grows until it is laid; the Breeder's Kit notice on the status bar follows it.");
		} else
			KitNote("Not pregnant.");
	}

	// the hormones, as the game groups them
	const auto& groups = KitChemicalGroups();
	const char* want = female ? "Female Hormones" : "Male Hormones";
	for (auto& g : groups) {
		if (g.first != want)
			continue;
		KitHeading(want);
		const auto& names = KitChemicalNames();
		for (int chem : g.second)
			KitBar(c->getChemical(chem) / 255.0f, fmt::format("{} {}", chem < (int)names.size() ? names[chem] : std::string("?"), (int)c->getChemical(chem)).c_str(), W);
	}
}

// ---- Insemination (owner, 2026-11-20) ----------------------------------------

// Female creatures only: choose any creature of her kind, male or female,
// living, departed (genes kept when it died) or a genome file, and she
// conceives from its genes (Breeding.h, inseminate).

// the creature's mother and father, by moniker, from the records
static std::pair<std::string, std::string> parentsOf(const std::string& m) {
	if (m.empty())
		return {};
	auto& all = world.history->all();
	auto it = all.find(m);
	if (it == all.end())
		return {};
	return {it->second.mother, it->second.father};
}

static std::string kinship(const std::string& a, const std::string& b, bool bFemale) {
	auto [am, af] = parentsOf(a);
	auto [bm, bf] = parentsOf(b);
	if (b == am || b == af)
		return bFemale ? "her mother" : "her father";
	if (a == bm || a == bf)
		return bFemale ? "her daughter" : "her son";
	std::set<std::string> ap, bp;
	for (auto& x : {am, af})
		if (!x.empty())
			ap.insert(x);
	for (auto& x : {bm, bf})
		if (!x.empty())
			bp.insert(x);
	int shared = 0;
	for (auto& x : ap)
		shared += (int)bp.count(x);
	if (shared == 2)
		return bFemale ? "her sister" : "her brother";
	if (shared == 1)
		return bFemale ? "her half-sister" : "her half-brother";
	std::set<std::string> ag, bg;
	for (auto& x : ap)
		for (auto& g : {parentsOf(x).first, parentsOf(x).second})
			if (!g.empty())
				ag.insert(g);
	for (auto& x : bp)
		for (auto& g : {parentsOf(x).first, parentsOf(x).second})
			if (!g.empty())
				bg.insert(g);
	for (auto& g : ag)
		if (bg.count(g) || bp.count(g))
			return "close kin (a shared grandparent)";
	for (auto& g : bg)
		if (ap.count(g))
			return "close kin (a shared grandparent)";
	return "";
}

struct InsemDonor {
	int source = 0; // 0 living, 1 departed, 2 genome file
	Agent* agent = nullptr;
	oldCreature* creature = nullptr;
	std::string name, moniker, file, photo, breed;
	bool female = false;
	int stage = -1;
};

// genome files of each kind (0 norn, 1 grendel, 2 ettin), by name, scanned
// once: the player's Genetics folder and the game's, without the generated
// "z" names (a conception's or the splicer's)
static std::map<int, std::vector<std::pair<std::string, fs::path>>> s_genomeFiles;
static bool s_genomeFilesScanned = false;
static void scanGenomeFiles() {
	s_genomeFilesScanned = true;
	s_genomeFiles.clear();
	std::set<std::string> seen;
	std::vector<fs::path> dirs = {getUserDataDir() / "Genetics"};
	for (auto& d : getMainDirectories())
		dirs.push_back(d / "Genetics");
	for (auto& dir : dirs) {
		std::error_code ec;
		for (auto& e : fs::directory_iterator(dir, ec)) {
			std::string fn = e.path().filename().string();
			if (fn.size() < 5 || to_ascii_lowercase(fn.substr(fn.size() - 4)) != ".gen")
				continue;
			std::string name = fn.substr(0, fn.size() - 4);
			std::string lower = to_ascii_lowercase(name);
			if (lower.size() == 4 && lower[0] == 'z')
				continue; // generated
			if (seen.count(lower))
				continue; // the player's copy first
			std::ifstream f(e.path(), std::ios::binary);
			std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
			int genus = -1;
			for (size_t i = 0; i + 12 < b.size(); i++)
				if (b[i] == 'g' && b[i + 1] == 'e' && b[i + 2] == 'n' && b[i + 3] == 'e' && b[i + 4] == 2 && b[i + 5] == 1) {
					genus = b[i + 11];
					break;
				}
			if (genus < 0)
				continue;
			seen.insert(lower);
			s_genomeFiles[genus].push_back({name, e.path()});
		}
	}
	for (auto& [g, v] : s_genomeFiles)
		std::sort(v.begin(), v.end(), [](auto& a, auto& b) { return to_ascii_lowercase(a.first) < to_ascii_lowercase(b.first); });
}

static std::string s_insemMessage;
static bool s_insemOk = false;

static void InseminationPage() {
	if (!KitNeedsCreature())
		return;
	c2Creature* c = KitCreature();
	Agent* a = KitCreatureAgent();
	auto* mumAgent = dynamic_cast<CreatureAgent*>(a);
	static const char* stages[] = {"baby", "child", "adolescent", "youth", "adult", "old", "senile"};
	static const char* kinds[] = {"norn", "grendel", "ettin"};
	const ImVec4 gold(0.86f, 0.70f, 0.40f, 1), green(0.35f, 0.77f, 0.41f, 1), amber(0.89f, 0.67f, 0.27f, 1), red(0.84f, 0.38f, 0.34f, 1);
	int genus = c->getGenus(); // 0 norn, 1 grendel, 2 ettin
	const char* kind = genus >= 0 && genus <= 2 ? kinds[genus] : "creature";
	std::string mumMon = world.history->findMoniker(AgentRef(a));

	static int from = 0;
	// developer aid: OPENC2E_TEST_INSEM="SOURCE[:NAME]" (0 living, 1 departed,
	// 2 genome files; NAME a donor's name, moniker or file name) picks that
	// donor and presses Inseminate once, when the page has been drawn a few
	// times (headless tests, with OPENC2E_OPEN_KITS and OPENC2E_KIT_PAGE)
	static const char* testInsem = getenv("OPENC2E_TEST_INSEM");
	static int testFrames = 0;
	static std::string testName;
	if (testInsem && testFrames == 0) {
		from = std::clamp(atoi(testInsem), 0, 2);
		const char* colon = std::strchr(testInsem, ':');
		testName = colon ? std::string(colon + 1) : std::string();
	}
	std::vector<InsemDonor> donors;
	if (from == 0) {
		// every other living creature of her kind, adolescent or older
		for (auto& ag : world.agents) {
			auto* ca = dynamic_cast<CreatureAgent*>(ag.get());
			if (!ca || ag.get() == a)
				continue;
			auto* oc = dynamic_cast<oldCreature*>(ca->getCreature());
			if (!oc || !oc->isAlive() || (int)oc->getGenus() != genus || (int)oc->getStage() < 2)
				continue;
			InsemDonor d;
			d.agent = ag.get();
			d.creature = oc;
			d.name = KitCreatureName(ag.get());
			d.moniker = world.history->findMoniker(AgentRef(ag.get()));
			d.female = oc->isFemale();
			d.stage = (int)oc->getStage();
			donors.push_back(d);
		}
		std::sort(donors.begin(), donors.end(), [](auto& x, auto& y) { return x.name < y.name; });
	} else if (from == 1) {
		// the departed whose genes were kept (from 2026-11-20 on)
		for (auto& [mon, r] : world.history->all()) {
			if (!r.died || (int)r.genus != genus + 1 || !departedGenesKept(mon))
				continue;
			InsemDonor d;
			d.source = 1;
			d.name = r.name.empty() ? mon : r.name;
			d.moniker = mon;
			d.female = departedSex(mon) == 1;
			d.stage = r.diedstage;
			if (!r.photos.empty())
				d.photo = r.photos.back();
			donors.push_back(d);
		}
		std::sort(donors.begin(), donors.end(), [](auto& x, auto& y) { return x.name < y.name; });
	} else {
		if (!s_genomeFilesScanned)
			scanGenomeFiles();
		for (auto& [name, path] : s_genomeFiles[genus]) {
			InsemDonor d;
			d.source = 2;
			d.name = name;
			d.file = path.string();
			donors.push_back(d);
		}
	}

	// the donor picked, kept by who it is while the list changes
	static std::string pickKey;
	static int pickFrom = -1;
	auto keyOf = [](const InsemDonor& d) { return d.source == 2 ? d.file : d.moniker; };
	int pick = -1;
	for (int i = 0; i < (int)donors.size(); i++)
		if (pickFrom == from && keyOf(donors[i]) == pickKey)
			pick = i;
	if (pick < 0 && !donors.empty()) {
		pick = 0;
		pickKey = keyOf(donors[0]);
		pickFrom = from;
	}

	static const int testAt = getenv("OPENC2E_TEST_INSEM_FRAMES") ? std::max(5, atoi(getenv("OPENC2E_TEST_INSEM_FRAMES"))) : 5;
	if (testInsem && testFrames < testAt && !testName.empty())
		for (int i = 0; i < (int)donors.size(); i++)
			if (donors[i].name == testName || donors[i].moniker == testName) {
				pick = i;
				pickKey = keyOf(donors[i]);
				pickFrom = from;
			}
	auto label = [&](const InsemDonor& d) {
		if (d.source == 2)
			return d.name + ".gen";
		std::string st = d.stage >= 0 && d.stage < 7 ? stages[d.stage] : "";
		return fmt::format("{}  ({}), {}{}{}", d.name, d.moniker, d.female ? "female" : "male", st.empty() ? "" : " ", st);
	};
	ImGui::TextColored(gold, "Donor");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(300);
	if (ImGui::BeginCombo("##donor", donors.empty() ? "(none)" : label(donors[pick]).c_str(), ImGuiComboFlags_HeightLarge)) {
		for (int i = 0; i < (int)donors.size(); i++)
			if (ImGui::Selectable(label(donors[i]).c_str(), i == pick)) {
				pick = i;
				pickKey = keyOf(donors[i]);
				pickFrom = from;
			}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	ImGui::TextDisabled("from");
	ImGui::SameLine();
	static const char* froms[] = {"Living creatures", "Departed creatures", "Genetics files"};
	ImGui::SetNextItemWidth(170);
	if (ImGui::Combo("##from", &from, froms, 3))
		s_insemMessage.clear();
	if (from == 2) {
		ImGui::SameLine();
		if (ImGui::SmallButton("Rescan"))
			scanGenomeFiles();
	}
	if (donors.empty()) {
		if (from == 0)
			KitNote("No other %s old enough to give its genes (adolescent or older).", kind);
		else if (from == 1)
			KitNote("No departed %s's genes are kept yet: they are kept for creatures that die from now on.", kind);
		else
			KitNote("No %s genome files in the Genetics folders.", kind);
		return;
	}
	InsemDonor& d = donors[pick];

	// the donor's genes and traits
	std::shared_ptr<genomeFile> dg;
	Temperament dt;
	bool hasT = false;
	float dz = 0;
	if (d.source == 0) {
		dg = d.creature->getGenome();
		dt = d.creature->temperament;
		dz = d.creature->seasonalZ;
		hasT = true;
	} else if (d.source == 1) {
		loadDepartedGenes(d.moniker, dg, dt, hasT, dz);
	} else {
		try {
			dg = std::make_shared<genomeFile>();
			FileReader f(d.file);
			f >> *dg;
		} catch (std::exception&) {
			dg.reset();
		}
		hasT = Temperaments::load(d.name, dt);
		float z;
		if (SeasonalRhythm::load(d.name, z))
			dz = z;
		else
			hasT = false;
	}

	// the two, side by side
	const float colW = 270, PW = 120, PH = 90;
	auto photoBox = [&](const std::string& photo) {
		ImDrawList* dl = ImGui::GetWindowDrawList();
		ImVec2 p0 = ImGui::GetCursorScreenPos();
		Texture t = photo.empty() ? Texture() : KitPhotoTexture(engine.currentworlddir / photo);
		if (t)
			dl->AddImage(t.as<ImTextureID>(), p0, p0 + ImVec2(PW, PH));
		else {
			dl->AddRectFilled(p0, p0 + ImVec2(PW, PH), IM_COL32(18, 18, 20, 255));
			const char* none = d.source == 2 ? "genome file" : "no photo";
			ImVec2 ns = ImGui::CalcTextSize(none);
			dl->AddText(p0 + ImVec2((PW - ns.x) / 2, (PH - ns.y) / 2), IM_COL32(110, 110, 110, 255), none);
		}
		dl->AddRect(p0 - ImVec2(1, 1), p0 + ImVec2(PW + 1, PH + 1), IM_COL32(205, 170, 95, 255));
		ImGui::Dummy(ImVec2(PW, PH));
	};
	ImGui::Spacing();
	ImGui::BeginGroup();
	KitCreaturePhoto(a, ImVec2(PW, PH));
	ImGui::SameLine();
	ImGui::BeginGroup();
	ImGui::TextUnformatted(KitCreatureName(a).c_str());
	ImGui::TextDisabled("%s", mumMon.c_str());
	ImGui::Text("Female %s, %s", kind, stages[std::clamp((int)c->getStage(), 0, 6)]);
	ImGui::TextColored(gold, "mother");
	ImGui::EndGroup();
	ImGui::EndGroup();
	ImGui::SameLine(colW + 24);
	ImGui::BeginGroup();
	if (d.source == 0)
		KitCreaturePhoto(d.agent, ImVec2(PW, PH));
	else
		photoBox(d.photo);
	ImGui::SameLine();
	ImGui::BeginGroup();
	ImGui::TextUnformatted(d.source == 2 ? (d.name + ".gen").c_str() : d.name.c_str());
	if (d.source == 2)
		ImGui::TextDisabled("a genome file");
	else
		ImGui::TextDisabled("%s%s", d.moniker.c_str(), d.source == 1 ? ", departed" : "");
	if (d.source != 2)
		ImGui::Text("%s %s%s%s", d.female ? "Female" : "Male", kind, d.stage >= 0 && d.stage < 7 ? ", " : "", d.stage >= 0 && d.stage < 7 ? stages[d.stage] : "");
	ImGui::TextColored(gold, "%s", d.source == 2 ? "donor (its genes)" : d.female ? "donor (her genes)" : "donor (his genes)");
	ImGui::EndGroup();
	ImGui::EndGroup();

	KitHeading("Temperament, side by side");
	for (int i = 0; i < NUM_TRAITS; i++) {
		std::string trait = Temperaments::traitName(i);
		trait[0] = (char)std::toupper((unsigned char)trait[0]);
		int p1 = c->temperament.percent(i);
		TraitBar(i, p1, fmt::format("{} {}%", trait, p1).c_str(), colW);
		if (hasT) {
			int p2 = dt.percent(i);
			ImGui::SameLine(colW + 24);
			TraitBar(i, p2, fmt::format("{} {}%", trait, p2).c_str(), colW);
		} else if (i == 0) {
			ImGui::SameLine(colW + 24);
			ImGui::TextDisabled("No temperament on record:");
		} else if (i == 1) {
			ImGui::SameLine(colW + 24);
			ImGui::TextDisabled("the donor's half is drawn at random.");
		}
	}
	KitNote("The child takes after both: each trait lands near the middle of the two, with some chance.");

	KitHeading("Kinship");
	std::string kin = d.source == 2 ? std::string() : kinship(mumMon, d.moniker, d.female);
	if (d.source == 2)
		ImGui::TextDisabled("A genome file: no family records.");
	else if (kin.empty())
		ImGui::TextColored(green, "Not kin: no shared parents or grandparents on record.");
	else
		ImGui::TextColored(amber, "%s is %s.", d.name.c_str(), kin.c_str());

	KitHeading("Insemination");
	std::string block = inseminationBlock(mumAgent);
	bool old = (int)c->getStage() >= 2, notPreg = c->getPregnant() == 0 && a->babymoniker == 0, fertile = fertileBySetting(c);
	auto check = [&](bool ok, const char* t) {
		ImGui::TextColored(ok ? green : red, "%s %s", ok ? "ok" : "--", t);
		ImGui::SameLine(0, 18);
	};
	check(old, "old enough");
	check(notPreg, "not pregnant");
	check(fertile, "fertile");
	check(dg != nullptr, "genes readable");
	ImGui::NewLine();
	ImGui::BeginDisabled(!block.empty() || !dg);
	bool testPress = testInsem && ++testFrames == testAt;
	if (testPress)
		fmt::print("* TEST INSEMINATE: {} ({}) with {} [{}]\n", KitCreatureName(a), mumMon, d.name, froms[from]);
	if (ImGui::Button("Inseminate", ImVec2(160, 0)) || (testPress && block.empty() && dg)) {
		InseminationDonor donor;
		donor.genome = dg;
		donor.moniker = d.source == 2 ? std::string() : d.moniker;
		donor.label = d.source == 2 ? d.name + ".gen" : d.name;
		donor.temperament = hasT ? &dt : nullptr;
		donor.seasonal = hasT ? &dz : nullptr;
		std::string why;
		s_insemOk = inseminate(mumAgent, donor, why);
		s_insemMessage = s_insemOk ? fmt::format("{} is pregnant with {}'s genes. She lays her egg as usual.", KitCreatureName(a), donor.label)
								   : fmt::format("Not done: {}.", why);
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (!block.empty())
		ImGui::TextColored(amber, "Not now: %s.", block.c_str());
	else if (d.source != 2 && d.female)
		ImGui::TextDisabled("Two mothers: her egg crosses the two genomes as a mating does.");
	else
		ImGui::TextDisabled("Her egg crosses the two genomes as a mating does.");
	if (testPress && (!block.empty() || !dg))
		fmt::print("* TEST INSEMINATE: not pressed ({})\n", block.empty() ? "genes unreadable" : block);
	if (!s_insemMessage.empty())
		ImGui::TextColored(s_insemOk ? green : red, "%s", s_insemMessage.c_str());
	KitNote("The donor is not touched: only a copy of its genes is used. She carries the egg and lays it as usual; the child's sex is chance.");
}

// ---- Aphrodisiacs ----------------------------------------------------------------

static void AphrodisiacsPage() {
	KitCobShelf("Aphrodisiacs", "Click an aphrodisiac to see what it is.", "{} added. It may put your creatures in the mood.");
}

// ---- Breeds ----------------------------------------------------------------------

// pictures of body-part files, by file name (first frame or a given frame)
static Texture breedPicture(const std::string& file, int frame) {
	auto& frames = KitSprites("Images/" + file);
	if (frames.empty())
		return {};
	return frames[std::min<size_t>(frame, frames.size() - 1)];
}

// the file name of a breed's part picture for a sex and stage, if any (the
// stage given or the nearest below, as the engine looks)
static std::string partFile(const BreedInfo& b, char part, bool female, int stage) {
	for (int st = stage; st >= 0; st--) {
		std::string stem = fmt::format("{}{}{}{}", part, b.species + (female ? 4 : 0), st, breedSlotName(b.slot));
		for (const char* ext : {".png", ".s16", ".c16"})
			if (!findImageFile(stem + ext).empty())
				return stem + ext;
	}
	return {};
}

static const char* partName(char p) {
	switch (p) {
		case 'a': return "head";
		case 'b': return "body";
		case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': return "legs";
		case 'i': case 'j': case 'k': case 'l': return "arms";
		case 'm': case 'n': return "tail";
		default: return "head extras";
	}
}

// ---- importing ---------------------------------------------------------------------

struct ImportFile {
	std::string name; // file name only
	std::vector<uint8_t> data;
};

struct ImportGroup {
	int species = 0, slot = 0, target = 0;
	// the set the pack filed it in, when a norn breed filed in another set
	// is installed in the norn set instead (species is then 0)
	int fromSpecies = -1;
	int srcSpecies() const { return fromSpecies >= 0 ? fromSpecies : species; }
	std::vector<size_t> files;
	bool install = true;
	// checks made when the pack is read: its own slot is taken (clash); a
	// breed already installed has the same files (sameAs: its slot; sameFiles
	// of them identical, all of them for a duplicate); asked once the owner
	// has answered the question about it
	bool clash = false, asked = false;
	int sameAs = -1, sameFiles = 0;
	// installed in the Overrides folder in its own slot, in place of the
	// game's own breed there (the Frog Norns V3 pack in C2's frog norn slot
	// l; BreedPacks.cpp)
	bool inPlaceOfGame = false;
	bool duplicate() const { return sameAs >= 0 && sameFiles == (int)files.size(); }
};

// a genome in the pack that uses a slot a breed imported earlier was moved
// out of: it may mean that breed (by its old letter)
struct SlotQuestion {
	int species = 0, slot = 0, movedTo = 0;
	std::string breed, genomes;
	bool remap = false;
};

struct ImportPlan {
	std::string source; // what was picked, for the registry
	char name[64] = {};
	std::vector<ImportFile> files;
	std::vector<ImportGroup> groups;
	std::vector<size_t> genomes, notes, eggs, cobs, skipped;
	std::vector<std::string> problems;
	std::vector<std::string> brain; // its genome's brain add-ons and the roles they get (BrainProfiles.h)
	std::vector<SlotQuestion> slotQuestions;
	int asking = -1; // the group the slot dialog is asking about
};

static std::unique_ptr<ImportPlan> s_plan;
static std::string s_breed_message;

static std::mutex s_pick_mutex;
static std::vector<std::string> s_picked;

static void SDLCALL BreedFilesChosen(void*, const char* const* files, int) {
	if (!files)
		return;
	std::lock_guard<std::mutex> lock(s_pick_mutex);
	for (const char* const* f = files; *f; f++)
		s_picked.push_back(*f);
}

static std::vector<uint8_t> readAll(const fs::path& p) {
	std::ifstream in(p, std::ios::binary);
	return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

static uint32_t le32(const std::vector<uint8_t>& d, size_t p) {
	return d[p] | (d[p + 1] << 8) | (d[p + 2] << 16) | ((uint32_t)d[p + 3] << 24);
}
static uint16_t le16(const std::vector<uint8_t>& d, size_t p) {
	return (uint16_t)(d[p] | (d[p + 1] << 8));
}

// the files in a zip (stored or deflated), names without folders
static bool readZip(const std::vector<uint8_t>& z, std::vector<ImportFile>& out, std::string& err) {
	if (z.size() < 22) {
		err = "too small to be a zip";
		return false;
	}
	size_t eocd = std::string::npos;
	for (size_t i = z.size() - 22 + 1; i-- > 0 && z.size() - i < 70000;)
		if (le32(z, i) == 0x06054b50) {
			eocd = i;
			break;
		}
	if (eocd == std::string::npos) {
		err = "no zip directory found";
		return false;
	}
	size_t count = le16(z, eocd + 10);
	size_t p = le32(z, eocd + 16);
	for (size_t n = 0; n < count; n++) {
		if (p + 46 > z.size() || le32(z, p) != 0x02014b50) {
			err = "damaged zip directory";
			return false;
		}
		int method = le16(z, p + 10);
		uint32_t csize = le32(z, p + 20), usize = le32(z, p + 24);
		size_t nlen = le16(z, p + 28), elen = le16(z, p + 30), clen = le16(z, p + 32);
		uint32_t local = le32(z, p + 42);
		std::string name(z.begin() + p + 46, z.begin() + p + 46 + nlen);
		p += 46 + nlen + elen + clen;
		if (name.empty() || name.back() == '/')
			continue;
		if (local + 30 > z.size() || le32(z, local) != 0x04034b50)
			continue;
		size_t data = local + 30 + le16(z, local + 26) + le16(z, local + 28);
		if (data + csize > z.size())
			continue;
		ImportFile f;
		f.name = fs::path(name).filename().string();
		if (method == 0) {
			f.data.assign(z.begin() + data, z.begin() + data + csize);
		} else if (method == 8) {
			f.data.resize(usize);
			z_stream zs{};
			if (inflateInit2(&zs, -15) != Z_OK)
				continue;
			zs.next_in = const_cast<Bytef*>(z.data() + data);
			zs.avail_in = csize;
			zs.next_out = f.data.data();
			zs.avail_out = usize;
			int r = inflate(&zs, Z_FINISH);
			inflateEnd(&zs);
			if (r != Z_STREAM_END)
				continue;
		} else {
			continue;
		}
		out.push_back(std::move(f));
	}
	return true;
}

bool KitReadZip(const std::vector<uint8_t>& zip, std::vector<std::pair<std::string, std::vector<uint8_t>>>& out, std::string& err) {
	std::vector<ImportFile> files;
	if (!readZip(zip, files, err))
		return false;
	for (auto& f : files)
		out.push_back({f.name, std::move(f.data)});
	return true;
}

// The self-extracting installers some C2 add-ons came as (Windows
// programs, never run here): after the program's own sections comes a zlib
// stream and an 8-byte trailer (the stream's offset, then "dbrf"). The stream
// holds three length-prefixed strings (title, credits, description), a file
// count, then each file as a length-prefixed name, a 32-bit size and its
// bytes. Found in "Boney Grendels.exe" (owner, 2026-11-09).
static bool readInstaller(const std::vector<uint8_t>& exe, std::vector<ImportFile>& out, std::string& err) {
	if (exe.size() < 64 || exe[0] != 'M' || exe[1] != 'Z') {
		err = "not a Windows program";
		return false;
	}
	if (std::memcmp(&exe[exe.size() - 4], "dbrf", 4) != 0) {
		err = "not an installer this can read";
		return false;
	}
	uint32_t start = le32(exe, exe.size() - 8);
	if (start >= exe.size() - 8) {
		err = "damaged installer";
		return false;
	}
	std::vector<uint8_t> raw;
	z_stream zs{};
	if (inflateInit(&zs) != Z_OK) {
		err = "zlib";
		return false;
	}
	zs.next_in = const_cast<uint8_t*>(&exe[start]);
	zs.avail_in = (uInt)(exe.size() - 8 - start);
	uint8_t buf[65536];
	int r;
	do {
		zs.next_out = buf;
		zs.avail_out = sizeof(buf);
		r = inflate(&zs, Z_NO_FLUSH);
		if (r != Z_OK && r != Z_STREAM_END)
			break;
		raw.insert(raw.end(), buf, buf + (sizeof(buf) - zs.avail_out));
	} while (r != Z_STREAM_END);
	inflateEnd(&zs);
	if (r != Z_STREAM_END) {
		err = "damaged installer data";
		return false;
	}
	size_t p = 0;
	auto u32 = [&](uint32_t& v) {
		if (p + 4 > raw.size())
			return false;
		v = le32(raw, p);
		p += 4;
		return true;
	};
	auto str = [&](std::string& s) {
		uint32_t n;
		if (!u32(n) || p + n > raw.size())
			return false;
		s.assign((const char*)&raw[p], n);
		p += n;
		return true;
	};
	std::string title, credits, about;
	uint32_t count;
	if (!str(title) || !str(credits) || !str(about) || !u32(count)) {
		err = "damaged installer contents";
		return false;
	}
	for (uint32_t i = 0; i < count; i++) {
		ImportFile f;
		uint32_t size;
		if (!str(f.name) || !u32(size) || p + size > raw.size()) {
			err = "damaged installer contents";
			return false;
		}
		f.data.assign(raw.begin() + p, raw.begin() + p + size);
		p += size;
		out.push_back(std::move(f));
	}
	return true;
}

bool KitReadInstaller(const std::vector<uint8_t>& exe, std::vector<std::pair<std::string, std::vector<uint8_t>>>& out, std::string& err) {
	std::vector<ImportFile> files;
	if (!readInstaller(exe, files, err))
		return false;
	for (auto& f : files)
		out.push_back({f.name, std::move(f.data)});
	return true;
}

// An exported creature (.exp) carries its genome as a CGenome record: the
// genes, without the genome file's "dna2" header, ending in "gend". Breeds
// that come without a genome file (goat norns, Kai norns) still have their
// sample creatures' genomes here; this makes a genome file of the first.

static void gather(const fs::path& path, ImportPlan& plan) {
	std::error_code ec;
	if (fs::is_directory(path, ec)) {
		for (auto& e : fs::recursive_directory_iterator(path, ec))
			if (e.is_regular_file())
				gather(e.path(), plan);
		return;
	}
	std::string ext = to_ascii_lowercase(path.extension().string());
	if (ext == ".zip") {
		std::string err;
		if (!readZip(readAll(path), plan.files, err))
			plan.problems.push_back(fmt::format("{}: {}", path.filename().string(), err));
		return;
	}
	if (ext == ".exe") { // a self-extracting zip, or an old add-on installer (read, never run)
		std::string err, zerr;
		auto data = readAll(path);
		if (!readInstaller(data, plan.files, err) && !readZip(data, plan.files, zerr))
			plan.problems.push_back(fmt::format("{}: {}", path.filename().string(), err));
		return;
	}
	if (ext == ".cob") {
		try {
			c2cobfile cob(path);
			int n = 0;
			for (auto b : cob.blocks) {
				if (b->type != "file")
					continue;
				cobFileBlock f(b);
				if (f.filetype != 0)
					continue; // sounds: not part of a breed
				unsigned char* d = f.getFileContents();
				plan.files.push_back({f.filename, std::vector<uint8_t>(d, d + f.filesize)});
				n++;
			}
			if (n == 0)
				plan.problems.push_back(fmt::format("{} holds no pictures (an object, not a breed?)", path.filename().string()));
		} catch (std::exception& e) {
			plan.problems.push_back(fmt::format("{}: {}", path.filename().string(), e.what()));
		}
		return;
	}
	plan.files.push_back({path.filename().string(), readAll(path)});
}

static bool slotFree(int species, int slot, const ImportPlan* plan, const ImportGroup* self) {
	if (const BreedInfo* b = FindBreed(species, slot))
		if (b->inGame || b->inUser)
			return false;
	if (plan)
		for (auto& g : plan->groups)
			if (&g != self && g.install && g.species == species && g.target == slot)
				return false;
	return true;
}

static int firstFreeSlot(int species, const ImportPlan* plan, const ImportGroup* self) {
	for (int s = 0; s < BREED_SLOTS; s++)
		if (slotFree(species, s, plan, self))
			return s;
	return -1;
}

static std::string renamed(const std::string& name, int target, int species = -1);

// an installed breed file (the game's folders or the user's), by name;
// empty if there is none
static std::vector<uint8_t> installedBreedFile(const std::string& name) {
	bool att = to_ascii_lowercase(fs::path(name).extension().string()) == ".att";
	{
		std::error_code ec;
		fs::path p = case_insensitive_filesystem::canonical(BreedDataSubdir(getUserDataDir() / "Overrides", att ? "Body Data" : "Images") / name, ec);
		if (!ec && fs::is_regular_file(p, ec))
			return readAll(p);
	}
	for (auto& d : data_directories) {
		std::error_code ec;
		fs::path p = case_insensitive_filesystem::canonical(BreedDataSubdir(d.main, att ? "Body Data" : "Images") / name, ec);
		if (!ec && fs::is_regular_file(p, ec))
			return readAll(p);
	}
	return {};
}

// Is this breed already installed? Each installed breed of the species is
// compared file by file with the pack's (under that breed's letter): the
// one with the most identical files is noted. Only breeds whose first
// file matches are compared in full.
static void findSameBreed(const ImportPlan& plan, ImportGroup& g) {
	if (g.files.empty())
		return;
	for (auto& b : KnownBreeds()) {
		if (b.species != g.species)
			continue;
		auto& first = plan.files[g.files[0]];
		if (installedBreedFile(renamed(first.name, b.slot)) != first.data)
			continue;
		int same = 0;
		for (size_t i : g.files)
			if (installedBreedFile(renamed(plan.files[i].name, b.slot)) == plan.files[i].data)
				same++;
		if (same > g.sameFiles) {
			g.sameFiles = same;
			g.sameAs = b.slot;
		}
	}
}

// genomes and eggs in the pack that use a slot no breed in the pack fills,
// where an earlier import was moved out of that slot
static void findSlotQuestions(ImportPlan& plan) {
	std::map<std::pair<int, int>, std::string> uses;
	auto note = [&](const std::vector<uint8_t>& genome, const std::string& file) {
		for (auto& k : GenomeAppearanceSlots(genome)) {
			auto& u = uses[k];
			if (u.find(file) == std::string::npos)
				u += (u.empty() ? "" : ", ") + file;
		}
	};
	for (size_t i : plan.genomes)
		note(plan.files[i].data, plan.files[i].name);
	for (size_t i : plan.eggs)
		C2EggWithGenomes(plan.files[i].data, [&](const std::vector<uint8_t>& g) {
			note(g, plan.files[i].name);
			return g;
		});
	for (auto& [k, files] : uses) {
		bool inPack = false;
		for (auto& g : plan.groups)
			inPack |= g.species == k.first && g.slot == k.second;
		if (inPack)
			continue;
		for (auto& b : KnownBreeds()) {
			if (!b.imported || b.species != k.first || b.origSlot != k.second || b.slot == k.second)
				continue;
			SlotQuestion q;
			q.species = k.first;
			q.slot = k.second;
			q.movedTo = b.slot;
			q.breed = BreedLabel(b);
			q.genomes = files;
			// if nothing is in the old slot here, the moved breed is surely meant
			const BreedInfo* there = FindBreed(k.first, k.second);
			q.remap = !there || !(there->inGame || there->inUser);
			plan.slotQuestions.push_back(q);
		}
	}
}

static void planImport(const std::vector<std::string>& picked) {
	auto plan = std::make_unique<ImportPlan>();
	for (auto& p : picked) {
		gather(p, *plan);
		if (!plan->source.empty())
			plan->source += ", ";
		plan->source += fs::path(p).filename().string();
	}
	std::string base = picked.empty() ? "Imported breed" : fs::path(picked[0]).stem().string();
	std::snprintf(plan->name, sizeof(plan->name), "%s", base.c_str());
	std::map<std::pair<int, int>, size_t> groupOf;
	std::vector<ImportFile> exportGenomes;
	for (size_t i = 0; i < plan->files.size(); i++) {
		auto& f = plan->files[i];
		std::string ext = to_ascii_lowercase(fs::path(f.name).extension().string());
		char part;
		int species, stage, slot;
		bool female;
		if ((ext == ".s16" || ext == ".c16" || ext == ".png" || ext == ".att") && ParseBreedFileName(f.name, part, species, female, stage, slot)) {
			auto key = std::make_pair(species, slot);
			if (!groupOf.count(key)) {
				groupOf[key] = plan->groups.size();
				ImportGroup g;
				g.species = species;
				g.slot = g.target = slot;
				plan->groups.push_back(g);
			}
			plan->groups[groupOf[key]].files.push_back(i);
		} else if (ext == ".gen") {
			plan->genomes.push_back(i);
		} else if (ext == ".gno") {
			plan->notes.push_back(i);
		} else if (ext == ".egg") {
			plan->eggs.push_back(i);
		} else if (ext == ".cob") {
			plan->cobs.push_back(i); // an object that comes with the breed (an update, a toy)
		} else if (ext == ".exp") {
			// a sample creature: its genome, for the breed's eggs
			auto g = GenomeFromExport(f.data);
			if (g.empty()) {
				plan->skipped.push_back(i);
				continue;
			}
			exportGenomes.push_back({fs::path(f.name).stem().string() + ".gen", g});
			plan->skipped.push_back(i);
		} else {
			plan->skipped.push_back(i);
		}
	}
	// genomes from the sample creatures come after any genome files (the
	// first genome is the breed's)
	for (auto& g : exportGenomes) {
		plan->files.push_back(std::move(g));
		plan->genomes.push_back(plan->files.size() - 1);
	}
	// a norn breed (by its first genome) filed in another picture set goes
	// to the norn set (owner, 2026-11-11); a clash there asks for a letter
	if (!plan->genomes.empty() && GenomeGenus(plan->files[plan->genomes[0]].data) == 0)
		for (auto& g : plan->groups)
			if (g.species != 0) {
				g.fromSpecies = g.species;
				g.species = 0;
			}
	for (auto& g : plan->groups) {
		findSameBreed(*plan, g);
		if (!slotFree(g.species, g.target, plan.get(), &g)) {
			g.clash = true;
			g.target = firstFreeSlot(g.species, plan.get(), &g);
		}
		if (g.duplicate())
			g.install = false; // already here: not installed again unless asked
	}
	findSlotQuestions(*plan);
	// the brain add-ons of its first genome (or its first egg's), and the
	// roles Zeetures's mind gives them
	{
		std::vector<uint8_t> g;
		if (!plan->genomes.empty())
			g = plan->files[plan->genomes[0]].data;
		else if (!plan->eggs.empty()) {
			C2EggFile e;
			if (ReadC2Egg(plan->files[plan->eggs[0]].data, e))
				g = e.genomes[0];
		}
		if (g.size() > 4) {
			try {
				genomeFile gf;
				SpanReader r(std::span<const uint8_t>(g.data(), g.size()));
				r >> gf;
				plan->brain = BrainProfiles::describe(BrainProfiles::read(gf));
			} catch (...) {
			}
		}
	}
	if (plan->groups.empty())
		plan->problems.push_back(plan->genomes.empty() && plan->eggs.empty() && plan->cobs.empty()
				? "No body-part pictures (like a04k.s16) were found, so there is no breed to install."
				: "No body-part pictures (like a04k.s16): only its genomes, eggs and objects are installed. Its eggs go on the Hatchery's shelf.");
	s_plan = std::move(plan);
}

static std::string renamed(const std::string& name, int target, int species) {
	fs::path p(name);
	std::string stem = to_ascii_lowercase(p.stem().string());
	if (species >= 0 && stem.size() >= 3 && stem[1] >= '0' && stem[1] <= '7')
		stem[1] = (char)('0' + species + (stem[1] >= '4' ? 4 : 0)); // into another set, the same sex
	return stem.substr(0, 3) + breedSlotName(target) + to_ascii_lowercase(p.extension().string());
}


static void installPlan(ImportPlan& plan) {
	fs::path main = getUserDataDir();
	fs::path images = BreedDataSubdir(main, "Images"), bodyData = BreedDataSubdir(main, "Body Data");
	fs::path genetics = BreedDataSubdir(main, "Genetics"), eggsDir = BreedDataSubdir(main, "Eggs");
	std::error_code ec;
	for (auto& d : {images, bodyData, genetics})
		fs::create_directories(d, ec);
	// every move at once, so a gene moved to a slot another breed of the
	// pack is leaving is not moved again
	std::map<std::pair<int, int>, std::pair<int, int>> moves;
	for (auto& grp : plan.groups)
		if (grp.install && grp.target >= 0 && (grp.target != grp.slot || grp.srcSpecies() != grp.species))
			moves[{grp.srcSpecies(), grp.slot}] = {grp.species, grp.target};
	for (auto& q : plan.slotQuestions)
		if (q.remap)
			moves[{q.species, q.slot}] = {q.species, q.movedTo};
	auto moveSlots = [&](const std::vector<uint8_t>& g) {
		return GenomeWithBreedsMoved(g, moves);
	};
	auto write = [&](const fs::path& dir, std::string name, const std::vector<uint8_t>& data, std::vector<std::string>& files) {
		fs::path dest = dir / name;
		// never overwrite: genomes and eggs of the same name get a number;
		// an identical file already there is used as it is (and stays the
		// earlier breed's, so removing this one leaves it)
		for (int k = 2; fs::exists(dest) && k < 100; k++) {
			if (readAll(dest) == data)
				return dest.filename().string();
			dest = dir / fmt::format("{}_{}{}", fs::path(name).stem().string(), k, fs::path(name).extension().string());
		}
		std::ofstream out(dest, std::ios::binary);
		out.write((const char*)data.data(), data.size());
		files.push_back(fs::relative(dest, main, ec).string());
		return dest.filename().string();
	};
	std::vector<std::string> shared; // genomes, notes, eggs: listed with every breed of the pack
	std::string genomeName;
	for (size_t i : plan.genomes) {
		std::string n = write(genetics, to_ascii_lowercase(plan.files[i].name), moveSlots(plan.files[i].data), shared);
		if (genomeName.empty())
			genomeName = n;
	}
	for (size_t i : plan.notes)
		write(genetics, to_ascii_lowercase(plan.files[i].name), plan.files[i].data, shared);
	for (size_t i : plan.eggs) {
		fs::create_directories(eggsDir, ec);
		write(eggsDir, plan.files[i].name, C2EggWithGenomes(plan.files[i].data, moveSlots), shared);
	}
	// objects that came with the breed go where the Agent Injector finds them
	for (size_t i : plan.cobs) {
		fs::path objects = getUserDataDir() / "Objects";
		fs::create_directories(objects, ec);
		write(objects, plan.files[i].name, plan.files[i].data, shared);
	}
	int installed = 0;
	std::vector<BreedInfo> added;
	for (auto& g : plan.groups) {
		if (!g.install || g.target < 0)
			continue;
		BreedInfo b;
		b.species = g.species;
		b.slot = g.target;
		b.imported = true;
		b.name = plan.name;
		b.source = plan.source;
		b.genome = genomeName;
		b.origSlot = g.target != g.slot || g.fromSpecies >= 0 ? g.slot : -1;
		b.origSpecies = g.fromSpecies;
		fs::path imagesHere = images, bodyDataHere = bodyData;
		if (g.inPlaceOfGame) {
			imagesHere = BreedDataSubdir(main / "Overrides", "Images");
			bodyDataHere = BreedDataSubdir(main / "Overrides", "Body Data");
			fs::create_directories(imagesHere, ec);
			fs::create_directories(bodyDataHere, ec);
		}
		for (size_t i : g.files) {
			auto& f = plan.files[i];
			bool att = to_ascii_lowercase(fs::path(f.name).extension().string()) == ".att";
			write(att ? bodyDataHere : imagesHere, renamed(f.name, g.target, g.fromSpecies >= 0 ? g.species : -1), f.data, b.files);
		}
		b.files.insert(b.files.end(), shared.begin(), shared.end());
		added.push_back(b);
		installed++;
	}
	KnownBreeds(true);
	for (auto& b : added) {
		bool merged = false;
		for (auto& k : KnownBreeds())
			if (k.species == b.species && k.slot == b.slot) {
				k.imported = true;
				k.name = b.name;
				k.source = b.source;
				k.genome = b.genome;
				k.files = b.files;
				k.origSlot = b.origSlot;
				k.origSpecies = b.origSpecies;
				merged = true;
			}
		if (!merged)
			KnownBreeds().push_back(b);
	}
	SaveBreedRegistry();
	KnownBreeds(true);
	s_breed_message = installed || shared.empty()
		? fmt::format("Installed {} breed{} from {}.", installed, installed == 1 ? "" : "s", plan.source)
		: fmt::format("Installed {} file{} (genomes, eggs, objects) from {}.", shared.size(), shared.size() == 1 ? "" : "s", plan.source);
}

// a pack imported without the review (Zeetures's recommended packs,
// BreedPacks.cpp): its breeds named name, in their own slots in place of the
// game's when inPlaceOfGame, else where the review would put them. A plan
// the player has open in the kit is kept.
bool ImportBreedPackQuietly(const std::string& path, const std::string& name, bool inPlaceOfGame, std::string& message) {
	auto open = std::move(s_plan);
	std::string openMessage = s_breed_message;
	planImport({path});
	auto plan = std::move(s_plan);
	s_plan = std::move(open);
	bool ok = plan && !plan->groups.empty();
	if (ok) {
		std::snprintf(plan->name, sizeof(plan->name), "%s", name.c_str());
		for (auto& g : plan->groups) {
			g.install = true;
			if (inPlaceOfGame) {
				g.target = g.slot;
				g.inPlaceOfGame = true;
			}
		}
		installPlan(*plan);
		message = s_breed_message;
	} else {
		message = plan && !plan->problems.empty() ? plan->problems[0] : "no breed found in it";
	}
	s_breed_message = openMessage;
	return ok;
}

static void removeBreed(const BreedInfo& b) {
	fs::path main = getUserDataDir();
	int n = 0;
	for (auto& f : b.files) {
		std::error_code ec;
		if (f.find("..") != std::string::npos || fs::path(f).is_absolute())
			continue; // only ever files inside the user data folder
		if (fs::remove(main / f, ec))
			n++;
	}
	for (auto& k : KnownBreeds())
		if (k.species == b.species && k.slot == b.slot)
			k.imported = false;
	SaveBreedRegistry();
	s_breed_message = fmt::format("Removed {} ({} files).", BreedLabel(b), n);
	KnownBreeds(true);
}

// The question about one breed of the pack, as a dialog: its slot is taken
// (put it in the next free one? yes, no, or another slot), or the same breed
// is already installed (skip it, or install a copy).
// breeds being installed keep distinct free slots (the first in the list
// keeps its choice when two want the same one)
static void settleTargets(ImportPlan& plan) {
	for (size_t i = 0; i < plan.groups.size(); i++) {
		auto& g = plan.groups[i];
		if (!g.install)
			continue;
		bool taken = g.target < 0;
		if (const BreedInfo* b = g.target >= 0 ? FindBreed(g.species, g.target) : nullptr)
			taken |= b->inGame || b->inUser;
		for (size_t j = 0; j < i; j++)
			taken |= plan.groups[j].install && plan.groups[j].species == g.species && plan.groups[j].target == g.target;
		if (taken)
			g.target = firstFreeSlot(g.species, &plan, &g);
	}
}

static void SlotDialog(ImportPlan& plan) {
	settleTargets(plan);
	if (plan.asking < 0) {
		for (size_t gi = 0; gi < plan.groups.size(); gi++) {
			auto& g = plan.groups[gi];
			if (!g.asked && (g.clash || g.sameAs >= 0)) {
				plan.asking = (int)gi;
				ImGui::OpenPopup("Breed slot");
				break;
			}
		}
	}
	if (plan.asking < 0)
		return;
	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (!ImGui::BeginPopupModal("Breed slot", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		return;
	auto& g = plan.groups[plan.asking];
	const char* sp = BreedSpeciesName(g.species);
	auto done = [&] {
		g.asked = true;
		plan.asking = -1;
		ImGui::CloseCurrentPopup();
	};
	ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30);
	ImGui::Text("%s: %s breed %s (%d files)", plan.source.c_str(), sp, breedSlotName(g.slot).c_str(), (int)g.files.size());
	ImGui::Spacing();
	const BreedInfo* same = g.sameAs >= 0 ? FindBreed(g.species, g.sameAs) : nullptr;
	if (g.duplicate() && same) {
		ImGui::TextWrapped("This breed is already installed as %s: all its files are the same.", BreedLabel(*same).c_str());
		ImGui::Spacing();
		if (ImGui::Button("Skip it")) {
			g.install = false;
			done();
		}
		ImGui::SameLine();
		if (ImGui::Button(g.target >= 0 ? fmt::format("Install another copy in slot {}", breedSlotName(g.target)).c_str() : "Install another copy")) {
			g.install = g.target >= 0;
			done();
		}
	} else {
		const BreedInfo* there = FindBreed(g.species, g.slot);
		ImGui::TextWrapped("Its slot, %s, is already taken by %s%s.", breedSlotName(g.slot).c_str(), there ? BreedLabel(*there).c_str() : "another breed",
			there && there->inGame && !there->inOverrides ? " (in the game's folders)" : "");
		if (same)
			ImGui::TextWrapped("%d of its %d files are the same as %s's: it may be another version of that breed.", g.sameFiles, (int)g.files.size(),
				BreedLabel(*same).c_str());
		ImGui::Spacing();
		static int other = -1;
		if (g.target < 0) {
			ImGui::TextColored(ImVec4(1, 0.6f, 0.4f, 1), "There is no free %s slot left.", sp);
			if (ImGui::Button("OK")) {
				g.install = false;
				done();
			}
		} else {
			ImGui::Text("Put it in the next free slot, %s?", breedSlotName(g.target).c_str());
			KitNote("Its files are renamed to the new letter, and the pack's genomes and eggs are changed to match.");
			ImGui::Spacing();
			if (ImGui::Button(fmt::format("Yes, slot {}", breedSlotName(g.target)).c_str())) {
				g.install = true;
				done();
			}
			ImGui::SameLine();
			if (ImGui::Button("No, don't install it")) {
				g.install = false;
				done();
			}
			ImGui::SameLine();
			ImGui::TextUnformatted("Other:");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(70);
			if (other < 0 || !slotFree(g.species, other, &plan, &g))
				other = g.target;
			if (ImGui::BeginCombo("##other", breedSlotName(other).c_str())) {
				for (int s = 0; s < BREED_SLOTS; s++)
					if (slotFree(g.species, s, &plan, &g))
						if (ImGui::Selectable(breedSlotName(s).c_str(), s == other))
							other = s;
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			if (ImGui::Button("Use it")) {
				g.target = other;
				g.install = true;
				other = -1;
				done();
			}
		}
	}
	ImGui::PopTextWrapPos();
	ImGui::EndPopup();
}

static void ImportReview() {
	ImportPlan& plan = *s_plan;
	ImGui::SeparatorText("Import");
	ImGui::Text("From: %s", plan.source.c_str());
	ImGui::SetNextItemWidth(260);
	ImGui::InputText("Name", plan.name, sizeof(plan.name));
	for (auto& p : plan.problems)
		ImGui::TextColored(ImVec4(1, 0.6f, 0.4f, 1), "%s", p.c_str());
	if (!plan.brain.empty()) {
		ImGui::TextUnformatted("Its brain, in Zeetures:");
		ImGui::PushTextWrapPos(0);
		for (auto& b : plan.brain)
			ImGui::BulletText("%s", b.c_str());
		ImGui::PopTextWrapPos();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("The original brain is not run: each added lobe gets a role in Zeetures's mind (brain-profiles.txt).");
	}
	SlotDialog(plan);
	for (size_t gi = 0; gi < plan.groups.size(); gi++) {
		auto& g = plan.groups[gi];
		ImGui::PushID((int)gi);
		ImGui::Checkbox("##install", &g.install);
		ImGui::SameLine();
		ImGui::Text("%s breed %s, %d files ->", BreedSpeciesName(g.species), breedSlotName(g.slot).c_str(), (int)g.files.size());
		ImGui::SameLine();
		ImGui::SetNextItemWidth(70);
		if (ImGui::BeginCombo("##slot", g.target < 0 ? "none free" : breedSlotName(g.target).c_str())) {
			for (int s = 0; s < BREED_SLOTS; s++)
				if (s == g.target || slotFree(g.species, s, &plan, &g))
					if (ImGui::Selectable(breedSlotName(s).c_str(), s == g.target))
						g.target = s;
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		const BreedInfo* same = g.sameAs >= 0 ? FindBreed(g.species, g.sameAs) : nullptr;
		if (g.duplicate() && same)
			ImGui::TextColored(ImVec4(0.6f, 0.85f, 1, 1), "(already installed as %s)", BreedLabel(*same).c_str());
		else if (g.target == g.slot)
			ImGui::TextDisabled("(its own slot is free)");
		else
			ImGui::TextColored(ImVec4(1, 0.85f, 0.4f, 1), "(%s is taken: moved)", breedSlotName(g.slot).c_str());
		ImGui::PopID();
	}
	for (size_t qi = 0; qi < plan.slotQuestions.size(); qi++) {
		auto& q = plan.slotQuestions[qi];
		ImGui::PushID(1000 + (int)qi);
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 520);
		ImGui::TextWrapped("%s uses %s slot %s, which %s was moved out of when it was imported.", q.genomes.c_str(), BreedSpeciesName(q.species),
			breedSlotName(q.slot).c_str(), q.breed.c_str());
		ImGui::PopTextWrapPos();
		ImGui::Checkbox(fmt::format("Point it at {} (slot {})", q.breed, breedSlotName(q.movedTo)).c_str(), &q.remap);
		ImGui::PopID();
	}
	if (!plan.genomes.empty() || !plan.eggs.empty() || !plan.notes.empty())
		KitNote("Also: %d genome%s, %d note file%s, %d egg%s (breed slots in them follow any move).",
			(int)plan.genomes.size(), plan.genomes.size() == 1 ? "" : "s", (int)plan.notes.size(), plan.notes.size() == 1 ? "" : "s",
			(int)plan.eggs.size(), plan.eggs.size() == 1 ? "" : "s");
	if (!plan.cobs.empty())
		ImGui::TextDisabled("And %d object%s for the Agent Injector.", (int)plan.cobs.size(), plan.cobs.size() == 1 ? "" : "s");
	if (!plan.skipped.empty()) {
		std::string names;
		for (size_t i : plan.skipped)
			names += (names.empty() ? "" : ", ") + plan.files[i].name;
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 520);
		ImGui::TextDisabled("Not used: %s", names.c_str());
		ImGui::PopTextWrapPos();
	}
	ImGui::TextDisabled("Files go to your openc2e data folder, never the game's.");
	bool any = plan.groups.empty() && (!plan.genomes.empty() || !plan.eggs.empty() || !plan.cobs.empty());
	for (auto& g : plan.groups)
		any |= g.install && g.target >= 0;
	if (!any)
		ImGui::BeginDisabled();
	if (ImGui::Button("Install")) {
		installPlan(plan);
		s_plan.reset();
	}
	if (!any)
		ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Cancel"))
		s_plan.reset();
}

static void BreedsPage() {
	{
		std::vector<std::string> picked;
		{
			std::lock_guard<std::mutex> lock(s_pick_mutex);
			picked.swap(s_picked);
		}
		if (!picked.empty())
			planImport(picked);
	}
	static int sel_species = -1, sel_slot = -1;
	auto& breeds = KnownBreeds();
	// developer aid: OPENC2E_TEST_BREED_SELECT="species,slot letter" (e.g.
	// "0,l") shows that breed on the Breeds page
	if (const char* t = getenv("OPENC2E_TEST_BREED_SELECT")) {
		static bool picked = false;
		char letter[8] = {};
		int sp;
		if (!picked && sscanf(t, "%d,%7s", &sp, letter) == 2) {
			picked = true;
			sel_species = sp;
			sel_slot = breedSlotFromName(letter);
		}
	}
	// developer aid: OPENC2E_TEST_BREED_PLAN="pack;pack" reads the packs as if
	// picked, leaving the review (and its questions) open, and prints what it
	// found about each breed
	if (const char* t = getenv("OPENC2E_TEST_BREED_PLAN")) {
		static bool done = false;
		if (!done) {
			done = true;
			std::vector<std::string> packs;
			std::string list = t;
			for (size_t a = 0, b; a < list.size(); a = b + 1) {
				b = list.find(';', a);
				if (b == std::string::npos)
					b = list.size();
				packs.push_back(list.substr(a, b - a));
			}
			planImport(packs);
			for (auto& g : s_plan->groups)
				fmt::print("* Breed plan: species {} slot {} -> {} ({} files) clash {} same as {} ({} files) install {}\n", g.species, breedSlotName(g.slot),
					g.target < 0 ? "-" : breedSlotName(g.target), g.files.size(), g.clash, g.sameAs < 0 ? "-" : breedSlotName(g.sameAs), g.sameFiles, g.install);
			for (auto& q : s_plan->slotQuestions)
				fmt::print("* Breed plan: {} use species {} slot {}: {} moved to {} (remap {})\n", q.genomes, q.species, breedSlotName(q.slot), q.breed,
					breedSlotName(q.movedTo), q.remap);
		}
	}
	// developer aid: OPENC2E_TEST_BREED_IMPORT=pack imports it at once and
	// shows the breed it became, printing a genome rewritten to wear it
	if (const char* t = getenv("OPENC2E_TEST_BREED_IMPORT")) {
		static bool done = false;
		if (!done) {
			done = true;
			planImport({t});
			for (auto& pr : s_plan->problems)
				fmt::print("* Breed import problem: {}\n", pr);
			for (auto& b : s_plan->brain)
				fmt::print("* Breed import brain: {}\n", b);
			for (auto& g : s_plan->groups)
				fmt::print("* Breed import: species {} slot {} -> {} ({} files)\n", g.species, breedSlotName(g.slot), breedSlotName(g.target), g.files.size());
			if (!s_plan->groups.empty()) {
				sel_species = s_plan->groups[0].species;
				sel_slot = s_plan->groups[0].target;
			}
			installPlan(*s_plan);
			s_plan.reset();
			fmt::print("* {}\n", s_breed_message);
			if (const BreedInfo* b = FindBreed(sel_species, sel_slot)) {
				auto g = GenomeWithBreed(BreedBaseGenome(*b), *b);
				genomeFile gf;
				SpanReader r(std::span<const uint8_t>(g.data(), g.size()));
				r >> gf;
				for (auto& gp : gf.genes)
					if (auto* a = dynamic_cast<creatureAppearanceGene*>(gp.get()))
						fmt::print("* rewritten genome ({} bytes): appearance part {} species {} slot {}\n", g.size(), (int)a->part, geneBreedSpecies(a->species), breedSlotName(geneBreedSlot(a->species, a->variant)));
			}
		}
	}
	if (sel_species < 0 && !breeds.empty()) {
		sel_species = breeds[0].species;
		sel_slot = breeds[0].slot;
	}

	// developer aid: OPENC2E_MOVE_NORN_BREEDS=1 moves every imported norn
	// breed filed in another set into the norn set, at its default letter
	static bool movedNorns = false;
	if (!movedNorns && getenv("OPENC2E_MOVE_NORN_BREEDS")) {
		movedNorns = true;
		std::vector<BreedInfo> todo;
		for (auto& b : KnownBreeds())
			if (b.imported && b.kind == 0 && b.species != 0)
				todo.push_back(b);
		for (auto& b : todo) {
			std::string err;
			int slot = DefaultNornSlotFor(b);
			bool ok = MoveBreedToNornSet(b, slot, err);
			fmt::print("* MOVE NORN BREED {} ({} {}) -> norn {}: {}\n", b.name, BreedSpeciesName(b.species), breedSlotName(b.slot), breedSlotName(slot), ok ? "done" : err);
		}
	}

	// developer aid: OPENC2E_MOVE_BREED="species,slot,toSpecies,toSlot" (slot
	// letters, e.g. "0,ak,0,ka") moves one imported breed once
	static bool movedOne = false;
	if (!movedOne && getenv("OPENC2E_MOVE_BREED")) {
		movedOne = true;
		int sp = -1, tsp = -1;
		char from[8] = "", to[8] = "";
		if (sscanf(getenv("OPENC2E_MOVE_BREED"), "%d,%7[a-z],%d,%7[a-z]", &sp, from, &tsp, to) == 4) {
			std::string err;
			const BreedInfo* b = FindBreed(sp, breedSlotFromName(from));
			bool ok = b && MoveBreedTo(BreedInfo(*b), tsp, breedSlotFromName(to), err);
			fmt::print("* MOVE BREED {} {} -> {} {}: {}\n", sp, from, tsp, to, ok ? "done" : b ? err : "no such breed");
		}
	}

	// the list, by species
	ImGui::BeginChild("list", ImVec2(230, 330), ImGuiChildFlags_Borders);
	for (int sp = 0; sp < 4; sp++) {
		int n = 0;
		for (auto& b : breeds)
			n += b.group() == sp;
		if (n == 0)
			continue;
		if (!ImGui::TreeNodeEx(BreedSpeciesName(sp), ImGuiTreeNodeFlags_DefaultOpen, "%s (%d breeds)", BreedSpeciesName(sp), n))
			continue;
		// in order of slot letter (breeds filed in other picture sets among
		// them; two may share a letter from different sets)
		std::vector<const BreedInfo*> inGroup;
		for (auto& b : breeds)
			if (b.group() == sp)
				inGroup.push_back(&b);
		std::stable_sort(inGroup.begin(), inGroup.end(), [](const BreedInfo* x, const BreedInfo* y) {
			return x->slot != y->slot ? x->slot < y->slot : x->species < y->species;
		});
		for (const BreedInfo* bp : inGroup) {
			const BreedInfo& b = *bp;
			std::string label = b.name.empty() ? fmt::format("Breed {}", breedSlotName(b.slot)) : fmt::format("{}  ({})", b.name, breedSlotName(b.slot));
			if (b.inUser && (!b.inGame || b.inOverrides))
				label += "  *";
			if (ImGui::Selectable(fmt::format("{}##{}_{}", label, b.species, b.slot).c_str(), sel_species == b.species && sel_slot == b.slot)) {
				sel_species = b.species; // its picture set and slot identify it
				sel_slot = b.slot;
			}
		}
		ImGui::TreePop();
	}
	ImGui::EndChild();
	ImGui::SameLine();

	// the chosen breed
	ImGui::BeginGroup();
	const BreedInfo* b = FindBreed(sel_species, sel_slot);
	if (!b) {
		ImGui::TextDisabled("Pick a breed to see it.");
		ImGui::TextDisabled("* marks breeds in your own data folder.");
	} else {
		ImGui::TextUnformatted(BreedLabel(*b).c_str());
		ImGui::TextDisabled("%s", b->inOverrides ? (b->inGame ? "Installed in your data folder, in place of the game's" : "Installed in your data folder")
														 : b->inGame ? (b->jointsAdjusted ? "In the game's folders (joints corrected in your data folder)" : "In the game's folders")
																	 : "Installed in your data folder");
		// portraits: the adult face (or the oldest there is), each sex
		int stage = b->stages.empty() ? 0 : std::min(4, *b->stages.rbegin());
		ImVec2 p = ImGui::GetCursorScreenPos();
		float x = 0, tallest = 0;
		for (bool female : {false, true}) {
			std::string f = partFile(*b, 'a', female, stage);
			Texture t = f.empty() ? Texture() : breedPicture(f, 8);
			if (!t)
				continue;
			ImGui::GetWindowDrawList()->AddRectFilled(p + ImVec2(x, 0), p + ImVec2(x + t.width * 2 + 8, t.height * 2 + 8), IM_COL32(60, 70, 90, 255), 4);
			ImGui::GetWindowDrawList()->AddImage(t.as<ImTextureID>(), p + ImVec2(x + 4, 4), p + ImVec2(x + 4 + t.width * 2, 4 + t.height * 2));
			ImGui::GetWindowDrawList()->AddText(p + ImVec2(x + 4, t.height * 2 + 10), IM_COL32(200, 200, 210, 255), female ? "female" : "male");
			x += t.width * 2 + 16;
			tallest = std::max(tallest, (float)t.height * 2 + 8);
		}
		if (x > 0)
			ImGui::Dummy(ImVec2(x, tallest + 22));
		std::set<std::string> parts;
		for (char c : b->parts)
			parts.insert(partName(c));
		std::string ps, ss;
		for (auto& s : parts)
			ps += (ps.empty() ? "" : ", ") + s;
		const char* stages[] = {"baby", "child", "adolescent", "youth", "adult", "old", "senile"};
		for (int s : b->stages)
			ss += fmt::format("{}{}", ss.empty() ? "" : ", ", s >= 0 && s < 7 ? stages[s] : "?");
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 300);
		ImGui::Text("Parts: %s", ps.c_str());
		ImGui::Text("Ages: %s", ss.c_str());
		ImGui::Text("%s%s%s", b->male ? "male" : "", b->male && b->female ? " and " : "", b->female ? "female" : "");
		if (!b->bodyData)
			KitNoteColored(ImVec4(1, 0.7f, 0.4f, 1), "No body data (.att) in this slot: creatures fall back to another breed's.");
		if (b->imported && !b->source.empty())
			ImGui::TextDisabled("From %s", b->source.c_str());
		if (const char* by = b->imported ? PackCredit(b->source) : nullptr)
			ImGui::TextWrapped("By %s", by);
		if (b->kind >= 0 && b->kind != b->species)
			KitNote("Its pictures are filed in the %s set (slot %s): the pack's authors put them there.", BreedSpeciesName(b->species),
				breedSlotName(b->slot).c_str());
		if (b->origSpecies >= 0)
			KitNote("Moved from the %s set, slot %s; the old files are kept for creatures hatched before.", BreedSpeciesName(b->origSpecies),
				breedSlotName(b->origSlot >= 0 ? b->origSlot : b->slot).c_str());
		// a norn breed filed in another set: move it into the norn set
		// (owner, 2026-11-11)
		if (b->imported && b->kind == 0 && b->species != 0) {
			static char letter[8] = "";
			static const BreedInfo* forBreed = nullptr;
			if (forBreed != b) {
				forBreed = b;
				int d = DefaultNornSlotFor(*b);
				std::snprintf(letter, sizeof(letter), "%s", d >= 0 ? breedSlotName(d).c_str() : "");
			}
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Move to the norn set as");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(50);
			ImGui::InputText("##nornletter", letter, sizeof(letter));
			ImGui::SameLine();
			if (ImGui::Button("Move")) {
				int slot = breedSlotFromName(letter);
				std::string err;
				if (slot < 0)
					s_breed_message = "That is not a slot name (a-z, then aa ... zz, then aaa ...).";
				else if (MoveBreedToNornSet(*b, slot, err)) {
					s_breed_message = fmt::format("Moved to norn slot {}.", breedSlotName(slot));
					sel_species = 0;
					sel_slot = slot;
					forBreed = nullptr;
				} else
					s_breed_message = "Couldn't move it: " + err;
			}
		}
		// an imported breed to another slot in its own set (owner, 2026-11-12:
		// the Kai norns to ka, past the old iv limit)
		if (b->imported && !(b->kind == 0 && b->species != 0)) {
			static char slotName[8] = "";
			static const BreedInfo* forMove = nullptr;
			if (forMove != b) {
				forMove = b;
				slotName[0] = 0;
			}
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Move to %s slot", BreedSpeciesName(b->species));
			ImGui::SameLine();
			ImGui::SetNextItemWidth(50);
			ImGui::InputText("##moveslot", slotName, sizeof(slotName));
			ImGui::SameLine();
			if (ImGui::Button("Move##toslot")) {
				int slot = breedSlotFromName(slotName);
				std::string err;
				if (slot < 0)
					s_breed_message = "That is not a slot name (a-z, then aa ... zz, then aaa ...).";
				else if (MoveBreedTo(BreedInfo(*b), b->species, slot, err)) {
					s_breed_message = fmt::format("Moved to slot {}.", breedSlotName(slot));
					sel_slot = slot;
					forMove = nullptr;
				} else
					s_breed_message = "Couldn't move it: " + err;
			}
		}
		if (b->imported && b->origSlot >= 0 && b->origSlot != b->slot)
			KitNote("Moved from slot %s; its files and genomes were changed to match.", breedSlotName(b->origSlot).c_str());
		ImGui::PopTextWrapPos();

		bool canLay = b->parts.count('a') && b->parts.count('b');
		// the egg's sex (owner, 2026-11-11): either at random, or the one
		// chosen, among the sexes the breed has parts for
		static int s_eggSex = 0; // 0 either, 1 male, 2 female (as the egg carries it)
		if (s_eggSex == 1 && !b->male)
			s_eggSex = 0;
		if (s_eggSex == 2 && !b->female)
			s_eggSex = 0;
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Egg:");
		ImGui::SameLine();
		ImGui::RadioButton("Either##eggsex", &s_eggSex, 0);
		ImGui::SameLine();
		if (!b->female)
			ImGui::BeginDisabled();
		ImGui::RadioButton("Female##eggsex", &s_eggSex, 2);
		if (!b->female)
			ImGui::EndDisabled();
		ImGui::SameLine();
		if (!b->male)
			ImGui::BeginDisabled();
		ImGui::RadioButton("Male##eggsex", &s_eggSex, 1);
		if (!b->male)
			ImGui::EndDisabled();
		if (!canLay)
			ImGui::BeginDisabled();
		// developer aid: OPENC2E_TEST_BREED_LAY=1 lays one egg of the breed shown
		static bool testLay = getenv("OPENC2E_TEST_BREED_LAY") != nullptr;
		bool layNow = ImGui::Button("Lay an egg of this breed") || (testLay && canLay);
		if (layNow && testLay)
			testLay = false;
		if (layNow) {
			auto genome = BreedBaseGenome(*b);
			if (genome.empty()) {
				s_breed_message = "No genome to start from (norn.gen, gren.gen or ettn.gen).";
			} else {
				std::string err;
				static int s_laid = 0;
				genome = GenomeWithBreed(genome, *b);
				int sex = s_eggSex;
				if (sex == 0) // either: at random among the sexes it has
					sex = b->male && b->female ? rand_int32(1, 2) : b->female ? 2 : b->male ? 1 : rand_int32(1, 2);
				if (LayC2Egg({genome}, sex, rand_int32(0, 10), fmt::format("b{:02d}", s_laid++ % 100), err)) {
					s_breed_message = fmt::format("Laid a {} egg of {} by the incubator.", sex == 2 ? "female" : "male", BreedLabel(*b));
					fmt::print("* {}\n", s_breed_message);
				}
				else
					s_breed_message = "Couldn't lay the egg: " + err;
			}
		}
		if (!canLay)
			ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip(canLay ? "A new egg near the incubator from %s, wearing this breed's parts." : "This breed has no head or no body.",
				b->genome.empty() ? "the species' standard genome" : b->genome.c_str());
		if (b->imported) {
			static char rename[64];
			static int renaming = -1;
			if (renaming != b->species * 1000 + b->slot) {
				renaming = b->species * 1000 + b->slot;
				std::snprintf(rename, sizeof(rename), "%s", b->name.c_str());
			}
			ImGui::SetNextItemWidth(180);
			if (ImGui::InputText("##rename", rename, sizeof(rename), ImGuiInputTextFlags_EnterReturnsTrue)) {
				for (auto& k : KnownBreeds())
					if (k.species == b->species && k.slot == b->slot)
						k.name = rename;
				SaveBreedRegistry();
			}
			ImGui::SameLine();
			ImGui::TextDisabled("name (Enter)");
			if (ImGui::Button("Remove..."))
				ImGui::OpenPopup("remove");
			if (ImGui::BeginPopup("remove")) {
				ImGui::Text("Delete the %d files this breed installed in your data folder?", (int)b->files.size());
				ImGui::TextDisabled("Creatures wearing it will use another breed's parts.");
				if (ImGui::Button("Delete them")) {
					removeBreed(*b);
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button("Keep"))
					ImGui::CloseCurrentPopup();
				ImGui::EndPopup();
			}
		}
	}
	ImGui::EndGroup();

	if (!s_breed_message.empty())
		ImGui::TextUnformatted(s_breed_message.c_str());
	if (s_plan) {
		ImportReview();
		return;
	}
	if (ImGui::Button("Import files...")) {
		static const SDL_DialogFileFilter filters[] = {{"Breed packs (zip, installer exe, COB, sprites, body data, genomes, eggs, exported creatures)", "zip;exe;cob;s16;c16;png;att;gen;gno;egg;exp"}, {"All files", "*"}};
		SDL_ShowOpenFileDialog(BreedFilesChosen, nullptr, nullptr, filters, 2, nullptr, true);
	}
	ImGui::SameLine();
	if (ImGui::Button("Import a folder..."))
		SDL_ShowOpenFolderDialog(BreedFilesChosen, nullptr, nullptr, nullptr, false);
	ImGui::SameLine();
	if (ImGui::Button("Rescan"))
		KnownBreeds(true);
	KitNote("Slots run a-z, then aa ... zz, then aaa ... (16,384 for each species; a creature in a slot past iv cannot go back to the original game).");
}

void DrawBreedersKit() {
	KitCreatureHeader();
	if (ImGui::BeginTabBar("pages")) {
		if (KitTab("Reproductive")) {
			ReproductivePage();
			ImGui::EndTabItem();
		}
		if (KitCreature() && KitCreature()->isFemale() && KitTab("Insemination")) {
			InseminationPage();
			ImGui::EndTabItem();
		}
		if (KitTab("Aphrodisiacs")) {
			AphrodisiacsPage();
			ImGui::EndTabItem();
		}
		if (KitTab("Breeds")) {
			BreedsPage();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

} // namespace Openc2eImgui
