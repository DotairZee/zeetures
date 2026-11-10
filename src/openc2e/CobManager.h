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
	void inject(const CobFileInfo& info);
	void remove(const CobFileInfo& info);
};