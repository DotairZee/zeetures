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
	// a world load: the scripts of the C2 agent of that name (and any image
	// or sound it needs), without its install script; false if no COB has it
	static bool reinstallScripts(const std::string& agentname, std::string& error);
};