/*
 *  CreatureSpeech.cpp
 *  openc2e
 *
 *  C1/C2 creatures speaking under a mind. See CreatureSpeech.h.
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

#include "CreatureSpeech.h"

#include "Agent.h"
#include "Bubble.h"
#include "Engine.h"
#include "World.h"
#include "common/Random.h"

#include <cstdlib>
#include <fmt/core.h>
#include <map>
#include <vector>

namespace {

// a word or two for each C2 drive (0 Pain ... 16 Stress)
const char* driveWord(int d) {
	static const char* words[17] = {"hurt", "want fun", "hungry", "cold", "hot", "tired", "sleepy", "lonely", "crowded",
		"scared", "bored", "angry", "want love", "hurt", "need air", "thirsty", "worried"};
	return (d >= 0 && d < 17) ? words[d] : "want";
}

const char* genusWord(int g) {
	switch (g) {
		case 1: return "norn";
		case 2: return "grendel";
		case 3: return "ettin";
		default: return "friend";
	}
}

std::string pick(std::initializer_list<std::string> options) {
	std::vector<std::string> v(options);
	return v[rand_size_t(0, v.size() - 1)];
}

// meaning "kind:a:b" split
std::vector<std::string> parts(const std::string& m) {
	std::vector<std::string> out;
	size_t start = 0;
	for (;;) {
		size_t c = m.find(':', start);
		out.push_back(m.substr(start, c == std::string::npos ? std::string::npos : c - start));
		if (c == std::string::npos)
			break;
		start = c + 1;
	}
	return out;
}

std::map<int, AgentRef> bubbles; // each speaker's bubble, by UNID

} // namespace

std::string phraseFor(const std::string& meaning, int stage) {
	std::vector<std::string> p = parts(meaning);
	const std::string& kind = p[0];
	int a = p.size() > 1 ? atoi(p[1].c_str()) : 0;
	int b = p.size() > 2 ? atoi(p[2].c_str()) : 0;

	// babies: sounds, not words
	if (stage <= 0) {
		if (kind == "ow")
			return pick({"waah!", "wah!", "eee!"});
		if (kind == "yum")
			return pick({"mmm", "nom nom"});
		if (kind == "grr")
			return "nnngh!";
		if (kind == "greet")
			return pick({"ba!", "da!", "ooh"});
		if (kind == "need")
			return pick({"waah", "mama", "eh eh"});
		return pick({"ba ba", "goo", "da da", "ma ma", "ga"});
	}
	// children: single words
	if (stage == 1) {
		if (kind == "ow")
			return pick({"ow!", "owie!"});
		if (kind == "yum")
			return "yum!";
		if (kind == "grr")
			return "grr!";
		if (kind == "love")
			return "hee";
		if (kind == "greet")
			return pick({"hi!", fmt::format("{}!", genusWord(a))});
		if (kind == "need")
			return std::string(driveWord(a)) + (b > 200 ? "!" : "");
		return pick({"la la", "hm", "ooh"});
	}
	std::string trail = stage >= 6 ? "..." : "";
	if (kind == "ow")
		return pick({"ow!", "ouch!", "ow ow!"});
	if (kind == "yum")
		return pick({"yum!", "food good", "mmm, good"});
	if (kind == "grr")
		return pick({"grr!", "me angry", "go away!"});
	if (kind == "love")
		return pick({"mm, love", "love you", "nice"});
	if (kind == "greet")
		return fmt::format("hi {}{}", genusWord(a), trail);
	if (kind == "need") {
		std::string w = driveWord(a);
		bool want = w.rfind("want", 0) == 0 || w.rfind("need", 0) == 0;
		std::string very = (b > 200 && !want) ? "very " : "";
		return fmt::format("me {}{}{}", very, w, trail.empty() ? (b > 200 ? "!" : "") : trail);
	}
	return pick({"hm", "la la", "ah"}) + trail;
}

float voicePitch(int stage) {
	static const float pitch[7] = {1.4f, 1.25f, 1.1f, 1.05f, 1.0f, 0.92f, 0.88f};
	return (stage >= 0 && stage < 7) ? pitch[stage] : 1.0f;
}

float voicePace(int stage) {
	static const float pace[7] = {0.8f, 1.0f, 1.1f, 1.15f, 1.0f, 0.85f, 0.7f};
	return (stage >= 0 && stage < 7) ? pace[stage] : 1.0f;
}

void creatureSays(Agent* creature, int stage, const std::string& phrase) {
	if (!creature || phrase.empty() || engine.version >= 3)
		return;
	// one bubble at a time for each speaker
	auto it = bubbles.find(creature->getUNID());
	if (it != bubbles.end() && it->second)
		it->second->kill();
	Bubble* b = Bubble::newBubble(creature, true, phrase);
	b->setTimeout(35); // 3.5 s
	bubbles[creature->getUNID()] = AgentRef(b);
	try {
		creature->speak(phrase, voicePitch(stage), voicePace(stage));
	} catch (std::exception&) {
		// no voice for this creature: the bubble alone
	}
	static const bool watch = getenv("OPENC2E_WATCH_SPEECH") != nullptr;
	if (watch)
		fmt::print("* SAYS {} {} {} (stage {}): \"{}\"\n", (int)creature->family, (int)creature->genus, creature->getUNID(), stage, phrase);
}
