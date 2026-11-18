#pragma once

// Creatures 2 breeds: which breed slots are in use, the breeds the player
// has imported, and genomes rewritten to use a breed. A breed is a set of
// body-part sprites (and body data) for one species in one slot (a-z, then
// aa, ab ...; creatures/BreedSlots.h).

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace Openc2eImgui {

struct BreedInfo {
	int species = 0; // 0 norn, 1 grendel, 2 ettin, 3 geat (C2's fourth species set): the picture set its files are in
	// what kind of creature it is, from its genome's genus gene (0 norn, 1
	// grendel, 2 ettin), or -1 if not known. Not always its picture set: a
	// creature's look is chosen by its appearance genes, so pack authors
	// filed norn breeds in the grendel set or the unused fourth set to keep
	// clear of other packs' norn slots (Midsummer, Goat, Kai Norns).
	int kind = -1;
	int group() const { return kind >= 0 ? kind : species; } // the kit lists breeds by this
	int slot = 0;
	std::set<char> parts; // body-part letters with sprites (a head ... n tail)
	std::set<int> stages; // life stages with sprites
	bool male = false, female = false;
	bool bodyData = false; // has .att files
	bool inGame = false; // files in the game's own folders
	bool inUser = false; // files in the user data folder
	bool inOverrides = false; // files in its Overrides folder, in place of the game's
	bool jointsAdjusted = false; // only body data in Overrides (corrected joints)
	// from breeds.txt, for breeds imported here
	bool imported = false;
	std::string name, source, genome;
	int origSlot = -1; // the slot its pack used, when it was moved to a free one
	// the picture set its pack used, when it was moved to another (a norn
	// breed its authors filed in the grendel or geat set); the old files are
	// kept there for creatures hatched before, and not listed as a breed
	int origSpecies = -1;
	std::vector<std::string> files; // installed files, relative to the user data folder
};

const char* BreedSpeciesName(int species);

// who made an imported breed pack, by the pack's file name; null if unknown
const char* PackCredit(const std::string& source);

// A data folder's subfolder ("Images", "Body Data", "Genetics", "Eggs"),
// matched without regard to case; the plain path if it doesn't exist yet.
std::filesystem::path BreedDataSubdir(const std::filesystem::path& main, const char* sub);

// "a04k.s16" -> part 'a', species 0, male, stage 4, slot k; false if the
// name is not a body-part file name
bool ParseBreedFileName(const std::string& filename, char& part, int& species, bool& female, int& stage, int& slot);
std::string BreedLabel(const BreedInfo& b); // "Norn breed k" or its name

// every breed found in the data folders (scanned once; rescan after changes)
std::vector<BreedInfo>& KnownBreeds(bool rescan = false);
const BreedInfo* FindBreed(int species, int slot);
// the breed a C2 genome looks like (its head's appearance gene), named as
// the Breeder's Kit names it ("Golden Desert"); empty if it can't be read
std::string BreedNameOfGenome(const std::vector<uint8_t>& genome);

// breeds.txt in the user data folder: one line per imported breed
void SaveBreedRegistry();

// A C2 genome ("dna2...") with the appearance genes of the parts the breed
// has pointed at it (species and slot). Unchanged on any error.
std::vector<uint8_t> GenomeWithBreed(const std::vector<uint8_t>& genome, const BreedInfo& b);
// ... with every appearance gene of one species and slot moved to another
std::vector<uint8_t> GenomeWithSlotMoved(const std::vector<uint8_t>& genome, int species, int from, int to);
// ... with several moves at once, {species, from slot} -> to slot (each gene
// moves at most once, so one move's new slot is never moved again)
std::vector<uint8_t> GenomeWithSlotsMoved(const std::vector<uint8_t>& genome, const std::map<std::pair<int, int>, int>& moves);
// The same, moving between picture sets too: (set, slot) -> (set, slot).
std::vector<uint8_t> GenomeWithBreedsMoved(const std::vector<uint8_t>& genome, const std::map<std::pair<int, int>, std::pair<int, int>>& moves);
// The genus a genome's genus gene gives (0 norn, 1 grendel, 2 ettin), or -1.
int GenomeGenus(const std::vector<uint8_t>& genome);
// A norn breed whose pack filed its pictures in another set: copies its
// pictures and body data into the norn set at `slot`, rewrites its genomes
// and eggs to match, and records the move (the old files stay, for
// creatures hatched before). The default slot: its own letter if free in
// the norn set, else that letter followed by a, b ... ("ka").
int DefaultNornSlotFor(const BreedInfo& b);
bool MoveBreedToNornSet(const BreedInfo& b, int slot, std::string& err);
// an imported breed to another free slot, in its own set or another (its
// picture and body-data files copied under the new names, the pack's
// genomes and eggs changed to match, the registry updated; the old files
// stay for creatures hatched before)
bool MoveBreedTo(const BreedInfo& b, int toSpecies, int slot, std::string& err);
// the {species, slot} pairs a C2 genome's appearance genes use
std::set<std::pair<int, int>> GenomeAppearanceSlots(const std::vector<uint8_t>& genome);

// A C2 egg file (the game's and add-on packs' Eggs folders). Layout: "eggf",
// gender (1 male, 2 female, 0 either), egg picture index, 8 unused bytes,
// two 4-character monikers, a 32-bit description length and the
// description (empty in the game's own eggs; pack authors fill it, e.g.
// Nova Subterra's "Nova Subterra: Breed H female"), then one or two
// genomes, each a 32-bit length and the genome ("dna2...").
struct C2EggFile {
	int gender = 0;
	int frame = 0;
	std::string description;
	std::vector<std::vector<uint8_t>> genomes;
};
// false if the data is not a C2 egg with at least one genome
bool ReadC2Egg(const std::vector<uint8_t>& data, C2EggFile& out);
// the egg with each genome passed through fn (header, description and
// anything after the genomes kept); unchanged if it is not a C2 egg
std::vector<uint8_t> C2EggWithGenomes(const std::vector<uint8_t>& egg, const std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)>& fn);

// The genome of an exported creature (.exp: the creature as the game saved
// it, its genome after "CGenome"), as a genome file ("dna2..."); empty if
// none
std::vector<uint8_t> GenomeFromExport(const std::vector<uint8_t>& exp);

// The genome a breed's eggs start from: its own (imported) or the species'
// standard one from Genetics (norn.gen, gren.gen, ettn.gen).
std::vector<uint8_t> BreedBaseGenome(const BreedInfo& b);

} // namespace Openc2eImgui
