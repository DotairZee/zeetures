/*
 *  CreatureMind.cpp
 *  openc2e
 *
 *  Mind registry and the "null" mind.
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

#include "CreatureMind.h"

#include <fmt/core.h>

namespace {

// Logs one line summarizing a perception.
void logPerception(const char* what, const MindPerception& p) {
	std::string drives;
	for (size_t i = 0; i < p.drives.size(); i++) {
		if (p.drives[i] > 0)
			drives += fmt::format(" {}={}", i, (int)p.drives[i]);
	}
	std::string seen;
	for (const auto& s : p.visible) {
		seen += fmt::format(" {}@{:.0f}", s.category, s.distance);
	}
	std::string stims;
	for (const auto& s : p.stimuli) {
		stims += fmt::format(" {}{}", s.id, s.source ? "" : "(no source)");
	}
	fmt::print("[mind] creature {} {}: brain tick {} stage {} {}{} | drives:{} | sees {}:{} | stimuli {}:{} | action {} attn {} decn {}\n",
		p.creatureId, what, p.tick, p.stage,
		p.female ? "female" : "male",
		p.asleep ? (p.dreaming ? " dreaming" : " asleep") : "",
		drives.empty() ? " none" : drives,
		p.visible.size(), seen,
		p.stimuli.size(), stims,
		p.actionFinished ? "finished" : "running",
		p.currentAttention, p.currentDecision);
}

// Milestone 1: proves the plumbing. Receives perception, logs it, and never
// changes attention or decision, so creatures stand still.
class NullMind : public CreatureMind {
  public:
	std::string name() const override { return "null"; }

	void born(const MindPerception& p) override {
		logPerception("born", p);
	}

	MindOutput tick(const MindPerception& p) override {
		ticks++;
		// Log every 50th tick, and any tick with stimuli so they are easy to test.
		if (ticks % 50 == 1 || !p.stimuli.empty())
			logPerception("tick", p);
		return {};
	}

	void died(const MindPerception& p) override {
		logPerception("died", p);
	}

	std::string debugState() const override {
		return fmt::format("null mind, {} ticks", ticks);
	}

  private:
	unsigned int ticks = 0;
};

} // namespace

std::unique_ptr<CreatureMind> createCreatureMind(const std::string& name) {
	if (name == "null")
		return std::make_unique<NullMind>();
	return nullptr;
}

std::string availableCreatureMinds() {
	return "null";
}
