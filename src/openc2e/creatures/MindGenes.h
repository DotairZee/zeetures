/*
 *  MindGenes.h
 *  openc2e
 *
 *  C2 under a mind: the mind's own heritable settings (owner, 2026-11-20:
 *  genes shape the mind). Beside temperament (Temperament.h), each
 *  creature has eight more z-scores (0 average, clipped to +-2), passed on
 *  like temperament (0.4 of the parents' average, the rest chance) in a
 *  small file beside its genome (Genetics/NAME.mind); the game's own eggs
 *  and older genomes draw them at random.
 *
 *  Four settings:
 *    learning      how fast it learns what things do
 *    decisiveness  how surely it picks its best choice (softmax temperature)
 *    patience      how far it will walk for something (trips' worth by distance)
 *    wanderlust    how much it explores
 *  and four readinesses to learn (the Baldwin effect, not Lamarck: nothing
 *  learned is passed on, only how readily a kind of thing is learned; a line
 *  that learns, say, water's danger sooner drowns less and breeds more, so
 *  over generations the population can drift; Baldwin 1896, Hinton and
 *  Nowlan 1987):
 *    danger   outcomes of pain, injury and fear
 *    food     hunger and thirst
 *    water    suffocation
 *    company  loneliness, crowding and anger
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

enum MindGene { MG_LEARNING, MG_DECISIVENESS, MG_PATIENCE, MG_WANDERLUST, MG_DANGER, MG_FOOD, MG_WATER, MG_COMPANY, NUM_MIND_GENES };

struct MindGenes {
	std::array<float, NUM_MIND_GENES> z{};
};

namespace MindGeneSet {

const char* name(int g); // "learning"
const char* lowWord(int g); // "slow to learn"
const char* highWord(int g); // "quick to learn"

MindGenes roll();
MindGenes child(const MindGenes& mum, const MindGenes& dad);

// the file beside a genome (Genetics/NAME.mind), as Temperaments::load
bool load(const std::string& genomeName, MindGenes& m);
void store(const std::string& genomeName, const MindGenes& m);

std::string toLine(const MindGenes& m);
bool fromLine(const std::string& line, MindGenes& m);

} // namespace MindGeneSet
