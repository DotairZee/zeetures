#pragma once

#include "common/Image.h"

#include <filesystem>
#include <string>
#include <vector>

class CobManager {
  public:
	struct CobFileInfo {
		CobFileInfo(std::string name_, std::filesystem::path filename_)
			: name(name_), filename(filename_) {}
		std::string name;
		std::filesystem::path filename;
		bool is_removable = false;
		// for the Agent Injector (C2): the COB's description; the category of
		// the first classified object its install script makes (helpers in
		// genus 1 are passed over; -1: none) and that classifier; when the
		// file was added (its modification time); whether it is in the
		// player's own data folder (imported) rather than the game's
		std::string description;
		int category = -1;
		int family = 0, genus = 0, species = 0;
		long long added = 0;
		bool imported = false;
	};

	std::vector<CobFileInfo> objects;

	void update();
	// a list of agent names, one per line (# starts a comment), as in the
	// Additions folder's builtin-cobs.txt and hidden-cobs.txt; empty if none
	static std::vector<std::string> readNameList(const std::filesystem::path& file);
	Image getPicture(const CobFileInfo& info);
	// byHand: added from the Agent Injector, so the objects it makes are
	// saved with the world and its scripts installed again at each load
	void inject(const CobFileInfo& info, bool byHand = false);
	void remove(const CobFileInfo& info, bool byHand = false);
	// when each object was last added from the Agent Injector (seconds since
	// 1970), kept in injector-used.txt in the user data folder
	static long long lastUsed(const std::string& name);
	static void noteUsed(const std::string& name);
	// a world load: the scripts of the C2 agent of that name (and any image
	// or sound it needs), without its install script; false if no COB has it
	static bool reinstallScripts(const std::string& agentname, std::string& error);
};