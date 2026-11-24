#include "common/io/SpanReader.h"
#include "common/io/VectorWriter.h"
#include "fileformats/genomeFile.h"

#include <cstdlib>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <gtest/gtest.h>
#include <algorithm>
#include <vector>

// Reading a genome and writing it back must give the same genes, "gend",
// and for Creatures 2 the fixed 36001-byte block, zero-padded.

// the genome up to and including "gend"
static std::vector<uint8_t> upToEnd(const std::vector<uint8_t>& d) {
	for (size_t k = 4; k + 4 <= d.size(); k++)
		if (d[k] == 'g' && d[k + 1] == 'e' && d[k + 2] == 'n' && d[k + 3] == 'd')
			return std::vector<uint8_t>(d.begin(), d.begin() + k + 4);
	return d;
}

static bool zeroPaddedTo(const std::vector<uint8_t>& d, size_t from, size_t size) {
	if (d.size() != size)
		return false;
	for (size_t i = from; i < d.size(); i++)
		if (d[i])
			return false;
	return true;
}

static std::vector<uint8_t> roundTrip(const std::vector<uint8_t>& in) {
	genomeFile g;
	SpanReader r(std::span<const uint8_t>(in.data(), in.size()));
	r >> g;
	VectorWriter w;
	w << g;
	return w.vector();
}

// where the first difference is, as gene number, type and offset in it
static std::string firstDifference(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
	size_t n = std::min(a.size(), b.size()), i = 0;
	while (i < n && a[i] == b[i])
		i++;
	if (i == n && a.size() == b.size())
		return "identical";
	// the gene the byte belongs to: the last "gene" marker before it
	size_t start = 0;
	int gene = -1;
	for (size_t k = 4; k + 4 <= a.size() && k <= i; k++)
		if (a[k] == 'g' && a[k + 1] == 'e' && a[k + 2] == 'n' && a[k + 3] == 'e') {
			start = k;
			gene++;
		}
	return fmt::format("sizes {} and {}; first difference at byte {}, gene {} (type {} subtype {}), {} bytes into it: {} vs {}",
		a.size(), b.size(), i, gene, a[start + 4], a[start + 5], i - start, i < a.size() ? a[i] : -1, i < b.size() ? b[i] : -1);
}

// A C2 genome (version 2) with one gene of most kinds, by hand. Each gene:
// "gene", type, subtype, then which, generation, switch-on stage, flags,
// mutation weight, then its own data.
static std::vector<uint8_t> syntheticC2Genome() {
	std::vector<uint8_t> d = {'d', 'n', 'a', '2'};
	auto gene = [&](uint8_t type, uint8_t sub, std::vector<uint8_t> data) {
		for (uint8_t c : {'g', 'e', 'n', 'e'})
			d.push_back(c);
		d.push_back(type);
		d.push_back(sub);
		for (uint8_t c : {1, 0, 4, 1, 128}) // which, generation, adult, mutable, weight
			d.push_back(c);
		d.insert(d.end(), data.begin(), data.end());
	};
	gene(3, 0, {5, 10, 200, 0, 20});                 // organ
	gene(1, 0, {1, 2, 3, 99, 50, 60, 70, 1});        // receptor
	gene(1, 1, {1, 2, 3, 99, 50, 60, 70, 2});        // emitter
	gene(1, 2, {1, 3, 1, 4, 1, 5, 1, 6, 40});        // reaction
	gene(1, 4, {72, 150});                           // initial concentration
	gene(2, 2, {0, 7, 0});                           // appearance
	return d;
}

TEST(GenomeFile, SyntheticC2GenomeRoundTrips) {
	auto in = syntheticC2Genome();
	in.insert(in.end(), {'g', 'e', 'n', 'd'});
	auto out = roundTrip(in);
	EXPECT_EQ(in, upToEnd(out)) << firstDifference(in, out);
	EXPECT_TRUE(zeroPaddedTo(out, in.size(), C2_GENOME_BLOCK)) << "written size " << out.size();
}

// OPENC2E_TEST_GENOME_DIR=".../Creatures 2/Genetics" also checks every .gen
// there (the games' genomes are not part of openc2e)
TEST(GenomeFile, RealGenomesRoundTrip) {
	const char* dir = getenv("OPENC2E_TEST_GENOME_DIR");
	if (!dir)
		GTEST_SKIP() << "set OPENC2E_TEST_GENOME_DIR to check real genomes";
	int checked = 0;
	for (auto& e : std::filesystem::directory_iterator(dir)) {
		auto ext = e.path().extension().string();
		if (ext != ".gen" && ext != ".GEN")
			continue;
		std::ifstream f(e.path(), std::ios::binary);
		std::vector<uint8_t> in((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
		auto out = roundTrip(in);
		auto genes = upToEnd(in);
		EXPECT_TRUE(genes == upToEnd(out)) << e.path().filename() << ": " << firstDifference(genes, upToEnd(out));
		EXPECT_TRUE(zeroPaddedTo(out, genes.size(), std::max(C2_GENOME_BLOCK, genes.size()))) << e.path().filename() << ": written size " << out.size();
		checked++;
	}
	EXPECT_GT(checked, 0);
}

// A C2 genome is read gene by gene from marker to marker: a gene with a stray
// byte, or a C3-only gene, does not stop the rest being read, and each is
// written back as it came.
TEST(GenomeFile, C2GenesReadMarkerToMarker) {
	std::vector<uint8_t> d = {'d', 'n', 'a', '2'};
	auto add = [&](std::vector<uint8_t> g) {
		d.insert(d.end(), {'g', 'e', 'n', 'e'});
		d.insert(d.end(), g.begin(), g.end());
	};
	// genus gene (2 1): header, genus, mum, dad
	add({2, 1, 1, 0, 0, 0, 128, 0, 'A', 'B', 'C', 'D', 0, 0, 0, 0});
	// a half-lives gene (1 3) with one byte too many
	std::vector<uint8_t> hl = {1, 3, 2, 0, 0, 1, 128};
	for (int i = 0; i < 256; i++)
		hl.push_back((uint8_t)i);
	hl.push_back(77);
	add(hl);
	// a C3 facial expression gene (2 8): read as a stimulus gene (2 0)
	add({2, 8, 3, 0, 0, 1, 128, 5, 6, 7, 8, 1, 10, 11, 12, 13, 14, 15});
	// an organ gene (3 0)
	add({3, 0, 4, 0, 0, 1, 128, 1, 2, 3, 4, 5});
	d.insert(d.end(), {'g', 'e', 'n', 'd'});

	genomeFile g;
	SpanReader r(std::span<const uint8_t>(d.data(), d.size()));
	ASSERT_NO_THROW(r >> g);
	ASSERT_EQ(g.genes.size(), 4u);
	EXPECT_EQ(g.genes[1]->rawTail, std::vector<uint8_t>{77});
	EXPECT_EQ(g.genes[2]->geneType(), 2);
	EXPECT_EQ(g.genes[2]->geneSubtype(), 0);
	EXPECT_EQ(g.genes[3]->geneType(), 3);
	auto out = roundTrip(d);
	EXPECT_TRUE(d == upToEnd(out)) << firstDifference(d, upToEnd(out));
}
