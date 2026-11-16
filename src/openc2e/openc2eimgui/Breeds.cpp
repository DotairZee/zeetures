#include "Additions.h"
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
#include <iterator>
#include <fmt/format.h>
#include <fstream>
#include <map>
#include <sstream>

namespace fs = std::filesystem;

namespace Openc2eImgui {

// The genus gene (the first gene: "gene", type 2, subtype 1, five header
// bytes, then the genus: 0 norn, 1 grendel, 2 ettin) of a genome file, or -1.
int GenomeGenus(const std::vector<uint8_t>& d) {
	for (size_t i = 0; i + 12 < d.size(); i++)
		if (d[i] == 'g' && d[i + 1] == 'e' && d[i + 2] == 'n' && d[i + 3] == 'e') {
			if (d[i + 4] == 2 && d[i + 5] == 1 && d[i + 11] <= 2)
				return d[i + 11];
			return -1;
		}
	return -1;
}

static int genomeGenus(const std::filesystem::path& file) {
	std::ifstream in(file, std::ios::binary);
	if (!in)
		return -1;
	std::vector<uint8_t> d((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	for (size_t i = 0; i + 12 < d.size(); i++)
		if (d[i] == 'g' && d[i + 1] == 'e' && d[i + 2] == 'n' && d[i + 3] == 'e') {
			if (d[i + 4] == 2 && d[i + 5] == 1 && d[i + 11] <= 2)
				return d[i + 11];
			return -1; // the first gene is not the genus gene
		}
	return -1;
}

const char* BreedSpeciesName(int species) {
	switch (species) {
		case 0: return "Norn";
		case 1: return "Grendel";
		case 2: return "Ettin";
		default: return "Geat"; // C2's fourth species set (owner, 2026-11-11)
	}
}

std::string BreedLabel(const BreedInfo& b) {
	if (!b.name.empty())
		return fmt::format("{} ({} {})", b.name, BreedSpeciesName(b.species), breedSlotName(b.slot));
	return fmt::format("{} breed {}", BreedSpeciesName(b.species), breedSlotName(b.slot));
}

std::string BreedNameOfGenome(const std::vector<uint8_t>& bytes) {
	try {
		if (bytes.size() < 8 || std::memcmp(bytes.data(), "dna2", 4) != 0)
			return {};
		genomeFile g;
		SpanReader r(std::span<const uint8_t>(bytes.data(), bytes.size()));
		r >> g;
		for (auto& gp : g.genes)
			if (auto* a = dynamic_cast<creatureAppearanceGene*>(gp.get()); a && a->part == 0) {
				int species = geneBreedSpecies(a->species), slot = geneBreedSlot(a->species, a->variant);
				if (const BreedInfo* b = FindBreed(species, slot))
					return !b->name.empty() ? b->name : BreedLabel(*b);
				return fmt::format("{} breed {}", BreedSpeciesName(species), breedSlotName(slot));
			}
	} catch (std::exception&) {
	}
	return {};
}

// "a04k.s16" -> part a, species 0, female no, stage 4, slot k
bool ParseBreedFileName(const std::string& filename, char& part, int& species, bool& female, int& stage, int& slot) {
	std::string n = to_ascii_lowercase(filename);
	auto dot = n.rfind('.');
	if (dot == std::string::npos)
		return false;
	std::string stem = n.substr(0, dot);
	if (stem.size() < 4 || stem.size() > 6)
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
	// the game's and the user's folders, and the user's Overrides folder
	// (pictures and body data there take the place of the game's: the
	// imported Frog Norn pack in C2's own frog norn's slot l)
	std::vector<std::pair<fs::path, int>> roots; // 0 game, 1 user, 2 overrides
	for (auto& d : data_directories)
		roots.push_back({d.main, d.main == user ? 1 : 0});
	if (!user.empty())
		roots.push_back({user / "Overrides", 2});
	for (auto& [main, kind] : roots) {
		bool isUser = kind >= 1;
		for (auto& dir : {BreedDataSubdir(main, "Images"), BreedDataSubdir(main, "Body Data")}) {
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
				// body data alone in Overrides only corrects a game breed's
				// joints (Golden Desert's neck, owner 2026-11-13): it stays
				// the game's breed
				if (kind == 2 && !sprite) {
					b.jointsAdjusted = true;
					continue;
				}
				(isUser ? b.inUser : b.inGame) = true;
				if (kind == 2)
					b.inOverrides = true;
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
		b.origSlot = f.size() > 6 && !f[6].empty() ? breedSlotFromName(f[6]) : -1;
		b.origSpecies = f.size() > 7 && !f[7].empty() ? std::atoi(f[7].c_str()) : -1;
	}
	// the old files of a breed moved to another set are kept for creatures
	// hatched before, but are not a breed of their own
	for (auto it = found.begin(); it != found.end();) {
		bool left = false;
		for (auto& [k, b] : found)
			left |= b.imported && b.origSpecies >= 0 && it->first == std::make_pair(b.origSpecies, b.origSlot >= 0 ? b.origSlot : b.slot) &&
					!it->second.imported;
		it = left ? found.erase(it) : std::next(it);
	}
	// names for the game's own breeds, from the user data folder's
	// breed-names.txt ("species|slot|name"; kept out of the engine: the
	// names are the game's)
	{
		std::istringstream in(Additions::text("breed-names.txt")); // (the player's own, or the built-in one)
		std::string line;
		while (std::getline(in, line)) {
			if (line.empty() || line[0] == '#')
				continue;
			auto f = split(line, '|');
			if (f.size() < 3)
				continue;
			int slot = breedSlotFromName(f[1]);
			auto it = found.find({std::atoi(f[0].c_str()), slot});
			if (slot >= 0 && it != found.end() && !it->second.imported && it->second.name.empty())
				it->second.name = f[2];
		}
	}
	s_breeds.clear();
	for (auto& [k, b] : found) {
		// the kind of creature: the game's own breeds are what their set says;
		// an imported one is what its genome's genus gene says
		if (!b.imported && b.species < 3)
			b.kind = b.species;
		if (!b.genome.empty())
			if (int g = genomeGenus(BreedDataSubdir(getUserDataDir(), "Genetics") / b.genome); g >= 0)
				b.kind = g;
		s_breeds.push_back(b);
	}
	return s_breeds;
}

const BreedInfo* FindBreed(int species, int slot) {
	for (auto& b : KnownBreeds())
		if (b.species == species && b.slot == slot)
			return &b;
	return nullptr;
}

void SaveBreedRegistry() {
	std::ofstream out(registryPath(), std::ios::binary);
	out << "# Breeds imported in openc2e (Breeder's Kit): species|slot|name|source|genome|files|the pack's own slot, if moved\n";
	for (auto& b : KnownBreeds()) {
		if (!b.imported)
			continue;
		std::string files;
		for (auto& f : b.files)
			files += (files.empty() ? "" : ";") + f;
		out << fmt::format("{}|{}|{}|{}|{}|{}|{}|{}\n", b.species, breedSlotName(b.slot), b.name, b.source, b.genome, files,
			b.origSlot >= 0 && (b.origSlot != b.slot || b.origSpecies >= 0) ? breedSlotName(b.origSlot) : "",
			b.origSpecies >= 0 ? std::to_string(b.origSpecies) : "");
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
// unchanged. It stops where the reader stopped, at "gend": the block's
// leftovers past it can hold an old appearance gene (the game's gren.gen,
// the Boney pack's bone.gen and the grendels made from them have one), and
// counting it made those genomes unchangeable: a grendel breed moved to
// another slot on import kept pointing at its old one, and an egg for a
// grendel breed without a genome of its own hatched a standard grendel
// (found 2026-11-16 from the "6 vs 5 appearance genes" warning on the
// owner's Mac install).
template <typename F>
static std::vector<uint8_t> editGenome(const std::vector<uint8_t>& bytes, F edit) {
	try {
		if (bytes.size() < 8 || std::memcmp(bytes.data(), "dna2", 4) != 0)
			return bytes;
		genomeFile g;
		SpanReader r(std::span<const uint8_t>(bytes.data(), bytes.size()));
		r >> g;
		const size_t genesEnd = std::min(r.tell(), bytes.size());
		std::vector<creatureAppearanceGene*> parsed;
		for (auto& gp : g.genes)
			if (auto* a = dynamic_cast<creatureAppearanceGene*>(gp.get()))
				parsed.push_back(a);
		std::vector<size_t> at;
		for (size_t i = 4; i + 14 <= genesEnd; i++)
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
		setGeneBreed(a.species, a.variant, b.species, b.slot);
		return true;
	});
}

std::vector<uint8_t> GenomeWithBreedsMoved(const std::vector<uint8_t>& genome, const std::map<std::pair<int, int>, std::pair<int, int>>& moves) {
	if (moves.empty())
		return genome;
	return editGenome(genome, [&](creatureAppearanceGene& a) {
		auto it = moves.find({geneBreedSpecies(a.species), geneBreedSlot(a.species, a.variant)});
		if (it == moves.end())
			return false;
		setGeneBreed(a.species, a.variant, it->second.first, it->second.second);
		return true;
	});
}

int DefaultNornSlotFor(const BreedInfo& b) {
	auto taken = [&](int slot) {
		for (auto& k : KnownBreeds())
			if (k.group() == 0 && k.slot == slot && &k != &b && !(k.species == b.species && k.slot == b.slot))
				if (k.species == 0)
					return true;
		return FindBreed(0, slot) != nullptr;
	};
	if (!taken(b.slot))
		return b.slot;
	// keeping its letter in a two-letter slot: that letter followed by a, b
	// ... ("g" -> "ga"); slots end at "iv" (a genome stores a slot as one
	// byte), so for letters past i, a, b ... followed by it ("k" -> "ak")
	if (b.slot < 26) {
		for (int second = 0; second < 26; second++) {
			int s = 26 + b.slot * 26 + second;
			if (s < BREED_SLOTS && !taken(s))
				return s;
		}
		for (int first = 0; first < 26; first++) {
			int s = 26 + first * 26 + b.slot;
			if (s < BREED_SLOTS && !taken(s))
				return s;
		}
	}
	for (int s = 0; s < BREED_SLOTS; s++)
		if (!taken(s))
			return s;
	return -1;
}

bool MoveBreedToNornSet(const BreedInfo& orig, int slot, std::string& err) {
	if (orig.species == 0) {
		err = "not an imported breed in another set";
		return false;
	}
	return MoveBreedTo(orig, 0, slot, err);
}

bool MoveBreedTo(const BreedInfo& orig, int toSpecies, int slot, std::string& err) {
	namespace fs = std::filesystem;
	BreedInfo b = orig;
	if (!b.imported || slot < 0 || slot >= BREED_SLOTS || toSpecies < 0 || toSpecies > 3 || (toSpecies == b.species && slot == b.slot)) {
		err = "not an imported breed, or not a new slot";
		return false;
	}
	if (FindBreed(toSpecies, slot)) {
		err = fmt::format("{} slot {} is taken", BreedSpeciesName(toSpecies), breedSlotName(slot));
		return false;
	}
	fs::path main = getUserDataDir();
	std::map<std::pair<int, int>, std::pair<int, int>> moves{{{b.species, b.slot}, {toSpecies, slot}}};
	auto readAll = [](const fs::path& p) {
		std::ifstream in(p, std::ios::binary);
		return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	};
	std::vector<std::string> added;
	for (auto& rel : b.files) {
		fs::path p = main / rel;
		std::string name = p.filename().string();
		char part;
		int species, stage, s;
		bool female;
		std::string ext = to_ascii_lowercase(p.extension().string());
		if (ParseBreedFileName(name, part, species, female, stage, s) && species == b.species && s == b.slot) {
			// a body-part picture or body data: a copy under the norn set's name
			std::string stem = fmt::format("{}{}{}{}", part, toSpecies + (female ? 4 : 0), stage, breedSlotName(slot));
			fs::path dest = p.parent_path() / (stem + ext);
			std::error_code ec;
			fs::copy_file(p, dest, fs::copy_options::overwrite_existing, ec);
			if (ec) {
				err = "couldn't copy " + name + ": " + ec.message();
				return false;
			}
			added.push_back(fs::relative(dest, main, ec).string());
		} else if (ext == ".gen" || ext == ".egg") {
			// its genomes and eggs now name the norn slot
			std::vector<uint8_t> d = readAll(p), out;
			if (ext == ".gen")
				out = GenomeWithBreedsMoved(d, moves);
			else {
				out = d;
				if (d.size() >= 30 && std::memcmp(d.data(), "eggf", 4) == 0) {
					out.assign(d.begin(), d.begin() + 26);
					size_t pos = 26;
					while (pos + 4 <= d.size()) {
						uint32_t len = d[pos] | (d[pos + 1] << 8) | (d[pos + 2] << 16) | ((uint32_t)d[pos + 3] << 24);
						if (len < 4 || pos + 4 + len > d.size() || std::memcmp(&d[pos + 4], "dna", 3) != 0)
							break;
						std::vector<uint8_t> g(d.begin() + pos + 4, d.begin() + pos + 4 + len);
						g = GenomeWithBreedsMoved(g, moves);
						uint32_t n = (uint32_t)g.size();
						out.insert(out.end(), {(uint8_t)n, (uint8_t)(n >> 8), (uint8_t)(n >> 16), (uint8_t)(n >> 24)});
						out.insert(out.end(), g.begin(), g.end());
						pos += 4 + len;
					}
					out.insert(out.end(), d.begin() + pos, d.end());
				}
			}
			if (out != d) {
				std::ofstream o(p, std::ios::binary);
				o.write((const char*)out.data(), out.size());
			}
		}
	}
	// the registry: now at `toSpecies`/`slot`, moved from its set and slot
	// (the old files stay, for creatures hatched before)
	for (auto& k : KnownBreeds())
		if (k.species == orig.species && k.slot == orig.slot && k.imported) {
			if (toSpecies != k.species)
				k.origSpecies = k.origSpecies >= 0 ? k.origSpecies : k.species;
			k.origSlot = k.origSlot >= 0 ? k.origSlot : k.slot;
			if (toSpecies == 0)
				k.kind = 0;
			k.species = toSpecies;
			k.slot = slot;
			k.files.insert(k.files.end(), added.begin(), added.end());
		}
	SaveBreedRegistry();
	KnownBreeds(true);
	return true;
}

std::vector<uint8_t> GenomeWithSlotMoved(const std::vector<uint8_t>& genome, int species, int from, int to) {
	return GenomeWithSlotsMoved(genome, {{{species, from}, to}});
}

std::vector<uint8_t> GenomeWithSlotsMoved(const std::vector<uint8_t>& genome, const std::map<std::pair<int, int>, int>& moves) {
	if (moves.empty())
		return genome;
	return editGenome(genome, [&](creatureAppearanceGene& a) {
		auto it = moves.find({geneBreedSpecies(a.species), geneBreedSlot(a.species, a.variant)});
		if (it == moves.end() || it->second < 0)
			return false;
		setGeneBreed(a.species, a.variant, geneBreedSpecies(a.species), it->second);
		return true;
	});
}

std::set<std::pair<int, int>> GenomeAppearanceSlots(const std::vector<uint8_t>& genome) {
	std::set<std::pair<int, int>> out;
	editGenome(genome, [&](creatureAppearanceGene& a) {
		out.insert({geneBreedSpecies(a.species), geneBreedSlot(a.species, a.variant)});
		return false;
	});
	return out;
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
