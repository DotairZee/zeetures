/*
 *  ImportCreature.cpp
 *  openc2e
 *
 *  File > Import Creature: a C2 exported creature (.exp) comes into the
 *  world as a living creature (owner, 2026-11-10). openc2e does not read
 *  C2's whole saved creature (its body, chemistry and C2 brain); what is
 *  taken from the export is what makes it that creature: its genome, name,
 *  sex and life stage. It is made anew from those (its chemistry at its
 *  genes' starting levels, its mind fresh) and handed to the hand to place.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 */

#include "ImportCreature.h"
#include "common/devenv.h"

#include "Engine.h"
#include "PointerAgent.h"
#include "World.h"
#include "common/creaturesImage.h"
#include "common/io/SpanReader.h"
#include "creatures/BreedSlots.h"
#include "creatures/Creature.h"
#include "creatures/oldCreature.h"
#include "creatures/SkeletalCreature.h"
#include "fileformats/genomeFile.h"
#include "historyManager.h"
#include "PathResolver.h"
#include "WorldSave.h"
#include "creatures/CreatureAgent.h"
#include "imageManager.h"
#include "PixelText.h"
#include "WordLists.h"
#include "creatures/CreatureSpeech.h"

#include <SDL3/SDL_dialog.h>
#include <algorithm>
#include <cstring>
#include <fmt/core.h>
#include <fstream>
#include <imgui.h>
#include <iterator>
#include <mutex>

namespace fs = std::filesystem;

namespace {

std::vector<uint8_t> readAll(const fs::path& p) {
	std::ifstream in(p, std::ios::binary);
	return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), {});
}

size_t find(const std::vector<uint8_t>& d, const std::string& s, size_t from = 0) {
	auto it = std::search(d.begin() + std::min(from, d.size()), d.end(), s.begin(), s.end());
	return it == d.end() ? std::string::npos : (size_t)(it - d.begin());
}

size_t rfind(const std::vector<uint8_t>& d, const std::string& s, size_t before) {
	if (s.size() > before)
		return std::string::npos;
	for (size_t i = before - s.size() + 1; i-- > 0;)
		if (std::memcmp(d.data() + i, s.data(), s.size()) == 0)
			return i;
	return std::string::npos;
}

int32_t i32(const std::vector<uint8_t>& d, size_t at) {
	if (at + 4 > d.size())
		return 0;
	int32_t v;
	std::memcpy(&v, d.data() + at, 4);
	return v;
}

// a length-prefixed string at "at" (one byte of length); false past the end
bool pstr(const std::vector<uint8_t>& d, size_t& at, std::string& out) {
	if (at >= d.size() || at + 1 + d[at] > d.size())
		return false;
	out.assign((const char*)d.data() + at + 1, d[at]);
	at += 1 + d[at];
	return true;
}

struct Exported {
	std::vector<uint8_t> genome; // a genome file ("dna2" and the genes)
	std::string moniker, name;
	int sex = 0; // 1 male, 2 female, 0 unknown
	std::vector<std::pair<int, int>> headFrames; // the first pictures of its gallery: its head
	// its mother and father in C2 (moniker and name; empty if none, or a
	// starting genome's name rather than a creature's), when and where it
	// was born, and its breeder's note
	std::string mother, motherName, father, fatherName, born, birthplace, note;
	// the words it knew: slot of the game's word list, and how well (C2's
	// 0-255: "pull" said right at 240, "push" said "puth" at 120)
	std::vector<std::pair<int, int>> words;
};

// a creature's moniker in C2: a digit and three capital letters ("2BAQ";
// breed packs put a starting genome's name there instead: "toby", "MLAM")
bool isMoniker(const std::string& m) {
	return m.size() == 4 && m[0] >= '0' && m[0] <= '9' && std::all_of(m.begin() + 1, m.end(), [](char ch) { return ch >= 'A' && ch <= 'Z'; });
}

// one word of its vocabulary: the word (as the list has it, or babble
// before it is learned), how it says it, and how well (32-bit); false if
// what is there is not one
bool vocabEntry(const std::vector<uint8_t>& d, size_t& at, std::string& word, std::string& said, int& strength) {
	size_t p = at;
	if (p >= d.size() || d[p] == 0 || d[p] > 20 || !pstr(d, p, word))
		return false;
	if (p >= d.size() || d[p] == 0 || d[p] > 24 || !pstr(d, p, said))
		return false;
	if (p + 4 > d.size())
		return false;
	strength = i32(d, p);
	if (strength < 0 || strength > 255)
		return false;
	for (char ch : word + said)
		if ((unsigned char)ch < 32 || (unsigned char)ch > 126)
			return false;
	at = p + 4;
	return true;
}

// What an export holds, as far as is used here. The layout was read from
// exports of four C2 breed packs (Midsummer, Scorpio): a "Creature" record
// whose picture gallery (CGallery) is named after its moniker and starts with
// its head's pictures (width and height of each); near the end its history
// record (moniker, name, parents, birth time and place, then numbers, the
// third of them its sex) and its genome (CGenome: the genes, ending "gend").
bool readExport(const std::vector<uint8_t>& d, Exported& e, std::string& err) {
	size_t g = find(d, "CGenome");
	if (g == std::string::npos) {
		err = "no genome in it (is it a Creatures 2 exported creature?)";
		return false;
	}
	size_t first = find(d, "gene", g), end = find(d, "gend", first);
	if (first == std::string::npos || end == std::string::npos) {
		err = "its genome is incomplete";
		return false;
	}
	e.genome = {'d', 'n', 'a', '2'};
	e.genome.insert(e.genome.end(), d.begin() + first, d.begin() + end + 4);

	// the gallery: picture count, its name (the moniker), then entries of
	// three bytes and a width, height and offset (32-bit each)
	size_t gal = find(d, "CGallery");
	if (gal != std::string::npos && gal + 8 + 8 + 8 < d.size()) {
		size_t p = gal + 8;
		e.moniker.assign((const char*)d.data() + p + 4, 4);
		p += 8 + 8;
		for (int k = 0; k < 6 && p + 15 <= d.size(); k++) {
			p += 3;
			e.headFrames.push_back({i32(d, p), i32(d, p + 4)});
			p += 12;
		}
	}
	// the history record: the moniker and name; its mother's and father's
	// moniker (4) and name; the time and place of its birth; four more
	// strings (an owner's name, one unused, the breeder's note, an address);
	// a number, then its sex (1 male, 2 female). (Read from twelve exports
	// of five breed packs on the Eem Foo archive, 2026-11-26: Scorpio,
	// Marriage, Speciation and Kai Norns, Drozzy. The parents' names were
	// once taken to be always empty, which lost the sex of a creature with
	// named parents, and the four strings likewise.)
	if (!e.moniker.empty()) {
		size_t h = rfind(d, e.moniker, g);
		size_t p = h == std::string::npos ? std::string::npos : h + 4;
		std::string owner, unused, address;
		if (p != std::string::npos && pstr(d, p, e.name) && p + 4 <= d.size()) {
			std::string mother((const char*)d.data() + p, 4), father;
			p += 4;
			bool ok = pstr(d, p, e.motherName) && p + 4 <= d.size();
			if (ok) {
				father.assign((const char*)d.data() + p, 4);
				p += 4;
				ok = pstr(d, p, e.fatherName) && pstr(d, p, e.born) && pstr(d, p, e.birthplace);
			}
			if (ok) {
				if (isMoniker(mother))
					e.mother = mother;
				else
					e.motherName.clear();
				if (isMoniker(father))
					e.father = father;
				else
					e.fatherName.clear();
				size_t q = p;
				if (pstr(d, q, owner) && pstr(d, q, unused) && pstr(d, q, e.note) && pstr(d, q, address)) {
					int sex = i32(d, q + 4);
					if (sex == 1 || sex == 2)
						e.sex = sex;
				}
				if (!e.sex) {
					int sex = i32(d, p + 8); // (four empty strings)
					if (sex == 1 || sex == 2)
						e.sex = sex;
				}
			}
		}
	}

	// its words: 77 of them, one for each slot of the game's word list, in
	// the creature's own record, after its body (Limb) and before its brain
	// (CBrain); found as the run of entries that are words
	size_t from = find(d, "Limb"), to = find(d, "CBrain");
	if (from != std::string::npos && to != std::string::npos && from < to)
		for (size_t start = from; start < to && e.words.empty(); start++) {
			size_t p = start;
			std::vector<std::tuple<std::string, std::string, int>> run;
			std::string word, said;
			int strength;
			while (vocabEntry(d, p, word, said, strength))
				run.push_back({word, said, strength});
			if (run.size() < 60)
				continue;
			// a slot counts only if it holds the list's own word there
			auto& en = WordLists::english();
			for (size_t k = 0; k < run.size() && k < en.size(); k++)
				if (!en[k].empty() && PixelText::lower(std::get<0>(run[k])) == PixelText::lower(en[k]))
					e.words.push_back({(int)k, std::get<2>(run[k])});
			break;
		}
	return true;
}

// The life stage it was exported at: the age whose head pictures (of its
// breed and sex, as the engine would find them) have the same sizes as the
// export's own; the youngest if several do, a baby if none
int stageOf(const Exported& e, std::shared_ptr<genomeFile> genome, bool female) {
	int species = -1, variant = -1;
	for (auto& gn : genome->genes)
		if (auto* a = dynamic_cast<creatureAppearanceGene*>(gn.get()))
			if (a->part == 0) {
				species = geneBreedSpecies(a->species);
				variant = geneBreedSlot(a->species, a->variant);
				break;
			}
	if (species < 0 || e.headFrames.empty())
		return 0;
	for (int age = 0; age <= 6; age++) {
		std::string name = "a";
		name += (char)('0' + species + (female ? 4 : 0));
		name += (char)('0' + age);
		name += breedSlotName(variant);
		auto img = world.gallery->getImage(name);
		if (!img)
			continue;
		bool same = img->numframes() >= e.headFrames.size();
		for (size_t k = 0; same && k < e.headFrames.size(); k++)
			same = (int)img->width(k) == e.headFrames[k].first && (int)img->height(k) == e.headFrames[k].second;
		if (same)
			return age;
	}
	return 0;
}

std::mutex s_mutex;
std::vector<std::string> s_picked;
std::string s_message;

// File > Export Current Creature: the creature asked about, whether to ask,
// and the file the save dialog chose (from its own thread)
AgentRef s_exporting;
bool s_askExport = false;
std::vector<std::string> s_exportTo;

void chosenExport(void*, const char* const* files, int) {
	if (!files || !*files)
		return;
	std::lock_guard<std::mutex> lock(s_mutex);
	s_exportTo.push_back(*files);
}

std::string creatureName(Agent* a) {
	auto* ca = dynamic_cast<CreatureAgent*>(a);
	if (!ca || !ca->getCreature())
		return "this creature";
	std::string mon = world.history->findMoniker(ca->getCreature()->getGenome());
	const std::string& name = world.history->getMoniker(mon).name;
	return name.empty() ? mon : name;
}

void exportTo(std::string path, std::string& message) {
	if (fs::path(path).extension().empty())
		path += ".zex";
	auto a = s_exporting.lock();
	std::string name = a ? creatureName(a.get()) : "";
	std::string err;
	if (WorldSave::ExportCreature(a, path, err))
		message = fmt::format("{} was exported to {}. File > Import Creature brings it back, in this world or another.", name, path);
	else
		message = err;
	s_exporting = AgentRef();
}

void chosen(void*, const char* const* files, int) {
	if (!files)
		return;
	std::lock_guard<std::mutex> lock(s_mutex);
	for (auto f = files; *f; f++)
		s_picked.push_back(*f);
}

} // namespace

bool ImportExportedCreature(const fs::path& path, std::string& message) {
	if (engine.version != 2) {
		message = "Importing creatures works in Creatures 2 worlds.";
		return false;
	}
	// one of Zeetures's own (.zex): the whole creature
	if (WorldSave::IsCreatureExport(path))
		return WorldSave::ImportCreatureExport(path, message);
	Exported e;
	std::string err;
	if (!readExport(readAll(path), e, err)) {
		message = fmt::format("Couldn't import {}: {}", path.filename().string(), err);
		return false;
	}
	auto genome = std::make_shared<genomeFile>();
	try {
		SpanReader r(std::span<const uint8_t>(e.genome.data(), e.genome.size()));
		r >> *genome;
	} catch (std::exception& ex) {
		message = fmt::format("Couldn't import {}: its genome can't be read ({})", path.filename().string(), ex.what());
		return false;
	}
	bool female = e.sex ? e.sex == 2 : rand() % 2;
	int stage = stageOf(e, genome, female);

	SkeletalCreature* a = new SkeletalCreature(4);
	oldCreature* c;
	try {
		c = new c2Creature(genome, female, 0, a);
	} catch (std::exception& ex) {
		delete a;
		message = fmt::format("Couldn't import {}: {}", path.filename().string(), ex.what());
		return false;
	}
	a->setCreature(c);
	try {
		a->finishInit();
	} catch (std::exception& ex) {
		a->kill(); // (never left half made in the world: its next step crashes the game)
		message = fmt::format("Couldn't import {}: {}", path.filename().string(), ex.what());
		return false;
	}
	a->genome_slots[0] = genome;
	world.newMoniker(genome, "imported", a);
	std::string mon = world.history->findMoniker(genome);
	world.history->getMoniker(mon).moveToCreature(a);
	monikerData& rec = world.history->getMoniker(mon);
	if (!e.name.empty())
		rec.name = e.name;
	// its parents in C2, by moniker (as an imported .zex keeps them: they
	// have no records here), unless that moniker is someone else here
	auto parent = [&](const std::string& pm, const std::string& pname) -> std::string {
		if (pm.empty() || pm == mon)
			return {};
		if (world.history->hasMoniker(pm)) {
			monikerData& p = world.history->getMoniker(pm);
			return p.name.empty() || pname.empty() || p.name == pname ? pm : std::string();
		}
		return pm;
	};
	rec.mother = parent(e.mother, e.motherName);
	rec.father = parent(e.father, e.fatherName);
	historyManager::lineageGeneration++;
	// when and where it was born, its parents' names, and its breeder's note
	std::string from = "Brought in from Creatures 2";
	if (!e.born.empty())
		from += ", born " + e.born + (e.birthplace.empty() || e.birthplace == "The birthplace" ? "" : " at " + e.birthplace);
	from += ".";
	if (!e.motherName.empty() || !e.fatherName.empty())
		from += fmt::format(" Its mother {}, its father {}.", e.motherName.empty() ? "unknown" : e.motherName, e.fatherName.empty() ? "unknown" : e.fatherName);
	if (!e.note.empty())
		from += " " + e.note;
	rec.notes = rec.notes.empty() ? from : from + "\n" + rec.notes;
	for (int i = 0; i < stage; i++)
		c->ageCreature();
	c->born();
	// the words it knew, on Zeetures' own scale: known (128) and better the
	// better C2 had it ("pull" said right at 240: 248)
	if (c->vocab.size() < engine.wordlist.size())
		c->vocab.resize(engine.wordlist.size(), 0);
	int known = 0;
	for (auto& [slot, strength] : e.words)
		if (slot >= 0 && (size_t)slot < c->vocab.size()) {
			c->vocab[slot] = (unsigned char)std::clamp(WORD_KNOWN + strength / 2, WORD_KNOWN, 255);
			known++;
		}
	world.hand()->addCarried(a);
	world.selectCreature(AgentRef(a).lock());

	static const char* stages[] = {"baby", "child", "adolescent", "youth", "adult", "old", "senile"};
	message = fmt::format("{} came in from {}: a {} {}{} (the hand holds it; drop it somewhere).", e.name.empty() ? "A creature" : e.name,
		path.filename().string(), female ? "female" : "male", stages[std::clamp(stage, 0, 6)],
		known ? fmt::format(" who knows {} word{}", known, known == 1 ? "" : "s") : "");
	world.history->censusNow().imported++;
	world.history->censusNow().alive++;
	fmt::print("* Imported creature: {}\n", message);
	return true;
}

void ExportCreatureMenuItem() {
	auto sel = world.selectedcreature.lock();
	auto* ca = sel ? dynamic_cast<CreatureAgent*>(sel.get()) : nullptr;
	bool can = engine.version == 2 && ca && ca->getCreature() && ca->getCreature()->isAlive() && !sel->isDying();
	if (ImGui::MenuItem("Export Current Creature...", nullptr, false, can)) {
		s_exporting = AgentRef(sel);
		s_askExport = true;
	}
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip(can ? "Saves the selected creature to a file and takes it out of this world. File > Import Creature brings it back, here or in another world."
							  : "Select a living creature first.");
}

void ImportCreatureMenuItem() {
	if (ImGui::MenuItem("Import Creature...")) {
		static const SDL_DialogFileFilter filters[] = {{"Exported creatures (zex, exp)", "zex;exp"}, {"All files", "*"}};
		SDL_ShowOpenFileDialog(chosen, nullptr, nullptr, filters, 2, nullptr, false);
	}
}

void ImportCreatureTick() {
	// developer aid: OPENC2E_TEST_IMPORT_CREATURE="path.exp" imports it once
	// the world is running
	static bool tested = false;
	if (const char* t = devenv("OPENC2E_TEST_IMPORT_CREATURE"); t && !tested && world.hand()) {
		tested = true;
		std::lock_guard<std::mutex> lock(s_mutex);
		s_picked.push_back(t);
	}
	// developer aid: OPENC2E_TEST_EXPORT_CREATURE="path.zex" exports the
	// selected creature (or the first living norn) once the world has run
	// 50 ticks, without asking
	static bool exportTested = false;
	if (const char* t = devenv("OPENC2E_TEST_EXPORT_CREATURE"); t && !exportTested && world.hand() && world.tickcount > 50) {
		exportTested = true;
		std::shared_ptr<Agent> pick = world.selectedcreature.lock();
		if (!pick)
			for (auto& a : world.agents)
				if (auto* ca = a ? dynamic_cast<CreatureAgent*>(a.get()) : nullptr; ca && a->genus == 1 && ca->getCreature() && ca->getCreature()->isAlive()) {
					pick = a;
					break;
				}
		s_exporting = AgentRef(pick);
		std::string msg;
		exportTo(t, msg);
		fmt::print("* TEST EXPORT: {}\n", msg);
	}
	if (s_askExport) {
		s_askExport = false;
		ImGui::OpenPopup("Export Creature");
	}
	if (ImGui::BeginPopupModal("Export Creature", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		auto a = s_exporting.lock();
		std::string name = a ? creatureName(a.get()) : "";
		ImGui::TextUnformatted(fmt::format("Export {}?", name).c_str());
		ImGui::TextUnformatted("It leaves this world and goes into a file. File > Import Creature brings it back,");
		ImGui::TextUnformatted("here or in another Zeetures world, as it was: its body, mind, words and life story.");
		if (ImGui::Button("Export...") && a) {
			std::error_code ec;
			fs::path dir = getUserDataDir() / "Exported Creatures";
			fs::create_directories(dir, ec);
			std::string safe;
			for (char ch : name)
				safe += (std::isalnum((unsigned char)ch) || ch == ' ' || ch == '-' || ch == '_') ? ch : '_';
			static std::string where;
			where = (dir / ((safe.empty() ? std::string("creature") : safe) + ".zex")).string();
			static const SDL_DialogFileFilter filters[] = {{"Zeetures exported creatures (zex)", "zex"}};
			SDL_ShowSaveFileDialog(chosenExport, nullptr, nullptr, filters, 1, where.c_str());
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) {
			s_exporting = AgentRef();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	std::vector<std::string> exportTo_;
	{
		std::lock_guard<std::mutex> lock(s_mutex);
		exportTo_.swap(s_exportTo);
	}
	for (auto& f : exportTo_) {
		exportTo(f, s_message);
		ImGui::OpenPopup("Exported");
	}
	if (ImGui::BeginPopupModal("Exported", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextUnformatted(s_message.c_str());
		if (ImGui::Button("OK"))
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
	std::vector<std::string> picked;
	{
		std::lock_guard<std::mutex> lock(s_mutex);
		picked.swap(s_picked);
	}
	for (auto& f : picked) {
		std::string msg;
		ImportExportedCreature(f, msg);
		s_message = msg;
		ImGui::OpenPopup("Import Creature");
	}
	if (ImGui::BeginPopupModal("Import Creature", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextUnformatted(s_message.c_str());
		if (ImGui::Button("OK"))
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
}
