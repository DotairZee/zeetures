#pragma once

#include <filesystem>
#include <memory>
#include <string>

class Agent;

// Saving and restoring C1/C2 world state in openc2e's own format (creatures,
// their bodies and minds, and eggs). See WorldSave.cpp.
namespace WorldSave {

bool HasSavedState(const std::filesystem::path& worlddir);
bool SaveWorldState(const std::filesystem::path& worlddir, std::string& error);
// Applies a saved state after the world file has loaded; false if none.
bool LoadWorldState(const std::filesystem::path& worlddir);
// Moves a living C1/C2 creature to life stage 0-6 (baby to senile), older or
// younger, keeping its chemistry, organs and mind; false (with error) if not.
bool SetCreatureLifeStage(std::shared_ptr<Agent>& creature, int stage, std::string& error); // creature is replaced when made younger

} // namespace WorldSave
