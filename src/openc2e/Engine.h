/*
 *  Engine.h
 *  openc2e
 *
 *  Created by Alyssa Milburn on Tue Nov 28 2006.
 *  Copyright (c) 2006 Alyssa Milburn. All rights reserved.
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

#ifndef _ENGINE_H
#define _ENGINE_H

#include <filesystem>
#include <map>
#include <memory>
#include <stdint.h>
#include <set>
#include <string>
#include <vector>

class caosValue;
class Backend;
class AudioBackend;
class MainCamera;
struct BackendEvent;

class Engine {
  protected:
	unsigned int tickdata;
	unsigned int ticktimes[10];
	unsigned int ticktimeptr;
	unsigned int lasttimestamp;

	bool w_down, a_down, s_down, d_down;
	std::map<std::string, std::string> wordlist_translations;

	bool cmdline_enable_sound;
	bool cmdline_norun;
	std::vector<std::string> cmdline_bootstrap;
	std::vector<std::string> cmdline_args;
	std::string cmdline_world;
	bool cmdline_no_world_chooser = false;

	std::string gamename;

	std::map<std::string, Backend*> possible_backends;
	std::map<std::string, AudioBackend*> possible_audiobackends;
	std::string preferred_backend, preferred_audiobackend;

	void handleKeyboardScrolling();
	void handleResizedWindow();
	void handleMouseMove(BackendEvent& event);
	void handleMouseButton(BackendEvent& event);
	void handleTextInput(BackendEvent& event);
	void handleRawKeyDown(BackendEvent& event);
	void handleRawKeyUp(BackendEvent& event);

	class peFile* exefile;
	void loadGameData();

  public:
	std::map<std::string, caosValue> eame_variables; // non-serialised

	std::shared_ptr<class NetBackend> net;
	std::unique_ptr<class MusicManager> musicmanager;
	std::string getBackendName() { return preferred_backend; }
	std::string getAudioBackendName() { return preferred_audiobackend; }

	std::unique_ptr<MainCamera> camera;

	// C1/C2 world view zoom (owner, 2026-11-09): the world is drawn at this
	// scale; camera sizes are in world pixels. See setZoom.
	float zoom = 1.0f;
	void setZoom(float z, int anchorx = -1, int anchory = -1);
	void zoomStep(int steps, int anchorx = -1, int anchory = -1);
	std::shared_ptr<class RenderTarget> zoomtarget;

	std::vector<std::string> wordlist;
	std::string translateWordlistWord(const std::string& s);
	class peFile* getExeFile() {
		return exefile;
	}

	std::string getGameName() { return gamename; }
	uint32_t get_ticks_msec() const;
	// how far we are from the last world tick to the next one, 0-1
	float tickFraction() const;

	bool done;
	bool dorendering, fastticks, refreshdisplay;
	// game speed (owner, 2026-11-13: fast forward up to 10x; 2026-11-14:
	// on the toolbar, in steps of 0.5 or typed, 0.5 to 10): world ticks run
	// this many times as often; 1 is the game's own pace
	float speed = 1.0f;
	float measuredSpeed = 1.0f; // the pace the world actually reached lately (a busy world may not keep up)
	unsigned int measureStart = 0, measureTicks = 0;
	unsigned int tickInterval() const;
	unsigned int version;
	std::string gametype;
	bool bmprenderer;
	std::string language;
	std::string mindname; // --mind: pluggable C1/C2 creature mind (empty = original brain)
	std::string currentworld; // C1/C2: name of the world folder in use, if any
	std::filesystem::path currentworlddir;
	bool creatingAdditions = false; // the Additions scripts are running at world load
	// C2: built-in objects (builtin-cobs.txt) the player had no COB for at
	// load; injectMissingBuiltinCobs() adds those found since (a breed pack
	// just imported, BreedPacks.cpp)
	std::vector<std::string> missingBuiltinCobs;
	// the script patches applied during this world load ("f g s e|find"), so
	// one a later patch of the same script changed again isn't reported as
	// missing at the end of the load
	std::set<std::string> patchesApplied;
	void injectMissingBuiltinCobs();
	bool worldready = false; // C1/C2: the world (and any saved state) has finished loading
	bool autosave = true; // C1/C2: save the world on quit and when switching worlds (--no-autosave)
	const std::vector<std::string>& getCommandLineArgs() const { return cmdline_args; }

	Engine();
	~Engine();
	void setBackend(Backend* b);
	std::string executeNetwork(std::string in);
	// C1/C2: the Additions folders' script-patches.txt applied to the
	// scripts now installed (at load, and after an object is injected);
	// quiet: no line for a patch whose text is not there
	void applyScriptPatches(bool quiet);
	unsigned int msUntilTick();
	void drawWorld();
	void update();
	bool tick();
	void processEvents();

	void addPossibleBackend(std::string, Backend*);
	void addPossibleAudioBackend(std::string, AudioBackend*);

	bool parseCommandLine(int argc, char* argv[]);
	bool initialSetup();
	void shutdown();
	// C1/C2 on quitting: save the open world now (if it is saved on quit),
	// true if that worked; the caller may then end the process at once
	bool saveForQuit();

	bool noRun() { return cmdline_norun; }
};

extern Engine engine;
#endif
/* vim: set noet: */
