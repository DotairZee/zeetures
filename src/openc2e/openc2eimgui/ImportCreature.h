#pragma once

#include <filesystem>
#include <string>

// File > Import Creature: a C2 exported creature (.exp) comes into the
// world, made from its genome with its name, sex and life stage; one of
// Zeetures's own (.zex) comes back whole. Held by the hand. Returns false (and why, in message) if it can't.
bool ImportExportedCreature(const std::filesystem::path& path, std::string& message);

// the File menu's item, and the per-frame part (the file dialog's answer,
// the result popup)
void ImportCreatureMenuItem();
// File > Export Current Creature: the selected creature, whole, to a .zex
// file (WorldSave::ExportCreature); asks first, as it leaves the world
void ExportCreatureMenuItem();
void ImportCreatureTick();
