#pragma once

#include <filesystem>
#include <string>

// Saving and restoring C1/C2 world state in openc2e's own format (creatures,
// their bodies and minds, and eggs). See WorldSave.cpp.
namespace WorldSave {

bool HasSavedState(const std::filesystem::path& worlddir);
bool SaveWorldState(const std::filesystem::path& worlddir, std::string& error);
// Applies a saved state after the world file has loaded; false if none.
bool LoadWorldState(const std::filesystem::path& worlddir);

} // namespace WorldSave
