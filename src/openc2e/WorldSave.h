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
// moves a save that cannot be read out of the way (save.unreadable-DATE)
void SetAsideUnreadableSave(const std::filesystem::path& worlddir, const std::string& why);
// Puts the view back where it was saved (place and zoom), once the world has
// finished starting: objects installed at load show themselves with SYS:
// CAMT, which moved the view after LoadWorldState had restored it.
void ApplySavedView();
// Moves a living C1/C2 creature to life stage 0-6 (baby to senile), older or
// younger, keeping its chemistry, organs and mind; false (with error) if not.
// The same, one stage at a time with a short pause between stages, ageing
// through the genome's own ageing receptors (Creatures menu); ticked by the world.
void RequestLifeStage(std::shared_ptr<Agent> creature, int stage);
void TickLifeStageChanges();
bool SetCreatureLifeStage(std::shared_ptr<Agent>& creature, int stage, std::string& error); // creature is replaced when made younger

} // namespace WorldSave
