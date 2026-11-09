#include "Breeds.h"

#include "PathResolver.h"
#include "common/Ascii.h"
#include "common/io/SpanReader.h"
#include "common/case_insensitive_filesystem.h"
#include "creatures/BreedSlots.h"
#include "fileformats/genomeFile.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <map>
#include <sstream>

namespace fs = std::filesystem;

namespace Openc2eImgui {

const char* BreedSpeciesName(int species) {
	switch (species) {
		case 0: return "Norn";
		case 1: return "Grendel";
		case 2: return "Ettin";
		default: return "Species 4";
	}
}

std::string BreedLabel(const BreedInfo& b) {
	if (!b.name.empty())
		return fmt::format("{} ({} {})", b.name, BreedSpeciesName(b.species), breedSlotName(b.slot));
	return fmt::format("{} breed {}", BreedSpeciesName(b.species), breedSlotName(b.slot));
}

// "a04k.s16" -> part a, species 0, female no, stage 4, slot k
bool ParseBreedFileName(const std::string& filename, char& part, int& species, bool& female, int& stage, int& slot) {
	std::string n = to_ascii_lowercase(filename);
	auto dot = n.rfind('.');
	if (dot == std::string::npos)
		return false;
	std::string stem = n.substr(0, dot);
	if (stem.size() != 4 && stem.size() != 5)
		return false;
	if (stem[0] < 'a' || stem[0] > 'q' || stem[1] < '0' || stem[1] > '7' || stem[2] < '0' || stem[2] > '6')
		return false;
	slot = breedSlotFromName(stem.substr(3));
	if (slot < 0 || slot >= BREED_SLOTS)
		return false;
	part = stem[0];
	species = (stem[1] - '0') % 4;
	female = stem[1] >= '4';
	stage = stem[2] - '0';
	return true;
}

fs::path BreedDataSubdir(const fs::path& main, const char* sub) {
	std::error_code ec;
	fs::path p = case_insensitive_filesystem::canonical(main / sub, ec);
	return ec ? main / sub : p;
}

static fs::path registryPath() {
	return getUserDataDir() / "breeds.txt";
}

static std::vector<std::string> split(const std::string& s, char sep) {
	std::vector<std::string> out;
	std::string cur;
	for (char c : s) {
		if (c == sep) {
			out.push_back(cur);
			cur.clear();
		} else if (c != '\r') {
			cur += c;
		}
	}
	out.push_back(cur);
	return out;
}

static std::vector<BreedInfo> s_breeds;
static bool s_scanned = false;

std::vector<BreedInfo>& KnownBreeds(bool rescan) {
	if (s_scanned && !rescan)
		return s_breeds;
	s_scanned = true;
	std::map<std::pair<int, int>, BreedInfo> found;
	fs::path user;
	try {
		user = getUserDataDir();
	} catch (...) {
	}
	for (auto& d : data_directories) {
		bool isUser = d.main == user;
		for (auto& dir : {BreedDataSubdir(d.main, "Images"), BreedDataSubdir(d.main, "Body Data")}) {
			std::error_code ec;
			for (auto& e : fs::directory_iterator(dir, ec)) {
				std::string fname = e.path().filename().string();
				std::string ext = to_ascii_lowercase(e.path().extension().string());
				bool sprite = ext == ".s16" || ext == ".c16" || ext == ".png";
				bool att = ext == ".att";
				if (!sprite && !att)
					continue;
				char part;
				int species, stage, slot;
				bool female;
				if (!ParseBreedFileName(fname, part, species, female, stage, slot))
					continue;
				auto& b = found[{species, slot}];
				b.species = species;
				b.slot = slot;
				if (sprite) {
					b.parts.insert(part);
					b.stages.insert(stage);
					(female ? b.female : b.male) = true;
				} else {
					b.bodyData = true;
				}
				(isUser ? b.inUser : b.inGame) = true;
			}
		}
	}
	// names and file lists of imported breeds
	std::ifstream in(registryPath());
	std::string line;
	while (std::getline(in, line)) {
		if (line.empty() || line[0] == '#')
			continue;
		auto f = split(line, '|');
		if (f.size() < 6)
			continue;
		int species = std::atoi(f[0].c_str());
		int slot = breedSlotFromName(f[1]);
		if (slot < 0)
			continue;
		auto& b = found[{species, slot}];
		b.species = species;
		b.slot = slot;
		b.imported = true;
		b.name = f[2];
		b.source = f[3];
		b.genome = f[4];
		b.files.clear();
		for (auto& x : split(f[5], ';'))
			if (!x.empty())
				b.files.push_back(x);
	}
	s_breeds.clear();
	for (auto& [k, b] : found)
		s_breeds.push_back(b);
	return s_breeds;
}

const BreedInfo* FindBreed(int species, int slot) {
	for (auto& b : KnownBreeds())
		if (b.species == species && b.slot == slot)
			return &b;
	return nullptr;
}

void SaveBreedRegistry() {
	std::ofstream out(registryPath());
	out << "# Breeds imported in openc2e (Breeder's Kit): species|slot|name|source|genome|files\n";
	for (auto& b : KnownBreeds()) {
		if (!b.imported)
			continue;
		std::string files;
		for (auto& f : b.files)
			files += (files.empty() ? "" : ";") + f;
		out << fmt::format("{}|{}|{}|{}|{}|{}\n", b.species, breedSlotName(b.slot), b.name, b.source, b.genome, files);
	}
}

// the appearance gene's part for a body-part letter (as SkeletalCreature)
static int appearancePart(char x) {
	if (x == 'a' || x >= 'o')
		return 0;
	if (x == 'b')
		return 1;
	if (x >= 'c' && x <= 'h')
		return 2;
	if (x >= 'i' && x <= 'l')
		return 3;
	if (x == 'm' || x == 'n')
		return 4;
	return -1;
}

// Edits a C2 genome's appearance genes in place, byte for byte: each is
// "gene", type 2, subtype 2, five header bytes, then part, slot, species.
// Patching keeps everything else exactly as it was, including what follows
// "gend" in the genome's 36001-byte block. The raw scan must find exactly
// the appearance genes the genome reader finds, or the genome is left
// unchanged.
template <typename F>
static std::vector<uint8_t> editGenome(const std::vector<uint8_t>& bytes, F edit) {
	try {
		if (bytes.size() < 8 || std::memcmp(bytes.data(), "dna2", 4) != 0)
			return bytes;
		genomeFile g;
		SpanReader r(std::span<const uint8_t>(bytes.data(), bytes.size()));
		r >> g;
		std::vector<creatureAppearanceGene*> parsed;
		for (auto& gp : g.genes)
			if (auto* a = dynamic_cast<creatureAppearanceGene*>(gp.get()))
				parsed.push_back(a);
		std::vector<size_t> at;
		for (size_t i = 4; i + 14 <= bytes.size(); i++)
			if (std::memcmp(&bytes[i], "gene", 4) == 0 && bytes[i + 4] == 2 && bytes[i + 5] == 2)
				at.push_back(i + 11);
		if (at.size() != parsed.size()) {
			fmt::print("Breeds: genome layout not as expected ({} vs {} appearance genes); unchanged\n", at.size(), parsed.size());
			return bytes;
		}
		std::vector<uint8_t> out = bytes;
		bool changed = false;
		for (size_t k = 0; k < at.size(); k++) {
			auto* a = parsed[k];
			if (out[at[k]] != a->part || out[at[k] + 1] != a->variant || out[at[k] + 2] != a->species) {
				fmt::print("Breeds: genome bytes don't match its genes; unchanged\n");
				return bytes;
			}
			if (edit(*a)) {
				out[at[k]] = a->part;
				out[at[k] + 1] = a->variant;
				out[at[k] + 2] = a->species;
				changed = true;
			}
		}
		return changed ? out : bytes;
	} catch (std::exception& e) {
		fmt::print("Breeds: couldn't read a genome: {}\n", e.what());
		return bytes;
	}
}

std::vector<uint8_t> GenomeWithBreed(const std::vector<uint8_t>& genome, const BreedInfo& b) {
	std::set<int> parts;
	for (char p : b.parts)
		parts.insert(appearancePart(p));
	return editGenome(genome, [&](creatureAppearanceGene& a) {
		if (!parts.count(a.part))
			return false;
		a.species = (uint8_t)b.species;
		a.variant = (uint8_t)b.slot;
		return true;
	});
}

std::vector<uint8_t> GenomeWithSlotMoved(const std::vector<uint8_t>& genome, int species, int from, int to) {
	return editGenome(genome, [&](creatureAppearanceGene& a) {
		if (a.species != species || a.variant != from)
			return false;
		a.variant = (uint8_t)to;
		return true;
	});
}

std::vector<uint8_t> BreedBaseGenome(const BreedInfo& b) {
	std::vector<std::string> tries;
	if (!b.genome.empty())
		tries.push_back(b.genome);
	tries.push_back(b.species == 1 ? "gren.gen" : b.species == 2 ? "ettn.gen" : "norn.gen");
	for (auto& t : tries) {
		auto path = findGeneticsFile(t);
		if (path.empty())
			continue;
		std::ifstream in(path, std::ios::binary);
		std::vector<uint8_t> d((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		if (d.size() > 4)
			return d;
	}
	return {};
}

} // namespace Openc2eImgui
