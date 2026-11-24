/*
 *  MusicManager.h
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

#ifndef _MUSICMANAGER_H
#define _MUSICMANAGER_H

#include "AgentRef.h"
#include "common/audio/AudioChannel.h"

#include <map>
#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

class MNGMusic;

class MusicManager {
  public:
	MusicManager();
	~MusicManager();
	void stop();

	float getVolume();
	void setVolume(float volume);
	float getMIDIVolume();
	void midi_set_volume(float volume);
	bool isMuted();
	void setMuted(bool muted);
	bool isMIDIMuted();
	void setMIDIMuted(bool muted);

	void tick();
	void playTrack(std::string track);
	void playTrackForAtLeastThisManyMilliseconds(std::string track, unsigned int how_long_before_changing_track_ms);

	// C2: a creature hatched or died (Creature::born, Creature::die), for the
	// score's Birth and Death pieces, played if it happened in view
	void creatureBorn(Agent* a);
	void creatureDied(Agent* a);
	// C2's CAOS MUSC: 0 the music's volume, 1 the mood it aims for, 2 the
	// threat it aims for (each 0-1), 3 the game choosing music by itself
	// (and setting mood and threat from the creatures) on or off
	void setMusicControl(int setting, float value);

  private:
	void updateVolumes();
	void tickC2();

	bool music_muted = false;
	float music_volume = 1.0;
	float midi_volume = 1.0;

	AudioChannel creatures1_channel;
	uint32_t creatures1_ticks_until_next_sound;
	std::map<std::string, class MNGFile*> files;
	std::unique_ptr<MNGMusic> mng_music;
	std::string last_track;
	unsigned int how_long_before_changing_track_ms = 0;

	// C2: the score follows the creatures in view. Mood and Threat (0-1, as
	// Music.mng uses them) move toward their targets a step each tick.
	float mood = 0, threat = 0, mood_target = 0, threat_target = 0;
	bool choose_music = true;
	std::vector<std::pair<AgentRef, std::string>> life_events; // since the last tick
	int piece_hold = 0; // ticks a Birth, Death or NearDeath piece keeps the room's music off
	// each creature's organs' lasting life when last looked at, to see one
	// failing fast (NearDeath)
	struct OrganTrend {
		unsigned int tick = 0;
		std::vector<float> lasting;
		bool failing = false;
	};
	std::map<std::weak_ptr<Agent>, OrganTrend, std::owner_less<std::weak_ptr<Agent>>> organ_trends;
};

#endif
/* vim: set noet: */
