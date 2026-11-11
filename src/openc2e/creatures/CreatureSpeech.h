/*
 *  CreatureSpeech.h
 *  openc2e
 *
 *  C1/C2 creatures speaking under a mind: what a creature of a given age says
 *  for a meaning the mind (or SAYN) chooses, in a speech bubble and its voice.
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

#include <string>

class Agent;

// Meanings (our own small vocabulary of intents, not C2's language):
//   "need:D:L"   wants drive D relieved (C2 drive number), at level L 0-255
//   "ow"         hurt just now
//   "yum"        a good meal
//   "grr"        angry
//   "love"       a kiss that pleased
//   "greet:G"    a creature of genus G (1 norn, 2 grendel, 3 ettin) came into view
//   "babble"     nothing in particular
// The words depend on the life stage: babies babble, children say single
// words, the grown say short phrases, the senile trail off.
std::string phraseFor(const std::string& meaning, int stage);

// A life stage's voice: pitch (playback rate, 1 = as recorded) and pace
// (syllables per usual time, 1 = as the voice file says).
float voicePitch(int stage);
float voicePace(int stage);

// The creature says the phrase: a speech bubble (the game's own, from the
// player's installed copy) and its voice, pitched and paced for its age.
void creatureSays(Agent* creature, int stage, const std::string& phrase);
