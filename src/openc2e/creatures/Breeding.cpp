/*
 *  Breeding.cpp
 *  openc2e
 *
 *  C1/C2 breeding: a child's genome made from its parents' (crossover and
 *  mutation), and stored where a laid egg can find it again. See Breeding.h.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 */

#include "Breeding.h"
#include "common/devenv.h"
#include "Exposure.h"
#include "Temperament.h"
#include "Relationships.h"
#include <set>

#include "Agent.h"
#include "Engine.h"
#include "PathResolver.h"
#include "World.h"
#include "creatures/CreatureAgent.h"
#include "creatures/oldCreature.h"
#include "historyManager.h"
#include "common/Random.h"
#include "common/io/FileReader.h"
#include "common/io/FileWriter.h"
#include "common/io/SpanReader.h"
#include "common/io/VectorWriter.h"
#include "fileformats/genomeFile.h"

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif
#include <algorithm>
#include <cctype>
#include <ctime>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <fmt/core.h>
#include <map>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace {

// The rates (per gene): our own, kept gentle. A stock norn genome has about
// 770 genes, most marked able to mutate at full weighting, so a child gets
// about three small changes; a duplication or a loss is rare (about one
// child in four has one). OPENC2E_MUTATION_RATE scales all three (1 as
// here, 0 for none).
const float MUTATION_CHANCE = 1.0f / 250.0f;
const float DUPLICATION_CHANCE = 1.0f / 6000.0f;
const float DELETION_CHANCE = 1.0f / 6000.0f;
// the copying switches parent at random, on average every this many genes
const float SEGMENT_GENES = 40.0f;

struct Record {
	std::vector<uint8_t> bytes; // "gene", the header, the gene's own data
	uint8_t type() const { return bytes[4]; }
	uint8_t subtype() const { return bytes[5]; }
	uint8_t id() const { return bytes[6]; }
	uint8_t flags() const { return bytes[9]; }
	size_t dataStart(int version) const { return 10 + (version > 1 ? 1 : 0) + (version == 3 ? 1 : 0); }
	uint8_t weighting(int version) const { return version > 1 ? bytes[10] : 255; }
	std::tuple<uint8_t, uint8_t, uint8_t> key() const { return {type(), subtype(), id()}; }
};

std::vector<Record> records(genomeFile& g) {
	std::vector<Record> out;
	for (auto& gn : g.genes) {
		VectorWriter w;
		w << *gn;
		out.push_back(Record{w.vector()});
	}
	return out;
}

bool isOrgan(const Record& r) {
	return (r.type() == 0 && r.subtype() == 1) || (r.type() == 3 && r.subtype() == 0);
}

bool isGenus(const Record& r) {
	return r.type() == 2 && r.subtype() == 1;
}

float rateScale() {
	static const float k = devenv("OPENC2E_MUTATION_RATE") ? (float)atof(devenv("OPENC2E_MUTATION_RATE")) : 1.0f;
	return k;
}

// a small change to one byte of a gene's own data: mostly a nudge of a few
// steps up or down, now and then a new value altogether
void mutate(Record& r, int version) {
	size_t start = r.dataStart(version);
	if (r.bytes.size() <= start)
		return;
	size_t i = rand_size_t(start, r.bytes.size() - 1);
	int v = r.bytes[i];
	if (rand_float(0, 1) < 0.15f)
		v = rand_int32(0, 255);
	else {
		int step = rand_int32(1, 12);
		v += rand_bool() ? step : -step;
	}
	r.bytes[i] = (uint8_t)std::clamp(v, 0, 255);
}

} // namespace

std::shared_ptr<genomeFile> crossGenomes(genomeFile& mum, genomeFile& dad, const std::string& mumMoniker,
	const std::string& dadMoniker, CrossReport* report) {
	CrossReport rep;
	const int version = mum.getVersion();
	std::vector<Record> m = records(mum), d = records(dad);

	// pair the father's genes with the mother's: the nth gene of a given
	// type, subtype and identifier in each
	std::map<std::tuple<uint8_t, uint8_t, uint8_t>, std::vector<size_t>> dadAt;
	for (size_t j = 0; j < d.size(); j++)
		dadAt[d[j].key()].push_back(j);
	std::map<std::tuple<uint8_t, uint8_t, uint8_t>, size_t> mumSeen;
	std::vector<long> pairOfMum(m.size(), -1);
	std::vector<bool> dadPaired(d.size(), false);
	for (size_t i = 0; i < m.size(); i++) {
		size_t n = mumSeen[m[i].key()]++;
		auto it = dadAt.find(m[i].key());
		if (it != dadAt.end() && n < it->second.size()) {
			pairOfMum[i] = (long)it->second[n];
			dadPaired[it->second[n]] = true;
		}
	}

	std::vector<Record> child;
	bool fromDad = rand_bool();
	const float scale = rateScale();
	auto take = [&](Record r) {
		// mutation, duplication and loss, as the gene's flags allow
		uint8_t f = r.flags();
		bool canMutate = (f & 1) && !isGenus(r);
		bool canDup = (f & 2) && !isOrgan(r) && !isGenus(r);
		bool canDel = (f & 4) && !isOrgan(r) && !isGenus(r);
		if (canDel && rand_float(0, 1) < DELETION_CHANCE * scale) {
			rep.deletions++;
			return;
		}
		if (canMutate && rand_float(0, 1) < MUTATION_CHANCE * scale * (r.weighting(version) / 255.0f)) {
			mutate(r, version);
			rep.mutations++;
		}
		child.push_back(r);
		if (canDup && rand_float(0, 1) < DUPLICATION_CHANCE * scale) {
			// the copy is a generation on (the gene header's generation,
			// byte 7); a mutation leaves it as it is. The brain places a
			// duplicated lobe after the rest (BrainProfiles::laterLobe), so
			// it does not renumber the lobes after it
			if (r.bytes[7] < 255)
				r.bytes[7]++;
			child.push_back(r);
			rep.duplications++;
		}
	};

	long lastDad = -1;
	for (size_t i = 0; i < m.size(); i++) {
		if (rand_float(0, 1) < 1.0f / SEGMENT_GENES) {
			fromDad = !fromDad;
			rep.crossovers++;
		}
		long j = pairOfMum[i];
		// the father's own genes (ones the mother has no copy of) that come
		// before this one in his genome, while copying from him
		if (j >= 0) {
			for (long k = lastDad + 1; k < j; k++)
				if (!dadPaired[k] && fromDad) {
					take(d[k]);
					rep.fromDad++;
				}
			lastDad = std::max(lastDad, j);
		}
		if (fromDad && j >= 0) {
			take(d[j]);
			rep.fromDad++;
		} else if (!fromDad && j < 0) {
			// the mother's own gene, while copying from her
			take(m[i]);
			rep.fromMum++;
		} else if (!fromDad) {
			take(m[i]);
			rep.fromMum++;
		}
		// (copying from the father where he has no copy: the gene is left
		// out, as his genome has none there)
	}
	for (long k = lastDad + 1; k < (long)d.size(); k++)
		if (!dadPaired[k] && fromDad) {
			take(d[k]);
			rep.fromDad++;
		}

	// read the child's genes back into a genome
	VectorWriter w;
	char majic[4] = {'d', 'n', 'a', static_cast<char>(version + 48)};
	w.write(majic, 4);
	for (auto& r : child)
		w.write(reinterpret_cast<const char*>(r.bytes.data()), r.bytes.size());
	w.write("gend", 4);
	auto out = std::make_shared<genomeFile>();
	SpanReader rd(std::span<const uint8_t>(w.vector().data(), w.vector().size()));
	rd >> *out;

	// its genus gene names its parents
	for (auto& gn : out->genes)
		if (auto* genus = dynamic_cast<creatureGenusGene*>(gn.get())) {
			genus->mum = mumMoniker;
			genus->dad = dadMoniker;
			genus->mum.resize(version == 3 ? 32 : 4, '\0');
			genus->dad.resize(version == 3 ? 32 : 4, '\0');
			break;
		}

	rep.genes = (int)out->genes.size();
	if (report)
		*report = rep;
	return out;
}

int genomeNameToInt(const std::string& name) {
	int v = 0;
	for (int i = 3; i >= 0; i--)
		v = (v << 8) | (unsigned char)(i < (int)name.size() ? name[i] : 0);
	return v;
}

std::string genomeNameFromInt(int value) {
	return std::string((char*)&value, 4);
}

std::shared_ptr<genomeFile> mutatedCopy(genomeFile& g, int count, int* applied) {
	int version = g.getVersion();
	std::vector<Record> recs = records(g);
	std::vector<size_t> can;
	for (size_t i = 0; i < recs.size(); i++)
		if ((recs[i].flags() & 1) && !isGenus(recs[i]) && recs[i].weighting(version) > 0)
			can.push_back(i);
	int made = 0;
	for (int k = 0; k < count && !can.empty(); k++) {
		Record& r = recs[can[rand_size_t(0, can.size() - 1)]];
		// a gene weighted to mutate less is hit less often
		if (rand_float(0, 1) < r.weighting(version) / 255.0f) {
			mutate(r, version);
			made++;
		}
	}
	VectorWriter w;
	char majic[4] = {'d', 'n', 'a', static_cast<char>(version + 48)};
	w.write(majic, 4);
	for (auto& r : recs)
		w.write(reinterpret_cast<const char*>(r.bytes.data()), r.bytes.size());
	w.write("gend", 4);
	auto out = std::make_shared<genomeFile>();
	SpanReader rd(std::span<const uint8_t>(w.vector().data(), w.vector().size()));
	rd >> *out;
	if (applied)
		*applied = made;
	return out;
}

std::string storeGenome(genomeFile& g) {
	static const char letters[] = "0123456789abcdefghijklmnopqrstuvwxyz";
	fs::path dir = getUserDataDir() / "Genetics";
	std::error_code ec;
	fs::create_directories(dir, ec);
	for (int attempt = 0; attempt < 1000; attempt++) {
		// "z" and three letters or digits: apart from the game's own names
		std::string name = "z";
		for (int i = 0; i < 3; i++)
			name += letters[rand_int32(0, 35)];
		if (!findGeneticsFiles(name + ".gen").empty() || fs::exists(dir / (name + ".gen")))
			continue;
		try {
			FileWriter f(dir / (name + ".gen"));
			f << g;
		} catch (std::exception& e) {
			fmt::print("* Couldn't write a child's genome: {}\n", e.what());
			return "";
		}
		return name;
	}
	return "";
}

// Fertility by life stage (owner, 2026-11-10): none as a baby or child, low
// as an adolescent, moderate as a youth, highest as an adult, low again when
// old (0.2), none when senile. Both partners' shares multiply the chance.
float stageFertility(int stage) {
	static const float share[7] = {0.0f, 0.0f, 0.25f, 0.6f, 1.0f, 0.2f, 0.0f};
	return (stage >= 0 && stage < 7) ? share[stage] : 0.0f;
}

namespace {

std::set<std::string> ancestorsOf(const std::string& moniker) {
	std::set<std::string> out;
	std::vector<std::string> todo{moniker};
	for (int depth = 0; depth < 12 && !todo.empty(); depth++) {
		std::vector<std::string> next;
		for (auto& m : todo) {
			if (m.empty() || !world.history->hasMoniker(m))
				continue;
			monikerData& d = world.history->getMoniker(m);
			for (const std::string* parent : {&d.mother, &d.father})
				if (!parent->empty() && out.insert(*parent).second)
					next.push_back(*parent);
		}
		todo.swap(next);
	}
	return out;
}

std::string monikerOf(CreatureAgent* a) {
	oldCreature* c = a ? dynamic_cast<oldCreature*>(a->getCreature()) : nullptr;
	return c ? world.history->findMoniker(c->getGenome()) : std::string();
}

} // namespace

namespace {

// closeKin's answers, until the lineage changes (historyManager::
// lineageGeneration): creatures ask it for every possible mate in sight, and
// each answer walks both family trees (performance, 2026-11-16)
struct KinKeyHash {
	size_t operator()(const std::pair<std::string, std::string>& k) const {
		return std::hash<std::string>()(k.first) * 31 + std::hash<std::string>()(k.second);
	}
};
std::unordered_map<std::pair<std::string, std::string>, bool, KinKeyHash> kinAnswers;
unsigned int kinAnswersGeneration = (unsigned int)-1;

bool closeKinUncached(const std::string& a, const std::string& b);

} // namespace

bool closeKin(const std::string& a, const std::string& b) {
	if (a.empty() || b.empty() || a == b)
		return false;
	if (kinAnswersGeneration != historyManager::lineageGeneration) {
		kinAnswers.clear();
		kinAnswersGeneration = historyManager::lineageGeneration;
	}
	// the answer is symmetric: one entry for both orders
	auto key = a < b ? std::make_pair(a, b) : std::make_pair(b, a);
	auto it = kinAnswers.find(key);
	if (it != kinAnswers.end())
		return it->second;
	bool kin = closeKinUncached(a, b);
	kinAnswers.emplace(std::move(key), kin);
	return kin;
}

namespace {

bool closeKinUncached(const std::string& a, const std::string& b) {
	std::set<std::string> ancA = ancestorsOf(a), ancB = ancestorsOf(b);
	if (ancA.count(b) || ancB.count(a))
		return true; // one descends from the other
	if (!world.history->hasMoniker(a) || !world.history->hasMoniker(b))
		return false;
	monikerData& da = world.history->getMoniker(a);
	monikerData& db = world.history->getMoniker(b);
	auto shared = [](const std::string& x, const std::string& y) { return !x.empty() && x == y; };
	return shared(da.mother, db.mother) || shared(da.father, db.father) || shared(da.mother, db.father) || shared(da.father, db.mother);
}

} // namespace

bool kinBarred(CreatureAgent* a, CreatureAgent* b) {
	std::string ma = monikerOf(a), mb = monikerOf(b);
	if (!closeKin(ma, mb))
		return false;
	// an unrelated partner for either of them?
	for (CreatureAgent* self : {a, b}) {
		Agent* body = dynamic_cast<Agent*>(self);
		oldCreature* cs = dynamic_cast<oldCreature*>(self->getCreature());
		std::string ms = monikerOf(self);
		if (!body || !cs)
			continue;
		for (auto& other : world.agents) {
			if (!other || other.get() == body || other->isDying() || other->genus != body->genus)
				continue;
			CreatureAgent* ca = dynamic_cast<CreatureAgent*>(other.get());
			oldCreature* co = ca ? dynamic_cast<oldCreature*>(ca->getCreature()) : nullptr;
			if (!co || ca == a || ca == b || !co->isAlive() || co->isFemale() == cs->isFemale())
				continue;
			int stage = (int)co->getStage();
			if (stage < 2 || stage > 5)
				continue;
			if (!closeKin(ms, monikerOf(ca)))
				return true;
		}
	}
	return false; // no one else: kin may breed
}

bool fertileBySetting(const oldCreature* c) {
	if (!c)
		return false;
	return c->fertilityOverride >= 0 ? c->fertilityOverride == 0 : world.allFertile;
}

static fs::path departedDir() {
	return engine.currentworlddir / "departed";
}

void keepDepartedGenes(const std::string& moniker, genomeFile& g, const Temperament& t, float seasonal, bool female) {
	if (moniker.empty() || engine.currentworlddir.empty())
		return;
	std::error_code ec;
	fs::create_directories(departedDir(), ec);
	try {
		FileWriter f(departedDir() / (moniker + ".gen"));
		f << g;
		std::ofstream tf(departedDir() / (moniker + ".traits"));
		tf << fmt::format("female {}\nseasonal {:.4f}\ntemperament", female ? 1 : 0, seasonal);
		for (float z : t.z)
			tf << fmt::format(" {:.4f}", z);
		tf << "\n";
	} catch (std::exception& e) {
		fmt::print("* Couldn't keep {}'s genes: {}\n", moniker, e.what());
	}
}

int departedSex(const std::string& moniker) {
	if (moniker.empty() || engine.currentworlddir.empty())
		return -1;
	std::ifstream tf(departedDir() / (moniker + ".traits"));
	std::string key;
	int v;
	while (tf >> key)
		if (key == "female" && (tf >> v))
			return v ? 1 : 0;
	return -1;
}

bool departedGenesKept(const std::string& moniker) {
	std::error_code ec;
	return !moniker.empty() && !engine.currentworlddir.empty() && fs::exists(departedDir() / (moniker + ".gen"), ec);
}

bool loadDepartedGenes(const std::string& moniker, std::shared_ptr<genomeFile>& g, Temperament& t, bool& hasTraits, float& seasonal) {
	hasTraits = false;
	if (!departedGenesKept(moniker))
		return false;
	try {
		g = std::make_shared<genomeFile>();
		FileReader f(departedDir() / (moniker + ".gen"));
		f >> *g;
	} catch (std::exception& e) {
		g.reset();
		return false;
	}
	std::ifstream tf(departedDir() / (moniker + ".traits"));
	std::string key;
	bool s = false, te = false;
	while (tf >> key) {
		if (key == "female") {
			int v;
			tf >> v;
		} else if (key == "seasonal")
			s = (bool)(tf >> seasonal);
		else if (key == "temperament") {
			te = true;
			for (auto& z : t.z)
				te = te && (bool)(tf >> z);
		}
	}
	hasTraits = s && te;
	return true;
}

// a file to the system's Trash (recoverable); false if it could not be
static bool moveToTrash(const fs::path& file) {
	std::error_code ec;
#if defined(_WIN32)
	// the Recycle Bin (SHFileOperation with undo)
	std::wstring from = file.wstring();
	from.push_back(L'\0');
	from.push_back(L'\0');
	SHFILEOPSTRUCTW op{};
	op.wFunc = FO_DELETE;
	op.pFrom = from.c_str();
	op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
	return SHFileOperationW(&op) == 0 && !op.fAnyOperationsAborted;
#elif defined(__APPLE__)
	const char* home = getenv("HOME");
	if (!home)
		return false;
	fs::path trash = fs::path(home) / ".Trash";
	fs::create_directories(trash, ec);
	fs::path to = trash / file.filename();
	for (int i = 1; fs::exists(to, ec) && i < 1000; i++)
		to = trash / fmt::format("{} {}{}", file.stem().string(), i, file.extension().string());
	fs::rename(file, to, ec);
	return !ec;
#else
	// the freedesktop.org Trash ($XDG_DATA_HOME/Trash, or ~/.local/share/Trash)
	fs::path base;
	if (const char* x = getenv("XDG_DATA_HOME"); x && *x)
		base = x;
	else if (const char* home = getenv("HOME"))
		base = fs::path(home) / ".local" / "share";
	else
		return false;
	fs::path files = base / "Trash" / "files", info = base / "Trash" / "info";
	fs::create_directories(files, ec);
	fs::create_directories(info, ec);
	std::string name = file.filename().string();
	for (int i = 1; (fs::exists(files / name, ec) || fs::exists(info / (name + ".trashinfo"), ec)) && i < 1000; i++)
		name = fmt::format("{}.{}{}", file.stem().string(), i, file.extension().string());
	{
		std::time_t now = std::time(nullptr);
		char when[32];
		std::strftime(when, sizeof when, "%Y-%m-%dT%H:%M:%S", std::localtime(&now));
		std::ofstream ti(info / (name + ".trashinfo"));
		ti << "[Trash Info]\nPath=" << fs::absolute(file).string() << "\nDeletionDate=" << when << "\n";
	}
	fs::rename(file, files / name, ec);
	if (ec) {
		// another drive: copy, then remove
		std::error_code ec2;
		fs::copy_file(file, files / name, ec2);
		if (!ec2)
			fs::remove(file, ec2);
		if (ec2) {
			fs::remove(info / (name + ".trashinfo"), ec2);
			return false;
		}
	}
	return true;
#endif
}

static bool generatedName(const std::string& stem) {
	if (stem.size() != 4 || stem[0] != 'z')
		return false;
	for (size_t i = 1; i < 4; i++)
		if (!((stem[i] >= '0' && stem[i] <= '9') || (stem[i] >= 'a' && stem[i] <= 'z')))
			return false;
	return true;
}

void tidyGeneratedGenomes() {
	if (engine.version != 2 || devenv("OPENC2E_NO_GENOME_TIDY") || engine.currentworlddir.empty())
		return;
	std::error_code ec;
	fs::path genetics = getUserDataDir() / "Genetics";
	fs::path worlds = engine.currentworlddir.parent_path();
	if (!fs::is_directory(genetics, ec) || !fs::is_directory(worlds, ec))
		return;
	// every generated name any world's saved text mentions (an egg's genome,
	// a pregnancy's baby, a record): read whole, the names picked out
	std::set<std::string> used;
	int savesRead = 0;
	for (auto& w : fs::directory_iterator(worlds, ec)) {
		std::error_code e2;
		for (auto& f : fs::recursive_directory_iterator(w.path(), e2)) {
			if (!f.is_regular_file(e2) || f.path().extension() != ".txt")
				continue;
			std::ifstream in(f.path(), std::ios::binary);
			std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
			if (f.path().filename() == "state.txt")
				savesRead++;
			for (size_t i = 0; i + 4 <= text.size(); i++) {
				if (text[i] != 'z' || (i > 0 && std::isalnum((unsigned char)text[i - 1])))
					continue;
				if (i + 4 < text.size() && std::isalnum((unsigned char)text[i + 4]))
					continue;
				std::string n = text.substr(i, 4);
				if (generatedName(n))
					used.insert(n);
			}
		}
	}
	if (savesRead == 0)
		return; // nothing read: keep everything
	auto cutoff = fs::file_time_type::clock::now() - std::chrono::hours(24);
	std::vector<fs::path> gone;
	for (auto& f : fs::directory_iterator(genetics, ec)) {
		std::error_code e2;
		if (!f.is_regular_file(e2))
			continue;
		std::string ext = f.path().extension().string(), stem = f.path().stem().string();
		if ((ext != ".gen" && ext != ".season" && ext != ".temperament" && ext != ".mind") || !generatedName(stem) || used.count(stem))
			continue;
		if (f.last_write_time(e2) > cutoff || e2)
			continue;
		gone.push_back(f.path());
	}
	if (gone.empty())
		return;
	auto t0 = std::chrono::steady_clock::now();
	int moved = 0, failed = 0;
	for (auto& f : gone)
		(moveToTrash(f) ? moved : failed)++;
	double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	fmt::print("* Tidied the Genetics folder: {} unused generated genome files to the Trash ({} kept: named in a world's save; {} could not be moved; {:.1f} s)\n",
		moved, used.size(), failed, secs);
}

std::string inseminationBlock(CreatureAgent* mumAgent) {
	if (!mumAgent || engine.version != 2)
		return "only in Creatures 2";
	oldCreature* mum = dynamic_cast<oldCreature*>(mumAgent->getCreature());
	Agent* body = dynamic_cast<Agent*>(mumAgent);
	if (!mum || !body || !mum->isAlive())
		return "she is not alive";
	if (!mum->isFemale())
		return "only a female can carry an egg";
	if ((int)mum->getStage() < 2)
		return "she is too young: creatures carry eggs from adolescence";
	if (body->babymoniker != 0 || mum->getPregnant() != 0)
		return "she is already pregnant";
	if (!fertileBySetting(mum))
		return "her Infertile box is ticked (Reproductive tab)";
	return "";
}

bool inseminate(CreatureAgent* mumAgent, const InseminationDonor& donor, std::string& why) {
	why = inseminationBlock(mumAgent);
	if (why.empty() && !donor.genome)
		why = "the donor's genes could not be read";
	oldCreature* mum = mumAgent ? dynamic_cast<oldCreature*>(mumAgent->getCreature()) : nullptr;
	Agent* mumBody = dynamic_cast<Agent*>(mumAgent);
	if (why.empty()) {
		// the same kind: the genus gene's genus (0 norn, 1 grendel, 2 ettin)
		int donorGenus = -1;
		for (auto& gn : donor.genome->genes)
			if (auto* g = dynamic_cast<creatureGenusGene*>(gn.get())) {
				donorGenus = g->genus;
				break;
			}
		if (donorGenus != (int)mum->getGenus())
			why = "the donor is not of her kind";
	}
	std::string mumMon = mum ? world.history->findMoniker(mum->getGenome()) : std::string();
	std::string donorName = donor.moniker.empty() ? donor.label : donor.moniker;
	if (!why.empty()) {
		fmt::print("* INSEMINATE {} with {}: not done ({})\n", mumMon, donorName, why);
		return false;
	}
	CrossReport rep;
	std::shared_ptr<genomeFile> child = crossGenomes(*mum->getGenome(), *donor.genome, mumMon, donor.moniker, &rep);
	std::string name = storeGenome(*child);
	if (name.empty()) {
		why = "the baby's genome could not be stored";
		return false;
	}
	float donorZ = donor.seasonal ? *donor.seasonal : SeasonalRhythm::roll();
	float childSeasonal = SeasonalRhythm::child(mum->seasonalZ, donorZ);
	SeasonalRhythm::store(name, childSeasonal);
	Temperament donorT = donor.temperament ? *donor.temperament : Temperaments::roll();
	Temperament childTemperament = Temperaments::child(mum->inheritableTemperament(), donorT);
	Temperaments::store(name, childTemperament);
	MindGenes donorM;
	if (donor.moniker.empty() || !MindGeneSet::load(donor.moniker, donorM))
		donorM = MindGeneSet::roll();
	MindGeneSet::store(name, MindGeneSet::child(mum->mindGenes, donorM));
	mumBody->babymoniker = genomeNameToInt(name);
	mum->setPregnant(255);
	// her life story: pregnant, marked inseminated (the Chronicles Kit words
	// it so); the donor's too, if it is a creature with a record
	world.history->tell(mumMon, storyEntry::pregnant, donor.moniker, donor.moniker.empty() ? "inseminated:" + donor.label : "inseminated");
	if (!donor.moniker.empty())
		world.history->tell(donor.moniker, storyEntry::fathered, mumMon, "donor");
	fmt::print("* INSEMINATE {} with {}: conceived, genome {} ({} genes: {} from her, {} from the donor, {} crossovers, {} mutations, {} duplications, {} losses; seasonal rhythm {:+.2f}; temperament {})\n",
		mumMon, donorName, name, rep.genes, rep.fromMum, rep.fromDad, rep.crossovers, rep.mutations, rep.duplications, rep.deletions, childSeasonal,
		Temperaments::toLine(childTemperament));
	return true;
}

bool tryConceive(CreatureAgent* a, CreatureAgent* b) {
	if (!a || !b || engine.version != 2 || a == b)
		return false;
	oldCreature* ca = dynamic_cast<oldCreature*>(a->getCreature());
	oldCreature* cb = dynamic_cast<oldCreature*>(b->getCreature());
	if (!ca || !cb || ca->isFemale() == cb->isFemale())
		return false;
	CreatureAgent* mumAgent = ca->isFemale() ? a : b;
	CreatureAgent* dadAgent = ca->isFemale() ? b : a;
	oldCreature* mum = ca->isFemale() ? ca : cb;
	oldCreature* dad = ca->isFemale() ? cb : ca;
	Agent* mumBody = dynamic_cast<Agent*>(mumAgent);
	if (!mumBody)
		return false;

	static const bool test = devenv("OPENC2E_TEST_CONCEIVE") != nullptr;
	const char* why = nullptr;
	if (mumBody->babymoniker != 0 || mum->getPregnant() != 0)
		why = "she is already pregnant";
	else if (!fertileBySetting(mum))
		why = "she is set infertile";
	else if (!fertileBySetting(dad))
		why = "he is set infertile";
	else if (kinBarred(a, b))
		why = "close kin, and an unrelated partner is alive";
	else if (!test && Relationships::mateFactor(a, b) <= 0.0f)
		why = "they dislike or fear each other";
	else if (!test && mum->getFertile() == 0)
		why = "she is not fertile";
	else if (!test && dad->getFertile() == 0)
		why = "he is not fertile";
	// her receptiveness adds to the chance rather than being required: with
	// the body chemistry as openc2e runs it, a norn's fertility (105) rarely
	// climbs high enough for her genes to make her receptive (2026-11-10)
	else if (!test && rand_float(0, 1) > (mum->getFertile() / 255.0f) * (0.5f + 0.5f * mum->getReceptive() / 255.0f) *
											   stageFertility((int)mum->getStage()) * stageFertility((int)dad->getStage()) *
											   Relationships::mateFactor(a, b) *
											   // the breeding season (Temperament.h, SeasonalRhythm)
											   SeasonalRhythm::fertilityFactor((int)world.season, mum->seasonalZ, (int)mum->getStage(),
												   dad->seasonalZ, (int)dad->getStage()) *
											   // radiation (Exposure.h; owner, 2026-11-21)
											   Exposure::fertilityFactor(world.history->findMoniker(mum->getGenome())) *
											   Exposure::fertilityFactor(world.history->findMoniker(dad->getGenome())))
		why = "chance";
	std::string mumMon = world.history->findMoniker(mum->getGenome());
	std::string dadMon = world.history->findMoniker(dad->getGenome());
	if (why) {
		fmt::print("* MATE {} and {}: no conception ({}; her fertile {} receptive {} stage {}, his fertile {} stage {})\n", mumMon, dadMon, why,
			(int)mum->getFertile(), (int)mum->getReceptive(), (int)mum->getStage(), (int)dad->getFertile(), (int)dad->getStage());
		return false;
	}

	CrossReport rep;
	std::shared_ptr<genomeFile> child = crossGenomes(*mum->getGenome(), *dad->getGenome(), mumMon, dadMon, &rep);
	std::string name = storeGenome(*child);
	if (name.empty())
		return false;
	// the child's seasonal rhythm, from both parents (Temperament.h)
	float childSeasonal = SeasonalRhythm::child(mum->seasonalZ, dad->seasonalZ);
	SeasonalRhythm::store(name, childSeasonal);
	// and its temperament, partly from them (Temperament.h)
	Temperament childTemperament = Temperaments::child(mum->inheritableTemperament(), dad->inheritableTemperament());
	Temperaments::store(name, childTemperament);
	// and its mind's own settings, the same way (MindGenes.h)
	MindGeneSet::store(name, MindGeneSet::child(mum->mindGenes, dad->mindGenes));
	mumBody->babymoniker = genomeNameToInt(name);
	mum->setPregnant(255);
	Relationships::conceived(mumAgent, dadAgent);
	world.history->tell(mumMon, storyEntry::pregnant, dadMon);
	world.history->tell(dadMon, storyEntry::fathered, mumMon);
	fmt::print("* MATE {} and {}: conceived, genome {} ({} genes: {} from her, {} from him, {} crossovers, {} mutations, {} duplications, {} losses; seasonal rhythm {:+.2f} from {:+.2f} and {:+.2f}; temperament {})\n",
		mumMon, dadMon, name, rep.genes, rep.fromMum, rep.fromDad, rep.crossovers, rep.mutations, rep.duplications, rep.deletions, childSeasonal,
		mum->seasonalZ, dad->seasonalZ, Temperaments::toLine(childTemperament));
	return true;
}
