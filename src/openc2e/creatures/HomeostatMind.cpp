/*
 *  HomeostatMind.cpp
 *  openc2e
 *
 *  Mind model v0 ("homeostat"): an original creature mind built from
 *  published research on motivation and learning. See mind-model-v0.md in
 *  the project notes for the design and references.
 *
 *  1. Drive and reward: overall drive D combines the body's 17 drives
 *     nonlinearly (severe needs dominate); an action's reward is the drop
 *     in D it produced (homeostatic reinforcement learning).
 *  2. Expectations: per (object category, action), the expected change in
 *     each drive, learned with the delta rule after every action.
 *  3. Wanting and choice: predicted drop in D given current needs, plus a
 *     curiosity bonus, minus a distance cost; softmax choice whose
 *     temperature falls as arousal rises; a commitment bonus for the
 *     current choice.
 *  4. Habituation: categories whose interactions change nothing lose
 *     attention, recovering over time.
 *  5. Affect: valence (running average of reward) and arousal (D) shape
 *     exploration.
 *  6. Instincts: weak innate expectations, overwritten by experience.
 *  7. Facial expression: chosen from affect (Anger, distress, recent mood,
 *     longer-term valence, overall drive).
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

#include <algorithm>
#include <array>
#include <cmath>
#include <fmt/core.h>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr int NUM_DRIVES = 17;

// Labels for log lines only (the order of drives in the owner's
// allchemicals.str, chemicals 1-17).
const char* const DRIVE_NAMES[NUM_DRIVES] = {
	"Pain", "Pleasure", "Hunger", "Cold", "Hot", "Tired", "Sleepy", "Lonely",
	"Crowded", "Fear", "Bored", "Anger", "Sex", "Injury", "Suffocate", "Thirst", "Stress"};
enum Drive { PAIN = 0,
	PLEASURE,
	HUNGER,
	COLD,
	HOT,
	TIRED,
	SLEEPY,
	LONELY,
	CROWDED,
	FEAR,
	BORED,
	ANGER,
	SEX,
	INJURY,
	SUFFOCATE,
	THIRST,
	STRESS };

// Action labels for log lines only, as read from the owner's world scripts.
std::string actionName(int decision) {
	switch (decision) {
		case 0: return "idle";
		case 1: return "activate1";
		case 2: return "activate2";
		case 3: return "deactivate";
		case 4: return "approach";
		case 6: return "pickup";
		case 7: return "drop";
		case 8: return "express";
		case 10: return "travel-a";
		case 11: return "travel-b";
		case 12: return "eat";
		case 13: return "hit";
		case 25: return "sleep";
		case 26: return "walk-a";
		case 27: return "walk-b";
		default: return fmt::format("action{}", decision);
	}
}

// Every tunable number in one place, so they can become genes later.
struct Params {
	float driveExpN = 3.0f; // D = (sum w_i d_i^n)^(1/m), n > m
	float driveExpM = 2.0f;
	std::array<float, NUM_DRIVES> weights;
	float learningRate = 0.3f; // delta rule
	float curiosity = 0.02f; // novelty bonus scale
	float boredomCuriosityGain = 2.0f; // Boredom raises curiosity
	float temperature = 0.015f; // softmax base temperature
	float arousalFocus = 2.0f; // high D lowers temperature
	float commitment = 0.01f; // bonus for repeating the current choice
	float distanceCost = 0.00002f; // per pixel to the object
	int maxEpisodeTicks = 12; // looping actions are evaluated this often
	float habituationRate = 0.7f; // multiplier after an uneventful interaction
	float habituationRecovery = 0.01f; // per tick, back toward 1
	float eventfulThreshold = 0.02f; // total |drive change| that counts as eventful
	float valenceRate = 0.1f; // running average of reward
	float driftRate = 0.1f; // running average of passive drive change per tick
	size_t choiceSetSize = 5; // softmax over the top few candidates only

	// 7. Facial expression from affect. Slots refer to the head sprites'
	// expression rows (in the owner's norn heads: 0 neutral, 1 happy,
	// 2 sad, 3 angry); other breeds may differ, so these belong in a breed file.
	int faceNeutral = 0, faceHappy = 1, faceSad = 2, faceAngry = 3;
	float angryAt = 0.5f; // Anger drive level (0-1)
	float distressAt = 0.6f; // max of Pain, Fear, Lonely (0-1)
	float moodRate = 0.5f; // how fast the recent mood follows each reward
	float moodDecay = 0.95f; // per tick, recent mood fades toward neutral
	float happyMood = 0.04f, sadMood = -0.05f; // recent-mood thresholds
	float sadValence = -0.03f; // longer-term valence threshold
	float contentValence = 0.01f, contentDrive = 0.3f; // calm and content

	Params() {
		weights.fill(1.0f);
		weights[SEX] = 0.3f;
	}
};

// Weak innate expectations, keyed by decision number (instincts).
// Values are expected drive changes on the 0-1 scale.
std::array<float, NUM_DRIVES> instinctFor(int decision) {
	std::array<float, NUM_DRIVES> e{};
	switch (decision) {
		case 12: // eat
			e[HUNGER] = -0.05f;
			break;
		case 25: // sleep
			e[TIRED] = -0.05f;
			e[SLEEPY] = -0.05f;
			break;
		default:
			break;
	}
	return e;
}

struct Expectation {
	std::array<float, NUM_DRIVES> delta{}; // expected change in each drive
	int count = 0; // times experienced
};

struct Candidate {
	int category; // -1 for actions that need no object
	int decision;
	float distance;
	float want; // predicted drop in D
	float novelty;
	float score;
};

class HomeostatMind : public CreatureMind {
  public:
	HomeostatMind() {
		habituation.fill(1.0f);
	}

	std::string name() const override { return "homeostat"; }

	void born(const MindPerception& p) override {
		rng.seed((unsigned int)p.creatureId);
		std::string acts;
		for (auto& a : *p.actions)
			acts += fmt::format(" {}{}", actionName(a.decision), a.needsObject ? "*" : "");
		fmt::print("[homeostat] creature {} born ({} {}); actions (* = on an object):{}\n",
			p.creatureId, p.stage, p.female ? "female" : "male", acts);
	}

	void died(const MindPerception& p) override {
		fmt::print("[homeostat] creature {} died at brain tick {}; {}\n", p.creatureId, p.tick, debugState());
	}

	MindOutput tick(const MindPerception& p) override {
		MindOutput out;
		std::array<float, NUM_DRIVES> d = normalize(p.drives);
		float D = overallDrive(d);

		for (auto& h : habituation)
			h += prm.habituationRecovery * (1.0f - h);

		// 7. facial expression from current affect; only sent when it changes
		mood *= prm.moodDecay;
		int face = chooseFace(d, D);
		if (face != lastFace) {
			out.expression = face;
			fmt::print("[homeostat] creature {} t{}: face {} -> {} (mood {:+.3f}, valence {:+.3f}, Anger {:.2f})\n",
				p.creatureId, p.tick, lastFace, face, mood, valence, d[ANGER]);
			lastFace = face;
		}

		for (auto& s : p.stimuli) {
			stimuliSeen++;
			if (s.numbered)
				fmt::print("[homeostat] creature {} stimulus {} #{}\n", p.creatureId, s.command, s.id);
			else
				fmt::print("[homeostat] creature {} stimulus {} chems {}:{} {}:{} {}:{} {}:{}\n", p.creatureId, s.command,
					s.chem[0], s.amount[0], s.chem[1], s.amount[1], s.chem[2], s.amount[2], s.chem[3], s.amount[3]);
		}

		if (!p.actions || p.actions->empty())
			return out;

		// Is the current episode over?
		if (inEpisode) {
			episodeTicks++;
			bool objectGone = current.category >= 0 && !isVisible(p, current.category);
			bool finished = p.actionFinished && episodeTicks > 1;
			if (finished || objectGone || episodeTicks >= prm.maxEpisodeTicks) {
				endEpisode(p, d, D, finished ? "finished" : objectGone ? "object gone" : "time");
			} else {
				return out; // let the current action run
			}
		}

		// Choose the next action.
		std::vector<Candidate> candidates = evaluate(p, d, D);
		if (candidates.empty())
			return out;
		Candidate choice = pick(candidates, D);
		logChoice(p, choice, candidates, D);

		current = choice;
		inEpisode = true;
		episodeTicks = 0;
		startDrives = d;
		startD = D;

		if (choice.category >= 0)
			out.attentionCategory = choice.category;
		out.decision = choice.decision;
		return out;
	}

	// World saving: one fact per line, readable and hand-editable.
	std::string saveState() const override {
		std::string s = fmt::format("valence {}\nmood {}\nepisodes {}\nstimuli {}\n", valence, mood, episodes, stimuliSeen);
		s += "drift";
		for (float f : drift)
			s += fmt::format(" {}", f);
		s += "\nhabituation";
		for (float f : habituation)
			s += fmt::format(" {}", f);
		s += "\n";
		for (auto& [key, e] : expectations) {
			// key = (category + 1) * 64 + decision
			s += fmt::format("expect {} {} {}", key / 64 - 1, key % 64, e.count);
			for (float f : e.delta)
				s += fmt::format(" {}", f);
			s += "\n";
		}
		return s;
	}

	void loadState(const std::string& state) override {
		std::istringstream in(state);
		std::string line;
		while (std::getline(in, line)) {
			std::istringstream l(line);
			std::string what;
			l >> what;
			if (what == "valence")
				l >> valence;
			else if (what == "mood")
				l >> mood;
			else if (what == "episodes")
				l >> episodes;
			else if (what == "stimuli")
				l >> stimuliSeen;
			else if (what == "drift")
				for (float& f : drift)
					l >> f;
			else if (what == "habituation")
				for (float& f : habituation)
					l >> f;
			else if (what == "expect") {
				int category, decision;
				Expectation e;
				l >> category >> decision >> e.count;
				for (float& f : e.delta)
					l >> f;
				if (l)
					expectations[(category + 1) * 64 + decision] = e;
			}
		}
	}

	std::string debugState() const override {
		return fmt::format("homeostat: {} episodes, valence {:+.3f}, {} stimuli, {} expectations",
			episodes, valence, stimuliSeen, expectations.size());
	}

  private:
	Params prm;
	std::map<int, Expectation> expectations; // key: (category + 1) * 64 + decision
	std::array<float, 40> habituation;
	float valence = 0.0f;
	std::mt19937 rng;

	bool inEpisode = false;
	Candidate current{};
	int episodeTicks = 0;
	std::array<float, NUM_DRIVES> startDrives{};
	float startD = 0.0f;
	unsigned int episodes = 0, stimuliSeen = 0;
	std::array<float, NUM_DRIVES> drift{}; // baseline drive change per tick
	float mood = 0.0f; // short-term affect: fast-following, decaying average of reward
	int lastFace = -1;

	int chooseFace(const std::array<float, NUM_DRIVES>& d, float D) const {
		float distress = std::max({d[PAIN], d[FEAR], d[LONELY]});
		if (d[ANGER] >= prm.angryAt)
			return prm.faceAngry;
		if (distress >= prm.distressAt || mood <= prm.sadMood || valence <= prm.sadValence)
			return prm.faceSad;
		if (mood >= prm.happyMood || (valence >= prm.contentValence && D <= prm.contentDrive))
			return prm.faceHappy;
		return prm.faceNeutral;
	}

	static std::array<float, NUM_DRIVES> normalize(const std::array<unsigned char, 17>& raw) {
		std::array<float, NUM_DRIVES> d{};
		for (int i = 0; i < NUM_DRIVES; i++)
			d[i] = raw[i] / 255.0f;
		return d;
	}

	// 1. Overall drive: severe needs dominate several mild ones.
	float overallDrive(const std::array<float, NUM_DRIVES>& d) const {
		float sum = 0.0f;
		for (int i = 0; i < NUM_DRIVES; i++)
			sum += prm.weights[i] * std::pow(std::clamp(d[i], 0.0f, 1.0f), prm.driveExpN);
		return std::pow(sum, 1.0f / prm.driveExpM);
	}

	static bool isVisible(const MindPerception& p, int category) {
		for (auto& s : p.visible)
			if (s.category == category)
				return true;
		return false;
	}

	Expectation& expectation(int category, int decision) {
		int key = (category + 1) * 64 + decision;
		auto it = expectations.find(key);
		if (it == expectations.end()) {
			// A new category starts from what this action has done on other
			// categories so far; the instinct is the fallback before any
			// experience with the action (6. instincts as starting points).
			Expectation e;
			e.delta = instinctFor(decision);
			int total = 0;
			std::array<float, NUM_DRIVES> sum{};
			for (auto& [k, other] : expectations) {
				if (k % 64 == decision && other.count > 0) {
					for (int i = 0; i < NUM_DRIVES; i++)
						sum[i] += other.delta[i] * other.count;
					total += other.count;
				}
			}
			if (total > 0) {
				for (int i = 0; i < NUM_DRIVES; i++)
					e.delta[i] = sum[i] / total;
			}
			it = expectations.emplace(key, e).first;
		}
		return it->second;
	}

	// 3. Wanting: predicted drop in D, given current needs.
	float predictedRelief(const std::array<float, NUM_DRIVES>& d, float D, const Expectation& e) const {
		std::array<float, NUM_DRIVES> after{};
		for (int i = 0; i < NUM_DRIVES; i++)
			after[i] = std::clamp(d[i] + e.delta[i], 0.0f, 1.0f);
		return D - overallDrive(after);
	}

	std::vector<Candidate> evaluate(const MindPerception& p, const std::array<float, NUM_DRIVES>& d, float D) {
		std::vector<Candidate> out;
		// 5. Affect: negative valence dampens curiosity; Boredom raises it.
		float curiosity = prm.curiosity * (1.0f + prm.boredomCuriosityGain * d[BORED]) * std::clamp(1.0f + 10.0f * valence, 0.2f, 1.5f);
		auto add = [&](int category, int decision, float distance) {
			Expectation& e = expectation(category, decision);
			Candidate c{category, decision, distance, 0, 0, 0};
			c.want = predictedRelief(d, D, e);
			c.novelty = curiosity / std::sqrt(1.0f + e.count);
			c.score = c.want + c.novelty - prm.distanceCost * distance;
			if (category >= 0 && category < (int)habituation.size()) {
				// 4. habituation scales the pull of an object
				float pull = c.want + c.novelty;
				c.score = pull * habituation[category] - prm.distanceCost * distance;
			}
			if (inEpisodeJustEnded && category == current.category && decision == current.decision)
				c.score += prm.commitment;
			out.push_back(c);
		};
		for (auto& a : *p.actions) {
			if (a.needsObject) {
				for (auto& s : p.visible)
					add(s.category, a.decision, s.distance);
			} else {
				add(-1, a.decision, 0.0f);
			}
		}
		inEpisodeJustEnded = false;
		return out;
	}
	bool inEpisodeJustEnded = false;

	// Softmax choice; temperature falls as arousal (D) rises.
	// Softmax over the top few candidates (an attention bottleneck, so many
	// mediocre options cannot outweigh a clearly better one together).
	Candidate pick(std::vector<Candidate> all, float D) {
		// What to do, then what to do it to: keep only each action's best
		// object, so one action on many objects cannot crowd out the rest.
		std::map<int, Candidate> bestPerAction;
		for (auto& c : all) {
			auto it = bestPerAction.find(c.decision);
			if (it == bestPerAction.end() || c.score > it->second.score)
				bestPerAction[c.decision] = c;
		}
		std::vector<Candidate> cands;
		for (auto& [decision, c] : bestPerAction)
			cands.push_back(c);
		std::sort(cands.begin(), cands.end(), [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
		if (cands.size() > prm.choiceSetSize)
			cands.resize(prm.choiceSetSize);
		float tau = prm.temperature / (1.0f + prm.arousalFocus * D);
		std::vector<double> w;
		for (auto& c : cands)
			w.push_back(std::exp((c.score - cands[0].score) / tau));
		std::discrete_distribution<size_t> dist(w.begin(), w.end());
		return cands[dist(rng)];
	}

	// 1 and 2: reward and learning at the end of an episode.
	void endEpisode(const MindPerception& p, const std::array<float, NUM_DRIVES>& d, float D, const char* why) {
		inEpisode = false;
		inEpisodeJustEnded = true;
		episodes++;
		std::array<float, NUM_DRIVES> change{};
		float total = 0.0f;
		for (int i = 0; i < NUM_DRIVES; i++) {
			change[i] = d[i] - startDrives[i];
			total += std::fabs(change[i]);
		}
		float reward = startD - D;

		// Learn what the action did beyond the passage of time: subtract the
		// baseline drift expected over this many ticks, then update the
		// baseline itself.
		int ticks = std::max(episodeTicks, 1);
		std::array<float, NUM_DRIVES> effect{};
		for (int i = 0; i < NUM_DRIVES; i++) {
			effect[i] = change[i] - drift[i] * ticks;
			drift[i] += prm.driftRate * (change[i] / ticks - drift[i]);
		}

		Expectation& e = expectation(current.category, current.decision);
		for (int i = 0; i < NUM_DRIVES; i++)
			e.delta[i] += prm.learningRate * (effect[i] - e.delta[i]);
		e.count++;

		// Mood follows how much better or worse this went than just letting
		// time pass (reward relative to the drift baseline).
		std::array<float, NUM_DRIVES> passive{};
		for (int i = 0; i < NUM_DRIVES; i++)
			passive[i] = std::clamp(startDrives[i] + (change[i] - effect[i]), 0.0f, 1.0f);
		float advantage = overallDrive(passive) - D;
		mood += prm.moodRate * (advantage - mood);

		valence += prm.valenceRate * (reward - valence);

		if (current.category >= 0 && current.category < (int)habituation.size()) {
			if (total < prm.eventfulThreshold)
				habituation[current.category] *= prm.habituationRate;
			else
				habituation[current.category] = 1.0f; // dishabituation
		}

		// the three largest drive changes, for the log
		std::array<int, NUM_DRIVES> idx;
		for (int i = 0; i < NUM_DRIVES; i++)
			idx[i] = i;
		std::sort(idx.begin(), idx.end(), [&](int a, int b) { return std::fabs(change[a]) > std::fabs(change[b]); });
		std::string changes;
		for (int k = 0; k < 3; k++) {
			if (std::fabs(change[idx[k]]) < 0.005f)
				break;
			changes += fmt::format(" {} {:+.2f}", DRIVE_NAMES[idx[k]], change[idx[k]]);
		}
		std::sort(idx.begin(), idx.end(), [&](int a, int b) { return std::fabs(effect[a]) > std::fabs(effect[b]); });
		std::string effects;
		for (int k = 0; k < 3; k++) {
			if (std::fabs(effect[idx[k]]) < 0.005f)
				break;
			effects += fmt::format(" {} {:+.2f}", DRIVE_NAMES[idx[k]], effect[idx[k]]);
		}
		fmt::print("[homeostat] creature {} t{}: {}@{} ended ({}, {} ticks): reward {:+.3f}{} | beyond drift:{} | D {:.2f} valence {:+.3f}\n",
			p.creatureId, p.tick, actionName(current.decision), current.category, why, episodeTicks, reward,
			changes.empty() ? " (no drive change)" : changes, effects.empty() ? " nothing" : effects, D, valence);
	}

	void logChoice(const MindPerception& p, const Candidate& choice, std::vector<Candidate> cands, float D) {
		std::sort(cands.begin(), cands.end(), [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
		std::string top;
		for (size_t i = 0; i < cands.size() && i < 3; i++)
			top += fmt::format(" {}@{} {:+.3f}", actionName(cands[i].decision), cands[i].category, cands[i].score);
		std::string needs;
		for (int i = 0; i < NUM_DRIVES; i++)
			if (p.drives[i] >= 64)
				needs += fmt::format(" {} {}", DRIVE_NAMES[i], (int)p.drives[i]);
		fmt::print("[homeostat] creature {} t{}: chose {}@{} (want {:+.3f} novelty {:+.3f}) | top:{} | needs:{} | D {:.2f}\n",
			p.creatureId, p.tick, actionName(choice.decision), choice.category, choice.want, choice.novelty,
			top, needs.empty() ? " none high" : needs, D);
	}
};

} // namespace

std::unique_ptr<CreatureMind> createHomeostatMind() {
	return std::make_unique<HomeostatMind>();
}
