/*
 *  GenomeTraits.h
 *  openc2e
 *
 *  C2 under a mind: part of each creature's temperament read from its own
 *  C2 genes (owner, 2026-11-20: genes shape the mind; he prefers the game
 *  working genomes out over translations written per genome). For each
 *  drive the genome's chemistry is compared with its species' standard
 *  genome (norn.gen, gren.gen, ettn.gen): how much its stimulus genes raise
 *  the drive (chemical 18 + drive), how much its emitters make the drive's
 *  chemicals (148 + drive, 18 + drive), how long the drive's chemical
 *  lingers (its half-life gene: 8 steps double it) and how strongly its
 *  receptors show the chemical as the drive (organ 1, tissue 5). The log
 *  ratios add up to the drive's "gain"; a trait moves by k times the gains
 *  of its drives:
 *    boldness     - fear
 *    curiosity    + boredom
 *    energy       - tiredness and sleepiness (their average)
 *    sociability  + loneliness, - half of crowdedness
 *    temper       + anger
 *    reactivity   + stress
 *    steadiness   (no drive: inherited and chance only)
 *  The stock eggs share their species' genome, so they read zero; bred,
 *  mutated, custom and edited genomes differ.
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

#include <array>
#include <string>

class genomeFile;

namespace GenomeTraits {

// the shift of each of the seven traits (Temperament.h order) this genome's
// chemistry gives, against its species' standard genome; zeros when there
// is no standard genome to compare with (or OPENC2E_NO_GENOME_TRAITS)
std::array<float, 7> shift(const genomeFile& g);

// each drive's gain against the standard genome (log ratio), for reports
std::array<float, 17> driveGains(const genomeFile& g, std::string* species = nullptr);

// developer aid: OPENC2E_DUMP_GENE_TRAITS=1 prints every genome in the
// Genetics folders with its gains and shifts, and a summary
void dumpIfAsked();

} // namespace GenomeTraits
