/*
 *  Breeding.h
 *  openc2e
 *
 *  C1/C2 breeding: a child's genome made from its parents' (crossover and
 *  mutation), and stored where a laid egg can find it again.
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

#pragma once

#include <memory>
#include <string>

class genomeFile;
class CreatureAgent;
struct Temperament;

struct CrossReport {
	int genes = 0; // in the child
	int fromMum = 0, fromDad = 0; // genes taken from each parent's copy
	int crossovers = 0; // times the copying switched parent
	int mutations = 0, duplications = 0, deletions = 0;
};

// A child's genome from its mother's and father's: the genes paired up
// between the two (by type, subtype and identifier, in the mother's order),
// each copied from one parent, the copying switching parent now and then;
// genes the genome marks as able to mutate occasionally changed a little,
// weighted by each gene's own mutation weighting; genes marked duplicable or
// deletable very rarely copied or lost. The child's genus gene names its
// parents (by moniker). Our own recipe: C2's is not documented.
std::shared_ptr<genomeFile> crossGenomes(genomeFile& mum, genomeFile& dad, const std::string& mumMoniker,
	const std::string& dadMoniker, CrossReport* report = nullptr);

// Writes a genome to the user data folder's Genetics under a new four-letter
// name (as an egg's genome is named in C2) and returns that name, or "" if
// it could not be written.
std::string storeGenome(genomeFile& g);

// A copy of a genome with `count` more mutations (as a child's: a small
// change to one byte of a gene that may mutate, its generation counted up),
// for radiation before birth (Exposure.h). `applied`: how many were made.
std::shared_ptr<genomeFile> mutatedCopy(genomeFile& g, int count, int* applied = nullptr);

// A four-letter genome name as the integer C2 scripts carry it (BABY, an
// egg's OV00, NEW: CREA), and back.
int genomeNameToInt(const std::string& name);
std::string genomeNameFromInt(int value);

// MATE: if one of the two is female and the other male, she is fertile and
// receptive (her reproductive loci) and not already pregnant, and he is
// fertile, she conceives (with a chance: her fertility times both partners'
// life-stage shares, stageFertility): the baby's genome is made from both (crossGenomes)
// and stored (storeGenome), she carries its name as BABY until the world's
// egg-laying script lays it in an egg, and her pregnant locus is set. C2
// only. OPENC2E_TEST_CONCEIVE=1 skips the fertility checks. Returns whether
// she conceived.
bool tryConceive(CreatureAgent* a, CreatureAgent* b);

// Insemination (Breeder's Kit, owner 2026-11-20): she conceives from a
// donor's genes, any creature of her kind, male or female, living, departed
// or a genome file. Her own readiness is checked (adolescent or older, not
// pregnant, not set infertile; the donor its kind), not her fertility cycle
// or any mating chance: the owner chose it. The genomes are crossed as a
// mating does (crossGenomes), the child's temperament and seasonal rhythm
// come from both (a donor without stored ones draws them at random), she
// carries the egg as BABY and lays it as usual. donorMoniker is empty for a
// genome file. Returns whether she conceived; why says why not.
struct InseminationDonor {
	std::shared_ptr<genomeFile> genome;
	std::string moniker; // empty: a genome file, no creature
	std::string label; // for the log and her life story when there is no moniker
	const Temperament* temperament = nullptr; // or nullptr: drawn at random
	const float* seasonal = nullptr; // or nullptr: drawn at random
};
bool inseminate(CreatureAgent* mum, const InseminationDonor& donor, std::string& why);
// A departed creature's genes, for insemination later: its genome, its
// temperament and seasonal rhythm, kept in the world's folder (departed/
// MONIKER.gen and MONIKER.traits) when it dies. C2 only.
void keepDepartedGenes(const std::string& moniker, genomeFile& g, const Temperament& t, float seasonal, bool female);
// its sex as kept (1 female, 0 male, -1 not known)
int departedSex(const std::string& moniker);
// whether a departed creature's genes were kept (its traits too, if their file is there)
bool departedGenesKept(const std::string& moniker);
bool loadDepartedGenes(const std::string& moniker, std::shared_ptr<genomeFile>& g, Temperament& t, bool& hasTraits, float& seasonal);
// Tidying the player's Genetics folder (owner, 2026-11-20: "tidy at world
// load"): every conception, NEW: GENE and splice writes a generated genome
// there ("z" and three letters, with .season and .temperament beside it),
// and most are needed only until the baby hatches (about 7,000 unused,
// 300 MB, in Home's folder by then). After a world loads, a generated
// genome that no world's save names, last changed over a day ago, goes to
// the system's Trash with its side files. Only generated names, only in the
// user data folder's Genetics. OPENC2E_NO_GENOME_TIDY turns it off. C2 only.
void tidyGeneratedGenomes();
// what stops her from being inseminated now ("" if nothing)
std::string inseminationBlock(CreatureAgent* mum);

// Close kin (owner, 2026-11-11: no incest unless there is no other choice):
// one descends from the other (parent, grandparent ...) or they share a
// parent, by the family records (monikers' mother and father).
bool closeKin(const std::string& monikerA, const std::string& monikerB);
// Kin recognition, as many animals do it by individual scent: two creatures
// are barred as mates when they are close kin and either of them has an
// unrelated partner alive (its own kind, the other sex, adolescent to old).
// Barred pairs do not conceive, and do not count for each other as the other
// sex in sight (so they arouse no mating chemistry).
bool kinBarred(CreatureAgent* a, CreatureAgent* b);

// Whether the owner's settings let it breed: its own Breeder's Kit setting
// if it has one, else the world's (the Hatchery's "All creatures fertile").
bool fertileBySetting(const class oldCreature* c);

// A life stage's share of full fertility (0 baby ... 6 senile).
float stageFertility(int stage);
