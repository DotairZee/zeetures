#pragma once

// Creatures 2 breeds: which breed slots are in use, the breeds the player
// has imported, and genomes rewritten to use a breed. A breed is a set of
// body-part sprites (and body data) for one species in one slot (a-z, then
// aa, ab ...; creatures/BreedSlots.h).

#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace Openc2eImgui {

struct BreedInfo {
	int species = 0; // 0 norn, 1 grendel, 2 ettin, 3 the fourth C2 species slot
	int slot = 0;
	std::set<char> parts; // body-part letters with sprites (a head ... n tail)
	std::set<int> stages; // life stages with sprites
	bool male = false, female = false;
	bool bodyData = false; // has .att files
	bool inGame = false; // files in the game's own folders
	bool inUser = false; // files in the user data folder
	// from breeds.txt, for breeds imported here
	bool imported = false;
	std::string name, source, genome;
	std::vector<std::string> files; // installed files, relative to the user data folder
};

const char* BreedSpeciesName(int species);

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

// breeds.txt in the user data folder: one line per imported breed
void SaveBreedRegistry();

// A C2 genome ("dna2...") with the appearance genes of the parts the breed
// has pointed at it (species and slot). Unchanged on any error.
std::vector<uint8_t> GenomeWithBreed(const std::vector<uint8_t>& genome, const BreedInfo& b);
// ... with every appearance gene of one species and slot moved to another
std::vector<uint8_t> GenomeWithSlotMoved(const std::vector<uint8_t>& genome, int species, int from, int to);

// The genome a breed's eggs start from: its own (imported) or the species'
// standard one from Genetics (norn.gen, gren.gen, ettn.gen).
std::vector<uint8_t> BreedBaseGenome(const BreedInfo& b);

} // namespace Openc2eImgui
