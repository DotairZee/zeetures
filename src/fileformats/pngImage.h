#pragma once

#include "common/Image.h"

#include <string>

void WritePngFile(const Image& image, const std::string& filename);

class Reader;
// Reads an 8-bit RGBA or RGB PNG (not interlaced) as if_rgba32. A sprite with
// several frames stores them stacked top to bottom, all the same height, and
// names how many in a tEXt chunk "openc2e-frames" (default 1).
MultiImage ReadPngFile(Reader& in);