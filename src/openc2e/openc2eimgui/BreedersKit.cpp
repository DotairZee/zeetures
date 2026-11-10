// The Breeder's Kit (Creatures 2): Reproductive and Aphrodisiacs pages, as
// C2 had, plus Breeds (owner, 2026-11-09): the breeds installed for each
// species, a portrait of each, laying an egg of a breed, and importing C2
// breed packs (files, a folder, a zip or a COB) into the user data folder,
// moved to a free breed slot when theirs is taken. Slots run a-z, then aa,
// ab ... (creatures/BreedSlots.h). C2's own art from the player's Applet
// Data; our layout.

#include "Agent.h"
#include "Breeds.h"
#include "C2KitsInternal.h"
#include "Hatchery.h"
#include "PathResolver.h"
#include "World.h"
#include "common/Ascii.h"
#include "common/Random.h"
#include "creatures/BreedSlots.h"
#include "creatures/oldCreature.h"
#include "common/io/SpanReader.h"
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

// ---- Reproductive --------------------------------------------------------------

static void ReproductivePage() {
	if (!KitNeedsCreature("Applet Data/SexCover.bmp"))
		return;
	c2Creature* c = KitCreature();
	Agent* a = KitCreatureAgent();
	bool female = c->isFemale();
	int preg = c->getPregnant();
	(void)a;
	int genus = c->getGenus() + 1; // the genome's genus: 0 norn, 1 grendel, 2 ettin

	// the figure: norn (male or female), grendel or ettin; pregnant
	// figures show the egg growing in the womb
	const char* figure = genus == 2 ? (preg ? "Applet Data/PregGrendel.bmp" : "Applet Data/grendel.bmp")
		: genus == 3			  ? (preg ? "Applet Data/PregEttin.bmp" : "Applet Data/ettin.bmp")
		: preg					  ? "Applet Data/pregnant.bmp"
		: female				  ? "Applet Data/female.bmp"
								  : "Applet Data/male.bmp";
	ImVec2 p = ImGui::GetCursorScreenPos();
	Texture fig = KitPicture(figure);
	DrawTexture(fig, p);
	auto& sprites = KitSprites("Applet Data/Reproductive.s16");
	if (preg && sprites.size() >= 12) {
		int f = 3 + std::clamp(preg * 9 / 256, 0, 8);
		ImVec2 at = genus == 1 ? ImVec2(49, 74) : ImVec2(40, 58);
		DrawTexture(sprites[f], p + at);
	}
	if (sprites.size() >= 3)
		DrawTexture(sprites[female ? 2 : 1], p + ImVec2(4, 4));
	ImGui::Dummy(ImVec2(108, 170));
	ImGui::SameLine();

	ImGui::BeginGroup();
	ImGui::Text("%s %s", female ? "Female" : "Male", genus == 2 ? "grendel" : genus == 3 ? "ettin" : "norn");
	KitBar(c->getDrive(12) / 255.0f, "Sex drive", 220, true);
	KitBar(c->getFertile() / 255.0f, female ? "Fertility" : "Fertility (sperm)", 220);
	if (female) {
		KitBar(c->getReceptive() / 255.0f, "Receptiveness", 220);
		KitBar(preg / 255.0f, preg ? "Pregnancy" : "Not pregnant", 220);
	}
	ImGui::Spacing();
	const auto& groups = KitChemicalGroups();
	const char* want = female ? "Female Hormones" : "Male Hormones";
	for (auto& g : groups) {
		if (g.first != want)
			continue;
		ImGui::TextDisabled("%s", want);
		const auto& names = KitChemicalNames();
		for (int chem : g.second) {
			ImGui::SetNextItemWidth(120);
			ImGui::ProgressBar(c->getChemical(chem) / 255.0f, ImVec2(100, 12), "");
			ImGui::SameLine();
			ImGui::Text("%s %d", chem < (int)names.size() ? names[chem].c_str() : "?", (int)c->getChemical(chem));
		}
	}
	ImGui::EndGroup();
	ImGui::TextDisabled("Mating and pregnancy are not built yet (next on the list), so these stay quiet for now.");
}

// ---- Aphrodisiacs ----------------------------------------------------------------

static void AphrodisiacsPage() {
	KitCobShelf("Applet Data/aphroshelves.bmp", "Aphrodisiacs", {80, 130, 172}, {100, 160, 220}, "Click an aphrodisiac to see what it is.", "{} added. It may put your creatures in the mood.");
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
	std::vector<size_t> files;
	bool install = true;
};

struct ImportPlan {
	std::string source; // what was picked, for the registry
	char name[64] = {};
	std::vector<ImportFile> files;
	std::vector<ImportGroup> groups;
	std::vector<size_t> genomes, notes, eggs, cobs, skipped;
	std::vector<std::string> problems;
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
static std::vector<uint8_t> genomeFromExport(const std::vector<uint8_t>& exp) {
	static const char tag[] = "CGenome";
	auto at = std::search(exp.begin(), exp.end(), tag, tag + 7);
	if (at == exp.end())
		return {};
	static const char gene[] = "gene", gend[] = "gend";
	auto first = std::search(at, exp.end(), gene, gene + 4);
	if (first == exp.end())
		return {};
	auto end = std::search(first, exp.end(), gend, gend + 4);
	if (end == exp.end())
		return {};
	std::vector<uint8_t> g = {'d', 'n', 'a', '2'};
	g.insert(g.end(), first, end + 4);
	return g;
}

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
			auto g = genomeFromExport(f.data);
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
	for (auto& g : plan->groups)
		if (!slotFree(g.species, g.target, plan.get(), &g))
			g.target = firstFreeSlot(g.species, plan.get(), &g);
	if (plan->groups.empty())
		plan->problems.push_back("No body-part pictures (like a04k.s16) were found, so there is no breed to install.");
	s_plan = std::move(plan);
}

static std::string renamed(const std::string& name, int target) {
	fs::path p(name);
	std::string stem = to_ascii_lowercase(p.stem().string());
	return stem.substr(0, 3) + breedSlotName(target) + to_ascii_lowercase(p.extension().string());
}

// a C2 egg with each genome rewritten
static std::vector<uint8_t> eggWithGenomes(const std::vector<uint8_t>& egg, const std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)>& fn) {
	if (egg.size() < 30 || std::memcmp(egg.data(), "eggf", 4) != 0)
		return egg;
	std::vector<uint8_t> out(egg.begin(), egg.begin() + 26);
	size_t pos = 26;
	while (pos + 4 <= egg.size()) {
		uint32_t len = le32(egg, pos);
		if (len < 4 || pos + 4 + len > egg.size() || std::memcmp(&egg[pos + 4], "dna", 3) != 0)
			break;
		std::vector<uint8_t> g(egg.begin() + pos + 4, egg.begin() + pos + 4 + len);
		g = fn(g);
		uint32_t n = (uint32_t)g.size();
		out.insert(out.end(), {(uint8_t)n, (uint8_t)(n >> 8), (uint8_t)(n >> 16), (uint8_t)(n >> 24)});
		out.insert(out.end(), g.begin(), g.end());
		pos += 4 + len;
	}
	out.insert(out.end(), egg.begin() + pos, egg.end());
	return out;
}

static void installPlan(ImportPlan& plan) {
	fs::path main = getUserDataDir();
	fs::path images = BreedDataSubdir(main, "Images"), bodyData = BreedDataSubdir(main, "Body Data");
	fs::path genetics = BreedDataSubdir(main, "Genetics"), eggsDir = BreedDataSubdir(main, "Eggs");
	std::error_code ec;
	for (auto& d : {images, bodyData, genetics})
		fs::create_directories(d, ec);
	auto moveSlots = [&](const std::vector<uint8_t>& g) {
		std::vector<uint8_t> out = g;
		for (auto& grp : plan.groups)
			if (grp.install && grp.target != grp.slot)
				out = GenomeWithSlotMoved(out, grp.species, grp.slot, grp.target);
		return out;
	};
	auto write = [&](const fs::path& dir, std::string name, const std::vector<uint8_t>& data, std::vector<std::string>& files) {
		fs::path dest = dir / name;
		// never overwrite: genomes and eggs of the same name get a number
		for (int k = 2; fs::exists(dest) && k < 100; k++)
			dest = dir / fmt::format("{}_{}{}", fs::path(name).stem().string(), k, fs::path(name).extension().string());
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
		write(eggsDir, plan.files[i].name, eggWithGenomes(plan.files[i].data, moveSlots), shared);
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
		for (size_t i : g.files) {
			auto& f = plan.files[i];
			bool att = to_ascii_lowercase(fs::path(f.name).extension().string()) == ".att";
			write(att ? bodyData : images, renamed(f.name, g.target), f.data, b.files);
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
				merged = true;
			}
		if (!merged)
			KnownBreeds().push_back(b);
	}
	SaveBreedRegistry();
	KnownBreeds(true);
	s_breed_message = fmt::format("Installed {} breed{} from {}.", installed, installed == 1 ? "" : "s", plan.source);
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

static void ImportReview() {
	ImportPlan& plan = *s_plan;
	ImGui::SeparatorText("Import");
	ImGui::Text("From: %s", plan.source.c_str());
	ImGui::SetNextItemWidth(260);
	ImGui::InputText("Name", plan.name, sizeof(plan.name));
	for (auto& p : plan.problems)
		ImGui::TextColored(ImVec4(1, 0.6f, 0.4f, 1), "%s", p.c_str());
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
		if (g.target == g.slot)
			ImGui::TextDisabled("(its own slot is free)");
		else
			ImGui::TextColored(ImVec4(1, 0.85f, 0.4f, 1), "(%s is taken: moved)", breedSlotName(g.slot).c_str());
		ImGui::PopID();
	}
	if (!plan.genomes.empty() || !plan.eggs.empty() || !plan.notes.empty())
		ImGui::TextDisabled("Also: %d genome%s, %d note file%s, %d egg%s (breed slots in them follow any move).",
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
	bool any = false;
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
	// developer aid: OPENC2E_TEST_BREED_IMPORT=pack imports it at once and
	// shows the breed it became, printing a genome rewritten to wear it
	if (const char* t = getenv("OPENC2E_TEST_BREED_IMPORT")) {
		static bool done = false;
		if (!done) {
			done = true;
			planImport({t});
			for (auto& pr : s_plan->problems)
				fmt::print("* Breed import problem: {}\n", pr);
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
						fmt::print("* rewritten genome ({} bytes): appearance part {} species {} slot {}\n", g.size(), (int)a->part, (int)a->species, breedSlotName(a->variant));
			}
		}
	}
	if (sel_species < 0 && !breeds.empty()) {
		sel_species = breeds[0].species;
		sel_slot = breeds[0].slot;
	}

	// the list, by species
	ImGui::BeginChild("list", ImVec2(230, 330), ImGuiChildFlags_Borders);
	for (int sp = 0; sp < 4; sp++) {
		int n = 0;
		for (auto& b : breeds)
			n += b.species == sp;
		if (n == 0)
			continue;
		if (!ImGui::TreeNodeEx(BreedSpeciesName(sp), ImGuiTreeNodeFlags_DefaultOpen, "%s (%d of %d slots)", BreedSpeciesName(sp), n, BREED_SLOTS))
			continue;
		for (auto& b : breeds) {
			if (b.species != sp)
				continue;
			std::string label = b.name.empty() ? fmt::format("Breed {}", breedSlotName(b.slot)) : fmt::format("{}  ({})", b.name, breedSlotName(b.slot));
			if (b.inUser && !b.inGame)
				label += "  *";
			if (ImGui::Selectable(fmt::format("{}##{}_{}", label, sp, b.slot).c_str(), sel_species == sp && sel_slot == b.slot)) {
				sel_species = sp;
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
		ImGui::TextDisabled("%s", b->inGame ? "In the game's folders" : "Installed in your data folder");
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
			ImGui::TextColored(ImVec4(1, 0.7f, 0.4f, 1), "No body data (.att) in this slot: creatures fall back to another breed's.");
		if (b->imported && !b->source.empty())
			ImGui::TextDisabled("From %s", b->source.c_str());
		ImGui::PopTextWrapPos();

		bool canLay = b->parts.count('a') && b->parts.count('b');
		if (!canLay)
			ImGui::BeginDisabled();
		if (ImGui::Button("Lay an egg of this breed")) {
			auto genome = BreedBaseGenome(*b);
			if (genome.empty()) {
				s_breed_message = "No genome to start from (norn.gen, gren.gen or ettn.gen).";
			} else {
				std::string err;
				static int s_laid = 0;
				genome = GenomeWithBreed(genome, *b);
				if (LayC2Egg({genome}, rand_int32(1, 2), rand_int32(0, 10), fmt::format("b{:02d}", s_laid++ % 100), err))
					s_breed_message = fmt::format("Laid an egg of {} by the incubator.", BreedLabel(*b));
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
	ImGui::TextDisabled("Slots run a-z, then aa, ab ... up to iv (256 for each species).");
}

void DrawBreedersKit() {
	if (ImGui::BeginTabBar("pages")) {
		if (KitTab("Reproductive")) {
			ReproductivePage();
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
