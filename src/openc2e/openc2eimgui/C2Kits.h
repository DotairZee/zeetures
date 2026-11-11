#pragma once

// The Creatures 2 kits (Health Kit, Science Kit and the rest), drawn inside
// the game window. Each kit is a floating panel with a minimize button in its
// title bar; a minimized kit becomes a labeled chip on the status bar, and
// clicking the chip brings it back where it was. Which kits are open or
// minimized, and where they sit, is kept between sessions (kits.ini in the
// user data folder).

#include <memory>
#include <string>
#include <vector>

class Agent;

namespace Openc2eImgui {

// the name a creature was given in the Owner's Kit, or empty
std::string KitGivenName(Agent* a);

// Kit ids: "health", "science", "breeders", "owners", "observation".
bool IsC2KitAvailable(const char* id);
void OpenC2Kit(const char* id);
// opens a kit at one of its pages ("Registration", "Reproductive")
void OpenC2KitAt(const char* id, const char* page);

// Births, deaths and pregnancies as small notices on the right of the status
// bar (in its horizontal row). Clicking one selects the creature, moves the
// camera to it and opens the Owner's Kit (birth, death) or the Breeder's Kit
// (pregnancy); right-click dismisses it. As many of the newest as fit in
// room (pixels), the rest behind a "+N" that opens the life log.
void DrawLifeNotices(float room);

// Creatures' eggs in the world (C2: 2 5 2, laid or taken from the Hatchery;
// not the world's own grendel and ettin eggs) that have not hatched, in the
// world's order. An egg holds its baby's genome name (OBV0, four letters);
// KitEggOf finds the unhatched egg with that genome, KitHatchedFrom the
// creature that hatched from it. Also a few words on whether and why an egg
// is waiting ("in water: it hatches once it is on dry land"), and moving
// the camera to an agent without selecting it.
std::vector<std::shared_ptr<Agent>> KitUnhatchedEggs();
std::string KitEggGenome(Agent* egg);
std::shared_ptr<Agent> KitEggOf(const std::string& genome);
std::shared_ptr<Agent> KitHatchedFrom(const std::string& genome);
std::string KitEggState(Agent* egg);
void KitLookAt(const std::shared_ptr<Agent>& a);

// Draws every open kit (call once a frame, C2 only).
void DrawC2Kits();

// Draws the chips for minimized kits, in the status bar's horizontal row.
void DrawC2KitChips();

// Celsius or Fahrenheit for every temperature shown (status bar and kits);
// clicking a temperature switches it.
bool KitFahrenheit();
void KitToggleFahrenheit();

} // namespace Openc2eImgui
