#include "SoundManager.h"
#include <filesystem>
#include <cstring>
#include <fstream>

#include "Engine.h"
#include "Map.h"
#include "PathResolver.h"
#include "World.h"
#include "caosValue.h"
#include "common/math/ModularArithmetic.h"

#include <fmt/core.h>

// this is all for SoundManager::tick
#include "Camera.h"
#include "MetaRoom.h"
#include "Room.h"
#include "World.h"

#include <cmath>

SoundManager soundmanager;

SoundManager::SoundManager() {
}
SoundManager::~SoundManager() {
}

float SoundManager::getVolume() {
	return sound_effects_volume;
}

void SoundManager::setVolume(float volume) {
	sound_effects_volume = volume;
	updateVolumes();
}

bool SoundManager::isMuted() {
	return sound_effects_muted;
}

void SoundManager::setMuted(bool muted) {
	sound_effects_muted = muted;
	updateVolumes();
}

void SoundManager::setVoicesMutedByPlayer(bool muted) {
	voices_muted = muted;
	updateVolumes();
}

bool SoundManager::SoundData::isAlive() {
	if (handle && get_audio_backend()->audio_channel_get_state(handle) == AUDIO_PLAYING) {
		return true;
	} else {
		// resetAndIncrementGeneration();
		return false;
	}
}

SoundManager::SoundData* SoundManager::getSoundData(Sound& sound) {
	static_assert(sizeof(Sound) == sizeof(SoundManager::SoundId), "");
	SoundId id = *reinterpret_cast<SoundId*>(&sound); // TODO

	SoundData* data = sources.try_get(id);
	if (!data) {
		return nullptr;
	}

	return data;
}

Sound SoundManager::getNewSound(AudioChannel handle, bool is_creature_voice) {
	SoundData data;
	data.handle = handle;
	data.is_creature_voice = is_creature_voice;
	updateVolume(data);

	SoundId id = sources.insert(std::move(data));
	static_assert(sizeof(Sound) == sizeof(SoundManager::SoundId), "");
	return *reinterpret_cast<Sound*>(&id); // TODO
}

bool SoundManager::areCreatureVoicesMuted() {
	if (engine.version == 3) {
		// TODO: is this slow?
		auto it = world.variables.find("engine_dumb_creatures");
		return it != world.variables.end() && it->second.hasInt() && it->second.getInt() != 0;
	} else {
		return false;
	}
}

template <typename T, typename U, typename V>
auto clamp(T value, U low, V high) {
	assert(!(high < low));
	return (value < low) ? low : (high < value) ? high
												: value;
}

void SoundManager::updateVolume(SoundData& s) {
	if (!s.isAlive()) {
		return;
	}

	// creature speech has its own switch; muting the world's sounds leaves
	// it alone, and the reverse
	float volume = (s.is_creature_voice ? voices_muted : sound_effects_muted) ? 0 : sound_effects_volume;

	if (s.is_creature_voice && areCreatureVoicesMuted()) {
		volume = 0;
	}

	if (s.positioned) {
		MetaRoom* room = world.map->metaRoomAt(s.x, s.y);
		if (room && engine.camera->getMetaRoom() == room) {
			// mod_distance gives the distance between x and camerax, taking
			// into account metaroom wraparound ("modular distance")
			const float distx = mod_distance(
				s.x + s.width / 2,
				static_cast<float>(engine.camera->getXCentre()),
				room->wraparound() ? static_cast<float>(room->width()) : INFINITY);
			const float disty = s.y + s.height / 2 - engine.camera->getYCentre();

			const float screen_width = engine.camera->getWidth();
			const float screen_height = engine.camera->getHeight();

			// If a sound is on-screen, then play it at full volume.
			// If it's more than a screen offscreen, then mute it.
			// TODO: Does this sound right?
			const float starts_fading = 0.5;
			const float cutoff = 1.5;
			// the math
			float volx = 1.0;
			if (std::abs(distx) / screen_width > starts_fading) {
				volx = clamp(1 - (std::abs(distx) / screen_width - starts_fading) / (cutoff - starts_fading), 0, 1);
			}
			float voly = 1.0;
			if (std::abs(disty) / screen_height > starts_fading) {
				voly = clamp(1 - (std::abs(disty) / screen_height - starts_fading) / (cutoff - starts_fading), 0, 1);
			}
			volume *= std::min(volx, voly);

			// Pan sound as we get closer to screen edge
			// TODO: Does this sound right?
			float pan = clamp(distx / screen_width, -1, 1);
			get_audio_backend()->audio_channel_set_pan(s.handle, pan);
		} else if (!room) {
			// TODO: think about volume when positioning outside-metaroom agents
			volume = 0;
		} else {
			volume = 0;
		}
	}

	if (s.fade_start != decltype(s.fade_start)()) {
		float volume_multiplier = 1 - (std::chrono::steady_clock::now() - s.fade_start) / s.fade_length;
		if (volume_multiplier <= 0) {
			get_audio_backend()->audio_channel_stop(s.handle);
		}
		volume *= volume_multiplier;
	}

	get_audio_backend()->audio_channel_set_volume(s.handle, volume);
}

void SoundManager::updateVolumes() {
	for (auto it = sources.begin(); it != sources.end();) {
		if (it->isAlive()) {
			updateVolume(*it);
			it++;
		} else {
			// safe because erasure invalidates the end() iterator,
			// but we're calculating it again at the start of each loop.
			it = sources.erase(it);
		}
	}
}

Sound SoundManager::playSound(std::string name, bool loop) {
	if (name.size() == 0)
		return {};

	auto filename = findSoundFile(name + ".wav");
	if (filename.empty()) {
		if (engine.version < 3)
			return {}; // creatures 1 and 2 ignore non-existent audio clips
		throw Exception(fmt::format("No such clip '{}.wav'", name));
	}

	auto handle = get_audio_backend()->play_clip(filename, loop, true); // getNewSound sets its volume
	if (!handle) {
		// note that more specific error messages can be thrown by implementations of play_clip
		throw_exception("failed to load audio clip {:?}", filename.string());
	}

	return getNewSound(handle);
}

// A copy of a PCM WAV clip resampled to play at `pitch` times its rate
// (higher and shorter above 1), made once in the user data folder's "Voice
// cache" and reused; the clip itself if it can't be made. For voices by a
// creature's age (CreatureSpeech): made on the player's machine from their
// own game's syllables.
static std::filesystem::path pitchedClip(const std::filesystem::path& clip, const std::string& name, float pitch) {
	int pct = (int)std::lround(pitch * 100.0f);
	if (pct == 100)
		return clip;
	std::filesystem::path dir = getUserDataDir() / "Voice cache";
	std::filesystem::path out = dir / fmt::format("{}_p{}.wav", name, pct);
	if (std::filesystem::exists(out))
		return out;
	std::ifstream in(clip, std::ios::binary);
	std::vector<uint8_t> d((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	auto u16 = [&](size_t o) { return (unsigned)(d[o] | (d[o + 1] << 8)); };
	auto u32 = [&](size_t o) { return (unsigned)(d[o] | (d[o + 1] << 8) | (d[o + 2] << 16) | ((unsigned)d[o + 3] << 24)); };
	if (d.size() < 12 || std::memcmp(d.data(), "RIFF", 4) != 0 || std::memcmp(d.data() + 8, "WAVE", 4) != 0)
		return clip;
	size_t fmtAt = 0, dataAt = 0, dataLen = 0;
	for (size_t o = 12; o + 8 <= d.size();) {
		unsigned len = u32(o + 4);
		if (std::memcmp(d.data() + o, "fmt ", 4) == 0)
			fmtAt = o + 8;
		else if (std::memcmp(d.data() + o, "data", 4) == 0) {
			dataAt = o + 8;
			dataLen = std::min<size_t>(len, d.size() - dataAt);
		}
		o += 8 + len + (len & 1);
	}
	if (!fmtAt || !dataAt || u16(fmtAt) != 1)
		return clip; // PCM only
	unsigned channels = u16(fmtAt + 2), bits = u16(fmtAt + 14);
	if ((bits != 8 && bits != 16) || channels < 1)
		return clip;
	unsigned frame = channels * bits / 8;
	size_t frames = dataLen / frame;
	size_t outFrames = std::max<size_t>(1, (size_t)(frames / pitch));
	std::vector<uint8_t> pcm(outFrames * frame);
	for (size_t i = 0; i < outFrames; i++) {
		float src = i * pitch;
		size_t a = std::min(frames - 1, (size_t)src), b = std::min(frames - 1, a + 1);
		float f = src - a;
		for (unsigned c = 0; c < channels; c++) {
			if (bits == 8) {
				float v = d[dataAt + a * frame + c] * (1 - f) + d[dataAt + b * frame + c] * f;
				pcm[i * frame + c] = (uint8_t)std::lround(v);
			} else {
				auto s16 = [&](size_t fr) { return (int16_t)u16(dataAt + fr * frame + c * 2); };
				int v = (int)std::lround(s16(a) * (1 - f) + s16(b) * f);
				pcm[i * frame + c * 2] = v & 0xff;
				pcm[i * frame + c * 2 + 1] = (v >> 8) & 0xff;
			}
		}
	}
	std::error_code ec;
	std::filesystem::create_directories(dir, ec);
	std::ofstream o(out, std::ios::binary);
	if (!o)
		return clip;
	auto w32 = [&](unsigned v) { char b[4] = {(char)(v & 0xff), (char)((v >> 8) & 0xff), (char)((v >> 16) & 0xff), (char)(v >> 24)}; o.write(b, 4); };
	o.write("RIFF", 4);
	w32((unsigned)(36 + pcm.size()));
	o.write("WAVE", 4);
	o.write("fmt ", 4);
	w32(16);
	o.write(reinterpret_cast<const char*>(d.data() + fmtAt), 16);
	o.write("data", 4);
	w32((unsigned)pcm.size());
	o.write(reinterpret_cast<const char*>(pcm.data()), pcm.size());
	return o ? out : clip;
}

Sound SoundManager::playVoice(std::string name, float pitch) {
	if (name.size() == 0)
		return {};

	auto filename = findSoundFile(name + ".wav");
	if (filename.empty()) {
		if (engine.version < 3)
			return {}; // creatures 1 and 2 ignore non-existent audio clips
		throw Exception(fmt::format("No such clip '{}.wav'", name));
	}
	filename = pitchedClip(filename, name, pitch);

	auto handle = get_audio_backend()->play_clip(filename, false, true); // getNewSound sets its volume
	if (!handle) {
		// note that more specific error messages can be thrown by implementations of play_clip
		throw_exception("failed to load audio clip {:?}", filename.string());
	}

	return getNewSound(handle, true);
}


void SoundManager::tick() {
	// track our current viewpoint center
	viewpoint_center_x = engine.camera->getXCentre();
	viewpoint_center_y = engine.camera->getYCentre();

	// update sounds
	// TODO: is this slow?
	updateVolumes();
}

/* vim: set noet: */
