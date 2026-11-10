#pragma once

#include <filesystem>
#include <string>

// File > Import Creature (C2): an exported creature (.exp) comes into the
// world, made from its genome with its name, sex and life stage, held by the
// hand. Returns false (and why, in message) if it can't.
bool ImportExportedCreature(const std::filesystem::path& path, std::string& message);

// the File menu's item, and the per-frame part (the file dialog's answer,
// the result popup)
void ImportCreatureMenuItem();
void ImportCreatureTick();
