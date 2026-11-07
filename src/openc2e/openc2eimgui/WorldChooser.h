#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Openc2eImgui {

struct WorldEntry {
	std::string name;
	std::filesystem::path dir;
};

std::filesystem::path GetWorldsDirectory();
std::filesystem::path WorldFile(const std::filesystem::path& worlddir);
std::vector<WorldEntry> ListWorlds();
std::optional<WorldEntry> FindWorld(const std::string& name);
std::vector<std::filesystem::path> ListWorldTemplates();
bool CreateWorld(const std::string& name, const std::filesystem::path& tmpl, std::string& error);

// Restarts openc2e with --world <name> (C1/C2 worlds load once, at start-up).
void RelaunchIntoWorld(const std::string& name);

// The new-world form; returns the created world's name once created.
// reset = true only clears the form (draws nothing).
std::string DrawNewWorldForm(bool reset);

// Start-up chooser, run before the world loads. Returns "" if the user quits.
std::string RunWorldChooser();

} // namespace Openc2eImgui
