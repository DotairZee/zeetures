#pragma once

#include "common/backend/BackendTexture.h"

#include <imgui.h>
#include <stdint.h>
#include <string>
#include <vector>

static inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs) {
	return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y);
}
static inline ImVec2 operator+(const ImVec2& lhs, double rhs) {
	return ImVec2(lhs.x + rhs, lhs.y + rhs);
}
static inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs) {
	return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y);
}
static inline ImVec2 operator-(const ImVec2& lhs, double rhs) {
	return ImVec2(lhs.x - rhs, lhs.y - rhs);
}
static inline ImVec2 operator*(double lhs, const ImVec2& rhs) {
	return ImVec2(lhs * rhs.x, lhs * rhs.y);
}
static inline ImVec2 operator/(const ImVec2& lhs, double rhs) {
	return ImVec2(lhs.x / rhs, lhs.y / rhs);
}

namespace ImGuiUtils {

void Image(Texture texture);

Texture GetTextureFromExeFile(uint32_t resource);
Texture GetTextureFromExeFileWithTransparentTopLeft(uint32_t resource);
std::vector<Texture> LoadImageWithTransparentTopLeft(const std::string& name);

bool BeginWindow(const char* title, bool* is_open, ImGuiWindowFlags flags = ImGuiWindowFlags_None);

// Kits in their own windows (experimental): with ImGui's multi-viewports on,
// window places are the desktop's, so a place in the game window is offset
// by the game window's corner (0,0 otherwise)
ImVec2 GameWindowPos();
// the next window stays in the game window (the menus and toolbars, the
// status bar, scroll bars and notices): no window of its own, and with
// ImGuiWindowFlags_NoDocking in its flags nothing docks to it
void KeepInGameWindow();
// the places saved for windows outside the game window are forgotten: they
// open in the game window, one below another from near its top-left corner
void ForgetOutsidePlaces();
void EndWindow();

void DrawTexture(Texture texture, ImVec2 p, float transparency = 1.0);

// the bold sans loaded at start-up at an exact size (8, 9, 10, 11, 15 or
// 17: the Hatchery's labels, headings and card), or null if there is none
ImFont* NurseryFont(int size);

void DisabledButton(const char* text);
bool ImageButton(Texture texture, bool enabled = true);

class TextureRect {
  public:
	Texture parent;
	int32_t x;
	int32_t y;
	int32_t w;
	int32_t h;
};
// (drawn at size, when given; else at the picture's own)
bool ImageButton(TextureRect texture, bool enabled = true, ImVec2 size = ImVec2(0, 0));
// the picture fitted, at its own shape, into a square button `side` pixels
// tall (as tall as the lists beside it)
bool ImageButtonFit(TextureRect tex, bool enabled, float side);

} // namespace ImGuiUtils
