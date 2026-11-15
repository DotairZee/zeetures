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
#include "common/io/FileWriter.h"
#include "common/io/SpanReader.h"
#include "common/io/VectorWriter.h"
#include "fileformats/genomeFile.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fmt/core.h>
#include <map>
#include <tuple>
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
	static const float k = getenv("OPENC2E_MUTATION_RATE") ? (float)atof(getenv("OPENC2E_MUTATION_RATE")) : 1.0f;
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
	if (r.bytes[7] < 255)
		r.bytes[7]++; // the gene's generation
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

bool closeKin(const std::string& a, const std::string& b) {
	if (a.empty() || b.empty() || a == b)
		return false;
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

	static const bool test = getenv("OPENC2E_TEST_CONCEIVE") != nullptr;
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
											   Relationships::mateFactor(a, b))
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
	mumBody->babymoniker = genomeNameToInt(name);
	mum->setPregnant(255);
	Relationships::conceived(mumAgent, dadAgent);
	world.history->tell(mumMon, storyEntry::pregnant, dadMon);
	world.history->tell(dadMon, storyEntry::fathered, mumMon);
	fmt::print("* MATE {} and {}: conceived, genome {} ({} genes: {} from her, {} from him, {} crossovers, {} mutations, {} duplications, {} losses)\n",
		mumMon, dadMon, name, rep.genes, rep.fromMum, rep.fromDad, rep.crossovers, rep.mutations, rep.duplications, rep.deletions);
	return true;
}
