#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Openc2eImgui {

bool LayC2Egg(const std::vector<std::vector<uint8_t>>& genomes, int gender, int frame, const std::string& token, std::string& err);

bool IsHatcheryEnabled();
void SetHatcheryOpen(bool value);
void DrawHatchery();
// C2: a minimized Hatchery's chip on the status bar (DrawC2KitChips calls it)
void DrawHatcheryChip();

} // namespace Openc2eImgui