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

// A life stage's share of full fertility (0 baby ... 6 senile).
float stageFertility(int stage);
