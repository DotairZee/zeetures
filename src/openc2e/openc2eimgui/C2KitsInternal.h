#pragma once

// Shared pieces for the kit pages (C2Kits.cpp, HealthKit.cpp, ScienceKit.cpp).

#include "ImGuiUtils.h"

#include <string>
#include <vector>

class Agent;
class c2Creature;

namespace Openc2eImgui {

// The kits' art comes from the player's own Creatures 2 ("Applet Data"),
// loaded at runtime and never shipped. Pictures are cached by name.
Texture KitPicture(const std::string& name); // a .bmp drawn as it is
const std::vector<Texture>& KitSprites(const std::string& name); // .s16 frames, transparent background

// The selected creature, if it is a C2 creature; and its agent.
c2Creature* KitCreature();
Agent* KitCreatureAgent();

// The game's chemical names (allchemicals.str) and the Applet Data
// ChemGroups (with "All" first).
const std::vector<std::string>& KitChemicalNames();
const std::vector<std::pair<std::string, std::vector<int>>>& KitChemicalGroups();

// For pages that need a creature: when none is selected, draws the kit's
// cover picture with a note and returns false.
bool KitNeedsCreature(const char* cover);

// Draws a number in the kit's green LED digits (led.s16: 0-9, 10 blank,
// 11 minus, 12 degrees C, 13 degrees F, 14 point) from a string of digits,
// '-', '.', 'C', 'F' and spaces; returns the width drawn.
float KitLed(ImVec2 p, const std::string& text);

// A horizontal level bar in the kits' style (StatBack.bmp behind,
// BigBarFront.bmp revealed up to the level), 0-1, with a label on it. A
// "need" bar starts green and turns red as it fills; otherwise full is green.
void KitBar(float level, const char* label, float width = 192, bool need = false);

// a page tab (ImGui::BeginTabItem; the test aid can pick one)
bool KitTab(const char* label);

void DrawHealthKit();
void DrawScienceKit();

} // namespace Openc2eImgui
