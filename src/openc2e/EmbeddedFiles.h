#pragma once

// openc2e's own art, built into the program (data/; the build embeds the
// files with scripts/embed_files.py). Our own work only: never game data.

#include <cstdint>
#include <span>
#include <string>

// the file's bytes, by file name ("wall.png"); empty if there is none
std::span<const uint8_t> EmbeddedFile(const std::string& name);
