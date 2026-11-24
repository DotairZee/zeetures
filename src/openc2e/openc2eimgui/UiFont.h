#pragma once

#include <string>

namespace Openc2eImgui {

// The program's own windows (menus, kits, Preferences) are drawn with a
// small font that has only Western European letters. Text shown in them in
// other alphabets (words in other languages, creatures' names) is passed
// here first: its letters are added to that font from the world's (Fusion
// Pixel's 12 pixel letters, PixelText) before the next frame. Common
// alphabets (Latin, Greek, Cyrillic, Hebrew, kana) are there from the start.
void UiFontNeed(const std::string& utf8);
// the text as such a window shows it: right-to-left runs turned around,
// Arabic letters joined (PixelText::visualOrder), with its letters added
std::string UiShown(const std::string& utf8);
// before each frame (g_beforeUiFrame, set in Openc2eImGui::Init): true when
// the font gained letters
bool UiFontUpdate();

} // namespace Openc2eImgui
