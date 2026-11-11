#pragma once

#include "Sound.h"
#include "common/audio/AudioBackend.h"
#include "common/slotmap/DenseSlotMap.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

using fmilliseconds = std::chrono::duration<float, std::milli>;

class SoundManager {
  public:
	SoundManager();
	~SoundManager();

	void tick();
	Sound playSound(std::string filename, bool loop = false);
	// pitch: playback rate (1 as recorded); other rates play a resampled copy
	// made once in the user data folder's "Voice cache"
	Sound playVoice(std::string filename, float pitch = 1.0f);

	float getVolume();
	void setVolume(float);
	bool isMuted();
	void setMuted(bool);
	bool areCreatureVoicesMuted();
	// the player's own switch for creature speech (bibble), apart from the
	// world's other sounds (isMuted/setMuted)
	bool areVoicesMutedByPlayer() const { return voices_muted; }
	void setVoicesMutedByPlayer(bool muted);

  private:
	friend class Sound;
	struct SoundData {
		SoundData() = default;
		SoundData(SoundData&&) = default;
		SoundData& operator=(SoundData&&) = default;
		SoundData(const SoundData&) = delete;
		SoundData& operator=(const SoundData&) = delete;

		bool isAlive();

		AudioChannel handle{};

		bool is_creature_voice = false;
		bool positioned = false;
		float x = 0;
		float y = 0;
		float width = 0;
		float height = 0;

		std::chrono::time_point<std::chrono::steady_clock> fade_start{};
		fmilliseconds fade_length{};
	};

	DenseSlotMap<SoundData> sources;
	using SoundId = decltype(sources)::Key;

	void updateVolume(SoundData& source);
	void updateVolumes();
	SoundData* getSoundData(Sound& source);
	Sound getNewSound(AudioChannel handle, bool is_creature_voice = false);

	bool sound_effects_muted = false;
	bool voices_muted = false;
	float sound_effects_volume = 1.0;

	float viewpoint_center_x;
	float viewpoint_center_y;
};

extern SoundManager soundmanager;
