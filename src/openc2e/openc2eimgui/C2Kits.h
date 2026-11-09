#pragma once

// The Creatures 2 kits (Health Kit, Science Kit and the rest), drawn inside
// the game window. Each kit is a floating panel with a minimize button in its
// title bar; a minimized kit becomes a labeled chip on the status bar, and
// clicking the chip brings it back where it was. Which kits are open or
// minimized, and where they sit, is kept between sessions (kits.ini in the
// user data folder).

namespace Openc2eImgui {

// Kit ids: "health", "science", "breeders".
bool IsC2KitAvailable(const char* id);
void OpenC2Kit(const char* id);

// Draws every open kit (call once a frame, C2 only).
void DrawC2Kits();

// Draws the chips for minimized kits, in the status bar's horizontal row.
void DrawC2KitChips();

// Celsius or Fahrenheit for every temperature shown (status bar and kits);
// clicking a temperature switches it.
bool KitFahrenheit();
void KitToggleFahrenheit();

} // namespace Openc2eImgui
