#pragma once

// Zeetures's world additions for Creatures 2 (owner, 2026-11-15: anyone who
// owns the game installs Zeetures and gets the same world): the scripts run
// after a world loads (*.cos) and the text files read with them
// (announcements.txt, script-patches.txt, builtin-cobs.txt, hidden-cobs.txt,
// pheromones.txt, breed-names.txt) are built into the program from the
// repository's additions/ folder.
//
// A file of the same name in the player's own user data folder wins over
// the built-in one (the Additions folder; pheromones.txt and
// breed-names.txt in the user data folder itself), so the player can
// change any of them, and an empty file there switches a built-in one off.
// Built-in additions apply to Creatures 2 only.

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

// generated from additions/ (scripts/embed_files.py --func EmbeddedAddition)
std::span<const uint8_t> EmbeddedAddition(const std::string& name);
std::vector<std::string> EmbeddedAdditionNames();

namespace Additions {

// where the player's own copy of a file goes: the Additions folder, or the
// user data folder itself for pheromones.txt and breed-names.txt
std::filesystem::path userPath(const std::string& name);

// whether built-in additions apply (Creatures 2 only)
bool builtinsApply();

// a text file's contents: the player's own copy if there is one, else the
// built-in one (when they apply), else empty. fromPlayer says which.
std::string text(const std::string& name, bool* fromPlayer = nullptr);

// the world-addition scripts to run, sorted by name: the player's own .cos
// files and the built-in ones they do not replace
struct Script {
	std::string name;
	std::string text;
	bool fromPlayer = false;
	// a built-in script whose art is not installed (the first such sprite,
	// e.g. "zbw1"): it is skipped whole rather than half run, since it may
	// change rooms before making the object that goes with them
	std::string missingArt;
};
std::vector<Script> scripts();

// before the program changes a file the player may not have yet (the Agent
// Injector's built-in list): write the built-in copy to the player's folder
void ensureUserCopy(const std::string& name);

} // namespace Additions
