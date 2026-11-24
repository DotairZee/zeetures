#include "UiFont.h"

#include "PixelText.h"
#include "common/backend/Backend.h"

#include <cmath>
#include <imgui.h>
#include <set>
#include <vector>

namespace Openc2eImgui {

namespace {

std::set<char32_t> s_have; // added
std::set<char32_t> s_want; // to add before the next frame
struct Added {
	int rect;
	PixelText::Glyph g;
};
std::vector<Added> s_added; // (each rebuild of the font draws them all again)

bool fontHas(ImFont* f, char32_t c) {
	return c <= 0xFFFF && f->FindGlyphNoFallback((ImWchar)c) != nullptr;
}

void preload() {
	static bool done = false;
	if (done)
		return;
	done = true;
	auto range = [](char32_t a, char32_t b) {
		for (char32_t c = a; c <= b; c++)
			s_want.insert(c);
	};
	range(0x0100, 0x024F); // Latin Extended
	range(0x0370, 0x03FF); // Greek
	range(0x0400, 0x04FF); // Cyrillic
	range(0x05D0, 0x05EA); // Hebrew
	range(0x0E01, 0x0E5B); // Thai
	range(0x1EA0, 0x1EF9); // Vietnamese
	range(0x3040, 0x30FF); // kana
	range(0xFE70, 0xFEFC); // Arabic, joined forms
	range(0xFEF5, 0xFEFC);
}

} // namespace

// before each frame: adds what was asked for to the default font (Fonts[0])
bool UiFontUpdate() {
	preload();
	if (s_want.empty() || !ImGui::GetCurrentContext())
		return false;
	ImGuiIO& io = ImGui::GetIO();
	if (io.Fonts->Fonts.empty())
		return false;
	ImFont* font = io.Fonts->Fonts[0];
	size_t before = s_added.size();
	// the line's baseline in the default font (its ascent, in whole pixels)
	float base = std::round(font->Ascent);
	for (char32_t c : s_want) {
		if (s_have.count(c) || c > 0xFFFF)
			continue;
		s_have.insert(c);
		if (fontHas(font, c))
			continue;
		PixelText::Glyph g;
		if (!PixelText::uiGlyph(c, g) || !g.width)
			continue;
		// a mark sits over the letter before it
		float xoff = g.mark ? -(float)(g.width + 6) / 2.0f - 1 : (float)g.xoff;
		int id = io.Fonts->AddCustomRectFontGlyph(font, (ImWchar)c, g.width, g.height, g.mark ? 0.0f : (float)g.advance, ImVec2(xoff, base + g.top));
		s_added.push_back({id, g});
	}
	s_want.clear();
	if (s_added.size() == before)
		return false;
	io.Fonts->Build();
	unsigned char* a8;
	int tw, th;
	io.Fonts->GetTexDataAsAlpha8(&a8, &tw, &th);
	for (auto& p : s_added) {
		const ImFontAtlasCustomRect* r = io.Fonts->GetCustomRectByIndex(p.rect);
		int stride = (p.g.width + 7) / 8;
		for (int y = 0; y < p.g.height; y++)
			for (int x = 0; x < p.g.width; x++)
				a8[(r->Y + y) * tw + r->X + x] = (p.g.bits[y * stride + x / 8] & (0x80 >> (x % 8))) ? 255 : 0;
	}
	// (the picture in colour is made again from these)
	if (io.Fonts->TexPixelsRGBA32) {
		IM_FREE(io.Fonts->TexPixelsRGBA32);
		io.Fonts->TexPixelsRGBA32 = nullptr;
	}
	return true;
}


void UiFontNeed(const std::string& utf8) {
	for (char32_t c : PixelText::decode(utf8))
		if (c >= 0x100 && !s_have.count(c))
			s_want.insert(c);
}

std::string UiShown(const std::string& utf8) {
	std::string shown = PixelText::encode(PixelText::visualOrder(PixelText::decode(utf8)));
	UiFontNeed(shown);
	return shown;
}

} // namespace Openc2eImgui
