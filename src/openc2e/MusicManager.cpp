/*
 *  MusicManager.cpp
 *  openc2e
 *
 *  Created by Alyssa Milburn on Fri Oct 31 2008.
 *  Copyright (c) 2008 Alyssa Milburn. All rights reserved.
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

#include "MusicManager.h"

#include "Engine.h"
#include "PathResolver.h"
#include "SoundManager.h"
#include "World.h"
#include "caosValue.h"
#include "common/devenv.h"
#include "common/Random.h"
#include "common/audio/AudioBackend.h"
#include "libmngmusic/MNGMusic.h"

#include <fmt/core.h>

// this is all for MusicManager::tick
#include "Camera.h"
#include "Map.h"
#include "MetaRoom.h"
#include "Room.h"
#include "World.h"
#include "creatures/CreatureAgent.h"
#include "creatures/CreatureMind.h"
#include "creatures/oldCreature.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>

MusicManager::MusicManager()
	: creatures1_ticks_until_next_sound(20), mng_music(std::make_unique<MNGMusic>()) {
}

MusicManager::~MusicManager() {
	stop();
	for (auto& file : files) {
		delete file.second;
	}
}

void MusicManager::stop() {
	mng_music->stop();
	get_audio_backend()->midi_stop();
}

float MusicManager::getVolume() {
	return music_volume;
}

void MusicManager::setVolume(float volume) {
	music_volume = volume;
	updateVolumes();
}

float MusicManager::getMIDIVolume() {
	return midi_volume;
}

void MusicManager::midi_set_volume(float volume) {
	midi_volume = volume;
	updateVolumes();
}

bool MusicManager::isMuted() {
	return music_muted;
}

void MusicManager::setMuted(bool muted) {
	music_muted = muted;
	updateVolumes();
}

bool MusicManager::isMIDIMuted() {
	auto it = world.variables.find("engine_mute");
	return it != world.variables.end() && it->second.hasInt() && it->second.getInt() != 0;
}

void MusicManager::setMIDIMuted(bool muted) {
	world.variables["engine_mute"] = muted ? 1 : 0;
	updateVolumes();
}

void MusicManager::updateVolumes() {
	mng_music->setVolume(music_muted ? 0 : music_volume);
	get_audio_backend()->midi_set_volume(isMIDIMuted() ? 0 : midi_volume);
	get_audio_backend()->audio_channel_set_volume(creatures1_channel, music_muted ? 0 : music_volume * 0.8);
}

void MusicManager::playTrack(std::string track) {
	return playTrackForAtLeastThisManyMilliseconds(track, 0);
}

void MusicManager::playTrackForAtLeastThisManyMilliseconds(std::string track, unsigned int _how_long_before_changing_track_ms) {
	if (track == last_track) {
		return;
	}
	last_track = track;
	if (devenv("OPENC2E_DEBUG_MUSIC"))
		fmt::print("* Music: now {}\n", track.empty() ? "(none)" : track);

	auto game_usemidimusicsystem = world.variables["engine_usemidimusicsystem"];
	bool usemidimusicsystem = game_usemidimusicsystem.hasInt() && game_usemidimusicsystem.getInt() != 0;
	// TODO: what happens if you call the CAOS command MIDI and usemidimusicsystem isn't enabled?

	if (usemidimusicsystem) {
		mng_music->playSilence(); // or just stop it?

		if (track == "") {
			get_audio_backend()->midi_stop();
			return;
		}

		auto filename = findSoundFile(track + ".mid");
		if (filename.empty()) {
			fmt::print("Couldn't find MIDI file '{}'!\n", track);
			return;
		}
		get_audio_backend()->play_midi_file(filename);
		how_long_before_changing_track_ms = _how_long_before_changing_track_ms;
		return;
	}

	if (track == "Silence") {
		mng_music->playSilence();
	} else {
		std::string filename, trackname;

		std::string::size_type n = track.find("\\");
		if (n == std::string::npos) {
			filename = "Music.mng"; // TODO
			trackname = track;
		} else {
			filename = track.substr(0, n);
			trackname = track.substr(n + 1, std::string::npos);
		}

		MNGFile* file;
		std::transform(filename.begin(), filename.end(), filename.begin(), (int (*)(int))tolower);
		if (files.find(filename) == files.end()) {
			auto realfilename = findSoundFile(filename);
			if (realfilename.empty()) {
				fmt::print("Couldn't find MNG file '{}'!\n", filename);
				return; // TODO: exception?
			}

			file = new MNGFile(realfilename);
			files[filename] = file;
		} else {
			file = files[filename];
		}
		mng_music->playTrack(file, trackname);
	}
	how_long_before_changing_track_ms = _how_long_before_changing_track_ms;
}

void MusicManager::tick() {
	// play C1 music
	// TODO: this should be linked to 'real' time, so it doesn't go crazy when game speed is modified
	// TODO: this should probably be in a separate C1MusicManager class
	if (engine.version == 1) {
		if (creatures1_ticks_until_next_sound == 0 && get_audio_backend()->audio_channel_get_state(creatures1_channel) == AUDIO_STOPPED) {
			auto sounds = findSoundFiles("MU*.wav");
			if (sounds.size()) {
				creatures1_channel = get_audio_backend()->play_clip(rand_choice(sounds));
			}
			creatures1_ticks_until_next_sound = rand_uint32(50, 99);
		} else {
			creatures1_ticks_until_next_sound--;
		}
	}

	// play MNG/MIDI music
	// TODO: how does engine_near_death_track_name work?
	if (how_long_before_changing_track_ms > 0) {
		how_long_before_changing_track_ms -= std::min(how_long_before_changing_track_ms, world.ticktime);
	}
	if (engine.version == 2) {
		tickC2();
	} else if (how_long_before_changing_track_ms == 0) {
		// TODO: this behaviour is different in C2, and should probably be cleverer
		MetaRoom* m = engine.camera->getMetaRoom();
		if (m) {
			std::shared_ptr<Room> r = m->roomAt(engine.camera->getXCentre(), engine.camera->getYCentre());
			if (r && r->music.size()) {
				playTrack(r->music);
			} else if (m->music.size()) {
				playTrack(m->music);
			}
		}
	}
	mng_music->update();

	// update volumes based on new volumes, muting, etc
	updateVolumes();
}

// C2's dynamic music. The creatures in view steer it: how content they are
// sets the score's Mood, the danger they are in its Threat (Music.mng's
// tracks use both), a hatching or a death in view plays the Birth or Death
// piece, and a creature close to death the NearDeath one. Otherwise the
// music is the room's: the selected creature's room if it is in view, else
// the room in the middle of the view. With no creature in view, silence.
namespace {

struct LiveCreature {
	Agent* agent;
	Creature* creature;
	float x, y; // its middle
	bool grendel;
};

// a grendel this close (across; and not too far above or below) makes a norn
// or ettin's danger rise as it nears
constexpr float GRENDEL_NEAR = 500, GRENDEL_NEAR_Y = 200;
// close to death (the NearDeath piece): fainted for want of energy, nearly out
// of breath under water, or an organ losing its lasting life fast enough to
// fail within about a minute (watched over a few seconds; aging alone wears
// organs down far too slowly for this)
constexpr float BREATH_NEARLY_OUT = 0.3f;
constexpr unsigned int ORGAN_WATCH_TICKS = 50, ORGAN_FAILS_WITHIN_TICKS = 600;
// ticks a Birth, Death or NearDeath piece plays before the room's music may
// come back
constexpr int PIECE_TICKS = 15;

// how content it is, 0-1: its mind's own measure, else 1 less its three
// strongest needs on average
float contentOf(Creature* c) {
	auto* oc = dynamic_cast<oldCreature*>(c);
	if (oc && oc->getMind()) {
		float v = oc->getMind()->contentment();
		if (v >= 0)
			return std::clamp(v, 0.0f, 1.0f);
	}
	if (auto* c2 = dynamic_cast<c2Creature*>(c)) {
		std::array<float, 17> d;
		for (unsigned int i = 0; i < d.size(); i++)
			d[i] = c2->getDrive(i) / 255.0f;
		std::partial_sort(d.begin(), d.begin() + 3, d.end(), std::greater<float>());
		return std::clamp(1.0f - (d[0] + d[1] + d[2]) / 3, 0.0f, 1.0f);
	}
	return 0.5f;
}

// the danger it is in, 0-1: the strongest of its pain, fear, injury and want
// of breath, or (a norn or ettin) a grendel close by
float dangerTo(const LiveCreature& v, const std::vector<LiveCreature>& grendels) {
	float danger = 0;
	if (auto* c2 = dynamic_cast<c2Creature*>(v.creature))
		for (unsigned int need : {0u, 9u, 13u, 14u}) // pain, fear, injury, suffocation
			danger = std::max(danger, c2->getDrive(need) / 255.0f);
	if (!v.grendel)
		for (auto& g : grendels) {
			float dx = std::fabs(g.x - v.x), dy = std::fabs(g.y - v.y);
			if (dx < GRENDEL_NEAR && dy < GRENDEL_NEAR_Y)
				danger = std::max(danger, 1.0f - dx / GRENDEL_NEAR);
		}
	return danger;
}

// fainted, or nearly out of breath
bool inExtremis(Creature* c) {
	auto* oc = dynamic_cast<oldCreature*>(c);
	return oc && (oc->isUnconscious() || oc->breathLeft() < BREATH_NEARLY_OUT);
}

} // namespace

void MusicManager::creatureBorn(Agent* a) {
	if (engine.version == 2 && a)
		life_events.push_back({AgentRef(a), "Birth"});
}

void MusicManager::creatureDied(Agent* a) {
	if (engine.version == 2 && a)
		life_events.push_back({AgentRef(a), "Death"});
}

void MusicManager::setMusicControl(int setting, float value) {
	value = std::clamp(value, 0.0f, 1.0f);
	if (setting == 0)
		setVolume(value);
	else if (setting == 1)
		mood_target = value;
	else if (setting == 2)
		threat_target = value;
	else if (setting == 3)
		choose_music = value > 0;
}

void MusicManager::tickC2() {
	// (a track asked for again while it fades out starts over once it has)
	mng_music->restart_returning_track = true;

	// the creatures alive, and those in view
	const float zoom = engine.zoom > 0 ? engine.zoom : 1.0f;
	const float left = engine.camera->getX(), top = engine.camera->getY();
	const float right = left + engine.camera->getWidth() / zoom, bottom = top + engine.camera->getHeight() / zoom;
	auto inView = [&](float x, float y) { return x >= left && x < right && y >= top && y < bottom; };
	auto middleInView = [&](Agent* a) { return a && inView(a->x + a->getWidth() / 2.0f, a->y + a->getHeight() / 2.0f); };
	std::vector<LiveCreature> grendels, seen;
	for (auto& a : world.agents) {
		if (!a || a->family != 4)
			continue;
		auto* ca = dynamic_cast<CreatureAgent*>(a.get());
		Creature* c = ca ? ca->getCreature() : nullptr;
		if (!c || !c->isAlive())
			continue;
		LiveCreature v{a.get(), c, a->x + a->getWidth() / 2.0f, a->y + a->getHeight() / 2.0f, a->genus == 2};
		if (v.grendel)
			grendels.push_back(v);
		if (inView(v.x, v.y))
			seen.push_back(v);
		// how fast its organs are failing (for NearDeath)
		auto* c2 = dynamic_cast<c2Creature*>(c);
		auto& trend = organ_trends[a];
		if (c2 && world.tickcount >= trend.tick + ORGAN_WATCH_TICKS) {
			auto& organs = c2->getOrgans();
			bool failing = false;
			for (size_t i = 0; i < organs.size(); i++) {
				float lasting = organs[i]->getLongTermLifeforce();
				if (i < trend.lasting.size() && organs[i]->getInitialLifeforce() > 0) {
					float lostPerTick = (trend.lasting[i] - lasting) / (world.tickcount - trend.tick);
					if (lostPerTick > 0 && lasting / lostPerTick < ORGAN_FAILS_WITHIN_TICKS)
						failing = true;
				}
			}
			trend.lasting.resize(organs.size());
			for (size_t i = 0; i < organs.size(); i++)
				trend.lasting[i] = organs[i]->getLongTermLifeforce();
			trend.failing = failing && trend.tick > 0;
			trend.tick = world.tickcount;
		}
	}
	// (forget the creatures gone)
	std::erase_if(organ_trends, [](auto& t) { return t.first.expired(); });

	// Mood: how content the creatures in view are, on average. Threat: the
	// greatest danger any of them is in.
	if (choose_music) {
		float content = 0, danger = 0;
		for (auto& v : seen) {
			content += contentOf(v.creature);
			danger = std::max(danger, dangerTo(v, grendels));
		}
		mood_target = seen.empty() ? 0 : content / seen.size();
		threat_target = danger;
	}
	mood += std::clamp(mood_target - mood, -0.05f, 0.05f);
	threat += std::clamp(threat_target - threat, -0.1f, 0.1f);
	mng_music->setMood(mood);
	mng_music->setThreat(threat);
	// developer aid: OPENC2E_DEBUG_MUSIC=1 prints the mood and threat now and
	// then, and each track the music changes to
	static const bool debug = devenv("OPENC2E_DEBUG_MUSIC") != nullptr;
	if (debug && world.tickcount % 25 == 0)
		fmt::print("* Music: {} creatures in view, mood {:.2f} (aim {:.2f}), threat {:.2f} (aim {:.2f}), track {}\n",
			seen.size(), mood, mood_target, threat, threat_target, last_track.empty() ? "(none)" : last_track);

	auto events = std::move(life_events);
	life_events.clear();
	if (!choose_music || how_long_before_changing_track_ms > 0)
		return;

	// a hatching or a death in view (a death first; a birth does not cut a
	// piece short)
	std::string piece;
	for (auto& [ref, name] : events)
		if (middleInView(ref.get()) && (piece.empty() || name == "Death"))
			piece = name;
	if (piece == "Birth" && piece_hold > 0)
		piece.clear();
	if (piece.empty() && piece_hold > 0) {
		piece_hold--;
		return;
	}
	if (piece.empty())
		for (auto& v : seen) {
			bool extremis = inExtremis(v.creature);
			bool failing = !extremis && organ_trends[v.agent->shared_from_this()].failing;
			if (extremis || failing) {
				piece = "NearDeath";
				if (debug && last_track != piece)
					fmt::print("* Music: a creature in view is close to death ({})\n", extremis ? "fainted or out of breath" : "an organ failing fast");
				break;
			}
		}
	if (!piece.empty()) {
		playTrack(piece);
		piece_hold = PIECE_TICKS;
		return;
	}

	if (seen.empty()) {
		playTrack("Silence");
		return;
	}
	std::shared_ptr<Room> room;
	Agent* selected = world.selectedcreature.get();
	if (world.map && middleInView(selected))
		room = world.map->roomAt(selected->x + selected->getWidth() / 2.0f, selected->y + selected->getHeight() / 2.0f);
	if (!room)
		if (MetaRoom* m = engine.camera->getMetaRoom())
			room = m->roomAt((left + right) / 2, (top + bottom) / 2);
	if (room && !room->music.empty())
		playTrack(room->music);
}

/* vim: set noet: */
